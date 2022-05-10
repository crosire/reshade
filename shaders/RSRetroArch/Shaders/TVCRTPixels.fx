/*
    TV CRT Pixels from https://www.shadertoy.com/view/XsfGDl
    Ported by luluco250

    LICENSE:
    Same license as the original shader: CC BY-NC-SA 3.0
    https://creativecommons.org/licenses/by-nc-sa/3.0/
*/

#include "ReShade.fxh"

uniform float fTVCRTPixels_PixelSize <
    ui_label = "Pixel Size [TV CRT Pixels]";
    ui_type = "drag";
    ui_min = 1.0;
    ui_max = 10.0;
    ui_step = 0.001;
> = 3.0;

uniform float fTVCRTPixels_Gamma <
    ui_label = "Gamma [TV CRT Pixels]";
    ui_type = "drag";
    ui_min = 1.0;
    ui_max = 2.2;
    ui_step = 0.001;
> = 1.2;

void TVCRTPixels(in float4 pos : SV_POSITION, in float2 uv : TEXCOORD0, out float4 col : COLOR0) {
    float2 texcoord = uv * ReShade::ScreenSize; //this is because the original shader uses OpenGL's fragCoord, which is in texels rather than pixels
    float2 coord;
    coord.x = texcoord.x / fTVCRTPixels_PixelSize;
    coord.y = (texcoord.y + fTVCRTPixels_PixelSize * 1.5 * (floor(coord.x) % 2.0)) / (fTVCRTPixels_PixelSize * 3.0);

    float2 iCoord = floor(coord);
    float2 fCoord = frac(coord);

    float3 pixel = step(1.5, (float3(0.0, 1.0, 2.0) + iCoord.x) % 3.0);
    float3 image = tex2D(ReShade::BackBuffer, fTVCRTPixels_PixelSize * iCoord * float2(1.0, 3.0) * ReShade::PixelSize).rgb;

    col.rgb = pixel * dot(pixel, image);

    col.rgb *= step(abs(fCoord.x - 0.5), 0.4);
    col.rgb *= step(abs(fCoord.y - 0.5), 0.4);

    col.rgb *= fTVCRTPixels_Gamma;
    col.a = 1.0;
}

technique TVCRTPixels {
    pass TVCRTPixels {
        VertexShader=PostProcessVS;
        PixelShader=TVCRTPixels;
    }
}