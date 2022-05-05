/*******************************************************
	ReShade Shader: Composition
	https://github.com/Daodan317081/reshade-shaders
*******************************************************/

#include "ReShade.fxh"

#define GOLDEN_RATIO 1.6180339887
#define INV_GOLDEN_RATIO  1.0 / 1.6180339887

uniform float4 UIGridColor <
	ui_type = "color";
    ui_label = "Grid Color";
> = float4(0.0, 0.0, 0.0, 1.0);

uniform float UIGridLineWidth <
	ui_type = "drag";
    ui_label = "Grid Line Width";
    ui_min = 0.0; ui_max = 5.0;
    ui_steps = 0.01;
> = 1.0;

struct sctpoint {
    float3 color;
    float2 coord;
    float2 offset;
};

sctpoint NewPoint(float3 color, float2 offset, float2 coord) {
    sctpoint p;
    p.color = color;
    p.offset = offset;
    p.coord = coord;
    return p;
}

float3 DrawPoint(float3 texcolor, sctpoint p, float2 texcoord) {
    float2 pixelsize = ReShade::PixelSize * p.offset;
    
    if(p.coord.x == -1 || p.coord.y == -1)
        return texcolor;

    if(texcoord.x <= p.coord.x + pixelsize.x &&
    texcoord.x >= p.coord.x - pixelsize.x &&
    texcoord.y <= p.coord.y + pixelsize.y &&
    texcoord.y >= p.coord.y - pixelsize.y)
        return p.color;
    return texcolor;
}


float3 DrawCenterLines(float3 background, float3 gridColor, float lineWidth, float2 texcoord) {
    float3 result;    

    sctpoint lineV1 = NewPoint(gridColor, lineWidth, float2(0.5, texcoord.y));
    sctpoint lineH1 = NewPoint(gridColor, lineWidth, float2(texcoord.x, 0.5));
    
    result = DrawPoint(background, lineV1, texcoord);
    result = DrawPoint(result, lineH1, texcoord);

    return result;
}

float3 DrawThirds(float3 background, float3 gridColor, float lineWidth, float2 texcoord) {
    float3 result;    

    sctpoint lineV1 = NewPoint(gridColor, lineWidth, float2(1.0 / 3.0, texcoord.y));
    sctpoint lineV2 = NewPoint(gridColor, lineWidth, float2(2.0 / 3.0, texcoord.y));

    sctpoint lineH1 = NewPoint(gridColor, lineWidth, float2(texcoord.x, 1.0 / 3.0));
    sctpoint lineH2 = NewPoint(gridColor, lineWidth, float2(texcoord.x, 2.0 / 3.0));
    
    result = DrawPoint(background, lineV1, texcoord);
    result = DrawPoint(result, lineV2, texcoord);
    result = DrawPoint(result, lineH1, texcoord);
    result = DrawPoint(result, lineH2, texcoord);

    return result;
}

float3 DrawFifths(float3 background, float3 gridColor, float lineWidth, float2 texcoord) {
    float3 result;    

    sctpoint lineV1 = NewPoint(gridColor, lineWidth, float2(1.0 / 5.0, texcoord.y));
    sctpoint lineV2 = NewPoint(gridColor, lineWidth, float2(2.0 / 5.0, texcoord.y));
    sctpoint lineV3 = NewPoint(gridColor, lineWidth, float2(3.0 / 5.0, texcoord.y));
    sctpoint lineV4 = NewPoint(gridColor, lineWidth, float2(4.0 / 5.0, texcoord.y));

    sctpoint lineH1 = NewPoint(gridColor, lineWidth, float2(texcoord.x, 1.0 / 5.0));
    sctpoint lineH2 = NewPoint(gridColor, lineWidth, float2(texcoord.x, 2.0 / 5.0));
    sctpoint lineH3 = NewPoint(gridColor, lineWidth, float2(texcoord.x, 3.0 / 5.0));
    sctpoint lineH4 = NewPoint(gridColor, lineWidth, float2(texcoord.x, 4.0 / 5.0));
    
    result = DrawPoint(background, lineV1, texcoord);
    result = DrawPoint(result, lineV2, texcoord);
    result = DrawPoint(result, lineV3, texcoord);
    result = DrawPoint(result, lineV4, texcoord);
    result = DrawPoint(result, lineH1, texcoord);
    result = DrawPoint(result, lineH2, texcoord);
    result = DrawPoint(result, lineH3, texcoord);
    result = DrawPoint(result, lineH4, texcoord);

    return result;
}

float3 DrawGoldenRatio(float3 background, float3 gridColor, float lineWidth, float2 texcoord) {
    float3 result;    

    sctpoint lineV1 = NewPoint(gridColor, lineWidth, float2(1.0 / GOLDEN_RATIO, texcoord.y));
    sctpoint lineV2 = NewPoint(gridColor, lineWidth, float2(1.0 - 1.0 / GOLDEN_RATIO, texcoord.y));

    sctpoint lineH1 = NewPoint(gridColor, lineWidth, float2(texcoord.x, 1.0 / GOLDEN_RATIO));
    sctpoint lineH2 = NewPoint(gridColor, lineWidth, float2(texcoord.x, 1.0 - 1.0 / GOLDEN_RATIO));
    
    result = DrawPoint(background, lineV1, texcoord);
    result = DrawPoint(result, lineV2, texcoord);
    result = DrawPoint(result, lineH1, texcoord);
    result = DrawPoint(result, lineH2, texcoord);

    return result;
}

float3 DrawDiagonals(float3 background, float3 gridColor, float lineWidth, float2 texcoord) {
    float3 result;    
    float slope = (float)BUFFER_WIDTH / (float)BUFFER_HEIGHT;

    sctpoint line1 = NewPoint(gridColor, lineWidth,    float2(texcoord.x, texcoord.x * slope));
    sctpoint line2 = NewPoint(gridColor, lineWidth,  float2(texcoord.x, 1.0 - texcoord.x * slope));
    sctpoint line3 = NewPoint(gridColor, lineWidth,   float2(texcoord.x, (1.0 - texcoord.x) * slope));
    sctpoint line4 = NewPoint(gridColor, lineWidth,  float2(texcoord.x, texcoord.x * slope + 1.0 - slope));
    
    result = DrawPoint(background, line1, texcoord);
    result = DrawPoint(result, line2, texcoord);
    result = DrawPoint(result, line3, texcoord);
    result = DrawPoint(result, line4, texcoord);

    return result;
}

float3 CenterLines_PS(float4 vpos : SV_Position, float2 texcoord : TexCoord) : SV_Target {
    float3 background = tex2D(ReShade::BackBuffer, texcoord).rgb;
    float3 result = DrawCenterLines(background, UIGridColor.rgb, UIGridLineWidth, texcoord);
    return lerp(background, result, UIGridColor.w);
}
float3 Thirds_PS(float4 vpos : SV_Position, float2 texcoord : TexCoord) : SV_Target {
    float3 background = tex2D(ReShade::BackBuffer, texcoord).rgb;
    float3 result = DrawThirds(background, UIGridColor.rgb, UIGridLineWidth, texcoord);
    return lerp(background, result, UIGridColor.w);
}
float3 Fifths_PS(float4 vpos : SV_Position, float2 texcoord : TexCoord) : SV_Target {
    float3 background = tex2D(ReShade::BackBuffer, texcoord).rgb;
    float3 result = DrawFifths(background, UIGridColor.rgb, UIGridLineWidth, texcoord);
    return lerp(background, result, UIGridColor.w);
}
float3 GoldenRatio_PS(float4 vpos : SV_Position, float2 texcoord : TexCoord) : SV_Target {
    float3 background = tex2D(ReShade::BackBuffer, texcoord).rgb;
    float3 result = DrawGoldenRatio(background, UIGridColor.rgb, UIGridLineWidth, texcoord);
    return lerp(background, result, UIGridColor.w);
}
float3 Diagonals_PS(float4 vpos : SV_Position, float2 texcoord : TexCoord) : SV_Target {
    float3 background = tex2D(ReShade::BackBuffer, texcoord).rgb;
    float3 result = DrawDiagonals(background, UIGridColor.rgb, UIGridLineWidth, texcoord);
    return lerp(background, result, UIGridColor.w);
}


technique CompositionCeterLines
{
	pass {
		VertexShader = PostProcessVS;
		PixelShader = CenterLines_PS;
	}
}
technique CompositionThirds
{
	pass {
		VertexShader = PostProcessVS;
		PixelShader = Thirds_PS;
	}
}
technique CompositionFifths
{
	pass {
		VertexShader = PostProcessVS;
		PixelShader = Fifths_PS;
	}
}
technique CompositionGoldenRatio
{
	pass {
		VertexShader = PostProcessVS;
		PixelShader = GoldenRatio_PS;
	}
}
technique CompositionDiagonals
{
	pass {
		VertexShader = PostProcessVS;
		PixelShader = Diagonals_PS;
	}
}