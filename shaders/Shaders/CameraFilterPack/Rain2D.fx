#include "ReShade.fxh"

uniform float Fade <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 3.0;
	ui_step = 0.001;
	ui_label = "Fade [Rain 2D]";
	ui_tooltip = "The distance the rain starts to fade";
> = 1.0;

uniform float Intensity <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 1.0;
	ui_step = 0.001;
	ui_label = "Intensity [Rain 2D]";
	ui_tooltip = "The intensity of the rain";
> = 0.5;

uniform float DirectionX <
	ui_type = "drag";
	ui_min = -0.45;
	ui_max = 0.45;
	ui_step = 0.001;
	ui_label = "Direction X [Rain 2D]";
	ui_tooltip = "The X Direction of the rain.";
> = 0.120;

uniform float Size <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 2.0;
	ui_step = 0.001;
	ui_label = "Size [Rain 2D]";
	ui_tooltip = "The size of the rain droplets";
> = 1.5;

uniform float Speed <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 0.5;
	ui_step = 0.001;
	ui_label = "Speed [Rain 2D]";
	ui_tooltip = "The speed of the rain";
> = 0.275;

uniform float Distortion <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 1.0;
	ui_step = 0.001;
	ui_label = "Distortion [Rain 2D]";
	ui_tooltip = "The amount of distortion on the rain position";
> = 0.025;

uniform float StormFlashOnOff <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 1.0;
	ui_step = 0.001;
	ui_label = "Thunder Flash [Rain 2D]";
	ui_tooltip = "The time the screen flashes due to thunders.";
> = 0.0;

uniform float DropOnOff <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 1.0;
	ui_step = 0.001;
	ui_label = "Droplets [Rain2D]";
	ui_tooltip = "The amount of droplets caused by the rain.";
> = 1.0;

//SHADER START

texture tRain2DFX <source="CameraFilterPack/CameraFilterPack_Atmosphere_Rain_FX.jpg";> { Width=1024; Height=1024; Format = RGBA8;};
sampler2D sRain2DFX { Texture=tRain2DFX; MinFilter=LINEAR; MagFilter=LINEAR; MipFilter=LINEAR; AddressU = REPEAT; AddressV = REPEAT; };

uniform int _TimerX < source = "frametime"; >;

float3 getPixel(int x, int y, float2 uv)
{
	return tex2D(ReShade::BackBuffer, ((uv.xy * ReShade::ScreenSize.xy) + float2(x, y)) / ReShade::ScreenSize.xy);
}

float3 Edge(float2 uv)
{
	float extrax=2;
	float extray=3;
	float3 sum = abs(getPixel(extrax, extray, uv) - getPixel(extrax, -extray, uv));
	sum += abs(getPixel(extrax, extray, uv) - getPixel(-extrax, extray, uv));
	sum = abs(getPixel(extrax, extray, uv) - getPixel(extrax, -extray, uv));
	sum += abs(getPixel(extrax, extray, uv) - getPixel(-extrax, extray, uv));
	sum *= 1.00;
	return length(sum);			
}

float4 PS_Rain2D(float4 pos : SV_Position, float2 texcoord : TEXCOORD0) : SV_Target
{	
	float _Value = Fade;
	float _Value2 = Intensity;
	float _Value3 = DirectionX;
	float _Value4 = Speed;
	float _Value5 = Size;
	float _Value6 = Distortion;
	float _Value7 = StormFlashOnOff;
	float _Value8 = DropOnOff;
	
	
	float _TimeX;
	_TimeX += _TimerX;
	if (_TimeX > 100) _TimeX = 0;
	
	float2 uv = texcoord.xy;
	float2 uv3 = uv;
	float2 uv2 = uv*_Value5;
	_TimeX*=_Value4;
	uv2.x+=_Value3*uv2.y;
	_Value2*=0.25;
	uv2*=3;
	uv2.x+=0.1;
	uv2.y+=_TimeX*0.8;
	float3 txt =tex2D(sRain2DFX, uv2).r*0.3*_Value2;

	uv2*=0.65;
	uv2.x+=0.1;
	uv2.y+=_TimeX;
	txt+=tex2D(sRain2DFX, uv2).r*0.5*_Value2;

	uv2*=0.65;
	uv2.x+=0.1;
	uv2.y+=_TimeX*1.2;
	txt+=tex2D(sRain2DFX, uv2).r*0.7*_Value2;

	uv2*=0.5;
	uv2.x+=0.1;
	uv2.y+=_TimeX*1.2;
	txt+=tex2D(sRain2DFX, uv2).r*0.9*_Value2;

	uv2*=0.4;
	uv2.x+=0.1;
	uv2.y+=_TimeX*1.2;
	txt+=tex2D(sRain2DFX, uv2).r*0.9*_Value2;

	uv = texcoord.xy;

	float3 old=tex2D(ReShade::BackBuffer, uv);
	uv+=float2(txt.r*_Value6,txt.r*_Value6);

	float3 nt=tex2D(ReShade::BackBuffer, uv)+txt;
	txt=lerp(old,nt,_Value);
	uv = texcoord.xy*0.001;
	uv.x+=_TimeX*0.2;
	uv.y=_TimeX*0.01;

	nt=lerp(txt,txt+tex2D(sRain2DFX, uv).g*2*_Value2,_Value7);
	float3 t2=nt;
	txt = Edge(uv);
	uv = uv3;
	uv.x*=2;
	uv.x += floor(_TimeX*32)/16;
	uv.y += floor(_TimeX*32)/18;
	uv.x -= _TimeX*0.55*_Value3;
	uv.y += _TimeX*0.15;
	txt = tex2D(sRain2DFX, uv).b;
	txt = lerp(float4(0,0,0,0),txt,Edge(uv));
	txt = lerp(t2,t2+txt,_Value8);

	txt=lerp(old,txt,_Value);

	return float4(txt,1.0);
}

//techniques///////////////////////////////////////////////////////////////////////////////////////////

technique Rain2D {
	pass Rain2D {
		VertexShader=PostProcessVS;
		PixelShader=PS_Rain2D;
	}
}