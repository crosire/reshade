/*
    Retro Fog by luluco250

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

// Used for scaling screen coordinates while keeping them centered.
#define scale(x, scale, center) ((x - center) * scale + center)

// May be faster for loops using offset coordinates
#define _tex2D(sp, uv) tex2Dlod(sp, float4(uv, 0.0, 0.0))

//Uniforms////////////////////////////////////////////////////////////////////////////////

uniform float fOpacity <
    ui_label = "Opacity";
    ui_type  = "drag";
    ui_min   = 0.0;
    ui_max   = 1.0;
    ui_step  = 0.001;
> = 1.0;

uniform bool bAutoColor <
    ui_label   = "Automatic Color";
    ui_tooltip = "Use average scene color as fog color.";
> = false;

uniform float3 f3Color <
    ui_label   = "Fog Color";
    ui_tooltip = "Unused if automatic color is enabled.";
    ui_type    = "color";
> = float3(0.0, 0.0, 0.0);

uniform bool bDithering <
    ui_label = "Dithering";
    ui_tooltip = "Enable a retro dithering pattern, making the fog pixelated like in old games such as Doom.";
> = true;

uniform float fQuantize <
    ui_label   = "Quantize";
    ui_tooltip = "Use to simulate lack of colors: 8.0 for 8bits, 16.0 for 16bits etc.\n"
                 "Set to 0.0 to disable quantization.\n"
                 "Only enabled if dithering is enabled as well.";
    ui_type    = "drag";
    ui_min     = 0.0;
    ui_max     = 255.0;
    ui_step    = 1.0;
> = 255.0;

uniform float2 f2Curve <
    ui_label   = "Fog Curve";
    ui_tooltip = "Controls the contrast of fog using start/end values for determining the range.";
    ui_type    = "drag";
    ui_min     = 0.0;
    ui_max     = 1.0;
    ui_step    = 0.001;
> = float2(0.0, 1.0);

uniform float fStart <
    ui_label   = "Fog Start";
    ui_tooltip = "Distance at which the fog center is away from the camera.";
    ui_type    = "drag";
    ui_min     = 0.0;
    ui_max     = 1.0;
    ui_step    = 0.001;
> = 0.0;

uniform bool bCurved <
    ui_label = "Curved";
    ui_tooltip = "If enabled the fog will curve around the start position, otherwise it'll be completely linear and ignore side distance.";
> = true;

//Textures////////////////////////////////////////////////////////////////////////////////

sampler2D sRetroFog_BackBuffer {
    Texture = ReShade::BackBufferTex;
    SRGBTexture = true;
};

//Functions///////////////////////////////////////////////////////////////////////////////

float get_fog(float2 uv) {
    float depth = ReShade::GetLinearizedDepth(uv);

    if (bCurved) {
        depth = distance(
            float2(scale(uv.x, depth * 2.0, 0.5), depth),
            float2(0.5, fStart - 0.45)
        );
    } else {
        depth = distance(depth, fStart - 0.45);
    }

    return smoothstep(f2Curve.x, f2Curve.y, depth);
}

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

// Adapted from: http://devlog-martinsh.blogspot.com.br/2011/03/glsl-dithering.html
float dither(float x, float2 uv) {
    x *= fOpacity;

    if (fQuantize > 0.0)
        x = round(x * fQuantize) / fQuantize;
    
    int2 index = int2(uv * ReShade::ScreenSize) % 8;
    float limit = (index.x < 8) ? float(get_bayer(index) + 1) / 64.0
                                : 0.0;

    if (x < limit)
        return 0.0;
    else
        return 1.0;
}

float3 get_scene_color(float2 uv) {
    static const int point_count = 8;
    static const float2 points[point_count] = {
        float2(0.0, 0.0),
        float2(0.0, 0.5),
        float2(0.0, 1.0),        
        float2(0.5, 0.0),
        //float2(0.5, 0.5),
        float2(0.5, 1.0),
        float2(1.0, 0.0),
        float2(1.0, 0.5),
        float2(1.0, 1.0)
    };

    float3 color = _tex2D(sRetroFog_BackBuffer, points[0]).rgb;
    [unroll]
    for (int i = 1; i < point_count; ++i)
        color += _tex2D(sRetroFog_BackBuffer, points[i]).rgb;

    return color / point_count;
}

//Shaders/////////////////////////////////////////////////////////////////////////////////

void PS_RetroFog(
    float4 position  : SV_POSITION,
    float2 uv        : TEXCOORD,
    out float4 color : SV_TARGET
) {
    color = tex2D(sRetroFog_BackBuffer, uv);
    float fog = get_fog(uv);
    
    if (bDithering)
        fog = dither(fog, uv);
    else
        fog *= fOpacity;

    float3 fog_color;
    if (bAutoColor)
        fog_color = get_scene_color(uv);
    else
        fog_color = f3Color;

    color.rgb = lerp(color.rgb, fog_color, fog);
}

//Technique///////////////////////////////////////////////////////////////////////////////

technique RetroFog {
    pass {
        VertexShader = PostProcessVS;
        PixelShader  = PS_RetroFog;
        SRGBWriteEnable = true;
    }
}
