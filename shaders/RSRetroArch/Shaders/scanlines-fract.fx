#include "ReShade.fxh"

uniform float video_sizeX <
	ui_type = "drag";
	ui_min = 1.0;
	ui_max = BUFFER_WIDTH;
	ui_label = "Frame Width [Scanlines-fract]";
	ui_tooltip = "This should be sized according to the video data in the texture (If you're using emulators, set this to the Emu video frame size, otherwise, keep it the same as Screen Width/Height or Fullscreen res.)";
> = 320.0;

uniform float video_sizeY <
	ui_type = "drag";
	ui_min = 1.0;
	ui_max = BUFFER_HEIGHT;
	ui_label = "Frame Height [Scanlines-fract]";
	ui_tooltip = "This should be sized according to the video data in the texture (If you're using emulators, set this to the Emu video frame size, otherwise, keep it the same as Screen Width/Height or Fullscreen res.)";
> = 240.0;

#define video_size float2(video_sizeX,video_sizeY)

// Super-basic scanline shader
// (looks bad at non-integer scales)
// by hunterk
// license: public domain

// Parameter lines go here:
uniform float THICKNESS <
	ui_type = "drag";
	ui_min = 1.0;
	ui_max = 12.0;
	ui_step = 1.0;
	ui_label = "Scanline Thickness [Scanlines-fract]";
> = 2.0;

uniform float DARKNESS <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 1.0;
	ui_step = 0.05;
	ui_label = "Scanline Darkness [Scanlines-fract]"; 
> = 0.50;

uniform float BRIGHTBOOST <
	ui_type = "drag";
	ui_min = 1.0;
	ui_max = 1.2;
	ui_step = 0.1;
	ui_label = "Scanline Boost Bright [Scanlines-fract]";
> = 1.1;

float3 RGBtoYIQ(float3 RGB){
	float3x3 yiqmat = float3x3(
		0.2989, 0.5870, 0.1140,
		0.5959, -0.2744, -0.3216,
		0.2115, -0.5229, 0.3114);
	return mul(RGB,yiqmat);
}

float3 YIQtoRGB(float3 YIQ){
	float3x3 rgbmat = float3x3(
		1.0, 0.956, 0.6210,
		1.0, -0.2720, -0.6474,
		1.0, -1.1060, 1.7046);
	return mul(YIQ,rgbmat);
}

float4 PS_ScanlinesAbs(float4 pos : SV_POSITION, float2 tex : TEXCOORD0) : SV_TARGET 
{
	float lines = frac(tex.y * video_size.y);
	float scale_factor = floor((ReShade::ScreenSize.y / video_size.y) + 0.4999);
	float4 screen = tex2D(ReShade::BackBuffer, tex);
	screen.rgb = RGBtoYIQ(screen.rgb);
	screen.r *= BRIGHTBOOST;
	screen.rgb = YIQtoRGB(screen.rgb);

    return (lines > (1.0 / scale_factor * THICKNESS)) ? screen : screen * float4(1.0 - DARKNESS,1.0 - DARKNESS,1.0 - DARKNESS,1.0 - DARKNESS);
}

technique Scanlinesfract {
    pass Scanlinesfract {
        VertexShader=PostProcessVS;
        PixelShader=PS_ScanlinesAbs;
    }
}