//===================================================================================================================
#include 				"Pirate_Global.fxh" //That evil corporation~
//===================================================================================================================
uniform float VIGNETTE_RANGE <
	ui_label = "Vignette - Range";
	ui_type = "drag";
	ui_min = 0.0001; ui_max = 1.0;
	ui_tooltip = "Distance from the center of the screen where the vignette should start.";
	> = 0.7;
uniform float VIGNETTE_CURVE <
	ui_label = "Vignette - Curve";
	ui_type = "drag";
	ui_min = 0.0001; ui_max = 4.0;
	ui_tooltip = "Contrast curve of the vignette.";
	> = 1.5;
uniform float VIGNETTE_STRENGTH <
	ui_label = "Vignette - Strength";
	ui_type = "drag";
	ui_min = 0.0001; ui_max = 4.0;
	ui_tooltip = "Strength of the vignette.";
	> = 1.0;
uniform int VIGNETTE_BLEND <
	ui_label = "Vignette - Blending mode";
	ui_type = "combo";
	ui_items = "Subtract\0Multiply\0Color Dodge\0";
	> = 0;
//===================================================================================================================
float Vignette(float2 coords : TEXCOORD){
	coords = coords * 2.0 - 1.0;
	coords = max(0.0, length(coords) - VIGNETTE_RANGE);
	return pow(coords.x, VIGNETTE_CURVE) * VIGNETTE_STRENGTH;
}
//===================================================================================================================
float4 PS_Vignette(float4 vpos : SV_Position, float2 texcoord : TEXCOORD) : COLOR
{
	float4 ret = tex2D(SamplerColor, texcoord);

	if (VIGNETTE_BLEND == 0) ret.rgb -= Vignette(texcoord);
	else if (VIGNETTE_BLEND == 1) ret.rgb *= 1.0 - Vignette(texcoord);
	else ret.rgb = BlendColorBurn(ret.rgb, 1.0 - saturate(Vignette(texcoord)));

	return saturate(ret);
}
//===================================================================================================================
technique Pirate_Vignette
{
	pass Vignette
	{
		VertexShader = VS_PostProcess;
		PixelShader  = PS_Vignette;
	}
}