
/*
    Various Pyramid Convolutions

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

#define BUFFER_SIZE_0 int2(BUFFER_WIDTH, BUFFER_HEIGHT)
#define BUFFER_SIZE_1 int2(BUFFER_WIDTH >> 1, BUFFER_HEIGHT >> 1)
#define BUFFER_SIZE_2 int2(BUFFER_WIDTH >> 2, BUFFER_HEIGHT >> 2)
#define BUFFER_SIZE_3 int2(BUFFER_WIDTH >> 3, BUFFER_HEIGHT >> 3)
#define BUFFER_SIZE_4 int2(BUFFER_WIDTH >> 4, BUFFER_HEIGHT >> 4)

#define TEXTURE(NAME, SIZE, FORMAT, LEVELS) \
    texture2D NAME < pooled = true; >       \
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

#define DOWNSAMPLE_VS(NAME, TEXEL_SIZE)                                                                             \
    void NAME(in uint ID : SV_VERTEXID, inout float4 Position : SV_POSITION, inout float4 TexCoords[4] : TEXCOORD0) \
    {                                                                                                               \
        Downsample_VS(ID, Position, TexCoords, TEXEL_SIZE);                                                         \
    }

#define UPSAMPLE_VS(NAME, TEXEL_SIZE)                                                                               \
    void NAME(in uint ID : SV_VERTEXID, inout float4 Position : SV_POSITION, inout float4 TexCoords[3] : TEXCOORD0) \
    {                                                                                                               \
        Upsample_VS(ID, Position, TexCoords, TEXEL_SIZE);                                                           \
    }

#define DOWNSAMPLE_PS(NAME, SAMPLER)                                                                                      \
    void NAME(in float4 Position : SV_POSITION, in float4 TexCoords[4] : TEXCOORD0, out float4 OutputColor0 : SV_TARGET0) \
    {                                                                                                                     \
        Downsample_PS(SAMPLER, TexCoords, OutputColor0);                                                                  \
    }

#define UPSAMPLE_PS(NAME, SAMPLER)                                                                                        \
    void NAME(in float4 Position : SV_POSITION, in float4 TexCoords[3] : TEXCOORD0, out float4 OutputColor0 : SV_TARGET0) \
    {                                                                                                                     \
        Upsample_PS(SAMPLER, TexCoords, OutputColor0);                                                                    \
    }

#define PASS(VERTEX_SHADER, PIXEL_SHADER, RENDER_TARGET) \
    pass                                                 \
    {                                                    \
        VertexShader = VERTEX_SHADER;                    \
        PixelShader = PIXEL_SHADER;                      \
        RenderTarget0 = RENDER_TARGET;                   \
    }

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

TEXTURE(Render_Common_1, BUFFER_SIZE_1, RGBA16F, 8)
SAMPLER(Sample_Common_1, Render_Common_1)

TEXTURE(Render_Common_2, BUFFER_SIZE_2, RGBA16F, 1)
SAMPLER(Sample_Common_2, Render_Common_2)

TEXTURE(Render_Common_3, BUFFER_SIZE_3, RGBA16F, 1)
SAMPLER(Sample_Common_3, Render_Common_3)

TEXTURE(Render_Common_4, BUFFER_SIZE_4, RGBA16F, 1)
SAMPLER(Sample_Common_4, Render_Common_4)

// Shader properties

uniform int _DownsampleMethod <
    ui_type = "combo";
    ui_items = " 2x2 Box\0 3x3 Tent\0 Jorge\0 Kawase\0";
    ui_label = "Downsample kernel";
    ui_tooltip = "Downsampling Method";
> = 0;

uniform int _UpsampleMethod <
    ui_type = "combo";
    ui_items = " 2x2 Box\0 3x3 Tent\0 Kawase\0";
    ui_label = "Upsample kernel";
    ui_tooltip = "Upsampling Method";
> = 0;

// Vertex shaders

void Basic_VS(in uint ID : SV_VERTEXID, inout float4 Position : SV_POSITION, inout float2 TexCoord : TEXCOORD0)
{
    TexCoord.x = (ID == 2) ? 2.0 : 0.0;
    TexCoord.y = (ID == 1) ? 2.0 : 0.0;
    Position = float4(TexCoord * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
}

void Downsample_VS(in uint ID, inout float4 Position, inout float4 TexCoords[4], float2 TexelSize)
{
    float2 CoordVS;;
    Basic_VS(ID, Position, CoordVS);
    switch(_DownsampleMethod)
    {
        case 0: // 4x4 Box
            TexCoords[0] = CoordVS.xyxy + float4(-1.0, -1.0, 1.0, 1.0) * TexelSize.xyxy;
            break;
        case 1: // 6x6 Tent
            TexCoords[0] = CoordVS.xyyy + float4(-2.0, 2.0, 0.0, -2.0) * TexelSize.xyyy;
            TexCoords[1] = CoordVS.xyyy + float4(0.0, 2.0, 0.0, -2.0) * TexelSize.xyyy;
            TexCoords[2] = CoordVS.xyyy + float4(2.0, 2.0, 0.0, -2.0) * TexelSize.xyyy;
            break;
        case 2: // Jorge
            TexCoords[0] = CoordVS.xyxy + float4(-1.0, -1.0, 1.0, 1.0) * TexelSize.xyxy;
            TexCoords[1] = CoordVS.xyyy + float4(-2.0, 2.0, 0.0, -2.0) * TexelSize.xyyy;
            TexCoords[2] = CoordVS.xyyy + float4(0.0, 2.0, 0.0, -2.0) * TexelSize.xyyy;
            TexCoords[3] = CoordVS.xyyy + float4(2.0, 2.0, 0.0, -2.0) * TexelSize.xyyy;
            break;
        case 3: // Kawase
            TexCoords[0] = CoordVS.xyxy;
            TexCoords[1] = CoordVS.xyxy + float4(-1.0, -1.0, 1.0, 1.0) * TexelSize.xyxy;
            break;
    }
}

DOWNSAMPLE_VS(Downsample_1_VS, 1.0 / BUFFER_SIZE_0)
DOWNSAMPLE_VS(Downsample_2_VS, 1.0 / BUFFER_SIZE_1)
DOWNSAMPLE_VS(Downsample_3_VS, 1.0 / BUFFER_SIZE_2)
DOWNSAMPLE_VS(Downsample_4_VS, 1.0 / BUFFER_SIZE_3)

void Upsample_VS(in uint ID, inout float4 Position, inout float4 TexCoords[3], float2 TexelSize)
{
    float2 CoordVS = 0.0;
    Basic_VS(ID, Position, CoordVS);
    switch(_UpsampleMethod)
    {
        case 0: // 4x4 Box
            TexCoords[0] = CoordVS.xyxy + float4(-0.5, -0.5, 0.5, 0.5) * TexelSize.xyxy;
            break;
        case 1: // 6x6 Tent
            TexCoords[0] = CoordVS.xyyy + float4(-1.0, 1.0, 0.0, -1.0) * TexelSize.xyyy;
            TexCoords[1] = CoordVS.xyyy + float4(0.0, 1.0, 0.0, -1.0) * TexelSize.xyyy;
            TexCoords[2] = CoordVS.xyyy + float4(1.0, 1.0, 0.0, -1.0) * TexelSize.xyyy;
            break;
        case 2: // Kawase
            TexCoords[0] = CoordVS.xyxy + float4(-0.5, -0.5, 0.5, 0.5) * TexelSize.xyxy;
            TexCoords[1] = CoordVS.xxxy + float4(1.0, 0.0, -1.0, 0.0) * TexelSize.xxxy;
            TexCoords[2] = CoordVS.xyyy + float4(0.0, 1.0, 0.0, -1.0) * TexelSize.xyyy;
            break;
    }
}

UPSAMPLE_VS(Upsample_3_VS, 1.0 / BUFFER_SIZE_4)
UPSAMPLE_VS(Upsample_2_VS, 1.0 / BUFFER_SIZE_3)
UPSAMPLE_VS(Upsample_1_VS, 1.0 / BUFFER_SIZE_2)
UPSAMPLE_VS(Upsample_0_VS, 1.0 / BUFFER_SIZE_1)

// Pixel Shaders
// 1: https://catlikecoding.com/unity/tutorials/advanced-rendering/bloom/
// 2: http://www.iryoku.com/next-generation-post-processing-in-call-of-duty-advanced-warfare
// 3: https://community.arm.com/cfs-file/__key/communityserver-blogs-components-weblogfiles/00-00-00-20-66/siggraph2015_2D00_mmg_2D00_marius_2D00_slides.pdf
// More: https://github.com/powervr-graphics/Native_SDK

void Downsample_PS(in sampler2D Source, in float4 TexCoords[4], out float4 OutputColor)
{
    OutputColor = 0.0;

    float4 A0, A1, A2, A3,
           B0, B1, B2, B3,
           C0, C1, C2, C3,
           D0, D1, D2, D3;

    switch(_DownsampleMethod)
    {
        case 0: // 2x2 Box
            OutputColor += tex2D(Source, TexCoords[0].xw);
            OutputColor += tex2D(Source, TexCoords[0].zw);
            OutputColor += tex2D(Source, TexCoords[0].xy);
            OutputColor += tex2D(Source, TexCoords[0].zy);
            OutputColor = OutputColor / 4.0;
            break;
        case 1: // 3x3 Tent
            // Sampler locations
            // A0 B0 C0
            // A1 B1 C1
            // A2 B2 C2
            A0 = tex2D(Source, TexCoords[0].xy);
            A1 = tex2D(Source, TexCoords[0].xz);
            A2 = tex2D(Source, TexCoords[0].xw);

            B0 = tex2D(Source, TexCoords[1].xy);
            B1 = tex2D(Source, TexCoords[1].xz);
            B2 = tex2D(Source, TexCoords[1].xw);

            C0 = tex2D(Source, TexCoords[2].xy);
            C1 = tex2D(Source, TexCoords[2].xz);
            C2 = tex2D(Source, TexCoords[2].xw);

            OutputColor += ((A0 + C0 + A2 + C2) * 1.0);
            OutputColor += ((B0 + A1 + C1 + B2) * 2.0);
            OutputColor += (B1 * 4.0);
            OutputColor = OutputColor / 16.0;
            break;
        case 2: // Jorge
            // Sampler locations
            // A0    B0    C0
            //    D0    D1
            // A1    B1    C1
            //    D2    D3
            // A2    B2    C2
            D0 = tex2D(Source, TexCoords[0].xw);
            D1 = tex2D(Source, TexCoords[0].zw);
            D2 = tex2D(Source, TexCoords[0].xy);
            D3 = tex2D(Source, TexCoords[0].zy);

            A0 = tex2D(Source, TexCoords[1].xy);
            A1 = tex2D(Source, TexCoords[1].xz);
            A2 = tex2D(Source, TexCoords[1].xw);

            B0 = tex2D(Source, TexCoords[2].xy);
            B1 = tex2D(Source, TexCoords[2].xz);
            B2 = tex2D(Source, TexCoords[2].xw);

            C0 = tex2D(Source, TexCoords[3].xy);
            C1 = tex2D(Source, TexCoords[3].xz);
            C2 = tex2D(Source, TexCoords[3].xw);

            const float2 Weights = float2(0.5, 0.125) / 4.0;
            OutputColor += (D0 + D1 + D2 + D3) * Weights.x;
            OutputColor += (A0 + B0 + A1 + B1) * Weights.y;
            OutputColor += (B0 + C0 + B1 + C1) * Weights.y;
            OutputColor += (A1 + B1 + A2 + B2) * Weights.y;
            OutputColor += (B1 + C1 + B2 + C2) * Weights.y;
            break;
        case 3: // Kawase
            OutputColor += tex2D(Source, TexCoords[0].xy) * 4.0;
            OutputColor += tex2D(Source, TexCoords[1].xw);
            OutputColor += tex2D(Source, TexCoords[1].zw);
            OutputColor += tex2D(Source, TexCoords[1].xy);
            OutputColor += tex2D(Source, TexCoords[1].zy);
            OutputColor = OutputColor / 8.0;
            break;
    }

    OutputColor.a = 1.0;
}

DOWNSAMPLE_PS(Downsample_1_PS, Sample_Color)
DOWNSAMPLE_PS(Downsample_2_PS, Sample_Common_1)
DOWNSAMPLE_PS(Downsample_3_PS, Sample_Common_2)
DOWNSAMPLE_PS(Downsample_4_PS, Sample_Common_3)

void Upsample_PS(in sampler2D Source, in float4 TexCoords[3], out float4 OutputColor)
{
    OutputColor = 0.0;

    switch(_UpsampleMethod)
    {
        case 0: // 2x2 Box
            OutputColor += tex2D(Source, TexCoords[0].xw);
            OutputColor += tex2D(Source, TexCoords[0].zw);
            OutputColor += tex2D(Source, TexCoords[0].xy);
            OutputColor += tex2D(Source, TexCoords[0].zy);
            OutputColor = OutputColor / 4.0;
            break;
        case 1: // 3x3 Tent
            // Sample locations:
            // A0 B0 C0
            // A1 B1 C1
            // A2 B2 C2
            float4 A0 = tex2D(Source, TexCoords[0].xy);
            float4 A1 = tex2D(Source, TexCoords[0].xz);
            float4 A2 = tex2D(Source, TexCoords[0].xw);
            float4 B0 = tex2D(Source, TexCoords[1].xy);
            float4 B1 = tex2D(Source, TexCoords[1].xz);
            float4 B2 = tex2D(Source, TexCoords[1].xw);
            float4 C0 = tex2D(Source, TexCoords[2].xy);
            float4 C1 = tex2D(Source, TexCoords[2].xz);
            float4 C2 = tex2D(Source, TexCoords[2].xw);
            OutputColor = (((A0 + C0 + A2 + C2) * 1.0) + ((B0 + A1 + C1 + B2) * 2.0) + (B1 * 4.0)) / 16.0;
            break;
        case 2: // Kawase
            OutputColor += tex2D(Source, TexCoords[0].xw) * 2.0;
            OutputColor += tex2D(Source, TexCoords[0].zw) * 2.0;
            OutputColor += tex2D(Source, TexCoords[0].xy) * 2.0;
            OutputColor += tex2D(Source, TexCoords[0].zy) * 2.0;
            OutputColor += tex2D(Source, TexCoords[1].xw);
            OutputColor += tex2D(Source, TexCoords[1].zw);
            OutputColor += tex2D(Source, TexCoords[2].xy);
            OutputColor += tex2D(Source, TexCoords[2].xw);
            OutputColor = OutputColor / 12.0;
            break;
    }

    OutputColor.a = 1.0;
}

UPSAMPLE_PS(Upsample_3_PS, Sample_Common_4)
UPSAMPLE_PS(Upsample_2_PS, Sample_Common_3)
UPSAMPLE_PS(Upsample_1_PS, Sample_Common_2)
UPSAMPLE_PS(Upsample_0_PS, Sample_Common_1)

technique cDualFilter
{
    PASS(Downsample_1_VS, Downsample_1_PS, Render_Common_1)
    PASS(Downsample_2_VS, Downsample_2_PS, Render_Common_2)
    PASS(Downsample_3_VS, Downsample_3_PS, Render_Common_3)
    PASS(Downsample_4_VS, Downsample_4_PS, Render_Common_4)

    PASS(Upsample_3_VS, Upsample_3_PS, Render_Common_3)
    PASS(Upsample_2_VS, Upsample_2_PS, Render_Common_2)
    PASS(Upsample_1_VS, Upsample_1_PS, Render_Common_1)

    pass
    {
        VertexShader = Upsample_0_VS;
        PixelShader = Upsample_0_PS;
        #if BUFFER_COLOR_BIT_DEPTH == 8
            SRGBWriteEnable = TRUE;
        #endif
    }
}
