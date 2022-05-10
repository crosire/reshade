//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// VR Toolkit Shaderset 
//
// external shader code, modified to work with the VR toolkit.
//
// by Alexandre Miguel Maia - Retrolux
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++


//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Helper Shader
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

// taken from DirectX-Graphics-Samples https://github.com/Microsoft/DirectX-Graphics-Samples/blob/master/MiniEngine/Core/Shaders/ColorSpaceUtility.hlsli
float3 ApplySRGBCurve( float3 x )
{
    // Approximately pow(x, 1.0 / 2.2)
    return x < 0.0031308 ? 12.92 * x : 1.055 * pow(x, 1.0 / 2.4) - 0.055;
}

// These functions avoid pow() to efficiently approximate sRGB with an error < 0.4%.
float3 ApplySRGBCurve_Fast( float3 x )
{
    return x < 0.0031308 ? 12.92 * x : 1.13005 * sqrt(x - 0.00228) - 0.13448 * x + 0.005719;
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Radial Mask check for VR
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
/*
from Holger Frydrych VR_CAS_Color.fx shader
*/

#if VRT_USE_CENTER_MASK //&& (VRT_SHARPENING_MODE != 0 || VRT_ANTIALIASING_MODE != 0) // && __RENDERER__ >= 0xa000 // If DX10 or higher

/**
* Smoothes the circular mask transition
*
* Mode:  0 - Disable mask smoothing
*        1 - Enable mask smoothing to reduce the visiblity of the edge of the mask
*/
#ifndef _MASK_SMOOTH
    #define _MASK_SMOOTH 1
#endif

/**
* Sets the circular mask work on the backbuffer that has both eyes merged into a single texture (left + Right)
* This is the default behaivor from reshade right now.
*
* Mode:  0 - Center Mask per eye
*        1 - Combined masks with a circle for the left eye and one for the right
*/
#ifndef _MASK_COMBINED_EYES
    #define _MASK_COMBINED_EYES 1
#endif


uniform int _CircularMaskHelp <
	ui_category = "Circular Masking Mask"; 
	ui_type = "radio"; 
	ui_label = " ";
	ui_text = "NOTE: Adjust \"Circle Radius\" for your VR headset below";
>; 

uniform float CircularMaskSize <
    ui_category = "Circular Masking Mask";
    ui_type = "slider";
    ui_label = "Circle Radius (?)";
    ui_tooltip = "Adjusts the size of the circle from the center of the screen towards the edges.\n"
                 "This can slightly improve the performance the smaller the radius is.\n\n"
                 "";
    ui_min = 0.0; ui_max = 1.0; ui_step = 0.01;
> = 0.30;

#if _MASK_SMOOTH
uniform float CircularMaskSmoothness <
    ui_category = "Circular Masking Mask";
    ui_type = "slider";
    ui_label = "Edge Sharpness";
    ui_tooltip = "Adjusts the edge of the circular mask to allow smaller radius while reducing the prominence of the edge";
    ui_min = 1.0; ui_max = 10; ui_step = 0.1;
> = 5.0;
#endif

uniform float CircularMaskHorizontalOffset <
    ui_category = "Circular Masking Mask";
    ui_type = "slider";
    ui_label = "Horizional offset";
    ui_tooltip = "Adjusts the mask offset from the center horizontally";
    ui_min = 0.3; ui_max = 0.5; ui_step = 0.001;
> = 0.3;

uniform bool CircularMaskPreview < __UNIFORM_INPUT_BOOL1
	ui_category = "Circular Masking Mask";    
    ui_label = "Mask Preview";
	ui_tooltip = "Preview circular mask for easy adjustments.";
> = false;


float CircularMask( float2 texcoord )
{
    float2 fromCenter;
   
    #if _MASK_COMBINED_EYES
        fromCenter = float2(texcoord.x < 0.5 ? CircularMaskHorizontalOffset :  1 - CircularMaskHorizontalOffset, 0.5) - texcoord;
    #else
        fromCenter = float2(0.5, 0.5) - texcoord;
    #endif

    //correct aspect ratio of mask circle                   
    fromCenter.x *= ReShade::AspectRatio;

    float distSqr = dot(fromCenter, fromCenter);

    // just apply sharpened image when inside the center mask
    float maskSizeSqr = CircularMaskSize * CircularMaskSize;
    if (distSqr < maskSizeSqr){
        #if _MASK_SMOOTH
            float diff = (distSqr/maskSizeSqr);
            return 1 - pow(diff,CircularMaskSmoothness);
        #else
            return 1;
        #endif
    } else{
        return 0;
    }
}

#endif

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Sharpen Shader (FilmicAnamorphSharpen.fx)
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
/*
Filmic Anamorph Sharpen PS v1.4.1 (c) 2018 Jakub Maximilian Fober

This work is licensed under the Creative Commons
Attribution-ShareAlike 4.0 International License.
To view a copy of this license, visit
http://creativecommons.org/licenses/by-sa/4.0/.
*/

#if (VRT_SHARPENING_MODE == 1)

uniform float FAS_Strength < __UNIFORM_SLIDER_FLOAT1
	ui_label = "Strength";
    ui_category = "Sharpening (MODE 1: Filmic Anamorph Sharpen)";
    ui_category_closed = false;
	ui_min = 0.0; ui_max = 250; ui_step = 1;
> = 125;

uniform float FAS_Radius < __UNIFORM_SLIDER_FLOAT1
    ui_label = "Radius";
    ui_tooltip = "High-pass cross offset in pixels";
    ui_category = "Sharpening (MODE 1: Filmic Anamorph Sharpen)";
    ui_min = 0.0; ui_max = 2.0; ui_step = 0.01;
> = 0.10;

uniform float FAS_Clamp < __UNIFORM_SLIDER_FLOAT1
    ui_label = "Clamping";
    ui_category = "Sharpening (MODE 1: Filmic Anamorph Sharpen)";
    ui_min = 0.5; ui_max = 1.0; ui_step = 0.001;
> = 0.525;

uniform bool FAS_Preview < __UNIFORM_INPUT_BOOL1
    ui_label = "Preview sharpen layer";
	ui_category = "Sharpening (MODE 1: Filmic Anamorph Sharpen)";    
	ui_tooltip = "Preview sharpen layer and mask for adjustment.\n";
    
> = false;



// RGB to YUV709 Luma
static const float3 LumaCoefficient = float3(0.2126, 0.7152, 0.0722);

// Overlay blending mode
float Overlay(float LayerA, float LayerB)
{
    float MinA = min(LayerA, 0.5);
    float MinB = min(LayerB, 0.5);
    float MaxA = max(LayerA, 0.5);
    float MaxB = max(LayerB, 0.5);
    return 2.0 * (MinA * MinB + MaxA + MaxB - MaxA * MaxB) - 1.5;
}


// Overlay blending mode with rgb values
float3 Overlay3(float3 LayerA, float3 LayerB)
{
    float3 MinA = min(LayerA, 0.5);
    float3 MinB = min(LayerB, 0.5);
    float3 MaxA = max(LayerA, 0.5);
    float3 MaxB = max(LayerB, 0.5);
    return 2.0 * (MinA * MinB + MaxA + MaxB - MaxA * MaxB) - 1.5;
}


// Overlay blending mode for one input
float Overlay(float LayerAB)
{
    float MinAB = min(LayerAB, 0.5);
    float MaxAB = max(LayerAB, 0.5);
    return 2.0 * (MinAB * MinAB + MaxAB + MaxAB - MaxAB * MaxAB) - 1.5;
}

// Sharpen pass
float3 FilmicAnamorphSharpenPS(float4 backBuffer, float4 pos : SV_Position, float2 UvCoord : TEXCOORD0) : COLOR
{
  
	// TODO can be set to a fixed value?
    // Get pixel size
	float2 Pixel = BUFFER_PIXEL_SIZE * FAS_Radius;

    float2 NorSouWesEst[4] = {
        float2(UvCoord.x, UvCoord.y + Pixel.y),
        float2(UvCoord.x, UvCoord.y - Pixel.y),
        float2(UvCoord.x + Pixel.x, UvCoord.y),
        float2(UvCoord.x - Pixel.x, UvCoord.y) 
    };

	// holds the sum of the pixel samples
	float3 neighborPixelSamples = float3(0,0,0);
	
	[unroll]
    for(int s = 0; s < 4; s++)
        neighborPixelSamples += tex2D(backBufferSamplerScalable, NorSouWesEst[s]).rgb;
  
	//calculate highpass 
	float HighPassColor = dot((neighborPixelSamples * 0.25) - backBuffer.rgb, LumaCoefficient);
 
 	// Sharpen strength
	HighPassColor *= FAS_Strength;
	
	// Offset for the mids to be at 50% grey
	HighPassColor = 0.5 - HighPassColor;
 
    // Clamping sharpen
    HighPassColor = max(min(HighPassColor, FAS_Clamp), 1.0 - FAS_Clamp);
	
    // Preview mode ON
    return FAS_Preview ? HighPassColor : saturate(Overlay3(backBuffer.rgb,HighPassColor.rrr));
}
#endif

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// AMD FidelityFX Contrast Adaptive Sharpening (CAS.fx)
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
/* Copyright (c) 2017-2019 Advanced Micro Devices, Inc. All rights reserved. */

#if (VRT_SHARPENING_MODE == 2)

uniform float CAS_Contrast <
	ui_type = "slider";
    ui_label = "Contrast Adaptation";
    ui_tooltip = "Adjusts the range the shader adapts to high contrast (0 is not all the way off).  Higher values = more high contrast sharpening.";
    ui_category = "Sharpening (MODE 2: CAS)";
    ui_category_closed = false;
	ui_min = 0.0; ui_max = 1.0;  ui_step = 0.01;
> = 0.5;

uniform float CAS_Sharpening <
	ui_type = "slider";
    ui_label = "Sharpening intensity";
    ui_tooltip = "Adjusts sharpening intensity by averaging the original pixels to the sharpened result.  1.0 is the unmodified default.";
    ui_category = "Sharpening (MODE 2: CAS)";
	ui_min = 0.0; ui_max = 5.0; ui_step = 0.01;
> = 1.5;

uniform float CAS_Contrast_Clamp <
    ui_category = "Sharpening (MODE 2: CAS)";  
    ui_type = "slider";
    ui_label = "Contrast Clamping";
    ui_tooltip = "Limits the bright & contrasty parts from beeing sharpened. Lower the value to reduce shimmering on bright lines";
    ui_min = 0.0; ui_max = 1.0; ui_step = 0.01;
> = 0.10;

uniform bool CAS_Preview < __UNIFORM_INPUT_BOOL1
    ui_label = "Preview sharpen layer";
	ui_category = "Sharpening (MODE 2: CAS)";  
	ui_tooltip = "Preview sharpen layer and mask for adjustment.\n";
    
> = false;

// Without CAS_BETTER_DIAGONALS defined, the algorithm is a little faster.
// Instead of using the 3x3 "box" with the 5-tap "circle" this uses just the "circle".
#ifndef CAS_BETTER_DIAGONALS
	#define CAS_BETTER_DIAGONALS 0
#endif

// RGB to YUV709 Luma
static const float2 pixelOffset = BUFFER_PIXEL_SIZE * 0.5;

float3 CASPass(float4 backBuffer, float4 vpos : SV_Position, float2 texcoord : TexCoord) : COLOR
{
	// fetch a 3x3 neighborhood around the pixel 'e',
	//  a b c
	//  d(e)f
	//  g h i
	
	float3 b = tex2Doffset(backBufferSampler, texcoord, int2(0, -1)).rgb;
	float3 d = tex2Doffset(backBufferSampler, texcoord, int2(-1, 0)).rgb;
 
#if __RENDERER__ >= 0xa000 // If DX10 or higher
	float4 red_efhi = tex2DgatherR(backBufferSampler, texcoord + pixelOffset);
	
	//float3 e = float3( red_efhi.w, red_efhi.w, red_efhi.w);
	float3 e = backBuffer.rgb;
	float3 f = float3( red_efhi.z, red_efhi.z, red_efhi.z);
	float3 h = float3( red_efhi.x, red_efhi.x, red_efhi.x);
	float3 i = float3( red_efhi.y, red_efhi.y, red_efhi.y);
	
	float4 green_efhi = tex2DgatherG(backBufferSampler, texcoord + pixelOffset);
	
	//e.g = green_efhi.w;
	f.g = green_efhi.z;
	h.g = green_efhi.x;
	i.g = green_efhi.y;
	
	float4 blue_efhi = tex2DgatherB(backBufferSampler, texcoord + pixelOffset);
	
	//e.b = blue_efhi.w;
	f.b = blue_efhi.z;
	h.b = blue_efhi.x;
	i.b = blue_efhi.y;


#else // If DX9
	float3 e = backBuffer.rgb;
	float3 f = tex2Doffset(backBufferSampler, texcoord, int2(1, 0)).rgb;

	float3 h = tex2Doffset(backBufferSampler, texcoord, int2(0, 1)).rgb;
	float3 i = tex2Doffset(backBufferSampler, texcoord, int2(1, 1)).rgb;

#endif

	float3 g = tex2Doffset(backBufferSampler, texcoord, int2(-1, 1)).rgb; 
	float3 a = tex2Doffset(backBufferSampler, texcoord, int2(-1, -1)).rgb;
	float3 c = tex2Doffset(backBufferSampler, texcoord, int2(1, -1)).rgb;
   

	// Soft min and max.
	//  a b c             b
	//  d e f * 0.5  +  d e f * 0.5
	//  g h i             h
    // These are 2.0x bigger (factored out the extra multiply).
    float3 mnRGB = min(min(min(d, e), min(f, b)), h);
#if CAS_BETTER_DIAGONALS
    float3 mnRGB2 = min(mnRGB, min(min(a, c), min(g, i)));
    mnRGB += mnRGB2;
#endif

    float3 mxRGB = max(max(max(d, e), max(f, b)), h);
#if CAS_BETTER_DIAGONALS
    float3 mxRGB2 = max(mxRGB, max(max(a, c), max(g, i)));
    mxRGB += mxRGB2;
#endif

	// Reduce strong contrast elements from being sharpened to reduce aliasing artifacts
	if(mxRGB.g - mnRGB.g > CAS_Contrast_Clamp){
		mnRGB = 0;
	}

	// Smooth minimum distance to signal limit divided by smooth max.
    float3 rcpMRGB = rcp(mxRGB);

#if CAS_BETTER_DIAGONALS
	float3 ampRGB = saturate(min(mnRGB, 2.0 - mxRGB) * rcpMRGB);
#else
	float3 ampRGB = saturate(min(mnRGB, 1.0 - mxRGB) * rcpMRGB);
#endif

	// Shaping amount of sharpening.
    ampRGB = rsqrt(ampRGB);
        
    float peak = -3.0 * CAS_Contrast + 8.0;
    float3 wRGB = -rcp(ampRGB * peak);

    float3 rcpWeightRGB = rcp(4.0 * wRGB + 1.0);

    //                          0 w 0
    //  Filter shape:           w 1 w
    //                          0 w 0
    float3 window = (b + d) + (f + h);
    float3 outColor = (window * wRGB + e) * rcpWeightRGB;
    
    // saturate the end result to avoid artifacts 
	return CAS_Preview ? outColor - e : saturate(lerp(e, outColor, CAS_Sharpening));
	
}

#endif


//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// LUT shader (LUT.fx)
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
/*
Marty's LUT shader 1.0 for ReShade 3.0
Copyright © 2008-2016 Marty McFly
*/

#if (VRT_COLOR_CORRECTION_MODE == 1)

#ifndef LUT_TextureName
    #define LUT_TextureName "lut.png"
#endif

#ifndef _fLUT_TileSizeXY
    #define _fLUT_TileSizeXY 32
#endif

#ifndef _fLUT_TileAmount
    #define _fLUT_TileAmount 32
#endif

// texel size of the lut
static const float2 LUT_TEXEL_SIZE = float2(1.0 /_fLUT_TileSizeXY / _fLUT_TileAmount, 1.0 /_fLUT_TileSizeXY);

texture texLUT < source = LUT_TextureName; > { Width = _fLUT_TileSizeXY*_fLUT_TileAmount; Height = _fLUT_TileSizeXY; Format = RGBA8; };
sampler	SamplerLUT 	{ Texture = texLUT; };

uniform int _VRT_ColorCorrectionMode1 <
	ui_category = "Color Correction (MODE 1: LUT)"; 
	ui_type = "radio"; 
	ui_label= " ";
	ui_text =
		"NOTE: For the LUT to work you need to define a proper lut texture in\n"
        "the \"Preprocessor definitions\" other than the default \"lut.png\"!";
>;

uniform float LUT_AmountChroma < __UNIFORM_SLIDER_FLOAT1
    ui_min = 0.00; ui_max = 1.00;
    ui_label = "LUT chroma amount";
    ui_tooltip = "Intensity of color/chroma change of the LUT.";
    ui_category = "Color Correction (MODE 1: LUT)";
> = 1.00;

uniform float LUT_AmountLuma < __UNIFORM_SLIDER_FLOAT1
    ui_min = 0.00; ui_max = 1.00;
    ui_label = "LUT luma amount";
    ui_tooltip = "Intensity of luma change of the LUT.";
    ui_category = "Color Correction (MODE 1: LUT)";
> = 1.00;

float3 PS_LUT_Apply(float4 backbuffer, float4 vpos : SV_Position, float2 texcoord : TEXCOORD) : COLOR
{
    float3 color = backbuffer.rgb;
    
    float3 lutcoord = float3((color.xy*_fLUT_TileSizeXY-color.xy+0.5)*LUT_TEXEL_SIZE.xy,color.z*_fLUT_TileSizeXY-color.z);
    float lerpfact = frac(lutcoord.z);
    lutcoord.x += (lutcoord.z-lerpfact)*LUT_TEXEL_SIZE.y;

    float3 lutcolor = lerp(tex2D(SamplerLUT, lutcoord.xy).xyz, tex2D(SamplerLUT, float2(lutcoord.x+LUT_TEXEL_SIZE.y,lutcoord.y)).xyz,lerpfact);

   // only process linear interpolation when needed 
   if( LUT_AmountChroma != 1 || LUT_AmountLuma != 1){
     	color.xyz = lerp(normalize(color.xyz), normalize(lutcolor.xyz), LUT_AmountChroma) *
    	            lerp(length(color.xyz),    length(lutcolor.xyz),    LUT_AmountLuma);
    	
        return color.xyz;    
	}else{
    	return lutcolor;
    }
}
#endif


//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Contrast & Saturation
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
/**
 * Uses elements from Curves and own extensions 
 * by Christian Cann Schuldt Jensen ~ CeeJay.dk
 */

#if (VRT_COLOR_CORRECTION_MODE == 2)

#ifndef _CURVES_FORMULA
    #define _CURVES_FORMULA 3
#endif

// Luma weight based on human color perception
static const float3 LUMINOSITY_WEIGHT = float3(0.2126, 0.7152, 0.0722);  

static const float PI = 3.1415927;

uniform float CS_Contrast < __UNIFORM_SLIDER_FLOAT1
 	ui_label = "Contrast";
	ui_min = -1.0; ui_max = 1.0;
	ui_tooltip = "The amount of contrast you want.";
  	ui_category = "Color Correction (MODE 2: Contrast & Saturation)";
> = 0.0;

uniform float CS_Saturation < __UNIFORM_SLIDER_FLOAT1
	ui_label = "Saturation";
	ui_min = 0.0; ui_max = 2.0;ui_step = 0.01;
	ui_tooltip = "Adjust saturation";
    ui_category = "Color Correction (MODE 2: Contrast & Saturation)";
> = 1.0;

float3 ContrastColorPass(float4 backBuffer, float4 position : SV_Position, float2 texcoord : TexCoord) : COLOR
{
	float3 color = backBuffer.rgb;

	// caluclate luma (grayscale)
	float luma = dot(color, LUMINOSITY_WEIGHT);
	
	// Adjust Contrast only when needed
	if(CS_Contrast != 0){
	
		// calculate chroma (colors - grayscale) 
		float3 chroma = color.rgb - luma;
		
		// copy luma for curve formula
		float x = luma;
		
		// Contrast
		#if _CURVES_FORMULA == 1
			// -- Curve 1 --
			x = sin(PI * 0.5 * x); // Sin - 721 amd fps, +vign 536 nv
			x *= x;
		#elif _CURVES_FORMULA == 2
			// -- Curve 2 --
			x = x - 0.5;
			x = (x / (0.5 + abs(x))) + 0.5;
		#elif _CURVES_FORMULA == 3 
			// -- Curve 3 --
			x = x*x*(3.0 - 2.0*x); //faster smoothstep alternative - 776 amd fps, +vign 536 nv
		#endif
	
		// apply contrast to luma
		luma = lerp(luma, x, CS_Contrast);
		// re-apply chroma to the final color
		color.rgb = luma + chroma;
	}

	// Adjust saturation 
	color.rgb = lerp(luma.rrr, color.rgb, CS_Saturation);

	return color;
}
#endif

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Simple Dithering shader
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
/*
 from Valve Advanced VR GDC 2015 presentation
*/

#if VRT_DITHERING 

uniform float DitheringStrength <
    ui_category = "Dithering";
    ui_type = "slider";
    ui_label = "Dithering Strength";
    ui_tooltip = "Adjusts the dithering strength to reduce banding artifacts";
    ui_min = 0.00; ui_max = 1.0;ui_step = 0.001;
> = 0.375;

uniform float timer < source = "timer"; >;

float3 ScreenSpaceDither(float2 vScreenPos: SV_Position)
{
    //Iestyn's RGB dither (7 asm instructions) from Portal2 X360 , slightly modified for VR
    float3 vDither=dot(float2(171.0,231.0),vScreenPos.xy + (timer * 0.001)).xxx;
    vDither.rgb = frac(vDither.rgb/float3(103.0,71.0,97.0)) - float3(0.5,0.5,0.5);
    return (vDither.rgb/255) * DitheringStrength;

}
#endif

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// FXAA 3.11 Shader
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
/*  NVIDIA FXAA 3.11 by TIMOTHY LOTTES */


#if (VRT_ANTIALIASING_MODE == 1)

uniform float Subpix < __UNIFORM_SLIDER_FLOAT1
	ui_category = "Antialiasing (MODE 1: FXAA)"; 
	ui_min = 0.0; ui_max = 1.0;ui_step = 0.05;
	ui_tooltip = "Amount of sub-pixel aliasing removal. Higher values makes the image softer/blurrier.";
> = 0.5;

uniform float EdgeThreshold < __UNIFORM_SLIDER_FLOAT1
	ui_category = "Antialiasing (MODE 1: FXAA)"; 
	ui_min = 0.0; ui_max = 1.0;ui_step = 0.005;
	ui_label = "Edge Detection Threshold";
	ui_tooltip = "The minimum amount of local contrast required to apply algorithm.";
> = 0.5;
uniform float EdgeThresholdMin < __UNIFORM_SLIDER_FLOAT1
	ui_category = "Antialiasing (MODE 1: FXAA)"; 
	ui_min = 0.0; ui_max = 1.0;ui_step = 0.01;
	ui_label = "Darkness Threshold";
	ui_tooltip = "Pixels darker than this are not processed in order to increase performance.";
> = 0.0;

//------------------------------ Non-GUI-settings -------------------------------------------------

#ifndef FXAA_QUALITY__PRESET
	// Valid Quality Presets
	// 10 to 15 - default medium dither (10=fastest, 15=highest quality)
	// 20 to 29 - less dither, more expensive (20=fastest, 29=highest quality)
	// 39       - no dither, very expensive
	#define FXAA_QUALITY__PRESET 12
#endif


#ifndef FXAA_GREEN_AS_LUMA
	#define FXAA_GREEN_AS_LUMA 1
#endif

//-------------------------------------------------------------------------------------------------

#if (__RENDERER__ == 0xb000 || __RENDERER__ == 0xb100 || __RENDERER__ >= 0x10000)
	#define FXAA_GATHER4_ALPHA 1
	#if (__RESHADE__ < 40800)
		#define FxaaTexAlpha4(t, p) tex2Dgather(t, p, 3)
		#define FxaaTexOffAlpha4(t, p, o) tex2Dgatheroffset(t, p, o, 3)
		#define FxaaTexGreen4(t, p) tex2Dgather(t, p, 1)
		#define FxaaTexOffGreen4(t, p, o) tex2Dgatheroffset(t, p, o, 1)
	#else
		#define FxaaTexAlpha4(t, p) tex2DgatherA(t, p)
		#define FxaaTexOffAlpha4(t, p, o) tex2DgatherA(t, p, o)
		#define FxaaTexGreen4(t, p) tex2DgatherG(t, p)
		#define FxaaTexOffGreen4(t, p, o) tex2DgatherG(t, p, o)
	#endif
#endif

#define FXAA_PC 1
#define FXAA_HLSL_3 1

#include "FXAA.fxh"


float4 FXAAPixelShader(float4 vpos : SV_Position, float2 texcoord : TEXCOORD) : COLOR
{
	return FxaaPixelShader(
		texcoord, // pos
		0, // fxaaConsolePosPos
		backBufferSamplerScalable, // tex
		backBufferSamplerScalable, // fxaaConsole360TexExpBiasNegOne
		backBufferSamplerScalable, // fxaaConsole360TexExpBiasNegTwo
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
#endif
