#version 430

layout(binding = 0) uniform sampler2D src;
layout(binding = 1) uniform writeonly image2D dest;

layout(location = 0) uniform int src_level;

layout(local_size_x = 8, local_size_y = 8) in;

vec4 reduce(ivec2 location)
{
	vec4 v0 = texelFetch(src, location + ivec2(0, 0), src_level);
	vec4 v1 = texelFetch(src, location + ivec2(0, 1), src_level);
	vec4 v2 = texelFetch(src, location + ivec2(1, 0), src_level);
	vec4 v3 = texelFetch(src, location + ivec2(1, 1), src_level);
	return (v0 + v1 + v2 + v3) * 0.25;
}

void main()
{
	vec4 v = reduce(ivec2(gl_GlobalInvocationID.xy * 2));
	imageStore(dest, ivec2(gl_GlobalInvocationID.xy), v);
}
