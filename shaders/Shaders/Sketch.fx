//#region Preprocessor

#define RESHADE_BACK_BUFFER_SAMPLER_OPTIONS \
	SRGBTexture = false; /*\
	MinFilter = POINT; \
	MagFilter = POINT; \
	MipFilter = POINT;*/

#define RESHADE_DEPTH_BUFFER_SAMPLER_OPTIONS \
	MinFilter = POINT; \
	MagFilter = POINT; \
	MipFilter = POINT;

#include "ReShade.fxh"
#include "ReShadeUI.fxh"

#ifndef SKETCH_USE_PATTERN_TEXTURE
#define SKETCH_USE_PATTERN_TEXTURE 0
#endif

#ifndef SKETCH_PATTERN_TEXTURE_NAME
#define SKETCH_PATTERN_TEXTURE_NAME "Sketch_Shadow.png"
#endif

#ifndef SKETCH_PATTERN_TEXTURE_SIZE
#define SKETCH_PATTERN_TEXTURE_SIZE 1024
#endif

#ifndef SKETCH_SMOOTH
#define SKETCH_SMOOTH 1
#endif

#ifndef SKETCH_SMOOTH_SAMPLES
#define SKETCH_SMOOTH_SAMPLES 5
#endif

#ifndef SKETCH_DEBUG
#define SKETCH_DEBUG 0
#endif

//#endregion

//#region Constants

static const int SmoothingSamples = SKETCH_SMOOTH_SAMPLES;
static const float SmoothingSamplesInv = 1.0 / SmoothingSamples;
static const float SmoothingSamplesHalf = SmoothingSamples / 2;

static const float2 ZeroOne = float2(0.0, 1.0);

#if SKETCH_USE_PATTERN_TEXTURE

static const float2 ShadowSize = SKETCH_PATTERN_TEXTURE_SIZE;
static const float2 ShadowPixelSize = 1.0 / ShadowSize;

#endif

#if SKETCH_DEBUG

static const int DebugOptions_None = 0;
static const int DebugOptions_ShowRGBPalette = 1;
static const int DebugOptions_ShowSmoothing = 2;

#endif

//#endregion

//#region Textures

sampler SRGBBackBuffer
{
	Texture = ReShade::BackBufferTex;
	SRGBTexture = true;
};

#if SKETCH_USE_PATTERN_TEXTURE

texture Sketch_Shadow <source = SKETCH_PATTERN_TEXTURE_NAME;>
{
	Width = SKETCH_PATTERN_TEXTURE_SIZE;
	Height = SKETCH_PATTERN_TEXTURE_SIZE;
};

sampler Shadow
{
	Texture = Sketch_Shadow;
	AddressU = REPEAT;
	AddressV = REPEAT;
};

#endif

//#endregion

//#region Uniforms

uniform float4 PatternColor
<
	__UNIFORM_COLOR_FLOAT4

	ui_label = "Pattern Color";
	ui_tooltip = "Default: 255 255 255 16";
> = float4(255, 255, 255, 16) / 255;

uniform float2 PatternRange
<
	__UNIFORM_DRAG_FLOAT2

	ui_label = "Pattern Range";
	ui_tooltip = "Default: 0.0 0.5";
	ui_min = 0.0;
	ui_max = 1.0;
	ui_step = 0.01;
> = float2(0.0, 0.5);

uniform float OutlineThreshold
<
	__UNIFORM_DRAG_FLOAT1

	ui_label = "Outline Threshold";
	ui_tooltip = "Default: 0.01";
	ui_min = 0.001;
	ui_max = 0.1;
	ui_step = 0.001;
> = 0.01;

uniform float Posterization
<
	__UNIFORM_DRAG_FLOAT1

	ui_tooltip = "Default: 5";
	ui_min = 1;
	ui_max = 255;
	ui_step = 1;
> = 5;

#if SKETCH_SMOOTH

uniform float SmoothingScale
<
	__UNIFORM_SLIDER_FLOAT1

	ui_label = "Smoothing Scale";
	ui_tooltip = "Default: 1.0";
	ui_min = 1.0;
	ui_max = 10.0;
	ui_step = 0.01;
> = 1.0;

#endif

#if SKETCH_DEBUG

uniform int DebugOptions
<
	__UNIFORM_COMBO_INT1

	ui_label = "Debug Options";
	ui_tooltip = "Default: None";
	ui_items = "None\0Show RGB Palette\0Show Smoothing\0";
> = 0;

#endif

//#endregion

//#region Functions

float get_depth(float2 uv)
{
	return ReShade::GetLinearizedDepth(uv);
}

float3 get_normals(float2 uv)
{
	float3 ps = float3(ReShade::PixelSize, 0.0);

	float3 normals;
	normals.x = get_depth(uv - ps.xz) - get_depth(uv + ps.xz);
	normals.y = get_depth(uv + ps.zy) - get_depth(uv - ps.zy);
	normals.z = get_depth(uv);

	normals.xy = abs(normals.xy) * 0.5 * ReShade::ScreenSize;
	normals = normalize(normals);

	return normals;
}

float get_outline(float2 uv)
{
	float3 normals = get_normals(uv);

	float outline = dot(normals, float3(0.0, 0.0, 1.0));
	outline = step(OutlineThreshold, outline);

	return outline;
}

float get_pattern(float2 uv)
{
	#if SKETCH_USE_PATTERN_TEXTURE
		uv.x *= ShadowPixelSize.x * ReShade::ScreenSize.y;

		float shadow = tex2D(Shadow, uv).a;
		shadow *= 10.0;

		return shadow;
	#else
		float x = uv.x + uv.y;
		x = abs(x);
		x %= 0.0125;
		x /= 0.0125;
		x = abs((x - 0.5) * 2.0);
		x = step(0.5, x);

		return x;
	#endif
}

float3 test_palette(float3 color, float2 uv)
{
	float2 bw = float2(1.0, 0.0);
	uv.y = 1.0 - uv.y;
	uv.y *= 20.0;

	return (uv.y < 0.333)
		? uv.x * bw.xyy
		: (uv.y < 0.666)
			? uv.x * bw.yxy
			: (uv.y < 1.0)
				? uv.x * bw.yyx
				: color;
}

float3 cel_shade(float3 color, out float gray)
{
	gray = dot(color, 0.333);
	color -= gray;

	gray *= Posterization;
	gray = round(gray);
	gray /= Posterization;

	color += gray;
	return color;
}

#if SKETCH_SMOOTH

float3 blur_old(sampler s, float2 uv, float2 dir)
{
	dir *= SmoothingScale;

	uv -= SmoothingSamplesHalf * dir;
	float3 color = tex2D(s, uv).rgb;

	[unroll]
	for (int i = 1; i < SmoothingSamples; ++i)
	{
		uv += dir;
		color += tex2D(s, uv).rgb;
	}

	color *= SmoothingSamplesInv;
	return color;
}

float3 blur_depth_threshold(sampler s, float2 uv, float2 dir)
{
	dir *= SmoothingScale;

	float depth = get_depth(uv);

	uv -= SmoothingSamplesHalf * dir;
	float4 color = 0.0;

	[unroll]
	for (int i = 0; i < SmoothingSamples; ++i)
	{
		float z = get_depth(uv);
		if (abs(z - depth) < 0.001)
			color += float4(tex2D(s, uv).rgb, 1.0);

		uv += dir;
	}

	color.rgb /= color.a;
	return color.rgb;
}

float3 blur(sampler s, float2 uv, float2 dir)
{
	dir *= SmoothingScale;

	float3 center = tex2D(s, uv).rgb;

	uv -= SmoothingSamplesHalf * dir;
	float4 color = 0.0;

	[unroll]
	for (int i = 0; i < SmoothingSamples; ++i)
	{
		float3 pixel = tex2D(s, uv).rgb;
		float delta = dot(1.0 - abs(pixel - center), 0.333);

		if (delta > 0.9		)
			color += float4(pixel, 1.0);
	}

	color.rgb /= color.a;
	return color.rgb;
}

#endif

//#endregion

//#region Shaders

#if SKETCH_SMOOTH

float4 BlurXPS(float4 p : SV_POSITION, float2 uv : TEXCOORD) : SV_TARGET
{
	float2 dir = float2(BUFFER_RCP_WIDTH, 0.0);
	float3 color = blur(SRGBBackBuffer, uv, dir);
	return float4(color, 1.0);
}

float4 BlurYPS(float4 p : SV_POSITION, float2 uv : TEXCOORD) : SV_TARGET
{
	float2 dir = float2(0.0, BUFFER_RCP_HEIGHT);
	float3 color = blur(ReShade::BackBuffer, uv, dir);
	return float4(color, 1.0);
}

#endif

void MainVS(
	uint id : SV_VERTEXID,
	out float4 p : SV_POSITION,
	out float2 uv : TEXCOORD0,
	out float2 pattern_uv : TEXCOORD1)
{
	PostProcessVS(id, p, uv);

	pattern_uv = uv;
	pattern_uv.x *= ReShade::AspectRatio;
}

float4 MainPS(
	float4 p : SV_POSITION,
	float2 uv : TEXCOORD0,
	float2 pattern_uv : TEXCOORD1) : SV_TARGET
{
	float4 color = tex2D(ReShade::BackBuffer, uv);

	#if SKETCH_DEBUG
		switch (DebugOptions)
		{
			case DebugOptions_ShowSmoothing:
				return color;
			case DebugOptions_ShowRGBPalette:
				color.rgb = test_palette(color.rgb, uv);
				break;
		}
	#endif

	float gray;
	color.rgb = cel_shade(color.rgb, gray);

	float pattern = get_pattern(pattern_uv);
	pattern *= 1.0 - smoothstep(PatternRange.x, PatternRange.y, gray);
	pattern *= (1.0 - gray) * PatternColor.a;
	color.rgb = lerp(color.rgb, PatternColor.rgb, pattern);

	float outline = get_outline(uv);
	color.rgb *= outline;

	return color;
}

//#endregion

//#region Technique

technique Sketch
{
	#if SKETCH_SMOOTH
		pass BlurX
		{
			VertexShader = PostProcessVS;
			PixelShader = BlurXPS;
		}
		pass BlurY
		{
			VertexShader = PostProcessVS;
			PixelShader = BlurYPS;
			SRGBWriteEnable = true;
		}
	#endif
	pass Main
	{
		VertexShader = MainVS;
		PixelShader = MainPS;
	}
}

//#endregion