/*
ContrastStretch
By: Lord Of Lunacy

A histogram based contrast stretching shader that adaptively adjusts the contrast of the image
based on its contents.

Srinivasan, S & Balram, Nikhil. (2006). Adaptive contrast enhancement using local region stretching.
http://citeseerx.ist.psu.edu/viewdoc/download?doi=10.1.1.526.5037&rep=rep1&type=pdf
*/

#if (((__RENDERER__ >= 0xb000 && __RENDERER__ < 0x10000) || (__RENDERER__ >= 0x14300)) && __RESHADE__ >=40800)
	#define CONTRAST_COMPUTE 0 //compute is disabled by default due to the vertex shader being faster
#else
	#define CONTRAST_COMPUTE 0
#endif

#ifndef CONTRAST_LUT_SIZE
	#define CONTRAST_LUT_SIZE 1
#endif

#define DIVIDE_ROUNDING_UP(numerator, denominator) (int(numerator + denominator - 1) / int(denominator))

#define TILES_SAMPLES_PER_THREAD uint2(2, 2)
#define COLUMN_SAMPLES_PER_THREAD 4
#define LOCAL_ARRAY_SIZE (TILES_SAMPLES_PER_THREAD.x * TILES_SAMPLES_PER_THREAD.y)

#if CONTRAST_LUT_SIZE == 3
	#define BIN_COUNT 4096
#elif CONTRAST_LUT_SIZE == 2
	#define BIN_COUNT 2048
#elif CONTRAST_LUT_SIZE == 1
	#define BIN_COUNT 1024
#else
	#define BIN_COUNT 256
#endif

#define GROUP_SIZE uint2(32, 32)
#if CONTRAST_COMPUTE != 0
	#undef RESOLUTION_MULTIPLIER
	#define RESOLUTION_MULTIPLIER 1
#else
	#undef RESOLUTION_MULTIPLIER
	#define RESOLUTION_MULTIPLIER 4
#endif
#define DISPATCH_SIZE uint2(DIVIDE_ROUNDING_UP(BUFFER_WIDTH, TILES_SAMPLES_PER_THREAD.x * GROUP_SIZE.x * RESOLUTION_MULTIPLIER), \
					  DIVIDE_ROUNDING_UP(BUFFER_HEIGHT, TILES_SAMPLES_PER_THREAD.y * GROUP_SIZE.y * RESOLUTION_MULTIPLIER))
					  
					  

#define SAMPLE_DIMENSIONS uint2((BUFFER_WIDTH / RESOLUTION_MULTIPLIER), \
							    (BUFFER_HEIGHT / RESOLUTION_MULTIPLIER))
					  
#define VERTEX_COUNT uint(SAMPLE_DIMENSIONS.x * SAMPLE_DIMENSIONS.y)
#define SAMPLES_PER_HISTOGRAM 128
#define ROW_COUNT uint(DIVIDE_ROUNDING_UP(VERTEX_COUNT, SAMPLES_PER_HISTOGRAM))




namespace Contrast 
{
	texture BackBuffer : COLOR;
	texture Histogram {Width = BIN_COUNT; Height = 1; Format = R32f;};
	texture PrefixSums {Width = BIN_COUNT; Height = 1; Format = RG32f;};

	sampler sBackBuffer {Texture = BackBuffer;};
	sampler sHistogram {Texture = Histogram;};
	sampler sPrefixSums {Texture = PrefixSums;};

	
#if CONTRAST_COMPUTE == 0
	texture HistogramRows {Width = BIN_COUNT; Height = ROW_COUNT; Format = R8;};
	
	sampler sHistogramRows {Texture = HistogramRows;};
#endif

	texture Variances {Width = 3; Height = 1; Format = R32f;};
	texture HistogramLUT {Width = BIN_COUNT; Height = 1; Format = R32f;};
	
	sampler sVariances {Texture = Variances;};
	sampler sHistogramLUT {Texture = HistogramLUT;};
	
#if CONTRAST_COMPUTE != 0
	texture LocalTiles {Width = BIN_COUNT; Height = DISPATCH_SIZE.x * DISPATCH_SIZE.y; Format = R32f;};
	
	sampler sLocalTiles {Texture = LocalTiles;};
	
	storage wLocalTiles {Texture = LocalTiles;};
	storage wHistogram {Texture = Histogram;};
	storage wHistogramLUT {Texture = HistogramLUT;};
#endif

	uniform float Minimum<
		ui_type = "slider";
		ui_label = "Minimum";
		ui_category = "Thresholds";
		ui_min = 0; ui_max = 1;
	> = 0;

	uniform float DarkThreshold<
		ui_type = "slider";
		ui_label = "Dark Threshold";
		ui_category = "Thresholds";
		ui_min = 0; ui_max = 1;
	> = 0.333;

	uniform float LightThreshold<
		ui_type = "slider";
		ui_label = "LightThreshold";
		ui_category = "Thresholds";
		ui_min = 0; ui_max = 1;
	> = 0.667;

	uniform float Maximum<
		ui_type = "slider";
		ui_label = "Maximum";
		ui_category = "Thresholds";
		ui_min = 0; ui_max = 1;
	> = 1;
	
	uniform float MaxVariance<
		ui_type = "slider";
		ui_label = "Maximum Curve Value";
		ui_tooltip = "Value that the weighting curves return to 0 at.";
		ui_category = "Maximum Curve Value";
		ui_min = 0.15; ui_max = 1;
	> = 1;

	uniform float DarkPeak<
		ui_type = "slider";
		ui_label = "Dark Blending Curve";
		ui_category = "Dark Settings";
		ui_min = 0; ui_max = 1;
	> = 0.5;
	
	uniform float DarkMax<
		ui_type = "slider";
		ui_label = "Dark Maximum Blending";
		ui_category = "Dark Settings";
		ui_min = 0; ui_max = 1;
	> = 0.45;

	uniform float MidPeak<
		ui_type = "slider";
		ui_label = "Mid Blending Curve";
		ui_category = "Mid Settings";
		ui_min = 0; ui_max = 1;
	> = 0.55;
	
	uniform float MidMax<
		ui_type = "slider";
		ui_label = "Mid Maximum Blending";
		ui_category = "Mid Settings";
		ui_min = 0; ui_max = 1;
	> = 0.45;

	uniform float LightPeak<
		ui_type = "slider";
		ui_label = "Light Blending Curve";
		ui_category = "Light Settings";
		ui_min = 0; ui_max = 1;
	> = 0.45;
	
	uniform float LightMax<
		ui_type = "slider";
		ui_label = "Light Maximum Blending";
		ui_category = "Light Settings";
		ui_min = 0; ui_max = 1;
	> = 0.3;
	
	uniform uint Debug<
		ui_type = "combo";
		ui_label = "Debug";
		ui_category = "Debug Views";
		ui_items = "None \0Histogram \0Dark Curve Input \0Mid Curve Input \0Light Curve Input \0";
	> = 0;
	
	float WeightingCurve(float peak, float variance, float maximumBlending)
	{
		variance = clamp(variance, 0, MaxVariance);
		if(variance <= peak)
		{
			return lerp(0, maximumBlending, abs(variance) / peak);
		}
		else
		{
			return lerp(maximumBlending, 0, abs(variance - peak) / (MaxVariance - peak));
		}
	}

#if CONTRAST_COMPUTE != 0
	groupshared uint HistogramBins[BIN_COUNT];
	void HistogramTilesCS(uint3 id : SV_DispatchThreadID, uint3 gid : SV_GroupID, uint3 gtid : SV_GroupThreadID)
	{
		uint threadIndex = gtid.x + gtid.y * GROUP_SIZE.x;
		uint groupIndex = gid.x + gid.y * DISPATCH_SIZE.x;
		
		uint bin = threadIndex;
		[unroll]
		while(bin < BIN_COUNT)
		{
			HistogramBins[bin] = 0;
			bin += GROUP_SIZE.x * GROUP_SIZE.y;
		}
		barrier();
		
		
		uint localValue[LOCAL_ARRAY_SIZE];
		[unroll]
		for(int i = 0; i < TILES_SAMPLES_PER_THREAD.x; i++)
		{
			[unroll]
			for(int j = 0; j < TILES_SAMPLES_PER_THREAD.y; j++)
			{
				uint2 coord = (id.xy * TILES_SAMPLES_PER_THREAD + float2(i, j)) * RESOLUTION_MULTIPLIER;
				uint arrayIndex = i + TILES_SAMPLES_PER_THREAD.x * j;
				if(any(coord >= uint2(BUFFER_WIDTH, BUFFER_HEIGHT)))
				{
					localValue[arrayIndex] = BIN_COUNT;
				}
				else
				{
					localValue[arrayIndex] = uint(round(dot(tex2Dfetch(sBackBuffer, float2(coord)).rgb, float3(0.299, 0.587, 0.114)) * (BIN_COUNT - 1)));
				}
			}
		}
		
		
		[unroll]
		for(int i = 0; i < LOCAL_ARRAY_SIZE; i++)
		{
			if(localValue[i] < BIN_COUNT)
			{
				atomicAdd(HistogramBins[localValue[i]], 1);
			}
		}
		barrier();
		
		bin = threadIndex;
		[unroll]
		while(bin < BIN_COUNT)
		{
			tex2Dstore(wLocalTiles, int2(bin, groupIndex), float4(HistogramBins[bin], 1, 1, 1));
			bin += GROUP_SIZE.x * GROUP_SIZE.y;
		}
	}

	groupshared uint columnSum;
	void ColumnSumCS(uint3 id : SV_DispatchThreadID)
	{
		if(id.y == 0)
		{
			columnSum = 0;
		}
		barrier();
		uint localSum = 0;
		[unroll]
		for(int i = 0; i < COLUMN_SAMPLES_PER_THREAD * 2; i += 2)
		{
			localSum += tex2Dfetch(sLocalTiles, int2(id.x, id.y * COLUMN_SAMPLES_PER_THREAD + i + 0.5)).x * 2;
		}
		
		atomicAdd(columnSum, localSum);
		barrier();
		
		if(id.y == 0)
		{
			tex2Dstore(wHistogram, id.xy, columnSum);
		}
	}
#endif
	
	// Vertex shader generating a triangle covering the entire screen
	void PostProcessVS(in uint id : SV_VertexID, out float4 position : SV_Position, out float2 texcoord : TEXCOORD)
	{
		texcoord.x = (id == 2) ? 2.0 : 0.0;
		texcoord.y = (id == 1) ? 2.0 : 0.0;
		position = float4(texcoord * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
	}

#if CONTRAST_COMPUTE == 0
	void HistogramVS(in uint id : SV_VertexID, out float4 pos : SV_Position)
	{
		float2 coord = floor(float2(id % SAMPLE_DIMENSIONS.x, id / SAMPLE_DIMENSIONS.x)) * RESOLUTION_MULTIPLIER + 0.5;
		float luma = (round(dot(tex2Dfetch(sBackBuffer, coord).rgb, float3(0.299, 0.587, 0.114)) * (BIN_COUNT - 1)) + 0.5) / float(BIN_COUNT);
		
		pos.x = luma * 2 - 1;
		pos.y = (floor(id / SAMPLES_PER_HISTOGRAM) + 0.5) / float(ROW_COUNT) * 2 - 1;
		pos.z = 0;
		pos.w = 1;
	}
	
	void HistogramMergeVS(in uint id : SV_VertexID, out float4 pos : SV_Position, out float2 sum : TEXCOORD)
	{
		float2 coord = floor(float2(id % BIN_COUNT, id / BIN_COUNT));
		coord.y = coord.y * 8 + 0.5;
		sum = 0;
		
		[unroll]
		for(int i = 0; i < 8; i += 2)
		{
			sum += tex2Dfetch(sHistogramRows, coord).xx * 512;
		}
		if(sum.x < 1)
		{
			pos.x = 2;
		}
		else
		{
			pos.x = ((coord.x + 0.5) / float(BIN_COUNT)) * 2 - 1;
		}
		pos.yz = 0;
		pos.w = 1;
	}
#endif
	
	void PrefixSumsVS(in uint id : SV_VertexID, out float4 pos : SV_Position, out float2 prefixes : TEXCOORD)
	{
		uint bin = id / 2;
		float2 coord;
		coord.x = (id % 2 == 0) ? bin : (BIN_COUNT);
		coord.y = 0;
		pos.x = ((coord.x + 0.5) / float(BIN_COUNT)) * 2 - 1;
		pos.yz = 0;
		pos.w = 1;
		prefixes = tex2Dfetch(sHistogram, float2(bin, 0)).xx;
		prefixes.y *= bin;
	}
	
	void VariancesVS(in uint id : SV_VertexID, out float4 pos : SV_Position, out float2 partialVariance : TEXCOORD)
	{
		uint bin = id;
		pos.yz = 0;
		pos.w = 1;
		uint2 thresholds = round(float2(DarkThreshold, LightThreshold) * (BIN_COUNT - 1));
		if(bin <= thresholds.x)
		{
			float2 sums = tex2Dfetch(sPrefixSums, float2(thresholds.x, 0)).xy;
			float mean = sums.y / sums.x;
			float localSum = tex2Dfetch(sHistogram, float2(bin, 0)).x;
			partialVariance = ((abs(bin - mean)) / float(thresholds.x - 1)) * (localSum / sums.x);
			pos.x = -0.66666666666;
		}
		else if(bin <= thresholds.y)
		{
			float2 sums = tex2Dfetch(sPrefixSums, float2(thresholds.y, 0)).xy;
			float2 previousSums = tex2Dfetch(sPrefixSums, float2(thresholds.x, 0)).xy;
			sums -= previousSums;
			float mean = sums.y / sums.x;
			float localSum = tex2Dfetch(sHistogram, float2(bin, 0)).x;
			partialVariance = ((abs(float(bin) - mean)) / float(thresholds.y - thresholds.x)) * (localSum / sums.x);
			pos.x = 0;
		}
		else
		{
			float2 sums = tex2Dfetch(sPrefixSums, float2(BIN_COUNT - 1, 0)).xy;
			float2 previousSums = tex2Dfetch(sPrefixSums, float2(thresholds.y, 0)).xy;
			sums -= previousSums;
			float mean = sums.y / sums.x;
			float localSum = tex2Dfetch(sHistogram, float2(bin, 0)).x;
			partialVariance = ((abs(float(bin) - mean)) / float(BIN_COUNT - thresholds.y)) * (localSum / sums.x);
			pos.x = 0.66666666666;
		}
			
	}
	void HistogramPS(float4 pos : SV_Position, out float add : SV_Target0)
	{
		add = 0.00390625; // (1 / 256)
	}
	
	void PrefixSumsPS(float4 pos : SV_Position, float2 prefixes : TEXCOORD, out float2 output : SV_Target0)
	{
		output = prefixes;
	}
	
	void TexcoordXPS(float4 pos : SV_Position, float2 texcoord : TEXCOORD, out float output : SV_Target0)
	{
		output = texcoord.x;
	}
	
	void LUTGenerationPS(float4 pos : SV_Position, float2 texcoord : TEXCOORD, out float LUT : SV_Target0)
	{
		float2 coord = float2(round(texcoord.x * float(BIN_COUNT - 1)), 0);
		float prefixSum = tex2Dfetch(sPrefixSums, coord).x;
		uint2 thresholds = round(float2(DarkThreshold, LightThreshold) * (BIN_COUNT - 1));
		if(coord.x <= thresholds.x)
		{
			if(prefixSum > 0)
			{
				LUT = (prefixSum / tex2Dfetch(sPrefixSums, float2(thresholds.x, 0)).x);
				LUT = LUT * (DarkThreshold - Minimum) + Minimum;
				float alpha = WeightingCurve(DarkPeak, tex2Dfetch(sVariances, float2(0, 0)).x, DarkMax);
				LUT = lerp(texcoord.x, LUT, alpha);
			}
			else
			{
				LUT = texcoord.x * (DarkThreshold - Minimum) + Minimum;
			}
		}
		else if(coord.x <= thresholds.y)
		{
			float previousSum = tex2Dfetch(sPrefixSums, float2(thresholds.x, 0)).x;
			prefixSum -= previousSum;
			float denominator = tex2Dfetch(sPrefixSums, float2(thresholds.y, 0)).x - previousSum;
			if(denominator < 1)
			{
				LUT = texcoord.x * (LightThreshold - DarkThreshold) + DarkThreshold;
			}
			else
			{
				LUT = prefixSum / denominator;
				LUT = LUT * (LightThreshold - DarkThreshold) + DarkThreshold;
				float alpha = WeightingCurve(MidPeak, tex2Dfetch(sVariances, float2(1, 0)).x, MidMax);
				LUT = lerp(texcoord.x, LUT, alpha);
			}
		}
		else
		{
			float previousSum = tex2Dfetch(sPrefixSums, float2(thresholds.y, 0)).x;
			prefixSum -= previousSum;
			float denominator = tex2Dfetch(sPrefixSums, float2(BIN_COUNT - 1, 0)).x - previousSum;
			if(denominator < 1)
			{
				LUT = texcoord.x * (Maximum - LightThreshold) + LightThreshold;
			}
			else
			{
				LUT = prefixSum / denominator;
				LUT = LUT * (Maximum - LightThreshold) + LightThreshold;
				float alpha = WeightingCurve(LightPeak, tex2Dfetch(sVariances, float2(2, 0)).x, LightMax);
				LUT = lerp(texcoord.x, LUT, alpha);
			}
		}
	}
	
	void OutputPS(float4 pos : SV_Position, float2 texcoord : TEXCOORD, out float3 color : SV_Target0)
	{
		color = tex2D(sBackBuffer, texcoord).rgb;
		float yOld = dot(color, float3(0.299, 0.587, 0.114));
		float y = tex2D(sHistogramLUT, float2(yOld, 0.5)).x;
		
		float cb = -0.168736 * color.r - 0.331264 * color.g + 0.500000 * color.b;
		float cr = +0.500000 * color.r - 0.418688 * color.g - 0.081312 * color.b;
		
		color = float3(
				y + 1.402 * cr,
				y - 0.344136 * cb - 0.714136 * cr,
				y + 1.772 * cb);
				
		if(Debug == 1)
		{
			texcoord = float2(3 * texcoord.x - 2, 1 - texcoord.y);
			uint2 coord = uint2(round(texcoord * float2((BIN_COUNT - 1), BUFFER_HEIGHT * (65536 / (BIN_COUNT * RESOLUTION_MULTIPLIER * 0.5 * RESOLUTION_MULTIPLIER)))));
			if(texcoord.x >= 0)
			{
				uint histogramBin = tex2D(sHistogram, float2(texcoord.x, 0.5)).x;
				if(coord.y <= histogramBin)
				{
					color = lerp(color, 1 - color, 0.7);
				}
			}
		}
		else if(Debug == 2)
		{
			texcoord = float2(1 - texcoord.x, texcoord.y * (float(BUFFER_HEIGHT) / float(BUFFER_WIDTH)));
			if(all(texcoord <= 0.125))
			{
				color = tex2Dfetch(sVariances, float2(0, 0)).xxx;
			}
		}
		else if(Debug == 3)
		{
			texcoord = float2(1 - texcoord.x, texcoord.y * (float(BUFFER_HEIGHT) / float(BUFFER_WIDTH)));
			if(all(texcoord <= 0.125))
			{
				color = tex2Dfetch(sVariances, float2(1, 0)).xxx;
			}
		}
		else if(Debug == 4)
		{
			texcoord = float2(1 - texcoord.x, texcoord.y * (float(BUFFER_HEIGHT) / float(BUFFER_WIDTH)));
			if(all(texcoord <= 0.125))
			{
				color = tex2Dfetch(sVariances, float2(2, 0)).xxx;
			}
		}
	}

	technique ContrastStretch<ui_tooltip = "A histogram based contrast stretching shader that adaptively adjusts the contrast of the image\n"
										   "based on its contents.\n\n"
										   "Part of Insane Shaders \n"
										   "By: Lord Of Lunacy\n\n"
										   "CONTRAST_LUT_SIZES correspond as follows: \n"
										   "0 for 256 \n1 for 1024 (default) \n2 for 2048 \n3 for 4096";>
	{
#if CONTRAST_COMPUTE != 0
		pass
		{
			ComputeShader = HistogramTilesCS<GROUP_SIZE.x, GROUP_SIZE.y>;
			DispatchSizeX = DISPATCH_SIZE.x;
			DispatchSizeY = DISPATCH_SIZE.y;
		}
		
		pass
		{
			ComputeShader = ColumnSumCS<1, DIVIDE_ROUNDING_UP(DISPATCH_SIZE.x * DISPATCH_SIZE.y, COLUMN_SAMPLES_PER_THREAD * 2)>;
			DispatchSizeX = BIN_COUNT;
			DispatchSizeY = 1;
		}
#else
		pass Histogram
		{
			VertexShader = HistogramVS;
			PixelShader = HistogramPS;
			RenderTarget0 = HistogramRows;
			PrimitiveTopology = POINTLIST;
			VertexCount = VERTEX_COUNT;
			ClearRenderTargets = true;
			BlendEnable = true;
			BlendOp = ADD;
			SrcBlend = ONE;
			DestBlend = ONE;
		}
		
		pass HistogramMerge
		{
			VertexShader = HistogramMergeVS;
			PixelShader = TexcoordXPS;
			RenderTarget0 = Histogram;
			PrimitiveTopology = POINTLIST;
			VertexCount = DIVIDE_ROUNDING_UP(BIN_COUNT * ROW_COUNT, 8);
			ClearRenderTargets = true;
			BlendEnable = true;
			BlendOp = ADD;
			SrcBlend = ONE;
			DestBlend = ONE;
		}
#endif
		
		pass PrefixSums
		{
			VertexShader = PrefixSumsVS;
			PixelShader = PrefixSumsPS;
			RenderTarget0 = PrefixSums;
			PrimitiveTopology = LINELIST;
			VertexCount = BIN_COUNT * 2;
			ClearRenderTargets = true;
			BlendEnable = true;
			BlendOp = ADD;
			SrcBlend = ONE;
			DestBlend = ONE;
		}
		
		pass Variances
		{
			VertexShader = VariancesVS;
			PixelShader = TexcoordXPS;
			RenderTarget0 = Variances;
			PrimitiveTopology = POINTLIST;
			VertexCount = BIN_COUNT;
			ClearRenderTargets = true;
			BlendEnable = true;
			BlendOp = ADD;
			SrcBlend = ONE;
			DestBlend = ONE;
		}
		
		pass LUTGeneration
		{
			VertexShader = PostProcessVS;
			PixelShader = LUTGenerationPS;
			RenderTarget0 = HistogramLUT;
		}
		
		pass Output
		{
			VertexShader = PostProcessVS;
			PixelShader = OutputPS;
		}
	}
}
