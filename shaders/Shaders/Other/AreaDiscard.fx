//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// AreaDiscard by brussell
// v. 1.0
// License: CC BY 4.0
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

//effect parameters
uniform bool bAD_EnableArea <
      ui_label = "Enable area";
      ui_tooltip = "Enable discarding/coloring of a custom rectangle area.";
> = false;

uniform float2 fAD_AreaXY <
    ui_label = "Area coordinates";
    ui_tooltip = "Top left x and y coordinate of the area.";
    ui_type = "drag";
    ui_min = 0.0;
    ui_max = BUFFER_WIDTH;
    ui_step = 1.0;
> = float2(800, 200);

uniform float2 fAD_AreaSize <
    ui_label = "Area dimensions";
    ui_tooltip = "Size of the area in x and y dimension.";
    ui_type = "drag";
    ui_min = 0.0;
    ui_max = BUFFER_WIDTH;
    ui_step = 1.0;
> = float2(400, 300);

uniform bool bAD_EnableEdges <
      ui_label = "Enable edges";
      ui_tooltip = "Enable discarding/coloring of screen edges.";
> = true;

uniform float fAD_EdgeLeftWidth <
    ui_label = "Left edge width";
    ui_tooltip = "Left edge width in pixels.";
    ui_type = "drag";
    ui_min = 0.0;
    ui_max = BUFFER_WIDTH / 2;
    ui_step = 1.0;
> = 240;

uniform float fAD_EdgeRightWidth <
    ui_label = "Right edge width";
    ui_tooltip = "Right edge width in pixels.";
    ui_type = "drag";
    ui_min = 0.0;
    ui_max = BUFFER_WIDTH / 2;
    ui_step = 1.0;
> = 240;

uniform float fAD_EdgeTopHeight <
    ui_label = "Top edge width";
    ui_tooltip = "Top edge height in pixels.";
    ui_type = "drag";
    ui_min = 0.0;
    ui_max = BUFFER_HEIGHT / 2;
    ui_step = 1.0;
> = 0;

uniform float fAD_EdgeBottomHeight <
    ui_label = "Bottom edge width";
    ui_tooltip = "Bottom edge height in pixels.";
    ui_type = "drag";
    ui_min = 0.0;
    ui_max = BUFFER_HEIGHT / 2;
    ui_step = 1.0;
> = 0;

uniform float3 fAD_FillColor <
    ui_label = "Area/Edges fill color\n";
    ui_type = "color";
> = float3(0.0, 0.0, 0.0);

#include "ReShade.fxh"

#ifndef Enable_Coloring
#define Enable_Coloring   0     // [0 or 1] Only coloring, no color restoration
#endif

//textures and samplers
#if (Enable_Coloring == 0)
texture texColorOrig { Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; };
sampler ColorOrig { Texture = texColorOrig; };
#endif

//pixel shaders
#if (Enable_Coloring == 0)
float4 PS_AreaDiscard_StoreColor(float4 vpos : SV_Position, float2 texcoord : TEXCOORD) : SV_Target {
    return tex2D(ReShade::BackBuffer, texcoord);
}
#endif

float4 PS_AreaDiscard(float4 vpos : SV_Position, float2 texcoord : TEXCOORD) : SV_Target {
    float2 pos = texcoord / ReShade::PixelSize;
    float4 color = tex2Dlod(ReShade::BackBuffer, float4(texcoord, 0, 0));
    float3 colorOrig = fAD_FillColor;

    #if (Enable_Coloring == 0)
    colorOrig =  tex2Dlod(ColorOrig, float4(texcoord, 0, 0)).xyz;
    #endif

    if (bAD_EnableArea) {
        if (pos.x >= fAD_AreaXY.x && pos.y >= fAD_AreaXY.y && pos.x <= (fAD_AreaXY + fAD_AreaSize).x && pos.y <= (fAD_AreaXY + fAD_AreaSize).y) {
            color.xyz = colorOrig;
        }
    }

    if (bAD_EnableEdges) {
        if (pos.x <= fAD_EdgeLeftWidth || pos.y <= fAD_EdgeTopHeight || pos.x >= (BUFFER_WIDTH - fAD_EdgeRightWidth) || pos.y >= (BUFFER_HEIGHT - fAD_EdgeBottomHeight)) {
            color.xyz = colorOrig;
        }
    }

    return color;
}

//techniques
#if (Enable_Coloring == 0)
technique AreaDiscard_StoreColor
{
    pass {
        VertexShader = PostProcessVS;
        PixelShader = PS_AreaDiscard_StoreColor;
        RenderTarget = texColorOrig;
    }
}
#endif
technique AreaDiscard {
    pass {
        VertexShader = PostProcessVS;
        PixelShader = PS_AreaDiscard;
    }
}
