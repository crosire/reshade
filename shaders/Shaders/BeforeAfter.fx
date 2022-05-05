/* 
Before-After PS v1.1.0 (c) 2018 Jacob Maximilian Fober, 

This work is licensed under the Creative Commons 
Attribution-ShareAlike 4.0 International License. 
To view a copy of this license, visit 
http://creativecommons.org/licenses/by-sa/4.0/.
*/


	  ////////////
	 /// MENU ///
	////////////

#include "ReShadeUI.fxh"

uniform bool Line <
> = true;

uniform float Offset <
	ui_type = "drag";
	ui_min = -1.0; ui_max = 1.0; ui_step = 0.001;
> = 0.5;

uniform float Blur <
	ui_label = "Edge Blur";
	ui_type = "drag";
	ui_min = 0.0; ui_max = 1.0; ui_step = 0.001;
> = 0.0;

uniform float3 Color < __UNIFORM_COLOR_FLOAT3
	ui_label = "Line color";
> = float3(0.337, 0.0, 0.118);

// First pass render target
texture BeforeTarget { Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; };
sampler BeforeSampler { Texture = BeforeTarget; };


	  /////////////////
	 /// FUNCTIONS ///
	/////////////////

// Overlay blending mode
float Overlay(float LayerAB)
{
	float MinAB = min(LayerAB, 0.5);
	float MaxAB = max(LayerAB, 0.5);
	return 2.0 * (MinAB * MinAB + MaxAB + MaxAB - MaxAB * MaxAB) - 1.5;
}


	  //////////////
	 /// SHADER ///
	//////////////

#include "ReShade.fxh"

void BeforePS(float4 vpos : SV_Position, float2 UvCoord : TEXCOORD, out float3 Image : SV_Target)
{
	// Grab screen texture
	Image = tex2D(ReShade::BackBuffer, UvCoord).rgb;
}

void AfterPS(float4 vpos : SV_Position, float2 UvCoord : TEXCOORD, out float3 Image : SV_Target)
{
	float Coordinates = Offset < 0.0 ? 1.0 - UvCoord.x : UvCoord.x;
	float AbsOffset = abs(Offset);
	// Separete Before/After
	if(Blur == 0.0)
	{
		bool WhichOne = Coordinates > AbsOffset;
		Image = WhichOne ? tex2D(ReShade::BackBuffer, UvCoord).rgb : tex2D(BeforeSampler, UvCoord).rgb;
		if(Line) Image = Coordinates < AbsOffset - 0.002 || Coordinates > AbsOffset + 0.002 ? Image : Color;
	}
	else
	{
		// Mask
		float Mask = clamp((Coordinates - AbsOffset + 0.5 * Blur) / Blur, 0.0, 1.0);
		Image = lerp(tex2D(BeforeSampler, UvCoord).rgb, tex2D(ReShade::BackBuffer, UvCoord).rgb, Overlay(Mask));
	}
}


	  //////////////
	 /// OUTPUT ///
	//////////////

technique Before < ui_tooltip = "Place this technique before effects you want compare.\nThen move technique 'After'"; >
{
	pass
	{
		VertexShader = PostProcessVS;
		PixelShader = BeforePS;
		RenderTarget = BeforeTarget;
	}
}
technique After < ui_tooltip = "Place this technique after effects you want compare.\nThen move technique 'Before'"; >
{
	pass
	{
		VertexShader = PostProcessVS;
		PixelShader = AfterPS;
	}
}
