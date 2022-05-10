#pragma once

#define FXSHADERS_ASPECT_RATIO_SCALE_TYPE_LIST \
"Cover\0" \
"Fit\0" \
"Stretch\0"

namespace FXShaders { namespace AspectRatio
{

namespace ScaleType
{
	static const int Cover = 0;
	static const int Fit = 1;
	static const int Stretch = 2;
}

float2 CoverScale(float2 uv)
{
	return BUFFER_WIDTH > BUFFER_HEIGHT
		? float2(1.0, BUFFER_HEIGHT * BUFFER_RCP_WIDTH)
		: float2(BUFFER_WIDTH * BUFFER_RCP_HEIGHT, 1.0);
}

float2 FitScale(float2 uv)
{
	return BUFFER_WIDTH > BUFFER_HEIGHT
		? float2(BUFFER_WIDTH * BUFFER_RCP_HEIGHT, 1.0)
		: float2(1.0, BUFFER_HEIGHT * BUFFER_RCP_WIDTH);
}

float2 ApplyScale(int type, float2 uv)
{
	switch (type)
	{
		case ScaleType::Cover:
			return CoverScale(uv);
		case ScaleType::Fit:
			return FitScale(uv);
		// case ScaleType::Stretch:
		default:
			return uv;
	}
}

}} // Namespace.
