#include "ReShade.fxh"

/*
    cgwg's CRT shader

    Copyright (C) 2010-2011 cgwg, Themaister

    This program is free software; you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by the Free
    Software Foundation; either version 2 of the License, or (at your option)
    any later version.

    (cgwg gave their consent to have their code distributed under the GPL in
    this message:

        http://board.byuu.org/viewtopic.php?p=26075#p26075

        "Feel free to distribute my shaders under the GPL. After all, the
        barrel distortion code was taken from the Curvature shader, which is
        under the GPL."
    )
*/

uniform float texture_sizeX <
	ui_type = "drag";
	ui_min = 1.0;
	ui_max = BUFFER_WIDTH;
	ui_label = "Screen Width [CRT-cgwg]";
> = 320.0;

uniform float texture_sizeY <
	ui_type = "drag";
	ui_min = 1.0;
	ui_max = BUFFER_HEIGHT;
	ui_label = "Screen Height [CRT-cgwg]";
> = 240.0;

uniform float video_sizeX <
	ui_type = "drag";
	ui_min = 1.0;
	ui_max = BUFFER_WIDTH;
	ui_label = "Frame Width [CRT-cgwg]";
	ui_tooltip = "This should be sized according to the video data in the texture (If you're using emulators, set this to the Emu video frame size, otherwise, keep it the same as Screen Width/Height or Fullscreen res.)";
> = 320.0;

uniform float video_sizeY <
	ui_type = "drag";
	ui_min = 1.0;
	ui_max = BUFFER_HEIGHT;
	ui_label = "Frame Height [CRT-cgwg]";
	ui_tooltip = "This should be sized according to the video data in the texture (If you're using emulators, set this to the Emu video frame size, otherwise, keep it the same as Screen Width/Height or Fullscreen res.)";
> = 240.0;

uniform float CRTCGWG_GAMMA <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 10.0;
	ui_step = 0.01;
	ui_label = "Gamma [CRT-cgwg]";
> = 2.7;

float fmod(float a, float b)
{
  float c = frac(abs(a/b))*abs(b);
  return (a < 0) ? -c : c;   /* if ( a < 0 ) c = 0-c */
}

#define TEX2D(c) tex2D(ReShade::BackBuffer,(c))
#define PI 3.141592653589
#define texture_size float2(texture_sizeX, texture_sizeY)
#define video_size float2(video_sizeX, video_sizeY)

float4 PS_CRTcgwg(float4 vpos : SV_Position, float2 uv : TexCoord) : SV_Target
{
	
   float2 delta = 1.0 / texture_size;
   float dx = delta.x;
   float dy = delta.y;
   
   float2 c01 = uv + float2(-dx, 0.0);
   float2 c11 = uv + float2(0.0, 0.0);
   float2 c21 = uv + float2(dx, 0.0);
   float2 c31 = uv + float2(2.0 * dx, 0.0);
   float2 c02 = uv + float2(-dx, dy);
   float2 c12 = uv + float2(0.0, dy);
   float2 c22 = uv + float2(dx, dy);
   float2 c32 = uv + float2(2.0 * dx, dy);
   float mod_factor = uv.x * ReShade::ScreenSize.x * texture_size.x / video_size.x;
   float2 ratio_scale = uv * texture_size;

   
   float2 uv_ratio = frac(ratio_scale);
   float4 col, col2;

   float4x4 texes0 = float4x4((TEX2D(c01).xyz),0.0, (TEX2D(c11).xyz),0.0, (TEX2D(c21).xyz),0.0, (TEX2D(c31).xyz),0.0);
   float4x4 texes1 = float4x4((TEX2D(c02).xyz),0.0, (TEX2D(c12).xyz),0.0, (TEX2D(c22).xyz),0.0, (TEX2D(c32).xyz),0.0);

   float4 coeffs = float4(1.0 + uv_ratio.x, uv_ratio.x, 1.0 - uv_ratio.x, 2.0 - uv_ratio.x) + 0.005;
   coeffs = sin(PI * coeffs) * sin(0.5 * PI * coeffs) / (coeffs * coeffs);
   coeffs = coeffs / dot(coeffs, float(1.0));

   float3 weights = float3(3.33 * uv_ratio.y, 3.33 * uv_ratio.y, 3.33 * uv_ratio.y);
   float3 weights2 = float3(uv_ratio.y * -3.33 + 3.33, uv_ratio.y * -3.33 + 3.33, uv_ratio.y * -3.33 + 3.33);

   col = saturate(mul(coeffs, texes0));
   col2 = saturate(mul(coeffs, texes1));

   float3 wid = 2.0 * pow(col, float4(4.0, 4.0, 4.0, 0.0)) + 2.0;
   float3 wid2 = 2.0 * pow(col2, float4(4.0, 4.0, 4.0, 0.0)) + 2.0;

   col = pow(col, float4(CRTCGWG_GAMMA, CRTCGWG_GAMMA, CRTCGWG_GAMMA,0.0));
   col2 = pow(col2, float4(CRTCGWG_GAMMA, CRTCGWG_GAMMA, CRTCGWG_GAMMA,0.0));

   float3 sqrt1 = rsqrt(0.5 * wid);
   float3 sqrt2 = rsqrt(0.5 * wid2);

   float3 pow_mul1 = weights * sqrt1;
   float3 pow_mul2 = weights2 * sqrt2;

   float3 div1 = 0.1320 * wid + 0.392;
   float3 div2 = 0.1320 * wid2 + 0.392;

   float3 pow1 = -pow(pow_mul1, wid);
   float3 pow2 = -pow(pow_mul2, wid2);

   weights = exp(pow1) / div1;
   weights2 = exp(pow2) / div2;

   float3 multi = col * weights + col2 * weights2;
   float3 mcol = lerp(float3(1.0, 0.7, 1.0), float3(0.7, 1.0, 0.7), floor(fmod(mod_factor, 2.0)));

   return float4(pow(mcol * multi, float3(0.454545, 0.454545, 0.454545)), 1.0);
}

technique cgwgCRT {
	pass CRT_cgwg_fast {
		VertexShader=PostProcessVS;
		PixelShader=PS_CRTcgwg;
	}
}