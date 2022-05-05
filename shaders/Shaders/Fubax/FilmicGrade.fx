/*
FilmicGrade v1.1.1 (c) 2018 Jacob Maximilian Fober,

This work is licensed under the Creative Commons 
Attribution-ShareAlike 4.0 International License. 
To view a copy of this license, visit 
http://creativecommons.org/licenses/by-sa/4.0/.
*/


	  ////////////
	 /// MENU ///
	////////////

#include "ReShadeUI.fxh"

uniform int Coefficients < __UNIFORM_RADIO_INT1
	ui_tooltip = "For digital video signal (HDMI, DVI, Display Port) use BT.709,\n"
	"for analog (like VGA, S-Video) use BT.601";
	#if __RESHADE__ < 40000
		ui_label = "YUV coefficients";
		ui_items = "BT.709 (digital signal)\0BT.601 (analog signal))\0";
	#else
		ui_items = "BT.709 - digital\0BT.601 - analog\0";
	#endif
> = 0;

uniform float2 LightControl < __UNIFORM_SLIDER_FLOAT2
	ui_label = "Shadow-Lights";
	ui_tooltip = "Luma low - highs";
	#if __RESHADE__ < 40000
		ui_step = 0.002;
	#endif
	ui_min = -1.0; ui_max = 1.0;
> = float2(0.0, 0.0);


	  //////////////
	 /// SHADER ///
	//////////////

#include "ReShade.fxh"

// RGB to YUV709
static const float3x3 ToYUV709 =
float3x3(
	float3(0.2126, 0.7152, 0.0722),
	float3(-0.09991, -0.33609, 0.436),
	float3(0.615, -0.55861, -0.05639)
);
// RGB to YUV601
static const float3x3 ToYUV601 =
float3x3(
	float3(0.299, 0.587, 0.114),
	float3(-0.14713, -0.28886, 0.436),
	float3(0.615, -0.51499, -0.10001)
);
// YUV709 to RGB
static const float3x3 ToRGB709 =
float3x3(
	float3(1, 0, 1.28033),
	float3(1, -0.21482, -0.38059),
	float3(1, 2.12798, 0)
);
// YUV601 to RGB
static const float3x3 ToRGB601 =
float3x3(
	float3(1, 0, 1.13983),
	float3(1, -0.39465, -0.58060),
	float3(1, 2.03211, 0)
);
static const float2 MaxUV = float2(0.492, 0.877);

// Overlay blending mode
float Overlay(float LayerAB)
{
	float MinAB = min(LayerAB, 0.5);
	float MaxAB = max(LayerAB, 0.5);
	return 2 * (MinAB * MinAB + MaxAB + MaxAB - MaxAB * MaxAB) - 1.5;
}

// Linear grading function
float SuperGrade(float2 Controls, float Input)
{
	// Color Grading
	float2 Grade = Overlay(Input);
	Grade.x = min(Grade.x, 0.5);
	Grade.y = max(Grade.y, 0.5) - 0.5;
	Grade.x = lerp(min(Input, 0.5), Grade.x, -Controls.x);
	Grade.y = lerp(max(0.5, Input) - 0.5, Grade.y * (0.5 - Grade.y), -Controls.y);

	return Grade.x + Grade.y;
}

// Shader
void FilmicGradePS(float4 vois : SV_Position, float2 texcoord : TexCoord, out float3 Display : SV_Target)
{
	if ( Coefficients==1 )
	{
		// Sample display image and convert to YUV
		Display = mul(ToYUV709, tex2D(ReShade::BackBuffer, texcoord).rgb);
		// Color Grade Luma
		Display.x = SuperGrade(LightControl, Display.x);
		// Convert YUV to RGB
		Display = mul(ToRGB709, Display);
	}
	else
	{
		// Sample display image and convert to YUV
		Display = mul(ToYUV601, tex2D(ReShade::BackBuffer, texcoord).rgb);
		// Color Grade Luma
		Display.x = SuperGrade(LightControl, Display.x);
		// Convert YUV to RGB
		Display = mul(ToRGB601, Display);
	}
}


	  //////////////
	 /// OUTPUT ///
	//////////////

technique FilmicGrade < ui_label = "Filmic Grade"; >
{
	pass
	{
		VertexShader = PostProcessVS;
		PixelShader = FilmicGradePS;
	}
}
