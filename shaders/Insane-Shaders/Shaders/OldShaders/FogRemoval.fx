/*
Reshade Fog Removal
By: Lord of Lunacy
This shader attempts to remove fog so that affects that experience light bleeding from it can be applied,
and then reintroduce the fog over the image.


This code was inspired by features mentioned in the following paper:

B. Cai, X. X, K. Jia, C. Qing, and D. Tao, “DehazeNet: An End-to-End System for Single Image Haze Removal,”
IEEE Transactions on Image Processing, vol. 25, no. 11, pp. 5187–5198, 2016.
*/


#undef SAMPLEDISTANCE
#define SAMPLEDISTANCE 15


#define SAMPLEDISTANCE_SQUARED (SAMPLEDISTANCE*SAMPLEDISTANCE)
#define SAMPLEHEIGHT (BUFFER_HEIGHT / SAMPLEDISTANCE)
#define SAMPLEWIDTH (BUFFER_WIDTH / SAMPLEDISTANCE)
#define SAMPLECOUNT (SAMPLEHEIGHT * SAMPLEWIDTH)
#define SAMPLECOUNT_RCP (1/SAMPLECOUNT)
#define HISTOGRAMPIXELSIZE (1/255)


#include "ReShade.fxh"
	uniform float STRENGTH<
		ui_category = "Fog Removal";
		ui_type = "drag";
		ui_min = 0.0; ui_max = 1.0;
		ui_label = "Strength";
		ui_tooltip = "Setting strength to high is known to cause bright regions to turn black before reintroduction.";
	> = 0.950;

	uniform float DEPTHCURVE<
		ui_type = "drag";
		ui_min = 0.0; ui_max = 1.0;
		ui_label = "Depth Curve";
		ui_category = "Fog Removal";
	> = 0;
	
	uniform float REMOVALCAP<
		ui_type = "drag";
		ui_min = 0.0; ui_max = 1.0;
		ui_label = "Fog Removal Cap";
		ui_category = "Fog Removal";
		ui_tooltip = "Prevents fog removal from trying to extract more details than can actually be removed, \n"
			"also helps preserve textures or lighting that may be detected as fog.";
	> = 0.35;
	
	uniform float2 MEDIANBOUNDS<
		ui_type = "drag";
		ui_min = 0.0; ui_max = 1.0;
		ui_label = "Average Light Levels";
		ui_category = "Fog Removal";
		ui_tooltip = "The number to the left should correspond to the average amount of light at night, \n"
			"the number to the right should correspond to the amount of light during the day.";
	> = float2(0.2, 0.8);
	
	uniform float2 SENSITIVITYBOUNDS<
		ui_type = "drag";
		ui_min = 0.0; ui_max = 1.0;
		ui_label = "Fog Sensitivity";
		ui_category = "Fog Removal";
		ui_tooltip = "This number adjusts how sensitive the shader is to fog, a lower number means that \n"
			"it will detect more fog in the scene, but will also be more vulnerable to false positives.\n"
			"A higher number means that it will detect less fog in the scene but will also be more \n"
			"likely to fail at detecting fog. The number on the left corresponds to the value used at night, \n"
			"while the number on the right corresponds to the value used during the day.";
	> = float2(0.2, 0.75);
	
	uniform bool USEDEPTH<
		ui_label = "Ignore the sky";
		ui_category = "Fog Removal";
		ui_tooltip = "Useful for shaders such as RTGI that rely on skycolor";
	> = 0;
	
	uniform bool PRESERVEDETAIL<
		ui_label = "Preserve Detail";
		ui_category = "Fog Removal";
		ui_tooltip = "Preserves finer details at the cost of some haloing and some performance.";
	> = 1;
	
	uniform bool CORRECTCOLOR<
		ui_category = "Color Correction";
		ui_label = "Apply color correction";
		ui_tooltip = "Helps with detecting fog that is not neutral in color. \n\n"
					"Note: This setting is not always needed or may sometimes \n"
					"      hinder fog removal.";
	> = 1;
	
	uniform float MAXPERCENTILE<
		ui_type = "drag";
		ui_label = "Color Correction Percentile";
		ui_category = "Color Correction";
		ui_min = 0.0; ui_max = 0.999;
		ui_tooltip = "This percentile is the one used when finding the max RGB values for color correction, \n"
					"having this set high may cause issues with things like UI elements, and image stability.";
	> = 0.95;
	
	uniform int DEBUG<
		ui_type = "combo";
		ui_category = "Debug";
		ui_label = "Debug";
		ui_items = "None \0Color Correction \0Transmission Map \0Fog Removed \0";
	> = 0;
	
/*texture ColorAttenuation {Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = R8;};
sampler sColorAttenuation {Texture = ColorAttenuation;};
texture DarkChannel {Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = R8;};
sampler sDarkChannel {Texture = DarkChannel;};*/
texture ColorCorrected {Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = RGBA16F;};
sampler sColorCorrected {Texture = ColorCorrected;};
texture Transmission {Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = R8;};
sampler sTransmission {Texture = Transmission;};
texture FogRemovalHistogram {Width = 256; Height = 12; Format = R32F;};
sampler sFogRemovalHistogram {Texture = FogRemovalHistogram;};
texture HistogramInfo {Width = 1; Height = 1; Format = RGBA8;};
sampler sHistogramInfo {Texture = HistogramInfo;};
texture FogRemoved {Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = RGBA16F;};
sampler sFogRemoved {Texture = FogRemoved;};
texture TruncatedPrecision {Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = RGBA16F;};
sampler sTruncatedPrecision {Texture = TruncatedPrecision;};


/*void HistogramVS(uint id : SV_VERTEXID, out float4 pos : SV_POSITION)
{
	uint xpos = id % SAMPLEWIDTH;
	uint ypos = id / SAMPLEWIDTH;
	xpos *= SAMPLEDISTANCE;
	ypos *= SAMPLEDISTANCE;
	int4 texturePos = int4(xpos, ypos, 0, 0);
	float color;
	float3 rgb = tex2Dfetch(ReShade::BackBuffer, texturePos).rgb;
	float3 luma = (0.3333, 0.3333, 0.3333);
	color = dot(rgb, luma);
	color = (color * 255 + 0.5)/256;
	pos = float4(color * 2 - 1, 0, 0, 1);
}*/

void HistogramVS(uint id : SV_VERTEXID, out float4 pos : SV_POSITION)
{
	uint iterationCount = 4;
	uint iteration = (id % iterationCount);
	uint xpos = uint(id / iterationCount) % SAMPLEWIDTH;
	uint ypos = uint(id / iterationCount) / SAMPLEWIDTH;
	xpos *= SAMPLEDISTANCE;
	ypos *= SAMPLEDISTANCE;
	int4 texturePos = int4(xpos, ypos, 0, 0);
	float color;
	float y;
	if (iteration == 0)
	{
		float3 rgb = tex2Dfetch(ReShade::BackBuffer, texturePos).rgb;
		float3 luma = (0.33333333, 0.33333333, 0.33333333);
		color = dot(rgb, luma);
		y = 0.75;
	}
	else if (iteration == 1) 
	{
		color = tex2Dfetch(ReShade::BackBuffer, texturePos).r;
		y = 0.25;
	}
	else if (iteration == 2)
	{
		color = tex2Dfetch(ReShade::BackBuffer, texturePos).g;
		y = -0.25;
	}
	else if (iteration == 3)
	{
		color = tex2Dfetch(ReShade::BackBuffer, texturePos).b;
		y = -0.75;
	}
	color = (color * 255 + 0.5)/256;
	pos = float4(color * 2 - 1, y, 0, 1);
}

void HistogramPS(float4 pos : SV_POSITION, out float col : SV_TARGET )
{
	col = 1;
}
void HistogramInfoPS(float4 pos : SV_Position, out float4 output : SV_Target0)
{

	int fifty = 0.5 * SAMPLECOUNT;
	int usedMax = (MAXPERCENTILE * SAMPLECOUNT);
	int4 sum = SAMPLECOUNT;
	output = 255;
	int i = 255;
	while((sum.r <= SAMPLECOUNT || sum.g <= SAMPLECOUNT || sum.b <= SAMPLECOUNT || sum.a <= SAMPLECOUNT) && i >= 0)
	{
		if(CORRECTCOLOR)
		{
			sum.r -= tex2Dfetch(sFogRemovalHistogram, int4(i, 4, 0, 0)).r;
			sum.g -= tex2Dfetch(sFogRemovalHistogram, int4(i, 7, 0, 0)).r;
			sum.b -= tex2Dfetch(sFogRemovalHistogram, int4(i, 10, 0, 0)).r;
			if (sum.r < usedMax)
			{
				sum.r = 2 * SAMPLECOUNT;
				output.r = i;
			}
			if (sum.g < usedMax)
			{
				sum.g = 2 * SAMPLECOUNT;
				output.g = i;
			}
			if (sum.b < usedMax)
			{
				sum.b = 2 * SAMPLECOUNT;
				output.b = i;
			}
		}
		else
		{
			sum.rgb = -SAMPLECOUNT;
		}
		sum.a -= tex2Dfetch(sFogRemovalHistogram, int4(i, 1, 0, 0)).r;
		if (sum.a < fifty)
		{
			sum.a =  2 * SAMPLECOUNT;
			output.a = i;
		}
		i--;
	}
	output = output / 255;
	output.rgb = dot(0.33333333, output.rgb) / output.rgb;
}

void ColorCorrectedPS(float4 pos: SV_Position, float2 texcoord : TexCoord, out float4 colorCorrected : SV_Target0)
{
	float3 color = tex2D(ReShade::BackBuffer, texcoord).rgb;
	if(CORRECTCOLOR)
	{
		color *= tex2Dfetch(sHistogramInfo, float4(0, 0, 0, 0)).rgb;
	}
	colorCorrected = float4(color, 1);
}

/*void FeaturesPS(float4 pos : SV_Position, float2 texcoord : TexCoord, out float colorAttenuation : SV_Target0, out float darkChannel : SV_Target1)
{
	float3 color = tex2D(sColorCorrected, texcoord).rgb;
	float value = max(max(color.r, color.g), color.b);
	float minimum = min(min(color.r, color.g), color.b);
	float saturation = (value - minimum) / (value);
	colorAttenuation = value - saturation;
	darkChannel = 1;
	float depth = ReShade::GetLinearizedDepth(texcoord);
	float2 pixSize = tex2Dsize(sColorCorrected, 0);
	pixSize.x = 1 / pixSize.x;
	pixSize.y = 1 / pixSize.y;
	float depthContrast = 0;
	[unroll]for(int i = -2; i <= 2; i++)
	{
		float sum = 0;
		float depthSum = 0;
		[unroll]for(int j = -2; j <= 2; j++)
		{
			color = tex2Doffset(sColorCorrected, texcoord, int2(i, j)).rgb;
			darkChannel = min(min(color.r, color.g), min(color.b, darkChannel));
			float2 matrixCoord;
			matrixCoord.x = texcoord.x + pixSize.x * i;
			matrixCoord.y = texcoord.y + pixSize.y * j;
			float depth1 = ReShade::GetLinearizedDepth(matrixCoord);
			float depthSubtract = depth - depth1;
			depthSum += depthSubtract * depthSubtract;
		}
		depthContrast = max(depthContrast, depthSum);
	}
	depthContrast = sqrt(0.2 * depthContrast);
	darkChannel = lerp(darkChannel, minimum, saturate(2 * depthContrast));
}*/

void TransmissionPS(float4 pos: SV_Position, float2 texcoord : TexCoord, out float transmission : SV_Target0)
{
	float depth = ReShade::GetLinearizedDepth(texcoord);
	if(USEDEPTH == 1)
	{
		if(depth >= 1)
		{
			transmission = 0;
			return;
		}
	}
	float3 color = tex2D(sColorCorrected, texcoord).rgb;
	float value = max(max(color.r, color.g), color.b);
	float minimum = min(min(color.r, color.g), color.b);
	float saturation = (value - minimum) / (value);
	float colorAttenuation = value - saturation;
	float darkChannel = minimum;
	float2 pixSize = tex2Dsize(sColorCorrected, 0);
	pixSize.x = 1 / pixSize.x;
	pixSize.y = 1 / pixSize.y;
	float depthContrast = 0;
	if(PRESERVEDETAIL)
	{
		[unroll]for(int i = -2; i <= 2; i++)
		{
			float depthSum = 0;
			[unroll]for(int j = -2; j <= 2; j++)
			{
				color = tex2Doffset(sColorCorrected, texcoord, int2(i, j)).rgb;
				darkChannel = min(min(color.r, color.g), min(color.b, darkChannel));
				float2 matrixCoord;
				matrixCoord.x = texcoord.x + pixSize.x * i;
				matrixCoord.y = texcoord.y + pixSize.y * j;
				float depth1 = ReShade::GetLinearizedDepth(matrixCoord);
				float depthSubtract = depth - depth1;
				depthSum += depthSubtract * depthSubtract;
			}
			depthContrast = max(depthContrast, depthSum);
		}
		depthContrast = sqrt(0.2 * depthContrast);
		darkChannel = lerp(darkChannel, minimum, saturate(2 * depthContrast));
	}
	transmission = (darkChannel / (1 - colorAttenuation));
	float median = clamp(tex2Dfetch(sHistogramInfo, int4(0, 0, 0, 0)).a, MEDIANBOUNDS.x, MEDIANBOUNDS.y);
	float v = (median - MEDIANBOUNDS.x) * ((SENSITIVITYBOUNDS.x - SENSITIVITYBOUNDS.y) / (MEDIANBOUNDS.x - MEDIANBOUNDS.y)) + SENSITIVITYBOUNDS.x;
	transmission = saturate(transmission - v * (darkChannel + darkChannel));
	transmission = clamp(transmission * (1-v), 0, REMOVALCAP);
	float strength = saturate((pow(depth, 100*DEPTHCURVE)) * STRENGTH);
	transmission *= strength;
}

void FogRemovalPS(float4 pos: SV_Position, float2 texcoord : TexCoord, out float4 output : SV_Target0)
{
	float transmission = tex2D(sTransmission, texcoord).r;
	float depth = ReShade::GetLinearizedDepth(texcoord);
	float3 original = tex2D(sColorCorrected, texcoord).rgb;
	float multiplier = max(((1 - transmission)), 0.01);
	output.rgb = (original.rgb - transmission) / (multiplier);
	if (CORRECTCOLOR)
	{
	output.rgb /= tex2Dfetch(sHistogramInfo, float4(0, 0, 0, 0)).rgb;
	}
	output = float4(output.rgb, 1);
	if (DEBUG == 1)
	{
		output = original;
	}
	else if (DEBUG == 2)
	{
		output = transmission;
	}
}

void WriteFogRemovedPS(float4 pos: SV_Position, float2 texcoord : TexCoord, out float3 output : SV_Target0)
{
	float depth = ReShade::GetLinearizedDepth(texcoord);
	output = tex2D(sFogRemoved, texcoord).rgb;
}

void TruncatedPrecisionPS(float4 pos: SV_Position, float2 texcoord : TexCoord, out float4 output : SV_Target0)
{
	float depth = ReShade::GetLinearizedDepth(texcoord);
	output = float4((tex2D(sFogRemoved, texcoord).rgb - tex2D(ReShade::BackBuffer, texcoord).rgb), 1);
}

void FogReintroductionPS(float4 pos : SV_Position, float2 texcoord : TexCoord, out float3 output : SV_Target0)
{
	float depth = ReShade::GetLinearizedDepth(texcoord);
	if(USEDEPTH == 1)
	{
		if(depth >= 1) discard;
	}
	float transmission = tex2D(sTransmission, texcoord).r;
	float3 original = tex2D(ReShade::BackBuffer, texcoord).rgb + tex2D(sTruncatedPrecision, texcoord).rgb;
	if (CORRECTCOLOR)
	{
	original *= tex2Dfetch(sHistogramInfo, float4(0, 0, 0, 0)).rgb;
	}
	float multiplier = max(((1 - transmission)), 0.01);
	output = original * multiplier + transmission;
	if (CORRECTCOLOR)
	{
	output /= tex2Dfetch(sHistogramInfo, float4(0, 0, 0, 0)).rgb;
	}
	if(DEBUG > 0)
	{
		output = tex2D(sFogRemoved, texcoord).rgb;
	}
}

technique FogRemoval
{
	pass Histogram
	{
		PixelShader = HistogramPS;
		VertexShader = HistogramVS;
		PrimitiveTopology = POINTLIST;
		VertexCount = SAMPLECOUNT * 4;
		RenderTarget0 = FogRemovalHistogram;
		ClearRenderTargets = true; 
		BlendEnable = true; 
		SrcBlend = ONE; 
		DestBlend = ONE;
		BlendOp = ADD;
	}
	
	pass HistogramInfo
	{
		VertexShader = PostProcessVS;
		PixelShader = HistogramInfoPS;
		RenderTarget0 = HistogramInfo;
		ClearRenderTargets = true;
	}
	
	pass ColorCorrected
	{
		VertexShader = PostProcessVS;
		PixelShader = ColorCorrectedPS;
		RenderTarget0 = ColorCorrected;
	}
	
/*	pass Features
	{
		VertexShader = PostProcessVS;
		PixelShader = FeaturesPS;
		RenderTarget0 = ColorAttenuation;
		RenderTarget1 = DarkChannel;
	}
*/	
	pass Transmission
	{
		VertexShader = PostProcessVS;
		PixelShader = TransmissionPS;
		RenderTarget0 = Transmission;
	}
	
	pass FogRemoval
	{
		VertexShader = PostProcessVS;
		PixelShader = FogRemovalPS;
		RenderTarget0 = FogRemoved;
	}
	
	pass FogRemoved
	{
		VertexShader = PostProcessVS;
		PixelShader = WriteFogRemovedPS;
	}
	
	pass TruncatedPrecision
	{
		VertexShader = PostProcessVS;
		PixelShader = TruncatedPrecisionPS;
		RenderTarget0 = TruncatedPrecision;
	}
	
}

technique FogReintroduction
{
	pass Reintroduction
	{
		VertexShader = PostProcessVS;
		PixelShader = FogReintroductionPS;
	}
}
