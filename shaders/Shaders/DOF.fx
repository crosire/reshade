//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//LICENSE AGREEMENT AND DISTRIBUTION RULES:
//1 Copyrights of the Master Effect exclusively belongs to author - Gilcher Pascal aka Marty McFly.
//2 Master Effect (the SOFTWARE) is DonateWare application, which means you may or may not pay for this software to the author as donation.
//3 If included in ENB presets, credit the author (Gilcher Pascal aka Marty McFly).
//4 Software provided "AS IS", without warranty of any kind, use it on your own risk. 
//5 You may use and distribute software in commercial or non-commercial uses. For commercial use it is required to warn about using this software (in credits, on the box or other places). Commercial distribution of software as part of the games without author permission prohibited.
//6 Author can change license agreement for new versions of the software.
//7 All the rights, not described in this license agreement belongs to author.
//8 Using the Master Effect means that user accept the terms of use, described by this license agreement.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// For more information about license agreement contact me:
// https://www.facebook.com/MartyMcModding
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Advanced Depth of Field 4.2 by Marty McFly 
// Version for release
// Copyright © 2008-2015 Marty McFly
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Credits :: Matso (Matso DOF), PetkaGtA, gp65cj042
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

#include "ReShadeUI.fxh"

uniform bool DOF_AUTOFOCUS <
	ui_tooltip = "Enables automated focus recognition based on samples around autofocus center.";
> = true;
uniform bool DOF_MOUSEDRIVEN_AF <
	ui_tooltip = "Enables mouse driven auto-focus. If 1 the AF focus point is read from the mouse coordinates, otherwise the DOF_FOCUSPOINT is used.";
> = false;
uniform float2 DOF_FOCUSPOINT < __UNIFORM_SLIDER_FLOAT2
	ui_min = 0.0; ui_max = 1.0;
	ui_tooltip = "X and Y coordinates of autofocus center. Axes start from upper left screen corner.";
> = float2(0.5, 0.5);
uniform int DOF_FOCUSSAMPLES < __UNIFORM_SLIDER_INT1
	ui_min = 3; ui_max = 10;
	ui_tooltip = "Amount of samples around the focus point for smoother focal plane detection.";
> = 6;
uniform float DOF_FOCUSRADIUS < __UNIFORM_SLIDER_FLOAT1
	ui_min = 0.02; ui_max = 0.20;
	ui_tooltip = "Radius of samples around the focus point.";
> = 0.05;
uniform float DOF_NEARBLURCURVE <
	ui_type = "drag";
	ui_min = 0.5; ui_max = 1000.0;
	ui_tooltip = "Curve of blur closer than focal plane. Higher means less blur.";
> = 1.60;
uniform float DOF_FARBLURCURVE <
	ui_type = "drag";
	ui_min = 0.05; ui_max = 5.0;
	ui_tooltip = "Curve of blur behind focal plane. Higher means less blur.";
> = 2.00;
uniform float DOF_MANUALFOCUSDEPTH < __UNIFORM_SLIDER_FLOAT1
	ui_min = 0.0; ui_max = 1.0;
	ui_tooltip = "Depth of focal plane when autofocus is off. 0.0 means camera, 1.0 means infinite distance.";
> = 0.02;
uniform float DOF_INFINITEFOCUS <
	ui_type = "drag";
	ui_min = 0.01; ui_max = 1.0;
	ui_tooltip = "Distance at which depth is considered as infinite. 1.0 is standard.\nLow values only produce out of focus blur when focus object is very close to the camera. Recommended for gaming.";
> = 1.00;
uniform float DOF_BLURRADIUS <
	ui_type = "drag";
	ui_min = 2.0; ui_max = 100.0;
	ui_tooltip = "Maximal blur radius in pixels.";
> = 15.0;

// Ring DOF Settings
uniform int iRingDOFSamples < __UNIFORM_SLIDER_INT1
	ui_min = 5; ui_max = 30;
	ui_tooltip = "Samples on the first ring. The other rings around have more samples.";
> = 6;
uniform int iRingDOFRings < __UNIFORM_SLIDER_INT1
	ui_min = 1; ui_max = 8;
	ui_tooltip = "Ring count";
> = 4;
uniform float fRingDOFThreshold < __UNIFORM_SLIDER_FLOAT1
	ui_min = 0.5; ui_max = 3.0;
	ui_tooltip = "Threshold for bokeh brightening. Above this value, everything gets much much brighter.\n1.0 is maximum value for LDR games like GTASA, higher values work only on HDR games like Skyrim etc.";
> = 0.7;
uniform float fRingDOFGain < __UNIFORM_SLIDER_FLOAT1
	ui_min = 0.1; ui_max = 30.0;
	ui_tooltip = "Amount of brightening for pixels brighter than threshold.";
> = 27.0;
uniform float fRingDOFBias < __UNIFORM_SLIDER_FLOAT1
	ui_min = 0.0; ui_max = 2.0;
	ui_tooltip = "Bokeh bias";
> = 0.0;
uniform float fRingDOFFringe < __UNIFORM_SLIDER_FLOAT1
	ui_min = 0.0; ui_max = 1.0;
	ui_tooltip = "Amount of chromatic aberration";
> = 0.5;

// Magic DOF Settings
uniform int iMagicDOFBlurQuality < __UNIFORM_SLIDER_INT1
	ui_min = 1; ui_max = 30;
	ui_tooltip = "Blur quality as control value over tap count.\nQuality 15 produces 721 taps, impossible with other DOF shaders by far, most they can do is about 150.";
> = 8;
uniform float fMagicDOFColorCurve < __UNIFORM_SLIDER_FLOAT1
	ui_min = 1.0; ui_max = 10.0;
	ui_tooltip = "DOF weighting curve";
> = 4.0;

// GP65CJ042 DOF Settings
uniform int iGPDOFQuality < __UNIFORM_SLIDER_INT1
	ui_min = 0; ui_max = 7;
	ui_tooltip = "0 = only slight gaussian farblur but no bokeh. 1-7 bokeh blur, higher means better quality of blur but less fps. ";
> = 6;
uniform bool bGPDOFPolygonalBokeh <
	ui_tooltip = "Enables polygonal bokeh shape, e.g. POLYGON_NUM 5 means pentagonal bokeh shape. Setting this value to false results in circular bokeh shape.";
> = true;
uniform int iGPDOFPolygonCount < __UNIFORM_SLIDER_INT1
	ui_min = 3; ui_max = 9;
	ui_tooltip = "Controls the amount pf polygons for polygonal bokeh shape. 3 = triangular, 4 = square, 5 = pentagonal etc.";
> = 5;
uniform float fGPDOFBias < __UNIFORM_SLIDER_FLOAT1
	ui_min = 0.0; ui_max = 20.0;
	ui_tooltip = "Shifts bokeh weighting to bokeh shape edge. Set to 0 for even bright bokeh shapes, raise it for darker bokeh shapes in center and brighter on edge.";
> = 10.0;
uniform float fGPDOFBiasCurve < __UNIFORM_SLIDER_FLOAT1
	ui_min = 0.0; ui_max = 3.0;
	ui_tooltip = "Power of Bokeh Bias. Raise for more defined bokeh outlining on bokeh shape edge.";
> = 2.0;
uniform float fGPDOFBrightnessThreshold < __UNIFORM_SLIDER_FLOAT1
	ui_min = 0.5; ui_max = 2.0;
	ui_tooltip = "Threshold for bokeh brightening. Above this value, everything gets much much brighter.\n1.0 is maximum value for LDR games like GTASA, higher values work only on HDR games like Skyrim etc.";
> = 0.5;
uniform float fGPDOFBrightnessMultiplier < __UNIFORM_SLIDER_FLOAT1
	ui_min = 0.0; ui_max = 2.0;
	ui_tooltip = "Amount of brightening for pixels brighter than fGPDOFBrightnessThreshold.";
> = 2.0;
uniform float fGPDOFChromaAmount < __UNIFORM_SLIDER_FLOAT1
	ui_min = 0.0; ui_max = 0.4;
	ui_tooltip = "Amount of color shifting applied on blurred areas. ";
> = 0.15;

// MATSO DOF Settings
uniform bool bMatsoDOFChromaEnable <
	ui_tooltip = "Enables chromatic aberration.";
> = true;
uniform float fMatsoDOFChromaPow < __UNIFORM_SLIDER_FLOAT1
	ui_min = 0.2; ui_max = 3.0;
	ui_tooltip = "Amount of chromatic aberration color shifting.";
> = 1.4;
uniform float fMatsoDOFBokehCurve < __UNIFORM_SLIDER_FLOAT1
	ui_min = 0.5; ui_max = 20.0;
	ui_tooltip = "Bokeh curve";
> = 8.0;
uniform int iMatsoDOFBokehQuality < __UNIFORM_SLIDER_INT1
	ui_min = 1; ui_max = 10;
	ui_tooltip = "Blur quality as control value over tap count.";
> = 2;
uniform float fMatsoDOFBokehAngle < __UNIFORM_SLIDER_FLOAT1
	ui_min = 0; ui_max = 360; ui_step = 1;
	ui_tooltip = "Rotation angle of bokeh shape.";
> = 0;

// MCFLY ADVANCED DOF Settings - SHAPE
#ifndef bADOF_ShapeTextureEnable
	#define bADOF_ShapeTextureEnable 0 // Enables the use of a texture overlay. Quite some performance drop.
	#define iADOF_ShapeTextureSize 63 // Higher texture size means less performance. Higher quality integers better work with detailed shape textures. Uneven numbers recommended because even size textures have no center pixel.
#endif

#ifndef iADOF_ShapeVertices
	#define iADOF_ShapeVertices 5 // Polygon count of bokeh shape. 4 = square, 5 = pentagon, 6 = hexagon and so on.
#endif

uniform int iADOF_ShapeQuality < __UNIFORM_SLIDER_INT1
	ui_min = 1; ui_max = 255;
	ui_tooltip = "Quality level of DOF shape. Higher means more offsets taken, cleaner shape but also less performance. Compilation time stays same.";
> = 17;
uniform float fADOF_ShapeRotation < __UNIFORM_SLIDER_FLOAT1
	ui_min = 0; ui_max = 360; ui_step = 1;
	ui_tooltip = "Static rotation of bokeh shape.";
> = 15;
uniform bool bADOF_RotAnimationEnable <
	ui_tooltip = "Enables constant shape rotation in time.";
> = false;
uniform float fADOF_RotAnimationSpeed < __UNIFORM_SLIDER_FLOAT1
	ui_min = -5; ui_max = 5;
	ui_tooltip = "Speed of shape rotation. Negative numbers change direction.";
> = 2.0;
uniform bool bADOF_ShapeCurvatureEnable <
	ui_tooltip = "Bends edges of polygonal shape outwards (or inwards). Circular shape best with vertices > 7";
> = false;
uniform float fADOF_ShapeCurvatureAmount < __UNIFORM_SLIDER_FLOAT1
	ui_min = 0.0; ui_max = 1.0;
	ui_tooltip = "Amount of edge bending. 1.0 results in circular shape. Values below 0 produce star-like shapes.";
> = 0.3;
uniform bool bADOF_ShapeApertureEnable <
	ui_tooltip = "Enables deformation of bokeh shape into swirl-like aperture. You will recognize it when you try it out. Best with big bokeh shapes.";
> = false;
uniform float fADOF_ShapeApertureAmount < __UNIFORM_SLIDER_FLOAT1
	ui_min = -0.05; ui_max = 0.05;
	ui_tooltip = "Amount of deformation. Negative values mirror the effect. ";
> = 0.01;
uniform bool bADOF_ShapeAnamorphEnable <
	ui_tooltip = "Lessens horizontal width of shape to simulate anamorphic bokeh shape seen in movies.";
> = false;
uniform float fADOF_ShapeAnamorphRatio < __UNIFORM_SLIDER_FLOAT1
	ui_min = 0.0; ui_max = 1.0;
	ui_tooltip = "Horizontal width factor. 1.0 means 100% width, 0.0 means 0% width (bokeh shape will be vertical line).";
> = 0.2;
uniform bool bADOF_ShapeDistortEnable <
	ui_tooltip = "Deforms bokeh shape at screen borders to simulate lens distortion. Bokeh shapes at screen egdes look like an egg.";
> = false;
uniform float fADOF_ShapeDistortAmount < __UNIFORM_SLIDER_FLOAT1
	ui_min = 0.0; ui_max = 1.0;
	ui_tooltip = "Amount of deformation.";
> = 0.2;
uniform bool bADOF_ShapeDiffusionEnable <
	ui_tooltip = "Enables some fuzzyness of bokeh shape, makes it less clearly defined.";
> = false;
uniform float fADOF_ShapeDiffusionAmount < __UNIFORM_SLIDER_FLOAT1
	ui_min = 0.0; ui_max = 1.0;
	ui_tooltip = "Amount of shape diffusion. High values look like the bokeh shape exploded.";
> = 0.1;
uniform bool bADOF_ShapeWeightEnable <
	ui_tooltip = "Enables bokeh shape weight bias and shifts color to the shape borders.";
> = false;
uniform float fADOF_ShapeWeightCurve < __UNIFORM_SLIDER_FLOAT1
	ui_min = 0.5; ui_max = 8.0;
	ui_tooltip = "Curve of shape weight bias.";
> = 4.0;
uniform float fADOF_ShapeWeightAmount < __UNIFORM_SLIDER_FLOAT1
	ui_min = 0.5; ui_max = 8.0;
	ui_tooltip = "Amount of shape weight bias.";
> = 1.0;
uniform float fADOF_BokehCurve < __UNIFORM_SLIDER_FLOAT1
	ui_min = 1.0; ui_max = 20.0;
	ui_tooltip = "Bokeh factor. Higher values produce more defined bokeh shapes for separated bright spots.";
> = 4.0;

// MCFLY ADVANCED DOF Settings - CHROMATIC ABERRATION
uniform bool bADOF_ShapeChromaEnable <
	ui_tooltip = "Enables chromatic aberration at bokeh shape borders. This means 3 times more samples = less performance.";
> = false;
uniform int iADOF_ShapeChromaMode <
	ui_type = "combo";
	ui_items = "Mode 1\0Mode 2\0Mode 3\0Mode 4\0Mode 5\0Mode 6\0";
	ui_tooltip = "Switches through the possible R G B shifts.";
> = 3;
uniform float fADOF_ShapeChromaAmount < __UNIFORM_SLIDER_FLOAT1
	ui_min = 0.0; ui_max = 0.5;
	ui_tooltip = "Amount of color shifting.";
> = 0.125;
uniform bool bADOF_ImageChromaEnable <
	ui_tooltip = "Enables image chromatic aberration at screen corners.\nThis one is way more complex than the shape chroma (and any other chroma on the web).";
> = false;
uniform int iADOF_ImageChromaHues < __UNIFORM_SLIDER_INT1
	ui_min = 2; ui_max = 20;
	ui_tooltip = "Amount of samples through the light spectrum to get a smooth gradient.";
> = 5;
uniform float fADOF_ImageChromaCurve < __UNIFORM_SLIDER_FLOAT1
	ui_min = 0.5; ui_max = 2.0;
	ui_tooltip = "Image chromatic aberration curve. Higher means less chroma at screen center areas.";
> = 1.0;
uniform float fADOF_ImageChromaAmount < __UNIFORM_SLIDER_FLOAT1
	ui_min = 0.25; ui_max = 10.0;
	ui_tooltip = "Linearly increases image chromatic aberration amount.";
> = 3.0;

// MCFLY ADVANCED DOF Settings - POSTFX
uniform float fADOF_SmootheningAmount < __UNIFORM_SLIDER_FLOAT1
	ui_min = 0.5; ui_max = 2.0;
	ui_tooltip = "Blur multiplicator of box blur after bokeh to smoothen shape. Box blur is better than gaussian.";
> = 1.0;

#ifndef bADOF_ImageGrainEnable
	#define bADOF_ImageGrainEnable 0 // Enables some fuzzyness in blurred areas. The more out of focus, the more grain
#endif

#if bADOF_ImageGrainEnable
uniform float fADOF_ImageGrainCurve < __UNIFORM_SLIDER_FLOAT1
	ui_min = 0.5; ui_max = 5.0;
	ui_tooltip = "Curve of Image Grain distribution. Higher values lessen grain at moderately blurred areas.";
> = 1.0;
uniform float fADOF_ImageGrainAmount < __UNIFORM_SLIDER_FLOAT1
	ui_min = 0.1; ui_max = 2.0;
	ui_tooltip = "Linearly multiplies the amount of Image Grain applied.";
> = 0.55;
uniform float fADOF_ImageGrainScale < __UNIFORM_SLIDER_FLOAT1
	ui_min = 0.5; ui_max = 2.0;
	ui_tooltip = "Grain texture scale. Low values produce more coarse Noise.";
> = 1.0;
#endif

/////////////////////////TEXTURES / INTERNAL PARAMETERS/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////TEXTURES / INTERNAL PARAMETERS/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#if bADOF_ImageGrainEnable
texture texNoise < source = "mcnoise.png"; > { Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = RGBA8; };
sampler SamplerNoise { Texture = texNoise; };
#endif
#if bADOF_ShapeTextureEnable
texture texMask < source = "mcmask.png"; > { Width = iADOF_ShapeTextureSize; Height = iADOF_ShapeTextureSize; Format = R8; };
sampler SamplerMask { Texture = texMask; };
#endif

#define DOF_RENDERRESMULT 0.6

texture texHDR1 { Width = BUFFER_WIDTH * DOF_RENDERRESMULT; Height = BUFFER_HEIGHT * DOF_RENDERRESMULT; Format = RGBA8; };
texture texHDR2 { Width = BUFFER_WIDTH * DOF_RENDERRESMULT; Height = BUFFER_HEIGHT * DOF_RENDERRESMULT; Format = RGBA8; }; 
sampler SamplerHDR1 { Texture = texHDR1; };
sampler SamplerHDR2 { Texture = texHDR2; };

/////////////////////////FUNCTIONS//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////FUNCTIONS//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "ReShade.fxh"

uniform float2 MouseCoords < source = "mousepoint"; >;

float GetCoC(float2 coords)
{
	float scenedepth = ReShade::GetLinearizedDepth(coords);
	float scenefocus, scenecoc = 0.0;

	if (DOF_AUTOFOCUS)
	{
		scenefocus = 0.0;

		float2 focusPoint = DOF_MOUSEDRIVEN_AF ? MouseCoords * ReShade::PixelSize : DOF_FOCUSPOINT;

		[loop]
		for (int r = DOF_FOCUSSAMPLES; 0 < r; r--)
		{
			sincos((6.2831853 / DOF_FOCUSSAMPLES) * r, coords.y, coords.x);
			coords.y *= ReShade::AspectRatio;
			scenefocus += ReShade::GetLinearizedDepth(coords * DOF_FOCUSRADIUS + focusPoint);
		}
		scenefocus /= DOF_FOCUSSAMPLES;
	}
	else
	{
		scenefocus = DOF_MANUALFOCUSDEPTH;
	}

	scenefocus = smoothstep(0.0, DOF_INFINITEFOCUS, scenefocus);
	scenedepth = smoothstep(0.0, DOF_INFINITEFOCUS, scenedepth);

	float farBlurDepth = scenefocus * pow(4.0, DOF_FARBLURCURVE);

	if (scenedepth < scenefocus)
	{
		scenecoc = (scenedepth - scenefocus) / scenefocus;
	}
	else
	{
		scenecoc = (scenedepth - scenefocus) / (farBlurDepth - scenefocus);
		scenecoc = saturate(scenecoc);
	}

	return saturate(scenecoc * 0.5 + 0.5);
}

/////////////////////////PIXEL SHADERS//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////PIXEL SHADERS//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void PS_Focus(float4 vpos : SV_Position, float2 texcoord : TEXCOORD, out float4 hdr1R : SV_Target0)
{
	float4 scenecolor = tex2D(ReShade::BackBuffer, texcoord);
	scenecolor.w = GetCoC(texcoord);
	hdr1R = scenecolor;
}

// RING DOF
void PS_RingDOF1(float4 vpos : SV_Position, float2 texcoord : TEXCOORD, out float4 hdr2R : SV_Target0)
{
	float4 scenecolor = tex2D(SamplerHDR1, texcoord);

	float centerDepth = scenecolor.w;
	float blurAmount = abs(centerDepth * 2.0 - 1.0);
	float discRadius = blurAmount * DOF_BLURRADIUS;

	discRadius *= (centerDepth < 0.5) ? (1.0 / max(DOF_NEARBLURCURVE * 2.0, 1.0)) : 1.0;

	float2 blurRadius = discRadius * ReShade::PixelSize / iRingDOFRings;
	scenecolor.x = tex2Dlod(SamplerHDR1, float4(texcoord + float2( 0.000,  1.0) * fRingDOFFringe * discRadius * ReShade::PixelSize, 0, 0)).x;
	scenecolor.y = tex2Dlod(SamplerHDR1, float4(texcoord + float2(-0.866, -0.5) * fRingDOFFringe * discRadius * ReShade::PixelSize, 0, 0)).y;
	scenecolor.z = tex2Dlod(SamplerHDR1, float4(texcoord + float2( 0.866, -0.5) * fRingDOFFringe * discRadius * ReShade::PixelSize, 0, 0)).z;

	scenecolor.w = centerDepth;
	hdr2R = scenecolor;
}
void PS_RingDOF2(float4 vpos : SV_Position, float2 texcoord : TEXCOORD, out float4 blurcolor : SV_Target)
{
	blurcolor = tex2D(SamplerHDR2, texcoord);
	float4 noblurcolor = tex2D(ReShade::BackBuffer, texcoord);

	float centerDepth = GetCoC(texcoord);

	float blurAmount = abs(centerDepth * 2.0 - 1.0);
	float discRadius = blurAmount * DOF_BLURRADIUS;

	discRadius *= (centerDepth < 0.5) ? (1.0 / max(DOF_NEARBLURCURVE * 2.0, 1.0)) : 1.0;

	if (discRadius < 1.2)
	{
		blurcolor = float4(noblurcolor.xyz, centerDepth);
		return;
	}

	blurcolor.w = 1.0;

	float s = 1.0;
	int ringsamples;

	[loop]
	for (int g = 1; g <= iRingDOFRings; g += 1)
	{
		ringsamples = g * iRingDOFSamples;

		[loop]
		for (int j = 0; j < ringsamples; j += 1)
		{
			float step = 6.283 / ringsamples;
			float2 sampleoffset = 0.0;
			sincos(j * step, sampleoffset.y, sampleoffset.x);
			float4 tap = tex2Dlod(SamplerHDR2, float4(texcoord + sampleoffset * ReShade::PixelSize * discRadius * g / iRingDOFRings, 0, 0));

			float tapluma = dot(tap.xyz, 0.333);
			float tapthresh = max((tapluma - fRingDOFThreshold) * fRingDOFGain, 0.0);
			tap.xyz *= 1.0 + tapthresh * blurAmount;

			tap.w = (tap.w >= centerDepth * 0.99) ? 1.0 : pow(abs(tap.w * 2.0 - 1.0), 4.0);
			tap.w *= lerp(1.0, g / iRingDOFRings, fRingDOFBias);
			blurcolor.xyz += tap.xyz * tap.w;
			blurcolor.w += tap.w;
		}
	}

	blurcolor.xyz /= blurcolor.w;
	blurcolor.xyz = lerp(noblurcolor.xyz, blurcolor.xyz, smoothstep(1.2, 2.0, discRadius)); // smooth transition between full res color and lower res blur
	blurcolor.w = centerDepth;
}

// MAGIC DOF
void PS_MagicDOF1(float4 vpos : SV_Position, float2 texcoord : TEXCOORD, out float4 hdr2R : SV_Target0)
{
	float4 blurcolor = tex2D(SamplerHDR1, texcoord);

	float centerDepth = blurcolor.w;
	float blurAmount = abs(centerDepth * 2.0 - 1.0);
	float discRadius = blurAmount * DOF_BLURRADIUS;

	discRadius *= (centerDepth < 0.5) ? (1.0 / max(DOF_NEARBLURCURVE * 2.0, 1.0)) : 1.0;

	if (discRadius < 1.2)
	{
		hdr2R = float4(blurcolor.xyz, centerDepth);
	}
	else
	{
		blurcolor = 0.0;

		[loop]
		for (int i = -iMagicDOFBlurQuality; i <= iMagicDOFBlurQuality; ++i)
		{
			float2 tapoffset = float2(1, 0) * i;
			float4 tap = tex2Dlod(SamplerHDR1, float4(texcoord + tapoffset * discRadius * ReShade::PixelSize.x / iMagicDOFBlurQuality, 0, 0));
			tap.w = (tap.w >= centerDepth*0.99) ? 1.0 : pow(abs(tap.w * 2.0 - 1.0), 4.0);
			blurcolor.xyz += tap.xyz*tap.w;
			blurcolor.w += tap.w;
		}

		blurcolor.xyz /= blurcolor.w;
		blurcolor.w = centerDepth;
		hdr2R = blurcolor;
	}
}
void PS_MagicDOF2(float4 vpos : SV_Position, float2 texcoord : TEXCOORD, out float4 blurcolor : SV_Target)
{
	blurcolor = 0.0;
	float4 noblurcolor = tex2D(ReShade::BackBuffer, texcoord);

	float centerDepth = GetCoC(texcoord); //use fullres CoC data
	float blurAmount = abs(centerDepth * 2.0 - 1.0);
	float discRadius = blurAmount * DOF_BLURRADIUS;

	discRadius *= (centerDepth < 0.5) ? (1.0 / max(DOF_NEARBLURCURVE * 2.0, 1.0)) : 1.0;

	if (discRadius < 1.2)
	{
		blurcolor = float4(noblurcolor.xyz, centerDepth);
		return;
	}

	[loop]
	for (int i = -iMagicDOFBlurQuality; i <= iMagicDOFBlurQuality; ++i)
	{
		float2 tapoffset1 = float2(0.5, 0.866) * i;
		float2 tapoffset2 = float2(-tapoffset1.x, tapoffset1.y);

		float4 tap1 = tex2Dlod(SamplerHDR2, float4(texcoord + tapoffset1 * discRadius * ReShade::PixelSize / iMagicDOFBlurQuality, 0, 0));
		float4 tap2 = tex2Dlod(SamplerHDR2, float4(texcoord + tapoffset2 * discRadius * ReShade::PixelSize / iMagicDOFBlurQuality, 0, 0));

		blurcolor.xyz += pow(abs(min(tap1.xyz, tap2.xyz)), fMagicDOFColorCurve);
		blurcolor.w += 1.0;
	}

	blurcolor.xyz /= blurcolor.w;
	blurcolor.xyz = pow(saturate(blurcolor.xyz), 1.0 / fMagicDOFColorCurve);
	blurcolor.xyz = lerp(noblurcolor.xyz, blurcolor.xyz, smoothstep(1.2, 2.0, discRadius));
}

// GP65CJ042 DOF
void PS_GPDOF1(float4 vpos : SV_Position, float2 texcoord : TEXCOORD, out float4 hdr2R : SV_Target0)
{
	float4 blurcolor = tex2D(SamplerHDR1, texcoord);

	float centerDepth = blurcolor.w;
	float blurAmount = abs(centerDepth * 2.0 - 1.0);
	float discRadius = max(0.0, blurAmount - 0.1) * DOF_BLURRADIUS; //optimization to clean focus areas a bit

	discRadius *= (centerDepth < 0.5) ? (1.0 / max(DOF_NEARBLURCURVE * 2.0, 1.0)) : 1.0;

	float3 distortion = float3(-1.0, 0.0, 1.0);
	distortion *= fGPDOFChromaAmount;

	float4 chroma1 = tex2D(SamplerHDR1, texcoord + discRadius * ReShade::PixelSize * distortion.x);
	chroma1.w = smoothstep(0.0, centerDepth, chroma1.w);
	blurcolor.x = lerp(blurcolor.x, chroma1.x, chroma1.w);

	float4 chroma2 = tex2D(SamplerHDR1, texcoord + discRadius * ReShade::PixelSize * distortion.z);
	chroma2.w = smoothstep(0.0, centerDepth, chroma2.w);
	blurcolor.z = lerp(blurcolor.z, chroma2.z, chroma2.w);

	blurcolor.w = centerDepth;
	hdr2R = blurcolor;
}
void PS_GPDOF2(float4 vpos : SV_Position, float2 texcoord : TEXCOORD, out float4 blurcolor : SV_Target)
{
	blurcolor = tex2D(SamplerHDR2, texcoord);
	float4 noblurcolor = tex2D(ReShade::BackBuffer, texcoord);

	float centerDepth = GetCoC(texcoord);

	float blurAmount = abs(centerDepth * 2.0 - 1.0);
	float discRadius = blurAmount * DOF_BLURRADIUS;

	discRadius *= (centerDepth < 0.5) ? (1.0 / max(DOF_NEARBLURCURVE * 2.0, 1.0)) : 1.0;

	if (discRadius < 1.2)
	{
		blurcolor = float4(noblurcolor.xyz, centerDepth);
		return;
	}

	blurcolor.w = dot(blurcolor.xyz, 0.3333);
	blurcolor.w = max((blurcolor.w - fGPDOFBrightnessThreshold) * fGPDOFBrightnessMultiplier, 0.0);
	blurcolor.xyz *= (1.0 + blurcolor.w * blurAmount);
	blurcolor.xyz *= lerp(1.0, 0.0, saturate(fGPDOFBias));
	blurcolor.w = 1.0;

	int sampleCycle = 0;
	int sampleCycleCounter = 0;
	int sampleCounterInCycle = 0;
	float basedAngle = 360.0 / iGPDOFPolygonCount;
	float2 currentVertex, nextVertex;

	int	dofTaps = bGPDOFPolygonalBokeh ? (iGPDOFQuality * (iGPDOFQuality + 1) * iGPDOFPolygonCount / 2.0) : (iGPDOFQuality * (iGPDOFQuality + 1) * 4);

	for (int i = 0; i < dofTaps; i++)
	{
		//dumb step incoming
		bool dothatstep = sampleCounterInCycle == 0;
		if (sampleCycle != 0)
		{
			if (sampleCounterInCycle % sampleCycle == 0)
				dothatstep = true;
		}
		//until here
		//ask yourself why so complicated? if(sampleCounterInCycle % sampleCycle == 0 ) gives warnings when sampleCycle=0
		//but it can only be 0 when sampleCounterInCycle is also 0 so it essentially is no division through 0 even if
		//the compiler believes it, it's 0/0 actually but without disabling shader optimizations this is the only way to workaround that.

		if (dothatstep)
		{
			sampleCounterInCycle = 0;
			sampleCycleCounter++;

			if (bGPDOFPolygonalBokeh)
			{
				sampleCycle += iGPDOFPolygonCount;
				currentVertex.xy = float2(1.0, 0.0);
				sincos(basedAngle* 0.017453292, nextVertex.y, nextVertex.x);
			}
			else
			{
				sampleCycle += 8;
			}
		}

		sampleCounterInCycle++;

		float2 sampleOffset;

		if (bGPDOFPolygonalBokeh)
		{
			float sampleAngle = basedAngle / float(sampleCycleCounter) * sampleCounterInCycle;
			float remainAngle = frac(sampleAngle / basedAngle) * basedAngle;

			if (remainAngle < 0.000001)
			{
				currentVertex = nextVertex;
				sincos((sampleAngle + basedAngle) * 0.017453292, nextVertex.y, nextVertex.x);
			}

			sampleOffset = lerp(currentVertex.xy, nextVertex.xy, remainAngle / basedAngle);
		}
		else
		{
			float sampleAngle = 0.78539816 / float(sampleCycleCounter) * sampleCounterInCycle;
			sincos(sampleAngle, sampleOffset.y, sampleOffset.x);
		}

		sampleOffset *= sampleCycleCounter;

		float4 tap = tex2Dlod(SamplerHDR2, float4(texcoord + sampleOffset * discRadius * ReShade::PixelSize / iGPDOFQuality, 0, 0));

		float brightMultipiler = max((dot(tap.xyz, 0.333) - fGPDOFBrightnessThreshold) * fGPDOFBrightnessMultiplier, 0.0);
		tap.xyz *= 1.0 + brightMultipiler * abs(tap.w * 2.0 - 1.0);

		tap.w = (tap.w >= centerDepth * 0.99) ? 1.0 : pow(abs(tap.w * 2.0 - 1.0), 4.0);
		float BiasCurve = 1.0 + fGPDOFBias * pow(abs((float)sampleCycleCounter / iGPDOFQuality), fGPDOFBiasCurve);

		blurcolor.xyz += tap.xyz * tap.w * BiasCurve;
		blurcolor.w += tap.w * BiasCurve;

	}

	blurcolor.xyz /= blurcolor.w;
	blurcolor.xyz = lerp(noblurcolor.xyz, blurcolor.xyz, smoothstep(1.2, 2.0, discRadius));
}

// MATSO DOF
float4 GetMatsoDOFCA(sampler col, float2 tex, float CoC)
{
	float3 chroma = pow(float3(0.5, 1.0, 1.5), fMatsoDOFChromaPow * CoC);

	float2 tr = ((2.0 * tex - 1.0) * chroma.r) * 0.5 + 0.5;
	float2 tg = ((2.0 * tex - 1.0) * chroma.g) * 0.5 + 0.5;
	float2 tb = ((2.0 * tex - 1.0) * chroma.b) * 0.5 + 0.5;
	
	float3 color = float3(tex2Dlod(col, float4(tr,0,0)).r, tex2Dlod(col, float4(tg,0,0)).g, tex2Dlod(col, float4(tb,0,0)).b) * (1.0 - CoC);
	
	return float4(color, 1.0);
}
float4 GetMatsoDOFBlur(int axis, float2 coord, sampler SamplerHDRX)
{
	float4 blurcolor = tex2D(SamplerHDRX, coord.xy);

	float centerDepth = blurcolor.w;
	float blurAmount = abs(centerDepth * 2.0 - 1.0);
	float discRadius = blurAmount * DOF_BLURRADIUS; //optimization to clean focus areas a bit

	discRadius*=(centerDepth < 0.5) ? (1.0 / max(DOF_NEARBLURCURVE * 2.0, 1.0)) : 1.0;

	blurcolor = 0.0;

	const float2 tdirs[4] = { 
		float2(-0.306,  0.739),
		float2( 0.306,  0.739),
		float2(-0.739,  0.306),
		float2(-0.739, -0.306)
	};

	for (int i = -iMatsoDOFBokehQuality; i < iMatsoDOFBokehQuality; i++)
	{
		float2 taxis =  tdirs[axis];

		taxis.x = cos(fMatsoDOFBokehAngle * 0.0175) * taxis.x - sin(fMatsoDOFBokehAngle * 0.0175) * taxis.y;
		taxis.y = sin(fMatsoDOFBokehAngle * 0.0175) * taxis.x + cos(fMatsoDOFBokehAngle * 0.0175) * taxis.y;
		
		float2 tcoord = coord.xy + (float)i * taxis * discRadius * ReShade::PixelSize * 0.5 / iMatsoDOFBokehQuality;

		float4 ct = bMatsoDOFChromaEnable ? GetMatsoDOFCA(SamplerHDRX, tcoord.xy, discRadius * ReShade::PixelSize.x * 0.5 / iMatsoDOFBokehQuality) : tex2Dlod(SamplerHDRX, float4(tcoord.xy, 0, 0));

		// my own pseudo-bokeh weighting
		float b = dot(ct.rgb, 0.333) + length(ct.rgb) + 0.1;
		float w = pow(abs(b), fMatsoDOFBokehCurve) + abs((float)i);

		blurcolor.xyz += ct.xyz * w;
		blurcolor.w += w;
	}

	blurcolor.xyz /= blurcolor.w;
	blurcolor.w = centerDepth;
	return blurcolor;
}

void PS_MatsoDOF1(float4 vpos : SV_Position, float2 texcoord : TEXCOORD, out float4 hdr2R : SV_Target0)
{
	hdr2R = GetMatsoDOFBlur(2, texcoord, SamplerHDR1);	
}
void PS_MatsoDOF2(float4 vpos : SV_Position, float2 texcoord : TEXCOORD, out float4 hdr1R : SV_Target0)
{
	hdr1R = GetMatsoDOFBlur(3, texcoord, SamplerHDR2);	
}
void PS_MatsoDOF3(float4 vpos : SV_Position, float2 texcoord : TEXCOORD, out float4 hdr2R : SV_Target0)
{
	hdr2R = GetMatsoDOFBlur(0, texcoord, SamplerHDR1);	
}
void PS_MatsoDOF4(float4 vpos : SV_Position, float2 texcoord : TEXCOORD, out float4 blurcolor : SV_Target)
{
	float4 noblurcolor = tex2D(ReShade::BackBuffer, texcoord);
	blurcolor = GetMatsoDOFBlur(1, texcoord, SamplerHDR2);
	float centerDepth = GetCoC(texcoord); //fullres coc data

	float blurAmount = abs(centerDepth * 2.0 - 1.0);
	float discRadius = blurAmount * DOF_BLURRADIUS;

	discRadius*=(centerDepth < 0.5) ? (1.0 / max(DOF_NEARBLURCURVE * 2.0, 1.0)) : 1.0; 

	//not 1.2 - 2.0 because matso's has a weird bokeh weighting that is almost like a tonemapping and border between blur and no blur appears to harsh
	blurcolor.xyz = lerp(noblurcolor.xyz,blurcolor.xyz,smoothstep(0.2,2.0,discRadius)); 
}

// MARTY MCFLY DOF
float2 GetDistortedOffsets(float2 intexcoord, float2 sampleoffset)
{
	float2 tocenter = intexcoord - float2(0.5, 0.5);
	float3 perp = normalize(float3(tocenter.y, -tocenter.x, 0.0));

	float rotangle = length(tocenter) * 2.221 * fADOF_ShapeDistortAmount;  
	float3 oldoffset = float3(sampleoffset, 0);

	float3 rotatedoffset =  oldoffset * cos(rotangle) + cross(perp, oldoffset) * sin(rotangle) + perp * dot(perp, oldoffset) * (1.0 - cos(rotangle));

	return rotatedoffset.xy;
}

float4 tex2Dchroma(sampler2D tex, float2 sourcecoord, float2 offsetcoord)
{
	float4 res = 0.0;

	float4 sample1 = tex2Dlod(tex, float4(sourcecoord.xy + offsetcoord.xy * (1.0 - fADOF_ShapeChromaAmount), 0, 0));
	float4 sample2 = tex2Dlod(tex, float4(sourcecoord.xy + offsetcoord.xy, 0, 0));
	float4 sample3 = tex2Dlod(tex, float4(sourcecoord.xy + offsetcoord.xy * (1.0 + fADOF_ShapeChromaAmount), 0, 0));

	if (iADOF_ShapeChromaMode == 0)		
		res.xyz = float3(sample1.x, sample2.y, sample3.z);
	else if (iADOF_ShapeChromaMode == 1)	
		res.xyz = float3(sample2.x, sample3.y, sample1.z);
	else if (iADOF_ShapeChromaMode == 2)
		res.xyz = float3(sample3.x, sample1.y, sample2.z);
	else if (iADOF_ShapeChromaMode == 3)
		res.xyz = float3(sample1.x, sample3.y, sample2.z);
	else if (iADOF_ShapeChromaMode == 4)
		res.xyz = float3(sample2.x, sample1.y, sample3.z);
	else if (iADOF_ShapeChromaMode == 5)
		res.xyz = float3(sample3.x, sample2.y, sample1.z);

	res.w = sample2.w;
	return res;
}

#if bADOF_ShapeTextureEnable
	#undef iADOF_ShapeVertices
	#define iADOF_ShapeVertices 4
#endif

uniform float Timer < source = "timer"; >;

float3 BokehBlur(sampler2D tex, float2 coord, float CoC, float centerDepth)
{
	float4 res = float4(tex2Dlod(tex, float4(coord.xy, 0.0, 0.0)).xyz, 1.0);
	int ringCount = round(lerp(1.0, (float)iADOF_ShapeQuality, CoC / DOF_BLURRADIUS));
	float rotAngle = fADOF_ShapeRotation;
	float2 discRadius = CoC * ReShade::PixelSize;
	float2 edgeVertices[iADOF_ShapeVertices + 1];

	if (bADOF_ShapeWeightEnable)
		res.w = (1.0 - fADOF_ShapeWeightAmount);

	res.xyz = pow(abs(res.xyz), fADOF_BokehCurve)*res.w;

	if (bADOF_ShapeAnamorphEnable)
		discRadius.x *= fADOF_ShapeAnamorphRatio;

	if (bADOF_RotAnimationEnable)
		rotAngle += fADOF_RotAnimationSpeed * Timer * 0.005;

	float2 Grain;
	if (bADOF_ShapeDiffusionEnable)
	{
		Grain = float2(frac(sin(coord.x + coord.y * 543.31) *  493013.0), frac(cos(coord.x - coord.y * 573.31) * 289013.0));
		Grain = (Grain - 0.5) * fADOF_ShapeDiffusionAmount + 1.0;
	}

	[unroll]
	for (int z = 0; z <= iADOF_ShapeVertices; z++)
	{
		sincos((6.2831853 / iADOF_ShapeVertices)*z + radians(rotAngle), edgeVertices[z].y, edgeVertices[z].x);
	}

	[fastopt]
	for (float i = 1; i <= ringCount; i++)
	{
		[fastopt]
		for (int j = 1; j <= iADOF_ShapeVertices; j++)
		{
			float radiusCoeff = i / ringCount;
			float blursamples = i;

#if bADOF_ShapeTextureEnable
			blursamples *= 2;
#endif

			[fastopt]
			for (float k = 0; k < blursamples; k++)
			{
				if (bADOF_ShapeApertureEnable)
					radiusCoeff *= 1.0 + sin(k / blursamples * 6.2831853 - 1.5707963)*fADOF_ShapeApertureAmount; // * 2 pi - 0.5 pi so it's 1x up and down in [0|1] space.

				float2 sampleOffset = lerp(edgeVertices[j - 1], edgeVertices[j], k / blursamples) * radiusCoeff;

				if (bADOF_ShapeCurvatureEnable)
					sampleOffset = lerp(sampleOffset, normalize(sampleOffset) * radiusCoeff, fADOF_ShapeCurvatureAmount);

				if (bADOF_ShapeDistortEnable)
					sampleOffset = GetDistortedOffsets(coord, sampleOffset);

				if (bADOF_ShapeDiffusionEnable)
					sampleOffset *= Grain;

				float4 tap = bADOF_ShapeChromaEnable ? tex2Dchroma(tex, coord, sampleOffset * discRadius) : tex2Dlod(tex, float4(coord.xy + sampleOffset.xy * discRadius, 0, 0));
				tap.w = (tap.w >= centerDepth*0.99) ? 1.0 : pow(abs(tap.w * 2.0 - 1.0), 4.0);

				if (bADOF_ShapeWeightEnable)
					tap.w *= lerp(1.0, pow(length(sampleOffset), fADOF_ShapeWeightCurve), fADOF_ShapeWeightAmount);

#if bADOF_ShapeTextureEnable
				tap.w *= tex2Dlod(SamplerMask, float4((sampleOffset + 0.707) * 0.707, 0, 0)).x;
#endif

				res.xyz += pow(abs(tap.xyz), fADOF_BokehCurve) * tap.w;
				res.w += tap.w;
			}
		}
	}

	res.xyz = max(res.xyz / res.w, 0.0);
	return pow(res.xyz, 1.0 / fADOF_BokehCurve);
}

void PS_McFlyDOF1(float4 vpos : SV_Position, float2 texcoord : TEXCOORD, out float4 hdr2R : SV_Target0)
{
	texcoord /= DOF_RENDERRESMULT;

	float4 blurcolor = tex2D(SamplerHDR1, saturate(texcoord));

	float centerDepth = blurcolor.w;
	float blurAmount = abs(centerDepth * 2.0 - 1.0);
	float discRadius = blurAmount * DOF_BLURRADIUS;

	discRadius *= (centerDepth < 0.5) ? (1.0 / max(DOF_NEARBLURCURVE * 2.0, 1.0)) : 1.0;

	if (max(texcoord.x, texcoord.y) <= 1.05 && discRadius >= 1.2)
	{
		//doesn't bring that much with intelligent tap calculation
		blurcolor.xyz = (discRadius >= 1.2) ? BokehBlur(SamplerHDR1, texcoord, discRadius, centerDepth) : blurcolor.xyz;
		blurcolor.w = centerDepth;
	}

	hdr2R = blurcolor;
}
void PS_McFlyDOF2(float4 vpos : SV_Position, float2 texcoord : TEXCOORD, out float4 scenecolor : SV_Target)
{   
	scenecolor = 0.0;
	float4 blurcolor = tex2D(SamplerHDR2, texcoord*DOF_RENDERRESMULT);
	float4 noblurcolor = tex2D(ReShade::BackBuffer, texcoord);
	
	float centerDepth = GetCoC(texcoord); 
	float blurAmount = abs(centerDepth * 2.0 - 1.0);
	float discRadius = blurAmount * DOF_BLURRADIUS;

	discRadius *= (centerDepth < 0.5) ? (1.0 / max(DOF_NEARBLURCURVE * 2.0, 1.0)) : 1.0;

#if __RENDERER__ < 0xa000 && !__RESHADE_PERFORMANCE_MODE__
	[flatten]
#endif
	if (bADOF_ImageChromaEnable)
	{
		float2 coord = texcoord * 2.0 - 1.0;
		float centerfact = length(coord);
		centerfact = pow(centerfact, fADOF_ImageChromaCurve) * fADOF_ImageChromaAmount;

		float chromafact = BUFFER_RCP_WIDTH * centerfact * discRadius;
		float3 chromadivisor = 0.0;

		for (float c = 0; c < iADOF_ImageChromaHues; c++)
		{
			float temphue = c / iADOF_ImageChromaHues;
			float3 tempchroma = saturate(float3(abs(temphue * 6.0 - 3.0) - 1.0, 2.0 - abs(temphue * 6.0 - 2.0), 2.0 - abs(temphue * 6.0 - 4.0)));
			float  tempoffset = (c + 0.5) / iADOF_ImageChromaHues - 0.5;
			float3 tempsample = tex2Dlod(SamplerHDR2, float4((coord.xy * (1.0 + chromafact * tempoffset) * 0.5 + 0.5) * DOF_RENDERRESMULT, 0, 0)).xyz;
			scenecolor.xyz += tempsample.xyz*tempchroma.xyz;
			chromadivisor += tempchroma;
		}

		scenecolor.xyz /= dot(chromadivisor.xyz, 0.333);
	}
	else
	{
		scenecolor = blurcolor;
	}

	scenecolor.xyz = lerp(scenecolor.xyz, noblurcolor.xyz, smoothstep(2.0,1.2,discRadius));

	scenecolor.w = centerDepth;
}
void PS_McFlyDOF3(float4 vpos : SV_Position, float2 texcoord : TEXCOORD, out float4 scenecolor : SV_Target)
{
	scenecolor = tex2D(ReShade::BackBuffer, texcoord);
	float4 blurcolor = 0.0001;
	float outOfFocus = abs(scenecolor.w * 2.0 - 1.0);

	//move all math out of loop if possible
	float2 blurmult = smoothstep(0.3, 0.8, outOfFocus) * ReShade::PixelSize * fADOF_SmootheningAmount;

	float weights[3] = { 1.0,0.75,0.5 };
	//Why not separable? For the glory of Satan, of course!
	for (int x = -2; x <= 2; x++)
	{
		for (int y = -2; y <= 2; y++)
		{
			float2 offset = float2(x, y);
			float offsetweight = weights[abs(x)] * weights[abs(y)];
			blurcolor.xyz += tex2Dlod(ReShade::BackBuffer, float4(texcoord + offset.xy * blurmult, 0, 0)).xyz * offsetweight;
			blurcolor.w += offsetweight;
		}
	}

	scenecolor.xyz = blurcolor.xyz / blurcolor.w;

#if bADOF_ImageGrainEnable
	float ImageGrain = frac(sin(texcoord.x + texcoord.y * 543.31) *  893013.0 + Timer * 0.001);

	float3 AnimGrain = 0.5;
	float2 GrainPixelSize = ReShade::PixelSize / fADOF_ImageGrainScale;
	//My emboss noise
	AnimGrain += lerp(tex2D(SamplerNoise, texcoord * fADOF_ImageGrainScale + float2(GrainPixelSize.x, 0)).xyz, tex2D(SamplerNoise, texcoord * fADOF_ImageGrainScale + 0.5 + float2(GrainPixelSize.x, 0)).xyz, ImageGrain) * 0.1;
	AnimGrain -= lerp(tex2D(SamplerNoise, texcoord * fADOF_ImageGrainScale + float2(0, GrainPixelSize.y)).xyz, tex2D(SamplerNoise, texcoord * fADOF_ImageGrainScale + 0.5 + float2(0, GrainPixelSize.y)).xyz, ImageGrain) * 0.1;
	AnimGrain = dot(AnimGrain.xyz, 0.333);

	//Photoshop overlay mix mode
	float3 graincolor = (scenecolor.xyz < 0.5 ? (2.0 * scenecolor.xyz * AnimGrain.xxx) : (1.0 - 2.0 * (1.0 - scenecolor.xyz) * (1.0 - AnimGrain.xxx)));
	scenecolor.xyz = lerp(scenecolor.xyz, graincolor.xyz, pow(outOfFocus, fADOF_ImageGrainCurve) * fADOF_ImageGrainAmount);
#endif

	//focus preview disabled!
}

/////////////////////////TECHNIQUES/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////TECHNIQUES/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

technique RingDOF
{
	pass Focus { VertexShader = PostProcessVS; PixelShader = PS_Focus; RenderTarget = texHDR1; }
	pass RingDOF1 { VertexShader = PostProcessVS; PixelShader = PS_RingDOF1; RenderTarget = texHDR2; }
	pass RingDOF2 { VertexShader = PostProcessVS; PixelShader = PS_RingDOF2; /* renders to backbuffer*/ }
}

technique MagicDOF
{
	pass Focus { VertexShader = PostProcessVS; PixelShader = PS_Focus; RenderTarget = texHDR1; }
	pass MagicDOF1 { VertexShader = PostProcessVS; PixelShader = PS_MagicDOF1; RenderTarget = texHDR2; }
	pass MagicDOF2 { VertexShader = PostProcessVS; PixelShader = PS_MagicDOF2; /* renders to backbuffer*/ }
}

technique GP65CJ042DOF
{
	pass Focus { VertexShader = PostProcessVS; PixelShader = PS_Focus; RenderTarget = texHDR1; }
	pass GPDOF1 { VertexShader = PostProcessVS; PixelShader = PS_GPDOF1; RenderTarget = texHDR2; }
	pass GPDOF2 { VertexShader = PostProcessVS; PixelShader = PS_GPDOF2; /* renders to backbuffer*/ }
}

technique MatsoDOF
{
	pass Focus { VertexShader = PostProcessVS; PixelShader = PS_Focus; RenderTarget = texHDR1; }
	pass MatsoDOF1 { VertexShader = PostProcessVS; PixelShader = PS_MatsoDOF1; RenderTarget = texHDR2; }
	pass MatsoDOF2 { VertexShader = PostProcessVS; PixelShader = PS_MatsoDOF2; RenderTarget = texHDR1; }
	pass MatsoDOF3 { VertexShader = PostProcessVS; PixelShader = PS_MatsoDOF3; RenderTarget = texHDR2; }
	pass MatsoDOF4 { VertexShader = PostProcessVS; PixelShader = PS_MatsoDOF4; /* renders to backbuffer*/ }
}

technique MartyMcFlyDOF
{
	pass Focus { VertexShader = PostProcessVS; PixelShader = PS_Focus; RenderTarget = texHDR1; }
	pass McFlyDOF1 { VertexShader = PostProcessVS; PixelShader = PS_McFlyDOF1; RenderTarget = texHDR2; }
	pass McFlyDOF2 { VertexShader = PostProcessVS; PixelShader = PS_McFlyDOF2; /* renders to backbuffer*/ }
	pass McFlyDOF3 { VertexShader = PostProcessVS; PixelShader = PS_McFlyDOF3; /* renders to backbuffer*/ }
}
