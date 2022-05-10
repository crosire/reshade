/*
    SMAA has three passes, chained together as follows:

    |input|
    v
    [SMAA*EdgeDetection]
    v
    |edgesTex|
    v
    [SMAABlendingWeightCalculation]
    v
    |blendTex|
    v
    [SMAANeighborhoodBlending]
    v
    |output|

    Copyright (C) 2013 Jorge Jimenez (jorge@iryoku.com)
    Copyright (C) 2013 Jose I. Echevarria (joseignacioechevarria@gmail.com)
    Copyright (C) 2013 Belen Masia (bmasia@unizar.es)
    Copyright (C) 2013 Fernando Navarro (fernandn@microsoft.com)
    Copyright (C) 2013 Diego Gutierrez (diegog@unizar.es)

    Permission is hereby granted, free of charge, to any person obtaining a copy
    this software and associated documentation files (the "Software"), to deal in
    the Software without restriction, including without limitation the rights to
    use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
    of the Software, and to permit persons to whom the Software is furnished to
    do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software. As clarification, there
    is no requirement that the copyright notice and permission be included in
    binary distributions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
    SOFTWARE.
*/

#define SMAA_RT_METRICS float4(1.0 / BUFFER_WIDTH, 1.0 / BUFFER_HEIGHT, BUFFER_WIDTH, BUFFER_HEIGHT)
#define SMAA_THRESHOLD 0.1
#define SMAA_MAX_SEARCH_STEPS 8

/*
    If there is an neighbor edge that has SMAA_LOCAL_CONTRAST_FACTOR times
    bigger contrast than current edge, current edge will be discarded.

    This allows to eliminate spurious crossing edges, and is based on the fact
    that, if there is too much contrast in a direction, that will hide
    perceptually contrast in the other neighbors.
*/

#ifndef SMAA_LOCAL_CONTRAST_ADAPTATION_FACTOR
    #define SMAA_LOCAL_CONTRAST_ADAPTATION_FACTOR 2.0
#endif

// Non-Configurable Defines

#define SMAA_AREATEX_MAX_DISTANCE 16
#define SMAA_AREATEX_MAX_DISTANCE_DIAG 20
#define SMAA_AREATEX_PixelSize (1.0 / float2(160.0, 560.0))
#define SMAA_AREATEX_SUBTEX_SIZE (1.0 / 7.0)
#define SMAA_SEARCHTEX_SIZE float2(66.0, 33.0)
#define SMAA_SEARCHTEX_PACKED_SIZE float2(64.0, 16.0)
#define SMAA_CORNER_ROUNDING_NORM (float(SMAA_CORNER_ROUNDING) / 100.0)

/* Misc functions */

/* Conditional move: */

void SMAA_Movc(bool2 Cond, inout float2 Variable, float2 Value)
{
    [flatten] if (Cond.x) Variable.x = Value.x;
    [flatten] if (Cond.y) Variable.y = Value.y;
}

void SMAA_Movc(bool4 Cond, inout float4 Variable, float4 Value)
{
    SMAA_Movc(Cond.xy, Variable.xy, Value.xy);
    SMAA_Movc(Cond.zw, Variable.zw, Value.zw);
}

/*
    Is_Horizontal/Vertical Search Functions

    This allows to determine how much length should we add in the last step
    of the searches. It takes the bilinearly interpolated edge (see
    @PSEUDO_GATHER4), and adds 0, 1 or 2, depEnding on which edges and
    crossing edges are active.
*/

float SMAA_Search_Length(sampler2D Search_Tex, float2 E, float Offset)
{
    // The texture is flipped vertically, with left and right cases taking half
    // of the space horizontally:
    float2 Scale = SMAA_SEARCHTEX_SIZE * float2(0.5, -1.0);
    float2 Bias = SMAA_SEARCHTEX_SIZE * float2(Offset, 1.0);

    // Scale and bias to access texel centers:
    Scale += float2(-1.0,  1.0);
    Bias  += float2( 0.5, -0.5);

    // Convert from pixel coordinates to TexCoords:
    // (We use SMAA_SEARCHTEX_PACKED_SIZE because the texture is cropped)
    Scale *= 1.0 / SMAA_SEARCHTEX_PACKED_SIZE;
    Bias *= 1.0 / SMAA_SEARCHTEX_PACKED_SIZE;

    // Lookup the search texture:
    return tex2Dlod(Search_Tex, mad(Scale, E, Bias).xyxy).r;
}

/* Is_Horizontal/vertical search functions for the 2nd pass. */

float SMAASearchXLeft(sampler2D Edges_Tex, sampler2D Search_Tex, float2 TexCoord, float End)
{
    /*
        @PSEUDO_GATHER4
        This TexCoord has been Offset by (-0.25, -0.125) in the vertex shader to
        sample between edge, thus fetching four edges in a row.
        Sampling with different offsets in each direction allows to disambiguate
        which edges are active from the four fetched ones.
    */

    float2 E = float2(0.0, 1.0);

    while (TexCoord.x > End &&
           E.g > 0.8281 && // Is there some edge not activated?
           E.r == 0.0) // Or is there a crossing edge that breaks the line?
    {
        E = tex2Dlod(Edges_Tex, TexCoord.xyxy).rg;
        TexCoord = mad(-float2(2.0, 0.0), SMAA_RT_METRICS.xy, TexCoord);
    }

    float Offset = mad(-(255.0 / 127.0), SMAA_Search_Length(Search_Tex, E, 0.0), 3.25);
    return mad(SMAA_RT_METRICS.x, Offset, TexCoord.x);
}

float SMAASearchXRight(sampler2D Edges_Tex, sampler2D Search_Tex, float2 TexCoord, float End)
{
    float2 E = float2(0.0, 1.0);

    while (TexCoord.x < End &&
           E.g > 0.8281 && // Is there some edge not activated?
           E.r == 0.0) // Or is there a crossing edge that breaks the line?
    {
        E = tex2Dlod(Edges_Tex, TexCoord.xyxy).rg;
        TexCoord = mad(float2(2.0, 0.0), SMAA_RT_METRICS.xy, TexCoord);
    }

    float Offset = mad(-(255.0 / 127.0), SMAA_Search_Length(Search_Tex, E, 0.5), 3.25);
    return mad(-SMAA_RT_METRICS.x, Offset, TexCoord.x);
}

float SMAASearchYUp(sampler2D Edges_Tex, sampler2D Search_Tex, float2 TexCoord, float End)
{
    float2 E = float2(1.0, 0.0);

    while (TexCoord.y > End &&
           E.r > 0.8281 && // Is there some edge not activated?
           E.g == 0.0) // Or is there a crossing edge that breaks the line?
    {
        E = tex2Dlod(Edges_Tex, TexCoord.xyxy).rg;
        TexCoord = mad(-float2(0.0, 2.0), SMAA_RT_METRICS.xy, TexCoord);
    }

    float Offset = mad(-(255.0 / 127.0), SMAA_Search_Length(Search_Tex, E.gr, 0.0), 3.25);
    return mad(SMAA_RT_METRICS.y, Offset, TexCoord.y);
}

float SMAASearchYDown(sampler2D Edges_Tex, sampler2D Search_Tex, float2 TexCoord, float End)
{
    float2 E = float2(1.0, 0.0);

    while (TexCoord.y < End &&
           E.r > 0.8281 && // Is there some edge not activated?
           E.g == 0.0) // Or is there a crossing edge that breaks the line?
    {
        E = tex2Dlod(Edges_Tex, TexCoord.xyxy).rg;
        TexCoord = mad(float2(0.0, 2.0), SMAA_RT_METRICS.xy, TexCoord);
    }

    float Offset = mad(-(255.0 / 127.0), SMAA_Search_Length(Search_Tex, E.gr, 0.5), 3.25);
    return mad(-SMAA_RT_METRICS.y, Offset, TexCoord.y);
}

/*
    Ok, we have the distance and both crossing edges. So, what are the areas
    at each side of current edge?
*/

float2 SMAAArea(sampler2D Area_Tex, float2 Dist, float E_1, float E_2, float Offset)
{
    // Rounding prevents precision errors of bilinear filtering:
    float2 TexCoord = mad(float2(SMAA_AREATEX_MAX_DISTANCE, SMAA_AREATEX_MAX_DISTANCE), round(4.0 * float2(E_1, E_2)), Dist);

    // We do a scale and bias for mapping to texel space:
    TexCoord = mad(SMAA_AREATEX_PixelSize, TexCoord, 0.5 * SMAA_AREATEX_PixelSize);

    // Move to proper place, according to the subpixel Offset:
    TexCoord.y = mad(SMAA_AREATEX_SUBTEX_SIZE, Offset, TexCoord.y);

    // Do it!
    return tex2Dlod(Area_Tex, TexCoord.xyxy).rg;
}
