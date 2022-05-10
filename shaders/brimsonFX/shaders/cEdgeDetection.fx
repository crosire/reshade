
/*
    Various edge detection shaders

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

uniform int _Method <
    ui_type = "combo";
    ui_items = " ddx(), ddy()\0 Bilinear 3x3 Laplacian\0 Bilinear 3x3 Sobel\0 Bilinear 5x5 Prewitt\0 Bilinear 5x5 Sobel\0 3x3 Prewitt\0 3x3 Scharr\0 None\0";
    ui_label = "Method";
    ui_tooltip = "Method Edge Detection";
> = 0;

uniform bool _Scale <
    ui_label = "Scale Gradients to [-1, 1] range";
    ui_type = "radio";
> = true;

uniform bool _Normalize <
    ui_label = "Normalize Gradients";
    ui_type = "radio";
> = true;

uniform bool _Normal <
    ui_label = "Scale Gradients to [0, 1] range";
    ui_type = "radio";
> = true;

uniform float _NormalizeWeight <
    ui_label = "Normalize Weight";
    ui_type = "drag";
    ui_min = 0.0;
> = 0.1;

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

void Edge_Detection_VS(in uint ID : SV_VERTEXID, out float4 Position : SV_POSITION, out float4 TexCoords[3] : TEXCOORD0)
{
    float2 CoordVS = 0.0;
    Basic_VS(ID, Position, CoordVS);
    const float2 PixelSize = 1.0 / int2(BUFFER_WIDTH, BUFFER_HEIGHT);

    TexCoords[0] = 0.0;
    TexCoords[1] = 0.0;
    TexCoords[2] = 0.0;

    switch(_Method)
    {
        case 0: // Fwidth
            TexCoords[0].xy = CoordVS;
            break;
        case 1: // Bilinear 3x3 Laplacian
            TexCoords[0].xy = CoordVS;
            TexCoords[1] = CoordVS.xyxy + (float4(-0.5, -0.5, 0.5, 0.5) * PixelSize.xyxy);
            break;
        case 2: // Bilinear 3x3 Sobel
            TexCoords[0] = CoordVS.xyxy + (float4(-0.5, -0.5, 0.5, 0.5) * PixelSize.xyxy);
            break;
        case 3: // Bilinear 5x5 Prewitt
            TexCoords[0] = CoordVS.xyyy + (float4(-1.5, 1.5, 0.0, -1.5) * PixelSize.xyyy);
            TexCoords[1] = CoordVS.xyyy + (float4( 0.0, 1.5, 0.0, -1.5) * PixelSize.xyyy);
            TexCoords[2] = CoordVS.xyyy + (float4( 1.5, 1.5, 0.0, -1.5) * PixelSize.xyyy);
            break;
        case 4: // Bilinear 5x5 Sobel
            TexCoords[0] = CoordVS.xxyy + (float4(-1.5, 1.5, -0.5, 0.5) * PixelSize.xxyy);
            TexCoords[1] = CoordVS.xxyy + (float4(-0.5, 0.5, -1.5, 1.5) * PixelSize.xxyy);
            break;
        case 5: // 3x3 Prewitt
            TexCoords[0] = CoordVS.xyyy + (float4(-1.0, 1.0, 0.0, -1.0) * PixelSize.xyyy);
            TexCoords[1] = CoordVS.xyyy + (float4(0.0, 1.0, 0.0, -1.0) * PixelSize.xyyy);
            TexCoords[2] = CoordVS.xyyy + (float4(1.0, 1.0, 0.0, -1.0) * PixelSize.xyyy);
            break;
        case 6: // 3x3 Scharr
            TexCoords[0] = CoordVS.xyyy + (float4(-1.0, 1.0, 0.0, -1.0) * PixelSize.xyyy);
            TexCoords[1] = CoordVS.xyyy + (float4(0.0, 1.0, 0.0, -1.0) * PixelSize.xyyy);
            TexCoords[2] = CoordVS.xyyy + (float4(1.0, 1.0, 0.0, -1.0) * PixelSize.xyyy);
            break;
    }
}

void Edge_Operator(in sampler2D Source, in float4 TexCoords[3], inout float4 Ix, inout float4 Iy, inout float4 Gradient)
{
    float4 A0, B0, C0;
    float4 A1, B1, C1;
    float4 A2, B2, C2;

    switch(_Method)
    {
        case 0: // Fwidth
            A0 = tex2D(Source, TexCoords[0].xy);
            Ix = ddx(A0);
            Iy = ddy(A0);
            break;
        case 1: // Bilinear 3x3 Laplacian
            // A0    C0
            //    B1
            // A2    C2
            A0 = tex2D(Source, TexCoords[1].xw); // <-0.5, +0.5>
            C0 = tex2D(Source, TexCoords[1].zw); // <+0.5, +0.5>
            B1 = tex2D(Source, TexCoords[0].xy); // < 0.0,  0.0>
            A2 = tex2D(Source, TexCoords[1].xy); // <-0.5, -0.5>
            C2 = tex2D(Source, TexCoords[1].zy); // <+0.5, -0.5>
            Gradient = (A0 + C0 + A2 + C2) - (B1 * 4.0);
            break;
        case 2: // Bilinear 3x3 Sobel
            A0 = tex2D(Source, TexCoords[0].xw).rgb; // <-0.5, +0.5>
            C0 = tex2D(Source, TexCoords[0].zw).rgb; // <+0.5, +0.5>
            A2 = tex2D(Source, TexCoords[0].xy).rgb; // <-0.5, -0.5>
            C2 = tex2D(Source, TexCoords[0].zy).rgb; // <+0.5, -0.5>
            Ix = ((C0 + C2) - (A0 + A2)) * 4.0;
            Iy = ((A0 + C0) - (A2 + C2)) * 4.0;
            break;
        case 3: // Bilinear 5x5 Prewitt
            // A0 B0 C0
            // A1    C1
            // A2 B2 C2
            A0 = tex2D(Source, TexCoords[0].xy) * 4.0; // <-1.5, +1.5>
            A1 = tex2D(Source, TexCoords[0].xz) * 2.0; // <-1.5,  0.0>
            A2 = tex2D(Source, TexCoords[0].xw) * 4.0; // <-1.5, -1.5>
            B0 = tex2D(Source, TexCoords[1].xy) * 2.0; // < 0.0, +1.5>
            B2 = tex2D(Source, TexCoords[1].xw) * 2.0; // < 0.0, -1.5>
            C0 = tex2D(Source, TexCoords[2].xy) * 4.0; // <+1.5, +1.5>
            C1 = tex2D(Source, TexCoords[2].xz) * 2.0; // <+1.5,  0.0>
            C2 = tex2D(Source, TexCoords[2].xw) * 4.0; // <+1.5, -1.5>

            // -1 -1  0  +1 +1
            // -1 -1  0  +1 +1
            // -1 -1  0  +1 +1
            // -1 -1  0  +1 +1
            // -1 -1  0  +1 +1
            Ix = (C0 + C1 + C2) - (A0 + A1 + A2);

            // +1 +1 +1 +1 +1
            // +1 +1 +1 +1 +1
            //  0  0  0  0  0
            // -1 -1 -1 -1 -1
            // -1 -1 -1 -1 -1
            Iy = (A0 + B0 + C0) - (A2 + B2 + C2);
            break;
        case 4: // Bilinear 5x5 Sobel by CeeJayDK
            //   B1 B2
            // A0     A1
            // A2     B0
            //   C0 C1
            A0 = tex2D(Source, TexCoords[0].xw) * 4.0; // <-1.5, +0.5>
            A1 = tex2D(Source, TexCoords[0].yw) * 4.0; // <+1.5, +0.5>
            A2 = tex2D(Source, TexCoords[0].xz) * 4.0; // <-1.5, -0.5>
            B0 = tex2D(Source, TexCoords[0].yz) * 4.0; // <+1.5, -0.5>
            B1 = tex2D(Source, TexCoords[1].xw) * 4.0; // <-0.5, +1.5>
            B2 = tex2D(Source, TexCoords[1].yw) * 4.0; // <+0.5, +1.5>
            C0 = tex2D(Source, TexCoords[1].xz) * 4.0; // <-0.5, -1.5>
            C1 = tex2D(Source, TexCoords[1].yz) * 4.0; // <+0.5, -1.5>

            //    -1 0 +1
            // -1 -2 0 +2 +1
            // -2 -2 0 +2 +2
            // -1 -2 0 +2 +1
            //    -1 0 +1
            Ix = (B2 + A1 + B0 + C1) - (B1 + A0 + A2 + C0);

            //    +1 +2 +1
            // +1 +2 +2 +2 +1
            //  0  0  0  0  0
            // -1 -2 -2 -2 -1
            //    -1 -2 -1
            Iy = (A0 + B1 + B2 + A1) - (A2 + C0 + C1 + B0);
            break;
        case 5: // 3x3 Prewitt
            // A0 B0 C0
            // A1    C1
            // A2 B2 C2
            A0 = tex2D(Sample_Color, TexCoords[0].xy);
            A1 = tex2D(Sample_Color, TexCoords[0].xz);
            A2 = tex2D(Sample_Color, TexCoords[0].xw);
            B0 = tex2D(Sample_Color, TexCoords[1].xy);
            B2 = tex2D(Sample_Color, TexCoords[1].xw);
            C0 = tex2D(Sample_Color, TexCoords[2].xy);
            C1 = tex2D(Sample_Color, TexCoords[2].xz);
            C2 = tex2D(Sample_Color, TexCoords[2].xw);

            Ix = (C0 + C1 + C2) - (A0 + A1 + A2);
            Iy = (A0 + B0 + C0) - (A2 + B2 + C2);
            break;
        case 6: // 3x3 Scharr
        {
            A0 = tex2D(Sample_Color, TexCoords[0].xy) * 3.0;
            A1 = tex2D(Sample_Color, TexCoords[0].xz) * 10.0;
            A2 = tex2D(Sample_Color, TexCoords[0].xw) * 3.0;
            B0 = tex2D(Sample_Color, TexCoords[1].xy) * 10.0;
            B2 = tex2D(Sample_Color, TexCoords[1].xw) * 10.0;
            C0 = tex2D(Sample_Color, TexCoords[2].xy) * 3.0;
            C1 = tex2D(Sample_Color, TexCoords[2].xz) * 10.0;
            C2 = tex2D(Sample_Color, TexCoords[2].xw) * 3.0;

            Ix = (C0 + C1 + C2) - (A0 + A1 + A2);
            Iy = (A0 + B0 + C0) - (A2 + B2 + C2);
            break;
        }
    }
}

void Edge_Detection_PS(in float4 Position : SV_POSITION, in float4 TexCoords[3] : TEXCOORD0, out float4 OutputColor0 : SV_TARGET0)
{
    OutputColor0 = 1.0;
    float4 Ix, Iy, Gradient;
    Edge_Operator(Sample_Color, TexCoords, Ix, Iy, Gradient);

    const float Weights[7] = 
    {
        1.0 / 1.0, // Fwidth
        1.0 / 1.0, // Bilinear 3x3 Laplacian
        1.0 / 4.0, // Bilinear 3x3 Sobel
        1.0 / 10.0, // Bilinear 5x5 Prewitt
        1.0 / 12.0, // Bilinear 5x5 Sobel by CeeJayDK
        1.0 / 3.0, // 3x3 Prewitt
        1.0 / 16.0, // 3x3 Scharr
    };

    Ix = (_Scale) ? Ix * Weights[_Method] : Ix;
    Iy = (_Scale) ? Iy * Weights[_Method] : Iy;

    Ix = (_Normalize) ? Ix / sqrt(dot(Ix.rgb, Ix.rgb) + _NormalizeWeight) : Ix;
    Iy = (_Normalize) ? Iy / sqrt(dot(Iy.rgb, Iy.rgb) + _NormalizeWeight) : Iy;

    // Output Results

    if(_Method == 1) // Laplacian
    {
        OutputColor0 = length(Gradient.rgb) * rsqrt(3.0);
    }
    else // Edge detection
    {
        OutputColor0.r = dot(Ix.rgb, 1.0 / 3.0);
        OutputColor0.g = dot(Iy.rgb, 1.0 / 3.0);
        OutputColor0.b = (_Normal) ? 1.0 : 0.0;
        OutputColor0 = (_Normal) ? OutputColor0 * 0.5 + 0.5 : OutputColor0;
    }
}

technique cEdgeDetection
{
    pass
    {
        VertexShader = Edge_Detection_VS;
        PixelShader = Edge_Detection_PS;
        #if BUFFER_COLOR_BIT_DEPTH == 8
            SRGBWriteEnable = TRUE;
        #endif
    }
}
