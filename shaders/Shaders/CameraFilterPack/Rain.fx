#include "ReShade.fxh"

uniform float fixDistance <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 100.0;
	ui_step = 0.001;
	ui_label = "Distance [Rain]";
	ui_tooltip = "Changes the distance the Rain gets rendered";
> = 3.0;

uniform float Fade <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 3.0;
	ui_step = 0.001;
	ui_label = "Fade [Rain]";
	ui_tooltip = "The distance the rain starts to fade";
> = 1.0;

uniform float Intensity <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 1.0;
	ui_step = 0.001;
	ui_label = "Intensity [Rain]";
	ui_tooltip = "The intensity of the rain";
> = 0.5;

uniform float DirectionX <
	ui_type = "drag";
	ui_min = -0.45;
	ui_max = 0.45;
	ui_step = 0.001;
	ui_label = "Direction X [Rain]";
	ui_tooltip = "The X Direction of the rain.";
> = 0.120;

uniform float Size <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 2.0;
	ui_step = 0.001;
	ui_label = "Size [Rain]";
	ui_tooltip = "The size of the rain droplets";
> = 1.5;

uniform float Speed <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 0.5;
	ui_step = 0.001;
	ui_label = "Speed [Rain]";
	ui_tooltip = "The speed of the rain";
> = 0.275;

uniform float Distortion <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 1.0;
	ui_step = 0.001;
	ui_label = "Distortion [Rain]";
	ui_tooltip = "The amount of distortion on the rain position";
> = 0.025;

uniform float StormFlashOnOff <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 1.0;
	ui_step = 0.001;
	ui_label = "Thunder Flash [Rain]";
	ui_tooltip = "The time the screen flashes due to thunders.";
> = 0.0;

uniform float DropOnOff <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 1.0;
	ui_step = 0.001;
	ui_label = "Droplets [Rain]";
	ui_tooltip = "The amount of droplets caused by the rain.";
> = 1.0;

uniform float Drop_Near <
	ui_type = "drag";
	ui_min = -0.5;
	ui_max = 1.0;
	ui_step = 0.001;
	ui_label = "Near Drop [Rain]";
	ui_tooltip = "The position raindrops start to fall";
> = 0.0;

uniform float Drop_Far <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 1.0;
	ui_step = 0.001;
	ui_label = "Far Drop [Rain]";
	ui_tooltip = "The position raindrops stops falling";
> = 0.5;

uniform float Drop_With_Obj <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 1.0;
	ui_step = 0.001;
	ui_label = "Object Drops [Rain]";
> = 0.2;

uniform float Myst <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 1.0;
	ui_label = "Myst Intensity [Rain]";
	ui_tooltip = "Intensity of the rain myst";
> = 0.1;

uniform float Drop_Floor_Fluid <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 1.0;
	ui_label = "Floor Fluid [Rain]";
	ui_tooltip = "Intensity of rain pools";
> = 0.0;

uniform float3 Myst_Color <
	ui_type = "color";
	ui_label = "Myst Color [Rain]";
> = float3(0.5,0.5,0.5);

uniform bool Debug <
	ui_type = "boolean";
	ui_tooltip = "Debug Mode [Rain]";
> = true;

//SHADER START

texture tRainFX <source="CameraFilterPack/CameraFilterPack_Atmosphere_Rain_FX.jpg";> { Width=1024; Height=1024; Format = RGBA8;};
sampler2D sRainFX { Texture=tRainFX; MinFilter=LINEAR; MagFilter=LINEAR; MipFilter=LINEAR; AddressU = REPEAT; AddressV = REPEAT; };

uniform int _TimerX < source = "frametime"; >;

float GetLinearDepth(float2 coords)
{
	return ReShade::GetLinearizedDepth(coords);
}

float3 getPixel(int x, int y, float2 uv)
{
	return tex2D(ReShade::BackBuffer, ((uv.xy * ReShade::ScreenSize.xy) + float2(x, y)) / ReShade::ScreenSize.xy);
}

float3 Edge(float2 uv)
{
	float extrax=2;
	float extray=4;
	float3 sum = abs(getPixel(extrax, extray, uv) - getPixel(extrax, -extray, uv));
	sum += abs(getPixel(extrax, extray, uv) - getPixel(-extrax, extray, uv));
	sum = abs(getPixel(extrax, extray, uv) - getPixel(extrax, -extray, uv));
	sum += abs(getPixel(extrax, extray, uv) - getPixel(-extrax, extray, uv));
	sum *= 1.00;
	return length(sum);			
}

float4 PS_RainPro(float4 pos : SV_Position, float2 texcoord : TEXCOORD0) : SV_Target
{

	float2 uv = texcoord.xy;
	float TimerX;
	TimerX += _TimerX;
	if (TimerX >100) TimerX = 0;
	
	float depth = GetLinearDepth(uv.xy) * RESHADE_DEPTH_LINEARIZATION_FAR_PLANE;
	depth/=fixDistance*10;
	depth=saturate(depth);
	depth = smoothstep(Drop_Near,Drop_Far,depth);

	float4 result=float4(1-depth,1-depth,1-depth,1);
	float dp = depth;
	float2 uv3 = uv;
	
	float _Value = Fade;
	float _Value2 = Intensity;
	float _Value3 = DirectionX;
	float _Value4 = Speed;
	float _Value5 = Size;
	float _Value6 = fixDistance;
	float _Value7 = StormFlashOnOff;
	float _Value8 = DropOnOff;

	_Value3=lerp(_Value3,0,Drop_Floor_Fluid*dp);

	float2 uv2 = uv*Size;
	TimerX*=Speed;
	uv2.x+=_Value3*uv2.y;
	_Value2*=0.25;
	uv2*=3;
	uv2.x+=0.1;
	uv2.y+=TimerX*0.8;
	float3 txt =tex2D(sRainFX, uv2).r*0.3*_Value2;
	uv2*=0.65;
	uv2.x+=0.1;
	uv2.y+=TimerX;
	txt+=tex2D(sRainFX, uv2).r*0.5*_Value2;
	uv2*=0.65;
	uv2.x+=0.1;
	uv2.y+=TimerX*1.2;
	txt+=tex2D(sRainFX, uv2).r*0.7*_Value2;
	uv2*=0.5;
	uv2.x+=0.1;
	uv2.y+=TimerX*1.2;
	txt+=tex2D(sRainFX, uv2).r*0.9*_Value2;
	uv2*=0.4;
	uv2.x+=0.1;
	uv2.y+=TimerX*1.2;
	txt+=tex2D(sRainFX, uv2).r*0.9*_Value2;
	txt=lerp(txt,0,Drop_With_Obj);
	uv = texcoord.xy;
	float3 old=tex2D(ReShade::BackBuffer, uv);
	_Value6*=depth;
	uv+=float2(txt.r*_Value6,txt.r*_Value6);
	float3 nt=tex2D(ReShade::BackBuffer, uv)+txt;
	txt=lerp(old,nt,_Value);
	uv = texcoord.xy*0.001;
	uv.x+=TimerX*0.2;
	uv.y=TimerX*0.01;
	nt=lerp(txt,txt+tex2D(sRainFX, uv).g*2*_Value2,_Value7);
	float3 t2=nt;
	txt = Edge(uv);
	float tmp=txt;
	uv = uv3+(float2(0.5,0.5)*0.8);
	uv.x*=2;
	float dpx=dp;
	dpx=floor(dpx*16)/16;
	uv/=dpx;
	uv.x += floor(TimerX*64)/64;
	uv.y += floor(TimerX*64)/16;
	uv.x -= TimerX*0.55*_Value3;
	uv.y -= TimerX*0.15;
	txt = tex2D(sRainFX, uv).b;
	txt = lerp(float4(0,0,0,0),txt,tmp);
	txt = lerp(t2,t2+txt*0.25,_Value8);
	txt=lerp(old,txt,_Value);
	txt=lerp(txt,Myst_Color,(1-dp)*Myst);
	
	if (Debug) {
		return dp;
	} else {
		return float4(txt,1.0);
	}

}

//techniques///////////////////////////////////////////////////////////////////////////////////////////

technique Rain {
	pass Rain {
		VertexShader=PostProcessVS;
		PixelShader=PS_RainPro;
	}
}