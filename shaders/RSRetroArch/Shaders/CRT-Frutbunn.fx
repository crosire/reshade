#include "ReShade.fxh"

/*
Adapted for RetroArch from frutbunn's "Another CRT shader" from shadertoy:
https://www.shadertoy.com/view/XdyGzR
*/ 

uniform float texture_sizeX <
	ui_type = "drag";
	ui_min = 1.0;
	ui_max = BUFFER_WIDTH;
	ui_label = "Screen Width [CRT-Frutbunn]";
> = 320.0;

uniform float texture_sizeY <
	ui_type = "drag";
	ui_min = 1.0;
	ui_max = BUFFER_HEIGHT;
	ui_label = "Screen Height [CRT-Frutbunn]";
> = 240.0;

uniform bool CURVATURE <
	ui_type = "boolean";
	ui_label = "Curvature Toggle [CRT-Frutbunn]";
> = true;

uniform bool SCANLINES <
	ui_type = "boolean";
	ui_label = "Scanlines Toggle [CRT-Frutbunn]";
> = true;

uniform bool CURVED_SCANLINES <
	ui_type = "boolean";
	ui_label = "Scanline Curve Toggle [CRT-Frutbunn]";
> = true;

uniform bool LIGHT <
	ui_type = "boolean";
	ui_label = "Vignetting Toggle [CRT-Frutbunn]";
> = true;

uniform int light <
	ui_type = "drag";
	ui_min = 0;
	ui_max = 20;
	ui_step = 1;
	ui_label = "Vignetting Strength [CRT-Frutbunn]";
> = 9;

uniform float blur <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 8.0;
	ui_step = 0.05;
	ui_label = "Blur Strength [CRT-Frutbunn]";
> = 1.0;

static const float gamma = 1.0;
static const float contrast = 1.0;
static const float saturation = 1.0;
static const float brightness = 1.0;

#define texture_size float2(texture_sizeX, texture_sizeY)
#define texture_size_pixel float2(1.0/texture_sizeX, 1.0/texture_sizeY)

// Sigma 1. Size 3
float3 gaussian(in float2 uv, in sampler tex, in float2 iResolution) {
    float b = blur / (iResolution.x / iResolution.y);

    float3 col = tex2D(tex, float2(uv.x - b/iResolution.x, uv.y - b/iResolution.y) ).rgb * 0.077847;
    col += tex2D(tex, float2(uv.x - b/iResolution.x, uv.y) ).rgb * 0.123317;
    col += tex2D(tex, float2(uv.x - b/iResolution.x, uv.y + b/iResolution.y) ).rgb * 0.077847;

    col += tex2D(tex, float2(uv.x, uv.y - b/iResolution.y) ).rgb * 0.123317;
    col += tex2D(tex, float2(uv.x, uv.y) ).rgb * 0.195346;
    col += tex2D(tex, float2(uv.x, uv.y + b/iResolution.y) ).rgb * 0.123317;

    col += tex2D(tex, float2(uv.x + b/iResolution.x, uv.y - b/iResolution.y) ).rgb * 0.077847;
    col += tex2D(tex, float2(uv.x + b/iResolution.x, uv.y) ).rgb * 0.123317;
    col += tex2D(tex, float2(uv.x + b/iResolution.x, uv.y + b/iResolution.y) ).rgb * 0.077847;

    return col;
}

float4 PS_CRTFrutbunn( float4 pos : SV_Position, float2 txcoord : TEXCOORD0) : SV_Target
{
    float2 st = txcoord - float2(0.5,0.5);
    
    // Curvature/light
    float d = length(st*.5 * st*.5);
	float2 uv;
	
	if (CURVATURE){
		uv = st*d + st*.935;
	}else{
		uv = st;
	}
		
	// CRT color blur
	float3 color = gaussian(uv+.5, ReShade::BackBuffer, texture_size.xy * 2.0);

	// Light
	if (LIGHT > 0.5){
		float l = 1. - min(1., d*light);
		color *= l;
	}

	// Scanlines
	float y;
	if (CURVED_SCANLINES){
		y = uv.y;
	}else{
		y = st.y;
	}

	float showScanlines = 1.;
	//    if (texture_size.y<360.) showScanlines = 0.;
		
	if (SCANLINES)
	{
		float s = 2.5 + ReShade::ScreenSize.y * texture_size_pixel.y;//1. - smoothstep(texture_size.x, ReShade::ScreenSize.x, texture_size.y) + 4.;//1. - smoothstep(320., 1440., texture_size.y) + 4.;
		float j = cos(y*texture_size.y*s)*.25; // values between .01 to .25 are ok.
		color = abs(showScanlines-1.)*color + showScanlines*(color - color*j);
	//    color *= 1. - ( .01 + ceil(mod( (st.x+.5)*texture_size.x, 3.) ) * (.995-1.01) )*showScanlines;
	}

		// Border mask
	if (CURVATURE)
	{
			float m = max(0.0, 1. - 2.*max(abs(uv.x), abs(uv.y) ) );
			m = min(m*200., 1.);
			color *= m;
	}

	return float4(color, 1.0);//float4(max(float3(0.0,0.0,0.0), min(float3(1.0,1.0,1.0), color)), 1.);
}

technique CRTFrutbunn {
    pass CRTFrutbunn {
		VertexShader = PostProcessVS;
		PixelShader = PS_CRTFrutbunn;
	}
}