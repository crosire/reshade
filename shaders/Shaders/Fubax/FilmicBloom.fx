/** Filmic Bloom PS, version 1.0.0
All rights (c) 2020 Jakub Maksymilian Fober (the Author).

The Author provides this shader (the Work)
under the Creative Commons CC BY-ND 4.0 license available online at
http://creatifloatommons.org/licenses/by-nd/4.0/.
The Author further grants permission for reuse of screen-shots and game-play
recordings derived from the Work, provided that the reuse is for the purpose of
promoting and/or summarizing the Work or is a part of an online forum post or
social media post and that any use is accompanied by a link to the Work and a
credit to the Author. (crediting Author by pseudonym "Fubax" is acceptable)

For inquiries please contact jakub.m.fober@pm.me
For updates visit GitHub repository at
https://github.com/Fubaxiusz/fubax-shaders/
*/


	  ////////////
	 /// MENU ///
	////////////

#include "ReShadeUI.fxh"

uniform float BlurRadius < __UNIFORM_SLIDER_FLOAT1
	ui_label = "Blur radius";
	ui_min = 0.0; ui_max = 32.0; ui_step = 0.2;
> = 6.0;

uniform int BlurSamples < __UNIFORM_SLIDER_INT1
	ui_label = "Blur samples";
	ui_min = 2; ui_max = 32;
> = 6;


	  /////////////////
	 /// FUNCTIONS ///
	/////////////////

// Overlay filter by Fubax
// Generates smooth falloff for blur
// input is between 0-1
float get_weight(float progress)
{
	float bottom = min(progress, 0.5);
	float top = max(progress, 0.5);
	return 2.0 *(bottom*bottom +top +top -top*top) -1.5;
}
// 2D falloff
float get_2D_weight(float2 position)
{ return get_weight( min(length(position), 1.0) ); }


	  //////////////
	 /// SHADER ///
	//////////////

#include "ReShade.fxh"

void FilmicBloomPS(float4 vpos : SV_Position, float2 tex_coord : TEXCOORD, out float3 softened : SV_Target)
{
	softened = 0.0;

	float total_weight = 0.0;
	// Blur pixel
	for(int y=0; y<BlurSamples; y++)
	for(int x=0; x<BlurSamples; x++)
	{
		float2 current_position = float2(x,y)/BlurSamples;
		// Get current step weight
		float current_weight = get_2D_weight(1.0-current_position);
		// Get current step offset
		float2 current_offset = BUFFER_PIXEL_SIZE*BlurRadius*current_position*(1.0-current_weight);
		// Add to total weight
		total_weight += current_weight;
		// Soften image
		softened += (
			tex2Dlod(ReShade::BackBuffer, float4(tex_coord+current_offset, 0.0, 0.0) ).rgb+
			tex2Dlod(ReShade::BackBuffer, float4(tex_coord-current_offset, 0.0, 0.0) ).rgb+
			tex2Dlod(ReShade::BackBuffer, float4(tex_coord+float2(current_offset.x, -current_offset.y), 0.0, 0.0) ).rgb+
			tex2Dlod(ReShade::BackBuffer, float4(tex_coord-float2(current_offset.x, -current_offset.y), 0.0, 0.0) ).rgb
		) *current_weight;
	}
	softened /= total_weight*4.0;
}

	  //////////////
	 /// OUTPUT ///
	//////////////

technique FilmicBloom
<
	ui_label = "Filmic Bloom";
	ui_tooltip = "Simulates organic look of film to digital content";
>
{
	pass
	{
		VertexShader = PostProcessVS;
		PixelShader = FilmicBloomPS;
	}
}
