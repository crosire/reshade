/**
 * Copyright (C) 2015 Ganossa (mediehawk@gmail.com)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software with restriction, including without limitation the rights to
 * use and/or sell copies of the Software, and to permit persons to whom the Software
 * is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and the permission notices (this and below) shall
 * be included in all copies or substantial portions of the Software.
 *
 * Permission needs to be specifically granted by the author of the software to any
 * person obtaining a copy of this software and associated documentation files
 * (the "Software"), to deal in the Software without restriction, including without
 * limitation the rights to copy, modify, merge, publish, distribute, and/or
 * sublicense the Software, and subject to the following conditions:
 *
 * The above copyright notice and the permission notices (this and above) shall
 * be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "ReShadeUI.fxh"

uniform bool alDebug <
	ui_tooltip = "Activates debug mode of AL, upper bar shows detected light, lower bar shows adaptation";
> = false;
uniform float alInt < __UNIFORM_SLIDER_FLOAT1
	ui_min = 0.0; ui_max = 20.0;
	ui_tooltip = "Base intensity of AL";
> = 10.15;
uniform float alThreshold < __UNIFORM_SLIDER_FLOAT1
	ui_min = 0.0; ui_max = 100.0;
	ui_tooltip = "Reduces intensity for not bright light";
> = 15.00;

uniform bool AL_Adaptation <
	ui_tooltip = "Activates adaptation algorithm";
> = true;
uniform float alAdapt < __UNIFORM_SLIDER_FLOAT1
	ui_min = 0.0; ui_max = 4.0;
	ui_tooltip = "Intensity of AL correction for bright light";
> = 0.70;
uniform float alAdaptBaseMult < __UNIFORM_SLIDER_FLOAT1
	ui_min = 0.0; ui_max = 4.0;
	ui_tooltip = "Multiplier for adaption applied to the original image";
> = 1.00;
uniform int alAdaptBaseBlackLvL < __UNIFORM_SLIDER_INT1
	ui_min = 0; ui_max = 4;
	ui_tooltip = "Distinction level of black and white (lower => less distinct)";
> = 2;

uniform bool AL_Dirt <
> = true;
uniform bool AL_DirtTex <
	ui_tooltip = "Defines if dirt texture is used as overlay";
> = false;
uniform bool AL_Vibrance <
	ui_tooltip = "Vibrance of dirt effect";
> = false;
uniform int AL_Adaptive <
	ui_type = "combo";
	ui_min = 0; ui_max = 2;
	ui_items = "Warm\0Cold\0Light Dependent\0";
> = 0;
uniform float alDirtInt < __UNIFORM_SLIDER_FLOAT1
	ui_min = 0.0; ui_max = 2.0;
	ui_tooltip = "Intensity of dirt effect";
> = 1.0;
uniform float alDirtOVInt < __UNIFORM_SLIDER_FLOAT1
	ui_min = 0.0; ui_max = 2.0;
	ui_tooltip = "Intensity of colored dirt effect";
> = 1.0;
uniform bool AL_Lens <
	ui_tooltip = "Lens effect based on AL";
> = false;
uniform float alLensThresh < __UNIFORM_SLIDER_FLOAT1
	ui_min = 0.0; ui_max = 1.0;
	ui_tooltip = "Reduces intensity of lens effect for not bright light";
> = 0.5;
uniform float alLensInt < __UNIFORM_SLIDER_FLOAT1
	ui_min = 0.0; ui_max = 10.0;
	ui_tooltip = "Intensity of lens effect";
> = 2.0;

#include "ReShade.fxh"

uniform float2 AL_t < source = "pingpong"; min = 0.0f; max = 6.28f; step = float2(0.1f, 0.2f); >;

#define GEMFX_PIXEL_SIZE float2(1.0f / (BUFFER_WIDTH / 16.0f), 1.0f / (BUFFER_HEIGHT / 16.0f))

texture alInTex  { Width = BUFFER_WIDTH / 16; Height = BUFFER_HEIGHT / 16; Format = RGBA32F; };
texture alOutTex { Width = BUFFER_WIDTH / 16; Height = BUFFER_HEIGHT / 16; Format = RGBA32F; };
texture detectIntTex { Width = 32; Height = 32; Format = RGBA8; };
sampler detectIntColor { Texture = detectIntTex; };
texture detectLowTex { Width = 1; Height = 1; Format = RGBA8; };
sampler detectLowColor { Texture = detectLowTex; };

texture dirtTex    < source = "Dirt.png";    > { Width = 1920; Height = 1080; MipLevels = 1; Format = RGBA8; };
texture dirtOVRTex < source = "DirtOVR.png"; > { Width = 1920; Height = 1080; MipLevels = 1; Format = RGBA8; };
texture dirtOVBTex < source = "DirtOVB.png"; > { Width = 1920; Height = 1080; MipLevels = 1; Format = RGBA8; };
texture lensDBTex  < source = "LensDB.png";  > { Width = 1920; Height = 1080; MipLevels = 1; Format = RGBA8; };
texture lensDB2Tex < source = "LensDB2.png"; > { Width = 1920; Height = 1080; MipLevels = 1; Format = RGBA8; };
texture lensDOVTex < source = "LensDOV.png"; > { Width = 1920; Height = 1080; MipLevels = 1; Format = RGBA8; };
texture lensDUVTex < source = "LensDUV.png"; > { Width = 1920; Height = 1080; MipLevels = 1; Format = RGBA8; };

sampler alInColor { Texture = alInTex; };
sampler alOutColor { Texture = alOutTex; };
sampler dirtSampler { Texture = dirtTex; };
sampler dirtOVRSampler { Texture = dirtOVRTex; };
sampler dirtOVBSampler { Texture = dirtOVBTex; };
sampler lensDBSampler { Texture = lensDBTex; };
sampler lensDB2Sampler { Texture = lensDB2Tex; };
sampler lensDOVSampler { Texture = lensDOVTex; };
sampler lensDUVSampler { Texture = lensDUVTex; };

void PS_AL_DetectInt(float4 vpos : SV_Position, float2 texcoord : TEXCOORD, out float4 detectInt : SV_Target0)
{
	detectInt = tex2D(ReShade::BackBuffer, texcoord);
}
void PS_AL_DetectLow(float4 vpos : SV_Position, float2 texcoord : TEXCOORD, out float4 detectLow : SV_Target0)
{
	detectLow = 0;

	if (texcoord.x != 0.5 && texcoord.y != 0.5)
		discard;

	[loop]
	for (float i = 0.0; i <= 1; i += 0.03125)
	{
		[unroll]
		for (float j = 0.0; j <= 1; j += 0.03125)
		{
			detectLow.xyz += tex2D(detectIntColor, float2(i, j)).xyz;
		}
	}

	detectLow.xyz /= 32 * 32;
}
void PS_AL_DetectHigh(float4 vpos : SV_Position, float2 texcoord : TEXCOORD, out float4 x : SV_Target)
{
	x = tex2D(ReShade::BackBuffer, texcoord);
	x = float4(x.rgb * pow(abs(max(x.r, max(x.g, x.b))), 2.0), 1.0f);

	float base = (x.r + x.g + x.b); base /= 3;
	
	float nR = (x.r * 2) - base;
	float nG = (x.g * 2) - base;
	float nB = (x.b * 2) - base;

	[flatten]
	if (nR < 0)
	{
		nG += nR / 2;
		nB += nR / 2;
		nR = 0;
	}
	[flatten]
	if (nG < 0)
	{
		nB += nG / 2;
		[flatten] if (nR > -nG / 2) nR += nG / 2; else nR = 0;
		nG = 0;
	}
	[flatten]
	if (nB < 0)
	{
		[flatten] if (nR > -nB / 2) nR += nB / 2; else nR = 0;
		[flatten] if (nG > -nB / 2) nG += nB / 2; else nG = 0;
		nB = 0;
	}

	[flatten]
	if (nR > 1)
	{
		nG += (nR - 1) / 2;
		nB += (nR - 1) / 2;
		nR = 1;
	}
	[flatten]
	if (nG > 1)
	{
		nB += (nG - 1) / 2;
		[flatten] if (nR + (nG - 1) < 1) nR += (nG - 1) / 2; else nR = 1;
		nG = 1;
	}
	[flatten]
	if (nB > 1)
	{
		[flatten] if (nR + (nB - 1) < 1) nR += (nB - 1) / 2; else nR = 1;
		[flatten] if (nG + (nB - 1) < 1) nG += (nB - 1) / 2; else nG = 1;
		nB = 1;
	}

	x.r = nR; x.g = nG; x.b = nB;
}

void PS_AL_HGB(float4 vpos : SV_Position, float2 texcoord : TEXCOORD, out float4 hgb : SV_Target)
{
	const float sampleOffsets[5] = { 0.0, 2.4347826, 4.3478260, 6.2608695, 8.1739130 };
	const float sampleWeights[5] = { 0.16818994, 0.27276957, 0.111690125, 0.024067905, 0.0021112196 };

	hgb = tex2D(alInColor, texcoord) * sampleWeights[0];
	hgb = float4(max(hgb.rgb - alThreshold, 0.0), hgb.a);
	float step = 1.08 + (AL_t.x / 100) * 0.02;

	[flatten]
	if ((texcoord.x + sampleOffsets[1] * GEMFX_PIXEL_SIZE.x) < 1.05)
		hgb += tex2D(alInColor, texcoord + float2(sampleOffsets[1] * GEMFX_PIXEL_SIZE.x, 0.0)) * sampleWeights[1] * step;
	[flatten]
	if ((texcoord.x - sampleOffsets[1] * GEMFX_PIXEL_SIZE.x) > -0.05)
		hgb += tex2D(alInColor, texcoord - float2(sampleOffsets[1] * GEMFX_PIXEL_SIZE.x, 0.0)) * sampleWeights[1] * step;

	[flatten]
	if ((texcoord.x + sampleOffsets[2] * GEMFX_PIXEL_SIZE.x) < 1.05)
		hgb += tex2D(alInColor, texcoord + float2(sampleOffsets[2] * GEMFX_PIXEL_SIZE.x, 0.0)) * sampleWeights[2] * step;
	[flatten]
	if ((texcoord.x - sampleOffsets[2] * GEMFX_PIXEL_SIZE.x) > -0.05)
		hgb += tex2D(alInColor, texcoord - float2(sampleOffsets[2] * GEMFX_PIXEL_SIZE.x, 0.0)) * sampleWeights[2] * step;

	[flatten]
	if ((texcoord.x + sampleOffsets[3] * GEMFX_PIXEL_SIZE.x) < 1.05)
		hgb += tex2D(alInColor, texcoord + float2(sampleOffsets[3] * GEMFX_PIXEL_SIZE.x, 0.0)) * sampleWeights[3] * step;
	[flatten]
	if ((texcoord.x - sampleOffsets[3] * GEMFX_PIXEL_SIZE.x) > -0.05)
		hgb += tex2D(alInColor, texcoord - float2(sampleOffsets[3] * GEMFX_PIXEL_SIZE.x, 0.0)) * sampleWeights[3] * step;

	[flatten]
	if ((texcoord.x + sampleOffsets[4] * GEMFX_PIXEL_SIZE.x) < 1.05)
		hgb += tex2D(alInColor, texcoord + float2(sampleOffsets[4] * GEMFX_PIXEL_SIZE.x, 0.0)) * sampleWeights[4] * step;
	[flatten]
	if ((texcoord.x - sampleOffsets[4] * GEMFX_PIXEL_SIZE.x) > -0.05)
		hgb += tex2D(alInColor, texcoord - float2(sampleOffsets[4] * GEMFX_PIXEL_SIZE.x, 0.0)) * sampleWeights[4] * step;
}
void PS_AL_VGB(float4 vpos : SV_Position, float2 texcoord : TEXCOORD, out float4 vgb : SV_Target)
{
	const float sampleOffsets[5] = { 0.0, 2.4347826, 4.3478260, 6.2608695, 8.1739130 };
	const float sampleWeights[5] = { 0.16818994, 0.27276957, 0.111690125, 0.024067905, 0.0021112196 };

	vgb = tex2D(alOutColor, texcoord) * sampleWeights[0];
	vgb = float4(max(vgb.rgb - alThreshold, 0.0), vgb.a);
	float step = 1.08 + (AL_t.x / 100) * 0.02;
	
	[flatten]
	if ((texcoord.y + sampleOffsets[1] * GEMFX_PIXEL_SIZE.y) < 1.05)
		vgb += tex2D(alOutColor, texcoord + float2(0.0, sampleOffsets[1] * GEMFX_PIXEL_SIZE.y)) * sampleWeights[1] * step;
	[flatten]
	if ((texcoord.y - sampleOffsets[1] * GEMFX_PIXEL_SIZE.y) > -0.05)
		vgb += tex2D(alOutColor, texcoord - float2(0.0, sampleOffsets[1] * GEMFX_PIXEL_SIZE.y)) * sampleWeights[1] * step;
	
	[flatten]
	if ((texcoord.y + sampleOffsets[2] * GEMFX_PIXEL_SIZE.y) < 1.05)
		vgb += tex2D(alOutColor, texcoord + float2(0.0, sampleOffsets[2] * GEMFX_PIXEL_SIZE.y)) * sampleWeights[2] * step;
	[flatten]
	if ((texcoord.y - sampleOffsets[2] * GEMFX_PIXEL_SIZE.y) > -0.05)
		vgb += tex2D(alOutColor, texcoord - float2(0.0, sampleOffsets[2] * GEMFX_PIXEL_SIZE.y)) * sampleWeights[2] * step;

	[flatten]
	if ((texcoord.y + sampleOffsets[3] * GEMFX_PIXEL_SIZE.y) < 1.05)
		vgb += tex2D(alOutColor, texcoord + float2(0.0, sampleOffsets[3] * GEMFX_PIXEL_SIZE.y)) * sampleWeights[3] * step;
	[flatten]
	if ((texcoord.y - sampleOffsets[3] * GEMFX_PIXEL_SIZE.y) > -0.05)
		vgb += tex2D(alOutColor, texcoord - float2(0.0, sampleOffsets[3] * GEMFX_PIXEL_SIZE.y)) * sampleWeights[3] * step;

	[flatten]
	if ((texcoord.y + sampleOffsets[4] * GEMFX_PIXEL_SIZE.y) < 1.05)
		vgb += tex2D(alOutColor, texcoord + float2(0.0, sampleOffsets[4] * GEMFX_PIXEL_SIZE.y)) * sampleWeights[4] * step;
	[flatten]
	if ((texcoord.y - sampleOffsets[4] * GEMFX_PIXEL_SIZE.y) > -0.05)
		vgb += tex2D(alOutColor, texcoord - float2(0.0, sampleOffsets[4] * GEMFX_PIXEL_SIZE.y)) * sampleWeights[4] * step;
}

float4 PS_AL_Magic(float4 vpos : SV_Position, float2 texcoord : TEXCOORD) : SV_Target
{
	float4 base = tex2D(ReShade::BackBuffer, texcoord);
	float4 high = tex2D(alInColor, texcoord);
	float adapt = 0;

#if __RENDERER__ < 0xa000 && !__RESHADE_PERFORMANCE_MODE__
	[flatten]
#endif
	if (AL_Adaptation)
	{
		//DetectLow	
		float4 detectLow = tex2D(detectLowColor, 0.5) / 4.215;
		float low = sqrt(0.241 * detectLow.r * detectLow.r + 0.691 * detectLow.g * detectLow.g + 0.068 * detectLow.b * detectLow.b);
		//.DetectLow

		low = pow(low * 1.25f, 2);
		adapt = low * (low + 1.0f) * alAdapt * alInt * 5.0f;

		if (alDebug)
		{
			float mod = (texcoord.x * 1000.0f) % 1.001f;
			//mod = abs(mod - texcoord.x / 4.0f);

			if (texcoord.y < 0.01f && (texcoord.x < low * 10f && mod < 0.3f))
				return float4(1f, 0.5f, 0.3f, 0f);

			if (texcoord.y > 0.01f && texcoord.y < 0.02f && (texcoord.x < adapt / (alInt * 1.5) && mod < 0.3f))
				return float4(0.2f, 1f, 0.5f, 0f);
		}
	}

	high = min(0.0325f, high) * 1.15f;
	float4 highOrig = high;

	float2 flipcoord = 1.0f - texcoord;
	float4 highFlipOrig = tex2D(alInColor, flipcoord);
	highFlipOrig = min(0.03f, highFlipOrig) * 1.15f;

	float4 highFlip = highFlipOrig;
	float4 highLensSrc = high;

#if __RENDERER__ < 0xa000 && !__RESHADE_PERFORMANCE_MODE__
	[flatten]
#endif
	if (AL_Dirt)
	{
		float4 dirt = tex2D(dirtSampler, texcoord);
		float4 dirtOVR = tex2D(dirtOVRSampler, texcoord);
		float4 dirtOVB = tex2D(dirtOVBSampler, texcoord);

		float maxhigh = max(high.r, max(high.g, high.b));
		float threshDiff = maxhigh - 3.2f;

		[flatten]
		if (threshDiff > 0)
		{
			high.r = (high.r / maxhigh) * 3.2f;
			high.g = (high.g / maxhigh) * 3.2f;
			high.b = (high.b / maxhigh) * 3.2f;
		}

		float4 highDirt = AL_DirtTex ? highOrig * dirt * alDirtInt : highOrig * high * alDirtInt;

		if (AL_Vibrance)
		{
			highDirt *= 1.0f + 0.5f * sin(AL_t.x);
		}

		float highMix = highOrig.r + highOrig.g + highOrig.b;
		float red = highOrig.r / highMix;
		float green = highOrig.g / highMix;
		float blue = highOrig.b / highMix;
		highOrig = highOrig + highDirt;

		if (AL_Adaptive == 2)
		{
			high = high + high * dirtOVR * alDirtOVInt * green;
			high = high + highDirt;
			high = high + highOrig * dirtOVB * alDirtOVInt * blue;
			high = high + highOrig * dirtOVR * alDirtOVInt* red;
		}
		else if (AL_Adaptive == 1)
		{
			high = high + highDirt;
			high = high + highOrig * dirtOVB * alDirtOVInt;
		}
		else
		{
			high = high + highDirt;
			high = high + highOrig * dirtOVR * alDirtOVInt;
		}

		highLensSrc = high * 85f * pow(1.25f - (abs(texcoord.x - 0.5f) + abs(texcoord.y - 0.5f)), 2);
	}

	float origBright = max(highLensSrc.r, max(highLensSrc.g, highLensSrc.b));
	float maxOrig = max((1.8f * alLensThresh) - pow(origBright * (0.5f - abs(texcoord.x - 0.5f)), 4), 0.0f);
	float smartWeight = maxOrig * max(abs(flipcoord.x - 0.5f), 0.3f * abs(flipcoord.y - 0.5f)) * (2.2 - 1.2 * (abs(flipcoord.x - 0.5f))) * alLensInt;
	smartWeight = min(0.85f, max(0, AL_Adaptation ? smartWeight - adapt : smartWeight));

#if __RENDERER__ < 0xa000 && !__RESHADE_PERFORMANCE_MODE__
	[flatten]
#endif
	if (AL_Lens)
	{
		float4 lensDB = tex2D(lensDBSampler, texcoord);
		float4 lensDB2 = tex2D(lensDB2Sampler, texcoord);
		float4 lensDOV = tex2D(lensDOVSampler, texcoord);
		float4 lensDUV = tex2D(lensDUVSampler, texcoord);

		float4 highLens = highFlip * lensDB * 0.7f * smartWeight;
		high += highLens;

		highLens = highFlipOrig * lensDUV * 1.15f * smartWeight;
		highFlipOrig += highLens;
		high += highLens;

		highLens = highFlipOrig * lensDB2 * 0.7f * smartWeight;
		highFlipOrig += highLens;
		high += highLens;

		highLens = highFlipOrig * lensDOV * 1.15f * smartWeight / 2f + highFlipOrig * smartWeight / 2f;
		highFlipOrig += highLens;
		high += highLens;
	}

	float dither = 0.15 * (1.0 / (pow(2, 10.0) - 1.0));
	dither = lerp(2.0 * dither, -2.0 * dither, frac(dot(texcoord, ReShade::ScreenSize * float2(1.0 / 16.0, 10.0 / 36.0)) + 0.25));

	if (all(base.xyz == 1.0))
	{
		return 1.0;
	}

#if __RENDERER__ < 0xa000 && !__RESHADE_PERFORMANCE_MODE__
	[flatten]
#endif
	if (AL_Adaptation)
	{
		base.xyz *= max(0.0f, (1.0f - adapt * 0.75f * alAdaptBaseMult * pow(abs(1.0f - (base.x + base.y + base.z) / 3), alAdaptBaseBlackLvL)));
		float4 highSampleMix = (1.0 - ((1.0 - base) * (1.0 - high * 1.0))) + dither;
		float4 baseSample = lerp(base, highSampleMix, max(0.0f, alInt - adapt));
		float baseSampleMix = baseSample.r + baseSample.g + baseSample.b;
		return baseSampleMix > 0.008 ? baseSample : lerp(base, highSampleMix, max(0.0f, (alInt - adapt) * 0.85f) * baseSampleMix);
	}
	else
	{
		float4 highSampleMix = (1.0 - ((1.0 - base) * (1.0 - high * 1.0))) + dither + adapt;
		float4 baseSample = lerp(base, highSampleMix, alInt);
		float baseSampleMix = baseSample.r + baseSample.g + baseSample.b;
		return baseSampleMix > 0.008 ? baseSample : lerp(base, highSampleMix, max(0.0f, alInt * 0.85f) * baseSampleMix);
	}
}

technique AmbientLight
{
	pass AL_DetectInt
	{
		VertexShader = PostProcessVS;
		PixelShader = PS_AL_DetectInt;
		RenderTarget = detectIntTex;
	}
	pass AL_DetectLow
	{
		VertexShader = PostProcessVS;
		PixelShader = PS_AL_DetectLow;
		RenderTarget = detectLowTex;
	}
	pass AL_DetectHigh
	{
		VertexShader = PostProcessVS;
		PixelShader = PS_AL_DetectHigh;
		RenderTarget = alInTex;
	}

#define PASS_AL_H(i) \
	pass AL_H##i \
	{ \
		VertexShader = PostProcessVS; \
		PixelShader = PS_AL_HGB; \
		RenderTarget = alOutTex; \
	}
#define PASS_AL_V(i) \
	pass AL_V##i \
	{ \
		VertexShader = PostProcessVS; \
		PixelShader = PS_AL_VGB; \
		RenderTarget = alInTex; \
	}

	PASS_AL_H(1)
	PASS_AL_V(1)
	PASS_AL_H(2)
	PASS_AL_V(2)
	PASS_AL_H(3)
	PASS_AL_V(3)
	PASS_AL_H(4)
	PASS_AL_V(4)
	PASS_AL_H(5)
	PASS_AL_V(5)
	PASS_AL_H(6)
	PASS_AL_V(6)
	PASS_AL_H(7)
	PASS_AL_V(7)
	PASS_AL_H(8)
	PASS_AL_V(8)
	PASS_AL_H(9)
	PASS_AL_V(9)
	PASS_AL_H(10)
	PASS_AL_V(10)
	PASS_AL_H(11)
	PASS_AL_V(11)
	PASS_AL_H(12)
	PASS_AL_V(12)

	pass AL_Magic
	{
		VertexShader = PostProcessVS;
		PixelShader = PS_AL_Magic;
	}
}
