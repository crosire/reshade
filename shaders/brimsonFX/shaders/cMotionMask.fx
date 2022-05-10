
/*
    Simple motion masking shader

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

namespace Motion_Mask
{
    uniform float _BlendFactor <
        ui_type = "slider";
        ui_label = "Temporal blending factor";
        ui_min = 0.0;
        ui_max = 1.0;
    > = 0.5;

    uniform float _MinThreshold <
        ui_type = "slider";
        ui_label = "Min threshold";
        ui_min = 0.0;
        ui_max = 1.0;
    > = 0.0;

    uniform float _MaxThreshold <
        ui_type = "slider";
        ui_label = "Max threshold";
        ui_min = 0.0;
        ui_max = 1.0;
    > = 0.5;

    uniform float _DifferenceWeight <
        ui_type = "slider";
        ui_label = "Difference Weight";
        ui_min = 0.0;
        ui_max = 2.0;
    > = 1.0;

    uniform bool _NormalizeInput <
        ui_type = "radio";
        ui_label = "Normalize Input";
    > = false;

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

    texture2D Render_Current
    {
        Width = BUFFER_WIDTH;
        Height = BUFFER_HEIGHT;
        Format = RG8;
    };

    sampler2D Sample_Current
    {
        Texture = Render_Current;
        MagFilter = LINEAR;
        MinFilter = LINEAR;
        MipFilter = LINEAR;
    };

    texture2D Render_Difference
    {
        Width = BUFFER_WIDTH;
        Height = BUFFER_HEIGHT;
        Format = R8;
    };

    sampler2D Sample_Difference
    {
        Texture = Render_Difference;
        MagFilter = LINEAR;
        MinFilter = LINEAR;
        MipFilter = LINEAR;
    };

    texture2D Render_Previous
    {
        Width = BUFFER_WIDTH;
        Height = BUFFER_HEIGHT;
        Format = RG8;
    };

    sampler2D Sample_Previous
    {
        Texture = Render_Previous;
        MagFilter = LINEAR;
        MinFilter = LINEAR;
        MipFilter = LINEAR;
    };

    // Vertex shaders

    void Basic_VS(in uint ID : SV_VERTEXID, out float4 Position : SV_POSITION, out float2 TexCoord : TEXCOORD0)
    {
        TexCoord.x = (ID == 2) ? 2.0 : 0.0;
        TexCoord.y = (ID == 1) ? 2.0 : 0.0;
        Position = TexCoord.xyxy * float4(2.0, -2.0, 0.0, 0.0) + float4(-1.0, 1.0, 0.0, 1.0);
    }

    // Pixel shaders

    void Blit_0_PS(in float4 Position : SV_POSITION, in float2 TexCoord : TEXCOORD0, out float4 OutputColor0 : SV_TARGET0)
    {
        float3 Color = max(tex2D(Sample_Color, TexCoord).rgb, exp2(-10.0));
        OutputColor0 = (_NormalizeInput) ? saturate(Color.xy / dot(Color, 1.0)) : max(max(Color.r, Color.g), Color.b);
    }

    void Difference_PS(in float4 Position : SV_POSITION, in float2 TexCoord : TEXCOORD0, out float4 OutputColor0 : SV_TARGET0)
    {
        float Difference = 0.0;

        if(_NormalizeInput)
        {
            float2 Current = tex2D(Sample_Current, TexCoord).rg;
            float2 Previous = tex2D(Sample_Previous, TexCoord).rg;
            Difference = abs(dot(Current - Previous, 1.0)) * _DifferenceWeight;
        }
        else
        {
            float Current = tex2D(Sample_Current, TexCoord).r;
            float Previous = tex2D(Sample_Previous, TexCoord).r;
            Difference = abs(Current - Previous) * _DifferenceWeight;
        }

        OutputColor0 = (Difference < _MinThreshold) ? 0.0 : Difference;
        OutputColor0 = (Difference > _MaxThreshold) ? 1.0 : Difference;
        OutputColor0.a = _BlendFactor;
    }

    void Output_PS(in float4 Position : SV_POSITION, in float2 TexCoord : TEXCOORD0, out float4 OutputColor0 : SV_TARGET0)
    {
        OutputColor0 = tex2D(Sample_Difference, TexCoord).r;
    }

    void Blit_1_PS(in float4 Position : SV_POSITION, in float2 TexCoord : TEXCOORD0, out float4 OutputColor0 : SV_TARGET0)
    {
        OutputColor0 = tex2D(Sample_Current, TexCoord);
    }

    technique cMotionMask
    {
        pass
        {
            VertexShader = Basic_VS;
            PixelShader = Blit_0_PS;
            RenderTarget0 = Render_Current;
        }

        pass
        {
            VertexShader = Basic_VS;
            PixelShader = Difference_PS;
            RenderTarget0 = Render_Difference;
            ClearRenderTargets = FALSE;
            BlendEnable = TRUE;
            BlendOp = ADD;
            SrcBlend = INVSRCALPHA;
            DestBlend = SRCALPHA;
        }

        pass
        {
            VertexShader = Basic_VS;
            PixelShader = Output_PS;
            #if BUFFER_COLOR_BIT_DEPTH == 8
                SRGBWriteEnable = TRUE;
            #endif
        }

        pass
        {
            VertexShader = Basic_VS;
            PixelShader = Blit_1_PS;
            RenderTarget0 = Render_Previous;
        }
    }
}
