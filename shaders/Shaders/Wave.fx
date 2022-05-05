/*-----------------------------------------------------------------------------------------------------*/
/* Wave Shader - by Radegast Stravinsky of Ultros.                                                */
/* There are plenty of shaders that make your game look amazing. This isn't one of them.               */
/*-----------------------------------------------------------------------------------------------------*/
#include "Include/Wave.fxh"
#include "ReShade.fxh"

texture texColorBuffer : COLOR;

texture waveTarget
{
    Width = BUFFER_WIDTH;
    Height = BUFFER_HEIGHT;
    MipLevels = LINEAR;
    Format = RGBA8;
};

sampler samplerColor
{
    Texture = texColorBuffer;

    AddressU = WRAP;
    AddressV = WRAP;
    AddressW = WRAP;

    MagFilter = LINEAR;
    MinFilter = LINEAR;
    MipFilter = LINEAR;

    MinLOD = 0.0f;
    MaxLOD = 1000.0f;

    MipLODBias = 0.0f;

    SRGBTexture = false;
};

// Vertex Shader
void FullScreenVS(uint id : SV_VertexID, out float4 position : SV_Position, out float2 texcoord : TEXCOORD0)
{
    if (id == 2)
        texcoord.x = 2.0;
    else
        texcoord.x = 0.0;

    if (id == 1)
        texcoord.y  = 2.0;
    else
        texcoord.y = 0.0;

    position = float4( texcoord * float2(2, -2) + float2(-1, 1), 0, 1);

}

float4 Wave(float4 pos : SV_Position, float2 texcoord : TEXCOORD0) : SV_TARGET 
{
    const float ar = 1.0 * (float)BUFFER_HEIGHT / (float)BUFFER_WIDTH;
    const float2 center = float2(0.5 / ar, 0.5);
    const float depth = ReShade::GetLinearizedDepth(texcoord).r;
    float2 tc = texcoord;
    const float4 base = tex2D(samplerColor, texcoord);
    float4 color;

    tc.x /= ar;

    const float theta = radians(animate == 3 ? (anim_rate * 0.01 % 360.0) : angle);
    const float s =  sin(theta);
    const float _s = sin(-theta);
    const float c =  cos(theta);
    const float _c = cos(-theta);

    tc = float2(dot(tc - center, float2(c, -s)), dot(tc - center, float2(s, c)));
    if(depth >= min_depth){
        if(wave_type == 0)
        {
            switch(animate)
            {
                default:
                    tc.x += amplitude * sin((tc.x * period * 10) + phase);
                    break;
                case 1:
                    tc.x += (sin(anim_rate * 0.001) * amplitude) * sin((tc.x * period * 10) + phase);
                    break;
                case 2:
                    tc.x += amplitude * sin((tc.x * period * 10) + (anim_rate * 0.001));
                    break;
            }
        }
        else
        {
            switch(animate)
            {
                default:
                    tc.x +=  amplitude * sin((tc.y * period * 10) + phase);
                    break;
                case 1:
                    tc.x += (sin(anim_rate * 0.001) * amplitude) * sin((tc.y * period * 10) + phase);
                    break;
                case 2:
                    tc.x += amplitude * sin((tc.y * period * 10) + (anim_rate * 0.001));
                    break;
            }
        }
        tc = float2(dot(tc, float2(_c, -_s)), dot(tc, float2(_s, _c))) + center;

        tc.x *= ar;

        color = tex2D(samplerColor, tc);
    }
    else
    {
        color = tex2D(samplerColor, texcoord);
    }
    if(depth >= min_depth)
        color = applyBlendingMode(base, color);

    return color;
}

technique Wave
{
    pass p0
    {
        VertexShader = FullScreenVS;
        PixelShader = Wave;
    }
}