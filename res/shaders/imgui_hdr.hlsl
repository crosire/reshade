#ifdef float3x3
	#define mul(a, b) (a) * (b)
	#define float1 float
#endif

#define COLOR_SPACE_SRGB 1
#define COLOR_SPACE_SCRGB 2
#define COLOR_SPACE_HDR10_PQ 3
#define COLOR_SPACE_HDR10_HLG 4

float3 bt709_to_bt2020(float3 col)
{
	const float3x3 bt709_to_bt2020 = float3x3(
		0.627403914928436279296875,      0.3292830288410186767578125,  0.0433130674064159393310546875,
		0.069097287952899932861328125,   0.9195404052734375,           0.011362315155565738677978515625,
		0.01639143936336040496826171875, 0.08801330626010894775390625, 0.895595252513885498046875);

	return mul(bt709_to_bt2020, col);
}

float3 pq_inverse_eotf(float3 col) // 1.0 = 10000 nits here
{
	// PQ constants as per Rec. ITU-R BT.2100-2 Table 4
	const float PQ_m1 = 0.1593017578125;
	const float PQ_m2 = 78.84375;
	const float PQ_c1 = 0.8359375;
	const float PQ_c2 = 18.8515625;
	const float PQ_c3 = 18.6875;

	float3 col_pow_m1 = pow(col, PQ_m1.xxx);

	return pow((PQ_c1 + PQ_c2 * col_pow_m1) / (1.f + PQ_c3 * col_pow_m1), PQ_m2.xxx);
}

float1 hlg_inverse_eotf(float1 col) // 1.0 = 1000 nits here
{
	// HLG constants as per Rec. ITU-R BT.2100-2 Table 5
	const float HLG_a = 0.17883277;
	const float HLG_b = 0.28466892; // 1 - 4 * HLG_a
	const float HLG_c = 0.559910714626312255859375; // 0.5 - HLG_a * ln(4 * HLG_a)

	if (col <= (1.0 / 12.0))
	{
		return sqrt(3.0 * col);
	}
	else
	{
		return HLG_a * log(12.0 * col - HLG_b) + HLG_c;
	}
}
float3 hlg_inverse_eotf(float3 col)
{
	return float3(hlg_inverse_eotf(col.r), hlg_inverse_eotf(col.g), hlg_inverse_eotf(col.b));
}

// BT.709/sRGB primaries + linear
float3 to_scrgb(float3 col) // 1.0 = 80 nits in scRGB
{
	col = pow(col, 2.2.xxx);
	col = col * hdr_overlay_brightness / 80.0;

	return col;
}

// BT.2020 primaries + PQ transfer function
float3 to_hdr10_pq(float3 col)
{
	col = pow(col, 2.2.xxx);
	col = bt709_to_bt2020(col);
	col = col * hdr_overlay_brightness / 10000.0;
	col = pq_inverse_eotf(col);

	return col;
}

// BT.2020 primaries + HLG transfer function
float3 to_hdr10_hlg(float3 col)
{
	col = pow(col, 2.2.xxx);
	col = bt709_to_bt2020(col);
	col = col * hdr_overlay_brightness /  1000.0;
	col = hlg_inverse_eotf(col);

	return col;
}
