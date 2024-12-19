struct VS_INPUT
{
	float2 pos : POSITION;
	float4 col : COLOR0;
	float2 tex : TEXCOORD0;
};
struct PS_INPUT
{
	float4 pos : SV_POSITION;
	float4 col : COLOR0;
	float2 tex : TEXCOORD0;
};

cbuffer PushConstants : register(b0)
{
	float4x4 ortho_projection;
};

void main(VS_INPUT input, out PS_INPUT output)
{
	output.pos = mul(ortho_projection, float4(input.pos.xy, 0.0, 1.0));
	output.col = input.col;
	output.tex = input.tex;
}
