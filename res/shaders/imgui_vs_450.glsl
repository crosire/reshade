#version 450 core

layout(location = 0) in vec2 pos;
layout(location = 1) in vec2 tex;
layout(location = 2) in vec4 col;
layout(location = 0) out struct { vec4 col; vec2 tex; } o;

layout(push_constant) uniform PushConstants {
	mat4 ProjectionMatrix;
} pc;

void main()
{
	o.col = col;
	o.tex = tex;
	gl_Position = pc.ProjectionMatrix * vec4(pos, 0, 1);
}
