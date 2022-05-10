#pragma once

namespace FXShaders
{

/**
 * Standard screen blend mode.
 *
 * @param a The original color, normalized.
 * @param b The color to blend with the original color, normalized.
 * @param w How much to blend with the original color.
 *          If set to 0.0 the result will be the original color left intact.
 */
float3 BlendScreen(float3 a, float3 b, float w)
{
	return 1.0 - (1.0 - a) * (1.0 - b * w);
}

/**
 * Standard overlay blend mode.
 *
 * @param a The original color, normalized.
 * @param b The color to blend with the original color, normalized.
 * @param w How much to blend with the original color.
 *          If set to 0.0 the result will be the original color left intact.
 */
float3 BlendOverlay(float3 a, float3 b, float w)
{
	float3 color = (a < 0.5)
		? 2.0 * a * b
		: 1.0 - 2.0 * (1.0 - a) * (1.0 - b);

	return lerp(a, color, w);
}

/**
 * Standard soft light blend mode.
 *
 * @param a The original color, normalized.
 * @param b The color to blend with the original color, normalized.
 * @param w How much to blend with the original color.
 *          If set to 0.0 the result will be the original color left intact.
 */
float3 BlendSoftLight(float3 a, float3 b, float w)
{
	float3 color = (1.0 - 2.0 * b) * (a * a) + 2.0 * b * a;

	return lerp(a, color, w);
}

/**
 * Standard hard light blend mode.
 *
 * @param a The original color, normalized.
 * @param b The color to blend with the original color, normalized.
 * @param w How much to blend with the original color.
 *          If set to 0.0 the result will be the original color left intact.
 */
float3 BlendHardLight(float3 a, float3 b, float w)
{
	float3 color = (a > 0.5)
		? 2.0 * a * b
		: 1.0 - 2.0 * (1.0 - a) * (1.0 - b);

	return lerp(a, color, w);
}

}
