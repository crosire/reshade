//--------------------------------------------------------------------------------------
// Copyright 2014 Intel Corporation
// All Rights Reserved
//
// Permission is granted to use, copy, distribute and prepare derivative works of this
// software for any purpose and without fee, provided, that the above copyright notice
// and this statement appear in all copies.  Intel makes no representations about the
// suitability of this software for any purpose.  THIS SOFTWARE IS PROVIDED "AS IS."
// INTEL SPECIFICALLY DISCLAIMS ALL WARRANTIES, EXPRESS OR IMPLIED, AND ALL LIABILITY,
// INCLUDING CONSEQUENTIAL AND OTHER INDIRECT DAMAGES, FOR THE USE OF THIS SOFTWARE,
// INCLUDING LIABILITY FOR INFRINGEMENT OF ANY PROPRIETARY RIGHTS, AND INCLUDING THE
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.  Intel does not
// assume any responsibility for any errors which may appear in this software nor any
// responsibility to update it.
//--------------------------------------------------------------------------------------

// BUTTERFLY_COUNT: number of passes to perform
// ROWPASS: defined for tranformation along the x axis
// LENGTH: pixel length of row or column
// BUTTERFLY_LUT: Defined if butterfly lookup texture should be used

/*
This is an implementation of Intel's Cooley-Tukey based FFT ported to ReShade

Ported By: Lord of Lunacy
*/

#ifndef IMPORT_IMAGE
	#define IMPORT_IMAGE 0
#endif

#ifndef USE_LUMA_IMAGE
	#define USE_LUMA_IMAGE 1
#endif

#ifndef IMAGE_FILENAME
	#define IMAGE_FILENAME "image.png"
#endif

#ifndef BUTTERFLY_LUT
	#define BUTTERFLY_LUT 0
#endif

#ifndef BUTTERFLY_COUNT
	#define BUTTERFLY_COUNT 8
#endif

//Prevent too large of a workgroup from being created
#if BUTTERFLY_COUNT > 10
	#undef BUTTERFLY_COUNT
	#define BUTTERFLY_COUNT 10
#endif

#define LENGTH (1 << BUTTERFLY_COUNT)

#define NVIDIA 0x10DE
#define AMD 0x1002

//Makes sure that each workgroup is full
#if __VENDOR__ == NVIDIA
	#define THRESHOLD 4
	#define MIN_WORKGROUP_SIZE 32
#else
	#define THRESHOLD 5
	#define MIN_WORKGROUP_SIZE 64
#endif

#if BUTTERFLY_COUNT < THRESHOLD
	#define THREADS MIN_WORKGROUP_SIZE
#else
	#define THREADS LENGTH
#endif

#define TILES_PER_WORKGROUP (THREADS / LENGTH)

#define DISPATCHSIZE uint2((BUFFER_WIDTH / LENGTH) + 1, (BUFFER_HEIGHT / LENGTH) + 1)
#define DISPATCH_RES uint2(DISPATCHSIZE * LENGTH)

//Preserve memory bandwidth by using half-precision or a smaller texture format when possible
#if USE_LUMA_IMAGE != 0
	#if BUTTERFLY_COUNT <= 8
		#define FORMAT R16f
	#else
		#define FORMAT R32f
	#endif
#else
	#if BUTTERFLY_COUNT <= 8
		#define FORMAT RGBA16f
	#else
		#define FORMAT RGBA32f
	#endif
#endif

//Use a LUT for weights and indices if desired
#if BUTTERFLY_LUT != 0
	texture ButterflyLUT {Width = LENGTH; Height = BUTTERFLY_COUNT; Format = RGBA32f;};
	sampler sButterflyLUT {Texture = ButterflyLUT;};
	storage wButterflyLUT {Texture = ButterflyLUT;};
#endif

#if IMPORT_IMAGE == 0
texture SourceImage : COLOR;
#else
texture SourceImage<source = IMAGE_FILENAME;>{Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = RGBA8;};
#endif
texture RowFFTReal {Width = DISPATCH_RES.x; Height = DISPATCH_RES.y; Format = FORMAT;};
texture RowFFTImaginary {Width = DISPATCH_RES.x; Height = DISPATCH_RES.y; Format = FORMAT;};
texture FFTReal {Width = DISPATCH_RES.x; Height = DISPATCH_RES.y; Format = FORMAT;};
texture FFTImaginary {Width = DISPATCH_RES.x; Height = DISPATCH_RES.y; Format = FORMAT;};
texture InverseFFT {Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = FORMAT;};

sampler sSourceImage {Texture = SourceImage;};
sampler sRowFFTReal {Texture = RowFFTReal;};
sampler sRowFFTImaginary {Texture = RowFFTImaginary;};
sampler sFFTReal {Texture = FFTReal;};
sampler sFFTImaginary {Texture = FFTImaginary;};
sampler sInverseFFT {Texture = InverseFFT;};

storage wRowFFTReal {Texture = RowFFTReal;};
storage wRowFFTImaginary {Texture = RowFFTImaginary;};
storage wFFTReal {Texture = FFTReal;};
storage wFFTImaginary {Texture = FFTImaginary;};
storage wInverseFFT {Texture = InverseFFT;};

uniform int Debug<
	ui_type = "combo";
	ui_category = "Debug";
	ui_label = "Debug";
	ui_items = "Inverse FFT \0FFT Real Numbers \0FFT Imaginary Numbers \0"
			   "Log-Magnitude \0Phase \0Input Image \0";
> = 0;


static const float PI = 3.14159265f;

uint2 ReverseBits(uint2 x)
{
	x = (((x & 0xaaaaaaaa) >> 1) | ((x & 0x55555555) << 1));
	x = (((x & 0xcccccccc) >> 2) | ((x & 0x33333333) << 2));
	x = (((x & 0xf0f0f0f0) >> 4) | ((x & 0x0f0f0f0f) << 4));
	x = (((x & 0xff00ff00) >> 8) | ((x & 0x00ff00ff) << 8));
	return ((x >> 16) | (x << 16));
}

void GetButterflyValues(uint passIndex, uint x, out uint2 indices, out float2 weights)
{
	uint sectionWidth = 2 << passIndex;
	uint halfSectionWidth = sectionWidth / 2;

	uint sectionStartOffset = x & ~(sectionWidth - 1);
	uint halfSectionOffset = x & (halfSectionWidth - 1);
	uint sectionOffset = x & (sectionWidth - 1);

	sincos( 2.0*PI*sectionOffset / (float)sectionWidth, weights.y, weights.x );
	weights.y = -weights.y;

	indices.x = sectionStartOffset + halfSectionOffset;
	indices.y = sectionStartOffset + halfSectionOffset + halfSectionWidth;

	if (passIndex == 0)
	{
		indices = ReverseBits(indices) >> (32 - BUTTERFLY_COUNT) & (LENGTH - 1);
	}
}

void PostProcessVS(in uint id : SV_VertexID, out float4 position : SV_Position, out float2 texcoord : Texcoord)
{
	texcoord.x = (id == 2) ? 2.0 : 0.0;
	texcoord.y = (id == 1) ? 2.0 : 0.0;
	position = float4(texcoord * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
}

#if USE_LUMA_IMAGE != 0
void OutputPS(float4 pos : SV_Position, float2 texcoord : Texcoord, out float4 color : SV_Target)
{
	uint2 coord = texcoord.xy * float2(BUFFER_WIDTH, BUFFER_HEIGHT);
	if(Debug == 0)
	{
		color = tex2Dfetch(sInverseFFT, float4(coord, 0, 0)).xxxx;
	}
	else if (Debug == 1)
	{
		color = tex2Dfetch(sFFTReal, float4(coord, 0, 0)).xxxx;
	}
	else if (Debug == 2)
	{
		color = tex2Dfetch(sFFTImaginary, float4(coord, 0, 0)).xxxx;
	}
	else if (Debug == 3)
	{
		float4 a = tex2Dfetch(sFFTReal, float4(coord, 0, 0)).xxxx;
		float4 b = tex2Dfetch(sFFTImaginary, float4(coord, 0, 0)).xxxx;
		color = log10(sqrt((a * a) + (b * b)));
	}
	else if (Debug == 4)
	{
		float4 a = tex2Dfetch(sFFTReal, float4(coord, 0, 0)).xxxx;
		float4 b = tex2Dfetch(sFFTImaginary, float4(coord, 0, 0)).xxxx;
		color = atan2(b, a);
	}
	else
	{
		discard;
		//color = tex2Dfetch(sSourceImage, float4(coord, 0, 0));
	}
}
#else
void OutputPS(float4 pos : SV_Position, float2 texcoord : Texcoord, out float4 color : SV_Target)
{
	uint2 coord = texcoord.xy * float2(BUFFER_WIDTH, BUFFER_HEIGHT);
	if(Debug == 0)
	{
		color = tex2Dfetch(sInverseFFT, float4(coord, 0, 0));
	}
	else if (Debug == 1)
	{
		color = tex2Dfetch(sFFTReal, float4(coord, 0, 0));
	}
	else if (Debug == 2)
	{
		color = tex2Dfetch(sFFTImaginary, float4(coord, 0, 0));
	}
	else if (Debug == 3)
	{
		float4 a = tex2Dfetch(sFFTReal, float4(coord, 0, 0));
		float4 b = tex2Dfetch(sFFTImaginary, float4(coord, 0, 0));
		color = log10(sqrt((a * a) + (b * b)));
	}
	else if (Debug == 4)
	{
		float4 a = tex2Dfetch(sFFTReal, float4(coord, 0, 0));
		float4 b = tex2Dfetch(sFFTImaginary, float4(coord, 0, 0));
		color = atan2(b, a);
	}
	else
	{
		discard;
	}
}
#endif

#if BUTTERFLY_LUT != 0
void ButterflyLUTCS(uint3 id : SV_DispatchThreadID, uint3 tid : SV_GroupThreadID)
{
	uint passIndex = id.y;
	uint x = id.x;
	uint2 indices;
	float2 weights;
	GetButterflyValues(passIndex, x, indices, weights);
	tex2Dstore(wButterflyLUT, id.xy, float4(indices, weights));
}
#endif

#if USE_LUMA_IMAGE != 0
groupshared float pingPongArray[4 * THREADS];
void ButterflyPass(int passIndex, uint x, uint y, uint t0, uint t1, out float resultR, out float resultI, bool transformInverse)
{
	uint2 Indices;
	float2 Weights;
#if BUTTERFLY_LUT != 0
	float4 IndicesAndWeights = tex2Dfetch(sButterflyLUT, float4(uint2(x, passIndex), 0, 0));
	Indices = IndicesAndWeights.xy;
	Weights = IndicesAndWeights.zw;
#else
	GetButterflyValues(passIndex, x, Indices, Weights);
#endif
	uint pingPongIndexR1 = (Indices.x * 4) + t0 + (y * LENGTH * 4);
	uint pingPongIndexI1 = (Indices.x * 4) + t1 + (y * LENGTH * 4);
	uint pingPongIndexR2 = (Indices.y * 4) + t0 + (y * LENGTH * 4);
	uint pingPongIndexI2 = (Indices.y * 4) + t1 + (y * LENGTH * 4);

	float inputR1 = pingPongArray[pingPongIndexR1];
	float inputI1 = pingPongArray[pingPongIndexI1];

	float inputR2 = pingPongArray[pingPongIndexR2];
	float inputI2 = pingPongArray[pingPongIndexI2];

	if(transformInverse)
	{
		resultR = (inputR1 + Weights.x * inputR2 + Weights.y * inputI2) * 0.5;
		resultI = (inputI1 - Weights.y * inputR2 + Weights.x * inputI2) * 0.5;
	}
	else
	{
		resultR = inputR1 + Weights.x * inputR2 - Weights.y * inputI2;
		resultI = inputI1 + Weights.y * inputR2 + Weights.x * inputI2;
	}
}

void ButterflyPassFinalNoI(int passIndex, int x, int y, int t0, int t1, out float3 resultR)
{
	uint2 Indices;
	float2 Weights;
	GetButterflyValues(passIndex, x, Indices, Weights);

	float3 inputR1 = pingPongArray[t0 + (Indices.x * 4)+ (y * LENGTH * 4)];

	float3 inputR2 = pingPongArray[t0 + (Indices.y * 4)+ (y * LENGTH * 4)];
	float3 inputI2 = pingPongArray[t1 + (Indices.y * 4)+ (y * LENGTH * 4)];

	resultR = (inputR1 + Weights.x * inputR2 + Weights.y * inputI2) * 0.5;
}



void ButterflySLM(uint3 id : SV_DispatchThreadID, uint3 position : SV_GroupThreadID, bool rowPass, bool transformInverse, sampler sReal, sampler sImaginary, storage wReal, storage wImaginary)
{
	uint2 texturePos;
	uint row = position.y;
	if(rowPass)
	{
		texturePos = uint2( id.xy );
	}
	else
	{
		texturePos = uint2( id.yx );
	}

	// Load entire row or column into scratch array
	if(rowPass && !transformInverse)
	{
		pingPongArray[0 + (position.x * 4) + (position.y * LENGTH * 4)].x = dot(tex2Dfetch(sReal, float4(texturePos, 0, 0)).rgb, float3(0.299, 0.587, 0.114));
		// don't load values from the imaginary texture when loading the original texture
		pingPongArray[1 + (position.x * 4) + (position.y * LENGTH * 4)].x = 0;
	}
	else
	{
		pingPongArray[0 + (position.x * 4) + (position.y * LENGTH * 4)].x = tex2Dfetch(sReal, float4(texturePos, 0, 0)).x;
		pingPongArray[1 + (position.x * 4) + (position.y * LENGTH * 4)].x = tex2Dfetch(sImaginary, float4(texturePos, 0, 0)).x;
	}
	
	uint4 textureIndices = uint4(0, 1, 2, 3);

	
	for (int i = 0; i < BUTTERFLY_COUNT - 1; i++)
	{
		barrier();
		ButterflyPass( i, position.x, position.y, textureIndices.x, textureIndices.y,
			pingPongArray[textureIndices.z + (position.x * 4) + (position.y * LENGTH * 4)].x,
			pingPongArray[textureIndices.w + (position.x * 4) + (position.y * LENGTH * 4)].x,
			transformInverse);
		textureIndices.xyzw = textureIndices.zwxy;
	}

	// Final butterfly will write directly to the target texture
	barrier();
	float3 real;
	float3 imaginary;

	// The final pass writes to the output UAV texture
	if(!rowPass && transformInverse)
	{
		// last pass of the inverse transform. The imaginary value is no longer needed
		ButterflyPassFinalNoI(BUTTERFLY_COUNT - 1, position.x, position.y, textureIndices.x, textureIndices.y, real);
		tex2Dstore(wReal, texturePos, float4(real.xxx, 1));
	}
	else
	{
		ButterflyPass(BUTTERFLY_COUNT - 1, position.x, position.y, textureIndices.x, textureIndices.y, real, imaginary, transformInverse);
		tex2Dstore(wReal, texturePos, float4(real.xxx, 1));
		tex2Dstore(wImaginary, texturePos, float4(imaginary.xxx, 1));
	}

}
#else
groupshared float3 pingPongArray[4 * THREADS];
void ButterflyPass(int passIndex, uint x, uint y, uint t0, uint t1, out float3 resultR, out float3 resultI, bool transformInverse)
{
	uint2 Indices;
	float2 Weights;
#if BUTTERFLY_LUT != 0
	float4 IndicesAndWeights = tex2Dfetch(sButterflyLUT, float4(uint2(x, passIndex), 0, 0));
	Indices = IndicesAndWeights.xy;
	Weights = IndicesAndWeights.zw;
#else
	GetButterflyValues(passIndex, x, Indices, Weights);
#endif
	uint pingPongIndexR1 = (Indices.x * 4) + t0 + (y * LENGTH * 4);
	uint pingPongIndexI1 = (Indices.x * 4) + t1 + (y * LENGTH * 4);
	uint pingPongIndexR2 = (Indices.y * 4) + t0 + (y * LENGTH * 4);
	uint pingPongIndexI2 = (Indices.y * 4) + t1 + (y * LENGTH * 4);

	float3 inputR1 = pingPongArray[pingPongIndexR1];
	float3 inputI1 = pingPongArray[pingPongIndexI1];

	float3 inputR2 = pingPongArray[pingPongIndexR2];
	float3 inputI2 = pingPongArray[pingPongIndexI2];

	if(transformInverse)
	{
		resultR = (inputR1 + Weights.x * inputR2 + Weights.y * inputI2) * 0.5;
		resultI = (inputI1 - Weights.y * inputR2 + Weights.x * inputI2) * 0.5;
	}
	else
	{
		resultR = inputR1 + Weights.x * inputR2 - Weights.y * inputI2;
		resultI = inputI1 + Weights.y * inputR2 + Weights.x * inputI2;
	}
}

void ButterflyPassFinalNoI(int passIndex, int x, int y, int t0, int t1, out float3 resultR)
{
	uint2 Indices;
	float2 Weights;
	GetButterflyValues(passIndex, x, Indices, Weights);

	float3 inputR1 = pingPongArray[t0 + (Indices.x * 4)+ (y * LENGTH * 4)];

	float3 inputR2 = pingPongArray[t0 + (Indices.y * 4)+ (y * LENGTH * 4)];
	float3 inputI2 = pingPongArray[t1 + (Indices.y * 4)+ (y * LENGTH * 4)];

	resultR = (inputR1 + Weights.x * inputR2 + Weights.y * inputI2) * 0.5;
}



void ButterflySLM(uint3 id : SV_DispatchThreadID, uint3 position : SV_GroupThreadID, bool rowPass, bool transformInverse, sampler sReal, sampler sImaginary, storage wReal, storage wImaginary)
{
	uint2 texturePos;
	uint row = position.y;
	if(rowPass)
	{
		texturePos = uint2( id.xy );
	}
	else
	{
		texturePos = uint2( id.yx );
	}

	// Load entire row or column into scratch array
	pingPongArray[0 + (position.x * 4) + (position.y * LENGTH * 4)].xyz = tex2Dfetch(sReal, float4(texturePos, 0, 0)).rgb;
	if(rowPass && !transformInverse)
	{
		// don't load values from the imaginary texture when loading the original texture
		pingPongArray[1 + (position.x * 4) + (position.y * LENGTH * 4)].xyz = 0;
	}
	else
		pingPongArray[1 + (position.x * 4) + (position.y * LENGTH * 4)].xyz = tex2Dfetch(sImaginary, float4(texturePos, 0, 0)).rgb;
	
	uint4 textureIndices = uint4(0, 1, 2, 3);

	
	for (int i = 0; i < BUTTERFLY_COUNT - 1; i++)
	{
		barrier();
		ButterflyPass( i, position.x, position.y, textureIndices.x, textureIndices.y,
			pingPongArray[textureIndices.z + (position.x * 4) + (position.y * LENGTH * 4)].xyz,
			pingPongArray[textureIndices.w + (position.x * 4) + (position.y * LENGTH * 4)].xyz,
			transformInverse);
		textureIndices.xyzw = textureIndices.zwxy;
	}

	// Final butterfly will write directly to the target texture
	barrier();
	float3 real;
	float3 imaginary;

	// The final pass writes to the output UAV texture
	if(!rowPass && transformInverse)
	{
		// last pass of the inverse transform. The imaginary value is no longer needed
		ButterflyPassFinalNoI(BUTTERFLY_COUNT - 1, position.x, position.y, textureIndices.x, textureIndices.y, real);
		tex2Dstore(wReal, texturePos, float4(real, 1));
	}
	else
	{
		ButterflyPass(BUTTERFLY_COUNT - 1, position.x, position.y, textureIndices.x, textureIndices.y, real, imaginary, transformInverse);
		tex2Dstore(wReal, texturePos, float4(real, 1));
		tex2Dstore(wImaginary, texturePos, float4(imaginary, 1));
	}

}
#endif

void RowCS(uint3 id : SV_DispatchThreadID, uint3 position : SV_GroupThreadID)
{
	ButterflySLM(id, position, 1, 0, sSourceImage, sSourceImage, wRowFFTReal, wRowFFTImaginary);
}

void ColumnCS(uint3 id : SV_DispatchThreadID, uint3 position : SV_GroupThreadID)
{
	ButterflySLM(id, position, 0, 0, sRowFFTReal, sRowFFTImaginary, wFFTReal, wFFTImaginary);
}

void RowInverseCS(uint3 id : SV_DispatchThreadID, uint3 position : SV_GroupThreadID)
{
	ButterflySLM(id, position, 1, 1, sFFTReal, sFFTImaginary, wRowFFTReal, wRowFFTImaginary);
}

void ColumnInverseCS(uint3 id : SV_DispatchThreadID, uint3 position : SV_GroupThreadID)
{
	ButterflySLM(id, position, 0, 1, sRowFFTReal, sRowFFTImaginary, wInverseFFT, wInverseFFT);
}


#if BUTTERFLY_LUT != 0
technique ButterflyLUT<enabled = true; hidden = true; timeout = 1;>
{
	pass
	{
		ComputeShader = ButterflyLUTCS<LENGTH, 1>;
		DispatchSizeX = 1;
		DispatchSizeY = BUTTERFLY_COUNT;
	}
}
#endif

technique FFTCompute
{
	pass
	{
		ComputeShader = RowCS<LENGTH, TILES_PER_WORKGROUP>;
		DispatchSizeX = DISPATCHSIZE.x;
		DispatchSizeY = (DISPATCH_RES.y / TILES_PER_WORKGROUP);
	}
	
	pass
	{
		ComputeShader = ColumnCS<LENGTH, TILES_PER_WORKGROUP>;
		DispatchSizeX = DISPATCHSIZE.y;
		DispatchSizeY = (DISPATCH_RES.x / TILES_PER_WORKGROUP);
	}
	
	pass
	{
		ComputeShader = RowInverseCS<LENGTH, TILES_PER_WORKGROUP>;
		DispatchSizeX = DISPATCHSIZE.x;
		DispatchSizeY = (DISPATCH_RES.y / TILES_PER_WORKGROUP);
	}
	
	pass
	{
		ComputeShader = ColumnInverseCS<LENGTH, TILES_PER_WORKGROUP>;
		DispatchSizeX = DISPATCHSIZE.y;
		DispatchSizeY = (DISPATCH_RES.x / TILES_PER_WORKGROUP);
	}
	
	pass
	{
		VertexShader = PostProcessVS;
		PixelShader = OutputPS;
	}
}
