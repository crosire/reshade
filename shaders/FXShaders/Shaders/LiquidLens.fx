#include "FXShaders/AspectRatio.fxh"
#include "FXShaders/Common.fxh"
#include "FXShaders/Convolution.fxh"
#include "FXShaders/Math.fxh"
#include "FXShaders/Tonemap.fxh"
#include "FXShaders/Transform.fxh"

#ifndef MAGIC_LENS_BLUR_SAMPLES
#define MAGIC_LENS_BLUR_SAMPLES 9
#endif

#ifndef MAGIC_LENS_DOWNSCALE
#define MAGIC_LENS_DOWNSCALE 4
#endif

namespace FXShaders { namespace LiquidLens
{

static const int BlurSamples = MAGIC_LENS_BLUR_SAMPLES;
static const int Downscale = MAGIC_LENS_DOWNSCALE;
static const float BaseFlareDownscale = 4.0;

FXSHADERS_WIP_WARNING();

uniform float Brightness
<
	ui_category = "Appearance";
	ui_type = "slider";
	ui_min = 0.0;
	ui_max = 1.0;
> = 0.1;

uniform float Saturation
<
	ui_category = "Appearance";
	ui_type = "slider";
	ui_min = 0.0;
	ui_max = 2.0;
> = 0.7;

uniform float Threshold
<
	ui_category = "Appearance";
	ui_type = "slider";
	ui_min = 0.0;
	ui_max = 1.0;
> = 0.95;

uniform int Tonemapper
<
	ui_category = "Appearance";
	ui_type = "combo";
	ui_items = FXSHADERS_TONEMAPPER_LIST;
> = Tonemap::Type::BakingLabACES;

uniform float BlurSize
<
	ui_category = "Gaussian Blur";
	ui_label = "Size";
	ui_type = "slider";
	ui_min = 0.0;
	ui_max = 1.0;
> = 1.0;

uniform float BlurSigma
<
	ui_category = "Gaussian Blur";
	ui_label = "Sigma";
	ui_type = "slider";
	ui_min = 0.0;
	ui_max = 1.0;
> = 0.5;

uniform float FisheyeAmount
<
	ui_category = "Fisheye Lens";
	ui_category_closed = true;
	ui_label = "Amount";
	ui_type = "slider";
	ui_min = -1.0;
	ui_max = 1.0;
> = -0.1;

uniform float FisheyeZoom
<
	ui_category = "Fisheye Lens";
	ui_category_closed = true;
	ui_label = "Zoom";
	ui_type = "slider";
	ui_min = -1.0;
	ui_max = 1.0;
> = 1.0;

uniform int FisheyeScaleType
<
	ui_category = "Fisheye Lens";
	ui_text = "Aspect Ratio Scaling";
	ui_label = " ";
	ui_type = "radio";
	ui_items = FXSHADERS_ASPECT_RATIO_SCALE_TYPE_LIST;
> = AspectRatio::ScaleType::Cover;

uniform float TintAmount
<
	ui_category = "Flares";
	ui_category_closed = true;
	ui_label = "Tinting Amount";
	ui_type = "slider";
	ui_min = 0.0;
	ui_max = 1.0;
> = 1.0;

#define _FLARE_TINT(id, defaultValue) \
uniform float4 Tint##id \
< \
	ui_category = "Flares"; \
	ui_label = "Tint " #id; \
	ui_type = "color"; \
> = defaultValue

_FLARE_TINT(1, float4(1.0, 0.0, 0.0, 1.0));
_FLARE_TINT(2, float4(1.0, 0.5, 0.0, 1.0));
_FLARE_TINT(3, float4(1.0, 1.0, 0.0, 1.0));
_FLARE_TINT(4, float4(0.0, 1.0, 0.0, 1.0));
_FLARE_TINT(5, float4(0.0, 1.0, 1.0, 1.0));
_FLARE_TINT(6, float4(0.0, 0.0, 1.0, 1.0));
_FLARE_TINT(7, float4(1.0, 0.0, 1.0, 1.0));

#undef _FLARE_TINT

#define _FLARE_SCALE(id, defaultValue) \
uniform float Scale##id \
< \
	ui_category = "Flares"; \
	ui_label = "Scale " #id; \
	ui_type = "slider"; \
	ui_min = -1.0; \
	ui_max = 1.0; \
> = defaultValue

_FLARE_SCALE(1, 0.01);
_FLARE_SCALE(2, 0.02);
_FLARE_SCALE(3, 0.03);
_FLARE_SCALE(4, 0.04);
_FLARE_SCALE(5, 0.05);
_FLARE_SCALE(6, 0.06);
_FLARE_SCALE(7, 0.07);

#undef _FLARE_SCALE

uniform bool ShowLens
<
	ui_category = "Debug";
	ui_category_closed = true;
	ui_label = "Show Lens Flare Texture";
> = false;

texture BackBufferTex : COLOR;

sampler BackBuffer
{
	Texture = BackBufferTex;
	SRGBTexture = true;
	AddressU = BORDER;
	AddressV = BORDER;
};

texture LensATex// <pooled = true;>
{
	Width = BUFFER_WIDTH / Downscale;
	Height = BUFFER_HEIGHT / Downscale;
	Format = RGBA16F;
};

sampler LensA
{
	Texture = LensATex;
	AddressU = BORDER;
	AddressV = BORDER;
};

texture LensBTex// <pooled = true;>
{
	Width = BUFFER_WIDTH / Downscale;
	Height = BUFFER_HEIGHT / Downscale;
	Format = RGBA16F;
};

sampler LensB
{
	Texture = LensBTex;
	AddressU = BORDER;
	AddressV = BORDER;
};

float4 PreparePS(float4 p : SV_POSITION, float2 uv : TEXCOORD) : SV_TARGET
{
	uv = ScaleCoord(1.0 - uv, BaseFlareDownscale);
	float4 color = tex2D(BackBuffer, uv);
	color.rgb = Tonemap::Inverse(Tonemapper, color.rgb);

	color.rgb = ApplySaturation(color.rgb, Saturation);
	color.rgb *= color.rgb >= Tonemap::Inverse(Tonemapper, Threshold).x;
	color.rgb *= Brightness;

	return color;
}

float4 Blur(sampler sp, float2 uv, float2 dir)
{
	dir *= BlurSize * Downscale;
	float sigma = sqrt(BlurSamples) * BlurSigma;
	return GaussianBlur1D(sp, uv, dir, sigma, BlurSamples);
}

float4 BlurXPS(float4 p : SV_POSITION, float2 uv : TEXCOORD) : SV_TARGET
{
	return Blur(LensA, uv, float2(BUFFER_RCP_WIDTH, 0));
}

float4 BlurYPS(float4 p : SV_POSITION, float2 uv : TEXCOORD) : SV_TARGET
{
	return Blur(LensB, uv, float2(0, BUFFER_RCP_HEIGHT));
}

float4 BlendPS(float4 p : SV_POSITION, float2 uv : TEXCOORD) : SV_TARGET
{
	float4 color = tex2D(BackBuffer, uv);
	color.rgb = Tonemap::Inverse(Tonemapper, color.rgb);

	uv = Transform::FisheyeLens(FisheyeScaleType, uv, FisheyeAmount * 10.0, FisheyeZoom * 3.0);

	#define _GET_TINT(id) float4(lerp(1.0, Tint##id##.rgb * Tint##id##.a, TintAmount), 1.0)

	#define _GET_FLARE(id) \
	(tex2D(LensA, ScaleCoord(uv, Scale##id * BaseFlareDownscale)) * _GET_TINT(id))

	float4 lens =
		_GET_FLARE(1) +
		_GET_FLARE(2) +
		_GET_FLARE(3) +
		_GET_FLARE(4) +
		_GET_FLARE(5) +
		_GET_FLARE(6) +
		_GET_FLARE(7);
	lens /= 7;

	#undef _GET_FLARE
	#undef _GET_TINT

	color.rgb = ShowLens
		? lens.rgb
		: color.rgb + lens.rgb; 

	color.rgb = Tonemap::Apply(Tonemapper, color.rgb);

	return color;
}

technique LiquidLens
{
	pass Prepare
	{
		VertexShader = ScreenVS;
		PixelShader = PreparePS;
		RenderTarget = LensATex;
	}
	pass BlurX
	{
		VertexShader = ScreenVS;
		PixelShader = BlurXPS;
		RenderTarget = LensBTex;
	}
	pass BlurY
	{
		VertexShader = ScreenVS;
		PixelShader = BlurYPS;
		RenderTarget = LensATex;
	}
	pass Blend
	{
		VertexShader = ScreenVS;
		PixelShader = BlendPS;
		SRGBWriteEnable = true;
	}
}

}} // Namespace.
