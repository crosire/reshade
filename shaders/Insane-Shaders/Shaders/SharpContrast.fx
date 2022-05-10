/*
	This sharpen filter uses a kuwahara filter to create an unsharp mask, and was originally developed from the anisotropic
	kuwahara technique that was used by my Oilify shader. Changes to this Kuwahara include, the removal of the anisotropy
	and the porting of it to a compute for the purposes of optimization and improved visuals.
	
	There is also an edge mask being implemented that uses a weighted minimum and maximum from the entirety of the sample
	patch to attempt to determine the size of halos that need to be masked.
	
	Kuwahara filter. (2020, May 01). Retrieved October 17, 2020, from https://en.wikipedia.org/wiki/Kuwahara_filter
	
	Kyprianidis, J. E., Kang, H., &amp; Dã¶Llner, J. (2009). Image and Video Abstraction by Anisotropic Kuwahara Filtering.
	Computer Graphics Forum, 28(7), 1955-1963. doi:10.1111/j.1467-8659.2009.01574.x
*/

#include "ReShade.fxh"

#define DIVIDE_ROUNDING_UP(n, d) uint(((n) + (d) - 1) / (d))
#ifndef SHARP_CONTRAST_SIZE
	#define SHARP_CONTRAST_SIZE 7
#endif


#undef SECTOR_COUNT

#if SHARP_CONTRAST_SIZE <= 2
	#undef SHARP_CONTRAST_SIZE
	#define SHARP_CONTRAST_SIZE 2
	#define SECTOR_COUNT 2
#elif SHARP_CONTRAST_SIZE == 3
	#define SECTOR_COUNT 3
#elif SHARP_CONTRAST_SIZE == 4
	#define SECTOR_COUNT 3
#elif SHARP_CONTRAST_SIZE == 5
	#define SECTOR_COUNT 4
#elif SHARP_CONTRAST_SIZE == 6
	#define SECTOR_COUNT 6
#elif SHARP_CONTRAST_SIZE == 7
	#define SECTOR_COUNT 7
#elif SHARP_CONTRAST_SIZE == 8
	#define SECTOR_COUNT 8
#elif SHARP_CONTRAST_SIZE == 9
	#define SECTOR_COUNT 9
#elif SHARP_CONTRAST_SIZE == 10
	#define SECTOR_COUNT 13
#elif SHARP_CONTRAST_SIZE >= 11
	#undef SHARP_CONTRAST_SIZE
	#define SHARP_CONTRAST_SIZE 11
	#define SECTOR_COUNT 15
#endif



#if (((__RENDERER__ >= 0xb000 && __RENDERER__ < 0x10000) || (__RENDERER__ >= 0x14300)) && __RESHADE__ >=40800)

	#ifndef SHARP_CONTRAST_COMPUTE
		#define SHARP_CONTRAST_COMPUTE 1
	#endif
#else
	#define SHARP_CONTRAST_COMPUTE 0
#endif

#define VULKAN 0x20000
#define NVIDIA 0x10DE


	
static const float PI = 3.1415926536;
static const float GAUSSIAN_WEIGHTS[5] = {0.095766,	0.303053,	0.20236,	0.303053,	0.095766};
static const float GAUSSIAN_OFFSETS[5] = {-3.2979345488, -1.40919905099, 0, 1.40919905099, 3.2979345488};
	

namespace SharpContrast
{
	texture BackBuffer : COLOR;
	texture Luma<pooled = true;>{Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = R8;};
	texture Temp<pooled = true;>{Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = R8;};
	

	sampler sBackBuffer{Texture = BackBuffer;};
	sampler sLuma {Texture = Luma;};
	sampler sTemp {Texture = Temp;};

	storage wLuma {Texture = Luma;};
	storage wTemp {Texture = Temp;};
	
	uniform float SharpenStrength<
		ui_type = "slider";
		ui_label = "Strength";
		ui_tooltip = "Strength of the sharpen.";
		ui_min = 0; ui_max = 2;
		ui_step = 0.001;
	> = 0.667;

	uniform float EdgeBias<
		ui_type = "slider";
		ui_label = "Edge Contrast";
		ui_tooltip = "Changes how much contrast and coarseness there is near edges.";
		ui_min = 0; ui_max = 1;
		ui_step = 0.001;
	> = 1;
	
	uniform float EdgeFloor<
		ui_type = "slider";
		ui_label = "Edge Floor";
		ui_tooltip = "Amount of sharpening allowed on edges.";
		ui_min = 0; ui_max = 1;
		ui_step = 0.001;
	> = 0;	
	
	uniform bool GammaCorrect<
		ui_label = "Use Linear Color Gamut";
		ui_tooltip = "Changes whether the sharpen is applied on a linear\n"
					 "or standard color gamut.";
	> = true;
	
	uniform bool EnableDepth<
		ui_category = "Depth Settings";
		ui_label = "Enable Depth Settings";
		ui_tooltip = "This setting must be enabled to be able to adjust the Depth Curve \n"
					"or enable the Mask Sky option.";
	> = false;
	
	uniform float DepthCurve<
		ui_type = "slider";
		ui_label = "Depth Curve";
		ui_category = "Depth Settings";
		ui_tooltip = "Adjusts how the sharpen fades with depth, lower values mean a shorter fadeout distance.";
		ui_min = 0; ui_max = 1;
		ui_step = 0.001;
	> = 1;
	
	uniform bool MaskSky<
		ui_category = "Depth Settings";
		ui_label = "Mask Sky";
		ui_tooltip = "This setting prevents the sky from having the sharpen applied.";
	> = true;
	
	
	uniform int Debug<
		ui_type = "combo";
		ui_items = "None\0Masking\0Sharpen\0";
		ui_label = "Debug View";
		ui_category = "Debug";
		ui_tooltip = "Masking: Shows the regions where the sharpen is having its application limited.\n"
					 "Sharpen: Shows the sharpen that's being applied to the image.";
	> = 0;
	void LumaPS(float4 vpos : SV_POSITION, float2 texcoord : TEXCOORD, out float output : SV_TARGET0)
	{
		output = dot(tex2Dfetch(sBackBuffer, texcoord * float2(BUFFER_WIDTH, BUFFER_HEIGHT)).rgb, float3(0.299, 0.587, 0.114));
	}

#if SHARP_CONTRAST_COMPUTE
	#ifndef SHARP_CONTRAST_FASTER_COMPILE
	#define SHARP_CONTRAST_FASTER_COMPILE 1
	#endif

	#if SHARP_CONTRAST_FASTER_COMPILE == 0 && SHARP_CONTRAST_SIZE < 10
		#define SAMPLES_PER_THREAD uint2(2, 2)
	#else
		#define SAMPLES_PER_THREAD uint2(1, 1)
	#endif
	
	#define SAMPLES_PER_THREAD_COUNT (SAMPLES_PER_THREAD.x * SAMPLES_PER_THREAD.y)
	
	#if SHARP_CONTRAST_SIZE > 7
		#define GROUP_SIZE uint2(16, 16)
	#else
		#define GROUP_SIZE uint2(8, 8)
	#endif
	
	#define GROUP_COUNT (GROUP_SIZE.x * GROUP_SIZE.y)
	#define GROUP_SAMPLE_SIZE ((GROUP_SIZE * SAMPLES_PER_THREAD) + (SHARP_CONTRAST_SIZE / 2).xx * 2)
	#define GROUP_SAMPLE_COUNT (GROUP_SAMPLE_SIZE.x * GROUP_SAMPLE_SIZE.y)
	#define INTERPOLATED_SAMPLE_SIZE (GROUP_SAMPLE_SIZE - 1)
	#define INTERPOLATED_SAMPLE_COUNT (INTERPOLATED_SAMPLE_SIZE.x * INTERPOLATED_SAMPLE_SIZE.y)
	#define ARRAY_INDEX(X, Y, SAMPLE_SIZE) uint((X) + (Y) * (SAMPLE_SIZE.x))
	#define ARRAY_COORD(INDEX, SAMPLE_SIZE) uint2(INDEX % SAMPLE_SIZE.x, INDEX / SAMPLE_SIZE.x)

	
	

	groupshared float samples[GROUP_SAMPLE_COUNT];
	groupshared uint throwAway;
	void KuwaharaSharpCS(uint3 id : SV_DispatchThreadID, uint3 tid : SV_GroupThreadID)
	{
		uint2 cornerCoord = (id.xy - tid.xy) * SAMPLES_PER_THREAD;
		//uint groupIndex = ARRAY_INDEX(tid.x, tid.y, GROUP_SIZE);
		float sharpnessMultiplier = max(1023 * pow(( 2 * EdgeBias / 3) + 0.333333, 4), 1e-10);
		[unroll]
		for(int j = tid.y * 2; j < GROUP_SAMPLE_SIZE.y; j += GROUP_SIZE.y * 2)
		{
			[unroll]
			for(int i = tid.x * 2; i < GROUP_SAMPLE_SIZE.x; i += GROUP_SIZE.x * 2)
			{
				float2 sampleCoord = (cornerCoord) + int2(i, j) - (SHARP_CONTRAST_SIZE / 2).xx;
				
				sampleCoord += 0.5;
				sampleCoord = (sampleCoord < 0) ? abs(sampleCoord) : 
							  (sampleCoord > float2(BUFFER_WIDTH, BUFFER_HEIGHT)) ? float2(BUFFER_WIDTH, BUFFER_HEIGHT) - (sampleCoord - float2(BUFFER_WIDTH, BUFFER_HEIGHT)) : sampleCoord;
				
				sampleCoord /= float2(BUFFER_WIDTH, BUFFER_HEIGHT);
				//sampleCoord /= 2;
				float4 r = tex2DgatherR(sLuma, sampleCoord);
				
				//approximate gamma correction
				if(GammaCorrect)
				{
					r *= r;
				}
				
				r *= sharpnessMultiplier;

				
				int arrayIndex = ARRAY_INDEX(i, j, GROUP_SAMPLE_SIZE);
				
				samples[arrayIndex + GROUP_SAMPLE_SIZE.x] = r.x;
				samples[arrayIndex + GROUP_SAMPLE_SIZE.x + 1] = r.y;
				samples[arrayIndex + 1] = r.z;
				samples[arrayIndex] = r.w;
			}
		}
		barrier();
		
		[unroll]
		for(int k = 0; k < SAMPLES_PER_THREAD_COUNT; k++)
		{
			float sum[SECTOR_COUNT];
			float squaredSum[SECTOR_COUNT];
			float sampleCount[SECTOR_COUNT];
			float maximum = 0;
			float minimum = sharpnessMultiplier;
			float center;
			uint2 coord = tid.xy * SAMPLES_PER_THREAD;
			coord.x += k % SAMPLES_PER_THREAD.x;
			coord.y += k / SAMPLES_PER_THREAD.y;
			
			
			[unroll]
			for(int i = -(SHARP_CONTRAST_SIZE/2); i <= (SHARP_CONTRAST_SIZE/2); i++)
			{
				[unroll]
				for(int j = -(SHARP_CONTRAST_SIZE/2); j <= (SHARP_CONTRAST_SIZE/2); j++)
				{
					[flatten]
					if(all(int2(i, j) == 0))
					{
						uint arrayIndex = ARRAY_INDEX(coord.x + (SHARP_CONTRAST_SIZE/2), coord.y + (SHARP_CONTRAST_SIZE/2), GROUP_SAMPLE_SIZE);
						float luma = samples[arrayIndex];
						center = luma * rcp(sharpnessMultiplier);
						maximum = max(maximum, luma);
						minimum = min(minimum, luma);
						[unroll]
						for(int i = 0; i < SECTOR_COUNT; i++)
						{
							sum[i] += luma;
							squaredSum[i] += luma * luma;
							sampleCount[i]++;
						}
					}
					else
					{
						float angle = atan2(float(j), float(i)) + PI;
						uint sector = float((angle * SECTOR_COUNT) / (PI * 2)) % SECTOR_COUNT;
						uint arrayIndex = ARRAY_INDEX(i + coord.x + (SHARP_CONTRAST_SIZE/2), j + coord.y + (SHARP_CONTRAST_SIZE/2), GROUP_SAMPLE_SIZE);
						float luma = samples[arrayIndex];
						float centerDistance = length(float2(i, j));
						maximum = max(maximum, luma * rcp((centerDistance)));
						minimum = min(minimum, luma * (centerDistance));
						sum[sector] += luma;
						squaredSum[sector] += luma * luma;
						sampleCount[sector]++;
					}
				}
			}
			float edgeMultiplier = (max((1-(maximum - minimum) * rcp(maximum)), 1e-5));
			edgeMultiplier = edgeMultiplier * (1 - EdgeFloor) + EdgeFloor;
			
			float weightedSum = 0;
			float weightSum = 0;
			uint count = 0;
			[unroll]
			for(int i = 0; i < SECTOR_COUNT; i++)
			{
				float sumSquared = sum[i] * sum[i];
				float mean = sum[i] / sampleCount[i];
				float variance = (squaredSum[i] - ((sumSquared) / sampleCount[i]));
				variance /= sampleCount[i];

				float weight = max(/*dot(variance, float3(0.299, 0.587, 0.114))*/ variance, 1e-5);
				weight *= weight;
				weight *= weight;
				weight = rcp(1 + weight);

				weightedSum += mean * weight;
				weightSum += weight;
			}
	
			float kuwahara = ((weightedSum) / weightSum) / sharpnessMultiplier;
			
			
			if(Debug == 1) kuwahara = edgeMultiplier;
			else kuwahara = center + (center - kuwahara) * SharpenStrength * edgeMultiplier * 1.5;
			if(GammaCorrect) kuwahara = sqrt(kuwahara);
			tex2Dstore(wTemp, cornerCoord + coord, kuwahara);
		}
	}
	
	void OutputPS(float4 vpos : SV_POSITION, float2 texcoord : TEXCOORD, out float4 output : SV_TARGET0)
	{
		float y = tex2D(sTemp, texcoord).x;
		float3 color = tex2D(sBackBuffer, texcoord).rgb;
		float cb = dot(color, float3(-0.168736, -0.331264, 0.5));
		float cr = dot(color, float3(0.5, -0.418688, -0.081312));
		float luma = dot(color, float3(0.299, 0.587, 0.114));


		output.r = dot(float2(y, cr), float2(1, 1.402));//y + 1.402 * cr;
		output.g = dot(float3(y, cb, cr), float3(1, -0.344135, -0.714136));
		output.b = dot(float2(y, cb), float2(1, 1.772));//y + 1.772 * cb;
		output.a = 1;
		
		float depthCurve = 1;
		if(EnableDepth)
		{
			float depth = ReShade::GetLinearizedDepth(texcoord);
			depthCurve = lerp(0, 1, 1/max(exp(depth * (1 - DepthCurve) * 3), 0.0001));
			if(MaskSky && depth > 0.999) depthCurve = 0;
			output = lerp(color.rgbr, output, depthCurve);
		}
		
		if(Debug == 1) output = y * depthCurve;
		else if(Debug == 2)
		{
			output = (color.xyzx - output) + 0.5;
		}
		
	}
	#else
	void OutputPS(float4 vpos : SV_POSITION, float2 texcoord : TEXCOORD, out float4 output : SV_TARGET0)
	{
		float2 coord = texcoord * float2(BUFFER_WIDTH, BUFFER_HEIGHT);
		float sharpnessMultiplier = max(1023 * pow(( 2 * EdgeBias / 3) + 0.333333, 4), 1e-10);
		float sum[SECTOR_COUNT];
		float squaredSum[SECTOR_COUNT];
		float sampleCount[SECTOR_COUNT];
		float maximum = 0;
		float minimum = sharpnessMultiplier;
		float center;
		[unroll]
		for(int i = -(SHARP_CONTRAST_SIZE/2); i <= (SHARP_CONTRAST_SIZE/2); i++)
		{
			[unroll]
			for(int j = -(SHARP_CONTRAST_SIZE/2); j <= (SHARP_CONTRAST_SIZE/2); j++)
			{
				[flatten]
				if(all(int2(i, j) == 0))
				{
					//uint arrayIndex = ARRAY_INDEX(coord.x + (SHARP_CONTRAST_SIZE/2), coord.y + (SHARP_CONTRAST_SIZE/2), GROUP_SAMPLE_SIZE);
					float luma = tex2Dfetch(sLuma, coord).r;
					if(GammaCorrect)
					{
						luma *= luma;
					}
					center = luma;
					luma *= sharpnessMultiplier;
					maximum = max(maximum, luma);
					minimum = min(minimum, luma);
					[unroll]
					for(int i = 0; i < SECTOR_COUNT; i++)
					{
						sum[i] += luma;
						squaredSum[i] += luma * luma;
						sampleCount[i]++;
					}
				}
				else
				{
					float angle = atan2(float(j), float(i)) + PI;
					uint sector = float((angle * SECTOR_COUNT) / (PI * 2)) % SECTOR_COUNT;
					float luma = tex2Dfetch(sLuma, coord + float2(i, j)).r;
					if(GammaCorrect)
					{
						luma *= luma;
					}
					luma *= sharpnessMultiplier;
					float centerDistance = length(float2(i, j));
					maximum = max(maximum, luma * rcp((centerDistance)));
					minimum = min(minimum, luma * (centerDistance));
					sum[sector] += luma;
					squaredSum[sector] += luma * luma;
					sampleCount[sector]++;
				}
			}
		}
		
		float edgeMultiplier = (max((1-(maximum - minimum) * rcp(maximum)), 1e-5));
		edgeMultiplier = edgeMultiplier * (1 - EdgeFloor) + EdgeFloor;
		
		float weightedSum = 0;
		float weightSum = 0;
		uint count = 0;
		[unroll]
		for(int i = 0; i < SECTOR_COUNT; i++)
		{
			float sumSquared = sum[i] * sum[i];
			float mean = sum[i] / sampleCount[i];
			float variance = (squaredSum[i] - ((sumSquared) / sampleCount[i]));
			variance /= sampleCount[i];

			float weight = max(/*dot(variance, float3(0.299, 0.587, 0.114))*/ variance, 1e-5);
			weight *= weight;
			weight *= weight;
			weight = rcp(1 + weight);

			weightedSum += mean * weight;
			weightSum += weight;
		}

		float kuwahara = ((weightedSum) / weightSum) / sharpnessMultiplier;
		
		kuwahara = center + (center - kuwahara) * SharpenStrength * edgeMultiplier * 1.5;
		if(GammaCorrect) kuwahara = sqrt(kuwahara);
		
		if(Debug == 1) kuwahara = edgeMultiplier;
		
		float3 color = tex2Dfetch(sBackBuffer, coord).rgb;
		float cb = dot(color, float3(-0.168736, -0.331264, 0.5));
		float cr = dot(color, float3(0.5, -0.418688, -0.081312));
		float luma = dot(color, float3(0.299, 0.587, 0.114));


		output.r = dot(float2(kuwahara, cr), float2(1, 1.402));//y + 1.402 * cr;
		output.g = dot(float3(kuwahara, cb, cr), float3(1, -0.344135, -0.714136));
		output.b = dot(float2(kuwahara, cb), float2(1, 1.772));//y + 1.772 * cb;
		output.a = 1;
		
		float depthCurve = 1;
		if(EnableDepth)
		{
			float depth = ReShade::GetLinearizedDepth(texcoord);
			depthCurve = lerp(0, 1, 1/max(exp(depth * (1 - DepthCurve) * 3), 0.0001));
			if(MaskSky && depth > 0.999) depthCurve = 0;
			output = lerp(color.rgbr, output, depthCurve);
		}
		
		if(Debug == 1) output = kuwahara * depthCurve;
		else if(Debug == 2)
		{
			output = (kuwahara - luma) + 0.5;//(color.xyzx - output) + 0.5;
		}
	}
	#endif
	technique SharpContrast< ui_tooltip =     "SharpContrast is a sharpen that takes into acount a wider screen area,\n"
						  "than most sharpen shaders, while also attempting to avoid crossing contrast boundries, \n"
						  "giving it a unique look.\n\n"
						  "Part of Insane Shaders\n"
						  "By: Lord of Lunacy\n\n"
						  "SHARP_CONTRAST_COMPUTE: Allows you to chose to disable or enable\n"
						  "\t\t the compute shader mode in supported APIs.\n"
						  "SHARP_CONTRAST_SIZE: Changes the size of the filter being used.\n"
						  "\t\t Set lower for more performance.\n"
						  "SHARP_CONTRAST_FASTER_COMPILE: Set to 0 for a slight performance increase,\n"
						  "\t\t at the expense of a longer compile time. (Compute Only)";
	>
	{	
		pass
		{
			VertexShader = PostProcessVS;
			PixelShader = LumaPS;
			RenderTarget = Luma;
		}
		#if SHARP_CONTRAST_COMPUTE
		pass
		{
			ComputeShader = KuwaharaSharpCS<GROUP_SIZE.x, GROUP_SIZE.y>;
			DispatchSizeX = DIVIDE_ROUNDING_UP(BUFFER_WIDTH, (SAMPLES_PER_THREAD.x * GROUP_SIZE.x));
			DispatchSizeY = DIVIDE_ROUNDING_UP(BUFFER_HEIGHT, (SAMPLES_PER_THREAD.y * GROUP_SIZE.y));
		}
		#endif
		
		pass
		{
			VertexShader = PostProcessVS;
			PixelShader = OutputPS;
		}
	}
	
}
