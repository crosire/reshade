/*
Display LUT PS v1.3.3 (c) 2018 Jacob Maximilian Fober;
Apply LUT PS v2.0.0 (c) 2018 Jacob Maximilian Fober,
(remix of LUT shader 1.0 (c) 2016 Marty McFly)

This work is licensed under the Creative Commons
Attribution-ShareAlike 4.0 International License.
To view a copy of this license, visit
http://creativecommons.org/licenses/by-sa/4.0/.
*/


  ////////////
 /// MENU ///
////////////

// Define LUT texture size
#ifndef LUT_BLOCK_SIZE
	#define LUT_BLOCK_SIZE 32
#endif
// Define LUT texture name
#ifndef LUT_FILE_NAME
	#define LUT_FILE_NAME "lut.png"
#endif
// Define LUT orientation
#ifndef LUT_VERTICAL
	#define LUT_VERTICAL false
#endif

#include "ReShadeUI.fxh"

uniform int LutRes <
	ui_label = "LUT box resolution";
	ui_tooltip = "Horizontal resolution equals value squared.\n"
		"Default 32 is 1024.\n"
		"To set texture size and name for ApplyLUT, define\n"
		" LUT_BLOCK_SIZE [number]\n"
		"and \n"
		" LUT_FILE_NAME [name]";
	ui_type = "drag";
	ui_category = "Display LUT settings";
	ui_min = 8; ui_max = 128; ui_step = 1;
> = 32;

uniform bool VerticalOrietation < __UNIFORM_INPUT_BOOL1
	ui_label = "Vertical LUT";
	ui_tooltip = "Select LUT texture orientation, default is horizontal.\n"
		"To change orientation for input LUT, add PreProcessor definition 'LUT_VERTICAL true'.";
	ui_category = "Display LUT settings";
> = false;

uniform float2 LutChromaLuma < __UNIFORM_SLIDER_FLOAT2
	ui_label = "LUT chroma/luma blend";
	ui_tooltip = "How much LUT affects chrominance/luminance";
	ui_category = "Apply LUT settings";
	ui_min = 0.0; ui_max = 1.0; ui_step = 0.005;
> = float2(1.0, 1.0);

  /////////////////
 /// FUNCTIONS ///
/////////////////

// Convert 3D LUT texel coordinates to 2D textel coordinates
int2 toLut2D(int3 lut3D)
{
	#if LUT_VERTICAL
		return int2(lut3D.x, lut3D.y+lut3D.z);
	#else
		return int2(lut3D.x+lut3D.z, lut3D.y);
	#endif
}

  //////////////
 /// SHADER ///
//////////////

// LUT texture for Apply Lut PS
#if LUT_VERTICAL
	#define LUT_DIMENSIONS int2(LUT_BLOCK_SIZE, LUT_BLOCK_SIZE*LUT_BLOCK_SIZE)
#else
	#define LUT_DIMENSIONS int2(LUT_BLOCK_SIZE*LUT_BLOCK_SIZE, LUT_BLOCK_SIZE)
#endif
#define LUT_PIXEL_SIZE 1.0/LUT_DIMENSIONS
texture LUTTex < source = LUT_FILE_NAME;>{ Width = LUT_DIMENSIONS.x; Height = LUT_DIMENSIONS.y; Format = RGBA8; };
sampler LUTSampler {Texture = LUTTex; Format = RGBA8;};


#include "ReShade.fxh"

// Shader No.1 pass
float3 DisplayLutPS(float4 vois : SV_Position, float2 TexCoord : TEXCOORD) : SV_Target
{
	// Calculate LUT texture bounds
	float2 LutBounds;
	if (VerticalOrietation)
		LutBounds = float2(LutRes, LutRes*LutRes);
	else
		LutBounds = float2(LutRes*LutRes, LutRes);
	LutBounds *= BUFFER_PIXEL_SIZE;

	if( any(TexCoord>=LutBounds) ) return tex2D(ReShade::BackBuffer, TexCoord).rgb;
	else
	{
		// Generate pattern UV
		float2 Gradient = TexCoord*BUFFER_SCREEN_SIZE/LutRes;
		// Convert pattern to RGB LUT
		float3 LUT;
		LUT.rg = frac(Gradient)-0.5/LutRes;
		LUT.rg /= 1.0-1.0/LutRes;
		LUT.b = floor(VerticalOrietation? Gradient.g : Gradient.r)/(LutRes-1);
		// Display LUT texture
		return LUT;
	}
}

// Shader No.2 pass
void ApplyLutPS(float4 vois : SV_Position, float2 TexCoord : TEXCOORD, out float3 Image : SV_Target)
{
	// Grab background color
	Image = tex2D(ReShade::BackBuffer, TexCoord).rgb;

	// Convert to sub pixel coordinates
	float3 lut3D = Image*(LUT_BLOCK_SIZE-1);

	// Get 2D LUT coordinates
	float2 lut2D[2];
	#if LUT_VERTICAL
		// Front
		lut2D[0].x = lut3D.x;
		lut2D[0].y = floor(lut3D.z)*LUT_BLOCK_SIZE+lut3D.y;
		// Back
		lut2D[1].x = lut3D.x;
		lut2D[1].y = ceil(lut3D.z)*LUT_BLOCK_SIZE+lut3D.y;
	#else
		// Front
		lut2D[0].x = floor(lut3D.z)*LUT_BLOCK_SIZE+lut3D.x;
		lut2D[0].y = lut3D.y;
		// Back
		lut2D[1].x = ceil(lut3D.z)*LUT_BLOCK_SIZE+lut3D.x;
		lut2D[1].y = lut3D.y;
	#endif

	// Convert from texel to texture coords
	lut2D[0] = (lut2D[0]+0.5)*LUT_PIXEL_SIZE;
	lut2D[1] = (lut2D[1]+0.5)*LUT_PIXEL_SIZE;

	// Bicubic LUT interpolation
	float3 LutImage = lerp(
		tex2D(LUTSampler, lut2D[0]).rgb, // Front Z
		tex2D(LUTSampler, lut2D[1]).rgb, // Back Z
		frac(lut3D.z)
	);

	// Blend LUT image with original
	if ( all(LutChromaLuma==1.0) )
		Image = LutImage;
	else
	{
		Image = lerp(
			normalize(Image),
			normalize(LutImage),
			LutChromaLuma.x
		)*lerp(
			length(Image),
			length(LutImage),
			LutChromaLuma.y
		);
	}
}


  //////////////
 /// OUTPUT ///
//////////////

technique DisplayLUT <
	ui_label = "Display LUT";
	ui_tooltip =
	"Display generated-neutral LUT texture in left to corner of the screen\n\n"
	"How to use:\n"
	"* adjust lut size\n"
	"* (optionally) adjust color effecs to bake shaders into LUT\n"
	"* take a screenshot\n"
	"* adjust and crop screenshot to texture using external image editor\n"
	"* load LUT texture in 'Apply LUT .fx'";
>
{
	pass
	{
		VertexShader = PostProcessVS;
		PixelShader = DisplayLutPS;
	}
}

technique ApplyLUT <
	ui_label = "Apply LUT";
	ui_tooltip =
	"Apply LUT texture color adjustment\n"
	"To change texture name, add following to global preprocessor definitions:\n\n"
	"   LUT_FILE_NAME 'YourLUT.png'\n\n"
	"To change LUT texture resolution, define:\n\n"
	"   LUT_BLOCK_SIZE 17\n\n"
	"To change LUT texture orientation, define:\n\n"
	"   LUT_VERTICAL true";
>
{
	pass
	{
		VertexShader = PostProcessVS;
		PixelShader = ApplyLutPS;
	}
}
