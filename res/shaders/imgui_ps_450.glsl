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

#include "imgui_hdr.h"

void main()
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

	col = texture(s0, i.tex);
	col *= vcol; // Blend vertex color and texture
}
