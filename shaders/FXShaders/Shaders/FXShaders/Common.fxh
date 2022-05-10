#pragma once

/*
	FXShaders common header file.

	Defines various general utilities for use in different shaders.
*/

#include "Math.fxh"

namespace FXShaders
{

/**
 * Arbitrary margin of error/minimal value for various calculations involving
 * floating point numbers.
 */
static const float FloatEpsilon = 0.001;

/**
 * Defines a stub uniform int with with a customized display text for
 * informational purposes.
 */
#define FXSHADERS_MESSAGE(name, text) \
uniform int _##name \
< \
	ui_text = text; \
	ui_label = " "; \
	ui_type = "radio"; \
>

/**
 * Defines a stub uniform int with with a customized display text for
 * informational purposes, including a category folder.
 */
#define FXSHADERS_MESSAGE_FOLDED(name, text) \
uniform int _##name \
< \
	ui_text = text; \
	ui_category = #name; \
	ui_category_closed = true; \
	ui_label = " "; \
	ui_type = "radio"; \
>

/**
 * Defines a _Help stub uniform int that serves as a help text in the UI.
 *
 * @param text The text to be displayed inside the Help fold.
 */
#define FXSHADERS_HELP(text) FXSHADERS_MESSAGE_FOLDED(Help, text)

/**
 * Defines a _WipWarn stub uniform int that serves as a warning that the current
 * effect is a work-in-progress.
 */
#define FXSHADERS_WIP_WARNING() \
FXSHADERS_MESSAGE( \
	WipWarn, \
	"This effect is currently a work in progress and as such some " \
	"aspects may change in the future and some features may still be " \
	"missing or incomplete.\n")

/**
 * Defines a _Credits stub uniform int that displays crediting information.
 */
#define FXSHADERS_CREDITS() FXSHADERS_MESSAGE_FOLDED( \
	Credits, \
	"This effect was made by Lucas Melo (luluco250).\n" \
	"Updates may be available at https://github.com/luluco250/FXShaders\n" \
	"Any issues, suggestions or requests can also be filed there.")

/**
 * Interpolate between two values using a time parameter.
 *
 * @param a The last value.
 * @param b The next value.
 * @param t The time to interpolate in seconds.
 * @param dt The delta time in seconds.
 */
#define FXSHADERS_INTERPOLATE(a, b, t, dt) \
lerp((a), (b), saturate((dt) / max((t), FloatEpsilon)))

/**
 * Get the maximum mipmap level that can be generated from a resolution.
 *
 * @param w The resolution width.
 * @param h The resolution height.
 */
#define FXSHADERS_GET_MAX_MIP(w, h) \
(FXSHADERS_LOG2(FXSHADERS_MAX((w), (h))) + 1)

/**
 * Get the power of two value closest to the given integer.
 *
 * @param x An integer number to find the POT value it's closest to.
 */
#define FXSHADERS_NPOT(x) (\
( \
	((x - 1) >> 1) | \
	((x - 1) >> 2) | \
	((x - 1) >> 4) | \
	((x - 1) >> 8) | \
	((x - 1) >> 16) \
) + 1)

/**
 * Utility macro for transforming an expression into a string constant.
 *
 * @param x The expression to be stringified.
 */
#define FXSHADERS_STRING(x) (#x)

/**
 * Utility macro for concatenating two expressions.
 *
 * @param a The left expression.
 * @param b The right expression.
 */
#define FXSHADERS_CONCAT(a, b) (a##b)

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
 * Returns the aspect ratio of the current resolution.
 */
float GetAspectRatio()
{
	return BUFFER_WIDTH * BUFFER_RCP_HEIGHT;
}

/**
 * Returns the current resolution and pixel size in a float4.
 */
float4 GetScreenParams()
{
	return float4(GetResolution(), GetPixelSize());
}

/**
 * Applies scaling to a coordinate while preserving its position relative to a
 * specified pivot.
 *
 * @param uv The screen coordinate to scale.
 * @param scale The value to be multiplied to the coordinate.
 *              Values above 1.0 will "zoom out", those below 1.0 will
 *              "zoom in".
 * @param pivot A 0.0<->1.0 2D pivot to use when scaling the coordinate.
 *              If set to 0.5 on both axes, it'll be centered.
 */
float2 ScaleCoord(float2 uv, float2 scale, float2 pivot)
{
	return (uv - pivot) * scale + pivot;
}

/**
 * Overload for ScaleCoord() but with the pivot set to 0.5 (centered).
 *
 * @param uv The screen coordinate to scale.
 * @param scale The value to be multiplied to the coordinate.
 *              Values above 1.0 will "zoom out", those below 1.0 will
 *              "zoom in".
 */
float2 ScaleCoord(float2 uv, float2 scale)
{
	return ScaleCoord(uv, scale, 0.5);
}

/**
 * Extracts luma information from an RGB color using the standard formula.
 *
 * @param color RGB color to extract luma from.
 */
float GetLumaGamma(float3 color)
{
	return dot(color, float3(0.299, 0.587, 0.114));
}

/**
 * Extracts luma information from an RGB color using a formula with non
 * gamma-compressed values.
 *
 * @param color RGB color to extract luma from.
 */
float GetLumaLinear(float3 color)
{
	return dot(color, float3(0.2126, 0.7152, 0.0722));
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

	uv *= GetResolution();
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
float3 ApplySaturation(float3 color, float amount)
{
	float gray = GetLumaLinear(color);
	return gray + (color - gray) * amount;
}

/**
 * Get a pseudo-random value from a given coordinate.
 *
 * @param uv Coordinate used as a seed for the random value.
 */
float GetRandom(float2 uv)
{
	// static const float A = 12.9898;
	// static const float B = 78.233;
	// static const float C = 43758.5453;
	static const float A = 23.2345;
	static const float B = 84.1234;
	static const float C = 56758.9482;

    return frac(sin(dot(uv, float2(A, B))) * C);
}

/**
 * Canonical fullscreen vertex shader.
 *
 * @param id The vertex id.
 * @param pos Output vertex position.
 * @param uv Output normalized pixel position.
 */
void ScreenVS(
	uint id : SV_VERTEXID,
	out float4 pos : SV_POSITION,
	out float2 uv : TEXCOORD)
{
	uv.x = (id == 2) ? 2.0 : 0.0;
	uv.y = (id == 1) ? 2.0 : 0.0;

	pos = float4(uv * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
}

/**
 * Apply CSS-style "cover" scaling to a normalized coordinate based on the
 * aspect ratios of the source and destination images.
 *
 * @param uv A normalized coordinate to be scaled.
 * @param a The source image's aspect ratio.
 * @param b The destination image's aspect ratio.
 */
float2 CorrectAspectRatio(float2 uv, float a, float b)
{
	if (a > b)
	{
		// Source is wider.

		uv = ScaleCoord(uv, float2(1.0 / a, 1.0));
	}
	else
	{
		// Destination is wider.

		uv = ScaleCoord(uv, float2(1.0, 1.0 / b));
	}

	return uv;
}

} // Namespace.
