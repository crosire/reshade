//#region Includes

#include "ReShade.fxh"
#include "ReShadeUI.fxh"
#include "ColorLab.fxh"
#include "FXShaders/Blending.fxh"
#include "FXShaders/Common.fxh"
#include "FXShaders/Convolution.fxh"
#include "FXShaders/Dithering.fxh"
#include "FXShaders/Tonemap.fxh"

//#endregion

//#region Macros

#ifndef NEO_BLOOM_TEXTURE_SIZE
#define NEO_BLOOM_TEXTURE_SIZE 1024
#endif

// Should be ((int)log2(NEO_BLOOM_TEXTURE_SIZE) + 1)
#ifndef NEO_BLOOM_TEXTURE_MIP_LEVELS
#define NEO_BLOOM_TEXTURE_MIP_LEVELS 11
#endif

#ifndef NEO_BLOOM_BLUR_SAMPLES
#define NEO_BLOOM_BLUR_SAMPLES 27
#endif

#ifndef NEO_BLOOM_DOWN_SCALE
#define NEO_BLOOM_DOWN_SCALE 2
#endif

#ifndef NEO_BLOOM_ADAPT
#define NEO_BLOOM_ADAPT 0
#endif

#ifndef NEO_BLOOM_DEBUG
#define NEO_BLOOM_DEBUG 0
#endif

#ifndef NEO_BLOOM_LENS_DIRT
#define NEO_BLOOM_LENS_DIRT 0
#endif

#ifndef NEO_BLOOM_LENS_DIRT_TEXTURE_NAME
#define NEO_BLOOM_LENS_DIRT_TEXTURE_NAME "NeoBloom_LensDirt.png"
#endif

#ifndef NEO_BLOOM_LENS_DIRT_TEXTURE_WIDTH
#define NEO_BLOOM_LENS_DIRT_TEXTURE_WIDTH 1280
#endif

#ifndef NEO_BLOOM_LENS_DIRT_TEXTURE_HEIGHT
#define NEO_BLOOM_LENS_DIRT_TEXTURE_HEIGHT 720
#endif

#ifndef NEO_BLOOM_LENS_DIRT_ASPECT_RATIO_CORRECTION
#define NEO_BLOOM_LENS_DIRT_ASPECT_RATIO_CORRECTION 1
#endif

#ifndef NEO_BLOOM_GHOSTING
#define NEO_BLOOM_GHOSTING 0
#endif

#ifndef NEO_BLOOM_GHOSTING_DOWN_SCALE
#define NEO_BLOOM_GHOSTING_DOWN_SCALE (NEO_BLOOM_DOWN_SCALE / 4.0)
#endif

#ifndef NEO_BLOOM_DEPTH
#define NEO_BLOOM_DEPTH 0
#endif

#ifndef NEO_BLOOM_DEPTH_ANTI_FLICKER
#define NEO_BLOOM_DEPTH_ANTI_FLICKER 0
#endif

#define NEO_BLOOM_NEEDS_LAST (NEO_BLOOM_GHOSTING || NEO_BLOOM_DEPTH && NEO_BLOOM_DEPTH_ANTI_FLICKER)

#ifndef NEO_BLOOM_DITHERING
#define NEO_BLOOM_DITHERING 0
#endif

//#endregion

namespace FXShaders
{

//#region Data Types

struct BlendPassParams
{
	float4 p : SV_POSITION;
	float2 uv : TEXCOORD0;

	#if NEO_BLOOM_LENS_DIRT
		float2 lens_uv : TEXCOORD1;
	#endif
};

//#endregion

//#region Constants

// Each bloom means: (x, y, scale, miplevel).
static const int BloomCount = 5;
static const float4 BloomLevels[] =
{
	float4(0.0, 0.5, 0.5, 1),
	float4(0.5, 0.0, 0.25, 2),
	float4(0.75, 0.875, 0.125, 3),
	float4(0.875, 0.0, 0.0625, 5),
	float4(0.0, 0.0, 0.03, 7)
	//float4(0.0, 0.0, 0.03125, 9)
};
static const int MaxBloomLevel = BloomCount - 1;

static const int BlurSamples = NEO_BLOOM_BLUR_SAMPLES;

static const float2 PixelScale = 1.0;

static const float2 DirtResolution = float2(
	NEO_BLOOM_LENS_DIRT_TEXTURE_WIDTH,
	NEO_BLOOM_LENS_DIRT_TEXTURE_HEIGHT);
static const float2 DirtPixelSize = 1.0 / DirtResolution;
static const float DirtAspectRatio = DirtResolution.x * DirtPixelSize.y;
static const float DirtAspectRatioInv = 1.0 / DirtAspectRatio;

static const int DebugOption_None = 0;
static const int DebugOption_OnlyBloom = 1;
static const int DebugOptions_TextureAtlas = 2;
static const int DebugOption_Adaptation = 3;

#if NEO_BLOOM_ADAPT
	static const int DebugOption_DepthRange = 4;
#else
	static const int DebugOption_DepthRange = 3;
#endif

static const int AdaptMode_FinalImage = 0;
static const int AdaptMode_OnlyBloom = 1;

static const int BloomBlendMode_Mix = 0;
static const int BloomBlendMode_Addition = 1;
static const int BloomBlendMode_Screen = 2;

//#endregion

//#region Uniforms

// Bloom

FXSHADERS_HELP(
	"NeoBloom has many options and may be difficult to setup or may look "
	"bad at first, but it's designed to be very flexible to adapt to many "
	"different cases.\n"
	"Make sure to take a look at the preprocessor definitions at the "
	"bottom!\n"
	"For more specific descriptions, move the mouse cursor over the name "
	"of the option you need help with.\n"
	"\n"
	"Here's a general description of the features:\n"
	"\n"
	"  Bloom:\n"
	"    Basic options for controlling the look of bloom itself.\n"
	"\n"
	"  Adaptation:\n"
	"    Used to dynamically increase or reduce the image brightness "
	"depending on the scene, giving an HDR look.\n"
	"    Looking at a bright object, like a lamp, would cause the image to "
	"darken; lookinng at a dark spot, like a cave, would cause the "
	"image to brighten.\n"
	"\n"
	"  Blending:\n"
	"    Used to control how the different bloom textures are blended, "
	"each representing a different level-of-detail.\n"
	"    Can be used to simulate an old mid-2000s bloom, ambient light "
	"etc.\n"
	"\n"
	"  Ghosting:\n"
	"    Smoothens the bloom between frames, causing a \"motion blur\" or "
	"\"trails\" effect.\n"
	"\n"
	"  Depth:\n"
	"    Used to increase or decrease the brightness of parts of the image "
	"depending on depth.\n"
	"    Can be used for effects like brightening the sky.\n"
	"    An optional anti-flicker feature is available to help with games "
	"with depth flickering problems, which can cause bloom to flicker as "
	"well with the depth feature enabled.\n"
	"\n"
	"  HDR:\n"
	"    Options for controlling the high dynamic range simulation.\n"
	"    Useful for simulating a more foggy bloom, like an old soap opera, "
	"a high-contrast sunny look etc.\n"
	"\n"
	"  Blur:\n"
	"    Options for controlling the blurring effect used to generate the "
	"bloom textures.\n"
	"    Mostly can be left untouched.\n"
	"\n"
	"  Debug:\n"
	"    Enables testing options, like viewing the bloom texture alone, "
	"before mixing with the image.\n"
);

uniform float Intensity
<
	__UNIFORM_SLIDER_FLOAT1

	ui_label = "Intensity";
	ui_tooltip =
		"Determines how much bloom is added to the image. For HDR games you'd "
		"generally want to keep this low-ish, otherwise everything might look "
		"too bright.\n"
		"\nDefault: 1.0";
	ui_category = "Bloom";
	ui_min = 0.0;
	ui_max = 1.0;
> = 1.0;

uniform float Saturation
<
	__UNIFORM_SLIDER_FLOAT1

	ui_label = "Saturation";
	ui_tooltip =
		"Saturation of the bloom texture.\n"
		"\nDefault: 1.0";
	ui_category = "Bloom";
	ui_min = 0.0;
	ui_max = 3.0;
> = 1.0;

uniform float3 ColorFilter
<
	__UNIFORM_COLOR_FLOAT3

	ui_label = "Color Filter";
	ui_tooltip =
		"Color multiplied to the bloom, filtering it.\n"
		"Set to full white (255, 255, 255) to disable it.\n"
		"\nDefault: 255 255 255";
	ui_category = "Bloom";
> = float3(1.0, 1.0, 1.0);

uniform int BloomBlendMode
<
	__UNIFORM_COMBO_INT1

	ui_label = "Blend Mode";
	ui_tooltip =
		"Determines the formula used to blend bloom with the scene color.\n"
		"Certain blend modes may not play well with other options.\n"
		"As a fallback, addition always works.\n"
		"\nDefault: Mix";
	ui_category = "Bloom";
	ui_items = "Mix\0Addition\0Screen\0";
> = 1;

#if NEO_BLOOM_LENS_DIRT

uniform float LensDirtAmount
<
	__UNIFORM_SLIDER_FLOAT1

	ui_text =
		"Set NEO_BLOOM_DIRT to 0 to disable this feature to reduce resource "
		"usage.";
	ui_label = "Amount";
	ui_tooltip =
		"Determines how much lens dirt is added to the bloom texture.\n"
		"\nDefault: 0.0";
	ui_category = "Lens Dirt";
	ui_min = 0.0;
	ui_max = 3.0;
> = 0.0;

#endif

#if NEO_BLOOM_ADAPT

// Adaptation

uniform int AdaptMode
<
	__UNIFORM_COMBO_INT1

	ui_text =
		"Set NEO_BLOOM_ADAPT to 0 to disable this feature to reduce resource "
		"usage.";
	ui_label = "Mode";
	ui_tooltip =
		"Select different modes of how adaptation is applied.\n"
		"  Final Image:\n"
		"    Apply adaptation to the image after it was mixed with bloom.\n"
		"  Bloom Only:\n"
		"    Apply adaptation only to bloom, before mixing with the image.\n"
		"\nDefault: Final Image";
	ui_category = "Adaptation";
	ui_items = "Final Image\0Bloom Only\0";
> = 0;

uniform float AdaptAmount
<
	__UNIFORM_SLIDER_FLOAT1

	ui_label = "Amount";
	ui_tooltip =
		"How much adaptation affects the image brightness.\n"
		"\bDefault: 1.0";
	ui_category = "Adaptation";
	ui_min = 0.0;
	ui_max = 2.0;
> = 1.0;

uniform float AdaptSensitivity
<
	__UNIFORM_SLIDER_FLOAT1

	ui_label = "Sensitivity";
	ui_tooltip =
		"How sensitive is the adaptation towards bright spots?\n"
		"\nDefault: 1.0";
	ui_category = "Adaptation";
	ui_min = 0.0;
	ui_max = 2.0;
> = 1.0;

uniform float AdaptExposure
<
	__UNIFORM_SLIDER_FLOAT1

	ui_label = "Exposure";
	ui_tooltip =
		"Determines the general brightness that the effect should adapt "
		"towards.\n"
		"This is measured in f-numbers, thus 0 is the base exposure, <0 will "
		"be darker and >0 brighter.\n"
		"\nDefault: 0.0";
	ui_category = "Adaptation";
	ui_min = -3.0;
	ui_max = 3.0;
> = 0.0;

uniform bool AdaptUseLimits
<
	ui_label = "Use Limits";
	ui_tooltip =
		"Should the adaptation be limited to the minimum and maximum values "
		"specified below?\n"
		"\nDefault: On";
	ui_category = "Adaptation";
> = true;

uniform float2 AdaptLimits
<
	__UNIFORM_SLIDER_FLOAT2

	ui_label = "Limits";
	ui_tooltip =
		"The minimum and maximum values that adaptation can achieve.\n"
		"Increasing the minimum value will lessen how bright the image can "
		"become in dark scenes.\n"
		"Decreasing the maximum value will lessen how dark the image can "
		"become in bright scenes.\n"
		"\nDefault: 0.0 1.0";
	ui_category = "Adaptation";
	ui_min = 0.0;
	ui_max = 1.0;
	ui_step = 0.001;
> = float2(0.0, 1.0);

uniform float AdaptTime
<
	__UNIFORM_SLIDER_FLOAT1

	ui_label = "Time";
	ui_tooltip =
		"The time it takes for the effect to adapt.\n"
		"\nDefault: 1.0";
	ui_category = "Adaptation";
	ui_min = 0.02;
	ui_max = 3.0;
> = 1.0;

uniform float AdaptPrecision
<
	__UNIFORM_SLIDER_FLOAT1

	ui_label = "Precision";
	ui_tooltip =
		"How precise adaptation will be towards the center of the image.\n"
		"This means that 0.0 will yield an adaptation of the overall image "
		"brightness, while higher values will focus more and more towards the "
		"center pixels.\n"
		"\nDefault: 0.0";
	ui_category = "Adaptation";
	ui_min = 0.0;
	ui_max = NEO_BLOOM_TEXTURE_MIP_LEVELS;
	ui_step = 1.0;
> = 0.0;

uniform int AdaptFormula
<
	__UNIFORM_COMBO_INT1

	ui_label = "Formula";
	ui_tooltip =
		"Which formula to use when extracting brightness information from "
		"color.\n"
		"\nDefault: Luma (Linear)";
	ui_category = "Adaptation";
	ui_items = "Average\0Luminance\0Luma (Gamma)\0Luma (Linear)\0";
> = 3;

#endif

// Blending

uniform float Mean
<
	__UNIFORM_SLIDER_FLOAT1

	ui_label = "Mean";
	ui_tooltip =
		"Acts as a bias between all the bloom textures/sizes. What this means "
		"is that lower values will yield more detail bloom, while the opposite "
		"will yield big highlights.\n"
		"The more variance is specified, the less effective this setting is, "
		"so if you want to have very fine detail bloom reduce both "
		"parameters.\n"
		"\nDefault: 0.0";
	ui_category = "Blending";
	ui_min = 0.0;
	ui_max = BloomCount;
	//ui_step = 0.005;
> = 0.0;

uniform float Variance
<
	__UNIFORM_SLIDER_FLOAT1

	ui_label = "Variance";
	ui_tooltip =
		"Determines the 'variety'/'contrast' in bloom textures/sizes. This "
		"means a low variance will yield more of the bloom size specified by "
		"the mean; that is to say that having a low variance and mean will "
		"yield more fine-detail bloom.\n"
		"A high variance will diminish the effect of the mean, since it'll "
		"cause all the bloom textures to blend more equally.\n"
		"A low variance and high mean would yield an effect similar to "
		"an 'ambient light', with big blooms of light, but few details.\n"
		"\nDefault: 1.0";
	ui_category = "Blending";
	ui_min = 1.0;
	ui_max = BloomCount;
	//ui_step = 0.005;
> = BloomCount;

#if NEO_BLOOM_GHOSTING

// Last

uniform float GhostingAmount
<
	__UNIFORM_SLIDER_FLOAT1

	ui_text =
		"Set NEO_BLOOM_GHOSTING to 0 if you don't use this feature to reduce "
		"resource usage.";
	ui_label = "Amount";
	ui_tooltip =
		"Amount of ghosting applied.\n"
		"\nDefault: 0.0";
	ui_category = "Ghosting";
	ui_min = 0.0;
	ui_max = 0.999;
> = 0.0;

#endif

#if NEO_BLOOM_DEPTH

uniform float3 DepthMultiplier
<
	__UNIFORM_DRAG_FLOAT3

	ui_text =
		"Set NEO_BLOOM_DEPTH to 0 if you don't use this feature to reduce "
		"resource usage.";
	ui_label = "Multiplier";
	ui_tooltip =
		"Defines the multipliers that will be applied to each range in depth.\n"
		" - The first value defines the multiplier for near depth.\n"
		" - The second value defines the multiplier for middle depth.\n"
		" - The third value defines the multiplier for far depth.\n"
		"\nDefault: 1.0 1.0 1.0";
	ui_category = "Depth";
	ui_min = 0.0;
	ui_max = 10.0;
	ui_step = 0.01;
> = float3(1.0, 1.0, 1.0);

uniform float2 DepthRange
<
	__UNIFORM_DRAG_FLOAT2

	ui_label = "Range";
	ui_tooltip =
		"Defines the depth range for thee depth multiplier.\n"
		" - The first value defines the start of the middle depth."
		" - The second value defines the end of the middle depth and the start "
		"of the far depth."
		"\nDefault: 0.0 1.0";
	ui_category = "Depth";
	ui_min = 0.0;
	ui_max = 1.0;
	ui_step = 0.001;
> = float2(0.0, 1.0);

uniform float DepthSmoothness
<
	__UNIFORM_DRAG_FLOAT1

	ui_label = "Smoothness";
	ui_tooltip =
		"Amount of smoothness in the transition between depth ranges.\n"
		"\nDefault: 1.0";
	ui_category = "Depth";
	ui_min = 0.0;
	ui_max = 1.0;
	ui_step = 0.001;
> = 1.0;

#if NEO_BLOOM_DEPTH_ANTI_FLICKER

uniform float DepthAntiFlicker
<
	__UNIFORM_SLIDER_FLOAT1

	ui_label = "Anti-Flicker Amount";
	ui_tooltip =
		"Amount of anti-flicker to apply to the depth feature.\n"
		"Use if you notice distant bloom flickering when using high "
		"multipliers.\n"
		"\nDefault: 0.999";
	ui_min = 0.0;
	ui_max = 0.999;
> = 0.999;

#endif

#endif

// HDR

uniform float MaxBrightness
<
	__UNIFORM_DRAG_FLOAT1

	ui_label  = "Max Brightness";
	ui_tooltip =
		"tl;dr: HDR contrast.\n"
		"\nDetermines the maximum brightness a pixel can achieve from being "
		"'reverse-tonemapped', that is to say, when the effect attempts to "
		"extract HDR information from the image.\n"
		"In practice, the difference between a value of 100 and one of 1000 "
		"would be in how bright/bloomy/big a white pixel could become, like "
		"the sun or the headlights of a car.\n"
		"Lower values can also work for making a more 'balanced' bloom, where "
		"there are less harsh highlights and the entire scene is equally "
		"foggy, like an old TV show or with dirty lenses.\n"
		"\nDefault: 100.0";
	ui_category = "HDR";
	ui_min = 1.0;
	ui_max = 1000.0;
	ui_step = 1.0;
> = 100.0;

uniform bool NormalizeBrightness
<
	ui_label = "Normalize Brightness";
	ui_tooltip =
		"Whether to normalize the bloom brightness when blending with the "
		"image.\n"
		"Without it, the bloom may have very harsh bright spots.\n"
		"\nDefault: On";
	ui_category = "HDR";
> = true;

uniform bool MagicMode
<
	ui_label = "Magic Mode";
	ui_tooltip =
		"When enabled, simulates the look of MagicBloom.\n"
		"This is an experimental option and may be inconsistent with other "
		"parameters.\n"
		"\nDefault: Off";
	ui_category = "HDR";
> = false;

// Blur

uniform float Sigma
<
	__UNIFORM_DRAG_FLOAT1

	ui_label = "Sigma";
	ui_tooltip =
		"Amount of blurriness. Values too high will break the blur.\n"
		"Recommended values are between 2 and 4.\n"
		"\nDefault: 2.0";
	ui_category = "Blur";
	ui_min = 1.0;
	ui_max = 10.0;
	ui_step = 0.01;
> = 4.0;

uniform float Padding
<
	__UNIFORM_DRAG_FLOAT1

	ui_label = "Padding";
	ui_tooltip =
		"Specifies additional padding around the bloom textures in the "
		"internal texture atlas, which is used during the blurring process.\n"
		"The reason for this is to reduce the loss of bloom brightness around "
		"the screen edges, due to the way the blurring works.\n"
		"\n"
		"If desired, it can be set to zero to purposefully reduce the "
		"amount of bloom around the edges.\n"
		"It may be necessary to increase this parameter when increasing the "
		"blur sigma, samples and/or bloom down scale.\n"
		"\n"
		"Due to the way it works, it's recommended to keep the value as low "
		"as necessary, since it'll cause the blurring process to work in a "
		"\"lower\" resolution.\n"
		"\n"
		"If you're still confused about this parameter, try viewing the "
		"texture atlas with debug mode and watch what happens when it is "
		"increased.\n"
		"\nDefault: 0.1";
	ui_category = "Blur";
	ui_min = 0.0;
	ui_max = 10.0;
	ui_step = 0.001;
> = 0.1;

#if NEO_BLOOM_DEBUG

// Debug

uniform int DebugOptions
<
	__UNIFORM_COMBO_INT1

	ui_text =
		"Set NEO_BLOOM_DEBUG to 0 if you don't use this feature to reduce "
		"resource usage.";
	ui_label = "Debug Options";
	ui_tooltip =
		"Debug options containing:\n"
		"  - Showing only the bloom texture. The 'bloom texture to show' "
		"parameter can be used to determine which bloom texture(s) to "
		"visualize.\n"
		"  - Showing the raw internal texture atlas used to blur all the bloom "
		"\"textures\", visualizing all the blooms at once in scale.\n"
		#if NEO_BLOOM_ADAPT
		"  - Showing the adaptation texture directly.\n"
		#endif
		#if NEO_BLOOM_DEPTH
		"  - Showing the depth ranges texture, displaying the near range as "
		"red, middle as green and far as blue.\n"
		#endif
		"\nDefault: None";
	ui_category = "Debug";
	ui_items =
		"None\0Show Only Bloom\0Show Texture Atlas\0"
		#if NEO_BLOOM_ADAPT
		"Show Adaptation\0"
		#endif
		#if NEO_BLOOM_DEPTH
		"Show Depth Range\0"
		#endif
		;
> = false;

uniform int BloomTextureToShow
<
	__UNIFORM_SLIDER_INT1

	ui_label = "Bloom Texture To Show";
	ui_tooltip =
		"Which bloom texture to show with the 'Show Only Bloom' debug option.\n"
		"Set to -1 to view all textures blended.\n"
		"\nDefault: -1";
	ui_category = "Debug";
	ui_min = -1;
	ui_max = MaxBloomLevel;
> = -1;

#endif

#if NEO_BLOOM_DITHERING

uniform float DitherAmount
<
	__UNIFORM_DRAG_FLOAT1

	ui_label = "Amount";
	ui_tooltip =
		"Amount of dithering to apply to the bloom.\n"
		"\nDefault: 0.1";
	ui_category = "Dithering";
	ui_min = 0.0;
	ui_max = 1.0;
	ui_step = 0.001;
> = 0.1;

#endif

#if NEO_BLOOM_ADAPT

uniform float FrameTime <source = "frametime";>;

#endif

//#endregion

//#region Textures

sampler BackBuffer
{
	Texture = ReShade::BackBufferTex;
	SRGBTexture = true;
};

texture NeoBloom_DownSample <pooled="true";>
{
	Width = NEO_BLOOM_TEXTURE_SIZE;
	Height = NEO_BLOOM_TEXTURE_SIZE;
	Format = RGBA16F;
	MipLevels = NEO_BLOOM_TEXTURE_MIP_LEVELS;
};
sampler DownSample
{
	Texture = NeoBloom_DownSample;
};

texture NeoBloom_AtlasA <pooled="true";>
{
	Width = BUFFER_WIDTH / NEO_BLOOM_DOWN_SCALE;
	Height = BUFFER_HEIGHT / NEO_BLOOM_DOWN_SCALE;
	Format = RGBA16F;
};
sampler AtlasA
{
	Texture = NeoBloom_AtlasA;
	AddressU = BORDER;
	AddressV = BORDER;
};

texture NeoBloom_AtlasB <pooled="true";>
{
	Width = BUFFER_WIDTH / NEO_BLOOM_DOWN_SCALE;
	Height = BUFFER_HEIGHT / NEO_BLOOM_DOWN_SCALE;
	Format = RGBA16F;
};
sampler AtlasB
{
	Texture = NeoBloom_AtlasB;
	AddressU = BORDER;
	AddressV = BORDER;
};

#if NEO_BLOOM_ADAPT

texture NeoBloom_Adapt <pooled="true";>
{
	Format = R16F;
};
sampler Adapt
{
	Texture = NeoBloom_Adapt;
	MinFilter = POINT;
	MagFilter = POINT;
	MipFilter = POINT;
};

texture NeoBloom_LastAdapt
{
	Format = R16F;
};
sampler LastAdapt
{
	Texture = NeoBloom_LastAdapt;
	MinFilter = POINT;
	MagFilter = POINT;
	MipFilter = POINT;
};

#endif

#if NEO_BLOOM_LENS_DIRT

texture NeoBloom_LensDirt
<
	source = NEO_BLOOM_LENS_DIRT_TEXTURE_NAME;
>
{
	Width = NEO_BLOOM_LENS_DIRT_TEXTURE_WIDTH;
	Height = NEO_BLOOM_LENS_DIRT_TEXTURE_HEIGHT;
};
sampler LensDirt
{
	Texture = NeoBloom_LensDirt;
};

#endif

#if NEO_BLOOM_NEEDS_LAST

texture NeoBloom_Last
{
	Width = BUFFER_WIDTH / NEO_BLOOM_GHOSTING_DOWN_SCALE;
	Height = BUFFER_HEIGHT / NEO_BLOOM_GHOSTING_DOWN_SCALE;

	#if NEO_BLOOM_GHOSTING && NEO_BLOOM_DEPTH_ANTI_FLICKER
		Format = RGBA16F;
	#else
		Format = R8;
	#endif
};
sampler Last
{
	Texture = NeoBloom_Last;
};

#if NEO_BLOOM_DEPTH

texture NeoBloom_Depth
{
	Width = BUFFER_WIDTH;
	Height = BUFFER_HEIGHT;
	Format = R8;
};
sampler Depth
{
	Texture = NeoBloom_Depth;
};

#endif

#endif

//#endregion

//#region Functions

float3 blend_bloom(float3 color, float3 bloom)
{
	float w;
	if (NormalizeBrightness)
		w = Intensity / MaxBrightness;
	else
		w = Intensity;

	switch (BloomBlendMode)
	{
		default:
			return 0.0;
		case BloomBlendMode_Mix:
			return lerp(color, bloom, log2(w + 1.0));
		case BloomBlendMode_Addition:
			return color + bloom * w * 3.0;
		case BloomBlendMode_Screen:
			return BlendScreen(color, bloom, w);
	}
}

float3 inv_tonemap_bloom(float3 color)
{
	if (MagicMode)
		return pow(abs(color), MaxBrightness * 0.01);

	return Tonemap::Reinhard::InverseOldLum(color, 1.0 / MaxBrightness);
}

float3 inv_tonemap(float3 color)
{
	if (MagicMode)
		return color;

	return Tonemap::Reinhard::InverseOld(color, 1.0 / MaxBrightness);
}

float3 tonemap(float3 color)
{
	if (MagicMode)
		return color;

	return Tonemap::Reinhard::Apply(color);
}

//#endregion

//#region Shaders

#if NEO_BLOOM_DEPTH && NEO_BLOOM_DEPTH_ANTI_FLICKER

float GetDepthPS(float4 p : SV_POSITION, float2 uv : TEXCOORD) : SV_TARGET
{
	float3 depth = ReShade::GetLinearizedDepth(uv);

	#if NEO_BLOOM_GHOSTING
		float last = tex2D(Last, uv).a;
	#else
		float last = tex2D(Last, uv).r;
	#endif

	depth = lerp(depth, last, DepthAntiFlicker);

	return depth;
}

#endif

float4 DownSamplePS(float4 p : SV_POSITION, float2 uv : TEXCOORD) : SV_TARGET
{
	float4 color = tex2D(BackBuffer, uv);
	color.rgb = saturate(ApplySaturation(color.rgb, Saturation));
	color.rgb *= ColorFilter;
	color.rgb = inv_tonemap_bloom(color.rgb);

	#if NEO_BLOOM_DEPTH
		#if NEO_BLOOM_DEPTH_ANTI_FLICKER
			float3 depth = tex2D(Depth, uv).x;
		#else
			float3 depth = ReShade::GetLinearizedDepth(uv);
		#endif

		float is_near = smoothstep(
			depth - DepthSmoothness,
			depth + DepthSmoothness,
			DepthRange.x);

		float is_far = smoothstep(
			DepthRange.y - DepthSmoothness,
			DepthRange.y + DepthSmoothness, depth);

		float is_middle = (1.0 - is_near) * (1.0 - is_far);

		color.rgb *= lerp(1.0, DepthMultiplier.x, is_near);
		color.rgb *= lerp(1.0, DepthMultiplier.y, is_middle);
		color.rgb *= lerp(1.0, DepthMultiplier.z, is_far);
	#endif

	return color;
}

float4 SplitPS(float4 p : SV_POSITION, float2 uv : TEXCOORD) : SV_TARGET
{
	float4 color = 0.0;

	[unroll]
	for (int i = 0; i < BloomCount; ++i)
	{
		float4 rect = BloomLevels[i];
		float2 rect_uv = ScaleCoord(uv - rect.xy, 1.0 / rect.z, 0.0);
		float inbounds =
			step(0.0, rect_uv.x) * step(rect_uv.x, 1.0) *
			step(0.0, rect_uv.y) * step(rect_uv.y, 1.0);

		rect_uv = ScaleCoord(rect_uv, 1.0 + Padding * (i + 1), 0.5);

		float4 pixel = tex2Dlod(DownSample, float4(rect_uv, 0, rect.w));
		pixel.rgb *= inbounds;
		pixel.a = inbounds;

		color += pixel;
	}

	return color;
}

float4 BlurXPS(float4 p : SV_POSITION, float2 uv : TEXCOORD) : SV_TARGET
{
	float2 dir =
		PixelScale *
		float2(BUFFER_RCP_WIDTH, 0.0) *
		NEO_BLOOM_DOWN_SCALE;

	return GaussianBlur1D(AtlasA, uv, dir, Sigma, BlurSamples);
}

float4 BlurYPS(float4 p : SV_POSITION, float2 uv : TEXCOORD) : SV_TARGET
{
	float2 dir =
		PixelScale *
		float2(0.0, BUFFER_RCP_HEIGHT) *
		NEO_BLOOM_DOWN_SCALE;

	return GaussianBlur1D(AtlasB, uv, dir, Sigma, BlurSamples);
}

#if NEO_BLOOM_ADAPT

float4 CalcAdaptPS(float4 p : SV_POSITION, float2 uv : TEXCOORD) : SV_TARGET
{
	float3 color = tex2Dlod(
		DownSample,
		float4(0.5, 0.5, 0.0, NEO_BLOOM_TEXTURE_MIP_LEVELS - AdaptPrecision)
	).rgb;
	color = tonemap(color);

	float gs;
	switch (AdaptFormula)
	{
		case 0:
			gs = dot(color, 0.333);
			break;
		case 1:
			gs = max(color.r, max(color.g, color.b));
			break;
		case 2:
			gs = GetLumaGamma(color);
			break;
		case 3:
			gs = GetLumaLinear(color);
			break;
	}

	gs *= AdaptSensitivity;
	gs = AdaptUseLimits ? clamp(gs, AdaptLimits.x, AdaptLimits.y) : gs;

	float last = tex2D(LastAdapt, 0.0).r;
	gs = lerp(last, gs, saturate((FrameTime * 0.001) / max(AdaptTime, 0.001)));

	return float4(gs, 0.0, 0.0, 1.0);
}

float4 SaveAdaptPS(float4 p : SV_POSITION, float2 uv : TEXCOORD) : SV_TARGET
{
	return tex2D(Adapt, 0.0);
}

#endif

float4 JoinBloomsPS(float4 p : SV_POSITION, float2 uv : TEXCOORD) : SV_TARGET
{
	float4 bloom = 0.0;
	float accum = 0.0;

	[unroll]
	for (int i = 0; i < BloomCount; ++i)
	{
		float4 rect = BloomLevels[i];
		float2 rect_uv = ScaleCoord(uv, 1.0 / (1.0 + Padding * (i + 1)), 0.5);
		rect_uv = ScaleCoord(rect_uv + rect.xy / rect.z, rect.z, 0.0);

		float weight = NormalDistribution(i, Mean, Variance);
		bloom += tex2D(AtlasA, rect_uv) * weight;
		accum += weight;
	}
	bloom /= accum;

	#if NEO_BLOOM_GHOSTING
		float3 last = tex2D(Last, uv).rgb;
		bloom.rgb = lerp(bloom.rgb, last.rgb, GhostingAmount);
	#endif

	return bloom;
}

#if NEO_BLOOM_NEEDS_LAST

float4 SaveLastBloomPS(
	float4 p : SV_POSITION,
	float2 uv : TEXCOORD
) : SV_TARGET
{
	float4 color = float4(0.0, 0.0, 0.0, 1.0);

	#if NEO_BLOOM_DEPTH && NEO_BLOOM_DEPTH_ANTI_FLICKER
		color = tex2D(Depth, uv).x;
	#endif

	#if NEO_BLOOM_GHOSTING
		color.rgb = tex2D(AtlasB, uv).rgb;
	#endif

	return color;
}

#endif

// As a workaround for a bug in the current ReShade DirectX 9 code generator,
// we have to return the parameters instead of using out.
// If we don't do that, the DirectX 9 half pixel offset bug is not automatically
// corrected by the code generator, which leads to a slightly blurry image.
BlendPassParams BlendVS(uint id : SV_VERTEXID)
{
	BlendPassParams p;

	PostProcessVS(id, p.p, p.uv);

	#if NEO_BLOOM_LENS_DIRT && NEO_BLOOM_LENS_DIRT_ASPECT_RATIO_CORRECTION
		float ar = BUFFER_WIDTH * BUFFER_RCP_HEIGHT;
		float ar_inv = BUFFER_HEIGHT * BUFFER_RCP_WIDTH;
		float is_horizontal = step(ar, DirtAspectRatio);
		float ratio = lerp(
			DirtAspectRatio * ar_inv,
			ar * DirtAspectRatioInv,
			is_horizontal);

		p.lens_uv = ScaleCoord(p.uv, float2(1.0, ratio), 0.5);
	#endif

	return p;
}

float4 BlendPS(BlendPassParams p) : SV_TARGET
{
	float2 uv = p.uv;

	float4 color = tex2D(BackBuffer, uv);
	color.rgb = inv_tonemap(color.rgb);

	#if NEO_BLOOM_GHOSTING
		float4 bloom = tex2D(AtlasB, uv);
	#else
		float4 bloom = JoinBloomsPS(p.p, uv);
	#endif

	#if NEO_BLOOM_DITHERING
		bloom.rgb = FXShaders::Dithering::Ordered16::Apply(
			bloom.rgb,
			uv,
			DitherAmount);
	#endif

	#if NEO_BLOOM_LENS_DIRT
		float4 dirt = tex2D(LensDirt, p.lens_uv);
		bloom.rgb = mad(dirt.rgb, bloom.rgb * LensDirtAmount, bloom.rgb);
	#endif

	#if NEO_BLOOM_DEBUG
		switch (DebugOptions)
		{
			case DebugOption_OnlyBloom:
				if (BloomTextureToShow == -1)
				{
					color.rgb = tonemap(bloom.rgb);
				}
				else
				{
					float4 rect = BloomLevels[BloomTextureToShow];
					float2 rect_uv = ScaleCoord(
						uv,
						1.0 / (1.0 + Padding * (BloomTextureToShow + 1)),
						0.5
					);

					rect_uv = ScaleCoord(rect_uv + rect.xy / rect.z, rect.z, 0.0);
					color = tex2D(AtlasA, rect_uv);
					color.rgb = tonemap(color.rgb);
				}

				return color;
			case DebugOptions_TextureAtlas:
				color = tex2D(AtlasA, uv);
				color.rgb = lerp(checkered_pattern(uv), color.rgb, color.a);
				color.a = 1.0;

				return color;

			#if NEO_BLOOM_ADAPT
				case DebugOption_Adaptation:
					color = tex2Dlod(
						DownSample,
						float4(
							uv,
							0.0,
							NEO_BLOOM_TEXTURE_MIP_LEVELS - AdaptPrecision)
					);
					color.rgb = tonemap(color.rgb);
					return color;
			#endif

			#if NEO_BLOOM_DEPTH
				case DebugOption_DepthRange:
					#if NEO_BLOOM_DEPTH_ANTI_FLICKER
						float depth = tex2D(Depth, uv).x;
					#else
						float depth = ReShade::GetLinearizedDepth(uv);
					#endif

					color.r = smoothstep(0.0, DepthRange.x, depth);
					color.g = smoothstep(DepthRange.x, DepthRange.y, depth);
					color.b = smoothstep(DepthRange.y, 1.0, depth);

					color.r *= smoothstep(
						depth - DepthSmoothness,
						depth + DepthSmoothness,
						DepthRange.x);

					color.g *= smoothstep(
						depth - DepthSmoothness,
						depth + DepthSmoothness,
						DepthRange.y);

					return color;
			#endif
		}
	#endif

	#if NEO_BLOOM_ADAPT
		float adapt = tex2D(Adapt, 0.0).r;
		float exposure = exp(AdaptExposure) / max(adapt, 0.001);
		exposure = lerp(1.0, exposure, AdaptAmount);

		if (MagicMode)
		{
			bloom.rgb = Tonemap::Uncharted2Filmic::Apply(
				bloom.rgb * exposure * 0.1);
		}

		switch (AdaptMode)
		{
			case AdaptMode_FinalImage:
				color = blend_bloom(color, bloom);
				color.rgb *= exposure;
				break;
			case AdaptMode_OnlyBloom:
				bloom.rgb *= exposure;
				color = blend_bloom(color, bloom);
				break;
		}
	#else
		if (MagicMode)
			bloom.rgb = Tonemap::Uncharted2Filmic::Apply(bloom.rgb * 10.0);

		color.rgb = blend_bloom(color.rgb, bloom.rgb);
	#endif

	if (!MagicMode)
		color.rgb = tonemap(color.rgb);

	return color;
}

//#endregion

//#region Technique

technique NeoBloom
{
	#if NEO_BLOOM_DEPTH && NEO_BLOOM_DEPTH_ANTI_FLICKER
		pass GetDepth
		{
			VertexShader = PostProcessVS;
			PixelShader = GetDepthPS;
			RenderTarget = NeoBloom_Depth;
		}
	#endif

	pass DownSample
	{
		VertexShader = PostProcessVS;
		PixelShader = DownSamplePS;
		RenderTarget = NeoBloom_DownSample;
	}
	pass Split
	{
		VertexShader = PostProcessVS;
		PixelShader = SplitPS;
		RenderTarget = NeoBloom_AtlasA;
	}
	pass BlurX
	{
		VertexShader = PostProcessVS;
		PixelShader = BlurXPS;
		RenderTarget = NeoBloom_AtlasB;
	}
	pass BlurY
	{
		VertexShader = PostProcessVS;
		PixelShader = BlurYPS;
		RenderTarget = NeoBloom_AtlasA;
	}

	#if NEO_BLOOM_ADAPT
		pass CalcAdapt
		{
			VertexShader = PostProcessVS;
			PixelShader = CalcAdaptPS;
			RenderTarget = NeoBloom_Adapt;
		}
		pass SaveAdapt
		{
			VertexShader = PostProcessVS;
			PixelShader = SaveAdaptPS;
			RenderTarget = NeoBloom_LastAdapt;
		}
	#endif

	#if NEO_BLOOM_NEEDS_LAST
		pass JoinBlooms
		{
			VertexShader = PostProcessVS;
			PixelShader = JoinBloomsPS;
			RenderTarget = NeoBloom_AtlasB;
		}
		pass SaveLastBloom
		{
			VertexShader = PostProcessVS;
			PixelShader = SaveLastBloomPS;
			RenderTarget = NeoBloom_Last;
		}
	#endif

	pass Blend
	{
		VertexShader = BlendVS;
		PixelShader = BlendPS;
		SRGBWriteEnable = true;
	}
}

//#endregion

}
