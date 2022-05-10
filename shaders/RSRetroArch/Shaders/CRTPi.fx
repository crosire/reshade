#include "ReShade.fxh"

/*
    crt-pi - A Raspberry Pi friendly CRT shader.
    Copyright (C) 2015-2016 davej
    This program is free software; you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by the Free
    Software Foundation; either version 2 of the License, or (at your option)
    any later version.

Notes:
This shader is designed to work well on Raspberry Pi GPUs (i.e. 1080P @ 60Hz on
a game with a 4:3 aspect ratio). 
It pushes the Pi's GPU hard and enabling some features will slow it down so that
it is no longer able to match 1080P @ 60Hz. 
You will need to overclock your Pi to the fastest setting in raspi-config to get
the best results from this shader: 'Pi2' for Pi2 and 'Turbo' for original Pi and
Pi Zero. 
Note: Pi2s are slower at running the shader than other Pis, this seems to be
down to Pi2s lower maximum memory speed. 
Pi2s don't quite manage 1080P @ 60Hz - they drop about 1 in 1000 frames. 
You probably won't notice this, but if you do, try enabling FAKE_GAMMA.
SCANLINES enables scanlines. 
You'll almost certainly want to use it with MULTISAMPLE to reduce moire effects. 
SCANLINE_WEIGHT defines how wide scanlines are (it is an inverse value so a
higher number = thinner lines). 
SCANLINE_GAP_BRIGHTNESS defines how dark the gaps between the scan lines are. 
Darker gaps between scan lines make moire effects more likely.
GAMMA enables gamma correction using the values in INPUT_GAMMA and OUTPUT_GAMMA.
FAKE_GAMMA causes it to ignore the values in INPUT_GAMMA and OUTPUT_GAMMA and 
approximate gamma correction in a way which is faster than true gamma whilst 
still looking better than having none. 
You must have GAMMA defined to enable FAKE_GAMMA.
CURVATURE distorts the screen by CURVATURE_X and CURVATURE_Y. 
Curvature slows things down a lot.
By default the shader uses linear blending horizontally. If you find this too
blury, enable SHARPER.
BLOOM_FACTOR controls the increase in width for bright scanlines.
MASK_TYPE defines what, if any, shadow mask to use. MASK_BRIGHTNESS defines how
much the mask type darkens the screen.
*/

/* MASK_TYPE: 0 = none, 1 = green/magenta, 2 = trinitron(ish) */

#ifndef MASK_TYPE
	#define MASK_TYPE 2
#endif

#ifndef SCANLINES
	#define SCANLINES 1 //[0 or 1] Enables Scanlines
#endif

#ifndef CURVATURE
	#define CURVATURE 1 //[0 or 1] Enables Curvature
#endif

#ifndef FAKE_GAMMA
	#define FAKE_GAMMA 0 //[0 or 1] Ignores manual gamma tweaks and tries to auto-compensate it.
#endif

#ifndef GAMMA
	#define GAMMA 0 //[0 or 1] Enables Gamma Control
#endif

#ifndef SHARPER
	#define SHARPER 0 //[0 or 1] Sharpens the image if its too blurry due to Linear sampling
#endif

#ifndef MULTISAMPLE
	#define MULTISAMPLE 1 //[0 or 1] Enables Multisampling
#endif

uniform float video_sizeX <
	ui_type = "drag";
	ui_min = 1.0;
	ui_max = BUFFER_WIDTH;
	ui_label = "Frame Width [CRT-Pi]";
	ui_tooltip = "This should be sized according to the video data in the texture (If you're using emulators, set this to the Emu video frame size, otherwise, keep it the same as Screen Width/Height or Fullscreen res.)";
> = 320.0;

uniform float video_sizeY <
	ui_type = "drag";
	ui_min = 1.0;
	ui_max = BUFFER_HEIGHT;
	ui_label = "Frame Height [CRT-Pi]";
	ui_tooltip = "This should be sized according to the video data in the texture (If you're using emulators, set this to the Emu video frame size, otherwise, keep it the same as Screen Width/Height or Fullscreen res.)";
> = 240.0;

uniform float CURVATURE_X <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 1.0;
	ui_step = 0.01;
	ui_label = "Screen curvature - horizontal [CRT-Pi]"; 
> = 0.10;

uniform float CURVATURE_Y <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 1.0;
	ui_step = 0.01;
	ui_label = "Screen curvature - vertical [CRT-Pi]";
> = 0.15;

uniform float MASK_BRIGHTNESS <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 1.0;
	ui_step = 0.01;
	ui_label = "Mask brightness [CRT-Pi]";
> = 0.70;

uniform float SCANLINE_WEIGHT <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 15.0;
	ui_step = 0.01;
	ui_label = "Scanline weight [CRT-Pi]";
> = 6.0;

uniform float SCANLINE_GAP_BRIGHTNESS <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 1.0;
	ui_step = 0.01;
	ui_label = "Scanline gap brightness [CRT-Pi]";
> = 0.12;

uniform float BLOOM_FACTOR <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 5.0;
	ui_step = 0.01;
	ui_label = "Bloom factor [CRT-Pi]";
> = 1.5;

uniform float INPUT_GAMMA <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 5.0;
	ui_step = 0.01;
	ui_label = "Input gamma [CRT-Pi]";
> = 2.4;

uniform float OUTPUT_GAMMA <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 5.0;
	ui_step = 0.01;
	ui_label = "Output gamma";
> = 2.2;

#define video_size float2(video_sizeX, video_sizeY)
#define video_size_rcp float2(1.0/video_sizeX, 1.0/video_sizeY)

#if (CURVATURE == 1)
	// Barrel distortion shrinks the display area a bit, this will allow us to counteract that.
	float2 Distort(float2 coord)
	{
		float2 CURVATURE_DISTORTION = float2(CURVATURE_X, CURVATURE_Y);
		float2 barrelScale = 1.0 - (0.23 * CURVATURE_DISTORTION);
		
	//  coord *= screenScale; // not necessary in slang
		coord -= float2(0.5,0.5);
		float rsq = coord.x * coord.x + coord.y * coord.y;
		coord += coord * (CURVATURE_DISTORTION * rsq);
		coord *= barrelScale;
		if (abs(coord.x) >= 0.5 || abs(coord.y) >= 0.5)
			coord = float2(-1.0,-1.0);     // If out of bounds, return an invalid value.
		else
		{
			coord += float2(0.5,0.5);
	//      coord /= screenScale; // not necessary in slang
		}

		return coord;
	}
#endif

float CalcScanLineWeight(float dist)
{
    return max(1.0-dist*dist*SCANLINE_WEIGHT, SCANLINE_GAP_BRIGHTNESS);
}

float CalcScanLine(float dy)
{
    float scanLineWeight = CalcScanLineWeight(dy);
#if (MULTISAMPLE == 1)
	float filterWidth = (video_size.y * ReShade::PixelSize.y) * 0.333333333;
    scanLineWeight += CalcScanLineWeight(dy - filterWidth);
    scanLineWeight += CalcScanLineWeight(dy + filterWidth);
    scanLineWeight *= 0.3333333;
#endif
    return scanLineWeight;
}

float4 PS_CRTPi(float4 vpos : SV_Position, float2 txcoord : TexCoord) : SV_Target
{
	float4 result;
    float2 texcoord = txcoord;

#if (CURVATURE == 1)
    texcoord = Distort(texcoord);
    if (texcoord.x < 0.0) 
    {
        result = float4(0.0,0.0,0.0,0.0);
        return result;
    }
#endif

    float2 texcoordInPixels = texcoord * video_size.xy;

#if (SHARPER == 1)
    float2 tempCoord       = floor(texcoordInPixels) + 0.5;
    float2 coord           = tempCoord * video_size_rcp.xy;
    float2 deltas          = texcoordInPixels - tempCoord;
    float scanLineWeight = CalcScanLine(deltas.y);
    
    float2 signs = sign(deltas);
    
    deltas   = abs(deltas) * 2.0;
    deltas.x = deltas.x * deltas.x;
    deltas.y = deltas.y * deltas.y * deltas.y;
    deltas  *= 0.5 * video_size_rcp.xy * signs;

    float2 tc = coord + deltas;
#else
    float tempCoord       = floor(texcoordInPixels.y) + 0.5;
    float coord           = tempCoord * video_size_rcp.y;
    float deltas          = texcoordInPixels.y - tempCoord;
    float scanLineWeight  = CalcScanLine(deltas);
    
    float signs = sign(deltas);
    
    deltas   = abs(deltas) * 2.0;
    deltas   = deltas * deltas * deltas;
    deltas  *= 0.5 * ReShade::PixelSize.y * signs;

    float2 tc = float2(texcoord.x, coord + deltas);
#endif

    float3 colour = tex2D(ReShade::BackBuffer, tc).rgb;

#if (SCANLINES == 1)


#if (GAMMA == 1) && (FAKEGAMMA == 1)
    colour = colour * colour;
#elif (GAMMA == 1)
    colour = pow(colour, float3(INPUT_GAMMA,INPUT_GAMMA,INPUT_GAMMA));
#endif
    
    /* Apply scanlines */
    scanLineWeight *= BLOOM_FACTOR;
    colour *= scanLineWeight;

#if (GAMMA == 1) && (FAKEGAMMA == 1)
    colour = sqrt(colour);
#elif (GAMMA == 1)
    colour = pow(colour, float3(1.0/OUTPUT_GAMMA,1.0/OUTPUT_GAMMA,1.0/OUTPUT_GAMMA));
#endif


#endif /* SCANLINES */

#if (MASK_TYPE == 1)
    float whichMask = frac((txcoord.x * ReShade::ScreenSize.x) * 0.5);
    float3 mask       = float3(1.0);

    if (whichMask < 0.5) mask.rb = float2(MASK_BRIGHTNESS,MASK_BRIGHTNESS);
    else                 mask.g  = MASK_BRIGHTNESS;

    colour *= mask;
#elif (MASK_TYPE == 2)
    float whichMask = frac((txcoord.x * ReShade::ScreenSize.x)  * 0.3333333);
    float3 mask       = float3(MASK_BRIGHTNESS,MASK_BRIGHTNESS,MASK_BRIGHTNESS);
    
    if      (whichMask < 0.3333333) mask.r = 1.0;
    else if (whichMask < 0.6666666) mask.g = 1.0;
    else                            mask.b = 1.0;

    colour *= mask;
#endif
	result = float4(colour, 1.0);
    return result;
}

technique CRTPi {
	pass CRT_Pi {
		VertexShader=PostProcessVS;
		PixelShader=PS_CRTPi;
	}
}