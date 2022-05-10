/*
	Histogram made using compute shaders
	By: Lord of Lunacy
*/


#define DIVIDE_ROUNDING_UP(n, d) uint(((n) + (d) - 1) / (d))

#define DISPATCH_X DIVIDE_ROUNDING_UP(BUFFER_WIDTH, 64)
#define DISPATCH_Y DIVIDE_ROUNDING_UP(BUFFER_HEIGHT, 64)

#define TILE_COUNT (DISPATCH_X * DISPATCH_Y)

uniform bool Show_Red = true;
uniform bool Show_Green = true;
uniform bool Show_Blue = true;
uniform bool Show_Luma = true;

namespace Histogram_Testing_999
{
	texture BackBuffer : COLOR;
	texture HistogramTiles {Width = TILE_COUNT; Height = 256; Format = RGBA16;};
	texture Histogram {Width = 256; Height = 1; Format = RGBA32f;};

	sampler sBackBuffer {Texture = BackBuffer;};
	sampler sHistogramTiles {Texture = HistogramTiles;};
	sampler sHistogram {Texture = Histogram;};
	
	storage wHistogramTiles {Texture = HistogramTiles;};
	storage wHistogram {Texture = Histogram;};
	
	groupshared uint bins[4096];
	void HistogramTilesCS(uint3 id : SV_DispatchThreadID, uint3 tid : SV_GroupThreadID)
	{
		uint threadIndex = tid.x + tid.y * 32;
		[unroll]
		for(int i = 0; i < 4; i++)
		{
			bins[1024 * i + threadIndex] = 0;
		}
		barrier(); //initialize all bins to 0
		
		uint2 groupWarp = tid.xy / 8;
		uint groupWarpIndex = groupWarp.x + groupWarp.y * 4;
		uint2 warpCoord = tid.xy % 8;
		uint binsOffset = groupWarpIndex * 256;
		float2 texcoord = float2(id.xy) * float2(BUFFER_RCP_WIDTH, BUFFER_RCP_HEIGHT) * 2 + float2(BUFFER_RCP_WIDTH, BUFFER_RCP_HEIGHT);
		//Prevent regions from outside the texture from contributing and prevent overflow by preventing the bottom corner texel of each warp from contributing
		if(all(texcoord < 1) && any(warpCoord != 7))
		{
			float4 red = tex2DgatherR(sBackBuffer, texcoord);
			float4 green = tex2DgatherG(sBackBuffer, texcoord);
			float4 blue = tex2DgatherB(sBackBuffer, texcoord);
			float4 luma;
			luma.x = dot(float3(red.x, green.x, blue.x), float3(0.299, 0.587, 0.114));
			luma.y = dot(float3(red.y, green.y, blue.y), float3(0.299, 0.587, 0.114));
			luma.z = dot(float3(red.z, green.z, blue.z), float3(0.299, 0.587, 0.114));
			luma.w = dot(float3(red.w, green.w, blue.w), float3(0.299, 0.587, 0.114));
			uint4 red_uint = uint4(red * 255.5);
			uint4 green_uint = uint4(green * 255.5);
			uint4 blue_uint = uint4(blue * 255.5);
			uint4 luma_uint = uint4(luma * 255.5);
			
			[unroll]
			for(int i = 0; i < 4; i++)
			{
				atomicAdd(bins[binsOffset + red_uint[i]], 1);
				atomicAdd(bins[binsOffset + green_uint[i]], (1 << 8));
				atomicAdd(bins[binsOffset + blue_uint[i]], (1 << 16));
				atomicAdd(bins[binsOffset + luma_uint[i]], (1 << 24));
			}
		}
		
		barrier(); //this needs to be completed across the entire group before merging to a single histogram and writing
		
		if(all(tid.xy < 16)) //lets go of any warps no longer needed so they can start on the next tile
		{
			threadIndex = tid.x + tid.y * 16;
			uint4 sum = 0;
			//avoids the extra barrier and other complexities that would be involved if atomics were used
			[unroll]
			for(int i = 0; i < 16; i++)
			{
				uint sampleBin = bins[i * 256 + threadIndex]; 
				sum += uint4(sampleBin & 0xFF, (sampleBin >> 8) & 0xFF, (sampleBin >> 16) & 0xFF, (sampleBin >> 24) & 0xFF);
			}
			uint dispatchIndex = id.x / 32 + (id.y / 32) * DISPATCH_X;
			tex2Dstore(wHistogramTiles, uint2(dispatchIndex, threadIndex), (float4(sum) / 4096));
		}
		
	}

	groupshared uint4 mergedBin;
	void TileMergeCS(uint3 id : SV_DispatchThreadID, uint3 tid : SV_GroupThreadID)
	{
		if(all(tid.xy == 0))
		{
			mergedBin = 0;
		}
		barrier();
		float2 coord = float2(id.x * 4, id.y) + 0.5;
		uint4 bins = 0;
		[unroll]
		for(int i = 0; i < 4; i++)
		{
			bins += uint4(tex2Dfetch(sHistogramTiles, coord + float2(i, 0)) * 4096);
		}
		//Could probably be made a little bit faster using a parallel sum instead, 
		//however the setup overhead will be much higher and could also just cancel out
		atomicAdd(mergedBin[0], bins[0]);
		atomicAdd(mergedBin[1], bins[1]);
		atomicAdd(mergedBin[2], bins[2]);
		atomicAdd(mergedBin[3], bins[3]);
		barrier();
		if(all(tid.xy == 0))
		{
			tex2Dstore(wHistogram, uint2(id.y, 0), mergedBin);
		}
	}
	
	// Vertex shader generating a triangle covering the entire screen
	void PostProcessVS(in uint id : SV_VertexID, out float4 position : SV_Position, out float2 texcoord : TEXCOORD)
	{
		texcoord.x = (id == 2) ? 2.0 : 0.0;
		texcoord.y = (id == 1) ? 2.0 : 0.0;
		position = float4(texcoord * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
	}
	
	//Slow and just thrown together from code I had lying around, but also not neccessary considering it just generates a debug view
	void OutputPS(float4 pos : SV_Position, float2 texcoord : TEXCOORD, out float4 color : SV_Target0)
	{
			float2 originalTexcoord = texcoord;
			color = tex2D(sBackBuffer, originalTexcoord);
			texcoord = float2(3 * texcoord.x - 2, 1 - texcoord.y);
			uint2 coord = uint2(round(texcoord * float2((255), BUFFER_HEIGHT * (512))));
			if(texcoord.x >= 0)
			{
				
				uint4 histogramBin = tex2D(sHistogram, float2(texcoord.x, 0.5)).rgba;
				if(coord.y <= histogramBin.r && Show_Red)
				{
					color = float4(1, 0, 0, 1);
				}
				else if(coord.y <= histogramBin.g && Show_Green)
				{
					color = float4(0, 1, 0, 1);
				}
				else if(coord.y <= histogramBin.b && Show_Blue)
				{
					color = float4(0, 0, 1, 1);
				}
				else if(coord.y <= histogramBin.a && Show_Luma)
				{
					color = float4(1, 1, 1, 1);
				}
			}
	}

	technique HistogramCS
	{
		pass
		{
			ComputeShader = HistogramTilesCS<32, 32>;
			DispatchSizeX = DISPATCH_X;
			DispatchSizeY = DISPATCH_Y;
		}
		
		pass
		{
			ComputeShader = TileMergeCS<DIVIDE_ROUNDING_UP(TILE_COUNT, 4), 1>;
			DispatchSizeX = 1;
			DispatchSizeY = 256;
		}
		
	}
	technique Show_Histogram
	{
		pass
		{
			VertexShader = PostProcessVS;
			PixelShader = OutputPS;
		}
	}
}
