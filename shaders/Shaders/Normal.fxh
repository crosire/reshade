/*=============================================================================

    Copyright (c) Pascal Gilcher. All rights reserved.

 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential

=============================================================================*/

#pragma once

/*===========================================================================*/

#include "Projection.fxh"

namespace Normal
{

//Generating normal vectors from depth buffer values
//v1 legacy 5 taps

float3 normal_from_depth(in float2 uv)
{
    float3 center_position = Projection::uv_to_proj(uv);

    float3 delta_x, delta_y;
    float4 neighbour_uv;
    
    neighbour_uv = uv.xyxy + float4(BUFFER_PIXEL_SIZE.x, 0, -BUFFER_PIXEL_SIZE.x, 0);

    float3 delta_right = Projection::uv_to_proj(neighbour_uv.xy) - center_position;
    float3 delta_left  = center_position - Projection::uv_to_proj(neighbour_uv.zw);

    delta_x = abs(delta_right.z) > abs(delta_left.z) ? delta_left : delta_right;

    neighbour_uv = uv.xyxy + float4(0, BUFFER_PIXEL_SIZE.y, 0, -BUFFER_PIXEL_SIZE.y);
        
    float3 delta_bottom = Projection::uv_to_proj(neighbour_uv.xy) - center_position;
    float3 delta_top  = center_position - Projection::uv_to_proj(neighbour_uv.zw);

    delta_y = abs(delta_bottom.z) > abs(delta_top.z) ? delta_top : delta_bottom;

    float3 normal = cross(delta_y, delta_x);
    normal *= rsqrt(dot(normal, normal)); //no epsilon, will cause issues for some reason

    return normal;
}   

float3x3 base_from_vector(float3 n)
{

    bool bestside = n.z < n.y;

    float3 n2 = bestside ? n.xzy : n;

    float3 k = (-n2.xxy * n2.xyy) * rcp(1.0 + n2.z) + float3(1, 0, 1);
    float3 u = float3(k.xy, -n2.x);
    float3 v = float3(k.yz, -n2.y);

    u = bestside ? u.xzy : u;
    v = bestside ? v.xzy : v;

    return float3x3(u, v, n);
}

} //Namespace