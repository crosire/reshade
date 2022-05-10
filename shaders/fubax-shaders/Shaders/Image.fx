/*
DisplayImage PS (c) 2019 Jacob Maximilian Fober

This work is licensed under the Creative Commons
Attribution-ShareAlike 4.0 International License.
To view a copy of this license, visit
http://creativecommons.org/licenses/by-sa/4.0/.
*/

// version 1.1.1

  ////////////
 /// MENU ///
////////////

// Image file name
#ifndef TEST_IMAGE_FILE
	#define TEST_IMAGE_FILE "image.png"
#endif
// Image horizontal resolution
#ifndef TEST_IMAGE_X
	#define TEST_IMAGE_X 1440
#endif
// Image vertical resolution
#ifndef TEST_IMAGE_Y
	#define TEST_IMAGE_Y 1080
#endif

uniform bool AspectCorrect <
	ui_label = "Preserve aspect ratio";
	ui_tooltip =
		"To change image source add following to preprocessor definitions:\n"
		"TEST_IMAGE_FILE 'filename.jpg'\n"
		"TEST_IMAGE_X [horizontal resolution]\n"
		"TEST_IMAGE_Y [vertical resolution]";
> = true;

  //////////////
 /// SHADER ///
//////////////

#include "ReShade.fxh"

// Define image texture
texture ImageTex < source = TEST_IMAGE_FILE; > {Width = TEST_IMAGE_X; Height = TEST_IMAGE_Y;};
sampler ImageSampler { Texture = ImageTex; };

// Anti-aliased border
float Border(float2 Coordinates)
{
	Coordinates = abs(Coordinates*2-1); // Radial Coordinates
	float2 Pixel = fwidth(Coordinates);
	Coordinates = smoothstep(1.0+Pixel, 1.0-Pixel, Coordinates);
	return min(Coordinates.x, Coordinates.y);
}

// Draw Image
float3 ImagePS(float4 vois : SV_Position, float2 texcoord : TexCoord) : SV_Target
{
	float3 Display = tex2D(ReShade::BackBuffer, texcoord).rgb;
	float4 ImageTex;
	// Bypass aspect ratio correction
	if(!AspectCorrect)
	{
		ImageTex = tex2D(ImageSampler, texcoord);
		return lerp(Display, ImageTex.rgb, ImageTex.a);
	}

	float DisplayAspect = ReShade::AspectRatio;
	float ImageAspect = float(TEST_IMAGE_X)/float(TEST_IMAGE_Y);
	float AspectDifference = DisplayAspect / ImageAspect;

	if(AspectDifference > 1.0)
	{
		texcoord.x -= 0.5;
		texcoord.x *= AspectDifference;
		texcoord.x += 0.5;
	}
	else if(AspectDifference < 1.0)
	{
		texcoord.y -= 0.5;
		texcoord.y /= AspectDifference;
		texcoord.y += 0.5;
	}
	else
	{
		ImageTex = tex2D(ImageSampler, texcoord);
		return lerp(Display, ImageTex.rgb, ImageTex.a);
	}

	ImageTex = tex2D(ImageSampler, texcoord);

	// Sample image
	return lerp(
		0.0,
		lerp(Display, ImageTex.rgb, ImageTex.a),
		Border(texcoord)
	);
}

  //////////////
 /// OUTPUT ///
//////////////

technique ImageTest <
	ui_label = "TEST image";
	ui_tooltip =
		"To change image file,\n"
		"define global preprocessor definition:\n"
		"  TEST_IMAGE 'image.png'\n"
		"  TEST_IMAGE_X 1440\n"
		"  TEST_IMAGE_Y 1080";
>
{
	pass
	{
		VertexShader = PostProcessVS;
		PixelShader = ImagePS;
	}
}
