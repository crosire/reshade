#pragma once

#define FXSHADERS_TONEMAPPER_LIST \
"Reinhard\0" \
"Uncharted 2 Filmic\0" \
"BakingLab ACES\0"  \
"Narkowicz ACES\0" \
"Unreal3\0" \
"Lottes\0"

namespace FXShaders { namespace Tonemap
{

namespace Type
{
	static const int Reinhard = 0;
	static const int Uncharted2Filmic = 1;
	static const int BakingLabACES = 2;
	static const int NarkowiczACES = 3;
	static const int Unreal3 = 4;
	static const int Lottes = 5;
}

namespace Reinhard
{
	/**
	* Standard Reinhard tonemapping formula.
	*
	* @param color The color to apply tonemapping to.
	*/
	float3 Apply(float3 color)
	{
		return color / (1.0 + color);
	}

	/**
	* Inverse of the standard Reinhard tonemapping formula.
	*
	* @param color The color to apply inverse tonemapping to.
	*/
	float3 Inverse(float3 color)
	{
		return -(color / min(color - 1.0, -0.1));
	}

	/**
	* Incorrect inverse of the standard Reinhard tonemapping formula.
	* This is only here for NeoBloom right now.
	*
	* @param color The color to apply inverse tonemapping to.
	* @param w The inverse/reciprocal of the maximum brightness to be
	*          generated.
	*          Sample parameter: rcp(100.0)
	*/
	float3 InverseOld(float3 color, float w)
	{
		return color / max(1.0 - color, w);
	}

	/**
	* Modified inverse of the Reinhard tonemapping formula that only applies to
	* the luma.
	*
	* @param color The color to apply inverse tonemapping to.
	* @param w The inverse/reciprocal of the maximum brightness to be
	*          generated.
	*          Sample parameter: rcp(100.0)
	*/
	float3 InverseOldLum(float3 color, float w)
	{
		float lum = max(color.r, max(color.g, color.b));
		return color * (lum / max(1.0 - lum, w));
	}
}

namespace Uncharted2Filmic
{
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

	float3 Apply(float3 color)
	{
		color =
		(
			(color * (A * color + C * B) + D * E) /
			(color * (A * color + B) + D * F)
		) - E / F;

		return color;
	}

	float3 Inverse(float3 color)
	{
		return abs(
			((B * C * F - B * E - B * F * color) -
			sqrt(
				pow(abs(-B * C * F + B * E + B * F * color), 2.0) -
				4.0 * D * (F * F) * color * (A * E + A * F * color - A * F))) /
			(2.0 * A * (E + F * color - F)));
	}
}

namespace BakingLabACES
{
	// sRGB => XYZ => D65_2_D60 => AP1 => RRT_SAT
	static const float3x3 ACESInputMat = float3x3
	(
		0.59719, 0.35458, 0.04823,
		0.07600, 0.90834, 0.01566,
		0.02840, 0.13383, 0.83777
	);

	// ODT_SAT => XYZ => D60_2_D65 => sRGB
	static const float3x3 ACESOutputMat = float3x3
	(
		1.60475, -0.53108, -0.07367,
		-0.10208,  1.10813, -0.00605,
		-0.00327, -0.07276,  1.07602
	);

	float3 RRTAndODTFit(float3 v)
	{
		float3 a = v * (v + 0.0245786f) - 0.000090537f;
		float3 b = v * (0.983729f * v + 0.4329510f) + 0.238081f;
		return a / b;
	}

	float3 ACESFitted(float3 color)
	{
		color = mul(ACESInputMat, color);

		// Apply RRT and ODT
		color = RRTAndODTFit(color);

		color = mul(ACESOutputMat, color);

		// Clamp to [0, 1]
		color = saturate(color);

		return color;
	}

	static const float A = 0.0245786;
	static const float B = 0.000090537;
	static const float C = 0.983729;
	static const float D = 0.4329510;
	static const float E = 0.238081;

	float3 Apply(float3 color)
	{
		return saturate(
			(color * (color + A) - B) /
			(color * (C * color + D) + E));
	}

	float3 Inverse(float3 color)
	{
		return abs(
			((A - D * color) -
			sqrt(
				pow(abs(D * color - A), 2.0) -
				4.0 * (C * color - 1.0) * (B + E * color))) /
			(2.0 * (C * color - 1.0)));
	}
}

namespace Lottes
{
	float3 Apply(float3 color)
	{
		return color * rcp(max(color.r, max(color.g, color.b)) + 1.0);
	}

	float3 Inverse(float3 color)
	{
		return color * rcp(max(1.0 - max(color.r, max(color.g, color.b)), 0.1));
	}
}

namespace NarkowiczACES
{
	static const float A = 2.51;
	static const float B = 0.03;
	static const float C = 2.43;
	static const float D = 0.59;
	static const float E = 0.14;

	float3 Apply(float3 color)
	{
		return saturate(
			(color * (A * color + B)) / (color * (C * color + D) + E));
	}

	float3 Inverse(float3 color)
	{
		return
			((D * color - B) +
			sqrt(
				4.0 * A * E * color + B * B -
				2.0 * B * D * color -
				4.0 * C * E * color * color +
				D * D * color * color)) /
			(2.0 * (A - C * color));
	}
}

namespace Unreal3
{
	float3 Apply(float3 color)
	{
		return color / (color + 0.155) * 1.019;
	}

	float3 Inverse(float3 color)
	{
		return (color * -0.155) / (max(color, 0.01) - 1.019);
	}
}

float3 Apply(int type, float3 color)
{
	switch (type)
	{
		case Type::Reinhard:
			return Reinhard::Apply(color);
		case Type::Uncharted2Filmic:
			return Uncharted2Filmic::Apply(color);
		case Type::BakingLabACES:
			return BakingLabACES::Apply(color);
		case Type::NarkowiczACES:
			return NarkowiczACES::Apply(color);
		case Type::Unreal3:
			return Unreal3::Apply(color);
		case Type::Lottes:
			return Lottes::Apply(color);
	}

	return color;
}

float3 Inverse(int type, float3 color)
{
	switch (type)
	{
		case Type::Reinhard:
			return Reinhard::Inverse(color);
		case Type::Uncharted2Filmic:
			return Uncharted2Filmic::Inverse(color);
		case Type::BakingLabACES:
			return BakingLabACES::Inverse(color);
		case Type::NarkowiczACES:
			return NarkowiczACES::Inverse(color);
		case Type::Unreal3:
			return Unreal3::Inverse(color);
		case Type::Lottes:
			return Lottes::Inverse(color);
	}

	return color;
}

}} // Namespace.
