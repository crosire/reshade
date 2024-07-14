#version 430

layout(location = 0) in vec2 pos;
layout(location = 1) in vec2 tex;
layout(location = 2) in vec4 col;

out vec4 frag_col;
out vec2 frag_tex;

layout(binding = 0) uniform Buf { mat4 proj; };

void main()
{
	frag_col = col;
	frag_tex = tex;
	gl_Position = proj * vec4(pos.xy, 0, 1);
}
