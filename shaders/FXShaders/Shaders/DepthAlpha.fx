//#region Preprocessor

#include "ReShade.fxh"
#include "ReShadeUI.fxh"
#include "FXShaders/Common.fxh"
#include "FXShaders/KeyCodes.fxh"

#ifndef DEPTH_APLHA_SCREENSHOT_KEY
#define DEPTH_APLHA_SCREENSHOT_KEY VK_SNAPSHOT
#endif

//#endregion

//#region Uniforms

FXSHADERS_HELP(
	"This effect allows you to overwrite the transparency of screenshots using "
	"the scene depth as the alpha channel.\n"
	"\n"
	"You'll need a version of ReShade with the option to disable "
	"\"Clear Alpha Channel\" in the screenshot settings.\n"
	"\n"
	"If you use a different key for taking screenshots, it may be useful to "
	"change the value of DEPTH_ALPHA_SCREENSHOT_KEY to something other than "
	"VK_SNAPSHOT (the keycode for the printscreen key).\n"
	"\n"
	"To see which keycodes are available, refer to the \"KeyCodes.fxh\" file "
	"in the FXShaders repository shaders folder."
);

uniform float2 DepthRange
<
	__UNIFORM_DRAG_FLOAT2

	ui_label = "Range";
	ui_tooltip =
		"Defines the depth range for the depth multipliers.\n"
		" - The first value defines the start of the middle depth."
		" - The second value defines the end of the middle depth and the start "
		"of the far depth."
		"\nDefault: 0.0 1.0";
	ui_category = "Depth";
	ui_min = 0.0;
	ui_max = 1.0;
	ui_step = 0.001;
> = float2(0.0, 1.0);

uniform float3 DepthMultiplier
<
	__UNIFORM_DRAG_FLOAT3

	ui_category = "Features";
	ui_label = "Depth Multiplier";
	ui_tooltip =
		"Multipliers for the transparency of near, middle and far depth "
		"respectively.\n"
		"Set to 0.0 to make a range completely transparent, 1.0 to make it "
		"completely opaque.\n"
		"\nDefault: 1.0 0.5 0.0";
	ui_min = 0.0;
	ui_max = 1.0;
	ui_step = 0.001;
> = float3(1.0, 0.5, 0.0);

uniform float DepthSmoothness
<
	__UNIFORM_DRAG_FLOAT1

	ui_category = "Features";
	ui_label = "Depth Smoothness";
	ui_tooltip =
		"Smoothness of the transitions between depth multiplier ranges.\n"
		"\nDefault: 1.0";
	ui_min = 0.0;
	ui_max = 1.0;
	ui_step = 0.001;
> = 1.0;

uniform bool ClearPreviousAlpha
<
	ui_category = "Features";
	ui_label = "Clear Previous Alpha";
	ui_tooltip =
		"Determines whether the effect should reset the alpha of the image to"
		"100% opaque before applying the effect or maintain it (applying the "
		"effect over the previous alpha).\n"
		"\nDefault: Off";
> = true;

uniform bool ColorizeTransparency
<
	ui_category = "Debug";
	ui_label = "Colorize Transparency";
	ui_tooltip =
		"Preview transparency by colorizing it.\n"
		"This option is automatically disabled when the key defined in "
		"DEPTH_ALPHA_SCREEENSHOT_KEY is being held down, which is the "
		"printscreen key by default.\n"
		"There's a small chance that the test color might still appear in the "
		"screenshot, but keeping the screenshot key held for a small time "
		"seems to help.\n"
		"You can also simply disable this option before taking a screenshot.\n"
		"\nDefault: Off";
> = false;

uniform float4 TestColor
<
	__UNIFORM_COLOR_FLOAT4

	ui_category = "Debug";
	ui_label = "Transparency Test Color";
	ui_tooltip =
		"The color used when Colorize Transparency is enabled.\n"
		"The alpha channel of this color has nothing to do with the final "
		"result, it merely serves to determine the opacity of this color while "
		"testing.\n"
		"\nDefault: 0 0 0 255";
> = float4(0.0, 0.0, 0.0, 1.0);

uniform bool IsScreenshotKeyDown
<
	source = "key";
	keycode = DEPTH_APLHA_SCREENSHOT_KEY;
>;

//#endregion

//#region Functions

void ApplyDepthToAlpha(inout float alpha, float depth)
{
	// if (DepthCurve.x < DepthCurve.y)
	// 	depth = smoothstep(DepthCurve.x, DepthCurve.y, 1.0 - depth);
	// else
	// 	depth = smoothstep(DepthCurve.y, DepthCurve.x, depth);

	float is_near = smoothstep(
		depth - DepthSmoothness,
		depth + DepthSmoothness,
		DepthRange.x);

	float is_far = smoothstep(
		DepthRange.y - DepthSmoothness,
		DepthRange.y + DepthSmoothness, depth);

	float is_middle = (1.0 - is_near) * (1.0 - is_far);

	alpha *= lerp(1.0, DepthMultiplier.x, is_near);
	alpha *= lerp(1.0, DepthMultiplier.y, is_middle);
	alpha *= lerp(1.0, DepthMultiplier.z, is_far);
}

//#endregion

//#region Shaders

float4 MainPS(float4 p : SV_POSITION, float2 uv : TEXCOORD) : SV_TARGET
{
	float4 color = tex2D(ReShade::BackBuffer, uv);
	color.a = ClearPreviousAlpha ? 1.0 : color.a;

	float depth = ReShade::GetLinearizedDepth(uv);
	ApplyDepthToAlpha(color.a, depth);

	if (!IsScreenshotKeyDown && ColorizeTransparency)
		color.rgb = lerp(
			lerp(color.rgb, TestColor.rgb, TestColor.a),
			color.rgb,
			color.a);

	return color;
}

//#endregion

//#region Technique

technique DepthAlpha
<
	ui_tooltip =
		"Effect that uses depth as transparency to allow for screenshots with "
		"transparency, like a chroma-key.";
>
{
	pass
	{
		VertexShader = PostProcessVS;
		PixelShader = MainPS;
	}
}

//#endregion
