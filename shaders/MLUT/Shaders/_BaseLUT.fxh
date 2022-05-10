#pragma once

//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// ReShade effect file
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Multi-LUT shader, using a texture atlas with multiple LUTs
// by Otis / Infuse Project. (https://github.com/FransBouma)
// Based on Marty's LUT shader 1.0 for ReShade 3.0 (https://github.com/martymcmodding)
// Thanks to kingeric1992 and Matsilagi for the tools (https://github.com/kingeric1992) (https://github.com/Matsilagi)
// Thanks to luluco250 for refactoring the code (https://github.com/luluco250)
// BlueSkyDefender for tweaking the shader every once in a while (https://github.com/BlueSkyDefender) 
// Thanks to etra0 (https://github.com/etra0) for helping speed up the creation process
// Converted and Modified by TheGordinho
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

#include "ReShade.fxh"
#include "ReShadeUI.fxh"

#if !defined(fLUT_Name)
    #error "LUT name not defined"
#endif

#if !defined(fLUT_LutList)
	#error "LUT list not defined"
#endif

#if !defined(fLUT_TextureName)
    #error "LUT texture name not defined"
#endif

#if !defined(fLUT_TileSizeXY)
    #error "LUT tile size not defined"
#endif

#if !defined(fLUT_TileAmount)
    #error "LUT tile amount not defined"
#endif

#if !defined(fLUT_LutAmount)
    #error "LUT amount not defined"
#endif

namespace fLUT_Name
{
uniform int fLUT_LutSelector<
	__UNIFORM_COMBO_INT1
	ui_label = "The LUT to use";
	ui_items = fLUT_LutList;
> = 0;

uniform float GlobalControl <
	__UNIFORM_SLIDER_FLOAT1
	ui_min = 0.00; ui_max = 1.00;
	ui_label = "Global Control";
	ui_tooltip = "Controls how much of the Lut is applyed.";
> = 1.00;

uniform float fLUT_AmountChroma <
	__UNIFORM_SLIDER_FLOAT1
	ui_min = 0.00; ui_max = 1.00;
	ui_label = "LUT chroma amount";
	ui_tooltip = "Intensity of color/chroma change of the LUT.";
> = 1.00;

uniform float fLUT_AmountLuma <
	__UNIFORM_SLIDER_FLOAT1
	ui_min = 0.00; ui_max = 1.00;
	ui_label = "LUT luma amount";
	ui_tooltip = "Intensity of luma change of the LUT.";

> = 1.00;

uniform bool bLUT_UseDepth <
	ui_label = "LUT Depth Distance";
	ui_tooltip = "Allows LUT to change colors based on the distance.";
> = false;

uniform float2 fLUT_DepthBlend <
	__UNIFORM_SLIDER_FLOAT2
	ui_min = 0.00; ui_max = 1.00;
	ui_label = "Lut Depth Control";
	ui_tooltip = "Distance where the LUT gets applied in the image.";
> = 0.5;

uniform float2 fLut_BlendPower <
	__UNIFORM_SLIDER_FLOAT2
	ui_min = 0.00; ui_max = 1.00;
	ui_label = "Blend Control";
	ui_tooltip = "Controls how much the 'edges' of the lut blends.";
> = 0.25;


texture MultiLutTex <source = fLUT_TextureName;>
{
	Width = fLUT_TileSizeXY * fLUT_TileAmount;
	Height = fLUT_TileSizeXY * fLUT_LutAmount;
};

sampler MultiLUT
{
	Texture = MultiLutTex;
};

	float Dot(float3 D) { return dot(D,float3(0.2125, 0.7154, 0.0721)); }; 
	float2 DSmooth() 
{
	float valueA = 1- fLUT_DepthBlend.x, valueB = 1- fLUT_DepthBlend.y;
	valueB = lerp(0, 0.5,saturate (valueB));
	return float2(saturate (valueA), valueB);
}; 

float4 MainPS(
	float4 pos : SV_POSITION,
	float2 uv : TEXCOORD) : SV_TARGET
{
	float4 color = tex2D(ReShade::BackBuffer, uv);
	float depth = ReShade::GetLinearizedDepth(uv);
	color.a = Dot(color.rgb);
	float2 lut_ps = rcp(fLUT_TileSizeXY);
	lut_ps.x /= fLUT_TileAmount;

	float3 lut_uv = float3(
		(color.rg * fLUT_TileSizeXY - color.rg + 0.5) * lut_ps,
		color.b * fLUT_TileSizeXY - color.b);

	lut_uv.y /= fLUT_LutAmount;
	lut_uv.y += float(fLUT_LutSelector) / fLUT_LutAmount;

	float lerpfact = frac(lut_uv.z);
	lut_uv.x += (lut_uv.z - lerpfact) * lut_ps.y;

	float4 lutcolor = (float4)lerp(
		(float4)tex2D(MultiLUT, lut_uv.xy),
		(float4)tex2D(
			MultiLUT,
			float2(lut_uv.x + lut_ps.y, lut_uv.y)),
		lerpfact);
		lutcolor.a = Dot(lutcolor.rgb);
		float3 storecolor = color.rgb ; 
		
    color.rgb    =  lerp(lutcolor.rgb, color.rgb, 1 - fLUT_AmountChroma);
    color.rgb    -= Dot(color.rgb);    
    color.rgb    += lerp(lutcolor.a, color.a, 1 - fLUT_AmountLuma);
	color.rgb	 = lerp(color.rgb, storecolor, 1 -  GlobalControl); 
	
	if (bLUT_UseDepth == 1){
		float2 depthBlend = DSmooth();
		float emphasizeLut = smoothstep(0.5,lerp(0.5,1,fLut_BlendPower.x),depth * depthBlend.x * 4);
		emphasizeLut -= smoothstep(0.5, lerp(0.5,1,fLut_BlendPower.y), depth * depthBlend.y * 4);
		color.rgb =  lerp(storecolor.rgb, color.rgb, saturate( emphasizeLut));
	}
	
	return color;
}

technique fLUT_Name
{
	pass MultiLUT_Apply
	{
		VertexShader = PostProcessVS;
		PixelShader = MainPS;
	}
}
}
