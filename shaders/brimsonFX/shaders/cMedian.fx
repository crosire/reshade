
/*
    Simple 3x3 median shader

    BSD 3-Clause License

    Copyright (c) 2022, Paul Dang <brimson.net@gmail.com>
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:

    1. Redistributions of source code must retain the above copyright notice, this
    list of conditions and the following disclaimer.

    2. Redistributions in binary form must reproduce the above copyright notice,
    this list of conditions and the following disclaimer in the documentation
    and/or other materials provided with the distribution.

    3. Neither the name of the copyright holder nor the names of its
    contributors may be used to endorse or promote products derived from
    this software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
    AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
    FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
    DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
    SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
    CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
    OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
    OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

texture2D Render_Color : COLOR;

sampler2D Sample_Color
{
    Texture = Render_Color;
    MagFilter = LINEAR;
    MinFilter = LINEAR;
    MipFilter = LINEAR;
    #if BUFFER_COLOR_BIT_DEPTH == 8
        SRGBTexture = TRUE;
    #endif
};

void Median_Offsets(in float2 TexCoord, in float2 PixelSize, out float4 SampleOffsets[3])
{
    // Sample locations:
    // [0].xy [1].xy [2].xy
    // [0].xz [1].xz [2].xz
    // [0].xw [1].xw [2].xw
    SampleOffsets[0] = TexCoord.xyyy + (float4(-1.0, 1.0, 0.0, -1.0) * PixelSize.xyyy);
    SampleOffsets[1] = TexCoord.xyyy + (float4(0.0, 1.0, 0.0, -1.0) * PixelSize.xyyy);
    SampleOffsets[2] = TexCoord.xyyy + (float4(1.0, 1.0, 0.0, -1.0) * PixelSize.xyyy);
}

void Basic_VS(in uint ID : SV_VERTEXID, out float4 Position : SV_POSITION, out float2 TexCoord : TEXCOORD0)
{
    TexCoord.x = (ID == 2) ? 2.0 : 0.0;
    TexCoord.y = (ID == 1) ? 2.0 : 0.0;
    Position = TexCoord.xyxy * float4(2.0, -2.0, 0.0, 0.0) + float4(-1.0, 1.0, 0.0, 1.0);
}

void Median_VS(in uint ID : SV_VERTEXID, out float4 Position : SV_POSITION, out float4 Offsets[3] : TEXCOORD0)
{
    float2 CoordVS = 0.0;
    Basic_VS(ID, Position, CoordVS);
    Median_Offsets(CoordVS, 1.0 / (float2(BUFFER_WIDTH, BUFFER_HEIGHT)), Offsets);
}

// Math functions: https://github.com/microsoft/DirectX-Graphics-Samples/blob/master/MiniEngine/Core/Shaders/DoFMedianFilterCS.hlsl

float4 Max_3(float4 A, float4 B, float4 C)
{
    return max(max(A, B), C);
}

float4 Min_3(float4 A, float4 B, float4 C)
{
    return min(min(A, B), C);
}

float4 Median_3(float4 A, float4 B, float4 C)
{
    return clamp(A, min(B, C), max(B, C));
}

float4 Median_9(float4 X0, float4 X1, float4 X2,
                float4 X3, float4 X4, float4 X5,
                float4 X6, float4 X7, float4 X8)
{
    float4 A = Max_3(Min_3(X0, X1, X2), Min_3(X3, X4, X5), Min_3(X6, X7, X8));
    float4 B = Min_3(Max_3(X0, X1, X2), Max_3(X3, X4, X5), Max_3(X6, X7, X8));
    float4 C = Median_3(Median_3(X0, X1, X2), Median_3(X3, X4, X5), Median_3(X6, X7, X8));
    return Median_3(A, B, C);
}

void Median_PS(in float4 Position : SV_POSITION, in float4 Offsets[3] : TEXCOORD0, out float4 OutputColor0 : SV_TARGET0)
{
    // Sample locations:
    // [0].xy [1].xy [2].xy
    // [0].xz [1].xz [2].xz
    // [0].xw [1].xw [2].xw
    float4 OutputColor = 0.0;
    float4 Sample[9];
    Sample[0] = tex2D(Sample_Color, Offsets[0].xy);
    Sample[1] = tex2D(Sample_Color, Offsets[1].xy);
    Sample[2] = tex2D(Sample_Color, Offsets[2].xy);
    Sample[3] = tex2D(Sample_Color, Offsets[0].xz);
    Sample[4] = tex2D(Sample_Color, Offsets[1].xz);
    Sample[5] = tex2D(Sample_Color, Offsets[2].xz);
    Sample[6] = tex2D(Sample_Color, Offsets[0].xw);
    Sample[7] = tex2D(Sample_Color, Offsets[1].xw);
    Sample[8] = tex2D(Sample_Color, Offsets[2].xw);
    OutputColor0 = Median_9(Sample[0], Sample[1], Sample[2],
                            Sample[3], Sample[4], Sample[5],
                            Sample[6], Sample[7], Sample[8]);
    }

technique cMedian
{
    pass
    {
        VertexShader = Median_VS;
        PixelShader = Median_PS;
        #if BUFFER_COLOR_BIT_DEPTH == 8
            SRGBWriteEnable = TRUE;
        #endif
    }
}
