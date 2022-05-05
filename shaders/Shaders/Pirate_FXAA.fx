//===================================================================================================================
#include "Pirate_Global.fxh"
//===================================================================================================================

uniform float FXAA_RADIUS <
	ui_label = "FXAA - Radius";
	ui_type = "drag";
	ui_min = 0.5; ui_max = 2.0;
	ui_tooltip = "Try to keep close to 1.0.";
	> = 1.0;
uniform float FXAA_STRENGTH <
	ui_label = "FXAA - Strength";
	ui_type = "drag";
	ui_min = 0.0001; ui_max = 1.0;
	ui_tooltip = "Self Explanatory.";
	> = 1.0;
uniform bool FXAA_DEBUG <
	ui_label = "FXAA - Debug";
	ui_tooltip = "Shows which area of the screen is being blurred.";
	> = false;

//===================================================================================================================
//===================================================================================================================

float4 FastFXAA(float4 colorIN : COLOR, float2 coord : TEXCOORD) : COLOR {
	const float2 tap[8] = {
		float2(1.0, 0.0),
		float2(-1.0, 0.0),
		float2(0.0, 1.0),
		float2(0.0, -1.0),
		float2(-1.0, -1.0),
		float2( 1.0, -1.0),
		float2( 1.0,  1.0),
		float2(-1.0,  1.0)
	};
	float4 ret;
	float edge;
	float3 blur = colorIN.rgb;
	float intensity = dot(blur, 0.3333);
	
	for(int i=0; i < 8; i++) {
		ret = tex2D(SamplerColor, coord + tap[i] * PixelSize * FXAA_RADIUS);
		float weight = abs(intensity - dot(ret.rgb, 0.33333));
		blur = lerp(blur, ret.rgb, weight / 8);
		edge += weight;
	}
	
	edge /= 8;
	ret.rgb = lerp(colorIN.rgb, blur, FXAA_STRENGTH);
	
	if (FXAA_DEBUG)	ret.rgb = edge;
	
	return ret;
}

//===================================================================================================================
//===================================================================================================================

float4 PS_FXAA(float4 vpos : SV_Position, float2 texcoord : TEXCOORD) : COLOR
{
	return FastFXAA(tex2D(SamplerColor, texcoord), texcoord);
}

//===================================================================================================================
//===================================================================================================================

technique Pirate_FXAA
{
	pass FXAA_Pass
	{
		VertexShader = VS_PostProcess;
		PixelShader  = PS_FXAA;
	}
}