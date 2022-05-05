/*
	Graphics Adapter Pro by Vladmir Storm
	
	Ported by luluco250	and Matsilagi
*/

#include "ReShade.fxh"

//defines//////////////////////////////////////////////////////////////////////////////////////////////

//user variables///////////////////////////////////////////////////////////////////////////////////////

uniform bool bGAP_doResolution <
	ui_type = "combo";
	ui_label = "Custom Screen Resolution [Graphics Adapter Pro]";
	ui_tooltip = "The picture will be quantized vertically and horizontally.";
> = false;

uniform bool bGAP_screenStretch <
	ui_type = "combo";
	ui_label = "Stretch Screen [Graphics Adapter Pro]";
	ui_tooltip = "Stretches the picture to the size of the whole window.";
> = false;

uniform float screenPAR <
	ui_type = "drag";
	ui_min = 1.0; ui_max = 5.0;
	ui_label = "Pixel Aspect Ratio [Graphics Adapter Pro]";
	ui_tooltip = "This allows you to emulate non-square pixels. The picture will be stretched automatically vertically or horizontally depending on the value.";
> = 1.0;

uniform bool bGAP_alternateResolution <
	ui_type = "combo";
	ui_label = "Screen Resolution Division [Graphics Adapter Pro]";
	ui_tooltip = "Divides the screen resolution by the Pixel Size value. Can be used with Custom Screen Resolution, but isn't recommended.";
> = false;
		
uniform int iGAP_PixelSize <
	ui_type = "drag";
	ui_min = 1; ui_max = 16;
	ui_label = "Pixel Size [Graphics Adapter Pro]";
	ui_tooltip = "This changes the pixel size for the Screen Resolution Division.";
> = 8;

uniform int iGAP_Denoiser <
	ui_type = "drag";
	ui_min = 0; ui_max = 16;
	ui_label = "Denoiser [Graphics Adapter Pro]";
	ui_tooltip = "This simplifies the scene, making it less detailed. Recommended if using low resolutions to keep objects recognizable.";
> = 8;

uniform float iGAP_resolutionX <
	ui_type = "drag";
	ui_min = 1.0;
	ui_max = BUFFER_WIDTH;
	ui_label = "Screen Resolution Width [Graphics Adapter Pro]";
	ui_tooltip = "The pixels per width in the output picture.";
> = 320;

uniform float iGAP_resolutionY <
	ui_type = "drag";
	ui_min = 1.0;
	ui_max = BUFFER_HEIGHT;
	ui_label = "Screen Resolution Height [Graphics Adapter Pro]";
	ui_tooltip = "The pixels per height in the output picture.";
> = 240;

uniform float3 f3GAP_backgroundColor <
	ui_type = "color";
	ui_label = "Background Color [Graphics Adapter Pro]";
	ui_tooltip = "Colors the unstretched part of the screen.";
> = float3(0.0,0.0,0.0);

uniform bool bGAP_useBackgroundTexture <
	ui_type = "combo";
	ui_label = "Background Texture [Graphics Adapter Pro]";
	ui_tooltip = "Use a texture for the background.\nSet its size and name in Preprocessor Definitions incase needed. (texBKName/bakTexSize)";
> = false;

uniform float iGAP_bakSize <
	ui_type = "drag";
	ui_min = 1.0; ui_max = 4096.0;
	ui_label = "Background Size [Graphics Adapter Pro]";
	ui_tooltip = "Pattern Repeat size. You can set it to 1 incase you wanna use a wallpaper.";
	ui_step = 1.0;
> = 8.0;

uniform int iGAP_colorMode <
	ui_type = "combo";
	ui_items = "RGB\0YIQ (YI-Synced)\0YIQ (NTSC)\0Grayscale\0";
	ui_label = "Color Mode [Graphics Adapter Pro]";
	ui_tooltip = "Changes the color encoding mode.";
> = 1;

uniform bool bGAP_syncChannels <
	ui_type = "combo";
	ui_label = "Sync Color Channels [Graphics Adapter Pro]";
	ui_tooltip = "Syncs the colors bits per pixel.";
> = true;

uniform int iGAP_bits <
	ui_type = "drag";
	ui_min = 0;
	ui_max = 255;
	ui_label = "Synced Color Bits [Graphics Adapter Pro]";
	ui_tooltip = "Amount of the color bits. (Synced Mode / Grayscale)";
> = 2;

uniform int3 i3GAP_colorBits <
	ui_type = "drag";
	ui_min = 1;
	ui_max = 255;
	ui_label = "Separate Color Bits [Graphics Adapter Pro]";
	ui_tooltip = "Amount of Synced color bits per channel (RGB/YIQ (NTSC))";
> = int3(2, 2, 2);

uniform int2 i2GAP_LCbits <
	ui_type = "drag";
	ui_min = 0;
	ui_max = 255;
	ui_label = "Luma|Chroma Color Bits [Graphics Adapter Pro]";
	ui_tooltip = "Amount of synced Luma and Chroma bits. (YI-Synced)";
> = int2(16, 2);

uniform float3 f3GAP_grayscaleColor <
	ui_type = "color";
	ui_label = "Grayscale Color [Graphics Adapter Pro]";
	ui_tooltip = "Color of the Grayscale.";
> = float3(0.5, 0.5, 0.5);

uniform bool bGAP_absoluteGrayscale <
	ui_type = "combo";
	ui_label = "Absolute Grayscale [Graphics Adapter Pro]";
	ui_tooltip = "Completely colors the Grayscale when using palettes.";
> = false;

uniform int bGAP_alternateGrayscale <
	ui_type = "combo";
	ui_items = "RGB\0YUV/YIQ\0ATSC\0";
	ui_label = "Grayscale Mode [Graphics Adapter Pro]";
	ui_tooltip = "Alternate Grayscale algorithms.";
> = 0;

uniform int iGAP_ditherMode <
	ui_type = "combo";
	ui_items = "No Dithering\0Horizontal Lines\0Vertical Lines\0Ordered 2x2\0Ordered Alternative 2x2\0Uniform Noise\0Triangle Noise\0Blue Noise\0Analytical Matrix\0Screenspace Dither\0Interleaved Gradient Noise\0Ceejay Dithering\0Three-Step Noise\0Bayer 8x8\0R4 Unit Pattern\0Tri-Dither\0PS1 Dither\0";
	ui_label = "Dither Mode [Graphics Adapter Pro]";
	ui_tooltip = "Switches between dithering patterns.";
> = 0;

uniform int iGAP_bayerMode <
	ui_type = "combo";
	ui_items = "Ordered 2x2\0Ordered 4x4\0Ordered 8x8\0Ordered 16x16\0Ordered 32x32\0Ordered 64x64\0Ordered 128x128\0";
	ui_label = "Bayer Mode [Graphics Adapter Pro]";
	ui_tooltip = "Switches between bayer patterns (Analytical Matrix).";
> = 0;

uniform float fGAP_ditherAmount <
	ui_type = "drag";
	ui_min = -15.0;
	ui_max = 15.0;
	ui_label = "Dither Amount [Graphics Adapter Pro]";
	ui_tooltip = "Amount of dithering applied on the image.";
> = 1.0;

uniform bool bGAP_temporalNoise <
	ui_type = "combo";
	ui_label = "Temporal Dithering [Graphics Adapter Pro]";
	ui_tooltip = "Makes Noise ditherings temporal.";
> = false;

uniform bool bGAP_doSubsampling <
	ui_type = "combo";
	ui_label = "Subsampling [Graphics Adapter Pro]";
	ui_tooltip = "Enables Chroma Subsampling.";
> = false;

uniform int iGAP_subsampleWidth <
	ui_type = "drag";
	ui_min = 1;
	ui_max = 8;
	ui_label = "Subsampling Width [Graphics Adapter Pro]";
	ui_tooltip = "Width of the Chroma Subsampling.";
> = 1;

uniform int iGAP_subsampleHeight <
	ui_type = "drag";
	ui_min = 1;
	ui_max = 8;
	ui_label = "Subsampling Height [Graphics Adapter Pro]";
	ui_tooltip = "Height of the Chroma Subsampling.";
> = 1;

uniform bool bGAP_usePalette <
	ui_type = "combo";
	ui_label = "Use Palette [Graphics Adapter Pro]";
	ui_tooltip = "Uses a specific color palette.\nSet its size and name in Preprocessor Definitions incase needed. (texName/paletteSize)";
> = false;

uniform bool bGAP_useRPalette <
	ui_type = "combo";
	ui_label = "Use Replacement Palette [Graphics Adapter Pro]";
	ui_tooltip = "Uses a replacement color palette.\nSet its size and name in Preprocessor Definitions incase needed. (texRName/rpaletteSize)";
> = false;

uniform int iGAP_delta <
	ui_type = "drag";
	ui_min = 0;
	ui_max = 30;
	ui_label = "Palette Accuracy [Graphics Adapter Pro]";
	ui_tooltip = "Changes how many colors it replaces.\nRealtime changes only on DX10/11/OpenGL, change iGAP_deltaDX9 in Preprocessor Definitions to change this in DX9";
> = 5;

uniform int iGAP_Filter <
	ui_type = "drag";
	ui_min ="0.0";
	ui_max ="0.0";
	ui_label = "Linear Filtering [Graphics Adapter Pro]";
	ui_tooltip = "Filters the image linearly, increasing quality.\nDefine GAPLINEARFILTERING in Preprocessor Definitions to take effect, this is only here as a reminder.";
> = 0.0;

//system variables/////////////////////////////////////////////////////////////////////////////////////

static const float3 MOD3 = float3(443.8975, 397.2973, 491.1871);

uniform float Timer < source = "timer"; >;

#ifndef iGAP_deltaDX9
	#define iGAP_deltaDX9 5 //DX9 Fix
#endif

#ifndef paletteSize
	#define paletteSize 8
#endif

#ifndef rpaletteSize
	#define rpaletteSize 8
#endif

#ifndef bakTexSize
	#define bakTexSize 64
#endif

#ifndef texName
	#define texName "teletext.png"
#endif

#ifndef texRName
	#define texRName "secam.png"
#endif

#ifndef texBKName
	#define texBKName "GRNROCK.png"
#endif

#if _RENDERER_ == 0x09300
	#define iGAP_int uint
#else
	#define iGAP_int int
#endif

#ifdef GAPLINEARFILTER
	#define GAPFILTERMODE LINEAR
#else
	#define GAPFILTERMODE POINT
#endif

//textures/////////////////////////////////////////////////////////////////////////////////////////////

texture tSrcPalette <source="Palettes/" texName;> { Width=paletteSize; Height=1;};
texture tSrcRPalette <source="Palettes/" texRName;> { Width=rpaletteSize; Height=1;};
texture tBackground <source="Backgrounds/" texBKName;> { Width=bakTexSize; Height=bakTexSize;};

//samplers/////////////////////////////////////////////////////////////////////////////////////////////

sampler _PaletteTex { Texture=tSrcPalette; MinFilter=POINT; MagFilter=POINT; };
sampler _ReplaceTex { Texture=tSrcRPalette; MinFilter=POINT; MagFilter=POINT; };

sampler2D SamplerColorGAP
{
	Texture = ReShade::BackBufferTex;
	MinFilter = GAPFILTERMODE;
	MagFilter = GAPFILTERMODE;
	MipFilter = GAPFILTERMODE;
	AddressU = Clamp;
	AddressV = Clamp;
};

sampler2D sBackground
{
	Texture = tBackground;
	MinFilter = GAPFILTERMODE;
	MagFilter = GAPFILTERMODE;
	MipFilter = GAPFILTERMODE;
	AddressU = REPEAT;
	AddressV = REPEAT;
};

//functions////////////////////////////////////////////////////////////////////////////////////////////

float mod(float x, float y){
	return (x-y*floor(x/y));
}

float fmod(float a, float b)
{
  float c = frac(abs(a/b))*abs(b);
  return (a < 0) ? -c : c;   /* if ( a < 0 ) c = 0-c */
}

float hash12(float2 p) {
	float3 p3  = frac(float3(p.xyx) * MOD3);
	p3 += dot(p3, p3.yzx + 19.19);
	return frac((p3.x + p3.y) * p3.z);
}

float3 rgb2yuvn(float3 c){ 					
	float y = 0.299*c.x + 0.587*c.y + 0.114*c.z;
	c = float3( y, 0.5 * (c.z - y) / (1.0 - 0.114), 0.5 * (c.x - y) / (1.0 - 0.299));
	c.yz += float(0.5).xx;
	return c;
}

float3 yuvn2rgb(float3 c){	 
	c.yz -= float(0.5).xx;						
	return float3( c.x + c.z*(1.0 - 0.299)/0.5,
				  c.x - c.y*0.114*(1.0 - 0.114)/(0.5*0.587) - c.z*0.299*(1.0 - 0.299)/(0.5*0.587),						
				  c.x + c.y*(1.0 - 0.114)/0.5 );
}

float2 p_sh(float2 p_){
			
	float2 ar = float2(1.0,1.0); //final aspect ratio (calculated)
	
	ar = float(1.0).xx;
	
	return 0.5*float2(step(0.0, 1.0-ar.y)*(1.0-1.0/ar.y),
					  step(1.0, ar.y)*(1.0-ar.y));
}

//Analytical Bayer
float bayer2(float2 a){
    a = floor(a);
    return frac(dot(a,float2(.5, a.y*.75)));
}

float bayer4(float2 a)   {return bayer2( .5*a)   * .25     + bayer2(a); }
float bayer8(float2 a)   {return bayer4( .5*a)   * .25     + bayer2(a); }
float bayer16(float2 a)  {return bayer4( .25*a)  * .0625   + bayer4(a); }
float bayer32(float2 a)  {return bayer8( .25*a)  * .0625   + bayer4(a); }
float bayer64(float2 a)  {return bayer8( .125*a) * .015625 + bayer8(a); }
float bayer128(float2 a) {return bayer16(.125*a) * .015625 + bayer8(a); }

//ScreenSpaceDither
float3 ScreenSpaceDither( float2 vScreenPos )
{
	// Iestyn's RGB dither (7 asm instructions) from Portal 2 X360, slightly modified for VR
	//vec3 vDither = vec3( dot( vec2( 171.0, 231.0 ), vScreenPos.xy + iTime ) );
    float3 vDither = float3( dot( float2( 171.0, 231.0 ), vScreenPos.xy ), dot( float2( 171.0, 231.0 ), vScreenPos.xy ), dot( float2( 171.0, 231.0 ), vScreenPos.xy ) );
    vDither.rgb = frac( vDither.rgb / float3( 103.0, 71.0, 97.0 ) );
    return vDither.rgb / 255.0; //note: looks better without 0.375...

    //note: not sure why the 0.5-offset is there...
    //vDither.rgb = fract( vDither.rgb / vec3( 103.0, 71.0, 97.0 ) ) - vec3( 0.5, 0.5, 0.5 );
	//return (vDither.rgb / 255.0) * 0.375;
}

//Interleaved Gradient Noise
float InterleavedGradientNoise( float2 uv )
{
    static const float3 magic = float3( 0.06711056, 0.00583715, 52.9829189 );
    return frac( magic.z * frac( dot( uv, magic.xy ) ) );
}

//3-step noise
float Noise(float2 n,float x){n+=x;return frac(sin(dot(n.xy,float2(12.9898, 78.233)))*43758.5453)*2.0-1.0;}
float Step1(float2 uv,float n){
    float a=1.0,b=2.0,c=-12.0,t=1.0;   
    return (1.0/(a*4.0+b*4.0-c))*(
        Noise(uv+float2(-1.0,-1.0)*t,n)*a+
        Noise(uv+float2( 0.0,-1.0)*t,n)*b+
        Noise(uv+float2( 1.0,-1.0)*t,n)*a+
        Noise(uv+float2(-1.0, 0.0)*t,n)*b+
        Noise(uv+float2( 0.0, 0.0)*t,n)*c+
        Noise(uv+float2( 1.0, 0.0)*t,n)*b+
        Noise(uv+float2(-1.0, 1.0)*t,n)*a+
        Noise(uv+float2( 0.0, 1.0)*t,n)*b+
        Noise(uv+float2( 1.0, 1.0)*t,n)*a+
        0.0);}

float Step2(float2 uv,float n){
    float a=1.0,b=2.0,c=-2.0,t=1.0;
    return (4.0/(a*4.0+b*4.0-c))*(
        Step1(uv+float2(-1.0,-1.0)*t,n)*a+
        Step1(uv+float2( 0.0,-1.0)*t,n)*b+
        Step1(uv+float2( 1.0,-1.0)*t,n)*a+
        Step1(uv+float2(-1.0, 0.0)*t,n)*b+
        Step1(uv+float2( 0.0, 0.0)*t,n)*c+
        Step1(uv+float2( 1.0, 0.0)*t,n)*b+
        Step1(uv+float2(-1.0, 1.0)*t,n)*a+
        Step1(uv+float2( 0.0, 1.0)*t,n)*b+
        Step1(uv+float2( 1.0, 1.0)*t,n)*a+
        0.0);}

float3 Step3(float2 uv){
    float a=Step2(uv,0.07);    
    return float3(a,a,a);
}

float3 Step3T(float2 uv){
    float a=Step2(uv,0.07*(frac(Timer*0.001)+1.0));    
    float b=Step2(uv,0.11*(frac(Timer*0.001)+1.0));    
    float c=Step2(uv,0.13*(frac(Timer*0.001)+1.0));
    return float3(a,b,c);}
	
//Bayer 8x8
int get_bayer(int2 i) {
    static const int bayer[8 * 8] = {
          0, 48, 12, 60,  3, 51, 15, 63,
         32, 16, 44, 28, 35, 19, 47, 31,
          8, 56,  4, 52, 11, 59,  7, 55,
         40, 24, 36, 20, 43, 27, 39, 23,
          2, 50, 14, 62,  1, 49, 13, 61,
         34, 18, 46, 30, 33, 17, 45, 29,
         10, 58,  6, 54,  9, 57,  5, 53,
         42, 26, 38, 22, 41, 25, 37, 21
    };
    return bayer[i.x + 8 * i.y];
}

// Adapted from: http://devlog-martinsh.blogspot.com.br/2011/03/glsl-dithering.html
float dither_bayer8(float x, float2 uv, float2 res) {

	x *= fGAP_ditherAmount;

    int2 index = int2(uv * res) % 8;
    float limit = (index < 8) ? float(get_bayer(index) + 1) / 64.0
                                : 0.0;

    if (x < limit)
        return 0.0;
    else
        return 1.0;
}

//Tri-Dither
#define remap(v, a, b) (((v) - (a)) / ((b) - (a)))
float rand21(float2 uv)
{
    float2 noise = frac(sin(dot(uv, float2(12.9898, 78.233) * 2.0)) * 43758.5453);
    return (noise.x + noise.y) * 0.5;
}

float rand11(float x) { return frac(x * 0.024390243); }
float permute(float x) { return ((34.0 * x + 1.0) * x) % 289.0; }

float3 triDither(float3 color, float2 uv, float timer)
{
	static const float bitstep = pow(2.0, 8) - 1.0;
	static const float lsb = 1.0 / bitstep;
	static const float lobit = 0.5 / bitstep;
	static const float hibit = (bitstep - 0.5) / bitstep;

	float3 m = float3(uv, rand21(uv + timer)) + 1.0;
	float h = permute(permute(permute(m.x) + m.y) + m.z);

	float3 noise1, noise2;
	noise1.x = rand11(h); h = permute(h);
	noise2.x = rand11(h); h = permute(h);
	noise1.y = rand11(h); h = permute(h);
	noise2.y = rand11(h); h = permute(h);
	noise1.z = rand11(h); h = permute(h);
	noise2.z = rand11(h);

	float3 lo = saturate(remap(color.xyz, 0.0, lobit));
	float3 hi = saturate(remap(color.xyz, 1.0, hibit));
	float3 uni = noise1 - 0.5;
	float3 tri = noise1 - noise2;
	return lerp(uni, tri, min(lo, hi)) * lsb;
}

//PS1 Dither
static const int4x4 psx_dither_table = int4x4
(
    0,     8,     2,   10,
    12,    4,    14,   6,
	3,    11,     1,   9,
    15,    7,    13,   5
);

#define mod1(x,y) (x-y*floor(x/y))
float3 dither(float3 col, float2 p) {

	float2 xx = float2(320.0, 240.0); //output screen res				
	
	xx = float2((float)BUFFER_WIDTH,(float)BUFFER_HEIGHT);
	
	if (bGAP_doResolution) {
		xx = float2(iGAP_resolutionX,iGAP_resolutionY);
	}
	
	float2 p_ = p;
	float2 shp_ = p_ * xx; //UVs in Texels for Shadertoy
	
	float3 col_og = tex2D(ReShade::BackBuffer,p_).xyz;

	//Dither modes :

	//0 No dithering
	float d = 0; // dither value (mask)

	//1 horizontal lines		
	if(iGAP_ditherMode == 1){	
		float pxx_y = ((p_-p_sh(p_))*xx.y+p_sh(p_)).y;
		d = floor(fmod(pxx_y, 2.0)).xxx*0.5;			
	}

	//2 vertical lines	
	if(iGAP_ditherMode == 2){				
		float pxx_x = ((p_.x-p_sh(p_))*xx.x+p_sh(p_)).x;
		d = floor(fmod(pxx_x, 2.0)).xxx*0.5;
	}

	//3 2x2 ordered 
	if (iGAP_ditherMode == 3){
		float2 pxx = (p_-p_sh(p_))*xx+p_sh(p_);
		float2 ij = floor(fmod(pxx, float(2.0).xx));						
		float idx = ij.x + 2.0*ij.y;
		float4 m = step( abs(idx.xxxx-float4(0.0,1.0,2.0,3.0)), float(0.5).xxxx ) 
						* float4(0.75,0.25,0.00,0.50);
		d = m.x+m.y+m.z+m.w;
	}

	//4 alternate 2x2 ordered
	if(iGAP_ditherMode == 4){
		float2 pxx = (p_-p_sh(p_))*xx+p_sh(p_);	
		float2 s = floor( frac( floor(abs( pxx )) / 2.0 ) * 2.0 );						
		float f = (  2.0 * s.x + s.y  ) / 4.0;
		d = (f - 0.375);
	}

	//5 uniform noise
	if (iGAP_ditherMode == 5){
		d = hash12(p_).xxx;
		
		if (bGAP_temporalNoise){
			d = hash12(p_ + Timer*0.001).xxx;
		}
	}

	//6 triangle noise
	if (iGAP_ditherMode == 6){
        d = (hash12(p_) + hash12(p_ + 0.59374) - 0.5).xxx;
		
		if (bGAP_temporalNoise){
			d = (hash12(p_ + Timer*0.001) + hash12((p_ + 0.59374) + Timer*0.001) - 0.5).xxx;
		}
	}

	//7 blue noise
	if (iGAP_ditherMode == 7){
		d = mod(p_.x+p_.y+mod(208.+p_.x*3.58,13.+mod(p_.y*22.9, 9.)),7.)*.143-d;
	}
	
	//8 analytical bayer
	if (iGAP_ditherMode == 8){
		if (iGAP_bayerMode == 0){
			d = bayer2(shp_)-.375;
		} else if (iGAP_bayerMode == 1){
			d = bayer4(shp_)-.46875;
		} else if (iGAP_bayerMode == 2){
			d = bayer8(shp_)-.4921875;
		} else if (iGAP_bayerMode == 3){
			d = bayer16(shp_)-.498046875;
		} else if (iGAP_bayerMode == 4){
			d = bayer32(shp_)-.499511719;
		} else if (iGAP_bayerMode == 5){
			d = bayer64(shp_)-.49987793;
		} else if (iGAP_bayerMode == 6){
			d = bayer128(shp_)-.499969482;
		}
	}
	
	//9 screenspace dithering
	if (iGAP_ditherMode == 9){
		d = ScreenSpaceDither( shp_ ) * (fGAP_ditherAmount * 100);
	}
	
	//10 interleaved gradient noise
	if (iGAP_ditherMode == 10){
		d = (InterleavedGradientNoise( shp_ ) / 255.0) * (fGAP_ditherAmount*100);
	}
	
	//11 ceejay dithering
	if (iGAP_ditherMode == 11){
		//note: from comment by CeeJayDK
		float dither_bit = 8.0; //Bit-depth of display. Normally 8 but some LCD monitors are 7 or even 6-bit.	

		//Calculate grid position
		float grid_position = frac( dot( shp_.xy - float2(0.5,0.5) , float2(1.0/16.0,10.0/36.0) + 0.25 ) );

		//Calculate how big the shift should be
		float dither_shift = (0.25) * (1.0 / (pow(2.0,dither_bit) - 1.0));

		//Shift the individual colors differently, thus making it even harder to see the dithering pattern
		float3 dither_shift_RGB = float3(dither_shift, -dither_shift, dither_shift); //subpixel dithering

		//modify shift acording to grid position.
		dither_shift_RGB = lerp(2.0 * dither_shift_RGB, -2.0 * dither_shift_RGB, grid_position); //shift acording to grid position.

		//shift the color by dither_shift
		d = dither_shift_RGB * (fGAP_ditherAmount*100); 
	}
	
	//12 three-step dithering
	if (iGAP_ditherMode == 12){
		d = (0.5+Step3(shp_.xy))/255.0 * (fGAP_ditherAmount*100);
		if (bGAP_temporalNoise){
			d = (0.5+Step3T(shp_.xy))/255.0 * (fGAP_ditherAmount*100);
		}
	}
	
	//13 - Bayer 8x8 Matrix
	if (iGAP_ditherMode == 13){
		d = dither_bayer8(col,p_,xx);
	}
	
	//14 - R4_Unit Dither
	if (iGAP_ditherMode == 14){
		d = ((9*(int)shp_.x+5*(int)shp_.y)%11+0.5)/11;
	}
	
	//15 - Tri-Dither
	if (iGAP_ditherMode == 15){
		if (bGAP_temporalNoise){
			d = (triDither(d, p_, Timer.x)* (fGAP_ditherAmount*100));
		}
		else {
			d = (triDither(d, p_, 0.0)* (fGAP_ditherAmount*100));
		}
	}
	
	//16 - PS1 Dither
	if (iGAP_ditherMode == 16){
		int dither = psx_dither_table[int(p_.x) % 4][int(p_.y) % 4];
		d += (dither / 2.0 - 4.0); //dithering process as described in PSYDEV SDK documentation
	}
	
	//amount of colors per channel
	float3 dv = float(0.0).xxx;

	if (iGAP_colorMode == 3){
		dv = float(iGAP_bits).xxx;	
	}

					if(iGAP_colorMode <= 0){
						if(bGAP_syncChannels){
							dv = float(iGAP_bits).xxx;
						} else {
							dv = float3(i3GAP_colorBits.x, i3GAP_colorBits.y, i3GAP_colorBits.z);
						}
						// return float3(1.0, 0.0, 0.0);
					}
					if (iGAP_colorMode == 1){
						if(bGAP_syncChannels){
							dv = float(iGAP_bits).xxx;
						} else {
							dv = float3(i2GAP_LCbits.x, i2GAP_LCbits.y, i2GAP_LCbits.y);	
						}
					}
					
					if (iGAP_colorMode == 2){
						if(bGAP_syncChannels){
							dv = float(iGAP_bits).xxx;
						} else {
							dv = float3(i3GAP_colorBits.x, i3GAP_colorBits.y, i3GAP_colorBits.z);	
						}
					}
					
					//with rounding fix for better color matching
					col = floor( col.xyz * dv.xyz + fGAP_ditherAmount * d ) / (dv.xyz - float(1.0).xxx);
					// col = col.xyz * dv.xyz / dv.xyz;
					// if(dv.x==0.0&&dv.y==0.0&&dv.y==0.0) col = float3(1.0, 1.0, 1.0);
    				return col;
}

float3 grayscale(float3 col) {
	if(bGAP_alternateGrayscale==1){
		col.xyz = (col.x*0.299+col.y*0.587+col.z*0.114);
	} else if(bGAP_alternateGrayscale==2) {
		col.xyz = (col.x*0.2126+col.y*0.7152+col.z*0.0722);
	} else {
		col.xyz = (col.x+col.y+col.z).xxx/3.0;
	}
	col.rgb *= f3GAP_grayscaleColor;
	return col;
}

float3 subsampling(float3 c1, float2 p_){

	float2 xx = float2(320.0, 240.0); //output screen res				
	
	xx = float2((float)BUFFER_WIDTH,(float)BUFFER_HEIGHT);

	float2 one = 1.0/xx;
	float2 ss_size = float2(4.0, 4.0);  //TODO maybe event less than half
	ss_size = float2(iGAP_subsampleWidth, iGAP_subsampleHeight); 

	float2 xx_ = xx/ss_size;

	float3 c2 = tex2D( SamplerColorGAP, p_ + ss_size*one*0.5 ).rgb;
						
	if (iGAP_colorMode == 1) {
		c2 = rgb2yuvn(c2); //, 0.436, 0.615 
	}
	
	else if (iGAP_colorMode == 2) {
		c2 = rgb2yuvn(c2); //, 0.436, 0.615 
	}
	
	else if (iGAP_colorMode == 3) {
		c2 = (c2.x+c2.y+c2.z).xxx/3.0;
	}
						
	c1.y = (c1.y + c2.y)*0.5;
	c1.z = (c1.z + c2.z)*0.5;

	return c1;
}

float3 len(float3 a, float3 b){
	return abs(a.x-b.x)+abs(a.y-b.y)+abs(a.z-b.z);
}

float3 doPalette (float3 c){

	float ox = 1.0/paletteSize; //1px in palette
	
	int delta = 5;
	
	//DX9 Fix
	if (__RENDERER__ != 0x09300){
		delta = iGAP_delta;
	} else {delta = iGAP_deltaDX9;}

		//TODO maybe sort buy YIQ-Q?
			float3 mcol = c.xyz; //this is current color 
			float mc = (mcol.x+mcol.y+mcol.z)/3.0; //back color	

			//TODO paletteSize/step etc. in int!
			float step = float(paletteSize)/2.0;
			float pos = step;
			float pc = 0.0;

			//find closest grayscale color 
			while(step>=1.0){				
				float4 gr = tex2D( _PaletteTex, float2(pos * ox, 0.5)).rgba;
				if(bGAP_alternateGrayscale==1){
					gr.a = (gr.r*0.299+gr.g*0.587+gr.b*0.114);
				}
				else if(bGAP_alternateGrayscale==2){
					gr.a = (gr.r*0.2126+gr.g*0.7152+gr.b*0.0722);
				} else { 
					gr.a = (gr.r+gr.g+gr.b)/3;
				}
				pc = gr.a;
				step /= 2.0;
				if(pc>mc) pos -= step; else pos += step;						
			}
			
			//adjustment
				iGAP_int firstPos = pos; // for debug
				iGAP_int bestPos = pos;
				float3 cc = tex2D( _PaletteTex, float2(pos * ox,0.5)).rgb;
				float l = distance(cc, mcol);
				float nl = 0.0;// new len
				float pp = 0.0; //TODO int
				// for(int ii=-delta;ii<delta;ii++){ //i is input!
				[loop]
				float ii = -delta; while(ii<delta){ ii += 1.0;
					pp = pos+ii;
					if( pp>=0.0 && pp<float(paletteSize) ){
					#if !defined(__RESHADE__) || __RESHADE__ > 30000
						cc = tex2Dlod(_PaletteTex, float4(pp * ox, 0.5,0,0)).rgb;
					#else
						cc = tex2D( _PaletteTex, float2( pp * ox, 0.5)).rgb;
					#endif
						nl = distance(cc, mcol);
					if(nl<l) {
							l = nl;//wasnt here
							bestPos = pp;
						}
					}
				}
				
			c.xyz = tex2D( _PaletteTex, float2(bestPos * ox, 0.5)).rgb;
			
			if(bGAP_useRPalette){
				c.xyz = tex2D( _ReplaceTex, float2(bestPos * ox, 0.5)).rgb;
			}
			
		return c;
}

//shaders//////////////////////////////////////////////////////////////////////////////////////////////

float3 paint_filter(in float2 uv, in float pass_id)
{
	float3 least_divergent = 0;
	float3 total_sum = 0;
	float min_divergence = 1e10;
	int GAP_NUM_STEPS_PER_PASS = min(4, iGAP_Denoiser);
	float3 col_untouched = tex2D(SamplerColorGAP,uv).rgb;
		
	[loop]
	for(int j = 0; j < 6; j++)
	{
		float2 dir; sincos(radians(180.0 * (j + pass_id / 3) / 6), dir.y, dir.x);

		float3 col_avg_per_dir = 0;
		float curr_divergence = 0;

		float3 col_prev = tex2Dlod(SamplerColorGAP , float4(uv.xy - dir * GAP_NUM_STEPS_PER_PASS * ReShade::PixelSize, 0, 0)).rgb;

		for(int k = -GAP_NUM_STEPS_PER_PASS + 1; k <= GAP_NUM_STEPS_PER_PASS; k++)
		{
			float3 col_curr = tex2Dlod(SamplerColorGAP , float4(uv.xy + dir * k * ReShade::PixelSize, 0, 0)).rgb;
			col_avg_per_dir += col_curr;

			float3 color_diff = abs(col_curr - col_prev);

			curr_divergence += max(max(color_diff.x, color_diff.y), color_diff.z);
			col_prev = col_curr;
		}
			
		[flatten]
		if(curr_divergence < min_divergence)
		{
			least_divergent = col_avg_per_dir;
			min_divergence = curr_divergence;
		}

		total_sum += col_avg_per_dir;
	}

	least_divergent /= 2 * GAP_NUM_STEPS_PER_PASS;
	total_sum /= 2 * GAP_NUM_STEPS_PER_PASS * 6;
	min_divergence /= 2 * GAP_NUM_STEPS_PER_PASS;

	//float lumasharpen = dot(least_divergent - total_sum, 0.333);
	//least_divergent += lumasharpen * SHARPEN_INTENSITY;

	//least_divergent *= saturate(1 - min_divergence * OUTLINE_INTENSITY * 0.5);
	if (iGAP_Denoiser <= 0){
		return col_untouched;
	} else {
		return least_divergent;
	}
}

void PS_Paint_1(in float4 position : SV_Position, in float2 uv: TEXCOORD, out float4 o : SV_Target){
	o.rgb = paint_filter(uv, 1);
	o.w = 1;
}

void PS_Paint_2(in float4 position : SV_Position, in float2 uv: TEXCOORD, out float4 o : SV_Target){
	o.rgb = paint_filter(uv, 2);
	o.w = 1;
}

void PS_Paint_3(in float4 position : SV_Position, in float2 uv: TEXCOORD, out float4 o : SV_Target){
	o.rgb = paint_filter(uv, 3);
	o.w = 1;
}

float3 GraphicsAdapterPro(float4 pos : SV_Position, float2 uv : TEXCOORD) : SV_Target {	
	
	float2 p = uv.xy;
	float2 p_ = p*float2((float)BUFFER_WIDTH,(float)BUFFER_HEIGHT);
	float2 xx = float2(320.0,240.0);
	float2 ar = float2(1.0,1.0); //final aspect ratio (calculated)
	
	xx = ReShade::ScreenSize.xy;
	ar = float(1.0).xx;
	
	if (bGAP_doResolution){
	
		xx = float2(iGAP_resolutionX, iGAP_resolutionY);
	
		if(bGAP_screenStretch==0){
				float sar = ReShade::ScreenSize.x/ReShade::ScreenSize.y;
				float nar = xx.x/xx.y;
				ar = float2(1.0, nar/sar*screenPAR);		
			}
			
		p_ = p;
		p_ -= 0.5;
		p_ *= ar;
		p_ += 0.5;
				
		if(ar.y<1.0){p_ -= 0.5; p_ = p_*(1.0/ar.y); p_ += 0.5; }
		p_ = floor(p_*xx)/xx;
		p_ +=  p_sh(p_);
	} else {
		p_ = uv;
	}
	
	if (bGAP_alternateResolution){
		p_ = floor(pos.xy / iGAP_PixelSize) * iGAP_PixelSize + iGAP_PixelSize * 0.5;
		p_ /= ReShade::ScreenSize.xy;
	}
	
	float3 col = tex2D(SamplerColorGAP, p_).rgb;
	
	col = iGAP_colorMode == 1 || iGAP_colorMode == 2 ? rgb2yuvn(col) : iGAP_colorMode == 3 ? grayscale(col) : col;
	
	col = bGAP_doSubsampling ? subsampling(col, p_) : col;
	
	col = dither(col, p_);
	
	col = iGAP_colorMode == 1 || iGAP_colorMode == 2 ? yuvn2rgb(col) : iGAP_colorMode == 3 ? grayscale(col) : col;
	
	if( p_.x<0.0 || p_.x>=1.0 || p_.y<0.0 || p_.y>=1.0) {
		if (bGAP_useBackgroundTexture) {
			float3 backcol = tex2D(sBackground, uv*iGAP_bakSize).rgb;
			backcol = iGAP_colorMode == 1 || iGAP_colorMode == 2 ? rgb2yuvn(backcol) : iGAP_colorMode == 3 ? grayscale(backcol) : backcol;
			backcol = iGAP_colorMode == 1 || iGAP_colorMode == 2 ? yuvn2rgb(backcol) : iGAP_colorMode == 3 ? grayscale(backcol) : backcol;
			col = backcol;
		 } else {
			col = f3GAP_backgroundColor;
		}
	}
	
	col = bGAP_usePalette ? (doPalette(col.xyz)) : col;
	
	if (bGAP_usePalette && iGAP_colorMode == 3 && bGAP_absoluteGrayscale){
		col = grayscale(col);
	}
	
	return float4(col.xyz,1.0);
}

//techniques///////////////////////////////////////////////////////////////////////////////////////////

technique GraphicsAdapterPro {

	pass GraphicsAdapterPro_Alt1
	{
		VertexShader = PostProcessVS;
		PixelShader  = PS_Paint_1;
	}
	
	pass GraphicsAdapterPro_Alt2
	{
		VertexShader = PostProcessVS;
		PixelShader  = PS_Paint_2;
	}
		
	pass GraphicsAdapterPro_Alt3
	{
		VertexShader = PostProcessVS;
		PixelShader  = PS_Paint_3;
	}
	
	pass GraphicsAdapterPro {
		VertexShader=PostProcessVS;
		PixelShader=GraphicsAdapterPro;
	}
}