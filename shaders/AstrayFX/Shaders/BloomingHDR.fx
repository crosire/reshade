 ////----------------//
 ///**Blooming HDR**///
 //----------------////

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////                                               																									*//
//*For Reshade 3.0+ HDR Bloom
//*--------------------------
//*                                                                   Blooming HDR
//*
//* Due Diligence
//* Eye Adaptation
//* https://knarkowicz.wordpress.com/2016/01/09/automatic-exposure/ Search Ref. Temporal Adaptation
//* Prods80 AKA ThatRedDot port of exposure.
//* Additional credits (exposure)
//* - Padraic Hennessy for the logic
//*   https://placeholderart.wordpress.com/2014/11/21/implementing-a-physically-based-camera-manual-exposure/
//* - Padraic Hennessy for the logic
//*   https://placeholderart.wordpress.com/2014/12/15/implementing-a-physically-based-camera-automatic-exposure/
//* - MJP and David Neubelt for the method
//*   https://github.com/TheRealMJP/BakingLab/blob/master/BakingLab/Exposure.hlsl
//*   License: MIT, Copyright (c) 2016 MJP
//* Reinhard by Tom Madams
//* http://imdoingitwrong.wordpress.com/2010/08/19/why-reinhard-desaturates-my-blacks-3/
//* Uncharted2 Tone Mapper by Hable John
//* https://www.slideshare.net/ozlael/hable-john-uncharted2-hdr-lighting
//* ACES Cinematic Tonemapping by Krzysztof Narkowicz and one by Stephen Hill
//* https://github.com/TheRealMJP/BakingLab/blob/master/BakingLab/ACES.hlsl
//* https://knarkowicz.wordpress.com/2016/01/06/aces-filmic-tone-mapping-curve/
//* Timothy Lottes Tone mapper and information from this site.
//* https://bartwronski.com/2016/09/01/dynamic-range-and-evs/
//* https://www.shadertoy.com/view/XljBRK
//* Generate Gold Noise image Based on this implamentation
//* https://www.shadertoy.com/view/wtsSW4
//* Depth Bloom CeeJay.dk
//* His brain told me to do it...... I just added it.
//* Adyss Bloom used as Insperation for Bloom Rework. Miss you man. Hope you come back.
//*
//* Since it was already witten out nicely was ported from. FidelityFX LPM was it self was not ported.
//* https://github.com/GPUOpen-Effects/FidelityFX-LPM
//* Most of the code is LICENSED this way.
//* https://github.com/GPUOpen-Effects/FidelityFX-LPM/blob/4a1442bf7405d0f703f7cf9c0bfe47a7559cc69b/sample/src/DX12/LPMSample.h#L3-L18
//*
//* The rest of the code below is under it's own license.
//* If I missed any please tell me.
//*
//* LICENSE
//* ============
//* Overwatch & Blooming HDR is licenses under: Attribution-NoDerivatives 4.0 International
//*
//* You are free to:
//* Share - copy and redistribute the material in any medium or format
//* for any purpose, even commercially.
//* The licensor cannot revoke these freedoms as long as you follow the license terms.
//* Under the following terms:
//* Attribution - You must give appropriate credit, provide a link to the license, and indicate if changes were made.
//* You may do so in any reasonable manner, but not in any way that suggests the licensor endorses you or your use.
//*
//* NoDerivatives - If you remix, transform, or build upon the material, you may not distribute the modified material.
//*
//* No additional restrictions - You may not apply legal terms or technological measures that legally restrict others from doing anything the license permits.
//*
//* https://creativecommons.org/licenses/by-nd/4.0/
//*
//* Have fun,
//* Jose Negrete AKA BlueSkyDefender
//*
//* https://github.com/BlueSkyDefender/Depth3D
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//Shared Texture for Blooming HDR or any other shader that want's to use it.
#if exists "Flair.fx"                                                   //Flair Interceptor//
	#define Flare_A 1
#else
	#define Flare_A 0
#endif

#if exists "Flare.fx"                                                   //Flare Interceptor//
	#define Flare_B 1
#else
	#define Flare_B 0
#endif

// Change this to set enable SRGB mode.
#define SRGB 0

//Do not Use.
//Use for real HDR.
#define HDR_Toggle 0 //For HDR10 10:10:10:2 use maybe 0.18/25.0 to start. Should only work with Thimothy
//Do not Use.

#if !defined(__RESHADE__) || __RESHADE__ < 40000
	#define Compatibility 1
#else
	#define Compatibility 0
#endif
/*
uniform float TEST <
	#if Compatibility
	ui_type = "drag";
	#else
	ui_type = "slider";
	#endif
    ui_min      = 0.0; ui_max      = 5.0;
> = 2.5;
*/
uniform int Auto_Bloom <
	ui_type = "combo";
	ui_label = "Auto Bloom";
	ui_items = "Off\0Auto Intensity\0Auto Desaturation\0Auto Saturation\0Auto Intensity & Desaturation\0Auto Intensity & Saturation\0";
	ui_tooltip = "Auto Intensity will enable the shader to adjust Bloom Intensity automaticly though Bloom Opacity above.\n"
				"Auto Saturation will enable the shader to adjust Bloom Saturation automaticly though Bloom Saturation above.\n"
				"Default is Off.";
	ui_category = "Bloom Adjustments";
> = 0;

uniform float CBT_Adjust <

	#if Compatibility
	ui_type = "drag";
	#else
	ui_type = "slider";
	#endif
	ui_min = 0.0; ui_max = 1.0;
	ui_label = "Extracting Bright Colors";
	ui_tooltip = "Use this to set the color based brightness threshold for what is and what isn't allowed.\n"
				"This is the most important setting, use Debug View to adjust this.\n"
				"Default Number is 0.5.";
	ui_category = "Bloom Adjustments";
> = 0.5;
/* This Has to be Culled because of Bloom
uniform float NFCD <
	ui_type = "slider";
	ui_min = 0.0; ui_max = 1.0;
	ui_label = "Sky Bloom Isolation";
	ui_tooltip = "Lets you Isolate Bloom Intensity far away areas like the sky.\n"
			     //"The 1nd Option is for Weapon Hands Boom Isolation.\n"
			     //"The 2nd Option is for Sky Bloom Isolation.\n"
			     //"The 3nd Option is for Intensity for both, Zero is off.\n"
			     "Defaults are [Near Isolation X 0.0] [Far Isolation Y 1.0], Off";
	ui_category = "Bloom Adjustments";
> = 1.0;
*/
uniform float2 Bloom_Intensity<
	#if Compatibility
	ui_type = "drag";
	#else
	ui_type = "slider";
	#endif
	ui_min = 0.0; ui_max = 1.0;
		ui_label = "Bloom Intensity & Bloom Opacity";
	ui_tooltip = "Use this to set Primary & Secondary Bloom Intensity and Overall Bloom Opacity for your content.\n"
				//"The 2nd Option only works when Bloom Intensity Isolation is set above.\n"
				"Number 0.1 & 0.5 is default.";
	ui_category = "Bloom Adjustments";
> = float2(0.1,0.5);

uniform float BloomSensitivity <
	#if Compatibility
	ui_type = "drag";
	#else
	ui_type = "slider";
	#endif
    ui_min      = 0.1; ui_max      = 5.0;
    ui_label    = "Bloom Sensitivity";
    ui_tooltip  = "A Curve that is applied to the bloom input.";
	ui_category = "Bloom Adjustments";
> = 1.0;

uniform float BloomCurve <
	#if Compatibility
	ui_type = "drag";
	#else
	ui_type = "slider";
	#endif
    ui_min      = 0.1; ui_max      = 5.0;
    ui_label    = "Bloom Curve";
    ui_tooltip  = "Defines the way the bloom spreads.";
 	ui_category = "Bloom Adjustments";
> = 2.0;

uniform float2 Saturation <
	#if Compatibility
	ui_type = "drag";
	#else
	ui_type = "slider";
	#endif
	ui_min = 0.0; ui_max = 1.0;
	ui_label = "Bloom Saturation & Auto Cutoff Point";
	ui_tooltip = "Adjustment The amount to adjust the saturation of the Bloom.\n"
				"Number 0.25 is default for both.";
	ui_category = "Bloom Adjustments";
> = float2(0.25,0.25);

uniform float Bloom_Spread <
	#if Compatibility
	ui_type = "drag";
	#else
	ui_type = "slider";
	#endif
	ui_min = 0.5; ui_max = 5.0;
	ui_label = "Bloom Spread";
	ui_tooltip = "Adjust to spread out the primary Bloom.\n"
				 "This is used for spreading Bloom.\n"
				 "Number 1.0 is default.";
	ui_category = "Bloom Adjustments";
> = 1.0;

uniform float Dither_Bloom <
 ui_type = "slider";
 ui_min = 0.0; ui_max = 1.0;
 ui_label = "Bloom Dither";
 ui_tooltip = "Adjustment The amount Dither on bloom to reduce banding.\n"
       "Number 0.25 is default.";
 ui_category = "Bloom Adjustments";
> = 0.125;

uniform int Tonemappers <
	ui_type = "combo";
	ui_label = "Tonemappers";
	ui_tooltip = "Changes how color get used for the other effects.\n";
	ui_items = "Timothy\0ACESFitted\0";
	ui_category = "Tonemapper Adjustments";
> = 0;

uniform float WP <
	ui_type = "drag";
	ui_min = 0.00; ui_max = 2.00;
	ui_label = "Linear White Point Value";
	ui_category = "Tonemapper Adjustments";
> = 1.0;

uniform float Exp <
	ui_type = "drag";
	ui_min = -4.0; ui_max = 4.00;
	ui_label = "Exposure";
	ui_category = "Tonemapper Adjustments";
> = 0.0;

uniform float GreyValue <
	ui_type = "drag";
    ui_min = 0.0; ui_max = 0.5;
    ui_label = "Exposure 50% Greyvalue";
    ui_tooltip = "Exposure 50% Greyvalue Set this Higher when using ACES Fitted and Lower when using Timoithy.";
    ui_category = "Tonemapper Adjustments";
> = 0.128;

uniform float Gamma <
	ui_type = "drag";
	ui_min = 1.0; ui_max = 4.0;
	ui_label = "Gamma value";
	ui_tooltip = "Most monitors/images use a value of 2.2. Setting this to 1 disables the inital color space conversion from gamma to linear.";
	ui_category = "Tonemapper Adjustments";
> = 2.2;

uniform float Contrast <
	ui_type = "drag";
	ui_min = 0.0; ui_max = 3.0;
	ui_tooltip = "Contrast Adjustment.\n"
				"Number 1.0 is default.";
	ui_category = "Tonemapper Adjustments";
> = 1.0;

uniform float Saturate <
	ui_type = "drag";
	ui_min = 0.0; ui_max = 2.0;
	ui_label = "Image Saturation";
	ui_tooltip = "Adjustment The amount to adjust the saturation of the color in the image.\n"
				"Number 1.0 is default.";
	ui_category = "Tonemapper Adjustments";
> = 1.0;

uniform int Inv_Tonemappers <
	ui_type = "combo";
	ui_label = "Extract HDR Information";
	ui_tooltip = "Changes how color get used for the other effects.\n"
				 "Turn this Off when the game has a good HDR implementation.";
	ui_items = "Off\0Luma\0Color\0Max Color Brightness\0";
	ui_category = "HDR";
> = 2;

uniform float HDR_BP <
	ui_type = "drag";
	ui_min = 0.0; ui_max = 1.0;
	ui_label = "HDR Power";
	ui_tooltip = "Use adjsut the HDR Power, You can override this value and set it to like 1.5 or something.\n"
				"Number 0.5 is default.";
	ui_category = "HDR";
> = 0.5;

uniform bool Bloom_BA_iToneMapper <
	ui_label = "HDR Bloom Application";
	ui_tooltip = "This will let you swap between Befor HDR ToneMapper and After.";
	ui_category = "HDR";
> = 0;

uniform int Auto_Exposure <
	ui_type = "combo";
	ui_label = "Auto Exposure Type";
	ui_items = "Off\0Auto Exposure & Eye Adaptation\0Auto Exposure & Eyelids Adaptation\0";
	ui_tooltip = "This will enable the shader to adjust Exposure automaticly.\n"
				 "This will also turn on Eye Adaptation for this shader.\n"
				 "This is based off Prod80's Port of an Auto-Expo Algo.\n"
				 "Padraic Hennessy, MJP and David Neubelt.";
	ui_category = "Adaptation";
> = 1;

uniform float Adapt_Adjust <
	ui_type = "drag";
	ui_min = 0.0; ui_max = 1.0;
	ui_label = "Eye Adapt Speed";
	ui_tooltip = "Use this to Adjust Eye Adaptation Speed.\n"
				 "Set from Zero to One, Zero is the slowest.\n"
				 "Number 0.5 is default.";
	ui_category = "Adaptation";
> = 0.5;

uniform float Adapt_Seek <
	ui_type = "drag";
	ui_min = 0.0; ui_max = 1.0;
	ui_label = "Eye Adapt Seeking";
			ui_tooltip = "Use this to Adjust Eye Seeking Radius for Average Brightness.\n"
				 "Set from 0 to 1, 1 is Full-Screen Average Brightness.\n"
				 "Number 0.5 is default.";
	ui_category = "Adaptation";
> = 0.5;

// Change this to set the partial resolution of the bloom buffer.
#define Set_Res 0.25 //0.25 should be the lowest you should Go and 1.0 is the Highest you can go.

//Depth Buffer Adjustments
#define DB_Size_Position 0     //[Off | On]         This is used to reposition and the size of the depth buffer.
#define BD_Correction 0        //[Off | On]         Barrel Distortion Correction for non conforming BackBuffer.


/*
uniform int Depth_Map <
	ui_type = "combo";
	ui_items = "DM0 Normal\0DM1 Reversed\0";
	ui_label = "Depth Map Selection";
	ui_tooltip = "Linearization for the zBuffer also known as Depth Map.\n"
			     "DM0 is Z-Normal and DM1 is Z-Reversed.\n";
	ui_category = "Depth Map";
> = DA_W;

uniform float Depth_Map_Adjust <
	ui_type = "drag";
	ui_min = 1.0; ui_max = 250.0;
	ui_label = "Depth Map Adjustment";
	ui_tooltip = "This allows for you to adjust the DM precision.\n"
				 "Adjust this to keep it as low as possible.\n"
				 "Default is 7.5";
	ui_category = "Depth Map";
> = DA_Y;
uniform float Offset <
	ui_type = "drag";
	ui_min = -1.0; ui_max = 1.0;
	ui_label = "Depth Map Offset";
	ui_tooltip = "Depth Map Offset is for non conforming ZBuffer.\n"
				 "It is rare that you would need to use this option.\n"
				 "Use this to make adjustments to DM 0 or DM 1.\n"
				 "Default and starts at Zero and it is Off.";
	ui_category = "Depth Map";
> = DA_Z;

uniform bool Depth_Map_Flip <
	ui_label = "Depth Map Flip";
	ui_tooltip = "Flip the depth map if it is upside down.";
	ui_category = "Depth Map";
> = DB_X;

#if DB_Size_Position || SP == 2
uniform float2 Horizontal_and_Vertical <
	ui_type = "drag";
	ui_min = 0.0; ui_max = 2;
	ui_label = "Horizontal & Vertical Size";
	ui_tooltip = "Adjust Horizontal and Vertical Resize. Default is 1.0.";
	ui_category = "Depth Corrections";
> = float2(DD_X,DD_Y);
uniform float2 Image_Position_Adjust<
	ui_type = "drag";
	ui_min = -1.0; ui_max = 1.0;
	ui_label = "Horizontal & Vertical Position";
	ui_tooltip = "Adjust the Image Position if it's off by a bit. Default is Zero.";
	ui_category = "Depth Corrections";
> = float2(DD_Z,DD_W);
#else
static const float2 Horizontal_and_Vertical = float2(DD_X,DD_Y);
static const float2 Image_Position_Adjust = float2(DD_Z,DD_W);
#endif
*/
uniform int Debug_View <
	ui_type = "combo";
	ui_label = "Debug View";
	ui_items = "Normal View\0Bloom View\0Heat Map\0";
	ui_tooltip = "To view Shade & Blur effect on the game, movie piture & ect.";
	ui_category = "Debugging";
> = 0;

/////////////////////////////////////////////////////D3D Starts Here/////////////////////////////////////////////////////////////////
#define pix float2(BUFFER_RCP_WIDTH, BUFFER_RCP_HEIGHT)
//#define BloomSpread Bloom_Spread * rcp(Bloom_Max)
#define DBP lerp(0.0,1.0,NFCD.z)
#define Sat lerp(0,10,Saturation.x)
#define PHI 1.61803398874989484820459 * 00000.1 // Golden Ratio
#define PI 3.14159265358979323846264 * 00000.1 // PI
#define SQ2 1.41421356237309504880169 * 10000.0 // Square Root of Two
#define BloomSamples 8

uniform float timer < source = "timer"; >;

texture DepthBufferTex : DEPTH;

sampler DepthBuffer
	{
		Texture = DepthBufferTex;
	};

texture BackBufferTex : COLOR;

sampler BackBuffer
	{
		Texture = BackBufferTex;
		#if SRGB
		SRGBTexture = true;
		#endif
	};

texture texCurrColor { Width = BUFFER_WIDTH * 0.5; Height = BUFFER_HEIGHT * 0.5; Format = RGBA8; MipLevels = 8;};

sampler SamplerCurrBB
	{
		Texture = texCurrColor;
	};

//Bloom Passes
texture2D BloomTexH_A	        	{ Width = BUFFER_WIDTH / 2;  	Height = BUFFER_HEIGHT / 2;    Format = RGBA16F;};
sampler2D TextureBloomH_A	        { Texture = BloomTexH_A;};

texture2D BloomTexV_A	        	{ Width = BUFFER_WIDTH / 2;  	Height = BUFFER_HEIGHT / 2;    Format = RGBA16F;};
sampler2D TextureBloomV_A	    	{ Texture = BloomTexV_A;};

texture2D BloomTexH_B	        	{ Width = BUFFER_WIDTH / 4;  	Height = BUFFER_HEIGHT / 4;    Format = RGBA16F;};
sampler2D TextureBloomH_B        	{ Texture = BloomTexH_B;};

texture2D BloomTexV_B	        	{ Width = BUFFER_WIDTH / 4;  	Height = BUFFER_HEIGHT / 4;    Format = RGBA16F;};
sampler2D TextureBloomV_B	        { Texture = BloomTexV_B;};

texture2D BloomTexH_C	        	{ Width = BUFFER_WIDTH / 8;  	Height = BUFFER_HEIGHT / 8;    Format = RGBA16F;};
sampler2D TextureBloomH_C        	{ Texture = BloomTexH_C;};

texture2D BloomTexV_C	        	{ Width = BUFFER_WIDTH / 8;  	Height = BUFFER_HEIGHT / 8;    Format = RGBA16F;};
sampler2D TextureBloomV_C	        { Texture = BloomTexV_C;};

texture2D BloomTex	        	{ Width = BUFFER_WIDTH / 2;  	Height = BUFFER_HEIGHT / 2;    Format = RGBA16F;};
sampler2D TextureBloom	        { Texture = BloomTex;};


#if Flare_A
texture TexFlareShared { Width = BUFFER_WIDTH ; Height = BUFFER_HEIGHT; Format = RGBA16F;};

sampler SamplerFlare
	{
		Texture = TexFlareShared;
	};
#endif
#if Flare_B
texture TexLensFlareShared { Width = BUFFER_WIDTH ; Height = BUFFER_HEIGHT; Format = RGBA16F;};

sampler SamplerLensFlare
	{
		Texture = TexLensFlareShared;
	};
#endif
//Total amount of frames since the game started.
uniform float frametime < source = "frametime";>;
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

float Luma(float3 C)
{
	float3 Luma;

	if (HDR_Toggle == 0)
	{
		Luma = float3(0.2126, 0.7152, 0.0722); // (HD video) https://en.wikipedia.org/wiki/Luma_(video)
	}
	else
	{
		Luma = float3(0.2627, 0.6780, 0.0593); // (HDR video) https://en.wikipedia.org/wiki/Rec._2100
	}

	return dot(C,Luma);
}

float3 ApplyPQ(float3 color)
{
    // Apply ST2084 curve
    float m1 = 2610.0 / 4096.0 / 4;
    float m2 = 2523.0 / 4096.0 * 128;
    float c1 = 3424.0 / 4096.0;
    float c2 = 2413.0 / 4096.0 * 32;
    float c3 = 2392.0 / 4096.0 * 32;
    float3 cp = pow(abs(color.xyz), m1);
    color.xyz = pow((c1 + c2 * cp) / (1 + c3 * cp), m2);
    return color;
}

//float3 ApplyscRGBScale(float3 color, float minLuminancePerNits, float maxLuminancePerNits)
//{
//    color.xyz = (color.xyz * (maxLuminancePerNits - minLuminancePerNits)) + float3(minLuminancePerNits, minLuminancePerNits, minLuminancePerNits);
//    return color;
//}

float Log2Exposure( in float avgLuminance, in float GreyValue )
    {
        float exposure   = 0.0f;
        avgLuminance     = max(avgLuminance, 0.000001f);
        // GreyValue should be 0.148 based on https://placeholderart.wordpress.com/2014/11/21/implementing-a-physically-based-camera-manual-exposure/
        float linExp     = GreyValue / avgLuminance;
        exposure         = log2( linExp );
        return exposure;
    }

float CalcExposedColor(in float avgLuminance, in float offset, in float GreyValue )
	{
	    float exposure   = Log2Exposure( avgLuminance, GreyValue  * 2.2 );
	    exposure         += offset; //offset = exposure
	    return exp2( exposure );
	}

/////////////////////////////////////////////////////////////////////////////////Adapted Luminance/////////////////////////////////////////////////////////////////////////////////
texture texDS {Width = 256; Height = 256; Format = RG16F; MipLevels = 9;}; //Sample at 256x256 map only has nine mip levels; 0-1-2-3-4-5-6-7-8 : 256,128,64,32,16,8,4,2, and 1 (1x1).

sampler SamplerDS
	{
		Texture = texDS;
	};

texture texTA {Width = 64; Height = 64; Format = R16F; };

sampler SamplerTA
	{
		Texture = texTA;
	};

texture texStored {Width = 64; Height = 64; Format = R16F; };

sampler SamplerStored
	{
		Texture = texStored;
	};

float3 Downsample(float4 pos : SV_Position, float2 texcoord : TEXCOORD) : SV_Target
{
	float2 texXY = texcoord;
	float2 midHV = (Adapt_Seek-1) * float2(BUFFER_WIDTH * 0.5,BUFFER_HEIGHT * 0.5) * pix;
	float2 TC = float2((texXY.x*Adapt_Seek)-midHV.x,(texXY.y*Adapt_Seek)-midHV.y);

	return float3(Luma(tex2D(BackBuffer,TC).rgb),0,0);
}

float PS_Temporal_Adaptation(float4 pos : SV_Position, float2 texcoord : TEXCOORD) : SV_Target
{   //Temporal adaptation https://knarkowicz.wordpress.com/2016/01/09/automatic-exposure/
	float TT = 32500,EA = (1-Adapt_Adjust)*1250, L =  tex2Dlod(SamplerDS,float4(texcoord,0,11)).x, PL_A = tex2D(SamplerStored, float2(0.25,0.25)).x, PL_B = tex2D(SamplerStored, float2(0.25,0.75)).x;
		  EA = Auto_Exposure >= 1 ? PL_A + (L - PL_A) * (1.0 - exp(-frametime/EA)) : L;
		  TT = Auto_Exposure >= 1 ? PL_B + (L - PL_B) * (1.0 - exp(-frametime/TT)) : L;
	float ITF= 1-(smoothstep(1,0,TT) < 0.85);// Is trigger Based on a Treshold... //smoothstep(1,0,tex2D(SamplerTA,0.0).y) < 0.85
	float FT = 2500, PStoredfade = tex2D(SamplerStored, float2(0.75,0.25)).x;//Top Right
	float TF = PStoredfade + (ITF - PStoredfade) * (1.0 - exp2(-frametime/FT)); //Fade Time
	//float3(EA,TT,TF)
	//EA = 0;//Top Left    float2(0.25,0.25).x
	//TT = 0;//Bottom Left float2(0.25,0.75).y
	//TF = 0;//Top Right   float2(0.75,0.25).z
	return texcoord.x > 0.5 ?  (texcoord.y > 0.5 ? 0 : TF) : (texcoord.y > 0.5 ? TT : EA);
}
//----------------------------------Inverse ToneMappers--------------------------------------------
float max3(float x, float y, float z)
{
    return max(x, max(y, z));
}

float3 inv_Tonemapper(float4 color, int TM)
{
	if(TM == 1)//Timothy Lottes fast_reversible
		return color.rgb * rcp((1.0 + max(color.w,0.001)) - max3(color.r, color.g, color.b));
	else if(TM == 2)//Reinhard
		return color.rgb * rcp(max((1.0 + color.w) - color.rgb,0.001));
	else//Luma Reinhard
		return color.rgb * rcp(max((1.0 + lerp(-0.5,0,color.w) ) - Luma(color.rgb),0.001));
}
//---------------------------------------ToneMappers-----------------------------------------------
// General tonemapping operator, build 'b' term.
float ColToneB(float hdrMax, float contrast, float shoulder, float midIn, float midOut)
{
    return
        -((-pow(midIn, contrast) + (midOut*(pow(hdrMax, contrast*shoulder)*pow(midIn, contrast) -
            pow(hdrMax, contrast)*pow(midIn, contrast*shoulder)*midOut)) /
            (pow(hdrMax, contrast*shoulder)*midOut - pow(midIn, contrast*shoulder)*midOut)) /
            (pow(midIn, contrast*shoulder)*midOut));
}

// General tonemapping operator, build 'c' term.
float ColToneC(float hdrMax, float contrast, float shoulder, float midIn, float midOut)
{
    return (pow(hdrMax, contrast*shoulder)*pow(midIn, contrast) - pow(hdrMax, contrast)*pow(midIn, contrast*shoulder)*midOut) /
           (pow(hdrMax, contrast*shoulder)*midOut - pow(midIn, contrast*shoulder)*midOut);
}

// General tonemapping operator, p := {contrast,shoulder,b,c}.
float ColTone(float x, float4 p)
{
    float z = pow(x, p.r);
    return z / (pow(z, p.g)*p.b + p.a);
}

float3 TimothyTonemapper(float3 color, float EX)
{
    float hdrMax =  HDR_Toggle ? 25.0 : 16.0; // How much HDR range before clipping. HDR modes likely need this pushed up to say 25.0.
    float contrast = Contrast + 0.250; // Use as a baseline to tune the amount of contrast the tonemapper has.
    static float shoulder = 1.0; // Likely don't need to mess with this factor, unless matching existing tonemapper is not working well..
    static float midIn = 0.11; // most games will have a {0.0 to 1.0} range for LDR so midIn should be 0.18.But,I settled around 0.11
    float midOut = HDR_Toggle ? 0.18/25.0 : 0.18; // Use for LDR. For HDR10 10:10:10:2 use maybe 0.18/25.0 to start. For scRGB, I forget what a good starting point is, need to re-calculate.

    float b = ColToneB(hdrMax, contrast, shoulder, midIn, midOut);
    float c = ColToneC(hdrMax, contrast, shoulder, midIn, midOut);
	//Tone map all the things. But, first start with exposure.
	color *= EX;

    #define EPS 1e-6f
    float peak = max(color.r, max(color.g, color.b));
    peak = max(EPS, peak);

    float3 ratio = color / peak;
    peak = ColTone(peak, float4(contrast, shoulder, b, c) );
    // then process ratio

    // probably want send these pre-computed (so send over saturation/crossSaturation as a constant)
    float crosstalk = 4.0; // controls amount of channel crosstalk
    float saturation = contrast * Saturate; // full tonal range saturation control
    float crossSaturation = contrast*16.0; // crosstalk saturation

    float white = WP;

    // wrap crosstalk in transform
    ratio = pow(abs(ratio), saturation / crossSaturation);
    ratio = lerp(ratio, white, pow(peak, crosstalk));
    ratio = pow(abs(ratio), crossSaturation);

    // then apply ratio to peak
    color = peak * ratio;
    return color;
}

// sRGB => XYZ => D65_2_D60 => AP1 => RRT_SAT
static const float3x3 ACESInputMat =
float3x3( float3(0.59719, 0.35458, 0.04823),
   	   float3(0.07600, 0.90834, 0.01566),
          float3(0.02840, 0.13383, 0.83777));

// ODT_SAT => XYZ => D60_2_D65 => sRGB
static const float3x3 ACESOutputMat =
float3x3( float3( 1.60475, -0.53108, -0.07367),
   	   float3(-0.10208,  1.10813, -0.00605),
   	   float3(-0.00327, -0.07276,  1.07602));

float3 RRTAndODTFit(float3 v)
{
    float3 a = v * (v + 0.0245786f) - 0.000090537f;
    float3 b = v * (0.983729f * v + 0.4329510f) + 0.238081f;
    return a / b;
}

float3 ACESFitted( float3 color, float EX)
{
	color *= EX + 0.5; //Was told this need to be pre adjusted exposure.
    color = mul(ACESInputMat,color);
    color = RRTAndODTFit(color);
    color = mul(ACESOutputMat,color);

    return  color/WP;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
float4 SimpleBlur(sampler2D InputTex, float2 Coord, float2 pixelsize)
{
    float4 Blur = 0.0;

    static const float2 Offsets[4]=
    {
        float2(1.0, 1.0),
        float2(1.0, -1.0),
        float2(-1.0, 1.0),
        float2(-1.0, -1.0)
    };

    for (int i = 0; i < 4; i++)
    {
        Blur += tex2D(InputTex, Coord + Offsets[i] * pixelsize);
    }

    return Blur * 0.25;
}

struct BloomData
{
    float4 Blur;
    float2 Pixelsize;
    float2 Coord;
    float  Offset;
    float  Weight;
    float  WeightSum;
};

float2 GetPixelSize(float texsize)
{
    return pix * texsize;//(1 / texsize) * float2(1, BUFFER_WIDTH / BUFFER_HEIGHT);
}

float3 Auto_Luma()
{
	return float3(saturate(smoothstep(0,1,1-tex2D(SamplerTA,float2(0.25,0.25)).x)), //Top Left    float2(0.25,0.25).x
				  tex2D(SamplerTA,float2(0.25,0.25)).x,                             //Top Left    float2(0.25,0.25).x
				  saturate(smoothstep(0,1,tex2D(SamplerTA,float2(0.75,0.25)).x)));  //Top Right   float2(0.75,0.25).z
}

float3 Color_GS(float4 BC)
{   // Luma Threshold Thank you Adyss
	float GS = Luma(BC.rgb), Saturate_A = Sat, Saturate_B = saturate(Saturation.y), AL = Auto_Luma().x;//Luma and more
          // Gamma Curve
          BC.rgb    = pow(abs(BC.rgb), BloomSensitivity);
	      BC.rgb /= max(GS, 0.001);
          BC.a    = max(0.0, GS - CBT_Adjust);
          BC.rgb *= BC.a;
		  //Bloom Saturation
		  if(Auto_Bloom == 2 || Auto_Bloom == 4) //Desaturate
		  	Saturate_A *= lerp(0.0 + Saturate_B,1,AL);

		  if(Auto_Bloom == 3 || Auto_Bloom == 5) //Saturate
		  	Saturate_A *= lerp(2.0 - Saturate_B,1,AL);

		  BC.rgb  = lerp(BC.a, BC.rgb, min(10,Saturate_A));

    return saturate(BC.rgb);
}

void PS_CurrentInfo(float4 pos : SV_Position, float2 texcoords : TEXCOORD, out float4 Color : SV_Target0)
{   //float2 D = Depth_Info(texcoords);
	Color = float4(Color_GS(tex2D(BackBuffer, texcoords)), 0);
}
//Bloom Fuction from Adyss Bloom Wanted a version of your code to live on Miss you man.
float4  Bloom(float2 Coord, float texsize, sampler2D InputTex, int Dir)
{
    BloomData Bloom;
    Bloom.Pixelsize      = GetPixelSize(texsize);
    Bloom.Offset         = Bloom_Spread * 0.5 ;

    for (int i = 1; i < BloomSamples; i++)
    {
		float2 D = Dir ? float2(Bloom.Offset * Bloom.Pixelsize.x, 0) : float2( 0, Bloom.Offset * Bloom.Pixelsize.y);
        Bloom.Weight     = pow(abs(BloomSamples - i), BloomCurve);
        Bloom.Blur      += tex2Dlod(InputTex, float4(Coord + D ,0,0)) * Bloom.Weight;
        Bloom.Blur      += tex2Dlod(InputTex, float4(Coord - D ,0,0)) * Bloom.Weight;
        Bloom.Offset    += Bloom_Spread ;
        Bloom.WeightSum += Bloom.Weight;
    }

    return Bloom.Blur /= Bloom.WeightSum * 2;
}

float4 PS_BloomH_A(float4 pos : SV_Position, float2 Coord : TEXCOORD) : SV_Target
{   //Prepass
    return Bloom(Coord, 2, SamplerCurrBB, 1);
}

float4 PS_BloomV_A(float4 pos : SV_Position, float2 Coord : TEXCOORD) : SV_Target
{
    return Bloom(Coord, 2, TextureBloomH_A, 0);
}

float4 PS_BloomH_B(float4 pos : SV_Position, float2 Coord : TEXCOORD) : SV_Target
{
    return Bloom(Coord, 8, TextureBloomV_A, 1);
}

float4 PS_BloomV_B(float4 pos : SV_Position, float2 Coord : TEXCOORD) : SV_Target
{
    return Bloom(Coord, 8, TextureBloomH_B, 0);
}

float4 PS_BloomH_C(float4 pos : SV_Position, float2 Coord : TEXCOORD) : SV_Target
{
    return Bloom(Coord, 16, TextureBloomV_B, 1);
}

float4 PS_BloomV_C(float4 pos : SV_Position, float2 Coord : TEXCOORD) : SV_Target
{
    return Bloom(Coord, 16, TextureBloomH_C, 0);
}

float4 Final_Bloom(float4 pos : SV_Position, float2 texcoords : TEXCOORD) : SV_Target
{
	float3 LP = tex2D(BackBuffer, texcoords).rgb;
    float4 Bloom  = SimpleBlur(TextureBloomH_A, texcoords, GetPixelSize(2)).rgba;
           Bloom += SimpleBlur(TextureBloomV_A, texcoords, GetPixelSize(2)).rgba;
           Bloom += SimpleBlur(TextureBloomH_B, texcoords, GetPixelSize(4)).rgba;
           Bloom += SimpleBlur(TextureBloomV_B, texcoords, GetPixelSize(4)).rgba;
           Bloom += SimpleBlur(TextureBloomH_C, texcoords, GetPixelSize(8)).rgba;
           Bloom += SimpleBlur(TextureBloomV_C, texcoords, GetPixelSize(8)).rgba;
           Bloom *= lerp(0,10,Bloom_Intensity.x) / 6; // Luma Preserving Blending Not used........
    return float4(lerp(Bloom.rgb, max(Bloom.rgb - LP, 0.0), 0),0);
}

float3 Green_Blue( float interpolant )
{
	if( interpolant < 0.5 )
		return float3(0.0, 1.0, 2.0 * interpolant);
	else
		return float3(0.0, 2.0 - 2.0 * interpolant, 1.0 );
}

float3 Red_Green( float interpolant )
{
	if( interpolant < 0.5 )
		return float3(1.0, 2.0 * interpolant, 0.0);
	else
		return float3(2.0 - 2.0 * interpolant, 1.0, 0.0 );
}

float3 FHeat( float interpolant )
{
    float invertedInterpolant = interpolant;
 	if( invertedInterpolant < 0.5 )
    {
        float remappedFirstHalf = 1.0 - 2.0 * invertedInterpolant;
        return Green_Blue( remappedFirstHalf );
    }
    else
    {
     	float remappedSecondHalf = 2.0 - 2.0 * invertedInterpolant;
        return Red_Green( remappedSecondHalf );
    }
}

float3 HeatMap( float interpolant )
{
	if( interpolant < 1.0 / 6.0 )
	{
		float firstSegmentInterpolant = 6.0 * interpolant;
		return ( 1.0 - firstSegmentInterpolant ) * float3(0.0, 0.0, 0.0) + firstSegmentInterpolant * float3(0.0, 0.0, 1.0);
	}
	else if( interpolant < 5.0 / 6.0 )
	{
		float midInterpolant = 0.25 * ( 6.0 * interpolant - 1.0 );
		return FHeat( midInterpolant );
	}
	else
	{
		float lastSegmentInterpolant = 6.0 * interpolant - 5.0;
		return ( 1.0 - lastSegmentInterpolant ) * float3(1.0, 0.0, 0.0) + lastSegmentInterpolant * float3(1.0, 1.0, 1.0);
	}
}

float Scale(float val,float max,float min) //Scale to 0 - 1
{
	return (val - min) / (max - min);
}

void GN(inout float Noise,float2 TC,float seed)
{
    Noise = frac(tan(distance(TC*((seed+10)+PHI), float2(PHI, PI)))*SQ2);
}

float4 HDROut(float2 texcoords)
{   float4 Out, Bloom = SimpleBlur(TextureBloom, texcoords, GetPixelSize(1)).rgba;
	float2 TC = 10 * texcoords.xy - 5;
	float AL = Auto_Luma().y, Ex;

	if(Auto_Exposure >= 1)
		Ex = Exp;
	else
		Ex = lerp(0,2.5,Scale(Exp, 4, -4));

	float NC = Bloom_Intensity.y;//BI_Brightness = lerp(1,0.01,Bloom_Intensity.y)

	if(Auto_Bloom == 1 || Auto_Bloom == 4 || Auto_Bloom == 5)
		NC *= max(0.25,Auto_Luma().x);

	float3 Noise, iFast, iReinhard, iReinhardLuma, Color = tex2D(BackBuffer, texcoords).rgb;
	// Goldern Noise RGB Dither
	GN( Noise.r, TC, 1 );
	GN( Noise.g, TC, 2 );
	GN( Noise.b, TC, 3 );
	float3 SS  = smoothstep( 0.0, 0.1, Bloom.rgb );
		   SS *= lerp(0.0,0.1,saturate(Dither_Bloom));
		Bloom.rgb = saturate( Bloom.rgb + Noise * SS );

	//Bloom should be applied before Tonemapping as otherwise all high ranges will be lost. Also can used as "resolve." But, I allow for both.
	Bloom.rgb = lerp( 0.0, Bloom.rgb, saturate(NC));
	//Bloom_B = lerp( 0.0, (( Bloom.rgb - 0 ) / ( BI_Brightness - 0)), saturate(NC));
	//Bloom.rgb = lerp(Bloom_A,Bloom_B,Bloom.w);

	if(Tonemappers >= 1)
   	Color = lerp(Luma(Color),Color,Saturate);
	// Do inital de-gamma of the game image to ensure we're operating in the correct colour range.
	if( Gamma > 1. )
		Color = pow(abs(Color),Gamma);
	//Bloom Befor Inverse ToneMapper
	if(!Bloom_BA_iToneMapper)
		Color += Bloom.rgb;
	//Evil bad Negitive colors be gone by the wind of dev....
	Color = max(0,Color);
	//HDR Map
	if(Inv_Tonemappers == 1)
		Color = inv_Tonemapper(float4(Color,1-HDR_BP), 0);
	else if(Inv_Tonemappers == 2)
		Color = inv_Tonemapper(float4(Color,1-HDR_BP), 2);
	else if(Inv_Tonemappers == 3)
		Color = inv_Tonemapper(float4(Color,1-HDR_BP), 1);
	//Bloom After Inverse ToneMapper
	if (Bloom_BA_iToneMapper)
		Color += Bloom.rgb;
	//Tone map all the things
	if(Auto_Exposure >= 1)
		Ex = CalcExposedColor(AL,Ex,GreyValue);
	else //Using the optimized tonemapper by Jim Hejl and Richard Burgess-Dawson 0.148 for the GreyValue works for me. https://imgflip.com/i/oos2z But, I will alow for this to be adjusted.
		Ex = Ex;

	if(Tonemappers == 0)
		Color = TimothyTonemapper(Color,Ex);
	else if (Tonemappers == 1)
		Color = ACESFitted(Color,Ex);
  //removed for now because of bug.
	#if Flare_A
		//Color += tex2D(SamplerFlare, texcoords).rgb;
	#endif

	#if Flare_B
		//Color += tex2D(SamplerLensFlare, texcoords).rgb;
	#endif

	#if HDR_Toggle
	// Do ST2084 curve
	Color = ApplyPQ(Color);
	#else
	// Do the post-tonemapping gamma correction
	if( Gamma > 1. )
		Color = pow(abs(Color),rcp(2.2));
	#endif

	//float2 D = Depth_Info(texcoords);
	float MD = 0;
	if(Tonemappers >= 1)
		Color = (Color - 0.5) * (Contrast) + 0.5;

	if (Debug_View == 0)
		Out = float4(Color, 1.);
	else if(Debug_View == 1)
		Out = Bloom;
	else if(Debug_View == 2)
		Out = texcoords.y < 0.975 ? HeatMap(Luma( Color )): HeatMap(texcoords.x);
	//else
		//Out = lerp(D.y,float3(D.y,0.5,D.y),MD) ;
    //May need to dither.......
    texcoords.x = texcoords.x * 0.75 + 0.125;//* 0.5 + 0.25;
    texcoords *=  1.0 - float2(texcoords.x,texcoords.y);
    float vignette = texcoords.x * texcoords.y  * 15.0, Mfactor = pow(smoothstep(0,1,vignette), Auto_Luma().y * 2.0);
	if(Auto_Exposure > 1)
		vignette = lerp(1,lerp(0,1,saturate(Mfactor)),Auto_Luma().z);
	else
    	vignette = 1;

	return float4(Out.rgb * vignette,1.0);
}

float PS_StoreInfo(float4 pos : SV_Position, float2 texcoord : TEXCOORD) : SV_Target
{
    return tex2D(SamplerTA,texcoord).x;
}

////////////////////////////////////////////////////////Logo/////////////////////////////////////////////////////////////////////////
float4 Out(float4 position : SV_Position, float2 texcoord : TEXCOORD) : SV_Target
{
	float PosX = 0.9525f*BUFFER_WIDTH*pix.x,PosY = 0.975f*BUFFER_HEIGHT*pix.y;
	float3 Color = HDROut(texcoord).rgb,D,E,P,T,H,Three,DD,Dot,I,N,F,O;
	//Color = tex2D(SamplerTA,texcoord).x;
	[branch] if(timer <= 12500)
	{
		//DEPTH
		//D
		float PosXD = -0.035+PosX, offsetD = 0.001;
		float3 OneD = all( abs(float2( texcoord.x -PosXD, texcoord.y-PosY)) < float2(0.0025,0.009));
		float3 TwoD = all( abs(float2( texcoord.x -PosXD-offsetD, texcoord.y-PosY)) < float2(0.0025,0.007));
		D = OneD-TwoD;

		//E
		float PosXE = -0.028+PosX, offsetE = 0.0005;
		float3 OneE = all( abs(float2( texcoord.x -PosXE, texcoord.y-PosY)) < float2(0.003,0.009));
		float3 TwoE = all( abs(float2( texcoord.x -PosXE-offsetE, texcoord.y-PosY)) < float2(0.0025,0.007));
		float3 ThreeE = all( abs(float2( texcoord.x -PosXE, texcoord.y-PosY)) < float2(0.003,0.001));
		E = (OneE-TwoE)+ThreeE;

		//P
		float PosXP = -0.0215+PosX, PosYP = -0.0025+PosY, offsetP = 0.001, offsetP1 = 0.002;
		float3 OneP = all( abs(float2( texcoord.x -PosXP, texcoord.y-PosYP)) < float2(0.0025,0.009*0.775));
		float3 TwoP = all( abs(float2( texcoord.x -PosXP-offsetP, texcoord.y-PosYP)) < float2(0.0025,0.007*0.680));
		float3 ThreeP = all( abs(float2( texcoord.x -PosXP+offsetP1, texcoord.y-PosY)) < float2(0.0005,0.009));
		P = (OneP-TwoP) + ThreeP;

		//T
		float PosXT = -0.014+PosX, PosYT = -0.008+PosY;
		float3 OneT = all( abs(float2( texcoord.x -PosXT, texcoord.y-PosYT)) < float2(0.003,0.001));
		float3 TwoT = all( abs(float2( texcoord.x -PosXT, texcoord.y-PosY)) < float2(0.000625,0.009));
		T = OneT+TwoT;

		//H
		float PosXH = -0.0072+PosX;
		float3 OneH = all( abs(float2( texcoord.x -PosXH, texcoord.y-PosY)) < float2(0.002,0.001));
		float3 TwoH = all( abs(float2( texcoord.x -PosXH, texcoord.y-PosY)) < float2(0.002,0.009));
		float3 ThreeH = all( abs(float2( texcoord.x -PosXH, texcoord.y-PosY)) < float2(0.00325,0.009));
		H = (OneH-TwoH)+ThreeH;

		//Three
		float offsetFive = 0.001, PosX3 = -0.001+PosX;
		float3 OneThree = all( abs(float2( texcoord.x -PosX3, texcoord.y-PosY)) < float2(0.002,0.009));
		float3 TwoThree = all( abs(float2( texcoord.x -PosX3 - offsetFive, texcoord.y-PosY)) < float2(0.003,0.007));
		float3 ThreeThree = all( abs(float2( texcoord.x -PosX3, texcoord.y-PosY)) < float2(0.002,0.001));
		Three = (OneThree-TwoThree)+ThreeThree;

		//DD
		float PosXDD = 0.006+PosX, offsetDD = 0.001;
		float3 OneDD = all( abs(float2( texcoord.x -PosXDD, texcoord.y-PosY)) < float2(0.0025,0.009));
		float3 TwoDD = all( abs(float2( texcoord.x -PosXDD-offsetDD, texcoord.y-PosY)) < float2(0.0025,0.007));
		DD = OneDD-TwoDD;

		//Dot
		float PosXDot = 0.011+PosX, PosYDot = 0.008+PosY;
		float3 OneDot = all( abs(float2( texcoord.x -PosXDot, texcoord.y-PosYDot)) < float2(0.00075,0.0015));
		Dot = OneDot;

		//INFO
		//I
		float PosXI = 0.0155+PosX, PosYI = 0.004+PosY, PosYII = 0.008+PosY;
		float3 OneI = all( abs(float2( texcoord.x - PosXI, texcoord.y - PosY)) < float2(0.003,0.001));
		float3 TwoI = all( abs(float2( texcoord.x - PosXI, texcoord.y - PosYI)) < float2(0.000625,0.005));
		float3 ThreeI = all( abs(float2( texcoord.x - PosXI, texcoord.y - PosYII)) < float2(0.003,0.001));
		I = OneI+TwoI+ThreeI;

		//N
		float PosXN = 0.0225+PosX, PosYN = 0.005+PosY,offsetN = -0.001;
		float3 OneN = all( abs(float2( texcoord.x - PosXN, texcoord.y - PosYN)) < float2(0.002,0.004));
		float3 TwoN = all( abs(float2( texcoord.x - PosXN, texcoord.y - PosYN - offsetN)) < float2(0.003,0.005));
		N = OneN-TwoN;

		//F
		float PosXF = 0.029+PosX, PosYF = 0.004+PosY, offsetF = 0.0005, offsetF1 = 0.001;
		float3 OneF = all( abs(float2( texcoord.x -PosXF-offsetF, texcoord.y-PosYF-offsetF1)) < float2(0.002,0.004));
		float3 TwoF = all( abs(float2( texcoord.x -PosXF, texcoord.y-PosYF)) < float2(0.0025,0.005));
		float3 ThreeF = all( abs(float2( texcoord.x -PosXF, texcoord.y-PosYF)) < float2(0.0015,0.00075));
		F = (OneF-TwoF)+ThreeF;

		//O
		float PosXO = 0.035+PosX, PosYO = 0.004+PosY;
		float3 OneO = all( abs(float2( texcoord.x -PosXO, texcoord.y-PosYO)) < float2(0.003,0.005));
		float3 TwoO = all( abs(float2( texcoord.x -PosXO, texcoord.y-PosYO)) < float2(0.002,0.003));
		O = OneO-TwoO;
		//Website
		return float4(D+E+P+T+H+Three+DD+Dot+I+N+F+O,1.) ? 1-texcoord.y*50.0+48.35f : float4(Color,1.);
	}
	else
		return float4(Color,1.);
}

///////////////////////////////////////////////////////////ReShade.fxh/////////////////////////////////////////////////////////////

// Vertex shader generating a triangle covering the entire screen
void PostProcessVS(in uint id : SV_VertexID, out float4 position : SV_Position, out float2 texcoord : TEXCOORD)
{
	texcoord.x = (id == 2) ? 2.0 : 0.0;
	texcoord.y = (id == 1) ? 2.0 : 0.0;
	position = float4(texcoord * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
}

//*Rendering passes*//
technique Blooming_HDR
{
	    pass Store_Info
    {
        VertexShader = PostProcessVS;
        PixelShader = PS_StoreInfo;
        RenderTarget = texStored;
    }
    	pass Current_Info
    {
        VertexShader = PostProcessVS;
        PixelShader = PS_CurrentInfo;
        RenderTarget0 = texCurrColor;
    }
	pass BloomH_A
	{
		VertexShader  = PostProcessVS;
		PixelShader   = PS_BloomH_A;
        RenderTarget = BloomTexH_A;
	}
    pass BloomV_A
	{
		VertexShader  = PostProcessVS;
		PixelShader   = PS_BloomV_A;
        RenderTarget = BloomTexV_A;
	}
	pass BloomV_B
	{
		VertexShader  = PostProcessVS;
		PixelShader   = PS_BloomH_B;
        RenderTarget = BloomTexH_B;
	}
    pass BloomV_B
	{
		VertexShader  = PostProcessVS;
		PixelShader   = PS_BloomV_B;
        RenderTarget = BloomTexV_B;
	}
	pass BloomV_C
	{
		VertexShader  = PostProcessVS;
		PixelShader   = PS_BloomH_C;
        RenderTarget = BloomTexH_C;
	}
    pass BloomV_C
	{
		VertexShader  = PostProcessVS;
		PixelShader   = PS_BloomV_C;
        RenderTarget = BloomTexV_C;
	}
	    pass Bloom
	{
		VertexShader  = PostProcessVS;
		PixelShader   = Final_Bloom;
        RenderTarget = BloomTex;
	}
		pass Downsampler
    {
        VertexShader = PostProcessVS;
        PixelShader = Downsample;
        RenderTarget = texDS;
    }
    	pass Temporal_Adaptation
    {
        VertexShader = PostProcessVS;
        PixelShader = PS_Temporal_Adaptation;
        RenderTarget = texTA;
    }
		pass HDROut
	{
		VertexShader = PostProcessVS;
		PixelShader = Out;
		#if SRGB
		SRGBWriteEnable = true;
		#endif
	}

}
