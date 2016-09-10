Texture2D texture0 : register(t0);
SamplerState sampler0 : register(s0);

float4 main(float4 vpos : SV_POSITION, float2 uv : TEXCOORD) : SV_TARGET
{
	return float4(texture0.Sample(sampler0, uv).rgb, 1.0);
}
