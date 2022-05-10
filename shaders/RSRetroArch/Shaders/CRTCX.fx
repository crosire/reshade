#include "ReShade.fxh"

uniform float m_curve_x <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 1.0;
	ui_step = 0.001;
	ui_label = "Curvature X [CRT-CX]";
> = 1.0;

uniform float m_curve_y <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 1.0;
	ui_step = 0.001;
	ui_label = "Curvature Y [CRT-CX]";
> = 1.0;

uniform float m_scale_x <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 2.0;
	ui_step = 0.001;
	ui_label = "Overscan X [CRT-CX]";
> = 1.0;

uniform float m_scale_y <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 2.0;
	ui_step = 0.001;
	ui_label = "Overscan Y [CRT-CX]";
> = 1.0;

uniform float m_translate_x <
	ui_type = "drag";
	ui_min = -1.0;
	ui_max = 1.0;
	ui_step = 0.001;
	ui_label = "Monitor Position X [CRT-CX]";
> = 0.0;

uniform float m_translate_y <
	ui_type = "drag";
	ui_min = -1.0;
	ui_max = 1.0;
	ui_step = 0.001;
	ui_label = "Moniton Position Y [CRT-CX]";
> = 0.0;

uniform int m_resolution_x <
	ui_type = "drag";
	ui_min = 1;
	ui_max = BUFFER_WIDTH;
	ui_step = 1;
	ui_label = "Resolution X [CRT-CX]";
> = 320;

uniform int m_resolution_y <
	ui_type = "drag";
	ui_min = 1;
	ui_max = BUFFER_HEIGHT;
	ui_step = 1;
	ui_label = "Resolution Y [CRT-CX]";
> = 240;

uniform float m_scanline_intensity <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 2.0;
	ui_step = 0.001;
	ui_label = "Scanline Intensity [CRT-CX]";
> = 0.2;

uniform float m_rgb_split_intensity <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 2.0;
	ui_step = 0.001;
	ui_label = "RGB Phosphor Intensity [CRT-CX]";
> = 0.4;

uniform float m_brightness <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 2.0;
	ui_step = 0.001;
	ui_label = "Brightness [CRT-CX]";
> = 0.5;

uniform float m_contrast <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 2.0;
	ui_step = 0.001;
	ui_label = "Contrast [CRT-CX]";
> = 0.5;

uniform float m_gamma <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 2.0;
	ui_step = 0.001;
	ui_label = "Gamma [CRT-CX]";
> = 0.5;

texture tFrameCX < source = "crt-cx/tvscreen.png"; >
{
	Width = 1024;
	Height = 1024;
	MipLevels = 1;
	
    MinFilter = LINEAR;
    MagFilter = LINEAR;
};

sampler sFrameCX { Texture = tFrameCX; AddressU = BORDER; AddressV = BORDER;};

float4 PS_CRTCX(float4 pos: SV_Position, float2 uv_tx : TEXCOORD0) : SV_Target
{

	//float2 texcoords = (b3d_ClipPosition.st/b3d_ClipPosition.w)*0.5+0.5;
	float2 texels = uv_tx * ReShade::ScreenSize; //this is because the original shader uses OpenGL's fragCoord, which is in texels rather than pixels
	float2 texcoords = uv_tx; //this is because the original shader uses OpenGL's fragCoord, which is in texels rather than pixels
	
	// get color for position on screen:
	float2 resfac = float2(m_resolution_x, m_resolution_y)/float2(BUFFER_WIDTH, BUFFER_HEIGHT);
	float bl = 0.03;
	float x = texcoords.x*(1.0+bl)-bl/2.0;
	float y = texcoords.y*(1.0+bl)-bl/2.0;
	float x2 = (x-0.5)*(1.0+  0.5*(0.3*m_curve_x)*((y-0.5)*(y-0.5)))/m_scale_x+0.5-m_translate_x;
	float y2 = (y-0.5)*(1.0+  0.25*(0.3*m_curve_y)*((x-0.5)*(x-0.5)))/m_scale_y+0.5-m_translate_y;
	float2 v2 = float2(x2, y2);
	float4 temp = tex2D(ReShade::BackBuffer, v2); // v2*resfac
	
	// brightness and contrast:
	temp = clamp(float4((temp.rgb - 0.5) * (m_contrast*2.0) + 0.5 + (m_brightness*2.0-1.0), 1.0), 0.0, 1.0);
	
	// gamma:
	float gamma2 = 2.0-m_gamma*2.0;
	temp = float4(pow(abs(temp.r), gamma2),pow(abs(temp.g), gamma2),pow(abs(temp.b), gamma2), 1.0);
	
	// grb splitting and scanlines:
	if (v2.x<0.0 || v2.x>1.0 || v2.y<0.0 || v2.y>1.0) {
		temp = float4(0.0,0.0,0.0,1.0);
	} else {

		float cr = sin((x2*m_resolution_x)               *2.0*3.1415) +0.1;
		float cg = sin((x2*m_resolution_x-1.0*2.0*3.1415)*2.0*3.1415) +0.1;
		float cb = sin((x2*m_resolution_x-2.0*2.0*3.1415)*2.0*3.1415) +0.1;
		temp = lerp(temp*float4(cr,cg,cb,1.0),temp,1.0-m_rgb_split_intensity);
		float ck = (sin((y2*m_resolution_y)*2.0*3.1415)+0.1)*m_scanline_intensity;
		temp += (temp*0.9)*ck*0.1;
	}
	
	// frame (taken from newpixie)
	
	float4 f= tex2D(sFrameCX,uv_tx.xy);//*((1.0)+2.0*fscale)-fscale-float2(-0.0, 0.005));
    f.xyz = lerp( f.xyz, float3(0.0,0.0,0.0), 0.5 );
	temp = lerp( temp, lerp( max( temp, 0.0), pow( abs( f.xyz ), float3( 1.0,1.0,1.0 ) ), f.w), float3( 1.0,1.0,1.0 ) );
	
	// final color:
	return float4((temp.rgb)*0.9, 1.0);
}

technique CRT_CX
{
	pass PS_CRTCX
	{
		VertexShader = PostProcessVS;
		PixelShader = PS_CRTCX;
	}
}