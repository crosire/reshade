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
//* http://www.oscars.org/science-technology/sci-tech-projects/aces
//* https://knarkowicz.wordpress.com/2016/01/06/aces-filmic-tone-mapping-curve/
//* Timothy Lottes Tone mapper and iformation from this site.
//* https://bartwronski.com/2016/09/01/dynamic-range-and-evs/
//* https://www.shadertoy.com/view/XljBRK
//*
//* Since it was already witten out nicely was ported from. FidelityFX LPM was it self was not ported.
//* https://github.com/GPUOpen-Effects/FidelityFX-LPM
//* Most of the code is LICENSED this way.
//* https://github.com/GPUOpen-Effects/FidelityFX-LPM/blob/4a1442bf7405d0f703f7cf9c0bfe47a7559cc69b/sample/src/DX12/LPMSample.h#L3-L18
//*
//* The rest of the code bellow is under it's own license.  
//* If I missed any please tell me.
//*
//* LICENSE
//* ============
//* Blooming HDR is licenses under: Attribution-NoDerivatives 4.0 International
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
#if exists "Flair.fx"                                           //Flair Intercepter//
	#define Flare_A 1
#else
	#define Flare_A 0
#endif

#if exists "Flare.fx"                                           //Flare Intercepter//
	#define Flare_B 1
#else
	#define Flare_B 0
#endif

// Max Bloom ammount.
#define Bloom_Max 250

//Do not Use. 
//Use for real HDR. 
#define HDR_Toggle 0 //For HDR10 10:10:10:2 use maybe 0.18/25.0 to start. Should only work with Thimothy 
//Do not Use. 

#if !defined(__RESHADE__) || __RESHADE__ < 40000
	#define Compatibility 1
#else
	#define Compatibility 0
#endif

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

uniform float2 Bloom_Intensity<
	#if Compatibility
	ui_type = "drag";
	#else
	ui_type = "slider";
	#endif
	ui_min = 0.0; ui_max = 1.0;
		ui_label = "Bloom Intensity & Bloom Opacity";
	ui_tooltip = "Use this to set Bloom Intensity and Overall Bloom Opacity for your content.\n"
				"Number 0.5 is default.";
	ui_category = "Bloom Adjustments";
> = float2(0.5,0.5);

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
	ui_min = 10.0; ui_max = Bloom_Max; ui_step = 0.25;
	ui_label = "Primary Bloom Spread";
	ui_tooltip = "Adjust to spread out the primary Bloom.\n"
				 "This is used for spreading Bloom.\n"
				 "Number 50.0 is default.";
	ui_category = "Bloom Adjustments";
> = 125.0;

uniform int Petal_Ammount <
	#if Compatibility
	ui_type = "drag";
	#else
	ui_type = "slider";
	#endif
	ui_min = 3; ui_max = 9;
	ui_label = "Bloom Petals";
	ui_tooltip = "Used to adjust the ammount of petals used in this bloom.\n"
				 "Number 5 is default.";
	ui_category = "Bloom Adjustments";
> = 5;

uniform float Petal_Power <
	#if Compatibility
	ui_type = "drag";
	#else
	ui_type = "slider";
	#endif
	ui_min = 0.0; ui_max = 1.0;
	ui_label = "Petal Definition & Bloom Boost";
	ui_tooltip = "This is used for adjusting Petal Definiton.\n"
				 "If you want Yuge Bloom Sizes override this.\n"
				 "Number 0.5 is default.";
	ui_category = "Bloom Adjustments";
> = 0.5;

uniform int Tonemappers <
	ui_type = "combo";
	ui_label = "Tonemappers";
	ui_tooltip = "Changes how color get used for the other effects.\n";
	ui_items = "Timothy\0Hable's filmic\0DX11DSK\0ACESFilm\0White Luma Reinhard\0None\0";
	ui_category = "Tonemapper Adjustments";
> = 1;

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
    ui_tooltip = "Bloom Exposure 50% Greyvalue";
    ui_category = "Tonemapper Adjustments";
> = 0.18;

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

uniform bool Auto_Exposure <
	ui_label = "Auto Exposure";
	ui_tooltip = "This will enable the shader to adjust Exposure automaticly.\n"
				 "This is based off Prod80's Port of a Exposure algorithm by.\n"
				 "Padraic Hennessy, MJP and David Neubelt."; 
	ui_category = "Tonemapper Adjustments";
> = 0;


uniform float Adapt_Adjust <
	ui_type = "drag";
	ui_min = 0.0; ui_max = 1.0;
	ui_label = "Eye Adapt Speed";
	ui_tooltip = "Use this to Adjust Eye Adaptation Speed.\n"
				 "Set from Zero to One, Zero is the slowest.\n"
				 "Number 0.5 is default.";
	ui_category = "Eye Adaptation";
> = 0.5;

uniform float Adapt_Seek <
	ui_type = "drag";
	ui_min = 0.0; ui_max = 1.0;
	ui_label = "Eye Adapt Seeking";
			ui_tooltip = "Use this to Adjust Eye Seeking Radius for Average Brightness.\n"
				 "Set from 0 to 1, 1 is Full-Screen Average Brightness.\n"
				 "Number 0.5 is default.";
	ui_category = "Eye Adaptation";
> = 0.5;

uniform bool Eye_Adaptation <
	ui_label = "Eye Adaptation";
	ui_tooltip = "Use this to turn on Eye Adaptation for this shader.\n"
				 "You need Auto Exposure to be on for this to work.";
	ui_category = "Eye Adaptation";
> = true;

uniform int Debug_View <
	ui_type = "combo";
	ui_label = "Debug View";
	ui_items = "Normal View\0Bloom View\0Heat Map\0";
	ui_tooltip = "To view Shade & Blur effect on the game, movie piture & ect.";
	ui_category = "Debugging";
> = 0;

/////////////////////////////////////////////////////D3D Starts Here/////////////////////////////////////////////////////////////////
#define pix float2(BUFFER_RCP_WIDTH, BUFFER_RCP_HEIGHT)
#define BloomSpread Bloom_Spread * rcp(Bloom_Max)
#define PP BloomSpread * lerp(5.0,20.0, Petal_Power)
#define Sat lerp(0,10,Saturation.x)

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
	};

texture texCurrent { Width = BUFFER_WIDTH * 0.5; Height = BUFFER_HEIGHT * 0.5; Format = RGBA8; MipLevels = 8;};

sampler SamplerCBB
	{
		Texture = texCurrent;
	};

texture texMBlur_HV0 { Width = BUFFER_WIDTH * 0.25; Height = BUFFER_HEIGHT * 0.25; Format = RGBA16F; MipLevels = 8;};

sampler SamplerBlur_HV0
	{
		Texture = texMBlur_HV0;
	};

texture texMBlur_HV1 { Width = BUFFER_WIDTH * 0.25; Height = BUFFER_HEIGHT * 0.25; Format = RGBA16F; MipLevels = 8;};

sampler SamplerBlur_HV1
	{
		Texture = texMBlur_HV1;
	};

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

texture texTA { Format = R16F; };

sampler SamplerTA
	{
		Texture = texTA;
	};

texture texStored { Format = R16F; };

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
{
	float AA = (1-Adapt_Adjust)*1000, L =  tex2Dlod(SamplerDS,float4(texcoord,0,11)).x, PL = tex2D(SamplerStored, texcoord).x;
	//Temporal adaptation https://knarkowicz.wordpress.com/2016/01/09/automatic-exposure/
	float EA = Eye_Adaptation ? PL + (L - PL) * (1.0 - exp(-frametime/AA)) : L;
	return EA;
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
    static float hdrMax =  HDR_Toggle ? 25.0 : 16.0; // How much HDR range before clipping. HDR modes likely need this pushed up to say 25.0.
    static float contrast = Contrast + 0.250; // Use as a baseline to tune the amount of contrast the tonemapper has.
    static float shoulder = 1.0; // Likely don't need to mess with this factor, unless matching existing tonemapper is not working well..
    static float midIn = 0.11; // most games will have a {0.0 to 1.0} range for LDR so midIn should be 0.18.But,I settled around 0.11
    static float midOut = HDR_Toggle ? 0.18/25.0 : 0.18; // Use for LDR. For HDR10 10:10:10:2 use maybe 0.18/25.0 to start. For scRGB, I forget what a good starting point is, need to re-calculate.

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

float3 HableTonemap(float3 color, float EX) // Habble
{   float A = 0.15;
	float B = 0.50;
	float C = 0.10;
	float D = 0.20;
	float E = 0.02;
	float F = 0.30;
	float W = WP;
	//Tone map all the things. But, first start with exposure.
	color *= EX;
	color = ((color * (A * color + C * B) + D * E) / (color * (A * color + B) + D * F)) - E / F;
	float white = ((W * (A * W + C * B) + D * E) / (W * (A * W + B) + D * F)) - E / F;
	color /= white;
	return color;
}

float3 DX11DSK(float3 color, float EX)
{
    float  MIDDLE_GRAY = 0.72;
    float  LUM_WHITE = WP;

	//Tone map all the things. But, first start with exposure.
	color *= EX;

    color.rgb *= MIDDLE_GRAY;
    color.rgb *= (1.0f + color/LUM_WHITE);
    color.rgb /= (1.0f + color);

    return color;
}

// https://knarkowicz.wordpress.com/2016/01/06/aces-filmic-tone-mapping-curve/
/* Not Used
float3 ACESFilmRec2020( float3 color, float EX)
{
    float a = 15.8f;
    float b = 2.12f;
    float c = 1.2f;
    float d = 5.92f;
    float e = 1.9f;
    float W = lerp(0,2,WP);//Poor Hack for adjusting ACES White point.
    //Tone map all the things. But, first start with exposure.
	color *= EX;
    color = (color*(a*color + b)) / (color*(c*color + d) + e);
    float white = (W*(a*W + b)) / (W*(c*W + d) + e);
	color /= white;    
    return color;
}
*/
float3 ACESFilm(float3 color, float EX)
{
    float a = 2.51f;
    float b = 0.03f;
    float c = 2.43f;
    float d = 0.59f;
    float e = 0.14f;
    float W = lerp(0,2,WP);//Poor Hack for adjusting ACES White point.
    //Tone map all the things. But, first start with exposure.
	color *= EX;
    color = saturate((color*(a*color + b)) / (color*(c*color + d) + e));
    float white = (W*(a*W + b)) / (W*(c*W + d) + e);
	color /= white;    
    return color;
}
// whitePreservingLumaBasedReinhardToneMapping
float3 WPLBR(float3 color, float EX)
{
    float white = WP;
    
    //Tone map all the things. But, first start with exposure.
	color *= EX;
    
    float luma = Luma(color);
    float toneMappedLuma = luma * (1. + luma / (white*white)) / (1. + luma);
    color *= toneMappedLuma / luma;
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
/*
//Luminance only fit
float3 ACESFitted(float3 color, float EX)
{    float W = WP;//Poor Hack for adjusting ACES White point.
    //Tone map all the things. But, first start with exposure.
	color *= EX;
	
    color = mul(ACESInputMat, color);

    // Apply RRT and ODT
    color = RRTAndODTFit(color);

    color = mul(ACESOutputMat, color);
    
	color /= RRTAndODTFit(W);  
	
    // Clamp to [0, 1]
    color = saturate(color);

    return color;
}
*/
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

float2 Auto_Luma()
{
	return float2(saturate(smoothstep(0,1,1-tex2D(SamplerTA,0.0).x)),tex2D(SamplerTA,0.0).x);
}

float4 Color_GS(float4 BC)
{
           // Luma Threshold Thank you Adyss
	float GS = Luma(BC.rgb), Saturate_A = Sat, Saturate_B = saturate(Saturation.y), AL = Auto_Luma().x;//Luma and more
          BC.rgb /= max(GS, 0.001);
          BC.a    = max(0.0, GS - CBT_Adjust);
          BC.rgb *= BC.a;
		  //Bloom Saturation
		  if(Auto_Bloom == 2 || Auto_Bloom == 4) //Desaturate
		  	Saturate_A *= lerp(0.0 + Saturate_B,1,AL);
		  	
		  if(Auto_Bloom == 3 || Auto_Bloom == 5) //Saturate
		  	Saturate_A *= lerp(2.0 - Saturate_B,1,AL);
	
		  BC.rgb  = lerp(BC.a, BC.rgb, min(10,Saturate_A));

    return saturate(float4(BC.rgb,GS));
}

void PS_CurrentInfo(float4 pos : SV_Position, float2 texcoords : TEXCOORD, out float4 Color : SV_Target0)//, out float4 bloom2 : SV_Target1)
{
	Color = Color_GS(tex2D(BackBuffer, texcoords));
}

float4 BloomBlur(sampler2D Sample, float2 texcoords, float Size_A, int BA , float Mip)
{ 
	//This code was  based on the code here https://xorshaders.weebly.com/tutorials/blur-shaders-5-part-2
	// Can be used for Flairs
	const float sigma   = 10.0;             // Gaussian sigma
    const float Pi = 3.14159265359;
    const float Tau = Pi * 2.0;
	//RADIAL GAUSSIAN BLUR
    int Directions = BA; // BLUR DIRECTIONS (Default 16 - More is better but slower) 4 6 8 12 16
    float norm, Quality = float(sigma * 0.3); //More is better but slower

    float2 Radius_A = Size_A * pix.xy; // BLUR SIZE (Radius)
    // Pixel colour
    float4 Color = tex2D( Sample, texcoords);
    
    [loop] // Blur calculations
    for( float d=0.0; d<Tau; d+=Tau/Directions)
    {
		for(float i=rcp(Quality); i<=1.0; i+=rcp(Quality))
        {
        	float coeff = exp(-0.5 * d * d * i / (sigma * sigma));
			Color += tex2Dlod( Sample, float4(texcoords+float2(cos(d),sin(d))*Radius_A*i,0,Mip)).rgba * coeff;
			norm += coeff;		
        }
    }
    
    // Output to screen
    Color *= rcp(norm);
    
return Color;                               
}

float4 GemsBlur(sampler2D Sample, float2 texcoords, float Size_A, int Dir)
{ 
const float sigma   = 10.0;             // Gaussian sigma
const int   support = int(sigma * 3.0); // int(sigma * 3.0) truncation
float2 SA = Size_A * pix;
	float4 acc;                      // accumulator
	float norm;
	[loop]
	for (int hv = -support; hv <= support; hv++) 
	{
		float2 D = Dir ? float2(0,hv) : float2(hv,0);
		float coeff = exp(-0.5 * hv * hv / (sigma * sigma));
		acc += tex2Dlod(Sample, float4(texcoords + D * SA,0, min(lerp(0,1,BloomSpread),abs(hv)) )).rgba * coeff;
		norm += coeff;
	}
	
	acc *= rcp(norm);	
	return acc;
}

void Blur_HV0(in float4 position : SV_Position, in float2 texcoords : TEXCOORD, out float4 color : SV_Target0)
{
	float S0 = BloomSpread * PP;
		
	color = GemsBlur(SamplerCBB, texcoords ,S0, 0).rgba;
}

void CombBlur_HV1(in float4 position : SV_Position, in float2 texcoords : TEXCOORD, out float4 color : SV_Target0 )
{
	float S0 = BloomSpread * PP;
 
	color = GemsBlur(SamplerBlur_HV0, texcoords, S0, 1).rgba;
 }


float4 Final_Bloom(float2 texcoords)
{
	float TM = Bloom_Spread.x * rcp(Bloom_Max*0.5), S0 = BloomSpread * 100.0;
 
	 return BloomBlur(SamplerBlur_HV1,texcoords,S0, Petal_Ammount , TM).rgba;
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
	
float4 HDROut(float2 texcoords : TEXCOORD0)
{
	float AL = Auto_Luma().y, Ex;
	
	if(Auto_Exposure == 1)
		Ex = Exp;
	else
		Ex = lerp(0,2.5,Scale(Exp, 4, -4));
	
	float BI_Brightness = lerp(1,0.001,Bloom_Intensity.x), NC = Bloom_Intensity.y;

	if(Auto_Bloom == 1 || Auto_Bloom == 4 || Auto_Bloom == 5)
		NC *= max(0.25,Auto_Luma().x);

	float3 Bloom = Final_Bloom( texcoords ).rgb;
	
	float4 Out;
    float3 Color = tex2D(BackBuffer, texcoords).rgb;

	if(Tonemappers >= 1)	 
   	Color = lerp(Luma(Color),Color,Saturate);

	// Do inital de-gamma of the game image to ensure we're operating in the correct colour range.
	if( Gamma > 1. )
		Color = pow(abs(Color),Gamma);
	
	//Bloom should be applied before Tonemapping as otherwise all high ranges will be lost.
	Color += lerp( 0.0, (( Bloom - 0 ) / ( BI_Brightness - 0)), saturate(NC));
	
	float3 Store_Color = Color;

	//Tone map all the things
	//Using the optimized tonemapper by Jim Hejl and Richard Burgess-Dawson 0.148 for the GreyValue works for me. https://imgflip.com/i/oos2z But, I will alow for this to be adjusted.
	if(Auto_Exposure == 1)
		Ex = CalcExposedColor(AL,Ex,GreyValue);
	else
		Ex = Ex;
	
	if(Tonemappers == 0)
		Color = TimothyTonemapper(Color,Ex);
	else if (Tonemappers == 1)
		Color = HableTonemap(Color,Ex);	
	else if (Tonemappers == 2)
		Color = DX11DSK(Color,Ex);
	else if (Tonemappers == 3)
		Color = ACESFilm(Color,Ex);	
	else if (Tonemappers == 4)
		Color = WPLBR(Color,Ex);	
	else
		Color = Color * Ex;	
		
	#if Flare_A
		Color += tex2D(SamplerFlare, texcoords).rgb;
	#endif
	
	#if Flare_B
		Color += tex2D(SamplerLensFlare, texcoords).rgb;
	#endif
	
	// Do the post-tonemapping gamma correction
	//if( Gamma > 1. )
		Color = pow(abs(Color),rcp(2.2));

	if(Tonemappers >= 1)
		Color = (Color - 0.5) * (Contrast) + 0.5; 

	#if HDR_Toggle
	Color = ApplyPQ(Color);
	#endif	

	if (Debug_View == 0)
		Out = float4(Color, 1.);
	else if(Debug_View == 1)
		Out = float4(pow(( abs(Bloom) - 0 ) / ( BI_Brightness - 0),rcp(Gamma)), 1.);
	else
		Out = texcoords.y < 0.975 ? HeatMap(Luma( Color )): HeatMap(texcoords.x);
	
	return float4(Out.rgb,1.0);
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
	//Color = saturate(smoothstep(0,1,1-tex2D(SamplerTA,0.0).x)); 
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
        RenderTarget = texCurrent;
    }
		pass MIP_Blur_HV_One
	{
		VertexShader = PostProcessVS;
		PixelShader = Blur_HV0;
		RenderTarget0 = texMBlur_HV0;
	}
		pass MIP_Blur_HV_Two
	{
		VertexShader = PostProcessVS;
		PixelShader = CombBlur_HV1;
		RenderTarget0 = texMBlur_HV1;
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
	}

}
