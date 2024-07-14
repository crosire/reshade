#version 430

layout(binding = 0) uniform sampler2D s0;

in vec4 frag_col;
in vec2 frag_tex;

out vec4 col;

void main()
{
	col = texture(s0, frag_tex);
	col *= frag_col; // Blend vertex color and texture
}
