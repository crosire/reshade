#pragma once

/*
	FXShaders common header file.

	Defines various general utilities for use in different shaders.
*/

namespace FXShaders
{

/**
 * Defines a _Help stub uniform int that serves as a help text in the UI.
 *
 * @param text The text to be displayed inside the Help fold.
 */
#define FXSHADERS_CREATE_HELP(text) \
uniform int _Help \
< \
	ui_text = text; \
	ui_category = "Help"; \
	ui_category_closed = true; \
	ui_label = " "; \
	ui_type = "radio"; \
>

/**
 * Bitwise int log2 using magic numbers.
 *
 * The only reason for this is because ReShade 4.0+ does not allow intrinsics in
 * constant value definitions.
 *
 * @param x The integer value to calculate the log2 of.
 */
#define FXSHADERS_LOG2(x) (\
	(((x) & 0xAAAAAAAA) != 0) | \
	((((x) & 0xFFFF0000) != 0) << 4) | \
	((((x) & 0xFF00FF00) != 0) << 3) | \
	((((x) & 0xF0F0F0F0) != 0) << 2) | \
	((((x) & 0xCCCCCCCC) != 0) << 1))

/**
 * Interpolate between two values using a time parameter.
 *
 * @param a The last value.
 * @param b The next value.
 * @param t The time to interpolate in seconds.
 * @param dt The delta time in seconds.
 */
#define FXSHADERS_INTERPOLATE(a, b, t, dt) \
lerp(a, b, saturate((dt) / max(t, 0.001)))

/**
 * The Pi constant.
 */
static const float Pi = 3.14159;

/**
 * Degrees to radians conversion multiplier.
 */
static const float DegreesToRadians = Pi / 180.0;

/**
 * Radians to degrees conversion multiplier.
 */
static const float RadiansToDegrees = 180.0 / Pi;

/**
 * Returns the current resolution.
 */
float2 GetResolution()
{
	return float2(BUFFER_WIDTH, BUFFER_HEIGHT);
}

/**
 * Returns the current pixel size/reciprocal resolution.
 */
float2 GetPixelSize()
{
	return float2(BUFFER_RCP_WIDTH, BUFFER_RCP_HEIGHT);
}

/**
 * Returns the current resolution and pixel size in a float4.
 */
float4 GetScreenParams()
{
	return float4(GetResolution(), GetPixelSize());
}

/**
 * Get the offset of a position by a given angle and distance.
 *
 * @param pos The position to calculate the offset from.
 * @param angle The angle of the offset in radians.
 * @param distance The distance of the offset from the original position.
 */
float2 GetOffsetByAngleDistance(float2 pos, float angle, float distance)
{
	float2 cosSin;
	sincos(angle, cosSin.y, cosSin.x);

	return mad(distance, cosSin, pos);
}

/**
 * Get a 2D direction from a given angle and magnitude.
 * It is the same as GetOffsetByAngleDistance() except the origin is (0, 0).
 *
 * @param angle The angle of the direction in radians.
 * @param magnitude The magnitude of the direction.
 */
float2 GetDirectionFromAngleMagnitude(float angle, float magnitude)
{
	return GetOffsetByAngleDistance(0.0, angle, magnitude);
}

/**
 * Applies scaling to a coordinate while preserving it's position relative to
 * a specified pivot.
 *
 * @param uv The screen coordinate to scale.
 * @param scale The value to be multiplied to the coordinate.
 *              Values above 1.0 will "zoom out", those below 1.0 will
 *              "zoom in".
 * @param pivot A 0.0<->1.0 2D pivot to use when scaling the coordinate.
 *              If set to 0.5 on both axes, it'll be centered.
 */
float2 scale_uv(float2 uv, float2 scale, float2 pivot)
{
	return (uv - pivot) * scale + pivot;
}

/**
 * Overload for scale_uv() but with the pivot set to 0.5 (centered).
 *
 * @param uv The screen coordinate to scale.
 * @param scale The value to be multiplied to the coordinate.
 *              Values above 1.0 will "zoom out", those below 1.0 will
 *              "zoom in".
 */
float2 scale_uv(float2 uv, float2 scale)
{
	return scale_uv(uv, scale, 0.5);
}

/**
 * Standard Reinhard tonemapping formula.
 *
 * @param color The color to apply tonemapping to.
 */
float3 reinhard(float3 color)
{
	return color / (1.0 + color);
}

/**
 * Inverse of the standard Reinhard tonemapping formula.
 *
 * @param color The color to apply inverse tonemapping to.
 * @param inv_max The inverse/reciprocal of the maximum brightness to be
 *                generated.
 *                Sample parameter: rcp(100.0)
 */
float3 inv_reinhard(float3 color, float inv_max)
{
	return (color / max(1.0 - color, inv_max));
}

/**
 * Modified inverse of the Reinhard tonemapping formula that only applies to
 * the luma.
 *
 * @param color The color to apply inverse tonemapping to.
 * @param inv_max The inverse/reciprocal of the maximum brightness to be
 *                generated.
 *                Sample parameter: rcp(100.0)
 */
float3 inv_reinhard_lum(float3 color, float inv_max)
{
	float lum = max(color.r, max(color.g, color.b));
	return color * (lum / max(1.0 - lum, inv_max));
}

/**
 * The standard, copy/paste Uncharted 2 filmic tonemapping formula.
 *
 * @param color The color to apply tonemapping to.
 * @param exposure The amount of exposure to be applied to the color during
 *                 tonemapping.
 */
float3 uncharted2_tonemap(float3 color, float exposure) {
    // Shoulder strength.
	static const float A = 0.15;

	// Linear strength.
    static const float B = 0.50;

	// Linear angle.
	static const float C = 0.10;

	// Toe strength.
	static const float D = 0.20;

	// Toe numerator.
	static const float E = 0.02;

	// Toe denominator.
	static const float F = 0.30;

	// Linear white point value.
	static const float W = 11.2;

    static const float white =
		1.0 / ((
			(W * (A * W + C * B) + D * E) /
			(W * (A * W + B) + D * F)
		) - E / F);

	color *= exposure;

    color = (
		(color * (A * color + C * B) + D * E) /
		(color * (A * color + B) + D * F)
	) - E / F;

	color *= white;

    return color;
}

/**
 * Standard normal distribution formula.
 * Adapted from: https://en.wikipedia.org/wiki/Normal_distribution#General_normal_distribution
 *
 * @param x The value to distribute.
 * @param u The distribution mean.
 * @param o The distribution sigma.
 *          This value is squared in this function, so there's no need to square
 *          it beforehand, unlike the standard formula.
 */
float normal_distribution(float x, float u, float o)
{
	o *= o;

	float a = 1.0 / sqrt(2.0 * Pi * o);
	float b = x - u;
	b *= b;
	b /= 2.0 * o;

	return a * exp(-(b));
}

/**
 * One dimensional gaussian distribution formula.
 *
 * @param x The value to distribute.
 * @param o The distribution sigma.
 *          This parameter is squared in this function, so there's no need to
 *          square it beforehand.
 */
float gaussian1D(float x, float o)
{
	o *= o;
	float a = 1.0 / sqrt(2.0 * Pi * o);
	float b = (x * x) / (2.0 * o);
	return a * exp(-b);
}

/**
 * Two dimensional gaussian distribution formula.
 *
 * @param i The x and y values to distribute.
 * @param o The distribution sigma.
 *          This parameter is squared in this function, so there's no need to
 *          square it beforehand.
 */
float gaussian2D(float2 i, float o)
{
	o *= o;
	float a = 1.0 / (2.0 * Pi * o);
	float b = (i.x * i.x + i.y * i.y) / (2.0 * o);
	return a * exp(-b);
}

/**
 * One dimensional gaussian blur.
 *
 * @param sp The sampler to read from.
 * @param uv The normalized screen coordinates used for reading from the
 *           sampler.
 * @param dir The 2D direction and scale of the blur.
 *            Remember to pass the pixel/texel size here.
 * @param sigma The sigma value used for the gaussian distribution.
 *              This does not need to be squared.
 * @param samples The amount of blur samples to perform.
 *                This value must be constant.
 */
float4 gaussian_blur1D(
	sampler sp,
	float2 uv,
	float2 dir,
	float sigma,
	int samples)
{
	static const float half_samples = samples * 0.5;

	float4 color = 0.0;
	float accum = 0.0;

	uv -= half_samples * dir;

	[unroll]
	for (int i = 0; i < samples; ++i)
	{
		float weight = gaussian1D(i - half_samples, sigma);

		color += tex2D(sp, uv) * weight;
		accum += weight;

		uv += dir;
	}

	return color / accum;
}

float4 gaussian_blur2D(
	sampler sp,
	float2 uv,
	float2 scale,
	float sigma,
	int2 samples)
{
	static const float2 half_samples = samples * 0.5;

	float4 color = 0.0;
	float accum = 0.0;

	uv -= half_samples * scale;

	[unroll]
	for (int x = 0; x < samples.x; ++x)
	{
		float init_x = uv.x;

		[unroll]
		for (int y = 0; y < samples.y; ++y)
		{
			float2 pos = float2(x, y);
			float weight = gaussian2D(abs(pos - half_samples), sigma);

			color += tex2D(sp, uv) * weight;
			accum += weight;

			uv.x += scale.x;
		}

		uv.x = init_x;
		uv.y += scale.y;
	}

	return color / accum;
}

/**
 * Extracts luma information from an RGB color using the standard formula.
 *
 * @param color RGB color to extract luma from.
 */
float get_luma_gamma(float3 color)
{
	return dot(color, float3(0.299, 0.587, 0.114));
}

/**
 * Extracts luma information from an RGB color using a formula with non
 * gamma-compressed values.
 *
 * @param color RGB color to extract luma from.
 */
float get_luma_linear(float3 color)
{
	return dot(color, float3(0.2126, 0.7152, 0.0722));
}

/**
 * Standard screen blend mode.
 *
 * @param a The original color.
 * @param b The color to blend with the original color.
 * @param w How much to blend with the original color.
 *          If set to 0.0 the result will be the original color left intact.
 * @param m The maximum brightness/value for each color.
 *          Typically it's 1.0.
 */
float3 blend_mode_screen(float3 a, float3 b, float w, float m)
{
	return m - (m - a) * (m - b * w);
}

/**
 * Overload of blend_mode_screen that sets the maximum brightness to 1.0.
 *
 * @param a The original color.
 * @param b The color to blend with the original color.
 * @param w How much to blend with the original color.
 *          If set to 0.0 the result will be the original color left intact.
 */
float3 blend_mode_screen(float3 a, float3 b, float w)
{
	return blend_mode_screen(a, b, w, 1.0);
}

/**
 * Generates a checkerboard pattern.
 *
 * @param uv A normalized pixel shader coordinate used to generate the pattern.
 * @param size The size of each cell in the pattern.
 * @param color_a The first color of the pattern.
 * @param color_b The second color of the pattern.
 */
float3 checkered_pattern(float2 uv, float size, float color_a, float color_b)
{
	static const float cSize = 32.0;
	static const float3 cColorA = pow(0.15, 2.2);
	static const float3 cColorB = pow(0.5, 2.2);

	uv *= ReShade::ScreenSize;
	uv %= cSize;

	float half_size = cSize * 0.5;
	float checkered = step(uv.x, half_size) == step(uv.y, half_size);
	return (cColorA * checkered) + (cColorB * (1.0 - checkered));
}

/**
 * Overload of checkered_pattern() with preset values.
 * size = 32.0
 * color_a = pow(0.15, 2.2)
 * color_b = pow(0.5, 2.2)
 */
float3 checkered_pattern(float2 uv)
{
	static const float Size = 32.0;
	static const float ColorA = pow(0.15, 2.2);
	static const float ColorB = pow(0.5, 2.2);

	return checkered_pattern(uv, Size, ColorA, ColorB);
}

/**
 * Modify the saturation of a given RGB color.
 *
 * @param color THe RGB color to modify.
 * @param amount The amount of saturation to use.
 *               Setting it to 0.0 returns a fully grayscale color.
 *               Setting it to 1.0 returns the same color.
 *               Values above 1.0 will increase saturation as well as some
 *               brightness.
 */
float3 apply_saturation(float3 color, float amount)
{
	float gray = get_luma_linear(color);
	return gray + (color - gray) * amount;
}

}
