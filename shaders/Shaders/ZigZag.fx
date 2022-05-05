/*-----------------------------------------------------------------------------------------------------*/
/* ZigZag Shader v2.0 - by Radegast Stravinsky of Ultros.                                               */
/* There are plenty of shaders that make your game look amazing. This isn't one of them.               */
/*-----------------------------------------------------------------------------------------------------*/
uniform float radius <
    #if __RESHADE__ < 40000
        ui_type = "drag";
    #else
        ui_type = "slider";
    #endif
    ui_min = 0.0; ui_max = 1.0;
> = 0.0;

uniform float angle <
    #if __RESHADE__ < 40000
        ui_type = "drag";
    #else
        ui_type = "slider";
    #endif
    ui_min = -999.0; ui_max = 999.0; ui_step = 1.0;
> = 180.0;

uniform float period <
    #if __RESHADE__ < 40000
        ui_type = "drag";
    #else
        ui_type = "slider";
    #endif
    ui_min = 0.1; ui_max = 10.0;
> = 0.5;

uniform float amplitude <
    #if __RESHADE__ < 40000
        ui_type = "drag";
    #else
        ui_type = "slider";
    #endif
    ui_min = -10.0; ui_max = 10.0;
> = 0.0;

uniform float center_x <
    #if __RESHADE__ < 40000
        ui_type = "drag";
    #else
        ui_type = "slider";
    #endif
    ui_min = 0.0; ui_max = 1.0;
> = 0.5;

uniform float center_y <
    #if __RESHADE__ < 40000
        ui_type = "drag";
    #else 
        ui_type = "slider";
    #endif
    ui_min = 0.0; ui_max = 1.0;
> = 0.5;

uniform float phase <
    #if __RESHADE__ < 40000
        ui_type = "drag";
    #else
        ui_type = "slider";
    #endif
    ui_min = -5.0; ui_max = 5.0;
> = 0.0;

uniform int animate <
    ui_type = "combo";
    ui_label = "Animate";
    ui_items = "No\0Amplitude\0Phase\0";
    ui_tooltip = "Enable or disable the animation. Animates the zigzag effect by phase or by amplitude.";
> = 0;

uniform float anim_rate <
    source = "timer";
>;

texture texColorBuffer : COLOR;
texture texDepthBuffer : DEPTH;

texture zzTarget
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

sampler samplerDepth
{
    Texture = texDepthBuffer;
};

sampler samplerTarget
{
    Texture = zzTarget;
};

//TODO: Learn what this vertex shader really does.
void FullScreenVS(uint id : SV_VertexID, out float4 position : SV_Position, out float2 texcoord : TEXCOORD0)
{
    texcoord.x = (id == 2) ? 2.0 : 0.0;
    texcoord.y = (id == 1) ? 2.0 : 0.0;
    
    position = float4( texcoord * float2(2, -2) + float2(-1, 1), 0, 1);
    //position /= BUFFER_HEIGHT/BUFFER_WIDTH;

}

void ZigZagVS(uint id : SV_VertexId, out float4 position : SV_Position, out float2 texcoord : TEXCOORD0)
{
    texcoord.x = radius;
    texcoord.y = radius;

    position = float4( texcoord * float2(2, -2) + float2(-1,1), 0, 1);
}

void DoNothingPS(float4 pos : SV_Position, float2 texcoord : TEXCOORD0, out float4 color : SV_TARGET)
{
    color = tex2D(samplerColor, texcoord);
}

float4 ZigZag(float4 pos : SV_Position, float2 texcoord : TEXCOORD0) : SV_TARGET
{
    float ar = BUFFER_WIDTH > BUFFER_HEIGHT ? (float)BUFFER_WIDTH/(float)BUFFER_HEIGHT : (float)BUFFER_HEIGHT/BUFFER_WIDTH;
    float2 center = float2(center_x, center_y);

    if(BUFFER_WIDTH > BUFFER_HEIGHT)
        texcoord.x *= ar;
    else
        texcoord.y *= ar;
    float2 tc = texcoord - center;
    

    float dist = distance(tc, center);
    if (dist < radius)
    {
        float percent = (radius-dist)/radius;
        
        //float theta = ar * sin(percent * radians(angle) + phase) * amplitude;
        float theta = percent * percent * (animate == 1 ? amplitude * sin(anim_rate * 0.0005) : amplitude) * sin(percent * percent / period * radians(angle) + (phase + (animate == 2 ? 0.00075 * anim_rate : 0)));

        float s =  sin(theta);
        //float c =   cos(theta) * cos(theta);
        float c = cos(theta);
        tc = float2(dot(tc-center, float2(c,-s)), dot(tc-center, float2(s,c)));

        tc += (2.0*center);

        if(BUFFER_WIDTH > BUFFER_HEIGHT)
            tc.x /= ar;
        else
            tc.y /= ar;
        
        return tex2D(samplerTarget, tc);
    }
    else
    {
        if(BUFFER_WIDTH > BUFFER_HEIGHT)
            texcoord.x /= ar;
        else
            texcoord.y /= ar;
        return tex2D(samplerTarget, texcoord);
    }
        
}

technique ZigZag
{
    pass p0
    {
       
        VertexShader = FullScreenVS;
        PixelShader = DoNothingPS;

        RenderTarget = zzTarget;
        ClearRenderTargets = true;

        RenderTargetWriteMask = 0xF;

        SRGBWriteEnable = false;

        BlendEnable = false;

        BlendOp = ADD;
        BlendOpAlpha = ADD;

        SrcBlend = ONE;
        //SrcBlendAlpha = ONE;
        DestBlend = ZERO;

        StencilEnable = false;

        StencilReadMask = 0xFF; // or StencilMask
        StencilWriteMask = 0xFF;

        StencilFunc = ALWAYS;

        StencilRef = 0;

        StencilPassOp = KEEP; 
        StencilFailOp = KEEP; 
        StencilDepthFailOp = KEEP; 

    }

    pass p1
    {
        VertexShader = FullScreenVS;
        PixelShader = ZigZag;
    }
};