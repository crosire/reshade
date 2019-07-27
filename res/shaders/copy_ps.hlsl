Texture2D t0 : register(t0);
SamplerState s0 : register(s0);

void main(float4 vpos : SV_POSITION, float2 uv : TEXCOORD0, out float4 col : SV_TARGET)
{
	col = t0.Sample(s0, uv);
	col.a = 1.0; // Clear alpha channel
}
