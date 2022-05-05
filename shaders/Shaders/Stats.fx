/*******************************************************
	ReShade Shader: Stats
	https://github.com/Daodan317081/reshade-shaders
*******************************************************/

#include "ReShade.fxh"
#include "Tools.fxh"
#include "Stats.fxh"

#ifndef STATS_DEBUG_IMAGE_WIDTH
    #define STATS_DEBUG_IMAGE_WIDTH 512
#endif

#ifndef STATS_DEBUG_IMAGE_HEIGHT
    #define STATS_DEBUG_IMAGE_HEIGHT 16
#endif

uniform float fUISpeed <
    ui_type = "drag";
    ui_min = 0.0; ui_max = 1.0;
> = 0.004;

uniform int iUIDebug <
    ui_type = "combo";
    ui_label = "Override Values";
    ui_items = "None\0Average Luma\0YIQ.I (\"Color Temperature\")\0Both\0";
> = 0;

uniform float fUIOverrideLuma <
    ui_type = "drag";
    ui_label = "Override Luma";
    ui_min = 0.0; ui_max = 1.0;
> = 0.5;

uniform float fUIOverrideYIQ_I <
    ui_type = "drag";
    ui_label = "Override YIQ_I";
    ui_min = 0.0; ui_max = 1.0;
> = 0.5;

uniform int2 iUIDebugImagePos <
    ui_type = "drag";
    ui_label = "Debug Image Pos";
    ui_min = 0; ui_max = BUFFER_WIDTH;
    ui_step = 1;
> = int2(0,0);

texture2D texStatsAvgColorLast { Format = RGBA8; };
sampler2D SamplerStatsAvgColorLast { Texture = texStatsAvgColorLast; };

texture2D texStatsAvgLumaLast { Format = R16F; };
sampler2D SamplerStatsAvgLumaLast { Texture = texStatsAvgLumaLast; };

texture2D texStatsAvgColorTempLast { Format = R16F; };
sampler2D SamplerStatsAvgColorTempLast { Texture = texStatsAvgColorTempLast; };

texture2D texStatsImage { Width = STATS_DEBUG_IMAGE_WIDTH; Height = STATS_DEBUG_IMAGE_HEIGHT; Format = RGBA8; };
sampler2D SamplerStatsImage { Texture = texStatsImage; };

void Stats_PreRender_PS(float4 vpos : SV_Position, float2 texcoord : TexCoord, out float3 color : SV_Target0) {
    color = tex2D(ReShade::BackBuffer, texcoord).rgb;
}

void Stats_Averages_PS(float4 vpos : SV_Position, float2 texcoord : TexCoord, out float3 avgColor : SV_Target0, out float avgLuma : SV_Target1, out float avgColorTemp : SV_Target2) {
    float3 color =  tex2Dlod(Stats::shared_SamplerStats, float4(0.5.xx, 0, STATS_MIPLEVEL)).rgb;
    avgColor =      lerp(tex2Dfetch(SamplerStatsAvgColorLast, int4(0, 0, 0, 0)).rgb,   color,                      fUISpeed);
    avgLuma =       lerp(tex2Dfetch(SamplerStatsAvgLumaLast, int4(0, 0, 0, 0)).x,      dot(color, LumaCoeff),      fUISpeed);
    avgColorTemp =  lerp(tex2Dfetch(SamplerStatsAvgColorTempLast, int4(0, 0, 0, 0)).x, Tools::Color::RGBtoYIQ(color).y,   fUISpeed);

    if(iUIDebug == 1)
        avgLuma = fUIOverrideLuma;
    else if(iUIDebug == 2)
        avgColorTemp = Tools::Functions::Map(fUIOverrideYIQ_I, FLOAT_RANGE, YIQ_I_RANGE);
    else if(iUIDebug == 3) {
        avgLuma = fUIOverrideLuma;
        avgColorTemp = Tools::Functions::Map(fUIOverrideYIQ_I, FLOAT_RANGE, YIQ_I_RANGE);
    }
}

void Stats_AveragesLast_PS(float4 vpos : SV_Position, float2 texcoord : TexCoord, out float3 avgColor : SV_Target0, out float avgLuma : SV_Target1, out float avgColorTemp : SV_Target2) {
    avgColor =      tex2Dfetch(Stats::shared_SamplerStatsAvgColor,     int4(0, 0, 0, 0)).rgb;
    avgLuma =       tex2Dfetch(Stats::shared_SamplerStatsAvgLuma,      int4(0, 0, 0, 0)).x;
    avgColorTemp =  tex2Dfetch(Stats::shared_SamplerStatsAvgColorTemp, int4(0, 0, 0, 0)).x;
}

void Create_Stats_Image_PS(float4 position : SV_Position, float2 texcoord : TEXCOORD, out float3 result : SV_Target0) {	
    float offset = 2.0;
    float avgLuma = tex2D(Stats::shared_SamplerStatsAvgLuma, 0.5.xx).r;
    float3 warm = Tools::Color::YIQtoRGB(float3(0.5, YIQ_I_RANGE.y, 0.0));
	float3 cold = Tools::Color::YIQtoRGB(float3(0.5, YIQ_I_RANGE.x, 0.0));
    float avgColorTemp = Tools::Functions::Map(tex2D(Stats::shared_SamplerStatsAvgColorTemp, 0.5.xx).r, YIQ_I_RANGE, FLOAT_RANGE);
    float3 avgColor = tex2Dfetch(Stats::shared_SamplerStatsAvgColor, int4(0, 0, 0, 0)).rgb;
    float3 tmpScale = lerp(cold, warm, texcoord.x);

	sctpoint scaleLuma = Tools::Types::Point(texcoord.x, float2(texcoord.x, texcoord.y < 0.5 ? texcoord.y : -1));
    sctpoint markerAvgLuma = Tools::Types::Point(MAGENTA, float2(avgLuma, texcoord.y < 0.5 ? texcoord.y : -1));

    sctpoint scaleColorTemp = Tools::Types::Point(tmpScale, float2(texcoord.x, texcoord.y > 0.5 ? texcoord.y : -1));
    sctpoint markerAvgColorTemp = Tools::Types::Point(BLACK, float2(avgColorTemp, texcoord.y > 0.5 ? texcoord.y : -1));
    
    result = Tools::Draw::PointEXP(MAGENTA, scaleLuma, texcoord, STATS_DEBUG_IMAGE_WIDTH * 0.66);
    result = Tools::Draw::PointEXP(result, markerAvgLuma, texcoord, STATS_DEBUG_IMAGE_WIDTH * 0.66);
    result = Tools::Draw::PointEXP(result, scaleColorTemp, texcoord, STATS_DEBUG_IMAGE_WIDTH * 0.66);
    result = Tools::Draw::PointEXP(result, markerAvgColorTemp, texcoord, STATS_DEBUG_IMAGE_WIDTH * 0.66);
}

/*******************************************************
	Draw Debugimage on backbuffer
*******************************************************/
float3 Overlay_Stats_PS(float4 vpos : SV_Position, float2 texcoord : TexCoord) : SV_Target {
	float3 backbuffer = tex2D(ReShade::BackBuffer, texcoord).rgb;
	int2 texsize = tex2Dsize(SamplerStatsImage, 0);
	int x = clamp(iUIDebugImagePos.x, 0, BUFFER_WIDTH - texsize.x);
	int y = clamp(iUIDebugImagePos.y, 0, BUFFER_HEIGHT - texsize.y);
	return Tools::Draw::OverlaySampler(backbuffer, SamplerStatsImage, 1.0, texcoord, int2(x,y), 1.0);
}

technique CalculateStats_MoveToTop {
    pass {
        VertexShader =  PostProcessVS;
        PixelShader =   Stats_PreRender_PS;
        RenderTarget0 = Stats::shared_texStats;
    }
    pass {
        VertexShader =  PostProcessVS;
        PixelShader =   Stats_Averages_PS;
        RenderTarget0 = Stats::shared_texStatsAvgColor;
        RenderTarget1 = Stats::shared_texStatsAvgLuma;
        RenderTarget2 = Stats::shared_texStatsAvgColorTemp;
    }
    pass {
        VertexShader =  PostProcessVS;
        PixelShader =   Stats_AveragesLast_PS;
        RenderTarget0 = texStatsAvgColorLast;
        RenderTarget1 = texStatsAvgLumaLast;
        RenderTarget2 = texStatsAvgColorTempLast;
    }
}
technique ShowStats_MoveToBottom {
    pass {
        VertexShader = PostProcessVS;
        PixelShader = Create_Stats_Image_PS;
        RenderTarget0 = texStatsImage;
    }
    pass {
        VertexShader = PostProcessVS;
        PixelShader = Overlay_Stats_PS;
    }
}
