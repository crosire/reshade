/*=============================================================================

    Copyright (c) Pascal Gilcher. All rights reserved.
    CC BY-NC-ND 3.0 licensed.

=============================================================================*/

#pragma once 

//All functions for projecting screen space data into a perspective viewport
//Since ReShade cannot access data such as field of view, it has to be inferred

/*===========================================================================*/

#ifndef QUINT_GLOBAL_FIELD_OF_VIEW
 #define QUINT_GLOBAL_FIELD_OF_VIEW 60.0
#endif

/*===========================================================================*/

float depth_to_z(in float depth)
{
    return depth * RESHADE_DEPTH_LINEARIZATION_FAR_PLANE + 1;
}

float z_to_depth(in float z)
{
    return (z - 1) / RESHADE_DEPTH_LINEARIZATION_FAR_PLANE;
}

float2 proj_to_uv(in float3 pos)
{
    //TODO: resolve the calculations to remove duplicate code
    static const float3 uvtoprojADD = float3(-tan(radians(QUINT_GLOBAL_FIELD_OF_VIEW) * 0.5).xx, 1.0) * qUINT::ASPECT_RATIO.yxx;
    static const float3 uvtoprojMUL = float3(-2.0 * uvtoprojADD.xy, 0.0);

    static const float4 projtouv = float4(rcp(uvtoprojMUL.xy), -rcp(uvtoprojMUL.xy) * uvtoprojADD.xy);
    return (pos.xy / pos.z) * projtouv.xy + projtouv.zw;
}

float3 uv_to_proj(float2 uv, float z)
{
    //optimized math to simplify matrix mul
    static const float3 uvtoprojADD = float3(-tan(radians(QUINT_GLOBAL_FIELD_OF_VIEW) * 0.5).xx, 1.0) * qUINT::ASPECT_RATIO.yxx;
    static const float3 uvtoprojMUL = float3(-2.0 * uvtoprojADD.xy, 0.0);

    return (uv.xyx * uvtoprojMUL + uvtoprojADD) * z;
}

float3 uv_to_proj(in VSOUT i)
{
    float z = depth_to_z(qUINT::linear_depth(i.uv.xy));
    return uv_to_proj(i.uv.xy, z);
}

float3 uv_to_proj(in float2 uv)
{
    float z = depth_to_z(qUINT::linear_depth(uv));
    return uv_to_proj(uv, z);
}

float3 uv_to_proj(in float2 uv, sampler2D ztex, in int mip)
{
    float z = tex2Dlod(ztex, float4(uv.xyx, mip)).x;
    return uv_to_proj(uv, z);
}