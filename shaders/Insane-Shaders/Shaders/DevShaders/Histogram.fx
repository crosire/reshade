/*
	ReShade Histogram
	By: Lord Of Lunacy
	
	This histogram shader was written to quickly generate 5 common histograms for use in other shaders.
	
	These are the rows that need to be sampled from the Histogram texture for the desired histogram data:
	Luma:  1
	Red:   4
	Green: 7
	Blue:  10
	Depth: 13
*/
#include "ReShade.fxh"

#ifndef SAMPLEDISTANCE
	#define SAMPLEDISTANCE 5
#endif

#define SAMPLEDISTANCE_SQUARED (SAMPLEDISTANCE*SAMPLEDISTANCE)
#define SAMPLEHEIGHT (BUFFER_HEIGHT / SAMPLEDISTANCE)
#define SAMPLEWIDTH (BUFFER_WIDTH / SAMPLEDISTANCE)
#define SAMPLECOUNT (SAMPLEHEIGHT * SAMPLEWIDTH)
#define SAMPLECOUNT_RCP (1/SAMPLECOUNT)

#ifndef HISTOGRAM_LUMA
	#define HISTOGRAM_LUMA 1
#endif

#ifndef HISTOGRAM_RED
	#define HISTOGRAM_RED 1
#endif

#ifndef HISTOGRAM_GREEN
	#define HISTOGRAM_GREEN 1
#endif

#ifndef HISTOGRAM_BLUE
	#define HISTOGRAM_BLUE 1
#endif

#ifndef HISTOGRAM_DEPTH
	#define HISTOGRAM_DEPTH 1
#endif


#define ITERATION_LUMA (5 * HISTOGRAM_LUMA)
#define ITERATION_RED (HISTOGRAM_LUMA + 5 * HISTOGRAM_RED)
#define ITERATION_GREEN (HISTOGRAM_LUMA + HISTOGRAM_RED + 5 * HISTOGRAM_GREEN)
#define ITERATION_BLUE (HISTOGRAM_LUMA + HISTOGRAM_RED + HISTOGRAM_GREEN + 5 * HISTOGRAM_BLUE)
#define ITERATION_DEPTH (HISTOGRAM_LUMA + HISTOGRAM_RED + HISTOGRAM_GREEN + HISTOGRAM_BLUE + 5 * HISTOGRAM_DEPTH)
#define ITERATIONS (HISTOGRAM_LUMA + HISTOGRAM_RED + HISTOGRAM_GREEN + HISTOGRAM_BLUE + HISTOGRAM_DEPTH)

uniform uint BARMULTIPLIER <
	ui_type = "slider";
	ui_min = 0; ui_max = 10;
	ui_label = "Histogram Bar Height Multiplier";
	ui_tooltip = "Use this if the bars of the histogram are too short/too tall";
> = 4;


texture Histogram {Width = 256; Height = 15; Format = R32F;};
sampler sHistogram {Texture = Histogram;};

texture LumaGraph {Width = 256; Height = 144; Format = R8;};
texture RedGraph {Width = 256; Height = 144; Format = R8;};
texture GreenGraph {Width = 256; Height = 144; Format = R8;};
texture BlueGraph {Width = 256; Height = 144; Format = R8;};
texture DepthGraph {Width = 256; Height = 144; Format = R8;};


void HistogramVS(uint id : SV_VERTEXID, out float4 pos : SV_POSITION)
{
	uint iterationCount = ITERATIONS;
	uint iteration = (id % iterationCount) + 5;
	uint xpos = (id / iterationCount) % SAMPLEWIDTH;
	uint ypos = (id / iterationCount) / SAMPLEWIDTH;
	xpos *= SAMPLEDISTANCE;
	ypos *= SAMPLEDISTANCE;
	int4 texturePos = int4(xpos, ypos, 0, 0);
	float color;
	float y;
	if (iteration == ITERATION_LUMA)
	{
		float3 rgb = tex2Dfetch(ReShade::BackBuffer, texturePos).rgb;
		float3 luma = (0.3, 0.59, 0.11);
		color = dot(rgb, luma)*3;
		y = 0.8;
	}
	else if (iteration == ITERATION_RED) 
	{
		color = tex2Dfetch(ReShade::BackBuffer, texturePos).r;
		y = 0.4;
	}
	else if (iteration == ITERATION_GREEN)
	{
		color = tex2Dfetch(ReShade::BackBuffer, texturePos).g;
		y = 0;
	}
	else if (iteration == ITERATION_BLUE)
	{
		color = tex2Dfetch(ReShade::BackBuffer, texturePos).b;
		y = -0.4;
	}
    else if (iteration == ITERATION_DEPTH)
    {
        color = ReShade::GetLinearizedDepth(texturePos.xy * float2(BUFFER_RCP_WIDTH, BUFFER_RCP_HEIGHT));
        y = -0.8;
    }
	color = (color * 255 + 0.5)/256;
	pos = float4(color * 2 - 1, y, 0, 1);
}

void LumaGraphVS(in uint id : SV_VertexID, out float4 pos : SV_Position)
{
	uint xpos = (id/2) % 256;
	bool odd = id % 2;
	int4 histogramPos = int4(xpos, 1, 0, 0);
	float barHeight = (tex2Dfetch(sHistogram, histogramPos).r * 255);
	float y = odd ? ((barHeight * 2 * SAMPLEDISTANCE_SQUARED)) / (256 * pow(10, BARMULTIPLIER)) - 1 : -1;
	float x = float((xpos+0.5) / 256);
	pos = float4(x * 2 - 1, y, 0, 1);
}

void RedGraphVS(in uint id : SV_VertexID, out float4 pos : SV_Position)
{
	uint xpos = (id/2) % 256;
	bool odd = id % 2;
	int4 histogramPos = int4(xpos, 4, 0, 0);
	float barHeight = (tex2Dfetch(sHistogram, histogramPos).r * 255);
	float y = odd ? ((barHeight * 2 * SAMPLEDISTANCE_SQUARED)) / (256 * pow(10, BARMULTIPLIER)) - 1 : -1;
	float x = float((xpos+0.5) / 256);
	pos = float4(x * 2 - 1, y, 0, 1);
}

void GreenGraphVS(in uint id : SV_VertexID, out float4 pos : SV_Position)
{
	uint xpos = (id/2) % 256;
	bool odd = id % 2;
	int4 histogramPos = int4(xpos, 7, 0, 0);
	float barHeight = (tex2Dfetch(sHistogram, histogramPos).r * 255);
	float y = odd ? ((barHeight * 2 * SAMPLEDISTANCE_SQUARED)) / (256 * pow(10, BARMULTIPLIER)) - 1 : -1;
	float x = float((xpos+0.5) / 256);
	pos = float4(x * 2 - 1, y, 0, 1);
}

void BlueGraphVS(in uint id : SV_VertexID, out float4 pos : SV_Position)
{
	uint xpos = (id/2) % 256;
	bool odd = id % 2;
	int4 histogramPos = int4(xpos, 10, 0, 0);
	float barHeight = (tex2Dfetch(sHistogram, histogramPos).r * 255);
	float y = odd ? ((barHeight * 2 * SAMPLEDISTANCE_SQUARED)) / (256 * pow(10, BARMULTIPLIER)) - 1 : -1;
	float x = float((xpos+0.5) / 256);
	pos = float4(x * 2 - 1, y, 0, 1);
}

void DepthGraphVS(in uint id : SV_VertexID, out float4 pos : SV_Position)
{
	uint xpos = (id/2) % 256;
	bool odd = id % 2;
	int4 histogramPos = int4(xpos, 13, 0, 0);
	float barHeight = (tex2Dfetch(sHistogram, histogramPos).r * 255);
	float y = odd ? ((barHeight * 2 * SAMPLEDISTANCE_SQUARED)) / (256 * pow(10, BARMULTIPLIER)) - 1 : -1;
	float x = float((xpos+0.5) / 256);
	pos = float4(x * 2 - 1, y, 0, 1);
}

void HistogramPS(float4 pos : SV_POSITION, out float col : SV_TARGET)
{
	col = 1;
}


technique Histogram <ui_tooltip = "Press PgUp to turn on rendering to graphs";>
{
	pass
	{
		PixelShader = HistogramPS;
		VertexShader = HistogramVS;
		PrimitiveTopology = POINTLIST;
		VertexCount = ITERATIONS * SAMPLECOUNT;
		RenderTarget0 = Histogram;
		ClearRenderTargets = true; 
		BlendEnable = true; 
		SrcBlend = ONE; 
		DestBlend = ONE;
		BlendOp = ADD;
	}
}

//This is seperated from the rest of the code, as graph visuals aren't useful for much more than debugging,
//and do cost a noticeable amount of resources to generate
technique HistogramGraphs <hidden = true; toggle = 0x21;>
{
	pass Luma
	{
		PixelShader = HistogramPS;
		VertexShader = LumaGraphVS;
		PrimitiveTopology = LINELIST;
		VertexCount = 512;
		RenderTarget0 = LumaGraph;
		ClearRenderTargets = true; 
		BlendEnable = true; 
		SrcBlend = ONE; 
		DestBlend = ONE;
	}
	
	pass Red
	{
		PixelShader = HistogramPS;
		VertexShader = RedGraphVS;
		PrimitiveTopology = LINELIST;
		VertexCount = 512;
		RenderTarget0 = RedGraph;
		ClearRenderTargets = true; 
		BlendEnable = true; 
		SrcBlend = ONE; 
		DestBlend = ONE;
	}
	
	pass Green
	{
		PixelShader = HistogramPS;
		VertexShader = GreenGraphVS;
		PrimitiveTopology = LINELIST;
		VertexCount = 512;
		RenderTarget0 = GreenGraph;
		ClearRenderTargets = true; 
		BlendEnable = true; 
		SrcBlend = ONE; 
		DestBlend = ONE;
	}
	
	pass Blue
	{
		PixelShader = HistogramPS;
		VertexShader = BlueGraphVS;
		PrimitiveTopology = LINELIST;
		VertexCount = 512;
		RenderTarget0 = BlueGraph;
		ClearRenderTargets = true; 
		BlendEnable = true; 
		SrcBlend = ONE; 
		DestBlend = ONE;
	}
	
	pass Depth
	{
		PixelShader = HistogramPS;
		VertexShader = DepthGraphVS;
		PrimitiveTopology = LINELIST;
		VertexCount = 512;
		RenderTarget0 = DepthGraph;
		ClearRenderTargets = true; 
		BlendEnable = true; 
		SrcBlend = ONE; 
		DestBlend = ONE;
	}
	
}
