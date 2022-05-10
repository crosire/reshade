
/*
    Convolution with Vogel spirals and mipmaps

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

uniform float _Offset <
    ui_label = "Sample offset";
    ui_type = "drag";
    ui_min = 0.0;
> = 0.0;

uniform float _Radius <
    ui_label = "Convolution radius";
    ui_type = "drag";
    ui_min = 0.0;
> = 64.0;

uniform int _Samples <
    ui_label = "Convolution sample count";
    ui_type = "drag";
    ui_min = 0;
> = 16;

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

texture2D Render_Common_1 < pooled = true; >
{
    Width = BUFFER_WIDTH >> 1;
    Height = BUFFER_HEIGHT >> 1;
    Format = RGBA8;
    MipLevels = 8;
};

sampler2D Sample_Common_RGBA8_1
{
    Texture = Render_Common_1;
    MagFilter = LINEAR;
    MinFilter = LINEAR;
    MipFilter = LINEAR;
    #if BUFFER_COLOR_BIT_DEPTH == 8
        SRGBTexture = TRUE;
    #endif
};

// Vertex shaders

void Basic_VS(in uint ID : SV_VERTEXID, out float4 Position : SV_POSITION, out float2 TexCoord : TEXCOORD0)
{
    TexCoord.x = (ID == 2) ? 2.0 : 0.0;
    TexCoord.y = (ID == 1) ? 2.0 : 0.0;
    Position = float4(TexCoord * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
}

// Pixel shaders
// Repurposed Wojciech Sterna's shadow sampling code as a screen-space convolution
// http://maxest.gct-game.net/content/chss.pdf

void Vogel_Sample(int Index, int SamplesCount, float Phi, out float2 Output)
{
    const float GoldenAngle = 2.4;
    float Radius = sqrt(float(Index) + 0.5) * rsqrt(float(SamplesCount));
    float Theta = float(Index) * GoldenAngle + Phi;

    float2 SineCosine;
    SineCosine[0] = sin(Theta);
    SineCosine[1] = cos(Theta);
    Output = Radius * SineCosine;
}

void VogelBlur(sampler2D Source, float2 TexCoord, float2 ScreenSize, float Radius, int Samples, float Phi, out float4 OutputColor)
{
    // Initialize variables we need to accumulate samples and calculate offsets
    float2 Output = 0.0;
    float2 Offset = 0.0;

    // LOD calculation to fill in the gaps between samples
    const float Pi = 3.1415926535897932384626433832795;
    float SampleArea = Pi * (Radius * Radius) / float(Samples);
    float LOD = max(0.0, 0.5 * log2(SampleArea));

    // Offset and weighting attributes
    float2 PixelSize = 1.0 / ldexp(ScreenSize, -LOD);
    float Weight = 1.0 / (float(Samples) + 1.0);

    for(int i = 0; i < Samples; i++)
    {
        Vogel_Sample(i, Samples, Phi, Offset);
        OutputColor += tex2Dlod(Source, float4(TexCoord + (Offset * PixelSize), 0.0, LOD)) * Weight;
    }
}

void Blit_PS(in float4 Position : SV_POSITION, in float2 TexCoord : TEXCOORD0, out float4 OutputColor0 : SV_TARGET0)
{
    OutputColor0 = tex2D(Sample_Color, TexCoord);
}

void Vogel_Convolution_PS(in float4 Position : SV_POSITION, in float2 TexCoord : TEXCOORD0, out float4 OutputColor0 : SV_TARGET0)
{
    VogelBlur(Sample_Common_RGBA8_1, TexCoord, int2(BUFFER_WIDTH / 2, BUFFER_HEIGHT / 2), _Radius, _Samples, _Offset, OutputColor0);
}

technique cBlur
{
    pass Generate_Mip_Levels
    {
        VertexShader = Basic_VS;
        PixelShader = Blit_PS;
        RenderTarget0 = Render_Common_1;
        #if BUFFER_COLOR_BIT_DEPTH == 8
            SRGBWriteEnable = TRUE;
        #endif
    }

    pass Vogel_Blur
    {
        VertexShader = Basic_VS;
        PixelShader = Vogel_Convolution_PS;
        #if BUFFER_COLOR_BIT_DEPTH == 8
            SRGBWriteEnable = TRUE;
        #endif
    }
}
