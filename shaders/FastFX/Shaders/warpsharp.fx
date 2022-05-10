/*------------------.
| :: Description :: |
'-------------------/

	warpsharp.fx

	Author: Robert Jessop	
	License: MIT

	About:
	
	Warp sharp sharpens blurry edges by detecting edges then sampling a point away from the centre line of the edge.
	
	Warp sharp is good for restoring clear edges (but not texture details) on images that have been blurred or upscaled.
	
	It is not necessarily good for sharpening game output, as games have a mixture of very sharp edges of objects, and varying levels of blurrines in different textures.
	
	This sharpening algorithm is based roughly on the description here: https://www.virtualdub.org/blog2/entry_079.html - except I have addressed the contrast problem describe there by taking the square root of the bump map value. Increasing the scale is fast but not as clever as it could be - ideally you'd use a bigger kernel at higher scales.
    
	Ideas for future improvement:
    *

	History:
	(*) Feature (+) Improvement (x) Bugfix (-) Information (!) Compatibility
	
	Version 1.0 - this.
    
*/


/*---------------.
| :: Includes :: |
'---------------*/

#include "ReShade.fxh"
#include "ReShadeUI.fxh"

namespace warpsharp {

uniform float warp_strength < __UNIFORM_SLIDER_FLOAT1
	ui_category = "WarpSharp";
	ui_min = 0; ui_max = 5; ui_step = .01;	
	ui_tooltip = "Multiplier for the warp distance. May need to be higher for low contrast images, or lower for high-contrast images.";
	ui_label = "WarpSharp Strength";
> = .5;

uniform float warp_scale < __UNIFORM_SLIDER_FLOAT1
	ui_category = "WarpSharp";
	ui_min = 1; ui_max = 25; ui_step = .5;	
	ui_tooltip = "Scale in pixels - the bigger the blur on the input image the larger this needs to be. This is both the distance of input points when calculating the edge bump map, and maximum displacement when warping the final output. ";
	ui_label = "WarpSharp Scale";
> = 1;


texture warpTex {
    Width = BUFFER_WIDTH ;
    Height = BUFFER_HEIGHT ;
    Format = R16F;
};


sampler warpSampler {
    Texture = warpTex;
	
};

texture warpTex2 {
    Width = BUFFER_WIDTH ;
    Height = BUFFER_HEIGHT ;
    Format = R16F;
};


sampler warpSampler2 {
    Texture = warpTex;
	
};



#define halfpixel ((warp_scale-.5)*1/float2(BUFFER_WIDTH,BUFFER_HEIGHT))

//Step 1: create bumpmap
float warpSharp1_PS(in float4 pos : SV_Position, in float2 texcoord : TEXCOORD) : COLOR
{	
	/*
	Thanks to GPU's built in bilinear sampling, at scale 1, these four reads give is the equivelent to these two kernels.
	-1  0 +1        -1 -2 -1
	-2  0 +2         0  0  0
	-1  0 +1        +1 +2 +1
	*/

	float3 ne = tex2D(ReShade::BackBuffer, texcoord + halfpixel).rgb;	
	float3 se = tex2D(ReShade::BackBuffer, texcoord + halfpixel*float2(1,-1)).rgb;	
	float3 sw = tex2D(ReShade::BackBuffer, texcoord - halfpixel).rgb;	
	float3 nw = tex2D(ReShade::BackBuffer, texcoord - halfpixel*float2(1,-1)).rgb;	
	
	float dx = length(ne+se-sw-nw);
	float dy = length(ne+nw-se-sw);
	
	//while we're here lets take sqrt so things to reduce difference in warp in high and low contrast areas.
	return sqrt(length(float2(dx,dy)));
}

//Step 2: blur bumpmap  - if you want to sharpen something really blurry you might need a bigger blur here.
float warpSharp2_PS(in float4 pos : SV_Position, in float2 texcoord : TEXCOORD) : COLOR
{
	float ne = tex2D(warpSampler, texcoord + halfpixel ).r;	
	float se = tex2D(warpSampler, texcoord + halfpixel*float2(1,-1)).r;	
	float sw = tex2D(warpSampler, texcoord - halfpixel).r;	
	float nw = tex2D(warpSampler, texcoord - halfpixel*float2(1,-1)).r;	
	
	float total = (ne+se+sw+nw);
	if(warp_scale>=1.5) total = (total + tex2D(warpSampler, texcoord)).r /5;
	else total = total /4;
	return total;
}

//Step 3: calculate displaement vectors from bump map and apply them to image.
float4 warpSharp3_PS(in float4 pos : SV_Position, in float2 texcoord : TEXCOORD) : COLOR
{
	float ne = tex2D(warpSampler2, texcoord + halfpixel).r;	
	float se = tex2D(warpSampler2, texcoord + halfpixel*float2(1,-1)).r;	
	float sw = tex2D(warpSampler2, texcoord - halfpixel).r;	
	float nw = tex2D(warpSampler2, texcoord - halfpixel*float2(1,-1)).r;	
	
	float dx = ne+se-sw-nw;
	float dy = ne+nw-se-sw;
	
	float2 offset = float2(dx, dy) * warp_strength*.01;
	
	offset = clamp(offset, -halfpixel, halfpixel);
	
	return tex2D(ReShade::BackBuffer, texcoord - offset);
}





technique WarpSharp <
	ui_tooltip = "Warp sharp sharpens blurry edges by detecting edges then sampling a point away from the centre line of the edge.\n\nWarp sharp is good for restoring clear edges (but not texture details) on images that have been blurred or upscaled.\n\nIt is not necessarily good for sharpening game output, as games have a mixture of very sharp edges of objects, and varying levels of blurrines in different textures.";
	>
{	

	pass  {
        VertexShader = PostProcessVS;
        PixelShader  = warpSharp1_PS;
        RenderTarget = warpTex;
    }
	
	pass  {
        VertexShader = PostProcessVS;
        PixelShader  = warpSharp2_PS;
        RenderTarget = warpTex2;
    }
	
    pass  {
        VertexShader = PostProcessVS;
        PixelShader  = warpSharp3_PS;
    }
}


}