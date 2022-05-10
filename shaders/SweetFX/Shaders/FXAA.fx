/**
 *                                  FXAA 3.11
 *
 *                               for ReShade 3.0+
 */

#include "ReShadeUI.fxh"

uniform float Subpix < __UNIFORM_SLIDER_FLOAT1
	ui_min = 0.0; ui_max = 1.0;
	ui_tooltip = "Amount of sub-pixel aliasing removal. Higher values makes the image softer/blurrier.";
> = 0.25;

uniform float EdgeThreshold < __UNIFORM_SLIDER_FLOAT1
	ui_min = 0.0; ui_max = 1.0;
	ui_label = "Edge Detection Threshold";
	ui_tooltip = "The minimum amount of local contrast required to apply algorithm.";
> = 0.125;
uniform float EdgeThresholdMin < __UNIFORM_SLIDER_FLOAT1
	ui_min = 0.0; ui_max = 1.0;
	ui_label = "Darkness Threshold";
	ui_tooltip = "Pixels darker than this are not processed in order to increase performance.";
> = 0.0;

//------------------------------ Non-GUI-settings -------------------------------------------------

#ifndef FXAA_QUALITY__PRESET
	// Valid Quality Presets
	// 10 to 15 - default medium dither (10=fastest, 15=highest quality)
	// 20 to 29 - less dither, more expensive (20=fastest, 29=highest quality)
	// 39       - no dither, very expensive
	#define FXAA_QUALITY__PRESET 15
#endif

#ifndef FXAA_GREEN_AS_LUMA
	#define FXAA_GREEN_AS_LUMA 0
#endif

#ifndef FXAA_LINEAR_LIGHT
	#define FXAA_LINEAR_LIGHT 0
#endif

//-------------------------------------------------------------------------------------------------

#if (__RENDERER__ == 0xb000 || __RENDERER__ == 0xb100)
	#define FXAA_GATHER4_ALPHA 1
	#define FxaaTexAlpha4(t, p) tex2Dgather(t, p, 3)
	#define FxaaTexOffAlpha4(t, p, o) tex2Dgatheroffset(t, p, o, 3)
	#define FxaaTexGreen4(t, p) tex2Dgather(t, p, 1)
	#define FxaaTexOffGreen4(t, p, o) tex2Dgatheroffset(t, p, o, 1)
#endif

#define FXAA_PC 1
#define FXAA_HLSL_3 1

// Green as luma requires non-linear colorspace
#if FXAA_GREEN_AS_LUMA
	#undef FXAA_LINEAR_LIGHT
#endif

#include "FXAA.fxh"
#include "ReShade.fxh"

// Samplers

sampler FXAATexture
{
	Texture = ReShade::BackBufferTex;
	MinFilter = Linear; MagFilter = Linear;
	#if FXAA_LINEAR_LIGHT
		SRGBTexture = true;
	#endif
};

// Pixel shaders

#if !FXAA_GREEN_AS_LUMA
float4 FXAALumaPass(float4 vpos : SV_Position, float2 texcoord : TEXCOORD) : SV_Target
{
	float4 color = tex2D(ReShade::BackBuffer, texcoord.xy);
	color.a = sqrt(dot(color.rgb * color.rgb, float3(0.299, 0.587, 0.114)));
	return color;
}
#endif

float4 FXAAPixelShader(float4 vpos : SV_Position, float2 texcoord : TEXCOORD) : SV_Target
{
	return FxaaPixelShader(
		texcoord, // pos
		0, // fxaaConsolePosPos
		FXAATexture, // tex
		FXAATexture, // fxaaConsole360TexExpBiasNegOne
		FXAATexture, // fxaaConsole360TexExpBiasNegTwo
		BUFFER_PIXEL_SIZE, // fxaaQualityRcpFrame
		0, // fxaaConsoleRcpFrameOpt
		0, // fxaaConsoleRcpFrameOpt2
		0, // fxaaConsole360RcpFrameOpt2
		Subpix, // fxaaQualitySubpix
		EdgeThreshold, // fxaaQualityEdgeThreshold
		EdgeThresholdMin, // fxaaQualityEdgeThresholdMin
		0, // fxaaConsoleEdgeSharpness
		0, // fxaaConsoleEdgeThreshold
		0, // fxaaConsoleEdgeThresholdMin
		0 // fxaaConsole360ConstDir
	);
}

// Rendering passes

technique FXAA
{
#if !FXAA_GREEN_AS_LUMA
	pass
	{
		VertexShader = PostProcessVS;
		PixelShader = FXAALumaPass;
	}
#endif
	pass
	{
		VertexShader = PostProcessVS;
		PixelShader = FXAAPixelShader;
		#if FXAA_LINEAR_LIGHT
			SRGBWriteEnable = true;
		#endif
	}
}
