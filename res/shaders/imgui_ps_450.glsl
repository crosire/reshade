#version 450 core
#extension GL_ARB_shading_language_include : require

layout(set = 0, binding = 0) uniform sampler2D s0;

layout(location = 0) in struct { vec4 col; vec2 tex; } i;
vec4 vcol = i.col;
layout(location = 0) out vec4 col;

layout(push_constant) uniform PushConstants
{
	// Offset from the orthographic projection matrix used in the vertex shader
	layout(offset = 64) uint color_space;
	layout(offset = 68) float hdr_overlay_brightness;
};

#include "imgui_hdr.hlsl"

void main()
{
	if (color_space == COLOR_SPACE_HDR10)
	{
		vcol.rgb = to_pq(vcol.rgb);
	}
	else if (color_space == COLOR_SPACE_HLG)
	{
		vcol.rgb = to_hlg(vcol.rgb);
	}
	else if (color_space == COLOR_SPACE_SCRGB)
	{
		vcol.rgb = to_scrgb(vcol.rgb);
	}

	col = texture(s0, i.tex);
	col *= vcol; // Blend vertex color and texture
}
