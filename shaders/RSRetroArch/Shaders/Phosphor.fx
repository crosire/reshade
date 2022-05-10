#include "ReShade.fxh"

uniform float PTime <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 1.0;
	ui_step = 0.001;
	ui_label = "Persistence Time [Phosphor]";
	ui_tooltip = "How slow the phosphor vanishes (Higher = less ghosting).";
> = 0.0;

uniform float3 Phosphor <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 1.0;
	ui_step = 0.001;
	ui_label = "Phosphor Color [Phosphor]";
	ui_tooltip = "Changes the RGB of the persistance color.";
> = float3(0.45, 0.45, 0.45);

static const float F = 30.0;
uniform float FTimer < source = "frametime"; >;

texture2D tPrevTex { Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = RGBA8; };
sampler sPrevTex { Texture=tPrevTex; };

float3 PrevColor(float4 pos : SV_Position, float2 uv : TEXCOORD0) : SV_Target
{
	return tex2D(ReShade::BackBuffer, uv).rgb;
}

float4 PS_PhosphorMain(float4 pos : SV_Position, float2 uv : TEXCOORD0) : SV_Target{
	float4 CurrY = tex2D(ReShade::BackBuffer, uv);
	float3 PrevY = tex2D(sPrevTex, uv).rgb;

	PrevY[0] *= Phosphor[0] == 0.0 ? 0.0 : pow(Phosphor[0], F * FTimer);
	PrevY[1] *= Phosphor[1] == 0.0 ? 0.0 : pow(Phosphor[1], F * FTimer);
	PrevY[2] *= Phosphor[2] == 0.0 ? 0.0 : pow(Phosphor[2], F * FTimer);
	float a = max(PrevY[0], CurrY[0]);
	float b = max(PrevY[1], CurrY[1]);
	float c = max(PrevY[2], CurrY[2]);
	float3 col = lerp(float3(a,b,c).rgb,CurrY,PTime); //Intensity Toggle
	//float3 col = lerp(float3(a,b,c).rgb,CurrY,float3(a,b,c).rgb); //Intensity depends on color, 1 will make eternal burn.
	//float3 col = lerp(float3(a,b,c).rgb,CurrY,PrevY); //Best of both.
	return float4(col.rgb, CurrY.a);
}

technique PhosphorMAME
{
	pass PS_PhosphorMain
	{
		VertexShader=PostProcessVS;
		PixelShader=PS_PhosphorMain;
	}
	
	pass SaveBuffer {
		VertexShader=PostProcessVS;
		PixelShader=PrevColor;
		RenderTarget = tPrevTex;
	}
}