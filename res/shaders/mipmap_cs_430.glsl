#version 430

layout(binding = 0) uniform sampler2D src;
layout(binding = 1) uniform writeonly image2D dest;

layout(location = 0) uniform vec3 texel;

layout(local_size_x = 8, local_size_y = 8) in;

void main()
{
	vec2 uv = texel.xy * (vec2(gl_GlobalInvocationID.xy) + vec2(0.5));
	imageStore(dest, ivec2(gl_GlobalInvocationID.xy), textureLod(src, uv, int(texel.z)));
}
