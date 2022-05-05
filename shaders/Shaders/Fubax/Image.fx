/*
DisplayImage PS (c) 2019 Jacob Maximilian Fober

This work is licensed under the Creative Commons 
Attribution-ShareAlike 4.0 International License. 
To view a copy of this license, visit 
http://creativecommons.org/licenses/by-sa/4.0/.
*/

// version 1.1.0


	  ////////////
	 /// MENU ///
	////////////

#ifndef TestImage
	#define TestImage "image.png" // Image file name
#endif
#ifndef TestImageX
	#define TestImageX 1440 // Image horizontal resolution
#endif
#ifndef TestImageY
	#define TestImageY 1080 // Image vertical resolution
#endif

uniform bool AspectCorrect <
	ui_label = "Preserve aspect ratio";
	ui_tooltip = "To change image source add following to preprocessor definitions:\n Image 'filename.jpg'\n ImageX [horizontal resolution]\n ImageY [vertical resolution]";
> = true;


	  //////////////
	 /// SHADER ///
	//////////////

#include "ReShade.fxh"

// Define image texture
texture ImageTex < source = TestImage; > {Width = TestImageX; Height = TestImageY;};
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
	float ImageAspect = float(TestImageX)/float(TestImageY);
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

technique ImageTest < ui_label = "TEST image"; ui_tooltip = "To change image file,\n"
"define global preprocessor definition:\n"
"  TestImage 'image.png'\n"
"  TestImageX 1440\n"
"  TestImageY 1080\n"; >
{
	pass
	{
		VertexShader = PostProcessVS;
		PixelShader = ImagePS;
	}
}
