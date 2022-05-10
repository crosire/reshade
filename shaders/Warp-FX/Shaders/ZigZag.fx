/*-----------------------------------------------------------------------------------------------------*/
/* ZigZag Shader - by Radegast Stravinsky of Ultros.                                               */
/* There are plenty of shaders that make your game look amazing. This isn't one of them.               */
/*-----------------------------------------------------------------------------------------------------*/
#include "ReShade.fxh"
#include "Include/ZigZag.fxh"

texture texColorBuffer : COLOR;

texture zzTarget
{
    Width = BUFFER_WIDTH;
    Height = BUFFER_HEIGHT;
    Format = RGBA8;
};


sampler samplerColor
{
    Texture = texColorBuffer;

    AddressU = MIRROR;
    AddressV = MIRROR;
    AddressW = MIRROR;

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

// Pixel Shaders (in order of appearance in the technique)
float4 ZigZag(float4 pos : SV_Position, float2 texcoord : TEXCOORD0) : SV_TARGET
{
    const float ar_raw = 1.0 * (float)BUFFER_HEIGHT / (float)BUFFER_WIDTH;
    const float depth = ReShade::GetLinearizedDepth(texcoord).r;
    float4 color;
    const float4 base = tex2D(samplerColor, texcoord);
    float ar = lerp(ar_raw, 1, aspect_ratio * 0.01);

    float2 center = coordinates / 2.0;
    if (use_mouse_point) 
        center = float2(mouse_coordinates.x * BUFFER_RCP_WIDTH / 2.0, mouse_coordinates.y * BUFFER_RCP_HEIGHT / 2.0);

    float2 offset_center = offset_coords / 2.0;

    float2 tc = texcoord - center;

    center.x /= ar;
    offset_center.x /= ar;
    tc.x /= ar;

    const float dist = distance(tc, center);
    const float tension_radius = lerp(radius-dist, radius, tension);
    const float percent = max(radius-dist, 0) / tension_radius;
    const float percentSquared = percent * percent;
    const float theta = percentSquared * (animate == 1 ? amplitude * sin(anim_rate * 0.0005) : amplitude) * sin(percentSquared / period * radians(angle) + (phase + (animate == 2 ? 0.00075 * anim_rate : 0)));
        
    if(!mode) {
        tc = mul(swirlTransform(theta), tc-center);
    } else {
        tc = mul(zigzagTransform(theta), tc-center);
    }

    if(use_offset_coords)
        tc += (2 * offset_center);
    else 
        tc += (2 * center);

    tc.x *= ar;

    float out_depth;
    bool inDepthBounds;
    if (depth_mode == 0) {
        out_depth =  ReShade::GetLinearizedDepth(texcoord).r;
        inDepthBounds = out_depth >= depth_threshold;
    }
    else{
        out_depth = ReShade::GetLinearizedDepth(tc).r;
        inDepthBounds = out_depth <= depth_threshold;
    }
       
    float blending_factor;
    if(render_type) 
        blending_factor = lerp(0, percentSquared, blending_amount);
    else
        blending_factor = blending_amount;
    if (inDepthBounds)
    {
        if(use_offset_coords){
            float2 offset_coords_adjust = offset_coords;
            offset_coords_adjust.x *= ar;
            if(dist <= tension_radius)
            {
                color = tex2D(samplerColor, tc);
                color.rgb = ComHeaders::Blending::Blend(render_type, base.rgb, color.rgb, blending_factor);
            }
            else
                color = tex2D(samplerColor, texcoord);
        } else
        {
            color = tex2D(samplerColor, tc);
            color.rgb = ComHeaders::Blending::Blend(render_type, base.rgb, color.rgb, blending_factor);
        }
        
        
    }
    else
    {
        color = base;
    }

    if(set_max_depth_behind) {
        const float mask_front = ReShade::GetLinearizedDepth(texcoord).r;
        if(mask_front < depth_threshold)
            color = tex2D(samplerColor, texcoord);
    }
    
    return color;
}

// Technique
technique ZigZag <ui_label="Zigzag";>
{
    pass p0
    {
        VertexShader = FullScreenVS;
        PixelShader = ZigZag;
    }

};