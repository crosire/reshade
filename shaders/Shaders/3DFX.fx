////----------//
////**3DFX**////
//----------////

#include "ReShade.fxh"
#include "ReShadeUI.fxh"

uniform float DITHERAMOUNT < __UNIFORM_SLIDER_FLOAT1
	ui_min = 0.0;
	ui_max = 1.0;
	ui_label = "Dither Amount [3DFX]";
> = 0.5;

uniform int DITHERBIAS < __UNIFORM_SLIDER_INT1
	ui_min = -16;
	ui_max = 16;
	ui_label = "Dither Bias [3DFX]";
> = -1;

uniform float LEIFX_LINES < __UNIFORM_SLIDER_FLOAT1
	ui_min = 0.0;
	ui_max = 2.0;
	ui_label = "Lines Intensity [3DFX]";
> = 1.0;

uniform float LEIFX_PIXELWIDTH < __UNIFORM_SLIDER_FLOAT1
	ui_min = 0.0;
	ui_max = 100.0;
	ui_label = "Pixel Width [3DFX]";
> = 1.5;

uniform float GAMMA_LEVEL < __UNIFORM_SLIDER_FLOAT1
	ui_min = 0.0;
	ui_max = 3.0;
	ui_label = "Gamma Level [3DFX]";
> = 1.0;

#ifndef FILTCAP
	#define	FILTCAP	  0.04	//[0.0:100.0] //-filtered pixel should not exceed this
#endif

#ifndef FILTCAPG
	#define	FILTCAPG (FILTCAP/2)
#endif

float mod2(float x, float y)
{
	return x - y * floor (x/y);
}

float fmod(float a, float b)
{
  float c = frac(abs(a/b))*abs(b);
  return (a < 0) ? -c : c;   /* if ( a < 0 ) c = 0-c */
}

float4 PS_3DFX(float4 vpos : SV_Position, float2 texcoord : TEXCOORD) : SV_Target
{
	float4 colorInput = tex2D(ReShade::BackBuffer, texcoord);

	float2 res;
	res.x = ReShade::ScreenSize.x;
	res.y = ReShade::ScreenSize.y;
	
	float2 ditheu = texcoord.xy * res.xy;

	ditheu.x = texcoord.x * res.x;
	ditheu.y = texcoord.y * res.y;

	// Dither. Total rewrite.
	// NOW, WHAT PIXEL AM I!??

	int ditx = int(fmod(ditheu.x, 4.0));
	int dity = int(fmod(ditheu.y, 4.0));
	int ditdex = ditx * 4 + dity; // 4x4!
	float3 color;
	float3 colord;
	color.r = colorInput.r * 255;
	color.g = colorInput.g * 255;
	color.b = colorInput.b * 255;
	int yeh = 0;
	int ohyes = 0;

    float erroredtable[16] = {
	16,4,13,1,   
	8,12,5,9,
	14,2,15,3,
	6,10,7,11		
    };
	
	// looping through a lookup table matrix
	//for (yeh=ditdex; yeh<(ditdex+16); yeh++) ohyes = pow(erroredtable[yeh-15], 0.72f);
	// Unfortunately, RetroArch doesn't support loops so I have to unroll this. =(
	// Dither method adapted from xTibor on Shadertoy ("Ordered Dithering"), generously
	// put into the public domain.  Thanks!
	if (yeh++==ditdex) ohyes = erroredtable[0];
	else if (yeh++==ditdex) ohyes = erroredtable[1];
	else if (yeh++==ditdex) ohyes = erroredtable[2];
	else if (yeh++==ditdex) ohyes = erroredtable[3];
	else if (yeh++==ditdex) ohyes = erroredtable[4];
	else if (yeh++==ditdex) ohyes = erroredtable[5];
	else if (yeh++==ditdex) ohyes = erroredtable[6];
	else if (yeh++==ditdex) ohyes = erroredtable[7];
	else if (yeh++==ditdex) ohyes = erroredtable[8];
	else if (yeh++==ditdex) ohyes = erroredtable[9];
	else if (yeh++==ditdex) ohyes = erroredtable[10];
	else if (yeh++==ditdex) ohyes = erroredtable[11];
	else if (yeh++==ditdex) ohyes = erroredtable[12];
	else if (yeh++==ditdex) ohyes = erroredtable[13];
	else if (yeh++==ditdex) ohyes = erroredtable[14];
	else if (yeh++==ditdex) ohyes = erroredtable[15];

	// Adjust the dither thing
	ohyes = 17 - (ohyes - 1); // invert
	ohyes *= DITHERAMOUNT;
	ohyes += DITHERBIAS;

	colord.r = color.r + ohyes;
	colord.g = color.g + (ohyes / 2);
	colord.b = color.b + ohyes;
	colorInput.rgb = colord.rgb * 0.003921568627451; // divide by 255, i don't trust em

	//
	// Reduce to 16-bit color
	//

	float why = 1;
	float3 reduceme = 1;
	float radooct = 32;	// 32 is usually the proper value

	reduceme.r = pow(colorInput.r, why);  
	reduceme.r *= radooct;	
	reduceme.r = float(floor(reduceme.r));	
	reduceme.r /= radooct; 
	reduceme.r = pow(reduceme.r, why);

	reduceme.g = pow(colorInput.g, why);  
	reduceme.g *= radooct * 2;	
	reduceme.g = float(floor(reduceme.g));	
	reduceme.g /= radooct * 2; 
	reduceme.g = pow(reduceme.g, why);

	reduceme.b = pow(colorInput.b, why);  
	reduceme.b *= radooct;	
	reduceme.b = float(floor(reduceme.b));	
	reduceme.b /= radooct; 
	reduceme.b = pow(reduceme.b, why);

	colorInput.rgb = reduceme.rgb;

	// Add the purple line of lineness here, so the filter process catches it and gets gammaed.
	{
		float leifx_linegamma = (LEIFX_LINES / 10);
		float horzline1 = 	(fmod(ditheu.y, 2.0));
		if (horzline1 < 1)	leifx_linegamma = 0;
	
		colorInput.r += leifx_linegamma;
		colorInput.g += leifx_linegamma;
		colorInput.b += leifx_linegamma;	
	}

   return colorInput;
}

float4 PS_3DFX1(float4 vpos : SV_Position, float2 texcoord : TEXCOORD) : SV_Target
{
   float4 colorInput = tex2D(ReShade::BackBuffer, texcoord);
   float2 pixel;

	pixel.x = 1 / ReShade::ScreenSize.x;
	pixel.y = 1 / ReShade::ScreenSize.y;

	float3 pixel1 = tex2D(ReShade::BackBuffer, texcoord + float2((pixel.x), 0)).rgb;
	float3 pixel2 = tex2D(ReShade::BackBuffer, texcoord + float2(-pixel.x, 0)).rgb;
	float3 pixelblend;

	// New filter
	{
		float3 pixeldiff;
		float3 pixelmake;		
		float3 pixeldiffleft;

		pixelmake.rgb = 0;
		pixeldiff.rgb = pixel2.rgb- colorInput.rgb;

		pixeldiffleft.rgb = pixel1.rgb - colorInput.rgb;

		if (pixeldiff.r > FILTCAP) 		pixeldiff.r = FILTCAP;
		if (pixeldiff.g > FILTCAPG) 		pixeldiff.g = FILTCAPG;
		if (pixeldiff.b > FILTCAP) 		pixeldiff.b = FILTCAP;

		if (pixeldiff.r < -FILTCAP) 		pixeldiff.r = -FILTCAP;
		if (pixeldiff.g < -FILTCAPG) 		pixeldiff.g = -FILTCAPG;
		if (pixeldiff.b < -FILTCAP) 		pixeldiff.b = -FILTCAP;

		if (pixeldiffleft.r > FILTCAP) 		pixeldiffleft.r = FILTCAP;
		if (pixeldiffleft.g > FILTCAPG) 	pixeldiffleft.g = FILTCAPG;
		if (pixeldiffleft.b > FILTCAP) 		pixeldiffleft.b = FILTCAP;

		if (pixeldiffleft.r < -FILTCAP) 	pixeldiffleft.r = -FILTCAP;
		if (pixeldiffleft.g < -FILTCAPG) 	pixeldiffleft.g = -FILTCAPG;
		if (pixeldiffleft.b < -FILTCAP) 	pixeldiffleft.b = -FILTCAP;

		pixelmake.rgb = (pixeldiff.rgb / 4) + (pixeldiffleft.rgb / 16);
		colorInput.rgb = (colorInput.rgb + pixelmake.rgb);
	}	

	return colorInput;
}

float4 PS_3DFX2(float4 vpos : SV_Position, float2 texcoord : TEXCOORD) : SV_Target
{
   float4 colorInput = tex2D(ReShade::BackBuffer, texcoord);

	float2 res;
	res.x = ReShade::ScreenSize.x;
	res.y = ReShade::ScreenSize.y;

	// Gamma scanlines
	// the Voodoo drivers usually supply a 1.3 gamma setting whether people liked it or not
	// but it was enough to brainwash the competition for looking 'too dark'

	colorInput.r = pow(abs(colorInput.r), 1.0 / GAMMA_LEVEL);
	colorInput.g = pow(abs(colorInput.g), 1.0 / GAMMA_LEVEL);
	colorInput.b = pow(abs(colorInput.b), 1.0 / GAMMA_LEVEL);

   return colorInput;
}

technique LeiFx_Tech
{
	pass LeiFx
	{
		VertexShader = PostProcessVS;
		PixelShader = PS_3DFX;
	}
	pass LeiFx1
	{
		VertexShader = PostProcessVS;
		PixelShader = PS_3DFX1;
	}
	pass LeiFx2
	{
		VertexShader = PostProcessVS;
		PixelShader = PS_3DFX1;
	}
	pass LeiFx3
	{
		VertexShader = PostProcessVS;
		PixelShader = PS_3DFX1;
	}
	pass LeiFx4
	{
		VertexShader = PostProcessVS;
		PixelShader = PS_3DFX1;
	}
	pass LeiFx5
	{
		VertexShader = PostProcessVS;
		PixelShader = PS_3DFX2;
	}
}
