uniform int LUMA_MODE <
	ui_label = "Luma Mode";
	ui_type = "combo";
	ui_items = "Intensity\0Value\0Lightness\0Luma\0";
	> = 3;
//===================================================================================================================
float4 LumaChroma(float4 col) {
	if (LUMA_MODE == 0) { 			// Intensity
		float i = dot(col.rgb, 0.3333);
		return float4(col.rgb / i, i);
	} else if (LUMA_MODE == 1) {		// Value
		float v = max(max(col.r, col.g), col.b);
		return float4(col.rgb / v, v);
	} else if (LUMA_MODE == 2) { 		// Lightness
		float high = max(max(col.r, col.g), col.b);
		float low = min(min(col.r, col.g), col.b);
		float l = (high + low) / 2;
		return float4(col.rgb / l, l);
	} else { 				// Luma
		float luma = dot(col.rgb, float3(0.21, 0.72, 0.07));
		return float4(col.rgb / luma, luma);
	}
}