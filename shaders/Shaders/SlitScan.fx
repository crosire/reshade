/*-----------------------------------------------------------------------------------------------------*/
/* Slit Scan Shader - by Radegast Stravinsky of Ultros.                                               */
/* There are plenty of shaders that make your game look amazing. This isn't one of them.               */
/*-----------------------------------------------------------------------------------------------------*/
#include "ReShade.fxh";
#include "Include/SlitScan.fxh";

texture texColorBuffer: COLOR;

texture ssTexture {
    Height = BUFFER_HEIGHT;
    Width = BUFFER_WIDTH;
};

sampler samplerColor {
    Texture = texColorBuffer;

    Width = BUFFER_WIDTH;
    Height = BUFFER_HEIGHT;
    Format = RGBA16;
    
};

sampler ssTarget {
    Texture = ssTexture;
        
    AddressU = WRAP;
    AddressV = WRAP;
    AddressW = WRAP;

    Height = BUFFER_HEIGHT;
    Width = BUFFER_WIDTH;
    Format = RGBA16;
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
};

void SlitScan(float4 pos : SV_Position, float2 texcoord : TEXCOORD0, out float4 color : SV_TARGET)
{
    float4 col_pixels =  tex2D(samplerColor, float2(x_col, texcoord.y));
    const float pix_w =  scan_speed * BUFFER_RCP_WIDTH;

    float slice_to_fill = (anim_rate * pix_w) % 1;

    float4 cols = tex2Dfetch(ssTarget, texcoord);
    float4 col_to_write = tex2Dfetch(ssTarget, texcoord);
    if(texcoord.x >= slice_to_fill - pix_w && texcoord.x <= slice_to_fill + pix_w)
        col_to_write.rgba = col_pixels.rgba;
    else
        discard;
    color = col_to_write;
};

void SlitScanPost(float4 pos : SV_Position, float2 texcoord : TEXCOORD0, out float4 color : SV_TARGET)
{
    const float depth = ReShade::GetLinearizedDepth(texcoord).r;
    float2 uv = texcoord;
    float4 screen = tex2D(samplerColor, texcoord);
    float pix_w =  scan_speed * BUFFER_RCP_WIDTH;
    uv.x +=  (anim_rate * pix_w ) - x_col % 1 ;
    float4 scanned = tex2D(ssTarget, uv);


    float4 mask = step(texcoord.x, x_col);

    if(depth >= min_depth)
        color = lerp(screen, scanned, mask);
    else
        color = screen;
}



technique SlitScan <
ui_label="Slit Scan";
> {
    pass p0 {

        VertexShader = FullScreenVS;
        PixelShader = SlitScan;
        
        RenderTarget = ssTexture;
    }

    pass p1 {

        VertexShader = FullScreenVS;
        PixelShader = SlitScanPost;

    }
}