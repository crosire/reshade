//#region Includes

#include "ReShade.fxh"
#include "ReShadeUI.fxh"

//#endregion

//#region Constants

static const float Pi = 3.14159;
static const float HalfPi = Pi * 0.5;

static const int BlendMode_Mix = 0;
static const int BlendMode_Multiply = 1;
static const int BlendMode_DarkenOnly = 2;
static const int BlendMode_LightenOnly = 3;
static const int BlendMode_Overlay = 4;
static const int BlendMode_Screen = 5;
static const int BlendMode_HardLight = 6;
static const int BlendMode_SoftLight = 7;

static const int VignetteShape_None = 0;
static const int VignetteShape_Radial = 1;
static const int VignetteShape_TopBottom = 2;
static const int VignetteShape_LeftRight = 3;
static const int VignetteShape_Box = 4;
static const int VignetteShape_Sky = 5;
static const int VignetteShape_Ground = 6;

//#endregion

//#region Uniforms

uniform int _Help
<
	ui_text =
		"This effect provides a flexible way to create a vignatte overlay.\n"
		"\n"
		"Specific help for each option can be found by moving the mouse over "
		"the option's name.\n"
		"\n"
		"The appearance can be controlled using a color, with opacity support "
		"through the alpha channel, and a blending mode, like Photoshop/GIMP.\n"
		"\n"
		"Various shapes can be used, with adjustable aspect ratio and gradient "
		"points.\n"
		;
	ui_category = "Help";
	ui_category_closed = true;
	ui_label = " ";
	ui_type = "radio";
>;

uniform float4 VignetteColor
<
	__UNIFORM_COLOR_FLOAT4

	ui_label = "Color";
	ui_tooltip =
		"Color of the vignette.\n"
		"Supports opacity control through the alpha channel.\n"
		"\nDefault: 0 0 0 255";
	ui_category = "Appearance";
> = float4(0.0, 0.0, 0.0, 1.0);

uniform int BlendMode
<
	__UNIFORM_COMBO_INT1

	ui_label = "Blending Mode";
	ui_tooltip =
		"Determines the way the vignette is blended with the image.\n"
		"\nDefault: Mix";
	ui_category = "Appearance";
	ui_items =
		"Mix\0Multiply\0Darken Only\0Lighten Only\0Overlay\0Screen\0"
		"Hard Light\0Soft Light\0";
> = 0;

uniform float2 VignetteStartEnd
<
	__UNIFORM_DRAG_FLOAT2

	ui_label = "Start/End";
	ui_tooltip =
		"The start and end points of the vignette gradient.\n"
		"The longer the distance, the smoother the vignette effect is.\n"
		"\nDefault: 0.0 1.0";
	ui_category = "Shape";
	ui_min = 0.0;
	ui_max = 3.0;
	ui_step = 0.01;
> = float2(0.0, 1.0);

uniform float VignetteRatio
<
	__UNIFORM_SLIDER_FLOAT1

	ui_label = "Ratio";
	ui_tooltip =
		"The aspect ratio of the vignette.\n"
		"0.0: Anamorphic.\n"
		"1.0: Corrected.\n"
		"\n"
		"For example, with 1.0 the radial shape produces a perfect circle.\n"
		"\nDefault: 0.0";
	ui_category = "Shape";
	ui_min = 0.0;
	ui_max = 1.0;
> = 0.0;

uniform int VignetteShape
<
	__UNIFORM_COMBO_INT1

	ui_label = "Shape";
	ui_tooltip =
		"The shape of the vignette.\n"
		"\nDefault: Radial";
	ui_category = "Shape";
	ui_items = "None\0Radial\0Top/Bottom\0Left/Right\0Box\0Sky\0Ground\0";
> = 1;

//#endregion

//#region Shaders

float4 MainPS(float4 p : SV_POSITION, float2 uv : TEXCOORD) : SV_TARGET
{
	float4 color = tex2D(ReShade::BackBuffer, uv);

	float2 ratio = (ReShade::AspectRatio > 1.0)
		? float2(BUFFER_WIDTH * BUFFER_RCP_HEIGHT, 1.0)
		: float2(1.0, BUFFER_HEIGHT * BUFFER_RCP_WIDTH);
	float2 ratio_uv = (uv - 0.5) * ratio + 0.5;

	uv = lerp(uv, ratio_uv, VignetteRatio);

	float vignette = 1.0;

	switch (VignetteShape)
	{
		case VignetteShape_Radial:
			vignette = distance(0.5, uv) * HalfPi;
			break;
		case VignetteShape_TopBottom:
			vignette = abs(uv.y - 0.5) * 2.0;
			break;
		case VignetteShape_LeftRight:
			vignette = abs(uv.x - 0.5) * 2.0;
			break;
		case VignetteShape_Box:
			float2 vig = abs(uv - 0.5) * 2.0;
			vignette = max(vig.x, vig.y);
			break;
		case VignetteShape_Sky:
			vignette = distance(float2(0.5, 1.0), uv);
			break;
		case VignetteShape_Ground:
			vignette = distance(float2(0.5, 0.0), uv);
			break;
	}

	vignette = smoothstep(VignetteStartEnd.x, VignetteStartEnd.y, vignette);

	float3 vig_color;

	switch (BlendMode)
	{
		case BlendMode_Mix:
			vig_color = VignetteColor.rgb;
			break;
		case BlendMode_Multiply:
			vig_color = color.rgb * VignetteColor.rgb;
			break;
		case BlendMode_DarkenOnly:
			vig_color = min(color.rgb, VignetteColor.rgb);
			break;
		case BlendMode_LightenOnly:
			vig_color = max(color.rgb, VignetteColor.rgb);
			break;
		case BlendMode_Overlay:
			vig_color = (color.rgb < 0.5)
				? 2.0 * color.rgb * VignetteColor.rgb
				: 1.0 - 2.0 * (1.0 - color.rgb) * (1.0 - VignetteColor.rgb);
			break;
		case BlendMode_Screen:
			vig_color = 1.0 - (1.0 - color.rgb) * (1.0 - VignetteColor.rgb);
			break;
		case BlendMode_HardLight:
			vig_color = (color.rgb > 0.5)
				? 2.0 * color.rgb * VignetteColor.rgb
				: 1.0 - 2.0 * (1.0 - color.rgb) * (1.0 - VignetteColor.rgb);
			break;
		case BlendMode_SoftLight:
			vig_color =
				(1.0 - 2.0 * VignetteColor.rgb) * (color.rgb * color.rgb) +
				2.0 * VignetteColor.rgb * color.rgb;
			break;
	}

	color.rgb = lerp(color.rgb, vig_color, vignette * VignetteColor.a);

	return color;
}

//#endregion

//#region Technique

technique ArtisticVignette
<
	ui_tooltip =
		"Flexible vignette overlay effect with multiple shapes and blend modes."
		;
>
{
	pass
	{
		VertexShader = PostProcessVS;
		PixelShader = MainPS;
	}
}

//#endregion