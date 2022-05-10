//===================================================================================================================
#include 	"Pirate_Global.fxh" //That evil corporation~
#include	"Pirate_Lumachroma.fxh"
//===================================================================================================================
uniform float VIBRANCE_STRENGTH <
	ui_label = "Vibrance - Strength";
	ui_type = "drag";
	ui_min = -1.0; ui_max = 10.0;
	ui_tooltip = "0.0 - No effect, >0.0 - Saturates, <0.0 - Desaturates.";
	> = 0.3;
//===================================================================================================================
float4 PS_Vibrance(float4 vpos : SV_Position, float2 texcoord : TEXCOORD) : COLOR
{
	float4 ret = tex2D(SamplerColor, texcoord);
	float4 lumasat; //x = luma, y = max color, z = min color, w = saturation
	lumasat.x = LumaChroma(ret).w;
	lumasat.y = max(max(ret.r, ret.g), ret.b);
	lumasat.z = min(min(ret.r, ret.g), ret.b);
	lumasat.w = lumasat.y - lumasat.z;
	ret.rgb = lerp(lumasat.x, ret.rgb, (1.0 + (VIBRANCE_STRENGTH * (1.0 - (sign(VIBRANCE_STRENGTH) * lumasat.w)))));
	ret.w = 1.0;
	return ret;
}
//===================================================================================================================
technique Pirate_Vibrance
{
	pass Vibrance
	{
		VertexShader = VS_PostProcess;
		PixelShader  = PS_Vibrance;
	}
}