#include "ReShade.fxh"

/*
	PowerVR2 buffer shader

    Authors: leilei
 
    This program is free software; you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by the Free
    Software Foundation; either version 2 of the License, or (at your option)
    any later version.
*/

uniform float texture_sizeX <
	ui_type = "drag";
	ui_min = 1.0;
	ui_max = BUFFER_WIDTH;
	ui_label = "Screen Width [PowerVR2]";
	ui_step = 1.0;
> = 320.0;

uniform float texture_sizeY <
	ui_type = "drag";
	ui_min = 1.0;
	ui_max = BUFFER_HEIGHT;
	ui_label = "Screen Height [PowerVR2]";
	ui_step = 1.0;
> = 240.0;

uniform bool INTERLACED <
	ui_type = "boolean";
	ui_label = "PVR - Interlace smoothing [PowerVR2]";
> = true;

uniform bool VGASIGNAL <
	ui_type = "boolean";
	ui_label = "PVR - VGA signal loss [PowerVR2]";
> = false;

#define HW 1.00
#define LUM_R (76.0f/255.0f)
#define LUM_G (150.0f/255.0f)
#define LUM_B (28.0f/255.0f)

static const float dithertable[16] = {
	16.0,4.0,13.0,1.0,   
	8.0,12.0,5.0,9.0,
	14.0,2.0,15.0,3.0,
	6.0,10.0,7.0,11.0		
};

#define mod(x,y) (x-y*floor(x/y))
uniform int FCount < source = "framecount"; >;
#define texture_size float2(texture_sizeX, texture_sizeY)

float4 PS_PowerVR2(float4 pos : SV_Position, float2 uv : TEXCOORD0) : SV_Target
{

	float blue;
	
	float2 texcoord  = uv;
	float2 texcoord2  = uv;
	texcoord2.x *= texture_size.x;
	texcoord2.y *= texture_size.y;
	float4 color = tex2D(ReShade::BackBuffer, texcoord);
	float fc = mod(float(FCount), 2.0);

	// Blend vertically for composite mode
	if (INTERLACED)
	{
	int taps = int(3);

	float tap = 2.0f/taps;
	float2 texcoord4  = uv;
	texcoord4.x = texcoord4.x;
	texcoord4.y = texcoord4.y + ((tap*(taps/2))/480.0f);
	float4 blur1 = tex2D(ReShade::BackBuffer, texcoord4);
	int bl;
	float4 ble;
	
	ble.r = 0.00f;
	ble.g = 0.00f;
	ble.b = 0.00f;

	for (bl=0;bl<taps;bl++)
		{
			texcoord4.y += (tap / 480.0f);
			ble.rgb += tex2D(ReShade::BackBuffer, texcoord4).rgb / taps;
		}

        	color.rgb = ( ble.rgb );
	}

	// Dither. ALWAYS do this for 16bpp
	int ditdex = 	int(mod(texcoord2.x, 4.0)) * 4 + int(mod(texcoord2.y, 4.0)); 	
	int yeh = 0;
	float ohyes;
	float4 how;

	for (yeh=ditdex; yeh<(ditdex+16); yeh++) 	ohyes =  ((((dithertable[yeh-15]) - 1) * 0.1));
	color.rb -= (ohyes / 128);
	color.g -= (ohyes / 128);
	{
	float4 reduct;		// 16 bits per pixel (5-6-5)
	reduct.r = 32;
	reduct.g = 64;	
	reduct.b = 32;
	how = color;
  	how = pow(how, float4(1.0f, 1.0f, 1.0f, 1.0f));  	how *= reduct;  	how = floor(how);	how = how / reduct;  	how = pow(how, float4(1.0f, 1.0f, 1.0f, 1.0f));
	}

	color.rb = how.rb;
	color.g = how.g;

	// There's a bit of a precision drop involved in the RGB565ening for VGA
	// I'm not sure why that is. it's exhibited on PVR1 and PVR3 hardware too
	if (INTERLACED < 0.5)
	{
		if (mod(color.r*32, 2.0)>0) color.r -= 0.023;
		if (mod(color.g*64, 2.0)>0) color.g -= 0.01;
		if (mod(color.b*32, 2.0)>0) color.b -= 0.023;
	}


	// RGB565 clamp

	color.rb = round(color.rb * 32)/32;
	color.g = round(color.g * 64)/64;

	// VGA Signal Loss, which probably is very wrong but i tried my best
	if (VGASIGNAL)
	{

	int taps = 32;
	float tap = 12.0f/taps;
	float2 texcoord4  = uv;
	texcoord4.x = texcoord4.x + (2.0f/640.0f);
	texcoord4.y = texcoord4.y;
	float4 blur1 = tex2D(ReShade::BackBuffer, texcoord4);
	int bl;
	float4 ble;
	for (bl=0;bl<taps;bl++)
		{
			float e = 1;
			if (bl>=3)
			e=0.35f;
			texcoord4.x -= (tap  / 640);
			ble.rgb += (tex2D(ReShade::BackBuffer, texcoord4).rgb * e) / (taps/(bl+1));
		}

        	color.rgb += ble.rgb * 0.015;

		//color.rb += (4.0f/255.0f);
		color.g += (9.0f/255.0f);
	}

	return color;

}

technique PowerVR2 {
    pass PowerVR2 {
        VertexShader=PostProcessVS;
        PixelShader=PS_PowerVR2;
    }
}