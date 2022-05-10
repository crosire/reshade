#include "ReShade.fxh"

/*
    CRT-interlaced

    Copyright (C) 2010-2012 cgwg, Themaister and DOLLS

    This program is free software; you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by the Free
    Software Foundation; either version 2 of the License, or (at your option)
    any later version.

    (cgwg gave their consent to have the original version of this shader
    distributed under the GPL in this message:

        http://board.byuu.org/viewtopic.php?p=26075#p26075

        "Feel free to distribute my shaders under the GPL. After all, the
        barrel distortion code was taken from the Curvature shader, which is
        under the GPL."
    )
	This shader variant is pre-configured with screen curvature
*/

uniform float texture_sizeX <
	ui_type = "drag";
	ui_min = 1.0;
	ui_max = BUFFER_WIDTH;
	ui_label = "Screen Width [CRT-Geom]";
> = 320.0;

uniform float texture_sizeY <
	ui_type = "drag";
	ui_min = 1.0;
	ui_max = BUFFER_HEIGHT;
	ui_label = "Screen Height [CRT-Geom]";
> = 240.0;

uniform float video_sizeX <
	ui_type = "drag";
	ui_min = 1.0;
	ui_max = BUFFER_WIDTH;
	ui_label = "Frame Width [CRT-Geom]";
	ui_tooltip = "This should be sized according to the video data in the texture (If you're using emulators, set this to the Emu video frame size, otherwise, keep it the same as Screen Width/Height or Fullscreen res.)";
> = 240.0;

uniform float video_sizeY <
	ui_type = "drag";
	ui_min = 1.0;
	ui_max = BUFFER_HEIGHT;
	ui_label = "Frame Height [CRT-Geom]";
	ui_tooltip = "This should be sized according to the video data in the texture (If you're using emulators, set this to the Emu video frame size, otherwise, keep it the same as Screen Width/Height or Fullscreen res.)";
> = 240.0;

uniform float CRTgamma <
	ui_type = "drag";
	ui_min  = 0.1;
	ui_max = 5.0;
	ui_step = 0.1;
	ui_label = "Target Gamma [CRT-Geom]";
> = 2.4;

uniform float monitorgamma <
	ui_type = "drag";
	ui_min = 0.1;
	ui_max = 5.0;
	ui_step = 0.1;
	ui_label = "Monitor Gamma [CRT-Geom]";
> = 2.2;

uniform float d <
	ui_type = "drag";
	ui_min = 0.1;
	ui_max = 3.0;
	ui_step = 0.1;
	ui_label =  "Distance [CRT-Geom]";
> = 1.5;

uniform float CURVATURE <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 1.0;
	ui_step = 1.0;
	ui_label = "Curvature Toggle [CRT-Geom]";
> = 1.0;

uniform float R <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 10.0;
	ui_step = 0.1;
	ui_label = "Curvature Radius [CRT-Geom]";
> = 2.0;

uniform float cornersize <
	ui_type = "drag";
	ui_min = 0.001;
	ui_max = 1.0;
	ui_step = 0.005;
	ui_label = "Corner Size [CRT-Geom]";
> = 0.03;

uniform float cornersmooth <
	ui_type = "drag";
	ui_min = 80.0;
	ui_max = 2000.0;
	ui_step = 100.0;
	ui_label = "Corner Smoothness [CRT-Geom]";
> = 1000.0;

uniform float x_tilt <
	ui_type = "drag";
	ui_min = -0.5;
	ui_max = 0.5;
	ui_step = 0.05;
	ui_label = "Horizontal Tilt [CRT-Geom]";
> = 0.0;

uniform float y_tilt <
	ui_type = "drag";
	ui_min = -0.5;
	ui_max = 0.5;
	ui_step = 0.05;
	ui_label = "Vertical Tilt [CRT-Geom]";
> = 0.0;

uniform float overscan_x <
	ui_type = "drag";
	ui_min = -125.0;
	ui_max = 125.0;
	ui_step = 1.0;
	ui_label = "Horiz. Overscan % [CRT-Geom]";
> = 100.0;

uniform float overscan_y <
	ui_type = "drag";
	ui_min = -125.0;
	ui_max = 125.0;
	ui_step = 1.0;
	ui_label = "Vert. Overscan % [CRT-Geom]";
> = 100.0;

uniform float DOTMASK <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 0.3;
	ui_step = 0.3;
	ui_label = "Dot Mask Toggle [CRT-Geom]";
> = 0.3;

uniform float SHARPER <
	ui_type = "drag";
	ui_min = 1.0;
	ui_max = 3.0;
	ui_step = 1.0;
	ui_label = "Sharpness [CRT-Geom]";
> = 1.0;

uniform float scanline_weight <
	ui_type = "drag";
	ui_min = 0.1;
	ui_max = 0.5;
	ui_step = 0.01;
	ui_label = "Scanline Weight [CRT-Geom]";
> = 0.3;

uniform float lum <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 1.0;
	ui_step = 0.01;
	ui_label = "Luminance Boost [CRT-Geom]";
> = 0.0;

uniform float interlace_toggle <
	ui_type = "drag";
	ui_min = 1.0;
	ui_max = 5.0;
	ui_step = 4.0;
	ui_label = "Interlacing [CRT-Geom]";
> = 1.0;

uniform bool OVERSAMPLE <
	ui_tooltip = "Enable 3x oversampling of the beam profile; improves moire effect caused by scanlines+curvature";
> = true;

uniform bool INTERLACED <
	ui_tooltip = "Use interlacing detection; may interfere with other shaders if combined";
> = true;

#define FIX(c) max(abs(c), 1e-5);
#define PI 3.141592653589

#define TEX2D(c) pow(tex2D(ReShade::BackBuffer, (c)), float4(CRTgamma,CRTgamma,CRTgamma,CRTgamma))
#define texture_size float2(texture_sizeX, texture_sizeY)
#define video_size float2(video_sizeX, video_sizeY)

static const float2 aspect = float2(1.0, 0.75);

uniform int framecount < source = "framecount"; >;

float fmod(float a, float b)
{
  float c = frac(abs(a/b))*abs(b);
  return (a < 0) ? -c : c;   /* if ( a < 0 ) c = 0-c */
}

float intersect(float2 xy, float2 sinangle, float2 cosangle)
{
    float A = dot(xy,xy)+d*d;
    float B = 2.0*(R*(dot(xy,sinangle)-d*cosangle.x*cosangle.y)-d*d);
	float C = d*d + 2.0*R*d*cosangle.x*cosangle.y;
    return (-B-sqrt(B*B-4.0*A*C))/(2.0*A);
}

float2 bkwtrans(float2 xy, float2 sinangle, float2 cosangle)
{
    float c = intersect(xy, sinangle, cosangle);
	float2 pnt = float2(c,c)*xy;
	pnt -= float2(-R,-R)*sinangle;
	pnt /= float2(R,R);
	float2 tang = sinangle/cosangle;
	float2 poc = pnt/cosangle;
	float A = dot(tang,tang)+1.0;
	float B = -2.0*dot(poc,tang);
	float C = dot(poc,poc)-1.0;
	float a = (-B+sqrt(B*B-4.0*A*C))/(2.0*A);
	float2 uv = (pnt-a*sinangle)/cosangle;
	float r = FIX(R*acos(a));
	return uv*r/sin(r/R);
}

float2 fwtrans(float2 uv, float2 sinangle, float2 cosangle)
{
	float r = FIX(sqrt(dot(uv,uv)));
	uv *= sin(r/R)/r;
	float x = 1.0-cos(r/R);
	float D = d/R + x*cosangle.x*cosangle.y+dot(uv,sinangle);
	return d*(uv*cosangle-x*sinangle)/D;
}

float3 maxscale(float2 sinangle, float2 cosangle)
{
	float2 c = bkwtrans(-R * sinangle / (1.0 + R/d*cosangle.x*cosangle.y), sinangle, cosangle);
	float2 a = float2(0.5,0.5)*aspect;
	float2 lo = float2(fwtrans(float2(-a.x,c.y), sinangle, cosangle).x,
						fwtrans(float2(c.x,-a.y), sinangle, cosangle).y)/aspect;
	float2 hi = float2(fwtrans(float2(+a.x,c.y), sinangle, cosangle).x,
						fwtrans(float2(c.x,+a.y), sinangle, cosangle).y)/aspect;
	return float3((hi+lo)*aspect*0.5,max(hi.x-lo.x,hi.y-lo.y));
}

float4 scanlineWeights(float distance, float4 color)
{
		// "wid" controls the width of the scanline beam, for each RGB
		// channel The "weights" lines basically specify the formula
		// that gives you the profile of the beam, i.e. the intensity as
		// a function of distance from the vertical center of the
		// scanline. In this case, it is gaussian if width=2, and
		// becomes nongaussian for larger widths. Ideally this should
		// be normalized so that the integral across the beam is
		// independent of its width. That is, for a narrower beam
		// "weights" should have a higher peak at the center of the
		// scanline than for a wider beam.
        float4 wid = 2.0 + 2.0 * pow(color, float4(4.0,4.0,4.0,4.0));
        float weights = float(distance / scanline_weight);
        return (lum + 1.4) * exp(-pow(weights * rsqrt(0.5 * wid), wid)) / (0.6 + 0.2 * wid);
}

float4 PS_CRTGeom(float4 vpos : SV_Position, float2 uv : TexCoord) : SV_Target
{

	float2 TextureSize = float2(SHARPER * texture_size.x, texture_size.y);
	float mod_factor = uv.x * texture_size.x * ReShade::ScreenSize.x / video_size.x;
	float2 ilfac = float2(1.0,clamp(floor(video_size.y/(200.0 * interlace_toggle)),1.0,2.0));
	float2 sinangle = sin(float2(x_tilt, y_tilt));
	float2 cosangle = cos(float2(x_tilt, y_tilt));
	float3 stretch = maxscale(sinangle, cosangle);
	float2 one = ilfac / TextureSize;

	// Here's a helpful diagram to keep in mind while trying to
	// understand the code:
	//
	//  |      |      |      |      |
	// -------------------------------
	//  |      |      |      |      |
	//  |  01  |  11  |  21  |  31  | <-- current scanline
	//  |      | @    |      |      |
	// -------------------------------
	//  |      |      |      |      |
	//  |  02  |  12  |  22  |  32  | <-- next scanline
	//  |      |      |      |      |
	// -------------------------------
	//  |      |      |      |      |
	//
	// Each character-cell represents a pixel on the output
	// surface, "@" represents the current pixel (always somewhere
	// in the bottom half of the current scan-line, or the top-half
	// of the next scanline). The grid of lines represents the
	// edges of the texels of the underlying texture.

	// Texture coordinates of the texel containing the active pixel.
	float2 xy = 0.0;
	if (CURVATURE > 0.5){
		float2 cd = uv;
		cd *= texture_size / video_size;
		cd = (cd-float2(0.5,0.5))*aspect*stretch.z+stretch.xy;
		xy =  (bkwtrans(cd, sinangle, cosangle)/float2(overscan_x / 100.0, overscan_y / 100.0)/aspect+float2(0.5,0.5)) * video_size / texture_size;
	} else {
		xy = uv;
	}

	float2 cd2 = xy;
	cd2 *= texture_size / video_size;
	cd2 = (cd2 - float2(0.5,0.5)) * float2(overscan_x / 100.0, overscan_y / 100.0) + float2(0.5,0.5);
	cd2 = min(cd2, float2(1.0,1.0)-cd2) * aspect;
	float2 cdist = float2(cornersize,cornersize);
	cd2 = (cdist - min(cd2,cdist));
	float dist = sqrt(dot(cd2,cd2));
	float cval = clamp((cdist.x-dist)*cornersmooth,0.0, 1.0);

	float2 xy2 = ((xy*TextureSize/video_size-float2(0.5,0.5))*float2(1.0,1.0)+float2(0.5,0.5))*video_size/TextureSize;
	// Of all the pixels that are mapped onto the texel we are
	// currently rendering, which pixel are we currently rendering?
	float2 ilfloat = float2(0.0,ilfac.y > 1.5 ? fmod(float(framecount),2.0) : 0.0);

	float2 ratio_scale = (xy * TextureSize - float2(0.5,0.5) + ilfloat)/ilfac;
      
	float filter = video_size.y / ReShade::ScreenSize.y;
	float2 uv_ratio = frac(ratio_scale);

	// Snap to the center of the underlying texel.

	xy = (floor(ratio_scale)*ilfac + float2(0.5,0.5) - ilfloat) / TextureSize;

	// Calculate Lanczos scaling coefficients describing the effect
	// of various neighbour texels in a scanline on the current
	// pixel.
	float4 coeffs = PI * float4(1.0 + uv_ratio.x, uv_ratio.x, 1.0 - uv_ratio.x, 2.0 - uv_ratio.x);

	// Prevent division by zero.
	coeffs = FIX(coeffs);

	// Lanczos2 kernel.
	coeffs = 2.0 * sin(coeffs) * sin(coeffs / 2.0) / (coeffs * coeffs);

	// Normalize.
	coeffs /= dot(coeffs, float4(1.0,1.0,1.0,1.0));

	// Calculate the effective colour of the current and next
	// scanlines at the horizontal location of the current pixel,
	// using the Lanczos coefficients above.
    float4 col  = clamp(mul(coeffs, float4x4(
		TEX2D(xy + float2(-one.x, 0.0)),
		TEX2D(xy),
		TEX2D(xy + float2(one.x, 0.0)),
		TEX2D(xy + float2(2.0 * one.x, 0.0)))),
	0.0, 1.0);
		
    float4 col2 = clamp(mul(coeffs, float4x4(
		TEX2D(xy + float2(-one.x, one.y)),
		TEX2D(xy + float2(0.0, one.y)),
		TEX2D(xy + one),
		TEX2D(xy + float2(2.0 * one.x, one.y)))),
	0.0, 1.0);

	// Calculate the influence of the current and next scanlines on
	// the current pixel.
	float4 weights  = scanlineWeights(uv_ratio.y, col);
	float4 weights2 = scanlineWeights(1.0 - uv_ratio.y, col2);
	
	if (OVERSAMPLE){
		uv_ratio.y =uv_ratio.y+1.0/3.0*filter;
		weights = (weights+scanlineWeights(uv_ratio.y, col))/3.0;
		weights2=(weights2+scanlineWeights(abs(1.0-uv_ratio.y), col2))/3.0;
		uv_ratio.y =uv_ratio.y-2.0/3.0*filter;
		weights=weights+scanlineWeights(abs(uv_ratio.y), col)/3.0;
		weights2=weights2+scanlineWeights(abs(1.0-uv_ratio.y), col2)/3.0;
	}
	float3 mul_res  = (col * weights + col2 * weights2).rgb;
	mul_res *= float3(cval,cval,cval);

	// dot-mask emulation:
	// Output pixels are alternately tinted green and magenta.
	float3 dotMaskWeights = lerp(
		float3(1.0, 1.0 - DOTMASK, 1.0),
		float3(1.0 - DOTMASK, 1.0, 1.0 - DOTMASK),
		floor(fmod(mod_factor, 2.0))
	); 
	mul_res *= dotMaskWeights;
        
		
	// Convert the image gamma for display on our output device.
	mul_res = pow(mul_res, float3(1.0 / monitorgamma,1.0 / monitorgamma,1.0 / monitorgamma));

	// Color the texel.
	return float4(mul_res, 1.0);
}

technique GeomCRT {
	pass CRT_Geom {
		VertexShader=PostProcessVS;
		PixelShader=PS_CRTGeom;
	}
}