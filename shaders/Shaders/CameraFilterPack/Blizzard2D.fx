#include "ReShade.fxh"

uniform float Speed <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 0.5;
	ui_step = 0.001;
	ui_label = "Speed [Blizzard]";
	ui_tooltip = "The speed of the snow";
> = 0.275;

uniform float Size <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 2.0;
	ui_step = 0.001;
	ui_label = "Size [Blizzard]";
	ui_tooltip = "The size of the snow";
> = 1.5;

uniform float Fade <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 3.0;
	ui_step = 0.001;
	ui_label = "Fade [Blizzard]";
	ui_tooltip = "The snow intensity";
> = 1.0;

uniform int _TimerX < source = "frametime"; >;

texture tBlizzard2DFX <source="CameraFilterPack/CameraFilterPack_Blizzard1.jpg";> { Width=1024; Height=1024; Format = RGBA8;};
sampler2D sBlizzard2DFX { Texture=tBlizzard2DFX; MinFilter=LINEAR; MagFilter=LINEAR; MipFilter=LINEAR; AddressU = REPEAT; AddressV = REPEAT; };

float4 PS_Blizzard2D(float4 pos : SV_Position, float2 texcoord : TEXCOORD0) : SV_Target
{	
	float _Value = Speed;
	float _Value2 = Size;
	float _Value3 = Fade;
	
	float2 uvx = texcoord.xy;
	float2 uv=uvx;
	
	float _TimeX;
	_TimeX += _TimerX;
	if (_TimeX > 100) _TimeX = 0;
	
	float3 col = tex2D(ReShade::BackBuffer,uv).rgb;
	float timx=_TimeX*_Value;
	float tx=timx;
	float t = 1+(tx * sin(tx)/16);
	uv.x+=t; uv.x-=tx+sin(uv.x+tx/16)/16; uv.y+=tx;
	uv.y=uv.y+(uv.x* t/16)/2;
	float4 col2=float4(0,0,0,0);
	col2.r = tex2D(sBlizzard2DFX,uv*_Value2).r;
	uv=uvx;
	tx=timx/2;
	t = 1+(tx * sin(tx)/4);
	uv.x+=t; uv.x-=tx+sin(uv.x+tx/8)/8; uv.y+=tx;
	col2.g = tex2D(sBlizzard2DFX,uv*_Value2).g;
	uv=uvx*2;
	tx=timx;
	t = 1+(tx * sin(tx)/2);
	uv.x+=t; uv.x-=tx+sin(uv.x+tx/12)/8; uv.y+=tx;
	uv.y=uv.y+(uv.x* t)/64;
	col2.b = tex2D(sBlizzard2DFX,uv*_Value2).b;
	uv=uvx/2;
	tx=timx/3;
	t = 1+(tx * sin(tx)/3);
	uv.x+=t; uv.x-=tx+sin(uv.x+tx/6)/12; uv.y+=tx;
	col2.a = tex2D(sBlizzard2DFX,uv*_Value2).g*2;

	col2.r=max(col2.r*sin(t/10),0);
	col2.b=max(col2.b*sin(2+t/64),0);

	col = lerp(col,col.rgb+(col2.r+col2.g+col2.b+col2.a)/4,_Value3);

	return float4(col, 1.0);
}

//techniques///////////////////////////////////////////////////////////////////////////////////////////

technique Blizzard2D {
	pass Blizzard2D {
		VertexShader=PostProcessVS;
		PixelShader=PS_Blizzard2D;
	}
}