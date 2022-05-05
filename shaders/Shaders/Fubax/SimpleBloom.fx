/* 
Simple Bloom PS v0.2.3 (c) 2018 Jacob Maximilian Fober, 

This work is licensed under the Creative Commons 
Attribution-ShareAlike 4.0 International License. 
To view a copy of this license, visit 
http://creativecommons.org/licenses/by-sa/4.0/.
*/


	  ////////////
	 /// MENU ///
	////////////

#include "ReShadeUI.fxh"

uniform float BlurMultiplier < __UNIFORM_SLIDER_FLOAT1
	ui_label = "Radius";
	ui_min = 1; ui_max = 16; ui_step = 0.01;
> = 1.23;

uniform float2 Blend < __UNIFORM_SLIDER_FLOAT2
	ui_min = 0; ui_max = 1; ui_step = 0.001;
> = float2(0.0, 0.8);

uniform bool Debug <
	ui_label = "Debug view";
> = false;


#if !defined(ResolutionX) || !defined(ResolutionY)
	texture SimpleBloomTarget
	{
		// resolution 25%
		Width = BUFFER_WIDTH * 0.25;
		Height = BUFFER_HEIGHT * 0.25;
		Format = RGBA8;
	};
#else
	texture SimpleBloomTarget
	{
		// resolution 25%
		Width = ResolutionX * 0.25;
		Height = ResolutionY * 0.25;
		Format = RGBA8;
	};
#endif
sampler SimpleBloomSampler { Texture = SimpleBloomTarget; };


	  //////////////
	 /// SHADER ///
	//////////////

#include "ReShade.fxh"

void BloomHorizontalPass(float4 vpos : SV_Position, float2 UvCoord : TEXCOORD,
out float3 Target : SV_Target)
{
	const float Weight[11] =
	{
		0.082607,
		0.080977,
		0.076276,
		0.069041,
		0.060049,
		0.050187,
		0.040306,
		0.031105,
		0.023066,
		0.016436,
		0.011254
	};
	// Grab screen texture
	Target.rgb = tex2D(ReShade::BackBuffer, UvCoord).rgb;
	float UvOffset = ReShade::PixelSize.x * BlurMultiplier;
	Target.rgb *= Weight[0];
	for (int i = 1; i < 11; i++)
	{
		float SampleOffset = i * UvOffset;
		Target.rgb += 
		max(
			Target.rgb,
			max(
				tex2Dlod(ReShade::BackBuffer, float4(UvCoord.xy + float2(SampleOffset, 0), 0, 0)).rgb
				, tex2Dlod(ReShade::BackBuffer, float4(UvCoord.xy - float2(SampleOffset, 0), 0, 0)).rgb
			)
		) * Weight[i];
	}
}

void SimpleBloomPS(float4 vpos : SV_Position, float2 UvCoord : TEXCOORD,
out float3 Image : SV_Target)
{
	const float Weight[11] =
	{
		0.082607,
		0.080977,
		0.076276,
		0.069041,
		0.060049,
		0.050187,
		0.040306,
		0.031105,
		0.023066,
		0.016436,
		0.011254
	};
	// Grab second pass screen texture
	float3 Target = tex2D(SimpleBloomSampler, UvCoord).rgb;
	// Vertical gaussian blur
	float UvOffset = ReShade::PixelSize.y * BlurMultiplier;
	Target.rgb *= Weight[0];
	for (int i = 1; i < 11; i++)
	{
		float SampleOffset = i * UvOffset;
		Target.rgb += 
		max(
			Target.rgb,
			max(
				tex2Dlod(SimpleBloomSampler, float4(UvCoord.xy + float2(0, SampleOffset), 0, 0)).rgb
				, tex2Dlod(SimpleBloomSampler, float4(UvCoord.xy - float2(0, SampleOffset), 0, 0)).rgb
			)
		) * Weight[i];
	}

	Image = tex2D(ReShade::BackBuffer, UvCoord).rgb;

	// Threshold
	Target = max(Target - Image, Blend.x) - Blend.x;
	Target /= 1 - min(0.999, Blend.x);

	// Mix with image
	Target *= Blend.y;
//	Image += Target; // Add
	Image = 1 - (1 - Image) * (1 - Target); // Screen

	Image = Debug ? Target : Image;
}


	  //////////////
	 /// OUTPUT ///
	//////////////

technique SimpleBloom < ui_label = "Simple Bloom"; >
{
	pass
	{
		VertexShader = PostProcessVS;
		PixelShader = BloomHorizontalPass;
		RenderTarget = SimpleBloomTarget;
	}
	pass
	{
		VertexShader = PostProcessVS;
		PixelShader = SimpleBloomPS;
	}
}
