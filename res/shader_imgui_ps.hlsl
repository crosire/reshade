Texture2D texture0 : register(t0);
SamplerState sampler0 : register(s0);

float4 main(float4 vpos : SV_POSITION, float4 col : COLOR0, float2 uv : TEXCOORD0) : SV_TARGET
{
	return col * texture0.Sample(sampler0, uv);
}
