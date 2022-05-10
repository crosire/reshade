/**
Perfect Perspective PS, version 4.0.1
All rights (c) 2018 Jakub Maksymilian Fober (the Author).

The Author provides this shader (the Work) under
the Creative Commons CC BY-NC-ND 3.0 license
available online at
http://creativecommons.org/licenses/by-nc-nd/3.0/

The Author further grants permission for commercial reuse of
screen-shots and game-play recordings derived from the Work, provided
that any use is accompanied by the link to the Work and a credit to the
Author. (crediting Author by pseudonym "Fubax" is acceptable)

For inquiries please contact jakub.m.fober@pm.me
For updates visit GitHub repository at
https://github.com/Fubaxiusz/fubax-shaders/

This shader version is based upon research papers
by Fober, J. M.
	Perspective picture from Visual Sphere:
	a new approach to image rasterization
	arXiv: 2003.10558 [cs.GR] (2020)
	https://arxiv.org/abs/2003.10558
and
	Temporally-smooth Antialiasing and Lens Distortion
	with Rasterization Map
	arXiv: 2010.04077 [cs.GR] (2020)
	https://arxiv.org/abs/2010.04077
*/


  ////////////
 /// MENU ///
////////////

#include "ReShadeUI.fxh"

// FIELD OF VIEW

uniform uint FOV < __UNIFORM_SLIDER_INT1
	ui_text =
		"Adjust first,\n"
		"match game settings";
	ui_category = "Game settings";
	ui_category_closed = true;
	ui_label = "field of view (FOV)";
	ui_tooltip = "This setting should match your in-game FOV setting (in degrees)";
	#if __RESHADE__ < 40000
		ui_step = 0.2;
	#endif
	ui_max = 170u;
> = 90u;

uniform uint FovType < __UNIFORM_COMBO_INT1
	ui_category = "Game settings";
	ui_label = "field of view type";
	ui_tooltip =
		"This setting should match game-specific FOV type\n\n"
		"Tip:\n\n"
		"If image bulges in movement (too high FOV),\n"
		"change it to 'diagonal'.\n"
		"When proportions are distorted at the periphery\n"
		"(too low FOV), choose 'vertical' or '4:3'.\n"
		"For ultra-wide display you may want '16:9' instead.\n\n"
		"Adjust so that round objects are still round when\n"
		"at the corner, and not oblong. Tilt head to see better.\n\n"
		"* This method works only in 'shape' preset,\n"
		"or with k = 0.5 in expert mode.";
	ui_items =
		"horizontal\0"
		"diagonal\0"
		"vertical\0"
		"4:3\0"
		"16:9\0";
> = 0u;

// PERSPECTIVE

uniform uint Projection < __UNIFORM_COMBO_INT1
	ui_text =
		"Adjust second,\n"
		"distortion amount";
	ui_category = "Perspective (presets)";
	ui_category_closed = true;
	ui_label = "perspective type";
	ui_tooltip =
		"Choose type of perspective, according to game-play style.\n\n"
		" perception |  K   | projection\n"
		" ------------------------------\n"
		" Shape      |  0.5 | Stereographic\n"
		" Position   |  0   | Equidistant\n"
		" Distance   | -0.5 | Equisolid";
	ui_items =
		"Perception of Shape\0"
		"Perception of Position\0"
		"Perception of Distance\0";
> = 0u;

uniform uint AnamorphicSqueeze < __UNIFORM_COMBO_INT1
	ui_category = "Perspective (presets)";
	ui_label = "squeeze factor";
	ui_tooltip =
		"Adjust vertical distortion amount\n\n"
		" squeeze | example\n"
		" -----------------\n"
		"      1x | square\n"
		"   1.25x | Ultra Panavision 70\n"
		// "   1.33x | 16x9 TV\n"
		"    1.5x | Technirama\n"
		// "    1.6x | digital anamorphic\n"
		"      2x | golden-standard\n";
	ui_items =
		"Anamorphic 1x\0"
		"Anamorphic 1.25x\0"
		// "Anamorphic 1.33x\0"
		"Anamorphic 1.5x\0"
		// "Anamorphic 1.6x\0"
		"Anamorphic 2x\0";
> = 2u;

uniform bool ExpertMode <
	ui_category = "Perspective (expert)";
	ui_category_closed = true;
	ui_tooltip = "Manually adjust projection type and anamorphic squeeze power";
	ui_label = "Expert mode";
> = false;

uniform float K < __UNIFORM_SLIDER_FLOAT1
	ui_category = "Perspective (expert)";
	ui_label = "K (projection)";
	ui_tooltip =
		"Projection coefficient\n\n"
		"  K   | perception   | projection\n"
		" --------------------------------\n"
		"  1   | Path         | Rectilinear\n"
		"  0.5 | Shape        | Stereographic\n"
		"  0   | Position     | Equidistant\n"
		" -0.5 | Distance     | Equisolid\n"
		" -1   | Illumination | Orthographic\n\n"
		"Rectilinear projection (standard):\n"
		" * doesn't preserve proportion, angle or scale\n"
		" * common standard projection (pinhole model)\n"
		"Stereographic projection (navigation, shape):\n"
		" * preserves angle and proportion\n"
		" * best for navigation through tight space\n"
		"Equidistant projection (aiming, speed):\n"
		" * maintains angular speed of motion\n"
		" * best for aiming at target\n"
		"Equisolid projection (distance):\n"
		" * preserves area relation\n"
		" * best for navigation in open space\n"
		"Orthographic projection:\n"
		" * preserves planar luminance as cosine-law\n"
		" * found in peephole viewer";
	ui_min = -1f; ui_max = 1f;
> = 1f;

uniform float S < __UNIFORM_SLIDER_FLOAT1
	ui_category = "Perspective (expert)";
	ui_label = "S (squeeze)";
	ui_tooltip =
		"Anamorphic squeeze factor\n\n"
		" squeeze power | example\n"
		" -----------------------\n"
		"            1x | square\n"
		"         1.25x | Ultra Panavision 70\n"
		"         1.33x | 16x9 TV\n"
		"          1.5x | Technirama\n"
		"          1.6x | digital anamorphic\n"
		"          1.8x | 4x3 full-frame\n"
		"            2x | golden-standard\n";
	ui_min = 1f; ui_max = 4f; ui_step = 0.01;
> = 1f;

// PICTURE

uniform float CroppingFactor < __UNIFORM_SLIDER_FLOAT1
	ui_text =
		"Adjust third,\n"
		"scale image";
	ui_category = "Picture adjustment";
	ui_category_closed = true;
	ui_label = "picture cropping";
	ui_tooltip =
		"Adjust image scale and cropped area size\n\n"
		" value | crop type\n"
		" -----------------\n"
		" 0     | Circular\n"
		" 0.5   | Cropped-circle\n"
		" 1     | Full-frame";
	ui_min = 0f; ui_max = 1f;
> = 0.5;

uniform bool UseVignette < __UNIFORM_INPUT_BOOL1
	ui_category = "Picture adjustment";
	ui_label = "Add vignetting";
	ui_tooltip = "Apply lens-correct natural vignetting effect";
> = true;

// BORDER

uniform float4 BorderColor < __UNIFORM_COLOR_FLOAT4
	ui_category = "Border settings";
	ui_category_closed = true;
	ui_label = "border color";
	ui_tooltip = "Use alpha to change border transparency";
> = float4(0.027, 0.027, 0.027, 0.96);

uniform bool MirrorBorder < __UNIFORM_INPUT_BOOL1
	ui_category = "Border settings";
	ui_label = "Mirror border";
	ui_tooltip = "Choose mirrored or original image on the border";
> = true;

uniform bool BorderVignette < __UNIFORM_INPUT_BOOL1
	ui_category = "Border settings";
	ui_label = "Vignette on border";
	ui_tooltip = "Apply vignetting effect to border";
> = false;

uniform float BorderCorner < __UNIFORM_SLIDER_FLOAT1
	ui_category = "Border settings";
	ui_label = "corner size";
	ui_tooltip = "0.0 gives sharp corners";
	ui_min = 0f; ui_max = 1f;
> = 0.062;

uniform uint BorderGContinuity < __UNIFORM_SLIDER_INT1
	ui_category = "Border settings";
	ui_label = "corner roundness";
	ui_tooltip =
		"G-surfacing continuity level for\n"
		"the corners\n\n"
		" G  | corner type\n"
		" ----------------\n"
		" G0 | sharp\n"
		" G1 | circular\n"
		" G2 | round\n"
		" G3 | smooth";
	ui_min = 1u; ui_max = 3u;
> = 2u;

// DEBUG OPTIONS

uniform bool DebugPreview < __UNIFORM_INPUT_BOOL1
	ui_text =
		"Visualize image scaling,\n"
		"get optimal value for super-resolution";
	ui_category = "Debugging tools";
	ui_category_closed = true;
	ui_label = "Debug mode";
	ui_tooltip =
		"Display color map of the resolution scale\n\n"
		" color | sampling type\n"
		" ---------------------\n"
		" Green | over\n"
		" Blue  | 1:1\n"
		" Red   | under";
> = false;

uniform uint ResScaleScreen < __UNIFORM_INPUT_INT1
	ui_category = "Debugging tools";
	ui_label = "screen (native) resolution";
	ui_tooltip = "Set it to default screen resolution";
> = 1920u;

uniform uint ResScaleVirtual < __UNIFORM_DRAG_INT1
	ui_category = "Debugging tools";
	ui_label = "virtual resolution";
	ui_tooltip =
		"Simulates application running beyond native\n"
		"screen resolution (using VSR or DSR)";
	#if __RESHADE__ < 40000
		ui_step = 0.2;
	#endif
	ui_min = 16u; ui_max = 16384u;
> = 1920u;

// Stereo 3D mode
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

	// Border style
	AddressU = MIRROR;
	AddressV = MIRROR;

	// Linear workflow
	#if BUFFER_COLOR_BIT_DEPTH != 10
		SRGBTexture = true;
	#endif
};


  /////////////////
 /// FUNCTIONS ///
/////////////////

// ITU REC 601 YCbCr coefficients
#define KR 0.299
#define KB 0.114
// RGB to YCbCr-luma matrix
static const float3 LumaMtx = float3(KR, 1f-KR-KB, KB); // Luma (Y)

// Convert gamma between linear and sRGB
#define TO_DISPLAY_GAMMA_HQ(g) ((g)<0.0031308? (g)*12.92 : pow(abs(g), rcp(2.4))*1.055-0.055)
#define TO_LINEAR_GAMMA_HQ(g) ((g)<0.04045? (g)/12.92 : pow((abs(g)+0.055)/1.055, 2.4))

// Get reciprocal screen aspect ratio (1/x)
#define BUFFER_RCP_ASPECT_RATIO (BUFFER_HEIGHT*BUFFER_RCP_WIDTH)

/**
G continuity distance function by Jakub Max Fober.
Determined empirically. (G from 0, to 3)
	G=0 -> Sharp corners
	G=1 -> Round corners
	G=2 -> Smooth corners
	G=3 -> Luxury corners
*/
float glength(uint G, float2 pos)
{
	// Sharp corner
	if (G==0u) return max(abs(pos.x), abs(pos.y)); // G0
	// Higher-power length function
	pos = pow(abs(pos), ++G); // Power of G+1
	return pow(pos.x+pos.y, rcp(G)); // Power G+1 root
}

/**
Linear pixel step function for anti-aliasing by Jakub Max Fober.
This algorithm is part of scientific paper:
	arXiv: 20104077 [cs.GR] (2020)
*/
float aastep(float grad)
{
	// Differential vector
	float2 Del = float2(ddx(grad), ddy(grad));
	// Gradient normalization to pixel size, centered at the step edge
	return saturate(rsqrt(dot(Del, Del))*grad+0.5); // half-pixel offset
}

/**
Universal perspective model by Jakub Max Fober,
Gnomonic to custom perspective variant.
This algorithm is part of a scientific paper:
	arXiv: 2003.10558 [cs.GR] (2020)
	arXiv: 2010.04077 [cs.GR] (2020)
Input data:
	FOV -> Camera Field of View in degrees.
	viewCoord -> screen coordinates [-1, 1] in R^2,
		where point [0 0] is at the center of the screen.
	k -> distortion parameter [-1, 1] in R
	s -> anamorphic squeeze power [1, 2] in R
Output data:
	vignette -> vignetting mask in linear space
	viewCoord -> texture lookup perspective coordinates
*/
float UniversalPerspective_vignette(inout float2 viewCoord, float k, float s) // Returns vignette
{
	// Get half field of view
	const float halfOmega = radians(FOV*0.5);

	// Get radius
	float R = (s==1f)?
		dot(viewCoord, viewCoord) : // Spherical
		(viewCoord.x*viewCoord.x)+(viewCoord.y*viewCoord.y)/s; // Anamorphic
	float rcpR = rsqrt(R); R = sqrt(R);

	// Get incident angle
	float theta;
	     if (k>0f) theta = atan(tan(k*halfOmega)*R)/k;
	else if (k<0f) theta = asin(sin(k*halfOmega)*R)/k;
	else /*k==0f*/ theta = halfOmega*R;

	// Generate vignette
	float vignetteMask;
	if (UseVignette && !DebugPreview)
	{
		// Limit FOV span, k+- in [0.5, 1] range
		float thetaLimit = max(abs(k), 0.5)*theta;
		// Create spherical vignette
		vignetteMask = cos(thetaLimit);
		vignetteMask = lerp(
			vignetteMask, // Cosine law of illumination
			vignetteMask*vignetteMask, // Inverse square law
			clamp(k+0.5, 0f, 1f) // For k in [-0.5, 0.5] range
		);
		// Anamorphic vignette
		if (s!=1f)
		{
			// Get anamorphic-incident 3D vector
			float3 perspVec = float3((sin(theta)*rcpR)*viewCoord, cos(theta));
			vignetteMask /= dot(perspVec, perspVec); // Inverse square law
		}
	}
	else // Bypass
		vignetteMask = 1f;

	// Radius for gnomonic projection wrapping
	const float rTanHalfOmega = rcp(tan(halfOmega));
	// Transform screen coordinates and normalize to FOV
	viewCoord *= tan(theta)*rcpR*rTanHalfOmega;

	// Return vignette
	return vignetteMask;
}

// Inverse transformation of universal perspective algorithm
float UniversalPerspective_inverse(float2 viewCoord, float k, float s) // Returns reciprocal radius
{
	// Get half field of view
	const float halfOmega = radians(FOV*0.5);

	// Get incident vector
	float3 incident;
	incident.xy = viewCoord;
	incident.z = rcp(tan(halfOmega));

	// Get theta angle
	float theta = (s==1f)?
		acos(normalize(incident).z) : // Spherical
		acos(incident.z*rsqrt((incident.y*incident.y)/s+dot(incident.xz, incident.xz))); // Anamorphic

	// Get radius
	float R;
	     if (k>0f) R = tan(k*theta)/tan(k*halfOmega);
	else if (k<0f) R = sin(k*theta)/sin(k*halfOmega);
	else /*k==0f*/ R = theta/halfOmega;

	// Calculate transformed position reciprocal radius
	if (s==1f) return R*rsqrt(dot(viewCoord, viewCoord));
	else return R*rsqrt((viewCoord.y*viewCoord.y)/s+(viewCoord.x*viewCoord.x));
}


  //////////////
 /// SHADER ///
//////////////

// Border mask shader with rounded corners
float GetBorderMask(float2 borderCoord)
{
	// Get coordinates for each corner
	borderCoord = abs(borderCoord);
	if (BorderGContinuity!=0u && BorderCorner!=0f) // If round corners
	{
		// Correct corner aspect ratio
		if (BUFFER_ASPECT_RATIO>1f) // If in landscape mode
			borderCoord.x = borderCoord.x*BUFFER_ASPECT_RATIO+(1f-BUFFER_ASPECT_RATIO);
		else if (BUFFER_ASPECT_RATIO<1f) // If in portrait mode
			borderCoord.y = borderCoord.y*BUFFER_RCP_ASPECT_RATIO+(1f-BUFFER_RCP_ASPECT_RATIO);
		// Generate scaled coordinates
		borderCoord = max(borderCoord+(BorderCorner-1f), 0f)/BorderCorner;

		// Round corner
		return aastep(glength(BorderGContinuity, borderCoord)-1f); // ...with G1 to G3 continuity
	}
	else // Just sharp corner, G0
		return aastep(glength(0u, borderCoord)-1f);
}

// Debug view mode shader
void DebugModeViewPass(inout float3 display, float2 sphCoord)
{
	// Define Mapping color
	const float3   underSmpl = TO_LINEAR_GAMMA_HQ(float3(1f, 0f, 0.2)); // Red
	const float3   superSmpl = TO_LINEAR_GAMMA_HQ(float3(0f, 1f, 0.5)); // Green
	const float3 neutralSmpl = TO_LINEAR_GAMMA_HQ(float3(0f, 0.5, 1f)); // Blue

	// Calculate Pixel Size difference
	float pixelScaleMap;
	// and simulate Dynamic Super Resolution (DSR) scalar
	pixelScaleMap  = ResScaleScreen*BUFFER_PIXEL_SIZE.x*BUFFER_PIXEL_SIZE.y;
	pixelScaleMap /= ResScaleVirtual*ddx(sphCoord.x)*ddy(sphCoord.y);
	pixelScaleMap -= 1f;

	// Generate super-sampled/under-sampled color map
	float3 resMap = lerp(
		superSmpl,
		underSmpl,
		step(0f, pixelScaleMap)
	);

	// Create black-white gradient mask of scale-neutral pixels
	pixelScaleMap = saturate(1f-4f*abs(pixelScaleMap)); // Clamp to more representative values

	// Color neutral scale pixels
	resMap = lerp(resMap, neutralSmpl, pixelScaleMap);

	// Blend color map with display image
	display = (0.8*dot(LumaMtx, display)+0.2)*resMap;
}

// Main perspective shader pass
float3 PerfectPerspectivePS(float4 pixelPos : SV_Position, float2 sphCoord : TEXCOORD0) : SV_Target
{
	// Bypass
	if (FOV==0u || ExpertMode && K==1f && !UseVignette)
		return tex2Dfetch(BackBuffer, uint2(pixelPos.xy)).rgb;

	#if SIDE_BY_SIDE_3D // Side-by-side 3D content
		float SBS3D = sphCoord.x*2f;
		sphCoord.x = frac(SBS3D);
		SBS3D = floor(SBS3D);
	#endif

	// Convert UV to centered coordinates
	sphCoord = sphCoord*2f-1f;
	// Correct aspect ratio
	sphCoord.y *= BUFFER_RCP_ASPECT_RATIO;

	// Get FOV type scalar
	float FovScalar; switch(FovType)
	{
		// Horizontal
		default: FovScalar = 1f; break;
		// Diagonal
		case 1: FovScalar = sqrt(BUFFER_RCP_ASPECT_RATIO*BUFFER_RCP_ASPECT_RATIO+1f); break;
		// Vertical
		case 2: FovScalar = BUFFER_RCP_ASPECT_RATIO; break;
		// Horizontal 4:3
		case 3: FovScalar = (4f/3f)*BUFFER_RCP_ASPECT_RATIO; break;
		// Horizontal 16:9
		case 4: FovScalar = (16f/9f)*BUFFER_RCP_ASPECT_RATIO; break;
	}

	// Adjust FOV type
	sphCoord /= FovScalar; // pass 1 of 2

	// Set perspective parameters
	float k, s; // Projection type and anamorphic squeeze factor
	if (!ExpertMode) // Perspective expert mode
	{
		// Choose projection type
		switch (Projection)
		{
			case 0u: k =  0.5; break; // Stereographic
			case 1u: k =  0f;  break; // Equidistant
			case 2u: k = -0.5; break; // Equisolid
		}

		// Choose anamorphic squeeze factor
		switch (AnamorphicSqueeze)
		{
			case 0u: s = 1f; break;
			case 1u: s = 1.25; break;
			// case 1u: s = 1.333; break;
			case 2u: s = 1.5; break;
			// case 2u: s = 1.6; break;
			case 3u: s = 2f; break;
		}
	}
	else // Manual perspective
	{
		k = clamp(K,-1f, 1f); // Projection type
		s = clamp(S, 1f, 4f); // Anamorphic squeeze factor
	}

	// Scale picture to cropping point
	{
		// Get cropping positions: vertical, horizontal, diagonal
		float2 normalizationPos[3u];
		normalizationPos[0u].x      // Vertical crop
			= normalizationPos[1u].y // Horizontal crop
			= 0f;
		normalizationPos[2u].x      // Diagonal crop
			= normalizationPos[1u].x // Horizontal crop
			= rcp(FovScalar);
		normalizationPos[2u].y      // Diagonal crop
			= normalizationPos[0u].y // Vertical crop
			= normalizationPos[2u].x*BUFFER_RCP_ASPECT_RATIO;

		// Get cropping option scalar
		float crop = CroppingFactor*2f;
		// Interpolate between cropping states
		sphCoord *= lerp(
			UniversalPerspective_inverse(normalizationPos[uint(floor(crop))], k, s),
			UniversalPerspective_inverse(normalizationPos[uint( ceil(crop))], k, s),
			frac(crop) // Weight interpolation
		);
	}

	// Perspective transform and create vignette
	float vignetteMask = UniversalPerspective_vignette(sphCoord, k, s);

	// Adjust FOV type
	sphCoord *= FovScalar; // pass 2 of 2

	// Aspect Ratio back to square
	sphCoord.y *= BUFFER_ASPECT_RATIO;

	// Outside border mask with anti-aliasing
	float borderMask = GetBorderMask(sphCoord);

	// Back to UV Coordinates
	sphCoord = sphCoord*0.5+0.5;

	#if SIDE_BY_SIDE_3D // Side-by-side 3D content
		sphCoord.x = (sphCoord.x+SBS3D)*0.5;
	#endif

	// Sample display image
	float3 display = (k==1f)?
		tex2Dfetch(BackBuffer, uint2(pixelPos.xy)).rgb : // No perspective change
		tex2D(BackBuffer, sphCoord).rgb; // Spherical perspective

	#if BUFFER_COLOR_BIT_DEPTH == 10 // Manually correct gamma
		display = TO_LINEAR_GAMMA_HQ(display);
	#endif

	if (k!=1f && CroppingFactor!=1f) // Visible borders
	{
		// Get border
		float3 border = lerp(
			// Border background
			MirrorBorder? display : tex2Dfetch(BackBuffer, uint2(pixelPos.xy)).rgb,
			// Border color
			TO_LINEAR_GAMMA_HQ(BorderColor.rgb),
			// Border alpha
			TO_LINEAR_GAMMA_HQ(BorderColor.a)
		);

		// Apply vignette with border
		display = BorderVignette?
			vignetteMask*lerp(display, border, borderMask) : // Vignette on border
			lerp(vignetteMask*display, border, borderMask);  // Vignette only inside
	}
	else
		display *= vignetteMask; // Apply vignette

	// Output type choice
	if (DebugPreview) DebugModeViewPass(display, sphCoord);

	#if BUFFER_COLOR_BIT_DEPTH == 10 // Manually correct gamma
		return TO_DISPLAY_GAMMA_HQ(display);
	#else
		return display;
	#endif
}


  //////////////
 /// OUTPUT ///
//////////////

technique PerfectPerspective <
	ui_label = "Perfect Perspective";
	ui_tooltip =
		"Adjust perspective for distortion-free picture:\n"
		" * fish-eye\n"
		" * panini\n"
		" * anamorphic\n"
		" * (natural) vignetting\n\n"
		"Instruction:\n\n"
		"Fist select proper FOV angle and type. If FOV type is unknown,\n"
		"find a round object within the game and look at it upfront,\n"
		"then rotate the camera so that the object is in the corner.\n"
		"Change squeeze factor to 1x and adjust FOV type such that\n"
		"the object does not have an egg shape, but a perfect round shape.\n\n"
		"Secondly adjust perspective type according to game-play style.\n"
		"If you look mostly at the horizon, anamorphic squeeze can be\n"
		"increased. For curved-display correction, set it higher.\n\n"
		"Thirdly, adjust visible borders. You can change the crop factor,\n"
		"such that no borders are visible, or that no image area is lost.\n\n"
		"Additionally for sharp image, use FilmicSharpen.fx or run game at a\n"
		"Super-Resolution. Debug options can help you find the proper value.\n\n"
		"The algorithm is part of a scientific paper:\n"
		"arXiv: 2003.10558 [cs.GR] (2020)\n"
		"arXiv: 2010.04077 [cs.GR] (2020)\n";
>
{
	pass
	{
		VertexShader = PostProcessVS;
		PixelShader = PerfectPerspectivePS;

		// Linear workflow
		#if BUFFER_COLOR_BIT_DEPTH != 10
			SRGBWriteEnable = true;
		#endif
	}
}
