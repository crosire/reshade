/*
Filmic Anamorph Sharpen PS v1.4.4 (c) 2018 Jakub Maximilian Fober

This work is licensed under the Creative Commons
Attribution-ShareAlike 4.0 International License.
To view a copy of this license, visit
http://creativecommons.org/licenses/by-sa/4.0/.
*/


	  ////////////
	 /// MENU ///
	////////////

#include "ReShadeUI.fxh"

uniform float Strength < __UNIFORM_SLIDER_FLOAT1
	ui_label = "Strength";
	ui_min = 0.0; ui_max = 100.0; ui_step = 0.01;
> = 60.0;

uniform float Offset < __UNIFORM_SLIDER_FLOAT1
	ui_label = "Radius";
	ui_tooltip = "High-pass cross offset in pixels";
	ui_min = 0.0; ui_max = 2.0; ui_step = 0.01;
> = 0.1;

uniform float Clamp < __UNIFORM_SLIDER_FLOAT1
	ui_label = "Clamping";
	ui_min = 0.5; ui_max = 1.0; ui_step = 0.001;
> = 0.65;

uniform bool UseMask < __UNIFORM_INPUT_BOOL1
	ui_label = "Sharpen only center";
	ui_tooltip = "Sharpen only in center of the image";
> = false;

uniform bool DepthMask < __UNIFORM_INPUT_BOOL1
	ui_label = "Enable depth rim masking";
	ui_tooltip = "Depth high-pass mask switch";
	ui_category = "Depth mask";
	ui_category_closed = true;
> = false;

uniform int DepthMaskContrast < __UNIFORM_DRAG_INT1
	ui_label = "Edges mask strength";
	ui_tooltip = "Depth high-pass mask amount";
	ui_category = "Depth mask";
	ui_min = 0; ui_max = 2000; ui_step = 1;
> = 128;

uniform int Coefficient < __UNIFORM_RADIO_INT1
	ui_tooltip = "For digital video signal use BT.709, for analog (like VGA) use BT.601";
	ui_label = "YUV coefficients";
	ui_items = "BT.709 - digital\0BT.601 - analog\0";
	ui_category = "Additional settings";
	ui_category_closed = true;
> = 0;

uniform bool Preview < __UNIFORM_INPUT_BOOL1
	ui_label = "Preview sharpen layer";
	ui_tooltip = "Preview sharpen layer and mask for adjustment.\n"
		"If you don't see red strokes,\n"
		"try changing Preprocessor Definitions in the Settings tab.";
	ui_category = "Debug View";
	ui_category_closed = true;
> = false;


	  //////////////
	 /// SHADER ///
	//////////////

#include "ReShade.fxh"

// RGB to YUV709 Luma
static const float3 Luma709 = float3(0.2126, 0.7152, 0.0722);
// RGB to YUV601 Luma
static const float3 Luma601 = float3(0.299, 0.587, 0.114);

// Overlay blending mode
float Overlay(float LayerA, float LayerB)
{
	float MinA = min(LayerA, 0.5);
	float MinB = min(LayerB, 0.5);
	float MaxA = max(LayerA, 0.5);
	float MaxB = max(LayerB, 0.5);
	return 2.0 * (MinA * MinB + MaxA + MaxB - MaxA * MaxB) - 1.5;
}

// Overlay blending mode for one input
float Overlay(float LayerAB)
{
	float MinAB = min(LayerAB, 0.5);
	float MaxAB = max(LayerAB, 0.5);
	return 2.0 * (MinAB * MinAB + MaxAB + MaxAB - MaxAB * MaxAB) - 1.5;
}

// Sharpen pass
float3 FilmicAnamorphSharpenPS(float4 pos : SV_Position, float2 UvCoord : TEXCOORD) : SV_Target
{
	// Sample display image
	float3 Source = tex2D(ReShade::BackBuffer, UvCoord).rgb;

	// Generate radial mask
	float Mask;
	if (UseMask)
	{
		// Generate radial mask
		Mask = 1.0-length(UvCoord*2.0-1.0);
		Mask = Overlay(Mask) * Strength;
		// Bypass
		if (Mask <= 0) return Source;
	}
	else Mask = Strength;

	// Get pixel size
	float2 Pixel = BUFFER_PIXEL_SIZE;
	// Choose luma coefficient, if False BT.709 luma, else BT.601 luma
	const float3 LumaCoefficient = bool(Coefficient) ? Luma601 : Luma709;

	if (DepthMask)
	{
		float2 DepthPixel = Pixel*Offset + Pixel;
		Pixel *= Offset;
		// Sample display depth image
		float SourceDepth = ReShade::GetLinearizedDepth(UvCoord);

		float2 NorSouWesEst[4] = {
			float2(UvCoord.x, UvCoord.y + Pixel.y),
			float2(UvCoord.x, UvCoord.y - Pixel.y),
			float2(UvCoord.x + Pixel.x, UvCoord.y),
			float2(UvCoord.x - Pixel.x, UvCoord.y)
		};

		float2 DepthNorSouWesEst[4] = {
			float2(UvCoord.x, UvCoord.y + DepthPixel.y),
			float2(UvCoord.x, UvCoord.y - DepthPixel.y),
			float2(UvCoord.x + DepthPixel.x, UvCoord.y),
			float2(UvCoord.x - DepthPixel.x, UvCoord.y)
		};

		// Luma high-pass color
		// Luma high-pass depth
		float HighPassColor = 0.0, DepthMask = 0.0;

		[unroll]for(int s = 0; s < 4; s++)
		{
			HighPassColor += dot(tex2D(ReShade::BackBuffer, NorSouWesEst[s]).rgb, LumaCoefficient);
			DepthMask += ReShade::GetLinearizedDepth(NorSouWesEst[s])
			+ ReShade::GetLinearizedDepth(DepthNorSouWesEst[s]);
		}

		HighPassColor = 0.5 - 0.5 * (HighPassColor * 0.25 - dot(Source, LumaCoefficient));

		DepthMask = 1.0 - DepthMask * 0.125 + SourceDepth;
		DepthMask = min(1.0, DepthMask) + 1.0 - max(1.0, DepthMask);
		DepthMask = saturate(DepthMaskContrast * DepthMask + 1.0 - DepthMaskContrast);

		// Sharpen strength
		HighPassColor = lerp(0.5, HighPassColor, Mask * DepthMask);

		// Clamping sharpen
		HighPassColor = (Clamp != 1.0) ? max(min(HighPassColor, Clamp), 1.0 - Clamp) : HighPassColor;

		float3 Sharpen = float3(
			Overlay(Source.r, HighPassColor),
			Overlay(Source.g, HighPassColor),
			Overlay(Source.b, HighPassColor)
		);

		if(Preview) // Preview mode ON
		{
			float PreviewChannel = lerp(HighPassColor, HighPassColor * DepthMask, 0.5);
			return float3(
				1.0 - DepthMask * (1.0 - HighPassColor),
				PreviewChannel,
				PreviewChannel
			);
		}

		return Sharpen;
	}
	else
	{
		Pixel *= Offset;

		float2 NorSouWesEst[4] = {
			float2(UvCoord.x, UvCoord.y + Pixel.y),
			float2(UvCoord.x, UvCoord.y - Pixel.y),
			float2(UvCoord.x + Pixel.x, UvCoord.y),
			float2(UvCoord.x - Pixel.x, UvCoord.y)
		};

		// Luma high-pass color
		float HighPassColor = 0.0;
		[unroll]
		for(int s = 0; s < 4; s++)
			HighPassColor += dot(tex2D(ReShade::BackBuffer, NorSouWesEst[s]).rgb, LumaCoefficient);
		HighPassColor = 0.5 - 0.5 * (HighPassColor * 0.25 - dot(Source, LumaCoefficient));

		// Sharpen strength
		HighPassColor = lerp(0.5, HighPassColor, Mask);

		// Clamping sharpen
		HighPassColor = (Clamp != 1.0) ? max(min(HighPassColor, Clamp), 1.0 - Clamp) : HighPassColor;

		float3 Sharpen = float3(
			Overlay(Source.r, HighPassColor),
			Overlay(Source.g, HighPassColor),
			Overlay(Source.b, HighPassColor)
		);

		// Preview mode ON
		return Preview ? HighPassColor : Sharpen;
	}
}


	  //////////////
	 /// OUTPUT ///
	//////////////

technique FilmicAnamorphSharpen < ui_label = "Filmic Anamorphic Sharpen"; >
{
	pass
	{
		VertexShader = PostProcessVS;
		PixelShader = FilmicAnamorphSharpenPS;
	}
}
