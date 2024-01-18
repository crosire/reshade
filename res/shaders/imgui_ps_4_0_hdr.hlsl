Texture2D t0 : register(t0);
SamplerState s0 : register(s0);

// offset from the orthographic projection matrix used in the vertex shader
cbuffer PSPushConstants : register(b0)
{
	uint  back_buffer_format               : packoffset(c4.x);
	uint  back_buffer_color_space          : packoffset(c4.y);
	float hdr_overlay_brightness           : packoffset(c4.z);
	int   overwrite_overlay_color_space_to : packoffset(c4.w);
};

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
static const float PQ_m1 =  0.1593017578125f;
static const float PQ_m2 = 78.84375f;
static const float PQ_c1 =  0.8359375f;
static const float PQ_c2 = 18.8515625f;
static const float PQ_c3 = 18.6875f;

// HLG constants as per Rec. ITU-R BT.2100-2 Table 5
static const float HLG_a = 0.17883277f;
static const float HLG_b = 0.28466892f; // 1 - 4 * HLG_a
static const float HLG_c = 0.559910714626312255859375f; // 0.5 - HLG_a * ln(4 * HLG_a)


static const float3x3 bt709_to_bt2020 = {
	0.627403914928436279296875f,      0.3292830288410186767578125f,  0.0433130674064159393310546875f,
	0.069097287952899932861328125f,   0.9195404052734375f,           0.011362315155565738677978515625f,
	0.01639143936336040496826171875f, 0.08801330626010894775390625f, 0.895595252513885498046875f };

// PQ inverse EOTF as per Rec. ITU-R BT.2100-2 Table 4 (end)
// for input 1.0 = 10000 nits here
float3 pq_inverse_eotf(float3 col)
{
	float3 col_pow_m1 = pow(col, PQ_m1);

	return pow((PQ_c1 + PQ_c2 * col_pow_m1) / (1.f + PQ_c3 * col_pow_m1), PQ_m2);
}

// HLG inverse EOTF as per Rec. ITU-R BT.2100-2 Table 5
// for input 1.0 = 1000 nits here
float hlg_inverse_eotf(float col)
{
	if (col <= (1.f / 12.f))
	{
		return sqrt(3.f * col);
	}
	else
	{
		return HLG_a * log(12.f * col - HLG_b) + HLG_c;
	}
}

float3 hlg_inverse_eotf(float3 col)
{
	return float3(hlg_inverse_eotf(col.r), hlg_inverse_eotf(col.g), hlg_inverse_eotf(col.b));
}

// HDR10 (PQ transfer function + BT.2020 primaries)
float4 to_pq(float4 col)
{
	return float4(pq_inverse_eotf(mul(bt709_to_bt2020, pow(col.rgb, 2.2f)) * hdr_overlay_brightness / 10000.f), col.a);
}

// scRGB (linear + BT.709/sRGB primaries)
// 1.0 = 80 nits in scRGB
float4 to_scrgb(float4 col)
{
	return float4(pow(col.rgb, 2.2f) * hdr_overlay_brightness / 80.f, col.a);
}

// HLG (HLG transfer function + BT.2020 primaries)
float4 to_hlg(float4 col)
{
	return float4(hlg_inverse_eotf(mul(bt709_to_bt2020, pow(col.rgb, 2.2f)) * hdr_overlay_brightness / 1000.f), col.a);
}


void main(float4 vpos : SV_POSITION, float4 vcol : COLOR0, float2 uv : TEXCOORD0, out float4 col : SV_TARGET)
{
	col = t0.Sample(s0, uv);

	// Blend vertex color and texture depending on color space
	if (overwrite_overlay_color_space_to == CSP_HDR10)
	{
		col *= to_pq(vcol);
	}
	else if (overwrite_overlay_color_space_to == CSP_SCRGB)
	{
		col *= to_scrgb(vcol);
	}
	else if (overwrite_overlay_color_space_to == CSP_HLG)
	{
		col *= to_hlg(vcol);
	}
	else if (overwrite_overlay_color_space_to == CSP_SRGB)
	{
		col *= vcol;
	}
	// rgb10a2_unorm or bgr10a2_unorm alone don't guarantee a hdr swapchain
	else if ((back_buffer_format == RGB10A2_UNORM || back_buffer_format == BGR10A2_UNORM)
		  && back_buffer_color_space == CSP_HDR10)
	{
		col *= to_pq(vcol);
	}
	// Workaround for early HDR games, rgba16f without a color space defined
	// is pretty much guaranteed to be HDR for games
	else if (back_buffer_format == RGBA16_FLOAT || back_buffer_color_space == CSP_SCRGB)
	{
		col *= to_scrgb(vcol);
	}
	else
	{
		col *= vcol;
	}
}
