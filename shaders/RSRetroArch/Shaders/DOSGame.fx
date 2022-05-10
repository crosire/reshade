#include "ReShade.fxh"

/**
* Dos Game shader by Boris Vorontsov
* enbdev.com/effect_dosgame.zip
*/

uniform int PIXELSIZE <
	ui_type = "drag";
	ui_min = 1;
	ui_max = 20;
	ui_label = "Pixel Size [DOSGame]";
	ui_step = 1;
> = 2;

uniform bool ENABLE_SCREENSIZE <
	ui_type = "boolean";
	ui_label = "Use Absolute Res [DOSGame]";
> = true;

uniform int2 DOSScreenSize <
	ui_type = "drag";
	ui_min = 1;
	ui_max = 99999;
	ui_step = 1;
	ui_label = "Screen Resolution [DOSGame]";
> = int2(320,240);

uniform bool DOSCOLOR <
	ui_type = "boolean";
	ui_label = "Enable Color Banding [DOSGame]";
> = true;

uniform int DOSColorsCount <
	ui_type = "drag";
	ui_min = 1;
	ui_max = 255;
	ui_step = 1;
	ui_label = "Colors Count [DOSGame]";
> = 16;

uniform bool ENABLE_POSTCURVE <
	ui_type = "boolean";
	ui_label = "Enable Brightness Curve [DOSGame]";
> = true;

uniform float POSTCURVE <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 5.0;
	ui_step = 0.001;
	ui_label = "Curve Adjustment [DOSGame]";
> = 0.5;

uniform bool ENABLE_AGD <
	ui_type = "boolean";
	ui_label = "Enable Gamma Adjustment [DOSGame]";
> = true;

uniform float DoSgammaValue <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 10.0;
	ui_step = 0.001;
	ui_label = "Gamma Value [DOSGame]";
> = 2.2;

float4 PS_DosFX(float4 vpos : SV_Position, float2 texcoord : TEXCOORD) : SV_Target
{
	float2 xs = ReShade::ScreenSize / PIXELSIZE;

	if (ENABLE_SCREENSIZE) xs=DOSScreenSize;
		
	texcoord.xy=floor(texcoord.xy * xs)/xs;

	float4 origcolor=tex2D(ReShade::BackBuffer, texcoord);

	origcolor+=0.0001;

	if (DOSCOLOR) {
		float graymax=max(origcolor.x, max(origcolor.y, origcolor.z));
		float3 ncolor=origcolor.xyz/graymax;
		graymax=floor(graymax * DOSColorsCount)/DOSColorsCount;
		origcolor.xyz*=graymax;
	
		if (ENABLE_POSTCURVE) origcolor.xyz = pow(origcolor.xyz, POSTCURVE);
	}

	return origcolor;
}

float4 PS_DosGamma(float4 vpos : SV_Position, float2 texcoord : TEXCOORD) : SV_Target
{
	float4 color=tex2D(ReShade::BackBuffer, texcoord);
	if (ENABLE_AGD) color.xyz = lerp(color.xyz,-0.0039*pow(1.0/0.0039, 1.0-color.xyz)+1.0,0.7*(DoSgammaValue/2.2));
	return color;
}

technique DosFX_Tech
{
	pass DosFXGammaPass
	{
		VertexShader = PostProcessVS;
		PixelShader = PS_DosGamma;
	}

	pass DosFXPass
	{
		VertexShader = PostProcessVS;
		PixelShader = PS_DosFX;
	}
}