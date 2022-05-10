#pragma once

#include "AspectRatio.fxh"

namespace FXShaders { namespace Transform
{

float2 FisheyeLens(
	int aspectRatioScaleType,
	float2 uv,
	float amount,
	float zoom)
{
	uv = uv * 2.0 - 1.0;

	float2 fishUv = uv * AspectRatio::ApplyScale(aspectRatioScaleType, uv);
	float distort = sqrt(1.0 - fishUv.x * fishUv.x - fishUv.y * fishUv.y);

	uv *= lerp(1.0, distort * zoom, amount);

	uv = (uv + 1.0) * 0.5;

	return uv;
}

}
}
