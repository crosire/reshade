//===================================================================================================================
#include 	"Pirate_Global.fxh" //That evil corporation~
#include	"Pirate_Lumachroma.fxh"
//===================================================================================================================
uniform int CURVE_METHOD <
	ui_label = "Contrast Curve - Light Method";
	ui_type = "combo";
	ui_items = "Luma\0Chroma\0Luma & Chroma\0";
	> = 0;
uniform float CURVE_STRENGTH <
	ui_label = "Contrast Curve - Strength";
	ui_type = "drag";
	ui_min = -1.0; ui_max = 4.0;
	ui_tooltip = "0.0 - No effect, >0.0 - More contrast, <0.0 - Less contrast.";
	> = 0.2;
//===================================================================================================================
float4 PS_Curve(float4 vpos : SV_Position, float2 texcoord : TEXCOORD) : COLOR
{
	float4 ret = tex2D(SamplerColor, texcoord);
	float4 lumachroma = LumaChroma(ret);
	
	if (CURVE_METHOD == 0)
		lumachroma.w = pow(lumachroma.w, 1.0 + CURVE_STRENGTH);
	else if (CURVE_METHOD == 1)
		lumachroma.rgb = pow(lumachroma.rgb, 1.0 + CURVE_STRENGTH);
	else if (CURVE_METHOD == 2)
		lumachroma = pow(lumachroma, 1.0 + CURVE_STRENGTH);

	
	ret.rgb = lumachroma.w * lumachroma.rgb;
	
	return saturate(ret);
}

//===================================================================================================================
//===================================================================================================================

technique Pirate_Curve
{
	pass Curve
	{
		VertexShader = VS_PostProcess;
		PixelShader  = PS_Curve;
	}
}