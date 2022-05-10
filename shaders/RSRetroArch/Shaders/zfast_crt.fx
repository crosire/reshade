#include "ReShade.fxh"

/*
    zfast_crt_standard - A simple, fast CRT shader.
    Copyright (C) 2017 Greg Hogan (SoltanGris42)
    This program is free software; you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by the Free
    Software Foundation; either version 2 of the License, or (at your option)
    any later version.
Notes:  This shader does scaling with a weighted linear filter for adjustable
	sharpness on the x and y axes based on the algorithm by Inigo Quilez here:
	http://http://www.iquilezles.org/www/articles/texture/texture.htm
	but modified to be somewhat sharper.  Then a scanline effect that varies
	based on pixel brighness is applied along with a monochrome aperture mask.
	This shader runs at 60fps on the Raspberry Pi 3 hardware at 2mpix/s
	resolutions (1920x1080 or 1600x1200).
*/

uniform bool FINEMASK <
	ui_type = "boolean";
	ui_label = "Enable FineMask [CRT-ZFast]";
> = true;

uniform bool BLACK_OUT_BORDER <
	ui_type = "boolean";
	ui_label = "Enable Black Border [CRT-ZFast]";
> = false;


// Parameter lines go here:
uniform float BLURSCALEX <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 1.0;
	ui_step = 0.05;
	ui_label = "Blur Amount X-Axis [CRT-ZFast]";
> = 0.30;

uniform float LOWLUMSCAN <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 10.0;
	ui_step = 0.5;
	ui_label = "Scanline Darkness - Low [CRT-ZFast]";
> = 6.0;

uniform float HILUMSCAN <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 50.0;
	ui_step = 1.0;
	ui_label = "Scanline Darkness - High [CRT-ZFast]";
> = 8.0;

uniform float BRIGHTBOOST <
	ui_type = "drag";
	ui_min = 0.5;
	ui_max = 1.5;
	ui_step = 0.05;
	ui_label = "Dark Pixel Brightness Boost [CRT-ZFast]";
> = 1.25;

uniform float MASK_DARK < 
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 1.0;
	ui_step = 0.05;
	ui_label = "Mask Effect Amount [CRT-ZFast]";
> = 0.25;

uniform float MASK_FADE < 
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 1.0;
	ui_step = 0.05;
	ui_label = "Mask/Scanline Fade [CRT-ZFast]";
> = 0.8;

uniform float video_sizeX <
	ui_type = "drag";
	ui_min = 1.0;
	ui_max = BUFFER_WIDTH;
	ui_label = "Screen Width [CRT-ZFast]";
	ui_tooltip = "This should be sized according to the video data in the texture (If you're using emulators, set this to the Emu video frame size, otherwise, keep it the same as Screen Width/Height or Fullscreen res.)";
> = 320.0;

uniform float video_sizeY <
	ui_type = "drag";
	ui_min = 1.0;
	ui_max = BUFFER_HEIGHT;
	ui_label = "Screen Height [CRT-ZFast]";
	ui_tooltip = "This should be sized according to the video data in the texture (If you're using emulators, set this to the Emu video frame size, otherwise, keep it the same as Screen Width/Height or Fullscreen res.)";
> = 240.0;

void PS_ZFastCRT(in float4 pos : SV_POSITION, in float2 txcoord : TEXCOORD0, out float4 fragColor : COLOR0)
{

	float maskFade;
	float2 invDims = float2(0.0,0.0);
	float2 TextureSize = float2(video_sizeX,video_sizeY);
	
	float2 texcoord = txcoord;
	float2 tc = txcoord * ReShade::ScreenSize;
	
	maskFade = 0.3333*MASK_FADE;
	invDims = 1.0/TextureSize.xy;

	//This is just like "Quilez Scaling" but sharper
	float2 p = texcoord * TextureSize;
	float2 i = floor(p) + 0.50;
	float2 f = p - i;
	p = (i + 4.0*f*f*f)*invDims;
	p.x = lerp( p.x , texcoord.x, BLURSCALEX);
	float Y = f.y*f.y;
	float YY = Y*Y;
	
	float whichmask;
	float mask;

	if(FINEMASK != 0){
		whichmask = frac( txcoord.x*-0.4999);
		mask = 1.0 + float(whichmask < 0.5) * -MASK_DARK;
	} else {
		whichmask = frac(txcoord.x * -0.3333);
		mask = 1.0 + float(whichmask <= 0.33333) * -MASK_DARK;
	}

	float3 colour = tex2D(ReShade::BackBuffer, p).rgb;
	float3 finalColor = float3(0.0,0.0,0.0);
	
	float scanLineWeight = (BRIGHTBOOST - LOWLUMSCAN*(Y - 2.05*YY));
	float scanLineWeightB = 1.0 - HILUMSCAN*(YY-2.8*YY*Y);	
	
	if (BLACK_OUT_BORDER) {
		colour.rgb*=float(texcoord.x > 0.0)*float(texcoord.y > 0.0); //why doesn't the driver do the right thing?
	}

	fragColor = float4(colour.rgb*lerp(scanLineWeight*mask, scanLineWeightB, dot(colour.rgb,float3(maskFade,maskFade,maskFade))),1.0);
	
} 

technique CRTZFast {
    pass CRTZFast {
        VertexShader=PostProcessVS;
        PixelShader=PS_ZFastCRT;
    }
}