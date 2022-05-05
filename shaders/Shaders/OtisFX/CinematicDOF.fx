////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Cinematic Depth of Field shader, using scatter-as-gather for ReShade 3.x+
// By Frans Bouma, aka Otis / Infuse Project (Otis_Inf)
// https://fransbouma.com 
//
// This shader has been released under the following license:
//
// Copyright (c) 2018-2019 Frans Bouma
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
// Version history:
// 26-mar-2020:    v1.1.17: FreeStyle support added (not yet ansel superres compatible). Fixed issue with far plane highlight causing near plane edge pixels getting highlighted.
// 15-mar-2020:    v1.1.16: Dithering added for low-luma areas to avoid banding. (Contributed by Prod80)
// 03-feb-2020:    v1.1.15: Experimental near plane edge blur improvements.
// 04-oct-2019:    v1.1.14: Fine-tuning of near plane blur using smaller tiles. 
// 23-jun-2019:    v1.1.13: Cleanup of highlight code, reimplementing of luma boost / highlightblending. Removal of unnecessary controls.
// 13-jun-2019:    v1.1.12: Bugfix in maxColor blending in near/far blur: no more dirty edges on large highlighted areas.
// 10-jun-2019:	   v1.1.11: Added new weight calculation, added near-plane highlight normalization.
// 25-may-2019:	   v1.1.10: Added white boost/correction in gathering passes to have lower-intensity highlights become less prominent. 
//							Added further weight adjustment tweaks. Changed highlight defaults to utilize code changed in 1.1.9/1.1.10
// 24-may-2019:		v1.1.9: Better near-plane bleed mask. Better far plane pixel weights so more samples get accepted.
// 02-mar-2019: 	v1.1.8: Added anamorphic bokeh support, so bokehs now get stretched and rotated based on the distance from the center of the screen, with various tweaks.
// 08-jan-2019:		v1.1.7: Added 9-tap tent filter as described in [Jimenez2014) for mitigating undersampling. Implementation is from KinoBokeh (see credits below).
// 02-jan-2019:		v1.1.6: When near plane max blur is set to 0, the original fragment is now used in the near plane instead of the half-res pixel. 
// 19-dec-2018:		v1.1.5: Added far plane highlight normalizing for non-gained highlights. Added tooltip for reshade v4.x
// 14-dec-2018:		v1.1.4: Far plane weight calculation tweaked a bit as near-focus plane elements could lead to hard edges which looked ugly. Highlight far plane
//							adjustments have been reworked because of this. 
// 10-dec-2018:		v1.1.3: Removed averaging pass for CoC values as it resulted in noticeable wrong CoC values around edges in some TAA using games. The net result
//							was minimal anyway. 
// 10-nov-2018:		v1.1.2: Near plane bugfix: tile gatherer should collect min CoC, not average of min CoC: now ends of narrow lines are properly handled too.
// 30-oct-2018:		v1.1.1: Near plane bugfix for high resolutions: it's now blurring resolution independently. Highlight bleed fix in near focus. 
// 21-oct-2018:		v1.1.0: Far plane weights adjustment, half-res with upscale combiner for performance, new highlights implementation, fixed 
//							pre-blur highlight smoothing.
// 10-oct-2018:		v1.0.8: Improved, tile-based near-plane bleed, optimizations, far-plane large CoC bleed limitation, Highlight dimming, fixed in-focus
// 						    bleed with post-smooth blur, fixed highlight edges, fixed pre-blur.
// 21-sep-2018:		v1.0.7: Better near-plane bleed. Optimized near plane CoC storage so less reads are needed. 
//							Corrected post-blur bleed. Corrected near plane highlight bleed. Overall micro-optimizations.
// 04-sep-2018:		v1.0.6: Small fix for DX9 and autofocus.
// 17-aug-2018:		v1.0.5: Much better highlighting, higher range for manual focus
// 12-aug-2018:		v1.0.4: Finetuned the workaround for d3d9 to only affect reshade 3.4 or lower. 
//							Finetuned the near highlight extrapolation a bit. Removed highlight threshold as it ruined the blur
// 10-aug-2018:		v1.0.3: Daodan's crosshair code added.
// 09-aug-2018:		v1.0.2: Added workaround for d3d9 glitch in reshade 3.4.
// 08-aug-2018:		v1.0.1: namespace addition for samplers/textures.
// 08-aug-2018:		v1.0.0: beta. Feature complete. 
//
////////////////////////////////////////////////////////////////////////////////////////////////////
// Additional credits:
// Gaussian blur code based on the Gaussian blur ReShade shader by Ioxa
// Thanks to Daodan for the crosshair code in the focus helper.
// 9 tap tent filter is from KinoBokeh Copyright (C) 2015 Keijiro Takahashi. MIT licensed. See file below for details.
//       Ref:  https://github.com/keijiro/KinoBokeh/blob/master/Assets/Kino/Bokeh/Shader/Composition.cginc
// Thanks to Prod80 for contributing dithering in combiner to avoid banding in low-luma blurred areas. 
////////////////////////////////////////////////////////////////////////////////////////////////////
// References:
//
// [Lee2008]		Sungkil Lee, Gerard Jounghyun Kim, and Seungmoon Choi: Real-Time Depth-of-Field Rendering Using Point Splatting 
//					on Per-Pixel Layers. 
//					https://pdfs.semanticscholar.org/80f6/f40fe971eddc810c3c86fca6fdfe5c0fdd76.pdf
// 
// [Jimenez2014]	Jorge Jimenez, Sledgehammer Games: Next generation post processing in Call of Duty Advanced Warfare, SIGGRAPH2014
//					http://www.iryoku.com/next-generation-post-processing-in-call-of-duty-advanced-warfare
//
// [Nilsson2012]	Filip Nilsson: Implementing realistic depth of field in OpenGL. 
//					http://fileadmin.cs.lth.se/cs/education/edan35/lectures/12dof.pdf
////////////////////////////////////////////////////////////////////////////////////////////////////

#include "ReShade.fxh"

namespace CinematicDOF
{
	#define CINEMATIC_DOF_VERSION "v1.1.17"

// Uncomment line below for debug info / code / controls
//	#define CD_DEBUG 1

	//////////////////////////////////////////////////
	//
	// User interface controls
	//
	//////////////////////////////////////////////////

	// ------------- FOCUSING
	uniform bool UseAutoFocus <
		ui_category = "Focusing";
		ui_label = "Use auto-focus";
		ui_tooltip = "If enabled it will make the shader focus on the point specified as 'Auto-focus point',\notherwise it will put the focus plane at the depth specified in 'Manual-focus plane'.";
	> = true;
#if __RESHADE_FXC__		// Freestyle
	uniform bool ShowFocusingOverlay <
		ui_category = "Focusing";
		ui_label = "Show focusing overlay";
		ui_tooltip = "Enables the out-of-focus plane overlay\nwhich helps with fine-tuning the focusing.";
	> = false;
#else
	uniform bool UseMouseDrivenAutoFocus <
		ui_category = "Focusing";
		ui_label = "Use mouse-driven auto-focus";
		ui_tooltip = "Enables mouse driven auto-focus. If enabled, and 'Use auto-focus' is enabled, the\nauto-focus point is read from the mouse coordinates, otherwise the 'Auto-focus point' is used.";
	> = true;
#endif
	uniform float2 AutoFocusPoint <
		ui_category = "Focusing";
		ui_label = "Auto-focus point";
		ui_type = "drag";
		ui_step = 0.001;
		ui_min = 0.000; ui_max = 1.000;
		ui_tooltip = "The X and Y coordinates of the auto-focus point. 0,0 is the upper left corner,\nand 0.5, 0.5 is at the center of the screen. Only used if 'Use auto focus' is enabled.";
	> = float2(0.5, 0.5);
	uniform float AutoFocusTransitionSpeed <
		ui_category = "Focusing";
		ui_label= "Auto-focus transition speed";
		ui_type = "drag";
		ui_min = 0.001; ui_max = 1.0;
		ui_step = 0.01;
		ui_tooltip = "The speed the shader will transition between different focus points when using auto-focus.\n0.001 means very slow, 1.0 means instantly. Only used if 'Use auto-focus' is enabled.";
	> = 0.2;
#if __RESHADE_FXC__		// Freestyle
	uniform float ManualFocusPlaneMaxRange <
		ui_category = "Focusing";
		ui_label= "Manual-focus plane max range";
		ui_type = "drag";
		ui_min = 10; ui_max = 150;
		ui_step = 1;
		ui_tooltip = "The depth of focal plane related to the camera when 'Use auto-focus' is off.\n'1.0' means the at the horizon. 0 means at the camera.\nOnly used if 'Use auto-focus' is disabled.";
	> = 10;
#endif
	uniform float ManualFocusPlane <
		ui_category = "Focusing";
		ui_label= "Manual-focus plane";
		ui_type = "drag";
#if __RESHADE_FXC__		// Freestyle
		ui_min = 0.000; ui_max = 1.0;
		ui_step = 0.001;
		ui_tooltip = "The depth of focal plane related to the camera when 'Use auto-focus' is off.\n'1.0' means the ManualFocusPlaneMaxRange. 0 means at the camera.\nOnly used if 'Use auto-focus' is disabled.";
	> = 0.01;
#else
		ui_min = 0.100; ui_max = 150.00;
		ui_step = 0.01;
		ui_tooltip = "The depth of focal plane related to the camera when 'Use auto-focus' is off.\nOnly used if 'Use auto-focus' is disabled.";
	> = 10.00;
#endif
	uniform float FocalLength <
		ui_category = "Focusing";
		ui_label = "Focal length (mm)";
		ui_type = "drag";
		ui_min = 10; ui_max = 300.0;
		ui_step = 1.0;
		ui_tooltip = "Focal length of the used lens. The longer the focal length, the narrower the\ndepth of field and thus the more is out of focus. For portraits, start with 120 or 150.";
	> = 100.00;
	uniform float FNumber <
		ui_category = "Focusing";
		ui_label = "Aperture (f-number)";
		ui_type = "drag";
		ui_min = 1; ui_max = 22.0;
		ui_step = 0.1;
		ui_tooltip = "The f-number (also known as f-stop) to use. The higher the number, the wider\nthe depth of field, meaning the more is in-focus and thus the less is out of focus.\nFor portraits, start with 2.8.";
	> = 2.8;
#ifndef __RESHADE_FXC__		// Reshade
	// ------------- FOCUSING, OVERLAY
	uniform bool ShowOutOfFocusPlaneOnMouseDown <
		ui_category = "Focusing, overlay";
		ui_label = "Show out-of-focus plane overlay on mouse down";
		ui_tooltip = "Enables the out-of-focus plane overlay when the left mouse button is pressed down,\nwhich helps with fine-tuning the focusing.";
	> = true;
	uniform float3 OutOfFocusPlaneColor <
		ui_category = "Focusing, overlay";
		ui_label = "Out-of-focus plane overlay color";
		ui_type= "color";
		ui_tooltip = "Specifies the color of the out-of-focus planes rendered when the left-mouse button\nis pressed and 'Show out-of-focus plane on mouse down' is enabled. In (red , green, blue)";
	> = float3(0.8,0.8,0.8);
	uniform float OutOfFocusPlaneColorTransparency <
		ui_category = "Focusing, overlay";
		ui_label = "Out-of-focus plane transparency";
		ui_type = "drag";
		ui_min = 0.01; ui_max = 1.0;
		ui_tooltip = "Amount of transparency of the out-of-focus planes. 0.0 is transparent, 1.0 is opaque.";
	> = 0.7;
	uniform float3 FocusPlaneColor <
		ui_category = "Focusing, overlay";
		ui_label = "Focus plane overlay color";
		ui_type= "color";
		ui_tooltip = "Specifies the color of the focus plane rendered when the left-mouse button\nis pressed and 'Show out-of-focus plane on mouse down' is enabled. In (red , green, blue)";
	> = float3(0.0, 0.0, 1.0);
	uniform float4 FocusCrosshairColor<
		ui_category = "Focusing, overlay";
		ui_label = "Focus crosshair color";
		ui_type = "color";
		ui_tooltip = "Specifies the color of the crosshair for the auto-focus.\nAuto-focus must be enabled";
	> = float4(1.0, 0.0, 1.0, 1.0);
#endif
	// ------------- BLUR TWEAKING
	uniform float FarPlaneMaxBlur <
		ui_category = "Blur tweaking";
		ui_label = "Far plane max blur";
		ui_type = "drag";
		ui_min = 0.000; ui_max = 4.0;
		ui_step = 0.01;
		ui_tooltip = "The maximum blur a pixel can have when it has its maximum CoC in the far plane. Use this as a tweak\nto adjust the max far plane blur defined by the lens parameters. Don't use this as your primarily\nblur factor, use the lens parameters Focal Length and Aperture for that instead.";
	> = 1.0;
	uniform float NearPlaneMaxBlur <
		ui_category = "Blur tweaking";
		ui_label = "Near plane max blur";
		ui_type = "drag";
		ui_min = 0.000; ui_max = 4.0;
		ui_step = 0.01;
		ui_tooltip = "The maximum blur a pixel can have when it has its maximum CoC in the near Plane. Use this as a tweak to\nadjust the max near plane blur defined by the lens parameters.  Don't use this as your primarily blur factor,\nuse the lens parameters Focal Length and Aperture for that instead.";
	> = 1.0;
	uniform float BlurQuality <
		ui_category = "Blur tweaking";
		ui_label = "Overall blur quality";
		ui_type = "drag";
		ui_min = 2.0; ui_max = 12.0;
		ui_tooltip = "The number of rings to use in the disc-blur algorithm. The more rings the better\nthe blur results, but also the slower it will get.";
		ui_step = 1;
	> = 5.0;
	uniform float BokehBusyFactor <
		ui_category = "Blur tweaking";
		ui_label="Bokeh busy factor";
		ui_type = "drag";
		ui_min = 0.00; ui_max = 1.00;
		ui_tooltip = "The 'bokeh busy factor' for the blur: 0 means no busyness boost, 1.0 means extra busyness boost.";
		ui_step = 0.01;
	> = 0.500;
	uniform float PostBlurSmoothing <
		ui_category = "Blur tweaking";
		ui_label = "Post-blur smoothing factor";
		ui_type = "drag";
		ui_min = 0.0; ui_max = 2.0;
		ui_tooltip = "The amount of post-blur smoothing blur to apply. 0.0 means no smoothing blur is applied.";
		ui_step = 0.01;
	> = 0.0;
	// ------------- HIGHLIGHT TWEAKING
	uniform float HighlightAnamorphicFactor <
		ui_category = "Highlight tweaking, anamorphism";
		ui_label="Anamorphic factor";
		ui_type = "drag";
		ui_min = 0.01; ui_max = 1.00;
		ui_tooltip = "The anamorphic factor of the bokeh highlights. A value of 1.0 (default) gives perfect\ncircles, a factor of e.g. 0.1 gives thin ellipses";
		ui_step = 0.01;
	> = 1.0;
	uniform float HighlightAnamorphicSpreadFactor <
		ui_category = "Highlight tweaking, anamorphism";
		ui_label="Anamorphic spread factor";
		ui_type = "drag";
		ui_min = 0.00; ui_max = 1.00;
		ui_tooltip = "The spread factor for the anamorphic factor. 0.0 means it's relative to the distance\nto the center of the screen, 1.0 means the factor is applied everywhere evenly,\nno matter how far the pixel is to the center of the screen.";
		ui_step = 0.01;
	> = 0.0;
	uniform float HighlightAnamorphicAlignmentFactor <
		ui_category = "Highlight tweaking, anamorphism";
		ui_label="Anamorphic alignment factor";
		ui_type = "drag";
		ui_min = 0.00; ui_max = 1.00;
		ui_tooltip = "The alignment factor for the anamorphic deformation. 0.0 means you get evenly rotated\nellipses around the center of the screen, 1.0 means all bokeh highlights are\naligned vertically.";
		ui_step = 0.01;
	> = 0.0;
	uniform float HighlightGainFarPlane <
		ui_category = "Highlight tweaking, far plane";
#if __RESHADE_FXC__		// Freestyle
		ui_label="Highlight gain far plane";
#else
		ui_label="Highlight gain";
#endif
		ui_type = "drag";
		ui_min = 0.00; ui_max = 5.00;
		ui_tooltip = "The gain for highlights in the far plane. The higher the more a highlight gets\nbrighter. Tweak this in tandem with the Highlight threshold. Best results are\nachieved with bright spots in dark(er) backgrounds.";
		ui_step = 0.01;
	> = 0.500;
	uniform float HighlightThresholdFarPlane <
		ui_category = "Highlight tweaking, far plane";
#if __RESHADE_FXC__		// Freestyle
		ui_label="Highlight threshold far plane";
#else
		ui_label="Highlight threshold";
#endif
		ui_type = "drag";
		ui_min = 0.00; ui_max = 1.00;
		ui_tooltip = "The threshold for the source pixels. Pixels with a luminosity above this threshold\nwill be highlighted. Raise this value to only keep the highlights you want.\nWhen highlight type is Twinkle circlets, set the threshold at 0.5 or higher\nfor blur without highlights.";
		ui_step = 0.01;
	> = 0.0;
	uniform float HighlightGainNearPlane <
		ui_category = "Highlight tweaking, near plane";
#if __RESHADE_FXC__		// Freestyle
		ui_label="Highlight gain near plane";
#else
		ui_label="Highlight gain";
#endif
		ui_type = "drag";
		ui_min = 0.00; ui_max = 5.00;
		ui_tooltip = "The gain for highlights in the near plane. The higher the more a highlight gets\nbrighter. Tweak this in tandem with the Highlight threshold. Best results are\nachieved with bright spots in dark(er) foregrounds.";
		ui_step = 0.01;
	> = 0.000;
	uniform float HighlightThresholdNearPlane <
		ui_category = "Highlight tweaking, near plane";
#if __RESHADE_FXC__		// Freestyle
		ui_label="Highlight threshold near plane";
#else
		ui_label="Highlight threshold";
#endif
		ui_type = "drag";
		ui_min = 0.00; ui_max = 1.00;
		ui_tooltip = "The threshold for the source pixels. Pixels with a luminosity above this threshold\nwill be highlighted. Raise this value to only keep the highlights you want.\nWhen highlight type is Twinkle circlets, set the threshold at 0.5 or higher\nfor blur without highlights.";
		ui_step = 0.01;
	> = 0.0;
	
	// ------------- ADVANCED SETTINGS
	uniform bool ShowCoCValues <
		ui_category = "Advanced";
		ui_label = "Show CoC values and focus plane";
		ui_tooltip = "Shows blur disc size (CoC) as grey (far plane) and red (near plane) and focus plane as blue";
	> = false;
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
	uniform bool ShowNearCoCTilesBlurredR <
		ui_category = "Debugging";
		ui_tooltip = "Shows the near coc blur buffer as b&w";
	> = false;
	uniform bool ShowNearCoCTiles <
		ui_category = "Debugging";
	> = false;
	uniform bool ShowNearCoCTilesNeighbor <
		ui_category = "Debugging";
	> = false;
	uniform bool ShowNearCoCTilesBlurredG <
		ui_category = "Debugging";
	> = false;
	uniform bool ShowNearPlaneAlpha <
		ui_category = "Debugging";
	> = false;
	uniform bool ShowNearPlaneBlurred <
		ui_category = "Debugging";
	> = false;
	uniform bool ShowOnlyFarPlaneBlurred <
		ui_category = "Debugging";
	> = false;
#endif
	//////////////////////////////////////////////////
	//
	// Defines, constants, samplers, textures, uniforms, structs
	//
	//////////////////////////////////////////////////

#if __RESHADE_FXC__		// Freestyle
	#define OUT_OF_FOCUS_PLANE_COLORTRANSPARENCY 	0.5
#endif
	#define SENSOR_SIZE			0.024		// Height of the 35mm full-frame format (36mm x 24mm)
	#define PI 					3.1415926535897932
	#define TILE_SIZE			1			// amount of pixels left/right/up/down of the current pixel. So 4 is 9x9
	#define TILE_MULTIPLY		1
	#define GROUND_TRUTH_SCREEN_WIDTH	1920.0f
	#define GROUND_TRUTH_SCREEN_HEIGHT	1200.0f

#ifndef BUFFER_PIXEL_SIZE
	#define BUFFER_PIXEL_SIZE	ReShade::PixelSize
#endif
#ifndef BUFFER_SCREEN_SIZE
	#define BUFFER_SCREEN_SIZE	ReShade::ScreenSize
#endif
	
	texture texCDCurrentFocus		{ Width = 1; Height = 1; Format = R16F; };		// for storing the current focus depth obtained from the focus point
	texture texCDPreviousFocus		{ Width = 1; Height = 1; Format = R16F; };		// for storing the previous frame's focus depth from texCDCurrentFocus.
	texture texCDCoC				{ Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = R16F; };
	texture texCDCoCTileTmp			{ Width = BUFFER_WIDTH/(TILE_SIZE*TILE_MULTIPLY); Height = BUFFER_HEIGHT/(TILE_SIZE*TILE_MULTIPLY); Format = R16F; };	// R is MinCoC
	texture texCDCoCTile			{ Width = BUFFER_WIDTH/(TILE_SIZE*TILE_MULTIPLY); Height = BUFFER_HEIGHT/(TILE_SIZE*TILE_MULTIPLY); Format = R16F; };	// R is MinCoC
	texture texCDCoCTileNeighbor	{ Width = BUFFER_WIDTH/(TILE_SIZE*TILE_MULTIPLY); Height = BUFFER_HEIGHT/(TILE_SIZE*TILE_MULTIPLY); Format = R16F; };	// R is MinCoC
	texture texCDCoCTmp1			{ Width = BUFFER_WIDTH/2; Height = BUFFER_HEIGHT/2; Format = R16F; };	// half res, single value
	texture texCDCoCBlurred			{ Width = BUFFER_WIDTH/2; Height = BUFFER_HEIGHT/2; Format = RG16F; };	// half res, blurred CoC (r) and real CoC (g)
	texture texCDBuffer1 			{ Width = BUFFER_WIDTH/2; Height = BUFFER_HEIGHT/2; Format = RGBA16F; };
	texture texCDBuffer2 			{ Width = BUFFER_WIDTH/2; Height = BUFFER_HEIGHT/2; Format = RGBA16F; }; 
	texture texCDBuffer3 			{ Width = BUFFER_WIDTH/2; Height = BUFFER_HEIGHT/2; Format = RGBA16F; };// needed for tentfilter as far/near have to be preserved in buffer 1 and 2
	texture texCDBuffer4 			{ Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = RGBA16F; }; 	// Full res upscale buffer
	texture texCDBuffer5 			{ Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = RGBA16F; }; 	// Full res upscale buffer. We need 2 as post smooth needs 2
	texture texCDNoise				< source = "monochrome_gaussnoise.png"; > { Width = 512; Height = 512; Format = RGBA8; };

	sampler	SamplerCDCurrentFocus		{ Texture = texCDCurrentFocus; };
	sampler SamplerCDPreviousFocus		{ Texture = texCDPreviousFocus; };
	sampler SamplerCDBuffer1 			{ Texture = texCDBuffer1; };
	sampler SamplerCDBuffer2 			{ Texture = texCDBuffer2; };
	sampler SamplerCDBuffer3 			{ Texture = texCDBuffer3; };
	sampler SamplerCDBuffer4 			{ Texture = texCDBuffer4; };
	sampler SamplerCDBuffer5 			{ Texture = texCDBuffer5; };
	sampler SamplerCDCoC				{ Texture = texCDCoC; MagFilter = POINT; MinFilter = POINT; MipFilter = POINT;};
	sampler SamplerCDCoCTmp1			{ Texture = texCDCoCTmp1; MagFilter = POINT; MinFilter = POINT; MipFilter = POINT;};
	sampler SamplerCDCoCBlurred			{ Texture = texCDCoCBlurred; MagFilter = POINT; MinFilter = POINT; MipFilter = POINT;};
	sampler SamplerCDCoCTileTmp			{ Texture = texCDCoCTileTmp; MagFilter = POINT; MinFilter = POINT; MipFilter = POINT;};
	sampler SamplerCDCoCTile			{ Texture = texCDCoCTile; MagFilter = POINT; MinFilter = POINT; MipFilter = POINT;};
	sampler SamplerCDCoCTileNeighbor	{ Texture = texCDCoCTileNeighbor; MagFilter = POINT; MinFilter = POINT; MipFilter = POINT;};
	sampler SamplerCDNoise				{ Texture = texCDNoise; MipFilter = POINT; MinFilter = POINT; MagFilter = POINT; AddressU = WRAP; AddressV = WRAP; AddressW = WRAP;};

#ifndef __RESHADE_FXC__		// Freestyle
	uniform float2 MouseCoords < source = "mousepoint"; >;
	uniform bool LeftMouseDown < source = "mousebutton"; keycode = 0; toggle = false; >;
#endif

	// simple struct for the Focus vertex shader.
	struct VSFOCUSINFO
	{
		float4 vpos : SV_Position;
		float2 texcoord : TEXCOORD0;
		float focusDepth : TEXCOORD1;
		float focusDepthInM : TEXCOORD2;
		float focusDepthInMM : TEXCOORD3;
		float pixelSizeLength : TEXCOORD4;
		float nearPlaneInMM : TEXCOORD5;
		float farPlaneInMM : TEXCOORD6;
	};

	struct VSDISCBLURINFO
	{
		float4 vpos : SV_Position;
		float2 texcoord : TEXCOORD0;
		float numberOfRings : TEXCOORD1;
		float farPlaneMaxBlurInPixels : TEXCOORD2;
		float nearPlaneMaxBlurInPixels : TEXCOORD3;
		float cocFactorPerPixel : TEXCOORD4;
	};
	
	//////////////////////////////////////////////////
	//
	// Functions
	//
	//////////////////////////////////////////////////
	
	float3 AccentuateWhites(float3 fragment)
	{
		return fragment / (1.5 - clamp(fragment, 0, 1.49));	// accentuate 'whites'. 1.5 factor was empirically determined.
	}
	
	float3 CorrectForWhiteAccentuation(float3 fragment)
	{
		return (fragment.rgb * 1.5) / (1.0 + fragment.rgb);		// correct for 'whites' accentuation in taps. 1.5 factor was empirically determined.
	}
	
	// returns 2 vectors, (x,y) are up vector, (z,w) are right vector. 
	// In: pixelVector which is the current pixel converted into a vector where (0,0) is the center of the screen.
	float4 CalculateAnamorphicFactor(float2 pixelVector)
	{
		float normalizedFactor = lerp(1, HighlightAnamorphicFactor, lerp(length(pixelVector * 2), 1, HighlightAnamorphicSpreadFactor));
		return float4(0, 1 + (1-normalizedFactor), normalizedFactor, 0);
	}
	
	// Calculates a rotation matrix for the current pixel specified in texcoord, which can be used to rotate the bokeh shape to match
	// a distored field around the center of the screen: it rotates the anamorphic factors with this matrix so the bokeh shapes form a circle
	// around the center of the screen. 
	float2x2 CalculateAnamorphicRotationMatrix(float2 texcoord)
	{
		float2 pixelVector = normalize(texcoord - 0.5);
		float limiter = (1-HighlightAnamorphicAlignmentFactor)/2;
		pixelVector.y = clamp(pixelVector.y, -limiter, limiter);
		float2 refVector = normalize(float2(-0.5, 0));
		float2 sincosFactor = float2(0,0);
		// calculate the angle between the pixelvector and the ref vector and grab the sin/cos for that angle for the rotation matrix.
		sincos(atan2(pixelVector.y, pixelVector.x) - atan2(refVector.y, refVector.x), sincosFactor.x, sincosFactor.y);
		return float2x2(sincosFactor.y, sincosFactor.x, -sincosFactor.x, sincosFactor.y);
	}
	
	float2 MorphPointOffsetWithAnamorphicDeltas(float2 pointOffset, float4 anamorphicFactors, float2x2 anamorphicRotationMatrix)
	{
		pointOffset.x = pointOffset.x * anamorphicFactors.x + pointOffset.x*anamorphicFactors.z;
		pointOffset.y = pointOffset.y * anamorphicFactors.y + pointOffset.y*anamorphicFactors.w;
		return mul(pointOffset, anamorphicRotationMatrix);
	}
	
	// Gathers min CoC from a horizontal range of pixels around the pixel at texcoord, for a range of -TILE_SIZE+1 to +TILE_SIZE+1.
	// returns minCoC
	float PerformTileGatherHorizontal(sampler source, float2 texcoord)
	{
		float tileSize = TILE_SIZE * (BUFFER_SCREEN_SIZE.x / GROUND_TRUTH_SCREEN_WIDTH);
		float minCoC = 10;
		float coc;
		float2 coordOffset = float2(BUFFER_PIXEL_SIZE.x, 0);
		for(float i = 0; i <= tileSize; ++i) 
		{
			coc = tex2Dlod(source, float4(texcoord + coordOffset, 0, 0)).r;
			minCoC = min(minCoC, coc);
			coc = tex2Dlod(source, float4(texcoord - coordOffset, 0, 0)).r;
			minCoC = min(minCoC, coc);
			coordOffset.x+=BUFFER_PIXEL_SIZE.x;
		}
		return minCoC;
	}

	// Gathers min CoC from a vertical range of pixels around the pixel at texcoord from the high-res focus plane, for a range of -TILE_SIZE+1 to +TILE_SIZE+1.
	// returns min CoC
	float PerformTileGatherVertical(sampler source, float2 texcoord)
	{
		float tileSize = TILE_SIZE * (BUFFER_SCREEN_SIZE.y / GROUND_TRUTH_SCREEN_HEIGHT);
		float minCoC = 10;
		float coc;
		float2 coordOffset = float2(0, BUFFER_PIXEL_SIZE.y);
		for(float i = 0; i <= tileSize; ++i) 
		{
			coc = tex2Dlod(source, float4(texcoord + coordOffset, 0, 0)).r;
			minCoC = min(minCoC, coc);
			coc = tex2Dlod(source, float4(texcoord - coordOffset, 0, 0)).r;
			minCoC = min(minCoC, coc);
			coordOffset.y+=BUFFER_PIXEL_SIZE.y;
		}
		return minCoC;
	}
	
	// Gathers the min CoC of the tile at texcoord and the 8 tiles around it. 
	float PerformNeighborTileGather(sampler source, float2 texcoord)
	{
		float minCoC = 10;
		float tileSizeX = TILE_SIZE * (BUFFER_SCREEN_SIZE.x / GROUND_TRUTH_SCREEN_WIDTH);
		float tileSizeY = TILE_SIZE * (BUFFER_SCREEN_SIZE.y / GROUND_TRUTH_SCREEN_HEIGHT);
		// tile is TILE_SIZE*2+1 wide. So add that and substract that to get to neighbor tile right/left.
		// 3x3 around center.
		float2 baseCoordOffset = float2(BUFFER_PIXEL_SIZE.x * (tileSizeX*2+1), BUFFER_PIXEL_SIZE.x * (tileSizeY*2+1));
		for(float i=-1;i<2;i++)
		{
			for(float j=-1;j<2;j++)
			{
				float2 coordOffset = float2(baseCoordOffset.x * i, baseCoordOffset.y * j);
				float coc = tex2Dlod(source, float4(texcoord + coordOffset, 0, 0)).r;
				minCoC = min(minCoC, coc);
			}
		}
		return minCoC;
	}

	// Calculates an RGBA fragment based on the CoC radius specified, for debugging purposes.
	// In: 	radius, the CoC radius to calculate the fragment for
	//		showInFocus, flag which will give a blue edge at the focus plane if true
	// Out:	RGBA fragment for color buffer based on the radius specified. 
	float4 GetDebugFragment(float radius, bool showInFocus)
	{
		float4 toReturn = (radius/2 <= length(BUFFER_PIXEL_SIZE)) && showInFocus ? float4(0.0, 0.0, 1.0, 1.0) : float4(radius, radius, radius, 1.0);
		if(radius < 0)
		{
			toReturn = float4(-radius, 0, 0, 1);
		}
		return toReturn;
	}

	// Calculates the blur disc size for the pixel at the texcoord specified. A blur disc is the CoC size at the image plane.
	// In:	VSFOCUSINFO struct filled by the vertex shader VS_Focus
	// Out:	The blur disc size for the pixel at texcoord. Format: near plane: < 0. In-focus: 0. Far plane: > 0. Range: [-1, 1].
	float CalculateBlurDiscSize(VSFOCUSINFO focusInfo)
	{
		float pixelDepth = ReShade::GetLinearizedDepth(focusInfo.texcoord);
		float pixelDepthInM = pixelDepth * 1000.0;			// in meter

		// CoC (blur disc size) calculation based on [Lee2008]
		// CoC = ((EF / Zf - F) * (abs(Z-Zf) / Z)
		// where E is aperture size in mm, F is focal length in mm, Zf is depth of focal plane in mm, Z is depth of pixel in mm.
		// To calculate aperture in mm, we use D = F/N, where F is focal length and N is f-number
		// For the people getting confused: 
		// Remember element sizes are in mm, our depth sizes are in meter, so we have to divide S1 by 1000 to get from meter -> mm. We don't have to
		// divide the elements in the 'abs(x-S1)/x' part, as the 1000.0 will then simply be muted out (as  a / (x/1000) == a * (1000/x))
		// formula: (((f*f) / N) / ((S1/1000.0) -f)) * (abs(x - S1) / x)
		// where f = FocalLength, N = FNumber, S1 = focusInfo.focusDepthInM, x = pixelDepthInM. In-lined to save on registers. 
		float cocInMM = (((FocalLength*FocalLength) / FNumber) / ((focusInfo.focusDepthInM/1000.0) -FocalLength)) * 
						(abs(pixelDepthInM - focusInfo.focusDepthInM) / (pixelDepthInM + (pixelDepthInM==0)));
		float toReturn = clamp(saturate(abs(cocInMM) * SENSOR_SIZE), 0, 1); // divide by sensor size to get coc in % of screen (or better: in sampler units)
		return (pixelDepth < focusInfo.focusDepth) ? -toReturn : toReturn;
	}
	
	// calculate the sample weight based on the values specified. 
	float CalculateSampleWeight(float sampleRadiusInCoC, float ringDistanceInCoC)
	{
		return  saturate(sampleRadiusInCoC - ringDistanceInCoC + 0.5);
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
	
	
	// Same as PerformDiscBlur but this time for the near plane. It's in a separate function to avoid a lot of if/switch statements as
	// the near plane blur requires different semantics.
	// Based on [Nilsson2012] and a variant of [Jimenez2014] where far/in-focus pixels are receiving a higher weight so they bleed into the near plane, 
	// In:	blurInfo, the pre-calculated disc blur information from the vertex shader.
	// 		source, the source to read RGBA fragments from. Luma in alpha
	// Out: RGBA fragment for the pixel at texcoord in source, which is the blurred variant of it if it's in the near plane. A is alpha
	// to blend with.
	float4 PerformNearPlaneDiscBlur(VSDISCBLURINFO blurInfo, sampler2D source)
	{
		const float3 lumaDotWeight = float3(0.3, 0.59, 0.11);
		float4 fragment = tex2Dlod(source, float4(blurInfo.texcoord, 0, 0));
		// r contains blurred CoC, g contains original CoC. Original is negative.
		float2 fragmentRadii = tex2Dlod(SamplerCDCoCBlurred, float4(blurInfo.texcoord, 0, 0)).rg;
		float fragmentRadiusToUse = fragmentRadii.r;

		if(fragmentRadii.r <=0)
		{
			// the blurred CoC value is still 0, we'll never end up with a pixel that has a different value than fragment, so abort now by
			// returning the fragment we already read.
			fragment.a = 0;
			return fragment;
		}
		
		// use one extra ring as undersampling is really prominent in near-camera objects.
		float numberOfRings = max(blurInfo.numberOfRings, 1) + 1;
		float pointsFirstRing = 7;
		// luma is stored in alpha
		float bokehBusyFactorToUse = saturate(1.0-BokehBusyFactor);		// use the busy factor as an edge bias on the blur, not the highlights
		float4 average = float4(fragment.rgb * fragmentRadiusToUse * bokehBusyFactorToUse, bokehBusyFactorToUse);
		float3 averageGained = AccentuateWhites(average.rgb);
		float2 pointOffset = float2(0,0);
		float nearPlaneBlurInPixels = blurInfo.nearPlaneMaxBlurInPixels * fragmentRadiusToUse;
		float2 ringRadiusDeltaCoords = BUFFER_PIXEL_SIZE * (nearPlaneBlurInPixels / (numberOfRings-1));
		float pointsOnRing = pointsFirstRing;
		float2 currentRingRadiusCoords = ringRadiusDeltaCoords;
		float maxLuma = dot(averageGained, lumaDotWeight) * -fragmentRadii.g * (1-HighlightThresholdNearPlane);
		float4 anamorphicFactors = CalculateAnamorphicFactor(blurInfo.texcoord - 0.5); // xy are up vector, zw are right vector
		float2x2 anamorphicRotationMatrix = CalculateAnamorphicRotationMatrix(blurInfo.texcoord);
		for(float ringIndex = 0; ringIndex < numberOfRings; ringIndex++)
		{
			float anglePerPoint = 6.28318530717958 / pointsOnRing;
			float angle = anglePerPoint;
			// no further weight needed, bleed all you want. 
			float weight = lerp(ringIndex/numberOfRings, 1, smoothstep(0, 1, bokehBusyFactorToUse));
			for(float pointNumber = 0; pointNumber < pointsOnRing; pointNumber++)
			{
				sincos(angle, pointOffset.y, pointOffset.x);
				// now transform the offset vector with the anamorphic factors and rotate it accordingly to the rotation matrix, so we get a nice
				// bending around the center of the screen.
				pointOffset = MorphPointOffsetWithAnamorphicDeltas(pointOffset, anamorphicFactors, anamorphicRotationMatrix);
				float4 tapCoords = float4(blurInfo.texcoord + (pointOffset * currentRingRadiusCoords), 0, 0);
				float4 tap = tex2Dlod(source, tapCoords);
				// r contains blurred CoC, g contains original CoC. Original can be negative
				float2 sampleRadii = tex2Dlod(SamplerCDCoCBlurred, tapCoords).rg;
				float blurredSampleRadius = sampleRadii.r;
				average.rgb += tap.rgb * weight;
				average.w += weight;
				float3 gainedTap = AccentuateWhites(tap.rgb);
				averageGained += gainedTap * weight;
				float lumaSample = dot(gainedTap, lumaDotWeight) * saturate(blurredSampleRadius) * (1-HighlightThresholdNearPlane);
				maxLuma = max(maxLuma, lumaSample);
				angle+=anglePerPoint;
			}
			pointsOnRing+=pointsFirstRing;
			currentRingRadiusCoords += ringRadiusDeltaCoords;
		}
		
		average.rgb/=(average.w + (average.w ==0));
		float alpha = saturate((min(2.5, NearPlaneMaxBlur) + 0.4) * (fragmentRadiusToUse > 0.1 ? (fragmentRadii.g <=0 ? 2 : 1) * fragmentRadiusToUse : max(fragmentRadiusToUse, -fragmentRadii.g)));
		fragment.rgb = average.rgb;
		fragment.a = alpha;
#if CD_DEBUG
		if(ShowNearPlaneAlpha)
		{
			fragment = float4(alpha, alpha, alpha, 1.0);
		}
#endif
		fragment.rgb = PostProcessBlurredFragment(fragment.rgb, maxLuma, (averageGained / (average.w + (average.w==0))), HighlightGainNearPlane);
#if CD_DEBUG
		if(ShowNearPlaneBlurred)
		{
			fragment.a = 1.0;
		}
#endif
		return fragment;
	}


	// Calculates the new RGBA fragment for a pixel at texcoord in source using a disc based blur technique described in [Jimenez2014] 
	// (Though without using tiles). Blurs far plane.
	// In:	blurInfo, the pre-calculated disc blur information from the vertex shader.
	// 		source, the source buffer to read RGBA data from. A contains luma.
	// Out: RGBA fragment that's the result of the disc-blur on the pixel at texcoord in source. A contains luma of pixel.
	float4 PerformDiscBlur(VSDISCBLURINFO blurInfo, sampler2D source)
	{
		const float3 lumaDotWeight = float3(0.3, 0.59, 0.11);
		const float pointsFirstRing = 7; 	// each ring has a multiple of this value of sample points. 
		float4 fragment = tex2Dlod(source, float4(blurInfo.texcoord, 0, 0));
		float fragmentRadius = tex2Dlod(SamplerCDCoC, float4(blurInfo.texcoord, 0, 0)).r;
		// we'll not process near plane fragments as they're processed in a separate pass. 
		if(fragmentRadius < 0 || blurInfo.farPlaneMaxBlurInPixels <=0)
		{
			// near plane fragment, will be done in near plane pass 
			return fragment;
		}
		// luma is stored in alpha
		float bokehBusyFactorToUse = saturate(1.0-BokehBusyFactor)-0.2;		// use the busy factor as an edge bias on the blur, not the highlights
		float4 average = float4(fragment.rgb * fragmentRadius * bokehBusyFactorToUse, bokehBusyFactorToUse);
		float3 averageGained = AccentuateWhites(average.rgb);
		float2 pointOffset = float2(0,0);
		float2 ringRadiusDeltaCoords =  (BUFFER_PIXEL_SIZE * blurInfo.farPlaneMaxBlurInPixels * fragmentRadius) / blurInfo.numberOfRings;
		float2 currentRingRadiusCoords = ringRadiusDeltaCoords;
		float cocPerRing = (fragmentRadius * FarPlaneMaxBlur) / blurInfo.numberOfRings;
		float pointsOnRing = pointsFirstRing;
		float maxLuma = dot(averageGained.rgb, lumaDotWeight) * fragmentRadius * (1-HighlightThresholdFarPlane);
		float4 anamorphicFactors = CalculateAnamorphicFactor(blurInfo.texcoord - 0.5); // xy are up vector, zw are right vector
		float2x2 anamorphicRotationMatrix = CalculateAnamorphicRotationMatrix(blurInfo.texcoord);
		for(float ringIndex = 0; ringIndex < blurInfo.numberOfRings; ringIndex++)
		{
			float anglePerPoint = 6.28318530717958 / pointsOnRing;
			float angle = anglePerPoint;
			float ringWeight = lerp(ringIndex/blurInfo.numberOfRings, 1, bokehBusyFactorToUse);
			float ringDistance = cocPerRing * ringIndex;
			for(float pointNumber = 0; pointNumber < pointsOnRing; pointNumber++)
			{
				sincos(angle, pointOffset.y, pointOffset.x);
				// now transform the offset vector with the anamorphic factors and rotate it accordingly to the rotation matrix, so we get a nice
				// bending around the center of the screen.
				pointOffset = MorphPointOffsetWithAnamorphicDeltas(pointOffset, anamorphicFactors, anamorphicRotationMatrix);
				float4 tapCoords = float4(blurInfo.texcoord + (pointOffset * currentRingRadiusCoords), 0, 0);
				float sampleRadius = tex2Dlod(SamplerCDCoC, tapCoords).r;
				float4 tap = tex2Dlod(source, tapCoords);
				float weight = (sampleRadius >=0) * ringWeight * CalculateSampleWeight(sampleRadius * FarPlaneMaxBlur, ringDistance);
				average.rgb += tap.rgb * weight;
				average.w += weight;
				float3 gainedTap = sampleRadius >= 0 ? AccentuateWhites(tap.rgb) : tap.rgb;
				averageGained += gainedTap * weight;
				float lumaSample = saturate(dot(gainedTap, lumaDotWeight) * sampleRadius * (1-HighlightThresholdFarPlane));
				maxLuma = sampleRadius > 0 ? max(maxLuma, lumaSample) : maxLuma;
				angle+=anglePerPoint;
			}
			pointsOnRing+=pointsFirstRing;
			currentRingRadiusCoords += ringRadiusDeltaCoords;
		}
		fragment.rgb = average.rgb / (average.w + (average.w==0));
		fragment.rgb = PostProcessBlurredFragment(fragment.rgb, maxLuma, (averageGained / (average.w + (average.w==0))), HighlightGainFarPlane);
		return fragment;
	}


	// Performs a small blur to the out of focus areas using a lower amount of rings. Additionally it calculates the luma of the fragment into alpha
	// and makes sure the fragment post-blur has the maximum luminosity from the taken samples to preserve harder edges on highlights. 
	// In:	blurInfo, the pre-calculated disc blur information from the vertex shader.
	// 		source, the source buffer to read RGBA data from
	// Out: RGBA fragment that's the result of the disc-blur on the pixel at texcoord in source. A contains luma of RGB.
	float4 PerformPreDiscBlur(VSDISCBLURINFO blurInfo, sampler2D source)
	{
		const float radiusFactor = 1.0/max(blurInfo.numberOfRings, 1);
		const float pointsFirstRing = max(blurInfo.numberOfRings-3, 2); 	// each ring has a multiple of this value of sample points. 
		float4 fragment = tex2Dlod(source, float4(blurInfo.texcoord, 0, 0));
		float signedFragmentRadius = tex2Dlod(SamplerCDCoC, float4(blurInfo.texcoord, 0, 0)).x * radiusFactor;
		float absoluteFragmentRadius = abs(signedFragmentRadius);
		bool isNearPlaneFragment = signedFragmentRadius < 0;
		float blurFactorToUse = isNearPlaneFragment ? NearPlaneMaxBlur : FarPlaneMaxBlur;
		// Substract 2 as we blur on a smaller range. Don't limit the rings based on radius here, as that will kill the pre-blur.
		float numberOfRings = max(blurInfo.numberOfRings-2, 1);
		float4 average = absoluteFragmentRadius == 0 ? fragment : float4(fragment.rgb * absoluteFragmentRadius, absoluteFragmentRadius);
		float2 pointOffset = float2(0,0);
		// pre blur blurs near plane fragments with near plane samples and far plane fragments with far plane samples [Jimenez2014].
		float2 ringRadiusDeltaCoords = BUFFER_PIXEL_SIZE 
												* ((isNearPlaneFragment ? blurInfo.nearPlaneMaxBlurInPixels : blurInfo.farPlaneMaxBlurInPixels) *  absoluteFragmentRadius) 
												* rcp((numberOfRings-1) + (numberOfRings==1));
		float pointsOnRing = pointsFirstRing;
		float2 currentRingRadiusCoords = ringRadiusDeltaCoords;
		float cocPerRing = (signedFragmentRadius * blurFactorToUse) / numberOfRings;
		for(float ringIndex = 0; ringIndex < numberOfRings; ringIndex++)
		{
			float anglePerPoint = 6.28318530717958 / pointsOnRing;
			float angle = anglePerPoint;
			float ringDistance = cocPerRing * ringIndex;
			for(float pointNumber = 0; pointNumber < pointsOnRing; pointNumber++)
			{
				sincos(angle, pointOffset.y, pointOffset.x);
				float4 tapCoords = float4(blurInfo.texcoord + (pointOffset * currentRingRadiusCoords), 0, 0);
				float signedSampleRadius = tex2Dlod(SamplerCDCoC, tapCoords).x * radiusFactor;
				float absoluteSampleRadius = abs(signedSampleRadius);
				float isSamePlaneAsFragment = ((signedSampleRadius > 0 && !isNearPlaneFragment) || (signedSampleRadius <= 0 && isNearPlaneFragment));
				float weight = CalculateSampleWeight(absoluteSampleRadius * blurFactorToUse, ringDistance) * isSamePlaneAsFragment * 
								(absoluteFragmentRadius - absoluteSampleRadius < 0.001);
				average.rgb += (tex2Dlod(source, tapCoords).rgb) * weight;
				average.w += weight;
				angle+=anglePerPoint;
			}
			pointsOnRing+=pointsFirstRing;
			currentRingRadiusCoords += ringRadiusDeltaCoords;
		}
		fragment.rgb = average.rgb/(average.w + (average.w==0));
		// store luma of new rgb in alpha so we don't need to calculate it again.
		fragment.a = dot(fragment.rgb, float3(0.3, 0.59, 0.11));
		return fragment;
	}

	
	// Function to obtain the blur disc radius from the source sampler specified and optionally flatten it to zero. Used to blur the blur disc radii using a 
	// separated gaussian blur function.
	// In:	source, the source to read the blur disc radius value to process from
	//		texcoord, the coordinate of the pixel which blur disc radius value we have to process
	//		flattenToZero, flag which if true will make this function convert a blur disc radius value bigger than 0 to 0. 
	//		Radii bigger than 0 are in the far plane and we only want near plane radii in our blurred buffer.
	// Out: processed blur disc radius for the pixel at texcoord in source.
	float GetBlurDiscRadiusFromSource(sampler2D source, float2 texcoord, bool flattenToZero)
	{
		float coc = tex2Dlod(source, float4(texcoord, 0, 0)).r;
		// we're only interested in negative coc's (near plane). All coc's in focus/far plane are flattened to 0. Return the
		// absolute value of the coc as we're working with positive blurred CoCs (as the sign is no longer needed)
		return (flattenToZero && coc >= 0) ? 0 : abs(coc);
	}

	// Performs a single value gaussian blur pass in 1 direction (18 taps). Based on Ioxa's Gaussian blur shader. Used for near plane CoC blur.
	// Used on tiles so not expensive.
	// In:	source, the source sampler to read blur disc radius values to blur from
	//		texcoord, the coordinate of the pixel to blur the blur disc radius for
	// 		offsetWeight, a weight to multiple the coordinate with, containing typically the x or y value of the pixel size
	//		flattenToZero, a flag to pass on to the actual blur disc radius read function to make sure in this pass the positive values are squashed to 0.
	// 					   This flag is needed as the gaussian blur is used separably here so the second pass should not look for positive blur disc radii
	//					   as all values are already positive (due to the first pass).
	// Out: the blurred value for the blur disc radius of the pixel at texcoord. Greater than 0 if the original CoC is in the near plane, 0 otherwise.
	float PerformSingleValueGaussianBlur(sampler2D source, float2 texcoord, float2 offsetWeight, bool flattenToZero)
	{
		float offset[18] = { 0.0, 1.4953705027, 3.4891992113, 5.4830312105, 7.4768683759, 9.4707125766, 11.4645656736, 13.4584295168, 15.4523059431, 17.4461967743, 19.4661974725, 21.4627427973, 23.4592916956, 25.455844494, 27.4524015179, 29.4489630909, 31.445529535, 33.4421011704 };
		float weight[18] = { 0.033245, 0.0659162217, 0.0636705814, 0.0598194658, 0.0546642566, 0.0485871646, 0.0420045997, 0.0353207015, 0.0288880982, 0.0229808311, 0.0177815511, 0.013382297, 0.0097960001, 0.0069746748, 0.0048301008, 0.0032534598, 0.0021315311, 0.0013582974 };

		float coc = GetBlurDiscRadiusFromSource(source, texcoord, flattenToZero);
		coc *= weight[0];
		
		float2 factorToUse = offsetWeight * NearPlaneMaxBlur * 0.8;
		for(int i = 1; i < 18; ++i)
		{
			float2 coordOffset = factorToUse * offset[i];
			float weightSample = weight[i];
			coc += GetBlurDiscRadiusFromSource(source, texcoord + coordOffset, flattenToZero) * weightSample;
			coc += GetBlurDiscRadiusFromSource(source, texcoord - coordOffset, flattenToZero) * weightSample;
		}
		
		return saturate(coc);
	}

	// Performs a full fragment (RGBA) gaussian blur pass in 1 direction (16 taps). Based on Ioxa's Gaussian blur shader.
	// Will skip any pixels which are in-focus. It will also apply the pixel's blur disc radius to further limit the blur range for near-focused pixels.
	// In:	source, the source sampler to read RGBA values to blur from
	//		texcoord, the coordinate of the pixel to blur. 
	// 		offsetWeight, a weight to multiple the coordinate with, containing typically the x or y value of the pixel size
	// Out: the blurred fragment(RGBA) for the pixel at texcoord. 
	float4 PerformFullFragmentGaussianBlur(sampler2D source, float2 texcoord, float2 offsetWeight)
	{
		float offset[6] = { 0.0, 1.4584295168, 3.40398480678, 5.3518057801, 7.302940716, 9.2581597095 };
		float weight[6] = { 0.13298, 0.23227575, 0.1353261595, 0.0511557427, 0.01253922, 0.0019913644 };
		const float3 lumaDotWeight = float3(0.3, 0.59, 0.11);
		
		float coc = tex2Dlod(SamplerCDCoC, float4(texcoord, 0, 0)).r;
		float4 fragment = tex2Dlod(source, float4(texcoord, 0, 0));
		float fragmentLuma = dot(fragment.rgb, lumaDotWeight);
		float4 originalFragment = fragment;
		float absoluteCoC = abs(coc);
		float lengthPixelSize = length(BUFFER_PIXEL_SIZE);
		if(absoluteCoC < 0.2 || PostBlurSmoothing < 0.01 || fragmentLuma < 0.3)
		{
			// in focus or postblur smoothing isn't enabled or not really a highlight, ignore
			return fragment;
		}
		fragment.rgb *= weight[0];
		float2 factorToUse = offsetWeight * PostBlurSmoothing;
		for(int i = 1; i < 6; ++i)
		{
			float2 coordOffset = factorToUse * offset[i];
			float weightSample = weight[i];
			float sampleCoC = tex2Dlod(SamplerCDCoC, float4(texcoord + coordOffset, 0, 0)).r;
			float maskFactor = abs(sampleCoC) < 0.2;		// mask factor to avoid near/in focus bleed.
			fragment.rgb += (originalFragment.rgb * maskFactor * weightSample) + 
							(tex2Dlod(source, float4(texcoord + coordOffset, 0, 0)).rgb * (1-maskFactor) * weightSample);
			sampleCoC = tex2Dlod(SamplerCDCoC, float4(texcoord - coordOffset, 0, 0)).r;
			maskFactor = abs(sampleCoC) < 0.2;
			fragment.rgb += (originalFragment.rgb * maskFactor * weightSample) + 
							(tex2Dlod(source, float4(texcoord - coordOffset, 0, 0)).rgb * (1-maskFactor) * weightSample);
		}
		return saturate(fragment);
	}

	// Functions which fills the passed in struct with focus data. This code is factored out to be able to call it either from a vertex shader
	// (in d3d10+) or from a pixel shader (d3d9) to work around compilation issues in reshade.
	void FillFocusInfoData(inout VSFOCUSINFO toFill)
	{
		// Reshade depth buffer ranges from 0.0->1.0, where 1.0 is 1000 in world units. All camera element sizes are in mm, so we state 1 in world units is 
		// 1 meter. This means to calculate from the linearized depth buffer value to meter we have to multiply by 1000.
		// Manual focus value is already in meter (well, sort of. This differs per game so we silently assume it's meter), so we first divide it by
		// 1000 to make it equal to a depth value read from the depth linearized depth buffer.
		// Read from sampler on current focus which is a 1x1 texture filled with the actual depth value of the focus point to use.
		toFill.focusDepth = tex2Dlod(SamplerCDCurrentFocus, float4(0, 0, 0, 0)).r;
		toFill.focusDepthInM = toFill.focusDepth * 1000.0; 		// km to m
		toFill.focusDepthInMM = toFill.focusDepthInM * 1000.0; 	// m to mm
		toFill.pixelSizeLength = length(BUFFER_PIXEL_SIZE);
		
		// HyperFocal calculation, see https://photo.stackexchange.com/a/33898. Useful to calculate the edges of the depth of field area
		float hyperFocal = (FocalLength * FocalLength) / (FNumber * SENSOR_SIZE);
		float hyperFocalFocusDepthFocus = (hyperFocal * toFill.focusDepthInMM);
		toFill.nearPlaneInMM = hyperFocalFocusDepthFocus / (hyperFocal + (toFill.focusDepthInMM - FocalLength));	// in mm
		toFill.farPlaneInMM = hyperFocalFocusDepthFocus / (hyperFocal - (toFill.focusDepthInMM - FocalLength));		// in mm
	}

	//////////////////////////////////////////////////
	//
	// Vertex Shaders
	//
	//////////////////////////////////////////////////

	
	// Vertex shader which is used to calculate per-frame static focus info so it's not done per pixel, but only per vertex. 
	VSFOCUSINFO VS_Focus(in uint id : SV_VertexID)
	{
		VSFOCUSINFO focusInfo;
		
		focusInfo.texcoord.x = (id == 2) ? 2.0 : 0.0;
		focusInfo.texcoord.y = (id == 1) ? 2.0 : 0.0;
		focusInfo.vpos = float4(focusInfo.texcoord * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);

#if __RENDERER__ <= 0x9300 	// doing focusing in vertex shaders in dx9 doesn't work for auto-focus, so we'll just do it in the pixel shader instead
		// fill in dummies, will be filled in pixel shader. Less fast but it is what it is...
		focusInfo.focusDepth = 0;
		focusInfo.focusDepthInM = 0;
		focusInfo.focusDepthInMM = 0;
		focusInfo.pixelSizeLength = 0;
		focusInfo.nearPlaneInMM = 0;
		focusInfo.farPlaneInMM = 0;
#else
		FillFocusInfoData(focusInfo);
#endif
		return focusInfo;
	}

	// Vertex shader which is used to calculate per-frame static info for the disc blur passes so it's not done per pixel, but only per vertex. 
	VSDISCBLURINFO VS_DiscBlur(in uint id : SV_VertexID)
	{
		VSDISCBLURINFO blurInfo;

		blurInfo.texcoord.x = (id == 2) ? 2.0 : 0.0;
		blurInfo.texcoord.y = (id == 1) ? 2.0 : 0.0;
		blurInfo.vpos = float4(blurInfo.texcoord * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
		
		blurInfo.numberOfRings = round(BlurQuality);
		float pixelSizeLength = length(BUFFER_PIXEL_SIZE);
		blurInfo.farPlaneMaxBlurInPixels = (FarPlaneMaxBlur / 100.0) / pixelSizeLength;
		blurInfo.nearPlaneMaxBlurInPixels = (NearPlaneMaxBlur / 100.0) / pixelSizeLength;
		blurInfo.cocFactorPerPixel = length(BUFFER_PIXEL_SIZE) * blurInfo.farPlaneMaxBlurInPixels;	// not needed for near plane.
		return blurInfo;
	}

	//////////////////////////////////////////////////
	//
	// Pixel Shaders
	//
	//////////////////////////////////////////////////

	// Pixel shader which determines the focus depth for the current frame, which will be stored in the currentfocus texture.
	void PS_DetermineCurrentFocus(float4 vpos : SV_Position, float2 texcoord : TEXCOORD, out float fragment : SV_Target0)
	{
#if __RESHADE_FXC__		// Freestyle
		float manualFocusPlaneValue = ManualFocusPlane * ManualFocusPlaneMaxRange;
		fragment = UseAutoFocus ? lerp(tex2D(SamplerCDPreviousFocus, float2(0, 0)).r, ReShade::GetLinearizedDepth(AutoFocusPoint), AutoFocusTransitionSpeed) 
								: (manualFocusPlaneValue / 1000);
#else
		float2 autoFocusPointToUse = UseMouseDrivenAutoFocus ? MouseCoords * BUFFER_PIXEL_SIZE : AutoFocusPoint;
		fragment = UseAutoFocus ? lerp(tex2D(SamplerCDPreviousFocus, float2(0, 0)).r, ReShade::GetLinearizedDepth(autoFocusPointToUse), AutoFocusTransitionSpeed) 
								: (ManualFocusPlane / 1000);
#endif
	}
	
	// Pixel shader which copies the single value of the current focus texture to the previous focus texture so it's preserved for the next frame.
	void PS_CopyCurrentFocus(float4 vpos : SV_Position, float2 texcoord : TEXCOORD, out float fragment : SV_Target0)
	{
		fragment = tex2D(SamplerCDCurrentFocus, float2(0, 0)).r;
	}
	
	// Pixel shader which produces a blur disc radius for each pixel and returns the calculated value. 
	void PS_CalculateCoCValues(VSFOCUSINFO focusInfo, out float fragment : SV_Target0)
	{
#if __RENDERER__ <= 0x9300 	// doing focusing in vertex shaders in dx9 doesn't work for auto-focus, so we'll just do it in the pixel shader instead
		FillFocusInfoData(focusInfo);
#endif
		fragment = CalculateBlurDiscSize(focusInfo);
	}
	
	// Pixel shader which will perform a pre-blur on the frame buffer using a blur disc smaller than the original blur disc of the pixel. 
	// This is done to overcome the undersampling gaps we have in the main blur disc sampler [Jimenez2014].
	void PS_PreBlur(VSDISCBLURINFO blurInfo, out float4 fragment : SV_Target0)
	{
		fragment = PerformPreDiscBlur(blurInfo, ReShade::BackBuffer);
	}

	// Pixel shader which performs the far plane blur pass.
	void PS_BokehBlur(VSDISCBLURINFO blurInfo, out float4 fragment : SV_Target0)
	{
		fragment = PerformDiscBlur(blurInfo, SamplerCDBuffer1);
	}

	// Pixel shader which performs the near plane blur pass. Uses a blurred buffer of blur disc radii, based on a combination of [Jimenez2014] (tiles)
	// and [Nilsson2012] (blurred CoC).
	void PS_NearBokehBlur(VSDISCBLURINFO blurInfo, out float4 fragment : SV_Target0)
	{
		fragment = PerformNearPlaneDiscBlur(blurInfo, ReShade::BackBuffer);//SamplerCDBuffer2);
	}
	
	// Pixel shader which performs the CoC tile creation (horizontal gather of min CoC)
	void PS_CoCTile1(float4 vpos : SV_Position, float2 texcoord : TEXCOORD, out float fragment : SV_Target0)
	{
		fragment = PerformTileGatherHorizontal(SamplerCDCoC, texcoord);
	}

	// Pixel shader which performs the CoC tile creation (vertical gather of min CoC)
	void PS_CoCTile2(float4 vpos : SV_Position, float2 texcoord : TEXCOORD, out float fragment : SV_Target0)
	{
		fragment = PerformTileGatherVertical(SamplerCDCoCTileTmp, texcoord);
	}
	
	// Pixel shader which performs the CoC tile creation with neighbor tile info (horizontal and vertical gather of min CoC)
	void PS_CoCTileNeighbor(float4 vpos : SV_Position, float2 texcoord : TEXCOORD, out float fragment : SV_Target0)
	{
		fragment = PerformNeighborTileGather(SamplerCDCoCTile, texcoord);
	}
	
	// Pixel shader which performs the first part of the gaussian blur on the blur disc values
	void PS_CoCGaussian1(float4 vpos : SV_Position, float2 texcoord : TEXCOORD, out float fragment : SV_Target0)
	{
		// from source CoC to tmp1
		fragment = PerformSingleValueGaussianBlur(SamplerCDCoCTileNeighbor, texcoord, 
												  float2(BUFFER_PIXEL_SIZE.x * (BUFFER_SCREEN_SIZE.x/GROUND_TRUTH_SCREEN_WIDTH), 0.0), true);
	}

	// Pixel shader which performs the second part of the gaussian blur on the blur disc values
	void PS_CoCGaussian2(float4 vpos : SV_Position, float2 texcoord : TEXCOORD, out float2 fragment : SV_Target0)
	{
		// from tmp1 to tmp2. Merge original CoC into g.
		fragment = float2(PerformSingleValueGaussianBlur(SamplerCDCoCTmp1, texcoord, 
														 float2(0.0, BUFFER_PIXEL_SIZE.y * (BUFFER_SCREEN_SIZE.y/GROUND_TRUTH_SCREEN_HEIGHT)), false), 
						  tex2D(SamplerCDCoCTileNeighbor, texcoord).x);
	}
	
	// Pixel shader which combines 2 half-res sources to a full res output. From texCDBuffer1 & 2 to texCDBuffer4.
	void PS_Combiner(float4 vpos : SV_Position, float2 texcoord : TEXCOORD, out float4 fragment : SV_Target0)
	{
		// first blend far plane with original buffer, then near plane on top of that. 
		float4 originalFragment = tex2D(ReShade::BackBuffer, texcoord);
		float4 farFragment = tex2D(SamplerCDBuffer3, texcoord);
		float4 nearFragment = tex2D(SamplerCDBuffer1, texcoord);
		// multiply with far plane max blur so if we need to have 0 blur we get full res 
		float realCoC = tex2D(SamplerCDCoC, texcoord).r * clamp(0, 1, FarPlaneMaxBlur);
		// all CoC's > 0.1 are full far fragment, below that, we're going to blend. This avoids shimmering far plane without the need of a 
		// 'magic' number to boost up the alpha.
		float blendFactor = (realCoC > 0.1) ? 1 : smoothstep(0, 1, (realCoC / 0.1));
		fragment = lerp(originalFragment, farFragment, blendFactor);
		fragment.rgb = lerp(fragment.rgb, nearFragment.rgb, nearFragment.a * (NearPlaneMaxBlur != 0));
#if CD_DEBUG
		if(ShowOnlyFarPlaneBlurred)
		{
			fragment = farFragment;
		}
#endif
		fragment.a = 1.0;
	}

	// Pixel shader which performs a 9 tap tentfilter on the far plane blur result. This tent filter is from the composition pass 
	// in KinoBokeh: https://github.com/keijiro/KinoBokeh/blob/master/Assets/Kino/Bokeh/Shader/Composition.cginc
	// See also [Jimenez2014] for a discussion about this filter.
	void PS_TentFilter(float4 vpos : SV_Position, float2 texcoord : TEXCOORD, out float4 fragment : SV_Target0)
	{
		float4 coord = BUFFER_PIXEL_SIZE.xyxy * float4(1, 1, -1, 0);
		float4 average;
		average = tex2D(SamplerCDBuffer2, texcoord - coord.xy);
		average += tex2D(SamplerCDBuffer2, texcoord - coord.wy) * 2;
		average += tex2D(SamplerCDBuffer2, texcoord - coord.zy);
		average += tex2D(SamplerCDBuffer2, texcoord + coord.zw) * 2;
		average += tex2D(SamplerCDBuffer2, texcoord) * 4;
		average += tex2D(SamplerCDBuffer2, texcoord + coord.xw) * 2;
		average += tex2D(SamplerCDBuffer2, texcoord + coord.zy);
		average += tex2D(SamplerCDBuffer2, texcoord + coord.wy) * 2;
		average += tex2D(SamplerCDBuffer2, texcoord + coord.xy);
		fragment = average / 16;
	}
	
	// Pixel shader which performs the first part of the gaussian post-blur smoothing pass, to iron out undersampling issues with the disc blur
	void PS_PostSmoothing1(float4 vpos : SV_Position, float2 texcoord : TEXCOORD, out float4 fragment : SV_Target0)
	{
		fragment = PerformFullFragmentGaussianBlur(SamplerCDBuffer4, texcoord, float2(BUFFER_PIXEL_SIZE.x, 0.0));
	}

	// Pixel shader which performs the second part of the gaussian post-blur smoothing pass, to iron out undersampling issues with the disc blur
	// It also displays the focusing overlay helpers if the mouse button is down and the user enabled ShowOutOfFocusPlaneOnMouseDown.
	// it displays the near and far plane at the hyperfocal planes (calculated in vertex shader) with the overlay color and the in-focus area in between
	// as normal. It then also blends the focus plane as a separate color to make focusing really easy. 
	void PS_PostSmoothing2AndFocusing(in VSFOCUSINFO focusInfo, in float2 texcoord: TEXCOORD, out float4 fragment : SV_Target0)
	{
		if(ShowCoCValues)
		{
			fragment = GetDebugFragment(tex2D(SamplerCDCoC, focusInfo.texcoord).r, true);
			return;
		}
#if CD_DEBUG
		if(ShowNearCoCTilesBlurredG)
		{
			fragment = GetDebugFragment(tex2D(SamplerCDCoCBlurred, focusInfo.texcoord).g, false);
			return;
		}
		if(ShowNearCoCTiles)
		{
			fragment = GetDebugFragment(tex2D(SamplerCDCoCTile, focusInfo.texcoord).r, true);
			return;
		}
		if(ShowNearCoCTilesBlurredR)
		{
			fragment = GetDebugFragment(tex2D(SamplerCDCoCBlurred, focusInfo.texcoord).r, true);
			return;
		}
		if(ShowNearCoCTilesNeighbor)
		{
			fragment = GetDebugFragment(tex2D(SamplerCDCoCTileNeighbor, focusInfo.texcoord).r, true);
			return;
		}
#endif
		fragment = PerformFullFragmentGaussianBlur(SamplerCDBuffer5, focusInfo.texcoord, float2(0.0, BUFFER_PIXEL_SIZE.y));
		float4 originalFragment = tex2D(SamplerCDBuffer4, focusInfo.texcoord);
		// Dither
		float2 uv = float2(BUFFER_WIDTH, BUFFER_HEIGHT) / float2( 512.0f, 512.0f ); // create multiplier on texcoord so that we can use 1px size reads on gaussian noise texture (since much smaller than screen)
		uv.xy = uv.xy * texcoord.xy;
		float noise = tex2D(SamplerCDNoise, uv).x; // read, uv is scaled, sampler is set to tile noise texture (WRAP)
		fragment.xyz = saturate(fragment.xyz + lerp( -0.5/255.0, 0.5/255.0, noise)); // apply dither
		// End Dither
		float coc = abs(tex2Dlod(SamplerCDCoC, float4(focusInfo.texcoord, 0, 0)).r);
		fragment.rgb = lerp(originalFragment.rgb, fragment.rgb, saturate(coc < length(BUFFER_PIXEL_SIZE) ? 0 : 4 * coc));
		fragment.w = 1.0;
		
#if __RENDERER__ <= 0x9300 	// doing focusing in vertex shaders in dx9 doesn't work for auto-focus, so we'll just do it in the pixel shader instead
		FillFocusInfoData(focusInfo);
#endif
#if __RESHADE_FXC__		// Freestyle
		bool ShowOutOfFocusPlaneOnMouseDown = ShowFocusingOverlay;
		bool LeftMouseDown = true;
		float3 OutOfFocusPlaneColor = float3(0.8, 0.8, 0.8);
		float3 FocusPlaneColor = float3(0.0, 0.0, 1.0);
		float OutOfFocusPlaneColorTransparency = OUT_OF_FOCUS_PLANE_COLORTRANSPARENCY;
		float4 FocusCrosshairColor = float4(1.0, 0.0, 1.0, 1.0);
#endif
		if(ShowOutOfFocusPlaneOnMouseDown && LeftMouseDown)
		{
			float depthPixelInMM = ReShade::GetLinearizedDepth(focusInfo.texcoord) * 1000.0 * 1000.0;
			float4 colorToBlend = fragment;
			if(depthPixelInMM < focusInfo.nearPlaneInMM || (focusInfo.farPlaneInMM > 0 && depthPixelInMM > focusInfo.farPlaneInMM))
			{
				colorToBlend = float4(OutOfFocusPlaneColor, 1.0);
			}
			else
			{
				if(abs(coc) < focusInfo.pixelSizeLength)
				{
					colorToBlend = float4(FocusPlaneColor, 1.0);
				}
			}
			fragment = lerp(fragment, colorToBlend, OutOfFocusPlaneColorTransparency);
			if(UseAutoFocus)
			{
#if __RESHADE_FXC__		// Freestyle
				float2 focusPointCoords = AutoFocusPoint;
#else
				float2 focusPointCoords = UseMouseDrivenAutoFocus ? MouseCoords * BUFFER_PIXEL_SIZE : AutoFocusPoint;
#endif
				fragment = lerp(fragment, FocusCrosshairColor, FocusCrosshairColor.w * saturate(exp(-BUFFER_WIDTH * length(focusInfo.texcoord - float2(focusPointCoords.x, focusInfo.texcoord.y)))));
				fragment = lerp(fragment, FocusCrosshairColor, FocusCrosshairColor.w * saturate(exp(-BUFFER_HEIGHT * length(focusInfo.texcoord - float2(focusInfo.texcoord.x, focusPointCoords.y)))));
			}
		}
	}

	//////////////////////////////////////////////////
	//
	// Techniques
	//
	//////////////////////////////////////////////////

	technique CinematicDOF
#if __RESHADE__ >= 40000
	< ui_tooltip = "Cinematic Depth of Field "
			CINEMATIC_DOF_VERSION
			"\n===========================================\n\n"
			"Cinematic Depth of Field is a state of the art depth of field shader,\n"
			"offering fine-grained control over focusing, blur aspects and highlights,\n"
			"using real-life lens aspects. Cinematic Depth of Field is based on\n"
			"various papers, of which the references are included in the source code.\n\n"
			"Cinematic Depth of Field was written by Frans 'Otis_Inf' Bouma and is part of OtisFX\n"
			"https://fransbouma.com | https://github.com/FransBouma/OtisFX"; >
#endif
	{
		pass DetermineCurrentFocus { VertexShader = PostProcessVS; PixelShader = PS_DetermineCurrentFocus; RenderTarget = texCDCurrentFocus; }
		pass CopyCurrentFocus { VertexShader = PostProcessVS; PixelShader = PS_CopyCurrentFocus; RenderTarget = texCDPreviousFocus; }
		pass CalculateCoC { VertexShader = VS_Focus; PixelShader = PS_CalculateCoCValues; RenderTarget = texCDCoC; }
		pass CoCTile1 { VertexShader = PostProcessVS; PixelShader = PS_CoCTile1; RenderTarget = texCDCoCTileTmp; }
		pass CoCTile2 { VertexShader = PostProcessVS; PixelShader = PS_CoCTile2; RenderTarget = texCDCoCTile; }
		pass CoCTileNeighbor { VertexShader = PostProcessVS; PixelShader = PS_CoCTileNeighbor; RenderTarget = texCDCoCTileNeighbor; }
		pass CoCBlur1 { VertexShader = PostProcessVS; PixelShader = PS_CoCGaussian1; RenderTarget = texCDCoCTmp1; }
		pass CoCBlur2 { VertexShader = PostProcessVS; PixelShader = PS_CoCGaussian2; RenderTarget = texCDCoCBlurred; }
		pass PreBlur { VertexShader = VS_DiscBlur; PixelShader = PS_PreBlur; RenderTarget = texCDBuffer1; }
		pass BokehBlur { VertexShader = VS_DiscBlur; PixelShader = PS_BokehBlur; RenderTarget = texCDBuffer2; }
		pass NearBokehBlur { VertexShader = VS_DiscBlur; PixelShader = PS_NearBokehBlur; RenderTarget = texCDBuffer1; }
		pass TentFilter { VertexShader = PostProcessVS; PixelShader = PS_TentFilter; RenderTarget = texCDBuffer3; }
		pass Combiner { VertexShader = PostProcessVS; PixelShader = PS_Combiner; RenderTarget = texCDBuffer4; }
		pass PostSmoothing1 { VertexShader = PostProcessVS; PixelShader = PS_PostSmoothing1; RenderTarget = texCDBuffer5; }
		pass PostSmoothing2AndFocusing { VertexShader = VS_Focus; PixelShader = PS_PostSmoothing2AndFocusing;}
	}
}