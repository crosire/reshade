// Copyright (c) 2009-2015 Gilcher Pascal aka Marty McFly

#include "ReShadeUI.fxh"

uniform int iBloomMixmode <
	ui_type = "combo";
	ui_items = "Linear add\0Screen add\0Screen/Lighten/Opacity\0Lighten\0";
> = 2;
uniform float fBloomThreshold < __UNIFORM_SLIDER_FLOAT1
	ui_min = 0.1; ui_max = 1.0;
	ui_tooltip = "Every pixel brighter than this value triggers bloom.";
> = 0.8;
uniform float fBloomAmount < __UNIFORM_SLIDER_FLOAT1
	ui_min = 0.0; ui_max = 20.0;
	ui_tooltip = "Intensity of bloom.";
> = 0.8;
uniform float fBloomSaturation < __UNIFORM_SLIDER_FLOAT1
	ui_min = 0.0; ui_max = 2.0;
	ui_tooltip = "Bloom saturation. 0.0 means white bloom, 2.0 means very, very colorful bloom.";
> = 0.8;
uniform float3 fBloomTint < __UNIFORM_COLOR_FLOAT3
	ui_tooltip = "R, G and B components of bloom tint the bloom color gets shifted to.";
> = float3(0.7, 0.8, 1.0);

uniform bool bLensdirtEnable <
> = false;
uniform int iLensdirtMixmode <
	ui_type = "combo";
	ui_items = "Linear add\0Screen add\0Screen/Lighten/Opacity\0Lighten\0";
> = 1;
uniform float fLensdirtIntensity < __UNIFORM_SLIDER_FLOAT1
	ui_min = 0.0; ui_max = 2.0;
	ui_tooltip = "Intensity of lensdirt.";
> = 0.4;
uniform float fLensdirtSaturation < __UNIFORM_SLIDER_FLOAT1
	ui_min = 0.0; ui_max = 2.0;
	ui_tooltip = "Color saturation of lensdirt.";
> = 2.0;
uniform float3 fLensdirtTint < __UNIFORM_COLOR_FLOAT3
	ui_tooltip = "R, G and B components of lensdirt tint the lensdirt color gets shifted to.";
> = float3(1.0, 1.0, 1.0);

uniform bool bAnamFlareEnable <
> = false;
uniform float fAnamFlareThreshold < __UNIFORM_SLIDER_FLOAT1
	ui_min = 0.1; ui_max = 1.0;
	ui_tooltip = "Every pixel brighter than this value gets a flare.";
> = 0.9;
uniform float fAnamFlareWideness < __UNIFORM_SLIDER_FLOAT1
	ui_min = 1.0; ui_max = 2.5;
	ui_tooltip = "Horizontal wideness of flare. Don't set too high, otherwise the single samples are visible.";
> = 2.4;
uniform float fAnamFlareAmount < __UNIFORM_SLIDER_FLOAT1
	ui_min = 1.0; ui_max = 20.0;
	ui_tooltip = "Intensity of anamorphic flare.";
> = 14.5;
uniform float fAnamFlareCurve < __UNIFORM_SLIDER_FLOAT1
	ui_min = 1.0; ui_max = 2.0;
	ui_tooltip = "Intensity curve of flare with distance from source.";
> = 1.2;
uniform float3 fAnamFlareColor < __UNIFORM_COLOR_FLOAT3
	ui_tooltip = "R, G and B components of anamorphic flare. Flare is always same color.";
> = float3(0.012, 0.313, 0.588);

uniform bool bLenzEnable <
> = false;
uniform float fLenzIntensity < __UNIFORM_SLIDER_FLOAT1
	ui_min = 0.2; ui_max = 3.0;
	ui_tooltip = "Power of lens flare effect";
> = 1.0;
uniform float fLenzThreshold < __UNIFORM_SLIDER_FLOAT1
	ui_min = 0.6; ui_max = 1.0;
	ui_tooltip = "Minimum brightness an object must have to cast lensflare.";
> = 0.8;

uniform bool bChapFlareEnable <
> = false;
uniform float fChapFlareTreshold < __UNIFORM_SLIDER_FLOAT1
	ui_min = 0.70; ui_max = 0.99;
	ui_tooltip = "Brightness threshold for lensflare generation. Everything brighter than this value gets a flare.";
> = 0.90;
uniform int iChapFlareCount < __UNIFORM_SLIDER_INT1
	ui_min = 1; ui_max = 20;
	ui_tooltip = "Number of single halos to be generated. If set to 0, only the curved halo around is visible.";
> = 15;
uniform float fChapFlareDispersal < __UNIFORM_SLIDER_FLOAT1
	ui_min = 0.25; ui_max = 1.00;
	ui_tooltip = "Distance from screen center (and from themselves) the flares are generated. ";
> = 0.25;
uniform float fChapFlareSize < __UNIFORM_SLIDER_FLOAT1
	ui_min = 0.20; ui_max = 0.80;
	ui_tooltip = "Distance (from screen center) the halo and flares are generated.";
> = 0.45;
uniform float3 fChapFlareCA < __UNIFORM_SLIDER_FLOAT3
	ui_min = -0.5; ui_max = 0.5;
	ui_tooltip = "Offset of RGB components of flares as modifier for Chromatic abberation. Same 3 values means no CA.";
> = float3(0.00, 0.01, 0.02);
uniform float fChapFlareIntensity < __UNIFORM_SLIDER_FLOAT1
	ui_min = 5.0; ui_max = 200.0;
	ui_tooltip = "Intensity of flares and halo, remember that higher threshold lowers intensity, you might play with both values to get desired result.";
> = 100.0;

uniform bool bGodrayEnable <
> = false;
uniform float fGodrayDecay <
	ui_type = "drag";
	ui_min = 0.5000; ui_max = 0.9999;
	ui_tooltip = "How fast they decay. It's logarithmic, 1.0 means infinite long rays which will cover whole screen";
> = 0.9900;
uniform float fGodrayExposure < __UNIFORM_SLIDER_FLOAT1
	ui_min = 0.7; ui_max = 1.5;
	ui_tooltip = "Upscales the godray's brightness";
> = 1.0;
uniform float fGodrayWeight < __UNIFORM_SLIDER_FLOAT1
	ui_min = 0.80; ui_max = 1.70;
	ui_tooltip = "weighting";
> = 1.25;
uniform float fGodrayDensity < __UNIFORM_SLIDER_FLOAT1
	ui_min = 0.2; ui_max = 2.0;
	ui_tooltip = "Density of rays, higher means more and brighter rays";
> = 1.0;
uniform float fGodrayThreshold < __UNIFORM_SLIDER_FLOAT1
	ui_min = 0.6; ui_max = 1.0;
	ui_tooltip = "Minimum brightness an object must have to cast godrays";
> = 0.9;
uniform int iGodraySamples <
	ui_tooltip = "2^x format values; How many samples the godrays get";
> = 128;

uniform float fFlareLuminance < __UNIFORM_SLIDER_FLOAT1
	ui_min = 0.000; ui_max = 1.000;
	ui_tooltip = "bright pass luminance value ";
> = 0.095;
uniform float fFlareBlur < __UNIFORM_SLIDER_FLOAT1
	ui_min = 1.0; ui_max = 10000.0;
	ui_tooltip = "manages the size of the flare";
> = 200.0;
uniform float fFlareIntensity < __UNIFORM_SLIDER_FLOAT1
	ui_min = 0.20; ui_max = 5.00;
	ui_tooltip = "effect intensity";
> = 2.07;
uniform float3 fFlareTint < __UNIFORM_COLOR_FLOAT3
	ui_tooltip = "effect tint RGB";
> = float3(0.137, 0.216, 1.0);

// If 1, only pixels with depth = 1 get lens flares
// This prevents white objects from getting lens flares sources, which would normally happen in LDR
#ifndef LENZ_DEPTH_CHECK
	#define LENZ_DEPTH_CHECK 0
#endif
#ifndef CHAPMAN_DEPTH_CHECK
	#define CHAPMAN_DEPTH_CHECK 0
#endif
#ifndef GODRAY_DEPTH_CHECK
	#define GODRAY_DEPTH_CHECK 0
#endif
#ifndef FLARE_DEPTH_CHECK
	#define FLARE_DEPTH_CHECK 0
#endif

texture texDirt < source = "LensDB.png"; > { Width = 1920; Height = 1080; Format = RGBA8; };
texture texSprite < source = "LensSprite.png"; > { Width = 1920; Height = 1080; Format = RGBA8; };

sampler SamplerDirt { Texture = texDirt; };
sampler SamplerSprite { Texture = texSprite; };

texture texBloom1
{
	Width = BUFFER_WIDTH;
	Height = BUFFER_HEIGHT;
	Format = RGBA16F;
};
texture texBloom2
{
	Width = BUFFER_WIDTH;
	Height = BUFFER_HEIGHT;
	Format = RGBA16F;
};
texture texBloom3
{
	Width = BUFFER_WIDTH / 2;
	Height = BUFFER_HEIGHT / 2;
	Format = RGBA16F;
};
texture texBloom4
{
	Width = BUFFER_WIDTH / 4;
	Height = BUFFER_HEIGHT / 4;
	Format = RGBA16F;
};
texture texBloom5
{
	Width = BUFFER_WIDTH / 8;
	Height = BUFFER_HEIGHT / 8;
	Format = RGBA16F;
};
texture texLensFlare1
{
	Width = BUFFER_WIDTH / 2;
	Height = BUFFER_HEIGHT / 2;
	Format = RGBA16F;
};
texture texLensFlare2
{
	Width = BUFFER_WIDTH / 2;
	Height = BUFFER_HEIGHT / 2;
	Format = RGBA16F;
};

sampler SamplerBloom1 { Texture = texBloom1; };
sampler SamplerBloom2 { Texture = texBloom2; };
sampler SamplerBloom3 { Texture = texBloom3; };
sampler SamplerBloom4 { Texture = texBloom4; };
sampler SamplerBloom5 { Texture = texBloom5; };
sampler SamplerLensFlare1 { Texture = texLensFlare1; };
sampler SamplerLensFlare2 { Texture = texLensFlare2; };

#include "ReShade.fxh"

float4 GaussBlur22(float2 coord, sampler tex, float mult, float lodlevel, bool isBlurVert)
{
	float4 sum = 0;
	float2 axis = isBlurVert ? float2(0, 1) : float2(1, 0);

	const float weight[11] = {
		0.082607,
		0.080977,
		0.076276,
		0.069041,
		0.060049,
		0.050187,
		0.040306,
		0.031105,
		0.023066,
		0.016436,
		0.011254
	};

	for (int i = -10; i < 11; i++)
	{
		float currweight = weight[abs(i)];
		sum += tex2Dlod(tex, float4(coord.xy + axis.xy * (float)i * ReShade::PixelSize * mult, 0, lodlevel)) * currweight;
	}

	return sum;
}

float3 GetDnB(sampler tex, float2 coords)
{
	float3 color = max(0, dot(tex2Dlod(tex, float4(coords.xy, 0, 4)).rgb, 0.333) - fChapFlareTreshold) * fChapFlareIntensity;
#if CHAPMAN_DEPTH_CHECK
	if (tex2Dlod(ReShade::DepthBuffer, float4(coords.xy, 0, 3)).x < 0.99999)
		color = 0;
#endif
	return color;
}
float3 GetDistortedTex(sampler tex, float2 sample_center, float2 sample_vector, float3 distortion)
{
	float2 final_vector = sample_center + sample_vector * min(min(distortion.r, distortion.g), distortion.b);

	if (final_vector.x > 1.0 || final_vector.y > 1.0 || final_vector.x < -1.0 || final_vector.y < -1.0)
		return float3(0, 0, 0);
	else
		return float3(
			GetDnB(tex, sample_center + sample_vector * distortion.r).r,
			GetDnB(tex, sample_center + sample_vector * distortion.g).g,
			GetDnB(tex, sample_center + sample_vector * distortion.b).b);
}

float3 GetBrightPass(float2 coords)
{
	float3 c = tex2D(ReShade::BackBuffer, coords).rgb;
	float3 bC = max(c - fFlareLuminance.xxx, 0.0);
	float bright = dot(bC, 1.0);
	bright = smoothstep(0.0f, 0.5, bright);
	float3 result = lerp(0.0, c, bright);
#if FLARE_DEPTH_CHECK
	float checkdepth = tex2D(ReShade::DepthBuffer, coords).x;
	if (checkdepth < 0.99999)
		result = 0;
#endif
	return result;
}
float3 GetAnamorphicSample(int axis, float2 coords, float blur)
{
	coords = 2.0 * coords - 1.0;
	coords.x /= -blur;
	coords = 0.5 * coords + 0.5;
	return GetBrightPass(coords);
}

void BloomPass0(float4 vpos : SV_Position, float2 texcoord : TEXCOORD, out float4 bloom : SV_Target)
{
	bloom = 0.0;

	const float2 offset[4] = {
		float2(1.0, 1.0),
		float2(1.0, 1.0),
		float2(-1.0, 1.0),
		float2(-1.0, -1.0)
	};

	for (int i = 0; i < 4; i++)
	{
		float2 bloomuv = offset[i] * ReShade::PixelSize.xy * 2;
		bloomuv += texcoord;
		float4 tempbloom = tex2Dlod(ReShade::BackBuffer, float4(bloomuv.xy, 0, 0));
		tempbloom.w = max(0, dot(tempbloom.xyz, 0.333) - fAnamFlareThreshold);
		tempbloom.xyz = max(0, tempbloom.xyz - fBloomThreshold); 
		bloom += tempbloom;
	}

	bloom *= 0.25;
}
void BloomPass1(float4 vpos : SV_Position, float2 texcoord : TEXCOORD, out float4 bloom : SV_Target)
{
	bloom = 0.0;

	const float2 offset[8] = {
		float2(1.0, 1.0),
		float2(0.0, -1.0),
		float2(-1.0, 1.0),
		float2(-1.0, -1.0),
		float2(0.0, 1.0),
		float2(0.0, -1.0),
		float2(1.0, 0.0),
		float2(-1.0, 0.0)
	};

	for (int i = 0; i < 8; i++)
	{
		float2 bloomuv = offset[i] * ReShade::PixelSize * 4;
		bloomuv += texcoord;
		bloom += tex2Dlod(SamplerBloom1, float4(bloomuv, 0, 0));
	}

	bloom *= 0.125;
}
void BloomPass2(float4 vpos : SV_Position, float2 texcoord : TEXCOORD, out float4 bloom : SV_Target)
{
	bloom = 0.0;

	const float2 offset[8] = {
		float2(0.707, 0.707),
		float2(0.707, -0.707),
		float2(-0.707, 0.707),
		float2(-0.707, -0.707),
		float2(0.0, 1.0),
		float2(0.0, -1.0),
		float2(1.0, 0.0),
		float2(-1.0, 0.0)
	};

	for (int i = 0; i < 8; i++)
	{
		float2 bloomuv = offset[i] * ReShade::PixelSize * 8;
		bloomuv += texcoord;
		bloom += tex2Dlod(SamplerBloom2, float4(bloomuv, 0, 0));
	}

	bloom *= 0.5; // brighten up the sample, it will lose brightness in H/V Gaussian blur
}
void BloomPass3(float4 vpos : SV_Position, float2 texcoord : TEXCOORD, out float4 bloom : SV_Target)
{
	bloom = GaussBlur22(texcoord.xy, SamplerBloom3, 16, 0, 0);
	bloom.w *= fAnamFlareAmount;
	bloom.xyz *= fBloomAmount;
}
void BloomPass4(float4 vpos : SV_Position, float2 texcoord : TEXCOORD, out float4 bloom : SV_Target)
{
	bloom.xyz = GaussBlur22(texcoord, SamplerBloom4, 16, 0, 1).xyz * 2.5;	
	bloom.w = GaussBlur22(texcoord, SamplerBloom4, 32 * fAnamFlareWideness, 0, 0).w * 2.5; // to have anamflare texture (bloom.w) avoid vertical blur
}

void LensFlarePass0(float4 vpos : SV_Position, float2 texcoord : TEXCOORD, out float4 lens : SV_Target)
{
	lens = 0;

	// Lenz
	if (bLenzEnable)
	{
		const float3 lfoffset[19] = {
			float3(0.9, 0.01, 4),
			float3(0.7, 0.25, 25),
			float3(0.3, 0.25, 15),
			float3(1, 1.0, 5),
			float3(-0.15, 20, 1),
			float3(-0.3, 20, 1),
			float3(6, 6, 6),
			float3(7, 7, 7),
			float3(8, 8, 8),
			float3(9, 9, 9),
			float3(0.24, 1, 10),
			float3(0.32, 1, 10),
			float3(0.4, 1, 10),
			float3(0.5, -0.5, 2),
			float3(2, 2, -5),
			float3(-5, 0.2, 0.2),
			float3(20, 0.5, 0),
			float3(0.4, 1, 10),
			float3(0.00001, 10, 20)
		};
		const float3 lffactors[19] = {
			float3(1.5, 1.5, 0),
			float3(0, 1.5, 0),
			float3(0, 0, 1.5),
			float3(0.2, 0.25, 0),
			float3(0.15, 0, 0),
			float3(0, 0, 0.15),
			float3(1.4, 0, 0),
			float3(1, 1, 0),
			float3(0, 1, 0),
			float3(0, 0, 1.4),
			float3(1, 0.3, 0),
			float3(1, 1, 0),
			float3(0, 2, 4),
			float3(0.2, 0.1, 0),
			float3(0, 0, 1),
			float3(1, 1, 0),
			float3(1, 1, 0),
			float3(0, 0, 0.2),
			float3(0.012,0.313,0.588)
		};

		float2 lfcoord = 0;
		float3 lenstemp = 0;
		float2 distfact = texcoord.xy - 0.5;
		distfact.x *= ReShade::AspectRatio;

		for (int i = 0; i < 19; i++)
		{
			lfcoord.xy = lfoffset[i].x * distfact;
			lfcoord.xy *= pow(2.0 * length(distfact), lfoffset[i].y * 3.5);
			lfcoord.xy *= lfoffset[i].z;
			lfcoord.xy = 0.5 - lfcoord.xy;
			float2 tempfact = (lfcoord.xy - 0.5) * 2;
			float templensmult = clamp(1.0 - dot(tempfact, tempfact), 0, 1);
			float3 lenstemp1 = dot(tex2Dlod(ReShade::BackBuffer, float4(lfcoord.xy, 0, 1)).rgb, 0.333);

#if LENZ_DEPTH_CHECK
			float templensdepth = tex2D(ReShade::DepthBuffer, lfcoord.xy).x;
			if (templensdepth < 0.99999)
				lenstemp1 = 0;
#endif

			lenstemp1 = max(0, lenstemp1.xyz - fLenzThreshold);
			lenstemp1 *= lffactors[i] * templensmult;

			lenstemp += lenstemp1;
		}

		lens.rgb += lenstemp * fLenzIntensity;
	}

	// Chapman Lens
	if (bChapFlareEnable)
	{
		float2 sample_vector = (float2(0.5, 0.5) - texcoord.xy) * fChapFlareDispersal;
		float2 halo_vector = normalize(sample_vector) * fChapFlareSize;

		float3 chaplens = GetDistortedTex(ReShade::BackBuffer, texcoord.xy + halo_vector, halo_vector, fChapFlareCA * 2.5f).rgb;

		for (int j = 0; j < iChapFlareCount; ++j)
		{
			float2 foffset = sample_vector * float(j);
			chaplens += GetDistortedTex(ReShade::BackBuffer, texcoord.xy + foffset, foffset, fChapFlareCA).rgb;
		}

		chaplens *= 1.0 / iChapFlareCount;
		lens.xyz += chaplens;
	}

	// Godrays
	if (bGodrayEnable)
	{
		const float2 ScreenLightPos = float2(0.5, 0.5);
		float2 texcoord2 = texcoord;
		float2 deltaTexCoord = (texcoord2 - ScreenLightPos);
		deltaTexCoord *= 1.0 / (float)iGodraySamples * fGodrayDensity;

		float illuminationDecay = 1.0;

		for (int g = 0; g < iGodraySamples; g++)
		{
			texcoord2 -= deltaTexCoord;;
			float4 sample2 = tex2Dlod(ReShade::BackBuffer, float4(texcoord2, 0, 0));
			float sampledepth = tex2Dlod(ReShade::DepthBuffer, float4(texcoord2, 0, 0)).x;
			sample2.w = saturate(dot(sample2.xyz, 0.3333) - fGodrayThreshold);
			sample2.r *= 1.00;
			sample2.g *= 0.95;
			sample2.b *= 0.85;
			sample2 *= illuminationDecay * fGodrayWeight;
#if GODRAY_DEPTH_CHECK == 1
			if (sampledepth > 0.99999)
				lens.rgb += sample2.xyz * sample2.w;
#else
			lens.rgb += sample2.xyz * sample2.w;
#endif
			illuminationDecay *= fGodrayDecay;
		}
	}

	// Anamorphic flare
	if (bAnamFlareEnable)
	{
		float3 anamFlare = 0;
		const float gaussweight[5] = { 0.2270270270, 0.1945945946, 0.1216216216, 0.0540540541, 0.0162162162 };

		for (int z = -4; z < 5; z++)
		{
			anamFlare += GetAnamorphicSample(0, texcoord.xy + float2(0, z * ReShade::PixelSize.y * 2), fFlareBlur) * fFlareTint * gaussweight[abs(z)];
		}

		lens.xyz += anamFlare * fFlareIntensity;
	}
}
void LensFlarePass1(float4 vpos : SV_Position, float2 texcoord : TEXCOORD, out float4 lens : SV_Target)
{
	lens = GaussBlur22(texcoord, SamplerLensFlare1, 2, 0, 1);
}
void LensFlarePass2(float4 vpos : SV_Position, float2 texcoord : TEXCOORD, out float4 lens : SV_Target)
{
	lens = GaussBlur22(texcoord, SamplerLensFlare2, 2, 0, 0);
}

void LightingCombine(float4 vpos : SV_Position, float2 texcoord : TEXCOORD, out float4 color : SV_Target)
{
	color = tex2D(ReShade::BackBuffer, texcoord);

	// Bloom
	float3 colorbloom = 0;
	colorbloom += tex2D(SamplerBloom3, texcoord).rgb * 1.0;
	colorbloom += tex2D(SamplerBloom5, texcoord).rgb * 9.0;
	colorbloom *= 0.1;
	colorbloom = saturate(colorbloom);
	float colorbloomgray = dot(colorbloom, 0.333);
	colorbloom = lerp(colorbloomgray, colorbloom, fBloomSaturation);
	colorbloom *= fBloomTint;

	if (iBloomMixmode == 0)
		color.rgb += colorbloom;
	else if (iBloomMixmode == 1)
		color.rgb = 1 - (1 - color.rgb) * (1 - colorbloom);
	else if (iBloomMixmode == 2)
		color.rgb = max(0.0f, max(color.rgb, lerp(color.rgb, (1 - (1 - saturate(colorbloom)) * (1 - saturate(colorbloom))), 1.0)));
	else if (iBloomMixmode == 3)
		color.rgb = max(color.rgb, colorbloom);

	// Anamorphic flare
	if (bAnamFlareEnable)
	{
		float3 anamflare = tex2D(SamplerBloom5, texcoord.xy).w * 2 * fAnamFlareColor;
		anamflare = max(anamflare, 0.0);
		color.rgb += pow(anamflare, 1.0 / fAnamFlareCurve);
	}

	// Lens dirt
	if (bLensdirtEnable)
	{
		float lensdirtmult = dot(tex2D(SamplerBloom5, texcoord).rgb, 0.333);
		float3 dirttex = tex2D(SamplerDirt, texcoord).rgb;
		float3 lensdirt = dirttex * lensdirtmult * fLensdirtIntensity;

		lensdirt = lerp(dot(lensdirt.xyz, 0.333), lensdirt.xyz, fLensdirtSaturation);

		if (iLensdirtMixmode == 0)
			color.rgb += lensdirt;
		else if (iLensdirtMixmode == 1)
			color.rgb = 1 - (1 - color.rgb) * (1 - lensdirt);
		else if (iLensdirtMixmode == 2)
			color.rgb = max(0.0f, max(color.rgb, lerp(color.rgb, (1 - (1 - saturate(lensdirt)) * (1 - saturate(lensdirt))), 1.0)));
		else if (iLensdirtMixmode == 3)
			color.rgb = max(color.rgb, lensdirt);
	}

	// Lens flares
	if (bAnamFlareEnable || bLenzEnable || bGodrayEnable || bChapFlareEnable)
	{
		float3 lensflareSample = tex2D(SamplerLensFlare1, texcoord.xy).rgb, lensflareMask;
		lensflareMask  = tex2D(SamplerSprite, texcoord + float2( 0.5,  0.5) * ReShade::PixelSize).rgb;
		lensflareMask += tex2D(SamplerSprite, texcoord + float2(-0.5,  0.5) * ReShade::PixelSize).rgb;
		lensflareMask += tex2D(SamplerSprite, texcoord + float2( 0.5, -0.5) * ReShade::PixelSize).rgb;
		lensflareMask += tex2D(SamplerSprite, texcoord + float2(-0.5, -0.5) * ReShade::PixelSize).rgb;

		color.rgb += lensflareMask * 0.25 * lensflareSample;
	}
}

technique BloomAndLensFlares
{
	pass BloomPass0
	{
		VertexShader = PostProcessVS;
		PixelShader = BloomPass0;
		RenderTarget = texBloom1;
	}
	pass BloomPass1
	{
		VertexShader = PostProcessVS;
		PixelShader = BloomPass1;
		RenderTarget = texBloom2;
	}
	pass BloomPass2
	{
		VertexShader = PostProcessVS;
		PixelShader = BloomPass2;
		RenderTarget = texBloom3;
	}
	pass BloomPass3
	{
		VertexShader = PostProcessVS;
		PixelShader = BloomPass3;
		RenderTarget = texBloom4;
	}
	pass BloomPass4
	{
		VertexShader = PostProcessVS;
		PixelShader = BloomPass4;
		RenderTarget = texBloom5;
	}

	pass LensFlarePass0
	{
		VertexShader = PostProcessVS;
		PixelShader = LensFlarePass0;
		RenderTarget = texLensFlare1;
	}
	pass LensFlarePass1
	{
		VertexShader = PostProcessVS;
		PixelShader = LensFlarePass1;
		RenderTarget = texLensFlare2;
	}
	pass LensFlarePass2
	{
		VertexShader = PostProcessVS;
		PixelShader = LensFlarePass2;
		RenderTarget = texLensFlare1;
	}

	pass LightingCombine
	{
		VertexShader = PostProcessVS;
		PixelShader = LightingCombine;
	}
}
