/**
 * Levels version 1.2
 * by Christian Cann Schuldt Jensen ~ CeeJay.dk
 *
 * Allows you to set a new black and a white level.
 * This increases contrast, but clips any colors outside the new range to either black or white
 * and so some details in the shadows or highlights can be lost.
 *
 * The shader is very useful for expanding the 16-235 TV range to 0-255 PC range.
 * You might need it if you're playing a game meant to display on a TV with an emulator that does not do this.
 * But it's also a quick and easy way to uniformly increase the contrast of an image.
 *
 * -- Version 1.0 --
 * First release
 * -- Version 1.1 --
 * Optimized to only use 1 instruction (down from 2 - a 100% performance increase :) )
 * -- Version 1.2 --
 * Added the ability to highlight clipping regions of the image with #define HighlightClipping 1
 */

#include "ReShadeUI.fxh"

uniform int BlackPoint < __UNIFORM_SLIDER_INT1
	ui_min = 0; ui_max = 255;
	ui_label = "Black Point";
	ui_tooltip = "The black point is the new black - literally. Everything darker than this will become completely black.";
> = 16;

uniform int WhitePoint < __UNIFORM_SLIDER_INT1
	ui_min = 0; ui_max = 255;
	ui_label = "White Point";
	ui_tooltip = "The new white point. Everything brighter than this becomes completely white";
> = 235;

uniform bool HighlightClipping <
	ui_label = "Highlight clipping pixels";
	ui_tooltip = "Colors between the two points will stretched, which increases contrast, but details above and below the points are lost (this is called clipping).\n"
		"This setting marks the pixels that clip.\n"
		"Red: Some detail is lost in the highlights\n"
		"Yellow: All detail is lost in the highlights\n"
		"Blue: Some detail is lost in the shadows\n"
		"Cyan: All detail is lost in the shadows.";
> = false;

#include "ReShade.fxh"

float3 LevelsPass(float4 vpos : SV_Position, float2 texcoord : TexCoord) : SV_Target
{
	float black_point_float = BlackPoint / 255.0;
	float white_point_float = WhitePoint == BlackPoint ? (255.0 / 0.00025) : (255.0 / (WhitePoint - BlackPoint)); // Avoid division by zero if the white and black point are the same

	float3 color = tex2D(ReShade::BackBuffer, texcoord).rgb;
	color = color * white_point_float - (black_point_float *  white_point_float);

	if (HighlightClipping)
	{
		float3 clipped_colors;

		clipped_colors = any(color > saturate(color)) // any colors whiter than white?
			? float3(1.0, 0.0, 0.0)
			: color;
		clipped_colors = all(color > saturate(color)) // all colors whiter than white?
			? float3(1.0, 1.0, 0.0)
			: clipped_colors;
		clipped_colors = any(color < saturate(color)) // any colors blacker than black?
			? float3(0.0, 0.0, 1.0)
			: clipped_colors;
		clipped_colors = all(color < saturate(color)) // all colors blacker than black?
			? float3(0.0, 1.0, 1.0)
			: clipped_colors;

		color = clipped_colors;
	}

	return color;
}

technique Levels
{
	pass
	{
		VertexShader = PostProcessVS;
		PixelShader = LevelsPass;
	}
}
