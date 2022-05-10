/*-----------------------------------------------------------------------------------------------------*/
/* Tiny Planet Shader - by Radegast Stravinsky of Ultros.                                         */
/* There are plenty of shaders that make your game look amazing. This isn't one of them.               */
/*-----------------------------------------------------------------------------------------------------*/

#include "Include/TinyPlanet.fxh"

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


// Vertex Shaders
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

// Pixel Shaders (in order of appearance in the technique)
float4 PreTP(float4 pos : SV_Position, float2 texcoord : TEXCOORD0) : SV_TARGET
{
    const float inv_seam = 1 - seam_scale;
    float4 tc1 =  tex2D(samplerColor, texcoord + float2(inv_seam, 0.0));
    float4 tc = tex2D(samplerColor, texcoord * float2(seam_scale, 1.0));
    
    if(texcoord.x < inv_seam){ 
        tc.rgb = lerp(tc1.rgb, tc.rgb, 1- clamp((inv_seam-texcoord.x) * 10., 0, 1));
    }
    if(texcoord.x > seam_scale) tc.rgb = lerp(tc.rgb, tc1.rgb, clamp((texcoord.x-seam_scale) * 10., 0, 1));
    return tc;
}

float4 TinyPlanet(float4 pos : SV_Position, float2 texcoord : TEXCOORD0) : SV_TARGET
{
    const float ar = 1.0 * (float)BUFFER_HEIGHT / (float)BUFFER_WIDTH;
    
    const float3x3 rot = getrot(float3(center_x,center_y, z_rotation));

    const float2 rads = float2(PI * 2.0 , PI);
    const float2 pnt = (texcoord - 0.5 - offset).xy * float2(scale, scale*ar);

    // Project to Sphere
    const float x2y2 = pnt.x * pnt.x + pnt.y * pnt.y;
    float3 sphere_pnt = float3(2.0 * pnt, x2y2 - 1.0) / (x2y2 + 1.0);
    
    sphere_pnt = mul(sphere_pnt, rot);

    // Convert to Spherical Coordinates
    const float r = length(sphere_pnt);
    const float lon = atan2(sphere_pnt.y, sphere_pnt.x);
    const float lat = acos(sphere_pnt.z / r);

    return tex2D(samplerColor, float2(lon, lat) / rads);
}

// Technique
technique TinyPlanet <ui_label="Tiny Planet";>
{
     pass p0
    {
        VertexShader = FullScreenVS;
        PixelShader = PreTP;
    }
    pass p1
    {
        VertexShader = FullScreenVS;
        PixelShader = TinyPlanet;
    }
};