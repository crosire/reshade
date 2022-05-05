/*=============================================================================

    Copyright (c) Pascal Gilcher. All rights reserved.

 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 
=============================================================================*/

#pragma once 

//Random functions and hashes

/*===========================================================================*/

namespace Random
{

float goldenweyl1(float n)
{
    return frac(0.5 + n * 0.6180339887498);
}

float goldenweyl1(float n, float x)
{
    return frac(x + n * 0.6180339887498);
}

float2 goldenweyl2(float n)
{
    return frac(0.5 + n * float2(0.7548776662467, 0.569840290998));
}

float2 goldenweyl2(float n, float2 x)
{
    return frac(x + n * float2(0.7548776662467, 0.569840290998));
}

float3 goldenweyl3(float n)
{
    return frac(0.5 + n * float3(0.8191725133961, 0.6710436067038, 0.5497004779019));
}

float3 goldenweyl3(float n, float3 x)
{
    return frac(x + n * float3(0.8191725133961, 0.6710436067038, 0.5497004779019));
}

} //namespace