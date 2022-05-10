/**
 * DPX/Cineon shader by Loadus
 */

#include "ReShadeUI.fxh"

uniform float3 RGB_Curve < __UNIFORM_SLIDER_FLOAT3
	ui_min = 1.0; ui_max = 15.0;
	ui_label = "RGB Curve";
> = float3(8.0, 8.0, 8.0);
uniform float3 RGB_C < __UNIFORM_SLIDER_FLOAT3
	ui_min = 0.2; ui_max = 0.5;
	ui_label = "RGB C";
> = float3(0.36, 0.36, 0.34);

uniform float Contrast < __UNIFORM_SLIDER_FLOAT1
	ui_min = 0.0; ui_max = 1.0;
> = 0.1;
uniform float Saturation < __UNIFORM_SLIDER_FLOAT1
	ui_min = 0.0; ui_max = 8.0;
> = 3.0;
uniform float Colorfulness < __UNIFORM_SLIDER_FLOAT1
	ui_min = 0.1; ui_max = 2.5;
> = 2.5;

uniform float Strength < __UNIFORM_SLIDER_FLOAT1
	ui_min = 0.0; ui_max = 1.0;
	ui_tooltip = "Adjust the strength of the effect.";
> = 0.20;

#include "ReShade.fxh"

static const float3x3 RGB = float3x3(
	 2.6714711726599600, -1.2672360578624100, -0.4109956021722270,
	-1.0251070293466400,  1.9840911624108900,  0.0439502493584124,
	 0.0610009456429445, -0.2236707508128630,  1.1590210416706100
);
static const float3x3 XYZ = float3x3(
	 0.5003033835433160,  0.3380975732227390,  0.1645897795458570,
	 0.2579688942747580,  0.6761952591447060,  0.0658358459823868,
	 0.0234517888692628,  0.1126992737203000,  0.8668396731242010
);

float3 DPXPass(float4 vois : SV_Position, float2 texcoord : TexCoord) : SV_Target
{
	float3 input = tex2D(ReShade::BackBuffer, texcoord).rgb;

	float3 B = input;
	B = B * (1.0 - Contrast) + (0.5 * Contrast);
	float3 Btemp = (1.0 / (1.0 + exp(RGB_Curve / 2.0)));
	B = ((1.0 / (1.0 + exp(-RGB_Curve * (B - RGB_C)))) / (-2.0 * Btemp + 1.0)) + (-Btemp / (-2.0 * Btemp + 1.0));

	float value = max(max(B.r, B.g), B.b);
	float3 color = B / value;
	color = pow(abs(color), 1.0 / Colorfulness);

	float3 c0 = color * value;
	c0 = mul(XYZ, c0);
	float luma = dot(c0, float3(0.30, 0.59, 0.11));
	c0 = (1.0 - Saturation) * luma + Saturation * c0;
	c0 = mul(RGB, c0);

	return lerp(input, c0, Strength);
}

technique DPX
{
	pass
	{
		VertexShader = PostProcessVS;
		PixelShader = DPXPass;
	}
}
