//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// ReShade effect file
// visit facebook.com/MartyMcModding for news/updates
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Marty's Lightroom shader 1.3.2 for ReShade 3.0
// Copyright © 2008-2016 Marty McFly
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

uniform bool bLightroom_LookupEnable <
	ui_label = "Enable LUT Overlay";
	ui_tooltip = "This enables a 256x16 3D LUT, all changes of this shader are applied in it.\nTaking a screenshot and using the cropped LUT with the TuningPalette\n will reproduce all changes of this shader.";
> = false;

uniform int iLightroom_LookupTileSizeXY <
	ui_type = "drag";
	ui_min = 8; ui_max = 64;
	ui_label = "LUT tile size";
	ui_tooltip = "This controls the XY size of tiles of the LUT (which is accuracy in red/green channel).";
> = 16;

uniform int iLightroom_LookupTileCount <
	ui_type = "drag";
	ui_min = 8; ui_max = 64;
	ui_label = "LUT tile count";
	ui_tooltip = "This controls the amount of tiles of the LUT (which is accuracy in blue channel).\nBe aware that Tile Size XY * Tile Amount is the width of the LUT and if this number\nis larger than your resolution width, the LUT won't fit on your screen.";
> = 16;

uniform int iLightroom_LookupScroll <
	ui_type = "drag";
	ui_min = 0; ui_max = 5;
	ui_label = "LUT scroll";
	ui_tooltip = "If your LUT size exceeds your screen width, set this to 0, take screenshot, set it to 1, take screenshot\netc until you  reach the end of your LUT and assemble the screenshots like a panorama.\nIf your LUT fits the screen size however, leave it at 0.";
> = 0;

uniform bool bLightroom_ColorPickerEnable <
	ui_label = "Enable Color Picker";
	ui_tooltip = "This enables a small overlay with color at mouse position.";
> = false;

uniform bool bLightroom_HistogramEnable <
	ui_label = "Enable Histogram";
	ui_tooltip = "This enables a small overlay with a histogram for monitoring purposes.\nFor higher performance, open shader and set iHistogramBins to a lower value.";
> = false;

uniform bool bLightroom_LumaCurveEnable <
	ui_label = "Enable Luma Curve";
	ui_tooltip = "This enables a small overlay with a luma curve\nso you can monitor changes made by exposure, levels etc.";
> = false;

uniform int iLightroom_HistogramSamplesSqrt <
	ui_type = "drag";
	ui_min = 2; ui_max = 32;
	ui_label = "Histogram Samples";
	ui_tooltip = "The amount of samples, 20 means 20x20 samples distributed on the screen.\nHigher means a more accurate histogram depicition and less temporal noise.";
> = 20;

uniform float fLightroom_HistogramHeight <
	ui_type = "drag";
	ui_min = 1.0; ui_max = 20.00;
	ui_label = "Histogram Curve Height";
	ui_tooltip = "Raises the Histogram curve if the values are highly distributed and not visible very well.";
> = 5.00;

uniform float fLightroom_HistogramSmoothness <
	ui_type = "drag";
	ui_min = 1.0; ui_max = 10.00;
	ui_label = "Histogram Curve Smoothness";
	ui_tooltip = "Smoothens the Histogram curve for a more temporal coherent result.";
> = 5.00;

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

uniform float fLightroom_Red_Hueshift <
	ui_type = "drag";
	ui_min = -1.00; ui_max = 1.00;
	ui_label = "Red Hue Control";
	ui_tooltip = "Magenta <= ... Red ... => Orange";
> = 0.00;

uniform float fLightroom_Orange_Hueshift <
	ui_type = "drag";
	ui_min = -1.00; ui_max = 1.00;
	ui_label = "Orange Hue Control";
	ui_tooltip = "Red <= ... Orange ... => Yellow";
> = 0.00;

uniform float fLightroom_Yellow_Hueshift <
	ui_type = "drag";
	ui_min = -1.00; ui_max = 1.00;
	ui_label = "Yellow Hue Control";
	ui_tooltip = "Orange <= ... Yellow ... => Green";
> = 0.00;

uniform float fLightroom_Green_Hueshift <
	ui_type = "drag";
	ui_min = -1.00; ui_max = 1.00;
	ui_label = "Green Hue Control";
	ui_tooltip = "Yellow <= ... Green ... => Aqua";
> = 0.00;

uniform float fLightroom_Aqua_Hueshift <
	ui_type = "drag";
	ui_min = -1.00; ui_max = 1.00;
	ui_label = "Aqua Hue Control";
	ui_tooltip = "Green <= ... Aqua ... => Blue";
> = 0.00;

uniform float fLightroom_Blue_Hueshift <
	ui_type = "drag";
	ui_min = -1.00; ui_max = 1.00;
	ui_label = "Blue Hue Control";
	ui_tooltip = "Aqua <= ... Blue ... => Magenta";
> = 0.00;

uniform float fLightroom_Magenta_Hueshift <
	ui_type = "drag";
	ui_min = -1.00; ui_max = 1.00;
	ui_label = "Magenta Hue Control";
	ui_tooltip = "Blue <= ... Magenta ... => Red";
> = 0.00;

//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

uniform float fLightroom_Red_Exposure <
	ui_type = "drag";
	ui_min = -1.00; ui_max = 1.00;
	ui_label = "Red Exposure";
	ui_tooltip = "Exposure control of Red colors";
> = 0.00;

uniform float fLightroom_Orange_Exposure <
	ui_type = "drag";
	ui_min = -1.00; ui_max = 1.00;
	ui_label = "Orange Exposure";
	ui_tooltip = "Exposure control of Orange colors";
> = 0.00;

uniform float fLightroom_Yellow_Exposure <
	ui_type = "drag";
	ui_min = -1.00; ui_max = 1.00;
	ui_label = "Yellow Exposure";
	ui_tooltip = "Exposure control of Yellow colors";
> = 0.00;

uniform float fLightroom_Green_Exposure <
	ui_type = "drag";
	ui_min = -1.00; ui_max = 1.00;
	ui_label = "Green Exposure";
	ui_tooltip = "Exposure control of Green colors";
> = 0.00;

uniform float fLightroom_Aqua_Exposure <
	ui_type = "drag";
	ui_min = -1.00; ui_max = 1.00;
	ui_label = "Aqua Exposure";
	ui_tooltip = "Exposure control of Aqua colors";
> = 0.00;

uniform float fLightroom_Blue_Exposure <
	ui_type = "drag";
	ui_min = -1.00; ui_max = 1.00;
	ui_label = "Blue Exposure";
	ui_tooltip = "Exposure control of Blue colors";
> = 0.00;

uniform float fLightroom_Magenta_Exposure <
	ui_type = "drag";
	ui_min = -1.00; ui_max = 1.00;
	ui_label = "Magenta Exposure";
	ui_tooltip = "Exposure control of Magenta colors";
> = 0.00;

//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

uniform float fLightroom_Red_Saturation <
	ui_type = "drag";
	ui_min = -1.00; ui_max = 1.00;
	ui_label = "Red Saturation";
	ui_tooltip = "Saturation control of Red colors";
> = 0.00;

uniform float fLightroom_Orange_Saturation <
	ui_type = "drag";
	ui_min = -1.00; ui_max = 1.00;
	ui_label = "Orange Saturation";
	ui_tooltip = "Saturation control of Orange colors";
> = 0.00;

uniform float fLightroom_Yellow_Saturation <
	ui_type = "drag";
	ui_min = -1.00; ui_max = 1.00;
	ui_label = "Yellow Saturation";
	ui_tooltip = "Saturation control of Yellow colors";
> = 0.00;

uniform float fLightroom_Green_Saturation <
	ui_type = "drag";
	ui_min = -1.00; ui_max = 1.00;
	ui_label = "Green Saturation";
	ui_tooltip = "Saturation control of Green colors";
> = 0.00;

uniform float fLightroom_Aqua_Saturation <
	ui_type = "drag";
	ui_min = -1.00; ui_max = 1.00;
	ui_label = "Aqua Saturation";
	ui_tooltip = "Saturation control of Aqua colors";
> = 0.00;

uniform float fLightroom_Blue_Saturation <
	ui_type = "drag";
	ui_min = -1.00; ui_max = 1.00;
	ui_label = "Blue Saturation";
	ui_tooltip = "Saturation control of Blue colors";
> = 0.00;

uniform float fLightroom_Magenta_Saturation <
	ui_type = "drag";
	ui_min = -1.00; ui_max = 1.00;
	ui_label = "Magenta Saturation";
	ui_tooltip = "Saturation control of Magenta colors";
> = 0.00;

//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

uniform float fLightroom_GlobalBlackLevel <
	ui_type = "drag";
	ui_min = 0; ui_max = 512;
	ui_step = 1;
	ui_label = "Global Black Level";
	ui_tooltip = "Scales input HSV value. Everything darker than this is mapped to black.";
> = 5.00;

uniform float fLightroom_GlobalWhiteLevel <
	ui_type = "drag";
	ui_min = 0; ui_max = 512;
	ui_step = 1;
	ui_label = "Global White Level";
	ui_tooltip = "Scales input HSV value. ";
> = 255.00;

uniform float fLightroom_GlobalExposure <
	ui_type = "drag";
	ui_min = -1.00; ui_max = 1.00;
	ui_label = "Global Exposure";
	ui_tooltip = "Global Exposure Control";
> = 0.00;

uniform float fLightroom_GlobalGamma <
	ui_type = "drag";
	ui_min = -1.00; ui_max = 1.00;
	ui_label = "Global Gamma";
	ui_tooltip = "Global Gamma Control";
> = 0.00;

uniform float fLightroom_GlobalBlacksCurve <
	ui_type = "drag";
	ui_min = -1.00; ui_max = 1.00;
	ui_label = "Global Blacks Curve";
	ui_tooltip = "Global Blacks Curve Control";
> = 0.00;

uniform float fLightroom_GlobalShadowsCurve <
	ui_type = "drag";
	ui_min = -1.00; ui_max = 1.00;
	ui_label = "Global Shadows Curve";
	ui_tooltip = "Global Shadows Curve Control";
> = 0.00;

uniform float fLightroom_GlobalMidtonesCurve <
	ui_type = "drag";
	ui_min = -1.00; ui_max = 1.00;
	ui_label = "Global Midtones Curve";
	ui_tooltip = "Global Midtones Curve Control";
> = 0.00;

uniform float fLightroom_GlobalHighlightsCurve <
	ui_type = "drag";
	ui_min = -1.00; ui_max = 1.00;
	ui_label = "Global Highlights Curve";
	ui_tooltip = "Global Highlights Curve Control";
> = 0.00;

uniform float fLightroom_GlobalWhitesCurve <
	ui_type = "drag";
	ui_min = -1.00; ui_max = 1.00;
	ui_label = "Global Whites Curve";
	ui_tooltip = "Global Whites Curve Control";
> = 0.00;

uniform float fLightroom_GlobalContrast <
	ui_type = "drag";
	ui_min = -1.00; ui_max = 1.00;
	ui_label = "Global Contrast";
	ui_tooltip = "Global Contrast Control";
> = 0.00;

uniform float fLightroom_GlobalSaturation <
	ui_type = "drag";
	ui_min = -1.00; ui_max = 1.00;
	ui_label = "Global Saturation";
	ui_tooltip = "Global Saturation Control";
> = 0.00;

uniform float fLightroom_GlobalVibrance <
	ui_type = "drag";
	ui_min = -1.00; ui_max = 1.00;
	ui_label = "Global Vibrance";
	ui_tooltip = "Global Vibrance Control";
> = 0.00;

uniform float fLightroom_GlobalTemperature <
	ui_type = "drag";
	ui_min = -1.00; ui_max = 1.00;
	ui_label = "Global Temperature";
	ui_tooltip = "Global Temperature Control";
> = 0.00;

uniform float fLightroom_GlobalTint <
	ui_type = "drag";
	ui_min = -1.00; ui_max = 1.00;
	ui_label = "Global Tint";
	ui_tooltip = "Global Tint Control";
> = 0.00;

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

uniform bool bLightroom_VignetteEnable <
	ui_label = "Enable Vignette Effect";
	ui_tooltip = "This enables a vignette effect (corner darkening).";
> = false;

uniform bool bLightroom_VignetteShowInOut <
	ui_label = "Show Vignette Inner and Outer radius";
	ui_tooltip = "This makes the inner and outer radius setting visible.\nVignette intensity builds up from green (no vignetting) to red (full vignetting).";
> = false;

uniform float fLightroom_VignetteRadiusInner <
	ui_type = "drag";
	ui_min = 0.00; ui_max = 2.00;
	ui_label = "Inner Vignette Radius";
	ui_tooltip = "Anything closer to the screen center than this is not affected by vignette.";
> = 0.00;

uniform float fLightroom_VignetteRadiusOuter <
	ui_type = "drag";
	ui_min = 0.00; ui_max = 3.00;
	ui_label = "Outer Vignette Radius";
	ui_tooltip = "Anything farther from the screen center than this gets fully vignette'd.";
> = 1.00;

uniform float fLightroom_VignetteWidth <
	ui_type = "drag";
	ui_min = 0.00; ui_max = 1.00;
	ui_label = "Vignette Width";
	ui_tooltip = "Higher values stretch the vignette horizontally.";
> = 0.00;

uniform float fLightroom_VignetteHeight <
	ui_type = "drag";
	ui_min = 0.00; ui_max = 1.00;
	ui_label = "Vignette Height";
	ui_tooltip = "Higher values stretch the vignette vertically.";
> = 0.00;

uniform float fLightroom_VignetteAmount <
	ui_type = "drag";
	ui_min = 0.00; ui_max = 1.00;
	ui_label = "Vignette Amount";
	ui_tooltip = "Intensity of vignette effect.";
> = 1.00;

uniform float fLightroom_VignetteCurve <
	ui_type = "drag";
	ui_min = 0.00; ui_max = 10.00;
	ui_label = "Vignette Curve";
	ui_tooltip = "Curve of gradient between inner and outer radius. 1.0 means linear.";
> = 1.00;

uniform int fLightroom_VignetteBlendMode <
	ui_type = "combo";
	ui_items = "Multiply\0Subtract\0Screen\0LumaPreserving\0";
	ui_tooltip = "Blending mode of vignette.";
> = 1;

#include "ReShade.fxh"

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

#define iHistogramBins 			255
uniform float2 mousepoint < source = "mousepoint"; >;
texture2D texHistogram		{ Width = iHistogramBins; Height = 1;  			Format = RGBA16F;			};
sampler2D SamplerHistogram 	{ Texture = texHistogram; };

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

float3 RGB2HCV(in float3 RGB)
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

float3 RGB2HSV(in float3 RGB)
{
    	float3 HCV = RGB2HCV(RGB);
    	float L = HCV.z - HCV.y * 0.5;
    	float S = HCV.y / (1.0000001 - abs(L * 2 - 1));
    	return float3(HCV.x, S, L);
}

float3 HSV2RGB(in float3 HSL)
{
	HSL = saturate(HSL);
	//HSL.z *= 0.99;
    	float3 RGB = saturate(float3(abs(HSL.x * 6.0 - 3.0) - 1.0,2.0 - abs(HSL.x * 6.0 - 2.0),2.0 - abs(HSL.x * 6.0 - 4.0)));
    	float C = (1 - abs(2 * HSL.z - 1)) * HSL.y;
    	return (RGB - 0.5) * C + HSL.z;
}

float linearstep(float lower, float upper, float value)
{
    	return saturate((value-lower)/(upper-lower));
}

float GetLumaGradient(float x)
{
	
	x = linearstep(fLightroom_GlobalBlackLevel / 255.0, fLightroom_GlobalWhiteLevel / 255.0, x);
	x = saturate(x * exp2(fLightroom_GlobalExposure));
	x = pow(x, exp2(-fLightroom_GlobalGamma));

	float BlacksMult   	= smoothstep(0.25,0.0,x);
	float ShadowsMult  	= smoothstep(0.00,0.25,x) * smoothstep(0.50,0.25,x);
	float MidtonesMult 	= smoothstep(0.25,0.50,x) * smoothstep(0.75,0.50,x);
	float HighlightsMult  	= smoothstep(0.50,0.75,x) * smoothstep(1.00,0.75,x);
	float WhitesMult  	= smoothstep(0.75,1.00,x);

	float BlacksCurve 	= fLightroom_GlobalBlacksCurve;
	float ShadowsCurve 	= fLightroom_GlobalShadowsCurve;
	float MidtonesCurve 	= fLightroom_GlobalMidtonesCurve;
	float HighlightsCurve 	= fLightroom_GlobalHighlightsCurve;
	float WhitesCurve 	= fLightroom_GlobalWhitesCurve;

	x = pow(saturate(x), exp2(BlacksMult*exp2(-BlacksCurve) + ShadowsMult*exp2(-ShadowsCurve) + MidtonesMult*exp2(-MidtonesCurve) + HighlightsMult*exp2(-HighlightsCurve) + WhitesMult*exp2(-WhitesCurve) - 1));
	x = lerp(x,smoothstep(0.0,1.0,x),fLightroom_GlobalContrast);
	return x;
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++


void PS_Lightroom(float4 vpos : SV_Position, float2 texcoord : TEXCOORD, out float4 res : SV_Target0)
{
	float4 color = tex2D(ReShade::BackBuffer, texcoord.xy);


	float TileSizeXY = iLightroom_LookupTileSizeXY;
	float TileAmount = iLightroom_LookupTileCount;

	float2 pixelcoord = vpos.xy - 0.5;
	pixelcoord.x += iLightroom_LookupScroll*(float)BUFFER_WIDTH;

	if(pixelcoord.x < TileSizeXY*TileAmount && pixelcoord.y < TileSizeXY && bLightroom_LookupEnable != 0)
	{
		color.xy = frac(pixelcoord.xy / TileSizeXY) * TileSizeXY / (TileSizeXY-1);
		color.z  = floor(pixelcoord.x/TileSizeXY)/(TileAmount-1); 
	}
//TEMPERATURE AND TINT
	float3 ColdKelvinColor = HSV2RGB(float3(0.56111,1.0,0.5));
	float3 WarmKelvinColor = HSV2RGB(float3(0.06111,1.0,0.5));

	float3 TempKelvinColor = lerp(ColdKelvinColor, WarmKelvinColor, fLightroom_GlobalTemperature*0.5+0.5);
	
	float oldLuma = RGB2HSV(color.xyz).z;
	float3 tintedColor = color.xyz * TempKelvinColor / dot(TempKelvinColor,float3(0.299,0.587,0.114)); //just using old luma does not work for some reason and weirds out colors.
	color.xyz = HSV2RGB(float3(RGB2HSV(tintedColor.xyz).xy,oldLuma));

	float3 TintColorGreen  = HSV2RGB(float3(0.31111,1.0,0.5));
	float3 TintColorPurple = HSV2RGB(float3(0.81111,1.0,0.5));

	float3 TempTintColor = lerp(TintColorGreen, TintColorPurple, fLightroom_GlobalTint*0.5+0.5);

	tintedColor = color.xyz * TempTintColor / dot(TempTintColor,float3(0.299,0.587,0.114)); //just using old luma does not work for some reason and weirds out colors.
	color.xyz = HSV2RGB(float3(RGB2HSV(tintedColor.xyz).xy,oldLuma));

//GLOBAL ADJUSTMENTS
	float3 HSV = RGB2HSV(color.xyz);
	HSV.z = GetLumaGradient(HSV.z);
	HSV.y = saturate(HSV.y*(1.0 + fLightroom_GlobalSaturation));
	HSV.y = pow(HSV.y,1.0 / exp2(fLightroom_GlobalVibrance)); //Preprocessor makes -f[..] to --f[..] so we need 1 / x
	HSV.xyz = saturate(HSV.xyz); //EXTREMELY IMPORTANT! FIXES BLACK PIXELS

//HUE ADJUSTMENTS
	float multRed 		= max(saturate(1.0 - abs(HSV.x*12.0)), saturate(1.0 - abs((HSV.x-12.0/12)* 6.0)));
	float multOrange 	= saturate(1.0 - abs((HSV.x-1.0/12)*12.0));
	float multYellow 	= max(saturate(1.0 - abs((HSV.x-2.0/12)*12.0))*step(HSV.x,2.0/12.0), saturate(1.0 - abs((HSV.x-2.0/12)* 6.0))*step(2.0/12.0,HSV.x));
	float multGreen 	= saturate(1.0 - abs((HSV.x-4.0/12)* 6.0));
	float multAqua 		= saturate(1.0 - abs((HSV.x-6.0/12)* 6.0));
	float multBlue 		= saturate(1.0 - abs((HSV.x-8.0/12)* 6.0));
	float multMagenta 	= saturate(1.0 - abs((HSV.x-10.0/12)* 6.0));

	float hueRed_old 	= 0.0;
	float hueOrange_old	= 0.0833333333333;
	float hueYellow_old	= 0.1666666666666;  
	float hueGreen_old	= 0.3333333333333;  
	float hueAqua_old	= 0.5; 
	float hueBlue_old	= 0.6666666666666; 
	float hueMagenta_old	= 0.8333333333333; 
	float hueRed_old2	= 1.0;

	float hueRed 		= (fLightroom_Red_Hueshift > 0.0) 	? lerp(hueRed_old, hueOrange_old, fLightroom_Red_Hueshift) 		: lerp(hueMagenta_old, hueRed_old2, fLightroom_Red_Hueshift+1.0);
	float hueOrange 	= (fLightroom_Orange_Hueshift > 0.0)    ? lerp(hueOrange_old, hueYellow_old, fLightroom_Orange_Hueshift) 	: lerp(hueRed_old, hueOrange_old, fLightroom_Orange_Hueshift+1.0);   
	float hueYellow 	= (fLightroom_Yellow_Hueshift > 0.0)    ? lerp(hueYellow_old, hueGreen_old, fLightroom_Yellow_Hueshift) 	: lerp(hueOrange_old, hueYellow_old, fLightroom_Yellow_Hueshift+1.0);
	float hueGreen 		= (fLightroom_Green_Hueshift > 0.0)     ? lerp(hueGreen_old, hueAqua_old, fLightroom_Green_Hueshift) 		: lerp(hueYellow_old, hueGreen_old, fLightroom_Green_Hueshift+1.0);
	float hueAqua 		= (fLightroom_Aqua_Hueshift > 0.0)      ? lerp(hueAqua_old, hueBlue_old, fLightroom_Aqua_Hueshift) 		: lerp(hueGreen_old, hueAqua_old, fLightroom_Aqua_Hueshift+1.0);
	float hueBlue 		= (fLightroom_Blue_Hueshift > 0.0)      ? lerp(hueBlue_old, hueMagenta_old, fLightroom_Blue_Hueshift) 		: lerp(hueAqua_old, hueBlue_old, fLightroom_Blue_Hueshift+1.0);
	float hueMagenta 	= (fLightroom_Magenta_Hueshift > 0.0)   ? lerp(hueMagenta_old, hueRed_old2, fLightroom_Magenta_Hueshift) 	: lerp(hueBlue_old, hueMagenta_old, fLightroom_Magenta_Hueshift+1.0);

	//could use exp2 for saturation as well but then grayscale would require -inf as saturation setting.
	color.xyz = 0.0;
	color.xyz += HSV2RGB(float3(hueRed,	saturate(HSV.y * (1.0 + fLightroom_Red_Saturation)), 		HSV.z * exp2(HSV.y*fLightroom_Red_Exposure)	)) * multRed;
	color.xyz += HSV2RGB(float3(hueOrange,	saturate(HSV.y * (1.0 + fLightroom_Orange_Saturation)), 	HSV.z * exp2(HSV.y*fLightroom_Orange_Exposure)	)) * multOrange;
	color.xyz += HSV2RGB(float3(hueYellow,	saturate(HSV.y * (1.0 + fLightroom_Yellow_Saturation)), 	HSV.z * exp2(HSV.y*fLightroom_Yellow_Exposure)	)) * multYellow;
	color.xyz += HSV2RGB(float3(hueGreen,	saturate(HSV.y * (1.0 + fLightroom_Green_Saturation)), 		HSV.z * exp2(HSV.y*fLightroom_Green_Exposure)	)) * multGreen;
	color.xyz += HSV2RGB(float3(hueAqua,	saturate(HSV.y * (1.0 + fLightroom_Aqua_Saturation)), 		HSV.z * exp2(HSV.y*fLightroom_Aqua_Exposure)	)) * multAqua;
	color.xyz += HSV2RGB(float3(hueBlue,	saturate(HSV.y * (1.0 + fLightroom_Blue_Saturation)), 		HSV.z * exp2(HSV.y*fLightroom_Blue_Exposure)	)) * multBlue;	
	color.xyz += HSV2RGB(float3(hueMagenta,	saturate(HSV.y * (1.0 + fLightroom_Magenta_Saturation)), 	HSV.z * exp2(HSV.y*fLightroom_Magenta_Exposure)	)) * multMagenta;

	res.xyz = color.xyz;
	res.w = 1.0;

}

void PS_HistogramToTexture(float4 vpos : SV_Position, float2 texcoord : TEXCOORD, out float4 res : SV_Target0)
{
	res = tex2D(ReShade::BackBuffer,texcoord.xy);
}

void PS_HistogramGenerate(float4 vpos : SV_Position, float2 texcoord : TEXCOORD, out float4 res : SV_Target0)
{
	float currentBin = vpos.x;
	float3 currentBinData = 0.0;
	[branch]
	if(bLightroom_HistogramEnable != 0)
	{
		[loop]
		for(float x=0; x<=iLightroom_HistogramSamplesSqrt; x++)
		{
			[loop]
			for(float y=0; y<=iLightroom_HistogramSamplesSqrt; y++)
			{
				float2 currentPosition = float2(x,y) / iLightroom_HistogramSamplesSqrt;
				float3 currentColor = tex2Dlod(ReShade::BackBuffer, float4(currentPosition.xy,0,0)).xyz;
				currentBinData += saturate(1.0 - abs(currentColor.xyz*iHistogramBins - currentBin)/fLightroom_HistogramSmoothness);
			}
 		}
	}
	res = currentBinData.xyzz / fLightroom_HistogramSmoothness;
}

void PS_DisplayStatistics(float4 vpos : SV_Position, float2 texcoord : TEXCOORD, out float4 res : SV_Target0)
{
	float4 color = tex2D(ReShade::BackBuffer,texcoord.xy);
	float2 vposfbl = float2(vpos.x, BUFFER_HEIGHT-vpos.y);

	float2 VignetteCoord = texcoord.xy * 2.0 - 1.0;
	VignetteCoord.xy *= 1.0 - float2(fLightroom_VignetteWidth,fLightroom_VignetteHeight);
	float VignetteGradient = length(VignetteCoord);
	float VignetteFactor = linearstep(fLightroom_VignetteRadiusInner, fLightroom_VignetteRadiusOuter, VignetteGradient);
	VignetteFactor = pow(VignetteFactor,fLightroom_VignetteCurve+0.00001) * fLightroom_VignetteAmount;
	
	float2 pixelcoord = vpos.xy - 0.5;
	float TileSizeXY = iLightroom_LookupTileSizeXY;
	float TileAmount = iLightroom_LookupTileCount;

	float3 novigcolor = color.xyz;

	if(bLightroom_VignetteEnable != 0)
	{
		
		color.xyz = (fLightroom_VignetteBlendMode == 0) ? color.xyz * saturate(1.0 - VignetteFactor) : color.xyz;
		color.xyz = (fLightroom_VignetteBlendMode == 1) ? saturate(color.xyz-VignetteFactor.xxx) : color.xyz;
		color.xyz = (fLightroom_VignetteBlendMode == 2) ? 1.0-(1.0-color.xyz)*(VignetteFactor + 1.0) : color.xyz;
		color.xyz = (fLightroom_VignetteBlendMode == 3) ? color.xyz * saturate(lerp(1.0 - VignetteFactor*2.0,1.0,dot(color.xyz,0.333))) : color.xyz;

		color.xyz = (bLightroom_VignetteShowInOut != 0) ? lerp(color.xyz,float3(0.0,1.0,0.0),saturate(1.0 - 500.0 * abs(VignetteGradient-fLightroom_VignetteRadiusInner))) : color.xyz;
		color.xyz = (bLightroom_VignetteShowInOut != 0) ? lerp(color.xyz,float3(1.0,0.0,0.0),saturate(1.0 - 500.0 * abs(VignetteGradient-fLightroom_VignetteRadiusOuter))) : color.xyz;
		color.xyz = (pixelcoord.x < TileSizeXY*TileAmount && pixelcoord.y < TileSizeXY && bLightroom_LookupEnable != 0) ? novigcolor.xyz : color.xyz;
	} 

	if(bLightroom_HistogramEnable != 0 || bLightroom_LumaCurveEnable != 0)
	{
		vposfbl.x += (bLightroom_ColorPickerEnable == 0) ? 295.0 : 0.0;
		float func = GetLumaGradient((vposfbl.x-295.0) / 255.0);
		float3 histogram = tex2D(SamplerHistogram,float2((vposfbl.x-295.0) / 255.0,0.0)).xyz / (iLightroom_HistogramSamplesSqrt*iLightroom_HistogramSamplesSqrt) * 255.0 * fLightroom_HistogramHeight;

		if(vposfbl.x >= 295 && vposfbl.x <= 550  && vposfbl.y <= 255) 
		{
			color.xyz = 0.0;
			color.xyz = (bLightroom_HistogramEnable != 0) ? vposfbl.yyy < histogram.xyz : color.xyz;
			color.xyz = (bLightroom_LumaCurveEnable != 0) ? lerp(color.xyz,1.0-color.xyz,smoothstep(BUFFER_RCP_HEIGHT*4.0, 0.0,abs(func-vposfbl.y/255.0))) : color.xyz;
		}
	}

	if(bLightroom_ColorPickerEnable != 0)
	{
		float2 samplecoord = mousepoint * ReShade::PixelSize;
		float3 samplecolor = RGB2HSV(tex2D(ReShade::BackBuffer, samplecoord.xy).xyz);
		//   HV table          H bar
		//0.............255...270..280
		color.xyz = (abs(vposfbl.x - 275.0) < 5.0 && vposfbl.y < 255) ? HSV2RGB(float3(vposfbl.y / 255.0,1.0,0.5)) : color.xyz;
		color.xyz = (all(vposfbl.xy < 255))  ? HSV2RGB(float3(samplecolor.x,vposfbl.x/255.0,vposfbl.y/255.0)) : color.xyz;
		color.xyz = (abs(vposfbl.x - 275.0) < 10.0 && abs(vposfbl.y - samplecolor.x * 255) < 2.0)  ? 0.0 : color.xyz;
		color.xyz = (all(vposfbl.xy < 255) && distance(vposfbl.xy,samplecolor.yz*255.0) < 5.0 && distance(vposfbl.xy,samplecolor.yz*255.0) > 3.0)  ? 0.0 : color.xyz;
		color.xyz = (abs(vpos.x - mousepoint.x - 50) < 46.0 && abs(vpos.y - mousepoint.y + 50) < 46.0) ? 0.0 : color.xyz;
		color.xyz = (abs(vpos.x - mousepoint.x - 50) < 40.0 && abs(vpos.y - mousepoint.y + 50) < 40.0) ? HSV2RGB(samplecolor.xyz) : color.xyz;
		color.xyz = (length(vpos.xy - mousepoint.xy) < 3) ? 1.0 : color.xyz;
		color.xyz = (length(vpos.xy - mousepoint.xy) < 2) ? 0.0 : color.xyz;
	}

	//color.xyz = HSV2RGB(float3(texcoord.x,1.0,0.5));

	res = color;
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++


technique Lightroom 
{
	pass PLightroom
	{
		VertexShader = PostProcessVS;
		PixelShader = PS_Lightroom;
	}
	pass PHistogramGenerate
	{
		VertexShader = PostProcessVS;
		PixelShader = PS_HistogramGenerate;
		RenderTarget = texHistogram;
	}
	pass PHistogram
	{
		VertexShader = PostProcessVS;
		PixelShader = PS_DisplayStatistics;
	}
}