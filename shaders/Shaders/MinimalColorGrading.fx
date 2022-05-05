/*
	Adapted from
	http://filmicworlds.com/blog/minimal-color-grading-tools/

	Work-in-progress.
*/
// #region Preprocessor

#include "ReShade.fxh"
#include "ReShadeUI.fxh"

#ifndef MINIMAL_COLOR_GRADING_USE_HDR_BUFFER
#define MINIMAL_COLOR_GRADING_USE_HDR_BUFFER 0
#endif

// #endregion

// #region Constants

static const float EPSILON = 1e-5;
static const float CONTRAST_LOG_MIDPOINT = 0.18;

// #endregion

// #region Textures

sampler BackBuffer
{
	Texture = ReShade::BackBufferTex;
	MinFilter = POINT;
	MagFilter = POINT;
	MipFilter = POINT;
	SRGBTexture = true;
};

#if MINIMAL_COLOR_GRADING_USE_HDR_BUFFER

texture MinimalColorGrading_HDRBufferTex <pooled = true;>
{
	Width = BUFFER_WIDTH;
	Height = BUFFER_HEIGHT;
	Format = RGB10A2;
};
sampler HDRBuffer
{
	Texture = MinimalColorGrading_HDRBufferTex;
	MinFilter = POINT;
	MagFilter = POINT;
	MipFilter = POINT;
};

#endif

// #endregion

// #region Uniforms

uniform float Exposure
<
	__UNIFORM_SLIDER_FLOAT1

	ui_label = "Exposure";
	ui_tooltip = "Default: 0.0";
	ui_category = "Basic Adjustments";
	ui_min = -3.0;
	ui_max = 3.0;
> = 0.0;

uniform float3 ColorFilter
<
	__UNIFORM_COLOR_FLOAT3

	ui_label = "Color Filter";
	ui_tooltip = "Default: 255 255 255";
	ui_category = "Basic Adjustments";
> = float3(1.0, 1.0, 1.0);

uniform float Saturation
<
	__UNIFORM_SLIDER_FLOAT1

	ui_label = "Saturation";
	ui_tooltip = "Default: 1.0";
	ui_category = "Basic Adjustments";
	ui_min = 0.0;
	ui_max = 2.0;
> = 1.0;

uniform float Contrast
<
	__UNIFORM_SLIDER_FLOAT1

	ui_label = "Contrast";
	ui_tooltip = "Default: 1.0";
	ui_category = "Basic Adjustments";
	ui_min = 0.0;
	ui_max = 2.0;
> = 1.0;

uniform int SaturationMode
<
	__UNIFORM_COMBO_INT1

	ui_label = "Saturation Mode";
	ui_tooltip = "Default: Luma";
	ui_items = "Average\0Luminance\0Luma\0Length\0";
> = 2;

uniform float2 MousePoint <source="mousepoint";>;

// #endregion

// #region Functions

float get_grayscale(float3 color)
{
	switch (SaturationMode)
	{
		case 0: // Average
			return dot(color, 0.333);
		case 1: // Luminance
			return max(color.r, max(color.g, color.b));
		case 2: // Luma
			return dot(color, float3(0.299, 0.587, 0.114));
		case 3: // Length
			return exp2(length(log2(color)));
	}

	return 0.0;
}

float3 apply_exposure_color(float3 color)
{
	return color.rgb * exp2(Exposure) * ColorFilter;
}

float3 apply_saturation(float3 color)
{
	float gray = get_grayscale(color.rgb);
	return gray + (color.rgb - gray) * Saturation;
}

float3 apply_contrast(float3 color)
{
	color = log2(color + EPSILON);
	color = CONTRAST_LOG_MIDPOINT + (color - CONTRAST_LOG_MIDPOINT) * Contrast;
	return max(0.0, exp2(color) - EPSILON);
}

// #endregion

// #region Shaders

#if MINIMAL_COLOR_GRADING_USE_HDR_BUFFER

float4 CopyToHDRPS(float4 p : SV_POSITION, float2 uv : TEXCOORD) : SV_TARGET
{
	float4 color = tex2D(BackBuffer, uv);
	//color.rgb = inv_reinhard(color.rgb, 1.0 / 10.0);
	return color;
}

#endif

float4 MainPS(float4 p : SV_POSITION, float2 uv : TEXCOORD) : SV_TARGET
{
	#if MINIMAL_COLOR_GRADING_USE_HDR_BUFFER

	float4 color = tex2D(HDRBuffer, uv);

	#else

	float4 color = tex2D(BackBuffer, uv);

	#endif

	color.rgb = apply_exposure_color(color.rgb);
	color.rgb = apply_saturation(color.rgb);
	color.rgb = apply_contrast(color.rgb);

	return color;
}

// #endregion

// #region Technique

technique MinimalColorGrading
{
	#if MINIMAL_COLOR_GRADING_USE_HDR_BUFFER

	pass CopyToHDR
	{
		VertexShader = PostProcessVS;
		PixelShader = CopyToHDRPS;
		RenderTarget = MinimalColorGrading_HDRBufferTex;
	}

	#endif

	pass Main
	{
		VertexShader = PostProcessVS;
		PixelShader = MainPS;
		SRGBWriteEnable = true;
	}
}

// #endregion