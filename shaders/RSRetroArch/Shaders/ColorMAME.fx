#include "ReShade.fxh"

uniform float3 RedRatios <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 2.0;
	ui_step = 0.001;
	ui_label = "Red Output from R,G and B [Color]";
	ui_tooltip = "Each float is a channel.";
> = float3(1.0, 0.0, 0.0);

uniform float3 GrnRatios <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 2.0;
	ui_step = 0.001;
	ui_label = "Green Output from R,G and B [Color]";
	ui_tooltip = "Each float is a channel.";
> = float3(0.0, 1.0, 0.0);

uniform float3 BluRatios <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 2.0;
	ui_step = 0.001;
	ui_label = "Blue Output from R,G and B [Color]";
	ui_tooltip = "Each float is a channel.";
> = float3(0.0, 0.0, 1.0);

uniform float3 Offset <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 2.0;
	ui_step = 0.001;
	ui_label = "Signal Offset [Color]";
> = float3(0.0, 0.0, 0.0);

uniform float3 Scale <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 2.0;
	ui_step = 0.001;
	ui_label = "Signal Scale [Color]";
> = float3(0.95, 0.95, 0.95);

uniform float Saturation <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 4.0;
	ui_step = 0.001;
	ui_label = "Saturation [Color]";
> = float(1.0);

uniform float3 Power <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 1.0;
	ui_step = 0.001;
	ui_label = "Signal Power [Color]";
> = float3(1.0,1.0,1.0);

uniform float3 Floor <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 1.0;
	ui_step = 0.001;
	ui_label = "Signal Floor [Color]";
> = float3(0.0, 0.0, 0.0);

float4 PS_ColorMAME(float4 vpos : SV_Position0, float2 texcoord : TEXCOORD0) : SV_Target
{
	texcoord += 0.5 / ReShade::ScreenSize.xy;
	
	float4 BaseTexel = tex2D(ReShade::BackBuffer, texcoord);

	float3 OutRGB = BaseTexel.rgb;

	// RGB Tint & Shift
	float ShiftedRed = dot(OutRGB, RedRatios);
	float ShiftedGrn = dot(OutRGB, GrnRatios);
	float ShiftedBlu = dot(OutRGB, BluRatios);

	// RGB Scale & Offset
	float3 OutTexel = float3(ShiftedRed, ShiftedGrn, ShiftedBlu) * Scale + Offset;

	// Saturation
	float3 Grayscale = float3(0.299, 0.587, 0.114);
	float OutLuma = dot(OutTexel, Grayscale);
	float3 OutChroma = OutTexel - OutLuma;
	float3 Saturated = OutLuma + OutChroma * Saturation;
	
	// Color Compression (may not affect bloom), which isn't there.
	// increasing the floor of the signal without affecting the ceiling
	Saturated.rgb = Floor + (1.0f - Floor) * Saturated.rgb;
	
	// Color Power (may affect bloom)
	Saturated.r = pow(Saturated.r, Power.r);
	Saturated.g = pow(Saturated.g, Power.g);
	Saturated.b = pow(Saturated.b, Power.b);

	return float4(Saturated, BaseTexel.a);
}

technique ColorMAME
{
	pass ColorMAME_PS1
	{
		VertexShader = PostProcessVS;
		PixelShader = PS_ColorMAME;
	}
}