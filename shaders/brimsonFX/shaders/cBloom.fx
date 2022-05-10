
/*
    Dual-filtering Bloom

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

#define BUFFER_SIZE_1 int2(BUFFER_WIDTH >> 1, BUFFER_HEIGHT >> 1)
#define BUFFER_SIZE_2 int2(BUFFER_WIDTH >> 2, BUFFER_HEIGHT >> 2)
#define BUFFER_SIZE_3 int2(BUFFER_WIDTH >> 3, BUFFER_HEIGHT >> 3)
#define BUFFER_SIZE_4 int2(BUFFER_WIDTH >> 4, BUFFER_HEIGHT >> 4)
#define BUFFER_SIZE_5 int2(BUFFER_WIDTH >> 5, BUFFER_HEIGHT >> 5)
#define BUFFER_SIZE_6 int2(BUFFER_WIDTH >> 6, BUFFER_HEIGHT >> 6)
#define BUFFER_SIZE_7 int2(BUFFER_WIDTH >> 7, BUFFER_HEIGHT >> 7)
#define BUFFER_SIZE_8 int2(BUFFER_WIDTH >> 8, BUFFER_HEIGHT >> 8)

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
        MagFilter = LINEAR;    \
        MinFilter = LINEAR;    \
        MipFilter = LINEAR;    \
    };

#define DOWNSAMPLE_VS(NAME, TEXEL_SIZE)                                                                        \
    void NAME(in uint ID : SV_VERTEXID, out float4 Position : SV_POSITION, out float4 TexCoord[4] : TEXCOORD0) \
    {                                                                                                          \
        Downsample_VS(ID, Position, TexCoord, TEXEL_SIZE);                                                     \
    }

#define UPSAMPLE_VS(NAME, TEXEL_SIZE)                                                                          \
    void NAME(in uint ID : SV_VERTEXID, out float4 Position : SV_POSITION, out float4 TexCoord[3] : TEXCOORD0) \
    {                                                                                                          \
        Upsample_VS(ID, Position, TexCoord, TEXEL_SIZE);                                                       \
    }

#define DOWNSAMPLE_PS(NAME, SAMPLER)                                                                                     \
    void NAME(in float4 Position : SV_POSITION, in float4 TexCoord[4] : TEXCOORD0, out float4 OutputColor0 : SV_TARGET0) \
    {                                                                                                                    \
        Downsample(SAMPLER, TexCoord, OutputColor0);                                                                     \
    }

#define UPSAMPLE_PS(NAME, SAMPLER, LEVEL_WEIGHT)                                                                         \
    void NAME(in float4 Position : SV_POSITION, in float4 TexCoord[3] : TEXCOORD0, out float4 OutputColor0 : SV_TARGET0) \
    {                                                                                                                    \
        Upsample(SAMPLER, TexCoord, LEVEL_WEIGHT, OutputColor0);                                                         \
    }

#define DOWNSAMPLE_PASS(VERTEX_SHADER, PIXEL_SHADER, RENDER_TARGET) \
    pass                                                            \
    {                                                               \
        VertexShader = VERTEX_SHADER;                               \
        PixelShader = PIXEL_SHADER;                                 \
        RenderTarget0 = RENDER_TARGET;                              \
    }

#define UPSAMPLE_BLEND_PASS(VERTEX_SHADER, PIXEL_SHADER, RENDER_TARGET) \
    pass                                                                \
    {                                                                   \
        VertexShader = VERTEX_SHADER;                                   \
        PixelShader = PIXEL_SHADER;                                     \
        RenderTarget0 = RENDER_TARGET;                                  \
        ClearRenderTargets = FALSE;                                     \
        BlendEnable = TRUE;                                             \
        BlendOp = ADD;                                                  \
        SrcBlend = SRCALPHA;                                            \
        DestBlend = ONE;                                                \
    }

#define OPTION(DATATYPE, NAME, CATEGORY, LABEL, TYPE, MAXIMUM, DEFAULT) \
    uniform DATATYPE NAME <                                             \
        ui_category = CATEGORY;                                         \
        ui_label = LABEL;                                               \
        ui_type = TYPE;                                                 \
        ui_min = 0.0;                                                   \
        ui_max = MAXIMUM;                                               \
    > = DEFAULT;

namespace Shared_Resources_Bloom
{
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

    TEXTURE(Render_Common_7, BUFFER_SIZE_7, RGBA16F, 1)
    SAMPLER(Sample_Common_7, Render_Common_7)

    TEXTURE(Render_Common_8, BUFFER_SIZE_8, RGBA16F, 1)
    SAMPLER(Sample_Common_8, Render_Common_8)
}

OPTION(float, _Threshold, "Main", "Threshold", "drag", 1.0, 0.8)
OPTION(float, _Smooth, "Main", "Smoothing", "drag", 1.0, 0.5)
OPTION(float, _Saturation, "Main", "Saturation", "drag", 4.0, 1.0)
OPTION(float3, _ColorShift, "Main", "Color shift", "color", 1.0, 1.0)
OPTION(float, _Intensity, "Main", "Color Intensity", "drag", 4.0, 1.0)

OPTION(float, _Level6Weight, "Level Weights", "Level 6", "drag", 2.0, 1.0)
OPTION(float, _Level5Weight, "Level Weights", "Level 5", "drag", 2.0, 1.0)
OPTION(float, _Level4Weight, "Level Weights", "Level 4", "drag", 2.0, 1.0)
OPTION(float, _Level3Weight, "Level Weights", "Level 3", "drag", 2.0, 1.0)
OPTION(float, _Level2Weight, "Level Weights", "Level 2", "drag", 2.0, 1.0)
OPTION(float, _Level1Weight, "Level Weights", "Level 1", "drag", 2.0, 1.0)

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

// Vertex shaders
// Sampling kernels: http://www.iryoku.com/next-generation-post-processing-in-call-of-duty-advanced-warfare

void Basic_VS(in uint ID : SV_VERTEXID, out float4 Position : SV_POSITION, out float2 TexCoord : TEXCOORD0)
{
    TexCoord.x = (ID == 2) ? 2.0 : 0.0;
    TexCoord.y = (ID == 1) ? 2.0 : 0.0;
    Position = float4(TexCoord * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
}

void Downsample_VS(in uint ID, out float4 Position, out float4 TexCoord[4], float2 PixelSize)
{
    float2 CoordVS = 0.0;
    Basic_VS(ID, Position, CoordVS);
    // Quadrant
    TexCoord[0] = CoordVS.xyxy + float4(-1.0, -1.0, 1.0, 1.0) * PixelSize.xyxy;
    // Left column
    TexCoord[1] = CoordVS.xyyy + float4(-2.0, 2.0, 0.0, -2.0) * PixelSize.xyyy;
    // Center column
    TexCoord[2] = CoordVS.xyyy + float4(0.0, 2.0, 0.0, -2.0) * PixelSize.xyyy;
    // Right column
    TexCoord[3] = CoordVS.xyyy + float4(2.0, 2.0, 0.0, -2.0) * PixelSize.xyyy;
}

void Upsample_VS(in uint ID, out float4 Position, out float4 TexCoord[3], float2 PixelSize)
{
    float2 CoordVS = 0.0;
    Basic_VS(ID, Position, CoordVS);
    // Left column
    TexCoord[0] = CoordVS.xyyy + float4(-2.0, 2.0, 0.0, -2.0) * PixelSize.xyyy;
    // Center column
    TexCoord[1] = CoordVS.xyyy + float4(0.0, 2.0, 0.0, -2.0) * PixelSize.xyyy;
    // Right column
    TexCoord[2] = CoordVS.xyyy + float4(2.0, 2.0, 0.0, -2.0) * PixelSize.xyyy;
}

DOWNSAMPLE_VS(Downsample_1_VS, 1.0 / BUFFER_SIZE_1)
DOWNSAMPLE_VS(Downsample_2_VS, 1.0 / BUFFER_SIZE_2)
DOWNSAMPLE_VS(Downsample_3_VS, 1.0 / BUFFER_SIZE_3)
DOWNSAMPLE_VS(Downsample_4_VS, 1.0 / BUFFER_SIZE_4)
DOWNSAMPLE_VS(Downsample_5_VS, 1.0 / BUFFER_SIZE_5)
DOWNSAMPLE_VS(Downsample_6_VS, 1.0 / BUFFER_SIZE_6)
DOWNSAMPLE_VS(Downsample_7_VS, 1.0 / BUFFER_SIZE_7)

UPSAMPLE_VS(Upsample_7_VS, 1.0 / BUFFER_SIZE_7)
UPSAMPLE_VS(Upsample_6_VS, 1.0 / BUFFER_SIZE_6)
UPSAMPLE_VS(Upsample_5_VS, 1.0 / BUFFER_SIZE_5)
UPSAMPLE_VS(Upsample_4_VS, 1.0 / BUFFER_SIZE_4)
UPSAMPLE_VS(Upsample_3_VS, 1.0 / BUFFER_SIZE_3)
UPSAMPLE_VS(Upsample_2_VS, 1.0 / BUFFER_SIZE_2)
UPSAMPLE_VS(Upsample_1_VS, 1.0 / BUFFER_SIZE_1)

// Pixel shaders
// Thresholding: https://github.com/keijiro/Kino [MIT]
// Tonemapping: https://github.com/TheRealMJP/BakingLab [MIT]

float Median_3(float x, float y, float z)
{
    return max(min(x, y), min(max(x, y), z));
}

void Downsample(in sampler2D Source, in float4 TexCoord[4], out float4 Output)
{
    // A0    B0    C0
    //    D0    D1
    // A1    B1    C1
    //    D2    D3
    // A2    B2    C2

    float4 D0 = tex2D(Source, TexCoord[0].xw);
    float4 D1 = tex2D(Source, TexCoord[0].zw);
    float4 D2 = tex2D(Source, TexCoord[0].xy);
    float4 D3 = tex2D(Source, TexCoord[0].zy);

    float4 A0 = tex2D(Source, TexCoord[1].xy);
    float4 A1 = tex2D(Source, TexCoord[1].xz);
    float4 A2 = tex2D(Source, TexCoord[1].xw);

    float4 B0 = tex2D(Source, TexCoord[2].xy);
    float4 B1 = tex2D(Source, TexCoord[2].xz);
    float4 B2 = tex2D(Source, TexCoord[2].xw);

    float4 C0 = tex2D(Source, TexCoord[3].xy);
    float4 C1 = tex2D(Source, TexCoord[3].xz);
    float4 C2 = tex2D(Source, TexCoord[3].xw);

    const float2 Weights = float2(0.5, 0.125) / 4.0;
    Output  = (D0 + D1 + D2 + D3) * Weights.x;
    Output += (A0 + B0 + A1 + B1) * Weights.y;
    Output += (B0 + C0 + B1 + C1) * Weights.y;
    Output += (A1 + B1 + A2 + B2) * Weights.y;
    Output += (B1 + C1 + B2 + C2) * Weights.y;
}

void Upsample(in sampler2D Source, in float4 TexCoord[3], in float Weight, out float4 Output)
{
    // A0 B0 C0
    // A1 B1 C1
    // A2 B2 C2

    float4 A0 = tex2D(Source, TexCoord[0].xy);
    float4 A1 = tex2D(Source, TexCoord[0].xz);
    float4 A2 = tex2D(Source, TexCoord[0].xw);

    float4 B0 = tex2D(Source, TexCoord[1].xy);
    float4 B1 = tex2D(Source, TexCoord[1].xz);
    float4 B2 = tex2D(Source, TexCoord[1].xw);

    float4 C0 = tex2D(Source, TexCoord[2].xy);
    float4 C1 = tex2D(Source, TexCoord[2].xz);
    float4 C2 = tex2D(Source, TexCoord[2].xw);

    Output  = (A0 + C0 + A2 + C2) * 1.0;
    Output += (A1 + B0 + C1 + B2) * 2.0;
    Output += B1 * 4.0;
    Output *= (1.0 / 16.0);
    Output.a = Weight;
}

// sRGB => XYZ => D65_2_D60 => AP1 => RRT_SAT
static const float3x3 ACESInputMat = float3x3
(
    0.59719, 0.35458, 0.04823,
    0.07600, 0.90834, 0.01566,
    0.02840, 0.13383, 0.83777
);

// ODT_SAT => XYZ => D60_2_D65 => sRGB
static const float3x3 ACESOutputMat = float3x3
(
     1.60475, -0.53108, -0.07367,
    -0.10208,  1.10813, -0.00605,
    -0.00327, -0.07276,  1.07602
);

float3 RRTODTFit(float3 V)
{
    float3 A = V * (V + 0.0245786f) - 0.000090537f;
    float3 B = V * (0.983729f * V + 0.4329510f) + 0.238081f;
    return A / B;
}

void Prefilter_PS(in float4 Position : SV_POSITION, in float2 TexCoord : TEXCOORD0, out float4 OutputColor0 : SV_TARGET0)
{
    const float Knee = mad(_Threshold, _Smooth, 1e-5f);
    const float3 Curve = float3(_Threshold - Knee, Knee * 2.0, 0.25 / Knee);
    float4 Color = tex2D(Sample_Color, TexCoord);

    // Under-threshold
    float Brightness = Median_3(Color.r, Color.g, Color.b);
    float Response_Curve = clamp(Brightness - Curve.x, 0.0, Curve.y);
    Response_Curve = Curve.z * Response_Curve * Response_Curve;

    // Combine and apply the brightness response curve
    Color = Color * max(Response_Curve, Brightness - _Threshold) / max(Brightness, 1e-10);
    Brightness = Median_3(Color.r, Color.g, Color.b);
    OutputColor0 = saturate(lerp(Brightness, Color.rgb, _Saturation)) * _ColorShift;

    // Set alpha to 1.0 so we can see the complete results in ReShade's statistics
    OutputColor0.a = 1.0;
}

DOWNSAMPLE_PS(Downsample_1_PS, Shared_Resources_Bloom::Sample_Common_1)
DOWNSAMPLE_PS(Downsample_2_PS, Shared_Resources_Bloom::Sample_Common_2)
DOWNSAMPLE_PS(Downsample_3_PS, Shared_Resources_Bloom::Sample_Common_3)
DOWNSAMPLE_PS(Downsample_4_PS, Shared_Resources_Bloom::Sample_Common_4)
DOWNSAMPLE_PS(Downsample_5_PS, Shared_Resources_Bloom::Sample_Common_5)
DOWNSAMPLE_PS(Downsample_6_PS, Shared_Resources_Bloom::Sample_Common_6)
DOWNSAMPLE_PS(Downsample_7_PS, Shared_Resources_Bloom::Sample_Common_7)

UPSAMPLE_PS(Upsample_7_PS, Shared_Resources_Bloom::Sample_Common_8, _Level6Weight)
UPSAMPLE_PS(Upsample_6_PS, Shared_Resources_Bloom::Sample_Common_7, _Level5Weight)
UPSAMPLE_PS(Upsample_5_PS, Shared_Resources_Bloom::Sample_Common_6, _Level4Weight)
UPSAMPLE_PS(Upsample_4_PS, Shared_Resources_Bloom::Sample_Common_5, _Level3Weight)
UPSAMPLE_PS(Upsample_3_PS, Shared_Resources_Bloom::Sample_Common_4, _Level2Weight)
UPSAMPLE_PS(Upsample_2_PS, Shared_Resources_Bloom::Sample_Common_3, _Level1Weight)
UPSAMPLE_PS(Upsample_1_PS, Shared_Resources_Bloom::Sample_Common_2, 0.0)

void Composite_PS(in float4 Position : SV_POSITION, in float2 TexCoord : TEXCOORD0, out float4 OutputColor0 : SV_TARGET0)
{
    float4 SourceColor = tex2D(Shared_Resources_Bloom::Sample_Common_1, TexCoord);
    SourceColor *= _Intensity;
    SourceColor = mul(ACESInputMat, SourceColor.rgb);
    SourceColor = RRTODTFit(SourceColor.rgb);
    SourceColor = saturate(mul(ACESOutputMat, SourceColor.rgb));
    OutputColor0 = SourceColor;
}

/* [ TECHNIQUE ] */

technique cBloom
{
    pass
    {
        VertexShader = Basic_VS;
        PixelShader = Prefilter_PS;
        RenderTarget0 = Shared_Resources_Bloom::Render_Common_1;
    }

    DOWNSAMPLE_PASS(Downsample_1_VS, Downsample_1_PS, Shared_Resources_Bloom::Render_Common_2)
    DOWNSAMPLE_PASS(Downsample_2_VS, Downsample_2_PS, Shared_Resources_Bloom::Render_Common_3)
    DOWNSAMPLE_PASS(Downsample_3_VS, Downsample_3_PS, Shared_Resources_Bloom::Render_Common_4)
    DOWNSAMPLE_PASS(Downsample_4_VS, Downsample_4_PS, Shared_Resources_Bloom::Render_Common_5)
    DOWNSAMPLE_PASS(Downsample_5_VS, Downsample_5_PS, Shared_Resources_Bloom::Render_Common_6)
    DOWNSAMPLE_PASS(Downsample_6_VS, Downsample_6_PS, Shared_Resources_Bloom::Render_Common_7)
    DOWNSAMPLE_PASS(Downsample_7_VS, Downsample_7_PS, Shared_Resources_Bloom::Render_Common_8)

    UPSAMPLE_BLEND_PASS(Upsample_7_VS, Upsample_7_PS, Shared_Resources_Bloom::Render_Common_7)
    UPSAMPLE_BLEND_PASS(Upsample_6_VS, Upsample_6_PS, Shared_Resources_Bloom::Render_Common_6)
    UPSAMPLE_BLEND_PASS(Upsample_5_VS, Upsample_5_PS, Shared_Resources_Bloom::Render_Common_5)
    UPSAMPLE_BLEND_PASS(Upsample_4_VS, Upsample_4_PS, Shared_Resources_Bloom::Render_Common_4)
    UPSAMPLE_BLEND_PASS(Upsample_3_VS, Upsample_3_PS, Shared_Resources_Bloom::Render_Common_3)
    UPSAMPLE_BLEND_PASS(Upsample_2_VS, Upsample_2_PS, Shared_Resources_Bloom::Render_Common_2)

    pass
    {
        VertexShader = Upsample_1_VS;
        PixelShader = Upsample_1_PS;
        RenderTarget0 = Shared_Resources_Bloom::Render_Common_1;
    }

    pass
    {
        VertexShader = Basic_VS;
        PixelShader = Composite_PS;
        ClearRenderTargets = FALSE;
        BlendEnable = TRUE;
        BlendOp = ADD;
        SrcBlend = ONE;
        DestBlend = INVSRCCOLOR;
        #if BUFFER_COLOR_BIT_DEPTH == 8
            SRGBWriteEnable = TRUE;
        #endif
    }
}
