
/*
    Linear Gaussian blur shader

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

uniform float _Sigma <
    ui_type = "drag";
    ui_min = 0.0;
> = 1.0;

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

void Basic_VS(in uint ID : SV_VERTEXID, out float4 Position : SV_POSITION, out float2 TexCoord : TEXCOORD0)
{
    TexCoord.x = (ID == 2) ? 2.0 : 0.0;
    TexCoord.y = (ID == 1) ? 2.0 : 0.0;
    Position = float4(TexCoord * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
}

// Pixel shaders
// Linear Gaussian blur based on https://www.rastergrid.com/blog/2010/09/efficient-Gaussian-blur-with-linear-sampling/

float Gaussian(float Pixel_Index, float Sigma)
{
    const float Pi = 3.1415926535897932384626433832795f;
    float Output = rsqrt(2.0 * Pi * (Sigma * Sigma));
    return Output * exp(-(Pixel_Index * Pixel_Index) / (2.0 * Sigma * Sigma));
}

void Gaussian_Blur(in float2 TexCoord, in bool Is_Horizontal, out float4 OutputColor0)
{
    float2 Direction = Is_Horizontal ? float2(1.0, 0.0) : float2(0.0, 1.0);
    float2 PixelSize = (1.0 / float2(BUFFER_WIDTH, BUFFER_HEIGHT)) * Direction;
    float KernelSize = _Sigma * 3.0;

    if(_Sigma == 0.0)
    {
        OutputColor0 = tex2Dlod(Sample_Color, float4(TexCoord, 0.0, 0.0));
    }
    else
    {
        // Sample and weight center first to get even number sides
        float TotalWeight = Gaussian(0.0, _Sigma);
        float4 OutputColor = tex2D(Sample_Color, TexCoord) * TotalWeight;

        for(float i = 1.0; i < KernelSize; i += 2.0)
        {
            float Offset1 = i;
            float Offset2 = i + 1.0;
            float Weight1 = Gaussian(Offset1, _Sigma);
            float Weight2 = Gaussian(Offset2, _Sigma);
            float LinearWeight = Weight1 + Weight2;
            float LinearOffset = ((Offset1 * Weight1) + (Offset2 * Weight2)) / LinearWeight;

            OutputColor += tex2Dlod(Sample_Color, float4(TexCoord - LinearOffset * PixelSize, 0.0, 0.0)) * LinearWeight;
            OutputColor += tex2Dlod(Sample_Color, float4(TexCoord + LinearOffset * PixelSize, 0.0, 0.0)) * LinearWeight;
            TotalWeight += LinearWeight * 2.0;
        }

        // Normalize intensity to prevent altered output
        OutputColor0 = OutputColor / TotalWeight;
    }
}

void Horizontal_Gaussian_Blur_PS(in float4 Position : SV_POSITION, in float2 TexCoord : TEXCOORD0, out float4 OutputColor0 : SV_TARGET0)
{
    Gaussian_Blur(TexCoord, true, OutputColor0);
}

void Vertical_Gaussian_Blur_PS(in float4 Position : SV_POSITION, in float2 TexCoord : TEXCOORD0, out float4 OutputColor0 : SV_TARGET0)
{
    Gaussian_Blur(TexCoord, false, OutputColor0);
}

technique cHorizontalGaussianBlur
{
    pass
    {
        VertexShader = Basic_VS;
        PixelShader = Horizontal_Gaussian_Blur_PS;
        #if BUFFER_COLOR_BIT_DEPTH == 8
            SRGBWriteEnable = TRUE;
        #endif
    }
}

technique cVerticalGaussianBlur
{
    pass
    {
        VertexShader = Basic_VS;
        PixelShader = Vertical_Gaussian_Blur_PS;
        #if BUFFER_COLOR_BIT_DEPTH == 8
            SRGBWriteEnable = TRUE;
        #endif
    }
}

