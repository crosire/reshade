////////////////////////////////////////////////////////////////////////////////////////////////////////
// Cobra_Mask.fx by SirCobra
// Version 0.2
// You can find info and my repository here: https://github.com/LordKobra/CobraFX
// This effect is designed to be used with the ColorSort and Gravity shader, to apply a Mask with
// similar settings to the scene. All shaders between Cobra_Masking_Start and Cobra_Masking_End
// are only affecting the unmasked area.
// The effect can be applied to a specific area like a DoF shader. The basic methods for this were taken with permission
// from https://github.com/FransBouma/OtisFX/blob/master/Shaders/Emphasize.fx
////////////////////////////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////////////////////////////
//***************************************                  *******************************************//
//***************************************   UI & Defines   *******************************************//
//***************************************                  *******************************************//
////////////////////////////////////////////////////////////////////////////////////////////////////////


// Shader Start
#include "Reshade.fxh"

// Namespace everything
namespace Cobra_Masking
{

	//defines
	#define MASKING_M "General Options\n"
	#define MASKING_C "Color Masking\n"
	#define MASKING_D "Depth Masking\n"

	#ifndef M_PI
		#define M_PI 3.1415927
	#endif

	//ui
	uniform int Buffer1 <
		ui_type = "radio"; ui_label = " ";
	>;	
	uniform bool InvertMask <
		ui_tooltip = "Invert the mask.";
		ui_category = MASKING_M;
	> = false;
	uniform bool ShowMask <
		ui_tooltip = "Show the masked pixels.";
		ui_category = MASKING_M;
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
		ui_min = 0.000; ui_max = 1.001;
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
	> = 1.0;
	uniform float ValueEdge <
		ui_type = "slider";
		ui_min = 0.000; ui_max = 1.000;
		ui_step = 0.001;
		ui_tooltip = "The smoothness beyond the value range.";
		ui_category = MASKING_C;
	> = 0.0;
	uniform float Hue <
		ui_type = "slider";
		ui_min = 0.000; ui_max = 1.000;
		ui_step = 0.001;
		ui_tooltip = "The hue describes the color category. It can be red, green, blue or a mix of them.";
		ui_category = MASKING_C;
	> = 1.0;
	uniform float HueRange <
		ui_type = "slider";
		ui_min = 0.000; ui_max = 0.500;
		ui_step = 0.001;
		ui_tooltip = "The tolerance around the hue.";
		ui_category = MASKING_C;
	> = 0.5;
	uniform float Saturation <
		ui_type = "slider";
		ui_min = 0.000; ui_max = 1.000;
		ui_step = 0.001;
		ui_tooltip = "The saturation determines the colorfulness. 0 is greyscale and 1 pure colors.";
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
	uniform float FocusEdgeDepth <
		ui_type = "slider";
		ui_min = 0.000; ui_max = 1.000;
		ui_tooltip = "The depth of the edge of the focus range. Range from 0.00, which means no depth, so at the edge of the focus range, the effect kicks in at full force,\ntill 1.00, which means the effect is smoothly applied over the range focusRangeEdge-horizon.";
		ui_step = 0.001;
	> = 0.050;
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


	texture texMask {Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = RGBA16F; };

	sampler2D SamplerMask { Texture = texMask; };


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


	////////////////////////////////////////////////////////////////////////////////////////////////////////
	//***************************************                  *******************************************//
	//***************************************     Masking      *******************************************//
	//***************************************                  *******************************************//
	////////////////////////////////////////////////////////////////////////////////////////////////////////

	// returns a value between 0 and 1 (1 = in focus)
	float inFocus(float4 rgbval, float scenedepth, float2 texcoord)
	{
		//colorfilter
		float4 hsvval = rgb2hsv(rgbval);
		float d1_f = abs(hsvval.b - Value) - ValueRange;
		d1_f = 1 - smoothstep(0, ValueEdge, d1_f);
		bool d2 = abs(hsvval.r - Hue) < (HueRange + pow(2.71828, -(hsvval.g * hsvval.g) / 0.005)) || (1 - abs(hsvval.r - Hue)) < (HueRange + pow(2.71828, -(hsvval.g * hsvval.g) / 0.01));
		bool d3 = abs(hsvval.g - Saturation) <= SaturationRange;
		float is_color_focus = max(d3 * d2 * d1_f, FilterColor == 0); // color threshold
		//depthfilter
		const float desaturateFullRange = FocusRangeDepth + FocusEdgeDepth;
		texcoord.x = (texcoord.x - Sphere_FocusHorizontal) * ReShade::ScreenSize.x;
		texcoord.y = (texcoord.y - Sphere_FocusVertical) * ReShade::ScreenSize.y;
		const float degreePerPixel = Sphere_FieldOfView / ReShade::ScreenSize.x;
		const float fovDifference = sqrt((texcoord.x * texcoord.x) + (texcoord.y * texcoord.y)) * degreePerPixel;
		float depthdiff = Spherical ? sqrt((scenedepth * scenedepth) + (FocusDepth * FocusDepth) - (2 * scenedepth * FocusDepth * cos(fovDifference * (2 * M_PI / 360)))) : abs(scenedepth - FocusDepth);
		float depthval = 1 - saturate((depthdiff > desaturateFullRange) ? 1.0 : smoothstep(FocusRangeDepth, desaturateFullRange, depthdiff));
		depthval = max(depthval, FilterDepth == 0);
		return is_color_focus * depthval;
	}

	void mask_start(float4 vpos : SV_Position, float2 texcoord : TEXCOORD, out float4 fragment : SV_Target)
	{
		float4 color = tex2D(ReShade::BackBuffer, texcoord);
		float scenedepth = ReShade::GetLinearizedDepth(texcoord);
		float in_focus = inFocus(color, scenedepth, texcoord);
		in_focus = (1 - InvertMask) * in_focus + InvertMask * (1 - in_focus);
		fragment = float4(color.rgb, in_focus);
	}

	void mask_end(float4 vpos : SV_Position, float2 texcoord : TEXCOORD, out float4 fragment : SV_Target)
	{
		fragment = tex2D(SamplerMask, texcoord);
		fragment = ShowMask ? fragment.aaaa : fragment;
		fragment = (!ShowMask) ? lerp(tex2D(ReShade::BackBuffer, texcoord), fragment, 1 - fragment.aaaa) : fragment;
		fragment = (ShowSelectedHue * FilterColor * !ShowMask) ? showHue(texcoord, fragment) : fragment;
	}


	/////////////////////////////////////////////////////////////////////////////////////////////////////////
	//*****************************************                  ******************************************//
	//*****************************************     Pipeline     ******************************************//
	//*****************************************                  ******************************************//
	/////////////////////////////////////////////////////////////////////////////////////////////////////////


	technique Cobra_Masking_Start 
	< ui_tooltip = "This is the masking part of the shader. It has to be placed before Cobra_Masking_End. The masked area is copied and stored here, meaning all effects applied between Start and End only affect the unmasked area."; >
	{
		pass mask { VertexShader = PostProcessVS; PixelShader = mask_start; RenderTarget = texMask; }
	}

	technique CobraFX_Masking_End < ui_tooltip = "The masked area is applied again onto the screen."; >
	{
		pass display { VertexShader = PostProcessVS; PixelShader = mask_end; }
	}
}
