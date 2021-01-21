Texture2D t0 : register(t0);
SamplerState s0 : register(s0);

void main(float4 vpos : SV_POSITION, float4 vcol : COLOR0, float2 uv : TEXCOORD0, out float4 col : SV_TARGET)
{
	col = t0.Sample(s0, uv);
	col *= vcol; // Blend vertex color and texture
}
