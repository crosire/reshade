#version 450 core

layout(set = 0, binding = 0) uniform sampler2D s0;

layout(location = 0) in struct { vec4 col; vec2 tex; } i;
layout(location = 0) out vec4 col;

void main()
{
	col = texture(s0, i.tex);
	col *= i.col; // Blend vertex color and texture
}
