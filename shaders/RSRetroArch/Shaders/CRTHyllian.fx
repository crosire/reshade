#include "ReShade.fxh"

/*
   Hyllian's CRT Shader
 
   Copyright (C) 2011-2016 Hyllian - sergiogdb@gmail.com
 
   Permission is hereby granted, free of charge, to any person obtaining a copy
   of this software and associated documentation files (the "Software"), to deal
   in the Software without restriction, including without limitation the rights
   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
   copies of the Software, and to permit persons to whom the Software is
   furnished to do so, subject to the following conditions:
 
   The above copyright notice and this permission notice shall be included in
   all copies or substantial portions of the Software.
 
   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
   THE SOFTWARE.
 
*/

uniform float CRT_MULRES_X <
	ui_type = "drag";
	ui_min = 1.0;
	ui_max = 8.0;
	ui_step = 1.0;
	ui_label = "Internal X Res Multiplier [CRT-Hyllian]";
> = 2.0;

uniform float CRT_MULRES_Y <
	ui_type = "drag";
	ui_min = 1.0;
	ui_max = 8.0;
	ui_step = 1.0;
	ui_label = "Internal Y Res Multiplier [CRT-Hyllian]";
> = 2.0;
 
uniform float texture_sizeX <
	ui_type = "drag";
	ui_min = 1.0;
	ui_max = BUFFER_WIDTH;
	ui_label = "Screen Width [CRT-Hyllian]";
> = 320.0;

uniform float texture_sizeY <
	ui_type = "drag";
	ui_min = 1.0;
	ui_max = BUFFER_HEIGHT;
	ui_label = "Screen Height [CRT-Hyllian]";
> = 240.0;

uniform float video_sizeX <
	ui_type = "drag";
	ui_min = 1.0;
	ui_max = BUFFER_WIDTH;
	ui_label = "Frame Width [CRT-Hyllian]";
	ui_tooltip = "This should be sized according to the video data in the texture (If you're using emulators, set this to the Emu video frame size, otherwise, keep it the same as Screen Width/Height or Fullscreen res.)";
> = 320.0;

uniform float video_sizeY <
	ui_type = "drag";
	ui_min = 1.0;
	ui_max = BUFFER_HEIGHT;
	ui_label = "Frame Height [CRT-Hyllian]";
	ui_tooltip = "This should be sized according to the video data in the texture (If you're using emulators, set this to the Emu video frame size, otherwise, keep it the same as Screen Width/Height or Fullscreen res.)";
> = 240.0;
 
uniform bool PHOSPHOR <
    ui_type = "bool";
    ui_label = "Toggle Phosphor [CRT-Hyllian]";
> = true;
 
uniform float VSCANLINES <
    ui_type = "drag";
    ui_min = 0.0;
    ui_max = 1.0;
    ui_step = 1.0;
    ui_label = "Scanlines Direction [CRT-Hyllian]";
> = 0.0;
 
uniform float InputGamma <
    ui_type = "drag";
    ui_min = 0.0;
    ui_max = 5.0;
    ui_step = 0.1;
    ui_label = "Input Gamma [CRT-Hyllian]";
> = 2.5;
 
uniform float OutputGamma <
    ui_type = "drag";
    ui_min = 0.0;
    ui_max = 5.0;
    ui_step = 0.1;
    ui_label = "Output Gamma [CRT-Hyllian]";
> = 2.2;
 
uniform float SHARPNESS <
    ui_type = "drag";
    ui_min = 1.0;
    ui_max = 5.0;
    ui_step = 1.0;
    ui_label = "Sharpness Hack [CRT-Hyllian]";
> = 1.0;
 
uniform float COLOR_BOOST <
    ui_type = "drag";
    ui_min = 1.0;
    ui_max = 2.0;
    ui_step = 0.05;
    ui_label = "Color Boost [CRT-Hyllian]";
> = 1.5;
 
uniform float RED_BOOST <
    ui_type = "drag";
    ui_min = 1.0;
    ui_max = 2.0;
    ui_step = 0.01;
    ui_label = "Red Boost [CRT-Hyllian]";
> = 1.0;
 
uniform float GREEN_BOOST <
    ui_type = "drag";
    ui_min = 1.0;
    ui_max = 2.0;
    ui_step = 0.01;
    ui_label = "Green Boost [CRT-Hyllian]";
> = 1.0;
 
uniform float BLUE_BOOST<
    ui_type = "drag";
    ui_min = 1.0;
    ui_max = 2.0;
    ui_step = 0.01;
    ui_label = "Blue Boost [CRT-Hyllian]";
> = 1.0;
 
uniform float SCANLINES_STRENGTH<
    ui_type = "drag";
    ui_min = 0.0;
    ui_max = 1.0;
    ui_step = 0.02;
    ui_label = "Scanlines Strength [CRT-Hyllian]";
> = 0.05;
 
uniform float BEAM_MIN_WIDTH <
    ui_type = "drag";
    ui_min = 0.0;
    ui_max = 1.0;
    ui_step = 0.02;
    ui_label = "Min Beam Width [CRT-Hyllian]";
> = 0.86;
 
uniform float BEAM_MAX_WIDTH <
    ui_type = "drag";
    ui_min = 0.0;
    ui_max = 1.0;
    ui_step = 0.02;
    ui_label = "Max Beam Width [CRT-Hyllian]";
> = 1.0;
 
uniform float CRT_ANTI_RINGING <
    ui_type = "drag";
    ui_min = 0.0;
    ui_max = 1.0;
    ui_step = 0.1;
    ui_label = "CRT Anti-Ringing [CRT-Hyllian]";
> = 0.8;
 
#define GAMMA_IN(color)     pow(color, float3(InputGamma, InputGamma, InputGamma))
#define GAMMA_OUT(color)    pow(color, float3(1.0 / OutputGamma, 1.0 / OutputGamma, 1.0 / OutputGamma))

#define texture_size float2(texture_sizeX, texture_sizeY)
#define video_size float2(video_sizeX, video_sizeY)
 
// Horizontal cubic filter.
 
// Some known filters use these values:
 
//    B = 0.0, C = 0.0  =>  Hermite cubic filter.
//    B = 1.0, C = 0.0  =>  Cubic B-Spline filter.
//    B = 0.0, C = 0.5  =>  Catmull-Rom Spline filter. This is the default used in this shader.
//    B = C = 1.0/3.0   =>  Mitchell-Netravali cubic filter.
//    B = 0.3782, C = 0.3109  =>  Robidoux filter.
//    B = 0.2620, C = 0.3690  =>  Robidoux Sharp filter.
//    B = 0.36, C = 0.28  =>  My best config for ringing elimination in pixel art (Hyllian).
 
 
// For more info, see: http://www.imagemagick.org/Usage/img_diagrams/cubic_survey.gif
 
// Change these params to configure the horizontal filter.

uniform float B <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 0.5;
	ui_step = 0.1;
	ui_label = "Horizontal Filter B [CRT-Hyllian]";
> = 0.0;

uniform float C < 
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 0.5;
	ui_step = 0.1;
	ui_label = "Horizontal Filter C [CRT-Hyllian]";
> = 0.5;
 
float fmod(float a, float b)
{
  float c = frac(abs(a/b))*abs(b);
  return (a < 0) ? -c : c;   /* if ( a < 0 ) c = 0-c */
}
                                                   
float4 PS_CRTHyllian(float4 pos : SV_Position, float2 uv : TEXCOORD0) : SV_Target{
 
    float4x4 invX = float4x4(            (-B - 6.0*C)/6.0,         (3.0*B + 12.0*C)/6.0,     (-3.0*B - 6.0*C)/6.0,             B/6.0,
                                            (12.0 - 9.0*B - 6.0*C)/6.0, (-18.0 + 12.0*B + 6.0*C)/6.0,                      0.0, (6.0 - 2.0*B)/6.0,
                                           -(12.0 - 9.0*B - 6.0*C)/6.0, (18.0 - 15.0*B - 12.0*C)/6.0,      (3.0*B + 6.0*C)/6.0,             B/6.0,
                                                       (B + 6.0*C)/6.0,                           -C,                      0.0,               0.0);
                                                   
    float3 color;
 
    float2 TextureSize = float2(SHARPNESS*texture_size.x, texture_size.y)/float2(CRT_MULRES_X, CRT_MULRES_Y);
 
    float2 dx = lerp(float2(1.0/TextureSize.x, 0.0), float2(0.0, 1.0/TextureSize.y), VSCANLINES);
    float2 dy = lerp(float2(0.0, 1.0/TextureSize.y), float2(1.0/TextureSize.x, 0.0), VSCANLINES);
 
    float2 pix_coord = uv*TextureSize+float2(-0.5,0.5);
 
    float2 tc = lerp((floor(pix_coord)+float2(0.5,0.5))/TextureSize, (floor(pix_coord)+float2(1.0,-0.5))/TextureSize, VSCANLINES);
 
    float2 fp = lerp(frac(pix_coord), frac(pix_coord.yx), VSCANLINES);
 
    float3 c00 = GAMMA_IN(tex2D(ReShade::BackBuffer, tc     - dx - dy));
    float3 c01 = GAMMA_IN(tex2D(ReShade::BackBuffer, tc          - dy));
    float3 c02 = GAMMA_IN(tex2D(ReShade::BackBuffer, tc     + dx - dy));
    float3 c03 = GAMMA_IN(tex2D(ReShade::BackBuffer, tc + 2.0*dx - dy));
    float3 c10 = GAMMA_IN(tex2D(ReShade::BackBuffer, tc     - dx));
    float3 c11 = GAMMA_IN(tex2D(ReShade::BackBuffer, tc         ));
    float3 c12 = GAMMA_IN(tex2D(ReShade::BackBuffer, tc     + dx));
    float3 c13 = GAMMA_IN(tex2D(ReShade::BackBuffer, tc + 2.0*dx));
 
    //  Get min/max samples
    float3 min_sample = min(min(c01.xyz,c11.xyz), min(c02.xyz,c12.xyz));
    float3 max_sample = max(max(c01.xyz,c11.xyz), max(c02.xyz,c12.xyz));
 
    float4x3 color_matrix0 = float4x3(c00, c01, c02, c03);
    float4x3 color_matrix1 = float4x3(c10, c11, c12, c13);
 
    float4 invX_Px  = mul(invX, float4(fp.x*fp.x*fp.x, fp.x*fp.x, fp.x, 1.0));
    float3 color0   = mul(invX_Px, color_matrix0);
    float3 color1   = mul(invX_Px, color_matrix1);
 
    // Anti-ringing
    float3 aux = color0;
    color0 = clamp(color0.xyz, min_sample, max_sample);
    color0 = lerp(aux.xyz, color0.xyz, CRT_ANTI_RINGING);
    aux = color1;
    color1 = clamp(color1.xyz, min_sample, max_sample);
    color1 = lerp(aux.xyz, color1.xyz, CRT_ANTI_RINGING);
 
    float pos0 = fp.y;
    float pos1 = 1 - fp.y;
 
    float3 lum0 = lerp(float3(BEAM_MIN_WIDTH,BEAM_MIN_WIDTH,BEAM_MIN_WIDTH), float3(BEAM_MAX_WIDTH,BEAM_MAX_WIDTH,BEAM_MAX_WIDTH), color0.xyz);
    float3 lum1 = lerp(float3(BEAM_MIN_WIDTH,BEAM_MIN_WIDTH,BEAM_MIN_WIDTH), float3(BEAM_MAX_WIDTH,BEAM_MAX_WIDTH,BEAM_MAX_WIDTH), color1.xyz);
 
    float3 d0 = clamp(pos0/(lum0+0.0000001), 0.0, 1.0);
    float3 d1 = clamp(pos1/(lum1+0.0000001), 0.0, 1.0);
 
    d0 = exp(-10.0*SCANLINES_STRENGTH*d0*d0);
    d1 = exp(-10.0*SCANLINES_STRENGTH*d1*d1);
 
    color = clamp(color0.xyz*d0+color1.xyz*d1, 0.0, 1.0);            
 
    color *= COLOR_BOOST*float3(RED_BOOST, GREEN_BOOST, BLUE_BOOST);
 
    float mod_factor = lerp(uv.x * ReShade::ScreenSize.x * texture_size.x / video_size.x, uv.y * ReShade::ScreenSize.y * texture_size.y / video_size.y, VSCANLINES);
 
    float3 dotMaskWeights = lerp(
                                 float3(1.0, 0.7, 1.0),
                                 float3(0.7, 1.0, 0.7),
                                 floor(fmod(mod_factor, 2.0))
                                  );
 
    color.rgb *= lerp(float3(1.0,1.0,1.0), dotMaskWeights, PHOSPHOR);
 
    color  = GAMMA_OUT(color);
 
    return float4(color, 1.0);
}
 
technique CRTHyllian {
    pass CRTHyllian {
        VertexShader=PostProcessVS;
        PixelShader=PS_CRTHyllian;
    }
}