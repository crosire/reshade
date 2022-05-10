//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Eye Adaption by brussell
// v. 2.31
// License: CC BY 4.0
//
// Credits:
// luluco250 - luminance get/store code from Magic Bloom
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

#include "ReShadeUI.fxh"

//effect parameters
uniform float fAdp_Delay < __UNIFORM_SLIDER_FLOAT1
    ui_label = "Adaption Delay";
    ui_tooltip = "How fast the image adapts to brightness changes.\n"
                 "0 = instantanous adaption\n"
                 "2 = very slow adaption";
    ui_category = "General settings";
    ui_min = 0.0;
    ui_max = 2.0;
> = 1.6;

uniform float fAdp_TriggerRadius < __UNIFORM_SLIDER_FLOAT1
    ui_label = "Adaption TriggerRadius";
    ui_tooltip = "Screen area, whose average brightness triggers adaption.\n"
                 "1 = only the center of the image is used\n"
                 "7 = the whole image is used";
    ui_category = "General settings";
    ui_min = 1.0;
    ui_max = 7.0;
    ui_step = 0.1;
> = 6.0;

uniform float fAdp_YAxisFocalPoint < __UNIFORM_SLIDER_FLOAT1
    ui_label = "Y Axis Focal Point";
    ui_tooltip = "Where along the Y Axis the Adaption TriggerRadius applies.\n"
                 "0 = Top of the screen\n"
                 "1 = Bottom of the screen";
    ui_category = "General settings";
    ui_min = 0.0;
    ui_max = 1.0;
> = 0.5;

uniform float fAdp_Equilibrium < __UNIFORM_SLIDER_FLOAT1
    ui_label = "Adaption Equilibrium";
    ui_tooltip = "The value of image brightness for which there is no brightness adaption.\n"
                 "0 = late brightening, early darkening\n"
                 "1 = early brightening, late darkening";
    ui_category = "General settings";
    ui_min = 0.0;
    ui_max = 1.0;
> = 0.5;

uniform float fAdp_Strength < __UNIFORM_SLIDER_FLOAT1
    ui_label = "Adaption Strength";
    ui_tooltip = "Base strength of brightness adaption.\n";
    ui_category = "General settings";
    ui_min = 0.0;
    ui_max = 2.0;
> = 1.0;

uniform float fAdp_BrightenHighlights < __UNIFORM_SLIDER_FLOAT1
    ui_label = "Brighten Highlights";
    ui_tooltip = "Brightening strength for highlights.";
    ui_category = "Brightening";
    ui_min = 0.0;
    ui_max = 1.0;
> = 0.1;

uniform float fAdp_BrightenMidtones < __UNIFORM_SLIDER_FLOAT1
    ui_label = "Brighten Midtones";
    ui_tooltip = "Brightening strength for midtones.";
    ui_category = "Brightening";
    ui_min = 0.0;
    ui_max = 1.0;
> = 0.2;

uniform float fAdp_BrightenShadows < __UNIFORM_SLIDER_FLOAT1
    ui_label = "Brighten Shadows";
    ui_tooltip = "Brightening strength for shadows.\n"
                 "Set this to 0 to preserve pure black.";
    ui_category = "Brightening";
    ui_min = 0.0;
    ui_max = 1.0;
> = 0.1;

uniform float fAdp_DarkenHighlights < __UNIFORM_SLIDER_FLOAT1
    ui_label = "Darken Highlights";
    ui_tooltip = "Darkening strength for highlights.\n"
                 "Set this to 0 to preserve pure white.";
    ui_category = "Darkening";
    ui_min = 0.0;
    ui_max = 1.0;
> = 0.1;

uniform float fAdp_DarkenMidtones < __UNIFORM_SLIDER_FLOAT1
    ui_label = "Darken Midtones";
    ui_tooltip = "Darkening strength for midtones.";
    ui_category = "Darkening";
    ui_min = 0.0;
    ui_max = 1.0;
> = 0.2;

uniform float fAdp_DarkenShadows < __UNIFORM_SLIDER_FLOAT1
    ui_label = "Darken Shadows";
    ui_tooltip = "Darkening strength for shadows.";
    ui_category = "Darkening";
    ui_min = 0.0;
    ui_max = 1.0;
> = 0.1;

#include "ReShade.fxh"

//global vars
#define LumCoeff float3(0.212656, 0.715158, 0.072186)
uniform float Frametime < source = "frametime";>;

//textures and samplers
texture2D TexLuma { Width = 256; Height = 256; Format = R8; MipLevels = 7; };
texture2D TexAvgLuma { Format = R16F; };
texture2D TexAvgLumaLast { Format = R16F; };

sampler SamplerLuma { Texture = TexLuma; };
sampler SamplerAvgLuma { Texture = TexAvgLuma; };
sampler SamplerAvgLumaLast { Texture = TexAvgLumaLast; };

//pixel shaders
float PS_Luma(float4 pos : SV_Position, float2 texcoord : TEXCOORD) : SV_Target
{
    float4 color = tex2Dlod(ReShade::BackBuffer, float4(texcoord, 0, 0));
    float luma = dot(color.xyz, LumCoeff);
    return luma;
}

float PS_AvgLuma(float4 pos : SV_Position, float2 texcoord : TEXCOORD) : SV_Target
{
    float avgLumaCurrFrame = tex2Dlod(SamplerLuma, float4(fAdp_YAxisFocalPoint.xx, 0, fAdp_TriggerRadius)).x;
    float avgLumaLastFrame = tex2Dlod(SamplerAvgLumaLast, float4(0.0.xx, 0, 0)).x;
    float delay = sign(fAdp_Delay) * saturate(0.815 + fAdp_Delay / 10.0 - Frametime / 1000.0);
    float avgLuma = lerp(avgLumaCurrFrame, avgLumaLastFrame, delay);
    return avgLuma;
}

float AdaptionDelta(float luma, float strengthMidtones, float strengthShadows, float strengthHighlights)
{
    float midtones = (4.0 * strengthMidtones - strengthHighlights - strengthShadows) * luma * (1.0 - luma);
    float shadows = strengthShadows * (1.0 - luma);
    float highlights = strengthHighlights * luma;
    float delta = midtones + shadows + highlights;
    return delta;
}

float4 PS_Adaption(float4 pos : SV_Position, float2 texcoord : TEXCOORD) : SV_Target
{
    float4 color = tex2Dlod(ReShade::BackBuffer, float4(texcoord, 0, 0));
    float avgLuma = tex2Dlod(SamplerAvgLuma, float4(0.0.xx, 0, 0)).x;

    color.xyz = pow(abs(color.xyz), 1.0/2.2);
    float luma = dot(color.xyz, LumCoeff);
    float3 chroma = color.xyz - luma;

    float avgLumaAdjusted = lerp (avgLuma, 1.4 * avgLuma / (0.4 + avgLuma), fAdp_Equilibrium);
    float delta = 0;

    float curve = fAdp_Strength * 10.0 * pow(abs(avgLumaAdjusted - 0.5), 4.0);
    if (avgLumaAdjusted < 0.5) {
        delta = AdaptionDelta(luma, fAdp_BrightenMidtones, fAdp_BrightenShadows, fAdp_BrightenHighlights);
    } else {
        delta = -AdaptionDelta(luma, fAdp_DarkenMidtones, fAdp_DarkenShadows, fAdp_DarkenHighlights);
    }
    delta *= curve;

    luma += delta;
    color.xyz = saturate(luma + chroma);
    color.xyz = pow(abs(color.xyz), 2.2);

    return color;
}

float PS_StoreAvgLuma(float4 pos : SV_Position, float2 texcoord : TEXCOORD) : SV_Target
{
    float avgLuma = tex2Dlod(SamplerAvgLuma, float4(0.0.xx, 0, 0)).x;
    return avgLuma;
}

//techniques
technique EyeAdaption {

    pass Luma
    {
        VertexShader = PostProcessVS;
        PixelShader = PS_Luma;
        RenderTarget = TexLuma;
    }

    pass AvgLuma
    {
        VertexShader = PostProcessVS;
        PixelShader = PS_AvgLuma;
        RenderTarget = TexAvgLuma;
    }

    pass Adaption
    {
        VertexShader = PostProcessVS;
        PixelShader = PS_Adaption;
    }

    pass StoreAvgLuma
    {
        VertexShader = PostProcessVS;
        PixelShader = PS_StoreAvgLuma;
        RenderTarget = TexAvgLumaLast;
    }
}
