/*
	Header file for ArcaneBloom by luluco250

	This file exposes some of the interals of the
	shader so you can use it in your own, simply
	by using '#include "ArcaneBloom.fxh"' at the
	top of the shader source file.
*/

#pragma once

/*
	Here are macro settings that affect both how
	Arcane Bloom itself behaves and how it can be
	used by other shaders.
*/

#ifndef ARCANE_BLOOM_USE_ADAPTATION
#define ARCANE_BLOOM_USE_ADAPTATION 1
#endif

namespace ArcaneBloom {
	/*
		Textures are defined here as "private".
		Please do not write to them in your techniques
		as it would lead to unexpected behavior in other
		shaders that also make use of them.

		Just use the samplers to read from them.
		If you need a custom sampler, use "ArcaneBloom::_::"
		before the texture name.
	*/
	namespace _ {
		#define DEF_BLOOM_TEX(NAME, DIV) \
		texture2D tArcaneBloom_##NAME { \
			Width  = BUFFER_WIDTH / DIV; \
			Height = BUFFER_HEIGHT / DIV; \
			Format = RGBA16F; \
		}

		DEF_BLOOM_TEX(Bloom0, 2);
		DEF_BLOOM_TEX(Bloom1, 4);
		DEF_BLOOM_TEX(Bloom2, 8);
		DEF_BLOOM_TEX(Bloom3, 16);
		DEF_BLOOM_TEX(Bloom4, 32);
		//DEF_BLOOM_TEX(Bloom5, 64);

		#undef DEF_BLOOM_TEX

		#if ARCANE_BLOOM_USE_ADAPTATION
		texture2D tArcaneBloom_Adapt {
			Format = R32F;
		};
		#endif
	}

	#define DEF_BLOOM_SAMPLER(NAME) \
	sampler2D s##NAME { \
		Texture = _::tArcaneBloom_##NAME; \
	}

	DEF_BLOOM_SAMPLER(Bloom0);
	DEF_BLOOM_SAMPLER(Bloom1);
	DEF_BLOOM_SAMPLER(Bloom2);
	DEF_BLOOM_SAMPLER(Bloom3);
	DEF_BLOOM_SAMPLER(Bloom4);
	//DEF_BLOOM_SAMPLER(Bloom5);

	#undef DEF_BLOOM_SAMPLER

	#if ARCANE_BLOOM_USE_ADAPTATION
	sampler2D sAdapt {
		Texture   = _::tArcaneBloom_Adapt;
		MinFilter = POINT;
		MagFilter = POINT;
		MipFilter = POINT;
		AddressU  = CLAMP;
		AddressV  = CLAMP;
		AddressW  = CLAMP;
	};
	#endif

	  //===========//
	 // Constants //
	//===========//

	static const float cPI = 3.1415926535897932384626433832795;

	  //===========//
	 // Functions //
	//===========//

	float3 inv_reinhard(float3 color, float inv_max) {
		return (color / max(1.0 - color, inv_max));
	}

	float3 inv_reinhard_lum(float3 color, float inv_max) {
		float lum = max(color.r, max(color.g, color.b));
		return color * (lum / max(1.0 - lum, inv_max));
	}

	float3 reinhard(float3 color) {
		return color / (1.0 + color);
	}

	float3 box_blur(sampler2D sp, float2 uv, float2 ps) {
		return (tex2D(sp, uv - ps * 0.5).rgb +
		        tex2D(sp, uv + ps * 0.5).rgb +
		        tex2D(sp, uv + float2(-ps.x, ps.y) * 0.5).rgb +
		        tex2D(sp, uv + float2( ps.x,-ps.y) * 0.5).rgb) * 0.25;
	}

	// Strange using 'static' in a header file when
	// coming from a C/C++ background, but hey it works.
	static const int cGaussianSamples = 13;
	float get_weight(int i) {
		static const float weights[cGaussianSamples] = {
			0.017997,
			0.033159,
			0.054670,
			0.080657,
			0.106483,
			0.125794,
			0.132981,
			0.125794,
			0.106483,
			0.080657,
			0.054670,
			0.033159,
			0.017997
		};
		return weights[i];
	}

	float3 gaussian_blur(sampler2D sp, float2 uv, float2 dir) {
		float3 color = 0.0;
		uv -= dir * floor(cGaussianSamples * 0.5);

		[unroll]
		for (int i = 0; i < cGaussianSamples; ++i) {
			color += tex2D(sp, uv).rgb * get_weight(i);
			uv += dir;
		}

		return color;
	}

	float get_luma_linear(float3 c) {
		return dot(c, float3(0.2126, 0.7152, 0.0722));
	}

	float normal_distribution(float x, float mean, float variance) {
		float sigma = variance * variance;
		float a = 1.0 / sqrt(2.0 * cPI * sigma);
		float b = x - mean;
		b *= b;
		b /= 2.0 * sigma;

		return a * exp(-b);
	}
}
