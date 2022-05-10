#include "ReShade.fxh"

uniform float texture_sizeY <
	ui_type = "drag";
	ui_min = 1.0;
	ui_max = BUFFER_HEIGHT;
	ui_label = "Scanlines Height [NES-MINI CRT]";
> = 240.0;

#define mod(x,y) (x-y*floor(x/y))

float4 PS_CRT_NESMINI(float4 vpos : SV_POSITION, float2 texcoord : TEXCOORD0) : SV_Target
{
    float3 texel = tex2D(ReShade::BackBuffer, texcoord).rgb;
    float3 pixelHigh = (1.2 - (0.2 * texel)) * texel;
    float3 pixelLow  = (0.85 + (0.1 * texel)) * texel;
    float selectY = mod(texcoord.y * 2 * texture_sizeY, 2.0);
    float selectHigh = step(1.0, selectY);
    float selectLow = 1.0 - selectHigh;
    float3 pixelColor = (selectLow * pixelLow) + (selectHigh * pixelHigh);

    return float4(pixelColor, 1.0);
}

technique CRT_NES_MINI {
	pass NESMINI{
		VertexShader = PostProcessVS;
		PixelShader = PS_CRT_NESMINI;
	}
}