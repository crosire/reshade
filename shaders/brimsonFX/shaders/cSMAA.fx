
/*
    Copyright (C) 2013 Jorge Jimenez (jorge@iryoku.com)
    Copyright (C) 2013 Jose I. Echevarria (joseignacioechevarria@gmail.com)
    Copyright (C) 2013 Belen Masia (bmasia@unizar.es)
    Copyright (C) 2013 Fernando Navarro (fernandn@microsoft.com)
    Copyright (C) 2013 Diego Gutierrez (diegog@unizar.es)

    Permission is hereby granted, free of charge, to any person obtaining a copy
    this software and associated documentation files (the "Software"), to deal in
    the Software without restriction, including without limitation the rights to
    use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
    the Software, and to permit persons to whom the Software is furnished to do so,
    subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software. As clarification, there is no
    requirement that the copyright notice and permission be included in binary
    distributions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
    FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
    COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
    IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
    CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

/*
    Lite SMAA port for ReShade 4.0+
    - Color and medium setting exclusive
    - Depth rendertarget yoinked
    - Stripped so I can learn about AA better
*/

#include "cSMAA.fxh"

texture2D Color_Tex : COLOR;

sampler2D Color_Linear_Sampler
{
    Texture = Color_Tex;
    #if BUFFER_COLOR_BIT_DEPTH == 8
        SRGBTexture = TRUE;
    #endif
};

texture2D Edges_Tex < pooled = true; >
{
    Width = BUFFER_WIDTH;
    Height = BUFFER_HEIGHT;
    Format = RG8;
};

sampler2D Edges_Sampler
{
    Texture = Edges_Tex;
};

texture2D Blend_Tex < pooled = true; >
{
    Width = BUFFER_WIDTH;
    Height = BUFFER_HEIGHT;
    Format = RGBA8;
};

sampler2D Blend_Sampler
{
    Texture = Blend_Tex;
};

texture2D Area_Tex < source = "AreaTex.dds"; >
{
    Width = 160;
    Height = 560;
    Format = RG8;
};

sampler2D Area_Sampler
{
    Texture = Area_Tex;
};

texture2D Search_Tex < source = "SearchTex.dds"; >
{
    Width = 64;
    Height = 16;
    Format = R8;
};

sampler2D Search_Sampler
{
    Texture = Search_Tex;
    MipFilter = POINT;
    MinFilter = POINT;
    MagFilter = POINT;
};

/*
    Color Edge Detection Pixel Shaders (First Pass)

    IMPORTANT NOTICE: color edge detection requires gamma-corrected colors, and
    thus 'Color_Tex' should be a non-sRGB texture.
*/

struct V2F_1
{
    float4 Pos : SV_POSITION;
    float2 Coord_0 : TEXCOORD0;
    float4 Coord_1[3] : TEXCOORD1;
};

V2F_1 SMAAEdgeDetectionWrapVS(in uint ID : SV_VERTEXID)
{
    V2F_1 Output;

    float2 TexCoord;
    TexCoord.x = (ID == 2) ? 2.0 : 0.0;
    TexCoord.y = (ID == 1) ? 2.0 : 0.0;
    Output.Pos = float4(TexCoord.xy * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);

    Output.Coord_0 = TexCoord;
    Output.Coord_1[0] = mad(SMAA_RT_METRICS.xyxy, float4(-1.0, 0.0, 0.0, -1.0), TexCoord.xyxy);
    Output.Coord_1[1] = mad(SMAA_RT_METRICS.xyxy, float4( 1.0, 0.0, 0.0,  1.0), TexCoord.xyxy);
    Output.Coord_1[2] = mad(SMAA_RT_METRICS.xyxy, float4(-2.0, 0.0, 0.0, -2.0), TexCoord.xyxy);
    return Output;
}

float2 SMAAEdgeDetectionWrapPS(V2F_1 Input) : SV_Target
{
    // Calculate the threshold:
    float2 Threshold = float2(SMAA_THRESHOLD, SMAA_THRESHOLD);

    // Calculate color deltas:
    float4 Delta;
    float3 C = tex2D(Color_Linear_Sampler, Input.Coord_0).rgb;

    float3 C_Left = tex2D(Color_Linear_Sampler, Input.Coord_1[0].xy).rgb;
    float3 T = abs(C - C_Left);
    Delta.x = max(max(T.r, T.g), T.b);

    float3 C_Top  = tex2D(Color_Linear_Sampler, Input.Coord_1[0].zw).rgb;
    T = abs(C - C_Top);
    Delta.y = max(max(T.r, T.g), T.b);

    // We do the usual Threshold:
    float2 Edges = step(Threshold, Delta.xy);

    // Then discard if there is no edge:
    if (dot(Edges, float2(1.0, 1.0)) == 0.0) discard;

    // Calculate right and bottom deltas:
    float3 C_Right = tex2D(Color_Linear_Sampler, Input.Coord_1[1].xy).rgb;
    T = abs(C - C_Right);
    Delta.z = max(max(T.r, T.g), T.b);

    float3 C_Bottom  = tex2D(Color_Linear_Sampler, Input.Coord_1[1].zw).rgb;
    T = abs(C - C_Bottom);
    Delta.w = max(max(T.r, T.g), T.b);

    // Calculate the maximum delta in the direct neighborhood:
    float2 Max_Delta = max(Delta.xy, Delta.zw);

    // Calculate left-left and top-top deltas:
    float3 C_Left_Left  = tex2D(Color_Linear_Sampler, Input.Coord_1[2].xy).rgb;
    T = abs(C_Left - C_Left_Left);
    Delta.z = max(max(T.r, T.g), T.b);

    float3 C_Top_Top = tex2D(Color_Linear_Sampler, Input.Coord_1[2].zw).rgb;
    T = abs(C_Top - C_Top_Top);
    Delta.w = max(max(T.r, T.g), T.b);

    // Calculate the final maximum Delta:
    Max_Delta = max(Max_Delta.xy, Delta.zw);
    float Final_Delta = max(Max_Delta.x, Max_Delta.y);

    // Local contrast adaptation:
    Edges.xy *= step(Final_Delta, SMAA_LOCAL_CONTRAST_ADAPTATION_FACTOR * Delta.xy);

    return Edges;
}

/* Blending Weight Calculation Pixel Shader (Second Pass) */

struct V2F_2
{
    float4 Pos : SV_POSITION;
    float4 Coord_0 : TEXCOORD0;
    float4 Coord_1[3] : TEXCOORD1;
};

V2F_2 SMAABlendingWeightCalculationWrapVS(in uint ID : SV_VERTEXID)
{
    V2F_2 Output;
    float2 TexCoord;
    TexCoord.x = (ID == 2) ? 2.0 : 0.0;
    TexCoord.y = (ID == 1) ? 2.0 : 0.0;
    Output.Pos = float4(TexCoord.xy * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);

    Output.Coord_0.xy = TexCoord;
    Output.Coord_0.zw = Output.Coord_0.xy * SMAA_RT_METRICS.zw;

    // We will use these offsets for the searches later on (see @PSEUDO_GATHER4):
    Output.Coord_1[0] = mad(SMAA_RT_METRICS.xyxy, float4(-0.25, -0.125,  1.25, -0.125), TexCoord.xyxy);
    Output.Coord_1[1] = mad(SMAA_RT_METRICS.xyxy, float4(-0.125, -0.25, -0.125,  1.25), TexCoord.xyxy);

    // And these for the searches, they indicate the ends of the loops:
    Output.Coord_1[2] = mad(SMAA_RT_METRICS.xxyy,
                            float2(-2.0, 2.0).xyxy * float(SMAA_MAX_SEARCH_STEPS),
                            float4(Output.Coord_1[0].xz, Output.Coord_1[1].yw));
    return Output;
}

float4 SMAABlendingWeightCalculationWrapPS(V2F_2 Input) : SV_Target
{
    float4 Weights = float4(0.0, 0.0, 0.0, 0.0);
    float2 E = tex2D(Edges_Sampler, Input.Coord_0.xy).rg;

    [branch]
    if (E.g > 0.0) // Edge at north
    {
        float2 D;

        // Find the distance to the left:
        float3 TexCoords;
        TexCoords.x = SMAASearchXLeft(Edges_Sampler, Search_Sampler, Input.Coord_1[0].xy, Input.Coord_1[2].x);
        TexCoords.y = Input.Coord_1[1].y;
        D.x = TexCoords.x;

        // Now fetch the left crossing Edges, two at a time using bilinear
        // filtering. Sampling at -0.25 (see @CROSSING_OFFSET) enables to
        // discern what value each edge has:
        float E_1 = tex2Dlod(Edges_Sampler, TexCoords.xyxy).r;

        // Find the distance to the right:
        TexCoords.z = SMAASearchXRight(Edges_Sampler, Search_Sampler, Input.Coord_1[0].zw, Input.Coord_1[2].y);
        D.y = TexCoords.z;

        // We want the distances to be in pixel units (doing this here allow to
        // better interleave arithmetic and memory accesses):
        D = abs(round(mad(SMAA_RT_METRICS.zz, D, -Input.Coord_0.zz)));

        // SMAAArea below needs a sqrt, as the areas texture is compressed
        // quadratically:
        float2 Sqrt_D = sqrt(D);

        // Fetch the right crossing Edges:
        float E_2 = tex2Dlod(Edges_Sampler, TexCoords.zyzy, int2(1, 0)).r;

        // Ok, we know how this pattern looks like, now it is time for getting
        // the actual area:
        Weights.rg = SMAAArea(Area_Sampler, Sqrt_D, E_1, E_2, 0.0);

        // Fix corners:
        TexCoords.y = Input.Coord_0.y;
    }

    [branch]
    if (E.r > 0.0) // Edge at west
    {
        float2 D;

        // Find the distance to the top:
        float3 TexCoords;
        TexCoords.y = SMAASearchYUp(Edges_Sampler, Search_Sampler, Input.Coord_1[1].xy, Input.Coord_1[2].z);
        TexCoords.x = Input.Coord_1[0].x;
        D.x = TexCoords.y;

        // Fetch the top crossing Edges:
        float E_1 = tex2Dlod(Edges_Sampler, TexCoords.xyxy).g;

        // Find the distance to the bottom:
        TexCoords.z = SMAASearchYDown(Edges_Sampler, Search_Sampler, Input.Coord_1[1].zw, Input.Coord_1[2].w);
        D.y = TexCoords.z;

        // We want the distances to be in pixel units:
        D = abs(round(mad(SMAA_RT_METRICS.ww, D, -Input.Coord_0.ww)));

        // SMAAArea below needs a sqrt, as the areas texture is compressed
        // quadratically:
        float2 Sqrt_D = sqrt(D);

        // Fetch the bottom crossing Edges:
        float E_2 = tex2Dlod(Edges_Sampler, TexCoords.xzxz, int2(0, 1)).g;

        // Get the area for this direction:
        Weights.ba = SMAAArea(Area_Sampler, Sqrt_D, E_1, E_2, 0.0);

        // Fix corners:
        TexCoords.x = Input.Coord_0.x;
    }

    return Weights;
}

/* Neighborhood Blending Pixel Shader (Third Pass) */

struct v2f_3
{
    float4 Pos : SV_POSITION;
    float2 Coord_0  : TEXCOORD0;
    float4 Coord_1  : TEXCOORD1;
};

v2f_3 SMAANeighborhoodBlendingWrapVS(in uint ID : SV_VERTEXID)
{
    v2f_3 Output;
    float2 TexCoord;
    TexCoord.x = (ID == 2) ? 2.0 : 0.0;
    TexCoord.y = (ID == 1) ? 2.0 : 0.0;
    Output.Pos = float4(TexCoord.xy * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);

    Output.Coord_0 = TexCoord;
    Output.Coord_1 = mad(SMAA_RT_METRICS.xyxy, float4( 1.0, 0.0, 0.0,  1.0), TexCoord.xyxy);
    return Output;
}

float4 SMAANeighborhoodBlendingWrapPS(v2f_3 Input) : SV_Target
{
    // Fetch the blending Weights for current pixel:
    float4 A;
    A.x = tex2D(Blend_Sampler, Input.Coord_1.xy).a; // Right
    A.y = tex2D(Blend_Sampler, Input.Coord_1.zw).g; // Top
    A.wz = tex2D(Blend_Sampler, Input.Coord_0).xz; // Bottom / Left

    // Is there any blending weight with a value greater than 0.0?

    [branch]
    if (dot(A, float4(1.0, 1.0, 1.0, 1.0)) < 1e-5)
    {
        return tex2Dlod(Color_Linear_Sampler, Input.Coord_0.xyxy);
    } 
    else 
    {
        bool H = max(A.x, A.z) > max(A.y, A.w); // max(horizontal) > max(vertical)

        // Calculate the blending offsets:
        float4 Blending_Offset = float4(0.0, A.y, 0.0, A.w);
        float2 Blending_Weight = A.yw;
        SMAA_Movc(bool4(H, H, H, H), Blending_Offset, float4(A.x, 0.0, A.z, 0.0));
        SMAA_Movc(bool2(H, H), Blending_Weight, A.xz);
        Blending_Weight /= dot(Blending_Weight, float2(1.0, 1.0));

        // Calculate the texture coordinates:
        float4 Blending_Coord = mad(Blending_Offset, float4(SMAA_RT_METRICS.xy, -SMAA_RT_METRICS.xy), Input.Coord_0.xyxy);

        // We exploit bilinear filtering to mix current pixel with the chosen neighbor:
        float4 Color = Blending_Weight.x * tex2Dlod(Color_Linear_Sampler, Blending_Coord.xyxy);
        Color += Blending_Weight.y * tex2Dlod(Color_Linear_Sampler, Blending_Coord.zwzw);
        return Color;
    }
}

technique SMAA
{
    pass EdgeDetectionPass
    {
        VertexShader = SMAAEdgeDetectionWrapVS;
        PixelShader = SMAAEdgeDetectionWrapPS;
        RenderTarget = Edges_Tex;
        ClearRenderTargets = TRUE;
        StencilEnable = TRUE;
        StencilPass = REPLACE;
        StencilRef = 1;
    }

    pass BlendWeightCalculationPass
    {
        VertexShader = SMAABlendingWeightCalculationWrapVS;
        PixelShader = SMAABlendingWeightCalculationWrapPS;
        RenderTarget = Blend_Tex;
        ClearRenderTargets = TRUE;
        StencilEnable = TRUE;
        StencilPass = KEEP;
        StencilFunc = EQUAL;
        StencilRef = 1;
    }

    pass NeighborhoodBlendingPass
    {
        VertexShader = SMAANeighborhoodBlendingWrapVS;
        PixelShader = SMAANeighborhoodBlendingWrapPS;
        StencilEnable = FALSE;
        #if BUFFER_COLOR_BIT_DEPTH == 8
            SRGBWriteEnable = TRUE;
        #endif
    }
}
