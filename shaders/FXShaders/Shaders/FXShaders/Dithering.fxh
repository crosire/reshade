#pragma once

#include "Common.fxh"

namespace FXShaders { namespace Dithering
{

namespace Ordered16
{
	static const int Width = 4;

	static const int Pattern[Width * Width] =
	{
		0, 8, 2, 10,
		12, 4, 14, 6,
		3, 11, 1, 9,
		15, 7, 13, 5
	};

	float3 Apply(float3 color, float2 uv, float amount)
	{
		int2 pos = uv * GetResolution();
		pos %= Width;

		float value = Pattern[pos.x * Width + pos.y];
		value /= Width * Width;

		color *= 1.0 + (value * 2.0 - 1.0) * amount;

		return color;
	}
}

}} // Namespace.
