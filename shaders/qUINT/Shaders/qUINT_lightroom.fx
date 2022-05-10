/*=============================================================================

	ReShade 4 effect file
    github.com/martymcmodding

	Support me:
   		paypal.me/mcflypg
   		patreon.com/mcflypg

    Lightroom 
    by Marty McFly / P.Gilcher
    part of qUINT shader library for ReShade 4

    Copyright (c) Pascal Gilcher / Marty McFly. All rights reserved.

=============================================================================*/

/*=============================================================================
	Preprocessor settings
=============================================================================*/

#ifndef ENABLE_HISTOGRAM
 #define ENABLE_HISTOGRAM	0
#endif

#ifndef HISTOGRAM_BINS_NUM
 #define HISTOGRAM_BINS_NUM 128
#endif

/*=============================================================================
	UI Uniforms
=============================================================================*/

uniform bool LIGHTROOM_ENABLE_LUT <
	ui_label = "Enable LUT Overlay";
	ui_tooltip = "This displays a neutral LUT onscreen, all color adjustments of this shader and consecutive ones are applied to it.\nTaking a screenshot and using the cropped LUT with the TuningPalette will reproduce all changes of this shader.\nTo make sure that all color changes of your current preset are saved, put Lightroom as early in the\ntechnique chain as possible. Putting grain, sharpening, bloom etc after Lightroom.fx will break the LUT.";
    ui_category = "LUT";
> = false;

uniform int LIGHTROOM_LUT_TILE_SIZE <
	ui_type = "drag";
	ui_min = 8; ui_max = 64;
	ui_label = "LUT tile size";
	ui_tooltip = "This controls the XY size of tiles of the LUT (which is accuracy in red/green channel).";
    ui_category = "LUT";
> = 16;

uniform int LIGHTROOM_LUT_TILE_COUNT <
	ui_type = "drag";
	ui_min = 8; ui_max = 64;
	ui_label = "LUT tile count";
	ui_tooltip = "This controls the amount of tiles of the LUT (which is accuracy in blue channel).\nBe aware that Tile Size XY * Tile Amount is the width of the LUT and if this value\nis larger than your resolution width, the LUT won't fit on your screen.";
    ui_category = "LUT";
> = 16;

uniform int LIGHTROOM_LUT_SCROLL <
	ui_type = "drag";
	ui_min = 0; ui_max = 5;
	ui_label = "LUT scroll";
	ui_tooltip = "If your LUT size exceeds your screen width, set this to 0, take screenshot, set it to 1, take screenshot\netc until you  reach the end of your LUT and assemble the screenshots like a panorama.\nIf your LUT fits the screen size however, leave it at 0.";
    ui_category = "LUT";
> = 0;

uniform bool LIGHTROOM_ENABLE_CURVE_DISPLAY <
	ui_label = "Enable Luma Curve Display";
	ui_tooltip = "This enables a small overlay with a luma curve\nso you can monitor changes made by exposure, levels etc.";
    ui_category = "Debug";
> = false;

uniform bool LIGHTROOM_ENABLE_CLIPPING_DISPLAY <
	ui_label = "Enable Black/White Clipping Mask";
	ui_tooltip = "This shows where colors reach #000000 black or #ffffff white, helpful for adjusting levels properly.\nNOTE: Any shader that operates after Lightroom in ReShade technique list can change final color levels afterwards\nso either put Lightroom last in line or take this with a grain of salt.";
    ui_category = "Debug";
> = false;

#if(ENABLE_HISTOGRAM == 1)

	uniform bool LIGHTROOM_ENABLE_HISTOGRAM <
		ui_label = "Enable Histogram";
		ui_tooltip = "This enables a small overlay with a histogram for monitoring purposes.\nFor higher performance, open shader and set HISTOGRAM_BINS_NUM to a lower value.";
        ui_category = "Histogram";
	> = false;

	uniform int LIGHTROOM_HISTOGRAM_SAMPLES <
		ui_type = "drag";
		ui_min = 32; ui_max = 96;
		ui_label = "Histogram Samples";
		ui_tooltip = "The amount of samples, 20 means 20x20 samples distributed on the screen.\nHigher means a more accurate histogram depicition and less temporal noise.";
        ui_category = "Histogram";
	> = 20;

	uniform float LIGHTROOM_HISTOGRAM_HEIGHT <
		ui_type = "drag";
		ui_step = 1;
		ui_min = 5.0; ui_max = 50.0;
		ui_label = "Histogram Curve Height";
		ui_tooltip = "Raises the Histogram curve if the values are highly distributed and not visible very well.";
        ui_category = "Histogram";
	> = 15;

	uniform float LIGHTROOM_HISTOGRAM_SMOOTHNESS <
		ui_type = "drag";
		ui_min = 1.0; ui_max = 10.00;
		ui_label = "Histogram Curve Smoothness";
		ui_tooltip = "Smoothens the Histogram curve for a more temporally coherent result.\nNote that raising this falsifies the Histogram data.";
        ui_category = "Histogram";
	> = 5.00;

#endif

//=============================================================================

uniform float LIGHTROOM_RED_HUESHIFT <
	ui_type = "drag";
	ui_min = -1.00; ui_max = 1.00;
	ui_label = "Red       Hue Control";
	ui_tooltip = "Magenta <= ... Red ... => Orange";
    ui_category = "Palette";
> = 0.00;

uniform float LIGHTROOM_ORANGE_HUESHIFT <
	ui_type = "drag";
	ui_min = -1.00; ui_max = 1.00;
	ui_label = "Orange    Hue Control";
	ui_tooltip = "Red <= ... Orange ... => Yellow";
    ui_category = "Palette";
> = 0.00;

uniform float LIGHTROOM_YELLOW_HUESHIFT <
	ui_type = "drag";
	ui_min = -1.00; ui_max = 1.00;
	ui_label = "Yellow    Hue Control";
	ui_tooltip = "Orange <= ... Yellow ... => Green";
    ui_category = "Palette";
> = 0.00;

uniform float LIGHTROOM_GREEN_HUESHIFT <
	ui_type = "drag";
	ui_min = -1.00; ui_max = 1.00;
	ui_label = "Green     Hue Control";
	ui_tooltip = "Yellow <= ... Green ... => Aqua";
    ui_category = "Palette";
> = 0.00;

uniform float LIGHTROOM_AQUA_HUESHIFT <
	ui_type = "drag";
	ui_min = -1.00; ui_max = 1.00;
	ui_label = "Aqua      Hue Control";
	ui_tooltip = "Green <= ... Aqua ... => Blue";
    ui_category = "Palette";
> = 0.00;

uniform float LIGHTROOM_BLUE_HUESHIFT <
	ui_type = "drag";
	ui_min = -1.00; ui_max = 1.00;
	ui_label = "Blue      Hue Control";
	ui_tooltip = "Aqua <= ... Blue ... => Magenta";
    ui_category = "Palette";
> = 0.00;

uniform float LIGHTROOM_MAGENTA_HUESHIFT <
	ui_type = "drag";
	ui_min = -1.00; ui_max = 1.00;
	ui_label = "Magenta   Hue Control";
	ui_tooltip = "Blue <= ... Magenta ... => Red";
    ui_category = "Palette";
> = 0.00;

//=============================================================================

uniform float LIGHTROOM_RED_EXPOSURE <
	ui_type = "drag";
	ui_min = -1.00; ui_max = 1.00;
	ui_label = "Red       Exposure";
	ui_tooltip = "Exposure control of Red colors";
    ui_category = "Palette";
> = 0.00;

uniform float LIGHTROOM_ORANGE_EXPOSURE <
	ui_type = "drag";
	ui_min = -1.00; ui_max = 1.00;
	ui_label = "Orange    Exposure";
	ui_tooltip = "Exposure control of Orange colors";
    ui_category = "Palette";
> = 0.00;

uniform float LIGHTROOM_YELLOW_EXPOSURE <
	ui_type = "drag";
	ui_min = -1.00; ui_max = 1.00;
	ui_label = "Yellow    Exposure";
	ui_tooltip = "Exposure control of Yellow colors";
    ui_category = "Palette";
> = 0.00;

uniform float LIGHTROOM_GREEN_EXPOSURE <
	ui_type = "drag";
	ui_min = -1.00; ui_max = 1.00;
	ui_label = "Green     Exposure";
	ui_tooltip = "Exposure control of Green colors";
    ui_category = "Palette";
> = 0.00;

uniform float LIGHTROOM_AQUA_EXPOSURE <
	ui_type = "drag";
	ui_min = -1.00; ui_max = 1.00;
	ui_label = "Aqua      Exposure";
	ui_tooltip = "Exposure control of Aqua colors";
    ui_category = "Palette";
> = 0.00;

uniform float LIGHTROOM_BLUE_EXPOSURE <
	ui_type = "drag";
	ui_min = -1.00; ui_max = 1.00;
	ui_label = "Blue      Exposure";
	ui_tooltip = "Exposure control of Blue colors";
    ui_category = "Palette";
> = 0.00;

uniform float LIGHTROOM_MAGENTA_EXPOSURE <
	ui_type = "drag";
	ui_min = -1.00; ui_max = 1.00;
	ui_label = "Magenta   Exposure";
	ui_tooltip = "Exposure control of Magenta colors";
    ui_category = "Palette";
> = 0.00;

//=============================================================================

uniform float LIGHTROOM_RED_SATURATION <
	ui_type = "drag";
	ui_min = -1.00; ui_max = 1.00;
	ui_label = "Red       Saturation";
	ui_tooltip = "Saturation control of Red colors";
    ui_category = "Palette";
> = 0.00;

uniform float LIGHTROOM_ORANGE_SATURATION <
	ui_type = "drag";
	ui_min = -1.00; ui_max = 1.00;
	ui_label = "Orange    Saturation";
	ui_tooltip = "Saturation control of Orange colors";
    ui_category = "Palette";
> = 0.00;

uniform float LIGHTROOM_YELLOW_SATURATION <
	ui_type = "drag";
	ui_min = -1.00; ui_max = 1.00;
	ui_label = "Yellow    Saturation";
	ui_tooltip = "Saturation control of Yellow colors";
    ui_category = "Palette";
> = 0.00;

uniform float LIGHTROOM_GREEN_SATURATION <
	ui_type = "drag";
	ui_min = -1.00; ui_max = 1.00;
	ui_label = "Green     Saturation";
	ui_tooltip = "Saturation control of Green colors";
    ui_category = "Palette";
> = 0.00;

uniform float LIGHTROOM_AQUA_SATURATION <
	ui_type = "drag";
	ui_min = -1.00; ui_max = 1.00;
	ui_label = "Aqua      Saturation";
	ui_tooltip = "Saturation control of Aqua colors";
    ui_category = "Palette";
> = 0.00;

uniform float LIGHTROOM_BLUE_SATURATION <
	ui_type = "drag";
	ui_min = -1.00; ui_max = 1.00;
	ui_label = "Blue      Saturation";
	ui_tooltip = "Saturation control of Blue colors";
    ui_category = "Palette";
> = 0.00;

uniform float LIGHTROOM_MAGENTA_SATURATION <
	ui_type = "drag";
	ui_min = -1.00; ui_max = 1.00;
	ui_label = "Magenta   Saturation";
	ui_tooltip = "Saturation control of Magenta colors";
    ui_category = "Palette";
> = 0.00;

//=============================================================================

uniform float LIGHTROOM_GLOBAL_BLACK_LEVEL <
	ui_type = "drag";
	ui_min = 0; ui_max = 512;
	ui_step = 1;
	ui_label = "Global Black Level";
	ui_tooltip = "Scales input HSL value. Everything darker than this is mapped to black.";
    ui_category = "Curves";
> = 0.00;

uniform float LIGHTROOM_GLOBAL_WHITE_LEVEL <
	ui_type = "drag";
	ui_min = 0; ui_max = 512;
	ui_step = 1;
	ui_label = "Global White Level";
	ui_tooltip = "Scales input HSL value. ";
    ui_category = "Curves";
> = 255.00;

uniform float LIGHTROOM_GLOBAL_EXPOSURE <
	ui_type = "drag";
	ui_min = -1.00; ui_max = 1.00;
	ui_label = "Global Exposure";
	ui_tooltip = "Global Exposure Control";
    ui_category = "Curves";
> = 0.00;

uniform float LIGHTROOM_GLOBAL_GAMMA <
	ui_type = "drag";
	ui_min = -1.00; ui_max = 1.00;
	ui_label = "Global Gamma";
	ui_tooltip = "Global Gamma Control";
    ui_category = "Curves";
> = 0.00;

uniform float LIGHTROOM_GLOBAL_BLACKS_CURVE <
	ui_type = "drag";
	ui_min = -1.00; ui_max = 1.00;
	ui_label = "Global Blacks Curve";
	ui_tooltip = "Global Blacks Curve Control";
    ui_category = "Curves";
> = 0.00;

uniform float LIGHTROOM_GLOBAL_SHADOWS_CURVE <
	ui_type = "drag";
	ui_min = -1.00; ui_max = 1.00;
	ui_label = "Global Shadows Curve";
	ui_tooltip = "Global Shadows Curve Control";
    ui_category = "Curves";
> = 0.00;

uniform float LIGHTROOM_GLOBAL_MIDTONES_CURVE <
	ui_type = "drag";
	ui_min = -1.00; ui_max = 1.00;
	ui_label = "Global Midtones Curve";
	ui_tooltip = "Global Midtones Curve Control";
    ui_category = "Curves";
> = 0.00;

uniform float LIGHTROOM_GLOBAL_HIGHLIGHTS_CURVE <
	ui_type = "drag";
	ui_min = -1.00; ui_max = 1.00;
	ui_label = "Global Highlights Curve";
	ui_tooltip = "Global Highlights Curve Control";
    ui_category = "Curves";
> = 0.00;

uniform float LIGHTROOM_GLOBAL_WHITES_CURVE <
	ui_type = "drag";
	ui_min = -1.00; ui_max = 1.00;
	ui_label = "Global Whites Curve";
	ui_tooltip = "Global Whites Curve Control";
    ui_category = "Curves";
> = 0.00;

uniform float LIGHTROOM_GLOBAL_CONTRAST <
	ui_type = "drag";
	ui_min = -1.00; ui_max = 1.00;
	ui_label = "Global Contrast";
	ui_tooltip = "Global Contrast Control";
    ui_category = "Curves";
> = 0.00;

uniform float LIGHTROOM_GLOBAL_SATURATION <
	ui_type = "drag";
	ui_min = -1.00; ui_max = 1.00;
	ui_label = "Global Saturation";
	ui_tooltip = "Global Saturation Control";
    ui_category = "Color & Saturation";
> = 0.00;

uniform float LIGHTROOM_GLOBAL_VIBRANCE <
	ui_type = "drag";
	ui_min = -1.00; ui_max = 1.00;
	ui_label = "Global Vibrance";
	ui_tooltip = "Global Vibrance Control";
    ui_category = "Color & Saturation";
> = 0.00;

uniform float LIGHTROOM_GLOBAL_TEMPERATURE <
	ui_type = "drag";
	ui_min = -1.00; ui_max = 1.00;
	ui_label = "Global White Balance: Temperature";
	ui_tooltip = "Global Temperature Control";
    ui_category = "Color & Saturation";
> = 0.00;

uniform float LIGHTROOM_GLOBAL_TINT <
	ui_type = "drag";
	ui_min = -1.00; ui_max = 1.00;
	ui_label = "Global White Balance: Tint";
	ui_tooltip = "Global Tint Control";
    ui_category = "Color & Saturation";
> = 0.00;

//=============================================================================

uniform bool LIGHTROOM_ENABLE_VIGNETTE <
	ui_label = "Enable Vignette Effect";
	ui_tooltip = "This enables a vignette effect (corner darkening).";
    ui_category = "Vignette";
> = false;

uniform bool LIGHTROOM_VIGNETTE_SHOW_RADII <
	ui_label = "Show Vignette Inner and Outer radius";
	ui_tooltip = "This makes the inner and outer radius setting visible.\nVignette intensity builds up from green (no vignetting) to red (full vignetting).";
    ui_category = "Vignette";
> = false;

uniform float LIGHTROOM_VIGNETTE_RADIUS_INNER <
	ui_type = "drag";
	ui_min = 0.00; ui_max = 2.00;
	ui_label = "Inner Vignette Radius";
	ui_tooltip = "Anything closer to the screen center than this is not affected by vignette.";
    ui_category = "Vignette";
> = 0.00;

uniform float LIGHTROOM_VIGNETTE_RADIUS_OUTER <
	ui_type = "drag";
	ui_min = 0.00; ui_max = 3.00;
	ui_label = "Outer Vignette Radius";
	ui_tooltip = "Anything farther from the screen center than this gets fully vignette'd.";
    ui_category = "Vignette";
> = 1.00;

uniform float LIGHTROOM_VIGNETTE_WIDTH <
	ui_type = "drag";
	ui_min = 0.00; ui_max = 1.00;
	ui_label = "Vignette Width";
	ui_tooltip = "Higher values stretch the vignette horizontally.";
    ui_category = "Vignette";
> = 0.00;

uniform float LIGHTROOM_VIGNETTE_HEIGHT <
	ui_type = "drag";
	ui_min = 0.00; ui_max = 1.00;
	ui_label = "Vignette Height";
	ui_tooltip = "Higher values stretch the vignette vertically.";
    ui_category = "Vignette";
> = 0.00;

uniform float LIGHTROOM_VIGNETTE_AMOUNT <
	ui_type = "drag";
	ui_min = 0.00; ui_max = 1.00;
	ui_label = "Vignette Amount";
	ui_tooltip = "Intensity of vignette effect.";
    ui_category = "Vignette";
> = 1.00;

uniform float LIGHTROOM_VIGNETTE_CURVE <
	ui_type = "drag";
	ui_min = 0.00; ui_max = 10.00;
	ui_label = "Vignette Curve";
	ui_tooltip = "Curve of gradient between inner and outer radius. 1.0 means linear.";
    ui_category = "Vignette";
> = 1.00;

uniform int LIGHTROOM_VIGNETTE_BLEND_MODE <
	ui_type = "combo";
	ui_items = "Multiply\0Subtract\0Screen\0LumaPreserving\0";
	ui_tooltip = "Select between different ways of applying vignette";
    ui_label = "Vignette Blend Mode";
    ui_category = "Vignette";
> = 1;

/*=============================================================================
	Textures, Samplers, Globals
=============================================================================*/

#define RESHADE_QUINT_COMMON_VERSION_REQUIRE 200
#include "qUINT_common.fxh"

#if(ENABLE_HISTOGRAM == 1)
texture2D HistogramTex			{ Width = HISTOGRAM_BINS_NUM;   Height = 1;  			Format = RGBA16F;  	};
sampler2D sHistogramTex 		{ Texture = HistogramTex; };
#endif

texture2D LutTexInternal			{ Width = 4096;   Height = 64;  			Format = RGBA8;  	};
sampler2D sLutTexInternal 		{ Texture = LutTexInternal; };

/*=============================================================================
	Vertex Shader
=============================================================================*/

void VS_Lightroom(in uint id : SV_VertexID, out float4 position : SV_Position, out float2 uv : TEXCOORD0, out nointerpolation float huefactors[7] : TEXCOORD1)
{
	uv.x = (id == 2) ? 2.0 : 0.0;
	uv.y = (id == 1) ? 2.0 : 0.0;
	position = float4(uv * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);

	static const float originalHue[8] = {0.0,0.0833333333333,0.1666666666666,0.3333333333333,0.5,0.6666666666666,0.8333333333333,1.0};

	huefactors[0] 	= (LIGHTROOM_RED_HUESHIFT     > 0) ? lerp(originalHue[0], originalHue[1], LIGHTROOM_RED_HUESHIFT) 	: lerp(originalHue[7], originalHue[6], -LIGHTROOM_RED_HUESHIFT);
	huefactors[1] 	= (LIGHTROOM_ORANGE_HUESHIFT  > 0) ? lerp(originalHue[1], originalHue[2], LIGHTROOM_ORANGE_HUESHIFT)  : lerp(originalHue[1], originalHue[0], -LIGHTROOM_ORANGE_HUESHIFT);
	huefactors[2]	= (LIGHTROOM_YELLOW_HUESHIFT  > 0) ? lerp(originalHue[2], originalHue[3], LIGHTROOM_YELLOW_HUESHIFT)  : lerp(originalHue[2], originalHue[1], -LIGHTROOM_YELLOW_HUESHIFT);
	huefactors[3] 	= (LIGHTROOM_GREEN_HUESHIFT   > 0) ? lerp(originalHue[3], originalHue[4], LIGHTROOM_GREEN_HUESHIFT)   : lerp(originalHue[3], originalHue[2], -LIGHTROOM_GREEN_HUESHIFT);
	huefactors[4] 	= (LIGHTROOM_AQUA_HUESHIFT    > 0) ? lerp(originalHue[4], originalHue[5], LIGHTROOM_AQUA_HUESHIFT) 	: lerp(originalHue[4], originalHue[3], -LIGHTROOM_AQUA_HUESHIFT);
	huefactors[5] 	= (LIGHTROOM_BLUE_HUESHIFT    > 0) ? lerp(originalHue[5], originalHue[6], LIGHTROOM_BLUE_HUESHIFT) 	: lerp(originalHue[5], originalHue[4], -LIGHTROOM_BLUE_HUESHIFT);
	huefactors[6]	= (LIGHTROOM_MAGENTA_HUESHIFT > 0) ? lerp(originalHue[6], originalHue[7], LIGHTROOM_MAGENTA_HUESHIFT) : lerp(originalHue[6], originalHue[5], -LIGHTROOM_MAGENTA_HUESHIFT);
}

/*=============================================================================
	Functions
=============================================================================*/

struct CurvesStruct
{
	float2 levels;
	float exposure;
	float gamma;
	float contrast;
	float blacks;
	float shadows;
	float midtones;
	float highlights;
	float whites;
};

struct PaletteStruct
{
  	float hue[7];
	float saturation[7];
	float exposure[7];
};

struct VignetteStruct
{
	float2 ratio;
	float2 radii;
	float amount;
	float curve;
	int blend;
	bool debug;
};

CurvesStruct setup_curves()
{
	CurvesStruct Curves;
	Curves.levels = float2(LIGHTROOM_GLOBAL_BLACK_LEVEL, LIGHTROOM_GLOBAL_WHITE_LEVEL) * rcp(255.0);
	Curves.exposure = exp2(LIGHTROOM_GLOBAL_EXPOSURE);
	Curves.gamma = exp2(-LIGHTROOM_GLOBAL_GAMMA);
	Curves.contrast = LIGHTROOM_GLOBAL_CONTRAST;
	Curves.blacks = exp2(-LIGHTROOM_GLOBAL_BLACKS_CURVE);
	Curves.shadows = exp2(-LIGHTROOM_GLOBAL_SHADOWS_CURVE);
	Curves.midtones = exp2(-LIGHTROOM_GLOBAL_MIDTONES_CURVE);
	Curves.highlights = exp2(-LIGHTROOM_GLOBAL_HIGHLIGHTS_CURVE);
	Curves.whites = exp2(-LIGHTROOM_GLOBAL_WHITES_CURVE);
	return Curves;
}

PaletteStruct setup_palette()
{
	PaletteStruct Palette;
	Palette.hue[0] = LIGHTROOM_RED_HUESHIFT;
	Palette.hue[1] = LIGHTROOM_ORANGE_HUESHIFT;
	Palette.hue[2] = LIGHTROOM_YELLOW_HUESHIFT;
	Palette.hue[3] = LIGHTROOM_GREEN_HUESHIFT;
	Palette.hue[4] = LIGHTROOM_AQUA_HUESHIFT;
	Palette.hue[5] = LIGHTROOM_BLUE_HUESHIFT;
	Palette.hue[6] = LIGHTROOM_MAGENTA_HUESHIFT;
	Palette.saturation[0] = LIGHTROOM_RED_SATURATION;
	Palette.saturation[1] = LIGHTROOM_ORANGE_SATURATION;
	Palette.saturation[2] = LIGHTROOM_YELLOW_SATURATION;
	Palette.saturation[3] = LIGHTROOM_GREEN_SATURATION;
	Palette.saturation[4] = LIGHTROOM_AQUA_SATURATION;
	Palette.saturation[5] = LIGHTROOM_BLUE_SATURATION;
	Palette.saturation[6] = LIGHTROOM_MAGENTA_SATURATION;
	Palette.exposure[0] = LIGHTROOM_RED_EXPOSURE;
	Palette.exposure[1] = LIGHTROOM_ORANGE_EXPOSURE;
	Palette.exposure[2] = LIGHTROOM_YELLOW_EXPOSURE;
	Palette.exposure[3] = LIGHTROOM_GREEN_EXPOSURE;
	Palette.exposure[4] = LIGHTROOM_AQUA_EXPOSURE;
	Palette.exposure[5] = LIGHTROOM_BLUE_EXPOSURE;
	Palette.exposure[6] = LIGHTROOM_MAGENTA_EXPOSURE;
	return Palette;
}

VignetteStruct setup_vignette()
{
	VignetteStruct Vignette;
	Vignette.ratio = float2(LIGHTROOM_VIGNETTE_WIDTH,LIGHTROOM_VIGNETTE_HEIGHT);
	Vignette.radii = float2(LIGHTROOM_VIGNETTE_RADIUS_INNER, LIGHTROOM_VIGNETTE_RADIUS_OUTER);
	Vignette.amount = LIGHTROOM_VIGNETTE_AMOUNT;
	Vignette.curve = LIGHTROOM_VIGNETTE_CURVE;
	Vignette.blend = LIGHTROOM_VIGNETTE_BLEND_MODE;
	Vignette.debug = LIGHTROOM_VIGNETTE_SHOW_RADII;
	return Vignette;
}

float3 rgb_to_hcv(in float3 RGB)
{
	RGB = saturate(RGB);
	float Epsilon = 1e-10;
    	// Based on work by Sam Hocevar and Emil Persson
	float4 P = (RGB.g < RGB.b) ? float4(RGB.bg, -1.0, 2.0/3.0) : float4(RGB.gb, 0.0, -1.0/3.0);
	float4 Q = (RGB.r < P.x) ? float4(P.xyw, RGB.r) : float4(RGB.r, P.yzx);
	float C = Q.x - min(Q.w, Q.y);
	float H = abs((Q.w - Q.y) / (6 * C + Epsilon) + Q.z);
	return float3(H, C, Q.x);
}

float3 rgb_to_hsl(in float3 RGB)
{
	float3 HCV = rgb_to_hcv(RGB);
	float L = HCV.z - HCV.y * 0.5;
	float S = HCV.y / (1.0000001 - abs(L * 2 - 1));
	return float3(HCV.x, S, L);
}

float3 hsl_to_rgb(in float3 HSL)
{
	HSL = saturate(HSL);
	float3 RGB = saturate(float3(abs(HSL.x * 6.0 - 3.0) - 1.0,2.0 - abs(HSL.x * 6.0 - 2.0),2.0 - abs(HSL.x * 6.0 - 4.0)));
	float C = (1 - abs(2 * HSL.z - 1)) * HSL.y;
	return (RGB - 0.5) * C + HSL.z;
}

float linearstep(float lower, float upper, float value)
{
    return saturate((value-lower)/(upper-lower));
}

float3 get_function_graph(float2 coords, float F, float3 origcolor, float thickness)
{
	F -= coords.y;
	float DistanceField = abs(F) / length(float2(ddx(F) / ddx(coords.x), -1.0));
	return lerp(origcolor, 1 - origcolor, smoothstep(qUINT::PIXEL_SIZE.y*thickness, 0.0, DistanceField));
}

float3 get_vignette(float3 color, float2 uv, VignetteStruct v)
{
	float2 vign_uv = uv * 2 - 1;
	vign_uv -= vign_uv * v.ratio;
	float vign_gradient = length(vign_uv);
	float vignette = linearstep(v.radii.x, v.radii.y, vign_gradient);
	vignette = pow(vignette, v.curve + 1e-6) * v.amount;

	color = (v.blend == 0) ? color * saturate(1 - vignette) : color;
	color = (v.blend == 1) ? saturate(color - vignette.xxx) : color;
	color = (v.blend == 2) ? 1 - (1 - color) * (vignette + 1) : color;
	color = (v.blend == 3) ? color * saturate(lerp(1 - vignette * 2 , 1, dot(color, 0.333))) : color;

	//can't use the graph function here, as it's not a y=f(x) function (at least not a real one)
	if(v.debug)
	{
		float2 radii_sdf = abs(vign_gradient - v.radii);
		radii_sdf *= qUINT::PIXEL_SIZE.yy / fwidth(radii_sdf); 
		radii_sdf = saturate(1 - 200 * radii_sdf);

		color = lerp(color, float3(0.0,1.0,0.0), radii_sdf.x);
		color = lerp(color, float3(1.0,0.0,0.0), radii_sdf.y);
	}

	return color;
}

float curves(in float x, in CurvesStruct c)
{
	x = linearstep(c.levels.x, c.levels.y, x);
	x = saturate(pow(x * c.exposure, c.gamma));
	
	float blacks_mult   	= smoothstep(0.25, 0.00, x);
	float shadows_mult  	= smoothstep(0.00, 0.25, x) * smoothstep(0.50, 0.25, x);
	float midtones_mult 	= smoothstep(0.25, 0.50, x) * smoothstep(0.75, 0.50, x);
	float highlights_mult  	= smoothstep(0.50, 0.75, x) * smoothstep(1.00, 0.75, x);
	float whites_mult  		= smoothstep(0.75, 1.00, x);

	x = pow(x, exp2(blacks_mult * c.blacks
			      + shadows_mult * c.shadows
			      + midtones_mult * c.midtones
			      + highlights_mult * c.highlights
			      + whites_mult * c.whites 
			      - 1));

	x = lerp(x, x * x * (3 - 2 * x), c.contrast);
	return saturate(x);
}

void draw_lut(inout float3 color, in float2 vpos, in float tile_size, in float tile_amount, in float scroll)
{
	float2 pixelcoord = vpos.xy; // - 0.5;
	pixelcoord.x += scroll * BUFFER_WIDTH;

	if(pixelcoord.x < tile_size * tile_amount && pixelcoord.y < tile_size)
	{
		color.rg = frac(pixelcoord.xy / tile_size) - 0.5 / tile_size;
		color.rg /= 1.0 - rcp(tile_size);
		color.b  = floor(pixelcoord.x / tile_size)/(tile_amount - 1);
		color.rgb = floor(color.rgb * 255.0) / 255.0;
	}
}

void draw_lut_4096x64(inout float3 color, in float2 vpos)
{
	color.rgb = vpos.xyx / 64.0;
	color.rg = frac(color.rg) - 0.5 / 64.0;
	color.rg /= 1.0 - 1.0 / 64.0;
	color.b = floor(color.b) / (64.0 - 1);
}

void read_lut_4096x64(inout float3 color)
{
	float4 lut_coord;
	lut_coord.xyz = color.rgb * 63.0;
	lut_coord.xy = (lut_coord.xy + 0.5) / float2(4096.0, 64.0);
	lut_coord.x += floor(lut_coord.z) / 64.0;
	lut_coord.z = frac(lut_coord.z);
	lut_coord.w = lut_coord.x + 0.015625;

	color.rgb = lerp(tex2D(sLutTexInternal, lut_coord.xy).rgb, tex2D(sLutTexInternal, lut_coord.wy).rgb, lut_coord.z);
}

float3 palette(in float3 hsl_color, in PaletteStruct p, in float huefactors[7])
{
	float huemults[7] =
	{
	max(saturate(1.0 - abs((hsl_color.x -  0.0/12) * 12.0)), 	//red left side - need this due to 360 degrees -> 0 degrees
		saturate(1.0 - abs((hsl_color.x - 12.0/12) * 6.0))),	//red right side
    	saturate(1.0 - abs((hsl_color.x -  1.0/12) * 12.0)),	//orange both sides
    max(saturate(1.0 - abs((hsl_color.x -  2.0/12) * 12.0)) * step(hsl_color.x,2.0/12.0), //yellow left side - need this because hues are not evenly distributed around color wheel, it's 1/12 from orange to yellow but 1/6 from yellow to green
    	saturate(1.0 - abs((hsl_color.x -  2.0/12) * 6.0)) * step(2.0/12.0,hsl_color.x)), //yellow right side
    	saturate(1.0 - abs((hsl_color.x -  4.0/12) * 6.0)), //green both sides
    	saturate(1.0 - abs((hsl_color.x -  6.0/12) * 6.0)), //aqua both sides
    	saturate(1.0 - abs((hsl_color.x -  8.0/12) * 6.0)), //blue both sides
    	saturate(1.0 - abs((hsl_color.x - 10.0/12) * 6.0)) //magenta both sides
	};

	float3 tcolor = 0; 
	for(int i=0; i < 7; i++)
		tcolor += huemults[i] * hsl_to_rgb(float3(huefactors[i], saturate(hsl_color.y + hsl_color.y * p.saturation[i]), hsl_color.z * exp2(sqrt(hsl_color.y) * p.exposure[i] * (1 - hsl_color.z) * hsl_color.y)));

	return tcolor;
}

/*=============================================================================
	Pixel Shaders
=============================================================================*/

#if(ENABLE_HISTOGRAM == 1)
void PS_HistogramGenerate(float4 vpos : SV_Position, float2 uv : TEXCOORD, out float4 res : SV_Target0)
{
	res = 0;float4 coord = 0;
	coord.z = rcp(LIGHTROOM_HISTOGRAM_SAMPLES);

    float2 histogram_data = float2(HISTOGRAM_BINS_NUM, vpos.x) / LIGHTROOM_HISTOGRAM_SMOOTHNESS;

	[loop]
	for(int x = 0; x < LIGHTROOM_HISTOGRAM_SAMPLES; x++)
	{
		coord.y = 0;
		[loop]
		for(int y = 0; y < LIGHTROOM_HISTOGRAM_SAMPLES; y++)
		{
			res.xyz += saturate(1.0 - abs(tex2Dlod(qUINT::sBackBufferTex,coord).xyz * histogram_data.xxx - histogram_data.yyy));
			coord.y += coord.z;
		}
		coord.x += coord.z;
	}
	res.xyz /= LIGHTROOM_HISTOGRAM_SMOOTHNESS;
}
#endif

void PS_ProcessLUT(float4 vpos : SV_Position, float2 uv : TEXCOORD0, nointerpolation float huefactors[7] : TEXCOORD1, out float4 color : SV_Target0)
{
	//ReShade bug :( can't initialize structs the old fashioned/C way
	const CurvesStruct Curves = setup_curves();
	const PaletteStruct Palette = setup_palette();

	draw_lut_4096x64(color.rgb, vpos.xy);

	color.a = 1;

	color.r = curves(color.r, Curves);
	color.g = curves(color.g, Curves);
	color.b = curves(color.b, Curves);
	float3 hsl_color = rgb_to_hsl(color.rgb);
	color.rgb = LIGHTROOM_GLOBAL_TEMPERATURE > 0 ? lerp(color.rgb, hsl_to_rgb(float3(0.06111, 1.0, hsl_color.z)), LIGHTROOM_GLOBAL_TEMPERATURE) : lerp(color.rgb, hsl_to_rgb(float3(0.56111, 1.0, hsl_color.z)), -LIGHTROOM_GLOBAL_TEMPERATURE);
	color.rgb = LIGHTROOM_GLOBAL_TEMPERATURE > 0 ? lerp(color.rgb, hsl_to_rgb(float3(0.31111, 1.0, hsl_color.z)), LIGHTROOM_GLOBAL_TINT) : lerp(color.rgb, hsl_to_rgb(float3(0.81111, 1.0, hsl_color.z)), -LIGHTROOM_GLOBAL_TINT);
	hsl_color = rgb_to_hsl(color.rgb);
	hsl_color.y = saturate(hsl_color.y + hsl_color.y * LIGHTROOM_GLOBAL_SATURATION);
	hsl_color.y = pow(hsl_color.y,exp2(-LIGHTROOM_GLOBAL_VIBRANCE));
	hsl_color = saturate(hsl_color); 

	color.rgb = palette(hsl_color, Palette, huefactors);
}

void PS_ApplyLUT(float4 vpos : SV_Position, float2 uv : TEXCOORD0, nointerpolation float huefactors[7] : TEXCOORD1, out float4 color : SV_Target0)
{
	color = tex2D(qUINT::sBackBufferTex, uv);

	if(LIGHTROOM_ENABLE_LUT) 
		draw_lut(color.rgb, vpos.xy, LIGHTROOM_LUT_TILE_SIZE, LIGHTROOM_LUT_TILE_COUNT, LIGHTROOM_LUT_SCROLL);

	read_lut_4096x64(color.rgb);	
}

void PS_DisplayStatistics(float4 vpos : SV_Position, float2 uv : TEXCOORD0, nointerpolation float huefactors[7] : TEXCOORD1, out float4 res : SV_Target0)
{
	const CurvesStruct Curves = setup_curves();
	const VignetteStruct Vignette = setup_vignette();

	float4 color = tex2D(qUINT::sBackBufferTex,uv);
	if(LIGHTROOM_ENABLE_VIGNETTE) color.rgb = get_vignette(color.rgb, uv, Vignette);

	float2 vposfbl = float2(vpos.x, BUFFER_HEIGHT-vpos.y);
	float2 vposfbl_n = vposfbl / 255.0;

	color.rgb = (LIGHTROOM_ENABLE_CLIPPING_DISPLAY && dot(color.rgb, 1.0) >= 3.0) ? float3(1.0,0.0,0.0) : color.rgb;
	color.rgb = (LIGHTROOM_ENABLE_CLIPPING_DISPLAY && dot(color.rgb, 1.0) <= 0.0) ? float3(0.0,0.0,1.0) : color.rgb;

#if(ENABLE_HISTOGRAM == 1)
	if(LIGHTROOM_ENABLE_HISTOGRAM || LIGHTROOM_ENABLE_CURVE_DISPLAY)
	{
		float luma_curve = curves(vposfbl_n.x, Curves);
		float3 histogram = tex2Dlod(sHistogramTex, vposfbl_n.xyxy).xyz / (LIGHTROOM_HISTOGRAM_SAMPLES * LIGHTROOM_HISTOGRAM_SAMPLES) * LIGHTROOM_HISTOGRAM_HEIGHT;

		if(all(saturate(-vposfbl_n * vposfbl_n + vposfbl_n)))
		{
			color.rgb = LIGHTROOM_ENABLE_HISTOGRAM ? vposfbl_n.yyy < histogram.xyz : color.rgb;	
			color.rgb = LIGHTROOM_ENABLE_CURVE_DISPLAY ? get_function_graph(vposfbl_n.xy, luma_curve, color.rgb, 20.0) : color.rgb;
		}
	}
#else
	if(LIGHTROOM_ENABLE_CURVE_DISPLAY)
	{
		float luma_curve = curves(vposfbl_n.x, Curves);

		if(all(saturate(-vposfbl_n * vposfbl_n + vposfbl_n)))
			color.rgb = LIGHTROOM_ENABLE_CURVE_DISPLAY ? get_function_graph(vposfbl_n.xy, luma_curve, color.rgb, 20.0) : color.rgb;
	}
#endif

	res.xyz = color.xyz;
	res.w = 1.0;
}

/*=============================================================================
	Techniques
=============================================================================*/

technique Lightroom
< ui_tooltip = "                      >> qUINT::Lightroom <<\n\n"
			   "Lightroom is a color grading toolbox that offers a multitude\n"
			   "of features commonly found in color grading software.\n"
               "You can do deep color modifications, adjust contrast and levels,\n"
               "tweak color balance, view a histogram and bake the CC into a 3D LUT.\n"
               "\nLightroom is written by Marty McFly / Pascal Gilcher"; >
{
	pass PProcessLUT
	{
		VertexShader = VS_Lightroom;
		PixelShader = PS_ProcessLUT;
		RenderTarget = LutTexInternal;
	}
	pass PApplyLUT
	{
		VertexShader = VS_Lightroom;
		PixelShader = PS_ApplyLUT;
	}

	#if(ENABLE_HISTOGRAM == 1)
	pass PHistogramGenerate
	{
		VertexShader = PostProcessVS;
		PixelShader = PS_HistogramGenerate;
		RenderTarget = HistogramTex;
	}
	#endif
	pass PHistogram
	{
		VertexShader = VS_Lightroom;
		PixelShader = PS_DisplayStatistics;
	}
}
