#include "ReShade.fxh"
#include "ReShadeUI.fxh"

uniform float Opacity
<
	__UNIFORM_SLIDER_FLOAT1

	ui_label = "Opacity";
	ui_tooltip = "Default: 0.5";
	ui_min = 0.0;
	ui_max = 1.0;
> = 0.5;

uniform float Spread
<
	__UNIFORM_SLIDER_FLOAT1
	
	ui_label = "Spread";
	ui_tooltip = "Default: 0.5";
	ui_min = 0.0;
	ui_max = 500.0;
> = 1.0;

uniform float Speed
<
	__UNIFORM_SLIDER_FLOAT1

	ui_label = "Speed";
	ui_tooltip = "Default: 1.0";
	ui_min = 0.0;
	ui_max = 1.0;
> = 1.0;

uniform float GlobalGrain
<
	__UNIFORM_SLIDER_FLOAT1

	ui_label = "Global Grain";
	ui_tooltip = "Default: 0.5";
	ui_min = 0.0;
	ui_max = 1.0;
> = 0.5;

uniform int BlendMode
<
	__UNIFORM_COMBO_INT1

	ui_label = "Blend Mode";
	ui_items = "Mix\0Addition\0Screen\0Lighten-Only\0";
> = 0;

uniform float Timer <source = "timer";>;

float rand(float2 uv, float t) {
    return frac(sin(dot(uv, float2(1225.6548, 321.8942))) * 4251.4865 + t);
}

float4 MainPS(float4 p : SV_POSITION, float2 uv : TEXCOORD) : SV_TARGET
{
	float t = Timer * 0.001 * Speed;
	float2 scale = Spread;
	float2 offset = float2(rand(uv, t), rand(uv.yx, t));
	offset = offset * scale - scale * 0.5;

	float3 grain = tex2Doffset(ReShade::BackBuffer, uv, offset).rgb;
	grain *= log10(Spread * 0.5 - distance(uv, uv + offset * ReShade::PixelSize));
	grain *= lerp(1.0, rand(uv + uv.yx, t), GlobalGrain);
	//grain *= saturate(length(offset));

	float3 color = tex2D(ReShade::BackBuffer, uv).rgb;

	switch (BlendMode)
	{
		case 0: // Mix
			color = lerp(color, max(color, grain), Opacity);
			break;
		case 1: // Addition
			color += grain * Opacity;
			break;
		case 2: // Screen
			color = 1.0 - (1.0 - color) * (1.0 - grain * Opacity);
			break;
		case 3: // Lighten-Only
			color = max(color, grain * Opacity);
			break;
	}
	
	return float4(color, 1.0);
}

technique GrainSpread
{
	pass MainPS
	{
		VertexShader = PostProcessVS;
		PixelShader = MainPS;
	}
}