/*=============================================================================

    Copyright (c) Pascal Gilcher. All rights reserved.
    CC BY-NC-ND 3.0 licensed.

=============================================================================*/

#pragma once

/*===========================================================================*/

#include "Projection.fxh"

//Generating normal vectors from depth buffer values
float3 normal_from_depth(in VSOUT i)
{
    float3 center_position = uv_to_proj(i);

    float3 delta_x, delta_y;
    {
        float4 neighbour_uv = i.uv.xyxy + float4(qUINT::PIXEL_SIZE.x, 0, -qUINT::PIXEL_SIZE.x, 0);

        float3 delta_right = uv_to_proj(neighbour_uv.xy) - center_position;
        float3 delta_left  = center_position - uv_to_proj(neighbour_uv.zw);

        delta_x = abs(delta_right.z) > abs(delta_left.z) ? delta_left : delta_right;
    }
    {
        float4 neighbour_uv = i.uv.xyxy + float4(0, qUINT::PIXEL_SIZE.y, 0, qUINT::PIXEL_SIZE.y);
        
        float3 delta_bottom = uv_to_proj(neighbour_uv.xy) - center_position;
        float3 delta_top  = center_position - uv_to_proj(neighbour_uv.zw);

        delta_y = abs(delta_bottom.z) > abs(delta_top.z) ? delta_top : delta_bottom;
    }

    float3 normal = cross(delta_y, delta_x);
    normal *= rsqrt(dot(normal, normal) + 1e-9);

    return normal;
}

float3x3 calculate_tbn_matrix(float3 normal)
{
    float3 tempv = abs(normal.z) < abs(normal.x) 
                 ? float3(normal.y, -normal.x, 0) 
                 : float3(0, -normal.z, normal.y);

    tempv = normalize(tempv);

    float3 u = tempv;
    float3 v = cross(u, normal);
    float3 w = normal;

    return float3x3(u, v, w);
}