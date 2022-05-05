#include "ReShade.fxh"

uniform float3 fUIColor<
    ui_type = "color";
    ui_label = "Color";
> = float3(0.1, 0.0, 0.3);

uniform float fUIStrength<
    ui_type = "drag";
    ui_label = "Srength";
    ui_min = 0.0; ui_max = 1.0;
    ui_step = 0.01;
> = 1.0;

float3 RetroTintPS(float4 vpos : SV_Position, float2 texcoord : TexCoord) : SV_Target {
    float3 color = tex2D(ReShade::BackBuffer, texcoord).rgb;
    //Blend mode: Screen
    return lerp(color, 1.0 - (1.0 - color) * (1.0 - fUIColor), fUIStrength);
}

technique RetroTint {
    pass {
        VertexShader = PostProcessVS;
        PixelShader = RetroTintPS;
        /* RenderTarget = BackBuffer */
    }
}