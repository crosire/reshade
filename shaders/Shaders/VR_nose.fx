/*
Nose PS (c) 2019 Jacob Maximilian Fober

Anti-nausea shader for VR

This work is licensed under the Creative Commons 
Attribution-ShareAlike 4.0 International License. 
To view a copy of this license, visit 
http://creativecommons.org/licenses/by-sa/4.0/.
*/


	  ////////////
	 /// MENU ///
	////////////

#include "ReShadeUI.fxh"

#ifndef nose
	#define nose 128 // Nose texture resolution
#endif

uniform float Brightness <
	ui_min = 0.0; ui_max = 1.0; ui_step = 0.002;
	ui_type = "drag";
	ui_category = "Virtual nose adjustment";
> = 1.0;

uniform float2 Scale < __UNIFORM_SLIDER_FLOAT2
	#if __RESHADE__ < 40000
		ui_step = 0.001;
	#endif
	ui_min = 0.1; ui_max = 1.0;
	ui_category = "Virtual nose adjustment";
> = float2(0.382, 0.618);


	  //////////////
	 /// SHADER ///
	//////////////

texture NoseTex < source = "nose.png"; > {Width = nose; Height = nose;};
sampler NoseSampler { Texture = NoseTex; AddressU = CLAMP; AddressV = BORDER; };

// Convert RGB to YUV
float3 yuv(float3 rgbImage)
{
	// RGB to YUV709 matrix
	static const float3x3 YUV709 =
	float3x3(
		float3(0.2126, 0.7152, 0.0722),
		float3(-0.09991, -0.33609, 0.436),
		float3(0.615, -0.55861, -0.05639)
	);
	return mul(YUV709, rgbImage);
}

// Overlay blending mode
float Overlay(float LayerAB)
{
	float MinAB = min(LayerAB, 0.5);
	float MaxAB = max(LayerAB, 0.5);
	return 2 * (MinAB*MinAB + 2*MaxAB - MaxAB*MaxAB) - 1.5;
}

#include "ReShade.fxh"

float3 NosePS(float4 vois : SV_Position, float2 texcoord : TexCoord) : SV_Target
{
	// Divide screen in two (mirrored)
	float2 StereoCoord = texcoord;
	StereoCoord.x = 1.0-abs(StereoCoord.x*2.0-1.0)/Scale.x;
	StereoCoord.y = 1.0-(1.0-StereoCoord.y)/Scale.y;

	// Sample display image
	float3 Display = tex2D(ReShade::BackBuffer, texcoord).rgb;
	// Sample nose texture
	float4 NoseTexture = tex2D(NoseSampler, StereoCoord);
	// Change skintone
	NoseTexture.rgb *= lerp(smoothstep(0.0, 1.0, yuv(NoseTexture.rgb).x), 1.0, Brightness);


	// Blend nose with display image
	return lerp(Display, NoseTexture.rgb, NoseTexture.a);
}


	  //////////////
	 /// OUTPUT ///
	//////////////

technique VR_nose < ui_label = "Virtual nose 4 VR"; ui_tooltip = "Virtual Reality:\n"
"* can reduce nausea\n"
"* highly recommended"; >
{
	pass
	{
		VertexShader = PostProcessVS;
		PixelShader = NosePS;
	}
}
