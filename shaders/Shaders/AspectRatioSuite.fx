// #region Includes

#include "ReShade.fxh"
#include "ReShadeUI.fxh"

// #endregion

// #region Macros

#ifndef ASPECT_RATIO_SUITE_VALUES
#define ASPECT_RATIO_SUITE_VALUES 9/16., 1, 4/3., 16/9., 64/27., 32/9.
#endif

// #endregion

// #region Data Types

struct Mask
{
	float value, is_horizontal;
};

struct Rect
{
	float2 pos, size;
};

Rect Rect_new(float4 value)
{
	Rect r;
	r.pos = value.xy;
	r.size = value.zw;
	
	return r;
}

Rect Rect_new(float2 pos, float2 size)
{
	return Rect_new(float4(pos, size));
}

Rect Rect_new(float x, float y, float width, float height)
{
	return Rect_new(float4(x, y, width, height));
}

float4 Rect_values(Rect r)
{
	return float4(r.pos, r.size);
}

float Rect_contains(Rect r, float2 p)
{
	return
		step(r.pos.x, p.x) * step(r.pos.y, p.y) *
		step(p.x, r.pos.x + r.size.x) * step(p.y, r.pos.y + r.size.y);
}

// #endregion

// #region Constants

static const float ASPECT_RATIOS[] = {ASPECT_RATIO_SUITE_VALUES, 0.0};

// #endregion

// #region Uniforms

uniform int Mode
<
	__UNIFORM_RADIO_INT1
	ui_text =
		"Set the mode you want to use and it's appropriate parameters:\n"
		" - Letterbox: Displays bars on the image to 'correct' aspect ratio.\n"
		" - Test Box: Displays a box in the center of the screen representing "
		"the selected aspect ratio.\n"
		" - Multi-Test Box: Same as Test Box but shows multiple boxes from "
		"aspect ratios defined in the ASPECT_RATIO_SUITE_VALUES macro.\n"
		"\nThe macro for Multi-Test Box mode is defined as a comma-separated "
		"list of aspect ratios, which can be defined as the division of two "
		"decimal numbers.\n"
		"\nFor example, '4.0 / 3.0, 16.0 / 9.0, 1.0', would tell the effect "
		"to display boxes for 4:3, 16:9 and 1:1 aspect ratios.\n"
		"\nFor short-handing, you could also define the same values as "
		"'4/3., 16/9., 1', since you only need one of the numbers in the "
		"division to be decimal and it's decimal places can be omitted.\n"
		"\nRemember you can also pass the mouse cursor over a parameter's name "
		"to see a more detailed description and it's default value.\n"
		"\n";
	ui_label = "Mode";
	ui_tooltip = "Default: Letterbox";
	ui_items = "Letterbox\0Test Box\0Multi-Test Box\0";
> = 0;

uniform float2 AspectRatio
<
	__UNIFORM_DRAG_FLOAT2
	ui_label = "Aspect Ratio";
	ui_tooltip =
		"The aspect ratio to calculate the border/box from.\n"
		"Unused for Multi-Test mode, which uses the "
		"ASPECT_RATIO_SUITE_VALUES macro.\n"
		"\nDefault: 4.0 3.0";
	ui_min = 1;
	ui_max = 100;
	ui_step = 0.01;
> = float2(4.0, 3.0);

uniform float4 LetterBoxColor
<
	__UNIFORM_COLOR_FLOAT4
	ui_label = "Letterbox Color";
	ui_tooltip =
		"Color of the bars that appear in Letterbox mode.\n"
		"\nDefault: 0 0 0 255";
> = float4(0.0, 0.0, 0.0, 1.0);

uniform float4 BackgroundColor
<
	__UNIFORM_COLOR_FLOAT4
	ui_label = "Background Color";
	ui_tooltip =
		"Background color applied in either test mode.\n"
		"To disable, simply set the alpha value to zero.\n"
		"\nDefault: 0 0 0 0";
> = float4(0.0, 0.0, 0.0, 0.0);

uniform float TestBoxSize
<
	__UNIFORM_SLIDER_FLOAT1
	ui_label = "Test Box Size";
	ui_tooltip =
		"Size of the box(es) that appear in either test mode.\n"
		"1.0: full screen size\n"
		"0.5: half screen size\n"
		"0.0: hidden\n"
		"\nDefault: 0.5";
	ui_min = 0.0;
	ui_max = 1.0;
> = 1.0;

uniform float4 TestBoxColor
<
	__UNIFORM_COLOR_FLOAT4
	ui_label = "Test Box Color";
	ui_tooltip =
		"Color of the Test Box mode's aspect ratio box.\n"
		"The alpha value is used for transparency in Multi-Test Box mode.\n"
		"\nDefault: 255 0 0 255";
> = float4(1.0, 0.0, 1.0, 1.0);

// #endregion

// #region Functions

/*
	Scales a coordinate while keeping it anchored to a pivot.
*/
float2 scale_uv(float2 uv, float2 scale, float2 pivot)
{
	return (uv - pivot) * scale + pivot;
}

/*
	Convert an HSV color to RGB.
*/
float3 hsv_to_rgb(float3 hsv)
{
	float4 k = float4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
	float3 p = abs(frac(hsv.xxx + k.xyz) * 6.0 - k.www);
	return hsv.z * lerp(k.xxx, saturate(p - k.xxx), hsv.y);
}

/*
	Check if x is between a and b.
*/
float inside(float x, float a, float b)
{
	return step(a, x) * step(x, b);
}

/*
	Get a mask based on the difference between the screen aspect ratio and
	another one.
*/
Mask calc_mask(float ar)
{
	Mask mask;
	mask.is_horizontal = step(ReShade::AspectRatio, ar);
	
	float ratio = lerp(
		ar / ReShade::AspectRatio,
		ReShade::AspectRatio / ar,
		mask.is_horizontal
	);
	mask.value = (1.0 - ratio) * 0.5;

	return mask;
}

/*
	Calculate a border/letterbox from a mask calculated from calc_mask().

	Returns whether or not the current coordinate is outside the borders.
*/
float calc_border(float2 uv, Mask mask)
{
	// The uv component to use (mask.y tells whether the mask is vertical or
	// horizontal).
	float pos = lerp(uv.x, uv.y, mask.is_horizontal);

	return step(mask.value, pos) * step(pos, 1.0 - mask.value);
}

/*
	Calculate a box from the aspect ratio mask from calc_mask().

	Returns whether the uv coordinate is within the box's outline.
*/
float calc_box(float2 uv, Mask m, float scale)
{
	float4 uv_ps = float4(uv, ReShade::PixelSize);
	uv_ps = lerp(uv_ps, uv_ps.yxwz, m.is_horizontal);

	m.value = lerp(0.5, m.value, scale);

	Rect r = Rect_new(
		m.value,
		0.5 - scale * 0.5,
		1.0 - (2.0 * m.value),
		scale
	);

	return
		Rect_contains(r, uv_ps.xy) !=
		Rect_contains(
			Rect_new(r.pos + uv_ps.zw, r.size - uv_ps.zw * 2.0),
			uv_ps.xy
		);
}

// #endregion

// #region Effect

float4 MainPS(float4 p : SV_POSITION, float2 uv : TEXCOORD) : SV_TARGET
{
	float4 color = tex2D(ReShade::BackBuffer, uv);

	switch (Mode)
	{
		case 0: // Border
		{
			Mask mask = calc_mask(AspectRatio.x / AspectRatio.y);
			float border = calc_border(uv, mask);

			color.rgb = lerp(
				LetterBoxColor.rgb,
				color.rgb,
				lerp(1.0, border, LetterBoxColor.a)
			);
		} break;
		case 1: // Test Box
		{
			color.rgb = lerp(color.rgb, BackgroundColor.rgb, BackgroundColor.a);

			Mask mask = calc_mask(AspectRatio.x / AspectRatio.y);
			
			float box = calc_box(uv, mask, TestBoxSize);
			box *= TestBoxColor.a;

			color.rgb = lerp(color.rgb, TestBoxColor.rgb, box);
		} break;
		case 2: // Multi-Test
		{
			color.rgb = lerp(color.rgb, BackgroundColor.rgb, BackgroundColor.a);

			for (int i = 0; ASPECT_RATIOS[i] != 0.0; ++i)
			{
				float3 ratio_color = hsv_to_rgb(float3(i / 10.0, 1.0, 1.0));
				
				Mask mask = calc_mask(ASPECT_RATIOS[i]);
				
				float box = calc_box(uv, mask, TestBoxSize);
				box *= TestBoxColor.a;

				color.rgb = lerp(color.rgb, ratio_color, box);
			}
		} break;
	}

	return color;
}

technique AspectRatioSuite
{
	pass
	{
		VertexShader = PostProcessVS;
		PixelShader = MainPS;
	}
}

// #endregion