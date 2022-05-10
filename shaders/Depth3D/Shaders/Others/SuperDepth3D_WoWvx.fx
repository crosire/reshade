////----------------------//
///**SuperDepth3D_WoWvx**///
//----------------------////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//* Depth Map Based 2D + 3D Shader v3.0.0
//* For Reshade 3.0+
//* ---------------------------------
//*
//* Original work was based on the shader code by me. The WoWvx stuff was reversed engineered I never had access to the original code.
//* Text rendering code Ported from https://www.shadertoy.com/view/4dtGD2 by Hamneggs to ReshadeFX used for error codes.
//* If I missed any information please contact me so I can make corrections.
//*
//* LICENSE
//* ============
//* Overwatch Interceptor & Code out side the work of people mention above is licenses under: Copyright (C) Depth3D - All Rights Reserved
//*
//* Unauthorized copying of this file, via any medium is strictly prohibited
//* Proprietary and confidential.
//*
//* You are allowed to obviously download this and use this for your personal use.
//* Just don't redistribute this file unless I authorize it.
//*
//* Have fun,
//* Written by Jose Negrete AKA BlueSkyDefender <UntouchableBlueSky@gmail.com>, December 2019
//*
//* Please feel free to contact me if you want to use this in your project.
//* https://github.com/BlueSkyDefender/Depth3D
//* http://reshade.me/forum/shader-presentation/2128-sidebyside-3d-depth-map-based-stereoscopic-shader
//* https://discord.gg/Q2n97Uj
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#if exists "Overwatch.fxh"                                           //Overwatch Interceptor//
	#include "Overwatch.fxh"
	#define OSW 0
#else// DA_X = [ZPD] DA_Y = [Depth Adjust] DA_Z = [Offset] DA_W = [Depth Linearization]
	static const float DA_X = 0.025, DA_Y = 7.5, DA_Z = 0.0, DA_W = 0.0;
	// DB_X = [Depth Flip] DB_Y = [Auto Balance] DB_Z = [Auto Depth] DB_W = [Weapon Hand]
	static const float DB_X = 0, DB_Y = 0, DB_Z = 0.1, DB_W = 0.0;
	// DC_X = [Barrel Distortion K1] DC_Y = [Barrel Distortion K2] DC_Z = [Barrel Distortion K3] DC_W = [Barrel Distortion Zoom]
	static const float DC_X = 0, DC_Y = 0, DC_Z = 0, DC_W = 0;
	// DD_X = [Horizontal Size] DD_Y = [Vertical Size] DD_Z = [Horizontal Position] DD_W = [Vertical Position]
	static const float DD_X = 1, DD_Y = 1, DD_Z = 0.0, DD_W = 0.0;
	// DE_X = [ZPD Boundary Type] DE_Y = [ZPD Boundary Scaling] DE_Z = [ZPD Boundary Fade Time] DE_W = [Weapon Near Depth Max]
	static const float DE_X = 0, DE_Y = 0.5, DE_Z = 0.25, DE_W = 0.0;
	// DF_X = [Weapon ZPD Boundary] DF_Y = [Separation] DF_Z = [ZPD Balance] DF_W = [HUD]
	static const float DF_X = 0.0, DF_Y = 0.0, DF_Z = 0.125, DF_W = 0.0;
	// DG_X = [Special Depth X] DG_Y = [Special Depth Y] DG_Z = [Weapon Near Depth Min] DG_W = [Check Depth Limit]
	static const float DG_X = 0.0, DG_Y = 0.0, DG_Z = 0.0, DG_W = 0.0;
	// DH_X = [LBC Size Offset X] DH_Y = [LBC Size Offset Y] DH_Z = [LBC Pos Offset X] DH_W = [LBC Pos Offset X]
	static const float DH_X = 1.0, DH_Y = 1.0, DH_Z = 0.0, DH_W = 0.0;
	// DI_X = [LBM Offset X] DI_Y = [LBM Offset Y] DI_Z = [Weapon Near Depth Trim] DI_W = [REF Check Depth Limit]
	static const float DI_X = 0.0, DI_Y = 0.0, DI_Z = 0.25, DI_W = 0.0;
	// DJ_X = [NULL X] DJ_Y = [NULL Y] DJ_Z = [NULL Z] DJ_W = [Check Depth Limit Weapon]
	static const float DJ_X = 0.0, DJ_Y = 0.0, DJ_Z = 0.25, DJ_W = -0.100;	
	// DK_X = [FPS Focus Method] DK_Y = [Eye Eye Selection] DK_Z = [Eye Fade Selection] DK_W = [Eye Fade Speed Selection]	
	static const float DK_X = 0, DK_Y = 0.0, DK_Z = 0, DK_W = 0;	
	// WSM = [Weapon Setting Mode]
	#define OW_WP "WP Off\0Custom WP\0"
	static const int WSM = 0;
	//Triggers
	static const int REF = 0, NCW = 0, RHW = 0, NPW = 0, IDF = 0, SPF = 0, BDF = 0, HMT = 0, DFW = 0, NFM = 0, DSW = 0, BMT = 0, LBC = 0, LBM = 0, DAA = 0, NDW = 0, PEW = 0, WPW = 0, FOV = 0, EDW = 0, SDT = 0;
	//Overwatch.fxh State
	#define OSW 1
#endif
//USER EDITABLE PREPROCESSOR FUNCTIONS START//

// Zero Parallax Distance Balance Mode allows you to switch control from manual to automatic and vice versa.
#define Balance_Mode 1 //Default 0 is Automatic. One is Manual.

// RE Fix is used to fix the issue with Resident Evil's 2 Remake 1-Shot cutscenes.
#define RE_Fix 0 //Default 0 is Off. One is High and Ten is Low        1-10

// Change the Cancel Depth Key. Determines the Cancel Depth Toggle Key using keycode info
// The Key Code for Decimal Point is Number 110. Ex. for Numpad Decimal "." Cancel_Depth_Key 110
#define Cancel_Depth_Key 0 // You can use http://keycode.info/ to figure out what key is what.

// Rare Games like Among the Sleep Need this to be turned on.
#define Invert_Depth 0 //Default 0 is Off. One is On.

// Barrel Distortion Correction For SuperDepth3D for non conforming BackBuffer.
#define BD_Correction 0 //Default 0 is Off. One is On.

// Horizontal & Vertical Depth Buffer Resize for non conforming DepthBuffer.
// Also used to enable Image Position Adjust is used to move the Z-Buffer around.
#define DB_Size_Position 0 //Default 0 is Off. One is On.

// Auto Letter Box Correction
#define LB_Correction 0 //[Zero is Off] [One is Auto Hoz] [Two is Auto Vert]
// Auto Letter Box Masking
#define LetterBox_Masking 0 //[Zero is Off] [One is Auto Hoz] [Two is Auto Vert]

// Specialized Depth Triggers
#define SD_Trigger 0 //Default is off. One is Mode A other Modes not added yet.

// HUD Mode is for Extra UI MASK and Basic HUD Adjustments. This is useful for UI elements that are drawn in the Depth Buffer.
// Such as the game Naruto Shippuden: Ultimate Ninja, TitanFall 2, and or Unreal Gold 277. That have this issue. This also allows for more advance users
// Too Make there Own UI MASK if need be.
// You need to turn this on to use UI Masking options Below.
#define HUD_MODE 0 // Set this to 1 if basic HUD items are drawn in the depth buffer to be adjustable.

// -=UI Mask Texture Mask Interceptor=- This is used to set Two UI Masks for any game. Keep this in mind when you enable UI_MASK.
// You Will have to create Three PNG Textures named DM_Mask_A.png & DM_Mask_B.png with transparency for this option.
// They will also need to be the same resolution as what you have set for the game and the color black where the UI is.
// This is needed for games like RTS since the UI will be set in depth. This corrects this issue.
#if ((exists "DM_Mask_A.png") || (exists "DM_Mask_B.png"))
	#define UI_MASK 1
#else
	#define UI_MASK 0
#endif
// To cycle through the textures set a Key. The Key Code for "n" is Key Code Number 78.
#define Set_Key_Code_Here 0 // You can use http://keycode.info/ to figure out what key is what.
// Texture EX. Before |::::::::::| After |**********|
//                    |:::       |       |***       |
//                    |:::_______|       |***_______|
// So :::: are UI Elements in game. The *** is what the Mask needs to cover up.
// The game part needs to be transparent and the UI part needs to be black.

// The Key Code for the mouse is 0-4 key 1 is right mouse button.
#define Cursor_Lock_Key 4 // Set default on mouse 4
#define Fade_Key 1 // Set default on mouse 1
#define Fade_Time_Adjust 0.5625 // From 0 to 1 is the Fade Time adjust for this mode. Default is 0.5625;

// Delay Frame for instances the depth bufferis 1 frame behind useful for games that need "Copy Depth Buffer
// Before Clear Operation," Is checked in the API Depth Buffer tab in ReShade.
#define D_Frame 0 //This should be set to 0 most of the times this will cause latency by one frame.

//Text Information Key Default Menu Key
#define Text_Info_Key 93

//This is to enable manual control over the Lens angle in degrees uses this to set the angle below for "Set_Degrees"
#define Lenticular_Degrees 0
#define Set_Degrees 12.5625 //This is set to my default and may/will not work for your screen.

//USER EDITABLE PREPROCESSOR FUNCTIONS END//
#if !defined(__RESHADE__) || __RESHADE__ < 40000
	#define Compatibility 1
#else
	#define Compatibility 0
#endif

#if __RESHADE__ >= 40300
	#define Compatibility_DD 1
#else
	#define Compatibility_DD 0
#endif
//FreePie Compatibility
#if __RESHADE__ >= 40600
	#if __RESHADE__ > 40700
		#define Compatibility_FP 2
	#else
		#define Compatibility_FP 1
	#endif
#else
	#define Compatibility_FP 0
#endif

//Flip Depth for OpenGL and Reshade 5.0 since older Profiles Need this.
#if __RESHADE__ >= 50000 && __RENDERER__ >= 0x10000 && __RENDERER__ <= 0x20000
	#define Flip_Opengl_Depth 1
#else
	#define Flip_Opengl_Depth 0
#endif

#if __VENDOR__ == 0x10DE //AMD = 0x1002 //Nv = 0x10DE //Intel = ???
	#define Ven 1
#else
	#define Ven 0
#endif
//Resolution Scaling because I can't tell your monitor size.
#if (BUFFER_HEIGHT <= 720)
	#define Max_Divergence 25.0
	#define Set_Divergence 12.5	
#elif (BUFFER_HEIGHT <= 1080)
	#define Max_Divergence 50.0
	#define Set_Divergence 25.0
#elif (BUFFER_HEIGHT <= 1440)
	#define Max_Divergence 75.0
	#define Set_Divergence 37.5
#elif (BUFFER_HEIGHT <= 2160)
	#define Max_Divergence 100.0
	#define Set_Divergence 50.0
#else
	#define Max_Divergence 125.0//Wow Must be the future and 8K Plus is normal now. If you are hear use AI infilling...... Future person.
	#define Set_Divergence 62.5
#endif                          //With love <3 Jose Negrete..
//New ReShade PreProcessor stuff
#if UI_MASK
	#ifndef Mask_Cycle_Key
		#define Mask_Cycle_Key Set_Key_Code_Here
	#endif
#else
	#define Mask_Cycle_Key Set_Key_Code_Here
#endif
//Divergence & Convergence//
uniform int Content <
 ui_type = "combo";
 ui_items = "NoDepth\0Reserved\0Still\0CGI\0Game\0Movie\0Signage\0";
 ui_label = "·Content Type·";
 ui_tooltip = "Content Type for WOWvx.";
 	ui_category = "WoWvx Settings";
> = 0;

uniform bool Signage_Flip <
	ui_label = " Signage Position";
	ui_tooltip = "Set Signage Upper Left or Bottom Left.";
	ui_category = "WoWvx Settings";
> = false;

uniform int Perspective <
	ui_type = "drag";
	ui_min = -100; ui_max = 100;
	ui_label = " Perspective Slider";
	ui_tooltip = "Determines the perspective point of the two images this shader produces.\n"
				 "For an HMD, use Polynomial Barrel Distortion shader to adjust for IPD.\n"
				 "Do not use this perspective adjustment slider to adjust for IPD.\n"
				 "Default is Zero.";
	ui_category = "Stereoscopic Options";
> = 0;

uniform float2 ZPD_Separation <
	ui_type = "drag";
	ui_min = 0.0; ui_max = 0.250;
	ui_label = " ZPD & Sepration";
	ui_tooltip = "Zero Parallax Distance controls the focus distance for the screen Pop-out effect also known as Convergence.\n"
				"Separation is a way to increase the intensity of Divergence without a performance cost.\n"
				"For FPS Games keeps this low Since you don't want your gun to pop out of screen.\n"
				"Default is 0.025, Zero is off.";
	ui_category = "Stereoscopic Options";
> = float2(DA_X,0.0);

#if Balance_Mode || BMT
uniform float ZPD_Balance <
	ui_type = "drag";
	ui_min = 0.0; ui_max = 1.0;
	ui_label = " ZPD Balance";
	ui_tooltip = "Zero Parallax Distance balances between ZPD Depth and Scene Depth.\n"
				"Default is Zero is full Convergence and One is Full Depth.";
	ui_category = "Stereoscopic Options";
> = DF_Z;

static const int Auto_Balance_Ex = 0;
#else
uniform int Auto_Balance_Ex <
	#if Compatibility
	ui_type = "drag";
	#else
	ui_type = "slider";
	#endif
	ui_min = 0; ui_max = 5;
	ui_label = " Auto Balance";
	ui_tooltip = "Automatically Balance between ZPD Depth and Scene Depth.\n"
				 "Default is Off.";
	ui_category = "Stereoscopic Options";
> = DB_Y;
#endif
uniform int ZPD_Boundary <
	ui_type = "combo";
	ui_items = "BD0 Off\0BD1 Full\0BD2 Narrow\0BD3 Wide\0BD4 FPS Center\0BD5 FPS Narrow\0BD6 FPS Edge\0BD7 FPS Mixed\0";
	ui_label = " ZPD Boundary Detection";
	ui_tooltip = "This selection menu gives extra boundary conditions to ZPD.\n"
				 			 "This treats your screen as a virtual wall.\n"
				 		   "Default is Off.";
	ui_category = "Stereoscopic Options";
> = DE_X;

uniform float2 ZPD_Boundary_n_Fade <
	#if Compatibility
	ui_type = "drag";
	#else
	ui_type = "slider";
	#endif
	ui_min = 0.0; ui_max = 0.5;
	ui_label = " ZPD Boundary & Fade Time";
	ui_tooltip = "This selection menu gives extra boundary conditions to scale ZPD & lets you adjust Fade time.";
	ui_category = "Stereoscopic Options";
> = float2(DE_Y,DE_Z);

uniform int View_Mode <
	ui_type = "combo";
	ui_items = "VM0 Linear\0VM1 Point\0";
	ui_label = "·View Mode·";
	ui_tooltip = "Changes the way the shader fills in the occlude sections in the image.\n"
				"Default is Linear.";
ui_category = "Occlusion Masking";
> = 0;

uniform float Max_Depth <
	#if Compatibility
	ui_type = "drag";
	#else
	ui_type = "slider";
	#endif
	ui_min = 0.5; ui_max = 1.0;
	ui_label = " Max Depth";
	ui_tooltip = "Max Depth lets you clamp the max depth range of your scene.\n"
				 "So it's not hard on your eyes looking off in to the distance .\n"
				 "Default and starts at One and it's Off.";
	ui_category = "Occlusion Masking";
> = 1.0;

uniform float Lower_Depth_Res <
	#if Compatibility
	ui_type = "drag";
	#else
	ui_type = "slider";
	#endif
	ui_min = 0.0; ui_max = 1.0;
	ui_label = " View Mode Res";
	ui_tooltip = "Default and starts at Zero and it's Off.";
	ui_category = "Occlusion Masking";
> = 0;

uniform float Depth_Edge_Mask <
	#if Compatibility
	ui_type = "drag";
	#else
	ui_type = "slider";
	#endif
	ui_min = -0.125; ui_max = 1.5;
	ui_label = "·Edge Mask·";
	ui_tooltip = "Use this to adjust for artifacts from a lower resolution Depth Buffer.\n"
				 "Default is Zero, Off";
	ui_category = "Occlusion Masking";
> = 0.0;

uniform float DLSS_FSR_Offset <
	#if Compatibility
	ui_type = "drag";
	#else
	ui_type = "slider";
	#endif
	ui_min = 0.0; ui_max = 4.0;
	ui_label = " Upscailer Offset";
	ui_tooltip = "This Offset is for non conforming ZBuffer Postion witch is normaly 1 pixel wide.\n"
				 "This issue only happens sometimes when using things like DLSS or FSR.\n"
				 "This does not solve for TAA artifacts like Jittering or smearing.\n"
				 "Default and starts at Zero and it's Off. With a max offset of 4pixels Wide.";
	ui_category = "Occlusion Masking";
> = 0;

uniform int Depth_Map <
	ui_type = "combo";
	ui_items = "DM0 Normal\0DM1 Reversed\0";
	ui_label = "·Depth Map Selection·";
	ui_tooltip = "Linearization for the zBuffer also known as Depth Map.\n"
			     "DM0 is Z-Normal and DM1 is Z-Reversed.\n";
	ui_category = "Depth Map";
> = DA_W;

uniform float Depth_Map_Adjust <
	ui_type = "drag";
	ui_min = 1.0; ui_max = 250.0; ui_step = 0.125;
	ui_label = " Depth Map Adjustment";
	ui_tooltip = "This allows for you to adjust the DM precision.\n"
				 "Adjust this to keep it as low as possible.\n"
				 "Default is 7.5";
	ui_category = "Depth Map";
> = DA_Y;

uniform float Offset <
	ui_type = "drag";
	ui_min = -1.0; ui_max = 1.0;
	ui_label = " Depth Map Offset";
	ui_tooltip = "Depth Map Offset is for non conforming ZBuffer.\n"
				 "It,s rare if you need to use this in any game.\n"
				 "Use this to make adjustments to DM 0 or DM 1.\n"
				 "Default and starts at Zero and it's Off.";
	ui_category = "Depth Map";
> = DA_Z;

uniform float Auto_Depth_Adjust <
	ui_type = "drag";
	ui_min = 0.0; ui_max = 0.625;
	ui_label = " Auto Depth Adjust";
	ui_tooltip = "Automatically scales depth so it fights out of game menu pop out.\n"
				 "Default is 0.1f, Zero is off.";
	ui_category = "Depth Map";
> = DB_Z;

uniform int Depth_Detection <
	ui_type = "combo";
#if Compatibility_DD
	ui_items = "Off\0Detection +Sky\0Detection -Sky\0ReShade's Detection\0";
#else
	ui_items = "Off\0Detection +Sky\0Detection -Sky\0";
#endif
	ui_label = " Depth Detection";
	ui_tooltip = "Use this to disable/enable in game Depth Detection.";
	ui_category = "Depth Map";
#if Compatibility_DD
> = 3;
#else
> = 0;
#endif
uniform bool Depth_Map_Flip <
	ui_label = " Depth Map Flip";
	ui_tooltip = "Flip the depth map if it is upside down.";
	ui_category = "Depth Map";
> = DB_X;
#if DB_Size_Position || SPF == 2 || LB_Correction
uniform float2 Horizontal_and_Vertical <
	ui_type = "drag";
	ui_min = 0.0; ui_max = 2;
	ui_label = "·Horizontal & Vertical Size·";
	ui_tooltip = "Adjust Horizontal and Vertical Resize. Default is 1.0.";
	ui_category = "Reposition Depth";
> = float2(DD_X,DD_Y);

uniform float2 Image_Position_Adjust<
	ui_type = "drag";
	ui_min = -1.0; ui_max = 1.0;
	ui_label = " Horizontal & Vertical Position";
	ui_tooltip = "Adjust the Image Position if it's off by a bit. Default is Zero.";
	ui_category = "Reposition Depth";
> = float2(DD_Z,DD_W);

#if LB_Correction
uniform float2 H_V_Offset <
	ui_type = "drag";
	ui_min = 0.0; ui_max = 2;
	ui_label = " Horizontal & Vertical Size Offset";
	ui_tooltip = "Adjust Horizontal and Vertical Resize Offset for Letter Box Correction. Default is 1.0.";
	ui_category = "Reposition Depth";
> = float2(1.0,1.0);

uniform float2 Image_Pos_Offset <
	ui_type = "drag";
	ui_min = 0.0; ui_max = 2;
	ui_label = " Horizontal & Vertical Position Offset";
	ui_tooltip = "Adjust the Image Position if it's off by a bit for Letter Box Correction. Default is Zero.";
	ui_category = "Reposition Depth";
> = float2(0.0,0.0);

uniform bool LB_Correction_Switch <
	ui_label = " Letter Box Correction Toggle";
	ui_tooltip = "Use this to turn off and on the correction when LetterBox Detection is active.";
	ui_category = "Reposition Depth";
> = true;
#endif

uniform bool Alinement_View <
	ui_label = " Alinement View";
	ui_tooltip = "A Guide to help aline the Depth Buffer to the Image.";
	ui_category = "Reposition Depth";
> = false;
#else
static const bool Alinement_View = false;
static const float2 Horizontal_and_Vertical = float2(DD_X,DD_Y);
static const float2 Image_Position_Adjust = float2(DD_Z,DD_W);

static const bool LB_Correction_Switch = true;
static const float2 H_V_Offset = float2(DH_X,DH_Y);
static const float2 Image_Pos_Offset  = float2(DH_Z,DH_W);
#endif
//Weapon Hand Adjust//
uniform int WP <
	ui_type = "combo";
	ui_items = OW_WP;
	ui_label = "·Weapon Profiles·";
	ui_tooltip = "Pick Weapon Profile for your game or make your own.";
	ui_category = "Weapon Hand Adjust";
> = DB_W;

uniform float4 Weapon_Adjust <
	ui_type = "drag";
	ui_min = 0.0; ui_max = 250.0;
	ui_label = " Weapon Hand Adjust";
	ui_tooltip = "Adjust Weapon depth map for your games.\n"
				 "X, CutOff Point used to set a different scale for first person hand apart from world scale.\n"
				 "Y, Precision is used to adjust the first person hand in world scale.\n"
				 "Z, Tuning is used to fine tune the precision adjustment above.\n"
				 "W, Scale is used to compress or rescale the weapon.\n"
	             "Default is float2(X 0.0, Y 0.0, Z 0.0, W 1.0)";
	ui_category = "Weapon Hand Adjust";
> = float4(0.0,0.0,0.0,0.0);

uniform float4 WZPD_and_WND <
	ui_type = "drag";
	ui_min = 0.0; ui_max = 0.5;
	ui_label = " Weapon ZPD, Min, Max, & Trim";
	ui_tooltip = "WZPD controls the focus distance for the screen Pop-out effect also known as Convergence for the weapon hand.\n"
				"Weapon ZPD Is for setting a Weapon Profile Convergence, so you should most of the time leave this Default.\n"
				"Weapon Min is used to adjust min weapon hand of the weapon hand when looking at the world near you.\n"
				"Weapon Max is used to adjust max weapon hand when looking out at a distance.\n"
				"Default is (ZPD X 0.03, Min Y 0.0, Max Z 0.0, Trim Z 0.250 ) & Zero is off.";
	ui_category = "Weapon Hand Adjust";
> = float4(0.03,DG_Z,DE_W,DI_Z);

uniform float Weapon_ZPD_Boundary <
	ui_type = "slider";
	ui_min = 0.0; ui_max = 0.5;
	ui_label = " Weapon Boundary Detection";
	ui_tooltip = "This selection menu gives extra boundary conditions to WZPD.";
	ui_category = "Weapon Hand Adjust";
> = DF_X;
#if HUD_MODE || HMT
//Heads-Up Display
uniform float2 HUD_Adjust <
	ui_type = "drag";
	ui_min = 0.0; ui_max = 1.0;
	ui_label = "·HUD Mode·";
	ui_tooltip = "Adjust HUD for your games.\n"
				 "X, CutOff Point used to set a separation point between world scale and the HUD also used to turn HUD MODE On or Off.\n"
				 "Y, Pushes or Pulls the HUD in or out of the screen if HUD MODE is on.\n"
				 "This is only for UI elements that show up in the Depth Buffer.\n"
	             "Default is float2(X 0.0, Y 0.5)";
	ui_category = "Heads-Up Display";
> = float2(DF_W,0.5);
#endif

uniform int FPSDFIO <
	ui_type = "combo";
	ui_items = "Off\0Press\0Hold\0";
	ui_label = "·FPS Focus Depth·";
	ui_tooltip = "This lets the shader handle real time depth reduction for aiming down your sights.\n"
				"This may induce Eye Strain so take this as an Warning.";
	ui_category = "FPS Focus";
> = DK_X;

uniform int3 Eye_Fade_Reduction_n_Power <
	ui_type = "slider";
	ui_min = 0; ui_max = 2;
	ui_label = " Eye Fade Options";
	ui_tooltip ="X, Eye Selection: One is Right Eye only, Two is Left Eye Only, and Zero Both Eyes.\n"
				"Y, Fade Reduction: Decreases the depth amount by a current percentage.\n"
				"Z, Fade Speed: Decreases or Incresses how fast it changes.\n"
				"Default is X[ 0 ] Y[ 0 ] Z[ 1 ].";
	ui_category = "FPS Focus";
> = int3(DK_Y,DK_Z,DK_W);

//Cursor Adjustments
uniform int Cursor_Type <
	ui_type = "combo";
	ui_items = "Off\0FPS\0ALL\0RTS\0";
	ui_label = "·Cursor Selection·";
	ui_tooltip = "Choose the cursor type you like to use.\n"
							 "Default is Zero.";
	ui_category = "Cursor Adjustments";
> = 0;

uniform int3 Cursor_SC <
	ui_type = "drag";
	ui_min = 0; ui_max = 10;
	ui_label = " Cursor Adjustments";
	ui_tooltip = "This controlls the Size & Color.\n"
							 "Defaults are ( X 1, Y 0, Z 0).";
	ui_category = "Cursor Adjustments";
> = int3(1,0,0);

uniform bool Cursor_Lock <
	ui_label = " Cursor Lock";
	ui_tooltip = "Screen Cursor to Screen Crosshair Lock.";
	ui_category = "Cursor Adjustments";
> = false;
#if BD_Correction
uniform int BD_Options <
	ui_type = "combo";
	ui_items = "On\0Off\0Guide\0";
	ui_label = "·Distortion Options·";
	ui_tooltip = "Use this to Turn Off, Turn On, & to use the BD Alinement Guide.\n"
				 "Default is ON.";
	ui_category = "Distortion Corrections";
> = 0;
uniform float3 Colors_K1_K2_K3 <
	#if Compatibility
	ui_type = "drag";
	#else
	ui_type = "slider";
	#endif
	ui_min = -2.0; ui_max = 2.0;
	ui_tooltip = "Adjust Distortions K1, K2, & K3.\n"
				 "Default is 0.0";
	ui_label = " Barrel Distortion K1 K2 K3 ";
	ui_category = "Distortion Corrections";
> = float3(DC_X,DC_Y,DC_Z);

uniform float Zoom <
	ui_type = "drag";
	ui_min = -0.5; ui_max = 0.5;
	ui_label = " Barrel Distortion Zoom";
	ui_tooltip = "Adjust Distortions Zoom.\n"
				 			 "Default is 0.0";
	ui_category = "Distortion Corrections";
> = DC_W;
#else
	#if BDF
	uniform bool BD_Options <
		ui_label = "·Toggle Barrel Distortion·";
		ui_tooltip = "Use this if you modded the game to remove Barrel Distortion.";
		ui_category = "Distortion Corrections";
	> = !true;
	#else
		static const int BD_Options = 1;
	#endif
static const float3 Colors_K1_K2_K3 = float3(DC_X,DC_Y,DC_Z);
static const float Zoom = DC_W;
#endif
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
uniform bool Cancel_Depth < source = "key"; keycode = Cancel_Depth_Key; toggle = true; mode = "toggle";>;
uniform bool Mask_Cycle < source = "key"; keycode = Mask_Cycle_Key; toggle = true; mode = "toggle";>;
uniform bool Text_Info < source = "key"; keycode = Text_Info_Key; toggle = true; mode = "toggle";>;
uniform bool CLK < source = "mousebutton"; keycode = Cursor_Lock_Key; toggle = true; mode = "toggle";>;
uniform bool Trigger_Fade_A < source = "mousebutton"; keycode = Fade_Key; toggle = true; mode = "toggle";>;
uniform bool Trigger_Fade_B < source = "mousebutton"; keycode = Fade_Key;>;
uniform float2 Mousecoords < source = "mousepoint"; > ;
uniform float frametime < source = "frametime";>;
uniform float timer < source = "timer"; >;
#if Compatibility_FP
uniform float3 motion[2] < source = "freepie"; index = 0; >;
//. motion[0] is yaw, pitch, roll and motion[1] is x, y, z. In ReShade 4.8+ in ReShade 4.7 it is x = y / y = z
//float3 FP_IO_Rot(){return motion[0];}
float3 FP_IO_Pos()
{
#if Compatibility_FP == 1
	#warning "Autostereoscopic enhanced features need ReShade 4.8.0 and above."
	return motion[1].yzz;
#elif Compatibility_FP == 2
	return motion[1];
#endif
}
#else
//float3 FP_IO_Rot(){return 0;}
float3 FP_IO_Pos(){return 0;}
#warning "Autostereoscopic Need ReShade 4.6.0 and above."
#endif

static const float Auto_Balance_Clamp = 0.5; //This Clamps Auto Balance's max Distance.

#if Compatibility_DD
uniform bool DepthCheck < source = "bufready_depth"; >;
#endif

#define pix float2(BUFFER_RCP_WIDTH, BUFFER_RCP_HEIGHT)
#define Res float2(BUFFER_WIDTH, BUFFER_HEIGHT)
#define Per float2( (Perspective * pix.x) * 0.5, 0) //Per is Perspective
#define AI Interlace_Anaglyph_Calibrate.x * 0.5 //Optimization for line interlaced Adjustment.
#define ARatio (BUFFER_WIDTH / BUFFER_HEIGHT)


float Scale(float val,float max,float min) //Scale to 0 - 1
{
	return (val - min) / (max - min);
}

float fmod(float a, float b)
{
	float c = frac(abs(a / b)) * abs(b);
	return a < 0 ? -c : c;
}
///////////////////////////////////////////////////////////Conversions/////////////////////////////////////////////////////////////
float3 RGBtoYCbCr(float3 rgb) // For Super3D a new Stereo3D output.
{   float TCoRF[1];//The Chronicles of Riddick: Assault on Dark Athena FIX I don't know why it works.......
	float Y  =  .299 * rgb.x + .587 * rgb.y + .114 * rgb.z; // Luminance
	float Cb = -.169 * rgb.x - .331 * rgb.y + .500 * rgb.z; // Chrominance Blue
	float Cr =  .500 * rgb.x - .419 * rgb.y - .081 * rgb.z; // Chrominance Red
	return float3(Y,Cb + 128./255.,Cr + 128./255.);
}//Code Not used for anything...
///////////////////////////////////////////////////////////////3D Starts Here///////////////////////////////////////////////////////////
texture DepthBufferTex : DEPTH;
sampler DepthBuffer
	{
		Texture = DepthBufferTex;
		AddressU = CLAMP;
		AddressV = CLAMP;
		AddressW = CLAMP;
		//Used Point for games like AMID Evil that don't have a proper Filtering.
		MagFilter = POINT;
		MinFilter = POINT;
		MipFilter = POINT;
	};

texture BackBufferTex : COLOR;
sampler BackBufferMIRROR
	{
		Texture = BackBufferTex;
		AddressU = MIRROR;
		AddressV = MIRROR;
		AddressW = MIRROR;
	};

sampler BackBufferBORDER
	{
		Texture = BackBufferTex;
		AddressU = BORDER;
		AddressV = BORDER;
		AddressW = BORDER;
	};

sampler BackBufferCLAMP
	{
		Texture = BackBufferTex;
		AddressU = CLAMP;
		AddressV = CLAMP;
		AddressW = CLAMP;
	};

#if D_Frame || DFW
texture texCF { Width = BUFFER_WIDTH ; Height = BUFFER_HEIGHT ; Format = RGBA8; };

sampler SamplerCF
	{
		Texture = texCF;
	};

texture texDF { Width = BUFFER_WIDTH ; Height = BUFFER_HEIGHT ; Format = RGBA8; };

sampler SamplerDF
	{
		Texture = texDF;
	};
#endif
texture texDMWoWvx { Width = BUFFER_WIDTH ; Height = BUFFER_HEIGHT; Format = RGBA16F; };

sampler SamplerDMWoWvx
	{
		Texture = texDMWoWvx;
	};

texture texzBufferWoWvx_P { Width = BUFFER_WIDTH ; Height = BUFFER_HEIGHT ; Format = RG16F; MipLevels = 6; };

sampler SamplerzBufferWoWvx_P
	{
		Texture = texzBufferWoWvx_P;
		MagFilter = POINT;
		MinFilter = POINT;
		MipFilter = POINT;
	};

texture texzBufferWoWvx_L { Width = BUFFER_WIDTH ; Height = BUFFER_HEIGHT ; Format = RG16F; MipLevels = 8; };

sampler SamplerzBufferWoWvx_L
	{
		Texture = texzBufferWoWvx_L;
	};


#if UI_MASK
texture TexMaskA < source = "DM_Mask_A.png"; > { Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = RGBA8; };
sampler SamplerMaskA { Texture = TexMaskA;};
texture TexMaskB < source = "DM_Mask_B.png"; > { Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = RGBA8; };
sampler SamplerMaskB { Texture = TexMaskB;};
#endif
#define Scale_Buffer 160 / BUFFER_WIDTH
////////////////////////////////////////////////////////Adapted Luminance/////////////////////////////////////////////////////////////////////
texture texLumWoWvx {Width = BUFFER_WIDTH * Scale_Buffer; Height = BUFFER_HEIGHT * Scale_Buffer; Format = RGBA16F; MipLevels = 8;};

sampler SamplerLumWoWvx
	{
		Texture = texLumWoWvx;
	};
	
float2 Lum(float2 texcoord)
	{ 
		return saturate(tex2Dlod(SamplerLumWoWvx,float4(texcoord,0,11)).xy);//Average Depth Brightnes Texture Sample
	}
	
////////////////////////////////////////////////////Distortion Correction//////////////////////////////////////////////////////////////////////
#if BD_Correction || BDF
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
///////////////////////////////////////////////////////////3D Image Adjustments/////////////////////////////////////////////////////////////////////

#if D_Frame || DFW
float4 CurrentFrame(in float4 position : SV_Position, in float2 texcoords : TEXCOORD) : SV_Target
{
		return tex2Dlod(BackBufferBORDER,float4(texcoords,0,0));
}

float4 DelayFrame(in float4 position : SV_Position, in float2 texcoords : TEXCOORD) : SV_Target
{
	return tex2Dlod(SamplerCF,float4(texcoords,0,0));
}

float4 CSB(float2 texcoords)
{
		return tex2Dlod(SamplerDF,float4(texcoords,0,0));
}
#else
float4 CSB(float2 texcoords)
{
		return tex2Dlod(BackBufferBORDER,float4(texcoords,0,0));
}
#endif

#if LBC || LBM || LB_Correction || LetterBox_Masking
float SLLTresh(float2 TCLocations, float MipLevel)
{ 
	return tex2Dlod(SamplerzBufferWoWvx_L,float4(TCLocations,0, MipLevel)).y;
}

float LBDetection()//Active RGB Detection
{   float MipLevel = 5,Center = SLLTresh(float2(0.5,0.5), 8) > 0, Top_Left = SLLTresh(float2(0.1,0.1), MipLevel) == 0;
	if ( LetterBox_Masking == 2 || LB_Correction == 2 || LBC == 2 || LBM == 2 )
		return Top_Left && (SLLTresh(float2(0.1,0.5), MipLevel) == 0 ) && (SLLTresh(float2(0.9,0.5), MipLevel) == 0 ) && Center ? 1 : 0; //Vert
	else                   //Left_Center                                  //Right_Center
		return Top_Left && (SLLTresh(float2(0.5,0.9), MipLevel) == 0 ) && Center ? 1 : 0; //Hoz
}			              //Bottom_Center
#endif

#if SDT || SD_Trigger
float TargetedDepth(float2 TC)
{
	return smoothstep(0,1,tex2Dlod(SamplerzBufferWoWvx_P,float4(TC,0,0)).y);
}

float SDTriggers()//Specialized Depth Triggers
{   float Threshold = 0.001;//Both this and the options below may need to be adjusted. A Value lower then 7.5 will break this.!?!?!?!
	if ( SD_Trigger == 1 || SDT == 1)//Top _ Left                             //Center_Left                             //Botto_Left
		return (TargetedDepth(float2(0.95,0.25)) >= Threshold ) && (TargetedDepth(float2(0.95,0.5)) >= Threshold) && (TargetedDepth(float2(0.95,0.75)) >= Threshold) ? 0 : 1;
	else
		return 0;
}
#endif
/////////////////////////////////////////////////////////////Cursor///////////////////////////////////////////////////////////////////////////
float4 MouseCursor(float2 texcoord )
{   float4 Out = CSB(texcoord),Color;
		float A = 0.959375, TCoRF = 1-A, Cursor;
		if(Cursor_Type > 0)
		{
			float CCA = 0.005, CCB = 0.00025, CCC = 0.25, CCD = 0.00125, Arrow_Size_A = 0.7, Arrow_Size_B = 1.3, Arrow_Size_C = 4.0;//scaling
			float2 MousecoordsXY = Mousecoords * pix, center = texcoord, Screen_Ratio = float2(ARatio,1.0), Size_Color = float2(1+Cursor_SC.x,Cursor_SC.y);
			float THICC = (2.0+Size_Color.x) * CCB, Size = Size_Color.x * CCA, Size_Cubed = (Size_Color.x*Size_Color.x) * CCD;

			if (Cursor_Lock && !CLK)
			MousecoordsXY = float2(0.5,lerp(0.5,0.5725,Scale(Cursor_SC.z,10,0) ));

			float4 Dist_from_Hori_Vert = float4( abs((center.x - (Size* Arrow_Size_B) / Screen_Ratio.x) - MousecoordsXY.x) * Screen_Ratio.x, // S_dist_fromHorizontal
												 abs(center.x - MousecoordsXY.x) * Screen_Ratio.x, 										  // dist_fromHorizontal
												 abs((center.y - (Size* Arrow_Size_B)) - MousecoordsXY.y),								   // S_dist_fromVertical
												 abs(center.y - MousecoordsXY.y));														   // dist_fromVertical

			//Cross Cursor
			float B = min(max(THICC - Dist_from_Hori_Vert.y,0),max(Size-Dist_from_Hori_Vert.w,0)), A = min(max(THICC - Dist_from_Hori_Vert.w,0),max(Size-Dist_from_Hori_Vert.y,0));
			float CC = A+B; //Cross Cursor

			//Solid Square Cursor
			float SSC = min(max(Size_Cubed - Dist_from_Hori_Vert.y,0),max(Size_Cubed-Dist_from_Hori_Vert.w,0)); //Solid Square Cursor

			if (Cursor_Type == 3)
			{
				Dist_from_Hori_Vert.y = abs((center.x - Size / Screen_Ratio.x) - MousecoordsXY.x) * Screen_Ratio.x ;
				Dist_from_Hori_Vert.w = abs(center.y - Size - MousecoordsXY.y);
			}
			//Cursor
			float C = all(min(max(Size - Dist_from_Hori_Vert.y,0),max(Size - Dist_from_Hori_Vert.w,0)));//removing the line below removes the square.
				  C -= all(min(max(Size - Dist_from_Hori_Vert.y * Arrow_Size_C,0),max(Size - Dist_from_Hori_Vert.w * Arrow_Size_C,0)));//Need to add this to fix a - bool issue in openGL
				  C -= all(min(max((Size * Arrow_Size_A) - Dist_from_Hori_Vert.x,0),max((Size * Arrow_Size_A)-Dist_from_Hori_Vert.z,0)));			// Cursor Array //
			if(Cursor_Type == 1)
				Cursor = CC;
			else if (Cursor_Type == 2)
				Cursor = SSC;
			else if (Cursor_Type == 3)
				Cursor = C;

			// Cursor Color Array //
			float3 CCArray[11] = {
			float3(1,1,1),//White
			float3(0,0,1),//Blue
			float3(0,1,0),//Green
			float3(1,0,0),//Red
			float3(1,0,1),//Magenta
			float3(0,1,1),
			float3(1,1,0),
			float3(1,0.4,0.7),
			float3(1,0.64,0),
			float3(0.5,0,0.5),
			float3(0,0,0) //Black
			};
			int CSTT = clamp(Cursor_SC.y,0,10);
			Color.rgb = CCArray[CSTT];
		}

return Cursor ? Color : Out;
}
//////////////////////////////////////////////////////////Depth Map Information/////////////////////////////////////////////////////////////////////
float DMA() //Small List of internal Multi Game Depth Adjustments.
{ float DMA = Depth_Map_Adjust;
	#if (__APPLICATION__ == 0xC0052CC4) //Halo The Master Chief Collection
	if( WP == 4) // Change on weapon selection.
		DMA *= 0.25;
	else if( WP == 5)
		DMA *= 0.8875;
	#endif
	return DMA;
}

float2 TC_SP(float2 texcoord)
{
	#if BD_Correction || BDF
	if(BD_Options == 0 || BD_Options == 2)
	{
		float3 K123 = Colors_K1_K2_K3 * 0.1;
		texcoord = D(texcoord.xy,K123.x,K123.y,K123.z);
	}
	#endif
	#if DB_Size_Position || SPF || LBC || LB_Correction || SDT || SD_Trigger

		#if SDT || SD_Trigger
			float2 X_Y = float2(Image_Position_Adjust.x,Image_Position_Adjust.y) + (SDTriggers() ? float2( DG_X , DG_Y) : 0.0);
		#else
			#if LBC || LB_Correction
				float2 X_Y = Image_Position_Adjust + (LBDetection() && LB_Correction_Switch ? Image_Pos_Offset : 0.0f );
			#else
				float2 X_Y = float2(Image_Position_Adjust.x,Image_Position_Adjust.y);
			#endif
		#endif

	texcoord.xy += float2(-X_Y.x,X_Y.y)*0.5;

		#if LBC || LB_Correction
			float2 H_V = Horizontal_and_Vertical * (LBDetection() && LB_Correction_Switch ? H_V_Offset : 1.0f );
		#else
			float2 H_V = Horizontal_and_Vertical;
		#endif
	float2 midHV = (H_V-1) * float2(BUFFER_WIDTH * 0.5,BUFFER_HEIGHT * 0.5) * pix;
	texcoord = float2((texcoord.x*H_V.x)-midHV.x,(texcoord.y*H_V.y)-midHV.y);
	#endif
	return texcoord;
}

float Depth(float2 texcoord)
{	//Conversions to linear space.....
	float zBuffer = tex2Dlod(DepthBuffer, float4(texcoord,0,0)).x, Far = 1.0, Near = 0.125/DMA(); //Near & Far Adjustment
	//Man Why can't depth buffers Just Be Normal
	float2 C = float2( Far / Near, 1.0 - Far / Near ), Z = Offset < 0 ? min( 1.0, zBuffer * ( 1.0 + abs(Offset) ) ) : float2( zBuffer, 1.0 - zBuffer );

	if(Offset > 0 || Offset < 0)
		Z = Offset < 0 ? float2( Z.x, 1.0 - Z.y ) : min( 1.0, float2( Z.x * (1.0 + Offset) , Z.y / (1.0 - Offset) ) );
	//MAD - RCP
	if (Depth_Map == 0) //DM0 Normal
		zBuffer = rcp(Z.x * C.y + C.x);
	else if (Depth_Map == 1) //DM1 Reverse
		zBuffer = rcp(Z.y * C.y + C.x);

	return saturate(zBuffer);
}
//Weapon Setting//
float4 WA_XYZW()
{
	float4 WeaponSettings_XYZW = Weapon_Adjust;
	#if WSM >= 1
		WeaponSettings_XYZW = Weapon_Profiles(WP, Weapon_Adjust);
	#endif
	//"X, CutOff Point used to set a different scale for first person hand apart from world scale.\n"
	//"Y, Precision is used to adjust the first person hand in world scale.\n"
	//"Z, Tuning is used to fine tune the precision adjustment above.\n"
	//"W, Scale is used to compress or rescale the weapon.\n"	
	return float4(WeaponSettings_XYZW.xyz,-WeaponSettings_XYZW.w + 1);
}
//Weapon Depth Buffer//
float2 WeaponDepth(float2 texcoord)
{   //Conversions to linear space.....
	float zBufferWH = tex2Dlod(DepthBuffer, float4(texcoord,0,0)).x, Far = 1.0, Near = 0.125/(0.00000001 + WA_XYZW().y);  //Near & Far Adjustment

	float2 Offsets = float2(1 + WA_XYZW().z,1 - WA_XYZW().z), Z = float2( zBufferWH, 1-zBufferWH );

	if (WA_XYZW().z > 0)
	Z = min( 1, float2( Z.x * Offsets.x , Z.y / Offsets.y  ));

	[branch] if (Depth_Map == 0)//DM0. Normal
		zBufferWH = Far * Near / (Far + Z.x * (Near - Far));
	else if (Depth_Map == 1)//DM1. Reverse
		zBufferWH = Far * Near / (Far + Z.y * (Near - Far));

	return float2(saturate(zBufferWH), WA_XYZW().x);
}
//3x2 and 2x3 not supported on older ReShade versions. I had to use 3x3. Old Values for 3x2
float3x3 PrepDepth(float2 texcoord)
{   int Flip_Depth = Flip_Opengl_Depth ? !Depth_Map_Flip : Depth_Map_Flip;

	if (Flip_Depth)
		texcoord.y =  1 - texcoord.y;
	
	//Texture Zoom & Aspect Ratio//
	//float X = TEST.x;
	//float Y = TEST.y * TEST.x * 2;
	//float midW = (X - 1)*(BUFFER_WIDTH*0.5)*pix.x;	
	//float midH = (Y - 1)*(BUFFER_HEIGHT*0.5)*pix.y;	
				
	//texcoord = float2((texcoord.x*X)-midW,(texcoord.y*Y)-midH);	
	
	texcoord.xy -= DLSS_FSR_Offset.x * pix;
	
	float4 DM = Depth(TC_SP(texcoord)).xxxx;
	float R, G, B, A, WD = WeaponDepth(TC_SP(texcoord)).x, CoP = WeaponDepth(TC_SP(texcoord)).y, CutOFFCal = (CoP/DMA()) * 0.5; //Weapon Cutoff Calculation
	CutOFFCal = step(DM.x,CutOFFCal);

	[branch] if (WP == 0)
		DM.x = DM.x;
	else
	{
		DM.x = lerp(DM.x,WD,CutOFFCal);
		DM.y = lerp(0.0,WD,CutOFFCal);
		DM.z = lerp(0.5,WD,CutOFFCal);
	}

	R = DM.x; //Mix Depth
	G = DM.y > saturate(smoothstep(0,2.5,DM.w)); //Weapon Mask
	B = DM.z; //Weapon Hand
	A = ZPD_Boundary >= 4 ? max( G, R) : R; //Grid Depth

	return float3x3( saturate(float3(R, G, B)), 	                                                      //[0][0] = R | [0][1] = G | [0][2] = B
					 saturate(float3(A, Depth( SDT || SD_Trigger ? texcoord : TC_SP(texcoord) ).x, DM.w)),//[1][0] = A | [1][1] = D | [1][2] = DM
							  float3(0,0,0) );                                                            //[2][0] = 0 | [2][1] = 0 | [2][2] = 0
}
//////////////////////////////////////////////////////////////Depth HUD Alterations///////////////////////////////////////////////////////////////////////
#if UI_MASK
float HUD_Mask(float2 texcoord )
{   float Mask_Tex;
	    if (Mask_Cycle == 1)
	        Mask_Tex = tex2Dlod(SamplerMaskB,float4(texcoord.xy,0,0)).a;
	    else
	        Mask_Tex = tex2Dlod(SamplerMaskA,float4(texcoord.xy,0,0)).a;

	return saturate(Mask_Tex);
}
#endif
/////////////////////////////////////////////////////////Fade In and Out Toggle/////////////////////////////////////////////////////////////////////
float Fade_in_out(float2 texcoord)
{ float TCoRF[1], Trigger_Fade, AA = Fade_Time_Adjust, PStoredfade = tex2D(SamplerLumWoWvx,float2(0,0.083)).z;
	if(Eye_Fade_Reduction_n_Power.z == 0)
		AA *= 0.5;
	else if(Eye_Fade_Reduction_n_Power.z == 2)
		AA *= 1.5;
	//Fade in toggle.
	if(FPSDFIO == 1 )//|| FPSDFIO == 3 )
		Trigger_Fade = Trigger_Fade_A;
	else if(FPSDFIO == 2)// || FPSDFIO == 4 )
		Trigger_Fade = Trigger_Fade_B;

	return PStoredfade + (Trigger_Fade - PStoredfade) * (1.0 - exp(-frametime/((1-AA)*1000))); ///exp2 would be even slower
}

float2 Fade(float2 texcoord) // Maybe make it float2 and pass the 2nd switch to swap it with grater strength onlu if it's beyond -1.0
{   //Check Depth
	float CD, Detect, Detect_Out_of_Range;
	if(ZPD_Boundary > 0)
	{   float4 Switch_Array = ZPD_Boundary == 6 ? float4(0.825,0.850,0.875,0.900) : float4(1.0,0.875,0.75,0.625);
		//Normal A & B for both	
		float CDArray_A[7] = { 0.125, 0.25, 0.375, 0.5, 0.625, 0.75, 0.875}, CDArray_B[7] = { 0.25, 0.375, 0.4375, 0.5, 0.5625, 0.625, 0.75}, CDArray_C[4] = { 0.875, 0.75, 0.5, 0.25};
		float CDArrayZPD_A[7] = { ZPD_Separation.x * Switch_Array.w, ZPD_Separation.x * Switch_Array.z, ZPD_Separation.x * Switch_Array.y, ZPD_Separation.x * Switch_Array.x, ZPD_Separation.x * Switch_Array.y, ZPD_Separation.x * Switch_Array.z, ZPD_Separation.x * Switch_Array.w },
			  CDArrayZPD_B[7] = { ZPD_Separation.x * 0.3, ZPD_Separation.x * 0.5, ZPD_Separation.x * 0.75, ZPD_Separation.x, ZPD_Separation.x * 0.75, ZPD_Separation.x * 0.5, ZPD_Separation.x * 0.3},
 			 CDArrayZPD_C[12] = { ZPD_Separation.x * 0.5, ZPD_Separation.x * 0.625, ZPD_Separation.x * 0.75, ZPD_Separation.x * 0.875, ZPD_Separation.x * 0.9375, 
								   ZPD_Separation.x, ZPD_Separation.x, 
								   ZPD_Separation.x * 0.9375, ZPD_Separation.x * 0.875, ZPD_Separation.x * 0.75, ZPD_Separation.x * 0.625, ZPD_Separation.x * 0.5 };	
		//Screen Space Detector 7x7 Grid from between 0 to 1 and ZPD Detection becomes stronger as it gets closer to the Center.
		float2 GridXY; int2 iXY = ZPD_Boundary == 3 ? int2( 12, 4) : int2( 7, 7) ;
		[loop]
		for( int iX = 0 ; iX < iXY.x; iX++ )
		{   [loop]
			for( int iY = 0 ; iY < iXY.y; iY++ )
			{
				if(ZPD_Boundary == 1 || ZPD_Boundary == 6 || ZPD_Boundary == 7)
					GridXY = float2( CDArray_A[iX], CDArray_A[iY]);
				else if(ZPD_Boundary == 2 || ZPD_Boundary == 5)
					GridXY = float2( CDArray_B[iX], CDArray_A[iY]);
				else if(ZPD_Boundary == 3)
					GridXY = float2( (iX + 1) * rcp(iXY.x + 2),CDArray_C[min(3,iY)]);
				else if(ZPD_Boundary == 4)
					GridXY = float2( CDArray_A[iX], CDArray_B[iY]);
				
				float ZPD_I = ZPD_Boundary == 3 ?  CDArrayZPD_C[iX] : (ZPD_Boundary == 2 || ZPD_Boundary == 5  ? CDArrayZPD_B[iX] : CDArrayZPD_A[iX]);

				if(ZPD_Boundary >= 4)
				{
					if ( PrepDepth(GridXY)[1][0] == 1 )
						ZPD_I = 0;
				}
				// CDArrayZPD[i] reads across prepDepth.......
				CD = 1 - ZPD_I / PrepDepth(GridXY)[1][0];

				#if UI_MASK
					CD = max( 1 - ZPD_I / HUD_Mask(GridXY), CD );
				#endif
				if ( CD < -DG_W )//may lower this to like -0.1
					Detect = 1;
				//Used if Depth Buffer is way out of range.
				if(REF || RE_Fix)
				{
				if ( CD < -DI_W )
					Detect_Out_of_Range = 1;
				}
			}
		}
	}
	float Trigger_Fade_A = Detect, Trigger_Fade_B = Detect_Out_of_Range, AA = (1-(ZPD_Boundary_n_Fade.y*2.))*1000, 
		  PStoredfade_A = tex2D(SamplerLumWoWvx,float2(0, 0.250)).z, PStoredfade_B = tex2D(SamplerLumWoWvx,float2(0, 0.416)).z;
	//Fade in toggle.
	return float2( PStoredfade_A + (Trigger_Fade_A - PStoredfade_A) * (1.0 - exp(-frametime/AA)), PStoredfade_B + (Trigger_Fade_B - PStoredfade_B) * (1.0 - exp(-frametime/AA)) ); ///exp2 would be even slower
}
#define FadeSpeed_AW 0.25
float AltWeapon_Fade()
{
	float  ExAd = (1-(FadeSpeed_AW * 2.0))*1000, Current =  min(0.75f,smoothstep(0,0.25f,PrepDepth(0.5f)[0][0])), Past = tex2D(SamplerLumWoWvx,float2(0,0.750)).z;
	return Past + (Current - Past) * (1.0 - exp(-frametime/ExAd));
}

float Weapon_ZPD_Fade(float Weapon_Con)
{
	float  ExAd = (1-(FadeSpeed_AW * 2.0))*1000, Current =  Weapon_Con, Past = tex2D(SamplerLumWoWvx,float2(0,0.916)).z;
	return Past + (Current - Past) * (1.0 - exp(-frametime/ExAd));
}
//////////////////////////////////////////////////////////Depth Map Alterations/////////////////////////////////////////////////////////////////////
float4 DepthMap(in float4 position : SV_Position,in float2 texcoord : TEXCOORD) : SV_Target
{
	float4 DM = float4(PrepDepth(texcoord)[0][0],PrepDepth(texcoord)[0][1],PrepDepth(texcoord)[0][2],PrepDepth(texcoord)[1][1]);
	float R = DM.x, G = DM.y, B = DM.z, Auto_Scale =  WZPD_and_WND.y > 0.0 ? tex2D(SamplerLumWoWvx,float2(0,0.750)).z : 1;

	//Fade Storage
	float ScaleND = saturate(lerp(R,1.0f,smoothstep(min(-WZPD_and_WND.y,-WZPD_and_WND.z * Auto_Scale),1.0f,R)));

	if (WZPD_and_WND.y > 0)
		R = saturate(lerp(ScaleND,R,smoothstep(0,WZPD_and_WND.w,ScaleND)));

	if(texcoord.x < pix.x * 2 && texcoord.y < pix.y * 2)//TL
		R = Fade_in_out(texcoord);
	if(1-texcoord.x < pix.x * 2 && 1-texcoord.y < pix.y * 2)//BR
		R = Fade(texcoord).x;
	if(texcoord.x < pix.x * 2 && 1-texcoord.y < pix.y * 2)//BL
		R = Fade(texcoord).y;
	
	float Luma_Map = dot(0.333, tex2D(BackBufferCLAMP,texcoord).rgb);
	
	return saturate(float4(R,G,B,Luma_Map));
}

float AutoDepthRange(float d, float2 texcoord )
{ float LumAdjust_ADR = smoothstep(-0.0175,Auto_Depth_Adjust,Lum(texcoord).y);
    return min(1,( d - 0 ) / ( LumAdjust_ADR - 0));
}

float3 Conv(float2 MD_WHD,float2 texcoord)
{	float D = MD_WHD.x, Z = ZPD_Separation.x, WZP = 0.5, ZP = 0.5, ALC = abs(Lum(texcoord).x), W_Convergence = WZPD_and_WND.x, WZPDB, Distance_From_Bottom = 0.9375, ZPD_Boundary = ZPD_Boundary_n_Fade.x, Store_WC;
    //Screen Space Detector.
	if (abs(Weapon_ZPD_Boundary) > 0)
	{   float WArray[8] = { 0.5, 0.5625, 0.625, 0.6875, 0.75, 0.8125, 0.875, 0.9375},
			  MWArray[8] = { 0.4375, 0.46875, 0.5, 0.53125, 0.625, 0.75, 0.875, 0.9375},
			  WZDPArray[8] = { 1.0, 0.5, 0.75, 0.5, 0.625, 0.5, 0.55, 0.5};//SoF ZPD Weapon Map
		[unroll] //only really only need to check one point just above the center bottom and to the right.
		for( int i = 0 ; i < 8; i++ )
		{
			if((WP == 22 || WP == 4) && WSM == 1)//SoF & BL 2
				WZPDB = 1 - (WZPD_and_WND.x * WZDPArray[i]) / tex2Dlod(SamplerDMWoWvx,float4(float2(WArray[i],0.9375),0,0)).z;
			else
			{
				if (Weapon_ZPD_Boundary < 0) //Code for Moving Weapon Hand stablity.
					WZPDB = 1 - WZPD_and_WND.x / tex2Dlod(SamplerDMWoWvx,float4(float2(MWArray[i],Distance_From_Bottom),0,0)).z;
				else //Normal
					WZPDB = 1 - WZPD_and_WND.x / tex2Dlod(SamplerDMWoWvx,float4(float2(WArray[i],Distance_From_Bottom),0,0)).z;
			}

			if (WZPDB < -DJ_W) // Default -0.1
				W_Convergence *= 1.0-abs(Weapon_ZPD_Boundary);
		}
	}
	//Store Weapon Convergence for Smoothing.
	Store_WC = W_Convergence;
	
	W_Convergence = 1 - tex2D(SamplerLumWoWvx,float2(0,0.916)).z / MD_WHD.y;// 1-W_Convergence/D
	float WD = MD_WHD.y; //Needed to seperate Depth for the  Weapon Hand. It was causing problems with Auto Depth Range below.

		if (Auto_Depth_Adjust > 0)
			D = AutoDepthRange(D,texcoord);

	#if Balance_Mode || BMT
			ZP = saturate(ZPD_Balance);
	#else
		if(Auto_Balance_Ex > 0 )
			ZP = saturate(ALC);
	#endif
		float DOoR = smoothstep(0,1,tex2D(SamplerLumWoWvx,float2(0, 0.416)).z), ZDP_Array[16] = { 0.0, 0.0125, 0.025, 0.0375, 0.04375, 0.05, 0.0625, 0.075, 0.0875, 0.09375, 0.1, 0.125, 0.150, 0.175, 0.20, 0.225};
		
		if(REF || RE_Fix)
		{
			if(RE_Fix)
				ZPD_Boundary = lerp(ZPD_Boundary,ZDP_Array[RE_Fix],DOoR);
			else
				ZPD_Boundary = lerp(ZPD_Boundary,ZDP_Array[REF],DOoR);
		}

		Z *= lerp( 1, ZPD_Boundary, smoothstep(0,1,tex2D(SamplerLumWoWvx,float2(0, 0.250)).z));
		float Convergence = 1 - Z / D;
		if (ZPD_Separation.x == 0)
			ZP = 1;

		if (WZPD_and_WND.x <= 0)
			WZP = 1;

		if (ALC <= 0.025)
			WZP = 1;

		ZP = min(ZP,Auto_Balance_Clamp);

   return float3( lerp(Convergence,min(saturate(Max_Depth),D), ZP), lerp(W_Convergence,WD,WZP), Store_WC);
}

float3 DB_Comb( float2 texcoord)
{
	// X = Mix Depth | Y = Weapon Mask | Z = Weapon Hand | W = Normal Depth
	float4 DM = float4(tex2Dlod(SamplerDMWoWvx,float4(texcoord,0,0)).xyz,PrepDepth( SDT || SD_Trigger ? TC_SP(texcoord) :  texcoord )[1][1]);
	//Hide Temporal passthrough
	if(texcoord.x < pix.x * 2 && texcoord.y < pix.y * 2)
		DM = PrepDepth(texcoord)[0][0];
	if(1-texcoord.x < pix.x * 2 && 1-texcoord.y < pix.y * 2)
		DM = PrepDepth(texcoord)[0][0];
	if(texcoord.x < pix.x * 2 && 1-texcoord.y < pix.y * 2)
		DM = PrepDepth(texcoord)[0][0];

	if (WP == 0 || WZPD_and_WND.x <= 0)
		DM.y = 0;
	//Handle Convergence Here
	float3 HandleConvergence = Conv(DM.xz,texcoord).xyz;	
	DM.y = lerp( HandleConvergence.x, HandleConvergence.y * WA_XYZW().w, DM.y);
	//Better mixing for eye Comfort
	DM.z = DM.y;
	DM.y += lerp(DM.y,DM.x,DM.w);
	DM.y *= 0.5f;
	DM.y = lerp(DM.y,DM.z,0.9375f);
	#if Compatibility_DD
	if (Depth_Detection == 1 || Depth_Detection == 2)
	{ //Check Depth at 3 Point D_A Top_Center / Bottom_Center
		float2 DA_DB = float2(tex2Dlod(SamplerDMWoWvx,float4(float2(0.5,0.0),0,0)).x, tex2Dlod(SamplerDMWoWvx,float4(float2(0.0,1.0),0,0)).x);
		if(Depth_Detection == 2)
		{
			if (DA_DB.x == DA_DB.y)
				DM = 0.0625;
		}
		else
		{   //Ignores Sky
			if (DA_DB.x != 1 && DA_DB.y != 1)
			{
				if (DA_DB.x == DA_DB.y)
					DM = 0.0625;
			}
		}
	}
	else if (Depth_Detection == 3)
	{
		if (!DepthCheck)
			DM = 0.0625;
	}
	#else
	if (Depth_Detection == 1 || Depth_Detection == 2)
	{ //Check Depth at 3 Point D_A Top_Center / Bottom_Center
		float2 DA_DB = float2(tex2Dlod(SamplerDMWoWvx,float4(float2(0.5,0.0),0,0)).x, tex2Dlod(SamplerDMWoWvx,float4(float2(0.0,1.0),0,0)).x);
		if(Depth_Detection == 2)
		{
			if (DA_DB.x == DA_DB.y)
				DM = 0.0625;
		}
		else
		{   //Ignores Sky
			if (DA_DB.x != 1 && DA_DB.y != 1)
			{
				if (DA_DB.x == DA_DB.y)
					DM = 0.0625;
			}
		}
	}
	#endif

	if (Cancel_Depth)
		DM = 0.0625;

	#if Invert_Depth || IDF
		DM.y = 1 - DM.y;
	#endif

	#if UI_MASK
		DM.y = lerp(DM.y,0,step(1.0-HUD_Mask(texcoord),0.5));
	#endif

	if(LBM || LetterBox_Masking)
	{
		float storeDM = DM.y;
		
		DM.y = texcoord.y > DI_Y && texcoord.y < DI_X ? storeDM : 0.0125;

	#if LBM || LetterBox_Masking
		if((LBM >= 1 || LetterBox_Masking >= 1) && !LBDetection())
			DM.y = storeDM;
	#endif
	}

	return float3(DM.y,PrepDepth(texcoord)[1][1],HandleConvergence.z);
}
////////////////////////////////////////////////////Depth & Special Depth Triggers//////////////////////////////////////////////////////////////////
void Mod_Z(in float4 position : SV_Position, in float2 texcoord : TEXCOORD, out float2 Point_Out : SV_Target0 , out float2 Linear_Out : SV_Target1)
{	
	float3 Set_Depth = DB_Comb( texcoord.xy ).xyz;

	if(1-texcoord.x < pix.x * 2 && 1-texcoord.y < pix.y * 2) //BR
		Set_Depth.y = AltWeapon_Fade();
	if(  texcoord.x < pix.x * 2 && 1-texcoord.y < pix.y * 2) //BL
		Set_Depth.y = Weapon_ZPD_Fade(Set_Depth.z);

	//Luma Map
	float3 Color = tex2D(BackBufferCLAMP,texcoord ).rgb;
		   Color.x = max(Color.r, max(Color.g, Color.b)); 
	
	Point_Out = Set_Depth.xy; 
	Linear_Out = float2(Set_Depth.x,Color.x);//is z when above code is on.	
}

float DB(float2 texcoord)
{
	float2 DepthBuffer_LP = float2(tex2Dlod(SamplerzBufferWoWvx_L, float4( texcoord, 0, Lower_Depth_Res) ).x,tex2Dlod(SamplerzBufferWoWvx_P, float4( texcoord, 0, Lower_Depth_Res) ).x);
	
	if(View_Mode)	
		DepthBuffer_LP.x = DepthBuffer_LP.y;

	float Separation = lerp(1.0,5.0,ZPD_Separation.y); 	
	return 1-(Separation * DepthBuffer_LP.x);
}

float GetDB(float2 texcoord)
{
  float Mask = DB( texcoord.xy );
	if(Depth_Edge_Mask > 0 || Depth_Edge_Mask < 0)
	{
		float t = DB( float2( texcoord.x , texcoord.y - pix.y ) ),
			  d = DB( float2( texcoord.x , texcoord.y + pix.y ) ),
			  l = DB( float2( texcoord.x - pix.x , texcoord.y ) ),
			  r = DB( float2( texcoord.x + pix.x , texcoord.y ) );
		float2 n = float2(t - d,-(r - l));
		// Lets make that mask from Edges
		Mask = length(n)*abs(Depth_Edge_Mask);
		Mask = Mask > 0 ? 1-Mask : 1;
		Mask = saturate(lerp(Mask,1,-1));// Super Evil Mix.
		// Final Depth
		if(Depth_Edge_Mask > 0)
		{
			float N = 0.5,F = 2,M = Mask, Z = (t + d + l + r) * 0.25;
			float ZS = ( Z - N ) / ( F - N);
			ZS += Z;
			ZS *= 0.5;
			Mask = lerp(ZS,DB( texcoord.xy ),Mask);
		}
		else if(Depth_Edge_Mask < 0)
			Mask = lerp(1,DB( texcoord.xy ),Mask);
	}

	return Mask;
}

//////////////////////////////////////////////////////////////HUD Alterations///////////////////////////////////////////////////////////////////////
#if HUD_MODE || HMT
float3 HUD(float3 HUD, float2 texcoord )
{
	float Mask_Tex, CutOFFCal = ((HUD_Adjust.x * 0.5)/DMA()) * 0.5, COC = step(PrepDepth(texcoord)[1][2],CutOFFCal); //HUD Cutoff Calculation
	//This code is for hud segregation.
	if (HUD_Adjust.x > 0)
		HUD = COC > 0 ? tex2D(BackBufferCLAMP,texcoord).rgb : HUD;

	#if UI_MASK
	    if (Mask_Cycle == true)
	        Mask_Tex = tex2Dlod(SamplerMaskB,float4(texcoord.xy,0,0)).a;
	    else
	        Mask_Tex = tex2Dlod(SamplerMaskA,float4(texcoord.xy,0,0)).a;

		float MAC = step(1.0-Mask_Tex,0.5); //Mask Adjustment Calculation
		//This code is for hud segregation.
		HUD = MAC > 0 ? tex2D(BackBufferCLAMP,texcoord).rgb : HUD;
	#endif
	return HUD;
}
#endif
///////////////////////////////////////////////////////////2D + Depth + Signage///////////////////////////////////////////////////////////////////////
float3 PS_calcLR(float2 texcoord)
{//WoWvx Code
float2 ScreenPos = Signage_Flip ? texcoord.xy * Res : float2(texcoord.x,1-texcoord.y) * Res;
float3 Color;
 //BLOCK ONE
 float A = all(abs(float2(0.5,BUFFER_HEIGHT)-ScreenPos.xy) < float2(0.5,1));
 float B = all(abs(float2(2.5,BUFFER_HEIGHT)-ScreenPos.xy) < float2(0.5,1));
 float C = all(abs(float2(4.5,BUFFER_HEIGHT)-ScreenPos.xy) < float2(0.5,1));
 float D = all(abs(float2(6.5,BUFFER_HEIGHT)-ScreenPos.xy) < float2(0.5,1));
 float E = all(abs(float2(14.5,BUFFER_HEIGHT)-ScreenPos.xy) < float2(0.5,1));
 float F = all(abs(float2(32.5,BUFFER_HEIGHT)-ScreenPos.xy) < float2(0.5,1));
 float G = all(abs(float2(34.5,BUFFER_HEIGHT)-ScreenPos.xy) < float2(0.5,1));
 float H = all(abs(float2(36.5,BUFFER_HEIGHT)-ScreenPos.xy) < float2(0.5,1));
 float I = all(abs(float2(38.5,BUFFER_HEIGHT)-ScreenPos.xy) < float2(0.5,1));
 float J = all(abs(float2(40.5,BUFFER_HEIGHT)-ScreenPos.xy) < float2(0.5,1));
 float K = all(abs(float2(42.5,BUFFER_HEIGHT)-ScreenPos.xy) < float2(0.5,1));
 float L = all(abs(float2(44.5,BUFFER_HEIGHT)-ScreenPos.xy) < float2(0.5,1));
 float M = all(abs(float2(46.5,BUFFER_HEIGHT)-ScreenPos.xy) < float2(0.5,1));
 float N = all(abs(float2(48.5,BUFFER_HEIGHT)-ScreenPos.xy) < float2(0.5,1));
 float BlockOne = A+B+C+D+E+F+G+H+I+J+K+L+M+N;

 //NO DEPTH
 float NDA = all(abs(float2(96.5,BUFFER_HEIGHT)-ScreenPos.xy) < float2(0.5,1));
 float NDB = all(abs(float2(98.5,BUFFER_HEIGHT)-ScreenPos.xy) < float2(0.5,1));
 float NDC = all(abs(float2(102.5,BUFFER_HEIGHT)-ScreenPos.xy) < float2(0.5,1));
 float NDD = all(abs(float2(108.5,BUFFER_HEIGHT)-ScreenPos.xy) < float2(0.5,1));
 float NDE = all(abs(float2(110.5,BUFFER_HEIGHT)-ScreenPos.xy) < float2(0.5,1));
 float NDF = all(abs(float2(114.5,BUFFER_HEIGHT)-ScreenPos.xy) < float2(0.5,1));
 float NDG = all(abs(float2(118.5,BUFFER_HEIGHT)-ScreenPos.xy) < float2(0.5,1));
 float NDH = all(abs(float2(124.5,BUFFER_HEIGHT)-ScreenPos.xy) < float2(0.5,1));
 float NDI = all(abs(float2(126.5,BUFFER_HEIGHT)-ScreenPos.xy) < float2(0.5,1));
 float NDJ = all(abs(float2(128.5,BUFFER_HEIGHT)-ScreenPos.xy) < float2(0.5,1));
 float NDK = all(abs(float2(138.5,BUFFER_HEIGHT)-ScreenPos.xy) < float2(0.5,1));
 float NDL = all(abs(float2(152.5,BUFFER_HEIGHT)-ScreenPos.xy) < float2(0.5,1));
 float NDM = all(abs(float2(154.5,BUFFER_HEIGHT)-ScreenPos.xy) < float2(0.5,1));
 float NoDepth = NDA+NDB+NDC+NDD+NDE+NDF+NDG+NDH+NDI+NDJ+NDK+NDL+NDM;

 //RESERVED
 float RA = all(abs(float2(26.5,BUFFER_HEIGHT)-ScreenPos.xy) < float2(0.5,1));
 float RB = all(abs(float2(28.5,BUFFER_HEIGHT)-ScreenPos.xy) < float2(0.5,1));
 float RC = all(abs(float2(98.5,BUFFER_HEIGHT)-ScreenPos.xy) < float2(0.5,1));
 float RD = all(abs(float2(100.5,BUFFER_HEIGHT)-ScreenPos.xy) < float2(0.5,1));
 float RE = all(abs(float2(110.5,BUFFER_HEIGHT)-ScreenPos.xy) < float2(0.5,1));
 float RF = all(abs(float2(112.5,BUFFER_HEIGHT)-ScreenPos.xy) < float2(0.5,1));
 float RG = all(abs(float2(116.5,BUFFER_HEIGHT)-ScreenPos.xy) < float2(0.5,1));
 float RH = all(abs(float2(118.5,BUFFER_HEIGHT)-ScreenPos.xy) < float2(0.5,1));
 float RI = all(abs(float2(120.5,BUFFER_HEIGHT)-ScreenPos.xy) < float2(0.5,1));
 float RJ = all(abs(float2(122.5,BUFFER_HEIGHT)-ScreenPos.xy) < float2(0.5,1));
 float RK = all(abs(float2(126.5,BUFFER_HEIGHT)-ScreenPos.xy) < float2(0.5,1));
 float RL = all(abs(float2(128.5,BUFFER_HEIGHT)-ScreenPos.xy) < float2(0.5,1));
 float RM = all(abs(float2(130.5,BUFFER_HEIGHT)-ScreenPos.xy) < float2(0.5,1));
 float RN = all(abs(float2(136.5,BUFFER_HEIGHT)-ScreenPos.xy) < float2(0.5,1));
 float RO = all(abs(float2(144.5,BUFFER_HEIGHT)-ScreenPos.xy) < float2(0.5,1));
 float RP = all(abs(float2(150.5,BUFFER_HEIGHT)-ScreenPos.xy) < float2(0.5,1));
 float RQ = all(abs(float2(154.5,BUFFER_HEIGHT)-ScreenPos.xy) < float2(0.5,1));
 float RR = all(abs(float2(158.5,BUFFER_HEIGHT)-ScreenPos.xy) < float2(0.5,1));
 float Reserved = RA+RB+RC+RD+RE+RF+RG+RH+RI+RJ+RK+RL+RM+RN+RO+RP+RQ+RR;

 //STILL
 float SA = all(abs(float2(26.5,BUFFER_HEIGHT)-ScreenPos.xy) < float2(0.5,1));
 float SB = all(abs(float2(30.5,BUFFER_HEIGHT)-ScreenPos.xy) < float2(0.5,1));
 float SC = all(abs(float2(96.5,BUFFER_HEIGHT)-ScreenPos.xy) < float2(0.5,1));
 float SD = all(abs(float2(100.5,BUFFER_HEIGHT)-ScreenPos.xy) < float2(0.5,1));
 float SE = all(abs(float2(102.5,BUFFER_HEIGHT)-ScreenPos.xy) < float2(0.5,1));
 float SF = all(abs(float2(104.5,BUFFER_HEIGHT)-ScreenPos.xy) < float2(0.5,1));
 float SG = all(abs(float2(108.5,BUFFER_HEIGHT)-ScreenPos.xy) < float2(0.5,1));
 float SH = all(abs(float2(112.5,BUFFER_HEIGHT)-ScreenPos.xy) < float2(0.5,1));
 float SI = all(abs(float2(116.5,BUFFER_HEIGHT)-ScreenPos.xy) < float2(0.5,1));
 float SJ = all(abs(float2(120.5,BUFFER_HEIGHT)-ScreenPos.xy) < float2(0.5,1));
 float SK = all(abs(float2(124.5,BUFFER_HEIGHT)-ScreenPos.xy) < float2(0.5,1));
 float SL = all(abs(float2(130.5,BUFFER_HEIGHT)-ScreenPos.xy) < float2(0.5,1));
 float SM = all(abs(float2(132.5,BUFFER_HEIGHT)-ScreenPos.xy) < float2(0.5,1));
 float SN = all(abs(float2(156.5,BUFFER_HEIGHT)-ScreenPos.xy) < float2(0.5,1));
 float Still = SA+SB+SC+SD+SE+SF+SG+SH+SI+SJ+SK+SL+SM+SN;

 //CGI
 float CA = all(abs(float2(26.5,BUFFER_HEIGHT)-ScreenPos.xy) < float2(0.5,1));
 float CB = all(abs(float2(96.5,BUFFER_HEIGHT)-ScreenPos.xy) < float2(0.5,1));
 float CC = all(abs(float2(98.5,BUFFER_HEIGHT)-ScreenPos.xy) < float2(0.5,1));
 float CD = all(abs(float2(100.5,BUFFER_HEIGHT)-ScreenPos.xy) < float2(0.5,1));
 float CE = all(abs(float2(102.5,BUFFER_HEIGHT)-ScreenPos.xy) < float2(0.5,1));
 float CF = all(abs(float2(108.5,BUFFER_HEIGHT)-ScreenPos.xy) < float2(0.5,1));
 float CG = all(abs(float2(110.5,BUFFER_HEIGHT)-ScreenPos.xy) < float2(0.5,1));
 float CH = all(abs(float2(112.5,BUFFER_HEIGHT)-ScreenPos.xy) < float2(0.5,1));
 float CI = all(abs(float2(116.5,BUFFER_HEIGHT)-ScreenPos.xy) < float2(0.5,1));
 float CJ = all(abs(float2(122.5,BUFFER_HEIGHT)-ScreenPos.xy) < float2(0.5,1));
 float CK = all(abs(float2(124.5,BUFFER_HEIGHT)-ScreenPos.xy) < float2(0.5,1));
 float CL = all(abs(float2(126.5,BUFFER_HEIGHT)-ScreenPos.xy) < float2(0.5,1));
 float CM = all(abs(float2(138.5,BUFFER_HEIGHT)-ScreenPos.xy) < float2(0.5,1));
 float CN = all(abs(float2(140.5,BUFFER_HEIGHT)-ScreenPos.xy) < float2(0.5,1));
 float CO = all(abs(float2(142.5,BUFFER_HEIGHT)-ScreenPos.xy) < float2(0.5,1));
 float CP = all(abs(float2(144.5,BUFFER_HEIGHT)-ScreenPos.xy) < float2(0.5,1));
 float CQ = all(abs(float2(152.5,BUFFER_HEIGHT)-ScreenPos.xy) < float2(0.5,1));
 float CR = all(abs(float2(154.5,BUFFER_HEIGHT)-ScreenPos.xy) < float2(0.5,1));
 float CS = all(abs(float2(156.5,BUFFER_HEIGHT)-ScreenPos.xy) < float2(0.5,1));
 float CT = all(abs(float2(158.5,BUFFER_HEIGHT)-ScreenPos.xy) < float2(0.5,1));
 float CGI = CA+CB+CC+CD+CE+CF+CG+CH+CI+CJ+CK+CL+CM+CN+CO+CP+CQ+CR+CS+CT;

 //GAME
 float GA = all(abs(float2(28.5,BUFFER_HEIGHT)-ScreenPos.xy) < float2(0.5,1));
 float GB = all(abs(float2(30.5,BUFFER_HEIGHT)-ScreenPos.xy) < float2(0.5,1));
 float GC = all(abs(float2(104.5,BUFFER_HEIGHT)-ScreenPos.xy) < float2(0.5,1));
 float GD = all(abs(float2(114.5,BUFFER_HEIGHT)-ScreenPos.xy) < float2(0.5,1));
 float GE = all(abs(float2(122.5,BUFFER_HEIGHT)-ScreenPos.xy) < float2(0.5,1));
 float GF = all(abs(float2(132.5,BUFFER_HEIGHT)-ScreenPos.xy) < float2(0.5,1));
 float GG = all(abs(float2(136.5,BUFFER_HEIGHT)-ScreenPos.xy) < float2(0.5,1));
 float GH = all(abs(float2(138.5,BUFFER_HEIGHT)-ScreenPos.xy) < float2(0.5,1));
 float GI = all(abs(float2(144.5,BUFFER_HEIGHT)-ScreenPos.xy) < float2(0.5,1));
 float GJ = all(abs(float2(150.5,BUFFER_HEIGHT)-ScreenPos.xy) < float2(0.5,1));
 float GK = all(abs(float2(152.5,BUFFER_HEIGHT)-ScreenPos.xy) < float2(0.5,1));
 float GL = all(abs(float2(156.5,BUFFER_HEIGHT)-ScreenPos.xy) < float2(0.5,1));
 float GM = all(abs(float2(158.5,BUFFER_HEIGHT)-ScreenPos.xy) < float2(0.5,1));
 float Game = GA+GB+GB+GC+GD+GE+GF+GG+GH+GI+GJ+GK+GL+GM;

 //MOVIE
 float MA = all(abs(float2(28.5,BUFFER_HEIGHT)-ScreenPos.xy) < float2(0.5,1));
 float MB = all(abs(float2(98.5,BUFFER_HEIGHT)-ScreenPos.xy) < float2(0.5,1));
 float MC = all(abs(float2(110.5,BUFFER_HEIGHT)-ScreenPos.xy) < float2(0.5,1));
 float MD = all(abs(float2(114.5,BUFFER_HEIGHT)-ScreenPos.xy) < float2(0.5,1));
 float ME = all(abs(float2(120.5,BUFFER_HEIGHT)-ScreenPos.xy) < float2(0.5,1));
 float MF = all(abs(float2(126.5,BUFFER_HEIGHT)-ScreenPos.xy) < float2(0.5,1));
 float MG = all(abs(float2(130.5,BUFFER_HEIGHT)-ScreenPos.xy) < float2(0.5,1));
 float MH = all(abs(float2(136.5,BUFFER_HEIGHT)-ScreenPos.xy) < float2(0.5,1));
 float MI = all(abs(float2(140.5,BUFFER_HEIGHT)-ScreenPos.xy) < float2(0.5,1));
 float MJ = all(abs(float2(142.5,BUFFER_HEIGHT)-ScreenPos.xy) < float2(0.5,1));
 float MK = all(abs(float2(150.5,BUFFER_HEIGHT)-ScreenPos.xy) < float2(0.5,1));
 float ML = all(abs(float2(154.5,BUFFER_HEIGHT)-ScreenPos.xy) < float2(0.5,1));
 float MM = all(abs(float2(156.5,BUFFER_HEIGHT)-ScreenPos.xy) < float2(0.5,1));
 float Movie = MA+MB+MC+MD+ME+MF+MG+MH+MI+MJ+MK+ML+MM;

 //SIGNAGE
 float SSA = all(abs(float2(30.5,BUFFER_HEIGHT)-ScreenPos.xy) < float2(0.5,1));
 float SSB = all(abs(float2(96.5,BUFFER_HEIGHT)-ScreenPos.xy) < float2(0.5,1));
 float SSC = all(abs(float2(102.5,BUFFER_HEIGHT)-ScreenPos.xy) < float2(0.5,1));
 float SSD = all(abs(float2(104.5,BUFFER_HEIGHT)-ScreenPos.xy) < float2(0.5,1));
 float SSE = all(abs(float2(108.5,BUFFER_HEIGHT)-ScreenPos.xy) < float2(0.5,1));
 float SSF = all(abs(float2(114.5,BUFFER_HEIGHT)-ScreenPos.xy) < float2(0.5,1));
 float SSG = all(abs(float2(118.5,BUFFER_HEIGHT)-ScreenPos.xy) < float2(0.5,1));
 float SSH = all(abs(float2(120.5,BUFFER_HEIGHT)-ScreenPos.xy) < float2(0.5,1));
 float SSI = all(abs(float2(122.5,BUFFER_HEIGHT)-ScreenPos.xy) < float2(0.5,1));
 float SSJ = all(abs(float2(124.5,BUFFER_HEIGHT)-ScreenPos.xy) < float2(0.5,1));
 float SSK = all(abs(float2(128.5,BUFFER_HEIGHT)-ScreenPos.xy) < float2(0.5,1));
 float SSL = all(abs(float2(130.5,BUFFER_HEIGHT)-ScreenPos.xy) < float2(0.5,1));
 float SSM = all(abs(float2(132.5,BUFFER_HEIGHT)-ScreenPos.xy) < float2(0.5,1));
 float SSN = all(abs(float2(140.5,BUFFER_HEIGHT)-ScreenPos.xy) < float2(0.5,1));
 float SSO = all(abs(float2(142.5,BUFFER_HEIGHT)-ScreenPos.xy) < float2(0.5,1));
 float SSP = all(abs(float2(144.5,BUFFER_HEIGHT)-ScreenPos.xy) < float2(0.5,1));
 float SSQ = all(abs(float2(158.5,BUFFER_HEIGHT)-ScreenPos.xy) < float2(0.5,1));
 float Signage = SSA+SSB+SSC+SSD+SSE+SSF+SSG+SSH+SSI+SSJ+SSK+SSL+SSM+SSN+SSO+SSP+SSQ;

 float Content_Type;

 if (Content == 0)
   Content_Type = NoDepth;
 else if (Content == 1)
   Content_Type = Reserved;
 else if (Content == 2)
   Content_Type = Still;
 else if (Content == 3)
   Content_Type = CGI;
 else if (Content == 4)
   Content_Type = Game;
 else if (Content == 5)
   Content_Type = Movie;
 else
   Content_Type = Signage;

Color = MouseCursor(float2(texcoord.x*2 + Perspective * pix.x,texcoord.y)).rgb;

	#if HUD_MODE || HMT
	float HUD_Adjustment = ((0.5 - HUD_Adjust.y)*25.) * pix.x;
	Color.rgb = HUD(Color.rgb,float2(texcoord.x - HUD_Adjustment,texcoord.y)).rgb;
	#endif

 Color = texcoord.x < 0.5 ? Color : GetDB(float2(texcoord.x*2-1 + Perspective * pix.x,texcoord.y)).x;

 float3 BACK = all(abs(float2(80,BUFFER_HEIGHT)-ScreenPos.xy) < float2(80,1) ) ? 0 : Color;

 return BlockOne + Content_Type ? float3(0,0,1) : BACK;
}
/////////////////////////////////////////////////////////Average Luminance Textures/////////////////////////////////////////////////////////////////
float4 Average_Luminance(float4 position : SV_Position, float2 texcoord : TEXCOORD) : SV_Target
{
	float4 ABEA, ABEArray[6] = {
		float4(0.0,1.0,0.0, 1.0),           //No Edit
		float4(0.0,1.0,0.0, 0.750),         //Upper Extra Wide
		float4(0.0,1.0,0.0, 0.5),           //Upper Wide
		float4(0.0,1.0, 0.15625, 0.46875),  //Upper Short
		float4(0.375, 0.250, 0.4375, 0.125),//Center Small
		float4(0.375, 0.250, 0.0, 1.0)      //Center Long
	};
	ABEA = ABEArray[Auto_Balance_Ex];

	float Average_Lum_ZPD = PrepDepth(float2(ABEA.x + texcoord.x * ABEA.y, ABEA.z + texcoord.y * ABEA.w))[0][0], Average_Lum_Bottom = PrepDepth( texcoord )[0][0];

	const int Num_of_Values = 6; //6 total array values that map to the textures width.
	float Storage__Array[Num_of_Values] = { tex2D(SamplerDMWoWvx,0).x,    //0.083
                                tex2D(SamplerDMWoWvx,1).x,                //0.250
                                tex2D(SamplerDMWoWvx,int2(0,1)).x,        //0.416
                                1.0,                                  //0.583
								tex2D(SamplerzBufferWoWvx_P,1).y,         //0.750
								tex2D(SamplerzBufferWoWvx_P,int2(0,1)).y};//0.916

	//Set a avr size for the Number of lines needed in texture storage.
	float Grid = floor(texcoord.y * BUFFER_HEIGHT * BUFFER_RCP_HEIGHT * Num_of_Values);

	return float4(Average_Lum_ZPD,Average_Lum_Bottom,Storage__Array[int(fmod(Grid,Num_of_Values))],tex2Dlod(SamplerDMWoWvx,float4(texcoord,0,0)).y);
}
////////////////////////////////////////////////////////////////////Logo////////////////////////////////////////////////////////////////////////////
#define _f float // Text rendering code copied/pasted from https://www.shadertoy.com/view/4dtGD2 by Hamneggs
static const _f CH_A    = _f(0x69f99), CH_B    = _f(0x79797), CH_C    = _f(0xe111e),
				CH_D    = _f(0x79997), CH_E    = _f(0xf171f), CH_F    = _f(0xf1711),
				CH_G    = _f(0xe1d96), CH_H    = _f(0x99f99), CH_I    = _f(0xf444f),
				CH_J    = _f(0x88996), CH_K    = _f(0x95159), CH_L    = _f(0x1111f),
				CH_M    = _f(0x9fd99), CH_N    = _f(0x9bd99), CH_O    = _f(0x69996),
				CH_P    = _f(0x79971), CH_Q    = _f(0x69b5a), CH_R    = _f(0x79759),
				CH_S    = _f(0xe1687), CH_T    = _f(0xf4444), CH_U    = _f(0x99996),
				CH_V    = _f(0x999a4), CH_W    = _f(0x999f9), CH_X    = _f(0x99699),
				CH_Y    = _f(0x99e8e), CH_Z    = _f(0xf843f), CH_0    = _f(0x6bd96),
				CH_1    = _f(0x46444), CH_2    = _f(0x6942f), CH_3    = _f(0x69496),
				CH_4    = _f(0x99f88), CH_5    = _f(0xf1687), CH_6    = _f(0x61796),
				CH_7    = _f(0xf8421), CH_8    = _f(0x69696), CH_9    = _f(0x69e84),
				CH_APST = _f(0x66400), CH_PI   = _f(0x0faa9), CH_UNDS = _f(0x0000f),
				CH_HYPH = _f(0x00600), CH_TILD = _f(0x0a500), CH_PLUS = _f(0x02720),
				CH_EQUL = _f(0x0f0f0), CH_SLSH = _f(0x08421), CH_EXCL = _f(0x33303),
				CH_QUES = _f(0x69404), CH_COMM = _f(0x00032), CH_FSTP = _f(0x00002),
				CH_QUOT = _f(0x55000), CH_BLNK = _f(0x00000), CH_COLN = _f(0x00202),
				CH_LPAR = _f(0x42224), CH_RPAR = _f(0x24442);
#define MAP_SIZE float2(4,5)
#undef flt
//returns the status of a bit in a bitmap. This is done value-wise, so the exact representation of the float doesn't really matter.
float getBit( float map, float index )
{   // Ooh -index takes out that divide :)
    return fmod( floor( map * exp2(-index) ), 2.0 );
}

float drawChar( float Char, float2 pos, float2 size, float2 TC )
{   // Subtract our position from the current TC so that we can know if we're inside the bounding box or not.
    TC -= pos;
    // Divide the screen space by the size, so our bounding box is 1x1.
    TC /= size;
    // Create a place to store the result & Branchless bounding box check.
    float res = step(0.0,min(TC.x,TC.y)) - step(1.0,max(TC.x,TC.y));
    // Go ahead and multiply the TC by the bitmap size so we can work in bitmap space coordinates.
    TC *= MAP_SIZE;
    // Get the appropriate bit and return it.
    res*=getBit( Char, 4.0*floor(TC.y) + floor(TC.x) );
    return saturate(res);
}

float3 Out(float4 position : SV_Position, float2 texcoord : TEXCOORD) : SV_Target
{
	float2 TC = float2(texcoord.x,1-texcoord.y);
	float Text_Timer = 25000, BT = smoothstep(0,1,sin(timer*(3.75/1000))), Size = 1.1, Depth3D, Read_Help, Supported, ET, ETC, ETTF, ETTC, SetFoV, FoV, Post, Effect, NoPro, NotCom, Mod, Needs, Net, Over, Set, AA, Emu, Not, No, Help, Fix, Need, State, SetAA, SetWP, Work;
	float3 Color = PS_calcLR(texcoord).rgb; //Color = BlurZ(SamplerzBufferN,texcoord);
		  
	if(RHW || NCW || NPW || NFM || PEW || DSW || OSW || DAA || NDW || WPW || FOV || EDW)
		Text_Timer = 30000;

	[branch] if(timer <= Text_Timer || Text_Info)
	{   // Set a general character size...
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
		//Supported Emulator Detected
		charPos = float2( 0.009, 0.9375);
		Supported += drawChar( CH_S, charPos, charSize, TC); charPos.x += .01 * Size;
		Supported += drawChar( CH_U, charPos, charSize, TC); charPos.x += .01 * Size;
		Supported += drawChar( CH_P, charPos, charSize, TC); charPos.x += .01 * Size;
		Supported += drawChar( CH_P, charPos, charSize, TC); charPos.x += .01 * Size;
		Supported += drawChar( CH_O, charPos, charSize, TC); charPos.x += .01 * Size;
		Supported += drawChar( CH_R, charPos, charSize, TC); charPos.x += .01 * Size;
		Supported += drawChar( CH_T, charPos, charSize, TC); charPos.x += .01 * Size;
		Supported += drawChar( CH_E, charPos, charSize, TC); charPos.x += .01 * Size;
		Supported += drawChar( CH_D, charPos, charSize, TC); charPos.x += .01 * Size;
		Supported += drawChar( CH_BLNK, charPos, charSize, TC); charPos.x += .01 * Size;
		Supported += drawChar( CH_E, charPos, charSize, TC); charPos.x += .01 * Size;
		Supported += drawChar( CH_M, charPos, charSize, TC); charPos.x += .01 * Size;
		Supported += drawChar( CH_U, charPos, charSize, TC); charPos.x += .01 * Size;
		Supported += drawChar( CH_L, charPos, charSize, TC); charPos.x += .01 * Size;
		Supported += drawChar( CH_A, charPos, charSize, TC); charPos.x += .01 * Size;
		Supported += drawChar( CH_T, charPos, charSize, TC); charPos.x += .01 * Size;
		Supported += drawChar( CH_O, charPos, charSize, TC); charPos.x += .01 * Size;
		Supported += drawChar( CH_R, charPos, charSize, TC); charPos.x += .01 * Size;
		Supported += drawChar( CH_BLNK, charPos, charSize, TC); charPos.x += .01 * Size;
		Supported += drawChar( CH_D, charPos, charSize, TC); charPos.x += .01 * Size;
		Supported += drawChar( CH_E, charPos, charSize, TC); charPos.x += .01 * Size;
		Supported += drawChar( CH_T, charPos, charSize, TC); charPos.x += .01 * Size;
		Supported += drawChar( CH_E, charPos, charSize, TC); charPos.x += .01 * Size;
		Supported += drawChar( CH_C, charPos, charSize, TC); charPos.x += .01 * Size;
		Supported += drawChar( CH_T, charPos, charSize, TC); charPos.x += .01 * Size;
		Supported += drawChar( CH_E, charPos, charSize, TC); charPos.x += .01 * Size;
		Supported += drawChar( CH_D, charPos, charSize, TC);
		//Disable CA/MB/Dof/Grain
		charPos = float2( 0.009, 0.920);
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
		//Disable Or Set TAA/MSAA/AA/DLSS
		charPos = float2( 0.009, 0.9025);
		SetAA += drawChar( CH_D, charPos, charSize, TC); charPos.x += .01 * Size;
		SetAA += drawChar( CH_I, charPos, charSize, TC); charPos.x += .01 * Size;
		SetAA += drawChar( CH_S, charPos, charSize, TC); charPos.x += .01 * Size;
		SetAA += drawChar( CH_A, charPos, charSize, TC); charPos.x += .01 * Size;
		SetAA += drawChar( CH_B, charPos, charSize, TC); charPos.x += .01 * Size;
		SetAA += drawChar( CH_L, charPos, charSize, TC); charPos.x += .01 * Size;
		SetAA += drawChar( CH_E, charPos, charSize, TC); charPos.x += .01 * Size;
		SetAA += drawChar( CH_BLNK, charPos, charSize, TC); charPos.x += .01 * Size;
		SetAA += drawChar( CH_O, charPos, charSize, TC); charPos.x += .01 * Size;
		SetAA += drawChar( CH_R, charPos, charSize, TC); charPos.x += .01 * Size;
		SetAA += drawChar( CH_BLNK, charPos, charSize, TC); charPos.x += .01 * Size;
		SetAA += drawChar( CH_S, charPos, charSize, TC); charPos.x += .01 * Size;
		SetAA += drawChar( CH_E, charPos, charSize, TC); charPos.x += .01 * Size;
		SetAA += drawChar( CH_T, charPos, charSize, TC); charPos.x += .01 * Size;
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
		SetAA += drawChar( CH_A, charPos, charSize, TC); charPos.x += .01 * Size;
		SetAA += drawChar( CH_SLSH, charPos, charSize, TC); charPos.x += .01 * Size;
		SetAA += drawChar( CH_D, charPos, charSize, TC); charPos.x += .01 * Size;
		SetAA += drawChar( CH_L, charPos, charSize, TC); charPos.x += .01 * Size;
		SetAA += drawChar( CH_S, charPos, charSize, TC); charPos.x += .01 * Size;
		SetAA += drawChar( CH_S, charPos, charSize, TC);
		//Set Weapon Profile
		charPos = float2( 0.009, 0.885);
		SetWP += drawChar( CH_S, charPos, charSize, TC); charPos.x += .01 * Size;
		SetWP += drawChar( CH_E, charPos, charSize, TC); charPos.x += .01 * Size;
		SetWP += drawChar( CH_T, charPos, charSize, TC); charPos.x += .01 * Size;
		SetWP += drawChar( CH_BLNK, charPos, charSize, TC); charPos.x += .01 * Size;
		SetWP += drawChar( CH_W, charPos, charSize, TC); charPos.x += .01 * Size;
		SetWP += drawChar( CH_E, charPos, charSize, TC); charPos.x += .01 * Size;
		SetWP += drawChar( CH_A, charPos, charSize, TC); charPos.x += .01 * Size;
		SetWP += drawChar( CH_P, charPos, charSize, TC); charPos.x += .01 * Size;
		SetWP += drawChar( CH_O, charPos, charSize, TC); charPos.x += .01 * Size;
		SetWP += drawChar( CH_N, charPos, charSize, TC); charPos.x += .01 * Size;
		SetWP += drawChar( CH_BLNK, charPos, charSize, TC); charPos.x += .01 * Size;
		SetWP += drawChar( CH_P, charPos, charSize, TC); charPos.x += .01 * Size;
		SetWP += drawChar( CH_R, charPos, charSize, TC); charPos.x += .01 * Size;
		SetWP += drawChar( CH_O, charPos, charSize, TC); charPos.x += .01 * Size;
		SetWP += drawChar( CH_F, charPos, charSize, TC); charPos.x += .01 * Size;
		SetWP += drawChar( CH_I, charPos, charSize, TC); charPos.x += .01 * Size;
		SetWP += drawChar( CH_L, charPos, charSize, TC); charPos.x += .01 * Size;
		SetWP += drawChar( CH_E, charPos, charSize, TC);
		//Set FoV
		charPos = float2( 0.009, 0.8675);
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
		if(DSW)
			Need = Needs;
		if(RHW)
			Help = Read_Help;
		if(NDW)
			Net = Work;
		if(PEW)
			Post = Effect;
		if(WPW)
			Set = SetWP;
		if(DAA)
			AA = SetAA;
		if(FOV)
			FoV = SetFoV;
		if(EDW)
			Emu = Supported;
		//Blinking Text Warnings
		if(NPW)
			No = NoPro * BT;
		if(NCW)
			Not = NotCom * BT;
		if(NFM)
			Fix = Mod * BT;
		if(OSW)
			Over = State * BT;
		//Website
		return Depth3D+Help+Post+No+Not+Net+Fix+Need+Over+AA+Set+FoV+Emu+ET ? (1-texcoord.y*50.0+48.85)*texcoord.y-0.500: Color;
	}
	else
		return Color;
}
///////////////////////////////////////////////////////////////////ReShade.fxh//////////////////////////////////////////////////////////////////////
void PostProcessVS(in uint id : SV_VertexID, out float4 position : SV_Position, out float2 texcoord : TEXCOORD)
{// Vertex shader generating a triangle covering the entire screen
	texcoord.x = (id == 2) ? 2.0 : 0.0;
	texcoord.y = (id == 1) ? 2.0 : 0.0;
	position = float4(texcoord * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
}

technique SuperDepth3D_WoWvx
< ui_tooltip = "Suggestion : You Can Enable 'Performance Mode Checkbox,' in the lower bottom right of the ReShade's Main UI.\n"
			   			 "Do this once you set your 3D settings of course."; >
{
	#if D_Frame || DFW
		pass Delay_Frame
	{
		VertexShader = PostProcessVS;
		PixelShader = DelayFrame;
		RenderTarget = texDF;
	}
		pass Current_Frame
	{
		VertexShader = PostProcessVS;
		PixelShader = CurrentFrame;
		RenderTarget = texCF;
	}
	#else
		pass AverageLuminance
	{
		VertexShader = PostProcessVS;
		PixelShader = Average_Luminance;
		RenderTarget = texLumWoWvx;
	}
	#endif
		pass DepthBuffer
	{
		VertexShader = PostProcessVS;
		PixelShader = DepthMap;
		RenderTarget = texDMWoWvx;
	}	
		pass Modzbuffer
	{
		VertexShader = PostProcessVS;
		PixelShader = Mod_Z;
		RenderTarget0 = texzBufferWoWvx_P;
		RenderTarget1 = texzBufferWoWvx_L;
	}
		pass StereoOut
	{
		VertexShader = PostProcessVS;
		PixelShader = Out;
	}
	#if D_Frame || DFW
		pass AverageLuminance
	{
		VertexShader = PostProcessVS;
		PixelShader = Average_Luminance;
		RenderTarget = texLumWoWvx;
	}
	#endif
}
