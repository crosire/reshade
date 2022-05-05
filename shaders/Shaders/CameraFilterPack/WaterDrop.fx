#include "ReShade.fxh"

uniform float Distortion <
	ui_type = "drag";
	ui_min = 8.0;
	ui_max = 64.0;
	ui_step = 0.001;
	ui_label = "Distortion [Water Drop]";
	ui_tooltip = "Amount of Distortion";
> = 8.0;

uniform float SizeX <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 7.0;
	ui_step = 0.001;
	ui_label = "Size X [Water Drop]";
	ui_tooltip = "Horizontal Drop Size";
> = 1.0;

uniform float SizeY <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 7.0;
	ui_step = 0.001;
	ui_label = "Size Y [Water Drop]";
	ui_tooltip = "Amount of Vertical Size";
> = 0.5;

uniform float Speed <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 10.0;
	ui_step = 0.001;
	ui_label = "Speed [Water Drop]";
	ui_tooltip = "Speed of the waterdrops";
> = 1.0;

//Shader Start

texture tWaterDrop <source="CameraFilterPack/CameraFilterPack_WaterDrop.png";> { Width=512; Height=512;};
sampler2D sWaterDrop { Texture=tWaterDrop; MinFilter=LINEAR; MagFilter=LINEAR; MipFilter=LINEAR; AddressU = REPEAT; AddressV = REPEAT; };

uniform int _TimerX < source = "frametime"; >;

float4 PS_WaterDrop(float4 pos : SV_Position, float2 texcoord : TEXCOORD0) : SV_Target
{
	float _TimeX;
	_TimeX += _TimerX;
	if (_TimeX > 100) _TimeX = 0;
	
	float2 uv = texcoord.xy;
	
	float3 raintex = tex2D(sWaterDrop,float2(uv.x*1.3*SizeX,(uv.y*SizeY*1.4)+_TimeX*Speed*0.125)).rgb/Distortion;
	float3 raintex2 = tex2D(sWaterDrop,float2(uv.x*1.15*SizeX-0.1,(uv.y*SizeY*1.1)+_TimeX*Speed*0.225)).rgb/Distortion;
	float3 raintex3 = tex2D(sWaterDrop,float2(uv.x*SizeX-0.2,(uv.y*SizeY)+_TimeX*Speed*0.025)).rgb/Distortion;
	float2 where = uv.xy-(raintex.xy-raintex2.xy-raintex3.xy)/3;
	float3 col = tex2D(ReShade::BackBuffer,float2(where.x,where.y)).rgb;
	
	return float4(col, 1.0);
}

//techniques///////////////////////////////////////////////////////////////////////////////////////////

technique WaterDrop {
	pass WaterDrop {
		VertexShader=PostProcessVS;
		PixelShader=PS_WaterDrop;
	}
}