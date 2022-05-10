
/*
    Ping-Pong gaussian blur shader, for BlueSkyDefender

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

uniform int _Radius <
    ui_min = 1;
    ui_type = "drag";
> = 1;

#ifndef ENABLE_PINGPONG
    #define ENABLE_PINGPONG 1
#endif

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

texture2D Render_Buffer_A
{
    Width = BUFFER_WIDTH / 2;
    Height = BUFFER_HEIGHT / 2;
    Format = RGBA8;
};

sampler2D Sample_Buffer_A
{
    Texture = Render_Buffer_A;
    MagFilter = LINEAR;
    MinFilter = LINEAR;
    MipFilter = LINEAR;
    #if BUFFER_COLOR_BIT_DEPTH == 8
        SRGBTexture = TRUE;
    #endif
};

texture2D Render_Buffer_B
{
    Width = BUFFER_WIDTH / 2;
    Height = BUFFER_HEIGHT / 2;
    Format = RGBA8;
};

sampler2D Sample_Buffer_B
{
    Texture = Render_Buffer_B;
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

// Pixel Shaders

/*
    Why is this method called ping-ponging?
        Answer: https://diplomacy.state.gov/u-s-diplomacy-stories/ping-pong-diplomacy/
                The game of ping-pong involves two players hitting a ball back-and-forth

        We can apply this logic to shader programming by setting up
            1. The 2 players (textures)
                - One texture will be the hitter (texture we sample from), the other the receiver (texture we write to)
                - The roles for both textures will switch at each pass
            2. The ball (the texels in the pixel shader)
            3. The way the player hits the ball (PixelShader)

    This shader's technique is an example of the 2 steps above
        Pregame: Set up 2 players (Render_Buffer_A and Render_Buffer_B)
        PingPong1: Render_Buffer_A hits (Horizontal_Blur_0_PS) to Render_Buffer_B
        PingPong2: Render_Buffer_B hits (Vertical_Blur_0_PS) to Render_Buffer_A
        PingPong3: Render_Buffer_A hits (Horizontal_Blur_1_PS) to Render_Buffer_B
        PingPong4: Render_Buffer_B hits (Vertical_Blur_1_PS) to Render_Buffer_A

    "Why two textures? Can't we just read and write to one texture"?
        Unfortunately we cannot sample from and to memory at the same time

    NOTES
        Be cautious when pingponging in shaders that use BlendOps or involve temporal accumulation.
        Therefore, I recommend you to enable ClearRenderTargets as a sanity check.
        In addition, you may need to use use RenderTargetWriteMask if you're pingponging using textures that stores
        components that do not need pingponging (see my motion shaders as an example of this)
*/

float4 Gaussian_Blur(sampler2D Source, float2 TexCoord, const float2 Direction)
{
    float4 Output = 0.0;
    const float2 PixelSize = (2.0 / float2(BUFFER_WIDTH, BUFFER_HEIGHT)) * Direction;
    const float Weight = 1.0 / _Radius;

    for(float Index = -_Radius + 0.5; Index <= _Radius; Index += 2.0)
    {
        Output += tex2Dlod(Source, float4(TexCoord + Index * PixelSize, 0.0, 0.0)) * Weight;
    }

    return Output;
}

void Blit_PS(in float4 Position : SV_POSITION, in float2 TexCoord : TEXCOORD0, out float4 OutputColor0 : SV_TARGET0)
{
    OutputColor0 = tex2D(Sample_Color, TexCoord);
}

void Horizontal_Blur_0_PS(in float4 Position : SV_POSITION, in float2 TexCoord : TEXCOORD0, out float4 OutputColor0 : SV_TARGET0)
{
    OutputColor0 = Gaussian_Blur(Sample_Buffer_A, TexCoord, float2(1.0, 0.0));
}

void Vertical_Blur_0_PS(in float4 Position : SV_POSITION, in float2 TexCoord : TEXCOORD0, out float4 OutputColor0 : SV_TARGET0)
{
    OutputColor0 = Gaussian_Blur(Sample_Buffer_B, TexCoord, float2(0.0, 1.0));
}

void Horizontal_Blur_1_PS(in float4 Position : SV_POSITION, in float2 TexCoord : TEXCOORD0, out float4 OutputColor0 : SV_TARGET0)
{
    OutputColor0 = Gaussian_Blur(Sample_Buffer_A, TexCoord, float2(1.0, 0.0));
}

void Vertical_Blur_1_PS(in float4 Position : SV_POSITION, in float2 TexCoord : TEXCOORD0, out float4 OutputColor0 : SV_TARGET0)
{
    OutputColor0 = Gaussian_Blur(Sample_Buffer_B, TexCoord, float2(0.0, 1.0));
}

void OutputPS(in float4 Position : SV_POSITION, in float2 TexCoord : TEXCOORD0, out float4 OutputColor0 : SV_TARGET0)
{
    OutputColor0 = tex2D(Sample_Buffer_A, TexCoord);
}

technique cPingPong
{
    pass
    {
        VertexShader = Basic_VS;
        PixelShader = Blit_PS;
        RenderTarget0 = Render_Buffer_A;
        #if BUFFER_COLOR_BIT_DEPTH == 8
            SRGBWriteEnable = TRUE;
        #endif
    }

    pass PingPong1
    {
        VertexShader = Basic_VS;
        PixelShader = Horizontal_Blur_0_PS;
        RenderTarget0 = Render_Buffer_B;
        #if BUFFER_COLOR_BIT_DEPTH == 8
            SRGBWriteEnable = TRUE;
        #endif
    }

    pass PingPong2
    {
        VertexShader = Basic_VS;
        PixelShader = Vertical_Blur_0_PS;
        RenderTarget0 = Render_Buffer_A;
        #if BUFFER_COLOR_BIT_DEPTH == 8
            SRGBWriteEnable = TRUE;
        #endif
    }

    #if ENABLE_PINGPONG
        pass PingPong3
        {
            VertexShader = Basic_VS;
            PixelShader = Horizontal_Blur_1_PS;
            RenderTarget0 = Render_Buffer_B;
            #if BUFFER_COLOR_BIT_DEPTH == 8
                SRGBWriteEnable = TRUE;
            #endif
        }

        pass PingPong4
        {
            VertexShader = Basic_VS;
            PixelShader = Vertical_Blur_1_PS;
            RenderTarget0 = Render_Buffer_A;
            #if BUFFER_COLOR_BIT_DEPTH == 8
                SRGBWriteEnable = TRUE;
            #endif
        }
    #endif

    pass
    {
        VertexShader = Basic_VS;
        PixelShader = OutputPS;
        #if BUFFER_COLOR_BIT_DEPTH == 8
            SRGBWriteEnable = TRUE;
        #endif
    }
}
