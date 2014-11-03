uniform float2 viewSize : register(c0);

struct VS_OUTPUT
{
    float4 position   : POSITION;       // vertex position
    float2 ftcoord    : TEXCOORD0;      // float 2 tex coord
    float2 fpos       : TEXCOORD1;      // float 2 position 
};
  
VS_OUTPUT D3D9VertexShader_Main(float2 pt : POSITION, float2 tex : TEXCOORD0)
{
    VS_OUTPUT Output;
    Output.ftcoord = tex;
    Output.fpos = pt;
    Output.position = float4(2.0 * pt.x / viewSize.x - 1.0, 1.0 - 2.0 * pt.y / viewSize.y, 0, 1);
	Output.position.xy += float2(-1.0 / viewSize.x, 1.0 / viewSize.y) * Output.position.ww;
     
    return Output;
}

 