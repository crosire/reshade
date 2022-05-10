/*
Tilt-Shift PS v1.2.0 (c) 2018 Jacob Maximilian Fober,
(based on TiltShift effect (c) 2016 kingeric1992)

This work is licensed under the Creative Commons
Attribution-ShareAlike 4.0 International License.
To view a copy of this license, visit
http://creativecommons.org/licenses/by-sa/4.0/.
*/


	  ////////////
	 /// MENU ///
	////////////

#include "ReShadeUI.fxh"

uniform bool Line <
	ui_label = "Show Center Line";
> = false;

uniform int Axis < __UNIFORM_SLIDER_INT1
	ui_label = "Angle";
	#if __RESHADE__ < 40000
		ui_step = 1;
	#endif
	ui_min = -89; ui_max = 90;
> = 0;

uniform float Offset < __UNIFORM_SLIDER_FLOAT1
	ui_min = -1.41; ui_max = 1.41; ui_step = 0.01;
> = 0.05;

uniform float BlurCurve < __UNIFORM_SLIDER_FLOAT1
	ui_label = "Blur Curve";
	ui_min = 1.0; ui_max = 5.0; ui_step = 0.01;
	ui_label = "Blur Curve";
> = 1.0;

uniform float BlurMultiplier < __UNIFORM_SLIDER_FLOAT1
	ui_label = "Blur Multiplier";
	ui_min = 0.0; ui_max = 100.0; ui_step = 0.2;
> = 6.0;

uniform int BlurSamples < __UNIFORM_SLIDER_INT1
	ui_label = "Blur Samples";
	ui_min = 2; ui_max = 32;
> = 11;

// First pass render target, to make sure Alpha channel exists
texture TiltShiftTarget < pooled = true; > { Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = RGBA8; };
sampler TiltShiftSampler { Texture = TiltShiftTarget; };


	  /////////////////
	 /// FUNCTIONS ///
	/////////////////

// Overlay filter by Fubax
// Generates smooth falloff for blur
// input is between 0-1
float get_weight(float t)
{
	float bottom = min(t, 0.5);
	float top = max(t, 0.5);
	return 2.0 *(bottom*bottom +top +top -top*top) -1.5;
}

	  //////////////
	 /// SHADER ///
	//////////////

#include "ReShade.fxh"

void TiltShiftPass1PS(float4 vpos : SV_Position, float2 UvCoord : TEXCOORD, out float4 Image : SV_Target)
{
	// Grab screen texture
	Image.rgb = tex2D(ReShade::BackBuffer, UvCoord).rgb;
	// Correct Aspect Ratio
	float2 UvCoordAspect = UvCoord;
	UvCoordAspect.y += BUFFER_ASPECT_RATIO * 0.5 - 0.5;
	UvCoordAspect.y /= BUFFER_ASPECT_RATIO;
	// Center coordinates
	UvCoordAspect = UvCoordAspect * 2.0 - 1.0;
	// Tilt vector
	float Angle = radians(-Axis);
	float2 TiltVector = float2(sin(Angle), cos(Angle));
	// Blur mask
	float BlurMask = abs(dot(TiltVector, UvCoordAspect) + Offset);
	BlurMask = max(0.0, min(1.0, BlurMask));
		// Set alpha channel
		Image.a = BlurMask;
	BlurMask = pow(Image.a, BlurCurve);

	// Horizontal blur
	if(BlurMask > 0)
	{
		// Get offset for this pixel
		float UvOffset = BUFFER_PIXEL_SIZE.x *BlurMask *BlurMultiplier;
		// Set initial weight for first dry single sample
		float total_weight = 0.5;
		// Blur with dynamic samples
		for (int i=1; i<BlurSamples; i++)
		{
			// Get current sample
			float current_sample = float(i)/float(BlurSamples);
			// Get current sample weight
			float current_weight = get_weight(1.0-current_sample);
			// Add to total weight
			total_weight += current_weight;
			// Get current sample offset
			float SampleOffset = current_sample*11.0 * UvOffset; // (*11.0) to maintain version compatibility
			// Add blur samples
			Image.rgb += (
				 tex2Dlod( ReShade::BackBuffer, float4(float2(UvCoord.x+SampleOffset, UvCoord.y), 0.0, 0.0) ).rgb
				+tex2Dlod( ReShade::BackBuffer, float4(float2(UvCoord.x-SampleOffset, UvCoord.y), 0.0, 0.0) ).rgb
			) *current_weight;
		}
		// Normalize
		Image.rgb /= total_weight*2.0;
	}
}

void TiltShiftPass2PS(float4 vpos : SV_Position, float2 UvCoord : TEXCOORD, out float4 Image : SV_Target)
{
	// Grab second pass screen texture
	Image = tex2D(TiltShiftSampler, UvCoord);
	// Blur mask
	float BlurMask = pow(abs(Image.a), BlurCurve);
	// Vertical blur
	if(BlurMask > 0)
	{
		// Get offset for this pixel
		float UvOffset = BUFFER_PIXEL_SIZE.y *BlurMask *BlurMultiplier;
		// Set initial weight for first dry single sample
		float total_weight = 0.5;
		// Blur with dynamic samples
		for (int i=1; i<BlurSamples; i++)
		{
			// Get current sample
			float current_sample = float(i)/float(BlurSamples);
			// Get current sample weight
			float current_weight = get_weight(1.0-current_sample);
			// Add to total weight
			total_weight += current_weight;
			// Get current sample offset
			float SampleOffset = current_sample*11.0 * UvOffset; // (*11.0) to maintain version compatibility
			// Add blur samples
			Image.rgb += (
				 tex2Dlod( TiltShiftSampler, float4(float2(UvCoord.x, UvCoord.y+SampleOffset), 0.0, 0.0) ).rgb
				+tex2Dlod( TiltShiftSampler, float4(float2(UvCoord.x, UvCoord.y-SampleOffset), 0.0, 0.0) ).rgb
			) *current_weight;
		}
		// Normalize
		Image.rgb /= total_weight*2.0;
	}
	// Draw red line
	// Image IS Red IF (Line IS True AND Image.a < 0.01), ELSE Image IS Image
	Image.rgb = (Line && Image.a < 0.01) ? float3(1.0, 0.0, 0.0) : Image.rgb;
}


	  //////////////
	 /// OUTPUT ///
	//////////////

technique TiltShift < ui_label = "Tilt Shift"; >
{
	pass AlphaAndHorizontalGaussianBlur
	{
		VertexShader = PostProcessVS;
		PixelShader = TiltShiftPass1PS;
		RenderTarget = TiltShiftTarget;
	}
	pass VerticalGaussianBlurAndRedLine
	{
		VertexShader = PostProcessVS;
		PixelShader = TiltShiftPass2PS;
	}
}
