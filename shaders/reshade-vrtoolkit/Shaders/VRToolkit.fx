//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// VR Toolkit 
//
// Combines multiple shaders usable in VR to be processed into one single pass to improve
// performance
//
// by Alexandre Miguel Maia - Retrolux
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

#include "ReShade.fxh"

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Preprocessor Settings (to hide prefix with _ or less than 10 characters)
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

/**
 * Configures the sharpening/clarity modes
 *
 * Mode:  0 - Disabled sharpening
 *        1 - Filmic Anamorph Sharpening 
 *        2 - AMD FidelityFX Contrast Adaptive Sharpening
 */
#ifndef VRT_SHARPENING_MODE
    #define VRT_SHARPENING_MODE 1 
#endif

/**
 * Configures the color correction modes
 *
 * Mode:  0 - Disabled color correction
 *        1 - Uses a LUT (Look up table) for specialized and complex corrections.
 *        2 - Simple Contrast & color saturation.
 */
#ifndef VRT_COLOR_CORRECTION_MODE
    #define VRT_COLOR_CORRECTION_MODE 0
#endif

/**
 * Masks the center of the screen with a circle to reduce pixel count that is processed by the shaders
 *
 * Mode:  0 - Disable circular masking
 *        1 - Uses circular mask to improve shader performance on games rendering on DX10 or higher
 */
#ifndef VRT_USE_CENTER_MASK
	#define VRT_USE_CENTER_MASK 1
#endif

// Only allow mask for DX10 or higher as lower version does not support dynamic branching
#if VRT_USE_CENTER_MASK && __RENDERER__ < 0xa000 
	#undef VRT_USE_CENTER_MASK
#endif

/**
 * Dithering Noise to reduce color banding on gradients 
 *
 * Mode:  0 - Disable dithering
 *        1 - Enable dithering that adds noise to the image to smoothen out gradients
 */
#ifndef VRT_DITHERING
	#define VRT_DITHERING 0
#endif

/**
 * Antialiasing to reduce aliasing/shimmering. This helps to further smoothen out the image after MSAA has done most of the work
 *
 * Mode:  0 - Disable antialiasing
 *        1 - Enable dithering that adds noise to the image to smoothen out gradients
 */
#ifndef VRT_ANTIALIASING_MODE
	#define VRT_ANTIALIASING_MODE 0
#endif

/**
 * Sets if the sharpening should work on the gamma corrected image to reduce artifacts.
 * This should be kept enabled and only disabled for debugging if required. 
 * Note: only needs to be applied when the backbuffer is 8 bit to ensure it is linear when fetched. 10 bit backbuffer is always linear
 */
#ifndef _VRT_LINEAR_BACKBUFFER 
    #if VRT_SHARPENING_MODE == 2 && BUFFER_COLOR_BIT_DEPTH == 8
        #define _VRT_LINEAR_BACKBUFFER 1
    #endif
#endif

/**
* Discards black pixels in the backbuffer to avoid gpu processing outside the viewble areas (usualy masked black) of the VR headset
* This might cause artifacts on dark scenes that contain black pixel when heavy color corrections are applied.
*/
#ifndef _VRT_DISCARD_BLACK
  	//#if VRT_USE_CENTER_MASK == 0 || VRT_COLOR_CORRECTION_MODE != 0
    	#define _VRT_DISCARD_BLACK 1
    //#endif
#endif

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// UI Elements
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

#include "ReShadeUI.fxh"

uniform int VRT_Advanced_help <
	ui_type = "radio"; 
	ui_category = "(?) VRToolkit Modes & Settings Help";
	ui_category_closed = true;
	ui_label = " ";
    ui_text =
    	" Open the \"Preprocess definitions\" section to change the folowing modes.\n"
    	"\n"
		" Sharpening:        0 - Disabled\n"
        "                    1 - Filmic Anamorph Sharpening\n" 
        "                    2 - CAS (Contrast Adaptive Sharpening)\n"
        "\n"
        " Color Correction:  0 - Disabled\n"
        "                    1 - LUT (Look up table)\n" 
		"                    2 - Contrast & Saturation\n"
        "\n"
        " Circular Masking:  0 - Disabled\n"
        "                    1 - Enabled to improve performance\n"
        "\n"
        " Dithering:         0 - Disabled\n"
        "                    1 - Enabled to reduce banding on gradients.\n"
     	"\n"
        " Antialiasing:      0 - Disabled\n"
        "                    1 - FXAA\n"
        "";
	     
    >;

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Variables (Uniform & Static)
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

static const float GAMMA_SRGB_FACTOR = 1/2.2;

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Textures & Sampler
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

// normal backbuffer that is not scalable but works with SGSSA 
sampler backBufferSampler {
    Texture = ReShade::BackBufferTex;
    //point filter for SGSSAA to avoid blur
	MagFilter = POINT; MinFilter = POINT;
    #if _VRT_LINEAR_BACKBUFFER 
       SRGBTexture = true;
    #endif
};

// scalable version of the backbuffer
sampler backBufferSamplerScalable {
    Texture = ReShade::BackBufferTex;
	MagFilter = LINEAR; MinFilter = LINEAR;
	#if _VRT_LINEAR_BACKBUFFER 
       SRGBTexture = true;
    #endif
};

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Shader stages
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

#include "VRToolkitShaders.fxh"

#if VRT_SHARPENING_MODE != 0
// sharpening step
float3 SharpeningStep(float4 backBuffer, float4 position , float2 texcoord ){

    // Sharpening Step
    #if (VRT_SHARPENING_MODE == 1)
        // use filmic sharpening 
        return FilmicAnamorphSharpenPS(backBuffer, position, texcoord);
    #elif (VRT_SHARPENING_MODE == 2)
        // use CAS sharpening
        return CASPass(backBuffer,position,texcoord);	
    #endif

}
#endif

#if (VRT_COLOR_CORRECTION_MODE != 0)
// color correction step
float3 ColorCorrectionStep(float4 backBuffer, float4 position , float2 texcoord ){

    #if (VRT_COLOR_CORRECTION_MODE == 1)
        // LUT shader
        return PS_LUT_Apply(backBuffer, position, texcoord);
    #elif (VRT_COLOR_CORRECTION_MODE == 2)
        // Tonemapping
        return ContrastColorPass(backBuffer, position, texcoord);
    #endif
  
}
#endif

#if VRT_ANTIALIASING_MODE != 0
// anti aliasing step
float4 AntialiasingStep(float4 position , float2 texcoord ){

    #if (VRT_ANTIALIASING_MODE == 1)
        // LUT shader
        return FXAAPixelShader(position, texcoord);
    #endif

}
#endif

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Main VRCombine Shader
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

float4 CombineVRShaderPS(in float4 position : SV_Position, in float2 texcoord : TEXCOORD) : SV_Target {

	// fetch initial unmodified back buffer but force opaque alpha
	float4 backBuffer = float4(tex2D(backBufferSampler, texcoord.xy).rgb, 1.0);

    #if _VRT_DISCARD_BLACK 
		// discards black pixels which are usually the ones masked outside the HMD visible area 
		if (!any(backBuffer.rgb))
		{
			discard;
			//return float4(1,0,1,1);
		}
    #endif
	
    #if VRT_USE_CENTER_MASK 
        float circularMask = CircularMask(texcoord);
	#endif

    #if (VRT_ANTIALIASING_MODE != 0)
		
 		#if VRT_USE_CENTER_MASK
 		
	        // only apply sharpen when the mask is not black
            if(circularMask != 0){

            	//get the anti aliased backbuffer to work on 
	            float4 antialiasedBackBuffer = AntialiasingStep(position,texcoord);

	            #if _MASK_SMOOTH
                	backBuffer.rgb = lerp(backBuffer.rgb,antialiasedBackBuffer.rgb,circularMask);
                #else
                	backBuffer = sharpeningResult;
                #endif
            } 
        #else
	        // directly apply anti aliasing without masking
	        backBuffer = AntialiasingStep(position,texcoord);
	    #endif
	#endif
  	
    #if VRT_DITHERING
        // apply dithering before any post processing is done
		backBuffer.rgb += ScreenSpaceDither(position.xy);
    #endif
			
	#if (VRT_SHARPENING_MODE != 0)
              
        #if VRT_USE_CENTER_MASK
         
            // only apply sharpen when the mask is not black
            if(circularMask != 0){
                 //backBuffer.rgb = SharpeningStep(backBuffer,position,texcoord);
                float3 sharpeningResult = SharpeningStep(backBuffer,position,texcoord);
	            #if _MASK_SMOOTH
                	backBuffer.rgb = lerp(backBuffer.rgb,sharpeningResult,circularMask);
                #else
                	backBuffer.rgb = sharpeningResult;
                #endif
              
            } 
        #else
	        // directly apply sharpening without mask
	        backBuffer.rgb = SharpeningStep(backBuffer,position,texcoord);
	    #endif
    #endif

        
   // Color Correction Step
    #if (VRT_COLOR_CORRECTION_MODE != 0)
    
        // convert backbuffer into linear mode for color corrections!
        #if (_VRT_LINEAR_BACKBUFFER )
            // convert from sRGB gamma back to linear 1/2.2
            //backBuffer.rgb = pow(backBuffer.rgb,GAMMA_SRGB_FACTOR);
            //TODO need to check performance vs pow() that is not even precise
            backBuffer.rgb = ApplySRGBCurve_Fast(backBuffer.rgb);
        #endif     
	     
        backBuffer.rgb = ColorCorrectionStep(backBuffer,position,texcoord);
    #endif
       
	#if VRT_USE_CENTER_MASK 
        if(CircularMaskPreview){
	        //backBuffer.rgb = lerp(backBuffer.rgb,backBuffer.rrr, circularMask) * 0.8;
			backBuffer.gb += circularMask * 0.20;
        }
	#endif  

  // pass in the modified back buffer to the output
   return backBuffer;
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Technique
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
technique VRToolkit  <ui_tooltip = "This shader combines multiple shaders tailored to be usable in VR into one render pass"; >
{
	pass
	{
		VertexShader = PostProcessVS;
		PixelShader = CombineVRShaderPS;

		#if (_VRT_LINEAR_BACKBUFFER && VRT_COLOR_CORRECTION_MODE == 0)
           SRGBWriteEnable = true;
        #endif

	}
}

