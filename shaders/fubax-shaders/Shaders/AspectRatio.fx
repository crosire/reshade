/** Aspect Ratio PS, version 1.1.1
by Fubax 2019 for ReShade
*/

#include "ReShadeUI.fxh"

uniform float A < __UNIFORM_SLIDER_FLOAT1
	ui_label = "Correct proportions";
	ui_category = "Aspect ratio";
	ui_min = -1.0; ui_max = 1.0;
> = 0.0;

uniform float Zoom < __UNIFORM_SLIDER_FLOAT1
	ui_label = "Scale image";
	ui_category = "Aspect ratio";
	ui_min = 1.0; ui_max = 1.5;
> = 1.0;

uniform bool FitScreen < __UNIFORM_INPUT_BOOL1
	ui_label = "Scale image to borders";
	ui_category = "Borders";
> = true;

uniform bool UseBackground < __UNIFORM_INPUT_BOOL1
	ui_label = "Use background image";
	ui_category = "Borders";
> = true;

uniform float4 Color < __UNIFORM_COLOR_FLOAT4
	ui_label = "Background color";
	ui_category = "Borders";
> = float4(0.027, 0.027, 0.027, 0.17);

#include "ReShade.fxh"

	  //////////////
	 /// SHADER ///
	//////////////

texture AspectBgTex < source = "AspectRatio.jpg"; > { Width = 1351; Height = 1013; };
sampler AspectBgSampler { Texture = AspectBgTex; };

float3 AspectRatioPS(float4 pos : SV_Position, float2 texcoord : TEXCOORD) : SV_Target
{
	bool Mask = false;

	// Center coordinates
	float2 coord = texcoord-0.5;

	// if (Zoom != 1.0) coord /= Zoom;
	if (Zoom != 1.0) coord /= clamp(Zoom, 1.0, 1.5); // Anti-cheat

	// Squeeze horizontally
	if (A<0)
	{
		coord.x *= abs(A)+1.0; // Apply distortion

		// Scale to borders
		if (FitScreen) coord /= abs(A)+1.0;
		else // Mask image borders
			Mask = abs(coord.x)>0.5;
	}
	// Squeeze vertically
	else if (A>0)
	{
		coord.y *= A+1.0; // Apply distortion

		// Scale to borders
		if (FitScreen) coord /= abs(A)+1.0;
		else // Mask image borders
			Mask = abs(coord.y)>0.5;
	}

	// Coordinates back to the corner
	coord += 0.5;

	// Sample display image and return
	if (UseBackground && !FitScreen) // If borders are visible
		return Mask?
			lerp( tex2D(AspectBgSampler, texcoord).rgb, Color.rgb, Color.a ) :
			tex2D(ReShade::BackBuffer, coord).rgb;
	else
		return Mask? Color.rgb : tex2D(ReShade::BackBuffer, coord).rgb;
}


	  ///////////////
	 /// DISPLAY ///
	///////////////

technique AspectRatioPS
<
	ui_label = "Aspect Ratio";
	ui_tooltip = "Correct image aspect ratio";
>
{
	pass
	{
		VertexShader = PostProcessVS;
		PixelShader = AspectRatioPS;
	}
}
