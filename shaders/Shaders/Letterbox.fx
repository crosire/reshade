// Shader by Jacob Maximilian Fober
// https://creativecommons.org/publicdomain/mark/1.0/
// This work is free of known copyright restrictions.
// Letterbox PS v1.2.0


 	  ////////////
	 /// MENU ///
	////////////

#include "ReShadeUI.fxh"

uniform float4 Color < __UNIFORM_COLOR_FLOAT4
	ui_label = "Bars Color";
> = float4(0.027, 0.027, 0.027, 1.0);

uniform float DesiredAspect <
	ui_label = "Aspect Ratio";
	ui_tooltip = "Desired Aspect Ratio float";
	ui_type = "drag";
	ui_min = 1.0; ui_max = 3.0;
> = 2.4;

uniform int2 Dimensions <
	ui_label = "XY Dimensions";
	ui_tooltip = "Aspect Ratio below is used if set to zero";
	ui_type = "drag";
	ui_min = 0; ui_max = 16;
> = int2(0, 0);


	  //////////////
	 /// SHADER ///
	//////////////

#include "ReShade.fxh"

float3 LetterboxPS(float4 vois : SV_Position, float2 texcoord : TexCoord) : SV_Target
{
	// Get Aspect Ratio
	float RealAspect = ReShade::AspectRatio;
	// Sample display image
	float3 Display = tex2D(ReShade::BackBuffer, texcoord).rgb;

	float UserAspect = (!bool(Dimensions.x) || !bool(Dimensions.y)) ?
		DesiredAspect : float(Dimensions.x) / float(Dimensions.y)
	;

	if (RealAspect == UserAspect) return Display;
	else if (UserAspect > RealAspect)
	{
		// Get Letterbox Bars width
		float Bars = 0.5 - 0.5 * RealAspect / UserAspect;
		return (texcoord.y > Bars && texcoord.y < 1.0 - Bars) ?
			Display : lerp(Display, Color.rgb, Color.a)
		;
	}
	else
	{
		// Get Pillarbox Bars width
		float Bars = 0.5 - 0.5 * UserAspect / RealAspect;
		return (texcoord.x > Bars && texcoord.x < 1.0 - Bars) ?
			Display : lerp(Display, Color.rgb, Color.a)
		;
	}
}


	  //////////////
	 /// OUTPUT ///
	//////////////

technique Letterbox < ui_tooltip = "Letterbox / Pillarbox\n"
"Generate black-bars"; >
{
	pass
	{
		VertexShader = PostProcessVS;
		PixelShader = LetterboxPS;
	}
}
