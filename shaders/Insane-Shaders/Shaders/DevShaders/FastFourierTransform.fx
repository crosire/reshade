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

/*
UAV Fast Fourier Transform ReShade Port

By: Lord Of Lunacy


This is my attempt at porting a FFT technique developed by Intel over to Reshade,
that uses unordered access views (UAV).

More details about it can be found here:

https://software.intel.com/content/www/us/en/develop/articles/implementation-of-fast-fourier-transform-for-image-processing-in-directx-10.html


Fast fourier transform is an optimized approach to applying discrete fourier transform,
and is used to convert images from the spatial domain to the frequency domain.
*/



#if (__RENDERER__ == 0x9000)
#define D3D9
#endif

#ifndef BUTTERFLY_COUNT
#define BUTTERFLY_COUNT 8
#endif

#define NEXTPASS (BUTTERFLY_COUNT)

#if (BUTTERFLY_COUNT >= 1)
#define FFTONE
#endif

#if (BUTTERFLY_COUNT >= 2)
#define FFTTWO
#endif

#if (BUTTERFLY_COUNT >= 3)
#define FFTTHREE
#endif

#if (BUTTERFLY_COUNT >= 4)
#define FFTFOUR
#endif

#if (BUTTERFLY_COUNT >= 5)
#define FFTFIVE
#endif

#if (BUTTERFLY_COUNT >= 6)
#define FFTSIX
#endif

#if (BUTTERFLY_COUNT >= 7)
#define FFTSEVEN
#endif

#if (BUTTERFLY_COUNT >= 8)
#define FFTEIGHT
#endif

#if (BUTTERFLY_COUNT >= 9)
#define FFTNINE
#endif

#if (BUTTERFLY_COUNT >= 10)
#define FFTTEN
#endif

#if (BUTTERFLY_COUNT >= 11)
#define FFTELEVEN
#endif

#if (BUTTERFLY_COUNT >= 12)
#define FFTTWELVE
#endif

#if (BUTTERFLY_COUNT >= 13)
#define FFTTHIRTEEN
#endif


#include "ReShade.fxh"
#define PI 3.141592654
#define SUBBLOCK_SIZE (1 << (BUTTERFLY_COUNT)) //Subblocks are size 2^(BUTTERFLY_COUNT)
#define BLOCK_COUNT (uint2(BUFFER_SCREEN_SIZE / SUBBLOCK_SIZE) + uint2(1, 1))
#define TEXTURE_SIZE (uint2(BLOCK_COUNT * SUBBLOCK_SIZE))
#define ODD (BUTTERFLY_COUNT % 2)
#define PRECISION BUTTERFLY_COUNT

#ifndef D3D9
	uniform bool UseLUT
	<
		ui_label = "Use The LUT for Butterfly Values";
		ui_tooltip = "Disabling this can improve performance in some scenarios by reducing demand on\n"
					"VRAM and transferring the load to the GPU for all operations except for the first\n"
					"pass in any direction, which is more computationally expensive in realtime.";
	> = 1;
#endif

uniform bool ApplyFilter
	<
		ui_label = "Apply FIlter";
		ui_tooltip = "As a demonstration for using FFT to apply convolutions a lowpass Butterworth \n"
					"filter was created to be applied to the frequency domain and modify the output.";
	> = 0;
	
uniform float FilterStrength
<
	ui_label = "FilterStrength";
	ui_type = "slider";
	ui_min = 0; ui_max = 1;
> = 0.075;

//For more than 8 butterfly passes (SUBBLOCK_SIZE of 256x256) 32 bit precision is required
#ifdef FFTNINE	
texture ButterflyTable{Width = SUBBLOCK_SIZE; Height = BUTTERFLY_COUNT; Format = RGBA32F;};
sampler sButterflyTable{Texture = ButterflyTable;};
texture Source{Width = TEXTURE_SIZE.x; Height = TEXTURE_SIZE.y; Format = RG32F;};
sampler sSource{Texture = Source;};
texture Source1{Width = TEXTURE_SIZE.x; Height = TEXTURE_SIZE.y; Format = RG32F;};
sampler sSource1{Texture = Source1;};
texture FrequencyDomain{Width = TEXTURE_SIZE.x; Height = TEXTURE_SIZE.y; Format = RG32F;};
sampler sFrequencyDomain{Texture = FrequencyDomain;};

#else
texture ButterflyTable{Width = SUBBLOCK_SIZE; Height = BUTTERFLY_COUNT; Format = RGBA16F;};
sampler sButterflyTable{Texture = ButterflyTable;};
texture Source{Width = TEXTURE_SIZE.x; Height = TEXTURE_SIZE.y; Format = RG16F;};
sampler sSource{Texture = Source;};
texture Source1{Width = TEXTURE_SIZE.x; Height = TEXTURE_SIZE.y; Format = RG16F;};
sampler sSource1{Texture = Source1;};
texture FrequencyDomain{Width = TEXTURE_SIZE.x; Height = TEXTURE_SIZE.y; Format = RG16F;};
sampler sFrequencyDomain{Texture = FrequencyDomain;};

#endif

texture FFTOutput{Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = R8;};
sampler sFFTOutput{Texture = FFTOutput;};
texture FFTInput{Width = TEXTURE_SIZE.x; Height = TEXTURE_SIZE.y; Format = RG8;};
sampler sFFTInput{Texture = FFTInput;};

uint BitwiseAnd(uint a, uint b)
{
	bool c = a;
	bool d = b;
	uint output;
	for(int i = 0; i < PRECISION; i++)
	{
		c = a % 2;
		d = b % 2;
		output += exp2(i) * c * d;
		a /= 2;
		b /= 2;
	}
	return output;
}

uint BitwiseAndNot(uint a, uint b)
{
	bool c = a;
	bool d = b;
	uint output;
	for(int i = 0; i < PRECISION; i++)
	{
		c = a % 2;
		d = 1 - (b % 2);
		output += exp2(i) * c * d;
		a /= 2;
		b /= 2;
	}
	return output;
}

uint LeftShift(uint a, uint b)
{
	return a * exp2(b);
}

uint RightShift(uint a, uint b)
{
	return a / exp2(b);
}

uint2 SubBlockCorner(float2 texcoord)
{
	uint x = int(texcoord.x * TEXTURE_SIZE.x) / SUBBLOCK_SIZE;
	uint y = int(texcoord.y * TEXTURE_SIZE.y) / SUBBLOCK_SIZE;
	x *= SUBBLOCK_SIZE;
	y *= SUBBLOCK_SIZE;
	return uint2(x, y);
}

uint2 closestSubBlockCorner(float2 texcoord)
{
	float2 subBlockUV = float2(SUBBLOCK_SIZE.xx) / float2(TEXTURE_SIZE);
	float2 coordinate = texcoord + 0.5 * subBlockUV;
	return SubBlockCorner(coordinate);
}

uint2 SubBlockPosition(float2 texcoord)
{
	uint x = uint(texcoord.x * TEXTURE_SIZE.x);
	uint y = uint(texcoord.y * TEXTURE_SIZE.y);
	uint z = x / SUBBLOCK_SIZE;
	uint w = y / SUBBLOCK_SIZE;
	x = x - z * SUBBLOCK_SIZE;
	y = y - w * SUBBLOCK_SIZE;
	return uint2(x, y);
}

uint2 SubBlockCenter(float2 texcoord)
{
	uint2 x = SubBlockCorner(texcoord);
	uint2 y = SUBBLOCK_SIZE.xx * 0.5;
	return y;
}

float DistanceToClosestSubBlockCorner(float2 texcoord)
{
	float2 x = closestSubBlockCorner(texcoord);
	float2 y = texcoord * TEXTURE_SIZE;
	return distance(x, y);
}

float4 GetButterflyValues(uint passIndex, uint x)
{
#ifdef D3D9 //D3D9 does not support bitwise functions and must make use of slower appoximate operations
	int sectionWidth = LeftShift(2, passIndex);
	int halfSectionWidth = sectionWidth / 2;

	int sectionStartOffset = BitwiseAndNot(x, (sectionWidth - 1));
	int halfSectionOffset = BitwiseAnd(x, (halfSectionWidth - 1));
	int sectionOffset = BitwiseAnd(x, (sectionWidth - 1));
#else
	int sectionWidth = 2 << passIndex;
	int halfSectionWidth = sectionWidth / 2;

	int sectionStartOffset = x & ~(sectionWidth - 1);
	int halfSectionOffset = x & (halfSectionWidth - 1);
	int sectionOffset = x & (sectionWidth - 1);
#endif
	
	float2 weights;
	uint2 indices;

	sincos( 2.0*PI*sectionOffset / (float)sectionWidth, weights.y, weights.x );
	weights.y = -weights.y;

	indices.x = sectionStartOffset + halfSectionOffset;
	indices.y = sectionStartOffset + halfSectionOffset + halfSectionWidth;

	if (passIndex == 0)
	{
		uint2 a = indices;
		uint2 reverse = 0;
		for(int i = 1; i <= PRECISION; i++) //bit reversal
		{
			reverse += exp2(PRECISION - i) * (a % 2);
			a /= 2;
		}
	#ifdef D3D9
		indices.x = RightShift(reverse.x, BitwiseAnd((PRECISION - BUTTERFLY_COUNT), (SUBBLOCK_SIZE - 1)));
		indices.y = RightShift(reverse.y, BitwiseAnd((PRECISION - BUTTERFLY_COUNT), (SUBBLOCK_SIZE - 1)));
	#else
		indices = reverse >> (PRECISION - BUTTERFLY_COUNT) & (SUBBLOCK_SIZE -1);
	#endif
	}
	return float4(weights, indices);
}

float2 ButterflyPass(float2 texcoord, uint passIndex, bool rowpass, bool inverse, sampler sourceSampler)
{
	uint2 position = SubBlockPosition(texcoord).xy;
	uint2 imagePosition = texcoord * TEXTURE_SIZE;
	uint2 corner = SubBlockCorner(texcoord).xy;
	float2 output;
	uint textureSampleX;
	if(rowpass) textureSampleX = position.x;
	else textureSampleX = position.y;
	
	uint2 Indices;
	float2 Weights;
	float4 IndicesAndWeights;

#ifdef D3D9 //Real-time computation of butterfly values will always be too costly in D3D9 with the current implementation
	IndicesAndWeights = tex2Dfetch(sButterflyTable, float4(textureSampleX, passIndex, 0, 0));
#else
	if (passIndex == 0 || UseLUT) //The first pass requires more intensive calculations that should not be computed realtime
	{
		IndicesAndWeights = tex2Dfetch(sButterflyTable, float4(textureSampleX, passIndex, 0, 0));
	}
	else
	{
		IndicesAndWeights = GetButterflyValues(passIndex, textureSampleX);
	}
#endif
	Indices = IndicesAndWeights.zw;
	Weights = IndicesAndWeights.xy;
	

	float inputR1;
	float inputI1;
	float inputR2;
	float inputI2;

	if(rowpass)
	{
		Indices = Indices.xy + corner.xx;
		inputR1 = tex2Dfetch(sourceSampler, float4(Indices.x, imagePosition.y, 0, 0)).r;
		inputI1 = tex2Dfetch(sourceSampler, float4(Indices.x, imagePosition.y, 0, 0)).g;

		inputR2 = tex2Dfetch(sourceSampler, float4(Indices.y, imagePosition.y, 0, 0)).r;
		inputI2 = tex2Dfetch(sourceSampler, float4(Indices.y, imagePosition.y, 0, 0)).g;
	}
	else
	{
		Indices = Indices.xy + corner.yy;
		inputR1 = tex2Dfetch(sourceSampler, float4(imagePosition.x, Indices.x, 0, 0)).r;
		inputI1 = tex2Dfetch(sourceSampler, float4(imagePosition.x, Indices.x, 0, 0)).g;

		inputR2 = tex2Dfetch(sourceSampler, float4(imagePosition.x, Indices.y, 0, 0)).r;
		inputI2 = tex2Dfetch(sourceSampler, float4(imagePosition.x, Indices.y, 0, 0)).g;
	}

	if(inverse)
	{
		output.x = (inputR1 + Weights.x * inputR2 + Weights.y * inputI2) * 0.5;
		output.y = (inputI1 - Weights.y * inputR2 + Weights.x * inputI2) * 0.5;
	}
	else
	{
		output.x = inputR1 + Weights.x * inputR2 - Weights.y * inputI2;
		output.y = inputI1 + Weights.y * inputR2 + Weights.x * inputI2;
	}
	return output;
}

//You can test different convolutions by applying them within this block
float FrequencyDomainFilter(float2 texcoord)
{
	// This is code for an Ideal low-pass filter:
	//if (DistanceToClosestSubBlockCorner(texcoord) < (SUBBLOCK_SIZE * FilterStrength))
	//return 1;
	//else return 0;
	
	//Calculates the multiplication value needed for the Butterworth high-pass filter
	return (1.001 / (1 + 1.2* pow(FilterStrength * 20 / (DistanceToClosestSubBlockCorner(texcoord)), 4)));
}

void DoNothingPS(out float4 output : SV_Target)
{
	output = 0;
	discard;
}

void ButterflyTablePS(float4 pos : SV_Position, float2 texcoord : Texcoord, out float4 output : SV_Target)
{
	output = GetButterflyValues((texcoord.y * (BUTTERFLY_COUNT)), (texcoord.x * (SUBBLOCK_SIZE)));
}

void InputPS(float4 pos : SV_Position, float2 texcoord : Texcoord, out float2 output : SV_Target)
{
	float2 coordinate = texcoord * (TEXTURE_SIZE / BUFFER_SCREEN_SIZE);
	//mirrors the image to minimize artifacting when the texture size exceeds
	//image bounds, and filters are applied
	if(coordinate.x > 1) coordinate.x = 1 - (coordinate.x - 1);
	if(coordinate.y > 1) coordinate.y = 1 - (coordinate.y - 1);
	float3 color = tex2D(ReShade::BackBuffer, coordinate).rgb;
	output = float2(dot(color, float3(0.3333, 0.3333, 0.3333)), 0);
	if ((coordinate.x > 2)) output = float2(0, 0);
	if (coordinate.y > 2) output = float2(0, 0);
}


void MoveToFrequencyDomainPS(float4 pos : SV_Position, float2 texcoord : Texcoord, out float2 output : SV_Target)
{
	output = tex2D(sSource1, texcoord).rg;
}

void MoveToOutputPS(float4 pos : SV_Position, float2 texcoord : Texcoord, out float output : SV_Target)
{
	float2 coordinate = texcoord * (BUFFER_SCREEN_SIZE / TEXTURE_SIZE);
	output = tex2D(sSource1, coordinate).r;
}

void FFTH1PS(float4 pos : SV_Position, float2 texcoord : Texcoord, out float2 output : SV_Target)
{
	output = ButterflyPass(texcoord, 0, 1, 0, sFFTInput);
}

void FFTH2PS(float4 pos : SV_Position, float2 texcoord : Texcoord, out float2 output : SV_Target)
{
	output = ButterflyPass(texcoord, 1, 1, 0, sSource);
}

void FFTH3PS(float4 pos : SV_Position, float2 texcoord : Texcoord, out float2 output : SV_Target)
{
	output = ButterflyPass(texcoord, 2, 1, 0, sSource1);
}

void FFTH4PS(float4 pos : SV_Position, float2 texcoord : Texcoord, out float2 output : SV_Target)
{
	output = ButterflyPass(texcoord, 3, 1, 0, sSource);
}

void FFTH5PS(float4 pos : SV_Position, float2 texcoord : Texcoord, out float2 output : SV_Target)
{
	output = ButterflyPass(texcoord, 4, 1, 0, sSource1);
}

void FFTH6PS(float4 pos : SV_Position, float2 texcoord : Texcoord, out float2 output : SV_Target)
{
	output = ButterflyPass(texcoord, 5, 1, 0, sSource);
}
		
void FFTH7PS(float4 pos : SV_Position, float2 texcoord : Texcoord, out float2 output : SV_Target)
{
	output = ButterflyPass(texcoord, 6, 1, 0, sSource1);
}

void FFTH8PS(float4 pos : SV_Position, float2 texcoord : Texcoord, out float2 output : SV_Target)
{
	output = ButterflyPass(texcoord, 7, 1, 0, sSource);
}

void FFTH9PS(float4 pos : SV_Position, float2 texcoord : Texcoord, out float2 output : SV_Target)
{
	output = ButterflyPass(texcoord, 8, 1, 0, sSource1);
}

void FFTH10PS(float4 pos : SV_Position, float2 texcoord : Texcoord, out float2 output : SV_Target)
{
	output = ButterflyPass(texcoord, 9, 1, 0, sSource);
}

void FFTH11PS(float4 pos : SV_Position, float2 texcoord : Texcoord, out float2 output : SV_Target)
{
	output = ButterflyPass(texcoord, 10, 1, 0, sSource1);
}

void FFTH12PS(float4 pos : SV_Position, float2 texcoord : Texcoord, out float2 output : SV_Target)
{
	output = ButterflyPass(texcoord, 11, 1, 0, sSource);
}
		
void FFTH13PS(float4 pos : SV_Position, float2 texcoord : Texcoord, out float2 output : SV_Target)
{
	output = ButterflyPass(texcoord, 12, 1, 0, sSource1);
}



void FFTV1PS(float4 pos : SV_Position, float2 texcoord : Texcoord, out float2 output : SV_Target)
{
#if ODD
	output = ButterflyPass(texcoord, 0, 0, 0, sSource);
#else
	output = ButterflyPass(texcoord, 0, 0, 0, sSource1);
#endif
#ifndef FFTTWO
	if(ApplyFilter)
	{
		output *= FrequencyDomainFilter(texcoord);
	}
#endif
}

void FFTV2PS(float4 pos : SV_Position, float2 texcoord : Texcoord, out float2 output : SV_Target)
{
#if ODD
	output = ButterflyPass(texcoord, 1, 0, 0, sSource1);
#else
	output = ButterflyPass(texcoord, 1, 0, 0, sSource);
#endif
#ifndef FFTTHREE
	if(ApplyFilter)
	{
		output *= FrequencyDomainFilter(texcoord);
	}
#endif
}

void FFTV3PS(float4 pos : SV_Position, float2 texcoord : Texcoord, out float2 output : SV_Target)
{
#if ODD
	output = ButterflyPass(texcoord, 2, 0, 0, sSource);
#else
	output = ButterflyPass(texcoord, 2, 0, 0, sSource1);
#endif
#ifndef FFTFOUR
	if(ApplyFilter)
	{
		output *= FrequencyDomainFilter(texcoord);
	}
#endif
}

void FFTV4PS(float4 pos : SV_Position, float2 texcoord : Texcoord, out float2 output : SV_Target)
{
#if ODD
	output = ButterflyPass(texcoord, 3, 0, 0, sSource1);
#else
	output = ButterflyPass(texcoord, 3, 0, 0, sSource);
#endif
#ifndef FFTFIVE
	if(ApplyFilter)
	{
		output *= FrequencyDomainFilter(texcoord);
	}
#endif
}

void FFTV5PS(float4 pos : SV_Position, float2 texcoord : Texcoord, out float2 output : SV_Target)
{
#if ODD
	output = ButterflyPass(texcoord, 4, 0, 0, sSource);
#else
	output = ButterflyPass(texcoord, 4, 0, 0, sSource1);
#endif
#ifndef FFTSIX
	if(ApplyFilter)
	{
		output *= FrequencyDomainFilter(texcoord);
	}
#endif
}

void FFTV6PS(float4 pos : SV_Position, float2 texcoord : Texcoord, out float2 output : SV_Target)
{
#if ODD
	output = ButterflyPass(texcoord, 5, 0, 0, sSource1);
#else
	output = ButterflyPass(texcoord, 5, 0, 0, sSource);
#endif
#ifndef FFTSEVEN
	if(ApplyFilter)
	{
		output *= FrequencyDomainFilter(texcoord);
	}
#endif
}
		
void FFTV7PS(float4 pos : SV_Position, float2 texcoord : Texcoord, out float2 output : SV_Target)
{
#if ODD
	output = ButterflyPass(texcoord, 6, 0, 0, sSource);
#else
	output = ButterflyPass(texcoord, 6, 0, 0, sSource1);
#endif
#ifndef FFTEIGHT
	if(ApplyFilter)
	{
		output *= FrequencyDomainFilter(texcoord);
	}
#endif
}

void FFTV8PS(float4 pos : SV_Position, float2 texcoord : Texcoord, out float2 output : SV_Target)
{
#if ODD
	output = ButterflyPass(texcoord, 7, 0, 0, sSource1);
#else
	output = ButterflyPass(texcoord, 7, 0, 0, sSource);
#endif
#ifndef FFTNINE
	if(ApplyFilter)
	{
		output *= FrequencyDomainFilter(texcoord);
	}
#endif
}

void FFTV9PS(float4 pos : SV_Position, float2 texcoord : Texcoord, out float2 output : SV_Target)
{
#if ODD
	output = ButterflyPass(texcoord, 8, 0, 0, sSource);
#else
	output = ButterflyPass(texcoord, 8, 0, 0, sSource1);
#endif
#ifndef FFTTEN
	if(ApplyFilter)
	{
		output *= FrequencyDomainFilter(texcoord);
	}
#endif
}

void FFTV10PS(float4 pos : SV_Position, float2 texcoord : Texcoord, out float2 output : SV_Target)
{
#if ODD
	output = ButterflyPass(texcoord, 9, 0, 0, sSource1);
#else
	output = ButterflyPass(texcoord, 9, 0, 0, sSource);
#endif
#ifndef FFTELEVEN
	if(ApplyFilter)
	{
		output *= FrequencyDomainFilter(texcoord);
	}
#endif
}

void FFTV11PS(float4 pos : SV_Position, float2 texcoord : Texcoord, out float2 output : SV_Target)
{
#if ODD
	output = ButterflyPass(texcoord, 10, 0, 0, sSource);
#else
	output = ButterflyPass(texcoord, 10, 0, 0, sSource1);
#endif
#ifndef FFTTWELVE
	if(ApplyFilter)
	{
		output *= FrequencyDomainFilter(texcoord);
	}
#endif
}

void FFTV12PS(float4 pos : SV_Position, float2 texcoord : Texcoord, out float2 output : SV_Target)
{
#if ODD
	output = ButterflyPass(texcoord, 11, 0, 0, sSource1);
#else
	output = ButterflyPass(texcoord, 11, 0, 0, sSource);
#endif
#ifndef FFTTHIRTEEN
	if(ApplyFilter)
	{
		output *= FrequencyDomainFilter(texcoord);
	}
#endif
}
		
void FFTV13PS(float4 pos : SV_Position, float2 texcoord : Texcoord, out float2 output : SV_Target)
{
#if ODD
	output = ButterflyPass(texcoord, 12, 0, 0, sSource);
#else
	output = ButterflyPass(texcoord, 12, 0, 0, sSource1);
#endif
	if(ApplyFilter)
	{
		output *= FrequencyDomainFilter(texcoord);
	}
}





void IFFTH1PS(float4 pos : SV_Position, float2 texcoord : Texcoord, out float2 output : SV_Target)
{
	output = ButterflyPass(texcoord, 0, 1, 1, sFrequencyDomain);
}

void IFFTH2PS(float4 pos : SV_Position, float2 texcoord : Texcoord, out float2 output : SV_Target)
{
	output = ButterflyPass(texcoord, 1, 1, 1, sSource);
}

void IFFTH3PS(float4 pos : SV_Position, float2 texcoord : Texcoord, out float2 output : SV_Target)
{
	output = ButterflyPass(texcoord, 2, 1, 1, sSource1);
}

void IFFTH4PS(float4 pos : SV_Position, float2 texcoord : Texcoord, out float2 output : SV_Target)
{
	output = ButterflyPass(texcoord, 3, 1, 1, sSource);
}

void IFFTH5PS(float4 pos : SV_Position, float2 texcoord : Texcoord, out float2 output : SV_Target)
{
	output = ButterflyPass(texcoord, 4, 1, 1, sSource1);
}

void IFFTH6PS(float4 pos : SV_Position, float2 texcoord : Texcoord, out float2 output : SV_Target)
{
	output = ButterflyPass(texcoord, 5, 1, 1, sSource);
}
		
void IFFTH7PS(float4 pos : SV_Position, float2 texcoord : Texcoord, out float2 output : SV_Target)
{
	output = ButterflyPass(texcoord, 6, 1, 1, sSource1);
}

void IFFTH8PS(float4 pos : SV_Position, float2 texcoord : Texcoord, out float2 output : SV_Target)
{
	output = ButterflyPass(texcoord, 7, 1, 1, sSource);
}

void IFFTH9PS(float4 pos : SV_Position, float2 texcoord : Texcoord, out float2 output : SV_Target)
{
	output = ButterflyPass(texcoord, 8, 1, 1, sSource1);
}

void IFFTH10PS(float4 pos : SV_Position, float2 texcoord : Texcoord, out float2 output : SV_Target)
{
	output = ButterflyPass(texcoord, 9, 1, 1, sSource);
}

void IFFTH11PS(float4 pos : SV_Position, float2 texcoord : Texcoord, out float2 output : SV_Target)
{
	output = ButterflyPass(texcoord, 10, 1, 1, sSource1);
}

void IFFTH12PS(float4 pos : SV_Position, float2 texcoord : Texcoord, out float2 output : SV_Target)
{
	output = ButterflyPass(texcoord, 11, 1, 1, sSource);
}
		
void IFFTH13PS(float4 pos : SV_Position, float2 texcoord : Texcoord, out float2 output : SV_Target)
{
	output = ButterflyPass(texcoord, 12, 1, 1, sSource1);
}



void IFFTV1PS(float4 pos : SV_Position, float2 texcoord : Texcoord, out float2 output : SV_Target)
{
#if ODD
	output = ButterflyPass(texcoord, 0, 0, 1, sSource);
#else
	output = ButterflyPass(texcoord, 0, 0, 1, sSource1);
#endif
}

void IFFTV2PS(float4 pos : SV_Position, float2 texcoord : Texcoord, out float2 output : SV_Target)
{
#if ODD
	output = ButterflyPass(texcoord, 1, 0, 1, sSource1);
#else
	output = ButterflyPass(texcoord, 1, 0, 1, sSource);
#endif
}

void IFFTV3PS(float4 pos : SV_Position, float2 texcoord : Texcoord, out float2 output : SV_Target)
{
#if ODD
	output = ButterflyPass(texcoord, 2, 0, 1, sSource);
#else
	output = ButterflyPass(texcoord, 2, 0, 1, sSource1);
#endif
}

void IFFTV4PS(float4 pos : SV_Position, float2 texcoord : Texcoord, out float2 output : SV_Target)
{
#if ODD
	output = ButterflyPass(texcoord, 3, 0, 1, sSource1);
#else
	output = ButterflyPass(texcoord, 3, 0, 1, sSource);
#endif
}

void IFFTV5PS(float4 pos : SV_Position, float2 texcoord : Texcoord, out float2 output : SV_Target)
{
#if ODD
	output = ButterflyPass(texcoord, 4, 0, 1, sSource);
#else
	output = ButterflyPass(texcoord, 4, 0, 1, sSource1);
#endif
}

void IFFTV6PS(float4 pos : SV_Position, float2 texcoord : Texcoord, out float2 output : SV_Target)
{
#if ODD
	output = ButterflyPass(texcoord, 5, 0, 1, sSource1);
#else
	output = ButterflyPass(texcoord, 5, 0, 1, sSource);
#endif
}
		
void IFFTV7PS(float4 pos : SV_Position, float2 texcoord : Texcoord, out float2 output : SV_Target)
{
#if ODD
	output = ButterflyPass(texcoord, 6, 0, 1, sSource);
#else
	output = ButterflyPass(texcoord, 6, 0, 1, sSource1);
#endif
}

void IFFTV8PS(float4 pos : SV_Position, float2 texcoord : Texcoord, out float2 output : SV_Target)
{
#if ODD
	output = ButterflyPass(texcoord, 7, 0, 1, sSource1);
#else
	output = ButterflyPass(texcoord, 7, 0, 1, sSource);
#endif

}

void IFFTV9PS(float4 pos : SV_Position, float2 texcoord : Texcoord, out float2 output : SV_Target)
{
#if ODD
	output = ButterflyPass(texcoord, 8, 0, 1, sSource);
#else
	output = ButterflyPass(texcoord, 8, 0, 1, sSource1);
#endif
}

void IFFTV10PS(float4 pos : SV_Position, float2 texcoord : Texcoord, out float2 output : SV_Target)
{
#if ODD
	output = ButterflyPass(texcoord, 9, 0, 1, sSource1);
#else
	output = ButterflyPass(texcoord, 9, 0, 1, sSource);
#endif
}

void IFFTV11PS(float4 pos : SV_Position, float2 texcoord : Texcoord, out float2 output : SV_Target)
{
#if ODD
	output = ButterflyPass(texcoord, 10, 0, 1, sSource);
#else
	output = ButterflyPass(texcoord, 10, 0, 1, sSource1);
#endif
}

void IFFTV12PS(float4 pos : SV_Position, float2 texcoord : Texcoord, out float2 output : SV_Target)
{
#if ODD
	output = ButterflyPass(texcoord, 11, 0, 1, sSource1);
#else
	output = ButterflyPass(texcoord, 11, 0, 1, sSource);
#endif
}
		
void IFFTV13PS(float4 pos : SV_Position, float2 texcoord : Texcoord, out float2 output : SV_Target)
{
#if ODD
	output = ButterflyPass(texcoord, 12, 0, 1, sSource);
#else
	output = ButterflyPass(texcoord, 12, 0, 1, sSource1);
#endif

}



technique ButterflyTable <enabled = true; hidden = true; timeout = 1000;>
{
	pass
	{
		VertexShader = PostProcessVS;
		PixelShader = ButterflyTablePS;
		RenderTarget = ButterflyTable;
	}
}

//Prevents ButterflyTable from being dumped from memory when not in use
technique DoNothing <enabled = true; hidden = true;>
{
	pass
	{
		VertexShader = PostProcessVS;
		PixelShader = DoNothingPS;
	}
}

technique FastFourierTransform<
	ui_tooltip = "Going above 8 (SUBBLOCK_SIZE of 256x256) passes requires the precision of textures \n"
				"to be changed from 16 to 32 bit, and will result in a significant performance penalty.\n"
				"Also note that using the max BUTTERFLY_COUNT of 13 (SUBBLOCK_SIZE of 8192x8192) requires \n"
				"more than 1GB of VRAM, so be careful when trying this.";>
{
	//By default InputPS uses the Luma of the Image
	pass Input
	{
		VertexShader = PostProcessVS;
		PixelShader = InputPS;
		RenderTarget = FFTInput;
	}
#ifdef FFTONE	
	pass FFTH1
	{
		VertexShader = PostProcessVS;
		PixelShader = FFTH1PS;
		RenderTarget = Source;
	}
#endif	

#ifdef FFTTWO
	pass FFTH2
	{
		VertexShader = PostProcessVS;
		PixelShader = FFTH2PS;
		RenderTarget = Source1;
	}
#endif
	
#ifdef FFTTHREE	
	pass FFTH3
	{
		VertexShader = PostProcessVS;
		PixelShader = FFTH3PS;
		RenderTarget = Source;
	}
#endif	

#ifdef FFTFOUR
	pass FFTH4
	{
		VertexShader = PostProcessVS;
		PixelShader = FFTH4PS;
		RenderTarget = Source1;
	}
#endif

#ifdef FFTFIVE
	pass FFTH5
	{
		VertexShader = PostProcessVS;
		PixelShader = FFTH5PS;
		RenderTarget = Source;
	}
#endif

#ifdef FFTSIX	
	pass FFTH6
	{
		VertexShader = PostProcessVS;
		PixelShader = FFTH6PS;
		RenderTarget = Source1;
	}
#endif

#ifdef FFTSEVEN	
	pass FFTH7
	{
		VertexShader = PostProcessVS;
		PixelShader = FFTH7PS;
		RenderTarget = Source;
	}
#endif

#ifdef FFTEIGHT	
	pass FFTH8
	{
		VertexShader = PostProcessVS;
		PixelShader = FFTH8PS;
		RenderTarget = Source1;
	}
#endif

#ifdef FFTNINE	
	pass FFTH9
	{
		VertexShader = PostProcessVS;
		PixelShader = FFTH9PS;
		RenderTarget = Source;
	}
#endif

#ifdef FFTTEN	
	pass FFTH10
	{
		VertexShader = PostProcessVS;
		PixelShader = FFTH10PS;
		RenderTarget = Source1;
	}
#endif

#ifdef FFTELEVEN
	pass FFTH11
	{
		VertexShader = PostProcessVS;
		PixelShader = FFTH11PS;
		RenderTarget = Source;
	}
#endif

#ifdef FFTTWELVE
	pass FFTH12
	{
		VertexShader = PostProcessVS;
		PixelShader = FFTH12PS;
		RenderTarget = Source1;
	}
#endif

#ifdef FFTTHIRTEEN
	pass FFTH13
	{
		VertexShader = PostProcessVS;
		PixelShader = FFTH13PS;
		RenderTarget = Source;
	}
#endif
	
#ifdef FFTONE	
	pass FFTV1
	{
		VertexShader = PostProcessVS;
		PixelShader = FFTV1PS;
	#ifndef FFTTWO
		RenderTarget = FrequencyDomain;
	#else
		#if ODD
			RenderTarget = Source1;
		#else
			RenderTarget = Source;
		#endif
	#endif
	}
#endif	

#ifdef FFTTWO
	pass FFTV2
	{
		VertexShader = PostProcessVS;
		PixelShader = FFTV2PS;
	#ifndef FFTTHREE
		RenderTarget = FrequencyDomain;
	#else
		#if ODD
			RenderTarget = Source;
		#else
			RenderTarget = Source1;
		#endif
	#endif
	}
#endif
	
#ifdef FFTTHREE	
	pass FFTV3
	{
		VertexShader = PostProcessVS;
		PixelShader = FFTV3PS;
	#ifndef FFTFOUR
		RenderTarget = FrequencyDomain;
	#else
		#if ODD
			RenderTarget = Source1;
		#else
			RenderTarget = Source;
		#endif
	#endif
	}
#endif	

#ifdef FFTFOUR
	pass FFTV4
	{
		VertexShader = PostProcessVS;
		PixelShader = FFTV4PS;
	#ifndef FFTFIVE
		RenderTarget = FrequencyDomain;
	#else
		#if ODD
			RenderTarget = Source;
		#else
			RenderTarget = Source1;
		#endif
	#endif
	}
#endif

#ifdef FFTFIVE
	pass FFTV5
	{
		VertexShader = PostProcessVS;
		PixelShader = FFTV5PS;
	#ifndef FFTSIX
		RenderTarget = FrequencyDomain;
	#else
		#if ODD
			RenderTarget = Source1;
		#else
			RenderTarget = Source;
		#endif
	#endif
	}
#endif

#ifdef FFTSIX	
	pass FFTV6
	{
		VertexShader = PostProcessVS;
		PixelShader = FFTV6PS;
	#ifndef FFTSEVEN
		RenderTarget = FrequencyDomain;
	#else
		#if ODD
			RenderTarget = Source;
		#else
			RenderTarget = Source1;
		#endif
	#endif
	}
#endif

#ifdef FFTSEVEN	
	pass FFTV7
	{
		VertexShader = PostProcessVS;
		PixelShader = FFTV7PS;
	#ifndef FFTEIGHT
		RenderTarget = FrequencyDomain;
	#else
		#if ODD
			RenderTarget = Source1;
		#else
			RenderTarget = Source;
		#endif
	#endif
	}
#endif

#ifdef FFTEIGHT	
	pass FFTV8
	{
		VertexShader = PostProcessVS;
		PixelShader = FFTV8PS;
	#ifndef FFTNINE
		RenderTarget = FrequencyDomain;
	#else
		#if ODD
			RenderTarget = Source;
		#else
			RenderTarget = Source1;
		#endif
	#endif
	}
#endif

#ifdef FFTNINE	
	pass FFTV9
	{
		VertexShader = PostProcessVS;
		PixelShader = FFTV9PS;
	#ifndef FFTTEN
		RenderTarget = FrequencyDomain;
	#else
		#if ODD
			RenderTarget = Source1;
		#else
			RenderTarget = Source;
		#endif
	#endif
	}
#endif

#ifdef FFTTEN	
	pass FFTV10
	{
		VertexShader = PostProcessVS;
		PixelShader = FFTV10PS;
	#ifndef FFTELEVEN
		RenderTarget = FrequencyDomain;
	#else
		#if ODD
			RenderTarget = Source;
		#else
			RenderTarget = Source1;
		#endif
	#endif
	}
#endif

#ifdef FFTELEVEN
	pass FFTV11
	{
		VertexShader = PostProcessVS;
		PixelShader = FFTV11PS;
	#ifndef FFTTWELVE
		RenderTarget = FrequencyDomain;
	#else
		#if ODD
			RenderTarget = Source1;
		#else
			RenderTarget = Source;
		#endif
	#endif
	}
#endif

#ifdef FFTTWELVE
	pass FFTV12
	{
		VertexShader = PostProcessVS;
		PixelShader = FFTV12PS;
	#ifndef FFTTHIRTEEN
		RenderTarget = FrequencyDomain;
	#else
		#if ODD
			RenderTarget = Source;
		#else
			RenderTarget = Source1;
		#endif
	#endif
	}
#endif

#ifdef FFTTHIRTEEN
	pass FFTV13
	{
		VertexShader = PostProcessVS;
		PixelShader = FFTV13PS;
		RenderTarget = FrequencyDomain;
	}
#endif




#ifdef FFTONE	
	pass IFFTH1
	{
		VertexShader = PostProcessVS;
		PixelShader = IFFTH1PS;
		RenderTarget = Source;
	}
#endif	

#ifdef FFTTWO
	pass IFFTH2
	{
		VertexShader = PostProcessVS;
		PixelShader = IFFTH2PS;
		RenderTarget = Source1;
	}
#endif
	
#ifdef FFTTHREE	
	pass IFFTH3
	{
		VertexShader = PostProcessVS;
		PixelShader = IFFTH3PS;
		RenderTarget = Source;
	}
#endif	

#ifdef FFTFOUR
	pass IFFTH4
	{
		VertexShader = PostProcessVS;
		PixelShader = IFFTH4PS;
		RenderTarget = Source1;
	}
#endif

#ifdef FFTFIVE
	pass IFFTH5
	{
		VertexShader = PostProcessVS;
		PixelShader = IFFTH5PS;
		RenderTarget = Source;
	}
#endif

#ifdef FFTSIX	
	pass IFFTH6
	{
		VertexShader = PostProcessVS;
		PixelShader = IFFTH6PS;
		RenderTarget = Source1;
	}
#endif

#ifdef FFTSEVEN	
	pass IFFTH7
	{
		VertexShader = PostProcessVS;
		PixelShader = IFFTH7PS;
		RenderTarget = Source;
	}
#endif

#ifdef FFTEIGHT	
	pass IFFTH8
	{
		VertexShader = PostProcessVS;
		PixelShader = IFFTH8PS;
		RenderTarget = Source1;
	}
#endif
	
#ifdef FFTNINE	
	pass IFFTH9
	{
		VertexShader = PostProcessVS;
		PixelShader = IFFTH9PS;
		RenderTarget = Source;
	}
#endif

#ifdef FFTTEN	
	pass IFFTH10
	{
		VertexShader = PostProcessVS;
		PixelShader = IFFTH10PS;
		RenderTarget = Source1;
	}
#endif

#ifdef FFTELEVEN
	pass IFFTH11
	{
		VertexShader = PostProcessVS;
		PixelShader = IFFTH11PS;
		RenderTarget = Source;
	}
#endif

#ifdef FFTTWELVE
	pass IFFTH12
	{
		VertexShader = PostProcessVS;
		PixelShader = IFFTH12PS;
		RenderTarget = Source1;
	}
#endif

#ifdef FFTTHIRTEEN
	pass IFFTH13
	{
		VertexShader = PostProcessVS;
		PixelShader = IFFTH13PS;
		RenderTarget = Source;
	}
#endif
	
#ifdef FFTONE	
	pass IFFTV1
	{
		VertexShader = PostProcessVS;
		PixelShader = IFFTV1PS;
	#if ODD
		RenderTarget = Source1;
	#else
		RenderTarget = Source;
	#endif
	}
#endif	

#ifdef FFTTWO
	pass IFFTV2
	{
		VertexShader = PostProcessVS;
		PixelShader = IFFTV2PS;
	#if ODD
		RenderTarget = Source;
	#else
		RenderTarget = Source1;
	#endif
	}
#endif
	
#ifdef FFTTHREE	
	pass IFFTV3
	{
		VertexShader = PostProcessVS;
		PixelShader = IFFTV3PS;
	#if ODD
		RenderTarget = Source1;
	#else
		RenderTarget = Source;
	#endif
	}
#endif	

#ifdef FFTFOUR
	pass IFFTV4
	{
		VertexShader = PostProcessVS;
		PixelShader = IFFTV4PS;
	#if ODD
		RenderTarget = Source;
	#else
		RenderTarget = Source1;
	#endif
	}
#endif

#ifdef FFTFIVE
	pass IFFTV5
	{
		VertexShader = PostProcessVS;
		PixelShader = IFFTV5PS;
	#if ODD
		RenderTarget = Source1;
	#else
		RenderTarget = Source;
	#endif
	}
#endif

#ifdef FFTSIX	
	pass IFFTV6
	{
		VertexShader = PostProcessVS;
		PixelShader = IFFTV6PS;
	#if ODD
		RenderTarget = Source;
	#else
		RenderTarget = Source1;
	#endif
	}
#endif

#ifdef FFTSEVEN	
	pass IFFTV7
	{
		VertexShader = PostProcessVS;
		PixelShader = IFFTV7PS;
	#if ODD
		RenderTarget = Source1;
	#else
		RenderTarget = Source;
	#endif
	}
#endif

#ifdef FFTEIGHT	
	pass IFFTV8
	{
		VertexShader = PostProcessVS;
		PixelShader = IFFTV8PS;
	#if ODD
		RenderTarget = Source;
	#else
		RenderTarget = Source1;
	#endif
	}
#endif

#ifdef FFTNINE	
	pass IFFTV9
	{
		VertexShader = PostProcessVS;
		PixelShader = IFFTV9PS;
	#if ODD
		RenderTarget = Source1;
	#else
		RenderTarget = Source;
	#endif
	}
#endif

#ifdef FFTTEN	
	pass IFFTV10
	{
		VertexShader = PostProcessVS;
		PixelShader = IFFTV10PS;
	#if ODD
		RenderTarget = Source;
	#else
		RenderTarget = Source1;
	#endif
	}
#endif

#ifdef FFTELEVEN
	pass IFFTV11
	{
		VertexShader = PostProcessVS;
		PixelShader = IFFTV11PS;
	#if ODD
		RenderTarget = Source1;
	#else
		RenderTarget = Source;
	#endif
	}
#endif

#ifdef FFTTWELVE
	pass IFFTV12
	{
		VertexShader = PostProcessVS;
		PixelShader = IFFTV12PS;
	#if ODD
		RenderTarget = Source;
	#else
		RenderTarget = Source1;
	#endif
	}
#endif

#ifdef FFTTHIRTEEN
	pass IFFTV13
	{
		VertexShader = PostProcessVS;
		PixelShader = IFFTV13PS;
		RenderTarget = Source1;
	}
#endif
	pass MoveToOutput
	{
		VertexShader = PostProcessVS;
		PixelShader = MoveToOutputPS;
		RenderTarget = FFTOutput;
	}
}
