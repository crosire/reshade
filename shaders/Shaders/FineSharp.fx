/**
 * FineSharp by Did�e
 * http://avisynth.nl/images/FineSharp.avsi
 *
 * Initial HLSL port by -Vit-
 * https://forum.doom9.org/showthread.php?t=171346
 *
 * Modified and optimized for ReShade by JPulowski
 *
 * Do not distribute without giving credit to the original author(s).
 *
 * 1.0  - Initial release
 */

#include "ReShadeUI.fxh"

uniform float sstr < __UNIFORM_SLIDER_FLOAT1
	ui_min = 0.00; ui_max = 8.00;
	ui_label = "Sharpening Strength";
> = 2.00;

uniform float cstr <
	ui_type = "drag";
	ui_min = 0.00; ui_max = 1.249;
	ui_label = "Equalization Strength";
	ui_tooltip = "Suggested settings for cstr based on sstr value: sstr=0->cstr=0, sstr=0.5->cstr=0.1, 1.0->0.6, 2.0->0.9, 2.5->1.00, 3.0->1.09, 3.5->1.15, 4.0->1.19, 8.0->1.249";
> = 0.90;

uniform float xstr < __UNIFORM_SLIDER_FLOAT1
	ui_min = 0.00; ui_max = 1.00;
	ui_tooltip = "Strength of XSharpen-style final sharpening.";
> = 0.19;

uniform float xrep < __UNIFORM_SLIDER_FLOAT1
	ui_min = 0.00; ui_max = 1.00;
	ui_tooltip = "Repair artefacts from final sharpening. (-Vit- addition to original script)";
> = 0.25;

uniform float lstr <
	ui_type = "input";
	ui_tooltip = "modifier for non-linear sharpening";
> = 1.49;

uniform float pstr <
	ui_type = "input";
	ui_tooltip = "exponent for non-linear sharpening";
> = 1.272;

// Viscera parameters

#ifndef ldmp
	#define ldmp (sstr+0.1)	//"low damp", to not overenhance very small differences (noise coming out of flat areas)
#endif

#include "ReShade.fxh"

// Helper functions

float4 Src(float a, float b, float2 tex) {
	return tex2D(ReShade::BackBuffer, mad(ReShade::PixelSize, float2(a, b), tex));
}

float3x3 RGBtoYUV(float Kb, float Kr) {
	return float3x3(float3(Kr, 1.0 - Kr - Kb, Kb), float3(-Kr, Kr + Kb - 1.0, 1.0 - Kb) / (2.0 * (1.0 - Kb)), float3(1.0 - Kr, Kr + Kb - 1.0, -Kb) / (2.0 * (1.0 - Kr)));
}

float3x3 YUVtoRGB(float Kb, float Kr) {
	return float3x3(float3(1.0, 0.0, 2.0 * (1.0 - Kr)), float3(Kb + Kr - 1.0, 2.0 * (1.0 - Kb) * Kb, 2 * Kr * (1.0 - Kr)) / (Kb + Kr - 1.0), float3(1.0, 2.0 * (1.0 - Kb), 0.0));
}

void sort(inout float a1, inout float a2) {
	float t = min(a1, a2);
	a2 = max(a1, a2);
	a1 = t;
}

float median3(float a1, float a2, float a3) {
	sort(a2, a3);
	sort(a1, a2);
	
	return min(a2, a3);
}

float median5(float a1, float a2, float a3, float a4, float a5) {
	sort(a1, a2);
	sort(a3, a4);
	sort(a1, a3);
	sort(a2, a4);
	
	return median3(a2, a3, a5);
}

float median9(float a1, float a2, float a3, float a4, float a5, float a6, float a7, float a8, float a9) {
	sort(a1, a2);
	sort(a3, a4);
	sort(a5, a6);
	sort(a7, a8);
	sort(a1, a3);
	sort(a5, a7);
	sort(a1, a5);
	
	sort(a3, a5);
	sort(a3, a7);
	sort(a2, a4);
	sort(a6, a8);
	sort(a4, a8);
	sort(a4, a6);
	sort(a2, a6);
	
	return median5(a2, a4, a5, a7, a9);
}

void sort_min_max7(inout float a1, inout float a2, inout float a3, inout float a4, inout float a5, inout float a6, inout float a7) {
	sort(a1, a2);
	sort(a3, a4);
	sort(a5, a6);
	
	sort(a1, a3);
	sort(a1, a5);
	sort(a2, a6);
	
	sort(a4, a5);
	sort(a1, a7);
	sort(a6, a7);
}

void sort_min_max9(inout float a1, inout float a2, inout float a3, inout float a4, inout float a5, inout float a6, inout float a7, inout float a8, inout float a9) {
	sort(a1, a2);
	sort(a3, a4);
	sort(a5, a6);
	sort(a7, a8);
	
	sort(a1, a3);
	sort(a5, a7);
	sort(a1, a5);
	sort(a2, a4);
	
	sort(a6, a7);
	sort(a4, a8);
	sort(a1, a9);
	sort(a8, a9);
}

void sort9_partial2(inout float a1, inout float a2, inout float a3, inout float a4, inout float a5, inout float a6, inout float a7, inout float a8, inout float a9) {
	sort_min_max9(a1,a2,a3,a4,a5,a6,a7,a8,a9);
	sort_min_max7(a2,a3,a4,a5,a6,a7,a8);
}


float SharpDiff(float4 c) {
	float t = c.a - c.x;
	return sign(t) * (sstr / 255.0) * pow(abs(t) / (lstr / 255.0), 1.0 / pstr) * ((t * t) / mad(t, t, ldmp / 65025.0));
}

// Main

float4 PS_FineSharp_P0(float4 vpos : SV_Position, float2 texcoord : TexCoord) : SV_Target {
	float3 yuv = mul(RGBtoYUV(0.0722, 0.2126), Src(0.0, 0.0, texcoord).rgb ) + float3(0.0, 0.5, 0.5);
	return float4(yuv, yuv.x);
}

float4 PS_FineSharp_P1(float4 vpos : SV_Position, float2 texcoord : TexCoord) : SV_Target {
	float4 o = Src(0.0, 0.0, texcoord);
	
	o.x += o.x;
	o.x += Src( 0.0, -1.0, texcoord).x + Src(-1.0,  0.0, texcoord).x + Src( 1.0, 0.0, texcoord).x + Src(0.0, 1.0, texcoord).x;
	o.x += o.x;
	o.x += Src(-1.0, -1.0, texcoord).x + Src( 1.0, -1.0, texcoord).x + Src(-1.0, 1.0, texcoord).x + Src(1.0, 1.0, texcoord).x;
	o.x *= 0.0625;

	return o;
}

float4 PS_FineSharp_P2(float4 vpos : SV_Position, float2 texcoord : TexCoord) : SV_Target {
	float4 o = Src(0.0, 0.0, texcoord);

	float t1 = Src(-1.0, -1.0, texcoord).x;
	float t2 = Src( 0.0, -1.0, texcoord).x;
	float t3 = Src( 1.0, -1.0, texcoord).x;
	float t4 = Src(-1.0,  0.0, texcoord).x;
	float t5 = o.x;
	float t6 = Src( 1.0,  0.0, texcoord).x;
	float t7 = Src(-1.0,  1.0, texcoord).x;
	float t8 = Src( 0.0,  1.0, texcoord).x;
	float t9 = Src( 1.0,  1.0, texcoord).x;
	o.x = median9(t1,t2,t3,t4,t5,t6,t7,t8,t9);
	
	return o;
}

float4 PS_FineSharp_P3(float4 vpos : SV_Position, float2 texcoord : TexCoord) : SV_Target {
	float4 o = Src(0.0, 0.0, texcoord);
	
	float sd = SharpDiff(o);
	o.x = o.a + sd;
	sd += sd;
	sd += SharpDiff(Src( 0.0, -1.0, texcoord)) + SharpDiff(Src(-1.0,  0.0, texcoord)) + SharpDiff(Src( 1.0, 0.0, texcoord)) + SharpDiff(Src( 0.0, 1.0, texcoord));
	sd += sd;
	sd += SharpDiff(Src(-1.0, -1.0, texcoord)) + SharpDiff(Src( 1.0, -1.0, texcoord)) + SharpDiff(Src(-1.0, 1.0, texcoord)) + SharpDiff(Src( 1.0, 1.0, texcoord));
	sd *= 0.0625;
	o.x -= cstr * sd;
	o.a = o.x;

	return o;
}

float4 PS_FineSharp_P4(float4 vpos : SV_Position, float2 texcoord : TexCoord) : SV_Target {
	float4 o = Src(0.0, 0.0, texcoord);

	float t1 = Src(-1.0, -1.0, texcoord).a;
	float t2 = Src( 0.0, -1.0, texcoord).a;
	float t3 = Src( 1.0, -1.0, texcoord).a;
	float t4 = Src(-1.0,  0.0, texcoord).a;
	float t5 = o.a;
	float t6 = Src( 1.0,  0.0, texcoord).a;
	float t7 = Src(-1.0,  1.0, texcoord).a;
	float t8 = Src( 0.0,  1.0, texcoord).a;
	float t9 = Src( 1.0,  1.0, texcoord).a;

	o.x += t1 + t2 + t3 + t4 + t6 + t7 + t8 + t9;
	o.x /= 9.0;
	o.x = mad(9.9, (o.a - o.x), o.a);
	
	sort9_partial2(t1, t2, t3, t4, t5, t6, t7, t8, t9);
	o.x = max(o.x, min(t2, o.a));
	o.x = min(o.x, max(t8, o.a));

	return o;
}

float4 PS_FineSharp_P5(float4 vpos : SV_Position, float2 texcoord : TexCoord) : SV_Target {
	float4 o = Src(0.0, 0.0, texcoord);

	float edge = abs(Src(0.0, -1.0, texcoord).x + Src(-1.0, 0.0, texcoord).x + Src(1.0, 0.0, texcoord).x + Src(0.0, 1.0, texcoord).x - 4 * o.x);
	o.x = lerp(o.a, o.x, xstr * (1.0 - saturate(edge * xrep)));

	return o;
}

float4 PS_FineSharp_P6(float4 vpos : SV_Position, float2 texcoord : TexCoord) : SV_Target {
	float4 rgb = Src(0.0, 0.0, texcoord);
	rgb.xyz = mul(YUVtoRGB(0.0722,0.2126), rgb.xyz - float3(0.0, 0.5, 0.5));
	return rgb;
}

technique Mode1
{
	pass ToYUV
	{
		VertexShader = PostProcessVS;
		PixelShader = PS_FineSharp_P0;
	}
	
	pass RemoveGrain11
	{
		VertexShader = PostProcessVS;
		PixelShader = PS_FineSharp_P1;
	}
	
	pass RemoveGrain4
	{
		VertexShader = PostProcessVS;
		PixelShader = PS_FineSharp_P2;
	}
	
	pass FineSharpA
	{
		VertexShader = PostProcessVS;
		PixelShader = PS_FineSharp_P3;
	}
	
	pass FineSharpB
	{
		VertexShader = PostProcessVS;
		PixelShader = PS_FineSharp_P4;
	}
	
	pass FineSharpC
	{
		VertexShader = PostProcessVS;
		PixelShader = PS_FineSharp_P5;
	}
	
	pass ToRGB
	{
		VertexShader = PostProcessVS;
		PixelShader = PS_FineSharp_P6;
	}
}

technique Mode2
{
	pass ToYUV
	{
		VertexShader = PostProcessVS;
		PixelShader = PS_FineSharp_P0;
	}
	
	pass RemoveGrain4
	{
		VertexShader = PostProcessVS;
		PixelShader = PS_FineSharp_P2;
	}
	
	pass RemoveGrain11
	{
		VertexShader = PostProcessVS;
		PixelShader = PS_FineSharp_P1;
	}
	
	pass FineSharpA
	{
		VertexShader = PostProcessVS;
		PixelShader = PS_FineSharp_P3;
	}
	
	pass FineSharpB
	{
		VertexShader = PostProcessVS;
		PixelShader = PS_FineSharp_P4;
	}
	
	pass FineSharpC
	{
		VertexShader = PostProcessVS;
		PixelShader = PS_FineSharp_P5;
	}
	
	pass ToRGB
	{
		VertexShader = PostProcessVS;
		PixelShader = PS_FineSharp_P6;
	}
}

technique Mode3
{
	pass ToYUV
	{
		VertexShader = PostProcessVS;
		PixelShader = PS_FineSharp_P0;
	}
	
	pass RemoveGrain4_1
	{
		VertexShader = PostProcessVS;
		PixelShader = PS_FineSharp_P2;
	}
	
	pass RemoveGrain11
	{
		VertexShader = PostProcessVS;
		PixelShader = PS_FineSharp_P1;
	}
	
	pass RemoveGrain4_2
	{
		VertexShader = PostProcessVS;
		PixelShader = PS_FineSharp_P2;
	}
	
	pass FineSharpA
	{
		VertexShader = PostProcessVS;
		PixelShader = PS_FineSharp_P3;
	}
	
	pass FineSharpB
	{
		VertexShader = PostProcessVS;
		PixelShader = PS_FineSharp_P4;
	}
	
	pass FineSharpC
	{
		VertexShader = PostProcessVS;
		PixelShader = PS_FineSharp_P5;
	}
	
	pass ToRGB
	{
		VertexShader = PostProcessVS;
		PixelShader = PS_FineSharp_P6;
	}
}
