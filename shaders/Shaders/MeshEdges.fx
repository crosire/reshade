///////////////////////////////////////////////////////////////////////////////
//
//ReShade Shader: MeshEdges
//https://github.com/Daodan317081/reshade-shaders
//
//BSD 3-Clause License
//
//Copyright (c) 2018-2019, Alexander Federwisch
//All rights reserved.
//
//Redistribution and use in source and binary forms, with or without
//modification, are permitted provided that the following conditions are met:
//
//* Redistributions of source code must retain the above copyright notice, this
//  list of conditions and the following disclaimer.
//
//* Redistributions in binary form must reproduce the above copyright notice,
//  this list of conditions and the following disclaimer in the documentation
//  and/or other materials provided with the distribution.
//
//* Neither the name of the copyright holder nor the names of its
//  contributors may be used to endorse or promote products derived from
//  this software without specific prior written permission.
//
//THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
//AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
//IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
//DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
//FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
//DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
//SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
//CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
//OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
//OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
///////////////////////////////////////////////////////////////////////////////

#include "ReShade.fxh"

uniform int iUIBackground <
    ui_type = "combo";
    ui_label = "Background Type";
    ui_items = "Backbuffer\0Color\0";
> = 1;

uniform float3 fUIColorBackground <
    ui_type = "color";
    ui_label = "Color Background";
> = float3(1.0, 1.0, 1.0);

uniform float3 fUIColorLines <
    ui_type = "color";
    ui_label = "Color Lines";
> = float3(0.0, 0.0, 0.0);

uniform float fUIStrength <
    ui_type = "drag";
    ui_label = "Strength";
    ui_min = 0.0; ui_max = 1.0;
    ui_step = 0.01;
> = 1.0;

#define MAX2(v) max(v.x, v.y)
#define MIN2(v) min(v.x, v.y)
#define MAX4(v) max(v.x, max(v.y, max(v.z, v.w)))
#define MIN4(v) min(v.x, min(v.y, min(v.z, v.w)))

float3 MeshEdges_PS(float4 vpos:SV_Position, float2 texcoord:TexCoord):SV_Target {
    float3 backbuffer = tex2D(ReShade::BackBuffer, texcoord).rgb;
    float4 pix = float4(ReShade::PixelSize, -ReShade::PixelSize);

    //Get depth of center pixel
    float c = ReShade::GetLinearizedDepth(texcoord);
    //Get depth of surrounding pixels
    float4 depthEven = float4(  ReShade::GetLinearizedDepth(texcoord + float2(0.0, pix.w)),
                                ReShade::GetLinearizedDepth(texcoord + float2(0.0, pix.y)),
                                ReShade::GetLinearizedDepth(texcoord + float2(pix.x, 0.0)),
                                ReShade::GetLinearizedDepth(texcoord + float2(pix.z, 0.0))   );

    float4 depthOdd  = float4(  ReShade::GetLinearizedDepth(texcoord + float2(pix.x, pix.w)),
                                ReShade::GetLinearizedDepth(texcoord + float2(pix.z, pix.y)),
                                ReShade::GetLinearizedDepth(texcoord + float2(pix.x, pix.y)),
                                ReShade::GetLinearizedDepth(texcoord + float2(pix.z, pix.w)) );
    
    //Normalize values
    float2 mind = float2(MIN4(depthEven), MIN4(depthOdd));
    float2 maxd = float2(MAX4(depthEven), MAX4(depthOdd));
    float span = MAX2(maxd) - MIN2(mind) + 0.00001;
    c /= span;
    depthEven /= span;
    depthOdd /= span;
    //Calculate the distance of the surrounding pixels to the center
    float4 diffsEven = abs(depthEven - c);
    float4 diffsOdd = abs(depthOdd - c);
    //Calculate the difference of the (opposing) distances
    float2 retVal = float2( max(abs(diffsEven.x - diffsEven.y), abs(diffsEven.z - diffsEven.w)),
                            max(abs(diffsOdd.x - diffsOdd.y), abs(diffsOdd.z - diffsOdd.w))     );

    float lineWeight = MAX2(retVal);

    return lerp(iUIBackground == 0 ? backbuffer : fUIColorBackground, fUIColorLines, lineWeight * fUIStrength);
}

technique MeshEdges {
    pass {
        VertexShader = PostProcessVS; 
        PixelShader = MeshEdges_PS; 
        /* RenderTarget = BackBuffer */
    }
}