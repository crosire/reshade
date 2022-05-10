////-----------//
///**GloomAO**///
//-----------////

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////                                               																									*//
//For Reshade 4.0+ SSDO Ver 0.2.7
//-----------------------------
//                                                                Screen Space Directional Occlusion
//
// Due Diligence
//
// Normal from Depth PureDepthAO
// http://theorangeduck.com/page/pure-depth-ssao
// - Daniel Holden
//   http://theorangeduck.com/page/about
//
// Approximating Dynamic Global Illumination in Image Space
// https://people.mpi-inf.mpg.de/~ritschel/Papers/SSDO.pdf
// - Tobias Ritschel | Thorsten Grosch | Hans-Peter Seidel
//   MPI Informatik - The Max Planck Institute for Informatics
//
// SSGI - OpenGL demonstration project - SSAO vs SSDO
// https://github.com/jdupuy/ssgi/blob/master/src/shaders/ssgi.glsl
// - Jonathan Dupuy AKA jdupuy
//   http://onrendering.com
//
// SSDO - ReShade Shader
// https://github.com/pascalmatthaeus/ppfx/blob/master/Shaders/PPFX_SSDO.fx
// - Pascal Matthäus AKA Euda
//   https://github.com/pascalmatthaeus
//
// Poisson Sampling Generator
// https://github.com/bartwronski/PoissonSamplingGenerator
// - Bart Wronski AKA bartwronski
//   http://bartwronski.com/
//
// Interleaved Gradient Noise
// http://www.iryoku.com/next-generation-post-processing-in-call-of-duty-advanced-warfare
// - Jorge Jimenez
//   https://www.iryoku.com/aboutme
//
// Wagner Based Poisson Noise
// https://github.com/spite/Wagner/blob/master/fragment-shaders/poisson-disc-blur-fs.glsl
// - Jaume Sanchez AKA spite
//   http://www.clicktorelease.com
//
// Adapted Median
// https://github.com/brimson/reshaders/blob/12619ecb296e058a4b20ec443d54611d87dd7e9c/shaders/cMotionBlur.fx
// - Paul Dang AKA Brimson
//   https://github.com/brimson
//
//  Temporal AA "Epic Games" implementation + Some Magic
// - yvtjp
//   https://www.shadertoy.com/view/4tcXD2
//   https://de45xmedrsdbp.cloudfront.net/Resources/files/TemporalAA_small-59732822.pdf
//   https://yvt.jp/
//
// If I missed any please tell me.
//
// Special Thank ????
//
// LICENSE
// ============
// Overwatch & Code out side the work of people mention above is licenses under: Attribution-NoDerivatives 4.0 International
//
// You are free to:
// Share - copy and redistribute the material in any medium or format
// for any purpose, even commercially.
//
// The licensor cannot revoke these freedoms as long as you follow the license terms.
//
// Under the following terms:
// Attribution - You must give appropriate credit, provide a link to the license, and indicate if changes were made.
// You may do so in any reasonable manner, but not in any way that suggests the licensor endorses you or your use.
//
// NoDerivatives - If you remix, transform, or build upon the material, you may not distribute the modified material.
//
// No additional restrictions - You may not apply legal terms or technological measures that legally restrict others from doing anything the license permits.
//
// https://creativecommons.org/licenses/by-nd/4.0
//
// Have fun,
// Written by Jose Negrete AKA BlueSkyDefender <UntouchableBlueSky@gmail.com>, October 2021
// https://github.com/BlueSkyDefender/Depth3D
//
// Notes to the other developers: https://github.com/BlueSkyDefender/AstrayFX
//
// GloomAO Update Notes are at the bottom
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#if exists "Overwatch.fxh"                                           //Overwatch Interceptor//
	#include "Overwatch.fxh"
	#define OS 0
#else// DA_Y = [Depth Adjust] DA_Z = [Offset] DA_W = [Depth Linearization] DB_X = [Depth Flip]
	static const float DA_Y = 7.5, DA_Z = 0.0, DA_W = 0.0, DB_X = 0;
	// DC_X = [Barrel Distortion K1] DC_Y = [Barrel Distortion K2] DC_Z = [Barrel Distortion K3] DC_W = [Barrel Distortion Zoom]
	static const float DC_X = 0, DC_Y = 0, DC_Z = 0, DC_W = 0;
	// DD_X = [Horizontal Size] DD_Y = [Vertical Size] DD_Z = [Horizontal Position] DD_W = [Vertical Position]
	static const float DD_X = 1, DD_Y = 1, DD_Z = 0.0, DD_W = 0.0;
	//Triggers
	static const int RE = 0, NC = 0, RH = 0, NP = 0, ID = 0, SP = 0, DC = 0, HM = 0, DF = 0, NF = 0, DS = 0, LBC = 0, LBM = 0, DA = 0, NW = 0, PE = 0, FV = 0, ED = 0;
	//Overwatch.fxh State
	#define OS 1
#endif

//Depth Buffer Adjustments
#define DB_Size_Position 0     //[Off | On]         This is used to reposition and adjust the size of the depth buffer.
#define BD_Correction 0        //[Off | On]         Barrel Distortion Correction for non conforming BackBuffer.

//Other Settings
#define Text_Info_Key 93       //Menu Key            Text Information Key Default 93 is the Menu Key. You can use this site https://keycode.info to pick your own.
#define Disable_Debug_Info 0   //[Off | On]          Use this to disable help information that gives you hints for fixing many games with Overwatch.fxh.
#define Minimize_Web_Info 0    //[Off | On]          Use this to minimize the website logo on startup.
#define Force_Texture_Details 0//[Off | On]          This is used to add Texture Detail AO into SSDO output. WIP
#define SSDO_Buffer_Size 2     //   [1-2]            You can use this to set the resolution of the main SSDO Buffer.

//Help / Guide Information stub uniform a idea from LucasM
uniform int GloomAO <
	ui_text = "GloomAO is an Screen Space Directional Occlusion algorithm based on generalize SSAO. SSDO was created by the people at MPI Informatik.\n"
					  "The researches Tobias Ritschel, Thorsten Grosch, and Hans-Peter Seidel. Where instrumental in making this happen, so Thank you.\n"
				  		"SSDO physically plausible occlusion allows better simulated depth cues based on gradient and contrast information.\n"
				  			  "This AO shader is free and shouldn't sit behind a paywall. If you paid for this shader ask for a refund right away.\n"
				  			  		"As for my self I do want to provide the community with free shaders and any donations will help keep that motivation alive.\n"
			 	 			  			  "For more information and help please feel free to visit http://www.Depth3D.info or https://blueskydefender.github.io/AstrayFX\n "
			  				  			 	 "Help with this shader fuctions specifically visit the WIki @ https://github.com/BlueSkyDefender/AstrayFX/wiki/RadiantGI\n"
			  "Please enjoy this shader and Thank You for using GloomAO.";
	ui_category = "GloomAO";
	ui_category_closed = true;
	ui_label = " ";
	ui_type = "radio";
>;
//uniform float3 TEST < ui_type = "slider"; ui_min = 0; ui_max = 1; > = 1.0;
uniform int SSDO_MipSampling <
	ui_type = "combo";
	ui_items = "Full Resolution\0Half Resolution\0Quarter Resolution\0Full Resolution Adptive\0Half Resolution Adaptive\0Quarter Resolution Adaptive\0";
    ui_label = "Sampling Quality";
    ui_tooltip = "Use this to improve performance by sampling Mipmaps.\n"
				 "Artifacts are more prominent at lower quality settings.";
	ui_category = "SSDO";
> = 4;

uniform int SSDO_Levels <
	ui_type = "slider";
    ui_min = 0; ui_max = 64;
    ui_label = "Samples";
    ui_tooltip = "The Samples slider is used to increase or decrease samples amount as a side effect this reduces noise at the cost of performance.";
	ui_category = "SSDO";
> = 32;

uniform float SSDO_SampleRadius <
	ui_type = "slider";
    ui_min = 1.0; ui_max = 5000.0;// ui_step = 0.1;
    ui_label = "Sampling Radius";
    ui_tooltip = "This lets you extend the Sample Radius or Sample Range of the SSDO Algo.\n"
				 "Setting this too high will decrease performance.";//High values reduce cache coherence, This will lead to cache misses and decrease performance.
	ui_category = "SSDO";
> = 2500.0;
#if Force_Texture_Details
uniform float SSDO_2DTexture_Detail <
	ui_type = "slider";
	ui_min = 0.0; ui_max = 1.0;
	ui_label = "Texture Details";
	ui_tooltip = "Lets you add Texture Details to SSDO so you can have Obscurance on 2D information.\n"
			     "Defaults is [0.0] Off";
	ui_category = "SSDO";
> = 0.0;
#else
static const int SSDO_2DTexture_Detail = 0;
#endif
uniform float2 NCD <
	ui_type = "slider";
	ui_min = 0.0; ui_max = 1.0;
	ui_label = "Near Details";
	ui_tooltip = "Lets you adjust detail of objects near the cam and or like weapon hand AO.\n"
			     "The 2nd Option is for Weapon Hands in game that fall out of range.\n"
			     "Defaults are [Near Details X 0.125] [Weapon Hand Y 0.0]";
	ui_category = "SSDO";
> = float2(0.125,0.0);

uniform float SSDO_Trimming <
	ui_type = "slider";
    ui_min = 0; ui_max = 0.5;
    ui_label = "Depth Trimming";
    ui_tooltip = "Use this to limit the local falloff of the ao in the image also known as Depth Range Check.";
	ui_category = "SSDO";
> = 0.1;

uniform float SSDO_Fade <
	ui_type = "slider";
    ui_min = -1.0; ui_max = 1.0;
	ui_label = "Depth Fade-Out";
	ui_tooltip = "SSDO Application Power that is based on Depth scaling for controlled fade In-N-Out.\n" //That's What A Hamburger's All About
			     "Can be set from -1 to 1 and is Set to Zero for No Culling.\n"
			     "Default is 0.5.";
	ui_category = "SSDO";
> = 1.0;
/*
uniform int BM <
	ui_type = "combo";
    ui_label = "Blend Mode";
    ui_tooltip = "Use this to change the look of GI when applied to the final image.";
    ui_items = "Mix\0Overlay\0Softlight\0Add\0";
    ui_category = "Image";
    > = 0;
*/
static const int Blend_Mode = 3; //Phased out Since I wanted this shader to be more simple.

uniform int SSDO_X2 < //Thank you Nathaniel for the name
	ui_type = "combo";
	ui_items = "Square Off\0Square Input\0Square Luma Positive\0Square Luma Negitive\0";
	ui_label = "Square Input";
	ui_tooltip = "This option squares the input of BackBuffer for SSDO.\n"
				 "You can also favor Illuminated areas or Darker areas.\n"
				 "This is basically effects the GI Color and AO, Default is Off.";
	ui_category = "Image";// Starting to define my look.
> = 0;

uniform float SSDO_Power<
	ui_type = "slider";
    ui_min = 0.0; ui_max = 5.0;
    ui_label = "Total Power";
    ui_tooltip = "This option controls the over all intensity of the effect.";
	ui_category = "Image";
> = 1.5;

uniform float SSDO_ColorPower<
	ui_type = "slider";
    ui_min = 0.0; ui_max = 5.0;
    ui_label = "Color Power";
    ui_tooltip = "This option controls the GI Color intensity.";
	ui_category = "Image";
> = 1.5;

uniform float SSDO_Saturation <
	ui_type = "slider";
    ui_min = 0.0; ui_max = 5.0;
    ui_label = "Saturation";
    ui_tooltip = "Applys color saturation to the indriect bounce of light.";
	ui_category = "Image";
> = 1.0;

uniform float SSDO_Intensity_Masking <
	ui_type = "slider";
    ui_min = 0.0; ui_max = 1.0;
    ui_label = "Intensity Mask";
    ui_tooltip = "Mask out intense light sources from receiving AO.";
	ui_category = "Image";
> = 0.0;

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
	ui_min = 1.0; ui_max = 500.0;
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

uniform float SSDO_Max_Depth <
	ui_type = "slider";
    ui_min = 0.5; ui_max = 1.0;
	ui_label = "Max Depth";
	ui_tooltip = "SSDO Max Depth is used to limit the max range SSDO should pull information from the Depth Buffer.\n" //That's What A Hamburger's All About
			     "Think about it like a Wall where all information stops, keep in mind this is not like Depth Fade.\n"
			     "Default is 0.999.";
	ui_category = "Depth Map";
> = 0.999;

uniform bool Depth_Map_Flip <
	ui_label = "Depth Map Flip";
	ui_tooltip = "Flip the depth map if it is upside down.";
	ui_category = "Depth Map";
> = DB_X;

uniform int Debug <
	ui_type = "combo";
	ui_items = "GloomAO\0Debug\0Depth & Normals\0";
	ui_label = "Debug View";
	ui_tooltip = "View Debug Buffers.";
	ui_category = "Extra Options";
> = 0;

uniform int SamplesXY <
	ui_type = "slider";
	ui_min = 1; ui_max = 6;
	ui_label = "Denoise Power";//Ya CeeJay.dk you got your way..
	ui_tooltip = "This raises or lowers Samples used for the Final DeNoisers which in turn affects Performance.\n"
				 "This also has the side effect of smoothing out the image so you get that Normal Like Smoothing.\n"
				 "Default is 3 and you can override this a bit.";
	ui_category = "Extra Options";
> = 3;

uniform float Persistence <
	ui_type = "slider";
	ui_min = 0.0; ui_max = 1.00;
	ui_label = "TAA Power";
	ui_tooltip = "Increase persistence of the frames this is really the Temporal Part.\n"
				 "Default is 0.125 and a value of 1.0 is off.";
	ui_category = "Extra Options";
> = 0.125;

uniform bool Dither_SSDO <
	ui_label = "Dither SSDO";
	ui_tooltip = "Add Noise to AO so that it can limit banding in some game.";
	ui_category = "Extra Options";
> = true;

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
#if BD_Correction
uniform int BD_Options <
	ui_type = "combo";
	ui_items = "On\0Off\0";
	ui_label = "Distortion Options";
	ui_tooltip = "Use this to Turn BD Off or On.\n"
				 "Default is ON.";
	ui_category = "Depth Corrections";
> = 0;

uniform float3 Colors_K1_K2_K3 <
	#if Compatibility
	ui_type = "drag";
	#else
	ui_type = "slider";
	#endif
	ui_min = -2.0; ui_max = 2.0;
	ui_tooltip = "Adjust the Distortion K1, K2, & K3.\n"
				 "Default is 0.0";
	ui_label = "BD K1 K2 K3";
	ui_category = "Depth Corrections";
> = float3(DC_X,DC_Y,DC_Z);

uniform float Zoom <
	ui_type = "drag";
	ui_min = -0.5; ui_max = 0.5;
	ui_label = "BD Zoom";
	ui_category = "Depth Corrections";
> = DC_W;
#else
	#if DC
	uniform bool BD_Options <
		ui_label = "Toggle Barrel Distortion";
		ui_tooltip = "Use this if you modded the game to remove Barrel Distortion.";
	ui_category = "Depth Corrections";
	> = !true;
	#else
		static const int BD_Options = 1;
	#endif
static const float3 Colors_K1_K2_K3 = float3(DC_X,DC_Y,DC_Z);
static const float Zoom = DC_W;
#endif
#if BD_Correction || DB_Size_Position
	uniform bool Depth_Guide <
		ui_label = "Alinement Guide";
		ui_tooltip = "Use this for a guide for alinement.";
	ui_category = "Depth Corrections";
	> = !true;
#else
		static const int Depth_Guide = 0;
#endif
//Use for real HDR. //Do not Use.
#define HDR_Toggle 0 //For HDR //Do not Use.
uniform bool Text_Info < source = "key"; keycode = Text_Info_Key; toggle = true; mode = "toggle";>;
uniform int framecount < source = "framecount"; >;     // Total amount of frames since the game started.
#define Alternate framecount % 2.0 == 0                  // Alternate per frame

texture DepthBufferTex : DEPTH;

sampler ZBufferSSDO
	{
		Texture = DepthBufferTex;
	};

texture BackBufferTex : COLOR;

sampler BackBufferSSDO
	{
		Texture = BackBufferTex;
	};

texture2D accuTexSSDO { Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT ; Format = RGBA8; MipLevels = 4; };
sampler2D SSDOaccuFrames { Texture = accuTexSSDO; };

texture texNormalsSSDO { Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = RG16f; MipLevels = 4; };

sampler SamplerNormalsSSDO
{
	Texture = texNormalsSSDO;
};

texture texColorsSSDO { Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = RGBA8; MipLevels = 8; };

sampler SamplerColorsSSDO
{
	Texture = texColorsSSDO;
};

texture texDepthSSDO { Width = BUFFER_WIDTH ; Height = BUFFER_HEIGHT; Format = R32f; MipLevels = 4; };

sampler SamplerDepthSSDO
{
	Texture = texDepthSSDO;
};

texture texSSDO { Width = BUFFER_WIDTH / SSDO_Buffer_Size; Height = BUFFER_HEIGHT / SSDO_Buffer_Size; Format = RGBA8; MipLevels = 2; };

sampler SamplerSSDO
{
	Texture = texSSDO;
};

texture texCEAGD_H_SSDO { Width = BUFFER_WIDTH / 2; Height = BUFFER_HEIGHT / 2; Format = RGBA8; };

sampler SamplerSSDOH
{
	Texture = texCEAGD_H_SSDO;
};

texture texCEAGD_V_SSDO { Width = BUFFER_WIDTH ; Height = BUFFER_HEIGHT; Format = RGBA8; };

sampler SamplerSSDOV
{
	Texture = texCEAGD_V_SSDO;
};
//Pre Defined value for PI
#define PI 3.14159265358979323846264
#define pix float2(BUFFER_RCP_WIDTH,BUFFER_RCP_HEIGHT)
uniform float clock < source = "timer"; >;             // A timer that starts when the Game starts.

static const float2 XYoffset[8] = { float2( 0,+pix.y ), float2( 0,-pix.y), float2(+pix.x, 0), float2(-pix.x, 0), float2(-pix.x,-pix.y), float2(+pix.x,-pix.y), float2(-pix.x,+pix.y), float2(+pix.x,+pix.y) };

float Scale_SSDO_Fade()
{
	return lerp(0,2,saturate(abs(SSDO_Fade)) );
}

float Offset_Switch()
{
	return Offset >= -0.0015 && Offset <= 0.0015 ? 0 : Offset;  
}

static const float2 POISSON_SAMPLES[64] =
{
float2( 0.5273496125406625f, 0.32843211798055333f ),
float2( -0.9204308268276714f, -0.07983643921713124f ),
float2( 0.6643335271236648f, -0.6896971714892444f ),
float2( -0.4485055311020943f, -0.8026704829903724f ),
float2( -0.5089046351832555f, 0.6458887612871638f ),
float2( 0.08897250178211012f, -0.2806489141686402f ),
float2( -0.29838092677534184f, 0.10278438048827801f ),
float2( -0.862115316272164f, -0.48356308603032117f ),
float2( 0.5999430799342932f, -0.14561568677130635f ),
float2( 0.08785280092966417f, -0.9462682543249354f ),
float2( 0.07087614249781364f, 0.8602082808602768f ),
float2( -0.8534726000488865f, 0.29681151269733946f ),
float2( 0.9539380967370391f, 0.030097899562570734f ),
float2( 0.10087936756781524f, 0.34573585367530424f ),
float2( -0.20159182487876662f, -0.4490941013720869f ),
float2( 0.6782242471078768f, 0.6658563096694241f ),
float2( -0.10394741987685427f, -0.7221204563776964f ),
float2( 0.319809955759203f, -0.6790019202399492f ),
float2( 0.3839401781148807f, -0.3690247708221736f ),
float2( -0.22207725541276904f, 0.849453247755017f ),
float2( -0.47529541958366694f, 0.37684164173216f ),
float2( -0.5367298351175375f, -0.19642553224785247f ),
float2( -0.13522275308104328f, 0.560514350786896f ),
float2( 0.3870397244317962f, -0.14841260985099053f ),
float2( 0.37049654041868246f, 0.5154922403112734f ),
float2( 0.6672724378901226f, -0.5213728466474493f ),
float2( 0.7826123622504166f, 0.4639934736267554f ),
float2( -0.015411930763345674f, 0.014250559941404296f ),
float2( -0.4760921395716165f, 0.022730166563585057f ),
float2( -0.6084367587131735f, -0.5187337391472053f ),
float2( 0.2795395707170242f, 0.16224024832087033f ),
float2( 0.3736006279406249f, 0.9117234096211635f ),
float2( -0.6919417915679925f, -0.07760236473727093f ),
float2( 0.0744476719248192f, 0.20311070507786347f ),
float2( -0.2222943155587649f, -0.9478651097451272f ),
float2( -0.10033941671817023f, 0.3960078920620986f ),
float2( 0.4568029060767225f, -0.8791627760764141f ),
float2( -0.8785382515186919f, 0.10180549622612695f ),
float2( 0.8537682559640656f, -0.38658822274799526f ),
float2( 0.0609609200214567f, -0.5420008791782059f ),
float2( -0.23669353949304686f, -0.26999886626839875f ),
float2( -0.7151483708986462f, 0.4483280195001876f ),
float2( -0.47610621490338256f, -0.4030389610541533f ),
float2( -0.6126689926468303f, -0.7490506324903827f ),
float2( -0.255018095671136f, -0.1266388390537941f ),
float2( 0.12077897961942986f, -0.7580461130803654f ),
float2( 0.25776824865464415f, -0.273887482324501f ),
float2( 0.36800354072014885f, 0.03615086613505087f ),
float2( 0.09086994649736232f, 0.5592777484289124f ),
float2( 0.3475335327722212f, 0.7240680900695486f ),
float2( -0.3390135610972872f, 0.43925808523465654f ),
float2( 0.20295677355509983f, 0.9663359071038066f ),
float2( -0.4532808819917399f, 0.8089615780465481f ),
float2( -0.09813012107560773f, -0.1766103593720822f ),
float2( 0.9222515851215884f, 0.283088863183107f ),
float2( -0.7572137302598909f, 0.5924401327732448f ),
float2( 0.27251111237710013f, 0.390873918753542f ),
float2( -0.7292711663101386f, -0.3397294444419196f ),
float2( -0.118184652957795f, 0.16462830825770766f ),
float2( -0.41261675793648533f, -0.6389515272662722f ),
float2( 0.7148481592421466f, 0.19825111300217793f ),
float2( 0.13181440606510209f, -0.13462617904858257f ),
float2( 0.6564465799531382f, -0.3053207424251268f ),
float2( -0.6667623629118123f, 0.1886689773307198f ),
};

// YUV-RGB conversion routine from Hyper3D
float3 encodePalYuv(float3 rgb)
{
	float3 RGB2Y =  float3( 0.299, 0.587, 0.114);
	float3 RGB2Cb = float3(-0.169,-0.331, 0.500);
	float3 RGB2Cr = float3( 0.500,-0.419,-0.081);

	return float3(dot(rgb, RGB2Y), dot(rgb, RGB2Cb), dot(rgb, RGB2Cr));
}

float3 decodePalYuv(float3 ycc)
{
	float3 YCbCr2R = float3( 1.000, 0.000, 1.400);
	float3 YCbCr2G = float3( 1.000,-0.343,-0.711);
	float3 YCbCr2B = float3( 1.000, 1.765, 0.000);

	return float3(dot(ycc, YCbCr2R), dot(ycc, YCbCr2G), dot(ycc, YCbCr2B));
}

float3 HUEToRGB( in float H )
{
    return saturate( float3( abs( H * 6.0f - 3.0f ) - 1.0f,
                                2.0f - abs( H * 6.0f - 2.0f ),
                                2.0f - abs( H * 6.0f - 4.0f )));
}

float3 RGBToHCV( in float3 RGB )
{
    // Based on work by Sam Hocevar and Emil Persson
    float4 P         = ( RGB.g < RGB.b ) ? float4( RGB.bg, -1.0f, 2.0f/3.0f ) : float4( RGB.gb, 0.0f, -1.0f/3.0f );
    float4 Q1        = ( RGB.r < P.x ) ? float4( P.xyw, RGB.r ) : float4( RGB.r, P.yzx );
    float C          = Q1.x - min( Q1.w, Q1.y );
    float H          = abs(( Q1.w - Q1.y ) / ( 6.0f * C + 0.000001f ) + Q1.z );
    return float3( H, C, Q1.x );
}

float3 RGBToHSL( in float3 RGB )
{
    RGB.xyz          = max( RGB.xyz, 0.000001f );
    float3 HCV       = RGBToHCV(RGB);
    float L          = HCV.z - HCV.y * 0.5f;
    float S          = HCV.y / ( 1.0f - abs( L * 2.0f - 1.0f ) + 0.000001f);
    return float3( HCV.x, S, L );
}

float3 HSLToRGB( in float3 HSL )
{
    float3 RGB       = HUEToRGB(HSL.x);
    float C          = (1.0f - abs(2.0f * HSL.z - 1.0f)) * HSL.y;
    return ( RGB - 0.5f ) * C + HSL.z;
}

float3 Saturator(float3 C)
{
	C.rgb = RGBToHSL(C.rgb);
	C.y *= SSDO_Saturation;
	return HSLToRGB(C.rgb);
}

float max3(float x, float y, float z)
{
    return max(x, max(y, z));
}

float3 inv_Tonemapper(float4 color)
{   //Timothy Lottes fast_reversible
	return color.rgb * rcp((1.0 + max(color.w,0.001)) - max3(color.r, color.g, color.b));
}

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

float fmod(float a, float b)
{
	float c = frac(abs(a / b)) * abs(b);
	return a < 0 ? -c : c;
}

float MCNoise(float2 TC,float FC ,float seed)
{   //This is the noise I used for rendering
	float motion = FC, a = 12.9898, b = 78.233, c = 43758.5453, dt = dot(TC.xy + 0.5, float2(a,b)), sn = fmod(dt,PI + seed) * motion;
	return frac( sin( sn ) * c );
}   int T_01() { return 12500; }

#define Scale SSDO_Buffer_Size
float Interleaved_Gradient_Noise(float2 TC)
{   //Magic Numbers
    float3 MNums = float3(0.06711056, 0.00583715, 52.9829189);
    return frac( MNums.z * frac(dot(TC,MNums.xy)) );
}

#if BD_Correction || DC
float2 D(float2 p, float k1, float k2, float k3) //Lens + Radial lens undistort filtering Left & Right
{   // Normalize the u,v coordinates in the range [-1;+1]
	p = (2. * p - 1.);
	// Calculate Zoom
	p *= 1 + Zoom;
	// Calculate l2 norm
	float r2 = p.x*p.x + p.y*p.y;
	float r4 = r2 * r2;
	float r6 = r4 * r2;
	// Forward transform
	float x2 = p.x * (1. + k1 * r2 + k2 * r4 + k3 * r6);
	float y2 = p.y * (1. + k1 * r2 + k2 * r4 + k3 * r6);
	// De-normalize to the original range
	p.x = (x2 + 1.) * 1. * 0.5;
	p.y = (y2 + 1.) * 1. * 0.5;

return p;
}
#endif

float Depth_Info(float2 texcoord)
{
	#if BD_Correction || DC
	if(BD_Options == 0)
	{
		float3 K123 = Colors_K1_K2_K3 * 0.1;
		texcoord = D(texcoord.xy,K123.x,K123.y,K123.z);
	}
	#endif
	#if DB_Size_Position || SP || LBC || LB_Correction
		texcoord.xy += float2(-Image_Position_Adjust.x,Image_Position_Adjust.y)*0.5;
	#if LBC || LB_Correction
		float2 H_V = Horizontal_and_Vertical * float2(1,LBDetection() ? 1.315 : 1 );
	#else
		float2 H_V = Horizontal_and_Vertical;
	#endif
		float2 midHV = (H_V-1) * float2(BUFFER_WIDTH * 0.5,BUFFER_HEIGHT * 0.5) * pix;
		texcoord = float2((texcoord.x*H_V.x)-midHV.x,(texcoord.y*H_V.y)-midHV.y);
	#endif

	if (Depth_Map_Flip)
		texcoord.y =  1 - texcoord.y;

	//Conversions to linear space.....
	float zBuffer = tex2Dlod(ZBufferSSDO, float4(texcoord,0,0)).x, zBufferWH = zBuffer, Far = 1.0, Near = 0.125/(0.00000001+Depth_Map_Adjust), NearWH = 0.125/(Depth_Map ? NCD.y : 10*NCD.y), OtherSettings = Depth_Map ? NCD.y : 100 * NCD.y ; //Near & Far Adjustment
	//Man Why can't depth buffers Just Be Normal
	float2 C = float2( Far / Near, 1.0 - Far / Near ), Z = Offset_Switch() < 0 ? min( 1.0, zBuffer * ( 1.0 + abs(Offset_Switch()) ) ) : float2( zBuffer, 1.0 - zBuffer ), Offsets = float2(1 + OtherSettings,1 - OtherSettings), zB = float2( zBufferWH, 1-zBufferWH );

	if(Offset_Switch() > 0 || Offset_Switch() < 0)
	Z = Offset_Switch() < 0 ? float2( Z.x, 1.0 - Z.y ) : min( 1.0, float2( Z.x * (1.0 + Offset_Switch()) , Z.y / (1.0 - Offset_Switch()) ) );

	if (NCD.y > 0)
	zB = min( 1, float2( zB.x * Offsets.x , zB.y / Offsets.y  ));

	if (Depth_Map == 0)
	{   //DM0 Normal
		zBuffer = rcp(Z.x * C.y + C.x);
		zBufferWH = Far * NearWH / (Far + zB.x * (NearWH - Far));
	}
	else if (Depth_Map == 1)
	{   //DM1 Reverse
		zBuffer = rcp(Z.y * C.y + C.x);
		zBufferWH = Far * NearWH / (Far + zB.y * (NearWH - Far));
	}

	return  saturate( lerp(NCD.y > 0 ? zBufferWH : zBuffer,zBuffer,0.925) );
}  int T_02() { return 25000; }
#if Force_Texture_Details
void SSDOColors(float4 vpos : SV_Position, float2 texcoords : TEXCOORD,
						out float4 Colors : SV_Target0)
{	   float Intensity = 1-dot(0.333,tex2Dlod(BackBufferSSDO,float4(texcoords,0,0)).xyz) > lerp(0.0,0.5,SSDO_Intensity_Masking);
		Colors = float4(tex2D(BackBufferSSDO,texcoords).rgb, Intensity);
}
#endif
float2 PackNormals(float3 n)
{
    float f = rsqrt(8*n.z+8);
    return n.xy * f + 0.5;
}

float SUMTexture_lookup(float2 TC, float dx, float dy)
{   float Depth = 1-Depth_Info( TC );
		  Depth = (Depth - 0)/ (lerp(1,10,saturate(1-SSDO_2DTexture_Detail)) - 0);
    float2 uv = (TC.xy + float2(dx , dy ) * pix);
    float3 c = tex2Dlod( SamplerColorsSSDO, float4(uv.xy,0, 0) ).rgb * 0.5;
    //c = smoothstep(0,1,normalize(c));
	// return as luma
    return (0.2126*c.r + 0.7152*c.g + 0.0722*c.b) * Depth * 0.00666f;
}

float3 TextureNormals(float2 UV, float Depth)
{
	if(saturate(SSDO_2DTexture_Detail) > 0)
	{
		// simple sobel edge detection
	    float dx = 0.0;
	    dx += -1.0 * SUMTexture_lookup(UV, -1.5, -1.5);
	    dx += -2.0 * SUMTexture_lookup(UV, -1.5,  0.0);
	    dx += -1.0 * SUMTexture_lookup(UV, -1.5,  1.5);
	    dx +=  1.0 * SUMTexture_lookup(UV,  1.5, -1.5);
	    dx +=  2.0 * SUMTexture_lookup(UV,  1.5,  0.0);
	    dx +=  1.0 * SUMTexture_lookup(UV,  1.5,  1.5);

	    float dy = 0.0;
	    dy += -1.0 * SUMTexture_lookup(UV, -1.5, -1.5);
	    dy += -2.0 * SUMTexture_lookup(UV,  0.0, -1.5);
	    dy += -1.0 * SUMTexture_lookup(UV,  1.5, -1.5);
	    dy +=  1.0 * SUMTexture_lookup(UV, -1.5,  1.5);
	    dy +=  2.0 * SUMTexture_lookup(UV,  0.0,  1.5);
	    dy +=  1.0 * SUMTexture_lookup(UV,  1.5,  1.5);

		float edge = sqrt(dx*dx + dy*dy);
			  edge *= edge;

		float angle = atan2(dx,dy);

		float X = edge * sin(angle); X = -X;
		float Y = edge * sin(angle + 7.5 * PI / 3.);// Adjust me to rotate Normals
		float Z = edge * (X - Y);

		return min(1,lerp(float3(X,Y,Z) * Depth, 0, float3(X,Y,Z) == 0.5));
	}
	else
		return 0;

}

//PureDepthAO
#if Force_Texture_Details
void NormalsDepth(float4 vpos : SV_Position, float2 texcoords : TEXCOORD,
						out float2 Normals : SV_Target0,
						out float Depth : SV_Target1)
{
#else
void NormalsColorsDepth(float4 vpos : SV_Position, float2 texcoords : TEXCOORD,
						out float2 Normals : SV_Target0,
						out float4 Colors : SV_Target1,
						out float Depth : SV_Target2)
{
#endif
    // WTF right well I got tired of working on this so I just half assed it.
	float2 off1 = float2( pix.x, 0); // right
	float2 off4 = float2( 0,-pix.y); // up
	//A 2x2 Taps is done here. You can also do 4x4 tap
	float2 uv0 = texcoords; // center
	float2 uv1 = texcoords + off1; // right
	float2 uv2 = texcoords + float2(-pix.x, 0); // left
	float2 uv3 = texcoords + float2( 0, pix.y); // down
	float2 uv4 = texcoords + off4; // up

	float depth  = Depth_Info( uv0 );
	float depthR = Depth_Info( uv1 );
	float depthL = Depth_Info( uv2 );
	float depthD = Depth_Info( uv3 );
	float depthU = Depth_Info( uv4 );

	//Had to add depth to offsets 1 and 2 and needed to flip offset 1.....
	float3 P1 = float3( off1 * depth, depth - depthR);
	float3 P2 = float3( off4 * depth, depth - depthD);

	float3 normal = cross(P2, P1);

	float3 P0;

	int best_Z_horizontal = abs(depthR - depth) < abs(depthL - depth) ? 1 : 2;
	int best_Z_vertical = abs(depthD - depth) < abs(depthU - depth) ? 3 : 4;

	if (best_Z_horizontal == 1 && best_Z_vertical == 4)
	{   //triangle 0 = P0: center, P1: right, P2: up
		P1 = float3(uv1 - 0.5, 1) * depthR;
		P2 = float3(uv4 - 0.5, 1) * depthU;
	}
	if (best_Z_horizontal == 1 && best_Z_vertical == 3)
	{   //triangle 1 = P0: center, P1: down, P2: right
		P1 = float3(uv3 - 0.5, 1) * depthD;
		P2 = float3(uv1 - 0.5, 1) * depthR;
	}
	if (best_Z_horizontal == 2 && best_Z_vertical == 4)
	{   //triangle 2 = P0: center, P1: up, P2: left
		P1 = float3(uv4 - 0.5, 1) * depthU;
		P2 = float3(uv2 - 0.5, 1) * depthL;
	}
	if (best_Z_horizontal == 2 && best_Z_vertical == 3)
	{   //triangle 3 = P0: center, P1: left, P2: down
		P1 = float3(uv2 - 0.5, 1) * depthL;
		P2 = float3(uv3 - 0.5, 1) * depthD;
	}

	P0 = float3(uv0 - 0.5, 1) * depth;

	float3 Enormal = cross(P2 - P0, P1 - P0);
	 Enormal.x = -Enormal.x;

	 Enormal = lerp( normal + TextureNormals(texcoords, depth), Enormal,  distance( normalize(normal), normalize(Enormal) ) >= 0.7f );

	Normals = PackNormals(normalize(Enormal));
	#if !Force_Texture_Details
	float Intensity = 1-dot(0.333,tex2Dlod(BackBufferSSDO,float4(texcoords,0,0)).xyz) > lerp(0.0,0.5,SSDO_Intensity_Masking);
	Colors = float4(tex2D(BackBufferSSDO,texcoords).rgb, Intensity);
	#endif
	Depth = depth;
}

float3 UnpackNormals(float2 enc)
{
    float2 fenc = enc*4-2;
    float f = dot(fenc,fenc), g = sqrt(1-f/4);
    float3 n;
    n.xy = fenc*g;
    n.z = 1-f/2;
    return n;
}

float4 NDSampler(float2 TC, float Mip)
{
	float3 Norm = UnpackNormals(tex2Dlod(SamplerNormalsSSDO,float4(TC,0,Mip)).xy);
	float Depth = tex2Dlod(SamplerDepthSSDO,float4(TC,0,Mip)).x;

	return float4(Norm,Depth);
}

float2 Rotate2D( float2 r, float l )
{   float2 Directions;
    sincos(l,Directions[0],Directions[1]);//same as float2(cos(l),sin(l))
    return float2( dot( r, float2(Directions[1], -Directions[0]) ), dot( r, Directions.xy ) );
}

float3 GetPosition(float2 texcoords,float2 raycoords,float Depth, float FDepth)
{
	// Compute Correct Pos
	return float3(float2(raycoords.x-texcoords.x,texcoords.y-raycoords.y) * Depth, Depth - FDepth);
}   int nonplus() { return T_01() == 0 || T_02() == 0 ? 1 : 0;}

float4 SSDO(float4 vpos : SV_Position, float2 texcoords : TEXCOORD) : SV_Target
{
	int SSDO_Samples = clamp(SSDO_Levels,1,64);
	float SSDO_SRM = SSDO_SampleRadius, SSDO_AngleThreshold = 1.0,
		  SSDO_Contribution_Range = SSDO_SRM*pix.x*max(SSDO_Trimming,0.001);//simple scale for aka "Zthiccness."
	const float ATTF = 1e-5; // attenuation factor
	float random = MCNoise( texcoords , 1, 1 ) * 2 - 1, IGN = Interleaved_Gradient_Noise(floor( texcoords.xy / pix / Scale));
	float4 SSDO, Normals_Depth = NDSampler(texcoords, 0);
	float D0 = smoothstep(-NCD.x,1.0,Normals_Depth.w),D_Fade = SSDO_Fade < 0 ? lerp(1-Scale_SSDO_Fade() * 2,0, 1-Normals_Depth.w ) : smoothstep(0,2,Normals_Depth.w / Scale_SSDO_Fade());
	int Mip_Switch = SSDO_MipSampling == 4 || SSDO_MipSampling == 5 ? 1 : 0;
	[loop]
	for (int i = 1.0; i <= SSDO_Samples; i++)
	{   //Ref said to use continue.... But, better perf with discard.
		if(texcoords.x >= 1 || texcoords.y >= 1)
			discard;

		if(Normals_Depth.w > SSDO_Max_Depth)
			continue;
		//Dumb Magic combo Noise.... Ended up liking this for some damn reason. ////normalize(frac(float2(IGN*BUFFER_WIDTH,IGN*BUFFER_HEIGHT)*i)*2.0-1.0)
		float2 RayDir  = (pix * (SSDO_SRM/SSDO_Samples)) * IGN * Rotate2D( POISSON_SAMPLES[i], IGN ) / D0; // tossed out reflect(coord,random) Because Sampled Mips didn't work with the code above.... May need Yakube to fix this.
			   RayDir *= sign(dot(normalize(float3(RayDir.x,-RayDir.y,1.0)),Normals_Depth.xyz)); // flip directions

		float2 RayCoords = texcoords + RayDir.xy;
		int Adaptive_Mipping = SSDO_MipSampling <= 2 ? SSDO_MipSampling : lerp(SSDO_MipSampling == 5 ? 3 : 2, Mip_Switch ? 1 : 0, Normals_Depth.w);
		float4 vsFetch = NDSampler(RayCoords,Adaptive_Mipping) ;

		float3 LocalGI = tex2Dlod(SamplerColorsSSDO,float4(RayCoords,0,3)).xyz ;
			   LocalGI *= SSDO_ColorPower;
			   
		// Compute the bounce geometric shape towards the direction of the blocker. Aka Position
		float3 Pos = GetPosition(  texcoords, RayCoords, Normals_Depth.w - ATTF, vsFetch.w + ATTF);
		float3 normalizedPos = normalize(Pos);

		float  AO  = smoothstep(0,SSDO_AngleThreshold,dot(normalizedPos,Normals_Depth.xyz));
		//float  AO  = max(0.0,dot(normalizedPos,Normals_Depth.xyz));
			   AO *= sign( length(Normals_Depth.xyz-vsFetch.xyz) );
		// Listed as attenuation......
		float SSDO_RangeCheck = max(0.0,SSDO_Contribution_Range-length(Pos))/SSDO_Contribution_Range;
		SSDO += lerp(saturate(1-LocalGI) * AO * SSDO_RangeCheck * SSDO_RangeCheck,0,D_Fade);
	}

	SSDO /= SSDO_Samples;
	return float4(saturate(1.0-SSDO.rgb),1);
}

float NormalMask(float2 texcoords,float Mip)
{
	float Mask = distance(NDSampler( texcoords, 0),NDSampler( texcoords, Mip));
	return Mask > 0.005;
}

float3 SSDO_MipBLur(sampler Tex, float2 texcoords,float Mip)
{
	return tex2Dlod(Tex, float4(texcoords, 0, Mip)).rgb;
}

static const float EvenSteven[21] = { 0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20 , 22, 24, 26, 28, 30, 32, 34, 36, 38, 40}; // It's not odd...

float gaussian(float x, float sigma)
{
    return (rsqrt( PI * pow(sigma,2))) * exp(-(pow(x,2) / (2.0 * pow(sigma,2))));
}   float  Helper() { float Temp_Location = T_01() == 12500 ? 0 : 1 ; return Temp_Location;}
//Custom Edge Avoiding Gaussian Denoiser
float4 Denoise(sampler Tex, float2 texcoords, int SXY, int Dir , float R )
{
	float4 StoredNormals_Depth = NDSampler( texcoords, 0);//Fix 2nd option by using the 2D texture Mask form the Normals from 2D.
	float4 center = tex2Dlod(Tex, float4(texcoords ,0, 0)), color = 0.0;//Like why do SmoothNormals when 2nd Level Denoiser is like Got you B#*@!........
	float total = 0.0, NormalBlurFactor = Debug == 1 ? 0.125f : 0.5f, DepthBlurFactor = 0.0125f,  DM = smoothstep(0,1,StoredNormals_Depth.w) > 0.999;

	if(SXY > 0) // Who would win Raw Boi or Gaussian Boi
	{   [fastopt]
	    for (int i = -SXY * 0.5; i <= SXY * 0.5; ++i)
		{
        	float2 D = Dir ? float2( i, 0) : float2( 0, i), TC = texcoords + D * R * pix;

			float4 ModifiedNormals_Depth = NDSampler( TC, Debug == 1 ? 1.0f : 0.0f);//Use lower mip level here on finnished product.
			float ModN = length(StoredNormals_Depth.xyz - ModifiedNormals_Depth.xyz), ModD = saturate( StoredNormals_Depth.w - ModifiedNormals_Depth.w);

			float D_Dist2 = max(ModD, 0.0), d_w = min(exp(-(D_Dist2)/DepthBlurFactor), 1.0)-0.0000000001f;//Magic number.....
	        float N_Dist2 = max(ModN, 0.0), n_w = min(exp(-(N_Dist2)/NormalBlurFactor), 1.0);

			//if(ModN < 0.25 && ModD < 0.01)
			float Weight = gaussian(i, sqrt(SXY));//Looks better
			Weight *= d_w;
			Weight *= n_w;
			color += tex2Dlod(Tex, float4(TC ,0, 0.5)) * Weight;
        	total += Weight;

    	}
	}
	return SamplesXY > 0 ? lerp(color / total,center,DM) : center;
}

float4 CEAGD_H_SSDO(float4 vpos : SV_Position, float2 texcoords : TEXCOORD) : SV_Target
{
	return pow(abs(Denoise(SamplerSSDO,texcoords, EvenSteven[clamp(SamplesXY,0,20)], 0, 2.5 )),SSDO_Power);
}

float4 CEAGD_V_SSDO(float4 vpos : SV_Position, float2 texcoords : TEXCOORD) : SV_Target
{
	return Denoise(SamplerSSDOH, texcoords, EvenSteven[clamp(SamplesXY,0,20)], 1, 2.5 );
}

float4 TAA_SSDO(float2 texcoords,float Mip)
{
	return tex2Dlod(SamplerSSDOV, float4(texcoords, 0, Mip)).rgba;
}

float4 TAA_SSDO(float4 vpos : SV_Position, float2 texcoords : TEXCOORD) : SV_Target
{
	float Per = 1-Persistence;
    float4 PastColor = tex2Dlod(SSDOaccuFrames,float4(texcoords,0,0) );//Past Back Buffer
		   PastColor = (1-Per) * TAA_SSDO(texcoords, 0) + Per * PastColor;

    float3 antialiased = PastColor.xyz;
    float mixRate = min(PastColor.w, 0.5), MB = 0.0;//WIP

    float3 BB = TAA_SSDO(texcoords, 0).rgb;

    antialiased = lerp(antialiased * antialiased, BB * BB, mixRate);
    antialiased = sqrt(antialiased);

	float3 minColor = encodePalYuv( TAA_SSDO(texcoords, 0).rgb ) - MB;
	float3 maxColor = encodePalYuv( TAA_SSDO(texcoords, 0).rgb ) + MB;
	for(int i = 1; i < 8; ++i)
	{   //DX9 work around.
		minColor = min(minColor,encodePalYuv( TAA_SSDO( texcoords + XYoffset[i], 0).rgb )) - MB;
		maxColor = max(maxColor,encodePalYuv( TAA_SSDO( texcoords + XYoffset[i], 0).rgb )) + MB;
	}
   	antialiased = encodePalYuv(antialiased);

   float3 preclamping = antialiased;
    antialiased = clamp(antialiased, minColor, maxColor);

    mixRate = rcp(1.0 / mixRate + 1.0);

    float3 diff = antialiased - preclamping;

    diff.x = dot(diff,diff);

    float clampAmount = diff.x;

    mixRate += clampAmount ;
    mixRate = clamp(mixRate, 0.05, 0.5);

    antialiased = decodePalYuv(antialiased);

    return float4(antialiased,mixRate);
}

void AccumulatedFramesSSDO(float4 vpos : SV_Position, float2 texcoords : TEXCOORD, out float4 acc : SV_Target0)
{
	acc = tex2D(BackBufferSSDO,texcoords).rgba;
}   float Text_Info_Plus() { return nonplus() ? Alternate : Text_Info; }

//Color Mixing
float3 overlay(float3 c, float3 b) 		{ return c<0.5f ? 2.0f*c*b:(1.0f-2.0f*(1.0f-c)*(1.0f-b));}
float3 softlight(float3 c, float3 b) 	{ return b<0.5f ? (2.0f*c*b+c*c*(1.0f-2.0f*b)):(sqrt(c)*(2.0f*b-1.0f)+2.0f*c*(1.0f-b));}
float3 mult(float3 c, float3 b) 	{ return c * b;}

float3 Composite(float3 Color, float3 Cloud)
{
	float3 Output, FiftyGray = Cloud - 0.5;

	if(Blend_Mode == 0)
		Output = lerp((overlay( Color,  FiftyGray) + softlight( Color,  FiftyGray)) * 0.5 , mult(  Color, Cloud ), 0.5);
	else if(Blend_Mode == 1)
		Output = overlay( Color, FiftyGray);
	else if(Blend_Mode == 2)
		Output = softlight( Color, FiftyGray);
	else
		Output = mult( Color ,  Cloud );

	return Output;
}

float4 SSDOMixing(float2 texcoords )
{
	float Depth = NDSampler( texcoords,0).w, Guide = Depth_Guide ? ( Depth +
													NDSampler(texcoords + float2( pix.x * 2,         0), 1).w +
													NDSampler(texcoords + float2(-pix.x * 2,         0), 2).w +
													NDSampler(texcoords + float2( 0        , pix.y * 2), 3).w +
													NDSampler(texcoords + float2( 0        ,-pix.y * 2), 4).w   ) * 0.2f : 0.00000001f;
	//Mip Denoiser
	float3 ssdo = SSDO_MipBLur( SSDOaccuFrames, texcoords,0.0);//lerp(SSDO_MipBLur( SSDOaccuFrames, texcoords,2),SSDO_MipBLur( SSDOaccuFrames, texcoords,0),NormalMask(texcoords,1));
	float Intencity_Mask = tex2Dlod(SamplerColorsSSDO,float4(texcoords,0,2.5)).w;
	float ILuma = Luma(tex2Dlod(SamplerColorsSSDO,float4(texcoords,0,4.5)).xyz) * 2.0;
		
	if(smoothstep(0,1,Depth) > SSDO_Max_Depth)
		ssdo = 1;
	float3 Noise = float3(MCNoise( texcoords , 1, 1 ), MCNoise( texcoords , 1, 2 ), MCNoise( texcoords , 1, 3 ));
	float3 SS  = smoothstep( 0.0, 0.1, Saturator(ssdo).rgb );
		   SS *= 0.0225;
	
	if(SSDO_X2 == 1)
	   ssdo *= ssdo;
	if(SSDO_X2 == 2)
	   ssdo *= lerp(1,ssdo,ILuma);
	if(SSDO_X2 == 3)
	   ssdo *= lerp(1,ssdo,1-ILuma);	   
		   
	if(Dither_SSDO)
		ssdo = saturate(Saturator(ssdo)+ Noise * SS);
	else
		ssdo = saturate(Saturator(ssdo));

	float3 Layer = lerp(Composite(tex2D(SamplerColorsSSDO,texcoords).xyz,ssdo),tex2D(SamplerColorsSSDO,texcoords).xyz,1-Intencity_Mask) ;

	if (Debug == 1)
		return float4(lerp(ssdo, tex2D(SamplerColorsSSDO,texcoords).xyz,1-Intencity_Mask) ,1.0) ;
	else if (Debug == 2)
		return float4(texcoords.x + texcoords.y < 1 ? NDSampler( texcoords, 0).w : NDSampler( texcoords, 0).xyz * 0.5 + 0.5,1.0);
	else
  	  return float4(Depth_Guide ? Layer.rgb * float3((Depth/Guide> 0.998),1,(Depth/Guide > 0.998))  : Layer.rgb,1.0);
}
////////////////////////////////////////////////////////////////Overwatch////////////////////////////////////////////////////////////////////////////
float Text_Switch() { return RH || NC || NP || NF || PE || DS || OS || DA || NW || FV ? 0 : 1; }
static const float  CH_A    = float(0x69f99), CH_B    = float(0x79797), CH_C    = float(0xe111e),
					CH_D    = float(0x79997), CH_E    = float(0xf171f), CH_F    = float(0xf1711),
					CH_G    = float(0xe1d96), CH_H    = float(0x99f99), CH_I    = float(0xf444f),
					CH_J    = float(0x88996), CH_K    = float(0x95159), CH_L    = float(0x1111f),
					CH_M    = float(0x9fd99), CH_N    = float(0x9bd99), CH_O    = float(0x69996),
					CH_P    = float(0x79971), CH_Q    = float(0x69b5a), CH_R    = float(0x79759),
					CH_S    = float(0xe1687), CH_T    = float(0xf4444), CH_U    = float(0x99996),
					CH_V    = float(0x999a4), CH_W    = float(0x999f9), CH_X    = float(0x99699),
					CH_Y    = float(0x99e8e), CH_Z    = float(0xf843f), CH_0    = float(0x6bd96),
					CH_1    = float(0x46444), CH_2    = float(0x6942f), CH_3    = float(0x69496),
					CH_4    = float(0x99f88), CH_5    = float(0xf1687), CH_6    = float(0x61796),
					CH_7    = float(0xf8421), CH_8    = float(0x69696), CH_9    = float(0x69e84),
					CH_APST = float(0x66400), CH_PI   = float(0x0faa9), CH_UNDS = float(0x0000f),
					CH_HYPH = float(0x00600), CH_TILD = float(0x0a500), CH_PLUS = float(0x02720),
					CH_EQUL = float(0x0f0f0), CH_SLSH = float(0x08421), CH_EXCL = float(0x33303),
					CH_QUES = float(0x69404), CH_COMM = float(0x00032), CH_FSTP = float(0x00002),
					CH_QUOT = float(0x55000), CH_BLNK = float(0x00000), CH_COLN = float(0x00202),
					CH_LPAR = float(0x42224), CH_RPAR = float(0x24442);
#define MAP_SIZE float2(4,5)
//returns the status of a bit in a bitmap. This is done value-wise, so the exact representation of the float doesn't really matter.
float getBit( float map, float index )
{   // Ooh -index takes out that divide :)
    return fmod( floor( map * exp2(-index) ), 2.0 );
}   float DT_Information(){ float DT_Text = Text_Switch() ? T_02() : T_01(); return clock <= DT_Text; }

float drawChar( float Char, float2 pos, float2 size, float2 TC )
{   // Subtract our position from the current TC so that we can know if we're inside the bounding box or not.
    TC -= pos;
    // Divide the screen space by the size, so our bounding box is 1x1.
    TC /= size;
    // Create a place to store the result & Branchless bounding box check.
    float res = step(0.0,min( TC.x, TC.y)) - step(1.0,max( TC.x, TC.y));
    // Go ahead and multiply the TC by the bitmap size so we can work in bitmap space coordinates.
    TC *= MAP_SIZE;
    // Get the appropriate bit and return it.
    res*=getBit( Char, 4.0 * floor( TC.y) + floor( TC.x) );
    return saturate( res);
}

float4 Out(float4 position : SV_Position, float2 texcoord : TEXCOORD) : SV_Target
{
	float2 TC = float2(texcoord.x,1-texcoord.y);
	float Gradient = (1-texcoord.y*50.0+48.85)*texcoord.y-0.500, BT = smoothstep(0,1,sin(clock*(3.75/1000))), Size = 1.1, Depth3D, Read_Help, Supported, SetFoV, FoV, Post, Effect, NoPro, NotCom, Mod, Needs, Net, Over, Set, AA, Emu, Not, No, Help, Fix, Need, State, SetAA, SetWP, Work;
	float4 Color = SSDOMixing(texcoord);

	[branch] if(DT_Information() || Text_Info_Plus())
	{ // Set a general character size...
		float2 charSize = float2(.00875, .0125) * Size;
		// Starting position.
		float2 charPos = float2( 0.009, 0.9725);
		//Needs Copy Depth and/or Depth Selection
		Needs += drawChar( CH_N, charPos, charSize, TC); charPos.x += .01 * Size;
		Needs += drawChar( CH_E, charPos, charSize, TC); charPos.x += .01 * Size;
		Needs += drawChar( CH_E, charPos, charSize, TC); charPos.x += .01 * Size;
		Needs += drawChar( CH_D, charPos, charSize, TC); charPos.x += .01 * Size;
		Needs += drawChar( CH_S, charPos, charSize, TC); charPos.x += .01 * Size;
		Needs += drawChar( CH_BLNK, charPos, charSize, TC); charPos.x += .01 * Size;
		Needs += drawChar( CH_C, charPos, charSize, TC); charPos.x += .01 * Size;
		Needs += drawChar( CH_O, charPos, charSize, TC); charPos.x += .01 * Size;
		Needs += drawChar( CH_P, charPos, charSize, TC); charPos.x += .01 * Size;
		Needs += drawChar( CH_Y, charPos, charSize, TC); charPos.x += .01 * Size;
		Needs += drawChar( CH_BLNK, charPos, charSize, TC); charPos.x += .01 * Size;
		Needs += drawChar( CH_D, charPos, charSize, TC); charPos.x += .01 * Size;
		Needs += drawChar( CH_E, charPos, charSize, TC); charPos.x += .01 * Size;
		Needs += drawChar( CH_P, charPos, charSize, TC); charPos.x += .01 * Size;
		Needs += drawChar( CH_T, charPos, charSize, TC); charPos.x += .01 * Size;
		Needs += drawChar( CH_H, charPos, charSize, TC); charPos.x += .01 * Size;
		Needs += drawChar( CH_BLNK, charPos, charSize, TC); charPos.x += .01 * Size;
		Needs += drawChar( CH_A, charPos, charSize, TC); charPos.x += .01 * Size;
		Needs += drawChar( CH_N, charPos, charSize, TC); charPos.x += .01 * Size;
		Needs += drawChar( CH_D, charPos, charSize, TC); charPos.x += .01 * Size;
		Needs += drawChar( CH_SLSH, charPos, charSize, TC); charPos.x += .01 * Size;
		Needs += drawChar( CH_O, charPos, charSize, TC); charPos.x += .01 * Size;
		Needs += drawChar( CH_R, charPos, charSize, TC); charPos.x += .01 * Size;
		Needs += drawChar( CH_BLNK, charPos, charSize, TC); charPos.x += .01 * Size;
		Needs += drawChar( CH_D, charPos, charSize, TC); charPos.x += .01 * Size;
		Needs += drawChar( CH_E, charPos, charSize, TC); charPos.x += .01 * Size;
		Needs += drawChar( CH_P, charPos, charSize, TC); charPos.x += .01 * Size;
		Needs += drawChar( CH_T, charPos, charSize, TC); charPos.x += .01 * Size;
		Needs += drawChar( CH_H, charPos, charSize, TC); charPos.x += .01 * Size;
		Needs += drawChar( CH_BLNK, charPos, charSize, TC); charPos.x += .01 * Size;
		Needs += drawChar( CH_S, charPos, charSize, TC); charPos.x += .01 * Size;
		Needs += drawChar( CH_E, charPos, charSize, TC); charPos.x += .01 * Size;
		Needs += drawChar( CH_L, charPos, charSize, TC); charPos.x += .01 * Size;
		Needs += drawChar( CH_E, charPos, charSize, TC); charPos.x += .01 * Size;
		Needs += drawChar( CH_C, charPos, charSize, TC); charPos.x += .01 * Size;
		Needs += drawChar( CH_T, charPos, charSize, TC); charPos.x += .01 * Size;
		Needs += drawChar( CH_I, charPos, charSize, TC); charPos.x += .01 * Size;
		Needs += drawChar( CH_O, charPos, charSize, TC); charPos.x += .01 * Size;
		Needs += drawChar( CH_N, charPos, charSize, TC);
		//Network Play May Need Modded DLL
		charPos = float2( 0.009, 0.955);
		Work += drawChar( CH_N, charPos, charSize, TC); charPos.x += .01 * Size;
		Work += drawChar( CH_E, charPos, charSize, TC); charPos.x += .01 * Size;
		Work += drawChar( CH_T, charPos, charSize, TC); charPos.x += .01 * Size;
		Work += drawChar( CH_W, charPos, charSize, TC); charPos.x += .01 * Size;
		Work += drawChar( CH_O, charPos, charSize, TC); charPos.x += .01 * Size;
		Work += drawChar( CH_R, charPos, charSize, TC); charPos.x += .01 * Size;
		Work += drawChar( CH_K, charPos, charSize, TC); charPos.x += .01 * Size;
		Work += drawChar( CH_BLNK, charPos, charSize, TC); charPos.x += .01 * Size;
		Work += drawChar( CH_P, charPos, charSize, TC); charPos.x += .01 * Size;
		Work += drawChar( CH_L, charPos, charSize, TC); charPos.x += .01 * Size;
		Work += drawChar( CH_A, charPos, charSize, TC); charPos.x += .01 * Size;
		Work += drawChar( CH_Y, charPos, charSize, TC); charPos.x += .01 * Size;
		Work += drawChar( CH_BLNK, charPos, charSize, TC); charPos.x += .01 * Size;
		Work += drawChar( CH_M, charPos, charSize, TC); charPos.x += .01 * Size;
		Work += drawChar( CH_A, charPos, charSize, TC); charPos.x += .01 * Size;
		Work += drawChar( CH_Y, charPos, charSize, TC); charPos.x += .01 * Size;
		Work += drawChar( CH_BLNK, charPos, charSize, TC); charPos.x += .01 * Size;
		Work += drawChar( CH_N, charPos, charSize, TC); charPos.x += .01 * Size;
		Work += drawChar( CH_E, charPos, charSize, TC); charPos.x += .01 * Size;
		Work += drawChar( CH_E, charPos, charSize, TC); charPos.x += .01 * Size;
		Work += drawChar( CH_D, charPos, charSize, TC); charPos.x += .01 * Size;
		Work += drawChar( CH_BLNK, charPos, charSize, TC); charPos.x += .01 * Size;
		Work += drawChar( CH_M, charPos, charSize, TC); charPos.x += .01 * Size;
		Work += drawChar( CH_O, charPos, charSize, TC); charPos.x += .01 * Size;
		Work += drawChar( CH_D, charPos, charSize, TC); charPos.x += .01 * Size;
		Work += drawChar( CH_D, charPos, charSize, TC); charPos.x += .01 * Size;
		Work += drawChar( CH_E, charPos, charSize, TC); charPos.x += .01 * Size;
		Work += drawChar( CH_D, charPos, charSize, TC); charPos.x += .01 * Size;
		Work += drawChar( CH_BLNK, charPos, charSize, TC); charPos.x += .01 * Size;
		Work += drawChar( CH_D, charPos, charSize, TC); charPos.x += .01 * Size;
		Work += drawChar( CH_L, charPos, charSize, TC); charPos.x += .01 * Size;
		Work += drawChar( CH_L, charPos, charSize, TC);
		//Disable CA/MB/Dof/Grain
		charPos = float2( 0.009, 0.9375);
		Effect += drawChar( CH_D, charPos, charSize, TC); charPos.x += .01 * Size;
		Effect += drawChar( CH_I, charPos, charSize, TC); charPos.x += .01 * Size;
		Effect += drawChar( CH_S, charPos, charSize, TC); charPos.x += .01 * Size;
		Effect += drawChar( CH_A, charPos, charSize, TC); charPos.x += .01 * Size;
		Effect += drawChar( CH_B, charPos, charSize, TC); charPos.x += .01 * Size;
		Effect += drawChar( CH_L, charPos, charSize, TC); charPos.x += .01 * Size;
		Effect += drawChar( CH_E, charPos, charSize, TC); charPos.x += .01 * Size;
		Effect += drawChar( CH_BLNK, charPos, charSize, TC); charPos.x += .01 * Size;
		Effect += drawChar( CH_C, charPos, charSize, TC); charPos.x += .01 * Size;
		Effect += drawChar( CH_A, charPos, charSize, TC); charPos.x += .01 * Size;
		Effect += drawChar( CH_SLSH, charPos, charSize, TC); charPos.x += .01 * Size;
		Effect += drawChar( CH_M, charPos, charSize, TC); charPos.x += .01 * Size;
		Effect += drawChar( CH_B, charPos, charSize, TC); charPos.x += .01 * Size;
		Effect += drawChar( CH_SLSH, charPos, charSize, TC); charPos.x += .01 * Size;
		Effect += drawChar( CH_D, charPos, charSize, TC); charPos.x += .01 * Size;
		Effect += drawChar( CH_O, charPos, charSize, TC); charPos.x += .01 * Size;
		Effect += drawChar( CH_F, charPos, charSize, TC); charPos.x += .01 * Size;
		Effect += drawChar( CH_SLSH, charPos, charSize, TC); charPos.x += .01 * Size;
		Effect += drawChar( CH_G, charPos, charSize, TC); charPos.x += .01 * Size;
		Effect += drawChar( CH_R, charPos, charSize, TC); charPos.x += .01 * Size;
		Effect += drawChar( CH_A, charPos, charSize, TC); charPos.x += .01 * Size;
		Effect += drawChar( CH_I, charPos, charSize, TC); charPos.x += .01 * Size;
		Effect += drawChar( CH_N, charPos, charSize, TC);
		//Disable TAA/MSAA/AA
		charPos = float2( 0.009, 0.920);
		SetAA += drawChar( CH_D, charPos, charSize, TC); charPos.x += .01 * Size;
		SetAA += drawChar( CH_I, charPos, charSize, TC); charPos.x += .01 * Size;
		SetAA += drawChar( CH_S, charPos, charSize, TC); charPos.x += .01 * Size;
		SetAA += drawChar( CH_A, charPos, charSize, TC); charPos.x += .01 * Size;
		SetAA += drawChar( CH_B, charPos, charSize, TC); charPos.x += .01 * Size;
		SetAA += drawChar( CH_L, charPos, charSize, TC); charPos.x += .01 * Size;
		SetAA += drawChar( CH_E, charPos, charSize, TC); charPos.x += .01 * Size;
		SetAA += drawChar( CH_BLNK, charPos, charSize, TC); charPos.x += .01 * Size;
		SetAA += drawChar( CH_T, charPos, charSize, TC); charPos.x += .01 * Size;
		SetAA += drawChar( CH_A, charPos, charSize, TC); charPos.x += .01 * Size;
		SetAA += drawChar( CH_A, charPos, charSize, TC); charPos.x += .01 * Size;
		SetAA += drawChar( CH_SLSH, charPos, charSize, TC); charPos.x += .01 * Size;
		SetAA += drawChar( CH_M, charPos, charSize, TC); charPos.x += .01 * Size;
		SetAA += drawChar( CH_S, charPos, charSize, TC); charPos.x += .01 * Size;
		SetAA += drawChar( CH_A, charPos, charSize, TC); charPos.x += .01 * Size;
		SetAA += drawChar( CH_A, charPos, charSize, TC); charPos.x += .01 * Size;
		SetAA += drawChar( CH_SLSH, charPos, charSize, TC); charPos.x += .01 * Size;
		SetAA += drawChar( CH_A, charPos, charSize, TC); charPos.x += .01 * Size;
		SetAA += drawChar( CH_A, charPos, charSize, TC);
		//Set FoV
		charPos = float2( 0.009, 0.9025);
		SetFoV += drawChar( CH_S, charPos, charSize, TC); charPos.x += .01 * Size;
		SetFoV += drawChar( CH_E, charPos, charSize, TC); charPos.x += .01 * Size;
		SetFoV += drawChar( CH_T, charPos, charSize, TC); charPos.x += .01 * Size;
		SetFoV += drawChar( CH_BLNK, charPos, charSize, TC); charPos.x += .01 * Size;
		SetFoV += drawChar( CH_F, charPos, charSize, TC); charPos.x += .01 * Size;
		SetFoV += drawChar( CH_O, charPos, charSize, TC); charPos.x += .01 * Size;
		SetFoV += drawChar( CH_V, charPos, charSize, TC);
		//Read Help
		charPos = float2( 0.894, 0.9725);
		Read_Help += drawChar( CH_R, charPos, charSize, TC); charPos.x += .01 * Size;
		Read_Help += drawChar( CH_E, charPos, charSize, TC); charPos.x += .01 * Size;
		Read_Help += drawChar( CH_A, charPos, charSize, TC); charPos.x += .01 * Size;
		Read_Help += drawChar( CH_D, charPos, charSize, TC); charPos.x += .01 * Size;
		Read_Help += drawChar( CH_BLNK, charPos, charSize, TC); charPos.x += .01 * Size;
		Read_Help += drawChar( CH_H, charPos, charSize, TC); charPos.x += .01 * Size;
		Read_Help += drawChar( CH_E, charPos, charSize, TC); charPos.x += .01 * Size;
		Read_Help += drawChar( CH_L, charPos, charSize, TC); charPos.x += .01 * Size;
		Read_Help += drawChar( CH_P, charPos, charSize, TC);
		//New Start
		charPos = float2( 0.009, 0.018);
		// No Profile
		NoPro += drawChar( CH_N, charPos, charSize, TC); charPos.x += .01 * Size;
		NoPro += drawChar( CH_O, charPos, charSize, TC); charPos.x += .01 * Size;
		NoPro += drawChar( CH_BLNK, charPos, charSize, TC); charPos.x += .01 * Size;
		NoPro += drawChar( CH_P, charPos, charSize, TC); charPos.x += .01 * Size;
		NoPro += drawChar( CH_R, charPos, charSize, TC); charPos.x += .01 * Size;
		NoPro += drawChar( CH_O, charPos, charSize, TC); charPos.x += .01 * Size;
		NoPro += drawChar( CH_F, charPos, charSize, TC); charPos.x += .01 * Size;
		NoPro += drawChar( CH_I, charPos, charSize, TC); charPos.x += .01 * Size;
		NoPro += drawChar( CH_L, charPos, charSize, TC); charPos.x += .01 * Size;
		NoPro += drawChar( CH_E, charPos, charSize, TC); charPos.x = 0.009;
		//Not Compatible
		NotCom += drawChar( CH_N, charPos, charSize, TC); charPos.x += .01 * Size;
		NotCom += drawChar( CH_O, charPos, charSize, TC); charPos.x += .01 * Size;
		NotCom += drawChar( CH_T, charPos, charSize, TC); charPos.x += .01 * Size;
		NotCom += drawChar( CH_BLNK, charPos, charSize, TC); charPos.x += .01 * Size;
		NotCom += drawChar( CH_C, charPos, charSize, TC); charPos.x += .01 * Size;
		NotCom += drawChar( CH_O, charPos, charSize, TC); charPos.x += .01 * Size;
		NotCom += drawChar( CH_P, charPos, charSize, TC); charPos.x += .01 * Size;
		NotCom += drawChar( CH_A, charPos, charSize, TC); charPos.x += .01 * Size;
		NotCom += drawChar( CH_T, charPos, charSize, TC); charPos.x += .01 * Size;
		NotCom += drawChar( CH_I, charPos, charSize, TC); charPos.x += .01 * Size;
		NotCom += drawChar( CH_B, charPos, charSize, TC); charPos.x += .01 * Size;
		NotCom += drawChar( CH_L, charPos, charSize, TC); charPos.x += .01 * Size;
		NotCom += drawChar( CH_E, charPos, charSize, TC); charPos.x = 0.009;
		//Needs Fix/Mod
		Mod += drawChar( CH_N, charPos, charSize, TC); charPos.x += .01 * Size;
		Mod += drawChar( CH_E, charPos, charSize, TC); charPos.x += .01 * Size;
		Mod += drawChar( CH_E, charPos, charSize, TC); charPos.x += .01 * Size;
		Mod += drawChar( CH_D, charPos, charSize, TC); charPos.x += .01 * Size;
		Mod += drawChar( CH_S, charPos, charSize, TC); charPos.x += .01 * Size;
		Mod += drawChar( CH_BLNK, charPos, charSize, TC); charPos.x += .01 * Size;
		Mod += drawChar( CH_F, charPos, charSize, TC); charPos.x += .01 * Size;
		Mod += drawChar( CH_I, charPos, charSize, TC); charPos.x += .01 * Size;
		Mod += drawChar( CH_X, charPos, charSize, TC); charPos.x += .01 * Size;
		Mod += drawChar( CH_SLSH, charPos, charSize, TC); charPos.x += .01 * Size;
		Mod += drawChar( CH_M, charPos, charSize, TC); charPos.x += .01 * Size;
		Mod += drawChar( CH_O, charPos, charSize, TC); charPos.x += .01 * Size;
		Mod += drawChar( CH_D, charPos, charSize, TC); charPos.x = 0.009;
		//Overwatch.fxh Missing
		State += drawChar( CH_O, charPos, charSize, TC); charPos.x += .01 * Size;
		State += drawChar( CH_V, charPos, charSize, TC); charPos.x += .01 * Size;
		State += drawChar( CH_E, charPos, charSize, TC); charPos.x += .01 * Size;
		State += drawChar( CH_R, charPos, charSize, TC); charPos.x += .01 * Size;
		State += drawChar( CH_W, charPos, charSize, TC); charPos.x += .01 * Size;
		State += drawChar( CH_A, charPos, charSize, TC); charPos.x += .01 * Size;
		State += drawChar( CH_T, charPos, charSize, TC); charPos.x += .01 * Size;
		State += drawChar( CH_C, charPos, charSize, TC); charPos.x += .01 * Size;
		State += drawChar( CH_H, charPos, charSize, TC); charPos.x += .01 * Size;
		State += drawChar( CH_FSTP, charPos, charSize, TC); charPos.x += .01 * Size;
		State += drawChar( CH_F, charPos, charSize, TC); charPos.x += .01 * Size;
		State += drawChar( CH_X, charPos, charSize, TC); charPos.x += .01 * Size;
		State += drawChar( CH_H, charPos, charSize, TC); charPos.x += .01 * Size;
		State += drawChar( CH_BLNK, charPos, charSize, TC); charPos.x += .01 * Size;
		State += drawChar( CH_M, charPos, charSize, TC); charPos.x += .01 * Size;
		State += drawChar( CH_I, charPos, charSize, TC); charPos.x += .01 * Size;
		State += drawChar( CH_S, charPos, charSize, TC); charPos.x += .01 * Size;
		State += drawChar( CH_S, charPos, charSize, TC); charPos.x += .01 * Size;
		State += drawChar( CH_I, charPos, charSize, TC); charPos.x += .01 * Size;
		State += drawChar( CH_N, charPos, charSize, TC); charPos.x += .01 * Size;
		State += drawChar( CH_G, charPos, charSize, TC);
		//New Size
		float D3D_Size_A = 1.375,D3D_Size_B = 0.75;
		float2 charSize_A = float2(.00875, .0125) * D3D_Size_A, charSize_B = float2(.00875, .0125) * D3D_Size_B;
		//New Start Pos
		charPos = float2( 0.862, 0.018);
		//Depth3D.Info Logo/Website
		Depth3D += drawChar( CH_D, charPos, charSize_A, TC); charPos.x += .01 * D3D_Size_A;
		Depth3D += drawChar( CH_E, charPos, charSize_A, TC); charPos.x += .01 * D3D_Size_A;
		Depth3D += drawChar( CH_P, charPos, charSize_A, TC); charPos.x += .01 * D3D_Size_A;
		Depth3D += drawChar( CH_T, charPos, charSize_A, TC); charPos.x += .01 * D3D_Size_A;
		Depth3D += drawChar( CH_H, charPos, charSize_A, TC); charPos.x += .01 * D3D_Size_A;
		Depth3D += drawChar( CH_3, charPos, charSize_A, TC); charPos.x += .01 * D3D_Size_A;
		Depth3D += drawChar( CH_D, charPos, charSize_A, TC); charPos.x += 0.008 * D3D_Size_A;
		Depth3D += drawChar( CH_FSTP, charPos, charSize_A, TC); charPos.x += 0.01 * D3D_Size_A;
		charPos = float2( 0.963, 0.018);
		Depth3D += drawChar( CH_I, charPos, charSize_B, TC); charPos.x += .01 * D3D_Size_B;
		Depth3D += drawChar( CH_N, charPos, charSize_B, TC); charPos.x += .01 * D3D_Size_B;
		Depth3D += drawChar( CH_F, charPos, charSize_B, TC); charPos.x += .01 * D3D_Size_B;
		Depth3D += drawChar( CH_O, charPos, charSize_B, TC);
		//Text Information
		if(DS)
			Need = Needs;
		if(RH)
			Help = Read_Help;
		if(NW)
			Net = Work;
		if(PE)
			Post = Effect;
		if(DA)
			AA = SetAA;
		if(FV)
			FoV = SetFoV;
		//Blinking Text Warnings
		if(NP)
			No = NoPro * BT;
		if(NC)
			Not = NotCom * BT;
		if(NF)
			Fix = Mod * BT;
		if(OS)
			Over = State * BT;
		//Website
		return Depth3D+(Disable_Debug_Info ? 0 : Help+Post+No+Not+Net+Fix+Need+Over+AA+Set+FoV+Emu) ? Minimize_Web_Info ? lerp(Gradient + Depth3D,Color,saturate(Depth3D*0.98)) : Gradient : Color;
	}
	else
		return Helper() ? 0 : Color;
}

//////////////////////////////////////////////////////////Reshade.fxh/////////////////////////////////////////////////////////////
// Vertex shader generating a triangle covering the entire screen
void PostProcessVS(in uint id : SV_VertexID, out float4 position : SV_Position, out float2 texcoord : TEXCOORD)
{
	texcoord.x = (id == 2) ? 2.0 : 0.0;
	texcoord.y = (id == 1) ? 2.0 : 0.0;
	position = float4(texcoord * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
}

technique SSDO_Plus
< ui_label = "GloomAO";
  ui_tooltip = "GloomAO: Screen Space Directional Occlusion"; >
{
	#if Force_Texture_Details
		pass SSDO_Colors
	{
		VertexShader = PostProcessVS;
		PixelShader = SSDOColors;
		RenderTarget = texColorsSSDO;
	}
		pass SSDO_Normals_Depth
	{
		VertexShader = PostProcessVS;
		PixelShader = NormalsDepth;
		RenderTarget0 = texNormalsSSDO;
		RenderTarget1 = texDepthSSDO;
	}
	#else
		pass SSDO_Normals_Depth_Colors
	{
		VertexShader = PostProcessVS;
		PixelShader = NormalsColorsDepth;
		RenderTarget0 = texNormalsSSDO;
		RenderTarget1 = texColorsSSDO;
		RenderTarget2 = texDepthSSDO;
	}
	#endif
		pass SSDO
	{
		VertexShader = PostProcessVS;
		PixelShader = SSDO;
		RenderTarget = texSSDO;
	}
		pass SSDO_EADFDDS
	{
		VertexShader = PostProcessVS;
		PixelShader = CEAGD_H_SSDO;
		RenderTarget = texCEAGD_H_SSDO;
	}
		pass SSDO_EADFDUS
	{
		VertexShader = PostProcessVS;
		PixelShader = CEAGD_V_SSDO;
		RenderTarget = texCEAGD_V_SSDO;
	}
		pass TAA_SSDO
	{
		VertexShader = PostProcessVS;
		PixelShader = TAA_SSDO;
	}
		pass AccumilateFrames
	{
		VertexShader = PostProcessVS;
		PixelShader = AccumulatedFramesSSDO;
		RenderTarget0 = accuTexSSDO;
	}
		pass SSDO_Out
	{
		VertexShader = PostProcessVS;
		PixelShader = Out;
		//SRGBWriteEnable = true;
	}


}
