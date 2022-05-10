////-------------//
///**RadiantGI**///
//-------------////

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////                                               																									*//
//For Reshade 3.0+ PCGI Ver 3.0.8
//-----------------------------
//                                                                Radiant Global Illumination
//                                                                              +
//                                                                    Subsurface Scattering
// Due Diligence
// Michael Bunnell Disk Screen Space Ambient Occlusion or Disk to Disk SSAO 14.3
// https://developer.nvidia.com/gpugems/gpugems2/part-ii-shading-lighting-and-shadows/chapter-14-dynamic-ambient-occlusion-and
// - Arkano22
//   https://www.gamedev.net/topic/550699-ssao-no-halo-artifacts/
// - Martinsh
//   http://devlog-martinsh.blogspot.com/2011/10/nicer-ssao.html
// - Boulotaur2024
//   https://github.com/PeterTh/gedosato/blob/master/pack/assets/dx9/martinsh_ssao.fx
// Nayar and Oren Simple Scattering Approximations 16.2
//   https://developer.nvidia.com/gpugems/gpugems/part-iii-materials/chapter-16-real-time-approximations-subsurface-scattering
// GDC 2011 ‚Äì Approximating Translucency for a Fast, Cheap and Convincing Subsurface Scattering Look
//   https://colinbarrebrisebois.com/2011/03/07/gdc-2011-approximating-translucency-for-a-fast-cheap-and-convincing-subsurface-scattering-look/
// Computer graphics & visualization Global Illumination Effects.
// - Christian A. Wiesner
//   https://slideplayer.com/slide/3533454/
//Improved Normal Reconstruction From Depth
// - Turanszkij
//   https://wickedengine.net/2019/09/22/improved-normal-reconstruction-from-depth/
// Upsample Code
// - PETER KL "I think"
//   https://frictionalgames.blogspot.com/2014/01/tech-feature-ssao-and-temporal-blur.html#Code
// TAA Based on my own port of Epics Temporal AA
//   https://de45xmedrsdbp.cloudfront.net/Resources/files/TemporalAA_small-59732822.pdf
// Joined Bilateral Upsampling Filtering
// - Bart Wronski
//   https://bartwronski.com/2019/09/22/local-linear-models-guided-filter/
// - Johannes Kopf | Michael F. Cohen | Dani Lischinski | Matt Uyttendaele
//   https://johanneskopf.de/publications/jbu/paper/FinalPaper_0185.pdf
// Reinhard by Tom Madams
//   http://imdoingitwrong.wordpress.com/2010/08/19/why-reinhard-desaturates-my-blacks-3/
// Generate Noise is Based on this implamentation
//   https://www.shadertoy.com/view/wtsSW4
// Poisson-Disc Sampling Evenly distributed points on a rotated 2D Disk
//   https://www.jasondavies.com/poisson-disc/
// Text rendering code by Hamneggs
//   https://www.shadertoy.com/view/4dtGD2
// A slightly faster buffer-less vertex shader trick by CeeJay.dk
//   https://www.reddit.com/r/gamedev/comments/2j17wk/a_slightly_faster_bufferless_vertex_shader_trick/
// Origina LUT algorithm by Ganossa edited by | MartyMcFly | Otis_Inf | prod80 | - Base LUT texture made by Prod80 Thank you.
//   https://github.com/prod80/prod80-ReShade-Repository/blob/master/Shaders/PD80_LUT_v2.fxh
// SRGB <--> CIELAB CONVERSIONS Ported by Prod80. Also More Stuff From Prod80
//   http://www.brucelindbloom.com/index.html
// Explicit Image Detection using YCbCr Space Color Model for basic Skin Detection
// - JORGE ALBERTO MARCIAL BASILI | GUALBERTO AGUILAR TORRES | GABRIEL S√ÅNCHEZ P√âREZ3 | L. KARINA TOSCANO MEDINA4 | H√âCTOR M. P√âREZ MEANA
//   http://www.wseas.us/e-library/conferences/2011/Mexico/CEMATH/CEMATH-20.pdf
// Unlimited Blur Filter
// - Paul Dang
//   http://blog.marmakoide.org/?p=1
//   https://github.com/spite/Wagner/blob/master/fragment-shaders/box-blur-fs.glsl [MIT]
//   https://github.com/Brimson/reshaders/blob/master/shaders/cBlur.fx
//
// If I missed any please tell me.
//
// Special Thank You to CeeJay.dk & Dorinte. May the Pineapple Kringle lead you too happiness.
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
// Written by Jose Negrete AKA BlueSkyDefender <UntouchableBlueSky@gmail.com>, October 2020
// https://github.com/BlueSkyDefender/Depth3D
//
// Notes to the other developers: https://github.com/BlueSkyDefender/AstrayFX
//
// I welcome almost any help that seems to improve the code. But, The changes need to be approved by myself. So feel free to submit changes here on github.
// Things to work on are listed here. Oh if you feel your code changes too much. Just add a preprocessor to section off your code. Thank you.
//
// Better TAA if you know how to do this better change it.Frame-to-frame coherence Is fast enough in my eyes now. but, I know other devs can do better.
// Much sparser sampling is need to hide low poly issues so we need better Smooth Normals code, Bent Normal maps ect.
// Better specular reflections.......... ect.
//
// Oh if you can make a 2nd Bounce almost as fast as One Bounce....... Do it with your magic you wizard.
//
// Write your name and changes/notes below.
// __________________________________________________________________________________________________________________________________________________________________________________
// -------------------------------------------------------------------Around Line 1390---------------------------------------------------------------------------------------
// Lord of Lunacy - https://github.com/LordOfLunacy
// Reveil DeHaze Masking was inserted around GI Creation.
// __________________________________________________________________________________________________________________________________________________________________________________
// -------------------------------------------------------------------Around Line 0000---------------------------------------------------------------------------------------
// Dev Name - Repo
// Notes from Dev.
//
// Upcoming Updates.............................................no guarantee
// Need Past Edge Brightness storage.
//
// Radiant GI Update Notes are at the bottom
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

//Keep in mind you are not licenced to redistribute this shader with setting modified below. Please Read the Licence.
//This GI shader is free and shouldn't sit behind a paywall. If you paid for this shader ask for a refund right away.

//Depth Buffer Adjustments
#define DB_Size_Position 0     //[Off | On]         This is used to reposition and adjusts the size of the depth buffer.
#define BD_Correction 0        //[Off | On]         Barrel Distortion Correction for non conforming BackBuffer.

//TAA Quality Level
#define TAA_Clamping 0.2       //[0.0 - 1.0]         Use this to adjust TAA clamping.

//Other Settings
#define Controlled_Blend 0     //[Off | On]          Use this if you want control over blending GI in to the final
#define Dark_Mode 0            //[Off | On]          Instead of using a 50% gray it displays Black for the absence of information.
#define Text_Info_Key 93       //Menu Key            Text Information Key Default 93 is the Menu Key. You can use this site https://keycode.info to pick your own.
#define Disable_Debug_Info 0   //[Off | On]          Use this to disable help information that gives you hints for fixing many games with Overwatch.fxh.
#define Minimize_Web_Info 0    //[Off | On]          Use this to minimize the website logo on startup.
#define ForcePool 0            //[Off | On]          Force Pooled Textures in versions 4.9.0+ If you get a black screen turn this too off. Seems to be a ReShade Issue.
#define Force_Texture_Details 1//[Off | On]          This is used to add Texture Detail AO into PCGI output.


//RadiantGI In menu Options
#ifndef MaxDepth_CutOff
	#define MaxDepth_CutOff 0.999 //[0.1 - 1.0]     Used to cutout the sky with depth buffer masking. This lets the shader save on performance by limiting what is used in GI.
#endif

//RadiantGI In menu Options
#ifndef Simple_Mode
	#define Simple_Mode 1         //[0 - 1]         Used this to enable or disable Simple User Mode.
#endif
#define Simp_Mode Simple_Mode

//Keep in mind you are not licenced to redistribute this shader with setting modified below. Please Read the Licence.
//This GI shader is free and shouldn't sit behind a paywall. If you paid for this shader ask for a refund right away.

//--------------------------------------------------------------------------------------//
// Common Intercept Code
// (Copy and paste this code into your shader to help with interception of it)
//--------------------------------------------------------------------------------------//
#define __SUPPORTED_VRS_MAP_COMPATIBILITY__ 11

#if exists "VRS_Map.fxh"
    #include "VRS_Map.fxh"
	#ifndef VRS_MAP
		#define VRS_MAP 1
	#endif
#else
    #define VRS_MAP 0
#endif

//Define the necessary functions and constants so the code can't break when the .fxh isn't present
#if VRS_MAP == 0
static const uint VRS_RATE1D_1X = 0x0;
static const uint VRS_RATE1D_2X = 0x1;
static const uint VRS_RATE1D_4X = 0x2;
#define VRS_MAKE_SHADING_RATE(x,y) ((x << 2) | (y))

static const uint VRS_RATE_1X1 = VRS_MAKE_SHADING_RATE(VRS_RATE1D_1X, VRS_RATE1D_1X); // 0;
static const uint VRS_RATE_1X2 = VRS_MAKE_SHADING_RATE(VRS_RATE1D_1X, VRS_RATE1D_2X); // 0x1;
static const uint VRS_RATE_2X1 = VRS_MAKE_SHADING_RATE(VRS_RATE1D_2X, VRS_RATE1D_1X); // 0x4;
static const uint VRS_RATE_2X2 = VRS_MAKE_SHADING_RATE(VRS_RATE1D_2X, VRS_RATE1D_2X); // 0x5;
//Only up to 2X2 is implemented currently
static const uint VRS_RATE_2X4 = VRS_MAKE_SHADING_RATE(VRS_RATE1D_2X, VRS_RATE1D_4X); // 0x6;
static const uint VRS_RATE_4X2 = VRS_MAKE_SHADING_RATE(VRS_RATE1D_4X, VRS_RATE1D_2X); // 0x9;
static const uint VRS_RATE_4X4 = VRS_MAKE_SHADING_RATE(VRS_RATE1D_4X, VRS_RATE1D_4X); // 0xa;

namespace VRS_Map
{
	uint ShadingRate(float2 texcoord, bool UseVRS, uint offRate)
	{
		return offRate;
	}
	uint ShadingRate(float2 texcoord, float VarianceCutoff, bool UseVRS, uint offRate)
	{
		return offRate;
	}
	float3 DebugImage(float3 originalImage, float2 texcoord, float VarianceCutoff, bool DebugView)
	{
		return originalImage;
	}
}
#endif //VRS_MAP

#if exists "IsolateSkintones.png"                                //Look Up Table Interceptor//
	#define  LUT_File_Name      "IsolateSkintones.png" //Base Texture also made by Prod80
	#define  Tile_SizeXY        64
	#define  Tile_Amount        64
	#define LutSD 1
#else // Texture name which contains the LUT(s) and the Tile Sizes, Amounts, etc. by Prod80
	#define LutSD 0
#endif
static const float  MixChroma = 2.0;         //LUT Chroma 0-1
static const float  MixLuma = 1.0;           //LUT Luma 0-1
static const float  Intensity = 1.5;         //LUT Intensity 0-1
static const float3 ib = float3(0.0,0.0,0.0);//LUT Black IN Level
static const float3 iw = float3(1.0,1.0,1.0);//LUT White IN Level
static const float3 ob = float3(0.0,0.0,0.0);//LUT Black OUT Level
static const float3 ow = float3(1.0,1.0,1.0);//LUT White OUT Level
static const float ig = 1.0;                 //LUT Gamma Adjustment 0.05 - 10.0
//Use for real HDR. //Do not Use.
#define HDR_Toggle 0 //For HDR //Do not Use.
//Pooled Texture Issue for 4.8.0
#if __RESHADE__ >= 40910 && ForcePool
	#define PoolTex < pooled = true; >
#else
	#define PoolTex
#endif
//Older ReShade Compatibility
#if __RESHADE__ >= 40300
	#define Compatibility_TF 1
#else
	#define Compatibility_TF 0
#endif
//Check DX9 Game
#if __RENDERER__ == 0x9000
	#define DX9 1
#else
	#define DX9 0
#endif
//This GI shader is free and shouldn't sit behind a paywall. If you paid for this shader ask for a refund right away.
//Automatic Adjustment based on Resolutionsup to 4k considered. LOL good luck with 8k in 2020
#if (BUFFER_HEIGHT <= 720)
	#define RSRes 1.0
#elif (BUFFER_HEIGHT <= 1080)
	#define RSRes 0.8
#elif (BUFFER_HEIGHT <= 1440)
	#define RSRes 0.7
#elif (BUFFER_HEIGHT <= 2160)
	#define RSRes 0.6
#else
	#define RSRes 0.5 //??? 8k Mystery meat
#endif

//Help / Guide Information stub uniform a idea from LucasM
uniform int RadiantGI <
	ui_text = "RadiantGI is an indirect lighting algorithm based on the disk-to-disk radiance transfer by Michael Bunnell.\n"
			  		"As you can tell its name is a play on words and it radiates the kind of feeling I want from it one Ray Bounce at a time.\n"
			  			  "This GI shader is free and shouldn't sit behind a paywall. If you paid for this shader ask for a refund right away.\n"
			  			  		"As for my self I do want to provide the community with free shaders and any donations will help keep that motivation alive.\n"
			  			  			  "For more information and help please feel free to visit http://www.Depth3D.info or https://blueskydefender.github.io/AstrayFX\n "
			  			  			 	 "Help with this shader fuctions specifically visit the WIki @ https://github.com/BlueSkyDefender/AstrayFX/wiki/RadiantGI\n"
			  "Please enjoy this shader and Thank You for using RadiantGI.";
	ui_category = "RadiantGI";
	ui_category_closed = true;
	ui_label = " ";
	ui_type = "radio";
>;
//uniform float TEST < ui_type = "slider"; ui_min = 0.0; ui_max = 1; ui_label = "TEST"; > = 1;
uniform float samples <
	ui_type = "slider";
	ui_min = 4; ui_max = 12; ui_step = 1;
	ui_label = "Samples";
	ui_tooltip = "GI Sample Quantity is used to increase samples amount as a side effect this reduces noise.";
	ui_category = "Global Illumination";
> = 6;

uniform float GI_Ray_Length <
	ui_type = "drag";
	ui_min = 1.0; ui_max = 500; ui_step = 1;
	ui_label = "General Ray Length";
	ui_tooltip = "General GI Ray Length is used to increase the Sampling Radius.\n" //Marty didn't want me using Ray Length. But, this is easyer for normies to understand.
				 "The byproduc of this causes the ray casting distance to extend.\n" //So I opted for keeping the name and explaining it for the user.
			     "This scales automatically with multi level detail."; // So the name not 100% correct. But, it's close enough.
	ui_category = "Global Illumination";
> = 250;

uniform float Target_Lighting <
	ui_type = "slider";
	ui_min = -1.0; ui_max = 1.0;
	ui_label = "Targeted Lighting";
	ui_tooltip = "Lets you target the Direct Lighting and/or Indirect Lighting specifically, so the shader can use your adjustment for its own GI calculations.\n"
			     "Negative values target Indirect lighting and positive values target direct lighting in the image./n"
				 "Defaults is [0.05] and Zero is full image sampling.";
	ui_category = "Global Illumination";
> = 0.05;

uniform float2 NCD <
	ui_type = "slider";
	ui_min = 0.0; ui_max = 1.0;
	ui_label = "Near Details";
	ui_tooltip = "Lets you adjust detail of objects near the cam and or like weapon hand GI.\n"
			     "The 2nd Option is for Weapon Hands in game that fall out of range.\n"
			     "Defaults are [Near Details X 0.125] [Weapon Hand Y 0.0]";
	ui_category = "Global Illumination";
> = float2(0.125,0.0);

uniform float PCGI_Fade < //Blame the pineapple for this option.
	ui_type = "slider";
	ui_min = -1.0; ui_max = 1.0; //Still need to make this better......
	ui_label = "Depth Fade-Out";
	ui_tooltip = "GI Application Power that is based on Depth scaling for controlled fade In-N-Out.\n" //That's What A Hamburger's All About
			     "Can be set from 0 to 1 and is Set to Zero for No Culling.\n"
			     "Default is 0.0.";
	ui_category = "Global Illumination";
> = 1.0;

uniform float2 Reflectivness <
	ui_type = "slider";
	ui_min = 0.0; ui_max = 1.0;
	ui_label = "Diffusion Amount";
	ui_tooltip = "This basicly adds control for how defused the lighting should look on the ground.\n"
			     "Default is [1.0 | 0.5]. One is Max Diffusion and the value is length.";
	ui_category = "Global Illumination";
> = float2(1.0,0.5);

uniform bool Scattering<
	ui_label = "Subsurface Light Transport";
	ui_tooltip = "Enable The Subsurface Light Transport Function to create a Subsurface Scattering effect.\n"
			     "Default is Off.";
	ui_category = "Subsurface Scattering";
> = false;

uniform float Wrap <
	ui_type = "slider";
	ui_min = 0.0; ui_max = 1.0;
	ui_label = "Upper Scattering";
	ui_tooltip = "Control the Light Wrap Form Factor to adjust the Subsurface Scattering effect.\n"
			     "Default is [0.5].";
	ui_category = "Subsurface Scattering";
> = 0.5;
/*
uniform float User_SSS_Luma <
	ui_type = "slider";
	ui_min = 0.0; ui_max = 1.0;
	ui_label = "Subsurface Brightness";
	ui_tooltip = "This is used to fine tune the automatic brightness of the upper layer scattering.\n"
			     "Can be set from Zero to One.\n"
			     "Default is [0.5].";
	ui_category = "SSLT";
> = 0.5;
*/
uniform float Deep_Scattering <
	ui_type = "slider";
	ui_min = 0.0; ui_max = 1.0;
	ui_label = "Deep Scattering";
	ui_tooltip = "Control Thickness Estimation Form Factor to create a Deep Tissue Scattering effect.\n"
			     "Default is [0.1].";
	ui_category = "Subsurface Scattering";
> = 0.1;

uniform float Luma_Map <
	ui_type = "slider";
	ui_min = 0.0; ui_max = 1.0;
	ui_label = "Tissue Luma Map";
	ui_tooltip = "Controls the Luma Map that lets bright lights approximate deep penotration of the tissue.\n" //So Deep....
			     "Default is [0.5].";
	ui_category = "Subsurface Scattering";
> = 0.5;
uniform float3 Internals < // We are all pink and fleshy on the inside?
	ui_type = "color";
	ui_min = 0.0; ui_max = 1.0;
	ui_label = "Internal Flesh Color";
	ui_tooltip = "Since I can't tell what the internal color of the Deep Tissue you need to set this your self for RGB.\n"
			     "Defaults are [R 0.25] [B 0.0] [G 0.0] [L 0.5].";
	ui_category = "Subsurface Scattering";
> = float3(0.54,0.01,0.01);

uniform float2 Diffusion_Saturation_Power <
	ui_type = "slider";
	ui_min = 0.0; ui_max = 1.0;
	ui_label = "Subsurface Blur & Flesh Saturation";
	ui_tooltip = "Diffusion Blur is used too softens the lighting and makes the person a little more realistic by mimicking this effect skin has on light.\n"
			     "Simulate light diffusion favor red blurring over other colors.\n"
			     "Default are [0.5] [0.5].";
	ui_category = "Subsurface Scattering";
> = float2(0.25,0.5);

#if LutSD //Man............................................
uniform float2 SSS_Seek <
#else
uniform float SSS_Seek <
#endif
	ui_type = "slider";
	ui_min = 0.0; ui_max = 1.0;
#if LutSD
	ui_label = "Skin Detect Distance & Seeking";
#else
	ui_label = "Skin Detect Distance";
#endif
	ui_tooltip = "Lets you control how far we need to search and seek with the human skin tone detection algorithm for SSLT.\n"
#if LutSD
			     "Defaults are [0.25] and [0.5].";
#else
				 "The 2nd option only shows if you use the special LUT for custom and or more accurate skin tone detection.\n"
				 "You can get this LUT @ https://github.com/BlueSkyDefender/AstrayFX/wiki/Subsurface-Light-Transport\n"
			     "Default is [0.25].";
#endif
	ui_category = "Subsurface Scattering";
#if LutSD
> = float2(0.25,0.5);
#else
> = 0.25;
#endif

uniform int Sky_Emissive_Selection < //Emissive_Mode //Sky_Contribution
	ui_type = "combo";
    ui_label = "Directional Sky & Emissive Mode";
    ui_tooltip = "Use this to add Directional Sky and or Emissive Mode.\n"
    			"\n"
				"Directional Sky Color:\n"
				"Allows for the use of Sky Color Contribution to effect the world directly.\n"
    			"\n"
				"Emissive Mode:\n"
				"Make all bright objects''Emissive Objects,'' based on approximated information from the emitter.\n"
				"\n"
    			"Default is Off.";
    ui_items = "Off\0Directional Sky Color\0Emissive Mode\0Sky & Emissive Mode\0";
	ui_category = "Supplemental Contributions";
    > = 0;

uniform float GI_Sky_Saturation <
	ui_type = "slider";
	ui_min = 0.0; ui_max = 5.0;
	ui_label = "Sky Saturation";
	ui_tooltip = "Sky Color Saturation.\n"
			     "Default is [Power 1.0].";
	ui_category = "Supplemental Contributions";
> = 1.0;

uniform int Sky_Adustment <
    ui_type = "slider";
    ui_min = 0; ui_max = 6;
    ui_label = "Sky Color Averaging";
    ui_tooltip = "This lets you Average the sky color so you can have general color.";
	ui_category = "Supplemental Contributions";
> = 3;

uniform float D_Irradiance <
	ui_type = "slider";
	ui_min = 0.0; ui_max = 1.0;
	ui_label = "Distance Irradiance";
	ui_tooltip = "Distance Irradiance Lets you control Irradiance that comes from the Refracted Light from distant ground.\n"
			     "Default is [0.0] and Zero is Off.";
	ui_category = "Supplemental Contributions";
> = 0.0;

#if Force_Texture_Details
uniform float2 PCGI_2DTexture_Detail <
	ui_type = "slider";
	ui_min = 0.0; ui_max = 1.0;
	ui_label = "Texture Details & Falloff";
	ui_tooltip = "Lets you add Texture Details to PCGI so you can have added 2D texture information applyed to the GI.\n"
				 "Turn this on by adjusting the first slider and then adjusting it's falloff with the second.\n"
			     "Defaults are [0.0,0.5] and Zero is Off";
	ui_category = "Supplemental Contributions";
> = float2(0.0,1.0);
#else
static const int2 PCGI_2DTexture_Detail = 0;
#endif

uniform float Trim <
	ui_type = "slider";
	ui_min = 0.0; ui_max = 1.0;
	ui_label = "Trim";
	ui_tooltip = "Trim GI by limiting how far the GI is able to effect the objects around them.\n"
				 "Directional Sky Color is negatively impacted by this option.\n"
				 "Use this mostly for Emissive Mode.\n"
			     "Default is [0.0] and Zero is Off.";
	ui_category = "Supplemental Contributions";
> = 0.0;

#if Controlled_Blend
uniform float Blend <
	ui_type = "slider";
	ui_min = 0.0; ui_max = 1.0;
	ui_label = "Blend";
    ui_tooltip = "Use this to change the look of GI when applied to the final image.";
    ui_category = "Image";
> = 0.5;
#else
uniform int Blend <
	ui_type = "combo";
    ui_label = "Blend Mode";
    ui_tooltip = "Use this to change the look of GI when applied to the final image.";
    ui_items = "Mix\0Overlay\0Softlight\0Add\0";
    ui_category = "Image";
    > = 0;
#endif

uniform float GI_Power <
	ui_type = "slider";
	ui_min = 0.0; ui_max = 2.5;
	ui_label = "Power";
	ui_tooltip = "Main overall GI application power control.\n"
			     "Default is [Power 1.0].";
	ui_category = "Image";
> = 1.0;

uniform float GI_Saturation <
	ui_type = "slider";
	ui_min = 0.0; ui_max = 5.0;
	ui_label = "GI Saturation";
	ui_tooltip = "Irradiance Map Saturation.\n"
			     "Default is [Power 1.0].";
	ui_category = "Image";
> = 1.0;
/*
uniform float shadows <
    ui_type = "slider";
    ui_min = -2.5; ui_max = 0.0;
    ui_label = "Enhance Shadows.";
    ui_tooltip = "Boost The Shadows in the image.";
	ui_category = "Image";
> = 0.0;
*/
uniform float HDR_BP <
	ui_type = "slider";
	ui_min = 0.0; ui_max = 1.0;
	ui_label = "HDR Extraction Power";
	ui_tooltip = "Use This to adjust the HDR Power, You can override this value and set it to like 1.5 or something.\n"
				 "Dedault is [0.5] and Zero and is Off.";//Because new HDR extraction works well now.
	ui_category = "Image";
> = 0.5;

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

uniform bool Depth_Map_Flip <
	ui_label = "Depth Map Flip";
	ui_tooltip = "Flip the depth map if it is upside down.";
	ui_category = "Depth Map";
> = DB_X;
uniform int Debug <
	ui_type = "combo";
	ui_items = "RadiantGI\0Irradiance Map\0Light Source Map\0Depth & Normals\0";
	ui_label = "Debug View";
	ui_tooltip = "View Debug Buffers.";
	ui_category = "Extra Options";
> = 0;

uniform int SamplesXY <
	ui_type = "slider";
	ui_min = 0; ui_max = 20;
	ui_label = "Denoise Power";//Ya CeeJay.dk you got your way..
	ui_tooltip = "This raises or lowers Samples used for the Final DeNoisers which in turn affects Performance.\n"
				 "This also has the side effect of smoothing out the image so you get that Normal Like Smoothing.\n"
				 "Default is 7 and you can override this a bit.";
	ui_category = "Extra Options";
> = 6;

uniform float GI_Res <
	ui_type = "slider";
	ui_min = 0.5; ui_max = 1;// ui_step = 0.1;
	ui_label = "Resolution";
	ui_tooltip = "GI Resolution is used too increase performance at the cost of quality.\n"
				 "Larger adjustments from default will reduce noise and vice versa.\n"
				 "Default is automaticly adjusted based on current resolution.";
	ui_category = "Extra Options";
> = RSRes;

uniform bool IGN_Toggle <
	ui_label = "Interleaved Gradient Noise";
	ui_tooltip = "In the on position it uses Interleaved Gradient Noise in stead of Regular Noise.";
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
uniform int PP_Options < // Yes I am a Grown Man....
	ui_text = "Max Depth Cutoff: Sets cutoff point for depth in RadiantGI. \n"
#if !VRS_MAP
			  "                  Affects masking for Directional Sky Color.\n"
			  "        DEFAULT [0.999]        RANGE [0.5] - [0.999]        ";
#else
			  "                  Affects masking for Directional Sky Color.\n"
			  "        DEFAULT [0.999]        RANGE [0.5] - [0.999]        "
			  "\n"
			  "VRS Optical Flow: Is used to turn on this feature in the VRS_Map.fx\n"
			  "                  shader by Lord of Lunacy a Shader Dev.\n"
			  "        DEFAULT [0]            TOGGLE [0] - [1]               ";
#endif
	ui_category = "Preprocessor Options Information";
	ui_category_closed = false;
	ui_label = " ";
	ui_type = "radio";
>;

//This GI shader is free and shouldn't sit behind a paywall. If you paid for this shader ask for a refund right away.
uniform bool Text_Info < source = "key"; keycode = Text_Info_Key; toggle = true; mode = "toggle";>;
#define pix float2(BUFFER_RCP_WIDTH, BUFFER_RCP_HEIGHT)
uniform float frametime < source = "frametime"; >;     // Time in milliseconds it took for the last frame to complete.
uniform int framecount < source = "framecount"; >;     // Total amount of frames since the game started.
uniform float clock < source = "timer"; >;             // A timer that starts when the Game starts.
#define Alternate framecount % 2.0 == 0                  // Alternate per frame
#define PI 3.14159265358979323846264                   // PI
#if LutSD //Extracted Look Up Table
texture TexName < source =  LUT_File_Name; > { Width =  Tile_SizeXY *  Tile_Amount; Height =  Tile_SizeXY ; };
sampler Sampler { Texture =  TexName; };
#endif

int2 Sky_Emissive_Bool()
{
	int Sky_Bool = Sky_Emissive_Selection == 1 || Sky_Emissive_Selection == 3 ? 1 : 0;
	int Emissive_Bool = Sky_Emissive_Selection == 2 || Sky_Emissive_Selection == 3 ? 1 : 0;
	return int2(Sky_Bool,Emissive_Bool);
}

float Scale_PCGI_Fade()
{
	return lerp(0,2,saturate(abs(PCGI_Fade)) );
}

float Offset_Switch()
{
	return Offset >= -0.0015 && Offset <= 0.0015 ? 0 : Offset;  
}

float2 Saturation()
{
	return  float2(GI_Saturation,GI_Sky_Saturation);
}

float Reflect()
{
	return Reflectivness.x == 0 ? -0.001 : Reflectivness.x;
}

float2 GIRL()
{
	return  float2(GI_Ray_Length,250);
}
static const float EvenSteven[21] = { 0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20 , 22, 24, 26, 28, 30, 32, 34, 36, 38, 40}; // It's not odd...
/////////////////////////////////////////////////////D3D Starts Here/////////////////////////////////////////////////////////////////
static const float2 XYoffset[8] = { float2( 0,+pix.y ), float2( 0,-pix.y), float2(+pix.x, 0), float2(-pix.x, 0), float2(-pix.x,-pix.y), float2(+pix.x,-pix.y), float2(-pix.x,+pix.y), float2(+pix.x,+pix.y) };
static const float DBoffsets[7] = { -5.5, -3.5, -1.5, 0.0, 1.5, 3.5, 5.5 };
static const float SkyMipLeves[7] = { 0, 5, 6, 7, 8, 9, 10};
//Diffusion Blur weights to blur red channel more than green and blue SSS
static const float3 DBweight[7] = { float3( 0.006, 0.00, 0.00),
									float3( 0.061, 0.00, 0.00),
									float3( 0.242, 0.25, 0.25),
									float3( 0.383, 0.50, 0.50),
									float3( 0.242, 0.25, 0.25),
									float3( 0.061, 0.00, 0.00),
									float3( 0.006, 0.00, 0.00) };
//Poisson Disk Precompute
static const float2 PoissonTaps[13] = { float2(-0.326,-0.406), //This Distribution seems faster.....
										float2(-0.840,-0.074), //Tried many from https://github.com/bartwronski/PoissonSamplingGenerator
										float2(-0.696, 0.457), //But they seems slower then this one I found online..... WTF
										float2(-0.203, 0.621),
										float2( 0.962,-0.195),
										float2( 0.473,-0.480),
										float2( 0.519, 0.767),
										float2( 0.185,-0.893),
										float2( 0.507, 0.064),
										float2( 0.896, 0.412),
										float2(-0.322,-0.933),
										float2(-0.792,-0.598),
										float2(-0.255, 0.464) };

float fmod(float a, float b)
{
	float c = frac(abs(a / b)) * abs(b);
	return a < 0 ? -c : c;
}

float Scale(float val,float max,float min) //Scale to 0 - 1
{
	return (val - min) / (max - min);
}

int useVRS(float2 texcoord, int UseVRS)
{
	return VRS_Map::ShadingRate(texcoord, UseVRS, VRS_RATE_1X1 ) == VRS_RATE_2X2;
}

float MCNoise(float FC ,float2 TC,float seed)
{   //This is the noise I used for rendering
	float motion = FC, a = 12.9898, b = 78.233, c = 43758.5453, dt = dot(TC.xy + 0.5, float2(a,b)), sn = fmod(dt,PI + seed);
	return frac(frac(sin(sn) * c) + 0.71803398875f * motion);
}   int T_01() { return 12500; }

float Interleaved_Gradient_Noise(float2 TC)
{   //Magic Numbers
    float3 MNums = float3(0.06711056, 0.00583715, 52.9829189);
    return frac( MNums.z * frac(dot(TC,MNums.xy)) );
}

float gaussian(float x, float sigma)
{
    return (rsqrt( PI * pow(sigma,2))) * exp(-(pow(x,2) / (2.0 * pow(sigma,2))));
}
//Color Mixing
float3 overlay(float3 c, float3 b) 		{ return c<0.5f ? 2.0f*c*b:(1.0f-2.0f*(1.0f-c)*(1.0f-b));}
float3 softlight(float3 c, float3 b) 	{ return b<0.5f ? (2.0f*c*b+c*c*(1.0f-2.0f*b)):(sqrt(c)*(2.0f*b-1.0f)+2.0f*c*(1.0f-b));}
float3 add(float3 c, float3 b) 	{ return c + (b * 0.5);}
//Enhace Shadows
//float3 Shadows( float3 color )
//{
//    float3 distribution = pow( 1.0f - color.rgb, 4.0f ), SH = color.rgb * ( distribution * shadows ) * ( 1.0f - color.rgb );
//    return saturate( color.rgb + SH );
//}
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
float3 RGBtoYCbCr(float3 rgb)
{   float C[1];//The Chronicles of Riddick: Assault on Dark Athena FIX I don't know why it works.......
	float Y  =  .299 * rgb.x + .587 * rgb.y + .114 * rgb.z; // Luminance
	float Cb = -.169 * rgb.x - .331 * rgb.y + .500 * rgb.z; // Chrominance Blue
	float Cr =  .500 * rgb.x - .419 * rgb.y - .081 * rgb.z; // Chrominance Red
	return float3(Y,Cb + 128./255.,Cr + 128./255.);
}

float3 YCbCrtoRGB(float3 ycc)
{
	float3 c = ycc - float3(0., 128./255., 128./255.);

	float R = c.x + 1.400 * c.z;
	float G = c.x - 0.343 * c.y - 0.711 * c.z;
	float B = c.x + 1.765 * c.y;
	return float3(R,G,B);
}
//----------------------------------Inverse ToneMappers--------------------------------------------
float Luma(float3 C)
{
	float3 Luma;

	if (HDR_Toggle == 0)
		Luma = float3(0.2126, 0.7152, 0.0722); // (HD video) https://en.wikipedia.org/wiki/Luma_(video)
	else
		Luma = float3(0.2627, 0.6780, 0.0593); // (HDR video) https://en.wikipedia.org/wiki/Rec._2100

	return dot(C,Luma);
}

float max3(float x, float y, float z)
{
    return max(x, max(y, z));
}

float3 inv_Tonemapper(float4 color)
{   //Timothy Lottes fast_reversible
	return color.rgb * rcp((1.0 + max(color.w,0.001)) - max3(color.r, color.g, color.b));
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

float3 Saturator_A(float3 C, float Depth_Mask,float DD)//DD is Distance Dimming so that Sky irradiance has prefrence.
{   float Mask = 1-Depth_Mask, Sat = Debug == 2 ? 0.0 : Saturation().x, Distance_Irradiance = lerp(0.5,1.0,saturate(D_Irradiance)), DDMasked = lerp(1-saturate((DD-Distance_Irradiance)/(1-Distance_Irradiance)),1,DD * Depth_Mask);
	C.rgb *= DDMasked;
	C.rgb *= lerp(1,lerp(1,6,Trim),Depth_Mask);//Used to keep trimming from effecting Sky color too...much....
	C.rgb = RGBToHSL(lerp(C.rgb, C.rgb * 1.25,Depth_Mask));// Can boost the sky color irradiance here.
	C.y *= (Sat * Mask) + lerp(1,Saturation().y,Depth_Mask);
	return HSLToRGB(C.rgb);
}

float3 Saturator_B(float3 C) // for SSS
{
	C.rgb = RGBToHSL(C.rgb);
	C.y *= saturate(Diffusion_Saturation_Power.y) * 2;
	return HSLToRGB(C.rgb);
}

float3 Saturator_C(float3 C) // for SSS
{
	C.rgb = RGBToHSL(C.rgb);
	//C.y *= 1.+ saturate(GI_LumaPower-0.625) ;
	return HSLToRGB(C.rgb);
}

float3 InternalFleshColor(float3 SSS, float Density, float3 Internal, float LumProfile)
{   float3 D = saturate(Density) * LumProfile;
	return SSS + lerp(D * Internal,0,saturate(SSS));
}
//// Skin Functions /////////////////////////////////////////////////// -- Ported by BSD from  02_Isolate_SkinTones.fx
#if LutSD
float3 levels( float3 color, float3 blackin, float3 whitein, float gamma, float3 outblack, float3 outwhite )
{
    float3 ret       = saturate( color.xyz - blackin.xyz ) / max( whitein.xyz - blackin.xyz, 0.000001f );
    ret.xyz          = pow( ret.xyz, gamma );
    ret.xyz          = ret.xyz * saturate( outwhite.xyz - outblack.xyz ) + outblack.xyz;
    return ret;
}
// SRGB <--> CIELAB CONVERSIONS Ported by Prod80.

// Reference white D65
#define reference_white     float3( 0.95047, 1.0, 1.08883 )
// Source
// http://www.brucelindbloom.com/index.html?Eqn_RGB_to_XYZ.html
#define K_val               float( 24389.0 / 27.0 )
#define E_val               float( 216.0 / 24389.0 )

float3 xyz_to_lab( float3 c )
{
    // .xyz output contains .lab
    float3 w       = c / reference_white;
    float3 v;
    v.x            = ( w.x >  E_val ) ? pow( abs(w.x), 1.0 / 3.0 ) : ( K_val * w.x + 16.0 ) / 116.0;
    v.y            = ( w.y >  E_val ) ? pow( abs(w.y), 1.0 / 3.0 ) : ( K_val * w.y + 16.0 ) / 116.0;
    v.z            = ( w.z >  E_val ) ? pow( abs(w.z), 1.0 / 3.0 ) : ( K_val * w.z + 16.0 ) / 116.0;
    return float3( 116.0 * v.y - 16.0,
                   500.0 * ( v.x - v.y ),
                   200.0 * ( v.y - v.z ));
}

float3 lab_to_xyz( float3 c )
{
    float3 v;
    v.y            = ( c.x + 16.0 ) / 116.0;
    v.x            = c.y / 500.0 + v.y;
    v.z            = v.y - c.z / 200.0;
    return float3(( v.x * v.x * v.x > E_val ) ? v.x * v.x * v.x : ( 116.0 * v.x - 16.0 ) / K_val,
                  ( c.x > K_val * E_val ) ? v.y * v.y * v.y : c.x / K_val,
                  ( v.z * v.z * v.z > E_val ) ? v.z * v.z * v.z : ( 116.0 * v.z - 16.0 ) / K_val ) *
                  reference_white;
}

float3 srgb_to_xyz( float3 c )
{
    // Source: http://www.brucelindbloom.com/index.html?Eqn_RGB_XYZ_Matrix.html
    // sRGB to XYZ (D65) - Standard sRGB reference white ( 0.95047, 1.0, 1.08883 )
    const float3x3 mat = float3x3(
    0.4124564, 0.3575761, 0.1804375,
    0.2126729, 0.7151522, 0.0721750,
    0.0193339, 0.1191920, 0.9503041
    );
    return mul( mat, c );
}

float3 xyz_to_srgb( float3 c )
{
    // Source: http://www.brucelindbloom.com/index.html?Eqn_RGB_XYZ_Matrix.html
    // XYZ to sRGB (D65) - Standard sRGB reference white ( 0.95047, 1.0, 1.08883 )
    const float3x3 mat = float3x3(
    3.2404542,-1.5371385,-0.4985314,
   -0.9692660, 1.8760108, 0.0415560,
    0.0556434,-0.2040259, 1.0572252
    );
    return mul( mat, c );
}
// Maximum value in LAB, B channel is pure blue with 107.8602... divide by 108 to get 0..1 range values

// Maximum value in LAB, L channel is pure white with 100
float3 srgb_to_lab( float3 c )
{
    float3 lab =  srgb_to_xyz( c );
    lab        =  xyz_to_lab( lab );
    return lab / float3( 100.0, 108.0, 108.0 );
}

float3 lab_to_srgb( float3 c )
{
    float3 rgb =  lab_to_xyz( c * float3( 100.0, 108.0, 108.0 ));
    rgb        =  xyz_to_srgb( max( min( rgb, reference_white ), 0.0 ));
    return saturate( rgb );
}
#endif
float  SkinDetection(float4 color)
{   //Can Also mask out the sky or things that are too far.......
	float3 YCbCr = RGBtoYCbCr(color.rgb) * 255.0;
	//So after exhaustive image histogram analysis, the optimal range threshold is....
	#if LutSD
	//Prod80 Port of Skin Isolation for RadiantGI
    float2 texelsize = rcp( Tile_SizeXY );
    texelsize.x     /=  Tile_Amount;

    float3 lutcoord  = float3(( color.xy * Tile_SizeXY - color.xy + 0.5f ) * texelsize.xy, color.z * Tile_SizeXY - color.z );
    float lerpfact   = frac( lutcoord.z );
    lutcoord.x      += ( lutcoord.z - lerpfact ) * texelsize.y;

    float3 lutcolor  = lerp( tex2D(  Sampler, lutcoord.xy ).xyz, tex2D(  Sampler, float2( lutcoord.x + texelsize.y, lutcoord.y )).xyz, lerpfact );
    lutcolor.xyz     = levels( lutcolor.xyz,    saturate( ib.xyz  ),
                                                saturate( iw.xyz  ),
                                                ig,
                                                saturate( ob.xyz  ),
                                                saturate( ow.xyz  ));
    float3 lablut    = srgb_to_lab( lutcolor.xyz );
    float3 labcol    = srgb_to_lab( color.xyz );
    float newluma    = lerp( labcol.x, lablut.x,  MixLuma );
    float2 newAB     = lerp( labcol.yz, lablut.yz,  MixChroma );
    lutcolor.xyz     = lab_to_srgb( float3( newluma, newAB ));
    color.xyz        = lerp( color.xyz, saturate( lutcolor.xyz  ),  Intensity );
	float Fin        = dot(color.xyz,color.xyz);
	if(Fin > SSS_Seek.y)
		return 1.0;
	else
		return 0.0;
	#else
    if (YCbCr.y > 80 && YCbCr.y < 118 && YCbCr.z > 133 && YCbCr.z < 173)
		return 0.0;
	else
		return 1.0;
	#endif
} int T_02() { return 25000; }
/////////////////////////////////////////////////////Texture Samplers////////////////////////////////////////////////////////////////
#if Compatibility_TF
	#define RGBAC RGB10A2
#else
	#define RGBAC RGBA8
#endif
texture DepthBufferTex : DEPTH;

sampler ZBuffer
	{
		Texture = DepthBufferTex;
	};

texture BackBufferTex : COLOR;

sampler BackBufferPCGI
	{
		Texture = BackBufferTex;
	};

texture2D PCGIpastTex { Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = R16f; MipLevels = 5;}; //Has to 16bit min
sampler2D PCGIpastFrame { Texture = PCGIpastTex;
	MagFilter = POINT;
	MinFilter = POINT;
	MipFilter = POINT;
};

texture2D PCGIaccuTex { Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT ; Format = RGBAC; };
sampler2D PCGIaccuFrames { Texture = PCGIaccuTex; };
//Seen issues with pooling this texture.. Workaround for 4.8.0-
texture2D PCGIcurrColorTex PoolTex { Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = RGBA16f; MipLevels = 11;}; //Cant use RGB10A2 // must be RGBA8 or RGBA16f since I store information on A
sampler2D PCGIcurrColor { Texture = PCGIcurrColorTex; };

texture2D PCGIcurrNormalsDepthTex < pooled = true; > { Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = RGBA16f; MipLevels = 11;};
sampler2D PCGIcurrNormalsDepth { Texture = PCGIcurrNormalsDepthTex; };

texture2D RadiantGITex { Width = BUFFER_WIDTH ; Height = BUFFER_HEIGHT ; Format = RGBAC; };
sampler2D PCGI_Info { Texture = RadiantGITex;
	MagFilter = POINT;
	MinFilter = POINT;
	MipFilter = POINT;
};

texture2D RadiantSSTex { Width = BUFFER_WIDTH ; Height = BUFFER_HEIGHT ; Format = RGBA16f;};
sampler2D PCSS_Info { Texture = RadiantSSTex;
	MagFilter = POINT;
	MinFilter = POINT;
	MipFilter = POINT;
};

texture2D PCGIReconstructionTex < pooled = true; > { Width = BUFFER_WIDTH ; Height = BUFFER_HEIGHT ; Format = RGBAC; MipLevels = 8;};
sampler2D PCGIReconstruction_Info { Texture = PCGIReconstructionTex;
	MagFilter = POINT;
	MinFilter = POINT;
	MipFilter = POINT;
};
//Seen issues with pooling this texture.. Workaround for 4.8.0-
texture2D PCGIHorizontalTex PoolTex { Width = BUFFER_WIDTH / 2; Height = BUFFER_HEIGHT / 2; Format = RGBAC; };
sampler2D PCGI_BGUHorizontal_Sample { Texture = PCGIHorizontalTex;};
//Seen issues with pooling this texture.. Workaround for 4.8.0-
texture2D PCGIVerticalTex PoolTex { Width = BUFFER_WIDTH ; Height = BUFFER_HEIGHT ; Format = RGBAC; };
sampler2D PCGI_BGUVertical_Sample { Texture = PCGIVerticalTex;
};

//texture2D StorageTex < pooled = true; > { Width = 256 ; Height = 256 ; Format = R8;  };
//sampler2D Storage { Texture = StroageTex; };
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
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
	float zBuffer = tex2Dlod(ZBuffer, float4(texcoord,0,0)).x, zBufferWH = zBuffer, Far = 1.0, Near = 0.125/(0.00000001+Depth_Map_Adjust), NearWH = 0.125/(0.00000001+(Depth_Map ? NCD.y : 10*NCD.y)), OtherSettings = Depth_Map ? NCD.y : 100 * NCD.y ; //Near & Far Adjustment
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
}

float SUMTexture_lookup(float2 TC, float dx, float dy)
{   float Depth = 1-Depth_Info( TC );
		  Depth = (Depth - 0)/ (lerp(1,10,saturate(1-PCGI_2DTexture_Detail.x)) - 0);
    float2 uv = (TC.xy + float2(dx , dy ) * pix);
    float3 c = tex2Dlod( BackBufferPCGI, float4(uv.xy,0, 0) ).rgb * 0.5;

	// return as luma
    return (0.2126*c.r + 0.7152*c.g + 0.0722*c.b) * Depth * 0.00666f;
}

float3 TextureNormals(float2 UV, float Depth )
{
	if(saturate(PCGI_2DTexture_Detail.x) > 0)
	{
		// simple sobel edge detection
	    float dx = 0.0;
	    dx += -1.0 * SUMTexture_lookup(UV, -1.5, -1.5);
	    dx += -2.0 * SUMTexture_lookup(UV, -1.5,  0.0);
	    dx += -1.0 * SUMTexture_lookup(UV, -1.5,  1.5);
	    dx +=        SUMTexture_lookup(UV,  1.5, -1.5);
	    dx +=  2.0 * SUMTexture_lookup(UV,  1.5,  0.0);
	    dx +=        SUMTexture_lookup(UV,  1.5,  1.5);

	    float dy = 0.0;
	    dy += -1.0 * SUMTexture_lookup(UV, -1.5, -1.5);
	    dy += -2.0 * SUMTexture_lookup(UV,  0.0, -1.5);
	    dy += -1.0 * SUMTexture_lookup(UV,  1.5, -1.5);
	    dy +=        SUMTexture_lookup(UV, -1.5,  1.5);
	    dy +=  2.0 * SUMTexture_lookup(UV,  0.0,  1.5);
	    dy +=        SUMTexture_lookup(UV,  1.5,  1.5);

		float edge = sqrt(dx*dx + dy*dy);
			  edge *= edge;
			  
		float angle = atan2(dx,dy);

		float X = edge * sin(angle); //X= -X;
		float Y = edge * sin(angle + 7.5 * PI / 3.);// Adjust me to rotate Normals
		float Z = edge * (X - Y);
	
		return min(1,lerp(float3(X,Y,Z) * lerp(1,Depth,PCGI_2DTexture_Detail.y), 0, float3(X,Y,Z) == 0.5) );
	}
	else
		return 0;

}
//Improved Normal reconstruction from Depth
float3 DepthNormals(float2 texcoord)
{
	float2 Pix_Offset = pix.xy;
	//A 2x2 Taps is done here. You can also do 4x4 tap
	float2 uv0 = texcoord; // center
	float2 uv1 = texcoord + float2( Pix_Offset.x, 0); // right
	float2 uv2 = texcoord + float2(-Pix_Offset.x, 0); // left
	float2 uv3 = texcoord + float2( 0, Pix_Offset.y); // down
	float2 uv4 = texcoord + float2( 0,-Pix_Offset.y); // up

	float depth = Depth_Info( uv0 );

	float depthR = Depth_Info( uv1 );
	float depthL = Depth_Info( uv2 );
	float depthD = Depth_Info( uv3 );
	float depthU = Depth_Info( uv4 );

	float3 P0, P1, P2;

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
	//Not the best way to do it be it had to be fast.
	float3 Normals_XYZ = cross(P2 - P0, P1 - P0);
	
	float3 Norm_XYZ = abs(normalize(Normals_XYZ) * 0.5 + 0.5);
	
	float3 DX = ddx(Norm_XYZ);
	float3 DY = ddy(Norm_XYZ);
	
	float DDX, DDY;
	DDX += depthD;
	DDX += depthL;
	DDX += depth;

	DDY += depth;
	DDY += depthR;
	DDY += depthU;
	
	float N_Mask = 1-saturate(distance(DX * DX , DY * DY ) > 0.006), D_Mask = 1-saturate(distance(DDX * DDX , DDY * DDY ) > 0.029);	

	return normalize( Normals_XYZ + lerp(0, TextureNormals(texcoord, depth ), saturate(D_Mask + D_Mask) ) );
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

float4 Normals_Depth(float2 texcoords, float Mips)
{
	float Depth = tex2Dlod(PCGIcurrNormalsDepth,float4(texcoords,0,Mips)).z;
	float3 Normals = UnpackNormals(tex2Dlod(PCGIcurrNormalsDepth,float4(texcoords,0,Mips)).xy);
	return float4(Normals,Depth);
}

float DepthMDC(float2 texcoords, float Mips)
{
	return smoothstep(0,1,Normals_Depth(texcoords, Mips).w) > MaxDepth_CutOff;
}
/////////////////////////////////////////////////////Blur Shaders////////////////////////////////////////////////////////////////
//Unlimited Blur
float2 Vogel2D(int uIndex, int nTaps, float phi)
{
    const float GoldenAngle = PI * (3.0 - sqrt(5.0));
    const float r = sqrt(uIndex + 0.5f) / sqrt(nTaps);
    float theta = uIndex * GoldenAngle + phi;

    float2 sc;
    sincos(theta, sc.x, sc.y);
    return r * sc.yx;
}

float4 Ublur(sampler2D Tex,in float2 TC,int Mip, float Radius)
{
    const int uTaps = 8;//went from 16 to 8 saves a little
    //float uBoard = fmod(dot(pos, 1.0), 2.0);
    float2 ps = pix * Radius;
    float urand = MCNoise( 1, TC, 1 ) * (PI*2);//nrand(pos * uBoard) * (PI*2);
    float4 uImage;

    [fastopt]
    for (int i = 0; i < uTaps; i++)
    {
        float2 ofs = Vogel2D(i, uTaps, urand);
        float4 uColor = tex2Dlod(Tex,float4(TC + ofs * ps,0,Mip)).rgba;
        uImage = lerp(uImage, uColor, rcp(i + 1));
    }

    return uImage;
}
//Custom Edge Avoiding Gaussian Denoiser
float4 Denoise(sampler Tex, float2 texcoords, int SXY, int Dir , float R )
{
	float4 StoredNormals_Depth = Normals_Depth( texcoords, 0);
	float4 center = tex2D(Tex,texcoords), color = 0.0;//Like why do SmoothNormals when 2nd Level Denoiser is like Got you B#*@!........
	float total = 0.0, NormalBlurFactor = Debug == 1 ? 0.25f : 1.0f, DepthBlurFactor = 0.011f,  DM = smoothstep(0,1,StoredNormals_Depth.w) > MaxDepth_CutOff;
	R += 1-tex2Dlod( PCGIcurrNormalsDepth,float4(texcoords,0,0)).w ;
	#if VRS_MAP
		R += useVRS(texcoords, 1);
	#endif
	if(SXY > 0) // Who would win Raw Boi or Gaussian Boi
	{   [fastopt]
	    for (int i = -SXY * 0.5; i <= SXY * 0.5; ++i)
		{
        	float2 D = Dir ? float2( i, 0) : float2( 0, i), TC = texcoords + D * R * pix;

			float4 ModifiedNormals_Depth = Normals_Depth( TC, 2);//Use lower mip level here on finnished product.
			float ModN = length(StoredNormals_Depth.xyz - ModifiedNormals_Depth.xyz), ModD = saturate( StoredNormals_Depth.w - ModifiedNormals_Depth.w);

			float D_Dist2 = max(ModD, 0.0), d_w = min(exp(-(D_Dist2)/DepthBlurFactor), 1.0);
	        float N_Dist2 = max(ModN, 0.0), n_w = min(exp(-(N_Dist2)/NormalBlurFactor), 1.0);


			float Weight = gaussian(i, sqrt(SXY));//Looks better
			Weight *= d_w;// Removed Depth Weighting because it was not really needed.
			Weight *= n_w;
			color += tex2Dlod(Tex, float4(TC ,0, 0)) * Weight;
        	total += Weight;
    	}
	}
	return SamplesXY > 0 ? lerp(color / total,center,DM) : center;
}
//Diffusion Blur not moved here.... will do this some other time.
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

float4 BBColor(float2 texcoords, int Mips)
{   float LL_Comp = 0.5; //Wanted to automate this but it's really not need.
	float4 BBC = tex2Dlod(PCGIcurrColor,float4(texcoords,0,Mips)).rgba;//PrepColor(texcoords, 0, Mips);
	BBC.rgb = Sky_Emissive_Bool().x ? BBC.rgb : BBC.rgb * ( 1-DepthMDC(texcoords, 0) );
	//	BBC.rgb = (BBC.rgb - 0.5) * (LL_Comp + 1.0) + 0.5;
	//	return BBC + (LL_Comp * 0.5625);
	return BBC;
}

float4 DirectLighting(float2 texcoords , int Mips)
{   float MDC = DepthMDC(texcoords, 0), Sky_Quality = lerp(Mips,SkyMipLeves[min(6,Sky_Adustment)],MDC);
	float4 BC = BBColor(texcoords, Sky_Quality);
	if(HDR_BP > 0)
		BC.rgb = inv_Tonemapper(float4(BC.rgb,1-HDR_BP));
	float  GS = Luma(BC.rgb), Boost = 1;

		if(Target_Lighting >= 0)
		{
		   BC.rgb /= GS;
		   BC.rgb *= saturate(GS - lerp(0.0,0.5,saturate(Target_Lighting)));
		}
		else
		{
		   BC.rgb /= 1-GS;
		   BC.rgb *= 1-saturate(GS + lerp(0.0,0.5,saturate(abs(Target_Lighting))));
		}

		   Boost = lerp(1.0,2.5,saturate(abs(Target_Lighting)));

	return float4(Saturator_A(BC.rgb * Boost, DepthMDC(texcoords, 0) , Normals_Depth(texcoords, 0).w),BC.a);
}

float3 GetPosition(float2 texcoord)
{
	float3 DM = -Normals_Depth(texcoord, 0 ).www;
	return float3(texcoord.xy*2-1,1.0)*DM;
} float GetPos() { float Postional_Information = T_02() == 25000 ? 0 : 1 ; return Postional_Information;}

float2 MultiPattern(float2 TC)
{   //const int Images = 6, Levels[Images] = { 0, 1, 2, 3, 4, 5} ;  //OrderedMixing = Levels[int(fmod(Grid.x+Grid.y*(Images*0.5),Images))];
	//float2 Grid = floor( TC * float2(BUFFER_WIDTH, BUFFER_HEIGHT ) );
	//float CheckerBoard = fmod(Grid.x+Grid.y,2.0);//, V_Interlaced = fmod(Grid.x,2.0), BayerLike = V_Interlaced ? CheckerBoard ? 0 : 1 : CheckerBoard ? 2 : 3;
    float2 Grid = floor( float2(TC.x * BUFFER_WIDTH, TC.y * BUFFER_HEIGHT) );
	return float2(fmod(Grid.x+Grid.y, 2.0),fmod(Grid.x, 2.0));
}

float2 Rotate2D_A( float2 r, float l , float2 TC)
{   float Reflective_Diffusion = lerp(saturate(abs(Reflect())),1.0,smoothstep(0,0.25,1-dot(float3(0,1,0) ,Normals_Depth(TC,0).xyz)));
	float2 Directions;
	sincos(l,Directions[0],Directions[1]);//same as float2(cos(l),sin(l))
	Reflective_Diffusion = Reflect() < 0 ? Reflective_Diffusion : MultiPattern(TC.xy * GI_Res).y ? 1 : Reflective_Diffusion;
	return float2( dot( r * Reflective_Diffusion, float2(Directions[1], -Directions[0]) ), dot( r, Directions.xy ) );
}

float2 Rotate2D_B( float2 r, float l )
{   float2 Directions;
	sincos(l,Directions[0],Directions[1]);//same as float2(cos(l),sin(l))
	return float2( dot( r, float2(Directions[1], -Directions[0]) ), dot( r, Directions.xy ) );
}

float SSSMasking(float2 TC)
{
	float SSSD = lerp(0,1, saturate(1-Normals_Depth(TC, 0 ).w * lerp(1,10,SSS_Seek.x)) );
	return SSSD * smoothstep(0,0.25,1-dot(float3(0,1,0) ,Normals_Depth(TC,0).xyz) * saturate(dot(float3(0,1,0) ,Normals_Depth(TC,0).xyz)) );// || dot(float3(0,1,0) ,NormalsMap(TC,0))
}
//Form Factor Approximations
float RadianceFF(in float2 texcoord,in float3 ddiff,in float3 normals, in float2 AB,in float STDepth)
{   //So normal and the vector between "Element to Element - Radiance Transfer."
	float4 v = float4(normalize(ddiff), length(ddiff));
	float Mnormals = abs(Reflect()) < 1 ? lerp(3,0,saturate(dot(float3(0,1,0),normals))) : 3, Trimming = lerp(1,5000,saturate(Trim)) ;
	//Emitter & Recever [With emulated Back-Face lighting.]
	float giE_Selection = Sky_Emissive_Bool().y ? dot( 1-v.xyz   , 1-Normals_Depth(texcoord+AB, Mnormals ).xyz ) : dot( -v.xyz   , Normals_Depth(texcoord+AB, Mnormals ).xyz );
	float2 giE_R =  max(float2(   step(   0.0,   giE_Selection    ),   dot( v.xyz, normals )   )   ,0);
	float FF_Dampen = Sky_Emissive_Bool().y ? PI*20 : PI*10; //Emulated Back-Face lighting Adjustment.
	return saturate((100 * giE_R.x * giE_R.y) / ( lerp(Trimming,1,STDepth * Sky_Emissive_Bool().x) * (v.w*v.w) + FF_Dampen) );
}
/* //This Code is Disabled Not going to use in RadiantGI
float AmbientOcclusionFF(in float2 texcoord,in float3 ddiff,in float3 normals, in float2 AB)
{   //So normal and the vector between "Element to Element - Occlusion."
	float4 v = float4(normalize(ddiff), length(ddiff));
	//Emitter & Recever - Clamped Values are used for self Shadowing.
	float2 aoE_R = 1.0*float2(1-clamp(dot(-v.xyz,NormalsMap(texcoord+AB,0)),-1,0), saturate(dot( v.xyz, normals )));
	return saturate(aoE_R.x * aoE_R.y * (1.0 - (1*8) / sqrt(1/(v.w*v.w) + PI )));
}
//This Code is Disabled Not going to use in RadiantGI
float GlossyFF(in float2 texcoord,in float3 ddiff,in float3 normals, in float2 AB, float PassD)
{   //So normal and the vector between "Element to Element - Specular Effect."
	float4 v = float4(normalize(ddiff), length(ddiff));
	//Emitter & Recever
	float2 E_R = max(float2(dot(-v.xyz,NormalsMap(texcoord+AB,3)), dot( v.xyz, reflect(normalize(float3(texcoord.xy*2-1,2)),normals) )),0.);
		   //E_R = pow(E_R,Roughness);
	float Global_Illumination = saturate(100.0 * E_R.x * E_R.y / ( PI * v.w * v.w + 100.0) );
	return Global_Illumination;
}
*/
float SubsurfaceScatteringFF(in float2 texcoord,in float3 ddiff,in float3 normals, in float2 AB)
{   //So normal and the vector between "Element to Element - Wrap Lighting."
	float4 v = float4(normalize(ddiff), length(ddiff));
	float LW = Wrap; //Emitter & Recever
	float2 ssE_R = saturate(float2(max(0, dot(-v.xyz,Normals_Depth(texcoord+AB,0).xyz)), max(0, dot( v.xyz, normals ) + LW) / (1 + LW)));
	float Scatter = saturate(100.0 * ssE_R.x * ssE_R.y / ( PI * v.w * v.w + 1.0) );
	return Scatter;
}

float ThiccnessFF(in float2 texcoord,in float3 ddiff,in float3 normals, in float2 AB)
{   //So normal and the vector between "Element to Element - Thiccness Approximation."
	float4 v = float4(normalize(ddiff), length(ddiff));
	//Emitter & Recever
	float2 fE_R = float2(1.0 - dot(-Normals_Depth(texcoord+AB, 1).xyz,v.xyz),  dot( normals, -v.xyz ) ); //flipped face is needed for generate local thiccness map.
	float Thicc = min(1.0,fE_R.x * fE_R.y * (1.0 - 1.0 / sqrt(rcp(v.w*v.w) + PI)));
	return Thicc;
}   int nonplus() { return T_01() == 0 || T_02() == 0 ? 1 : 0;}

void PCGI(float4 vpos : SV_Position, float2 texcoords : TEXCOORD, out float4 GlobalIllumination : SV_Target0, out float4 SubsurfaceScattering : SV_Target1)
{   float2 stexcoords = texcoords;
	texcoords /= GI_Res; int Samples = lerp(samples,4,1-tex2Dlod( PCGIcurrNormalsDepth,float4(texcoords,0,0)).w );
	#if VRS_MAP
		Samples = lerp(Samples,4,useVRS(texcoords, 1));
	#endif
	//Global Illumination Ray Length & Depth // * 0.125
	float depth = Normals_Depth( texcoords, 0 ).w, D = depth;// * 0.9992;
	float4 Noise = float4( MCNoise( framecount, stexcoords, 1 ), MCNoise( framecount, stexcoords, 2 ), MCNoise( framecount, stexcoords, 3 ), MCNoise( framecount, stexcoords, 4 ));	//Smoothing for AO not needed since this shader not going to use AO code above.//for GetPos * 0.990
	float4 random = normalize(Noise.xyzw * 2 - 1), GI, SS, PWH;//!Smooth ? 0 : 9 // sn = lerp(NormalsMap(texcoords,0),lerp(NormalsMap(texcoords,0),NormalsMap(texcoords,9),lerp(1.0,0.5,D)),0.9)
	float3 n = Normals_Depth( texcoords,0).xyz, p = GetPosition( texcoords) * 0.990, ddiff, ddiff_tc, ddiff_gi ,ddiff_gd, ddiff_ss, II_gi, II_gd, II_ss;
	float4 rl_gi_sss = float4( GIRL().x, GIRL().y, 75,lerp(1,125, Deep_Scattering ));
	//CheckerBoard Pattern Grab to save Perf
	float CB0 = MultiPattern( stexcoords.xy ).x, CB1 = Scattering ? CB0 || SkinDetection(tex2Dlod(PCGIcurrColor,float4(texcoords,0,2))) : 1; //|| SkinDetection(tex2D(BackBufferPCGI,texcoords))
	//Interlaced Scaling
	rl_gi_sss.xy *= MultiPattern(stexcoords.xy).y ? Alternate ? 1.0 : 0.5 : Alternate ? 0.05 : 0.10 ;
	float MaskDir = saturate( dot(float3(0,1,0),Normals_Depth(texcoords, 0).xyz) ), Diffusion = lerp(1.0,lerp(Reflectivness.y * 2,1.0,abs(Reflect())), MaskDir );
	rl_gi_sss.xy *= Reflect() < 0 ? Diffusion : MultiPattern(stexcoords.xy).y ? 1 : Diffusion;
	//Basic depth rescaling from Near to Far
	float D0 = smoothstep(-NCD.x,1, depth ), D1 = smoothstep(-1,1, depth ), D_Fade = PCGI_Fade < 0 ? lerp(1-Scale_PCGI_Fade() * 2,0, 1-D ) : smoothstep(0,2,D / Scale_PCGI_Fade()), MDCutOff = smoothstep(0,1,D) > MaxDepth_CutOff;//smoothstep(0,saturate(GI_Fade),D);
	float4 IGN = IGN_Toggle ? Interleaved_Gradient_Noise(stexcoords / pix / 2) * 2 - 1 : random;
	//SSS, GI, Gloss, and AO Form Factor code look above
	[fastopt] // Dose this even do anything better vs unroll? Compile times seem the same too me. Maybe this will work better if I use the souls I collect of the users that use this shader?
	for (int i = 0; i <= Samples; i++)
	{ 
		#if DX9 //DX 9 issues......
		if( clock == 0 || texcoords.x > 1.0 || texcoords.y > 1.0 )
			discard;
		#else //VRS and Max Depth Exclusion...... every ms counts.........
		if( MDCutOff || clock == 0 || texcoords.x > 1.0 || texcoords.y > 1.0 )
			break;
		#endif
		//Evenly distributed points on Poisson Disk.... But, with High Frequency noise.
		float2 GIWH = (pix * rl_gi_sss[0]) * IGN[0] * Rotate2D_A( PoissonTaps[i], random[3], texcoords) / D0,
			   //GDWH = (pix * rl_gi_sss[1]) * IGN[1] * Rotate2D_B( PoissonTaps[i], random[2]) / D0,
			   SSWH = (pix * rl_gi_sss[2]) * IGN[2] * Rotate2D_B( PoissonTaps[i], random[1]) / D1,
			   TCWH = (pix * rl_gi_sss[3]) * IGN[3] * Rotate2D_B( PoissonTaps[i], random[0]) / D1;
		//Recever to Emitter vector
			ddiff_tc = GetPosition( texcoords + TCWH) - p;
		//Thiccness Form Factor
		if(!CB1)
			SS.w += lerp(0,ThiccnessFF(texcoords, ddiff_tc, n, TCWH), SSSMasking( texcoords ));

		if(CB0)
		{
			//Recever to Emitter vector
			ddiff_gi = GetPosition( texcoords + GIWH) - p;
			//Irradiance Information
			II_gi = DirectLighting( texcoords + GIWH, 3 ).rgb * 1.125;
			//Radiance Form Factor
			GI.rgb += lerp(II_gi, 0, D_Fade) * RadianceFF(texcoords, ddiff_gi, n, GIWH, smoothstep(1,0,D));
		}

		if(!CB1)
		{   //Recever to Emitter vector
			ddiff_ss = GetPosition( texcoords + SSWH) - p;
			//Irradiance Information
			II_ss = BBColor( texcoords + SSWH, 3).rgb;
			//SubsurfaceScattering Form Factor
			//lerp( 0,lerp(II_ss, 0, N_F), SSSMasking( texcoords))
			SS.rgb += lerp( 0,lerp(II_ss, 0, D_Fade), SSSMasking( texcoords)) * SubsurfaceScatteringFF( texcoords, ddiff_ss, n, SSWH);
		}
	}
	float Samp = rcp(Samples);
	GI *= Samp; SS *= Samp;

	GlobalIllumination = float4( min( 1.0 , GI.rgb ) , GI.w);
	SubsurfaceScattering = min( 1.0 , float4(SS.rgb, SS.w * 2.0));
}

float4 GI_Adjusted(float2 TC, int Mip)
{
	float4 ConvertGI = tex2Dlod( PCGI_Info, float4( TC , 0, Mip)) , ConvertSS = tex2Dlod( PCSS_Info, float4( TC , 0, Mip));

	ConvertGI.xyz = RGBtoYCbCr(ConvertGI.xyz);
	ConvertGI.x *= 0.75;//GI_LumaPower.x;
	ConvertGI.xyz = Saturator_C(YCbCrtoRGB( ConvertGI.xyz));

	float DTMip = lerp(3,5,dot(BBColor(TC / GI_Res, 6.0).rgb,0.333)),DT = BBColor(TC / GI_Res, DTMip  ).w * 2.0, SSL = 1;//clamp(dot(BBColor(TC / GI_Res, 2.0).rgb,0.333) * lerp(1.0,4.0,User_SSS_Luma),1,2);
	float3 SSS = InternalFleshColor(ConvertSS.xyz * SSL, ConvertSS.w , lerp(0,lerp(0,10,Luma_Map),saturate(Internals.xyz)), DT );

	ConvertGI.xyz = MultiPattern( TC ).x ? ConvertGI.xyz *  min( GI_Power.x, 5.0) : Saturator_B(SSS) ;

	return float4( ConvertGI.xyz , 0) ;
}

void CBReconstruction(float4 vpos : SV_Position, float2 texcoords : TEXCOORD, out float4 UpGI : SV_Target0)
{  int MipLevel = 0;
   float4 tl = GI_Adjusted(texcoords,MipLevel);
   float4 tr = GI_Adjusted(texcoords + float2( pix.x, 0.0  ),MipLevel);
   float4 bl = GI_Adjusted(texcoords + float2( 0.0  , pix.y),MipLevel);
   float4 br = GI_Adjusted(texcoords + float2( pix.x, pix.y),MipLevel);

   float2 f = frac( texcoords * float2(BUFFER_WIDTH, BUFFER_HEIGHT ) );

   float4 tA = lerp( tl, tr, f.x );
   float4 tB = lerp( bl, br, f.x );

	UpGI = lerp( tA, tB, f.y ) * 1.25;//GI_Adjusted(texcoords,0);
}

float3 GI(float2 TC, float Mips)
{
	#line 4 "For the latest version go https://blueskydefender.github.io/AstrayFX/ or http://www.Depth3D.info ø"
	#warning " ¬T ¬A ¬M ¬P ¬E ¬R ¬E ¬D "
	float3 GI_Out = tex2Dlod( PCGIReconstruction_Info, float4( TC * GI_Res , 0, Mips)).xyz ;
	return GetPos() ? GI_Out * TC.xyx : GI_Out;
}   float  Helper() { float Temp_Location = T_01() == 12500 ? 0 : 1 ; return Temp_Location;}

float4 GI_TAA(float4 vpos : SV_Position, float2 texcoords : TEXCOORD) : SV_Target
{   //Depth Similarity
	float M_Similarity = 0.5, D_Similarity = saturate(pow(abs(tex2D(PCGIpastFrame,texcoords).x/Normals_Depth(texcoords, 0).w), 10) + M_Similarity);
	//Velocity Scaler
	//float S_Velocity = 12.5 * lerp( 1, 80,TAA_Clamping), V_Buffer = saturate(abs(DepthMap(texcoords, 0)-tex2D(PCGIpastFrame,texcoords).w )* S_Velocity);
	//Accumulation buffer Start
	//Resolution Scale & GI_Power
	float ReSRes = Scale(GI_Res,1.0,0.5), power = GI_Power >= 1 ? Scale(GI_Power,1.0,2.5) : 1, Res_Scale = lerp(1.0,0.0,ReSRes);
	float3 GISamples, CurrAOGI = GI( texcoords, Debug == 1 ? 0 : Res_Scale).rgb, MB = 0;
	float3 minColor = CurrAOGI - MB;
	float3 maxColor = CurrAOGI + MB;
	[unroll]
	for(int i = 0; i < 8; ++i)
	{
		float2 Offset = XYoffset[i] * 4;//Use this on color image first.
		GISamples = GI( texcoords + Offset , Res_Scale ).rgb;
		minColor = min( minColor, GISamples) - MB;
		maxColor = max( maxColor, GISamples) + MB;
	}
	//Insert your motion buffer here...... Also leaving this here so when ReShade or a cleaver Dev can make his own.
	//float2 Motion_Buffer = tex2D(OpticalFlow::sMotion5, texcoords).xy / float2(BUFFER_WIDTH, BUFFER_HEIGHT);

	float2 PastTexcoords = texcoords;// + Motion_Buffer;
	// this is done with a ACC Buffer in reshade. But, you may want to create your own PastFrames
	//Min Max neighbourhood clamping.
	float3 Past = clamp( tex2Dlod( PCGIaccuFrames, float4(PastTexcoords, 0, 0) ).rgb, minColor, maxColor);
	//float mixRate = min( tex2Dlod( PCGIaccuFrames, float4(texcoords, 0, 0) ).w, 0.5);

	// Simple AB clamping used for mixing
	//float2 A = PastTexcoords > 1., B = PastTexcoords < 0.;
	//TAA Mixing for real motion buffer.......
	// float Mixing = any(float2(any(A), any(B))) ? 1 : 0;

	//float diff = length(antialiased - preclamping) * 4;//Alternet way of doing it
	//Added Velocity Clamping.......
	float3 clampAmount = abs(Past - CurrAOGI); //V_Buffer;//For AO But,I Droped this and use MixRate;
	clampAmount.x = saturate(dot(clampAmount, clampAmount));

	float Mixing = saturate(lerp( lerp(0.02,0.2,ReSRes) , lerp(0.2,0.5, power ) , clampAmount.x));// Use mixRate or AB Clamping for Mixing.....
	//Simple Blending
	float3 AA = lerp(Past, CurrAOGI, Mixing );
	//Sample from Accumulation buffer, with mix rate clamping.
	AA = lerp( 0 , AA, D_Similarity);
	return float4( AA, 1.0) ;
}

void AccumulatedFramesGI(float4 vpos : SV_Position, float2 texcoords : TEXCOORD, out float4 acc : SV_Target)
{
	acc = tex2D(BackBufferPCGI,texcoords).rgba;
}
//Horizontal Denoising Upscaling
float4 BGU_Hoz(float4 position : SV_Position, float2 texcoords : TEXCOORD) : SV_Target
{
	return float4( Denoise( PCGIaccuFrames          , texcoords, EvenSteven[clamp(SamplesXY,0,20)], 0, 2.5 ).rgb, 0);
}
//Vertical Denoising Upscaling
float4 BGU_Ver(float4 position : SV_Position, float2 texcoords : TEXCOORD) : SV_Target
{
	return float4( Denoise( PCGI_BGUHorizontal_Sample, texcoords, EvenSteven[clamp(SamplesXY,0,20)], 1, 2.5).rgb, 0);
}

float3 ControlledBlend(float3 Color, float3 Cloud)
{
	float3 FiftyGray = Cloud + 0.490;
	#if Controlled_Blend
		return add( lerp( overlay( Color,  FiftyGray), softlight( Color,  FiftyGray),Blend),Cloud * 0.1875);
	#else
		return add( lerp( overlay( Color,  FiftyGray), softlight( Color,  FiftyGray), 0.5 ),Cloud * 0.1875);
	#endif
}

float3 Composite(float3 Color, float3 Cloud)
{
	float3 FiftyGray = Cloud + 0.490;
	//Left for Rework
	//FiftyGray = Shadows(FiftyGray);

	if(Blend == 0)
		return ControlledBlend( Color,  Cloud);
	else if(Blend == 1)
		return overlay( Color, FiftyGray);
	else if(Blend == 2)
		return softlight( Color, FiftyGray);
	else
		return add( Color, Cloud );
}

float3 Mix(float2 texcoords)
{
	return Composite(tex2Dlod(PCGIcurrColor,float4(texcoords,0,0)).rgb , tex2Dlod(PCGI_BGUVertical_Sample,float4(texcoords,0,0)).rgb) ;
}   float Text_Info_Plus() { return nonplus() ? Alternate : Text_Info; }
//Diffusion Blur - Man....... Talk about strange....
float3 DiffusionBlur(float2 texcoords)
{
	float3 Layer,total;
	float ML = SSSMasking(texcoords).x,
		  SD = 1-SkinDetection(tex2Dlod(PCGIcurrColor,float4(texcoords,0,2)) ),
		  NormalBlurFactor = 1.0f,
		  DepthBlurFactor = 0.009f;

	float4 StoredNormals_Depth = Normals_Depth( texcoords, 0);

	if(SD && Scattering && ML && Diffusion_Saturation_Power.x > 0 )
	{
		[loop]
		for(int i = 0; i < 7; ++i)
		{
			if(i > 7 || smoothstep( 0, 1, DepthMDC(texcoords, 0)))
				break;

			float2 offsetxy = texcoords + float2( DBoffsets[i], 0) * pix * lerp( 0.0, 2.0,saturate(Diffusion_Saturation_Power.x));
			float3 CMix =  Mix( offsetxy ).rgb;

				float4 ModifiedNormals_Depth = Normals_Depth( offsetxy, 2);//Use lower mip level here on finnished product.
				float ModN = length(StoredNormals_Depth.xyz - ModifiedNormals_Depth.xyz), ModD = saturate( StoredNormals_Depth.w - ModifiedNormals_Depth.w);

			float D_Dist2 = max(ModD, 0.0), d_w = min(exp(-(D_Dist2)/DepthBlurFactor), 1.0);
		    float N_Dist2 = max(ModN, 0.0), n_w = min(exp(-(N_Dist2)/NormalBlurFactor), 1.0);


			float3 Weight = DBweight[i];
				   Weight *= d_w;
				   Weight *= n_w;
				   Layer += CMix * Weight;
				   total += Weight;
		}
		return Layer / total;
	}
	else
		return Mix(texcoords).rgb;
}

#define DScale -1
float D_Scale(float Input)
{
	return (Input-DScale)/(1-DScale);
}

float4 MixOut(float2 texcoords)
{
	//float2 Grid = floor( texcoords * float2(BUFFER_WIDTH, BUFFER_HEIGHT ) * pix * 125);//125 lines				   
	float  Depth = D_Scale(Normals_Depth(texcoords,0).w), AO_Scale = 0.5,FakeAO = Debug == 1 ? (
				   Normals_Depth(texcoords ,1).w +			   
				   Normals_Depth(texcoords ,2).w +
				   Normals_Depth(texcoords ,3).w +
				   Normals_Depth(texcoords ,4).w +
				   Normals_Depth(texcoords ,5).w +
				   Normals_Depth(texcoords ,6).w +
				   Normals_Depth(texcoords ,7).w +
				   Normals_Depth(texcoords ,8).w +
				   Normals_Depth(texcoords ,9).w +
				   Normals_Depth(texcoords ,10).w
					) * 0.1: 0.00000001;		   
					FakeAO = (1-Depth/D_Scale(FakeAO) * AO_Scale ) * (1+AO_Scale);
					
	float  Guide = Depth_Guide ? ( Depth +
				   				Normals_Depth(texcoords + float2( pix.x * 2.5, 0), 1 ).w +
				   				Normals_Depth(texcoords + float2(-pix.x * 2.5, 0), 2 ).w +
				   				Normals_Depth(texcoords + float2( 0, pix.y * 2.5), 3 ).w +
				   				Normals_Depth(texcoords + float2( 0,-pix.y * 2.5), 4 ).w ) * 0.2 : 0.00000001;
				   
	float3 Output = tex2D( PCGI_BGUVertical_Sample, texcoords).rgb, Layer = DiffusionBlur( texcoords ); float4 Done = float4(Layer,0.5);

	if(Debug == 0)
		Done.rgb = Depth_Guide ? Layer.rgb * float3((Depth/Guide> 0.998),1,(Depth/Guide > 0.998))  : Layer.rgb;
	else if(Debug == 1)//(1-Depth/FakeAO * 0.75)  ;
		Done.rgb = lerp(lerp(smoothstep(0.5,1,(Output * 0.5) + FakeAO),1,smoothstep(0,1,Depth) == 1),Output,Dark_Mode); //This fake AO is a lie..........
	else if(Debug == 2)
		Done.rgb = DirectLighting( texcoords, 0).rgb;
	else
		Done.rgb = texcoords.x + texcoords.y < 1 ? Normals_Depth(texcoords, 0 ).w :  Normals_Depth(texcoords,0).xyz * 0.5 + 0.5;

	return Done;
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
	float4 Color = MixOut(texcoord);

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

float4 BackBufferCG(float2 texcoords)
{   float4 C = tex2D(BackBufferPCGI,texcoords);
	float GS = dot(C.rgb,0.333);
	return float4(C.rgb,GS);
}

float2 PackNormals(float3 n)
{
    float f = rsqrt(8*n.z+8);
    return n.xy * f + 0.5;
}

void CurrentFrame(float4 vpos : SV_Position, float2 texcoords : TEXCOORD, out float4 Color : SV_Target0, out float4 NormalsDepth : SV_Target1)
{
	float4 BBCG = BackBufferCG(texcoords);
	float DI = Depth_Info(texcoords), LRBLM = saturate( dot( Ublur( PCGIcurrColor, texcoords, 5, 100) ,0.333) );
	Color = float4(BBCG.rgb,max(0.0, BBCG.w - lerp(0,1,saturate(Luma_Map))));//,0, smoothstep(0,1,DI) > MaxDepth_CutOff ) ;
	//Packed Normals / Depth / Large Radius Blured Luma Mask
	NormalsDepth = float4(PackNormals(DepthNormals(texcoords)),DI,LRBLM);
}

void PreviousFrame(float4 vpos : SV_Position, float2 texcoords : TEXCOORD, out float4 prev : SV_Target)
{	//float PD = dot(tex2D(PCGI_BGUVertical_Sample ,texcoords).rgb,0.333);
	prev = Normals_Depth(texcoords, 0).w;//float4(0,0,0,Normals_Depth(texcoords, 0).w);
}
/* // Saved for Next Release
float4 GI_Storage(float4 position : SV_Position, float2 texcoords : TEXCOORD) : SV_Target
{
	return (tex2D(BackBufferPCGI,0.5).w * 255) == 127 ?  1.0 : 0;
}
*/
//////////////////////////////////////////////////////////Reshade.fxh/////////////////////////////////////////////////////////////
// Vertex shader generating a triangle covering the entire screen
void PostProcessVS(in uint id : SV_VertexID, out float4 position : SV_Position, out float2 texcoord : TEXCOORD)
{
	texcoord.x = (id == 2) ? 2.0 : 0.0;
	texcoord.y = (id == 1) ? 2.0 : 0.0;
	position = float4(texcoord * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
}
//*Rendering passes*//
#if Simp_Mode
technique PCGI_One
< toggle = 0x2E; ui_label = "RadiantGI";
ui_tooltip = "RadiantGI: Point Cloud Global Illumination."; >
#else
technique PCGI_One
< toggle = 0x2E; ui_label = "PCGI¬π";
ui_tooltip = "Alpha: Global Illumination Primary Generator.¬π"; >
#endif
{
		pass PastFrame
	{
		VertexShader = PostProcessVS;
		PixelShader = PreviousFrame;
		RenderTarget = PCGIpastTex;
	}
		pass CopyFrame
	{
		VertexShader = PostProcessVS;
		PixelShader = CurrentFrame;
		RenderTarget0 = PCGIcurrColorTex;
		RenderTarget1 = PCGIcurrNormalsDepthTex;
	}
		pass SSGI
	{
		VertexShader = PostProcessVS;
		PixelShader = PCGI;
		RenderTarget0 = RadiantGITex;
		RenderTarget1 = RadiantSSTex;
	}
		pass Reconstruction
	{
		VertexShader = PostProcessVS;
		PixelShader = CBReconstruction;
		RenderTarget = PCGIReconstructionTex;
	}
		pass TAA
	{
		VertexShader = PostProcessVS;
		PixelShader = GI_TAA;
	}
		pass AccumilateFrames
	{
		VertexShader = PostProcessVS;
		PixelShader = AccumulatedFramesGI;
		RenderTarget = PCGIaccuTex;
	}
#if Simp_Mode
// Simple Mode
#else
}

technique PCGI_Two
< toggle = 0x2E; ui_label = "PCGI¬≤";
ui_tooltip = "Beta: Global Illumination Secondary Output.¬≤"; >
{

#endif
		pass Edge_Avoiding_Denoiser_H
	{
		VertexShader = PostProcessVS;
		PixelShader = BGU_Hoz;
		RenderTarget = PCGIHorizontalTex;
	}
		pass Edge_Avoiding_Denoiser_V
	{
		VertexShader = PostProcessVS;
		PixelShader = BGU_Ver;
		RenderTarget = PCGIVerticalTex;
	}
		pass Done
	{
		VertexShader = PostProcessVS;
		PixelShader = Out;
	}
	/* // Saved for next release
		pass GIStorage
	{
		VertexShader = PostProcessVS;
		PixelShader = GI_Storage;
		//RenderTarget = GI_StoredTex;
	}
	*/
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                       Update Notes
//
// Update 1.2
// This update did change a few things here and there like PCGI_Alpha and PCGI_Beta are now PCGI_One and PCGI_Two.
// Error Code edited so it does not show the warning anymore. But, it's still shows the shader name as yellow.¬Ø\_('_')_/¬Ø
// This version has a small perf loss. Also removed some extra code that was not mine. Forgot to replace it my bad.....
// TAA now gives The JBGU shader motion information to blur in motion so there is less noise in motion. :)
//
// Update 2.0
// So reworked the Joint Bilateral Gaussian Upscaling for sharper Debug and fewer artifacts in motion. Now I am using a plain
// old box blur it seems to run faster and look sharper at masking high frequency noise. Also reworked how to sample the GI.
// Radiant Global Illumination now uses a proper Poisson Disk Sampling. This kind of sampling should improve the accuracy of GI displayed.
// Shader motion information removed for now to get this out by the deadline around Xmas time. Some Defaults and max setting were changed.
// Subsurface Scattering AKA Subsurface Light Transport Added. This feature is heavy. But, should be doable to find/look for Flesh in an image.
// Once found it will try to add SSS with Diffusion Blur to give skin that soft look to it. This was a lot harder than expected...............
// It Just works well most of the time...... Just issues come up with some games with earthy tones. I will allow an option to apply SSS to everything later.
// I was also able to do thickness estimation this lets things like ears and other thin/dangly parts to allow Deep Tissue Scattering. So Deep.....
// So all in all this was capable of being done. The Thickness estimation may cause issues on White objects that trigger the Skin Detection so be warned.
// This shader not done. But, it is close.
//
// Update 2.1
// Small changes to help guide the user a bit more with SSLT.
//
// Update 2.2
// More Pooled Texture issues. Black screen is shown when Pooling some textures. Issue now is that this shader uses around 605.0 MiB at 4k......
// Need to talk to the main man about this. When time allows.
//
// Update 2.3
// Added a way to control the Subsurface Scattering Brightness.
//
// Update 2.4
// Added a way to control Diffusion profile for added Reflectivness. Notices a bad bug with TAA not working in some game I need to find out why.
//
// Update 2.5
// Changed the way I am doing my 2nd Denoiser. Also reworked the Saturation system and the HDR extraction.
// Targeted lighting works as intended now. Indirect and Direct world color sampling.
// RadiantGI now has a pure GI mode that runs 2x times as slow but, gives 2x times more control.
// This mode disables SSLT. So keep that in mind.
//
// Update 2.6
// Added a way to force pooling off for 4.9.0+ and small perf boost.
//
// Update 2.7
//
// Improved Normal smoothing on my 2nd Denoiser and looser setting on my TAA solution should get most of the noise in motion.
// Tested an Edge-Avoiding √Ä-Trous Wavelet Transform Denoiser that was  "Fast." But, My own solution was faster and got rid of more noise.
// All I can say is that I learned a lot about 2nd level Denoisers. I think I will be ready to may my own AO shader now.
// For now enjoy this update.
//
// TLDR: Better Normal smoothing and noise removal in motion.
//
// Update 2.8
//
// I targeted Diffusion to work on the floors only. Because having it affect everything was not correct to me. I also Made the Debug View a bit cleaner.
// With The cleaner Debug you  will see more detail. But, It will not cause the final output to show normal edges even if you see them in the Debug View.
// I also made the De-Noiser go up to 20 now so........ Ya...... At any rate it better now!!!!!
//
// Update 2.9
//
// So Small Performance Boost over old way by taking in to account Depth Fade. Also, some other code adjustments and moved the Update Notes section down to the
// bottom of the shader. Now I am using a extra Buffer for SSS and allow for RadiantGI Internal Resolution to be changed in UI so no more need to go into the
// PreProcessors to-do it. Fixed the Blue hue Bug in SSLT should more accurately portray skin color. Cleaned up the Noise a bit better this time around so that
// nothing short of reworking my TAA will help now. Sill would not mind help on that BTW.
//
// Sub-Update 2.9.1
//
// Bug fix Low light issue. Shader will no longer generate rays in low light situations. Rays went Burrrrrrrrrrrrr too much needed and to cap it.
// Ya,too much Burrrrrrr.
//
// Sub-Update 2.9.2
//
// Light Source Map was added a Debug mode for Targeted Lighting. This is so you can see what is getting used as lighting information.
// Change use Transmission from ReVeil to default to "False" also changed Default End toggle = 0x23 from to Delete toggle = 0x2E For the next version of ReShade
//
// Sub-Update 2.9.3
//
// Targeted Lighting now allows for Direct Lighting and Indirect Lighitng calibrations.
//
// Sub-Update 2.9.4
//
// Fixed a small issue with the shader. Swizzle me this.........
//
// Sub-Update 2.9.5
//
// Default Key Change for debug menu from F11 to the Menu Key.........
//
// Update 2.9.6
//
// Rewrote my TAA implementation so if there is a update for ReShade that can give us real motion buffer It can be easy addition to the current TAA.
//
// Update 2.9.7
//
// So Made every thing a little faster. But, also changed many default setting and added correct staturation control.I also changed the way denoise works with the image.
// Added support for Lord of Lunacy VRS "Variable Rate Shading." VRS works better the more samples you use.Sadly VRS auto lower res areas don't work due to the newly added
// checkerboard reconstruction.This Preping it rework for next release. Removed Pure GI mode since it complicated things.Added Sky Color Contribution. Lets the sky scatter
// on to the world based on Direction. I do hope this update make things better to use. I also tried to make SSS easier by automating luminace clipping.
//
// For the next release I will be reworking the Denoiser / TAA 100% Thank you Robloxian AKA Clown Comp AKA Brimsion on GitHubs on the interwebs.
//
// Update 2.9.7
//
// Mini Update Adjusting the Denoiser so it gives a sharper image out.Thank you Lord of Lunacy.
//
// Update 2.9.8
//
// Perf update based on a blured Luma Map. This uses lower samples in areas that are darker. Temporal Mask was also added to TAA. It replace the old Mixing system.
// This was done too reduce the noise in the final image. I also made it sample from Lower MipLeves when lowering Resolution. This seems to reduce noise overall.
// I also adjusted the color system and the Luma Power options too automaticly increase the TAA accumulation. This seems to be at the veary limit of what
// my TAA filter can do. I got to admit my 2nd Level Denoiser needs work. But, for now I leave it be. A option was added to adjust for distance irradiance.
//
// Update 2.9.9
//
// Perf update Texture format change and removed render target. Also added simple mode for new users as an default setting. This change comes after seeing new users struggle
// too use my shader for the first time. I also changed the tag PCGI_One and PCGI_Two too PCGI¬π and PCGI¬≤. Also in simple mode the tag is changed too RadiantGI. I do hope
// that this change helps users with lower end hardware as well. Shout out to lelfarian moral support and Gordinho for being there I think.
//
// Update 3.0.0
//
// Emissive Mode added! It lets the shader make all objects emissive based on approximated information from the emitter's luminace information. This adjusted Form Factor can
// be used to allow emissive light mirror what on the opposite side. Simple and effective option for older games. Also added back Trimming that was removed back in an later update.
// The two options do effect Directional Sky Color. But, mitagations where added to help prevent this.
//
// Update 3.0.2
//
// DX 9 workaround................ it works.... I think.
//
// Update 3.0.3
//
// DX 9 Sky fixed: No More Ghosts
// Added Interleaved Gradient Noise as default on option. Gives the GI a smoother look when denoising. Because of this I lowered default Defnoise power to 6...
// Also added an Experimental Texture Details option.
// Now even your crevices can have GI................
//
// Update 3.0.4
//
// Simplified RadiantGI setting and recategorized many of them to make the shader easier to use. This update also makes Enhanced Texture Details a standard for this shader.
// Supplemental contributions has the extra setting needed for enhanced features. 
// Moved Resolution scaling to the Extra Options area.
//
// Update 3.0.5
//
// Simplified RadiantGI setting even more and tried to make the lighting more accurate. I removed luma power and left normal power there since was almost the same thing.
//
// Update 3.0.6
//
// Made the sampling radius change every other frame to grab a larger sample base. So that the TAA can blend them and provide a more accurate / better GI output.
// It been fun learning on this shader. I do hope you enjoy this update.
//
// Update 3.0.7
//
// Simple HDR adjustment. Made sure it works with HDR output. PCGI Fade now works the same as GloomAO. Since I need the shaders to have parity.
//
// Update 3.0.8
//
// Set the default for Trim to off and adjusted a few things. Changed the UI a little bit to make it easier on new users.
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
