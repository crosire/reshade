/*
	Optical Flow For ReShade
	By: Lord Of Lunacy
	
	This shader is my attempt at generating optical flow motion vectors for real-time use in shaders, such as for
	motion blur.
	
	This shader is still in a beta state, so there will be bugs at times.
*/

#define DIVIDE_ROUNDING_UP(numerator, denominator) (int(numerator + denominator - 1) / int(denominator))
#define DIVIDE_ROUNDING_UP2(numerator, denominator) (int2(numerator + denominator - 1) / int2(denominator))

#define MIP_LEVELS 6
#define BUFFER_SIZE5 uint2(BUFFER_WIDTH, BUFFER_HEIGHT)
#define BUFFER_SIZE4 (BUFFER_SIZE5 / 2)
#define BUFFER_SIZE3 (BUFFER_SIZE4 / 2)
#define BUFFER_SIZE2 (BUFFER_SIZE3 / 2)
#define BUFFER_SIZE1 (BUFFER_SIZE2 / 2)
#define BUFFER_SIZE0 (BUFFER_SIZE1 / 2)

#ifndef OPTICAL_FLOW_SUBPIXEL
	#define OPTICAL_FLOW_SUBPIXEL 0
#endif

#include "ReShade.fxh"

namespace OpticalFlow
{
texture BackBuffer : COLOR;
texture Output {Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = RGB10A2;};
texture PrevBackBuffer {Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = RGB10A2;};
texture PrevSAD {Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = R8; MipLevels = MIP_LEVELS;};
texture CurrSAD {Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = R8; MipLevels = MIP_LEVELS;};
texture Motion0 {Width = BUFFER_SIZE0.x; Height = BUFFER_SIZE0.y; Format = RG8;};
texture Motion1 {Width = BUFFER_SIZE1.x; Height = BUFFER_SIZE1.y; Format = RG8;};
texture Motion2 {Width = BUFFER_SIZE2.x; Height = BUFFER_SIZE2.y; Format = RG8;};
texture Motion3 {Width = BUFFER_SIZE3.x; Height = BUFFER_SIZE3.y; Format = RG8;};
texture Motion4 {Width = BUFFER_SIZE4.x; Height = BUFFER_SIZE4.y; Format = RG8;};
texture Motion5 {Width = BUFFER_SIZE5.x; Height = BUFFER_SIZE5.y; Format = RG8;};


sampler sBackBuffer {Texture = BackBuffer;};
sampler sOutput {Texture = Output;};
sampler sPrevBackBuffer {Texture = PrevBackBuffer;};
sampler sPrevSAD {Texture = PrevSAD;};
sampler sCurrSAD {Texture = CurrSAD;};
sampler sMotion0 {Texture = Motion0;};
sampler sMotion1 {Texture = Motion1;};
sampler sMotion2 {Texture = Motion2;};
sampler sMotion3 {Texture = Motion3;};
sampler sMotion4 {Texture = Motion4;};
sampler sMotion5 {Texture = Motion5;};


#if OPTICAL_FLOW_SUBPIXEL != 0
	texture Motion6 {Width = BUFFER_SIZE5.x; Height = BUFFER_SIZE5.y; Format = RG8;};
	sampler sMotion6 {Texture = Motion6;};
#endif

uniform float Threshold<
	ui_type = "slider";
	ui_label = "Center Bias";
	ui_min = 0.00; ui_max = 1;
> = 0.005;

uniform float ReprojectionThreshold<
	ui_type = "slider";
	ui_label = "Reprojection Threshold";
	ui_min = 0.00; ui_max = 1;
> = 0.02;

uniform float ReprojectionBlending<
	ui_type = "slider";
	ui_label = "Previous Frame Blend";
	ui_min = 0.00; ui_max = 1;
> = 0.1;

uniform int Debug<
	ui_type = "combo";
	ui_label = "Debug";
	ui_items = "None \0Reprojection \0Reprojectable Samples \0Reprojected Frame\0 Motion Vectors \0";
> = 1;

//https://vec3.ca/bicubic-filtering-in-fewer-taps/
float3 BSplineBicubicFilter(sampler sTexture, float2 texcoord)
{
	float2 textureSize = tex2Dsize(sTexture);
	float2 coord = texcoord * textureSize;
	float2 x = frac(coord);
	coord = floor(coord) - 0.5;
	float2 x2 = x * x;
	float2 x3 = x2 * x;
	//compute the B-Spline weights
 
	float2 w0 = x2 - 0.5 * (x3 + x);
	float2 w1 = 1.5 * x3 - 2.5 * x2 + 1.0;
	float2 w3 = 0.5 * (x3 - x2);
	float2 w2 = 1.0 - w0 - w1 - w3;

	//get our texture coordinates
 
	float2 s0 = w0 + w1;
	float2 s1 = w2 + w3;
 
	float2 f0 = w1 / (w0 + w1);
	float2 f1 = w3 / (w2 + w3);
 
	float2 t0 = coord - 1 + f0;
	float2 t1 = coord + 1 + f1;
	t0 /= textureSize;
	t1 /= textureSize;
	
	
	return
		(tex2D(sTexture, float2(t0.x, t0.y)).rgb * s0.x
		 +  tex2D(sTexture, float2(t1.x, t0.y)).rgb * s1.x) * s0.y
		 + (tex2D(sTexture, float2(t0.x, t1.y)).rgb * s0.x
		 +  tex2D(sTexture, float2(t1.x, t1.y)).rgb * s1.x) * s1.y;
		 //return tex2D(sTexture, texcoord).rgb;

}

// Vertex shader generating a triangle covering the entire screen
void PostProcessVS(in uint id : SV_VertexID, out float4 position : SV_Position, out float2 texcoord : TEXCOORD)
{
	texcoord.x = (id == 2) ? 2.0 : 0.0;
	texcoord.y = (id == 1) ? 2.0 : 0.0;
	position = float4(texcoord * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
}

void CurrToPrevPS(float4 pos : SV_Position, float2 texcoord : TEXCOORD, out float prevSAD : SV_Target0)
{
	prevSAD = tex2Dfetch(sCurrSAD, floor(texcoord * BUFFER_SIZE5)).x;
	//float2 coord = floor(texcoord * BUFFER_SIZE5);
	//prevSAD = dot(tex2Dfetch(sPrevBackBuffer, coord).rgb, float3(0.299, 0.587, 0.114));
}

void OutputPS(float4 pos : SV_Position, float2 texcoord : TEXCOORD, out float3 output : SV_Target0)
{
	output = tex2D(sOutput, texcoord).rgb;
}

void PrevToCurrPS(float4 pos : SV_Position, float2 texcoord : TEXCOORD, out float4 color : SV_Target0, out float4 history : SV_Target1)
{
#if OPTICAL_FLOW_SUBPIXEL != 0
	float scaleFactor = 128;
	float2 motionVector = ((BSplineBicubicFilter(sMotion6, texcoord)) - 0.5).xy * scaleFactor;//tex2Dlod(sMotion5, float4(texcoord, 0, 1));
#else
	float scaleFactor = 256;
	float2 motionVector = ((BSplineBicubicFilter(sMotion5, texcoord)) - 0.5).xy * scaleFactor;//tex2Dlod(sMotion5, float4(texcoord, 0, 1));
#endif
	
	if(Debug == 0)
	{
		color.rgb = tex2D(sBackBuffer, texcoord).rgb;
		history = float4(color.rgb, 1);
	}
	else if(Debug == 1)
	{
		float3 prevR = tex2Dfetch(sPrevBackBuffer, floor(texcoord * BUFFER_SIZE5) + motionVector).rgb;
		float3 curr = tex2Dfetch(sBackBuffer, floor(texcoord * BUFFER_SIZE5)).rgb;
		//color.b = tex2Dfetch(sCurrSAD, floor(texcoord * BUFFER_SIZE5)).xx;
		//color.rg = tex2Dfetch(sPrevSAD, floor(texcoord * BUFFER_SIZE5) + motionVector).xx;
		if(dot(abs(prevR - curr), float3(0.299, 0.587, 0.114)) <= ReprojectionThreshold)
		{
			color.rgb = lerp(prevR, curr, ReprojectionBlending);
		}
		else
		{
			float3 prev = tex2Dfetch(sPrevBackBuffer, floor(texcoord * BUFFER_SIZE5)).rgb;
			if(dot(abs(prev - curr), float3(0.299, 0.587, 0.114)) <= ReprojectionThreshold)
			{
				color.rgb = lerp(prev, curr, ReprojectionBlending);
			}
			else color.rgb = curr;
		}
		history = float4(color.rgb, 1);
	}
	else if(Debug == 2)
	{
		float3 prev = tex2Dfetch(sPrevBackBuffer, floor(texcoord * BUFFER_SIZE5) + motionVector).rgb;
		float3 curr = tex2Dfetch(sBackBuffer, floor(texcoord * BUFFER_SIZE5)).rgb;
		//color.b = tex2Dfetch(sCurrSAD, floor(texcoord * BUFFER_SIZE5)).xx;
		//color.rg = tex2Dfetch(sPrevSAD, floor(texcoord * BUFFER_SIZE5) + motionVector).xx;
		bool historyNeeded = true;
		if(dot(abs(prev - curr), float3(0.299, 0.587, 0.114)) > ReprojectionThreshold)
		{
			color.rg = 0;
		}
		else
		{
			color.rg = 1;
			history = float4(lerp(prev, curr, ReprojectionBlending), 1);
			historyNeeded = false;
		}
		
		prev = tex2Dfetch(sPrevBackBuffer, floor(texcoord * BUFFER_SIZE5)).rgb;
		
		if(dot(abs(prev - curr), float3(0.299, 0.587, 0.114)) > ReprojectionThreshold)
		{
			color.b = 0;
		}
		else
		{
			if(historyNeeded)
			{
				history = float4(lerp(prev, curr, ReprojectionBlending), 1);
				historyNeeded = false;
			}
			color.b = 1;
		}
		if(historyNeeded)
		{
			history = float4(curr, 1);
		}
	}
	else if(Debug == 3)
	{
		float3 prevR = tex2Dfetch(sPrevBackBuffer, floor(texcoord * BUFFER_SIZE5) + motionVector).rgb;
		float3 curr = tex2Dfetch(sBackBuffer, floor(texcoord * BUFFER_SIZE5)).rgb;
		//color.b = tex2Dfetch(sCurrSAD, floor(texcoord * BUFFER_SIZE5)).xx;
		//color.rg = tex2Dfetch(sPrevSAD, floor(texcoord * BUFFER_SIZE5) + motionVector).xx;
		if(dot(abs(prevR - curr), float3(0.299, 0.587, 0.114)) <= ReprojectionThreshold)
		{
			history = float4(lerp(prevR, curr, ReprojectionBlending), 1);
			color.rgb = prevR;
		}
		else
		{
			float3 prev = tex2Dfetch(sPrevBackBuffer, floor(texcoord * BUFFER_SIZE5)).rgb;
			if(dot(abs(prev - curr), float3(0.299, 0.587, 0.114)) <= ReprojectionThreshold)
			{
				history = float4(lerp(prev, curr, ReprojectionBlending), 1);
				color.rgb = prev;
			}
			else
			{
				color.rgb = 0;
				history = float4(curr, 1);
			}
		}
		//history = float4(color, 1);
	}
	else
	{
		motionVector = (motionVector / scaleFactor) + 0.5;
		color.rg = motionVector.x;
		color.b = motionVector.y;
		history = float4(tex2D(sBackBuffer, texcoord).rgb, 1);
	}
	color.a = 1;
}

void PreparationPS(float4 pos : SV_Position, float2 texcoord : TEXCOORD, out float currSAD : SV_Target0, out float prevSAD : SV_Target1)
{
	float2 coord = floor(texcoord * BUFFER_SIZE5);
	
	
	/*currSAD = dot(tex2Dfetch(sBackBuffer, coord + float2(-0.5, -0.5) + float2(-1, -1)).rgb, float3(0.299, 0.587, 0.114));
	currSAD += dot(tex2Dfetch(sBackBuffer, coord + float2(-0.5, 0.5) + float2(-1, -1)).rgb, float3(0.299, 0.587, 0.114));
	currSAD += dot(tex2Dfetch(sBackBuffer, coord + float2(0.5, -0.5) + float2(-1, -1)).rgb, float3(0.299, 0.587, 0.114));
	currSAD += dot(tex2Dfetch(sBackBuffer, coord + float2(0.5, 0.5) + float2(-1, -1)).rgb, float3(0.299, 0.587, 0.114));
	
	currSAD += dot(tex2Dfetch(sBackBuffer, coord + float2(-0.5, -0.5) + float2(-1, 1)).rgb, float3(0.299, 0.587, 0.114));
	currSAD += dot(tex2Dfetch(sBackBuffer, coord + float2(-0.5, 0.5) + float2(-1, 1)).rgb, float3(0.299, 0.587, 0.114));
	currSAD += dot(tex2Dfetch(sBackBuffer, coord + float2(0.5, -0.5) + float2(-1, 1)).rgb, float3(0.299, 0.587, 0.114));
	currSAD += dot(tex2Dfetch(sBackBuffer, coord + float2(0.5, 0.5) + float2(-1, 1)).rgb, float3(0.299, 0.587, 0.114));
	
	currSAD += dot(tex2Dfetch(sBackBuffer, coord + float2(-0.5, -0.5) + float2(1, 1)).rgb, float3(0.299, 0.587, 0.114));
	currSAD += dot(tex2Dfetch(sBackBuffer, coord + float2(-0.5, 0.5) + float2(1, 1)).rgb, float3(0.299, 0.587, 0.114));
	currSAD += dot(tex2Dfetch(sBackBuffer, coord + float2(0.5, -0.5) + float2(1, 1)).rgb, float3(0.299, 0.587, 0.114));
	currSAD += dot(tex2Dfetch(sBackBuffer, coord + float2(0.5, 0.5) + float2(1, 1)).rgb, float3(0.299, 0.587, 0.114));
	
	currSAD += dot(tex2Dfetch(sBackBuffer, coord + float2(-0.5, -0.5) + float2(1, -1)).rgb, float3(0.299, 0.587, 0.114));
	currSAD += dot(tex2Dfetch(sBackBuffer, coord + float2(-0.5, 0.5) + float2(1, -1)).rgb, float3(0.299, 0.587, 0.114));
	currSAD += dot(tex2Dfetch(sBackBuffer, coord + float2(0.5, -0.5) + float2(1, -1)).rgb, float3(0.299, 0.587, 0.114));
	currSAD += dot(tex2Dfetch(sBackBuffer, coord + float2(0.5, 0.5) + float2(1, -1)).rgb, float3(0.299, 0.587, 0.114));
	
	currSAD /= 16;*/
	currSAD = dot(tex2Dfetch(sBackBuffer, coord).rgb, float3(0.299, 0.587, 0.114));
	prevSAD = dot(tex2Dfetch(sPrevBackBuffer, coord).rgb, float3(0.299, 0.587, 0.114));
	
	//currSAD = length(float2(currSAD, ReShade::GetLinearizedDepth(texcoord)));
	
	/*currSAD = dot(tex2Dfetch(sBackBuffer, coord + float2(-0.5, -0.5)).rgb, float3(0.299, 0.587, 0.114));
	currSAD += dot(tex2Dfetch(sBackBuffer, coord + float2(-0.5, 0.5)).rgb, float3(0.299, 0.587, 0.114));
	currSAD += dot(tex2Dfetch(sBackBuffer, coord + float2(0.5, -0.5)).rgb, float3(0.299, 0.587, 0.114));
	currSAD += dot(tex2Dfetch(sBackBuffer, coord + float2(0.5, 0.5)).rgb, float3(0.299, 0.587, 0.114));
	currSAD /= 4;*/
}

float2 MotionVector(const bool firstPass, float2 texcoord, const float2 bufferSize, const sampler sPreviousVector, const int mipLevel, const float centerBias)
{
	float2 motionVectorPrev = 0;
	if(!firstPass)
	{
		//motionVectorPrev = (tex2Dfetch(sPreviousVector, floor((texcoord * bufferSize) / 2)).xy - 0.5) * 255.5;
		//motionVectorPrev = (BSplineBicubicFilter(sPreviousVector, texcoord).xy - 0.5) * 255.5;
		motionVectorPrev = ((tex2D(sPreviousVector, /*floor((texcoord * bufferSize) / 2)/ (bufferSize / 2)*/texcoord).xy - 0.5)) * (256 / bufferSize);
	}
	float2 coord = floor(texcoord * bufferSize);
	float currReference;
	float prevReference = 0;
	float reprojectedCenter = 0;
	currReference = tex2Dlod(sCurrSAD, float4(texcoord, 0, mipLevel)).x;
	if(!firstPass)
	{
		prevReference = tex2Dlod(sPrevSAD, float4(texcoord, 0, mipLevel)).x;
		reprojectedCenter = tex2Dlod(sPrevSAD, float4(texcoord + (motionVectorPrev), 0, mipLevel)).x;
	}
	
	//check the validity of the previous motion vector estimate
	if(abs(prevReference - currReference) < abs(reprojectedCenter - currReference))
	{
		motionVectorPrev = 0;
	}
	
	texcoord += motionVectorPrev;
	float minimum = 2;
	float2 motionVector;
	
	[unroll]
	for(int i = -1; i <= 1; i++)
	{
		[unroll]
		for(int j = -1; j <= 1; j++)
		{
			//float2 sampleCoord = texcoord + (float2(i, j) / bufferSize);
			float2 offset = (float2(i, j) / bufferSize);
			if((all((texcoord + offset) >= 0) || all((texcoord + offset) < 1)))
			{
				float compare;
				//if(mipLevel != -1)
				{
					compare = abs(tex2Dlod(sPrevSAD, float4((texcoord + offset), 0, mipLevel)).x - currReference);
				}
				/*else
				{
					compare = abs(BSplineBicubicFilter(sPrevSAD, sampleCoord).x - currReference.x);//abs(tex2Dlod(sPrevSAD, float4(sampleCoord, 0, mipLevel)).x - currReference);
				}*/
				if(all(int2(i, j) == 0))
				{
					compare -= centerBias;
				}
				if(compare < minimum)
				{
					motionVector = float2(i, j) / (256);
					minimum = compare;
				}
			}			
		}	
	}
	return (motionVector + motionVectorPrev * (bufferSize / 256)) + 0.5;
}


void MotionVectorPS0(float4 pos : SV_POSITION, float2 texcoord : TEXCOORD, out float2 motionVector : SV_TARGET0)
{
	float2 bufferSize = BUFFER_SIZE0;
	uint mipLevel = 5;
	float centerBias = (Threshold * exp2(mipLevel))  / (10);
	motionVector = MotionVector(true, texcoord, bufferSize, sBackBuffer, mipLevel, centerBias);
}

void MotionVectorPS1(float4 pos : SV_POSITION, float2 texcoord : TEXCOORD, out float2 motionVector : SV_TARGET0)
{
	float2 bufferSize = BUFFER_SIZE1;
	uint mipLevel = 4;
	float centerBias = (Threshold * exp2(mipLevel))  / (10);
	motionVector = MotionVector(false, texcoord, bufferSize, sMotion0, mipLevel, centerBias);
}

void MotionVectorPS2(float4 pos : SV_POSITION, float2 texcoord : TEXCOORD, out float2 motionVector : SV_TARGET0)
{
	float2 bufferSize = BUFFER_SIZE2;
	uint mipLevel = 3;
	float centerBias = (Threshold * exp2(mipLevel))  / (10);
	motionVector = MotionVector(false, texcoord, bufferSize, sMotion1, mipLevel, centerBias);
}

void MotionVectorPS3(float4 pos : SV_POSITION, float2 texcoord : TEXCOORD, out float2 motionVector : SV_TARGET0)
{
	float2 bufferSize = BUFFER_SIZE3;
	uint mipLevel = 2;
	float centerBias = (Threshold * exp2(mipLevel))  / (10);
	motionVector = MotionVector(false, texcoord, bufferSize, sMotion2, mipLevel, centerBias);
}

void MotionVectorPS4(float4 pos : SV_POSITION, float2 texcoord : TEXCOORD, out float2 motionVector : SV_TARGET0)
{
	float2 bufferSize = BUFFER_SIZE4;
	uint mipLevel = 1;
	float centerBias = (Threshold * exp2(mipLevel))  / (10);
	motionVector = MotionVector(false, texcoord, bufferSize, sMotion3, mipLevel, centerBias);
}

void MotionVectorPS5(float4 pos : SV_POSITION, float2 texcoord : TEXCOORD, out float2 motionVector : SV_TARGET0)
{
	float2 bufferSize = BUFFER_SIZE5;
	uint mipLevel = 0;
	float centerBias = (Threshold * exp2(mipLevel))  / (10);
	motionVector = MotionVector(false, texcoord, bufferSize, sMotion4, mipLevel, centerBias);
}

#if OPTICAL_FLOW_SUBPIXEL != 0
void MotionVectorPS6(float4 pos : SV_POSITION, float2 texcoord : TEXCOORD, out float2 motionVector : SV_TARGET0)
{
	float2 bufferSize = BUFFER_SIZE5 * 2;
	int mipLevel = -1;
	float centerBias = (Threshold * exp2(mipLevel))  / (10);
	motionVector = MotionVector(false, texcoord, bufferSize, sMotion5, mipLevel, centerBias);
}
#endif

technique OpticalFlow
{
	/*pass
	{
		VertexShader = PostProcessVS;
		PixelShader = CurrToPrevPS;
		RenderTarget0 = PrevSAD;
	}*/
	
	pass
	{
		VertexShader = PostProcessVS;
		PixelShader = PreparationPS;
		RenderTarget0 = CurrSAD;
		RenderTarget1 = PrevSAD;
	}
	
	pass
	{
		VertexShader = PostProcessVS;
		PixelShader = MotionVectorPS0;
		RenderTarget0 = Motion0;
	}
	
	pass
	{
		VertexShader = PostProcessVS;
		PixelShader = MotionVectorPS1;
		RenderTarget0 = Motion1;
	}
	
	pass
	{
		VertexShader = PostProcessVS;
		PixelShader = MotionVectorPS2;
		RenderTarget0 = Motion2;
	}
	
	pass
	{
		VertexShader = PostProcessVS;
		PixelShader = MotionVectorPS3;
		RenderTarget0 = Motion3;
	}
	
	pass
	{
		VertexShader = PostProcessVS;
		PixelShader = MotionVectorPS4;
		RenderTarget0 = Motion4;
	}
	
	pass
	{
		VertexShader = PostProcessVS;
		PixelShader = MotionVectorPS5;
		RenderTarget0 = Motion5;
	}

#if OPTICAL_FLOW_SUBPIXEL != 0	
	pass
	{
		VertexShader = PostProcessVS;
		PixelShader = MotionVectorPS6;
		RenderTarget0 = Motion6;
	}
#endif
	
	pass
	{
		VertexShader = PostProcessVS;
		PixelShader = PrevToCurrPS;
		RenderTarget0 = Output;
		RenderTarget1 = PrevBackBuffer;
	}
	
	pass
	{
		VertexShader = PostProcessVS;
		PixelShader = OutputPS;
	}
	
}
}
