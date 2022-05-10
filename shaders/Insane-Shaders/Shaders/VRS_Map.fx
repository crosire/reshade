// VRS_Map.fx
//
// Copyright (c) 2020 Advanced Micro Devices, Inc. All rights reserved.
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

/*
	Variable Rate Shading Map for ReShade
	Port and Framework by: Lord of Lunacy
	https://github.com/LordOfLunacy/Insane-Shaders
	
	This is a port of the VRS Image generation shader in AMD's FidelityFX,
	currently it is lacking support for tile sizes besides 8, and the
	option for more shading rates.
	https://github.com/GPUOpen-Effects/FidelityFX-VariableShading
	
	To make the shader compatible with ReshadeFX, I had to replace the wave intrinsics
	with atomic intrinsics.
	
	
	The method used for the optical flow was provided by Jose Negrete AKA BlueSkyDefender
	<UntouchableBlueSky@gmail.com>
	https://github.com/BlueSkyDefender/
*/

//////////////////////////////////////////////////////////////////////////
// VRS constant buffer parameters:
//
// Resolution The resolution of the surface a VRSImage is to be generated for
// TileSize Hardware dependent tile size (query from API; 8 on AMD RDNA2 based GPUs)
// VarianceCutoff Maximum luminance variance acceptable to accept reduced shading rate
// MotionFactor Length of the motion vector * MotionFactor gets deducted from luminance variance
// to allow lower VS rates on fast moving objects
//
//////////////////////////////////////////////////////////////////////////
#define __SUPPORTED_VRS_MAP_COMPATIBILITY__ 11

#if exists "VRS_Map.fxh"                                          
    #include "VRS_Map.fxh"
	#ifndef VRS_MAP
		#define VRS_MAP 1
	#else
		#error "VRS_Map.fxh and VRS_Map.fx versions don't match, please download the latest version off Github"
	#endif
#else
    #define VRS_MAP 0
	#error "VRS_Map.fx requires VRS_Map.fxh to work, please download the latest version of both off Github"
#endif

#if _VRS_COMPUTE != 0
#if VRS_USE_OPTICAL_FLOW != 0
	#if exists "ReShade.fxh"
		#include "ReShade.fxh"
	#else
		#warning "VRSMap.fx requires ReShade.fxh to use optical flow"
		#undef VRS_USE_OPTICAL_FLOW
		#define VRS_USE_OPTICAL_FLOW 0
	#endif
#endif
texture BackBuffer : COLOR;

sampler sBackBuffer {Texture = BackBuffer;};

storage wVRS {Texture = VRS;};
storage wVRSUpdated {Texture = VRSUpdated;};

static const int2 g_Resolution = int2(BUFFER_WIDTH, BUFFER_HEIGHT);
static const uint g_TileSize = TILE_SIZE;
uniform float g_VarianceCutoff<
	ui_type = "slider";
	ui_label = "Variance Cutoff";
	ui_tooltip = "Maximum luminance variance acceptable to accept reduced shading rate";
	ui_min = 0; ui_max = 0.1;
	ui_step = 0.001;
> = 0.05;

#if VRS_USE_OPTICAL_FLOW != 0
uniform float g_MotionFactor<
	ui_type = "slider";
	ui_label = "Motion Factor";
	ui_tooltip = "Length of the motion vector * MotionFactor gets deducted from luminance variance \n"
				 "to allow lower VS rates on fast moving objects";
	ui_min = 0; ui_max = 0.1;
	ui_step = 0.001;
> = 0.05;

uniform float2 PingPong < source = "pingpong"; min = 0; max = 1; step = 1; >;
texture VRS_Depth0 {Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = R8;};
texture VRS_Depth1 {Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = R8;};

sampler sVRS_Depth0 {Texture = VRS_Depth0;};
sampler sVRS_Depth1 {Texture = VRS_Depth1;};

storage wVRS_Depth0 {Texture = VRS_Depth0;};
storage wVRS_Depth1 {Texture = VRS_Depth1;};
#else
static const float g_MotionFactor = 0;
#endif

uniform bool ShowOverlay <
	ui_label = "Show Overlay";
> = 1;

// Forward declaration of functions that need to be implemented by shader code using this technique

float   VRS_ReadLuminance(int2 pos)
{
	return dot(tex2Dfetch(sBackBuffer, pos).rgb, float3(0.299, 0.587, 0.114));
}

#if VRS_USE_OPTICAL_FLOW != 0
	float2  VRS_ReadMotionVec2D(int2 pos)
	{
		float currDepth = ReShade::GetLinearizedDepth(pos * float2(BUFFER_RCP_WIDTH, BUFFER_RCP_HEIGHT));
		float prevDepth;
		if(uint(VRS_FrameCount) % 2 == 0)
		{
			prevDepth = tex2Dfetch(sVRS_Depth0, pos).r;
			tex2Dstore(wVRS_Depth1, pos, currDepth);
		}
		else
		{
			prevDepth = tex2Dfetch(sVRS_Depth1, pos).r;
			tex2Dstore(wVRS_Depth0, pos, currDepth);
		}
		//Velocity Scalar
		float S_Velocity = 12.5 * lerp(1,80,0.5), V_Buffer = saturate(distance(currDepth,prevDepth) * S_Velocity);
		return float2(V_Buffer, 0);
	}
#else
	float2  VRS_ReadMotionVec2D(int2 pos)
	{
		return float2(0, 0);
	}
#endif
void    VRS_WriteVrsImage(int2 pos, uint value)
{
	tex2Dstore(wVRS, pos, float4(float(value)/255, 0, 0, 0));
}

static const uint VRS_ThreadCount1D = TILE_SIZE;
static const uint VRS_NumBlocks1D = 2;

static const uint VRS_SampleCount1D = VRS_ThreadCount1D + 2;

groupshared uint VRS_LdsGroupReduce;

static const uint VRS_ThreadCount = VRS_ThreadCount1D * VRS_ThreadCount1D;
static const uint VRS_SampleCount = VRS_SampleCount1D * VRS_SampleCount1D;
static const uint VRS_NumBlocks = VRS_NumBlocks1D * VRS_NumBlocks1D;

groupshared float3 VRS_LdsVariance[VRS_SampleCount];
groupshared float VRS_LdsMin[VRS_SampleCount];
groupshared float VRS_LdsMax[VRS_SampleCount];

float VRS_GetLuminance(int2 pos)
{
    return VRS_ReadLuminance(pos);
}

int VRS_FlattenLdsOffset(int2 coord)
{
    coord += 1;
    return coord.y * VRS_SampleCount1D + coord.x;
}

groupshared uint4 diffX;
groupshared uint4 diffY;
groupshared uint4 diffZ;

int floatToOrderedInt( float floatVal ) {
 int intVal = asint( floatVal );
 return (intVal >= 0 ) ? intVal : intVal ^ 0x7FFFFFFF;
}

float orderedIntToFloat( int intVal ) {
 return asfloat( (intVal >= 0) ? intVal : intVal ^ 0x7FFFFFFF);
}

//--------------------------------------------------------------------------------------//
// Main function
//--------------------------------------------------------------------------------------//
void VRS_GenerateVrsImage(uint3 id : SV_DispatchThreadID, uint3 Gtid : SV_GroupThreadID)
{
	uint3 Gid = uint3(id.x / TILE_SIZE, id.y / TILE_SIZE, 0);
    int2 tileOffset = Gid.xy * VRS_ThreadCount1D * 2;
    int2 baseOffset = tileOffset + int2(-2, -2);
	uint Gidx = Gtid.y * TILE_SIZE + Gtid.x;
    uint index = Gidx;
	if(all(id.xy == 0))
	{
		tex2Dstore(wVRSUpdated, Gtid.xy, float4(asfloat(VRS_FrameCount), 0, 0, 0));
	}
	
	// sample source texture (using motion vectors)
    while (index < VRS_SampleCount)
    {
        int2 index2D = 2 * int2(index % VRS_SampleCount1D, index / VRS_SampleCount1D);
        float4 lum = 0;
        lum.x = VRS_GetLuminance(baseOffset + index2D + int2(0, 0));
        lum.y = VRS_GetLuminance(baseOffset + index2D + int2(1, 0));
        lum.z = VRS_GetLuminance(baseOffset + index2D + int2(0, 1));
        lum.w = VRS_GetLuminance(baseOffset + index2D + int2(1, 1));

        // compute the 2x1, 1x2 and 2x2 variance inside the 2x2 coarse pixel region
        float3 delta;
        delta.x = max(abs(lum.x - lum.y), abs(lum.z - lum.w));
        delta.y = max(abs(lum.x - lum.z), abs(lum.y - lum.w));
        float2 minmax = float2(min(min(min(lum.x, lum.y), lum.z), lum.w), max(max(max(lum.x, lum.y), lum.z), lum.w));
        delta.z = minmax.y - minmax.x;

        // reduce variance value for fast moving pixels
        float v = length(VRS_ReadMotionVec2D(baseOffset + index2D));
        v *= g_MotionFactor;
        delta -= v;
        minmax.y -= v;

        // store variance as well as min/max luminance
        VRS_LdsVariance[index] = delta;
        VRS_LdsMin[index] = minmax.x;
        VRS_LdsMax[index] = minmax.y;

        index += VRS_ThreadCount;
    }
	//Initialized here to reduce the number of barrier statements
	if(Gtid.x == 0 && Gtid.y == 0)
	{
		diffX = 0;
		diffY = 0;
		diffZ = 0;
	}
	barrier();
	
    // upper left coordinate in LDS
    int2 threadUV = Gtid.xy;

    // look at neighbouring coarse pixels, to combat burn in effect due to frame dependence
    float3 delta = VRS_LdsVariance[VRS_FlattenLdsOffset(threadUV + int2(0, 0))];

    // read the minimum luminance for neighbouring coarse pixels
    float minNeighbour = VRS_LdsMin[VRS_FlattenLdsOffset(threadUV + int2(0, -1))];
    minNeighbour = min(minNeighbour, VRS_LdsMin[VRS_FlattenLdsOffset(threadUV + int2(-1, 0))]);
    minNeighbour = min(minNeighbour, VRS_LdsMin[VRS_FlattenLdsOffset(threadUV + int2(0, 1))]);
    minNeighbour = min(minNeighbour, VRS_LdsMin[VRS_FlattenLdsOffset(threadUV + int2(1, 0))]);
    float dMin = max(0, VRS_LdsMin[VRS_FlattenLdsOffset(threadUV + int2(0, 0))] - minNeighbour);

    // read the maximum luminance for neighbouring coarse pixels
    float maxNeighbour = VRS_LdsMax[VRS_FlattenLdsOffset(threadUV + int2(0, -1))];
    maxNeighbour = max(maxNeighbour, VRS_LdsMax[VRS_FlattenLdsOffset(threadUV + int2(-1, 0))]);
    maxNeighbour = max(maxNeighbour, VRS_LdsMax[VRS_FlattenLdsOffset(threadUV + int2(0, 1))]);
    maxNeighbour = max(maxNeighbour, VRS_LdsMax[VRS_FlattenLdsOffset(threadUV + int2(1, 0))]);
    float dMax = max(0, maxNeighbour - VRS_LdsMax[VRS_FlattenLdsOffset(threadUV + int2(0, 0))]);

    // assume higher luminance based on min & max values gathered from neighbouring pixels
    delta = max(0, delta + dMin + dMax);

    // Reduction: find maximum variance within VRS tile
	uint idx = (Gtid.y & (VRS_NumBlocks1D - 1)) * VRS_NumBlocks1D + (Gtid.x & (VRS_NumBlocks1D - 1));
	atomicMax(diffX[idx], floatToOrderedInt(delta.x));
	atomicMax(diffY[idx], floatToOrderedInt(delta.y));
	atomicMax(diffZ[idx], floatToOrderedInt(delta.z));
	
	
	// write out shading rates to VRS image
    if (Gidx < VRS_NumBlocks)
    {
        float varH = orderedIntToFloat(diffX[Gidx]);
        float varV = orderedIntToFloat(diffY[Gidx]);
        float var = orderedIntToFloat(diffZ[Gidx]);;
        uint shadingRate = VRS_MAKE_SHADING_RATE(VRS_RATE1D_1X, VRS_RATE1D_1X);

        if (var < g_VarianceCutoff)
        {
            shadingRate = VRS_MAKE_SHADING_RATE(VRS_RATE1D_2X, VRS_RATE1D_2X);
        }
        else
        {
            if (varH > varV)
            {
                shadingRate = VRS_MAKE_SHADING_RATE(VRS_RATE1D_1X, (varV > g_VarianceCutoff) ? VRS_RATE1D_1X : VRS_RATE1D_2X);
            }
            else
            {
                shadingRate = VRS_MAKE_SHADING_RATE((varH > g_VarianceCutoff) ? VRS_RATE1D_1X : VRS_RATE1D_2X, VRS_RATE1D_1X);
            }
        }
        // Store
        //VRS_WriteVrsImage(Gid.xy* VRS_NumBlocks1D + uint2(Gidx / VRS_NumBlocks1D, Gidx % VRS_NumBlocks1D), shadingRate);
		tex2Dstore(wVRS, Gid.xy* VRS_NumBlocks1D + uint2(Gidx / VRS_NumBlocks1D, Gidx % VRS_NumBlocks1D), float4(float3(varH, varV, var) * 4, float(shadingRate) / 255));
    }
}

struct VERTEX_OUT
{
    float4 vPosition : SV_POSITION;
	float2 texcoord : TEXCOORD;
};

VERTEX_OUT mainVS(uint id : SV_VertexID)
{
    VERTEX_OUT output;
    output.vPosition = float4(float2(id & 1, id >> 1) * float2(4, -4) + float2(-1, 1), 0, 1);
	output.texcoord = float2(0, 0);
    return output;
}

float3 mainPS(VERTEX_OUT input) : SV_Target
{
	if(!ShowOverlay) discard;
	float2 texcoord = input.vPosition.xy * float2(BUFFER_RCP_WIDTH, BUFFER_RCP_HEIGHT);
	float3 originalImage = tex2Dfetch(sBackBuffer, input.vPosition.xy).rgb;
	return VRS_Map::DebugImage(originalImage, texcoord, g_VarianceCutoff, ShowOverlay);
}

technique VariableRateShading
{
	pass
	{
		ComputeShader = VRS_GenerateVrsImage<TILE_SIZE, TILE_SIZE>;
		DispatchSizeX = THREAD_GROUPS.x;
		DispatchSizeY = THREAD_GROUPS.y;
	}
	pass
	{
		VertexShader = mainVS;
		PixelShader = mainPS;
	}
}
#endif //_VRS_COMPUTE
