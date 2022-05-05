// Copyright (c) 2016-2018, bacondither
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

// Colourfulness - version 2018-11-12
// EXPECTS FULL RANGE GAMMA LIGHT

#include "ReShadeUI.fxh"

uniform float colourfulness < __UNIFORM_SLIDER_FLOAT1
	ui_min = -1.0; ui_max = 2.0;
	ui_tooltip = "Degree of colourfulness, 0 = neutral";
	ui_step = 0.01;
> = 0.4;

uniform float lim_luma < __UNIFORM_SLIDER_FLOAT1
	ui_min = 0.1; ui_max = 1.0;
	ui_tooltip = "Lower values allows for more change near clipping";
	ui_step = 0.01;
> = 0.7;

uniform bool enable_dither <
	ui_tooltip = "Enables dithering, avoids introducing banding in gradients";
	ui_category = "Dither";
> = false;

uniform bool col_noise <
	ui_tooltip = "Coloured dither noise, lower subjective noise level";
	ui_category = "Dither";
> = true;

uniform float backbuffer_bits <
	ui_min = 1.0; ui_max = 32.0;
	ui_tooltip = "Backbuffer bith depth, most likely 8 or 10 bits";
	ui_category = "Dither";
> = 8.0;

//-------------------------------------------------------------------------------------------------
#ifndef fast_luma
	#define fast_luma 1 // Rapid approx of sRGB gamma, small difference in quality
#endif

#ifndef temporal_dither
	#define temporal_dither 0 // Dither changes with every frame
#endif
//-------------------------------------------------------------------------------------------------

#include "Reshade.fxh"

#if (temporal_dither == 1)
	uniform int rnd < source = "random"; min = 0; max = 1000; >;
#endif

// Sigmoid function, sign(v)*pow(pow(abs(v), -2) + pow(s, -2), 1.0/-2)
#define soft_lim(v,s)  ( (v*s)*rcp(sqrt(s*s + v*v)) )

// Weighted power mean, p = 0.5
#define wpmean(a,b,w)  ( pow(abs(w)*sqrt(abs(a)) + abs(1-w)*sqrt(abs(b)), 2) )

// Max/Min RGB components
#define maxRGB(c)      ( max((c).r, max((c).g, (c).b)) )
#define minRGB(c)      ( min((c).r, min((c).g, (c).b)) )

// Mean of Rec. 709 & 601 luma coefficients
#define lumacoeff        float3(0.2558, 0.6511, 0.0931)

float3 Colourfulness(float4 vpos : SV_Position, float2 tex : TEXCOORD) : SV_Target
{
	#if (fast_luma == 1)
		float3 c0  = tex2D(ReShade::BackBuffer, tex).rgb;
		float luma = sqrt(dot(saturate(c0*abs(c0)), lumacoeff));
		c0 = saturate(c0);
	#else // Better approx of sRGB gamma
		float3 c0  = saturate(tex2D(ReShade::BackBuffer, tex).rgb);
		float luma = pow(dot(pow(c0 + 0.06, 2.4), lumacoeff), 1.0/2.4) - 0.06;
	#endif

	// Calc colour saturation change
	float3 diff_luma = c0 - luma;
	float3 c_diff = diff_luma*(colourfulness + 1) - diff_luma;

	if (colourfulness > 0.0)
	{
		// 120% of c_diff clamped to max visible range + overshoot
		float3 rlc_diff = clamp((c_diff*1.2) + c0, -0.0001, 1.0001) - c0;

		// Calc max saturation-increase without altering RGB ratios
		float poslim = (1.0002 - luma)/(abs(maxRGB(diff_luma)) + 0.0001);
		float neglim = (luma + 0.0002)/(abs(minRGB(diff_luma)) + 0.0001);

		float3 diffmax = diff_luma*min(min(poslim, neglim), 32) - diff_luma;

		// Soft limit diff
		c_diff = soft_lim( c_diff, max(wpmean(diffmax, rlc_diff, lim_luma), 1e-7) );
	}

	if (enable_dither == true)
	{
		// Interleaved gradient noise by Jorge Jimenez
		const float3 magic = float3(0.06711056, 0.00583715, 52.9829189);
		#if (temporal_dither == 1)
			float xy_magic = (vpos.x + rnd)*magic.x + (vpos.y + rnd)*magic.y;
		#else
			float xy_magic = vpos.x*magic.x + vpos.y*magic.y;
		#endif
		float noise = (frac(magic.z*frac(xy_magic)) - 0.5)/(exp2(backbuffer_bits) - 1);
		c_diff += col_noise == true ? float3(-noise, noise, -noise) : noise;
	}

	return saturate(c0 + c_diff);
}

technique Colourfulness
{
	pass
	{
		VertexShader = PostProcessVS;
		PixelShader  = Colourfulness;
	}
}
