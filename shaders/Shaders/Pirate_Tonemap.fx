//===================================================================================================================
#include 				"Pirate_Global.fxh" //That evil corporation~
#include				"Pirate_Tonemap.cfg"
//===================================================================================================================
uniform int TONEMAP_MODE <
	ui_label = "Tonemap - Mode";
	ui_type = "combo";
	ui_items = "Average - Desaturates\0Per Channel - Keeps saturated colors\0";
	> = 1;
uniform float TONEMAP_STRENGTH <
	ui_label = "Tonemap - Strength";
	ui_type = "drag";
	ui_min = 0.0; ui_max = 1.0;
	> = 0.2;

texture		TexTonemap <source = TONEMAP_FILENAME;> { Width = 500; Height = 1; Format = RGBA8;};
sampler2D	SamplerTonemap { Texture = TexTonemap; MinFilter = LINEAR; MagFilter = LINEAR; MipFilter = LINEAR; AddressU = Clamp; AddressV = Clamp;};

//===================================================================================================================
//===================================================================================================================
//Post Processing Effects

float4 PS_Tonemap(float4 vpos : SV_Position, float2 texcoord : TEXCOORD) : COLOR
{
	float4 ret = tex2D(SamplerColor, texcoord);
	float3 tone;
	if (TONEMAP_MODE) {
		tone.r = tex2D(SamplerTonemap, float2(saturate(ret.r), 0.5)).r;
		tone.g = tex2D(SamplerTonemap, float2(saturate(ret.g), 0.5)).g;
		tone.b = tex2D(SamplerTonemap, float2(saturate(ret.b), 0.5)).b;
	} else {
		tone = tex2D(SamplerTonemap, float2(saturate(dot(ret.rgb, 0.3333)), 0.5)).rgb;
	}
	return float4(lerp(ret.rgb, tone, TONEMAP_STRENGTH), ret.w);
}

technique Pirate_Tonemap
{
	pass Tonemap_Pass
	{
		VertexShader = VS_PostProcess;
		PixelShader  = PS_Tonemap;
	}
}