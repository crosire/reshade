sampler2D g_sampler : register(s0);

struct PS_INPUT
{
    float4 position   : POSITION;       // vertex position
    float2 ftcoord    : TEXCOORD0;      // float 2 tex coord
    float2 fpos       : TEXCOORD1;      // float 2 position 
};

uniform float4x4 scissorMat : register(c0);
uniform float4 scissorExtScale : register(c4);
uniform float4x4 paintMat : register(c5);
uniform float4 innerCol : register(c9);
uniform float4 outerCol : register(c10);
uniform float4 extent : register(c11);
uniform float4 strokeMult : register(c12);
uniform float2 type : register(c13);

 
float sdroundrect(float2 pt, float2 ext, float rad) 
{
    float2 ext2 = ext - float2(rad,rad);
    float2 d = abs(pt) - ext2;
    return min(max(d.x,d.y),0.0) + length(max(d,0.0)) - rad;
}

// Scissoring
float scissorMask(float2 p) 
{
    float2 sc = (abs((mul((float3x3)scissorMat, float3(p.x, p.y, 1.0))).xy) - scissorExtScale.xy);
    sc = float2(0.5,0.5) - sc * scissorExtScale.zw;
    return clamp(sc.x,0.0,1.0) * clamp(sc.y,0.0,1.0);
}

float4 D3D9PixelShader_Main(PS_INPUT input) : COLOR
{
    float4 result;
    float scissor = scissorMask(input.fpos);
    float strokeAlpha = 1.0f;

    if (type.x == 0) 
    {
        // Calculate gradient color using box gradient
        float2 pt = (mul((float3x3)paintMat, float3(input.fpos,1.0))).xy;
        float d = clamp((sdroundrect(pt, extent.xy, extent.z) + extent.w*0.5) / extent.w, 0.0, 1.0);
        float4 color = lerp(innerCol, outerCol, d);
        
        // Combine alpha
        color *= strokeAlpha * scissor;
        result = color;
    }
    else if (type.x == 1)
    {
        // Calculate color fron texture
        float2 pt = (mul((float3x3)paintMat, float3(input.fpos,1.0))).xy / extent.xy;
        float4 color = tex2D(g_sampler, pt);
        if (type.y == 1) color = float4(color.xyz*color.w,color.w);
		if (type.y == 2) color = float4(color.x, color.x, color.x, color.x);
		
        // Apply color tint and alpha.
        color *= innerCol;
        // Combine alpha
        color *= strokeAlpha * scissor;
        result = color;
    }
    else if (type.x == 2)
    {
        // Stencil fill
        result = float4(1,1,1,1);
    } 
    else 
    {
        // Textured tris
        float4 color = tex2D(g_sampler, input.ftcoord);
        if (type.y == 1) color = float4(color.xyz*color.w,color.w);
		if (type.y == 2) color = float4(color.x, color.x, color.x, color.x);
		color *= scissor;
        result = (color * innerCol);
    }

	return result;
}

