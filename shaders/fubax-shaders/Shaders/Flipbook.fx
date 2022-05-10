/*
Flipbook Animation PS (c) 2018 Jacob Maximilian Fober

This work is licensed under the Creative Commons
Attribution-ShareAlike 4.0 International License.
To view a copy of this license, visit
http://creativecommons.org/licenses/by-sa/4.0/
*/

// version 1.2.2


	  ////////////
	 /// MENU ///
	////////////

#include "ReShadeUI.fxh"

uniform int3 Size < __UNIFORM_INPUT_INT3
	ui_label = "X frames, Y frames, FPS";
	ui_tooltip = "Adjust flipbook texture dimensions and framerate\n"
		"To change texture resolution and name,\n"
		"add following preprocessor definition:\n"
		"  FLIPBOOK_TEXTURE 'name.png'\n"
		"  FLIPBOOK_SIZE_X [ResolutionX]\n"
		"  FLIPBOOK_SIZE_Y [ResolutionY]";
	#if __RESHADE__ < 40000
		ui_step = 0.2;
	#endif
	ui_min = 1; ui_max = 30;
	ui_category = "Texture dimensions";
> = int3(10, 9, 30);

uniform float3 Position < __UNIFORM_SLIDER_FLOAT3
	ui_label = "X position, Y position, Scale";
	ui_tooltip = "Adjust flipbook texture size and position";
	#if __RESHADE__ < 40000
		ui_step = 0.002;
	#endif
	ui_min = float3(0.0, 0.0, 0.1); ui_max = float3(1.0, 1.0, 1.0);
	ui_category = "Position on screen";
> = float3(1.0, 0.0, 1.0);

// Get time in milliseconds from start
uniform float timer < source = "timer"; >;

//-------------------------------------------------------------------------------------------------

#ifndef FLIPBOOK_PIXELATED
	#define FLIPBOOK_PIXELATED 0
#endif

#ifndef FLIPBOOK_TEXTURE
	#define FLIPBOOK_TEXTURE "waow.png" // Texture file name
#endif

#ifndef FLIPBOOK_SIZE_X
	#define FLIPBOOK_SIZE_X 2550 // Texture horizontal resolution
#endif

#ifndef FLIPBOOK_SIZE_Y
	#define FLIPBOOK_SIZE_Y 1710 // Texture vertical resolution
#endif

//-------------------------------------------------------------------------------------------------


	  //////////////
	 /// SHADER ///
	//////////////

texture FlipbookTex < source = FLIPBOOK_TEXTURE; > {Width = FLIPBOOK_SIZE_X; Height = FLIPBOOK_SIZE_Y;};
sampler FlipbookSampler
{
	Texture = FlipbookTex;
	AddressU = REPEAT;
	AddressV = REPEAT;
	#if FLIPBOOK_PIXELATED==1
		MagFilter = POINT;
		MinFilter = POINT;
	#endif
};

#include "ReShade.fxh"

float Mask(float2 Coord)
{
	Coord = abs(Coord*2.0-1.0);
	float2 Pixel = fwidth(Coord);
	float2 Borders = 1.0 - smoothstep(1.0-Pixel, 1.0+Pixel, Coord);
	return min(Borders.x, Borders.y);
}

float3 FlipbookPS(float4 vois : SV_Position, float2 texcoord : TexCoord) : SV_Target
{
	float ScreenAspect = ReShade::AspectRatio;
	// Screen aspect divided by animation frame aspect
	float AspectDifference = (ScreenAspect*float(Size.x*FLIPBOOK_SIZE_Y))/float(Size.y*FLIPBOOK_SIZE_X);

	// Scale coordinates
	float2 Scale = 1.0/Position.z;
	float2 ScaledCoord = texcoord * Scale;

	// Adjust aspect ratio
	if(AspectDifference > 1.0)
	{
		ScaledCoord.x *= AspectDifference;
		Scale.x *= AspectDifference;
	}
	else if(AspectDifference < 1.0)
	{
		ScaledCoord.y /= AspectDifference;
		Scale.y /= AspectDifference;
	}

	// Offset coordinates
	ScaledCoord += (1.0-Scale)*float2(Position.x, 1.0-Position.y);

	float BorderMask = Mask(ScaledCoord);
	// Frame time in milliseconds
	float FramerateInMs = 1000.0 / Size.z;
	float2 AnimationCoord = ScaledCoord / Size.xy;
	// Sample UVs for horizontal and vertical frames
	AnimationCoord.x += frac(floor(timer / FramerateInMs) / Size.x);
	AnimationCoord.y += frac(floor( timer / (FramerateInMs * Size.x) )/Size.y);

	// Sample display image
	float3 Display = tex2D(ReShade::BackBuffer, texcoord).rgb;
	// Sample flipbook texture
	float4 AnimationTexture = tex2D(FlipbookSampler, AnimationCoord);

	return lerp(Display, AnimationTexture.rgb, AnimationTexture.a * BorderMask);
}


	  //////////////
	 /// OUTPUT ///
	//////////////

technique Flipbook < ui_tooltip = "Flipbook animation FX:\n"
	"======================\n"
	"To change texture resolution and name,\n"
	"add following preprocessor definition:\n"
	"  flipbook 'name.png'\n"
	"  FLIPBOOK_SIZE_X [ResolutionX]\n"
	"  FLIPBOOK_SIZE_Y [ResolutionY]"; >
{
	pass
	{
		VertexShader = PostProcessVS;
		PixelShader = FlipbookPS;
	}
}
