/*
Bessel_Bloom
By: Lord of Lunacy

Instead of using the typical Gaussian filter used by bloom, an approximate is implemented instead
using a 2nd order Bessel IIR filter.

The blending and prefiltering methods come from kino-bloom.
*/

//
// Kino/Bloom v2 - Bloom filter for Unity
//
// Copyright (C) 2015, 2016 Keijiro Takahashi
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//

#define COMPUTE 1
#define DIVIDE_ROUNDING_UP(n, d) uint(((n) + (d) - 1) / (d))
#define FILTER_WIDTH 256
#define PIXELS_PER_THREAD 256
#define H_GROUPS uint2(DIVIDE_ROUNDING_UP(BUFFER_WIDTH, PIXELS_PER_THREAD), DIVIDE_ROUNDING_UP(BUFFER_HEIGHT, 64))
#define V_GROUPS uint2(DIVIDE_ROUNDING_UP(BUFFER_WIDTH, 64), DIVIDE_ROUNDING_UP(BUFFER_HEIGHT, PIXELS_PER_THREAD))
#define H_GROUP_SIZE uint2(1, 64)
#define V_GROUP_SIZE uint2(64, 1)
#define PI 3.1415962

#ifndef BESSEL_BLOOM_USE_RGBA16F
#define BESSEL_BLOOM_USE_RGBA16F 0
#endif

#undef FORMAT

#if BESSEL_BLOOM_USE_RGBA16F != 0
	#define FORMAT RGBA16f
#else
	#define FORMAT RGB10A2
#endif


#if __RENDERER__ < 0xb000
	#warning "DX9 and DX10 APIs are unsupported by compute"
	#undef COMPUTE
	#define COMPUTE 0
#endif

#if __RESHADE__ < 50000 && __RENDERER__ == 0xc000
	#warning "Due to a bug in the current version of ReShade, this shader is disabled in DX12 until the release of ReShade 5.0."
	#undef COMPUTE
	#define COMPUTE 0
#endif

#if COMPUTE != 0
static const float4 coefficients = float4(1, 1.5, -0.8333333, 0.1666667);
namespace Bessel_Bloom
{
	texture BackBuffer:COLOR;
	texture Blur0{Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = FORMAT;};
	texture Blur1{Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = FORMAT;};

	sampler sBackBuffer{Texture = BackBuffer;};
	sampler sBlur0{Texture = Blur0;};
	sampler sBlur1{Texture = Blur1;};
	
	
	storage wBlur0{Texture = Blur0;};
	storage wBlur1{Texture = Blur1;};
	
	
	uniform float Intensity<
		ui_type = "slider";
		ui_label = "Intensity";
		ui_tooltip = "How strong the effect is.";
		ui_min = 0; ui_max = 2;
		ui_step = 0.001;
	> = 1;
	
	uniform float K<
		ui_type = "slider";
		ui_label = "Radius";
		ui_tooltip = "This changes the radius of the effect, and should be\n"
		             "used in combination with the Performance dropdown.";
		ui_min = 64; ui_max = 128;
		ui_step = 0.001;
	> = 128;
	
	uniform float Saturation<
		ui_type = "slider";
		ui_label = "Saturation";
		ui_tooltip = "How strong the effect is.";
		ui_min = 0; ui_max = 2;
		ui_step = 0.001;
	> = 1;
	
	uniform float Threshold<
		ui_type = "slider";
		ui_label = "Threshold";
		ui_tooltip = "Brightness threshold for contributing to the bloom.";
		ui_min = 0; ui_max = 1;
		ui_step = 0.001;
	> = 0.7;
	
	uniform float SoftKnee<
		ui_type = "slider";
		ui_label = "Soft Knee";
		ui_tooltip = "A tuning to make the transition between the threshold smoother,\n"
		             "0 corresponds to a hard threshold while a value of 1 corresponds\n"
					 "to a soft threshold.";
		ui_min = 0; ui_max = 1;
		ui_step = 0.001;
	> = 0.5;
	
	uniform float Gamma<
		ui_type = "slider";
		ui_label = "Gamma";
		ui_min = 1; ui_max = 4;
		ui_step = 0.1;
	> = 2.2;
	
	uniform int Performance<
		ui_type = "combo";
		ui_label = "Performance";
		ui_items = "Very Low\0Low\0Medium\0High\0";
		ui_tooltip = "Has an impact on the radius of the effect, going down to low\n"
		             "will make the max radius half the size it is on medium,\n"
				     "and going to high will make it twice the size it is on medium.";
	> = 2;
	
	uniform bool Debug<
		ui_label = "Show Bloom";
	> = 0;
	
	// Vertex shader generating a triangle covering the entire screen
	void PostProcessVS(in uint id : SV_VertexID, out float4 position : SV_Position, out float2 texcoord : TEXCOORD)
	{
		texcoord.x = (id == 2) ? 2.0 : 0.0;
		texcoord.y = (id == 1) ? 2.0 : 0.0;
		position = float4(texcoord * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
	}
	
	float4 PackColors(float3 color)
	{
		
		#if BESSEL_BLOOM_USE_RGBA16F != 0
		return float4(color.rgb, 1);
		#else
		float4 output;
		output.rgb = color.rgb;
		float3 outputRcp = rcp(output.rgb);
		float minRCP = min(min(outputRcp.r, outputRcp.g), outputRcp.b);
		output.a = (minRCP > 8) ? (3/3) :
		           (minRCP > 4) ? (2/3) :
				   (minRCP > 2) ? (1/3) : 0;
		output.rgb *= exp2(output.a * 3);
		return output;
		#endif
	}
	
	float3 UnpackColors(float4 color)
	{
		#if BESSEL_BLOOM_USE_RGBA16F != 0
		return color.rgb;
		#else
		return color.rgb * exp2(-(3 * color.a));
		#endif
	}

	void PrefilterPS(float4 vpos : SV_POSITION, float2 texcoord : TEXCOORD, out float4 output : SV_TARGET0)
	{
		float3 color = tex2D(sBackBuffer, texcoord).rgb;
		color = pow(abs(color), Gamma);
		float brightness = max(max(color.r, color.g), color.b);
		float knee = Threshold * SoftKnee + 1e-5f;
		float3 curve = float3(Threshold - knee, knee * 2, 0.25f / knee);
		float rq = clamp(brightness - curve.x, 0, curve.y);
		rq = curve.z * rq * rq;
		output.rgb = color * (max(rq, brightness - Threshold) / max(brightness, 1e-5));
		output.rgb *= rcp(1-Threshold);
		output = PackColors(output.rgb);
		
	}
	
	void HorizontalForwardFilter(uint3 id, int filterWidth, float k)
	{
		float2 coord = float2(id.x * PIXELS_PER_THREAD, id.y) + 0.5;
		float4 curr[3];
		float4 prev[3];
		
		float denominator = k * k + 3 * k + 3;
		float3 currWeights = float3(3, 6, 3) / denominator;
		float2 prevWeights = float2(-6 + 2*k*k, -k*k + 3 * k - 3) / denominator;
		prev[0] = UnpackColors(tex2Dfetch(sBlur1, clamp(float2(coord.x - filterWidth, coord.y), 0, float2(BUFFER_WIDTH, BUFFER_HEIGHT) - 1))).xyzx;
		float brightness = max(max(prev[0].r, prev[0].g), prev[0].b);
		prev[1] = prev[0].yyyy;
		prev[2] = prev[0].zzzz;
		prev[0] = prev[0].xxxx;
		curr[0] = prev[0];
		curr[1] = prev[1];
		curr[2] = prev[2];

		for(int i = -filterWidth + 1; i < PIXELS_PER_THREAD; i++)
		{
			float2 sampleCoord = clamp(float2(coord.x + i, coord.y), 0, float2(BUFFER_WIDTH, BUFFER_HEIGHT) - 1);
			float3 newSample = UnpackColors(tex2Dfetch(sBlur1, sampleCoord));
			[unroll]
			for(int j = 0; j < 3; j++)
			{
				curr[j] = float4(newSample[j], curr[j].xyz);
				prev[j] = float4(dot(curr[j].xyz, currWeights) + dot(prev[j].xy, prevWeights), prev[j].xyz);
			}
			if(i >= 0)
			{
				tex2Dstore(wBlur0, int2(coord.x + i, coord.y), PackColors(float3(prev[0].x, prev[1].x, prev[2].x)));
			}
		}
	}
	
	void HorizontalBackwardFilter(uint3 id, int filterWidth, float k)
	{
		float2 coord = float2(id.x * PIXELS_PER_THREAD + PIXELS_PER_THREAD, id.y) + 0.5;
		float4 curr[3];
		float4 prev[3];
		float denominator = k * k + 3 * k + 3;
		float3 currWeights = float3(3, 6, 3) / denominator;
		float2 prevWeights = float2(-6 + 2*k*k, -k*k + 3 * k - 3) / denominator;
		prev[0] = UnpackColors(tex2Dfetch(sBlur0, clamp(float2(coord.x + filterWidth, coord.y), 0, float2(BUFFER_WIDTH, BUFFER_HEIGHT) - 1))).xyzx;
		prev[1] = prev[0].yyyy;
		prev[2] = prev[0].zzzz;
		prev[0] = prev[0].xxxx;
		curr[0] = prev[0];
		curr[1] = prev[1];
		curr[2] = prev[2];


		for(int i = filterWidth - 1; i > -PIXELS_PER_THREAD; i--)
		{
			float2 sampleCoord = clamp(float2(coord.x + i, coord.y), 0, float2(BUFFER_WIDTH, BUFFER_HEIGHT) - 1);
			float3 newSample = UnpackColors(tex2Dfetch(sBlur0, sampleCoord));
			[unroll]
			for(int j = 0; j < 3; j++)
			{
				curr[j] = float4(newSample[j], curr[j].xyz);
				prev[j] = float4(dot(curr[j].xyz, currWeights) + dot(prev[j].xy, prevWeights), prev[j].xyz);
			}
			if(i <= 0)
			{
				//float storedSample = (prev + tex2Dfetch(sBlur0, int2(coord.x + i, coord.y)).x) * 0.5;
				barrier();
				tex2Dstore(wBlur1, int2(coord.x + i, coord.y), PackColors(float3(prev[0].x, prev[1].x, prev[2].x)));
			}
		}
	}
	
	void VerticalForwardFilter(uint3 id, int filterWidth, float k)
	{
		float2 coord = float2(id.x, id.y * PIXELS_PER_THREAD) + 0.5;
		float4 curr[3];
		float4 prev[3];
		float denominator = k * k + 3 * k + 3;
		float3 currWeights = float3(3, 6, 3) / denominator;
		float2 prevWeights = float2(-6 + 2*k*k, -k*k + 3 * k - 3) / denominator;
		
		prev[0] = UnpackColors(tex2Dfetch(sBlur1, clamp(float2(coord.x, coord.y - filterWidth), 0, float2(BUFFER_WIDTH, BUFFER_HEIGHT) - 1))).xyzx;
		prev[1] = prev[0].yyyy;
		prev[2] = prev[0].zzzz;
		prev[0] = prev[0].xxxx;
		curr[0] = prev[0];
		curr[1] = prev[1];
		curr[2] = prev[2];

		for(int i = -filterWidth + 1; i < PIXELS_PER_THREAD; i++)
		{
			float2 sampleCoord = clamp(float2(coord.x, coord.y + i), 0, float2(BUFFER_WIDTH, BUFFER_HEIGHT) - 1);
			float3 newSample = UnpackColors(tex2Dfetch(sBlur1, sampleCoord));
			[unroll]
			for(int j = 0; j < 3; j++)
			{
				curr[j] = float4(newSample[j], curr[j].xyz);
				prev[j] = float4(dot(curr[j].xyz, currWeights) + dot(prev[j].xy, prevWeights), prev[j].xyz);
			}
			if(i >= 0)
			{
				tex2Dstore(wBlur0, int2(coord.x, coord.y + i), PackColors(float3(prev[0].x, prev[1].x, prev[2].x)));
			}
		}
	}

	void VerticalBackwardFilter(uint3 id, int filterWidth, float k)
	{
		float2 coord = float2(id.x, id.y * PIXELS_PER_THREAD + PIXELS_PER_THREAD) + 0.5;
		float4 curr[3];
		float4 prev[3];
		float denominator = k * k + 3 * k + 3;
		float3 currWeights = float3(3, 6, 3) / denominator;
		float2 prevWeights = float2(-6 + 2*k*k, -k*k + 3 * k - 3) / denominator;

		prev[0] = UnpackColors(tex2Dfetch(sBlur0, clamp(float2(coord.x, coord.y + filterWidth), 0, float2(BUFFER_WIDTH, BUFFER_HEIGHT) - 1))).xyzx;
		prev[1] = prev[0].yyyy;
		prev[2] = prev[0].zzzz;
		prev[0] = prev[0].xxxx;
		curr[0] = prev[0];
		curr[1] = prev[1];
		curr[2] = prev[2];


		for(int i = filterWidth - 1; i > -PIXELS_PER_THREAD; i--)
		{
			float2 sampleCoord = clamp(float2(coord.x, coord.y + i), 0, float2(BUFFER_WIDTH, BUFFER_HEIGHT) - 1);
			float3 newSample = UnpackColors(tex2Dfetch(sBlur0, sampleCoord));
			[unroll]
			for(int j = 0; j < 3; j++)
			{
				curr[j] = float4(newSample[j], curr[j].xyz);
				prev[j] = float4(dot(curr[j].xyz, currWeights) + dot(prev[j].xy, prevWeights), prev[j].xyz);
			}
			if(i <= 0)
			{
				tex2Dstore(wBlur1, int2(coord.x, coord.y + i), PackColors(float3(prev[0].x, prev[1].x, prev[2].x)));
			}
		}
	}
	
	void HorizontalFilterCS0(uint3 id : SV_DispatchThreadID)
	{
		switch(Performance)
		{
			case 0:
				HorizontalForwardFilter(id, FILTER_WIDTH/4, K/4);
				break;
			case 1:
				HorizontalForwardFilter(id, FILTER_WIDTH/2, K/2);
				break;
			case 3:
				HorizontalForwardFilter(id, FILTER_WIDTH * 2, K * 2);
				break;
			default:
				HorizontalForwardFilter(id, FILTER_WIDTH, K);
				break;
		}
	}
	
	void HorizontalFilterCS1(uint3 id : SV_DispatchThreadID)
	{
		switch(Performance)
		{
			case 0:
				HorizontalBackwardFilter(id, FILTER_WIDTH/4, K/4);
				break;
			case 1:
				HorizontalBackwardFilter(id, FILTER_WIDTH/2, K/2);
				break;
			case 3:
				HorizontalBackwardFilter(id, FILTER_WIDTH * 2, K * 2);
				break;
			default:
				HorizontalBackwardFilter(id, FILTER_WIDTH, K);
				break;
		}
	}
	
	void VerticalFilterCS0(uint3 id : SV_DispatchThreadID)
	{
		switch(Performance)
		{
			case 0:
				VerticalForwardFilter(id, FILTER_WIDTH/4, K/4);
				break;
			case 1:
				VerticalForwardFilter(id, FILTER_WIDTH/2, K/2);
				break;
			case 3:
				VerticalForwardFilter(id, FILTER_WIDTH * 2, K * 2);
				break;
			default:
				VerticalForwardFilter(id, FILTER_WIDTH, K);
				break;
		}
	}
	
	void VerticalFilterCS1(uint3 id : SV_DispatchThreadID)
	{
		switch(Performance)
		{
			case 0:
				VerticalBackwardFilter(id, FILTER_WIDTH/4, K/4);
				break;
			case 1:
				VerticalBackwardFilter(id, FILTER_WIDTH/2, K/2);
				break;
			case 3:
				VerticalBackwardFilter(id, FILTER_WIDTH * 2, K * 2);
				break;
			default:
				VerticalBackwardFilter(id, FILTER_WIDTH, K);
				break;
		}
	}
	
	void OutputPS(float4 vpos : SV_POSITION, float2 texcoord : TEXCOORD, out float4 output : SV_TARGET0)
	{
		float3 color = tex2D(sBackBuffer, texcoord).rgb;
		float3 bloom = UnpackColors(tex2D(sBlur1, texcoord));
		float greyComponent = dot(bloom, 0.33333);
		bloom = lerp(greyComponent, bloom, Saturation);
		
		output.a = 1;
		output.rgb = pow(abs(pow(abs(color), Gamma) + bloom * Intensity * (1-Threshold)), rcp(Gamma));
		if(Debug)
		{
			output.rgb = pow(abs(bloom * Intensity * (1-Threshold)), rcp(Gamma));
		}
			
	}
	
	technique Bessel_Bloom< ui_tooltip = "Instead of using the typical Gaussian filter used by bloom, an approximate is implemented instead\n"
	                                     "using a 2nd order Bessel IIR filter.\n\n"
										 "Part of Insane Shaders\n"
										 "By: Lord Of Lunacy";>
	{	
		pass
		{
			VertexShader = PostProcessVS;
			PixelShader = PrefilterPS;
			RenderTarget = Blur1;
		}
		
		pass
		{
			ComputeShader = HorizontalFilterCS0<H_GROUP_SIZE.x, H_GROUP_SIZE.y>;
			DispatchSizeX = H_GROUPS.x;
			DispatchSizeY = H_GROUPS.y;
		}
		
		pass
		{
			ComputeShader = HorizontalFilterCS1<H_GROUP_SIZE.x, H_GROUP_SIZE.y>;
			DispatchSizeX = H_GROUPS.x;
			DispatchSizeY = H_GROUPS.y;
		}
		
		pass
		{
			ComputeShader = VerticalFilterCS0<V_GROUP_SIZE.x, V_GROUP_SIZE.y>;
			DispatchSizeX = V_GROUPS.x;
			DispatchSizeY = V_GROUPS.y;
		}
		
		pass
		{
			ComputeShader = VerticalFilterCS1<V_GROUP_SIZE.x, V_GROUP_SIZE.y>;
			DispatchSizeX = V_GROUPS.x;
			DispatchSizeY = V_GROUPS.y;
		}
		
		pass
		{
			VertexShader = PostProcessVS;
			PixelShader = OutputPS;
		}
	}
}
#endif
