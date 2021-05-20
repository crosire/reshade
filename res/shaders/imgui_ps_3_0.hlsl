sampler2D s0 : register(s0);

void main(float4 vpos : VPOS, float4 vcol : COLOR0, float2 uv : TEXCOORD0, out float4 col : COLOR)
{
	col = tex2D(s0, uv);
	col *= vcol; // Blend vertex color and texture
}
