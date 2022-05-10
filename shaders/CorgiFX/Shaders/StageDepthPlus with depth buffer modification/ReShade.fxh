#pragma once

#if !defined(__RESHADE__) || __RESHADE__ < 30000
	#error "ReShade 3.0+ is required to use this header file"
#endif

#ifndef RESHADE_DEPTH_INPUT_IS_UPSIDE_DOWN
	#define RESHADE_DEPTH_INPUT_IS_UPSIDE_DOWN 0
#endif
#ifndef RESHADE_DEPTH_INPUT_IS_REVERSED
	#define RESHADE_DEPTH_INPUT_IS_REVERSED 1
#endif
#ifndef RESHADE_DEPTH_INPUT_IS_LOGARITHMIC
	#define RESHADE_DEPTH_INPUT_IS_LOGARITHMIC 0
#endif

#ifndef RESHADE_DEPTH_MULTIPLIER
	#define RESHADE_DEPTH_MULTIPLIER 1
#endif
#ifndef RESHADE_DEPTH_LINEARIZATION_FAR_PLANE
	#define RESHADE_DEPTH_LINEARIZATION_FAR_PLANE 1000.0
#endif

// Above 1 expands coordinates, below 1 contracts and 1 is equal to no scaling on any axis
#ifndef RESHADE_DEPTH_INPUT_Y_SCALE
	#define RESHADE_DEPTH_INPUT_Y_SCALE 1
#endif
#ifndef RESHADE_DEPTH_INPUT_X_SCALE
	#define RESHADE_DEPTH_INPUT_X_SCALE 1
#endif
// An offset to add to the Y coordinate, (+) = move up, (-) = move down
#ifndef RESHADE_DEPTH_INPUT_Y_OFFSET
	#define RESHADE_DEPTH_INPUT_Y_OFFSET 0
#endif
#ifndef RESHADE_DEPTH_INPUT_Y_PIXEL_OFFSET
	#define RESHADE_DEPTH_INPUT_Y_PIXEL_OFFSET 0
#endif
// An offset to add to the X coordinate, (+) = move right, (-) = move left
#ifndef RESHADE_DEPTH_INPUT_X_OFFSET
	#define RESHADE_DEPTH_INPUT_X_OFFSET 0
#endif
#ifndef RESHADE_DEPTH_INPUT_X_PIXEL_OFFSET
	#define RESHADE_DEPTH_INPUT_X_PIXEL_OFFSET 0
#endif

#define BUFFER_PIXEL_SIZE float2(BUFFER_RCP_WIDTH, BUFFER_RCP_HEIGHT)
#define BUFFER_SCREEN_SIZE float2(BUFFER_WIDTH, BUFFER_HEIGHT)
#define BUFFER_ASPECT_RATIO (BUFFER_WIDTH * BUFFER_RCP_HEIGHT)

namespace ReShade
{
#if defined(__RESHADE_FXC__)
	float GetAspectRatio() { return BUFFER_WIDTH * BUFFER_RCP_HEIGHT; }
	float2 GetPixelSize() { return float2(BUFFER_RCP_WIDTH, BUFFER_RCP_HEIGHT); }
	float2 GetScreenSize() { return float2(BUFFER_WIDTH, BUFFER_HEIGHT); }
	#define AspectRatio GetAspectRatio()
	#define PixelSize GetPixelSize()
	#define ScreenSize GetScreenSize()
#else
	// These are deprecated and will be removed eventually.
	static const float AspectRatio = BUFFER_WIDTH * BUFFER_RCP_HEIGHT;
	static const float2 PixelSize = float2(BUFFER_RCP_WIDTH, BUFFER_RCP_HEIGHT);
	static const float2 ScreenSize = float2(BUFFER_WIDTH, BUFFER_HEIGHT);
#endif

	// Global textures and samplers
	texture BackBufferTex : COLOR;
	texture DepthBufferTex : DEPTH;

	sampler BackBuffer { Texture = BackBufferTex; };
	sampler DepthBuffer { Texture = DepthBufferTex; };



#if RESHADE_MIX_STAGE_DEPTH_PLUS_MAP

	#ifndef STAGE_TEXTURE_WIDTH
	#define STAGE_TEXTURE_WIDTH BUFFER_WIDTH
	#endif

	#ifndef STAGE_TEXTURE_HEIGHT
	#define STAGE_TEXTURE_HEIGHT BUFFER_HEIGHT
	#endif

	#ifndef RESHADE_MIX_STAGE_DEPTH_PLUS_MAP
    #define RESHADE_MIX_STAGE_DEPTH_PLUS_MAP 0
	#endif

	#ifndef StageDepthTex
	#define StageDepthTex "StageDepthmap.png"//Remplace the original file or change the preprocessor definition inside reshade
	#endif

	#ifndef StageMaskTex
	#define StageMaskTex "StageMask.png"//Remplace the original file or change the preprocessor definition inside reshade
	#endif

	texture RSDepthMap_Tex <
	    source = StageDepthTex;
	> {
	    Format = RGBA8;
	    Width  = STAGE_TEXTURE_WIDTH;
	    Height = STAGE_TEXTURE_HEIGHT;
	};

	sampler RSDepthMap_sampler
	{
	    Texture  = RSDepthMap_Tex;
	    AddressU = BORDER;
	    AddressV = BORDER;
	};

	texture RSMask_Tex <
	    source = StageMaskTex;
	> {
	    Format = RGBA8;
	    Width  = STAGE_TEXTURE_WIDTH;
	    Height = STAGE_TEXTURE_HEIGHT;
	};

	sampler RSMask_sampler
	{
	    Texture  = RSMask_Tex;
	    AddressU = BORDER;
	    AddressV = BORDER;
	};


    uniform float2 RSLayer_Pos <
		ui_text = "----------------------------------------------------------------------\nThe options below show up in every shader because you turned on RESHADE_MIX_STAGE_DEPTH_PLUS_MAP. If you are not using a depthmap image to be mixed with the game depthbuffer turn it off. \n \nIf you are using it make sure to configure the image controls with the same as the controls in StageDepthPlus so the depth buffer matches the image.";
		ui_category = "DepthMapImage configs";
      	ui_type = "slider";
    	ui_label = "Light direction";
    	ui_step = 0.001;
    	ui_min = -1.5; ui_max = 1.5;
    > = (0,0);	

    uniform float2 RSLayer_Scale <
    	ui_category = "DepthMapImage configs";
      	ui_type = "slider";
    	ui_label = "Size";
    	ui_step = 0.01;
    	ui_min = 0.01; ui_max = 5.0;
    > = (1.001,1.001);

    uniform float RSAxis <
    	ui_category = "DepthMapImage configs";
    	ui_type = "slider";
    	ui_label = "Angle";
    	ui_step = 0.1;
    	ui_min = -180.0; ui_max = 180.0;
    > = 0.0;

    uniform bool RSFlipH < 
    	ui_label = "Flip Horizontal";
    	ui_category = "DepthMapImage configs";
    > = false;

    uniform bool RSFlipV < 
    	ui_label = "Flip Vertical";
    	ui_category = "DepthMapImage configs";
    > = false;

    uniform float RSStage_depth <
    	ui_category = "DepthMapImage configs";
		ui_type = "slider";
		ui_min = 0.0; ui_max = 1.0;
		ui_label = "Depth";
	> = 0;

	uniform bool RSUseMask <
		ui_category = "DepthMapImage configs";
		ui_label = "Use a mask image";
		ui_category = "Controls";
	> = false;
#else
#endif




#include "StageDepthPlusMap.fxh"

#if RESHADE_MIX_STAGE_DEPTH_PLUS_MAP

		float GetLinearizedDepth(float2 texcoord)
		{
			
			float2 depthmapcoord = TextureCoordsModifier(texcoord, RSLayer_Pos, RSLayer_Scale, RSAxis, RSFlipH, RSFlipV);

			float mask = (RSUseMask) ? tex2D(RSMask_sampler, depthmapcoord).r : 1;

			float depthimage=tex2D(RSDepthMap_sampler,depthmapcoord).x * RESHADE_DEPTH_MULTIPLIER;//getValuesImageDepthMap(texcoord)).x);

	#if RESHADE_DEPTH_INPUT_IS_UPSIDE_DOWN
			texcoord.y = 1.0 - texcoord.y;
	#endif
			texcoord.x /= RESHADE_DEPTH_INPUT_X_SCALE;
			texcoord.y /= RESHADE_DEPTH_INPUT_Y_SCALE;
	#if RESHADE_DEPTH_INPUT_X_PIXEL_OFFSET
			texcoord.x -= RESHADE_DEPTH_INPUT_X_PIXEL_OFFSET * BUFFER_RCP_WIDTH;
	#else // Do not check RESHADE_DEPTH_INPUT_X_OFFSET, since it may be a decimal number, which the preprocessor cannot handle
			texcoord.x -= RESHADE_DEPTH_INPUT_X_OFFSET / 2.000000001;
	#endif
	#if RESHADE_DEPTH_INPUT_Y_PIXEL_OFFSET
			texcoord.y += RESHADE_DEPTH_INPUT_Y_PIXEL_OFFSET * BUFFER_RCP_HEIGHT;
	#else
			texcoord.y += RESHADE_DEPTH_INPUT_Y_OFFSET / 2.000000001;
	#endif
			float depth = tex2Dlod(DepthBuffer, float4(texcoord, 0, 0)).x * RESHADE_DEPTH_MULTIPLIER;
			

	#if RESHADE_DEPTH_INPUT_IS_LOGARITHMIC
			const float C = 0.01;
			depth = (exp(depth * log(C + 1.0)) - 1.0) / C;
	#endif
	#if RESHADE_DEPTH_INPUT_IS_REVERSED
			depth = 1.0 - depth;
	#endif
			const float N = 1.0;
			depth /= RESHADE_DEPTH_LINEARIZATION_FAR_PLANE - depth * (RESHADE_DEPTH_LINEARIZATION_FAR_PLANE - N);
        	
			depthimage /= RESHADE_DEPTH_LINEARIZATION_FAR_PLANE - depthimage * (RESHADE_DEPTH_LINEARIZATION_FAR_PLANE - N);
			depthimage=saturate(depthimage +RSStage_depth);
			
			

			

			if ((depthmapcoord.x > 0) && (depthmapcoord.x < 1) && (depthmapcoord.y > 0) && (depthmapcoord.y < 1) && (mask==1)) return min(depth,depthimage);
			else return depth;

			//return depth;
		}
	}
#else
		float GetLinearizedDepth(float2 texcoord)
		{
	#if RESHADE_DEPTH_INPUT_IS_UPSIDE_DOWN
			texcoord.y = 1.0 - texcoord.y;
	#endif
			texcoord.x /= RESHADE_DEPTH_INPUT_X_SCALE;
			texcoord.y /= RESHADE_DEPTH_INPUT_Y_SCALE;
	#if RESHADE_DEPTH_INPUT_X_PIXEL_OFFSET
			texcoord.x -= RESHADE_DEPTH_INPUT_X_PIXEL_OFFSET * BUFFER_RCP_WIDTH;
	#else // Do not check RESHADE_DEPTH_INPUT_X_OFFSET, since it may be a decimal number, which the preprocessor cannot handle
			texcoord.x -= RESHADE_DEPTH_INPUT_X_OFFSET / 2.000000001;
	#endif
	#if RESHADE_DEPTH_INPUT_Y_PIXEL_OFFSET
			texcoord.y += RESHADE_DEPTH_INPUT_Y_PIXEL_OFFSET * BUFFER_RCP_HEIGHT;
	#else
			texcoord.y += RESHADE_DEPTH_INPUT_Y_OFFSET / 2.000000001;
	#endif
			float depth = tex2Dlod(DepthBuffer, float4(texcoord, 0, 0)).x * RESHADE_DEPTH_MULTIPLIER;

	#if RESHADE_DEPTH_INPUT_IS_LOGARITHMIC
			const float C = 0.01;
			depth = (exp(depth * log(C + 1.0)) - 1.0) / C;
	#endif
	#if RESHADE_DEPTH_INPUT_IS_REVERSED
			depth = 1.0 - depth;
	#endif
			const float N = 1.0;
			depth /= RESHADE_DEPTH_LINEARIZATION_FAR_PLANE - depth * (RESHADE_DEPTH_LINEARIZATION_FAR_PLANE - N);

			return depth;
		}
	}
#endif

// Vertex shader generating a triangle covering the entire screen
void PostProcessVS(in uint id : SV_VertexID, out float4 position : SV_Position, out float2 texcoord : TEXCOORD)
{
	texcoord.x = (id == 2) ? 2.0 : 0.0;
	texcoord.y = (id == 1) ? 2.0 : 0.0;
	position = float4(texcoord * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
}
