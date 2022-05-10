/**
 * Cartoon
 * by Christian Cann Schuldt Jensen ~ CeeJay.dk
 */

#include "ReShadeUI.fxh"

uniform float Power < __UNIFORM_SLIDER_FLOAT1
	ui_min = 0.1; ui_max = 10.0;
	ui_tooltip = "Amount of effect you want.";
> = 1.5;
uniform float EdgeSlope < __UNIFORM_SLIDER_FLOAT1
	ui_min = 0.1; ui_max = 6.0;
	ui_label = "Edge Slope";
	ui_tooltip = "Raise this to filter out fainter edges. You might need to increase the power to compensate. Whole numbers are faster.";
> = 1.5;

#include "ReShade.fxh"

float3 CartoonPass(float4 position : SV_Position, float2 texcoord : TEXCOORD) : SV_Target
{
	float3 color = tex2D(ReShade::BackBuffer, texcoord).rgb;
	const float3 coefLuma = float3(0.2126, 0.7152, 0.0722);

	float diff1 = dot(coefLuma, tex2D(ReShade::BackBuffer, texcoord + BUFFER_PIXEL_SIZE).rgb);
	diff1 = dot(float4(coefLuma, -1.0), float4(tex2D(ReShade::BackBuffer, texcoord - BUFFER_PIXEL_SIZE).rgb , diff1));
	float diff2 = dot(coefLuma, tex2D(ReShade::BackBuffer, texcoord + BUFFER_PIXEL_SIZE * float2(1, -1)).rgb);
	diff2 = dot(float4(coefLuma, -1.0), float4(tex2D(ReShade::BackBuffer, texcoord + BUFFER_PIXEL_SIZE * float2(-1, 1)).rgb , diff2));

	float edge = dot(float2(diff1, diff2), float2(diff1, diff2));

	return saturate(pow(abs(edge), EdgeSlope) * -Power + color);
}

technique Cartoon
{
	pass
	{
		VertexShader = PostProcessVS;
		PixelShader = CartoonPass;
	}
}
