#pragma once

/*

Many thanks to http://www.brucelindbloom.com/

Provides facilities to convert to and from CIE LAB as well as XYZ.
All functions taking RGB input assume it's in linear space.
RGB<->XYZ conversions use the sRGB matrix.

*/

namespace FXShaders { namespace ColorLab
{

static const float Gamma = 2.2;
static const float GammaInv = 1.0 / Gamma;

static const float L_k = 903.3;
static const float L_e = 0.008856;

static const float WhitePoint = 1.0;

static const float3x3 RGBToXYZ_sRGB = float3x3(
	0.4124564, 0.3575761, 0.1804375,
	0.2126729, 0.7151522, 0.0721750,
	0.0193339, 0.1191920, 0.9503041);

static const float3x3 XYZToRGB_sRGB = float3x3(
	3.2404542, -1.5371385, -0.4985314,
	-0.9692660,  1.8760108,  0.0415560,
	0.0556434, -0.2040259,  1.0572252);

float3 gamma_to_linear(float3 c, float g)
{
	return pow(abs(c), g);
}

float3 gamma_to_linear(float3 c)
{
	return gamma_to_linear(c, Gamma);
}

float3 linear_to_gamma(float3 c, float g)
{
	return pow(abs(c), rcp(g));
}

float3 linear_to_gamma(float3 c)
{
	return pow(abs(c), GammaInv);
}

float3 srgb_to_linear(float3 c)
{
	return (c < 0.04045)
		? c / 12.92
		: pow(abs((c + 0.055) / 1.055), 2.4);
}

float3 linear_to_srgb(float3 c)
{
	return (c < 0.0031308)
		? 12.92 * c
		: 1.055 * pow(abs(c), rcp(2.4)) - 0.055;
}

float3 l_to_linear(float3 c)
{
	return (c < 0.08)
		? (100 * c) / L_k
		: pow(abs((c + 0.16) / 1.16), 3.0);
}

float3 linear_to_l(float3 c)
{
	return (c < L_e)
		? (c * L_k) / 100.0
		: 1.16 * pow(abs(c), rcp(3.0)) - 0.16;
}

float3 rgb_to_xyz(float3 c)
{
	return mul(RGBToXYZ_sRGB, c);
}

float3 xyz_to_rgb(float3 c)
{
	return mul(XYZToRGB_sRGB, c);
}

float3 xyz_to_lab(float3 c, float w)
{
	c /= w;

	c = (c > L_e)
		? pow(abs(c), rcp(3.0))
		: (L_k * c + 16.0) / 116.0;

	float3 lab;
	lab.x = 116.0 * c.y - 16.0;
	lab.y = 500.0 * (c.x - c.y);
	lab.z = 200.0 * (c.y - c.z);

	return lab;
}

float3 xyz_to_lab(float3 c)
{
	return xyz_to_lab(c, WhitePoint);
}

float3 lab_to_xyz(float3 lab, float w)
{
	float3 c;
	c.y = (lab.x + 16.0) / 116.0;
	c.x = lab.y / 500.0 + c.y;
	c.z = c.y - lab.z / 200.0;

	float f3 = c.x * c.x * c.x;
	c.x = (f3 > L_e)
		? f3
		: (116.0 * c.x - 16.0) / L_k;

	c.y = (lab.x > L_k * L_e)
		? pow(abs((lab.x + 16.0) / 116.0), 3.0)
		: lab.x / L_k;

	f3 = c.z * c.z * c.z;
	c.z = (f3 > L_e)
		? f3
		: (116.0 * c.z - 16.0) / L_k;

	c *= w;
	return c;
}

float3 lab_to_xyz(float3 lab)
{
	return lab_to_xyz(lab, WhitePoint);
}

float3 rgb_to_lab(float3 c, float w)
{
	return xyz_to_lab(rgb_to_xyz(c), w);
}

float3 rgb_to_lab(float3 c)
{
	return rgb_to_lab(c, WhitePoint);
}

float3 lab_to_rgb(float3 c, float w)
{
	return xyz_to_rgb(lab_to_xyz(c, w));
}

float3 lab_to_rgb(float3 c)
{
	return lab_to_rgb(c, WhitePoint);
}

}}
