//#region Includes

#include "ReShade.fxh"
#include "ReShadeUI.fxh"

//#endregion

//#region Uniforms

uniform int _Help
<
	ui_text =
		"This effect generates a checkerboard pattern on the screen, which can "
		"help testing overlay effects.\n"
		"\n"
		"To use it effectively, place it above the effect you wish to test, "
		"like a vignette effect.\n"
		"\n"
		"You can change the color opacity for either of the two tile colors "
		"separately.\n"
		"\n"
		"Note that some tile sizes may not align properly to the bottom "
		"right.\n"
		;
	ui_category = "Help";
	ui_category_closed = true;
	ui_label = " ";
	ui_type = "radio";
>;

uniform float TileSize
<
	__UNIFORM_DRAG_FLOAT1

	ui_label = "Size";
	ui_tooltip =
		"The size of the checkerboard tiles.\n"
		"\nDefault: 20";
	ui_category = "Tiles";
	ui_min = 1.0;
	ui_max = 100.0;
	ui_step = 1.0;
> = 20.0;

uniform float4 TileColorA
<
	__UNIFORM_COLOR_FLOAT4

	ui_label = "Color A";
	ui_tooltip =
		"The odd color of the pattern.\n"
		"\nDefault: 89 89 89 255";
	ui_category = "Tiles";
> = float4(0.35, 0.35, 0.35, 1.0);

uniform float4 TileColorB
<
	__UNIFORM_COLOR_FLOAT4

	ui_label = "Color B";
	ui_tooltip =
		"The even color of the pattern.\n"
		"\nDefault: 166 166 166 255";
	ui_category = "Tiles";
> = float4(0.65, 0.65, 0.65, 1.0);

//#endregion

//#region Shaders

float4 MainPS(float4 p : SV_POSITION, float2 uv : TEXCOORD) : SV_TARGET
{
	float4 color = tex2D(ReShade::BackBuffer, uv);
	float2 coord = uv * ReShade::ScreenSize;
	
	float size = TileSize * 2.0;
	float inv_size = rcp(size);

	float2 tiles = coord % size * inv_size;
	tiles = step(0.5, tiles);

	float4 checkerboard = (tiles.x != tiles.y) ? TileColorA : TileColorB;	
	color.rgb = lerp(color.rgb, checkerboard.rgb, checkerboard.a);

	return color;
}

//#endregion

//#region Technique

technique Checkerboard
<
	ui_tooltip =
		"Display a simple checkerboard pattern to test overlay effects.\n"
		"Place it above the effect you wish to test.\n"
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