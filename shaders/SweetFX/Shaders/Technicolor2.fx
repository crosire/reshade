/**
 * Technicolor2 version 1.0
 * Original by Prod80
 * Optimized by CeeJay.dk
 */

#include "ReShadeUI.fxh"

uniform float3 ColorStrength < __UNIFORM_COLOR_FLOAT3
	ui_tooltip = "Higher means darker and more intense colors.";
> = float3(0.2, 0.2, 0.2);

uniform float Brightness < __UNIFORM_SLIDER_FLOAT1
	ui_min = 0.5; ui_max = 1.5;
	ui_tooltip = "Higher means brighter image.";
> = 1.0;
uniform float Saturation < __UNIFORM_SLIDER_FLOAT1
	ui_min = 0.0; ui_max = 1.5;
	ui_tooltip = "Additional saturation control since this effect tends to oversaturate the image.";
> = 1.0;

uniform float Strength < __UNIFORM_SLIDER_FLOAT1
	ui_min = 0.0; ui_max = 1.0;
	ui_tooltip = "Adjust the strength of the effect.";
> = 1.0;

#include "ReShade.fxh"

float3 TechnicolorPass(float4 vpos : SV_Position, float2 texcoord : TexCoord) : SV_Target
{
	float3 color = saturate(tex2D(ReShade::BackBuffer, texcoord).rgb);
	
	float3 temp = 1.0 - color;
	float3 target = temp.grg;
	float3 target2 = temp.bbr;
	float3 temp2 = color * target;
	temp2 *= target2;

	temp = temp2 * ColorStrength;
	temp2 *= Brightness;

	target = temp.grg;
	target2 = temp.bbr;

	temp = color - target;
	temp += temp2;
	temp2 = temp - target2;

	color = lerp(color, temp2, Strength);
	color = lerp(dot(color, 0.333), color, Saturation);

	return color;
}

technique Technicolor2
{
	pass
	{
		VertexShader = PostProcessVS;
		PixelShader = TechnicolorPass;
	}
}
