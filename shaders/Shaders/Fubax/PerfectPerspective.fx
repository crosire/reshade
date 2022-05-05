/** Perfect Perspective PS, version 3.3.0
All rights (c) 2018 Jakub Maksymilian Fober (the Author).

The Author provides this shader (the Work)
under the Creative Commons CC BY-NC-ND 4.0 license available online at
http://creativecommons.org/licenses/by-nc-nd/4.0/.
The Author further grants permission for reuse of screen-shots and game-play
recordings derived from the Work, provided that the reuse is for the purpose of
promoting and/or summarizing the Work or is a part of an online forum post or
social media post and that any use is accompanied by a link to the Work and a
credit to the Author. (crediting Author by pseudonym "Fubax" is acceptable)

For inquiries please contact jakub.m.fober@pm.me
For updates visit GitHub repository at
https://github.com/Fubaxiusz/fubax-shaders/

This shader version is based upon research paper by
Fober, J. M. Perspective picture from Visual Sphere. a new approach to image
rasterization. second revision. arXiv: 2003.10558 [cs.GR] (2020)
available online at https://arxiv.org/abs/2003.10558
*/


  ////////////
 /// MENU ///
////////////

#include "ReShadeUI.fxh"

uniform int FOV < __UNIFORM_SLIDER_INT1
	ui_label = "Game field of view";
	ui_tooltip = "This setting should match your in-game FOV (in degrees)";
	#if __RESHADE__ < 40000
		ui_step = 0.2;
	#endif
	ui_min = 0; ui_max = 170;
	ui_category = "Field of View";
> = 90;

uniform int Type < __UNIFORM_COMBO_INT1
	ui_label = "Type of FOV";
	ui_tooltip = "...in stereographic mode\n"
		"If image bulges in movement (too high FOV),\n"
		"change it to 'Diagonal'.\n"
		"When proportions are distorted at the periphery\n"
		"(too low FOV), choose 'Vertical' or '4:3'.\n"
		"\nAdjust so that round objects are still round \n"
		"in the corners, and not oblong.\n"
		"\n*This method works only in 'navigation' preset,\n"
		"or k=0.5 on manual.";
	ui_items =
		"Horizontal FOV\0"
		"Diagonal FOV\0"
		"Vertical FOV\0"
		"4:3 FOV\0";
	ui_category = "Field of View";
> = 0;

uniform int Projection < __UNIFORM_RADIO_INT1
	ui_label = "Type of perspective";
	ui_text = "Select game style";
	ui_tooltip =
		"Choose type of perspective, according to game-play style.\n"
		"For manual perspective adjustment, select last option.";
	ui_items =
		"Navigation\0"
		"Aiming\0"
		"Feel of distance\0"
		"manual adjustment *\0";
	ui_category = "Perspective";
	ui_category_closed = true;
	ui_spacing = 1;
> = 0;

uniform float K < __UNIFORM_SLIDER_FLOAT1
	ui_label = "K (manual perspective) *";
	ui_tooltip =
		"K 1.0 ....Rectilinear projection (standard), preserves straight lines,"
		" but doesn't preserve proportions, angles or scale.\n"
		"K 0.5 ....Stereographic projection (navigation, shape) preserves angles and proportions,"
		" best for navigation through tight spaces.\n"
		"K 0 ......Equidistant (aiming) maintains angular speed of motion,"
		" best for aiming at fast targets.\n"
		"K -0.5 ...Equisolid projection (distance) preserves area relation,"
		" best for navigation in open space.\n"
		"K -1.0 ...Orthographic projection preserves planar luminance,"
		" has extreme radial compression. Found in peephole viewer.";
	ui_min = -1.0; ui_max = 1.0;
	ui_category = "Perspective";
> = 1.0;

uniform float Vertical < __UNIFORM_SLIDER_FLOAT1
	ui_label = "Vertical distortion";
	ui_tooltip = "Cylindrical perspective <<>> Spherical perspective";
	ui_min = 0.001; ui_max = 1.0;
	ui_category = "Perspective";
	ui_text = "Global settings";
> = 1.0;

uniform float VerticalScale < __UNIFORM_SLIDER_FLOAT1
	ui_label = "Vertical proportions";
	ui_tooltip = "Adjust anamorphic correction for cylindrical perspective";
	ui_min = 0.8; ui_max = 1.0;
	ui_category = "Perspective";
> = 0.95;

uniform float Zooming < __UNIFORM_DRAG_FLOAT1
	ui_label = "Border scale";
	ui_tooltip = "Adjust image scale and cropped area";
	ui_min = 0.5; ui_max = 2.0; ui_step = 0.001;
	ui_category = "Border";
	ui_category_closed = true;
> = 1.0;

uniform float BorderCorner < __UNIFORM_SLIDER_FLOAT1
	ui_label = "Corner roundness";
	ui_tooltip = "Represents corners curvature\n0.0 gives sharp corners";
	ui_min = 1.0; ui_max = 0.0;
	ui_category = "Border";
> = 0.062;

uniform float4 BorderColor < __UNIFORM_COLOR_FLOAT4
	ui_label = "Border color";
	ui_tooltip = "Use Alpha to change transparency";
	ui_category = "Border";
> = float4(0.027, 0.027, 0.027, 0.784);

uniform bool MirrorBorder < __UNIFORM_INPUT_BOOL1
	ui_label = "Mirrored border";
	ui_tooltip = "Choose original or mirrored image at the border";
	ui_category = "Border";
> = true;

uniform bool DebugPreview < __UNIFORM_INPUT_BOOL1
	ui_label = "Resolution scale map";
	ui_tooltip = "Color map of the Resolution Scale:\n"
		" Red   - under-sampling\n"
		" Green - super-sampling\n"
		" Blue  - neutral sampling";
	ui_category = "Tools for debugging";
	ui_category_closed = true;
	ui_spacing = 2;
> = false;

uniform int2 ResScale < __UNIFORM_INPUT_INT2
	ui_label = "Difference";
	ui_text = "screen resolution  |  virtual, super resolution";
	ui_tooltip = "Simulates application running beyond\n"
		"native screen resolution (using VSR or DSR)";
	ui_type = "drag";
	ui_min = 16; ui_max = 16384; ui_step = 0.2;
	ui_category = "Tools for debugging";
> = int2(1920, 1920);


#include "ReShade.fxh"

// Define screen texture with mirror tiles
sampler SamplerColor
{
	Texture = ReShade::BackBufferTex;
	AddressU = MIRROR;
	AddressV = MIRROR;
};


  /////////////////
 /// FUNCTIONS ///
/////////////////

// Convert RGB to gray-scale
float grayscale(float3 Color)
{ return max(max(Color.r, Color.g), Color.b); }
// Square function
float sq(float input)
{ return input*input; }
// Linear pixel step function for anti-aliasing
float linearstep(float x)
{ return clamp(x/fwidth(x), 0.0, 1.0); }

/*Universal perspective model by Jakub Max Fober,
Gnomonic to custom perspective variant.
This algorithm is a part of scientific paper DOI:
Input data:
	FOV -> Camera Field of View in degrees.
	coord -> screen coordinates (from -1, to 1),
		where p(0,0) is at the center of the screen.
	k -> distortion parameter (from -1, to 1).
	l -> vertical distortion parameter (from 0, to 1).
*/
float univPerspective(float k, float l, float2 coord)
{
	// Bypass
	if (k >= 1.0 || FOV == 0)
		return 1.0;

	float R = (l == 1.0) ?
		length(coord) : // Spherical
		sqrt(sq(coord.x) + sq(coord.y)*l); // Cylindrical

	const float omega = radians(FOV*0.5);

	float theta;
	if (k>0.0)
		theta = atan(R*tan(k*omega))/k;
	else if (k<0.0)
		theta = asin(R*sin(k*omega))/k;
	else // k==0.0
		theta = R*omega;

	const float tanOmega = tan(omega);
	return tan(theta)/(tanOmega*R);
}


  //////////////
 /// SHADER ///
//////////////

// Shader pass
float3 PerfectPerspectivePS(float4 pos : SV_Position, float2 texCoord : TEXCOORD) : SV_Target
{
	// Get inverted screen aspect ratio
	const float aspectRatioInv = 1.0 / BUFFER_ASPECT_RATIO;

	// Convert FOV type..
	float FovType; switch(Type)
	{
		case 0: FovType = 1.0; break; // Horizontal
		case 1: FovType = sqrt(sq(aspectRatioInv) + 1.0); break; // Diagonal
		case 2: FovType = aspectRatioInv; break; // Vertical
		case 3: FovType = 4.0/3.0*aspectRatioInv; break; // Horizontal 4:3
	}

	// Convert UV to centered coordinates
	float2 sphCoord = texCoord*2.0 -1.0;
	// Aspect Ratio correction
	sphCoord.y *= aspectRatioInv;
	// Zoom in image and adjust FOV type (pass 1 of 2)
	// sphCoord *= Zooming / FovType;
	sphCoord *= clamp(Zooming, 0.5, 2.0) / FovType; // Anti-cheat

	// Choose projection type
	float k; switch (Projection)
	{
		case 0: k = 0.5; break;		// Stereographic
		case 1: k = 0.0; break;		// Equidistant
		case 2: k = -0.5; break;	// Equisolid
		default: k = clamp(K, -1.0, 1.0); break; // Manual perspective
	}
	// Perspective transform, vertical distortion and FOV type (pass 2 of 2)
	sphCoord *= univPerspective(k, Vertical, sphCoord) * FovType;

	// Aspect Ratio back to square
	sphCoord.y *= BUFFER_ASPECT_RATIO;

	// Anamorphic correction
	if(VerticalScale != 1.0 && Vertical != 1.0)
		sphCoord.y /= lerp(VerticalScale, 1.0, Vertical);

	// Outside border mask with Anti-Aliasing
	float borderMask;
	if (BorderCorner == 0.0) // If sharp corners
		borderMask = linearstep( max(abs(sphCoord.x), abs(sphCoord.y)) -1.0 );
	else // If round corners
	{
		// Get coordinates for each corner
		float2 borderCoord = abs(sphCoord);
		// Correct corner aspect ratio
		if (ReShade::AspectRatio > 1.0) // If landscape mode
			borderCoord.x = borderCoord.x*ReShade::AspectRatio + 1.0-ReShade::AspectRatio;
		else if (ReShade::AspectRatio < 1.0) // If portrait mode
			borderCoord.y = borderCoord.y*aspectRatioInv + 1.0-aspectRatioInv;

		// Generate mask
		borderMask = length(max(borderCoord +BorderCorner -1.0, 0.0)) /BorderCorner;
		borderMask = linearstep( borderMask-1.0 );
	}

	// Back to UV Coordinates
	sphCoord = sphCoord*0.5 +0.5;

	// Sample display image
	float3 display = tex2D(SamplerColor, sphCoord).rgb;

	// Mask outside-border pixels or mirror
	display = lerp(
		display,
		lerp(
			MirrorBorder ? display : tex2D(SamplerColor, texCoord).rgb,
			BorderColor.rgb,
			BorderColor.a
		),
		borderMask
	);

	// Output type choice
	if(DebugPreview)
	{
		// Calculate radial screen coordinates before and after perspective transformation
		float4 radialCoord = float4(texCoord, sphCoord) * 2.0 - 1.0;
		// Correct vertical aspect ratio
		radialCoord.yw *= aspectRatioInv;

		// Define Mapping color
		const float3 underSmpl = float3(1.0, 0.0, 0.2); // Red
		const float3 superSmpl = float3(0.0, 1.0, 0.5); // Green
		const float3 neutralSmpl = float3(0.0, 0.5, 1.0); // Blue

		// Calculate Pixel Size difference...
		float pixelScaleMap = fwidth( length(radialCoord.xy) );
		// ...and simulate Dynamic Super Resolution (DSR) scalar
		pixelScaleMap *= ResScale.x / (fwidth( length(radialCoord.zw) ) * ResScale.y);
		pixelScaleMap -= 1.0;

		// Generate super-sampled/under-sampled color map
		float3 resMap = lerp(
			superSmpl,
			underSmpl,
			step(0.0, pixelScaleMap)
		);

		// Create black-white gradient mask of scale-neutral pixels
		pixelScaleMap = 1.0 - abs(pixelScaleMap);
		pixelScaleMap = saturate(pixelScaleMap * 4.0 - 3.0); // Clamp to more representative values

		// Color neutral scale pixels
		resMap = lerp(resMap, neutralSmpl, pixelScaleMap);

		// Blend color map with display image
		display = normalize(resMap) * (0.8 * grayscale(display) + 0.2);
	}

	return display;
}


  //////////////
 /// OUTPUT ///
//////////////

technique PerfectPerspective
<
	ui_label = "Perfect Perspective";
	ui_tooltip = "Adjust perspective for distortion-free picture\n"
		"(fish-eye, panini shader)";
>
{
	pass
	{
		VertexShader = PostProcessVS;
		PixelShader = PerfectPerspectivePS;
	}
}
