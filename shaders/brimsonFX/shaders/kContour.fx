
/*
    Heavily modified version of KinoContour

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

/*
    https://github.com/keijiro/KinoContour

    MIT License

    Copyright (C) 2015-2017 Keijiro Takahashi

    Permission is hereby granted, free of charge, to any person obtaining a copy of
    this software and associated documentation files (the "Software"), to deal in
    the Software without restriction, including without limitation the rights to
    use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
    the Software, and to permit persons to whom the Software is furnished to do so,
    subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
    FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
    COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
    IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
    CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include "ReShade.fxh"

uniform float _Threshold <
    ui_label = "Threshold";
    ui_type = "slider";
    ui_min = 0.0; ui_max = 1.0;
> = 0.05f;

uniform float _InverseRange <
    ui_label = "Inverse Range";
    ui_type = "slider";
    ui_min = 0.0; ui_max = 1.0;
> = 0.05f;

uniform float _ColorSensitivity <
    ui_label = "Color Sensitivity";
    ui_type = "slider";
    ui_min = 0.0; ui_max = 1.0;
> = 0.0f;

uniform float4 _FrontColor <
    ui_label = "Front Color";
    ui_type = "color";
    ui_min = 0.0; ui_max = 1.0;
> = float4(1.0, 1.0, 1.0, 1.0);

uniform float4 _BackColor <
    ui_label = "Back Color";
    ui_type = "color";
    ui_min = 0.0; ui_max = 1.0;
> = float4(0.0, 0.0, 0.0, 0.0);

uniform int _Method <
    ui_type = "combo";
    ui_items = " ddx(), ddy()\0 Bilinear 3x3 Laplacian\0 Bilinear 3x3 Sobel\0 Bilinear 5x5 Prewitt\0 Bilinear 5x5 Sobel\0 3x3 Prewitt\0 3x3 Scharr\0 None\0";
    ui_label = "Method";
    ui_tooltip = "Method Edge Detection";
> = 0;

uniform bool _ScaleDerivatives <
    ui_label = "Scale Derivatives to [-1, 1] range";
    ui_type = "radio";
> = true;

uniform bool _NormalizeOutput <
    ui_label = "Normalize Output";
    ui_type = "radio";
> = true;

uniform float _NormalizeWeight <
    ui_label = "Normal Weight";
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

texture2D Render_Normals
{
    Width = BUFFER_WIDTH;
    Height = BUFFER_HEIGHT;
    Format = RG8;
};

sampler2D Sample_Normals
{
    Texture = Render_Normals;
    MagFilter = LINEAR;
    MinFilter = LINEAR;
    MipFilter = LINEAR;
};

// Vertex shaders

void Basic_VS(in uint ID : SV_VERTEXID, out float4 Position : SV_POSITION, out float2 TexCoord : TEXCOORD0)
{
    TexCoord.x = (ID == 2) ? 2.0 : 0.0;
    TexCoord.y = (ID == 1) ? 2.0 : 0.0;
    Position = float4(TexCoord * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
}

void Contour_VS(in uint ID : SV_VERTEXID, out float4 Position : SV_POSITION, out float4 TexCoords[3] : TEXCOORD0)
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

// Pixel shaders
// Generate normals: https://github.com/crosire/reshade-shaders/blob/slim/Shaders/DisplayDepth.fx [MIT]
// Normal encodes: https://knarkowicz.wordpress.com/2014/04/16/octahedron-normal-vector-encoding/
// Contour pass: https://github.com/keijiro/KinoContour [MIT]

float3 Get_Screen_Space_Normal(float2 TexCoord)
{
    float3 Offset = float3(BUFFER_PIXEL_SIZE, 0.0);
    float2 PosCenter = TexCoord.xy;
    float2 PosNorth = PosCenter - Offset.zy;
    float2 PosEast = PosCenter + Offset.xz;

    float3 VertCenter = float3(PosCenter - 0.5, 1.0) * ReShade::GetLinearizedDepth(PosCenter);
    float3 VertNorth = float3(PosNorth - 0.5,  1.0) * ReShade::GetLinearizedDepth(PosNorth);
    float3 VertEast = float3(PosEast - 0.5,   1.0) * ReShade::GetLinearizedDepth(PosEast);

    return normalize(cross(VertCenter - VertNorth, VertCenter - VertEast));
}

float2 OctWrap(float2 V)
{
    return (1.0 - abs(V.yx)) * (V.xy >= 0.0 ? 1.0 : -1.0);
}

float2 Encode(float3 Normal)
{
    // max() divide based on
    Normal /= max(max(abs(Normal.x), abs(Normal.y)), abs(Normal.z));
    Normal.xy = Normal.z >= 0.0 ? Normal.xy : OctWrap(Normal.xy);
    Normal.xy = saturate(Normal.xy * 0.5 + 0.5);
    return Normal.xy;
}

float3 Decode(float2 f)
{
    f = f * 2.0 - 1.0;
    // https://twitter.com/Stubbesaurus/status/937994790553227264
    float3 Normal = float3(f.x, f.y, 1.0 - abs(f.x) - abs(f.y));
    float T = saturate(-Normal.z);
    Normal.xy += Normal.xy >= 0.0 ? -T : T;
    return normalize(Normal);
}

void Generate_Normals_PS(in float4 Position : SV_POSITION, in float2 TexCoord : TEXCOORD0, out float2 OutputColor0 : SV_TARGET0)
{
    OutputColor0 = Encode(Get_Screen_Space_Normal(TexCoord));
}

// 0 = Color, 1 = Normal, 2 = Depth

float3 SampleTexture(float2 TexCoord, int Mode)
{
    float3 Texture = 0.0;

    switch(Mode)
    {
        case 0:
            Texture = tex2D(Sample_Color, TexCoord).xyz;
            break;
        case 1:
            Texture = Decode(tex2D(Sample_Normals, TexCoord).xy);
            break;
        case 2:
            Texture = ReShade::GetLinearizedDepth(TexCoord);
            break;
    }

    return Texture;
}

float4 Scale_Derivative(float4 Input)
{
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

    Input = (_ScaleDerivatives) ? Input * Weights[_Method] : Input;
    return Input;
}

void Contour(in float4 TexCoords[3], in int Mode, out float4 OutputColor0)
{
    float4 Ix, Iy, Gradient;
    float4 A0, B0, C0;
    float4 A1, B1, C1;
    float4 A2, B2, C2;

    switch(_Method)
    {
        case 0: // Fwidth
            A0 = SampleTexture(TexCoords[0].xy, Mode);
            Ix = ddx(A0);
            Iy = ddy(A0);
            break;
        case 1: // Bilinear 3x3 Laplacian
            // A0    C0
            //    B1
            // A2    C2
            A0 = SampleTexture(TexCoords[1].xw, Mode); // <-0.5, +0.5>
            C0 = SampleTexture(TexCoords[1].zw, Mode); // <+0.5, +0.5>
            B1 = SampleTexture(TexCoords[0].xy, Mode); // < 0.0,  0.0>
            A2 = SampleTexture(TexCoords[1].xy, Mode); // <-0.5, -0.5>
            C2 = SampleTexture(TexCoords[1].zy, Mode); // <+0.5, -0.5>
            Gradient = (A0 + C0 + A2 + C2) - (B1 * 4.0);
            break;
        case 2: // Bilinear 3x3 Sobel
            A0 = SampleTexture(TexCoords[0].xw, Mode).rgb * 4.0; // <-0.5, +0.5>
            C0 = SampleTexture(TexCoords[0].zw, Mode).rgb * 4.0; // <+0.5, +0.5>
            A2 = SampleTexture(TexCoords[0].xy, Mode).rgb * 4.0; // <-0.5, -0.5>
            C2 = SampleTexture(TexCoords[0].zy, Mode).rgb * 4.0; // <+0.5, -0.5>

            Ix = Scale_Derivative((C0 + C2) - (A0 + A2));
            Iy = Scale_Derivative((A0 + C0) - (A2 + C2));
            break;
        case 3: // Bilinear 5x5 Prewitt
            // A0 B0 C0
            // A1    C1
            // A2 B2 C2
            A0 = SampleTexture(TexCoords[0].xy, Mode) * 4.0; // <-1.5, +1.5>
            A1 = SampleTexture(TexCoords[0].xz, Mode) * 2.0; // <-1.5,  0.0>
            A2 = SampleTexture(TexCoords[0].xw, Mode) * 4.0; // <-1.5, -1.5>
            B0 = SampleTexture(TexCoords[1].xy, Mode) * 2.0; // < 0.0, +1.5>
            B2 = SampleTexture(TexCoords[1].xw, Mode) * 2.0; // < 0.0, -1.5>
            C0 = SampleTexture(TexCoords[2].xy, Mode) * 4.0; // <+1.5, +1.5>
            C1 = SampleTexture(TexCoords[2].xz, Mode) * 2.0; // <+1.5,  0.0>
            C2 = SampleTexture(TexCoords[2].xw, Mode) * 4.0; // <+1.5, -1.5>

            // -1 -1  0  +1 +1
            // -1 -1  0  +1 +1
            // -1 -1  0  +1 +1
            // -1 -1  0  +1 +1
            // -1 -1  0  +1 +1
            Ix = Scale_Derivative((C0 + C1 + C2) - (A0 + A1 + A2));

            // +1 +1 +1 +1 +1
            // +1 +1 +1 +1 +1
            //  0  0  0  0  0
            // -1 -1 -1 -1 -1
            // -1 -1 -1 -1 -1
            Iy = Scale_Derivative((A0 + B0 + C0) - (A2 + B2 + C2));
            break;
        case 4: // Bilinear 5x5 Sobel by CeeJayDK
            //   B1 B2
            // A0     A1
            // A2     B0
            //   C0 C1
            A0 = SampleTexture(TexCoords[0].xw, Mode) * 4.0; // <-1.5, +0.5>
            A1 = SampleTexture(TexCoords[0].yw, Mode) * 4.0; // <+1.5, +0.5>
            A2 = SampleTexture(TexCoords[0].xz, Mode) * 4.0; // <-1.5, -0.5>
            B0 = SampleTexture(TexCoords[0].yz, Mode) * 4.0; // <+1.5, -0.5>
            B1 = SampleTexture(TexCoords[1].xw, Mode) * 4.0; // <-0.5, +1.5>
            B2 = SampleTexture(TexCoords[1].yw, Mode) * 4.0; // <+0.5, +1.5>
            C0 = SampleTexture(TexCoords[1].xz, Mode) * 4.0; // <-0.5, -1.5>
            C1 = SampleTexture(TexCoords[1].yz, Mode) * 4.0; // <+0.5, -1.5>

            //    -1 0 +1
            // -1 -2 0 +2 +1
            // -2 -2 0 +2 +2
            // -1 -2 0 +2 +1
            //    -1 0 +1
            Ix = Scale_Derivative((B2 + A1 + B0 + C1) - (B1 + A0 + A2 + C0));

            //    +1 +2 +1
            // +1 +2 +2 +2 +1
            //  0  0  0  0  0
            // -1 -2 -2 -2 -1
            //    -1 -2 -1
            Iy = Scale_Derivative((A0 + B1 + B2 + A1) - (A2 + C0 + C1 + B0));
            break;
        case 5: // 3x3 Prewitt
            // A0 B0 C0
            // A1     C1
            // A2 B2 C2
            A0 = SampleTexture(TexCoords[0].xy, Mode);
            A1 = SampleTexture(TexCoords[0].xz, Mode);
            A2 = SampleTexture(TexCoords[0].xw, Mode);
            B0 = SampleTexture(TexCoords[1].xy, Mode);
            B2 = SampleTexture(TexCoords[1].xw, Mode);
            C0 = SampleTexture(TexCoords[2].xy, Mode);
            C1 = SampleTexture(TexCoords[2].xz, Mode);
            C2 = SampleTexture(TexCoords[2].xw, Mode);

            Ix = Scale_Derivative((C0 + C1 + C2) - (A0 + A1 + A2));
            Iy = Scale_Derivative((A0 + B0 + C0) - (A2 + B2 + C2));
            break;
        case 6: // 3x3 Scharr
        {
            A0 = SampleTexture(TexCoords[0].xy, Mode) * 3.0;
            A1 = SampleTexture(TexCoords[0].xz, Mode) * 10.0;
            A2 = SampleTexture(TexCoords[0].xw, Mode) * 3.0;
            B0 = SampleTexture(TexCoords[1].xy, Mode) * 10.0;
            B2 = SampleTexture(TexCoords[1].xw, Mode) * 10.0;
            C0 = SampleTexture(TexCoords[2].xy, Mode) * 3.0;
            C1 = SampleTexture(TexCoords[2].xz, Mode) * 10.0;
            C2 = SampleTexture(TexCoords[2].xw, Mode) * 3.0;

            Ix = Scale_Derivative((C0 + C1 + C2) - (A0 + A1 + A2));
            Iy = Scale_Derivative((A0 + B0 + C0) - (A2 + B2 + C2));
            break;
        }
    }

    if(_Method == 1)
    {
        Gradient = length(Gradient) * rsqrt(3.0);
    }
    else
    {
        Ix.rgb = (_NormalizeOutput) ?  Ix.rgb * rsqrt(dot(Ix.rgb, Ix.rgb) + _NormalizeWeight) : Ix.rgb;
        Iy.rgb = (_NormalizeOutput) ?  Iy.rgb * rsqrt(dot(Iy.rgb, Iy.rgb) + _NormalizeWeight) : Iy.rgb;
        Gradient = sqrt(dot(Ix.rgb, Ix.rgb) + dot(Iy.rgb, Iy.rgb));
    }

    // Thresholding
    Gradient = Gradient * _ColorSensitivity;

    float3 Base = 0.0;

    if(_Method == 0 || _Method == 1)
    {
        Base = tex2D(Sample_Color, TexCoords[0].xy).rgb;
    }
    else
    {
        Base = tex2D(Sample_Color, TexCoords[1].xz).rgb;
    }

    Gradient = saturate((Gradient - _Threshold) * _InverseRange);
    float3 Color_Background = lerp(Base, _BackColor.rgb, _BackColor.a);
    OutputColor0 = lerp(Color_Background, _FrontColor.rgb, Gradient.a * _FrontColor.a);
}

void Contour_Color_PS(in float4 Position : SV_POSITION, in float4 TexCoords[3] : TEXCOORD0, out float4 OutputColor0 : SV_TARGET0)
{
    Contour(TexCoords, 0, OutputColor0);
}

void Contour_Normal_PS(in float4 Position : SV_POSITION, in float4 TexCoords[3] : TEXCOORD0, out float4 OutputColor0 : SV_TARGET0)
{
    Contour(TexCoords, 1, OutputColor0);
}

void Contour_Depth_PS(in float4 Position : SV_POSITION, in float4 TexCoords[3] : TEXCOORD0, out float4 OutputColor0 : SV_TARGET0)
{
    Contour(TexCoords, 2, OutputColor0);
}

technique KinoContourColor
{
    pass
    {
        VertexShader = Contour_VS;
        PixelShader = Contour_Color_PS;
        #if BUFFER_COLOR_BIT_DEPTH == 8
            SRGBWriteEnable = TRUE;
        #endif
    }
}

technique KinoContourDepth
{
    pass
    {
        VertexShader = Contour_VS;
        PixelShader = Contour_Depth_PS;
        #if BUFFER_COLOR_BIT_DEPTH == 8
            SRGBWriteEnable = TRUE;
        #endif
    }
}

technique KinoContourNormal
{
    pass
    {
        VertexShader = Basic_VS;
        PixelShader = Generate_Normals_PS;
        RenderTarget0 = Render_Normals;
    }

    pass
    {
        VertexShader = Contour_VS;
        PixelShader = Contour_Normal_PS;
        #if BUFFER_COLOR_BIT_DEPTH == 8
            SRGBWriteEnable = TRUE;
        #endif
    }
}
