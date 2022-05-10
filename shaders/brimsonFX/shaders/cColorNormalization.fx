
/*
    Various color normalization algorithms

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

uniform int _Select <
    ui_type = "combo";
    ui_items = " Length (RG)\0 Length (RGB)\0 Average (RG)\0 Average (RGB)\0 Sum (RG)\0 Sum (RGB)\0 Max (RG)\0 Max (RGB)\0 None\0";
    ui_label = "Method";
    ui_tooltip = "Select Chromaticity";
> = 0;

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

void Normalization_PS(in float4 Position : SV_POSITION, in float2 TexCoord : TEXCOORD0, out float3 OutputColor0 : SV_TARGET0)
{
    OutputColor0 = 0.0;
    const float Minima = exp2(-10.0);
    float3 Color = max(tex2D(Sample_Color, TexCoord).rgb, Minima);
    switch(_Select)
    {
        case 0: // Length (RG)
            OutputColor0.rg = saturate(normalize(Color).rg);
            break;
        case 1: // Length (RGB)
            OutputColor0 = saturate(normalize(Color));
            break;
        case 2: // Average (RG)
            OutputColor0.rg = saturate(Color.rg / dot(Color, 1.0 / 3.0));
            break;
        case 3: // Average (RGB)
            OutputColor0 = saturate(Color / dot(Color, 1.0 / 3.0));
            break;
        case 4: // Sum (RG)
            OutputColor0.rg = saturate(Color.rg /  dot(Color, 1.0));
            break;
        case 5: // Sum (RGB)
            OutputColor0 = saturate(Color / dot(Color, 1.0));
            break;
        case 6: // Max (RG)
            OutputColor0.rg = saturate(Color.rg / max(max(Color.r, Color.g), Color.b));
            break;
        case 7: // Max (RGB)
            OutputColor0 = saturate(Color / max(max(Color.r, Color.g), Color.b));
            break;
        default:
            // No Chromaticity
            OutputColor0 = Color;
            break;
    }
}

technique cNormalizedColor
{
    pass
    {
        VertexShader = Basic_VS;
        PixelShader = Normalization_PS;
        #if BUFFER_COLOR_BIT_DEPTH == 8
            SRGBWriteEnable = TRUE;
        #endif
    }
}
