
/*
    Three-point estimation shader

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

#define FP16_MINIMUM float((1.0 / float(1 << 14)) * (0.0 + (1.0 / 1024.0)))

#define RCP_HEIGHT (1.0 / BUFFER_HEIGHT)
#define ASPECT_RATIO (BUFFER_WIDTH * RCP_HEIGHT)
#define ROUND_UP_EVEN(x) int(x) + (int(x) % 2)
#define RENDER_BUFFER_WIDTH int(ROUND_UP_EVEN(128.0 * ASPECT_RATIO))
#define RENDER_BUFFER_HEIGHT int(128.0)

#define SIZE int2(RENDER_BUFFER_WIDTH, RENDER_BUFFER_HEIGHT)
#define BUFFER_SIZE_1 int2(ROUND_UP_EVEN(SIZE.x >> 0), ROUND_UP_EVEN(SIZE.y >> 0))
#define BUFFER_SIZE_2 int2(ROUND_UP_EVEN(SIZE.x >> 1), ROUND_UP_EVEN(SIZE.y >> 1))
#define BUFFER_SIZE_3 int2(ROUND_UP_EVEN(SIZE.x >> 2), ROUND_UP_EVEN(SIZE.y >> 2))
#define BUFFER_SIZE_4 int2(ROUND_UP_EVEN(SIZE.x >> 3), ROUND_UP_EVEN(SIZE.y >> 3))
#define BUFFER_SIZE_5 int2(ROUND_UP_EVEN(SIZE.x >> 4), ROUND_UP_EVEN(SIZE.y >> 4))
#define BUFFER_SIZE_6 int2(ROUND_UP_EVEN(SIZE.x >> 5), ROUND_UP_EVEN(SIZE.y >> 5))

#define TEXTURE(NAME, SIZE, FORMAT, LEVELS) \
    texture2D NAME                          \
    {                                       \
        Width = SIZE.x;                     \
        Height = SIZE.y;                    \
        Format = FORMAT;                    \
        MipLevels = LEVELS;                 \
    };

#define SAMPLER(NAME, TEXTURE) \
    sampler2D NAME             \
    {                          \
        Texture = TEXTURE;     \
        AddressU = MIRROR;     \
        AddressV = MIRROR;     \
        MagFilter = LINEAR;    \
        MinFilter = LINEAR;    \
        MipFilter = LINEAR;    \
    };

namespace Shared_Resources
{
    // Store convoluted normalized frame 1 and 3
    TEXTURE(Render_Common_1, BUFFER_SIZE_1, RGBA16F, 8)
    SAMPLER(Sample_Common_1, Render_Common_1)

    TEXTURE(Render_Common_2, BUFFER_SIZE_2, RGBA16F, 1)
    SAMPLER(Sample_Common_2, Render_Common_2)

    TEXTURE(Render_Common_3, BUFFER_SIZE_3, RGBA16F, 1)
    SAMPLER(Sample_Common_3, Render_Common_3)

    TEXTURE(Render_Common_4, BUFFER_SIZE_4, RGBA16F, 1)
    SAMPLER(Sample_Common_4, Render_Common_4)

    TEXTURE(Render_Common_5, BUFFER_SIZE_5, RGBA16F, 1)
    SAMPLER(Sample_Common_5, Render_Common_5)

    TEXTURE(Render_Common_6, BUFFER_SIZE_6, RGBA16F, 1)
    SAMPLER(Sample_Common_6, Render_Common_6)
}

namespace cInterpolation
{
    // Shader properties

    #define OPTION(DATA_TYPE, NAME, TYPE, CATEGORY, LABEL, MINIMUM, MAXIMUM, DEFAULT) \
        uniform DATA_TYPE NAME <                                                      \
            ui_type = TYPE;                                                           \
            ui_category = CATEGORY;                                                   \
            ui_label = LABEL;                                                         \
            ui_min = MINIMUM;                                                         \
            ui_max = MAXIMUM;                                                         \
        > = DEFAULT;

    OPTION(float, _Constraint, "slider", "Optical flow", "Motion constraint", 0.0, 1.0, 0.5)
    OPTION(float, _Smoothness, "slider", "Optical flow", "Motion smoothness", 0.0, 2.0, 1.0)
    OPTION(float, _MipBias, "drag", "Optical flow", "Optical flow mipmap bias", 0.0, 7.0, 0.0)

    // Consideration: Use A8 channel for difference requirement (normalize BW image)

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

    // Three-point backbuffer storage for interpolation

    TEXTURE(Render_Frame3, int2(BUFFER_WIDTH, BUFFER_HEIGHT), RGBA8, 4)
    SAMPLER(Sample_Frame3, Render_Frame3)

    TEXTURE(Render_Frame_2, int2(BUFFER_WIDTH, BUFFER_HEIGHT), RGBA8, 1)
    SAMPLER(Sample_Frame_2, Render_Frame_2)

    TEXTURE(Render_Frame1, int2(BUFFER_WIDTH, BUFFER_HEIGHT), RGBA8, 4)
    SAMPLER(Sample_Frame1, Render_Frame1)

    // Normalized, prefiltered frames for processing

    TEXTURE(Render_Normalized_Frame, BUFFER_SIZE_1, RGBA16F, 8)
    SAMPLER(Sample_Normalized_Frame, Render_Normalized_Frame)

    // Vertex Shaders

    void Basic_VS(in uint ID : SV_VERTEXID, out float4 Position : SV_POSITION, out float2 TexCoord : TEXCOORD0)
    {
        TexCoord.x = (ID == 2) ? 2.0 : 0.0;
        TexCoord.y = (ID == 1) ? 2.0 : 0.0;
        Position = float4(TexCoord * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
    }

    static const float2 BlurOffsets[8] =
    {
        float2(0.0, 0.0),
        float2(0.0, 1.4850045),
        float2(0.0, 3.4650571),
        float2(0.0, 5.445221),
        float2(0.0, 7.4255576),
        float2(0.0, 9.406127),
        float2(0.0, 11.386987),
        float2(0.0, 13.368189)
    };

    void Blur_0_VS(in uint ID : SV_VERTEXID, out float4 Position : SV_POSITION, out float4 TexCoords[8] : TEXCOORD0)
    {
        float2 CoordVS = 0.0;
        Basic_VS(ID, Position, CoordVS);
        TexCoords[0] = CoordVS.xyxy;

        for(int i = 1; i < 8; i++)
        {
            TexCoords[i].xy = CoordVS.xy - (BlurOffsets[i].yx / BUFFER_SIZE_1);
            TexCoords[i].zw = CoordVS.xy + (BlurOffsets[i].yx / BUFFER_SIZE_1);
        }
    }

    void Blur_1_VS(in uint ID : SV_VERTEXID, out float4 Position : SV_POSITION, out float4 TexCoords[8] : TEXCOORD0)
    {
        float2 CoordVS = 0.0;
        Basic_VS(ID, Position, CoordVS);
        TexCoords[0] = CoordVS.xyxy;

        for(int i = 1; i < 8; i++)
        {
            TexCoords[i].xy = CoordVS.xy - (BlurOffsets[i].xy / BUFFER_SIZE_1);
            TexCoords[i].zw = CoordVS.xy + (BlurOffsets[i].xy / BUFFER_SIZE_1);
        }
    }

    void Sample_3x3_VS(in uint ID, in float2 TexelSize, out float4 Position, out float4 TexCoords[3])
    {
        float2 CoordVS = 0.0;
        Basic_VS(ID, Position, CoordVS);
        // Sample locations:
        // [0].xy [1].xy [2].xy
        // [0].xz [1].xz [2].xz
        // [0].xw [1].xw [2].xw
        TexCoords[0] = CoordVS.xyyy + (float4(-1.0, 1.0, 0.0, -1.0) / TexelSize.xyyy);
        TexCoords[1] = CoordVS.xyyy + (float4(0.0, 1.0, 0.0, -1.0) / TexelSize.xyyy);
        TexCoords[2] = CoordVS.xyyy + (float4(1.0, 1.0, 0.0, -1.0) / TexelSize.xyyy);
    }

    #define SAMPLE_3X3_VS(NAME, BUFFER_SIZE)                                                                        \
        void NAME(in uint ID : SV_VERTEXID, out float4 Position : SV_POSITION, out float4 TexCoords[3] : TEXCOORD0) \
        {                                                                                                           \
            Sample_3x3_VS(ID, BUFFER_SIZE, Position, TexCoords);                                                    \
        }

    SAMPLE_3X3_VS(Sample_3x3_1_VS, BUFFER_SIZE_1)
    SAMPLE_3X3_VS(Sample_3x3_2_VS, BUFFER_SIZE_2)
    SAMPLE_3X3_VS(Sample_3x3_3_VS, BUFFER_SIZE_3)
    SAMPLE_3X3_VS(Sample_3x3_4_VS, BUFFER_SIZE_4)
    SAMPLE_3X3_VS(Sample_3x3_5_VS, BUFFER_SIZE_5)
    SAMPLE_3X3_VS(Sample_3x3_6_VS, BUFFER_SIZE_6)

    void Derivatives_VS(in uint ID : SV_VERTEXID, inout float4 Position : SV_POSITION, inout float4 TexCoords[2] : TEXCOORD0)
    {
        float2 CoordVS = 0.0;
        Basic_VS(ID, Position, CoordVS);
        TexCoords[0] = CoordVS.xxyy + (float4(-1.5, 1.5, -0.5, 0.5) / BUFFER_SIZE_1.xxyy);
        TexCoords[1] = CoordVS.xxyy + (float4(-0.5, 0.5, -1.5, 1.5) / BUFFER_SIZE_1.xxyy);
    }

    // Pixel Shaders

    /*
        BlueSkyDefender's three-frame storage

        [Frame1] [Frame_2] [Frame3]

        Scenario: Three Frames
        Frame 0: [Frame1 (new back buffer data)] [Frame_2 (no data yet)] [Frame3 (no data yet)]
        Frame 1: [Frame1 (new back buffer data)] [Frame_2 (sample Frame1 data)] [Frame3 (no data yet)]
        Frame 2: [Frame1 (new back buffer data)] [Frame_2 (sample Frame1 data)] [Frame3 (sample Frame_2 data)]
        ... and so forth
    */

    void Store_Frame3_PS(in float4 Position : SV_POSITION, in float2 TexCoord : TEXCOORD, out float4 OutputColor0 : SV_TARGET0)
    {
        OutputColor0 = tex2D(Sample_Frame_2, TexCoord);
    }

    void Store_Frame_2_PS(in float4 Position : SV_POSITION, in float2 TexCoord : TEXCOORD, out float4 OutputColor0 : SV_TARGET0)
    {
        OutputColor0 = tex2D(Sample_Frame1, TexCoord);
    }

    void Current_Frame1_PS(float4 Position : SV_POSITION, in float2 TexCoord : TEXCOORD, out float4 OutputColor0 : SV_TARGET0)
    {
        OutputColor0 = tex2D(Sample_Color, TexCoord);
    }

    /*
        1. Store previous filtered frames into their respective buffers
        2. Filter incoming frame
    */

    void Normalize_Frame_PS(in float4 Position : SV_POSITION, float2 TexCoord : TEXCOORD, out float4 OutputColor0 : SV_TARGET0)
    {
        float4 Frame1 = tex2D(Sample_Frame1, TexCoord);
        float4 Frame3 = tex2D(Sample_Frame3, TexCoord);
        float2 NFrame1 = saturate(Frame1.xy / dot(Frame1.rgb, 1.0));
        float2 NFrame3 = saturate(Frame3.xy / dot(Frame3.rgb, 1.0));
        OutputColor0.xy = saturate(NFrame1.xy / max(max(NFrame1.r, NFrame1.g), 1.0 - NFrame1.r - NFrame1.g));
        OutputColor0.zw = saturate(NFrame3.xy / max(max(NFrame3.r, NFrame3.g), 1.0 - NFrame3.r - NFrame3.g));
    }

    static const float BlurWeights[8] =
    {
        0.079788454,
        0.15186256,
        0.12458323,
        0.08723135,
        0.05212966,
        0.026588224,
        0.011573823,
        0.0042996835
    };

    void Gaussian_Blur(in sampler2D Source, in float4 TexCoords[8], out float4 OutputColor0)
    {
        float TotalWeights = BlurWeights[0];
        OutputColor0 = (tex2D(Source, TexCoords[0].xy) * BlurWeights[0]);

        for(int i = 1; i < 8; i++)
        {
            OutputColor0 += (tex2D(Source, TexCoords[i].xy) * BlurWeights[i]);
            OutputColor0 += (tex2D(Source, TexCoords[i].zw) * BlurWeights[i]);
            TotalWeights += (BlurWeights[i] * 2.0);
        }

        OutputColor0 = OutputColor0 / TotalWeights;
    }

    void Pre_Blur_0_PS(in float4 Position : SV_POSITION, in float4 TexCoords[8] : TEXCOORD0, out float4 OutputColor0 : SV_TARGET0)
    {
        Gaussian_Blur(Sample_Normalized_Frame, TexCoords, OutputColor0);
    }

    void Pre_Blur_1_PS(in float4 Position : SV_POSITION, in float4 TexCoords[8] : TEXCOORD0, out float4 OutputColor0 : SV_TARGET0)
    {
        Gaussian_Blur(Shared_Resources::Sample_Common_1, TexCoords, OutputColor0);
    }

    void Derivatives_PS(in float4 Position : SV_POSITION, in float4 TexCoords[2] : TEXCOORD0, out float4 OutputColor0 : SV_TARGET0)
    {
        // Bilinear 5x5 Sobel by CeeJayDK
        //   B1 B2
        // A0     A1
        // A2     B0
        //   C0 C1
        float2 A0 = tex2D(Sample_Normalized_Frame, TexCoords[0].xw).xy * 4.0; // <-1.5, +0.5>
        float2 A1 = tex2D(Sample_Normalized_Frame, TexCoords[0].yw).xy * 4.0; // <+1.5, +0.5>
        float2 A2 = tex2D(Sample_Normalized_Frame, TexCoords[0].xz).xy * 4.0; // <-1.5, -0.5>
        float2 B0 = tex2D(Sample_Normalized_Frame, TexCoords[0].yz).xy * 4.0; // <+1.5, -0.5>
        float2 B1 = tex2D(Sample_Normalized_Frame, TexCoords[1].xw).xy * 4.0; // <-0.5, +1.5>
        float2 B2 = tex2D(Sample_Normalized_Frame, TexCoords[1].yw).xy * 4.0; // <+0.5, +1.5>
        float2 C0 = tex2D(Sample_Normalized_Frame, TexCoords[1].xz).xy * 4.0; // <-0.5, -1.5>
        float2 C1 = tex2D(Sample_Normalized_Frame, TexCoords[1].yz).xy * 4.0; // <+0.5, -1.5>

        OutputColor0 = 0.0;
        OutputColor0.xz = ((B2 + A1 + B0 + C1) - (B1 + A0 + A2 + C0)) / 12.0;
        OutputColor0.yw = ((A0 + B1 + B2 + A1) - (A2 + C0 + C1 + B0)) / 12.0;
    }

    /*
        https://github.com/Dtananaev/cv_opticalFlow

        Copyright (c) 2014-2015, Denis Tananaev All rights reserved.

        Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

        Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

        Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

        THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
    */

    #define COARSEST_LEVEL 5

    void Coarse_Optical_Flow_TV(in float2 TexCoord, in float Level, in float4 UV, out float4 OpticalFlow)
    {
        OpticalFlow = 0.0;
        const float Alpha = max((_Constraint * 1e-3) / exp2(COARSEST_LEVEL - Level), FP16_MINIMUM);

        float4 Frames = tex2D(Sample_Normalized_Frame, TexCoord);

        // <Rx, Gx, Ry, Gy>
        float4 SD = tex2D(Shared_Resources::Sample_Common_1, TexCoord);

        // <Rz, Gz>
        float2 TD = Frames.xy - Frames.zw;

        float2 C = 0.0;
        float4 Aii = 0.0;
        float2 Aij = 0.0;
        float4 Bi = 0.0;

        // Calculate constancy assumption nonlinearity
        C = rsqrt((TD.rg * TD.rg) + (1e-7 * _Smoothness));

        // Build linear equation
        // [Aii Aij] [X] = [Bi]
        // [Aij Aii] [Y] = [Bi]
        Aii = 1.0 / (C.rrgg * (SD.xyzw * SD.xyzw) + Alpha);
        Aij = C.rg * (SD.xz * SD.yw);
        Bi = C.rrgg * (SD.xyzw * TD.rrgg);

        // Solve linear equation for [U, V]
        // [Ix^2+A IxIy] [U] = -[IxIt]
        // [IxIy Iy^2+A] [V] = -[IyIt]
        OpticalFlow.xz = Aii.xz * ((Alpha * UV.xz) - (Aij.rg * UV.yw) - Bi.xz);
        OpticalFlow.yw = Aii.yw * ((Alpha * UV.yw) - (Aij.rg * OpticalFlow.xz) - Bi.yw);
    }

    void Gradient(in float4x2 Samples, out float Gradient)
    {
        // 2x2 Robert's cross
        // [0] [2]
        // [1] [3]
        float4 SqGradientUV = 0.0;
        SqGradientUV.xy = (Samples[0] - Samples[3]); // <IxU, IxV>
        SqGradientUV.zw = (Samples[2] - Samples[1]); // <IyU, IyV>
        Gradient = rsqrt((dot(SqGradientUV, SqGradientUV) * 0.25) + FP16_MINIMUM);
    }

    float2 Prewitt(float2 SampleUV[9], float3x3 Weights)
    {
        // [0] [3] [6]
        // [1] [4] [7]
        // [2] [5] [8]
        float2 Output;
        Output += (SampleUV[0] * Weights[0][0]);
        Output += (SampleUV[1] * Weights[0][1]);
        Output += (SampleUV[2] * Weights[0][2]);
        Output += (SampleUV[3] * Weights[1][0]);
        Output += (SampleUV[4] * Weights[1][1]);
        Output += (SampleUV[5] * Weights[1][2]);
        Output += (SampleUV[6] * Weights[2][0]);
        Output += (SampleUV[7] * Weights[2][1]);
        Output += (SampleUV[8] * Weights[2][2]);
        return Output;
    }

    void Process_Gradients(in float2 SampleUV[9], inout float4 AreaGrad, inout float4 UVGradient)
    {
        // Center smoothness gradient using Prewitt compass
        // https://homepages.inf.ed.ac.uk/rbf/HIPR2/prewitt.htm
        // 0.xy           | 0.zw           | 1.xy           | 1.zw           | 2.xy           | 2.zw           | 3.xy           | 3.zw
        // .......................................................................................................................................
        // -1.0 +1.0 +1.0 | +1.0 +1.0 +1.0 | +1.0 +1.0 +1.0 | +1.0 +1.0 +1.0 | +1.0 +1.0 -1.0 | +1.0 -1.0 -1.0 | -1.0 -1.0 -1.0 | -1.0 -1.0 +1.0 |
        // -1.0 -2.0 +1.0 | -1.0 -2.0 +1.0 | +1.0 -2.0 +1.0 | +1.0 -2.0 -1.0 | +1.0 -2.0 -1.0 | +1.0 -2.0 -1.0 | +1.0 -2.0 +1.0 | -1.0 -2.0 +1.0 |
        // -1.0 +1.0 +1.0 | -1.0 -1.0 +1.0 | -1.0 -1.0 -1.0 | +1.0 -1.0 -1.0 | +1.0 +1.0 -1.0 | +1.0 +1.0 +1.0 | +1.0 +1.0 +1.0 | +1.0 +1.0 +1.0 |

        float4 PrewittUV[4];
        PrewittUV[0].xy = Prewitt(SampleUV, float3x3(-1.0, -1.0, -1.0, +1.0, -2.0, +1.0, +1.0, +1.0, +1.0));
        PrewittUV[0].zw = Prewitt(SampleUV, float3x3(+1.0, -1.0, -1.0, +1.0, -2.0, -1.0, +1.0, +1.0, +1.0));
        PrewittUV[1].xy = Prewitt(SampleUV, float3x3(+1.0, +1.0, -1.0, +1.0, -2.0, -1.0, +1.0, +1.0, -1.0));
        PrewittUV[1].zw = Prewitt(SampleUV, float3x3(+1.0, +1.0, +1.0, +1.0, -2.0, -1.0, +1.0, -1.0, -1.0));
        PrewittUV[2].xy = Prewitt(SampleUV, float3x3(+1.0, +1.0, +1.0, +1.0, -2.0, +1.0, -1.0, -1.0, -1.0));
        PrewittUV[2].zw = Prewitt(SampleUV, float3x3(+1.0, +1.0, +1.0, -1.0, -2.0, +1.0, -1.0, -1.0, +1.0));
        PrewittUV[3].xy = Prewitt(SampleUV, float3x3(-1.0, +1.0, +1.0, -1.0, -2.0, +1.0, -1.0, +1.0, +1.0));
        PrewittUV[3].zw = Prewitt(SampleUV, float3x3(-1.0, -1.0, +1.0, -1.0, -2.0, +1.0, +1.0, +1.0, +1.0));

        float2 MaxGradient[3];
        MaxGradient[0] = max(max(abs(PrewittUV[0].xy), abs(PrewittUV[0].zw)), max(abs(PrewittUV[1].xy), abs(PrewittUV[1].zw)));
        MaxGradient[1] = max(max(abs(PrewittUV[2].xy), abs(PrewittUV[2].zw)), max(abs(PrewittUV[3].xy), abs(PrewittUV[3].zw)));

        const float Weight = 1.0 / 5.0;
        MaxGradient[2] = max(abs(MaxGradient[0]), abs(MaxGradient[1])) * Weight;
        float CenterGradient = rsqrt((dot(MaxGradient[2], MaxGradient[2]) * 0.5) + FP16_MINIMUM);

        // Area smoothness gradients
        // .............................
        //  [0]     [1]     [2]     [3]
        // 0 3 . | . 3 6 | . . . | . . .
        // 1 4 . | . 4 7 | 1 4 . | . 4 7
        // . . . | . . . | 2 5 . | . 5 8
        Gradient(float4x2(SampleUV[0], SampleUV[3], SampleUV[1], SampleUV[4]), AreaGrad[0]);
        Gradient(float4x2(SampleUV[3], SampleUV[6], SampleUV[4], SampleUV[7]), AreaGrad[1]);
        Gradient(float4x2(SampleUV[1], SampleUV[4], SampleUV[2], SampleUV[5]), AreaGrad[2]);
        Gradient(float4x2(SampleUV[4], SampleUV[7], SampleUV[5], SampleUV[8]), AreaGrad[3]);
        UVGradient = 0.5 * (CenterGradient + AreaGrad);
    }

    void Area_Average(in float4 SampleNW, in float4 SampleNE, in float4 SampleSW, in float4 SampleSE, out float4 Color)
    {
        Color = (SampleNW + SampleNE + SampleSW + SampleSE) * 0.25;
    }

    void Optical_Flow_TV(in sampler2D SourceUV, in float4 TexCoords[3], in float Level, out float4 OpticalFlow)
    {
        OpticalFlow = 0.0;
        const float Alpha = max((_Constraint * 1e-3) / exp2(COARSEST_LEVEL - Level), FP16_MINIMUM);

        // Load textures

        float4 Frames = tex2D(Sample_Normalized_Frame, TexCoords[1].xz);

        // <Rx, Gx, Ry, Gy>
        float4 SD = tex2D(Shared_Resources::Sample_Common_1, TexCoords[1].xz);

        // <Rz, Gz>
        float2 TD = Frames.xy - Frames.zw;

        // Optical flow calculation

        // <Ru, Rv, Gu, Gv>
        float4 SampleUV[9];
        float2 SampleUVR[9];
        float2 SampleUVG[9];

        // [0] = Red, [1] = Green
        float4 AreaGrad[2];
        float4 UVGradient[2];

        // <Ru, Rv, Gu, Gv>
        float4 AreaAvg[4];
        float4 CenterAverage;
        float4 UVAverage;

        // SampleUV[i]
        // 0 3 6
        // 1 4 7
        // 2 5 8
        SampleUV[0] = tex2D(SourceUV, TexCoords[0].xy);
        SampleUV[1] = tex2D(SourceUV, TexCoords[0].xz);
        SampleUV[2] = tex2D(SourceUV, TexCoords[0].xw);
        SampleUV[3] = tex2D(SourceUV, TexCoords[1].xy);
        SampleUV[4] = tex2D(SourceUV, TexCoords[1].xz);
        SampleUV[5] = tex2D(SourceUV, TexCoords[1].xw);
        SampleUV[6] = tex2D(SourceUV, TexCoords[2].xy);
        SampleUV[7] = tex2D(SourceUV, TexCoords[2].xz);
        SampleUV[8] = tex2D(SourceUV, TexCoords[2].xw);

        [unroll]for(int i = 0; i < 9; i++)
        {
            SampleUVR[i] = SampleUV[i].xy;
            SampleUVG[i] = SampleUV[i].zw;
        }

        // Process area gradients in each patch, per plane

        Process_Gradients(SampleUVR, AreaGrad[0], UVGradient[0]);
        Process_Gradients(SampleUVG, AreaGrad[1], UVGradient[1]);

        // Calculate area + center averages of estimated vectors

        Area_Average(SampleUV[0], SampleUV[3], SampleUV[1], SampleUV[4], AreaAvg[0]);
        Area_Average(SampleUV[3], SampleUV[6], SampleUV[4], SampleUV[7], AreaAvg[1]);
        Area_Average(SampleUV[1], SampleUV[4], SampleUV[2], SampleUV[5], AreaAvg[2]);
        Area_Average(SampleUV[4], SampleUV[7], SampleUV[5], SampleUV[8], AreaAvg[3]);

        CenterAverage += ((SampleUV[0] + SampleUV[6] + SampleUV[2] + SampleUV[8]) * 1.0);
        CenterAverage += ((SampleUV[3] + SampleUV[1] + SampleUV[7] + SampleUV[5]) * 2.0);
        CenterAverage += (SampleUV[4] * 4.0);
        CenterAverage = CenterAverage / 16.0;

        float2 C = 0.0;
        float4 Aii = 0.0;
        float2 Aij = 0.0;
        float4 Bi = 0.0;

        // Calculate constancy assumption nonlinearity
        // Dot-product increases when the current gradient + previous estimation are parallel
        C.r = dot(SD.xy, CenterAverage.xy) + TD.r;
        C.g = dot(SD.zw, CenterAverage.zw) + TD.g;
        C.rg = rsqrt((C.rg * C.rg) + (1e-7 * _Smoothness));

        // Build linear equation
        // [Aii Aij] [X] = [Bi]
        // [Aij Aii] [Y] = [Bi]
        Aii.xy = 1.0 / (dot(UVGradient[0], 1.0) * Alpha + (C.rr * (SD.xy * SD.xy)));
        Aii.zw = 1.0 / (dot(UVGradient[1], 1.0) * Alpha + (C.gg * (SD.zw * SD.zw)));
        Aij.xy = C.rg * (SD.xz * SD.yw);
        Bi = C.rrgg * (SD.xyzw * TD.rrgg);

        // Solve linear equation for [U, V]
        // [Ix^2+A IxIy] [U] = -[IxIt]
        // [IxIy Iy^2+A] [V] = -[IyIt]
        UVAverage.xy = (AreaGrad[0].xx * AreaAvg[0].xy) + (AreaGrad[0].yy * AreaAvg[1].xy) + (AreaGrad[0].zz * AreaAvg[2].xy) + (AreaGrad[0].ww * AreaAvg[3].xy);
        UVAverage.zw = (AreaGrad[1].xx * AreaAvg[0].zw) + (AreaGrad[1].yy * AreaAvg[1].zw) + (AreaGrad[1].zz * AreaAvg[2].zw) + (AreaGrad[1].ww * AreaAvg[3].zw);
        OpticalFlow.xz = Aii.xz * ((Alpha * UVAverage.xz) - (Aij.rg * CenterAverage.yw) - Bi.xz);
        OpticalFlow.yw = Aii.yw * ((Alpha * UVAverage.yw) - (Aij.rg * OpticalFlow.xz) - Bi.yw);
    }

    #define LEVEL_PS(NAME, SAMPLER, LEVEL)                                                                             \
        void NAME(in float4 Position : SV_POSITION, in float4 TexCoords[3] : TEXCOORD0, out float4 Color : SV_TARGET0) \
        {                                                                                                              \
            Optical_Flow_TV(SAMPLER, TexCoords, LEVEL, Color);                                                         \
        }

    void Level_6_PS(in float4 Position : SV_POSITION, in float2 TexCoord : TEXCOORD0, out float4 Color : SV_TARGET0)
    {
        Coarse_Optical_Flow_TV(TexCoord, 5.0, 0.0, Color);
    }

    LEVEL_PS(Level_5_PS, Shared_Resources::Sample_Common_6, 4.0)
    LEVEL_PS(Level_4_PS, Shared_Resources::Sample_Common_5, 3.0)
    LEVEL_PS(Level_3_PS, Shared_Resources::Sample_Common_4, 2.0)
    LEVEL_PS(Level_2_PS, Shared_Resources::Sample_Common_3, 1.0)

    void Level_1_PS(in float4 Position : SV_POSITION, in float4 TexCoords[3] : TEXCOORD0, out float4 OutputColor0 : SV_TARGET0)
    {
        float4 OpticalFlow = 0.0;
        Optical_Flow_TV(Shared_Resources::Sample_Common_2, TexCoords, 0.0, OpticalFlow);
        OutputColor0.rg = OpticalFlow.xy + OpticalFlow.zw;
        OutputColor0.ba = float2(0.0, 1.0);
    }

    void Post_Blur_0_PS(in float4 Position : SV_POSITION, in float4 TexCoords[8] : TEXCOORD0, out float4 OutputColor0 : SV_TARGET0)
    {
        Gaussian_Blur(Shared_Resources::Sample_Common_1, TexCoords, OutputColor0);
        OutputColor0.a = 1.0;
    }

    void Post_Blur_1_PS(in float4 Position : SV_POSITION, in float4 TexCoords[8] : TEXCOORD0, out float4 OutputColor0 : SV_TARGET0)
    {
        Gaussian_Blur(Sample_Normalized_Frame, TexCoords, OutputColor0);
        OutputColor0.a = 1.0;
    }

    /*
        Cascaded median algorithm (Fig. 3.)
        Link: http://citeseerx.ist.psu.edu/viewdoc/download?doi=10.1.1.64.7794&rep=rep1&type=pdf
        Title: Temporal video up-conversion on a next generation media-processor
        Authors: Jan-Willem van de Waerdt, Stamatis Vassiliadis, Erwin B. Bellers, and Johan G. Janssen
    */

    float4 Median(float4 A, float4 B, float4 C)
    {
        return min(max(min(A, B), C), max(A, B));
    }

    void Interpolate_PS(in float4 Position : SV_POSITION, in float2 TexCoord : TEXCOORD0, out float4 OutputColor0 : SV_TARGET0)
    {
        float2 TexelSize = 1.0 / BUFFER_SIZE_1;
        float2 MotionVectors = tex2Dlod(Shared_Resources::Sample_Common_1, float4(TexCoord, 0.0, _MipBias)).xy * TexelSize.xy;

        float4 StaticLeft = tex2D(Sample_Frame3, TexCoord);
        float4 StaticRight = tex2D(Sample_Frame1, TexCoord);
        float4 DynamicLeft = tex2D(Sample_Frame3, TexCoord + MotionVectors);
        float4 DynamicRight = tex2D(Sample_Frame1, TexCoord - MotionVectors);

        float4 StaticAverage = lerp(StaticLeft, StaticRight, 0.5);
        float4 DynamicAverage = lerp(DynamicLeft, DynamicRight, 0.5);

        float4 StaticMedian = Median(StaticLeft, StaticRight, DynamicAverage);
        float4 DynamicMedian = Median(StaticAverage, DynamicLeft, DynamicRight);
        float4 MotionFilter = lerp(StaticAverage, DynamicAverage, DynamicMedian);

        float4 CascadedMedian = Median(StaticMedian, MotionFilter, DynamicMedian);

        OutputColor0 = lerp(CascadedMedian, DynamicAverage, 0.5);
        OutputColor0.a = 1.0;
    }

    /*
        TODO (bottom text)
        - Calculate vectors on Frame 3 and Frame 1 (can use pyramidal method via MipMaps)
        - Calculate warp Frame 3 and Frame 1 to Frame 2
    */

    #define PASS(VERTEX_SHADER, PIXEL_SHADER, RENDER_TARGET) \
        pass                                                 \
        {                                                    \
            VertexShader = VERTEX_SHADER;                    \
            PixelShader = PIXEL_SHADER;                      \
            RenderTarget0 = RENDER_TARGET;                   \
        }

    technique cInterpolation
    {
        // Store frames
        PASS(Basic_VS, Store_Frame3_PS, Render_Frame3)
        PASS(Basic_VS, Store_Frame_2_PS, Render_Frame_2)
        PASS(Basic_VS, Current_Frame1_PS, Render_Frame1)

        // Store previous frames, normalize current
        PASS(Basic_VS, Normalize_Frame_PS, Render_Normalized_Frame)

        // Gaussian blur
        PASS(Blur_0_VS, Pre_Blur_0_PS, Shared_Resources::Render_Common_1)
        PASS(Blur_1_VS, Pre_Blur_1_PS, Render_Normalized_Frame)

        // Calculate spatial derivative pyramid
        PASS(Derivatives_VS, Derivatives_PS, Shared_Resources::Render_Common_1)

        // Bilinear Optical Flow
        PASS(Basic_VS, Level_6_PS, Shared_Resources::Render_Common_6)
        PASS(Sample_3x3_6_VS, Level_5_PS, Shared_Resources::Render_Common_5)
        PASS(Sample_3x3_5_VS, Level_4_PS, Shared_Resources::Render_Common_4)
        PASS(Sample_3x3_4_VS, Level_3_PS, Shared_Resources::Render_Common_3)
        PASS(Sample_3x3_3_VS, Level_2_PS, Shared_Resources::Render_Common_2)
        PASS(Sample_3x3_2_VS, Level_1_PS, Shared_Resources::Render_Common_1)

        // Gaussian blur
        PASS(Blur_0_VS, Post_Blur_0_PS, Render_Normalized_Frame)
        PASS(Blur_1_VS, Post_Blur_1_PS, Shared_Resources::Render_Common_1)

        // Interpolate

        pass Interpolate
        {
            VertexShader = Basic_VS;
            PixelShader = Interpolate_PS;
            #if BUFFER_COLOR_BIT_DEPTH == 8
                SRGBWriteEnable = TRUE;
            #endif
        }
    }
}
