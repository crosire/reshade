#include "ReShade.fxh"

uniform float Size <
	ui_type = "drag";
	ui_min = 0.4;
	ui_max = 5.0;
	ui_step = 0.001;
	ui_label = "Size [Digital Matrix]";
	ui_tooltip = "Size of the falling numbers.";
> = 1.0;

uniform float Speed <
	ui_type = "drag";
	ui_min = -1.0;
	ui_max = 1.0;
	ui_step = 0.001;
	ui_label = "Speed [Digital Matrix]";
	ui_tooltip = "Speed that the numbers fall";
> = 1.0;

uniform float ColorR <
	ui_type = "drag";
	ui_min = -1.0;
	ui_max = 1.0;
	ui_step = 0.001;
	ui_label = "Red [Digital Matrix]";
	ui_tooltip = "Red Color Value";
> = -1.0;

uniform float ColorG <
	ui_type = "drag";
	ui_min = -1.0;
	ui_max = 1.0;
	ui_step = 0.001;
	ui_label = "Green [Digital Matrix]";
	ui_tooltip = "Green Color Value";
> = 1.0;

uniform float ColorB <
	ui_type = "drag";
	ui_min = -1.0;
	ui_max = 1.0;
	ui_step = 0.001;
	ui_label = "Blue [Digital Matrix]";
	ui_tooltip = "Blue Color Value";
> = -1.0;


float fmod(float a, float b)
{
  float c = frac(abs(a/b))*abs(b);
  return (a < 0) ? -c : c;   /* if ( a < 0 ) c = 0-c */
}

uniform int _TimeX < source = "frametime"; >;

#define R(v) fmod(4e4*sin(dot(ceil(v),float2(12,7))),10.)

float4 PS_DigitalMatrix(float4 pos : SV_Position, float2 texcoord : TEXCOORD0) : SV_Target {

	float2 u=texcoord;
	float timerX = 1.0;
	timerX += _TimeX;
	
	if (timerX >100) timerX = 0;
	
	float time=timerX*Speed;

	float2 p = 6.*frac(u *= 24./Size) - .5;
	u.y += ceil(time*2.*R(u.xx));
	float4 o=float4(0,0,0,0);
	int i=int(p.y);
	i = ( abs(p.x-1.5)>1.1 ? 0: i==5? 972980223: i==4? 690407533: i==3? 704642687: i==2? 696556137:i==1? 972881535: 0 
	) / int(exp2(30.-ceil(p.x)-3.*floor(R(u))));  
	if(R(++u)<9.9) o=float4(1,1,1,1); 
	if (i > i/2*2) o+=float4(1,0,0,1); else o=float4(0,0,0,0);  

	u=texcoord;
	p = 6.*frac(u *= 24./(Size/2)) - .5;
	u.y += ceil(time*2.*R(u.xx)/2);
	i=int(p.y);
	i = ( abs(p.x-1.5)>1.1 ? 0: i==5? 972980223: i==4? 690407533: i==3? 704642687: i==2? 696556137:i==1? 972881535: 0 
	) / int(exp2(30.-ceil(p.x)-3.*floor(R(u))));  
	if(R(++u)<9.9) o/=float4(1,1,1,1); 
	if (i > i/2*2) o+=float4(1,0,0,1); else o+=float4(0,0,0,0);  

	o.r*=ColorR;
	o.g*=ColorG;
	o.b*=ColorB;


	float4 v=tex2D(ReShade::BackBuffer,texcoord);



	return v+ o;   

}

//techniques///////////////////////////////////////////////////////////////////////////////////////////

technique DigitalMatrix {
	pass DigitalMatrix {
		VertexShader=PostProcessVS;
		PixelShader=PS_DigitalMatrix;
	}
}