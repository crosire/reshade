// Vertex shader generating a triangle covering the entire screen
void main(uint id : SV_VERTEXID, out float4 pos : SV_POSITION, out float2 uv : TEXCOORD0)
{
	uv.x = (id == 1) ? 2.0 : 0.0;
	uv.y = (id == 2) ? 2.0 : 0.0;
	pos = float4(uv * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
}
