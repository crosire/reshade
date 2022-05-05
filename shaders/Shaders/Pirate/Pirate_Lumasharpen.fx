//===================================================================================================================
#include	"Pirate_Global.fxh" //That evil corporation~
#include	"Pirate_Lumachroma.fxh"
//===================================================================================================================
uniform float LUMASHARPEN_RADIUS <
	ui_label = "Luma Sharpen - Radius";
	ui_type = "drag";
	ui_min = 0.0001; ui_max = 2.0;
	ui_tooltip = "Values larger than 1.0 will give blurry results.";
	> = 0.5;
uniform float LUMASHARPEN_STRENGTH <
	ui_label = "Luma Sharpen - Strength";
	ui_type = "drag";
	ui_min = 0.0001; ui_max = 2.0;
	ui_tooltip = "Too much strength will just create artefacts.";
	> = 0.5;
uniform bool LUMASHARPEN_DEBUG <
	ui_label = "Luma Sharpen - Debug";
	ui_tooltip = "Turn off FXAA to see this.";
	> = false;
//===================================================================================================================
float4 PS_Lumasharpen(float4 vpos : SV_Position, float2 texcoord : TEXCOORD) : COLOR
{
	float4 ret = tex2D(SamplerColor, texcoord);
	
	const float2 tap[4] = {
		float2(-1.0, -1.0),
		float2( 1.0, -1.0),
		float2( 1.0,  1.0),
		float2(-1.0,  1.0)
	};
	
	float mask;
	
	for(int i=0; i < 4; i++) {
		mask += LumaChroma(tex2D(SamplerColor, texcoord + tap[i] * PixelSize * LUMASHARPEN_RADIUS)).w;
	}

	float4 lumachroma = LumaChroma(ret);
	mask += lumachroma.w;
	mask /= 5;
	//mask -= lumachroma.w;
	//lumachroma.w += (1.0 - lumachroma.w) * (max(mask, 0.0) * LUMASHARPEN_STRENGTH);
	lumachroma.w += (lumachroma.w - mask) * LUMASHARPEN_STRENGTH;
	ret.rgb = lumachroma.w * lumachroma.rgb;
	ret.w = 1.0;
	
	if (LUMASHARPEN_DEBUG) ret.rgb = (mask - LumaChroma(tex2D(SamplerColor, texcoord)).w) * LUMASHARPEN_STRENGTH;
	
	return ret;
}
//===================================================================================================================
technique Pirate_LumaSharpen
{
	pass LumaSharpen
	{
		VertexShader = VS_PostProcess;
		PixelShader  = PS_Lumasharpen;
	}
}