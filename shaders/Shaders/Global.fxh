/*=============================================================================

    Copyright (c) Pascal Gilcher. All rights reserved.

 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 
=============================================================================*/

#pragma once

/*===========================================================================*/

//no namespace
#define BUFFER_PIXEL_SIZE       float2(BUFFER_RCP_WIDTH, BUFFER_RCP_HEIGHT)
#define BUFFER_SCREEN_SIZE      uint2(BUFFER_WIDTH, BUFFER_HEIGHT)
#define BUFFER_ASPECT_RATIO     float2(1.0, BUFFER_WIDTH * BUFFER_RCP_HEIGHT)

texture ColorInputTex : COLOR;
texture DepthInputTex : DEPTH;
sampler ColorInput 	{ Texture = ColorInputTex; MinFilter=POINT; MipFilter=POINT; MagFilter=POINT;};
sampler DepthInput  { Texture = DepthInputTex; MinFilter=POINT; MipFilter=POINT; MagFilter=POINT;};

void VS_FullscreenTriangle(in uint id : SV_VertexID, out float4 vpos : SV_Position, out float2 uv : TEXCOORD)
{
	uv.x = (id == 2) ? 2.0 : 0.0;
	uv.y = (id == 1) ? 2.0 : 0.0;
	vpos = float4(uv * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
}

struct MRT2
{
    float4 t0 : SV_Target0;
    float4 t1 : SV_Target1;
};

struct MRT3
{
    float4 t0 : SV_Target0;
    float4 t1 : SV_Target1;
    float4 t2 : SV_Target2;
};

struct MRT4
{
    float4 t0 : SV_Target0;
    float4 t1 : SV_Target1;
    float4 t2 : SV_Target2;
    float4 t3 : SV_Target3;
};

//ReShade FX extensions - those should be intrinsics for how often they are needed
#define linearstep(_a, _b, _x) saturate((_x - _a) * rcp(_b - _a))

float4 tex2Dlod(sampler i, float2 uv, float mip)
{
    return tex2Dlod(i, float4(uv, 0, mip));
}

