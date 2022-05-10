/*------------------.
| :: Description :: |
'-------------------/

	fastBlur.fx

	Author: Robert Jessop	
	License: MIT

	About:
	
	A big and fast blur. Only 4 passes, each with 5 texture reads.
	
	For an extra big blur set preprocessor definition FAST_BLUR_SCALE to an integer bigger than 1. High values of for FAST_BLUR_SCALE might only look right with fast_blur_size=1.
	    
	Ideas for future improvement:
    
	History:
	(*) Feature (+) Improvement (x) Bugfix (-) Information (!) Compatibility
	
	Version 1.0 - this.
    
*/


/*---------------.
| :: Includes :: |
'---------------*/

#include "ReShade.fxh"
#include "ReShadeUI.fxh"

namespace fastblur {

#ifndef FAST_BLUR_SCALE
	#define FAST_BLUR_SCALE 1
#endif




uniform float fast_blur_size < __UNIFORM_SLIDER_FLOAT1
	ui_category = "Fast Blur";
	ui_min = 0; ui_max = 1.5; ui_step = .01;	
	ui_tooltip = "For an extra big blur set preprocessor definition FAST_BLUR_SCALE to an integer bigger than 1. High values of for FAST_BLUR_SCALE might only look right with fast_blur_size=1.";
	ui_label = "Fast Blur size";
> = 1;

uniform int fast_blur_shape <
    ui_category = "Fast Blur";
	ui_type = "combo";
    ui_label = "Fast Blur shape";
    ui_items = "Gaussian (6 15 20 15 6)\0"
	           "Block (1 1 1 1 1)\0";	
> = 0;


sampler2D samplerColor
{
	// The texture to be used for sampling.
	Texture = ReShade::BackBufferTex;

	// Enable converting  to linear colors when sampling from the texture.
	SRGBTexture = true;
	
};

texture HBlurTex {
    Width = BUFFER_WIDTH/FAST_BLUR_SCALE ;
    Height = BUFFER_HEIGHT/FAST_BLUR_SCALE ;
    Format = RGBA16F;
};

texture VBlurTex {
    Width = BUFFER_WIDTH/FAST_BLUR_SCALE ;
    Height = BUFFER_HEIGHT/FAST_BLUR_SCALE ;
    Format = RGBA16F;
};

sampler HBlurSampler {
    Texture = HBlurTex;
	
};

sampler VBlurSampler {
    Texture = VBlurTex;
	
};


float4 fastBlur(sampler s, in float4 pos, in float2 texcoord, in float2 step  ) {
	step *= fast_blur_size/float2(BUFFER_WIDTH/FAST_BLUR_SCALE,BUFFER_HEIGHT/FAST_BLUR_SCALE);
	
	float4 color = 0;
	
	const uint steps=5;
	
	//The numbers are from pascals triange. You could add more steps and/or change the row used to change the shape of the blur. using float4 because for w we want brightness over a larger area - it's basically two different blurs in one.
	const float sum=6+15+20+15+6;
    float w[steps] = { 6/sum, 15/sum, 20/sum, 15/sum, 6/sum};
	if(fast_blur_shape) w = { .2, .2, .2, .2, .2 };
		
	float2 offset=-floor(steps/2)*step ; ;
	for( uint i=0; i<steps; i++) {
		float4 c = tex2D(s, texcoord + offset);
		offset+=step;
		c *= (w[i]);
		color+=c;
		//sum+=w[i];
	}
	return color;
}

#define halfpixel (.5/float2(BUFFER_WIDTH/FAST_BLUR_SCALE,BUFFER_HEIGHT/FAST_BLUR_SCALE))

float4 fastBlur1_PS(in float4 pos : SV_Position, in float2 texcoord : TEXCOORD) : COLOR
{
	
	return fastBlur(samplerColor, pos, texcoord+halfpixel, float2(5,2) );
}

float4 fastBlur1b_PS(in float4 pos : SV_Position, in float2 texcoord : TEXCOORD) : COLOR
{
	
	return fastBlur(VBlurSampler, pos, texcoord+halfpixel, float2(5,2) );
}

float4 fastBlur2_PS(in float4 pos : SV_Position, in float2 texcoord : TEXCOORD) : COLOR
{
	
	return fastBlur(HBlurSampler, pos, texcoord-halfpixel, float2(-2,5) );
}

float4 fastBlur3_PS(in float4 pos : SV_Position, in float2 texcoord : TEXCOORD) : COLOR
{
	
	return fastBlur(VBlurSampler, pos, texcoord-halfpixel, float2(2,5) );
}

float4 fastBlur4_PS(in float4 pos : SV_Position, in float2 texcoord : TEXCOORD) : COLOR
{
	
	return fastBlur(HBlurSampler, pos, texcoord+halfpixel, float2(-5,2) );
}


float4 copy_VBlurSampler_PS(in float4 pos : SV_Position, in float2 texcoord : TEXCOORD) : COLOR {
	return tex2D(VBlurSampler, texcoord);	
}

float4 copy_PS(in float4 pos : SV_Position, in float2 texcoord : TEXCOORD) : COLOR {
	return tex2D(samplerColor, texcoord);	
}


technique fastblur <
	ui_tooltip = "Big and fast blur.\n\nFor an extra big blur set preprocessor definition FAST_BLUR_SCALE to an integer bigger than 1. High values of for FAST_BLUR_SCALE might only look right with fast_blur_size=1.";
	>
{	
  #if FAST_BLUR_SCALE > 2 
	pass  {
        VertexShader = PostProcessVS;
        PixelShader  = copy_PS;
        RenderTarget = VBlurTex;
    }
	
	pass  {
        VertexShader = PostProcessVS;
        PixelShader  = fastBlur1b_PS;
        RenderTarget = HBlurTex;
    }
		
    pass  {
        VertexShader = PostProcessVS;
        PixelShader  = fastBlur2_PS;
        RenderTarget = VBlurTex;
    }
	
	pass  {
        VertexShader = PostProcessVS;
        PixelShader  = fastBlur3_PS;
        RenderTarget = HBlurTex;
    }
	
    pass  {
        VertexShader = PostProcessVS;
        PixelShader  = fastBlur4_PS;
        RenderTarget = VBlurTex;
    }
	
	pass  {
        VertexShader = PostProcessVS;
        PixelShader  = copy_VBlurSampler_PS;
        SRGBWriteEnable = true;
    }
  #else
	pass  {
        VertexShader = PostProcessVS;
        PixelShader  = fastBlur1_PS;
        RenderTarget = HBlurTex;
    }
	
    pass  {
        VertexShader = PostProcessVS;
        PixelShader  = fastBlur2_PS;
        RenderTarget = VBlurTex;
    }
	
	pass  {
        VertexShader = PostProcessVS;
        PixelShader  = fastBlur3_PS;
        RenderTarget = HBlurTex;
    }
	
    pass  {
        VertexShader = PostProcessVS;
        PixelShader  = fastBlur4_PS;
        SRGBWriteEnable = true;
    }
  #endif
}


}
