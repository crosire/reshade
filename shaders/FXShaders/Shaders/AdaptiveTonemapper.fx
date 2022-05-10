//#region Includes

//#include "ReShade.fxh"
#include "ReShadeUI.fxh"
#include "ACES.fxh"

//#endregion

//#region Macros

#ifndef ADAPTIVE_TONEMAPPER_SMALL_TEX_SIZE
#define ADAPTIVE_TONEMAPPER_SMALL_TEX_SIZE 256
#endif

// Should be set to `int(log2(ADAPTIVE_TONEMAPPER_SMALL_TEX_SIZE)) + 1`.
#ifndef ADAPTIVE_TONEMAPPER_SMALL_TEX_MIPLEVELS
#define ADAPTIVE_TONEMAPPER_SMALL_TEX_MIPLEVELS 9
#endif

//#endregion

//#region Constants

static const int2 AdaptResolution = ADAPTIVE_TONEMAPPER_SMALL_TEX_SIZE;
static const int AdaptMipLevels = ADAPTIVE_TONEMAPPER_SMALL_TEX_MIPLEVELS;

static const float3 LumaWeights = float3(0.299, 0.587, 0.114);

static const int TonemapOperator_Reinhard = 0;
static const int TonemapOperator_Filmic = 1;
static const int TonemapOperator_ACES = 2;

//#endregion

//#region Uniforms

uniform int TonemapOperator
<
	__UNIFORM_COMBO_INT1

	ui_label = "Operator";
	ui_tooltip =
		"Determines the formula used for tonemapping the image.\n"
		"\nDefault: ACES (Unreal Engine 4)";
	ui_items = "Reinhard\0Filmic (Uncharted 2)\0ACES (Unreal Engine 4)\0";
> = 2;

uniform float Amount
<
	__UNIFORM_SLIDER_FLOAT1

	ui_tooltip =
		"Interpolation between the original color and after tonemapping.\n"
		"\nDefault: 1.0";
	ui_category = "Tonemapping";
	ui_min = 0.0;
	ui_max = 2.0;
> = 1.0;

uniform float Exposure
<
	__UNIFORM_SLIDER_FLOAT1

	ui_tooltip =
		"Determines the brightness/camera exposure of the image.\n"
		"Measured in f-stops, thus:\n"
		"  |Dark|     |Neutral|   |Bright|\n"
		"  ... -2.0 -1.0 0.0 +1.0 +2.0 ...\n"
		"\nDefault: 0.0";
	ui_category = "Tonemapping";
	ui_min = -6.0;
	ui_max = 6.0;
> = 0.0;

uniform bool FixWhitePoint
<
	ui_label = "Fix White Point";
	ui_tooltip =
		"Apply brightness correction after tonemapping.\n"
		"\nDefault: On";
	ui_category = "Tonemapping";
> = true;

uniform float2 AdaptRange
<
	__UNIFORM_DRAG_FLOAT2

	ui_label = "Range";
	ui_tooltip =
		"The minimum and maximum values that adaptation can use.\n"
		"Increasing the first value will limit how brighter the image can "
		"become.\n"
		"Decreasing the second value will limit how darker the image can "
		"become.\n"
		"The first value should always be less or equal to the second.\n"
		"\nDefault: 0.0 1.0";
	ui_category = "Adaptation";
	ui_min = 0.001;
	ui_max = 1.0;
	ui_step = 0.001;
> = float2(0.0, 1.0);

uniform float AdaptTime
<
	__UNIFORM_DRAG_FLOAT1

	ui_label = "Time";
	ui_tooltip =
		"The time in seconds that adaptation takes to occur.\n"
		"Setting it to 0.0 makes it instantaneous.\n"
		"\nDefault: 1.0";
	ui_category = "Adaptation";
	ui_min = 0.0;
	ui_max = 3.0;
	ui_step = 0.01;
> = 1.0;

uniform float AdaptSensitivity
<
	__UNIFORM_DRAG_FLOAT1

	ui_label = "Sensititvity";
	ui_tooltip =
		"Determines how sensitive adaptation is to bright lights, making it "
		"less linear.\n"
		"Essentially acts as a multiplier.\n"
		"\nDefault: 1.0";
	ui_category = "Adaptation";
	ui_min = 0.0;
	ui_max = 3.0;
	ui_step = 0.01;
> = 1.0;

uniform int AdaptPrecision
<
	__UNIFORM_SLIDER_INT1

	ui_label = "Precision";
	ui_tooltip =
		"The amount of precision used when determining the overrall brightness "
		"around the screen center point.\n"
		"At 0, the entire scene is accounted for in the process.\n"
		"The maximum value may vary depending on the amount of LODs available "
		"for a given adaptation texture size, but it always results in only "
		"the absolute center of the screen being considered for adaptation.\n"
		"\nDefault: 0";
	ui_category = "Adaptation";
	ui_min = 0;
	ui_max = AdaptMipLevels;
> = 0;

uniform float2 AdaptFocalPoint
<
	__UNIFORM_DRAG_FLOAT2

	ui_label = "Focal Point";
	ui_tooltip =
		"Determines a point in the screen that adaptation will be centered "
		"around.\n"
		"Doesn't really matter when Precision is set to 0, but otherwise "
		"can help focus on things that are not necessarily in the center of "
		"the screen, like the ground.\n"
		"The first value controls the horizontal position, from left to "
		"right.\n"
		"The second value controls the vertical position, from top to "
		"bottom.\n"
		"Set both to 0.5 for the absolute screen center point.\n"
		"\nDefault: 0.5 0.5";
	ui_category = "Adaptation";
	ui_min = 0.0;
	ui_max = 1.0;
	ui_step = 0.001;
> = 0.5;

uniform float FrameTime <source = "frametime";>;

//#endregion

//#region Textures

texture BackBufferTex : COLOR;

sampler BackBuffer_Point
{
	Texture = BackBufferTex;
	SRGBTexture = true;
	MinFilter = POINT;
	MagFilter = POINT;
	MipFilter = POINT;
};

sampler BackBuffer_Linear
{
	Texture = BackBufferTex;
	SRGBTexture = true;
};

texture SmallTex
{
	Width = AdaptResolution.x;
	Height = AdaptResolution.y;
	Format = R32F;
	MipLevels = AdaptMipLevels;
};
sampler Small
{
	Texture = SmallTex;
};

texture LastAdaptTex
{
	Format = R32F;
};
sampler LastAdapt
{
	Texture = LastAdaptTex;
	MinFilter = POINT;
	MagFilter = POINT;
	MipFilter = POINT;
};

//#endregion

//#region Functions

float get_adapt()
{
	return tex2Dlod(
		Small,
		float4(AdaptFocalPoint, 0, AdaptMipLevels - AdaptPrecision)).x;
}

float3 reinhard(float3 color)
{
	return color / (1.0 + color);
}

float3 uncharted2_tonemap(float3 col, float exposure) {
    static const float A = 0.15; //shoulder strength
    static const float B = 0.50; //linear strength
	static const float C = 0.10; //linear angle
	static const float D = 0.20; //toe strength
	static const float E = 0.02; //toe numerator
	static const float F = 0.30; //toe denominator
	static const float W = 11.2; //linear white point value

    col *= exposure;

    col = ((col * (A * col + C * B) + D * E) / (col * (A * col + B) + D * F)) - E / F;
    static const float white = 1.0 / (((W * (A * W + C * B) + D * E) / (W * (A * W + B) + D * F)) - E / F);
    col *= white;
    return col;
}

float3 tonemap(float3 color, float exposure)
{
	switch (TonemapOperator)
	{
		default:
			return 0.0;
		case TonemapOperator_Reinhard:
			return reinhard(color * exposure);
		case TonemapOperator_Filmic:
			return uncharted2_tonemap(color, exposure);
		case TonemapOperator_ACES:
			return ACESFitted(color * exposure);
	}
}

//#endregion

//#region Shaders

void PostProcessVS(
	uint id : SV_VERTEXID,
	out float4 p : SV_POSITION,
	out float2 uv : TEXCOORD)
{
	uv.x = (id == 2) ? 2.0 : 0.0;
	uv.y = (id == 1) ? 2.0 : 0.0;
	p = float4(uv * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
}

float4 GetSmallPS(float4 p : SV_POSITION, float2 uv : TEXCOORD) : SV_TARGET
{
	float adapt = dot(tex2D(BackBuffer_Linear, uv).rgb, LumaWeights);
	adapt *= AdaptSensitivity;

	float last = tex2Dfetch(LastAdapt, 0).x;

	if (AdaptTime > 0.0)
		adapt = lerp(last, adapt, saturate((FrameTime * 0.001) / AdaptTime));

	return adapt;
}

float4 SaveAdaptPS(float4 p : SV_POSITION, float2 uv : TEXCOORD) : SV_TARGET
{
	return get_adapt();
}

void MainVS(
	uint id : SV_VERTEXID,
	out float4 p : SV_POSITION,
	out float2 uv : TEXCOORD0,
	out float inv_white : TEXCOORD1,
	out float exposure : TEXCOORD2)
{
	PostProcessVS(id, p, uv);
	exposure = exp2(Exposure);

	float adapt = get_adapt();
	adapt = clamp(adapt, AdaptRange.x, AdaptRange.y);
	exposure /= adapt;

	inv_white = FixWhitePoint
		? rcp(tonemap(1.0, exposure).x)
		: 1.0;
}

float4 MainPS(
	float4 p : SV_POSITION,
	float2 uv : TEXCOORD0,
	float inv_white : TEXCOORD1,
	float exposure : TEXCOORD2) : SV_TARGET
{
	float4 color = tex2D(BackBuffer_Point, uv);
	color.rgb = lerp(
		color.rgb,
		tonemap(color.rgb, exposure) * inv_white,
		Amount);

	return color;
}

//#endregion

//#region Technique

technique AdaptiveTonemapper
{
	pass GetSmall
	{
		VertexShader = PostProcessVS;
		PixelShader = GetSmallPS;
		RenderTarget = SmallTex;
	}
	pass SaveAdapt
	{
		VertexShader = PostProcessVS;
		PixelShader = SaveAdaptPS;
		RenderTarget = LastAdaptTex;
	}
	pass Main
	{
		VertexShader = MainVS;
		PixelShader = MainPS;
		SRGBWriteEnable = true;
	}
}

//#endregion
