/*-----------------------------------------------------------------------------------------------------*/
/* Tiny Planet Shader v1.0 - by Radegast Stravinsky of Ultros.                                         */
/* There are plenty of shaders that make your game look amazing. This isn't one of them.               */
/*-----------------------------------------------------------------------------------------------------*/
#define PI 3.141592358

uniform float center_x <
    #if __RESHADE__ < 40000
        ui_type = "drag";
    #else
        ui_type = "slider";
    #endif
    ui_min = 0.0; ui_max = 100.0;
> = 0.5;

uniform float center_y <
    #if __RESHADE__ < 40000
        ui_type = "drag";
    #else 
        ui_type = "slider";
    #endif
    ui_min = 0.0; ui_max = 100.0;
> = 0.25;

uniform float offset_x <
    #if __RESHADE__ < 40000
        ui_type = "drag";
    #else
        ui_type = "slider";
    #endif
    ui_min = 0.0; ui_max = 1.0;
> = 0.5;

uniform float offset_y <
    #if __RESHADE__ < 40000
        ui_type = "drag";
    #else 
        ui_type = "slider";
    #endif
    ui_min = 0.0; ui_max = .5;
> = 0.25;

uniform float scale <
    #if __RESHADE__ < 40000
        ui_type = "drag";
    #else
        ui_type = "slider";
    #endif
    ui_min = 0.0; ui_max = 10.0;
> = 0.5;

uniform float z_rotation <
    #if __RESHADE__ < 40000
        ui_type = "drag";
    #else
        ui_type = "slider";
    #endif
    ui_min = 0.0; ui_max = 360.0;
> = 0.5;



texture texColorBuffer : COLOR;
texture texDepthBuffer : DEPTH;


texture TinyPlanetTarget
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
    Width = BUFFER_WIDTH;
    Height = BUFFER_HEIGHT;
};

sampler samplerTarget
{
    Texture = TinyPlanetTarget;
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

//TODO: Learn what this vertex shader really does.
void FullScreenVS(uint id : SV_VertexID, out float4 position : SV_Position, out float2 texcoord : TEXCOORD0)
{
    texcoord.x = (id == 2) ? 2.0 : 0.0;
    texcoord.y = (id == 1) ? 2.0 : 0.0;
    
    position = float4( texcoord * float2(2, -2) + float2(-1, 1), 0, 1);
    //position /= BUFFER_HEIGHT/BUFFER_WIDTH;

}


void TinyPlanetVS(uint id : SV_VertexId, out float4 position : SV_Position, out float2 texcoord : TEXCOORD0)
{
    texcoord.x = 0.25;
    texcoord.y = 0.25;

    position = float4( texcoord * float2(2, -2) + float2(-1,1), 0, 1);
}

void DoNothingPS(float4 pos : SV_Position, float2 texcoord : TEXCOORD0, out float4 color : SV_TARGET)
{
    color = tex2D(samplerColor, texcoord);
}


float3x3 getrot(float3 r)
{
    float cx = cos(radians(r.x));
    float sx = sin(radians(r.x));
    float cy = cos(radians(r.y));
    float sy = sin(radians(r.y));
    float cz = cos(radians(r.z));
    float sz = sin(radians(r.z));

    float m1,m2,m3,m4,m5,m6,m7,m8,m9;

    m1=cy*cz;
    m2=cx*sz+sx*sy*cz;
    m3=sx*sz-cx*sy*cz;
    m4=-cy*sz;
    m5=cx*cz-sx*sy*sz;
    m6=sx*cz+cx*sy*sz;
    m7=sy;
    m8=-sx*cy;
    m9=cx*cy;

    return float3x3
    (
        m1,m2,m3,
        m4,m5,m6,
        m7,m8,m9
    );
}


float4 TinyPlanet(float4 pos : SV_Position, float2 texcoord : TEXCOORD0) : SV_TARGET
{
    float ar = 1.0f * (float)BUFFER_HEIGHT/(float)BUFFER_WIDTH;
    
    float3 rot = float3(center_x,center_y, z_rotation);
    float3x3 t=getrot(rot);

    float2 rads = float2(PI*2.0 , PI);
    float2 offset=float2(offset_x,offset_y);
    float2 pnt = (texcoord - .5-offset).xy * float2(scale, scale*ar);

    // Project to Sphere
    float x2y2 = pnt.x * pnt.x + pnt.y * pnt.y;
    float3 sphere_pnt = float3(2. * pnt, x2y2 - 1.0) / (x2y2 + 1.0);
    
    sphere_pnt = mul(sphere_pnt, t);

    // Convert to Spherical Coordinates
    float r = length(sphere_pnt);
    float lon = atan2(sphere_pnt.y, sphere_pnt.x);
    float lat = acos(sphere_pnt.z / r);

    return tex2D(samplerTarget, float2(lon, lat)/rads);
}


technique TinyPlanet
{
    pass p0
    {
       
        VertexShader = FullScreenVS;
        PixelShader = DoNothingPS;

        RenderTarget = TinyPlanetTarget;
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
        PixelShader = TinyPlanet;
    }
};