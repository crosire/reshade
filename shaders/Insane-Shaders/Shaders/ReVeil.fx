#include "ReShade.fxh"
/*
ReVeil for Reshade
By: Lord of Lunacy
This shader attempts to remove fog using a dark channel prior technique that has been
refined using 2 passes over an iterative guided Wiener filter ran on the image dark channel.
The purpose of the Wiener filters is to minimize the root mean square error between
the given dark channel, and the true dark channel, making the removal more accurate.
The airlight of the image is estimated by using the max values that appears in the each
window of the dark channel. This window is then averaged together with every mip level
that is larger than the current window size.
Koschmeider's airlight equation is then used to remove the veil from the image, and the inverse
is applied to reverse this affect, blending any new image components with the fog.
This method was adapted from the following paper:
Gibson, Kristofor & Nguyen, Truong. (2013). Fast single image fog removal using the adaptive Wiener filter.
2013 IEEE International Conference on Image Processing, ICIP 2013 - Proceedings. 714-718. 10.1109/ICIP.2013.6738147. 
*/
#undef REVEIL_RES_DENOMINATOR
#define REVEIL_RES_DENOMINATOR 4

#define RENDER_WIDTH uint((BUFFER_WIDTH + REVEIL_RES_DENOMINATOR - 1) / (REVEIL_RES_DENOMINATOR))
#define RENDER_HEIGHT uint((BUFFER_HEIGHT + REVEIL_RES_DENOMINATOR - 1) / (REVEIL_RES_DENOMINATOR))

#define CONST_LOG2(x) (\
    (uint((x) & 0xAAAAAAAA) != 0) | \
    (uint(((x) & 0xFFFF0000) != 0) << 4) | \
    (uint(((x) & 0xFF00FF00) != 0) << 3) | \
    (uint(((x) & 0xF0F0F0F0) != 0) << 2) | \
    (uint(((x) & 0xCCCCCCCC) != 0) << 1))
	
#define BIT2_LOG2(x) ( (x) | (x) >> 1)
#define BIT4_LOG2(x) ( BIT2_LOG2(x) | BIT2_LOG2(x) >> 2)
#define BIT8_LOG2(x) ( BIT4_LOG2(x) | BIT4_LOG2(x) >> 4)
#define BIT16_LOG2(x) ( BIT8_LOG2(x) | BIT8_LOG2(x) >> 8)

#define FOGREMOVAL_LOG2(x) (CONST_LOG2( (BIT16_LOG2(x) >> 1) + 1))
	    
#if __RESHADE__ < 40800	
	#error "ReVeil requires ReShade 4.8 or higher to run"
#endif

#define FOGREMOVAL_MAX(a, b) (int((a) > (b)) * (a) + int((b) > (a)) * (b))

#define FOGREMOVAL_GET_MAX_MIP(w, h) \
(FOGREMOVAL_LOG2((FOGREMOVAL_MAX((w), (h))) + 1))

#define MAX_MIP (FOGREMOVAL_GET_MAX_MIP(RENDER_WIDTH * 2 - 1, RENDER_HEIGHT * 2 - 1))

#define REVEIL_WINDOW_SIZE 16
#define REVEIL_WINDOW_SIZE_SQUARED 256
#define RENDERER __RENDERER__

#if (((RENDERER >= 0xb000 && RENDERER < 0x10000) || (RENDERER >= 0x14300)) && __RESHADE__ >=40800)
	#ifndef REVEIL_COMPUTE
	#define REVEIL_COMPUTE 1
	#endif
#else
#define REVEIL_COMPUTE 0
#endif

//VRS_Map support
#define __SUPPORTED_VRS_MAP_COMPATIBILITY__ 10
#undef RENDERER
#if exists "VRS_Map.fxh"                                          
    #include "VRS_Map.fxh"
	#ifndef VRS_MAP
		#define VRS_MAP 1
	#endif
#else
    #define VRS_MAP 0
#endif

//Define the necessary functions and constants so the code can't break when the .fxh isn't present
#if VRS_MAP == 0
static const uint VRS_RATE1D_1X = 0x0;
static const uint VRS_RATE1D_2X = 0x1;
static const uint VRS_RATE1D_4X = 0x2;
#define VRS_MAKE_SHADING_RATE(x,y) ((x << 2) | (y))

static const uint VRS_RATE_1X1 = VRS_MAKE_SHADING_RATE(VRS_RATE1D_1X, VRS_RATE1D_1X); // 0;
static const uint VRS_RATE_1X2 = VRS_MAKE_SHADING_RATE(VRS_RATE1D_1X, VRS_RATE1D_2X); // 0x1;
static const uint VRS_RATE_2X1 = VRS_MAKE_SHADING_RATE(VRS_RATE1D_2X, VRS_RATE1D_1X); // 0x4;
static const uint VRS_RATE_2X2 = VRS_MAKE_SHADING_RATE(VRS_RATE1D_2X, VRS_RATE1D_2X); // 0x5;
//Only up to 2X2 is implemented currently
static const uint VRS_RATE_2X4 = VRS_MAKE_SHADING_RATE(VRS_RATE1D_2X, VRS_RATE1D_4X); // 0x6;
static const uint VRS_RATE_4X2 = VRS_MAKE_SHADING_RATE(VRS_RATE1D_4X, VRS_RATE1D_2X); // 0x9;
static const uint VRS_RATE_4X4 = VRS_MAKE_SHADING_RATE(VRS_RATE1D_4X, VRS_RATE1D_4X); // 0xa;

namespace VRS_Map
{
	uint ShadingRate(float2 texcoord, bool UseVRS)
	{
		return VRS_RATE_2X2;
	}
	uint ShadingRate(float2 texcoord, float VarianceCutoff, bool UseVRS)
	{
		return VRS_RATE_2X2;
	}
	uint ShadingRate(float2 texcoord, bool UseVRS, uint offRate)
	{
		return offRate;
	}
	uint ShadingRate(float2 texcoord, float VarianceCutoff, bool UseVRS, uint offRate)
	{
		return offRate;
	}
	float3 DebugImage(float3 originalImage, float2 texcoord, float VarianceCutoff, bool DebugView)
	{
		return originalImage;
	}
}
#endif //VRS_MAP

uniform float TransmissionMultiplier<
	ui_type = "slider";
	ui_label = "Strength";
	ui_category = "General";
	ui_tooltip = "The overall strength of the removal, negative values correspond to more removal,\n"
				"and positive values correspond to less.";
	ui_min = -1; ui_max = 1;
	ui_step = 0.001;
> = -0.125;

uniform float DepthMultiplier<
	ui_type = "slider";
	ui_label = "Depth Sensitivity";
	ui_category = "General";
	ui_tooltip = "This setting is for adjusting how much of the removal is depth based, or if\n"
				"positive values are set, it will actually add fog to the scene. 0 means it is\n"
				"unaffected by depth.";
	ui_min = -1; ui_max = 1;
	ui_step = 0.001;
> = -0.075;

#if (VRS_MAP != 0 && REVEIL_COMPUTE != 0)
	uniform float VarianceCutoff<
		ui_type = "slider";
		ui_label = "Variance Cutoff";
		ui_category = "Variable Rate Map";
		ui_tooltip = "Use this to adjust the level of variance at which ReVeil stops looking for fog (fog should be green in the debug view)";
		ui_min = 0.1; ui_max = 0.249;
		ui_step = 0.001;
	> = 0.15;
	
	uniform bool UseVRS<
		ui_label = "Experimental Variable Rate Map Support";
		ui_category = "Variable Rate Map";
		ui_tooltip = "Use this to allow ReVeil to use a variable rate map (VariableRateShading must be enabled for it to make a change)";
	> = false;
	
	uniform int Debug <
		ui_type = "combo";
		ui_items = "None\0Transmission Map\0Variable Rate Map\0";
		ui_label = "Debug View";
		ui_category = "Debug";
	> = 0;
#else
	static const float VarianceCutoff = 0;
	static const bool UseVRS = false;
	uniform int Debug <
	ui_type = "combo";
	ui_items = "None\0Transmission Map\0";
	ui_label = "Debug View";
	ui_category = "Debug";
	> = 0;
#endif
#if exists"RadiantGI.fx"
	texture Transmission {Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = R16f;};
	sampler sGITransmission {Texture = Transmission;};
#endif

namespace ReVeilCS
{
texture BackBuffer : COLOR;
texture Mean <Pooled = true;> {Width = RENDER_WIDTH; Height = RENDER_HEIGHT; Format = R16f;};
texture Variance <Pooled = true;> {Width = RENDER_WIDTH; Height = RENDER_HEIGHT; Format = R16f; MipLevels = MAX_MIP;};
texture Airlight {Width = RENDER_WIDTH; Height = RENDER_HEIGHT; Format = R16f;};
texture OriginalImage {Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = RGB10A2;};
texture Transmission {Width = RENDER_WIDTH; Height = RENDER_HEIGHT; Format = R16f;};

#if REVEIL_COMPUTE == 1
texture Maximum <Pooled = true;> {Width = ((RENDER_WIDTH - 1) / 16) + 1; Height = ((RENDER_HEIGHT - 1) / 16) + 1; Format = R8; MipLevels = MAX_MIP - 4;};
#else
texture MeanAndVariance <Pooled = true;> {Width = RENDER_WIDTH; Height = RENDER_HEIGHT; Format = RG16f;};
texture Maximum0 <Pooled = true;> {Width = RENDER_WIDTH; Height = RENDER_HEIGHT; Format = R8;};
texture Maximum <Pooled = true;> {Width = RENDER_WIDTH; Height = RENDER_HEIGHT; Format = R8; MipLevels = MAX_MIP;};


sampler sMeanAndVariance {Texture = MeanAndVariance;};
sampler sMaximum0 {Texture = Maximum0;};
#endif

sampler sBackBuffer {Texture = BackBuffer;};
sampler sMean {Texture = Mean;};
sampler sVariance {Texture = Variance;};
sampler sMaximum {Texture = Maximum;};
sampler sTransmission {Texture = ReVeilCS::Transmission;};
sampler sAirlight {Texture = Airlight;};
sampler sOriginalImage {Texture = OriginalImage;};

#if REVEIL_COMPUTE == 1
storage wMean {Texture = Mean;};
storage wVariance {Texture = Variance;};
storage wMaximum {Texture = Maximum;};
storage wTransmission {Texture = ReVeilCS::Transmission;};
storage wAirlight {Texture = Airlight;};
storage wOriginalImage {Texture = OriginalImage;};

groupshared float4 prefixSums[1024];
void StorePrefixSums(uint index, float2 value, uint iteration)
{
	if(iteration % 2 == 0)
	{
		prefixSums[index].xy = value;
	}
	else
	{
		prefixSums[index].zw = value;
	}
}

float2 AccessPrefixSums(uint index, uint iteration)
{
	if(iteration % 2 == 1)
	{
		return prefixSums[index].xy;
	}
	else
	{
		return prefixSums[index].zw;
	}
}

groupshared uint maximum;

void MeanAndVarianceCS(uint3 id : SV_DispatchThreadID, uint3 tid : SV_GroupThreadID)
{
	uint iteration = 0;
	uint2 groupCoord = id.xy - tid.xy;
	if(tid.x == 0)
	{
		maximum = 0;
	}
	barrier();
	
	//Indexing locations for the shader
	uint index[5];
	int x[4] = {int(tid.x), int(tid.x + 16), int(tid.x), int(tid.x + 16)};
	int y[4] = {int(tid.y), int(tid.y), int(tid.y + 16), int(tid.y + 16)};
	index[0] = y[0] * 32 + x[0];
	index[1] = y[1] * 32 + x[1];
	index[2] = y[2] * 32 + x[2];
	index[3] = y[3] * 32 + x[3];
	index[4] = index[0] + 264;
	int a[4] = {int(int(tid.x) - 8), int(int(tid.x) + 8), int(int(tid.x) - 8), int(int(tid.x) + 8)};
	int b[4] = {int(int(tid.y) - 8), int(int(tid.y) - 8), int(int(tid.y) + 8), int(int(tid.y) + 8)};
	/**/
	float3 originalImage;
	float2 sum[4];
	[unroll]
	for(int i = 0; i < 4; i++)
	{
		int2 coord = groupCoord + int2(a[i], b[i]);
		//float3 color = tex2Dlod(sBackBuffer, float4(float2(coord) / float2(RENDER_WIDTH, RENDER_HEIGHT), 0, 3)).rgb;
		float3 color = tex2Dfetch(sBackBuffer, coord * REVEIL_RES_DENOMINATOR).rgb;
		float minimum = min(min(color.r, color.b), color.g);
		sum[i] = float2(minimum, minimum * minimum);
		StorePrefixSums(index[i], sum[i], iteration);
		prefixSums[index[i]] = sum[i];
		if(i == 0)
		{
			atomicMax(maximum, uint((prefixSums[index[i]].x) * 255));
		}
	}
	barrier();
	iteration++;

	if(all(tid==0))
	{
		uint2 coord = id.xy / 16;
		tex2Dstore(wMaximum, coord, float4((float(maximum) * 1) / 255, 0, 0, 0));
	}
	
	//Generating rows of summed area table
	[unroll]
	for(int j = 0; j < 5; j++)
	{
		
		[unroll]
		for(int i = 0; i < 4; i++)
		{
			int address = index[i];
			
			int access = x[i] - exp2(j);
			access += y[i] * 32;
			if(x[i] >= exp2(j))
			{
			sum[i] += AccessPrefixSums(access, iteration);
			}
			StorePrefixSums(address, sum[i], iteration);
		}
		barrier();
		iteration++;
	}
	
	//Generating columns of summed area table
	[unroll]
	for(int j = 0; j < 5; j++)
	{

		[unroll]
		for(int i = 0; i < 4; i++)
		{
			int address = index[i];
			
			int access = y[i] - exp2(j);
			access *= 32;
			access += x[i];
			if(y[i] >= exp2(j))
			{
			sum[i] += AccessPrefixSums(access, iteration);
			}
			if(j != 4)
				StorePrefixSums(address, sum[i], iteration);
		}
		if(j != 4)
			barrier();
		iteration++;
	}

	//sampling from summed area table, and extractions the desired values
	float2 sums = sum[3] - sum[2] - sum[1] + sum[0];
	float mean = sums.x / 256;
	float variance = ((sums.y) - ((sums.x * sums.x) / 256));
	variance /= 256;
	

	
	tex2Dstore(wMean, id.xy, float4(mean, 0, 0, 0));
	tex2Dstore(wVariance, id.xy, float4(variance, 0, 0, 0));
}

void WienerFilterPS(float4 vpos : SV_POSITION, float2 texcoord : TEXCOORD, out float transmission : SV_TARGET0, out float airlight : SV_TARGET1)
{
	if(VRS_Map::ShadingRate(texcoord, VarianceCutoff, UseVRS, VRS_RATE_2X2) == VRS_RATE_1X1 && Debug == 0)
	{
		transmission = 1;
	}
	else
	{
		float mean = tex2D(sMean, texcoord).r;
		float variance = tex2D(sVariance, texcoord).r;
		float noise = tex2Dlod(sVariance, float4(texcoord, 0, MAX_MIP - 1)).r;
		float3 color = tex2D(sBackBuffer, texcoord).rgb;
		float darkChannel = min(min(color.r, color.g), color.b);
		float maximum = 0;
		
		[unroll]
		for(int i = 1; i < MAX_MIP - 4; i++)
		{
			maximum += tex2Dlod(sMaximum, float4(texcoord, 0, i)).r;
		}
		maximum /= MAX_MIP - 5;	
		
		float filter = saturate((max((variance - noise), 0) / variance) * (darkChannel - mean));
		float veil = saturate(mean + filter);
		//filter = ((variance - noise) / variance) * (darkChannel - mean);
		//mean += filter;
		float usedVariance = variance;
		
		airlight = clamp(maximum, 0.05, 1);//max(saturate(mean + sqrt(usedVariance) * StandardDeviations), 0.05);
		transmission = (1 - ((veil * darkChannel) / airlight));
		transmission *= (exp(DepthMultiplier * ReShade::GetLinearizedDepth(texcoord)));
		transmission *= exp(TransmissionMultiplier);  
	 }

}
#else

void MeanAndVariancePS0(float4 vpos : SV_POSITION, float2 texcoord : TEXCOORD, out float2 meanAndVariance : SV_TARGET0, out float maximum : SV_TARGET1)
{
	float darkChannel;
	float sum = 0;
	float squaredSum = 0;
	maximum = 0;
	for(int i = -(REVEIL_WINDOW_SIZE / 2); i < ((REVEIL_WINDOW_SIZE + 1) / 2); i++)
	{
			float2 offset = float2(i * BUFFER_RCP_WIDTH, 0);
			float3 color = tex2D(sBackBuffer, texcoord + offset).rgb;
			darkChannel = min(min(color.r, color.g), color.b);
			float darkChannelSquared = darkChannel * darkChannel;
			float darkChannelCubed = darkChannelSquared * darkChannel;
			sum += darkChannel;
			squaredSum += darkChannelSquared;
			maximum = max(maximum, darkChannel);
			
	}
	meanAndVariance = float2(sum, squaredSum);
}


void MeanAndVariancePS1(float4 vpos : SV_POSITION, float2 texcoord : TEXCOORD, out float mean : SV_TARGET0, out float variance : SV_TARGET1, out float maximum : SV_TARGET2)
{
	float2 meanAndVariance;
	float sum = 0;
	float squaredSum = 0;
	maximum = 0;
	for(int i = -(REVEIL_WINDOW_SIZE / 2); i < ((REVEIL_WINDOW_SIZE + 1) / 2); i++)
	{
			float2 offset = float2(0, i * BUFFER_RCP_HEIGHT);
			meanAndVariance = tex2D(sMeanAndVariance, texcoord + offset).rg;
			sum += meanAndVariance.r;
			squaredSum += meanAndVariance.g;
			maximum = max(maximum, tex2D(sMaximum0, texcoord + offset).r);
	}
	float sumSquared = sum * sum;
	
	mean = sum / REVEIL_WINDOW_SIZE_SQUARED;
	variance = (squaredSum - ((sumSquared) / REVEIL_WINDOW_SIZE_SQUARED));
	variance /= REVEIL_WINDOW_SIZE_SQUARED;
}

void WienerFilterPS(float4 vpos : SV_POSITION, float2 texcoord : TEXCOORD, out float transmission : SV_TARGET0, out float airlight : SV_TARGET1, out float4 originalImage : SV_TARGET2)
{
	if(VRS_Map::ShadingRate(texcoord, VarianceCutoff, UseVRS, VRS_RATE_2X2) == VRS_RATE_1X1 && Debug == 0)
	{
		transmission = 1;
	}
	else
	{
		float mean = tex2D(sMean, texcoord).r;
		float variance = tex2D(sVariance, texcoord).r;
		float noise = tex2Dlod(sVariance, float4(texcoord, 0, MAX_MIP - 1)).r;
		float3 color = tex2D(sBackBuffer, texcoord).rgb;
		float darkChannel = min(min(color.r, color.g), color.b);
		float maximum = 0;
		
		[unroll]
		for(int i = 4; i < MAX_MIP; i++)
		{
			maximum += tex2Dlod(sMaximum, float4(texcoord, 0, i)).r;
		}
		maximum /= MAX_MIP - 5;	
		
		float filter = saturate((max((variance - noise), 0) / variance) * (darkChannel - mean));
		float veil = saturate(mean + filter);
		//filter = ((variance - noise) / variance) * (darkChannel - mean);
		//mean += filter;
		float usedVariance = variance;
		
		airlight = clamp(maximum, 0.05, 1);//max(saturate(mean + sqrt(usedVariance) * StandardDeviations), 0.05);
		transmission = (1 - ((veil * darkChannel) / airlight));
		transmission *= (exp(DepthMultiplier * ReShade::GetLinearizedDepth(texcoord)));
		transmission *= exp(TransmissionMultiplier);
		transmission = saturate(transmission);

		 originalImage = float4(color, 1);   
	 }

}
#endif
void OriginalImagePS(float4 vpos : SV_POSITION, float2 texcoord : TEXCOORD, out float4 originalImage : SV_Target0
#if exists("RadiantGI.fx")
	, out float4 transmission : SV_Target1
#endif
)
{
	originalImage = float4(tex2Dfetch(sBackBuffer, texcoord * float2(BUFFER_WIDTH, BUFFER_HEIGHT)).rgb, 1);
#if exists("RadiantGI.fx")
	transmission = tex2D(sTransmission, texcoord);
#endif
}

void FogReintroductionPS(float4 vpos : SV_POSITION, float2 texcoord : TEXCOORD, out float4 fogReintroduced : SV_TARGET0)
{
	float transmission = max((tex2D(sTransmission, texcoord).r), 0.05);

	if(transmission == 1 && Debug == 0) discard;
	else if(Debug == 1)
	{
		fogReintroduced = transmission.rrrr;
	}
	else if(Debug == 2)
	{
		float3 originalImage = tex2D(sOriginalImage, texcoord).rgb;
		fogReintroduced = float4(VRS_Map::DebugImage(originalImage, texcoord, VarianceCutoff, true), 1);
	}
	else
	{
		float airlight = tex2D(sAirlight, texcoord).r;
		float3 newImage = (tex2D(sBackBuffer, texcoord).rgb);
		float3 originalImage = tex2D(sOriginalImage, texcoord).rgb;
		
		float y = dot(originalImage, float3(0.299, 0.587, 0.114));
		float originalLuma = ((y - airlight) / transmission) + airlight;
		
		y = dot(newImage, float3(0.299, 0.587, 0.114));
		float newLuma = ((y - airlight) / max(transmission, 0.05)) + airlight;
		
		float blended = (originalLuma - newLuma) * (1 - transmission);
		blended += newLuma;
		blended = lerp(originalLuma, newLuma, max(transmission, 0.05));
		
		blended = ((blended - airlight) * max(transmission, 0.05)) + airlight;
		
		float cb = -0.168736 * newImage.r - 0.331264 * newImage.g + 0.500000 * newImage.b;
		float cr = +0.500000 * newImage.r - 0.418688 * newImage.g - 0.081312 * newImage.b;
		newImage = float3(
			blended + 1.402 * cr,
			blended - 0.344136 * cb - 0.714136 * cr,
			blended + 1.772 * cb);
			
		fogReintroduced = float4(newImage, 1);
	}
	
}

}

technique ReVeil_Top <ui_tooltip = "This goes above any shaders you want to apply ReVeil to. \n\n"
								  "(Don't worry if it looks like its doing nothing, what its doing here won't take effect until ReVeil_Bottom is applied)\n\n"
								  "Part of Insane Shaders\n"
								  "By: Lord of Lunacy";>
{
#if REVEIL_COMPUTE == 1
	pass MeanAndVariance
	{
		ComputeShader = ReVeilCS::MeanAndVarianceCS<16, 16>;
		DispatchSizeX = ((RENDER_WIDTH - 1) / 16) + 1;
		DispatchSizeY = ((RENDER_HEIGHT - 1) / 16) + 1;
	}
#else
	pass MeanAndVariance
	{
		VertexShader = PostProcessVS;
		PixelShader = ReVeilCS::MeanAndVariancePS0;
		RenderTarget0 = ReVeilCS::MeanAndVariance;
		RenderTarget1 = ReVeilCS::Maximum0;
	}
	
	pass MeanAndVariance
	{
		VertexShader = PostProcessVS;
		PixelShader = ReVeilCS::MeanAndVariancePS1;
		RenderTarget0 = ReVeilCS::Mean;
		RenderTarget1 = ReVeilCS::Variance;
		RenderTarget2 = ReVeilCS::Maximum;
	}
#endif

	
	pass WienerFilter
	{
		VertexShader = PostProcessVS;
		PixelShader = ReVeilCS::WienerFilterPS;
		RenderTarget0 = ReVeilCS::Transmission;
		RenderTarget1 = ReVeilCS::Airlight;
	}
	
	pass OriginalImage
	{
		VertexShader = PostProcessVS;
		PixelShader = ReVeilCS::OriginalImagePS;
		RenderTarget0 = ReVeilCS::OriginalImage;
#if exists("RadiantGI.fx")
		RenderTarget1 = Transmission;
#endif
	}
}

technique ReVeil_Bottom <ui_tooptip = "This goes beneath the shaders you want to apply ReVeil to.";>
{
	pass FogReintroduction
	{
		VertexShader = PostProcessVS;
		PixelShader = ReVeilCS::FogReintroductionPS;
	}
}
