/**
Pantomorphic PS, version 4.5.0
(c) 2021 Jakub Maksymilian Fober (the Author).

The Author provides this shader (the Work)
under the Creative Commons CC BY-NC-ND 3.0 license available online at
	http://creativecommons.org/licenses/by-nc-nd/3.0/.
The Author further grants permission for reuse of screen-shots and game-play
recordings derived from the Work, provided that the reuse is for the purpose of
promoting and/or summarizing the Work, or is a part of an online forum post or
social media post and that any use is accompanied by a link to the Work and a
credit to the Author. (crediting Author by pseudonym "Fubax" is acceptable)

For all other uses and inquiries please contact the Author,
jakub.m.fober@pm.me
For updates visit GitHub repository,
https://github.com/Fubaxiusz/fubax-shaders
*/


  ////////////
 /// MENU ///
////////////

#include "ReShadeUI.fxh"

// FIELD OF VIEW

uniform int FovAngle < __UNIFORM_SLIDER_INT1
	ui_category = "Game field-of-view"; ui_category_closed = false;
	ui_label = "FOV angle";
	ui_tooltip = "This setting should match your in-game FOV (in degrees).";
	#if __RESHADE__ < 40000
		ui_step = 0.2;
	#endif
	ui_min = 0; ui_max = 170;
> = 90;

uniform int FovType < __UNIFORM_COMBO_INT1
	ui_category = "Game field-of-view";
	ui_label = "FOV type";
	ui_tooltip = "This setting should match your in-game FOV type.\n"
		"\nIn stereographic mode (ref. option), adjust\n"
		"such that round objects are still round, not oblong,\n"
		"when at corners.\n"
		"\nIf image bulges in movement (too high FOV),\n"
		"change it to 'Diagonal'.\n"
		"When proportions are distorted at the periphery\n"
		"(too low FOV), choose 'Vertical' or '4:3'.\n"
		"For ultra-wide display you may want '16:9' instead.";
	ui_items =
		"horizontal\0"
		"diagonal\0"
		"vertical\0"
		"4:3\0"
		"16:9\0";
> = 0;

// PERSPECTIVE

uniform int SimplePresets < __UNIFORM_RADIO_INT1
	ui_category = "Presets"; ui_category_closed = false;
	ui_label = "Gaming style";
	ui_tooltip = "Choose preferable gaming style.\n\n"
		"Shooting    [0.0 0.75 -0.5]  (aiming, panini, distance)\n"
		"Racing      [0.5 -0.5 0.0]   (cornering, distance, speed)\n"
		"Skating     [0.5 0.5]        (stereographic lens)\n"
		"Flying      [-0.5 0.0]       (distance, pitch)\n"
		"Stereopsis  [0.0 -0.5]       (aiming, distance)\n"
		"Cinematic   [0.618 0.862]    (panini-anamorphic)\n";
	ui_items =
		"shooting (as.)\0"
		"racing (as.)\0"
		"skating (ref.)\0"
		"flying\0"
		"stereopsis\0"
		"cinematic\0";
> = 2;


uniform bool Manual < __UNIFORM_INPUT_BOOL1
	ui_category = "Manual"; ui_category_closed = true;
	ui_label = "enable Manual";
	ui_tooltip = "Change horizontal and vertical perspective manually.";
> = false;

uniform bool AsymmetricalManual < __UNIFORM_INPUT_BOOL1
	ui_category = "Manual";
	ui_label = "enable Asymmetrical";
	ui_spacing = 1;
	ui_tooltip = "Third option drives the bottom half of the screen.";
> = false;

uniform int PresetKx < __UNIFORM_LIST_INT1
	ui_category = "Manual";
	ui_label = "Horizontal persp.";
	ui_tooltip = "Kx\n"
		"\nProjection type for horizontal axis.";
	ui_items =
		"shape,angles\0"
		"speed,aim\0"
		"distance,size\0";
> = 0;

uniform int PresetKy < __UNIFORM_LIST_INT1
	ui_category = "Manual";
	ui_label = "Vertical persp.";
	ui_tooltip = "Ky\n"
		"\nProjection type for vertical axis.";
	ui_items =
		"shape,angles\0"
		"speed,aim\0"
		"distance,size\0";
> = 0;

uniform int PresetKz < __UNIFORM_LIST_INT1
	ui_category = "Manual";
	ui_label = "Vertical bottom (as.)";
	ui_tooltip = "Kz\n"
		"\nProjection type for bottom-vertical axis,\n"
		"an asymmetrical perspective.";
	ui_items =
		"shape,angles\0"
		"speed,aim\0"
		"distance,size\0";
> = 0;


uniform bool Expert < __UNIFORM_INPUT_BOOL1
	ui_category = "Expert"; ui_category_closed = true;
	ui_label = "enable Expert";
	ui_tooltip = "Change pantomorphic K values manually.";
> = false;

uniform bool AsymmetricalExpert < __UNIFORM_INPUT_BOOL1
	ui_category = "Expert";
	ui_label = "enable Asymmetrical";
	ui_spacing = 1;
	ui_tooltip = "Third value of K, drives the bottom-half of the screen.";
> = false;

uniform float3 K < __UNIFORM_SLIDER_FLOAT3
	ui_category = "Expert";
	ui_label = "K pantomorphic";
	ui_tooltip =
		"K   1   Rectilinear projection (standard), preserves straight lines,"
		" but doesn't preserve proportions, angles or scale.\n"
		"K  0.5  Stereographic projection (navigation, shapes) preserves angles and proportions,"
		" best for navigation through tight spaces.\n"
		"K   0   Equidistant (aiming) maintains angular speed of motion,"
		" best for aiming at fast targets.\n"
		"K -0.5  Equisolid projection (distances) preserves area relation,"
		" best for navigation in open spaces.\n"
		"K  -1   Orthographic projection preserves planar luminance as cosine-law,"
		" has extreme radial compression. Found in peephole viewer.";
	ui_min = -1; ui_max = 1;
> = float3(1, 1, 1);

// BORDER

uniform float Zoom < __UNIFORM_DRAG_FLOAT1
	ui_category = "Border settings"; ui_category_closed = true;
	ui_label = "Scale image";
	ui_tooltip = "Adjust image scale and cropped area size.";
	ui_min = 0.8; ui_max = 1.5; ui_step = 0.001;
> = 1;

uniform float4 BorderColor < __UNIFORM_COLOR_FLOAT4
	ui_category = "Border settings";
	ui_label = "Border color";
	ui_tooltip = "Use Alpha to change transparency.";
> = float4(0.027, 0.027, 0.027, 0.5);

uniform float BorderCorners < __UNIFORM_SLIDER_FLOAT1
	ui_category = "Border settings";
	ui_label = "Corner roundness";
	ui_spacing = 2;
	ui_tooltip = "Represents corners curvature.\n"
		"Value of 0, gives sharp corners.";
	ui_min = 0; ui_max = 1;
> = 0.0862;

uniform bool MirrorBorder < __UNIFORM_INPUT_BOOL1
	ui_category = "Border settings";
	ui_label = "Mirror background";
	ui_tooltip = "Choose original or mirrored image at the border";
> = true;

uniform int VignettingStyle < __UNIFORM_RADIO_INT1
	ui_category = "Border settings";
	ui_label = "Vignetting style";
	ui_spacing = 1;
	ui_tooltip = "Redering options for automatic vignette.";
	ui_items =
		"vignette turned off\0"
		"vignette inside\0"
		"vignette on borders\0";
> = 1;

#ifndef G_CONTINUITY_CORNER_ROUNDING
	#define G_CONTINUITY_CORNER_ROUNDING 2
#endif

#ifndef SIDE_BY_SIDE_3D
	#define SIDE_BY_SIDE_3D 0
#endif


  ////////////////
 /// TEXTURES ///
////////////////

#include "ReShade.fxh"

// Define screen texture with mirror tiles
sampler BackBuffer
{
	Texture = ReShade::BackBufferTex;
	AddressU = MIRROR;
	AddressV = MIRROR;
	#if BUFFER_COLOR_BIT_DEPTH != 10
		SRGBTexture = true;
	#endif
};


  /////////////////
 /// FUNCTIONS ///
/////////////////

// Get reciprocal screen aspect ratio (1/x)
#define BUFFER_RCP_ASPECT_RATIO (BUFFER_HEIGHT*BUFFER_RCP_WIDTH)
// Convert to linear gamma all vector types
#define sRGB_TO_LINEAR(g) pow(abs(g), 2.2)
/**
Linear pixel step function for anti-aliasing by Jakub Max Fober.
This algorithm is part of scientific paper:
	arXiv: 20104077 [cs.GR] (2020)
*/
float aastep(float grad)
{
	float2 Del = float2(ddx(grad), ddy(grad));
	return saturate(rsqrt(dot(Del, Del))*grad+0.5);
}

/**
G continuity distance function by Jakub Max Fober.
Determined empirically. (G from 0, to 3)
	G=0 -> Sharp corners
	G=1 -> Round corners
	G=2 -> Smooth corners
	G=3 -> Luxury corners
*/
float glength(int G, float2 pos)
{
	if (G<=0) return max(abs(pos.x), abs(pos.y)); // G0
	pos = pow(abs(pos), ++G); // Power of G+1
	return pow(pos.x+pos.y, rcp(G)); // Power G+1 root
}

/**
Pantomorphic perspective model by Jakub Max Fober,
Gnomonic to anamorphic-fisheye variant.
This algorithm is a part of scientific paper:
	arXiv: 2102.12682 [cs.GR] (2021)
Input data:
	k [x, y]  -> distortion parameter (from -1, to 1).
	halfOmega -> Camera half Field of View in radians.
	viewcoord -> view coordinates (centered at 0), with correct aspect ratio.
Output data:
	viewcoord -> rectilinear lookup view coordinates
	vignette  -> vignetting mask in linear space
*/
float pantomorphic(float halfOmega, float2 k, inout float2 viewcoord)
{
	// Bypass
	if (halfOmega==0.0) return 1.0;

	// Get reciprocal focal length from horizontal FOV
	float rcp_f;
	{
		// Horizontal
		if      (k.x>0.0) rcp_f = tan(k.x*halfOmega)/k.x;
		else if (k.x<0.0) rcp_f = sin(k.x*halfOmega)/k.x;
		else              rcp_f = halfOmega;
	}

	// Get radius
	float r = length(viewcoord);

	// Get incident angle
	float2 theta2;
	{
		// Horizontal
		if      (k.x>0.0) theta2.x = atan(r*k.x*rcp_f)/k.x;
		else if (k.x<0.0) theta2.x = asin(r*k.x*rcp_f)/k.x;
		else              theta2.x = r*rcp_f;
		// Vertical
		if      (k.y>0.0) theta2.y = atan(r*k.y*rcp_f)/k.y;
		else if (k.y<0.0) theta2.y = asin(r*k.y*rcp_f)/k.y;
		else              theta2.y = r*rcp_f;
	}

	// Get phi interpolation weights
	float2 philerp = viewcoord*viewcoord; philerp /= philerp.x+philerp.y; // cos^2 sin^2 of phi angle

	// Generate vignette
	float vignetteMask; if (VignettingStyle!=0)
	{	// Limit FOV span, k-+ in [0.5, 1] range
		float2 vignetteMask2 = cos(max(abs(k), 0.5)*theta2);
		// Mix cosine-law of illumination and inverse-square law
		vignetteMask2 = pow(abs(vignetteMask2), k*0.5+1.5);
		// Blend horizontal and vertical vignetting
		vignetteMask = dot(vignetteMask2, philerp);
	} else vignetteMask = 1.0; // Bypass

	// Blend projections
	float theta = dot(theta2, philerp);
	// Transform screen coordinates and normalize to FOV
	viewcoord *= tan(theta)/(tan(halfOmega)*r);

	return vignetteMask;
}


  //////////////
 /// SHADER ///
//////////////

// Border mask shader with rounded corners
float BorderMaskPS(float2 borderCoord)
{
	// Get coordinates for each corner
	borderCoord = abs(borderCoord);
	if (BorderCorners!=0.0) // If round corners
	{
		// Correct corner aspect ratio
		if (BUFFER_ASPECT_RATIO>1.0) // If in landscape mode
			borderCoord.x = borderCoord.x*BUFFER_ASPECT_RATIO+(1.0-BUFFER_ASPECT_RATIO);
		else if (BUFFER_ASPECT_RATIO<1.0) // If in portrait mode
			borderCoord.y = borderCoord.y*BUFFER_RCP_ASPECT_RATIO+(1.0-BUFFER_RCP_ASPECT_RATIO);
		// Generate scaled coordinates
		borderCoord = max(borderCoord+(BorderCorners-1.0), 0.0)/BorderCorners;
		// Round corner
		return aastep(glength(G_CONTINUITY_CORNER_ROUNDING, borderCoord)-1.0); // with G1 to 3 continuity
	} // Just sharp corner, G0
	else return aastep(glength(0, borderCoord)-1.0);
}

// Main perspective shader pass
float3 PantomorphicPS(float4 pos : SV_Position, float2 texCoord : TEXCOORD) : SV_Target
{
#if SIDE_BY_SIDE_3D
	// Side-by-side 3D content
	float SBS3D = texCoord.x*2f;
	texCoord.x = frac(SBS3D);
	SBS3D = floor(SBS3D);
#endif

	// Convert FOV to horizontal
	float halfHorizontalFov = tan(radians(FovAngle*0.5));
	// Scale to horizontal tangent
	switch(FovType)
	{
		// Diagonal
		case 1: halfHorizontalFov *= rsqrt(BUFFER_RCP_ASPECT_RATIO*BUFFER_RCP_ASPECT_RATIO+1); break;
		// Vertical
		case 2: halfHorizontalFov *= BUFFER_ASPECT_RATIO; break;
		// Horizontal 4:3
		case 3: halfHorizontalFov *= (3.0/4.0)*BUFFER_ASPECT_RATIO; break;
		// case 3: halfHorizontalFov /= (4.0/3.0)*BUFFER_RCP_ASPECT_RATIO; break;
		// Horizontal 16:9
		case 4: halfHorizontalFov *= (9.0/16.0)*BUFFER_ASPECT_RATIO; break;
		// case 4: halfHorizontalFov /= (16.0/9.0)*BUFFER_RCP_ASPECT_RATIO; break;
		// ...more
	}
	// Half-horizontal FOV in radians
	halfHorizontalFov = atan(halfHorizontalFov);

	// Convert UV to centered coordinates
	float2 sphCoord = texCoord*2.0-1.0;
	// Aspect Ratio correction
	sphCoord.y *= BUFFER_RCP_ASPECT_RATIO;
	// Zooming
	sphCoord *= clamp(Zoom, 0.8, 1.5); // Anti-cheat clamp

	// Manage presets
	float2 k;
	if (Expert) k = clamp(
		// Create asymmetrical anamorphic
		(AsymmetricalExpert && sphCoord.y >= 0.0)? K.xz : K.xy,
		-1.0, 1.0);
	else if (Manual)
	{
		switch (PresetKx)
		{
			default: k.x =  0.5; break; // x Shape/angle
			case 1:  k.x =  0.0; break; // x Speed/aim
			case 2:  k.x = -0.5; break; // x Distance/size
			// ...more
		}
		switch ((AsymmetricalManual && sphCoord.y >= 0.0)? PresetKz : PresetKy)
		{
			default: k.y =  0.5; break; // y Shape/angle
			case 1:  k.y =  0.0; break; // y Speed/aim
			case 2:  k.y = -0.5; break; // y Distance/size
			// ...more
		}
	}
	else switch (SimplePresets)
	{
		default: k = float2( 0.0, sphCoord.y < 0.0? 0.75 :-0.5); break; // Shooting
		case 1:  k = float2( 0.5, sphCoord.y < 0.0?-0.5  : 0.0); break; // Racing
		case 2:  k = float2( 0.5, 0.5); break; // Skating (reference)
		case 3:  k = float2(-0.5, 0.0); break; // Flying
		case 4:  k = float2( 0.0,-0.5); break; // Stereopsis
		case 5:  k = float2( 0.618, 0.862); break; // Cinematic
		// ...more
	}

	// Perspective transform and create vignette
	float vignetteMask = pantomorphic(halfHorizontalFov, k, sphCoord);

	// Aspect Ratio back to square
	sphCoord.y *= BUFFER_ASPECT_RATIO;
	// Get no-border flag
	bool noBorder = VignettingStyle!=1 && BorderColor.a==0.0 && MirrorBorder;
	// Outside border mask with Anti-Aliasing
	float borderMask = noBorder? 0f : BorderMaskPS(sphCoord);
	// Back to UV Coordinates
	sphCoord = sphCoord*0.5+0.5;

#if SIDE_BY_SIDE_3D
	// Side-by-side 3D content
	sphCoord.x = (sphCoord.x+SBS3D)*0.5;
	texCoord.x = (texCoord.x+SBS3D)*0.5;
#endif

	// Sample display image
	float3 display = tex2D(BackBuffer, sphCoord).rgb;
	// Return image if no border is visible
	if (noBorder) return vignetteMask*display;

	// Mask outside-border pixels or mirror
	if (VignettingStyle==2)
		return vignetteMask*lerp(
			display,
			lerp(
				MirrorBorder? display : tex2D(BackBuffer, texCoord).rgb,
				sRGB_TO_LINEAR(BorderColor.rgb),
				sRGB_TO_LINEAR(BorderColor.a)
			), borderMask);
	else return lerp(
		vignetteMask*display,
		lerp(
			MirrorBorder? display : tex2D(BackBuffer, texCoord).rgb,
			sRGB_TO_LINEAR(BorderColor.rgb),
			sRGB_TO_LINEAR(BorderColor.a)
		), borderMask);
}


  //////////////
 /// OUTPUT ///
//////////////

technique Pantomorphic <
	ui_tooltip =
		"Adjust perspective for distortion-free picture\n"
		"(asymmetrical anamorphic, fish-eye and vignetting)\n"
		"\nManual:\n"
		"Fist select proper FOV angle and type.\n"
		"If FOV type is unknown, set preset to 'skating' and find a round object within the game.\n"
		"look at it upfront, then rotate the camera so that the object is in the corner.\n"
		"Change FOV type such that the object does not have an egg shape, but a perfect round shape.\n"
		"\nSecondly adjust perspective type according to game-play style.\n"
		"Thirdly, adjust visible borders. You can zoom in the picture to hide borders and reveal UI.\n"
		"\nAdditionally for sharp image, use FilmicSharpen.fx or run the game at Super-Resolution.";
>
{
	pass
	{
		VertexShader = PostProcessVS;
		PixelShader = PantomorphicPS;
		SRGBWriteEnable = true;
	}
}
