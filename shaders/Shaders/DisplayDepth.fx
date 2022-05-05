///////////////////////////////////////////////////////
// Ported from Reshade v2.x. Original by CeeJay.
// Displays the depth buffer: further away is more white than close by. 
// Use this to configure the depth buffer preprocessor settings
// in Reshade's settings. (The RESHADE_DEPTH_INPUT_* ones)
///////////////////////////////////////////////////////

#include "Reshade.fxh"

#if __RESHADE__ < 30101
	#define __DISPLAYDEPTH_UI_FAR_PLANE_DEFAULT__ 1000.0
	#define __DISPLAYDEPTH_UI_UPSIDE_DOWN_DEFAULT__ 0
	#define __DISPLAYDEPTH_UI_REVERSED_DEFAULT__ 0
	#define __DISPLAYDEPTH_UI_LOGARITHMIC_DEFAULT__ 0
#else
	#define __DISPLAYDEPTH_UI_FAR_PLANE_DEFAULT__ 1000.0
	#define __DISPLAYDEPTH_UI_UPSIDE_DOWN_DEFAULT__ 0
	#define __DISPLAYDEPTH_UI_REVERSED_DEFAULT__ 1
	#define __DISPLAYDEPTH_UI_LOGARITHMIC_DEFAULT__ 0
#endif

uniform bool bUIUsePreprocessorDefs <
	ui_label = "Use global preprocessor definitions";
	ui_tooltip = "Enable this to override the values from\n"
	             "'Depth Input Settings' with the\n"
	             "preprocessor definitions. If all is set\n"
	             "up correctly, no difference should be\n"
	             "noticed.";
> = false;

uniform float fUIFarPlane <
	ui_type = "drag";
	ui_label = "Far Plane";
	ui_tooltip = "RESHADE_DEPTH_LINEARIZATION_FAR_PLANE=<value>\n"
	             "Changing this value is not necessary in most cases.";
	ui_min = 0.0; ui_max = 1000.0;
	ui_step = 0.1;
> = __DISPLAYDEPTH_UI_FAR_PLANE_DEFAULT__;

uniform int iUIUpsideDown <
	ui_type = "combo";
	ui_label = "";
	ui_items = "RESHADE_DEPTH_INPUT_IS_UPSIDE_DOWN=0\0RESHADE_DEPTH_INPUT_IS_UPSIDE_DOWN=1\0";
> = __DISPLAYDEPTH_UI_UPSIDE_DOWN_DEFAULT__;

uniform int iUIReversed <
	ui_type = "combo";
	ui_label = "";
	ui_items = "RESHADE_DEPTH_INPUT_IS_REVERSED=0\0RESHADE_DEPTH_INPUT_IS_REVERSED=1\0";
> = __DISPLAYDEPTH_UI_REVERSED_DEFAULT__;

uniform int iUILogarithmic <
	ui_type = "combo";
	ui_label = "";
	ui_items = "RESHADE_DEPTH_INPUT_IS_LOGARITHMIC=0\0RESHADE_DEPTH_INPUT_IS_LOGARITHMIC=1\0";
	ui_tooltip = "Change this setting if the displayed surface normals have stripes in them";
> = __DISPLAYDEPTH_UI_LOGARITHMIC_DEFAULT__;

uniform int iUIPresentType <
	ui_category = "Options";
	ui_type = "combo";
	ui_label = "Present type";
	ui_items = "Depth map\0Normal map\0Show both (Vertical 50/50)\0";
> = 2;

float GetDepth(float2 texcoord)
{
	//Return the depth value as defined in the preprocessor definitions
	if(bUIUsePreprocessorDefs)
	{
		return ReShade::GetLinearizedDepth(texcoord);
	}

	//Calculate the depth value as defined by the user
	//RESHADE_DEPTH_INPUT_IS_UPSIDE_DOWN
	if(iUIUpsideDown)
	{
		texcoord.y = 1.0 - texcoord.y;
	}

	float depth = tex2Dlod(ReShade::DepthBuffer, float4(texcoord, 0, 0)).x;
	//RESHADE_DEPTH_INPUT_IS_LOGARITHMIC
	if(iUILogarithmic)
	{
		const float C = 0.01;
		depth = (exp(depth * log(C + 1.0)) - 1.0) / C;
	}
	//RESHADE_DEPTH_INPUT_IS_REVERSED
	if(iUIReversed)
	{
		depth = 1.0 - depth;
	}

	const float N = 1.0;
	return depth /= fUIFarPlane - depth * (fUIFarPlane - N);
}

float3 NormalVector(float2 texcoord)
{
	float3 offset = float3(ReShade::PixelSize.xy, 0.0);
	float2 posCenter = texcoord.xy;
	float2 posNorth  = posCenter - offset.zy;
	float2 posEast   = posCenter + offset.xz;

	float3 vertCenter = float3(posCenter - 0.5, 1) * GetDepth(posCenter);
	float3 vertNorth  = float3(posNorth - 0.5,  1) * GetDepth(posNorth);
	float3 vertEast   = float3(posEast - 0.5,   1) * GetDepth(posEast);

	return normalize(cross(vertCenter - vertNorth, vertCenter - vertEast)) * 0.5 + 0.5;
}

void PS_DisplayDepth(in float4 position : SV_Position, in float2 texcoord : TEXCOORD0, out float3 color : SV_Target)
{
	float3 normal_color = NormalVector(texcoord);
	
	if(iUIPresentType == 1)
	{
		color = normal_color;
		return;
	}

	const float dither_bit = 8.0; //Number of bits per channel. Should be 8 for most monitors.

	/*------------------------.
	| :: Ordered Dithering :: |
	'------------------------*/
	//Calculate grid position
	float grid_position = frac(dot(texcoord, (ReShade::ScreenSize * float2(1.0 / 16.0, 10.0 / 36.0)) + 0.25));

	//Calculate how big the shift should be
	float dither_shift = 0.25 * (1.0 / (pow(2, dither_bit) - 1.0));

	//Shift the individual colors differently, thus making it even harder to see the dithering pattern
	float3 dither_shift_RGB = float3(dither_shift, -dither_shift, dither_shift); //subpixel dithering

	//modify shift acording to grid position.
	dither_shift_RGB = lerp(2.0 * dither_shift_RGB, -2.0 * dither_shift_RGB, grid_position); //shift acording to grid position.

	//shift the color by dither_shift
	float3 depth_color = GetDepth(texcoord).rrr + dither_shift_RGB;

	if(iUIPresentType == 0)
	{
		color = depth_color;
		return;
	}
	
	color = lerp(normal_color, depth_color, step(ReShade::ScreenSize.x / 2, position.x));
}

technique DisplayDepth <
	ui_tooltip = "This shader helps finding the right\n"
                 "preprocessor settings for the depth\n"
                 "input.\n"
                 "By default the calculated normals\n"
                 "are shown and the goal is to make the\n"
                 "displayed surface normals look smooth.\n"
                 "Change the options for *_IS_REVERSED\n"
                 "and *_IS_LOGARITHMIC in the variable editor\n"
                 "until this happens.\n"
                 "\n"
                 "Change the 'Present type' to 'Depth map'\n"
                 "and check whether close objects are dark\n"
                 "and far away objects are white.\n"
                 "\n"
                 "When the right settings are found click\n"
                 "'Edit global preprocessor definitions'\n"
                 "(Variable editor in the 'Home' tab)\n"
                 "and put them in there.\n"
                 "\n"
                 "Switching between normal map and\n"
                 "depth map is possible via 'Present type'\n"
                 "in the Options category.";
>
{
	pass
	{
		VertexShader = PostProcessVS;
		PixelShader = PS_DisplayDepth;
	}
}
