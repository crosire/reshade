#include "ReShade.fxh"
 
uniform float Timer < source = "timer"; >;

uniform float rowNum <
	ui_type = "drag";
	ui_min = "1.0";
	ui_max = "BUFFER_HEIGHT";
	ui_label = "Row Count [BasicCRT]";
> = 320.0;

uniform float refreshTime <
	ui_type = "drag";
	ui_min = "0.0";
	ui_max = "120.0";
	ui_label = "RefreshTime [BasicCRT]";
> = 32.0;

float4 CRTTest( float4 pos : SV_Position, float2 uv : TEXCOORD0) : SV_Target { 
	float3 col = tex2D(ReShade::BackBuffer, uv).rgb;
	// per row offset
	float f  = sin( uv.y * rowNum * 3.14 );
	float o  = f * (0.35 / rowNum);
	// scale for subtle effect
	float s  = f * 0.03 + 0.97;
	
	float l  = sin( Timer*0.001 * refreshTime )*0.03 + 0.97;
	// sample in 3 colour offset
	float r = tex2D(ReShade::BackBuffer, float2( uv.x+o, uv.y+o ) ).x;
	float g = tex2D(ReShade::BackBuffer, float2( uv.x-o, uv.y+o ) ).y;
	float b = tex2D(ReShade::BackBuffer, float2( uv.x  , uv.y-o ) ).z;
    // combine as 
    return float4( r*0.7f, g, b*0.9f, l)*l*s;
}

technique BasicCRT
{
	pass CRT1
	{
		VertexShader = PostProcessVS;
		PixelShader = CRTTest;
	}
}