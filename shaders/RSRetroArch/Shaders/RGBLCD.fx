#include "ReShade.fxh"

uniform int2 RES <
	ui_type = "drag";
	ui_min = 1;
	ui_max = BUFFER_WIDTH;
	ui_step = 1;
	ui_label = "Resolution [RGB-LCD]";
> = int2(200, 100);

uniform float2 BORDER <
	ui_type = "drag";
	ui_min = 0.001;
	ui_max = 5.0;
	ui_step = 0.001;
	ui_label = "Border [RGB-LCD]";
> = float2(0.1, 0.1);

float4 PS_RGBLCD(float4 position:SV_Position,float2 texcoord:TEXCOORD):SV_Target
{
	float2 fragCrd = texcoord * ReShade::ScreenSize;
	float4 frgCol;
    float2 uv = fragCrd / ReShade::ScreenSize.xy * RES;
    float2 p = frac(uv);
    float barW = (1.0 - 2.0 * BORDER.x) / 3.0;
    float3 rgb = step(
        float3(p.x, BORDER.x + barW, BORDER.x + 2.0 * barW),
        float3(BORDER.x + barW, p.x, p.x)
    );
    rgb.g -= rgb.b;
    float3 tex = tex2D(ReShade::BackBuffer, floor(uv) / RES).rgb;
    float2 mask = step(BORDER, p) * step(p, 1.0 - BORDER);
    frgCol = float4(rgb * tex * mask.x * mask.y,1.0);
	return frgCol;
}

technique RGBLCD {
    pass RGBLCD {
        VertexShader=PostProcessVS;
        PixelShader=PS_RGBLCD;
    }
}