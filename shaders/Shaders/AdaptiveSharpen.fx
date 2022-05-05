// Copyright (c) 2015-2018, bacondither
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
// 1. Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer
//    in this position and unchanged.
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
// IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
// OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
// IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
// INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
// NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
// THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// Adaptive sharpen - version 2018-04-14
// EXPECTS FULL RANGE GAMMA LIGHT

#include "ReShadeUI.fxh"

uniform float curve_height < __UNIFORM_SLIDER_FLOAT1
	ui_min = 0.01; ui_max = 2.0;
	ui_label = "Sharpening strength";
	ui_tooltip = "Main control of sharpening strength";
	ui_step = 0.01;
> = 1.0;

uniform float curveslope <
	ui_min = 0.01; ui_max = 2.0;
	ui_tooltip = "Sharpening curve slope, high edge values";
	ui_category = "Advanced";
> = 0.5;

uniform float L_overshoot <
	ui_min = 0.001; ui_max = 0.1;
	ui_tooltip = "Max light overshoot before compression";
	ui_category = "Advanced";
> = 0.003;

uniform float L_compr_low <
	ui_min = 0.0; ui_max = 1.0;
	ui_tooltip = "Light compression, default (0.167=~6x)";
	ui_category = "Advanced";
> = 0.167;

uniform float L_compr_high <
	ui_min = 0.0; ui_max = 1.0;
	ui_tooltip = "Light compression, surrounded by edges (0.334=~3x)";
	ui_category = "Advanced";
> = 0.334;

uniform float D_overshoot <
	ui_min = 0.001; ui_max = 0.1;
	ui_tooltip = "Max dark overshoot before compression";
	ui_category = "Advanced";
> = 0.009;

uniform float D_compr_low <
	ui_min = 0.0; ui_max = 1.0;
	ui_tooltip = "Dark compression, default (0.250=4x)";
	ui_category = "Advanced";
> = 0.250;

uniform float D_compr_high <
	ui_min = 0.0; ui_max = 1.0;
	ui_tooltip = "Dark compression, surrounded by edges (0.500=2x)";
	ui_category = "Advanced";
> = 0.500;

uniform float scale_lim <
	ui_min = 0.01; ui_max = 1.0;
	ui_tooltip = "Abs max change before compression";
	ui_category = "Advanced";
> = 0.1;

uniform float scale_cs <
	ui_min = 0.0; ui_max = 1.0;
	ui_tooltip = "Compression slope above scale_lim";
	ui_category = "Advanced";
> = 0.056;

uniform float pm_p <
	ui_min = 0.01; ui_max = 1.0;
	ui_tooltip = "Power mean p-value";
	ui_category = "Advanced";
> = 0.7;

//-------------------------------------------------------------------------------------------------
#ifndef fast_ops
	#define fast_ops 1 // Faster code path, small difference in quality
#endif
//-------------------------------------------------------------------------------------------------

#include "ReShade.fxh"

texture AS_Pass0Tex { Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = RG16F; };
sampler AS_Pass0Sampler { Texture = AS_Pass0Tex; };

// Helper funcs
#define sqr(a)         ( (a)*(a) )
#define max4(a,b,c,d)  ( max(max(a, b), max(c, d)) )

// Get destination pixel values
#define texc(x,y)      ( ReShade::PixelSize*float2(x, y) + tex )
#define getB(x,y)      ( saturate(tex2D(ReShade::BackBuffer, texc(x, y)).rgb) )
#define getT(x,y)      ( tex2D(AS_Pass0Sampler, texc(x, y)).xy )

// Soft if, fast linear approx
#define soft_if(a,b,c) ( saturate((a + b + c + 0.056)*rcp(abs(maxedge) + 0.03) - 0.85) )

// Soft limit, modified tanh
#if (fast_ops == 1) // Tanh approx
	#define soft_lim(v,s)  ( saturate(abs(v/s)*(27 + sqr(v/s))/(27 + 9*sqr(v/s)))*s )
#else
	#define soft_lim(v,s)  ( (exp(2*min(abs(v), s*24)/s) - 1)/(exp(2*min(abs(v), s*24)/s) + 1)*s )
#endif

// Weighted power mean
#define wpmean(a,b,w)  ( pow(abs(w)*pow(abs(a), pm_p) + abs(1-w)*pow(abs(b), pm_p), (1.0/pm_p)) )

// Component-wise distance
#define b_diff(pix)    ( abs(blur - c[pix]) )

// Fast-skip threshold, keep max possible luma error under 0.5/2^bit-depth
#if (fast_ops == 1)
	// Approx of x = tanh(x/y)*y + 0.5/2^bit-depth, y = min(L_overshoot, D_overshoot)
	#define min_overshoot  ( min(abs(L_overshoot), abs(D_overshoot)) )
	#define fskip_th       ( 0.114*pow(min_overshoot, 0.676) + 3.20e-4 ) // 10-bits
	//#define fskip_th       ( 0.045*pow(min_overshoot, 0.667) + 1.75e-5 ) // 14-bits
#else
	// x = tanh(x/y)*y + 0.5/2^bit-depth, y = 0.0001
	#define fskip_th       ( 0.000110882 ) // 14-bits
#endif

// Smoothstep to linearstep approx
//#define SStLS(a,b,x,c) ( clamp(-(6*(c - 1)*(b - x))/(5*(a - b)) - 0.1*c + 1.1, c, 1) )

// Center pixel diff
#define mdiff(a,b,c,d,e,f,g) ( abs(luma[g] - luma[a]) + abs(luma[g] - luma[b])       \
                             + abs(luma[g] - luma[c]) + abs(luma[g] - luma[d])       \
                             + 0.5*(abs(luma[g] - luma[e]) + abs(luma[g] - luma[f])) )

float2 AdaptiveSharpenP0(float4 vpos : SV_Position, float2 tex : TEXCOORD) : SV_Target
{
	// Get points and clip out of range values (BTB & WTW)
	// [                c9                ]
	// [           c1,  c2,  c3           ]
	// [      c10, c4,  c0,  c5, c11      ]
	// [           c6,  c7,  c8           ]
	// [                c12               ]
	float3 c[13] = { getB( 0, 0), getB(-1,-1), getB( 0,-1), getB( 1,-1), getB(-1, 0),
	                 getB( 1, 0), getB(-1, 1), getB( 0, 1), getB( 1, 1), getB( 0,-2),
	                 getB(-2, 0), getB( 2, 0), getB( 0, 2) };

	// Colour to luma, fast approx gamma, avg of rec. 709 & 601 luma coeffs
	float luma = sqrt(dot(float3(0.2558, 0.6511, 0.0931), sqr(c[0])));

	// Blur, gauss 3x3
	float3 blur = (2*(c[2]+c[4]+c[5]+c[7]) + (c[1]+c[3]+c[6]+c[8]) + 4*c[0])/16;

	// Contrast compression, center = 0.5, scaled to 1/3
	float c_comp = saturate(4.0/15.0 + 0.9*exp2(dot(blur, -37.0/15.0)));

	// Edge detection
	// Relative matrix weights
	// [          1          ]
	// [      4,  5,  4      ]
	// [  1,  5,  6,  5,  1  ]
	// [      4,  5,  4      ]
	// [          1          ]
	float edge = length( 1.38*(b_diff(0))
	                   + 1.15*(b_diff(2) + b_diff(4)  + b_diff(5)  + b_diff(7))
	                   + 0.92*(b_diff(1) + b_diff(3)  + b_diff(6)  + b_diff(8))
	                   + 0.23*(b_diff(9) + b_diff(10) + b_diff(11) + b_diff(12)) );

	return float2(edge*c_comp, luma);
}

float3 AdaptiveSharpenP1(float4 vpos : SV_Position, float2 tex : TEXCOORD) : SV_Target
{
	float3 origsat = getB(0, 0);

	// Get texture points, .x = edge, .y = luma
	// [                d22               ]
	// [           d24, d9,  d23          ]
	// [      d21, d1,  d2,  d3, d18      ]
	// [ d19, d10, d4,  d0,  d5, d11, d16 ]
	// [      d20, d6,  d7,  d8, d17      ]
	// [           d15, d12, d14          ]
	// [                d13               ]
	float2 d[25] = { getT( 0, 0), getT(-1,-1), getT( 0,-1), getT( 1,-1), getT(-1, 0),
	                 getT( 1, 0), getT(-1, 1), getT( 0, 1), getT( 1, 1), getT( 0,-2),
	                 getT(-2, 0), getT( 2, 0), getT( 0, 2), getT( 0, 3), getT( 1, 2),
	                 getT(-1, 2), getT( 3, 0), getT( 2, 1), getT( 2,-1), getT(-3, 0),
	                 getT(-2, 1), getT(-2,-1), getT( 0,-3), getT( 1,-2), getT(-1,-2) };

	// Allow for higher overshoot if the current edge pixel is surrounded by similar edge pixels
	float maxedge = max4( max4(d[1].x,d[2].x,d[3].x,d[4].x), max4(d[5].x,d[6].x,d[7].x,d[8].x),
	                      max4(d[9].x,d[10].x,d[11].x,d[12].x), d[0].x );

	// [          x          ]
	// [       z, x, w       ]
	// [    z, z, x, w, w    ]
	// [ y, y, y, 0, y, y, y ]
	// [    w, w, x, z, z    ]
	// [       w, x, z       ]
	// [          x          ]
	float sbe = soft_if(d[2].x,d[9].x, d[22].x)*soft_if(d[7].x,d[12].x,d[13].x)  // x dir
	          + soft_if(d[4].x,d[10].x,d[19].x)*soft_if(d[5].x,d[11].x,d[16].x)  // y dir
	          + soft_if(d[1].x,d[24].x,d[21].x)*soft_if(d[8].x,d[14].x,d[17].x)  // z dir
	          + soft_if(d[3].x,d[23].x,d[18].x)*soft_if(d[6].x,d[20].x,d[15].x); // w dir

	#if (fast_ops == 1)
		float2 cs = lerp( float2(L_compr_low,  D_compr_low),
		                  float2(L_compr_high, D_compr_high), saturate(1.091*sbe - 2.282) );
	#else
		float2 cs = lerp( float2(L_compr_low,  D_compr_low),
		                  float2(L_compr_high, D_compr_high), smoothstep(2, 3.1, sbe) );
	#endif

	float luma[25] = { d[0].y,  d[1].y,  d[2].y,  d[3].y,  d[4].y,
	                   d[5].y,  d[6].y,  d[7].y,  d[8].y,  d[9].y,
	                   d[10].y, d[11].y, d[12].y, d[13].y, d[14].y,
	                   d[15].y, d[16].y, d[17].y, d[18].y, d[19].y,
	                   d[20].y, d[21].y, d[22].y, d[23].y, d[24].y };

	// Pre-calculated default squared kernel weights
	const float3 W1 = float3(0.5,           1.0, 1.41421356237); // 0.25, 1.0, 2.0
	const float3 W2 = float3(0.86602540378, 1.0, 0.54772255751); // 0.75, 1.0, 0.3

	// Transition to a concave kernel if the center edge val is above thr
	#if (fast_ops == 1)
		float3 dW = sqr(lerp( W1, W2, saturate(2.4*d[0].x - 0.82) ));
	#else
		float3 dW = sqr(lerp( W1, W2, smoothstep(0.3, 0.8, d[0].x) ));
	#endif

	float mdiff_c0 = 0.02 + 3*( abs(luma[0]-luma[2]) + abs(luma[0]-luma[4])
	                          + abs(luma[0]-luma[5]) + abs(luma[0]-luma[7])
	                          + 0.25*(abs(luma[0]-luma[1]) + abs(luma[0]-luma[3])
	                                 +abs(luma[0]-luma[6]) + abs(luma[0]-luma[8])) );

	// Use lower weights for pixels in a more active area relative to center pixel area
	// This results in narrower and less visible overshoots around sharp edges
	float weights[12] = { ( min(mdiff_c0/mdiff(24, 21, 2,  4,  9,  10, 1),  dW.y) ),   // c1
	                      ( dW.x ),                                                    // c2
	                      ( min(mdiff_c0/mdiff(23, 18, 5,  2,  9,  11, 3),  dW.y) ),   // c3
	                      ( dW.x ),                                                    // c4
	                      ( dW.x ),                                                    // c5
	                      ( min(mdiff_c0/mdiff(4,  20, 15, 7,  10, 12, 6),  dW.y) ),   // c6
	                      ( dW.x ),                                                    // c7
	                      ( min(mdiff_c0/mdiff(5,  7,  17, 14, 12, 11, 8),  dW.y) ),   // c8
	                      ( min(mdiff_c0/mdiff(2,  24, 23, 22, 1,  3,  9),  dW.z) ),   // c9
	                      ( min(mdiff_c0/mdiff(20, 19, 21, 4,  1,  6,  10), dW.z) ),   // c10
	                      ( min(mdiff_c0/mdiff(17, 5,  18, 16, 3,  8,  11), dW.z) ),   // c11
	                      ( min(mdiff_c0/mdiff(13, 15, 7,  14, 6,  8,  12), dW.z) ) }; // c12

	weights[0] = (max(max((weights[8]  + weights[9])/4,  weights[0]), 0.25) + weights[0])/2;
	weights[2] = (max(max((weights[8]  + weights[10])/4, weights[2]), 0.25) + weights[2])/2;
	weights[5] = (max(max((weights[9]  + weights[11])/4, weights[5]), 0.25) + weights[5])/2;
	weights[7] = (max(max((weights[10] + weights[11])/4, weights[7]), 0.25) + weights[7])/2;

	// Calculate the negative part of the laplace kernel and the low threshold weight
	float lowthrsum   = 0;
	float weightsum   = 0;
	float neg_laplace = 0;

	[unroll] for (int pix = 0; pix < 12; ++pix)
	{
		#if (fast_ops == 1)
			float lowthr = clamp((13.2*d[pix + 1].x - 0.221), 0.01, 1);

			neg_laplace += sqr(luma[pix + 1])*(weights[pix]*lowthr);
		#else
			float t = saturate((d[pix + 1].x - 0.01)/0.09);
			float lowthr = t*t*(2.97 - 1.98*t) + 0.01; // t*t*(3 - a*3 - (2 - a*2)*t) + a

			neg_laplace += pow(abs(luma[pix + 1]) + 0.06, 2.4)*(weights[pix]*lowthr);
		#endif
		weightsum += weights[pix]*lowthr;
		lowthrsum += lowthr/12;
	}

	#if (fast_ops == 1)
		neg_laplace = sqrt(neg_laplace/weightsum);
	#else
		neg_laplace = pow(abs(neg_laplace/weightsum), (1.0/2.4)) - 0.06;
	#endif

	// Compute sharpening magnitude function
	float sharpen_val = curve_height/(curve_height*curveslope*pow(abs(d[0].x), 3.5) + 0.625);

	// Calculate sharpening diff and scale
	float sharpdiff = (d[0].y - neg_laplace)*(lowthrsum*sharpen_val + 0.01);

	// Skip limiting on flat areas where sharpdiff is low
	[branch] if (abs(sharpdiff) > fskip_th)
	{
		// Calculate local near min & max, partial sort
		// Manually unrolled outer loop, solves OpenGL slowdown
		{
			float temp; int i; int ii;

			// 1st iteration
			[unroll] for (i = 0; i < 24; i += 2)
			{
				temp = luma[i];
				luma[i]   = min(luma[i], luma[i+1]);
				luma[i+1] = max(temp, luma[i+1]);
			}
			[unroll] for (ii = 24; ii > 0; ii -= 2)
			{
				temp = luma[0];
				luma[0]    = min(luma[0], luma[ii]);
				luma[ii]   = max(temp, luma[ii]);

				temp = luma[24];
				luma[24]   = max(luma[24], luma[ii-1]);
				luma[ii-1] = min(temp, luma[ii-1]);
			}

			// 2nd iteration
			[unroll] for (i = 1; i < 23; i += 2)
			{
				temp = luma[i];
				luma[i]   = min(luma[i], luma[i+1]);
				luma[i+1] = max(temp, luma[i+1]);
			}
			[unroll] for (ii = 23; ii > 1; ii -= 2)
			{
				temp = luma[1];
				luma[1]    = min(luma[1], luma[ii]);
				luma[ii]   = max(temp, luma[ii]);

				temp = luma[23];
				luma[23]   = max(luma[23], luma[ii-1]);
				luma[ii-1] = min(temp, luma[ii-1]);
			}

			#if (fast_ops != 1) // 3rd iteration
				[unroll] for (i = 2; i < 22; i += 2)
				{
					temp = luma[i];
					luma[i]   = min(luma[i], luma[i+1]);
					luma[i+1] = max(temp, luma[i+1]);
				}
				[unroll] for (ii = 22; ii > 2; ii -= 2)
				{
					temp = luma[2];
					luma[2]    = min(luma[2], luma[ii]);
					luma[ii]   = max(temp, luma[ii]);

					temp = luma[22];
					luma[22]   = max(luma[22], luma[ii-1]);
					luma[ii-1] = min(temp, luma[ii-1]);
				}
			#endif
		}

		// Calculate tanh scale factors
		#if (fast_ops == 1)
			float nmax = (max(luma[23], d[0].y)*2 + luma[24])/3;
			float nmin = (min(luma[1],  d[0].y)*2 + luma[0])/3;

			float min_dist  = min(abs(nmax - d[0].y), abs(d[0].y - nmin));
			float pos_scale = min_dist + L_overshoot;
			float neg_scale = min_dist + D_overshoot;
		#else
			float nmax = (max(luma[22] + luma[23]*2, d[0].y*3) + luma[24])/4;
			float nmin = (min(luma[2]  + luma[1]*2,  d[0].y*3) + luma[0])/4;

			float min_dist  = min(abs(nmax - d[0].y), abs(d[0].y - nmin));
			float pos_scale = min_dist + min(L_overshoot, 1.0001 - min_dist - d[0].y);
			float neg_scale = min_dist + min(D_overshoot, 0.0001 + d[0].y - min_dist);
		#endif

		pos_scale = min(pos_scale, scale_lim*(1 - scale_cs) + pos_scale*scale_cs);
		neg_scale = min(neg_scale, scale_lim*(1 - scale_cs) + neg_scale*scale_cs);

		// Soft limited anti-ringing with tanh, wpmean to control compression slope
		sharpdiff = wpmean( max(sharpdiff, 0), soft_lim( max(sharpdiff, 0), pos_scale ), cs.x )
		          - wpmean( min(sharpdiff, 0), soft_lim( min(sharpdiff, 0), neg_scale ), cs.y );
	}

	// Compensate for saturation loss/gain while making pixels brighter/darker
	float sharpdiff_lim = saturate(d[0].y + sharpdiff) - d[0].y;
	float satmul = (d[0].y + max(sharpdiff_lim*0.9, sharpdiff_lim)*1.03 + 0.03)/(d[0].y + 0.03);
	float3 res = d[0].y + (sharpdiff_lim*3 + sharpdiff)/4 + (origsat - d[0].y)*satmul;

	return saturate(res);
}

technique AdaptiveSharpen
{
	pass AdaptiveSharpenPass1
	{
		VertexShader = PostProcessVS;
		PixelShader  = AdaptiveSharpenP0;
		RenderTarget = AS_Pass0Tex;
	}

	pass AdaptiveSharpenPass2
	{
		VertexShader = PostProcessVS;
		PixelShader  = AdaptiveSharpenP1;
	}
}
