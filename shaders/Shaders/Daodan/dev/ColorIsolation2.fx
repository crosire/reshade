///////////////////////////////////////////////////////////////////////////////
//
//ReShade Shader: ColorIsolation2
//https://github.com/Daodan317081/reshade-shaders
//
//BSD 3-Clause License
//
//Copyright (c) 2018-2020, Alexander Federwisch
//All rights reserved.
//
//Redistribution and use in source and binary forms, with or without
//modification, are permitted provided that the following conditions are met:
//
//* Redistributions of source code must retain the above copyright notice, this
//  list of conditions and the following disclaimer.
//
//* Redistributions in binary form must reproduce the above copyright notice,
//  this list of conditions and the following disclaimer in the documentation
//  and/or other materials provided with the distribution.
//
//* Neither the name of the copyright holder nor the names of its
//  contributors may be used to endorse or promote products derived from
//  this software without specific prior written permission.
//
//THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
//AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
//IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
//DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
//FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
//DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
//SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
//CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
//OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
//OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
///////////////////////////////////////////////////////////////////////////////

#include "ReShade.fxh"

#define COLORISOLATION_CATEGORY_SETUP "Setup"
#define COLORISOLATION_CATEGORY_DEBUG "Debug"

uniform bool SHOW_DEBUG_OVERLAY <
    ui_label = "Show Overlay";
    ui_category = COLORISOLATION_CATEGORY_SETUP;
> = false;

uniform bool ENABLE_CURVE2 <
    ui_type = "radio";
    ui_label = "Enable Curve 2";
    ui_category = COLORISOLATION_CATEGORY_SETUP;
> = false;

uniform bool BOOL_UNUSED <
    ui_type = "radio";
    ui_label = "Left Values: Curve 1, Right Values: Curve 2";
    ui_category = COLORISOLATION_CATEGORY_SETUP;
> = true;

uniform float2 CURVE_CENTER <
    ui_type = "drag";
    ui_category = COLORISOLATION_CATEGORY_SETUP;
    ui_label = "Hue";
    ui_tooltip = "Select the hue to isolate";
    ui_min = 0.0; ui_max = 1.0;
    ui_step = 0.005;
> = float2(0.0, 0.5);

uniform float2 CURVE_HEIGHT<
    ui_type = "drag";
    ui_category = COLORISOLATION_CATEGORY_SETUP;
    ui_label = "Strength";
    ui_tooltip = "Select the saturation of the isolated color";
    ui_min = 0.0; ui_max = 1.0;
    ui_step = 0.005;
> = float2(1.0, 1.0);

uniform float2 CURVE_OVERLAP <
    ui_type = "drag";
    ui_category = COLORISOLATION_CATEGORY_SETUP;
    ui_label = "Overlap";
    ui_tooltip = "Select how much neighbouring colors to isolate";
    ui_min = 0.0; ui_max = 1.0;
    ui_step = 0.001;
> = float2(0.5, 0.5);

uniform bool CURVE_INVERT <
    ui_type = "radio";
    ui_label = "Invert";
    ui_category = COLORISOLATION_CATEGORY_SETUP;
> = false;

uniform bool SHOW_COLOR_DIFFERENCE <
    ui_type = "radio";
    ui_label = "Show Color Difference";
    ui_category = COLORISOLATION_CATEGORY_DEBUG;
> = false;

uniform float2 DEBUG_OVERLAY_POSITION<
    ui_type = "drag";
    ui_category = COLORISOLATION_CATEGORY_DEBUG;
    ui_category_closed = true;
    ui_label = "Overlay Position";
    ui_min = 0.0; ui_max = 1.0;
    ui_step = 0.01;
> = float2(0.0, 0.15);

uniform int2 DEBUG_OVERLAY_SIZE <
    ui_type = "drag";
    ui_category = COLORISOLATION_CATEGORY_DEBUG;
    ui_category_closed = true;
    ui_label = "Overlay Size";
    ui_tooltip = "x: width\nz: height";
    ui_min = 50; ui_max = BUFFER_WIDTH;
    ui_step = 1;
> = int2(1000, 300);

uniform float DEBUG_OVERLAY_OPACITY <
    ui_type = "drag";
    ui_category = COLORISOLATION_CATEGORY_DEBUG;
    ui_category_closed = true;
    ui_label = "Overlay Opacity";
    ui_min = 0.0; ui_max = 1.0;
    ui_step = 0.01;
> = 1.0;

float3 RGBfromHue(float3 c) {
    const float3 A = float3(120.0, 60.0, 180.0)/360.0;
    const float3 B = float3(240.0, 180.0, 300.0)/360.0;
    float3 rgb = (saturate(-6.0 * (c.xxx - A)) + saturate(6.0 * (c.xxx - B)))*float3(1.0,-1.0,-1.0)+float3(0.0,1.0,1.0);
    return rgb;
}

float Map(float value, float2 span_old, float2 span_new) {
    float span_old_diff = abs(span_old.y - span_old.x) < 1e-6 ? 1e-6 : span_old.y - span_old.x;
    return lerp(span_new.x, span_new.y, (clamp(value, span_old.x, span_old.y)-span_old.x)/(span_old_diff));
}

//These RGB/HSV conversion functions are based on the blogpost from:
//http://lolengine.net/blog/2013/07/27/rgb-to-hsv-in-glsl
float3 RGBtoHSV(float3 c) {
    float4 K = float4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);
    float4 p = c.g < c.b ? float4(c.bg, K.wz) : float4(c.gb, K.xy);
    float4 q = c.r < p.x ? float4(p.xyw, c.r) : float4(c.r, p.yzx);

    float d = q.x - min(q.w, q.y);
    float e = 1.0e-10;
    return float3(abs(q.z + (q.w - q.y) / (6.0 * d + e)), d / (q.x + e), q.x);
}

float Curve(float x, float p, float s, float h) {
    float offset = p.x;
    s = (1.1 - s) * 10.0;
    float sr = -smoothstep(0.0,1.0,s*(x-offset))+1;
    float sl = -smoothstep(0.0,1.0,s*((1-x)-(1-offset)))+1;
    float value = x < offset ? sl : sr;
    return value * h;
}

float CalculateWeight(float x, float2 pos, float2 slope, float2 height) {
    float value = Curve(x, pos.x, slope.x, height.x)+Curve(x, pos.x+1, slope.x, height.x)+Curve(x, pos.x-1, slope.x, height.x);
    float value2 = Curve(x, pos.y, slope.y, height.y)+Curve(x, pos.y+1, slope.y, height.y)+Curve(x, pos.y-1, slope.y, height.y);
    value = ENABLE_CURVE2 ? saturate(value + value2) : value;
    return CURVE_INVERT ? 1.0 - value : value;
}

float3 DrawDebugOverlay(float3 background, float3 param, float2 pos, int2 size, float opacity, int2 vpos, float2 texcoord) {
    float x, y, value, value2, luma;
    float3 overlay, hsvStrip;

	float2 overlayPos = pos * (ReShade::ScreenSize - size);

    if(all(vpos.xy >= overlayPos) && all(vpos.xy < overlayPos + size))
    {
        x = Map(texcoord.x, float2(overlayPos.x, overlayPos.x + size.x) / BUFFER_WIDTH, float2(0.0, 1.0));
        y = Map(texcoord.y, float2(overlayPos.y, overlayPos.y + size.y) / BUFFER_HEIGHT, float2(0.0, 1.0));
        hsvStrip = RGBfromHue(float3(x, 1.0, 1.0));
        luma = dot(hsvStrip, float3(0.2126, 0.7151, 0.0721));
        value = CalculateWeight(x, CURVE_CENTER, CURVE_OVERLAP, CURVE_HEIGHT);
        overlay = lerp(luma.rrr, hsvStrip, value);
        overlay = lerp(overlay, 0.0.rrr, exp(-size.y * length(float2(x, 1.0 - y) - float2(x, value))));
        background = lerp(background, overlay, opacity);
    }

    return background;
}

float3 ColorIsolationPS(float4 vpos : SV_Position, float2 texcoord : TexCoord) : SV_Target {
    float3 color = tex2D(ReShade::BackBuffer, texcoord).rgb;
    float3 luma = dot(color, float3(0.2126, 0.7151, 0.0721)).rrr;
    float value = CalculateWeight(RGBtoHSV(color), CURVE_CENTER, CURVE_OVERLAP, CURVE_HEIGHT);
    float3 retVal = lerp(luma, color, value);

    if(SHOW_COLOR_DIFFERENCE)
    {
        retVal = value.rrr;
    }
    if(SHOW_DEBUG_OVERLAY)
    {
        retVal = DrawDebugOverlay(retVal, 1.0, DEBUG_OVERLAY_POSITION, DEBUG_OVERLAY_SIZE, DEBUG_OVERLAY_OPACITY, vpos.xy, texcoord);
    }

    return retVal;
}

technique ColorIsolation2 {
    pass {
        VertexShader = PostProcessVS;
        PixelShader = ColorIsolationPS;
        /* RenderTarget = BackBuffer */
    }
}