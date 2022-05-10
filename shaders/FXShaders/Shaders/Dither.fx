/*
    Dither by luluco250

    Copyright (c) 2017 Lucas Melo

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
    SOFTWARE.
*/

#include "ReShade.fxh"

//Macros//////////////////////////////////////////////////////////////////////////////////

#ifndef DITHER_NOQUANTIZATION
#define DITHER_NOQUANTIZATION 0
#endif

//Uniforms////////////////////////////////////////////////////////////////////////////////

uniform float fDithering <
    ui_label = "Dithering";
    ui_type  = "drag";
    ui_min   = 0.0;
    ui_max   = 1.0;
    ui_step  = 0.001;
> = 0.5;
#if !DITHER_NOQUANTIZATION
uniform float fQuantization <
    ui_label   = "Quantization";
    ui_tooltip = "Use to simulate lack of colors: 8.0 for 8bits, 16.0 for 16bits etc.\n"
                 "Set to 0.0 to disable quantization.\n"
                 "Only enabled if dithering is enabled as well.";
    ui_type    = "drag";
    ui_min     = 0.0;
    ui_max     = 255.0;
    ui_step    = 1.0;
> = 0.0;
#endif

uniform int iDitherMode <
    ui_label = "Dither Mode";
    ui_type  = "combo";
    ui_items = "Add\0Multiply\0";
> = 0;

//Textures////////////////////////////////////////////////////////////////////////////////

sampler2D sRetroFog_BackBuffer {
    Texture = ReShade::BackBufferTex;
    SRGBTexture = true;
};

//Functions///////////////////////////////////////////////////////////////////////////////

// Source: https://en.wikipedia.org/wiki/Ordered_dithering
int get_bayer(int2 i) {
    static const int bayer[8 * 8] = {
          0, 48, 12, 60,  3, 51, 15, 63,
         32, 16, 44, 28, 35, 19, 47, 31,
          8, 56,  4, 52, 11, 59,  7, 55,
         40, 24, 36, 20, 43, 27, 39, 23,
          2, 50, 14, 62,  1, 49, 13, 61,
         34, 18, 46, 30, 33, 17, 45, 29,
         10, 58,  6, 54,  9, 57,  5, 53,
         42, 26, 38, 22, 41, 25, 37, 21
    };
    return bayer[i.x + 8 * i.y];
}

//#define fmod(a, b) ((frac(abs(a / b)) * abs(b)) * ((step(a, 0) - 0.5) * 2.0))
float2 fmod(float2 a, float2 b) {
    float2 c = frac(abs(a / b)) * abs(b);
    return (a < 0) ? -c : c;
}

// Adapted from: http://devlog-martinsh.blogspot.com.br/2011/03/glsl-dithering.html
float dither(float x, float2 uv) {
    #if (__RENDERER__ & 0x10000) // If OpenGL
    
    float2 index = fmod(uv * ReShade::ScreenSize, 8.0);
    float limit  = (float(get_bayer(int2(index)) + 1) / 64.0) * step(index.x, 8.0);
    
    #else // DirectX

    int2 index = int2(uv * ReShade::ScreenSize) % 8;
    float limit = (float(get_bayer(index) + 1) / 64.0) * step(index.x, 8);

    #endif
    return step(limit, x);
}

float get_luma_linear(float3 color) {
    return dot(color, float3(0.2126, 0.7152, 0.0722));
}

float rand(float2 uv, float t) {
    float seed = dot(uv, float2(12.9898, 78.233));
    float noise = frac(sin(seed) * 43758.5453 + t);
    return noise;
}

//Shaders/////////////////////////////////////////////////////////////////////////////////

void PS_Dither(
    float4 position  : SV_POSITION,
    float2 uv        : TEXCOORD,
    out float4 color : SV_TARGET
) {
    color = tex2D(sRetroFog_BackBuffer, uv);

    if (fQuantization > 0.0)
        color = round(color * fQuantization) / fQuantization;

    float luma = get_luma_linear(color.rgb);
    float pattern = dither(luma, uv);
    
    if (iDitherMode == 0) // Add
        color.rgb += color.rgb * pattern * fDithering;
    else                  // Multiply
        color.rgb *= lerp(1.0, pattern, fDithering);
}

//Technique///////////////////////////////////////////////////////////////////////////////

technique Dither {
    pass {
        VertexShader = PostProcessVS;
        PixelShader  = PS_Dither;
        SRGBWriteEnable = true;
    }
}
