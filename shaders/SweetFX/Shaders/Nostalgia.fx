/*------------------.
| :: Description :: |
'-------------------/

	Nostalgia (version 1.3)

	Author: CeeJay.dk
	License: MIT

	About:
	In this effect I try to recreate the looks of systems from a bygone era.
	I've started with reducing the color to that of systems with 16 color palette.

	Ideas for future improvement:
	* Try HSL / HCY / Lab or other colorspaces. I'm not sure RGB is the best choice for color matching.
	* Pixelation
	* Scanlines
	* CRT patterns
	* Curvature
	* More Dithering (both good and the bad dithering used back then)
	* Levels (might be needed because older system were often displayed on televisions and older monitors - not modern monitors)

	History:
	(*) Feature (+) Improvement (x) Bugfix (-) Information (!) Compatibility
	
	Version 1.0 by CeeJay.dk
	* Color reduction to C64 palette

	Version 1.1 by CeeJay.dk
	* Added ability to set a custom palette
	* Added EGA palette
	+ Improved settings UI
	- Commented much of the code

	Version 1.2 by microdee
	+ Added more color palettes from wikipedia
	+ Palettes can have different color counts

	Version 1.3 by CeeJay.dk
	* Added Aek16 palette
	+ Made Nostalgia do color matching in linear space which improves color matching
	* Added checker board dithering
	* Added scanlines
*/


/*---------------.
| :: Includes :: |
'---------------*/

#include "ReShade.fxh"
#include "ReShadeUI.fxh"

/*--------------.
| :: Defines :: |
'--------------*/

#ifndef Nostalgia_linear
	#define Nostalgia_linear 1
#endif

/*------------------.
| :: UI Settings :: |
'------------------*/

uniform int Nostalgia_scanlines
<
	ui_type = "combo";
	ui_label = "Scanlines";
	ui_items = 
	"None\0"
	"Type 1\0"
	"Type 2\0";
	//ui_category = "";
> = 1;

uniform int Nostalgia_color_reduction
<
	ui_type = "combo";
	ui_label = "Color reduction type";
	//ui_tooltip = "Choose a color reduction type";
	//ui_category = "";
	ui_items = 
	"None\0"
	"Palette\0"
	//"Quantize\0"
	;
> = 1;

uniform bool Nostalgia_dither 
<
	ui_label = "Dither";
> = 0;

uniform int Nostalgia_palette <
	ui_type = "combo";
	ui_label = "Palette";
	ui_tooltip = "Choose a palette";
	//ui_category = "";
	ui_items = 
	"Custom\0"
	"C64 palette\0"
	"EGA palette\0"
	"IBMPC palette\0"
	"ZXSpectrum palette\0"
	"AppleII palette\0"
	"NTSC palette\0"
	"Commodore VIC-20\0"
	"MSX Systems\0"
	"Thomson MO5\0"
	"Amstrad CPC\0"
	"Atari ST\0"
	"Mattel Aquarius\0"
	"Gameboy\0"
	"Aek16 palette";
> = 0;

uniform float3 Nostalgia_color_0 < __UNIFORM_COLOR_FLOAT3
	ui_label = "Color 0";
	ui_category = "Custom palette";
> = float3(  0. ,   0. ,   0. ); //Black;

uniform float3 Nostalgia_color_1 < __UNIFORM_COLOR_FLOAT3
	ui_label = "Color 1";
	ui_category = "Custom palette"; > 
= float3(255. , 255. , 255. ) / 255.; //White

uniform float3 Nostalgia_color_2 < __UNIFORM_COLOR_FLOAT3
	ui_label = "Color 2";
	ui_category = "Custom palette";
> = float3(136. ,   0. ,   0. ) / 255.; //Red;

uniform float3 Nostalgia_color_3 < __UNIFORM_COLOR_FLOAT3
	ui_label = "Color 3";
	ui_category = "Custom palette";
> = float3(170. , 255. , 238. ) / 255.; //Cyan

uniform float3 Nostalgia_color_4 < __UNIFORM_COLOR_FLOAT3
	ui_label = "Color 4";
	ui_category = "Custom palette";
> = float3(204. ,  68. , 204. ) / 255.; //Violet

uniform float3 Nostalgia_color_5 < __UNIFORM_COLOR_FLOAT3
	ui_label = "Color 5";
	ui_category = "Custom palette";
> = float3(  0. , 204. ,  85. ) / 255.; //Green

uniform float3 Nostalgia_color_6 < __UNIFORM_COLOR_FLOAT3
	ui_label = "Color 6";
	ui_category = "Custom palette";
> = float3(  0. ,   0. , 170. ) / 255.; //Blue

uniform float3 Nostalgia_color_7 < __UNIFORM_COLOR_FLOAT3
	ui_label = "Color 7";
	ui_category = "Custom palette";
> = float3(238. , 238. , 119. ) / 255.; //Yellow 1

uniform float3 Nostalgia_color_8 < __UNIFORM_COLOR_FLOAT3
	ui_label = "Color 8";
	ui_category = "Custom palette";
> = float3(221. , 136. ,  85. ) / 255.; //Orange

uniform float3 Nostalgia_color_9 < __UNIFORM_COLOR_FLOAT3
	ui_label = "Color 9";
	ui_category = "Custom palette";
> = float3(102. ,  68. ,   0. ) / 255.; //Brown

uniform float3 Nostalgia_color_10 < __UNIFORM_COLOR_FLOAT3
	ui_label = "Color 10";
	ui_category = "Custom palette";
> = float3(255. , 119. , 119. ) / 255.; //Yellow 2

uniform float3 Nostalgia_color_11 < __UNIFORM_COLOR_FLOAT3
	ui_label = "Color 11";
	ui_category = "Custom palette";
> = float3( 51. ,  51. ,  51. ) / 255.; //Grey 1

uniform float3 Nostalgia_color_12 < __UNIFORM_COLOR_FLOAT3
	ui_label = "Color 12";
	ui_category = "Custom palette";
> = float3(119. , 119. , 119. ) / 255.; //Grey 2

uniform float3 Nostalgia_color_13 < __UNIFORM_COLOR_FLOAT3
	ui_label = "Color 13";
	ui_category = "Custom palette";
> = float3(170. , 255. , 102. ) / 255.; //Lightgreen

uniform float3 Nostalgia_color_14 < __UNIFORM_COLOR_FLOAT3
	ui_label = "Color 14";
	ui_category = "Custom palette";
> = float3(  0. , 136. , 255. ) / 255.; //Lightblue

uniform float3 Nostalgia_color_15 < __UNIFORM_COLOR_FLOAT3
	ui_label = "Color 15";
	ui_category = "Custom palette";
> = float3(187. , 187. , 187. ) / 255.;  //Grey 3

/*
uniform bool Nostalgia_linear //Can't currently make a UI setting for this since I need the preprocessor for that and it does not accept uniforms from the UI
<
	ui_label = "Linear";
	//ui_category = "Color options";
> = 0;
*/

/*--------------.
| :: Sampler :: |
'--------------*/

sampler Linear
{
	Texture = ReShade::BackBufferTex;
	SRGBTexture = true;
};


/*-------------.
| :: Effect :: |
'-------------*/

float3 PS_Nostalgia(float4 vpos : SV_Position, float2 texcoord : TEXCOORD) : SV_Target
{
	float3 color;
	int colorCount = 16;

	#if Nostalgia_linear == 1
		color = tex2D(Linear, texcoord.xy).rgb;
	#else
		color = tex2D(ReShade::BackBuffer, texcoord.xy).rgb;
	#endif
	

	if (Nostalgia_color_reduction)
	{
		float3 palette[16] = //Custom palette
		{
			Nostalgia_color_0,
			Nostalgia_color_1,
			Nostalgia_color_2,
			Nostalgia_color_3,
			Nostalgia_color_4,
			Nostalgia_color_5,
			Nostalgia_color_6,
			Nostalgia_color_7,
			Nostalgia_color_8,
			Nostalgia_color_9,
			Nostalgia_color_10,
			Nostalgia_color_11,
			Nostalgia_color_12,
			Nostalgia_color_13,
			Nostalgia_color_14,
			Nostalgia_color_15
		};

		if (Nostalgia_palette == 1) //C64 palette from http://www.c64-wiki.com/index.php/Color
		{
			palette[0]  = float3(  0. ,   0. ,   0. ) / 255.; //Black
			palette[1]  = float3(255. , 255. , 255. ) / 255.; //White
			palette[2]  = float3(136. ,   0. ,   0. ) / 255.; //Red
			palette[3]  = float3(170. , 255. , 238. ) / 255.; //Cyan
			palette[4]  = float3(204. ,  68. , 204. ) / 255.; //Violet
			palette[5]  = float3(  0. , 204. ,  85. ) / 255.; //Green
			palette[6]  = float3(  0. ,   0. , 170. ) / 255.; //Blue
			palette[7]  = float3(238. , 238. , 119. ) / 255.; //Yellow 1
			palette[8]  = float3(221. , 136. ,  85. ) / 255.; //Orange
			palette[9]  = float3(102. ,  68. ,   0. ) / 255.; //Brown
			palette[10] = float3(255. , 119. , 119. ) / 255.; //Yellow 2
			palette[11] = float3( 51. ,  51. ,  51. ) / 255.; //Grey 1
			palette[12] = float3(119. , 119. , 119. ) / 255.; //Grey 2
			palette[13] = float3(170. , 255. , 102. ) / 255.; //Lightgreen
			palette[14] = float3(  0. , 136. , 255. ) / 255.; //Lightblue
			palette[15] = float3(187. , 187. , 187. ) / 255.; //Grey 3
		}

		if (Nostalgia_palette == 2) //EGA palette
		{
			palette[0] 	= float3(0.0,		0.0,		0.0		); //Black
			palette[1] 	= float3(0.0,		0.0,		0.666667); //Blue
			palette[2] 	= float3(0.0,		0.666667,	0.0		); //Green
			palette[3] 	= float3(0.0,		0.666667,	0.666667); //Cyan
			palette[4] 	= float3(0.666667,	0.0,		0.0		); //Red
			palette[5] 	= float3(0.666667,	0.0,		0.666667); //Magenta
			palette[6] 	= float3(0.666667,	0.333333,	0.0		); //Brown 
			palette[7] 	= float3(0.666667,	0.666667,	0.666667); //Light gray
			palette[8] 	= float3(0.333333,	0.333333,	0.333333); //Dark gray
			palette[9] 	= float3(0.333333,	0.333333,	1.0		); //Bright blue
			palette[10]	= float3(0.333333,	1.0,		0.333333); //Bright green
			palette[11]	= float3(0.333333,	1.0,		1.0		); //Bright cyan
			palette[12]	= float3(1.0,		0.333333,	0.333333); //Bright red
			palette[13]	= float3(1.0,		0.333333,	1.0		); //Bright magenta
			palette[14]	= float3(1.0,		1.0,		0.333333); //Bright yellow
			palette[15]	= float3(1.0,		1.0,		1.0		); //White
		}
	
		if (Nostalgia_palette == 3) //IBMPC palette
		{
			palette[0] = float3(0,0,0);
			palette[1] = float3(0,0,0.8);
			palette[2] = float3(0,0.6,0);
			palette[3] = float3(0,0.6,0.8);
			palette[4] = float3(0.8,0,0);
			palette[5] = float3(0.8,0,0.8);
			palette[6] = float3(0.8,0.6,0);
			palette[7] = float3(0.8,0.8,0.8);
			palette[8] = float3(0.4,0.4,0.4);
			palette[9] = float3(0.4,0.4,1);
			palette[10] = float3(0.4,1,0.4);
			palette[11] = float3(0.4,1,1);
			palette[12] = float3(0.99,0.4,0.4);
			palette[13] = float3(1,0.4,1);
			palette[14] = float3(1,1,0.4);
			palette[15] = float3(1,1,1);
		}

		if (Nostalgia_palette == 4) //ZX Spectrum palette
		{
			palette[0] = float3(0,0,0);
			palette[1] = float3(0,0,0.811764705882353);
			palette[2] = float3(0,0.811764705882353,0);
			palette[3] = float3(0,0.811764705882353,0.811764705882353);
			palette[4] = float3(0.811764705882353,0,0);
			palette[5] = float3(0.811764705882353,0,0.752941176470588);
			palette[6] = float3(0.811764705882353,0.811764705882353,0);
			palette[7] = float3(0.811764705882353,0.811764705882353,0.811764705882353);
			palette[8] = float3(0,0,0);
			palette[9] = float3(0,0,1);
			palette[10] = float3(0,1,0);
			palette[11] = float3(0,1,1);
			palette[12] = float3(1,0,0);
			palette[13] = float3(1,0,1);
			palette[14] = float3(1,1,0);
			palette[15] = float3(1,1,1);
		}

		if (Nostalgia_palette == 5) //AppleII palette
		{
			palette[0] = float3(0,0,0);
			palette[1] = float3(0.890196078431373,0.117647058823529,0.376470588235294);
			palette[2] = float3(0.376470588235294,0.305882352941176,0.741176470588235);
			palette[3] = float3(1,0.266666666666667,0.992156862745098);
			palette[4] = float3(0,0.63921568627451,0.376470588235294);
			palette[5] = float3(0.611764705882353,0.611764705882353,0.611764705882353);
			palette[6] = float3(0.0784313725490196,0.811764705882353,0.992156862745098);
			palette[7] = float3(0.815686274509804,0.764705882352941,1);
			palette[8] = float3(0.376470588235294,0.447058823529412,0.0117647058823529);
			palette[9] = float3(1,0.415686274509804,0.235294117647059);
			palette[10] = float3(0.611764705882353,0.611764705882353,0.611764705882353);
			palette[11] = float3(1,0.627450980392157,0.815686274509804);
			palette[12] = float3(0.0784313725490196,0.96078431372549,0.235294117647059);
			palette[13] = float3(0.815686274509804,0.866666666666667,0.552941176470588);
			palette[14] = float3(0.447058823529412,1,0.815686274509804);
			palette[15] = float3(1,1,1);
		}
		
		if (Nostalgia_palette == 6) //NTSC palette
		{
			palette[0] = float3(0.831372549019608,0.831372549019608,0.831372549019608);
			palette[1] = float3(0.866666666666667,0.776470588235294,0.474509803921569);
			palette[2] = float3(0.0392156862745098,0.96078431372549,0.776470588235294);
			palette[3] = float3(0.0470588235294118,0.917647058823529,0.380392156862745);
			palette[4] = float3(1,0.156862745098039,0.709803921568627);
			palette[5] = float3(1,0.109803921568627,0.298039215686275);
			palette[6] = float3(0.149019607843137,0.254901960784314,0.607843137254902);
			palette[7] = float3(0,0.87843137254902,0.905882352941176);
			palette[8] = float3(1,1,1);
			palette[9] = float3(1,0.317647058823529,1);
			palette[10] = float3(0.16078431372549,0.16078431372549,0.16078431372549);
			palette[11] = float3(0.16078431372549,0.16078431372549,0.16078431372549);
			palette[12] = float3(0.16078431372549,0.16078431372549,0.16078431372549);
			palette[13] = float3(0.831372549019608,0.831372549019608,0.831372549019608);
			palette[14] = float3(0.866666666666667,0.776470588235294,0.474509803921569);
			palette[15] = float3(0.0392156862745098,0.96078431372549,0.776470588235294);
		}
		
		if (Nostalgia_palette == 7) // Commodore VIC-20
		{
			palette[0] = float3(0,0,0);
			palette[1] = float3(1,1,1);
			palette[2] = float3(0.470588235294118,0.16078431372549,0.133333333333333);
			palette[3] = float3(0.529411764705882,0.83921568627451,0.866666666666667);
			palette[4] = float3(0.666666666666667,0.372549019607843,0.713725490196078);
			palette[5] = float3(0.101960784313725,0.509803921568627,0.149019607843137);
			palette[6] = float3(0.250980392156863,0.192156862745098,0.552941176470588);
			palette[7] = float3(0.749019607843137,0.807843137254902,0.447058823529412);
			palette[8] = float3(0.666666666666667,0.454901960784314,0.286274509803922);
			palette[9] = float3(0.917647058823529,0.705882352941177,0.537254901960784);
			palette[10] = float3(0.72156862745098,0.411764705882353,0.384313725490196);
			palette[11] = float3(0.780392156862745,1,1);
			palette[12] = float3(0.917647058823529,0.623529411764706,0.964705882352941);
			palette[13] = float3(0.580392156862745,0.87843137254902,0.537254901960784);
			palette[14] = float3(0.501960784313725,0.443137254901961,0.8);
			palette[15] = float3(1,1,0.698039215686274);
		}
		
		if (Nostalgia_palette == 8) // MSX Systems
		{
			palette[0] = float3(0,0,0);
			palette[1] = float3(1,1,1);
			palette[2] = float3(0.243137254901961,0.72156862745098,0.286274509803922);
			palette[3] = float3(0.454901960784314,0.815686274509804,0.490196078431373);
			palette[4] = float3(0.349019607843137,0.333333333333333,0.87843137254902);
			palette[5] = float3(0.501960784313725,0.462745098039216,0.945098039215686);
			palette[6] = float3(0.725490196078431,0.368627450980392,0.317647058823529);
			palette[7] = float3(0.396078431372549,0.858823529411765,0.937254901960784);
			palette[8] = float3(0.858823529411765,0.396078431372549,0.349019607843137);
			palette[9] = float3(1,0.537254901960784,0.490196078431373);
			palette[10] = float3(0.8,0.764705882352941,0.368627450980392);
			palette[11] = float3(0.870588235294118,0.815686274509804,0.529411764705882);
			palette[12] = float3(0.227450980392157,0.635294117647059,0.254901960784314);
			palette[13] = float3(0.717647058823529,0.4,0.709803921568627);
			palette[14] = float3(0.8,0.8,0.8);
			palette[15] = float3(1,1,0.698039215686274);
		}
		
		if (Nostalgia_palette == 9) // Thomson MO5
		{
			palette[0] = float3(0,0,0);
			palette[1] = float3(1,1,1);
			palette[2] = float3(1,0,0);
			palette[3] = float3(0,1,0);
			palette[4] = float3(1,1,0);
			palette[5] = float3(0,0,1);
			palette[6] = float3(1,0,1);
			palette[7] = float3(0,1,1);
			palette[8] = float3(0,0,0);
			palette[9] = float3(0.733333333333333,0.733333333333333,0.733333333333333);
			palette[10] = float3(0.866666666666667,0.466666666666667,0.466666666666667);
			palette[11] = float3(0.466666666666667,0.866666666666667,0.466666666666667);
			palette[12] = float3(0.866666666666667,0.866666666666667,0.466666666666667);
			palette[13] = float3(0.466666666666667,0.466666666666667,0.866666666666667);
			palette[14] = float3(0.866666666666667,0.466666666666667,0.933333333333333);
			palette[15] = float3(0.733333333333333,1,1);
		}
		
		if (Nostalgia_palette == 10) // Amstrad CPC
		{
			palette[0] = float3(0,0,0);
			palette[1] = float3(1,1,1);
			palette[2] = float3(0,0,0.498039215686275);
			palette[3] = float3(0.498039215686275,0,0);
			palette[4] = float3(0.498039215686275,0,0.498039215686275);
			palette[5] = float3(0,0.498039215686275,0);
			palette[6] = float3(1,0,0);
			palette[7] = float3(0,0.498039215686275,0.498039215686275);
			palette[8] = float3(0.498039215686275,0.498039215686275,0);
			palette[9] = float3(0.498039215686275,0.498039215686275,0.498039215686275);
			palette[10] = float3(0.498039215686275,0.498039215686275,1);
			palette[11] = float3(1,0.498039215686275,0);
			palette[12] = float3(1,0.498039215686275,0.498039215686275);
			palette[13] = float3(0.498039215686275,1,0.498039215686275);
			palette[14] = float3(0.498039215686275,1,1);
			palette[15] = float3(1,1,0.498039215686275);
		}
		
		if (Nostalgia_palette == 11) // Atari ST
		{
			palette[0] = float3(0,0,0);
			palette[1] = float3(1,0.886274509803922,0.882352941176471);
			palette[2] = float3(0.376470588235294,0.0392156862745098,0.0117647058823529);
			palette[3] = float3(0.811764705882353,0.133333333333333,0.0549019607843137);
			palette[4] = float3(0.16078431372549,0.345098039215686,0.0352941176470588);
			palette[5] = float3(0.937254901960784,0.16078431372549,0.0705882352941176);
			palette[6] = float3(0.356862745098039,0.349019607843137,0.0431372549019608);
			palette[7] = float3(0.352941176470588,0.352941176470588,0.352941176470588);
			palette[8] = float3(0.803921568627451,0.372549019607843,0.207843137254902);
			palette[9] = float3(0.494117647058824,0.509803921568627,0.756862745098039);
			palette[10] = float3(0.305882352941176,0.623529411764706,0.0980392156862745);
			palette[11] = float3(0.792156862745098,0.509803921568627,0.364705882352941);
			palette[12] = float3(1,0.392156862745098,0.215686274509804);
			palette[13] = float3(1,0.525490196078431,0.368627450980392);
			palette[14] = float3(0.631372549019608,0.63921568627451,0.76078431372549);
			palette[15] = float3(1,0.768627450980392,0.517647058823529);
		}
		
		if (Nostalgia_palette == 12) // Mattel Aquarius
		{
			palette[0] = float3(0,0,0);
			palette[1] = float3(1,1,1);
			palette[2] = float3(0.494117647058824,0.0980392156862745,0.164705882352941);
			palette[3] = float3(0.764705882352941,0,0.105882352941176);
			palette[4] = float3(0.725490196078431,0.694117647058824,0.337254901960784);
			palette[5] = float3(0.784313725490196,0.725490196078431,0.0274509803921569);
			palette[6] = float3(0.231372549019608,0.592156862745098,0.180392156862745);
			palette[7] = float3(0.0274509803921569,0.749019607843137,0);
			palette[8] = float3(0.250980392156863,0.650980392156863,0.584313725490196);
			palette[9] = float3(0,0.776470588235294,0.643137254901961);
			palette[10] = float3(0.749019607843137,0.749019607843137,0.749019607843137);
			palette[11] = float3(0.513725490196078,0.152941176470588,0.564705882352941);
			palette[12] = float3(0.717647058823529,0,0.819607843137255);
			palette[13] = float3(0.0196078431372549,0.0509803921568627,0.407843137254902);
			colorCount = 14;
		}
		
		if (Nostalgia_palette == 13) // Gameboy
		{
			palette[0] = float3(0.0588235294117647,0.219607843137255,0.0588235294117647);
			palette[1] = float3(0.607843137254902,0.737254901960784,0.0588235294117647);
			palette[2] = float3(0.188235294117647,0.384313725490196,0.188235294117647);
			palette[3] = float3(0.545098039215686,0.674509803921569,0.0588235294117647);
			colorCount = 4;
		}


		if (Nostalgia_palette == 14) //aek16 ( http://eastfarthing.com/blog/2016-05-06-palette/ )
		{
			palette[0] 	= float3(0.247059,	0.196078,	0.682353); //
			palette[1] 	= float3(0.890196,	0.054902,	0.760784); //
			palette[2] 	= float3(0.729412,	0.666667,	1.000000); //
			palette[3] 	= float3(1.,		  1.000000,	1.      ); //White
			palette[4] 	= float3(1.000000,	0.580392,	0.615686); //
			palette[5] 	= float3(0.909804,	0.007843,	0.000000); //
			palette[6] 	= float3(0.478431,	0.141176,	0.239216); //
			palette[7] 	= float3(0.,		  0.	  ,	0.		); //Black
			palette[8] 	= float3(0.098039,	0.337255,	0.282353); //
			palette[9] 	= float3(0.415686,	0.537255,	0.152941); //
			palette[10] 	= float3(0.086275,	0.929412,	0.458824); //
			palette[11] 	= float3(0.196078,	0.756863,	0.764706); //
			palette[12] 	= float3(0.019608,	0.498039,	0.756863); //
			palette[13] 	= float3(0.431373,	0.305882,	0.137255); //
			palette[14] 	= float3(0.937255,	0.890196,	0.019608); //
			palette[15] 	= float3(0.788235,	0.560784,	0.298039); //
		}

		// :: Dither :: //

		if (Nostalgia_dither == 1) //aek16 ( http://eastfarthing.com/blog/2016-05-06-palette/ )
		{
		
			//Calculate grid position
			float grid_position = frac(dot(texcoord, BUFFER_SCREEN_SIZE * 0.5) + 0.25); //returns 0.25 and 0.75
	
			//Calculate how big the shift should be
			float dither_shift = (0.25) * (1.0 / (pow(2,2.0) - 1.0)); // 0.25 seems good both when using math and when eyeballing it. So does 0.75 btw.
	
			//Shift the individual colors differently, thus making it even harder to see the dithering pattern
			float3 dither_shift_RGB = float3(dither_shift, dither_shift, dither_shift); //dithering
	
			//modify shift acording to grid position.
			dither_shift_RGB = lerp(2.0 * dither_shift_RGB, -2.0 * dither_shift_RGB, grid_position); //shift acording to grid position.
	
			//shift the color by dither_shift
			//color.rgb += lerp(2.0 * dither_shift_RGB, -2.0 * dither_shift_RGB, grid_position); //shift acording to grid position.
			color.rgb += dither_shift_RGB;
		}
		
		// :: Color matching :: //
		
		float3 diff = color - palette[0]; //find the difference in color compared to color 0
		
		float dist = dot(diff,diff); //squared distance of difference - we don't need to calculate the square root of this

		float closest_dist = dist; //this has to be the closest distance so far as it's the first we have checked
		float3 closest_color = palette[0]; //and closest color so far is this one

		for (int i = 1 ; i < colorCount ; i++) //for colors 1 to colorCount
		{
			diff = color - palette[i]; //find the difference in color
		
			dist = dot(diff,diff); //squared distance of difference - we don't need to calculate the square root of this
		
			if (dist < closest_dist) //is the distance closer than the previously closest distance?
			{ 
				closest_dist = dist; //closest distance is now this distance
				closest_color = palette[i]; //closest color is now this color
			}
		}
		
		color = closest_color; //return the pixel
	}

	if (Nostalgia_scanlines == 1)
	{
		color *= frac(texcoord.y * (BUFFER_HEIGHT * 0.5)) + 0.5; //Scanlines
	}
	if (Nostalgia_scanlines == 2)
	{
		float grey  = dot(color,float(1.0/3.0));
		color = (frac(texcoord.y * (BUFFER_HEIGHT * 0.5)) < 0.25) ? color : color * ((-grey*grey+grey+grey) * 0.5 + 0.5);
	}

	return color; //return the pixel
}


/*----------------.
| :: Technique :: |
'----------------*/

technique Nostalgia
{
	pass NostalgiaPass
	{
		VertexShader = PostProcessVS;
		PixelShader = PS_Nostalgia;
		
		#if Nostalgia_linear == 1
			SRGBWriteEnable = true;
		#endif	
		
		ClearRenderTargets = false;
	}
}
