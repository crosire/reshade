////////////////////////////////////////////////////////////////////////////////////////////////////////
// ColorSort.fx by SirCobra
// Version 0.5
// You can find info and my repository here: https://github.com/LordKobra/CobraFX
// currently resource-intensive
// This compute shader only runs on the ReShade 4.8 Beta and DX11+ or newer OpenGL games.
// This effect does sort all colors on a vertical line by brightness.
// The effect can be applied to a specific area like a DoF shader. The basic methods for this were taken with permission
// from https://github.com/FransBouma/OtisFX/blob/master/Shaders/Emphasize.fx
// Thanks to kingeric1992 & Lord of Lunacy for tips on how to construct the algorithm. :)
// The merge_sort function is adapted from this website: https://www.techiedelight.com/iterative-merge-sort-algorithm-bottom-up/
// The multithreaded merge sort is constructed as described here: https://www.nvidia.in/docs/IO/67073/nvr-2008-001.pdf
//
// If the quality of the shader feels to low, you can adjust COLOR_HEIGHT in the preprocessor options. 
// Only choose these numbers: 640 is Default, 768 is HQ, 1024 is Ultra.
////////////////////////////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////////////////////////////
//***************************************                  *******************************************//
//***************************************   UI & Defines   *******************************************//
//***************************************                  *******************************************//
////////////////////////////////////////////////////////////////////////////////////////////////////////

// We need Compute Shader Support
#if (((__RENDERER__ >= 0xb000 && __RENDERER__ < 0x10000) || (__RENDERER__ >= 0x14300)) && __RESHADE__ >=40800)
	#define COLORSORT_COMPUTE 1
#else
	#define COLORSORT_COMPUTE 0
#endif

// Shader Start
#if COLORSORT_COMPUTE != 0
#include "Reshade.fxh"

// Namespace everything
namespace ColorSorter
{

//defines
#define SORTING   "Sorting Options\n"
#define MASKING_C "Color Masking\n"
#define MASKING_D "Depth Masking\n"

#ifndef M_PI
	#define M_PI 3.1415927
#endif
#ifndef COLOR_HEIGHT
	#define COLOR_HEIGHT	640 //maybe needs multiple of 64 :/
#endif
#ifndef THREAD_HEIGHT
	#define THREAD_HEIGHT	((uint)16) // 2^n
#endif

#define COLOR_NOISE_WIDTH 4096
#define COLOR_NOISE_HEIGHT 1024

	//ui
	uniform int Buffer1 <
		ui_type = "radio"; ui_label = " ";
	>;
	uniform uint RotationAngle <
		ui_type = "slider";
		ui_min = 0; ui_max = 360;
		ui_step = 1;
		ui_tooltip = "Rotation in Degrees.";
		ui_category = SORTING;
	> = 0;
	uniform float BrightnessThresholdStart <
		ui_type = "slider";
		ui_min = -0.050; ui_max = 1.050;
		ui_step = 0.001;
		ui_tooltip = "Pixels with brightness close to this parameter serve as starting threshold for the sorting algorithm and fragment the area. Set both to their max value to disable them.";
		ui_category = SORTING;
	> = 1.050;
	uniform float BrightnessThresholdEnd <
		ui_type = "slider";
		ui_min = -0.050; ui_max = 1.050;
		ui_step = 0.001;
		ui_tooltip = "Pixels with brightness close to this parameter serve as finishing threshold for the sorting algorithm and fragment the area. Set both to their max value to disable them.";
		ui_category = SORTING;
	> = 1.050;
	uniform float GradientStrength <
		ui_type = "slider";
		ui_min = 0.000; ui_max = 1.000;
		ui_step = 0.001;
		ui_tooltip = "Strength of the noise applied to the masked area. More noise results in more brightness variance. Only recommended in monotone environments. For color gradients on the sorted area, better apply other effects between Masking and Main effect order.";
		ui_category = SORTING;
	> = 0.0;
	uniform float MaskingNoise <
		ui_type = "slider";
		ui_min = 0.000; ui_max = 1.001;
		ui_step = 0.001;
		ui_tooltip = "Strength of the noise applied to mask itself.";
		ui_category = SORTING;
	> = 0.0;
	uniform float NoiseSize <
		ui_type = "slider";
		ui_min = 0.001; ui_max = 1;
		ui_step = 0.001;
		ui_tooltip = "Size of the noise texture. A lower value means larger noise pixels.";
		ui_category = SORTING;
	> = 1.0;
	uniform bool ReverseSort <
		ui_tooltip = "While active, it orders from dark to bright, top to bottom. Else it will sort from bright to dark.";
		ui_category = SORTING;
	> = false;
	uniform bool InvertMask <
		ui_tooltip = "Invert the mask.";
		ui_category = SORTING;
	> = false;
	uniform bool ShowMask <
		ui_tooltip = "Show the masked pixels.";
		ui_category = SORTING;
	> = false;
	uniform bool HotsamplingMode <
		ui_tooltip = "The noise will be the same at all resolutions. Activate this, then adjust your options and it will stay the same at all resolutions. Turn this off when you do not intend to hotsample.";
		ui_category = SORTING;
	> = false;
	uniform int Buffer2 <
		ui_type = "radio"; ui_label = " ";
	>;
	uniform bool FilterColor <
		ui_tooltip = "Activates the color filter option.";
		ui_category = MASKING_C;
	> = false;
	uniform bool ShowSelectedHue <
		ui_tooltip = "Display the current selected hue range on the top of the image.";
		ui_category = MASKING_C;
	> = false;
	uniform float Value <
		ui_type = "slider";
		ui_min = 0.000; ui_max = 1.000;
		ui_step = 0.001;
		ui_tooltip = "The Value describes the brightness of the hue. 0 is black/no hue and 1 is maximum hue(e.g. pure red).";
		ui_category = MASKING_C;
	> = 1.0;
	uniform float ValueRange <
		ui_type = "slider";
		ui_min = 0.000; ui_max = 1.001;
		ui_step = 0.001;
		ui_tooltip = "The tolerance around the value.";
		ui_category = MASKING_C;
	> = 1.001;
	uniform float Hue <
		ui_type = "slider";
		ui_min = 0.000; ui_max = 1.000;
		ui_step = 0.001;
		ui_tooltip = "The hue describes the color category. It can be red, green, blue or a mix of them.";
		ui_category = MASKING_C;
	> = 1.0;
	uniform float HueRange <
		ui_type = "slider";
		ui_min = 0.000; ui_max = 0.501;
		ui_step = 0.001;
		ui_tooltip = "The tolerance around the hue.";
		ui_category = MASKING_C;
	> = 0.501;
	uniform float Saturation <
		ui_type = "slider";
		ui_min = 0.000; ui_max = 1.000;
		ui_step = 0.001;
		ui_tooltip = "The saturation determins the colorfulness. 0 is greyscale and 1 pure colors.";
		ui_category = MASKING_C;
	> = 1.0;
	uniform float SaturationRange <
		ui_type = "slider";
		ui_min = 0.000; ui_max = 1.000;
		ui_step = 0.001;
		ui_tooltip = "The tolerance around the saturation.";
		ui_category = MASKING_C;
	> = 1.0;
	uniform int Buffer3 <
		ui_type = "radio"; ui_label = " ";
	>;
	uniform bool FilterDepth <
		ui_tooltip = "Activates the depth filter option.";
		ui_category = MASKING_D;
	> = false;
	uniform float FocusDepth <
		ui_type = "slider";
		ui_min = 0.000; ui_max = 1.000;
		ui_step = 0.001;
		ui_tooltip = "Manual focus depth of the point which has the focus. Range from 0.0, which means camera is the focus plane, till 1.0 which means the horizon is focus plane.";
		ui_category = MASKING_D;
	> = 0.026;
	uniform float FocusRangeDepth <
		ui_type = "slider";
		ui_min = 0.0; ui_max = 1.000;
		ui_step = 0.001;
		ui_tooltip = "The depth of the range around the manual focus depth which should be emphasized. Outside this range, de-emphasizing takes place";
		ui_category = MASKING_D;
	> = 0.010;
	uniform bool Spherical <
		ui_tooltip = "Enables Emphasize in a sphere around the focus-point instead of a 2D plane";
		ui_category = MASKING_D;
	> = false;
	uniform int Sphere_FieldOfView <
		ui_type = "slider";
		ui_min = 1; ui_max = 180;
		ui_tooltip = "Specifies the estimated Field of View you are currently playing with. Range from 1, which means 1 Degree, till 180 which means 180 Degree (half the scene).\nNormal games tend to use values between 60 and 90.";
		ui_category = MASKING_D;
	> = 75;
	uniform float Sphere_FocusHorizontal <
		ui_type = "slider";
		ui_min = 0; ui_max = 1;
		ui_tooltip = "Specifies the location of the focuspoint on the horizontal axis. Range from 0, which means left screen border, till 1 which means right screen border.";
		ui_category = MASKING_D;
	> = 0.5;
	uniform float Sphere_FocusVertical <
		ui_type = "slider";
		ui_min = 0; ui_max = 1;
		ui_tooltip = "Specifies the location of the focuspoint on the vertical axis. Range from 0, which means upper screen border, till 1 which means bottom screen border.";
		ui_category = MASKING_D;
	> = 0.5;
	uniform int Buffer4 <
		ui_type = "radio"; ui_label = " ";
	>;

////////////////////////////////////////////////////////////////////////////////////////////////////////
//*************************************                       ****************************************//
//*************************************  Textures & Samplers  ****************************************//
//*************************************                       ****************************************//
////////////////////////////////////////////////////////////////////////////////////////////////////////

	texture texHalfRes { Width = BUFFER_WIDTH; Height = COLOR_HEIGHT; Format = RGBA16F; };
	texture texNoise < source = "colorsort_noise.png"; > { Width = COLOR_NOISE_WIDTH; Height = COLOR_NOISE_HEIGHT; Format = R8; };
	texture texMask {Width = BUFFER_WIDTH; Height = COLOR_HEIGHT; Format = R16F; };
	texture texBackground { Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = RGBA16F; };
	texture texColorSort { Width = BUFFER_WIDTH; Height = COLOR_HEIGHT; Format = RGBA16F; };
	storage texColorSortStorage { Texture = texColorSort; };

	sampler2D SamplerHalfRes { Texture = texHalfRes; };
	sampler2D SamplerNoise { Texture = texNoise; MagFilter = POINT; MinFilter = POINT; MipFilter = POINT; };
	sampler2D SamplerMask { Texture = texMask; MagFilter = POINT; MinFilter = POINT; MipFilter = POINT; };
	sampler2D SamplerBackground { Texture = texBackground; };
	sampler2D SamplerColorSort { Texture = texColorSort; };

////////////////////////////////////////////////////////////////////////////////////////////////////////
//***************************************                  *******************************************//
//*************************************** Helper Functions *******************************************//
//***************************************                  *******************************************//
////////////////////////////////////////////////////////////////////////////////////////////////////////

	//vector mod and normal fmod
	float3 mod(float3 x, float y) 
	{
		return x - y * floor(x / y);
	}
	float fmod(float x, float y) 
	{
		return x - y * floor(x / y);
	}

	//HSV functions from iq (https://www.shadertoy.com/view/MsS3Wc)
	float4 hsv2rgb(float4 c)
	{
		float3 rgb = clamp(abs(mod(float3(c.x * 6.0, c.x * 6.0 + 4.0, c.x * 6.0 + 2.0), 6.0) - 3.0) - 1.0, 0.0, 1.0);
		rgb = rgb * rgb * (3.0 - 2.0 * rgb); // cubic smoothing
		return float4(c.z * lerp(float3(1.0, 1.0, 1.0), rgb, c.y), 1.0);
	}

	//From Sam Hocevar: http://lolengine.net/blog/2013/07/27/rgb-to-hsv-in-glsl
	float4 rgb2hsv(float4 c)
	{
		const float4 K = float4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);
		float4 p = lerp(float4(c.bg, K.wz), float4(c.gb, K.xy), step(c.b, c.g));
		float4 q = lerp(float4(p.xyw, c.r), float4(c.r, p.yzx), step(p.x, c.r));

		float d = q.x - min(q.w, q.y);
		const float e = 1.0e-10;
		return float4(abs(q.z + (q.w - q.y) / (6.0 * d + e)), d / (q.x + e), q.x, 1.0);
	}

	// show the color bar. inspired by originalcodrs design
	float4 showHue(float2 texcoord, float4 fragment)
	{
		float range = 0.145;
		float depth = 0.06;
		if (abs(texcoord.x - 0.5) < range && texcoord.y < depth)
		{
			float4 hsvval = float4(saturate(texcoord.x - 0.5 + range) / (2 * range), 1, 1, 1);
			float4 rgbval = hsv2rgb(hsvval);
			bool active = min(abs(hsvval.r - Hue), (1 - abs(hsvval.r - Hue))) < HueRange;
			fragment = active ? rgbval : float4(0.5, 0.5, 0.5, 1);
		}
		return fragment;
	}

	//rotate the screen
	float2 rotate(float2 texcoord, uint angle, bool revert)
	{
		float2 rotated = texcoord;
		//easy cases to avoid dividing by zero
		rotated = (angle == 90) ? float2(texcoord.y, texcoord.x) : rotated;
		rotated = (angle == 180) ? float2(1 - texcoord.x, 1 - texcoord.y) : rotated;
		rotated = (angle == 270) ? float2(1 - texcoord.y, 1 - texcoord.x) : rotated;
		rotated = (angle == 360 || angle == 0) ? rotated : rotated;
		//harder cases
		if (!(angle == 0 || angle == 90 || angle == 180 || angle == 270 || angle == 360))
		{
			//neccessary transformations from picture coordinates to normal coordinate system for better visualization of the concept
			angle = fmod(angle + 180, 360); // we only need to rotate the angle, because although texcoord is inverted applying it twice fixes it.
			float phi = angle * M_PI / 180;
			//rotate the borders
			float2 p00 = float2(0, 0); //0
			float2 p01 = float2(0, 1); //1
			float2 p10 = float2(1, 0); //2
			float2 p11 = float2(1, 1); //3
			p00 = float2(cos(phi) * p00.x - sin(phi) * p00.y, sin(phi) * p00.x + cos(phi) * p00.y);
			p01 = float2(cos(phi) * p01.x - sin(phi) * p01.y, sin(phi) * p01.x + cos(phi) * p01.y);
			p10 = float2(cos(phi) * p10.x - sin(phi) * p10.y, sin(phi) * p10.x + cos(phi) * p10.y);
			p11 = float2(cos(phi) * p11.x - sin(phi) * p11.y, sin(phi) * p11.x + cos(phi) * p11.y);
			//find min and max values
			uint l, r;
			float lval, rval;
			lval = p00.x;
			rval = p00.y;
			l = 0;
			r = 0;
			l = (p01.x < lval) ? 1 : l;
			lval = (p01.x < lval) ? p01.x : lval;
			r = (p01.x > rval) ? 1 : r;
			rval = (p01.x > rval) ? p01.x : rval;
			l = (p10.x < lval) ? 2 : l;
			lval = (p10.x < lval) ? p10.x : lval;
			r = (p10.x > rval) ? 2 : r;
			rval = (p10.x > rval) ? p10.x : rval;
			l = (p11.x < lval) ? 3 : l;
			lval = (p11.x < lval) ? p11.x : lval;
			r = (p11.x > rval) ? 3 : r;
			rval = (p11.x > rval) ? p11.x : rval;
			//REVERT ?
			float current_x = revert ? float2(cos(phi) * texcoord.x - sin(phi) * texcoord.y, sin(phi) * texcoord.x + cos(phi) * texcoord.y).x : lval + texcoord.x * (rval - lval); //  just rotate x or interpolate between rotation start and end and find correct x: lval - x*lval + x*rval = lval + x(rval-lval)
			float current_x_rel = abs(lval - current_x) / abs(lval - rval);
			float current_y = float2(cos(phi) * texcoord.x - sin(phi) * texcoord.y, sin(phi) * texcoord.x + cos(phi) * texcoord.y).y;
			//there exist 4 borders, find the intersections
			// 0-1 0-2 1-3 2-3
			float3 ylow = 1000;
			float3 yhigh = -1000;
			//0-1
			float x_rel = abs(p00.x - current_x) / abs(p00.x - p01.x);
			float y_abs = (1 - x_rel) * p00.y + x_rel * p01.y;
			ylow = ((p00.x < current_x && current_x < p01.x) || (p00.x > current_x && current_x > p01.x)) ? float3(0, x_rel, y_abs) : ylow;
			yhigh = ((p00.x < current_x && current_x < p01.x) || (p00.x > current_x && current_x > p01.x)) ? ylow : yhigh;
			//0-2
			x_rel = abs(p00.x - current_x) / abs(p00.x - p10.x);
			y_abs = (1 - x_rel) * p00.y + x_rel * p10.y;
			ylow = ((p00.x < current_x && current_x < p10.x) || (p00.x > current_x && current_x > p10.x)) && (y_abs < ylow.z) ? float3(x_rel, 0, y_abs) : ylow;
			yhigh = ((p00.x < current_x && current_x < p10.x) || (p00.x > current_x && current_x > p10.x)) && (y_abs > yhigh.z) ? float3(x_rel, 0, y_abs) : yhigh;
			//1-3
			x_rel = abs(p01.x - current_x) / abs(p01.x - p11.x);
			y_abs = (1 - x_rel) * p01.y + x_rel * p11.y;
			ylow = ((p01.x < current_x && current_x < p11.x) || (p01.x > current_x && current_x > p11.x)) && (y_abs < ylow.z) ? float3(x_rel, 1, y_abs) : ylow;
			yhigh = ((p01.x < current_x && current_x < p11.x) || (p01.x > current_x && current_x > p11.x)) && (y_abs > yhigh.z) ? float3(x_rel, 1, y_abs) : yhigh;
			//2-3
			x_rel = abs(p10.x - current_x) / abs(p10.x - p11.x);
			y_abs = (1 - x_rel) * p10.y + x_rel * p11.y;
			ylow = ((p10.x < current_x && current_x < p11.x) || (p10.x > current_x && current_x > p11.x)) && (y_abs < ylow.z) ? float3(1, x_rel, y_abs) : ylow;
			yhigh = ((p10.x < current_x && current_x < p11.x) || (p10.x > current_x && current_x > p11.x)) && (y_abs > yhigh.z) ? float3(1, x_rel, y_abs) : yhigh;

			//interpolate and check revert
			rotated = revert ? float2(current_x_rel, abs(yhigh.z - current_y) / abs(ylow.z - yhigh.z)) : (1 - texcoord.y) * yhigh.xy + texcoord.y * ylow.xy; //find the y position on the original grid : find the y position on the rotated grid
		}
		return rotated;
	}

////////////////////////////////////////////////////////////////////////////////////////////////////////
//***************************************                  *******************************************//
//***************************************     Masking      *******************************************//
//***************************************                  *******************************************//
////////////////////////////////////////////////////////////////////////////////////////////////////////



	bool inFocus(float4 rgbval, float scenedepth, float2 texcoord)
	{
		//colorfilter
		float4 hsvval = rgb2hsv(rgbval);
		bool d1 = abs(hsvval.b - Value) < ValueRange;
		bool d2 = abs(hsvval.r - Hue) < (HueRange + pow(2.71828, -(hsvval.g * hsvval.g) / 0.005)) || (1 - abs(hsvval.r - Hue)) < (HueRange + pow(2.71828, -(hsvval.g * hsvval.g) / 0.01));
		bool d3 = abs(hsvval.g - Saturation) <= SaturationRange;
		bool is_color_focus = (d3 && d2 && d1) || FilterColor == 0; // color threshold
		//depthfilter
		float depthdiff;
		texcoord.x = (texcoord.x - Sphere_FocusHorizontal) * ReShade::ScreenSize.x;
		texcoord.y = (texcoord.y - Sphere_FocusVertical) * ReShade::ScreenSize.y;
		const float degreePerPixel = Sphere_FieldOfView / ReShade::ScreenSize.x;
		const float fovDifference = sqrt((texcoord.x * texcoord.x) + (texcoord.y * texcoord.y)) * degreePerPixel;
		depthdiff = Spherical ? sqrt((scenedepth * scenedepth) + (FocusDepth * FocusDepth) - (2 * scenedepth * FocusDepth * cos(fovDifference * (2 * M_PI / 360)))) : depthdiff = abs(scenedepth - FocusDepth);

		bool is_depth_focus = (depthdiff < FocusRangeDepth) || FilterDepth == 0;
		bool in_focus = is_color_focus && is_depth_focus;
		return (1 - InvertMask) * in_focus + InvertMask * (1 - in_focus);
	}

	void mask_color(float4 vpos : SV_Position, float2 texcoord : TEXCOORD, out float fragment : SV_Target)
	{
		//focus
		float4 color = tex2D(ReShade::BackBuffer, texcoord);
		float scenedepth = ReShade::GetLinearizedDepth(texcoord);
		float in_focus = inFocus(color, scenedepth, texcoord);
		//seperator
		uint hs_width = HotsamplingMode ? 2036 : BUFFER_WIDTH;
		float2 t_noise = float2(texcoord.x, texcoord.y) * NoiseSize;
		float angle = RotationAngle;
		float phi = angle * M_PI / 180;
		t_noise = float2(cos(phi) * t_noise.x - sin(phi) * t_noise.y, sin(phi) * t_noise.x + cos(phi) * t_noise.y);
		t_noise = float2(fmod(t_noise.x * hs_width, COLOR_NOISE_WIDTH) / (float) COLOR_NOISE_WIDTH, fmod(t_noise.y * COLOR_HEIGHT, COLOR_NOISE_HEIGHT) / (float)COLOR_NOISE_HEIGHT);
		float noise_1 = tex2D(SamplerNoise, t_noise).r; // add some point-color.
		//bool is_noisy = MaskingNoise > noise_1;
		bool seperator_1 = abs((color.r + color.g + color.b) / 3 - BrightnessThresholdStart) < 0.04;
		bool seperator_2 = abs((color.r + color.g + color.b) / 3 - BrightnessThresholdEnd) < 0.04;
		//bool seperator = seperator_1 || seperator_2;
		noise_1 = 0.5 * noise_1;
		noise_1 = seperator_1 ? 0.8 : noise_1;
		noise_1 = seperator_2 ? 0.7 : noise_1;
		fragment = saturate(!in_focus + noise_1); // 1 -not in focus 0-0.5 infocus+noiselevel 0.8:seperator_1 0.7 sep2
	}
	void save_background(float4 vpos : SV_Position, float2 texcoord : TEXCOORD, out float4 fragment : SV_Target)
	{
		fragment = tex2D(ReShade::BackBuffer, texcoord);
	}

////////////////////////////////////////////////////////////////////////////////////////////////////////
//***************************************                  *******************************************//
//***************************************     Gradient     *******************************************//
//***************************************                  *******************************************//
////////////////////////////////////////////////////////////////////////////////////////////////////////

	void grad_color(float4 vpos : SV_Position, float2 texcoord : TEXCOORD, out float4 fragment : SV_Target) 
	{
		fragment = tex2D(ReShade::BackBuffer, texcoord);
		//Gradient Noise
		float2 t_noise = float2(frac(texcoord.x * BUFFER_WIDTH / COLOR_NOISE_WIDTH), frac(texcoord.y * COLOR_HEIGHT / COLOR_NOISE_HEIGHT));
		float noise_1 = tex2D(SamplerNoise, t_noise).r;
		noise_1 = (sin(4 * M_PI * noise_1) + 4 * M_PI * noise_1) / (4 * M_PI);
		noise_1 = GradientStrength * (noise_1 - 0.5);
		fragment = saturate(fragment + float4(noise_1, noise_1, noise_1, 0));
	}
	void pre_color(float4 vpos : SV_Position, float2 texcoord : TEXCOORD, out float4 fragment : SV_Target) 
	{
		//prepare and rotate texture for sorting
		float2 texcoord_new = rotate(texcoord, RotationAngle, false);
		fragment = tex2D(ReShade::BackBuffer, texcoord_new);
		float mask = tex2D(SamplerMask, texcoord_new).r;
		fragment.a = mask;
	}

////////////////////////////////////////////////////////////////////////////////////////////////////////
//***************************************                  *******************************************//
//***************************************     Sorting      *******************************************//
//***************************************                  *******************************************//
////////////////////////////////////////////////////////////////////////////////////////////////////////

	//groupshared memory
	groupshared float4 colortable[2 * COLOR_HEIGHT];
	groupshared int evenblock[2 * THREAD_HEIGHT];
	groupshared int oddblock[2 * THREAD_HEIGHT];

	// core sorting decider
	bool min_color(float4 a, float4 b)
	{
		float val = b.a - a.a; // val > 0 for a smaller
		val = (abs(val) < 0.1) ? ((a.r + a.g + a.b) - (b.r + b.g + b.b)) * (1 - ReverseSort - ReverseSort) : val;
		return (val < 0) ? false : true; // Returns False if a smaller, yes its weird
	}

	//single thread merge sort
	void merge_sort(int low, int high, int em)
	{
		float4 temp[COLOR_HEIGHT / THREAD_HEIGHT];
		for (int i = 0; i < COLOR_HEIGHT / THREAD_HEIGHT; i++)
		{
			temp[i] = colortable[low + i];
		}
		for (int m = em; m <= high - low; m = 2 * m)
		{
			for (int i = low; i < high; i += 2 * m)
			{
				int from = i;
				int mid = i + m - 1;
				int to = min(i + 2 * m - 1, high);
				//inside function //////////////////////////////////
				int k = from, i_2 = from, j = mid + 1;
				while (i_2 <= mid && j <= to)
				{
					if (min_color(colortable[i_2], colortable[j]))
					{
						temp[k++ - low] = colortable[i_2++];
					}
					else
					{
						temp[k++ - low] = colortable[j++];
					}
				}
				while (i_2 < high && i_2 <= mid)
				{
					temp[k++ - low] = colortable[i_2++];
				}
				for (i_2 = from; i_2 <= to; i_2++)
				{
					colortable[i_2] = temp[i_2 - low];
				}
			}
		}
	}

	//multithread merge sort
	void sort_color(uint3 id : SV_DispatchThreadID, uint3 tid : SV_GroupThreadID)
	{
		int row = tid.y * COLOR_HEIGHT / THREAD_HEIGHT;
		int interval_start = row + tid.x * COLOR_HEIGHT;
		int interval_end = row - 1 + COLOR_HEIGHT / THREAD_HEIGHT + tid.x * COLOR_HEIGHT;
		uint i;
		//masking
		for (i = row; i <= row - 1 + COLOR_HEIGHT / THREAD_HEIGHT; i++)
		{
				colortable[i + tid.x * COLOR_HEIGHT] = tex2Dfetch(SamplerHalfRes, int2(id.x, i));
		}
		if (tid.y == 0)
		{
			bool was_focus = false; //last array element
			bool is_focus = false;	// current array element
			float noise = 0;
			bool is_seperate = false;
			bool was_seperate = false;
			bool active_seperator = BrightnessThresholdStart < 1.02 || BrightnessThresholdEnd < 1.02;
			bool seperate_area = !active_seperator;
			int maskval = 0;
			for (i = 0; i < COLOR_HEIGHT; i++)
			{
				//determine focus mask
				//colortable[i + tid.x * COLOR_HEIGHT] = tex2Dfetch(SamplerHalfRes, int2(id.x, i));
				is_focus = colortable[i + tid.x * COLOR_HEIGHT].a < 0.9;  // 1 -not in focus 0-0.5 infocus+noiselevel 0.8:seperator_1 0.7 sep2
				//thresholding cells
				noise = colortable[i + tid.x * COLOR_HEIGHT].a < 0.6 ? 2 * colortable[i + tid.x * COLOR_HEIGHT].a : 0;
				is_seperate = is_focus &&  MaskingNoise > noise;
				seperate_area = active_seperator && is_focus && colortable[i + tid.x * COLOR_HEIGHT].a > 0.75 ? true : seperate_area;
				seperate_area = active_seperator && is_focus && colortable[i + tid.x * COLOR_HEIGHT].a > 0.65 && is_focus && colortable[i + tid.x * COLOR_HEIGHT].a < 0.75  ? false : seperate_area;

				if (!(is_focus && was_focus && seperate_area && !(is_seperate && !was_seperate)))
					maskval++;
				was_focus = is_focus;
				was_seperate = is_seperate;
				colortable[i + tid.x * COLOR_HEIGHT].a = (float)maskval + 0.5 * is_focus; // is the is_focus carryover depreciated? - Yes :)
			}
		}
		barrier();
		// sort the small arrays
		merge_sort(interval_start, interval_end, 1);
		//combine
		float4 key[THREAD_HEIGHT];
		float4 key_sorted[THREAD_HEIGHT];
		float4 sorted_array[2 * COLOR_HEIGHT / THREAD_HEIGHT];
		for (i = 1; i < THREAD_HEIGHT; i = 2 * i) // the amount of merges, just like a normal merge sort
		{
			barrier();
			uint groupsize = 2 * i;
			//keylist
			for (int j = 0; j < groupsize; j++) //probably redundancy between threads. optimzable
			{
				int curr = tid.y - (tid.y % groupsize) + j;
				key[curr] = colortable[curr * COLOR_HEIGHT / THREAD_HEIGHT + tid.x * COLOR_HEIGHT];
			}
			//sort keys
			int idy_sorted;
			int even = tid.y - (tid.y % groupsize);
			int k = even;
			int mid = even + groupsize / 2.0 - 1;
			int odd = mid + 1;
			int to = even + groupsize - 1;
			while (even <= mid && odd <= to)
			{
				if (min_color(key[even], key[odd]))
				{
					if (tid.y == even)
						idy_sorted = k;
					key_sorted[k++] = key[even++];
				}
				else
				{
					if (tid.y == odd)
						idy_sorted = k;
					key_sorted[k++] = key[odd++];
				}
			}
			// Copy remaining elements
			while (even <= mid)
			{
				if (tid.y == even)
					idy_sorted = k;
				key_sorted[k++] = key[even++];
			}
			while (odd <= to)
			{
				if (tid.y == odd)
					idy_sorted = k;
				key_sorted[k++] = key[odd++];
			}
			// calculate the real distance
			int diff_sorted = (idy_sorted % groupsize) - (tid.y % (groupsize / 2.0 ));
			int pos1 = tid.y * COLOR_HEIGHT / THREAD_HEIGHT;
			bool is_even = (tid.y % groupsize) < groupsize / 2.0;
			if (is_even)
			{
				evenblock[idy_sorted + tid.x * THREAD_HEIGHT] = pos1;
				if (diff_sorted == 0)
				{
					oddblock[idy_sorted + tid.x * THREAD_HEIGHT] = (tid.y - (tid.y % groupsize) + groupsize / 2.0) * COLOR_HEIGHT / THREAD_HEIGHT;
				}
				else
				{
					int odd_block_search_start = (tid.y - (tid.y % groupsize) + groupsize / 2.0 + diff_sorted - 1) * COLOR_HEIGHT / THREAD_HEIGHT;
					for (int i2 = 0; i2 < COLOR_HEIGHT / THREAD_HEIGHT; i2++)
					{ // n pls make logn in future
						oddblock[idy_sorted + tid.x * THREAD_HEIGHT] = odd_block_search_start + i2;
						if (min_color(key_sorted[idy_sorted], colortable[odd_block_search_start + i2 + tid.x * COLOR_HEIGHT]))
						{
							break;
						}
						else
						{
							oddblock[idy_sorted + tid.x * THREAD_HEIGHT] = odd_block_search_start + i2 + 1;
						}
					}
				}
			}
			else
			{
				oddblock[idy_sorted + tid.x * THREAD_HEIGHT] = pos1;
				if (diff_sorted == 0)
				{
					evenblock[idy_sorted + tid.x * THREAD_HEIGHT] = (tid.y - (tid.y % groupsize)) * COLOR_HEIGHT / THREAD_HEIGHT;
				}
				else
				{
					int even_block_search_start = (tid.y - (tid.y % groupsize) + diff_sorted - 1) * COLOR_HEIGHT / THREAD_HEIGHT;
					for (int i2 = 0; i2 < COLOR_HEIGHT / THREAD_HEIGHT; i2++)
					{
						evenblock[idy_sorted + tid.x * THREAD_HEIGHT] = even_block_search_start + i2;
						if (min_color(key_sorted[idy_sorted], colortable[even_block_search_start + i2 + tid.x * COLOR_HEIGHT]))
						{
							break;
						}
						else
						{
							evenblock[idy_sorted + tid.x * THREAD_HEIGHT] = even_block_search_start + i2 + 1;
						}
					}
				}
			}
			// find the corresponding block
			barrier();
			int even_start, even_end, odd_start, odd_end;
			even_start = evenblock[tid.y + tid.x * THREAD_HEIGHT];
			odd_start = oddblock[tid.y + tid.x * THREAD_HEIGHT];
			if ((tid.y + 1) % groupsize == 0)
			{
				even_end = (tid.y - (tid.y % groupsize) + groupsize / 2.0 ) * COLOR_HEIGHT / THREAD_HEIGHT;
				odd_end = (tid.y - (tid.y % groupsize) + groupsize) * COLOR_HEIGHT / THREAD_HEIGHT;
			}
			else
			{
				even_end = evenblock[tid.y + 1 + tid.x * THREAD_HEIGHT];
				odd_end = oddblock[tid.y + 1 + tid.x * THREAD_HEIGHT];
			}
			//sort the block
			int even_counter = even_start;
			int odd_counter = odd_start;
			int cc = 0;
			while (even_counter < even_end && odd_counter < odd_end)
			{
				if (min_color(colortable[even_counter + tid.x * COLOR_HEIGHT], colortable[odd_counter + tid.x * COLOR_HEIGHT]))
				{
					sorted_array[cc++] = colortable[even_counter++ + tid.x * COLOR_HEIGHT];
				}
				else
				{
					sorted_array[cc++] = colortable[odd_counter++ + tid.x * COLOR_HEIGHT];
				}
			}
			while (even_counter < even_end)
			{
				sorted_array[cc++] = colortable[even_counter++ + tid.x * COLOR_HEIGHT];
			}
			while (odd_counter < odd_end)
			{
				sorted_array[cc++] = colortable[odd_counter++ + tid.x * COLOR_HEIGHT];
			}
			//replace
			barrier();
			int sorted_array_size = cc;
			int global_position = odd_start + even_start - (tid.y - (tid.y % groupsize) + groupsize / 2.0 ) * COLOR_HEIGHT / THREAD_HEIGHT;
			for (int w = 0; w < cc; w++)
			{
				colortable[global_position + w + tid.x * COLOR_HEIGHT] = sorted_array[w];
			}
		}
		barrier();
		for (i = 0; i < COLOR_HEIGHT / THREAD_HEIGHT; i++)
		{
			colortable[row + i + tid.x * COLOR_HEIGHT].a = colortable[row + i + tid.x * COLOR_HEIGHT].a % 1;
			tex2Dstore(texColorSortStorage, float2(id.x, row + i), float4(colortable[row + i + tid.x * COLOR_HEIGHT]));
		}
	}

	//reproject to output window
	void downsample_color(float4 vpos : SV_Position, float2 texcoord : TEXCOORD, out float4 fragment : SV_Target)
	{
		float2 texcoord_new = rotate(texcoord, RotationAngle, true);
		fragment = tex2D(SamplerBackground, texcoord);
		float fragment_depth = ReShade::GetLinearizedDepth(texcoord);
		fragment = inFocus(fragment, fragment_depth, texcoord) ? tex2D(SamplerColorSort, texcoord_new) : fragment;
		fragment = (ShowSelectedHue * FilterColor) ? showHue(texcoord, fragment) : fragment;
		fragment = ShowMask ? tex2D(SamplerMask, texcoord).rrrr : fragment;
	}


	////////////////////////////////////////////////////////////////////////////////////////////////////////
	//***************************************                  *******************************************//
	//***************************************     Pipeline     *******************************************//
	//***************************************                  *******************************************//
	////////////////////////////////////////////////////////////////////////////////////////////////////////

	technique ColorSort_Masking < ui_tooltip = "This is the masking part of the shader. It has to be placed before ColorSort_Main. All effects between Masking and Main (e.g. Monochrome) will only apply to the sorted area."; >
	{
		pass maskColor { VertexShader = PostProcessVS; PixelShader = mask_color; RenderTarget = texMask; }
		pass saveBackground { VertexShader = PostProcessVS; PixelShader = save_background; RenderTarget = texBackground; }
		pass gradColor { VertexShader = PostProcessVS; PixelShader = grad_color; }
	}

	technique ColorSort_Main < ui_tooltip = "This is a compute shader, which sorts colors from brightest to darkest. You can filter the selection by depth and color."; >
	{
		pass preColor { VertexShader = PostProcessVS; PixelShader = pre_color; RenderTarget = texHalfRes; }
		pass sortColor { ComputeShader = sort_color< 2, THREAD_HEIGHT >; DispatchSizeX = BUFFER_WIDTH / 2; DispatchSizeY = 1; }
		pass downsampleColor { VertexShader = PostProcessVS; PixelShader = downsample_color; }
	}
} // Namespace End

#endif // Shader End

//sampling:
/*
64 threads normal merge sort											n*logn	parallel
now normal merge sort on 2 arrays the following way:
currently n<=32 arrays e.g. 32
split in 64/n e.g. 2 per array											n
take two arrays and compute key for each split Array a b e.g.a1a2b1b2	n
sort keys eg a1b1...													n		non-parallel
compute difference rank between each key and sorted						n		parallel
find each key in the other array										logn	parallel  currently n
then make an odd even list for both arrays and the keys

TODO:
-edge detection or other ideas, e.g. the triangle grid
*/
