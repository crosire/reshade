#version 450 core

layout(set = 0, binding = 0) uniform sampler2D s0;

layout(location = 0) in struct { vec4 col; vec2 tex; } i;
layout(location = 0) out vec4 col;

// offset from the orthographic projection matrix used in the vertex shader
layout(push_constant) uniform FragmentPushConstants {
	layout(offset = 64) uint  back_buffer_format;
	layout(offset = 68) uint  back_buffer_color_space;
	layout(offset = 72) float hdr_overlay_brightness;
	layout(offset = 76) int   overwrite_overlay_color_space_to;
} fpc;

// values mirror "api::format"
#define RGB10A2_UNORM 24
#define BGR10A2_UNORM 1111970609
#define RGBA16_FLOAT  10

// values mirror "api::color_space"
#define CSP_UNKNOWN 0
#define CSP_SRGB    1
#define CSP_SCRGB   2
#define CSP_HDR10   3
#define CSP_HLG     4


// PQ constants as per Rec. ITU-R BT.2100-2 Table 4
const vec3 PQ_m1 = vec3( 0.1593017578125);
const vec3 PQ_m2 = vec3(78.84375);
const vec3 PQ_c1 = vec3( 0.8359375);
const vec3 PQ_c2 = vec3(18.8515625);
const vec3 PQ_c3 = vec3(18.6875);

// HLG constants as per Rec. ITU-R BT.2100-2 Table 5
const float HLG_a = 0.17883277;
const float HLG_b = 0.28466892; // 1 - 4 * HLG_a
const float HLG_c = 0.559910714626312255859375; // 0.5 - HLG_a * ln(4 * HLG_a)


const mat3 bt709_to_bt2020 = mat3(
	0.627403914928436279296875,      0.3292830288410186767578125,  0.0433130674064159393310546875,
	0.069097287952899932861328125,   0.9195404052734375,           0.011362315155565738677978515625,
	0.01639143936336040496826171875, 0.08801330626010894775390625, 0.895595252513885498046875);

// PQ inverse EOTF as per Rec. ITU-R BT.2100-2 Table 4 (end)
// for input 1.0 = 10000 nits here
vec3 pq_inverse_eotf(vec3 col)
{
	vec3 col_pow_m1 = pow(col, PQ_m1);

	return pow((PQ_c1 + PQ_c2 * col_pow_m1) / (vec3(1.0) + PQ_c3 * col_pow_m1), PQ_m2);
}

// HLG inverse EOTF as per Rec. ITU-R BT.2100-2 Table 5
// for input 1.0 = 1000 nits here
float hlg_inverse_eotf(float col)
{
	if (col <= (1.0 / 12.0))
	{
		return sqrt(3.0 * col);
	}
	else
	{
		return HLG_a * log(12.0 * col - HLG_b) + HLG_c;
	}
}

vec3 hlg_inverse_eotf(vec3 col)
{
	return vec3(hlg_inverse_eotf(col.r), hlg_inverse_eotf(col.g), hlg_inverse_eotf(col.b));
}

// HDR10 (PQ transfer function + BT.2020 primaries)
vec4 to_pq(vec4 col)
{
	return vec4(pq_inverse_eotf(bt709_to_bt2020 * pow(col.rgb, vec3(2.2)) * vec3(fpc.hdr_overlay_brightness / 10000.0)), col.a);
}

// scRGB (linear + BT.709/sRGB primaries)
// 1.0 = 80 nits in scRGB
vec4 to_scrgb(vec4 col)
{
	return vec4(pow(col.rgb, vec3(2.2)) * vec3(fpc.hdr_overlay_brightness / 80.0), col.a);
}

// HLG (HLG transfer function + BT.2020 primaries)
vec4 to_hlg(vec4 col)
{
	return vec4(pq_inverse_eotf(bt709_to_bt2020 * pow(col.rgb, vec3(2.2)) * vec3(fpc.hdr_overlay_brightness / 1000.0)), col.a);
}

void main()
{
	col = texture(s0, i.tex);

	// Blend vertex color and texture depending on color space
	if (fpc.overwrite_overlay_color_space_to == CSP_HDR10)
	{
		col *= to_pq(i.col);
	}
	else if (fpc.overwrite_overlay_color_space_to == CSP_SCRGB)
	{
		col *= to_scrgb(i.col);
	}
	else if (fpc.overwrite_overlay_color_space_to == CSP_HLG)
	{
		col *= to_hlg(i.col);
	}
	else if (fpc.overwrite_overlay_color_space_to == CSP_SRGB)
	{
		col *= i.col;
	}
	// rgb10a2_unorm or bgr10a2_unorm alone don't guarantee a hdr swapchain
	else if ((fpc.back_buffer_format == RGB10A2_UNORM || fpc.back_buffer_format == BGR10A2_UNORM)
		  && fpc.back_buffer_color_space == CSP_HDR10)
	{
		col *= to_pq(i.col);
	}
	// Workaround for early HDR games, rgba16f without a color space defined
	// is pretty much guaranteed to be HDR for games
	else if (fpc.back_buffer_format == RGBA16_FLOAT || fpc.back_buffer_color_space == CSP_SCRGB)
	{
		col *= to_scrgb(i.col);
	}
	else
	{
		col *= i.col;
	}
}
