/* Shoot Shake PS, version 1.0.1
(c) 2019 Jakub Max Fober

This work is licensed under a Creative Commons 
Attribution 4.0 International License.
To view a copy of this license, visit 
http://creativecommons.org/licenses/by/4.0/

Please leave proper copyright somewhere appropriate.
*/

#include "ReShade.fxh"
#include "ReShadeUI.fxh"

texture texCamShakeFalloff { Width = 1; Height = 1; Format = R16F; };
sampler samperCamShakeFalloff
{
	Texture = texCamShakeFalloff;
	SRGBTexture = false;
	MagFilter = POINT;
	MinFilter = POINT;
	MipFilter = POINT;
};
uniform bool mouseClick < source = "mousebutton"; keycode = 0; mode = "press"; >;
uniform float frameTime < source = "frametime"; >;

//////////
// MENU //
//////////

uniform float K0 < __UNIFORM_SLIDER_FLOAT1
	ui_label = "Shake scale";
	ui_min = 0.01; ui_max = -0.25;
> = -0.07;

uniform float FalloffTime < __UNIFORM_SLIDER_FLOAT1
	ui_label = "Shake time";
	ui_min = 0.07; ui_max = 0.1;
> = 0.07;

// uniform int Order < __UNIFORM_SLIDER_INT1
// 	ui_label = "Shake radius";
// 	ui_min = 1; ui_max = 3;
// > = 1;

////////////
// SHADER //
////////////

// Generate shake falloff
float ClickFalloff (float4 pos : SV_Position, float2 texcoord : TEXCOORD) : SV_Target
{
	// Get current falloff value
	float falloff = tex2D(samperCamShakeFalloff, float2(0, 0)).r;

	// If mouse clicked, reset falloff
	if (mouseClick) falloff = 1.0;
	// Else apply falloff
	else falloff /= frameTime*FalloffTime;

	return falloff;
}

// Apply shake
float3 ScreenShakePS (float4 pos : SV_Position, float2 texcoord : TEXCOORD) : SV_Target
{
	// Get diagonal length
	const float diagonal = length( float2(ReShade::AspectRatio, 1.0) );
	// Center coordinates
	texcoord = texcoord*2.0-1.0;
	// Correct aspect ratio
	texcoord.x *= ReShade::AspectRatio;
	// Normalize diagonally
	texcoord /= diagonal;

	// Get falloff
	float falloff = tex2D(samperCamShakeFalloff, float2(0, 0)).r;
	// Apply Brown-Conrady distortion model
	// texcoord *= 1.0 + K0*pow(length(texcoord), Order)*falloff;
	texcoord *= 1.0 + K0*length(texcoord)*falloff;

	// Normalize vertically
	texcoord *= diagonal;
	// Aspect ratio back to square
	texcoord.x /= ReShade::AspectRatio;
	// Center back to corner
	texcoord = texcoord*0.5+0.5;

	return tex2D(ReShade::BackBuffer, texcoord).rgb;
}

////////////
// OUTPUT //
////////////

technique ScreenShake
<
	ui_label = "ScreenShake";
	ui_tooltip = "Shakes the screen when mouse button pressed.";
>
{
	pass
	{
		VertexShader = PostProcessVS;
		PixelShader = ClickFalloff;
		RenderTarget = texCamShakeFalloff;
		ClearRenderTargets = false;
		BlendEnable = true;
			BlendOp = ADD;
	}
	pass
	{
		VertexShader = PostProcessVS;
		PixelShader = ScreenShakePS;
	}
}
