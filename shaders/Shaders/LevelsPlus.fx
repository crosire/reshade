/**
 * Levels version 1.8.2
 * by Christian Cann Schuldt Jensen ~ CeeJay.dk
 * updated to 1.3+ by Kirill Yarovoy ~ v00d00m4n
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
 *
 * -- Version 1.3 --
 * Added independant RGB channel levels that allow to fix impropely balanced console specific color space.
 *
 * Most of modern Xbox One \ PS4 ports has white point around 233 222 211 instead of TV 235 235 235
 * which can be seen and aproximated by analyzing histograms of hundreds of hudless screenshots of modern games
 * including big titles such as GTAV, Witcher 3, Watch_Dogs, most of UE4 based titles and so on.
 *
 * Most of these games lacking true balanced white and black colors and looks like if you play on very old and dusty display.
 * This problem could be related to improper usage and settings of popular FILMIC shader, introduced in Uncharted 2.
 *
 * I used to prebake static luts to restore color balance, but doing so in image editors was slow, so once i discovered
 * that Reshade 3 has RGB UI settings i decided that dynamic in-game correction would be more productive, so i updated this
 * old shader to correct color mess in game. I can spot white oddities wiht my naked eyes, but i suggest to combine this shader
 * with Ganossa Histogram shader, loaded after levels for this, but you need to update it for Rehade 3 and get it here:
 * https://github.com/crosire/reshade-shaders/blob/382b28f33034809e52513332ca36398e72563e10/ReShade/Shaders/Ganossa/Histogram.fx
 *
 * -- Version 1.4 --
 * Added ability to upshift color range before expanding it. Needed to fix stupid Ubisoft mistake in Watch Dogs 2 where they
 * somehow downshifted color range.
 *
 * -- Version 1.5 --
 * Changed formulas to allow gamma and output range controls.
 *
 * -- Version 1.6 --
 * Added ACES curve, to avoid clipping.
 *
 * -- Version 1.7 --
 * Removed ACES and added linear Z-curve to avoid clipping. Optional Alt calculation added.
 *
 * -- Version 1.8
 * Previous version features was broken when i was sleepy, than i did not touch this shader for months and forgot what i did there.
 * So, i commented messed up code in hope to fix it later, and reintroduced ACES in useful way.
 *
 * -- Version 1.8.1
 * Added 2 new ACES modes.
 *
 * -- Version 1.8.2
 * Fixed some things, broke others. Restored 1.8 version ACES as ACES OLD.
 *
 * -- Version 1.8.3
 * Changed shader name to LevelsPlus to avoid conflicts with old shader settings.
 * Added Reshade 3 and 4 compatibility fix.
 *
 */


#include "ReShade.fxh"
static const float PI = 3.141592653589793238462643383279f;


// Settings

#include "ReShadeUI.fxh"

uniform bool EnableLevels <
	ui_tooltip = "Enable or Disable Levels for TV <> PC or custome color range";
> = true;

uniform float3 InputBlackPoint < __UNIFORM_COLOR_FLOAT3
	ui_tooltip = "The black point is the new black - literally.\n0 Everything darker than this will become completely black.";
> = float3(16/255.0f, 18/255.0f, 20/255.0f);

uniform float3 InputWhitePoint < __UNIFORM_COLOR_FLOAT3
	ui_tooltip = "The new white point.\n0 Everything brighter than this becomes completely white";
> = float3(233/255.0f, 222/255.0f, 211/255.0f);

uniform float3 InputGamma < __UNIFORM_SLIDER_FLOAT3
	ui_min = 0.01f; ui_max = 10.00f; step = 0.01f;
	ui_label = "RGB Gamma";
	ui_tooltip = "Adjust midtones for Red, Green and Blue.";
> = float3(1.00f,1.00f,1.00f);

uniform float3 OutputBlackPoint < __UNIFORM_COLOR_FLOAT3
	ui_tooltip = "The black point is the new black - literally.\n0 Everything darker than this will become completely black.";
> = float3(0/255.0f, 0/255.0f, 0/255.0f);

uniform float3 OutputWhitePoint < __UNIFORM_COLOR_FLOAT3
	ui_tooltip = "The new white point.\n0 Everything brighter than this becomes completely white";
> = float3(255/255.0f, 255/255.0f, 255/255.0f);

// Anti clipping measures

/*
uniform float3 MinBlackPoint <
	ui_type = "color";
	ui_min = 0.0f; ui_max = 0.5f;
	ui_tooltip = "If avoid clipping enabled this is the percentage break point relative to Output black.\n0 Anything lower than this will be compressed to fit into output range.";
> = float3(16/255.0f, 18/255.0f, 20/255.0f);

uniform float3 MinWhitePoint <
	ui_type = "color";
	ui_min = 0.5f; ui_max = 1.0f;
	ui_tooltip = "If avoid clipping enabled this is the percentage white point relative to Output white.\n0 Anything higher than this will be compressed to fit into output range.";
> = float3(233/255.0f/1.1f, 222/255.0f/1.1f, 211/255.0f/1.1f);
*/

uniform float3 ColorRangeShift < __UNIFORM_COLOR_FLOAT3
	ui_tooltip = "Some games like Watch Dogs 2 has color range 16-235 downshifted to 0-219,\n0 so this option was added to upshift color range before expanding it.\n0 RGB value entered here will be just added to default color value.\n0 Negative values impossible at the moment in game, but can be added,\n0 in shader if downshifting needed.\n0 0 disables shifting.";
> = float3(0/255.0f, 0/255.0f, 0/255.0f);

uniform int ColorRangeShiftSwitch < __UNIFORM_SLIDER_INT1
	ui_min = -1; ui_max = 1;
	ui_tooltip = "Workaround for lack of negative color values in Reshade UI:\n0 -1 to downshift,\n0 1 to upshift,\n0 0 to disable";
> = 0;

/*
uniform bool AvoidClipping <
	ui_tooltip = "Avoid pixels clip.";
> = false;

uniform bool AvoidClippingWhite <
	ui_tooltip = "Avoid white pixels clip.";
> = false;

uniform bool AvoidClippingBlack <
	ui_tooltip = "Avoid black pixels clip.";
> = false;

uniform bool SmoothCurve <
	ui_tooltip = "Improves contrast";
> = true;
*/

uniform bool HighlightClipping <
	ui_tooltip = "Colors between the two points will stretched, which increases contrast, but details above and below the points are lost (this is called clipping).\n0 Highlight the pixels that clip.\n0 Red = Some details are lost in the highlights,\n0 Yellow = All details are lost in the highlights,\n0 Blue = Some details are lost in the shadows,\n0 Cyan = All details are lost in the shadows.";
> = false;


//------ ACES -------

uniform bool enableACESFilmRec2020old <
	ui_tooltip = "Enable or Disable OLD ACES for improved contrast and luminance";
> = false;

uniform bool enableACESFilmRec2020 <
	ui_tooltip = "Enable or Disable ACES for improved contrast and luminance";
> = false;


uniform bool enableACESFitted <
	ui_tooltip = "Enable or Disable ALT ACES for improved contrast and luminance";
> = false;

uniform int3 ACESLuminancePercentage < __UNIFORM_SLIDER_INT3
	ui_min = 0; ui_max = 200; step = 1;
	ui_tooltip = "Percentage of ACES Luminance.\n0 Can be used to avoid some color clipping.";
> = int3(100,100,100);

//--------------------

float3 ACESFilmRec2020old( float3 color )
{
    float Slope = 15.8f;
    float Toe = 2.12f;
    float Shoulder = 1.2f;
    float BlackClip = 5.92f;
    float WhiteClip = 1.9f;
    color = color * ACESLuminancePercentage * 0.005f; // Restores luminance
    return ( color * ( Slope * color + Toe ) ) / ( color * ( Shoulder * color + BlackClip ) + WhiteClip );
}

float3 ACESFilmRec2020( float3 color )
{
	float Slope = 0.98;
	float Toe = 0.3;
	float Shoulder = 0.22;
	float BlackClip = 0;
	float WhiteClip = 0.025;
    color = color * ACESLuminancePercentage * 0.005f; // Restores luminance
    return ( color * ( Slope * color + Toe ) ) / ( color * ( Shoulder * color + BlackClip ) + WhiteClip );
}

//=================================================================================================
//
//  Baking Lab
//  by MJP and David Neubelt
//  http://mynameismjp.wordpress.com/
//
//  All code licensed under the MIT license
//
//=================================================================================================

// The code in this file was originally written by Stephen Hill (@self_shadow), who deserves all
// credit for coming up with this fit and implementing it. :)

// sRGB => XYZ => D65_2_D60 => AP1 => RRT_SAT
static const float3x3 ACESInputMat = float3x3
(
    0.59719, 0.35458, 0.04823,
    0.07600, 0.90834, 0.01566,
    0.02840, 0.13383, 0.83777
);

// ODT_SAT => XYZ => D60_2_D65 => sRGB
static const float3x3 ACESOutputMat = float3x3
(
     1.60475, -0.53108, -0.07367,
    -0.10208,  1.10813, -0.00605,
    -0.00327, -0.07276,  1.07602
);

float3 RRTAndODTFit(float3 v)
{
    float3 a = v * (v + 0.0245786f) - 0.000090537f;
    float3 b = v * (0.983729f * v + 0.4329510f) + 0.238081f;
    return a / b;
}

float3 ACESFitted(float3 color)
{
    color = mul(ACESInputMat, color);

    // Apply RRT and ODT
    color = RRTAndODTFit(color);

    color = mul(ACESOutputMat, color);

    // Clamp to [0, 1]
    color = saturate(color);

    return color;
}


//--------------------




// Helper functions

/*
float3 Smooth(float3 color, float3 inputwhitepoint, float3 inputblackpoint)
{
    //color =
    return clamp((color - inputblackpoint)/(inputwhitepoint - inputblackpoint), 0.0, 1.0);
    //return pow(sin(PI * 0.5 * color),2);
}
*/

/*
float Curve(float x, float centerX, float centerY)
{
    if (centerX > 0  && centerX < 1 && centerY > 0  && centerY < 1)
    {
      if (x < 0.5)
      {
        return 0-pow(sin(PI * ((0-x)/4*(0-centerX))),2)*2*(0-centerY);
      } else if (x > 0.5)
      {
        return 1-pow(sin(PI * ((1-x)/4*(1-centerX))),2)*2*(1-centerY);
      } else
      {
        return x;
      }
    } else
    {
      return x;
    }
}
*/

//RGB input levels
float3 InputLevels(float3 color, float3 inputwhitepoint, float3 inputblackpoint)
{
  return color = (color - inputblackpoint)/(inputwhitepoint - inputblackpoint);
  //return pow(sin(PI * 0.5 * color),2);
}

//RGB output levels
float3  Outputlevels(float3 color, float3 outputwhitepoint, float3 outputblackpoint)
{
  return color * (outputwhitepoint - outputblackpoint) + outputblackpoint;
}

//1 channel input level
float  InputLevel(float color, float inputwhitepoint, float inputblackpoint)
{
  return (color - inputblackpoint)/(inputwhitepoint - inputblackpoint);
}

//1 channel output level
float  Outputlevel(float color, float outputwhitepoint, float outputblackpoint)
{
  return color * (outputwhitepoint - outputblackpoint) + outputblackpoint;
}


// Main function

float3 LevelsPlusPass(float4 vpos : SV_Position, float2 texcoord : TexCoord) : SV_Target
{
  float3 InputColor = tex2D(ReShade::BackBuffer, texcoord).rgb;
  float3 OutputColor = InputColor;

  // outPixel = (pow(((inPixel * 255.0) - inBlack) / (inWhite - inBlack), inGamma) * (outWhite - outBlack) + outBlack) / 255.0; // Nvidia reference formula


	/*
	if (EnableLevels == true)
	{
		OutputColor = Outputlevels(pow(InputLevels(OutputColor + (ColorRangeShift * ColorRangeShiftSwitch), InputWhitePoint, InputBlackPoint), InputGamma), OutputWhitePoint, OutputBlackPoint);

		/*
		if (AvoidClipping == true)
		{

			//float3 OutputMaxBlackPoint = pow(((0 + (ColorRangeShift * ColorRangeShiftSwitch)) - InputBlackPoint)/(InputWhitePoint - InputBlackPoint) , InputGamma) * (OutputWhitePoint - OutputBlackPoint) + OutputBlackPoint;
			//float3 OutputMaxWhitePoint = pow(((1 + (ColorRangeShift * ColorRangeShiftSwitch)) - InputBlackPoint)/(InputWhitePoint - InputBlackPoint) , InputGamma) * (OutputWhitePoint - OutputBlackPoint) + OutputBlackPoint;

			if (AvoidClippingWhite == true)
			{
				//White
				float3 OutputMaxWhitePoint;
				float3 OutputMinWhitePoint;

				// doest not give smooth gradient :-(
				OutputMaxWhitePoint = Outputlevels(pow(InputLevels(OutputWhitePoint + (ColorRangeShift * ColorRangeShiftSwitch), InputWhitePoint, InputBlackPoint), InputGamma), OutputWhitePoint, OutputBlackPoint);
				OutputMinWhitePoint = Outputlevels(pow(InputLevels(InputWhitePoint + (ColorRangeShift * ColorRangeShiftSwitch), InputWhitePoint, InputBlackPoint), InputGamma), OutputWhitePoint, OutputBlackPoint);

				OutputColor.r = (OutputColor.r >= OutputMinWhitePoint.r)
				? Curve( InputColor.r, MinWhitePoint.r, OutputMinWhitePoint.r)
				//? Outputlevel( InputLevel( OutputColor.r, OutputMaxWhitePoint.r, OutputMinWhitePoint.r ), OutputWhitePoint.r, OutputMinWhitePoint.r)
				: OutputColor.r;

				OutputColor.g = (OutputColor.g >= OutputMinWhitePoint.g)
				? Curve( InputColor.g, MinWhitePoint.g, OutputMinWhitePoint.g)
				//? Outputlevel( InputLevel( OutputColor.g, OutputMaxWhitePoint.g, OutputMinWhitePoint.g ), OutputWhitePoint.g, OutputMinWhitePoint.g)
				: OutputColor.g;

				OutputColor.b = (OutputColor.b >= OutputMinWhitePoint.b)
				? Curve( InputColor.b, MinWhitePoint.b, OutputMinWhitePoint.b)
				//? Outputlevel( InputLevel( OutputColor.b, OutputMaxWhitePoint.b, OutputMinWhitePoint.b ), OutputWhitePoint.b, OutputMinWhitePoint.b)
				: OutputColor.b;
			}

			if (AvoidClippingBlack == true)
			{
				//Black

				float3 OutputMaxBlackPoint;
				float3 OutputMinBlackPoint;
				float3 OutputMinBlackPointY;

				OutputMaxBlackPoint = pow(((0 + (ColorRangeShift * ColorRangeShiftSwitch)) - InputBlackPoint)/(InputWhitePoint - InputBlackPoint) , InputGamma) * (OutputWhitePoint - OutputBlackPoint) + OutputBlackPoint;
				OutputMinBlackPoint = MinBlackPoint;
				OutputMinBlackPointY = pow(((OutputMinBlackPoint + (ColorRangeShift * ColorRangeShiftSwitch)) - InputBlackPoint)/(InputWhitePoint - InputBlackPoint) , InputGamma) * (OutputWhitePoint - OutputBlackPoint) + OutputBlackPoint;

				OutputColor.r = (OutputColor.r <= OutputMinBlackPoint.r)
				? Curve(OutputMinBlackPoint.r,OutputMinBlackPointY.r,((OutputColor.r - OutputMaxBlackPoint.r)/(OutputMinBlackPoint.r - OutputMaxBlackPoint.r)) * (OutputMinBlackPoint.r - OutputBlackPoint.r) + OutputBlackPoint.r)
				: OutputColor.r;

				OutputColor.g = (OutputColor.g <= OutputMinBlackPoint.g)
				? Curve(OutputMinBlackPoint.g,OutputMinBlackPointY.g,((OutputColor.g - OutputMaxBlackPoint.g)/(OutputMinBlackPoint.g - OutputMaxBlackPoint.g)) * (OutputMinBlackPoint.g - OutputBlackPoint.g) + OutputBlackPoint.g)
				: OutputColor.g;

				OutputColor.b = (OutputColor.b <= OutputMinBlackPoint.b)
				? Curve(OutputMinBlackPoint.b,OutputMinBlackPointY.b,((OutputColor.b - OutputMaxBlackPoint.b)/(OutputMinBlackPoint.b - OutputMaxBlackPoint.b)) * (OutputMinBlackPoint.b - OutputBlackPoint.b) + OutputBlackPoint.b)
				: OutputColor.b;
			}
		}
		//
	}
	*/

	if (EnableLevels == true)
	{
		OutputColor = pow(((InputColor + (ColorRangeShift * ColorRangeShiftSwitch)) - InputBlackPoint)/(InputWhitePoint - InputBlackPoint) , InputGamma) * (OutputWhitePoint - OutputBlackPoint) + OutputBlackPoint;
	} else {
		OutputColor = InputColor;
	}

  if (enableACESFilmRec2020old == true)
	{
		OutputColor = ACESFilmRec2020old(OutputColor);
	}

	if (enableACESFilmRec2020 == true)
	{
		OutputColor = ACESFilmRec2020(OutputColor);
	}

	if (enableACESFitted == true)
	{
		OutputColor = ACESFitted(OutputColor);
	}

	if (HighlightClipping == true)
	{
		float3 ClippedColor;

		ClippedColor = any(OutputColor > saturate(OutputColor)) // any colors whiter than white?
			? float3(1.0, 1.0, 0.0)
			: OutputColor;
		ClippedColor = all(OutputColor > saturate(OutputColor)) // all colors whiter than white?
			? float3(1.0, 0.0, 0.0)
			: ClippedColor;
		ClippedColor = any(OutputColor < saturate(OutputColor)) // any colors blacker than black?
			? float3(0.0, 1.0, 1.0)
			: ClippedColor;
		ClippedColor = all(OutputColor < saturate(OutputColor)) // all colors blacker than black?
			? float3(0.0, 0.0, 1.0)
			: ClippedColor;

		OutputColor = ClippedColor;
	}


	return OutputColor;
}

technique LevelsPlus
{
	pass
	{
		VertexShader = PostProcessVS;
		PixelShader = LevelsPlusPass;
	}
}


/*
for visualisation
https://www.desmos.com/calculator
\frac{\left(x-\frac{16}{255}\right)}{\left(\frac{233}{255}-\frac{16}{255}\ \right)}\cdot \left(\frac{255}{255}-0\right)+0

\left(\frac{\left(\left(\frac{\left(x-\frac{16}{255}\right)}{\left(\frac{233}{255}-\frac{16}{255}\ \right)}\cdot \left(\frac{255}{255}-0\right)+0\right)-\frac{250}{255}\right)}{\left(\left(\frac{\left(1-\frac{16}{255}\right)}{\left(\frac{233}{255}-\frac{16}{255}\ \right)}\cdot \left(\frac{255}{255}-0\right)+0\right)-\frac{250}{255}\ \right)}\cdot \left(\frac{255}{255}-\frac{250}{255}\right)+\frac{250}{255}\right)

\left(\frac{\left(\left(\frac{\left(x-\frac{16}{255}\right)}{\left(\frac{233}{255}-\frac{16}{255}\ \right)}\cdot \left(\frac{255}{255}-0\right)+0\right)-\left(\frac{\left(0-\frac{16}{255}\right)}{\left(\frac{233}{255}-\frac{16}{255}\ \right)}\cdot \left(\frac{255}{255}-0\right)+0\right)\right)}{\left(\frac{5}{255}-\left(\frac{\left(0-\frac{16}{255}\right)}{\left(\frac{233}{255}-\frac{16}{255}\ \right)}\cdot \left(\frac{255}{255}-0\right)+0\right)\right)}\cdot \left(\frac{5}{255}-\frac{0}{255}\right)+0\right)

//
//this is for x,y<0.5
\left(\sin (\pi *\left(-\frac{x}{4\cdot 0.1352}\right))^2\right)\cdot 2\cdot 0.0782

\left(\sin (\pi *\left(-\frac{x}{4\cdot [black point curve break\center] x}\right))^2\right)\cdot 2\cdot [black point curve break\center] y

//this is for x,y>0.5

1-\left(\sin (\pi *\left(-\frac{1-x}{4\cdot \left(1-0.8528\right)}\right))^2\right)\cdot 2\cdot \left(1-0.9137\right)

1-\left(\sin (\pi *\left(-\frac{1-x}{4\cdot \left(1-[white point curve break\center] x\right)}\right))^2\right)\cdot 2\cdot \left(1-[white point curve break\center] y\right)

*/
