#include "ReShade.fxh"

uniform bool NoiseDistort <
	ui_type = "boolean";
	ui_label = "Enable Distortion [RealVHS Noise]";
> = false;

uniform float TRACKING <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 1.0;
	ui_step = 0.001;
	ui_label = "Tracking [RealVHS Noise]";
	ui_tooltip = "Amount of VHS tracks";
> = 0.212;

uniform float GLITCH <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 1.0;
	ui_step = 0.001;
	ui_label = "Glitch [RealVHS Noise]";
	ui_tooltip = "Amount of the VHS Glitching";
> = 1.0;

uniform float NOISE <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 1.0;
	ui_step = 0.001;
	ui_label = "Noise [RealVHS Noise]";
	ui_tooltip = "Amount of VHS Noise";
> = 1.0;

//SHADER START
uniform float Timer < source = "timer"; >;

texture tVHSNoise1 <source="CameraFilterPack_VHS1.jpg";> { Width=2048; Height=2048; Format = RGBA8;};
texture tVHSNoise2 <source="CameraFilterPack_VHS2.jpg";> { Width=2048; Height=2048; Format = RGBA8;};

sampler2D VHSNoise { Texture=tVHSNoise1; MinFilter=LINEAR; MagFilter=LINEAR; MipFilter=LINEAR; AddressU = REPEAT; AddressV = REPEAT; };
sampler2D VHSNoise2 { Texture=tVHSNoise2; MinFilter=LINEAR; MagFilter=LINEAR; MipFilter=LINEAR; AddressU = REPEAT; AddressV = REPEAT; };

float fmod(float a, float b) {
	float c = frac(abs(a / b)) * abs(b);
	return a < 0 ? -c : c;
}

float hardLight_s( float s, float d )
{
	return (s < 0.5) ? 2.0 * s * d : 1.0 - 2.0 * (1.0 - s) * (1.0 - d);
}

float3 hardLight( float3 s, float3 d )
{
	float3 c;
	c.x = hardLight_s(s.x,d.x);
	c.y = hardLight_s(s.y,d.y);
	c.z = hardLight_s(s.z,d.z);
	return c;
}

float4 PS_TapeNoiseVHS(float4 pos : SV_Position, float2 texcoord : TEXCOORD0) : SV_Target
{
	float2 uv = texcoord.xy;
	float _Time = Timer*0.001;
	float3 col = tex2D(ReShade::BackBuffer,uv).rgb;
	uv = texcoord.xy/8;
	float tm=(_Time)*30;
	uv.x += floor(fmod(tm, 1.0)*8)/8;
	uv.y += (1-floor(fmod(tm/8, 1.0)*8)/8);
	float4 t2 =  tex2D(VHSNoise, uv);
	t2 = lerp(float4(0.5, 0.5, 0.5, 1), t2, NOISE);
	col=hardLight(col,t2);
	uv = texcoord.xy/8;
	tm=(_Time)*30;
	uv.x += floor(fmod(tm, 1.0)*8)/8;
	uv.y += (1-floor(fmod(tm/8, 1.0)*8)/8);
	uv = lerp(texcoord, uv, GLITCH);
	t2 = tex2D(VHSNoise2, uv);
	uv = texcoord.xy;
	col=lerp(col,col+t2,TRACKING);
	return float4(col,1.0);
}

//techniques///////////////////////////////////////////////////////////////////////////////////////////

technique TapeNoiseVHS {
	pass TapeNoiseVHS {
		VertexShader=PostProcessVS;
		PixelShader=PS_TapeNoiseVHS;
	}
}