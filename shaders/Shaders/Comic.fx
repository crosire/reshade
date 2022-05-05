///////////////////////////////////////////////////////////////////////////////
//
//ReShade Shader: Comic
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
//
// Version History:
// 20-nov-2018:     v1.2.2    Update uniforms
// 17-nov-2018:     v1.2.1    Update code for line width
// 15-nov-2018:     v1.2.0    Simplify outlines Caclulation,
//                            Add line width control to mesh-edges
// 09-nov-2018:     v1.1.2    Simplify code for debug-output
// 07-nov-2018:     v1.1.1    Place convolution code in its own function,
//                            code cleanup
// 05-nov-2018:     v1.1.0    Optimization, new default values,
//                            fixed wrong variable used in interpolation,
//                            added comments.
// 01-nov-2018:     v1.0.0    Feature complete beta.
//
///////////////////////////////////////////////////////////////////////////////

#include "ReShade.fxh"

#define UI_CATEGORY_COLOR "Edges: Color"
#define UI_CATEGORY_CHROMA "Edges: Chroma"
#define UI_CATEGORY_OUTLINES "Outlines 1"
#define UI_CATEGORY_MESH_EDGES "Outlines 2 (Mesh Edges)"
#define UI_CATEGORY_MISC "Luma/Saturation Weight"
#define UI_CATEGORY_DEBUG "Debug"
#define UI_CATEGORY_EFFECT "Effect"

#define UI_EDGES_LABEL_ENABLE "Type"
#define UI_EDGES_LABEL_DETAILS "Details"
#define UI_EDGES_LABEL_STRENGTH "Power, Slope"
#define UI_EDGES_LABEL_DISTANCE_STRENGTH "Distance Strength"
#define UI_EDGES_LABEL_DEBUG "Add to Debug Layer"

#define UI_EDGES_ITEMS "Disabled\0Value Difference\0Single Pass Convolution\0Two Pass Convolution"
#define UI_EDGES_TOOLTIP_DISTANCE_STRENGTH "x: Fade In\ny: Fade Out\nz: Slope"
#define UI_EDGES_TOOLTIP_WEIGHTS "x: Min\ny: Max\nz: Slope"
#define UI_EDGES_TOOLTIP_DETAILS "Only for Convolution"


#ifndef MAX2
#define MAX2(v) max(v.x, v.y)
#endif
#ifndef MIN2
#define MIN2(v) min(v.x, v.y)
#endif
#ifndef MAX3
#define MAX3(v) max(v.x, max(v.y, v.z))
#endif
#ifndef MIN3
#define MIN3(v) min(v.x, min(v.y, v.z))
#endif
#ifndef MAX4
#define MAX4(v) max(v.x, max(v.y, max(v.z, v.w)))
#endif
#ifndef MIN4
#define MIN4(v) min(v.x, min(v.y, min(v.z, v.w)))
#endif
#ifndef LumaCoeff
#define LumaCoeff float3(0.2126, 0.7151, 0.0721)
#endif
/******************************************************************************
    Uniforms
******************************************************************************/
////////////////////////// Color //////////////////////////
uniform int iUIColorEdgesType <
    ui_type = "combo";
    ui_category = UI_CATEGORY_COLOR;
    ui_label = UI_EDGES_LABEL_ENABLE;
    ui_items = UI_EDGES_ITEMS;
> = 1;

uniform float fUIColorEdgesDetails <
    ui_type = "drag";
    ui_category = UI_CATEGORY_COLOR;
    ui_label = UI_EDGES_LABEL_DETAILS;
    ui_tooltip = UI_EDGES_TOOLTIP_DETAILS;
    ui_min = 0.0; ui_max = 1.0;
    ui_step = 0.01;
> = 1.0;

uniform float2 fUIColorEdgesStrength <
    ui_type = "drag";
    ui_category = UI_CATEGORY_COLOR;
    ui_label = UI_EDGES_LABEL_STRENGTH;
    ui_min = 0.1; ui_max = 10.0;
    ui_step = 0.01;
> = float2(1.0, 1.0);

uniform float3 fUIColorEdgesDistanceFading<
    ui_type = "drag";
    ui_category = UI_CATEGORY_COLOR;
    ui_label = UI_EDGES_LABEL_DISTANCE_STRENGTH;
    ui_tooltip = UI_EDGES_TOOLTIP_DISTANCE_STRENGTH;
    ui_min = -1.0; ui_max = 1.0;
    ui_step = 0.001;
> = float3(0.0, 1.0, 0.8);

uniform bool bUIColorEdgesDebugLayer <
    ui_label = UI_EDGES_LABEL_DEBUG;
    ui_category = UI_CATEGORY_COLOR;
> = true;

////////////////////////// Chroma //////////////////////////
uniform int iUIChromaEdgesType <
    ui_type = "combo";
    ui_category = UI_CATEGORY_CHROMA;
    ui_label = UI_EDGES_LABEL_ENABLE;
    ui_items = UI_EDGES_ITEMS;
> = 3;

uniform float fUIChromaEdgesDetails <
    ui_type = "drag";
    ui_category = UI_CATEGORY_CHROMA;
    ui_label = UI_EDGES_LABEL_DETAILS;
    ui_tooltip = UI_EDGES_TOOLTIP_DETAILS;
    ui_min = 0.0; ui_max = 1.0;
    ui_step = 0.01;
> = 0.0;

uniform float2 fUIChromaEdgesStrength <
    ui_type = "drag";
    ui_category = UI_CATEGORY_CHROMA;
    ui_label = UI_EDGES_LABEL_STRENGTH;
    ui_min = 0.01; ui_max = 10.0;
    ui_step = 0.01;
> = float2(1.0, 0.5);

uniform float3 fUIChromaEdgesDistanceFading<
    ui_type = "drag";
    ui_category = UI_CATEGORY_CHROMA;
    ui_label = UI_EDGES_LABEL_DISTANCE_STRENGTH;
    ui_tooltip = UI_EDGES_TOOLTIP_DISTANCE_STRENGTH;
    ui_min = -1.0; ui_max = 1.0;
    ui_step = 0.001;
> = float3(0.0, 0.5, 0.8);

uniform bool bUIChromaEdgesDebugLayer <
    ui_label = UI_EDGES_LABEL_DEBUG;
    ui_category = UI_CATEGORY_CHROMA;
> = true;

////////////////////////// Outlines //////////////////////////
uniform int iUIOutlinesEnable <
    ui_type = "combo";
    ui_category = UI_CATEGORY_OUTLINES;
    ui_label = UI_EDGES_LABEL_ENABLE;
    ui_items = "Disabled\0Type 1\0Type 2\0";
> = 1;

uniform float fUIOutlinesThreshold <
    ui_type = "drag";
    ui_category = UI_CATEGORY_OUTLINES;
    ui_label = "Threshold";
    ui_min = 0.0; ui_max = 1000.0;
    ui_step = 0.01;
> = 1.0;

uniform float2 fUIOutlinesStrength <
    ui_type = "drag";
    ui_category = UI_CATEGORY_OUTLINES;
    ui_label = UI_EDGES_LABEL_STRENGTH;
    ui_min = 0.01; ui_max = 10.0;
    ui_step = 0.01;
> = float2(1.0, 1.0);

uniform float3 fUIOutlinesDistanceFading<
    ui_type = "drag";
    ui_category = UI_CATEGORY_OUTLINES;
    ui_label = UI_EDGES_LABEL_DISTANCE_STRENGTH;
    ui_tooltip = UI_EDGES_TOOLTIP_DISTANCE_STRENGTH;
    ui_min = -1.0; ui_max = 1.0;
    ui_step = 0.001;
> = float3(0.0, 1.0, 0.8);

uniform bool bUIOutlinesDebugLayer <
    ui_label = UI_EDGES_LABEL_DEBUG;
    ui_category = UI_CATEGORY_OUTLINES;
> = true;

////////////////////////// Mesh Edges //////////////////////////
uniform int iUIMeshEdgesEnable <
    ui_type = "combo";
    ui_category = UI_CATEGORY_MESH_EDGES;
    ui_label = UI_EDGES_LABEL_ENABLE;
    ui_items = "Disabled\0Enabled\0";
> = 1;

uniform float2 fUIMeshEdgesStrength <
    ui_type = "drag";
    ui_category = UI_CATEGORY_MESH_EDGES;
    ui_label = UI_EDGES_LABEL_STRENGTH;
    ui_min = 0.01; ui_max = 10.0;
    ui_step = 0.01;
> = float2(3.0, 3.0);

#define COMIC_MESHEDGES_ITERATIONS_MAX 3

uniform int iUIMeshEdgesIterations <
    ui_type = "slider";
    ui_category = UI_CATEGORY_MESH_EDGES;
    ui_label = "Line Width";
    ui_min = 1; ui_max = COMIC_MESHEDGES_ITERATIONS_MAX;
    ui_step = 0.01;
> = 1;

uniform float3 fUIMeshEdgesDistanceFading<
    ui_type = "drag";
    ui_category = UI_CATEGORY_MESH_EDGES;
    ui_label = UI_EDGES_LABEL_DISTANCE_STRENGTH;
    ui_tooltip = UI_EDGES_TOOLTIP_DISTANCE_STRENGTH;
    ui_min = -1.0; ui_max = 1.0;
    ui_step = 0.001;
> = float3(-1.0, 0.1, 0.8);

uniform bool bUIMeshEdgesDebugLayer <
    ui_label = UI_EDGES_LABEL_DEBUG;
    ui_category = UI_CATEGORY_MESH_EDGES;
> = true;

////////////////////////// Misc //////////////////////////
uniform float3 fUIEdgesLumaWeight <
    ui_type = "drag";
    ui_category = UI_CATEGORY_MISC;
    ui_label = "Luma";
    ui_tooltip = UI_EDGES_TOOLTIP_WEIGHTS;
    ui_min = -1.0; ui_max = 1.0;
    ui_step = 0.001;
> = float3(0.0, 1.0, 0.8);

uniform float3 fUIEdgesSaturationWeight <
    ui_type = "drag";
    ui_category = UI_CATEGORY_MISC;
    ui_label = "Saturation";
    ui_tooltip = UI_EDGES_TOOLTIP_WEIGHTS;
    ui_min = -1.0; ui_max = 1.0;
    ui_step = 0.001;
> = float3(0.0, 1.0, 0.8);

////////////////////////// Debug //////////////////////////
uniform bool bUIEnableDebugLayer <
    ui_label = "Enable Debug Layer";
    ui_category = UI_CATEGORY_DEBUG;
> = false;

uniform int iUIShowFadingOverlay <
    ui_type = "combo";
    ui_category = UI_CATEGORY_DEBUG;
    ui_label = "Weight Overlay";
    ui_items = "None\0Luma Edges\0Chroma Edges\0Outlines\0Mesh Edges\0Luma\0Saturation\0";
> = 0;

uniform float3 fUIOverlayColor<
    ui_type = "color";
    ui_category = UI_CATEGORY_DEBUG;
    ui_label = "Overlay Color";
> = float3(1.0, 0.0, 0.0);

////////////////////////// Effect //////////////////////////
uniform float3 fUIColor <
    ui_type = "color";
    ui_category = UI_CATEGORY_EFFECT;
    ui_label = "Color";
> = float3(0.0, 0.0, 0.0);

uniform float fUIStrength <
    ui_type = "drag";
    ui_category = UI_CATEGORY_EFFECT;
    ui_label = "Strength";
    ui_min = 0.0; ui_max = 1.0;
> = 1.0;

/******************************************************************************
    Textures
******************************************************************************/
namespace Comic {
     /******************************************************************************
        Functions
    ******************************************************************************/
    float Convolution(float4 values1, float4 values2, int type, float detail) {
        static const float4 Sobel_X1       = float4(  0.0, -1.0, -2.0, -1.0);
        static const float4 Sobel_X2       = float4(  0.0,  1.0,  2.0,  1.0);
        static const float4 Sobel_Y1       = float4(  2.0,  1.0,  0.0, -1.0);
        static const float4 Sobel_Y2       = float4( -2.0, -1.0,  0.0,  1.0);
        static const float4 Sobel_X_M1     = float4(  0.0,  1.0,  2.0,  1.0);
        static const float4 Sobel_X_M2     = float4(  0.0, -1.0, -2.0, -1.0);
        static const float4 Sobel_Y_M1     = float4( -2.0, -1.0,  0.0,  1.0);
        static const float4 Sobel_Y_M2     = float4(  2.0,  1.0,  0.0, -1.0);
        static const float4 Scharr_X1      = float4(  0.0, -3.0,-10.0, -3.0);
        static const float4 Scharr_X2      = float4(  0.0,  3.0, 10.0,  3.0);
        static const float4 Scharr_Y1      = float4( 10.0,  3.0,  0.0, -3.0);
        static const float4 Scharr_Y2      = float4(-10.0, -3.0,  0.0,  3.0);
        static const float4 Scharr_X_M1    = float4(  0.0,  3.0, 10.0,  3.0);
        static const float4 Scharr_X_M2    = float4(  0.0, -3.0,-10.0, -3.0);
        static const float4 Scharr_Y_M1    = float4(-10.0, -3.0,  0.0,  3.0);
        static const float4 Scharr_Y_M2    = float4(-10.0,  3.0,  0.0, -3.0);

        float retVal;

        /******************************************************************************
            Edge detection type 2: Edge detection by convolution
            This is basically just convolution: Multiply all the pixels with
            the corresponding value from the kernel and add all of them together.
            In order to be able to have control over the detail (detect less/more edges)
            the kernel used is a linear interpolation between the Sobel- and Scharr-Operator.
            Gives the image some sort of embossed look.
        ******************************************************************************/
        float4 cX1 = values1 * lerp(Sobel_X1, Scharr_X1, detail);
        float4 cX2 = values2 * lerp(Sobel_X2, Scharr_X2, detail);
        float  cX  = cX1.x + cX1.y + cX1.z + cX1.w + cX2.x + cX2.y + cX2.z + cX2.w;
        float4 cY1 = values1 * lerp(Sobel_Y1, Scharr_Y1, detail);
        float4 cY2 = values2 * lerp(Sobel_Y2, Scharr_Y2, detail);
        float  cY  = cY1.x + cY1.y + cY1.z + cY1.w + cY2.x + cY2.y + cY2.z + cY2.w;
        retVal = max(cX, cY);
        if(type == 3)
        {
            /******************************************************************************
                Edge detection type 3:
                Same as above but the kernels are flipped.
                Adds more edges.
            ******************************************************************************/
            float4 cX1 = values1 * lerp(Sobel_X_M1, Scharr_X_M1, detail);
            float4 cX2 = values2 * lerp(Sobel_X_M2, Scharr_X_M2, detail);
            float  cX  = cX1.x + cX1.y + cX1.z + cX1.w + cX2.x + cX2.y + cX2.z + cX2.w;
            float4 cY1 = values1 * lerp(Sobel_Y_M1, Scharr_Y_M1, detail);
            float4 cY2 = values2 * lerp(Sobel_Y_M2, Scharr_Y_M2, detail);
            float  cY  = cY1.x + cY1.y + cY1.z + cY1.w + cY2.x + cY2.y + cY2.z + cY2.w;
            retVal = max(retVal, max(cX, cY));
        }
        return retVal;
    }

    float MeshEdges(float depthC, float4 depth1, float4 depth2) {
            /******************************************************************************
                Outlines type 2:
                This method calculates how flat the plane around the center pixel is.
                Can be used to draw the polygon edges of a mesh and its outline.
            ******************************************************************************/
            float depthCenter = depthC;
            float4 depthCardinal = float4(depth1.x, depth2.x, depth1.z, depth2.z);
            float4 depthInterCardinal = float4(depth1.y, depth2.y, depth1.w, depth2.w);
            //Calculate the min and max depths
            float2 mind = float2(MIN4(depthCardinal), MIN4(depthInterCardinal));
            float2 maxd = float2(MAX4(depthCardinal), MAX4(depthInterCardinal));
            float span = MAX2(maxd) - MIN2(mind) + 0.00001;

            //Normalize values
            depthCenter /= span;
            depthCardinal /= span;
            depthInterCardinal /= span;
            //Calculate the (depth-wise) distance of the surrounding pixels to the center
            float4 diffsCardinal = abs(depthCardinal - depthCenter);
            float4 diffsInterCardinal = abs(depthInterCardinal - depthCenter);
            //Calculate the difference of the (opposing) distances
            float2 meshEdge = float2(
                max(abs(diffsCardinal.x - diffsCardinal.y), abs(diffsCardinal.z - diffsCardinal.w)),
                max(abs(diffsInterCardinal.x - diffsInterCardinal.y), abs(diffsInterCardinal.z - diffsInterCardinal.w))
            );

            return MAX2(meshEdge);
    }

    float4 EdgeDetection(sampler s, int2 vpos, float2 texcoord,
                         int luma_type, float luma_detail,
                         int chroma_type, float chroma_detail,
                         int outlines_enable, int mesh_edges_enable) {
        float4 retVal;

        /******************************************************************************
            1. Gather the color of the current pixel and its eight neighbours.
            2. Calculate the luma of all nine pixels
            3. Calculate the chroma of all nine pixels and save that colors value
            4. Get all nine depth values
        ******************************************************************************/
        float3 colorC = tex2Dfetch(s, int4(vpos, 0, 0)).rgb;//C
        float3 color1[4] = {
            tex2Dfetch(s, int4(vpos + int2( 0, -1), 0, 0)).rgb,//N
            tex2Dfetch(s, int4(vpos + int2( 1, -1), 0, 0)).rgb,//NE
            tex2Dfetch(s, int4(vpos + int2( 1,  0), 0, 0)).rgb,//E
            tex2Dfetch(s, int4(vpos + int2( 1,  1), 0, 0)).rgb,//SE
        };
        float3 color2[4] = {    
            tex2Dfetch(s, int4(vpos + int2( 0,  1), 0, 0)).rgb,//S
            tex2Dfetch(s, int4(vpos + int2(-1,  1), 0, 0)).rgb,//SW
            tex2Dfetch(s, int4(vpos + int2(-1,  0), 0, 0)).rgb,//W
            tex2Dfetch(s, int4(vpos + int2(-1, -1), 0, 0)).rgb //NW
        };

        float lumaC = dot(colorC, LumaCoeff);
        float4 luma1 = float4(
            dot(color1[0], LumaCoeff),
            dot(color1[1], LumaCoeff),
            dot(color1[2], LumaCoeff),
            dot(color1[3], LumaCoeff)
        );
        float4 luma2 = float4(
            dot(color2[0], LumaCoeff),
            dot(color2[1], LumaCoeff),
            dot(color2[2], LumaCoeff),
            dot(color2[3], LumaCoeff)
        );

        float chromaVC = dot(colorC - lumaC.xxx, LumaCoeff);
        float4 chromaV1 = float4(
            MAX3((color1[0] - luma1.xxx)),
            MAX3((color1[1] - luma1.yyy)),
            MAX3((color1[2] - luma1.zzz)),
            MAX3((color1[3] - luma1.www))
        );
        float4 chromaV2 = float4(
            MAX3((color2[0] - luma2.xxx)),
            MAX3((color2[1] - luma2.yyy)),
            MAX3((color2[2] - luma2.zzz)),
            MAX3((color2[3] - luma2.www))
        );

        float2 pix = ReShade::PixelSize;
        float depthC = ReShade::GetLinearizedDepth(texcoord);//C
        float4 depth1[COMIC_MESHEDGES_ITERATIONS_MAX];
        float4 depth2[COMIC_MESHEDGES_ITERATIONS_MAX];

        int iterations = clamp(iUIMeshEdgesIterations, 1, COMIC_MESHEDGES_ITERATIONS_MAX);
        [unroll]
        for(int i = 0; i < iterations; i++)
        {
            depth1[i] = float4(
                ReShade::GetLinearizedDepth(texcoord + (i + 1.0) * float2(   0.0, -pix.y)),//N
                ReShade::GetLinearizedDepth(texcoord + (i + 1.0) * float2( pix.x, -pix.y)),//NE
                ReShade::GetLinearizedDepth(texcoord + (i + 1.0) * float2( pix.x,    0.0)),//E
                ReShade::GetLinearizedDepth(texcoord + (i + 1.0) * float2( pix.x,  pix.y))//SE
            );
            depth2[i]  = float4(
                ReShade::GetLinearizedDepth(texcoord + (i + 1.0) * float2(   0.0,  pix.y)),//S
                ReShade::GetLinearizedDepth(texcoord + (i + 1.0) * float2(-pix.x,  pix.y)),//SW
                ReShade::GetLinearizedDepth(texcoord + (i + 1.0) * float2(-pix.x,    0.0)), //W
                ReShade::GetLinearizedDepth(texcoord + (i + 1.0) * float2(-pix.x, -pix.y)) //NW
            );
        }

        if(luma_type == 1)
        {
            //******************************************************************************
            //    Edge detection type 1:
            //    1. Calculate the difference of the pixels that oppose each other
            //       (e.g. Difference of the pixel north and south of the center)
            //    2. Add all diffs together and weight the result with the center pixel
            //******************************************************************************
            float4 diffsLuma = abs(luma1 - luma2);
            retVal.x = (diffsLuma.x + diffsLuma.y + diffsLuma.z + diffsLuma.w) * (1.0 - lumaC);
        }
        else if(luma_type > 1)
        {
            retVal.x = Comic::Convolution(luma1, luma2, luma_type, luma_detail);
        }

        //Same as above but with the value of the croma as source
        if(chroma_type == 1)
        {
            float4 diffsChromaLuma = abs(chromaV1 - chromaV2);
            retVal.y = (diffsChromaLuma.x + diffsChromaLuma.y + diffsChromaLuma.z + diffsChromaLuma.w) * (1.0 - chromaVC);
        }
        else if(chroma_type > 1)
        {
            retVal.y = Comic::Convolution(chromaV1, chromaV2, chroma_type, chroma_detail);
        }

        if(outlines_enable == 1)
        {
            /******************************************************************************
                Outlines type 1:
                1. Calculate the normal vector of the current pixel
                2. Calculate how much the normal vector differs from the view direction.
            ******************************************************************************/
            float3 vertCenter = float3(texcoord, depthC);
            float3 vertNorth = float3(texcoord + float2(0.0, -pix.y), depth1[0].x);
            float3 vertEast = float3(texcoord + float2(pix.x, 0.0), depth1[0].z);
            retVal.z = (1.0 - saturate(normalize(cross(vertCenter - vertNorth, vertCenter - vertEast)) * 0.5 + 0.5)).z;
        }
        else if(outlines_enable == 2)
        {
            float maxDiff = max(MAX4((depth1[0])), MAX4((depth2[0]))) - min(MIN4((depth1[0])), MIN4((depth2[0])));
            retVal.z = maxDiff < fUIOutlinesThreshold / RESHADE_DEPTH_LINEARIZATION_FAR_PLANE ? 0.0 : 1.0;
        }

        if(mesh_edges_enable)
        {
            for(int i = 0; i < iUIMeshEdgesIterations; i++)
            {
                retVal.w = max(retVal.w, MeshEdges(depthC, depth1[i], depth2[i]));
            }
        }

        return saturate(retVal);
    }

    float StrengthCurve(float3 fade, float depth) {
        float curveMin = smoothstep(0.0, 1.0 - fade.z, depth + (0.2 - 1.2 * fade.x));
        float curveMax = smoothstep(0.0, 1.0 - fade.z, 1.0 - depth + (1.2 * fade.y - 1.0));
        return curveMin * curveMax;
    }

    /******************************************************************************
        Pixel Shader
    ******************************************************************************/
    float3 Sketch_PS(float4 vpos : SV_Position, float2 texcoord : TexCoord) : SV_Target {
        float3 color = tex2D(ReShade::BackBuffer, texcoord).rgb;
        float currentDepth = ReShade::GetLinearizedDepth(texcoord);
        float4 edges = EdgeDetection(ReShade::BackBuffer, 
                                     vpos.xy,
                                     texcoord,
                                     iUIColorEdgesType,
                                     fUIColorEdgesDetails,
                                     iUIChromaEdgesType,
                                     fUIChromaEdgesDetails,
                                     iUIOutlinesEnable,
                                     iUIMeshEdgesEnable);
        
        edges = float4(
            pow(edges.x, fUIColorEdgesStrength.x) * fUIColorEdgesStrength.y,
            pow(edges.y, fUIChromaEdgesStrength.x) * fUIChromaEdgesStrength.y,
            pow(edges.z, fUIOutlinesStrength.x) * fUIOutlinesStrength.y,
            pow(edges.w, fUIMeshEdgesStrength.x) * fUIMeshEdgesStrength.y
        );

        float2 fadeAll =  float2(   
            StrengthCurve(fUIEdgesLumaWeight, dot(color, LumaCoeff)),
            StrengthCurve(fUIEdgesSaturationWeight, MAX3(color) - MIN3(color))
        );

        float4 fadeDist = float4(
            StrengthCurve(fUIColorEdgesDistanceFading, currentDepth),
            StrengthCurve(fUIChromaEdgesDistanceFading, currentDepth),
            StrengthCurve(fUIOutlinesDistanceFading, currentDepth),
            StrengthCurve(fUIMeshEdgesDistanceFading, currentDepth)
        );

        edges *= fadeDist * MIN2(fadeAll);
        
        float3 result = saturate(lerp(color, fUIColor, MAX4(edges) * fUIStrength));

        /******************************************************************************
            Debug Output
        ******************************************************************************/
        float3 edgeDebugLayer = 0.0.rrr;
        if(bUIEnableDebugLayer) {
            int4 enabled = int4(bUIColorEdgesDebugLayer,bUIChromaEdgesDebugLayer,bUIOutlinesDebugLayer,bUIMeshEdgesDebugLayer);
            edgeDebugLayer = MAX4((edges * enabled));

            if(iUIShowFadingOverlay != 0) {
                if(iUIShowFadingOverlay == 1)
                    edgeDebugLayer = lerp(fUIOverlayColor, edgeDebugLayer.rrr, fadeDist.x);
                else if(iUIShowFadingOverlay == 2)
                    edgeDebugLayer = lerp(fUIOverlayColor, edgeDebugLayer.rrr, fadeDist.y);
                else if(iUIShowFadingOverlay == 3)
                    edgeDebugLayer = lerp(fUIOverlayColor, edgeDebugLayer.rrr, fadeDist.z);
                else if(iUIShowFadingOverlay == 4)
                    edgeDebugLayer = lerp(fUIOverlayColor, edgeDebugLayer.rrr, fadeDist.w);
                else if(iUIShowFadingOverlay == 5)
                    edgeDebugLayer = lerp(fUIOverlayColor, edgeDebugLayer.rrr, fadeAll.x);
                else if(iUIShowFadingOverlay == 6)
                    edgeDebugLayer = lerp(fUIOverlayColor, edgeDebugLayer.rrr, fadeAll.y);
            }
            return edgeDebugLayer;
        }
        return result;
    }
}

technique Comic
{
    pass {
        VertexShader = PostProcessVS;
        PixelShader = Comic::Sketch_PS;
    }
}
