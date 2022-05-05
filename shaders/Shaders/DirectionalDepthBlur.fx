////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Directional Depth Blur shader for ReShade
// By Frans Bouma, aka Otis / Infuse Project (Otis_Inf)
// https://fransbouma.com 
//
// This shader has been released under the following license:
//
// Copyright (c) 2020 Frans Bouma
// All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
// 
// * Redistributions of source code must retain the above copyright notice, this
//   list of conditions and the following disclaimer.
// 
// * Redistributions in binary form must reproduce the above copyright notice,
//   this list of conditions and the following disclaimer in the documentation
//   and/or other materials provided with the distribution.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
////////////////////////////////////////////////////////////////////////////////////////////////////
// 
// Version History
// 13-apr-2020:		v1.1: Added highlight control (I know it flips the hue in focus point mode, it's a bug that actually looks great), 
//					      higher precision in buffers, better defaults
// 10-apr-2020:		v1.0: First release
//
////////////////////////////////////////////////////////////////////////////////////////////////////


#include "ReShade.fxh"

namespace DirectionalDepthBlur
{
// Uncomment line below for debug info / code / controls
//	#define CD_DEBUG 1
	
	#define DIRECTIONAL_DEPTH_BLUR_VERSION "v1.0"

	//////////////////////////////////////////////////
	//
	// User interface controls
	//
	//////////////////////////////////////////////////

	uniform float FocusPlane <
		ui_category = "Focusing";
		ui_label= "Focus plane";
		ui_type = "drag";
		ui_min = 0.001; ui_max = 1.000;
		ui_step = 0.001;
		ui_tooltip = "The depth of the plane where the blur starts, related to the camera";
	> = 0.010;
	uniform float FocusRange <
		ui_category = "Focusing";
		ui_label= "Focus range";
		ui_type = "drag";
		ui_min = 0.001; ui_max = 1.000;
		ui_step = 0.001;
		ui_tooltip = "The range around the focus plane that's more or less not blurred.\n1.0 is the FocusPlaneMaxRange.";
	> = 0.001;
	uniform float FocusPlaneMaxRange <
		ui_category = "Focusing";
		ui_label= "Focus plane max range";
		ui_type = "drag";
		ui_min = 10; ui_max = 300;
		ui_step = 1;
		ui_tooltip = "The max range Focus Plane for when Focus Plane is 1.0.\n1000 is the horizon.";
	> = 150;
	uniform float BlurAngle <
		ui_category = "Blur tweaking";
		ui_label="Blur angle";
		ui_type = "drag";
		ui_min = 0.01; ui_max = 1.00;
		ui_tooltip = "The angle of the blur direction";
		ui_step = 0.01;
	> = 1.0;
	uniform float BlurLength <
		ui_category = "Blur tweaking";
		ui_label = "Blur length";
		ui_type = "drag";
		ui_min = 0.000; ui_max = 1.0;
		ui_step = 0.001;
		ui_tooltip = "The length of the blur strokes per pixel. 1.0 is the entire screen.";
	> = 0.1;
	uniform float BlurQuality <
		ui_category = "Blur tweaking";
		ui_label = "Blur quality";
		ui_type = "drag";
		ui_min = 0.01; ui_max = 1.0;
		ui_step = 0.01;
		ui_tooltip = "The quality of the blur. 1.0 means all pixels in the blur length are\nread, 0.5 means half of them are read.";
	> = 0.5;
	uniform float ScaleFactor <
		ui_category = "Blur tweaking";
		ui_label = "Scale factor";
		ui_type = "drag";
		ui_min = 0.010; ui_max = 1.000;
		ui_step = 0.001;
		ui_tooltip = "The scale factor for the pixels to blur. Lower values downscale the\nsource frame and will result in wider blur strokes.";
	> = 1.000;
	uniform int BlurType < 
		ui_category = "Blur tweaking";
		ui_type = "combo";
		ui_min= 0; ui_max=1;
		ui_items="Parallel Strokes\0Focus Point Targeting Strokes\0";
		ui_label = "The blur type";
		ui_tooltip = "The blur type. Focus Point Targeting Strokes means the blur directions\nper pixel are towards the Focus Point.";
	> = 0;
	uniform float2 FocusPoint <
		ui_category = "Blur tweaking";
		ui_label = "Blur focus point";
		ui_type = "drag";
		ui_step = 0.001;
		ui_min = 0.000; ui_max = 1.000;
		ui_tooltip = "The X and Y coordinates of the blur focus point, which is used for\nthe Blur type 'Focus Point Targeting Strokes'. 0,0 is the\nupper left corner, and 0.5, 0.5 is at the center of the screen.";
	> = float2(0.5, 0.5);
	uniform float3 FocusPointBlendColor <
		ui_category = "Blur tweaking";
		ui_label = "Focus point color";
		ui_type= "color";
		ui_tooltip = "The color of the focus point in Point focused mode. The closer a\npixel is to the focus point, the more it will become this color.\nIn (red , green, blue)";
	> = float3(0.0,0.0,0.0);
	uniform float FocusPointBlendFactor <
		ui_category = "Blur tweaking";
		ui_label = "Focus point color blend factor";
		ui_type = "drag";
		ui_min = 0.000; ui_max = 1.000;
		ui_step = 0.001;
		ui_tooltip = "The factor with which the focus point color is blended with the final image";
	> = 1.000;
	uniform float HighlightGain <
		ui_category = "Blur tweaking";
		ui_label="Highlight gain";
		ui_type = "drag";
		ui_min = 0.00; ui_max = 5.00;
		ui_tooltip = "The gain for highlights in the strokes plane. The higher the more a highlight gets\nbrighter.";
		ui_step = 0.01;
	> = 0.500;	
#if CD_DEBUG
	// ------------- DEBUG
	uniform bool DBVal1 <
		ui_category = "Debugging";
	> = false;
	uniform bool DBVal2 <
		ui_category = "Debugging";
	> = false;
	uniform float DBVal3f <
		ui_category = "Debugging";
		ui_type = "drag";
		ui_min = 0.00; ui_max = 1.00;
		ui_step = 0.01;
	> = 0.0;
	uniform float DBVal4f <
		ui_category = "Debugging";
		ui_type = "drag";
		ui_min = 0.00; ui_max = 10.00;
		ui_step = 0.01;
	> = 1.0;
#endif
	//////////////////////////////////////////////////
	//
	// Defines, constants, samplers, textures, uniforms, structs
	//
	//////////////////////////////////////////////////

#ifndef BUFFER_PIXEL_SIZE
	#define BUFFER_PIXEL_SIZE	ReShade::PixelSize
#endif
#ifndef BUFFER_SCREEN_SIZE
	#define BUFFER_SCREEN_SIZE	ReShade::ScreenSize
#endif

	uniform float2 MouseCoords < source = "mousepoint"; >;
	
	texture texDownsampledBackBuffer { Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = RGBA16F; };
	texture texBlurDestination { Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = RGBA16F; }; 
	
	sampler samplerDownsampledBackBuffer { Texture = texDownsampledBackBuffer; AddressU = MIRROR; AddressV = MIRROR; AddressW = MIRROR;};
	sampler samplerBlurDestination { Texture = texBlurDestination; };
	
	struct VSPIXELINFO
	{
		float4 vpos : SV_Position;
		float2 texCoords : TEXCOORD0;
		float2 pixelDelta: TEXCOORD1;
		float blurLengthInPixels: TEXCOORD2;
		float focusPlane: TEXCOORD3;
		float focusRange: TEXCOORD4;
		float4 texCoordsScaled: TEXCOORD5;
	};
	
	//////////////////////////////////////////////////
	//
	// Functions
	//
	//////////////////////////////////////////////////
	
	float2 CalculatePixelDeltas(float2 texCoords)
	{
		float2 mouseCoords = MouseCoords * BUFFER_PIXEL_SIZE;
		
		return (float2(FocusPoint.x - texCoords.x, FocusPoint.y - texCoords.y)) * length(BUFFER_PIXEL_SIZE);
	}
	
	float3 AccentuateWhites(float3 fragment)
	{
		return fragment / (1.5 - clamp(fragment, 0, 1.49));	// accentuate 'whites'. 1.5 factor was empirically determined.
	}
	
	float3 CorrectForWhiteAccentuation(float3 fragment)
	{
		return (fragment.rgb * 1.5) / (1.0 + fragment.rgb);		// correct for 'whites' accentuation in taps. 1.5 factor was empirically determined.
	}
	
	float3 PostProcessBlurredFragment(float3 fragment, float maxLuma, float3 averageGained, float normalizationFactor)
	{
		const float3 lumaDotWeight = float3(0.3, 0.59, 0.11);

		float newFragmentLuma = dot(fragment, lumaDotWeight);
		averageGained.rgb = CorrectForWhiteAccentuation(averageGained.rgb);
		// increase luma to the max luma found on the gained taps. This over-boosts the luma on the averageGained, which we'll use to blend
		// together with the non-boosted fragment using the normalization factor to smoothly merge the highlights.
		averageGained.rgb *= 1+saturate(maxLuma-newFragmentLuma);
		fragment = (1-normalizationFactor) * fragment + normalizationFactor * averageGained.rgb;
		return fragment;
	}
	
	//////////////////////////////////////////////////
	//
	// Vertex Shaders
	//
	//////////////////////////////////////////////////
	
	VSPIXELINFO VS_PixelInfo(in uint id : SV_VertexID)
	{
		VSPIXELINFO pixelInfo;
		
		pixelInfo.texCoords.x = (id == 2) ? 2.0 : 0.0;
		pixelInfo.texCoords.y = (id == 1) ? 2.0 : 0.0;
		pixelInfo.vpos = float4(pixelInfo.texCoords * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
		float angleToUse = 6.28318530717958 * BlurAngle;
		sincos(angleToUse, pixelInfo.pixelDelta.y, pixelInfo.pixelDelta.x);
		float pixelSizeLength = length(BUFFER_PIXEL_SIZE);
		pixelInfo.pixelDelta *= pixelSizeLength;
		pixelInfo.blurLengthInPixels = length(BUFFER_SCREEN_SIZE) * BlurLength;
		pixelInfo.focusPlane = (FocusPlane * FocusPlaneMaxRange) / 1000.0; 
		pixelInfo.focusRange = (FocusRange * FocusPlaneMaxRange) / 1000.0;
		pixelInfo.texCoordsScaled = float4(pixelInfo.texCoords * ScaleFactor, pixelInfo.texCoords / ScaleFactor);
		return pixelInfo;
	}

	//////////////////////////////////////////////////
	//
	// Pixel Shaders
	//
	//////////////////////////////////////////////////

	void PS_Blur(VSPIXELINFO pixelInfo, out float4 fragment : SV_Target0)
	{
		const float3 lumaDotWeight = float3(0.3, 0.59, 0.11);
		// pixelInfo.texCoordsScaled.xy is for scaled down UV, pixelInfo.texCoordsScaled.zw is for scaled up UV
		float3 color = tex2Dlod(samplerDownsampledBackBuffer, float4(pixelInfo.texCoordsScaled.xy, 0, 0)).rgb;
		float4 average = float4(color, 1.0);
		float3 averageGained = AccentuateWhites(average.rgb);
		float2 pixelDelta = BlurType==0 ? pixelInfo.pixelDelta : CalculatePixelDeltas(pixelInfo.texCoords);
		float maxLuma = dot(averageGained.rgb, lumaDotWeight);
		for(float tapIndex=0.0;tapIndex<pixelInfo.blurLengthInPixels;tapIndex+=(1/BlurQuality))
		{
			float2 tapCoords = (pixelInfo.texCoords + (pixelDelta * tapIndex));
			float3 tapColor = tex2Dlod(samplerDownsampledBackBuffer, float4(tapCoords * ScaleFactor, 0, 0)).rgb;
			float tapDepth = ReShade::GetLinearizedDepth(tapCoords);
			float weight = tapDepth <= pixelInfo.focusPlane ? 0.0 : 1-(tapIndex/ pixelInfo.blurLengthInPixels);
			average.rgb+=(tapColor * weight);
			average.a+=weight;
			float3 gainedTap = AccentuateWhites(tapColor.rgb);
			averageGained += gainedTap * weight;
			float lumaSample = saturate(dot(gainedTap, lumaDotWeight));
			maxLuma = weight > 0 ? max(maxLuma, lumaSample) : maxLuma;
		}
		float distanceToFocusPoint = distance(pixelInfo.texCoords, FocusPoint);
		fragment.rgb = average.rgb / (average.a + (average.a==0));
		fragment.rgb = BlurType==0 
							? fragment.rgb
							: lerp(fragment, lerp(FocusPointBlendColor, fragment.rgb, smoothstep(0, 1, distanceToFocusPoint)), FocusPointBlendFactor);
		fragment.rgb = PostProcessBlurredFragment(fragment.rgb, saturate(maxLuma), (averageGained / (average.a + (average.a==0))), HighlightGain);
		fragment.a = 1.0;
	}


	void PS_Combiner(VSPIXELINFO pixelInfo, out float4 fragment : SV_Target0)
	{
		float colorDepth = ReShade::GetLinearizedDepth(pixelInfo.texCoords);
		float4 realColor = tex2Dlod(ReShade::BackBuffer, float4(pixelInfo.texCoords, 0, 0));
		if(colorDepth <= pixelInfo.focusPlane || (BlurLength <= 0.0))
		{
			fragment = realColor;
			return;
		}
		float3 color = tex2Dlod(samplerBlurDestination, float4(pixelInfo.texCoords, 0, 0)).rgb;
		float rangeEnd = (pixelInfo.focusPlane+pixelInfo.focusRange);
		float blendFactor = rangeEnd < colorDepth 
								? 1.0 
								: smoothstep(0, 1, 1-((rangeEnd-colorDepth) / pixelInfo.focusRange));
		fragment.rgb = lerp(realColor.rgb, color, blendFactor);
	}
	
	void PS_DownSample(VSPIXELINFO pixelInfo, out float4 fragment : SV_Target0)
	{
		// pixelInfo.texCoordsScaled.xy is for scaled down UV, pixelInfo.texCoordsScaled.zw is for scaled up UV
		float2 sourceCoords = pixelInfo.texCoordsScaled.zw;
		if(max(sourceCoords.x, sourceCoords.y) > 1.0001)
		{
			// source pixel is outside the frame
			discard;
		}
		fragment = tex2D(ReShade::BackBuffer, sourceCoords);
	}
	
	//////////////////////////////////////////////////
	//
	// Techniques
	//
	//////////////////////////////////////////////////

	technique DirectionalDepthBlur
#if __RESHADE__ >= 40000
	< ui_tooltip = "Directional Depth Blur "
			DIRECTIONAL_DEPTH_BLUR_VERSION
			"\n===========================================\n\n"
			"Directional Depth Blur is a shader for adding far plane directional blur\n"
			"based on the depth of each pixel\n\n"
			"Directional Depth Blur was written by Frans 'Otis_Inf' Bouma and is part of OtisFX\n"
			"https://fransbouma.com | https://github.com/FransBouma/OtisFX"; >
#endif	
	{
		pass Downsample { VertexShader = VS_PixelInfo ; PixelShader = PS_DownSample; RenderTarget = texDownsampledBackBuffer; }
		pass BlurPass { VertexShader = VS_PixelInfo; PixelShader = PS_Blur; RenderTarget = texBlurDestination; }
		pass Combiner { VertexShader = VS_PixelInfo; PixelShader = PS_Combiner; }
	}
}