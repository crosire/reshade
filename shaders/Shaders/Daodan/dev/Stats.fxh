/*******************************************************
	ReShade Header: Stats
	https://github.com/Daodan317081/reshade-shaders
*******************************************************/

#include "ReShade.fxh"

#ifndef STATS_MIPLEVEL
    #define STATS_MIPLEVEL 7.0
#endif

namespace Stats {
	texture2D shared_texStats { Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = RGBA8; MipLevels =  STATS_MIPLEVEL; };
	sampler2D shared_SamplerStats { Texture = shared_texStats; };
	float3 OriginalBackBuffer(float2 texcoord) { return tex2D(shared_SamplerStats, texcoord).rgb; }

	texture2D shared_texStatsAvgColor { Format = RGBA8; };
	sampler2D shared_SamplerStatsAvgColor { Texture = shared_texStatsAvgColor; };
	float3 AverageColor() { return tex2Dfetch(shared_SamplerStatsAvgColor, int4(0, 0, 0, 0)).rgb; }

	texture2D shared_texStatsAvgLuma { Format = R16F; };
	sampler2D shared_SamplerStatsAvgLuma { Texture = shared_texStatsAvgLuma; };
	float AverageLuma() { return tex2Dfetch(shared_SamplerStatsAvgLuma, int4(0, 0, 0, 0)).r; }

	texture2D shared_texStatsAvgColorTemp { Format = R16F; };
	sampler2D shared_SamplerStatsAvgColorTemp { Texture = shared_texStatsAvgColorTemp; };
	float AverageColorTemp() { return tex2Dfetch(shared_SamplerStatsAvgColorTemp, int4(0, 0, 0, 0)).r; }
}