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

float get_pix_w() {
    float output;
    switch(direction) {
        case 0:
        case 1:
            output = scan_speed * BUFFER_RCP_WIDTH;
            break;
        case 2:
        case 3:
            output = scan_speed * BUFFER_RCP_HEIGHT;
            break;
    }
    return output;
}

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

// Pixel Shaders
void SlitScan(float4 pos : SV_Position, float2 texcoord : TEXCOORD0, out float4 color : SV_TARGET)
{
    float4 col_pixels;
    float scan_col = animate ? anim_rate.x : x_col;

    switch(direction) {
        case 0:
        case 1:
            col_pixels =  tex2D(samplerColor, float2(scan_col, texcoord.y));
            break;
        case 2:
        case 3:
            col_pixels =  tex2D(samplerColor, float2(texcoord.x, scan_col));
            break;

    } 
    float pix_w = get_pix_w();
    float slice_to_fill;
    switch(direction){
        case 0:
        case 2:
            slice_to_fill = (frame_rate * pix_w) % 1;
            break;
        case 1:
        case 3:
            slice_to_fill = abs(1-((frame_rate * pix_w) % 1));
            break;
    } 

    float4 cols = tex2Dfetch(ssTarget, texcoord);
    float4 col_to_write = tex2Dfetch(ssTarget, texcoord);
    switch(direction) {
        case 0:
        case 1:
            if(texcoord.x >= slice_to_fill - pix_w && texcoord.x <= slice_to_fill + pix_w)
                col_to_write.rgba = col_pixels.rgba;
            else
                discard;
            break;
        case 2:
        case 3:
            if(texcoord.y >= slice_to_fill - pix_w && texcoord.y <= slice_to_fill + pix_w){
                
                    col_to_write.rgba = col_pixels.rgba;
               
            }
            else
                discard;
            break;
    }
    
    color = col_to_write;
};

void SlitScanPost(float4 pos : SV_Position, float2 texcoord : TEXCOORD0, out float4 color : SV_TARGET)
{
    const float depth = ReShade::GetLinearizedDepth(texcoord).r;
    float2 uv = texcoord;
    float4 screen = tex2D(samplerColor, texcoord);
    float pix_w = get_pix_w();
    float scan_col;
    
    if(animate) scan_col = anim_rate.x;
    else scan_col = x_col;

    switch(direction) {
        case 0:
            uv.x +=  (frame_rate * pix_w) - scan_col % 1 ;
            break;
        case 1:
            uv.x -= (frame_rate * pix_w) - (1 - scan_col) % 1 ;
            break;
        case 2:
            uv.y +=  (frame_rate * pix_w) - scan_col % 1 ;
            break;
        case 3:
            uv.y -=  (frame_rate * pix_w) - (1 - scan_col) % 1 ;
            break;
    }
    
    float4 scanned = tex2D(ssTarget, uv);


    float4 mask;
    switch(direction) {
        case 0:
            mask = step(texcoord.x, scan_col);
            break;
        case 1:
            mask = step(scan_col, texcoord.x);
            break;
        case 2:
            mask = step(texcoord.y, scan_col);
            break;
        case 3:
            mask = step(scan_col, texcoord.y);
            break;
    }
    
    bool inDepthBounds;
    if(depth_mode == 0)
        inDepthBounds = depth >= depth_threshold;
    else 
        inDepthBounds = depth <= depth_threshold;

    if(inDepthBounds)
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