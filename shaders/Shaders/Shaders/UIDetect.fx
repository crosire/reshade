//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// UIDetect by brussell
// v. 2.2
// License: CC BY 4.0
//
// UIDetect is configured by editing the file UIDetect.fxh.
// Please look there for a full description and usage of this shader.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

#include "ReShadeUI.fxh"

//effect parameters
uniform int infotext<
	ui_label = " ";
    ui_text =
		"Note:\n"
        "UIDetect is configured by editing the file UIDetect.fxh.\n"
		"Please look there for a full description and usage of this shader.\n";
	ui_type = "radio";
> ;

#include "ReShade.fxh"
#include "UIDetect.fxh"

//global vars
#if (UIDetect_LEGACYMODE == 1)
    #define offset 0.0
#else
    #define offset 0.5
#endif

//textures and samplers
texture texColorOrig { Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; };
sampler ColorOrig { Texture = texColorOrig; };

texture texUIDetect { Width = 1; Height = 1; Format = R8; };
sampler UIDetect { Texture = texUIDetect; };

#if (UIDetect_USE_RGB_MASK == 1)
    texture texUIDetectMask <source="UIDetectMaskRGB.png";>
    { Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = RGBA8; };
    sampler UIDetectMask { Texture = texUIDetectMask; };
#endif

//pixel shaders
float PS_UIDetect(float4 pos : SV_Position, float2 texcoord : TEXCOORD) : SV_Target
{
    float3 pixelColor, uiPixelColor, diff;
    float2 pixelCoord;
    float ui = 1;
    bool uiDetected = false;
    bool uiNext = false;

    for (int i=0; i < PIXELNUMBER; i++)
    {
        [branch]
        if (UIPixelCoord_UINr[i].z - ui == 0) {
            if (uiNext == false){
                pixelCoord = float2(UIPixelCoord_UINr[i].x + offset, UIPixelCoord_UINr[i].y + offset) * BUFFER_PIXEL_SIZE;
                pixelColor = round(tex2Dlod(ReShade::BackBuffer, float4(pixelCoord, 0, 0)).rgb * 255);
                uiPixelColor = UIPixelRGB[i].rgb;
                diff = pixelColor - uiPixelColor;
                if (!any(diff)) {
                    uiDetected = true;
                }else{
                    uiDetected = false;
                    uiNext = true;
                }
            }
        } else {
            if (uiDetected == true) return ui * 0.1;
            ui += 1;
            uiNext = false;
            i -= 1;
        }
    }
    return uiDetected * ui * 0.1;
}

float4 PS_StoreColor(float4 pos : SV_Position, float2 texcoord : TEXCOORD) : SV_Target
{
    return tex2D(ReShade::BackBuffer, texcoord);
}

float4 PS_RestoreColor(float4 pos : SV_Position, float2 texcoord : TEXCOORD) : SV_Target
{
    float ui = tex2D(UIDetect, float2(0, 0)).x;
    #if (UIDetect_INVERT == 0)
        float4 colorOrig = tex2D(ColorOrig, texcoord);
        float4 color = tex2D(ReShade::BackBuffer, texcoord);
    #else
        float4 color = tex2D(ColorOrig, texcoord);
        float4 colorOrig = tex2D(ReShade::BackBuffer, texcoord);
    #endif

    #if (UIDetect_USE_RGB_MASK == 1)
        float3 uiMaskRGB = 1 - tex2D(UIDetectMask, texcoord).rgb;
        float3 uiMask = 0;

        if      (ui > .39) uiMask = 1;            //UI-Nr >3 -> apply no mask
        else if (ui > .29) uiMask = uiMaskRGB.b;  //UI-Nr  3 -> apply masklayer blue
        else if (ui > .19) uiMask = uiMaskRGB.g;  //UI-Nr  2 -> apply masklayer green
        else if (ui > .09) uiMask = uiMaskRGB.r;  //UI-Nr  1 -> apply masklayer red
        color.rgb = lerp(color.rgb, colorOrig.rgb, uiMask);
    #else
        color = ui == 0 ? color : colorOrig;
    #endif
    return color;
}

//techniques
technique UIDetect
{
    pass {
        VertexShader = PostProcessVS;
        PixelShader = PS_UIDetect;
        RenderTarget = texUIDetect;
        ClearRenderTargets = true;
    }
}

technique UIDetect_Before
{
    pass {
        VertexShader = PostProcessVS;
        PixelShader = PS_StoreColor;
        RenderTarget = texColorOrig;
        ClearRenderTargets = true;
    }
}

technique UIDetect_After
{
    pass {
        VertexShader = PostProcessVS;
        PixelShader = PS_RestoreColor;
        ClearRenderTargets = true;
    }
}
