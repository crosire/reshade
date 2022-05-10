#pragma once

namespace FXShaders
{

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
 * Get the lesser of two values.
 *
 * The only reason for this is because ReShade 4.0+ does not allow intrinsics in
 * constant value definitions.
 *
 * @param a The first value.
 * @param b The second value.
 */
#define FXSHADERS_MIN(a, b) (int((a) < (b)) * (a) + int((b) < (a)) * (b))

/**
 * Get the greater of two values.
 *
 * The only reason for this is because ReShade 4.0+ does not allow intrinsics in
 * constant value definitions.
 *
 * @param a The first value.
 * @param b The second value.
 */
#define FXSHADERS_MAX(a, b) (int((a) > (b)) * (a) + int((b) > (a)) * (b))

/**
 * Constrain a value between minimum and maximum values.
 *
 * The only reason for this is because ReShade 4.0+ does not allow intrinsics in
 * constant value definitions.
 *
 * @param x The value to constrain.
 * @param a The minimum value.
 * @param b The maximum value.
 */
#define FXSHADERS_CLAMP(x, a, b) (FXSHADERS_MAX((a), FXSHADERS_MIN((x), (b))))

/**
 * Constrain a value between 0 and 1.
 *
 * The only reason for this is because ReShade 4.0+ does not allow intrinsics in
 * constant value definitions.
 *
 * @param x The value to constrain.
 */
#define FXSHADERS_SATURATE(x) (FXSHADERS_CLAMP((x), 0, 1))

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
 * Clamps the magnitude/length of a 2D vector.
 * Better than clamp() because it maintains the "direction" of the vector.
 *
 * @param v The vector to clamp the magnitude of.
 * @param minMax The minimum and maximum magnitude values respectively.
 */
float2 ClampMagnitude(float2 v, float2 minMax) {
	if (v.x == 0.0 && v.y == 0.0)
	{
		return 0.0;
	}
	else
	{
		float mag = length(v);
		return (mag < minMax.x) ? 0.0 : (v / mag) * min(mag, minMax.y);
	}
}

/**
 * Apply rotation to a x/y point.
 *
 * @param uv The point to rotate.
 * @param angle The rotation angle in degrees.
 * @param pivot The origin point.
 */
float2 RotatePoint(float2 uv, float angle, float2 pivot)
{
	float2 sc;
	sincos(DegreesToRadians * angle, sc.x, sc.y);

	uv -= pivot;
	uv = uv.x * sc.yx + float2(-uv.y, uv.y) * sc;
	uv += pivot;

	return uv;
}

}
