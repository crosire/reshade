Texture2D t0 : register(t0);
SamplerState s0 : register(s0);

cbuffer PushConstants : register(b0)
{
	// Offset from the orthographic projection matrix used in the vertex shader
	uint color_space : packoffset(c4.x);
	float hdr_overlay_brightness : packoffset(c4.y);
};

#include "imgui_hdr.h"

void main(float4 vpos : SV_POSITION, float4 vcol : COLOR0, float2 uv : TEXCOORD0, out float4 col : SV_TARGET)
{
	if (color_space == COLOR_SPACE_HDR10)
	{
		vcol = to_pq(vcol);
	}
	else if (color_space == COLOR_SPACE_HLG)
	{
		vcol = to_hlg(vcol);
	}
	else if (color_space == COLOR_SPACE_SCRGB)
	{
		vcol = to_scrgb(vcol);
	}

	col = t0.Sample(s0, uv);
	col *= vcol; // Blend vertex color and texture
}
