/* 
Before-After with Depth PS v1.3.0 (c) 2018 Jacob Maximilian Fober, 

This work is licensed under the Creative Commons 
Attribution-ShareAlike 4.0 International License. 
To view a copy of this license, visit 
http://creativecommons.org/licenses/by-sa/4.0/.
*/


	  ////////////
	 /// MENU ///
	////////////

#include "ReShadeUI.fxh"

uniform bool DepthBased <
	ui_label = "Use Depth";
	ui_category = "Separation settings";
> = false;

uniform bool Line <
	ui_category = "Separation settings";
> = true;

uniform float Offset <
	ui_type = "drag";
	ui_min = -1.0; ui_max = 1.0; ui_step = 0.001;
	ui_category = "Separation settings";
> = 0.5;

uniform float Blur <
	ui_label = "Edge Blur";
	ui_type = "drag";
	ui_min = 0.0; ui_max = 1.0; ui_step = 0.001;
	ui_category = "Separation settings";
> = 0.0;

uniform float3 Color < __UNIFORM_COLOR_FLOAT3
	ui_label = "Line color";
	ui_category = "Separation settings";
> = float3(0.337, 0, 0.118);

uniform bool RadialX <
	ui_label = "Horizontally radial";
	ui_category = "Radial depth adjustment";
> = false;

uniform bool RadialY <
	ui_label = "Vertically radial";
	ui_category = "Radial depth adjustment";
> = false;

uniform int FOV < __UNIFORM_SLIDER_INT1
	ui_label = "FOV (horizontal)";
	ui_tooltip = "Field of view in degrees";
	#if __RESHADE__ < 40000
		ui_step = 1;
	#endif
	ui_min = 0; ui_max = 170;
	ui_category = "Radial depth adjustment";
> = 90;


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
	bool Inverted = Offset < 0.0;

	// Separete Before/After
	float Coordinates = DepthBased ?
	ReShade::GetLinearizedDepth(UvCoord) : Inverted ? 1.0 - UvCoord.x : UvCoord.x;

	// Convert to radial depth
	if( DepthBased && (RadialX || RadialY) )
	{
		float2 Size;
		Size.x = tan(radians(FOV*0.5));
		Size.y = Size.x / ReShade::AspectRatio;
		if(RadialX) Coordinates *= length(float2((UvCoord.x-0.5)*Size.x, 1.0));
		if(RadialY) Coordinates *= length(float2((UvCoord.y-0.5)*Size.y, 1.0));
	}

	float AbsOffset = abs(Offset);

	if(Blur == 0)
	{
		bool WhichOne = Coordinates > AbsOffset;
		WhichOne = DepthBased && Inverted ? !WhichOne : WhichOne;
		Image = WhichOne ? tex2D(ReShade::BackBuffer, UvCoord).rgb : tex2D(BeforeSampler, UvCoord).rgb;
		if(Line) Image = Coordinates < AbsOffset - 0.002 || Coordinates > AbsOffset + 0.002 ? Image : Color;
	}
	else
	{
		// Mask
		float Mask = clamp((Coordinates - AbsOffset + 0.5 * Blur) / Blur, 0.0, 1.0);
		Mask = DepthBased && Inverted ? 1.0 - Mask : Mask;
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
