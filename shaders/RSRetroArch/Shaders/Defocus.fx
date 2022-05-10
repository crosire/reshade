#include "ReShade.fxh"

uniform float2 Defocus <
	ui_type = "drag";
	ui_min  = 0.0;
	ui_max = 1.0;
	ui_step = 0.001;
	ui_label = "Defocus [Defocus]";
	ui_tooltip = "X and Y Axis Defocusing";
> = float2(0.0f, 0.0f);

// previously this pass was applied two times with offsets of 0.25, 0.5, 0.75, 1.0
// now this pass is applied only once with offsets of 0.25, 0.55, 1.0, 1.6 to achieve the same appearance as before till a maximum defocus of 2.0
static const float2 CoordOffset8[8] =
{
	// 0.075x² + 0.225x + 0.25
	float2(-1.60f,  0.25f),
	float2(-1.00f, -0.55f),
	float2(-0.55f,  1.00f),
	float2(-0.25f, -1.60f),
	float2( 0.25f,  1.60f),
	float2( 0.55f, -1.00f),
	float2( 1.00f,  0.55f),
	float2( 1.60f, -0.25f),
};

float4 PS_Defocus(float4 vpos : SV_Position, float2 texcoord : TEXCOORD) : SV_Target
{
	// imaginary texel dimensions independed from source and target dimension
	float2 TexelDims = (1.0f / 1024.0f);

	float2 DefocusTexelDims = Defocus * TexelDims;

	float3 texel = tex2D(ReShade::BackBuffer, texcoord).rgb;
	float samples = 1.0f;

	for (int i = 0; i < 8; i++)
	{
		texel += tex2D(ReShade::BackBuffer, texcoord + CoordOffset8[i] * DefocusTexelDims).rgb;
		samples += 1.0f;
	}

	return float4(texel / samples, 1.0f);
}

technique Defocus
{
	pass
	{
		VertexShader = PostProcessVS;
		PixelShader = PS_Defocus;
	}
}