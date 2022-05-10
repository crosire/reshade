////----------------//
///**SuperDepth3D**///
//----------------////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//* Depth Map Based 3D post-process shader v3.1.8
//* For Reshade 3.0+
//* ---------------------------------
//*
//* Original work was based on the shader code from
//* Also Fu-Bama a shader dev at the reshade forums https://reshade.me/forum/shader-presentation/5104-vr-universal-shader
//* Also had to rework Philippe David http://graphics.cs.brown.edu/games/SteepParallax/index.html code to work with ReShade. This is used for the parallax effect.
//* This idea was taken from this shader here located at https://github.com/Fubaxiusz/fubax-shaders/blob/596d06958e156d59ab6cd8717db5f442e95b2e6b/Shaders/VR.fx#L395
//* It's also based on Philippe David Steep Parallax mapping code.
//* Text rendering code Ported from https://www.shadertoy.com/view/4dtGD2 by Hamneggs for ReShadeFX
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
	static const float DF_X = 0.0, DF_Y = 0.0, DF_Z = 0.5, DF_W = 0.0;
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
	static const int REF = 0, NCW = 0, RHW = 0, NPW = 0, IDF = 0, SPF = 0, BDF = 0, HMT = 0, DFW = 0, NFM = 0, DSW = 0, BMT = 0, LBC = 0, LBS = 0, LBM = 0, DAA = 0, NDW = 0, PEW = 0, WPW = 0, FOV = 0, EDW = 0, SDT = 0;
	//Overwatch.fxh State
	#define OSW 1
#endif
//USER EDITABLE PREPROCESSOR FUNCTIONS START//

// Zero Parallax Distance Balance Mode allows you to switch control from manual to automatic and vice versa.
#define Balance_Mode 0 //Default 0 is Automatic. One is Manual.

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
	#define Max_Divergence 125.0//Wow Must be the future and 8K Plus is normal now. If you are here use AI infilling...... Future person.
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
//uniform float TEST < ui_type = "drag"; ui_min = 0; ui_max = 1; > = 1.0;
//Divergence & Convergence//
uniform float Divergence <
	ui_type = "drag";
	ui_min = 1.0; ui_max = Max_Divergence; ui_step = 0.5;
	ui_label = "·Divergence Slider·";
	ui_tooltip = "Divergence increases differences between the left and right retinal images and allows you to experience depth.\n"
				 "The process of deriving binocular depth information is called stereopsis.";
	ui_category = "Divergence & Convergence";
> = Set_Divergence;

uniform float2 ZPD_Separation <
	ui_type = "drag";
	ui_min = 0.0; ui_max = 0.250;
	ui_label = " ZPD & Sepration";
	ui_tooltip = "Zero Parallax Distance controls the focus distance for the screen Pop-out effect also known as Convergence.\n"
				"Separation is a way to increase the intensity of Divergence without a performance cost.\n"
				"For FPS Games keeps this low Since you don't want your gun to pop out of screen.\n"
				"Default is 0.025, Zero is off.";
	ui_category = "Divergence & Convergence";
> = float2(DA_X,DF_Y);

#if Balance_Mode || BMT
uniform float ZPD_Balance <
	ui_type = "drag";
	ui_min = 0.0; ui_max = 1.0;
	ui_label = " ZPD Balance";
	ui_tooltip = "Zero Parallax Distance balances between ZPD Depth and Scene Depth.\n"
				"Default is Zero is full Convergence and One is Full Depth.";
	ui_category = "Divergence & Convergence";
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
	ui_category = "Divergence & Convergence";
> = DB_Y;
#endif
uniform int ZPD_Boundary <
	ui_type = "combo";
	ui_items = "BD0 Off\0BD1 Full\0BD2 Narrow\0BD3 Wide\0BD4 FPS Center\0BD5 FPS Narrow\0BD6 FPS Edge\0BD7 FPS Mixed\0";
	ui_label = " ZPD Boundary Detection";
	ui_tooltip = "This selection menu gives extra boundary conditions to ZPD.\n"
				 			 "This treats your screen as a virtual wall.\n"
				 		   "Default is Off.";
	ui_category = "Divergence & Convergence";
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
	ui_category = "Divergence & Convergence";
> = float2(DE_Y,DE_Z);

uniform int View_Mode <
	ui_type = "combo";
	ui_items = "VM0 Normal \0VM1 Alpha \0VM2 Reiteration \0VM3 Stamped \0VM4 Mixed \0VM5 Adaptive \0";
	ui_label = "·View Mode·";
	ui_tooltip = "Changes the way the shader fills in the occlude sections in the image.\n"
				"Normal      | Normal output used for most games with it's streched look.\n"
				"Alpha       | Like Normal But with a bit more sepration in the gap filling.\n"
				"Reiteration | Same thing as Stamped but with brakeage points.\n"
				"Stamped     | Stamps out a transparent area on the occluded area.\n"
				"Mixed       | Used for higher amounts of Semi-Transparent objects like foliage.\n"
				"Adaptive    | is a scene adapting infilling that uses disruptive reiterative sampling.\n"
				"\n"
				"Warning: Also Make sure you turn on Performance Mode before you close this menu.\n"
				"\n"
				"Default is Normal.";
ui_category = "Occlusion Masking";
> = 0;

uniform int Custom_Sidebars <
	ui_type = "combo";
	ui_items = "Mirrored Edges\0Black Edges\0Stretched Edges\0";
	ui_label = " Edge Handling";
	ui_tooltip = "Edges selection for your screen output.";
	ui_category = "Occlusion Masking";
> = 1;

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

uniform int Performance_Level <
	ui_type = "combo";
	ui_items = "Performant\0Normal\0";
	ui_label = " Performance Mode";
	ui_tooltip = "Performance Mode Lowers or Raises Occlusion Quality Processing so that there is a performance is adjustable.\n"
				 "Please enable the 'Performance Mode Checkbox,' in ReShade's GUI.\n"
				 "It's located in the lower bottom right of the ReShade's Main UI.\n"
				 "Default is Normal.";
	ui_category = "Occlusion Masking";
> = 1;

uniform float Compatibility_Power <
	#if Compatibility
	ui_type = "drag";
	#else
	ui_type = "slider";
	#endif
	ui_min = 0.0; ui_max = 1.0;
	ui_label = " Compatibility Power";
	ui_tooltip = "Not all games need a high offset for infilling.\n"
				 "This option lets you increase this offset in both directions to limit artifacts.\n"
				 "With this on it should work better in games with TAA, FSR,and or DLSS sometimes.\n"
				 "Default is 0.625.";
	ui_category = "Compatibility Options";
> = 0.625;

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
	ui_category = "Compatibility Options";
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
uniform int Depth_Map_View <
	ui_type = "combo";
	ui_items = "Off\0Stereo Depth View\0Normal Depth View\0";
	ui_label = " Depth Map View";
	ui_tooltip = "Display the Depth Map";
	ui_category = "Depth Map";
> = 0;

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
//Stereoscopic Options//
uniform int Stereoscopic_Mode <
	ui_type = "combo";
	ui_items = "Side by Side\0Top and Bottom\0Line Interlaced\0Column Interlaced\0Checkerboard 3D\0Autostereoscopic\0Quad Lightfield 2x2\0Anaglyph 3D Red/Cyan\0Anaglyph 3D Red/Cyan Dubois\0Anaglyph 3D Red/Cyan Anachrome\0Anaglyph 3D Green/Magenta\0Anaglyph 3D Green/Magenta Dubois\0Anaglyph 3D Green/Magenta Triochrome\0Anaglyph 3D Blue/Amber ColorCode\0";
	ui_label = "·3D Display Modes·";
	ui_tooltip = "Stereoscopic 3D display output selection.";
	ui_category = "Stereoscopic Options";
> = 0;

uniform float3 Interlace_Anaglyph_Calibrate <
	ui_type = "drag";
	ui_min = 0.0; ui_max = 1.0;
	ui_label = " Interlace, Anaglyph & Calibration";
	ui_tooltip = "Interlace Optimization is used to reduce aliasing in a Line or Column interlaced image. This has the side effect of softening the image.\n"
	             "Anaglyph Desaturation allows for removing color from an anaglyph 3D image. Zero is Black & White, One is full color.\n"
	    		 "Tobii Calibration for adjusting the Eye Tracking offset with Tobii, FreePie app, and Script.\n"
				 "Default for Interlace Optimization is 0.5 and for Anaglyph Desaturation is One.";
	ui_category = "Stereoscopic Options";
> = float3(0.5,1.0,0.5);
#if Lenticular_Degrees
uniform float Lens_Angle <
	ui_type = "drag";
	ui_min = 0.0; ui_max = 90.0;
	ui_label = " Lens Angle";
	ui_tooltip = "Determines Angle of your lens in Degrees.\n"
				 "This is for AutoStereo Displays.\n"
				 "Default is Zero.";
	ui_category = "Stereoscopic Options";
> = Set_Degrees;
#else
static const float Lens_Angle = Set_Degrees;
#endif
#if Ven
uniform int Scaling_Support <
	ui_type = "combo";
	ui_items = "SR Native\0SR 2160p A\0SR 2160p B\0SR 1080p A\0SR 1080p B\0SR 1050p A\0SR 1050p B\0SR 720p A\0SR 720p B\0";
	ui_label = " Downscaling Support";
	ui_tooltip = "Dynamic Super Resolution scaling support for Line Interlaced, Column Interlaced, & Checkerboard 3D displays.\n"
				 "Set this to your native Screen Resolution A or B, DSR Smoothing must be set to 0%.\n"
				 "This does not work with a hardware ware scaling done by VSR.\n"
				 "Default is SR Native.";
	ui_category = "Stereoscopic Options";
> = 0;
#else
static const int Scaling_Support = 0;
#endif
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

uniform bool Eye_Swap <
	ui_label = " Swap Eyes";
	ui_tooltip = "L/R to R/L.";
	ui_category = "Stereoscopic Options";
> = false;

uniform int FPSDFIO <
	ui_type = "combo";
	ui_items = "Off\0Press\0Hold\0";
	ui_label = "·FPS Focus Depth·";
	ui_tooltip = "This lets the shader handle real time depth reduction for aiming down your sights.\n"
				"This may induce Eye Strain so take this as an Warning.";
	ui_category = "FPS Focus";
> = DK_X;

uniform int Focus_Reduction_Type <
	ui_type = "combo";
	ui_items = "World\0Weapon\0Mix\0";
	ui_label = " Focus Type";
	ui_tooltip = "This lets the shader handle real time depth reduction for aiming down your sights.\n"
				"This may induce Eye Strain so take this as an Warning.";
	ui_category = "FPS Focus";
> = 0;

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
uniform bool Vert_3D_Pinball <
	ui_label = "·Swap 3D Axis·";
	ui_tooltip = "Use this to swap the axis that the Parallax is generated.\n"
				 "Useful for 3D Pinball Games, You may have to swap eyes.\n"
				 "Default is Off.";
	ui_category = "Miscellaneous Options";
> = false;
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
texture texDMN { Width = BUFFER_WIDTH ; Height = BUFFER_HEIGHT; Format = RGBA16F; };

sampler SamplerDMN
	{
		Texture = texDMN;
	};

texture texzBufferN_P { Width = BUFFER_WIDTH ; Height = BUFFER_HEIGHT ; Format = RG16F; MipLevels = 6; };

sampler SamplerzBufferN_P
	{
		Texture = texzBufferN_P;
		MagFilter = POINT;
		MinFilter = POINT;
		MipFilter = POINT;
	};

texture texzBufferN_L { Width = BUFFER_WIDTH ; Height = BUFFER_HEIGHT ; Format = RG16F; MipLevels = 8; };

sampler SamplerzBufferN_L
	{
		Texture = texzBufferN_L;
	};


#if UI_MASK
texture TexMaskA < source = "DM_Mask_A.png"; > { Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = RGBA8; };
sampler SamplerMaskA { Texture = TexMaskA;};
texture TexMaskB < source = "DM_Mask_B.png"; > { Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = RGBA8; };
sampler SamplerMaskB { Texture = TexMaskB;};
#endif
#define Scale_Buffer 160 / BUFFER_WIDTH
////////////////////////////////////////////////////////Adapted Luminance/////////////////////////////////////////////////////////////////////
texture texLumN {Width = BUFFER_WIDTH * Scale_Buffer; Height = BUFFER_HEIGHT * Scale_Buffer; Format = RGBA16F; MipLevels = 8;};

sampler SamplerLumN
	{
		Texture = texLumN;
	};
	
float2 Lum(float2 texcoord)
	{ 
		return saturate(tex2Dlod(SamplerLumN,float4(texcoord,0,11)).xy);//Average Depth Brightnes Texture Sample
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
	if(Custom_Sidebars == 0)
		return tex2Dlod(BackBufferMIRROR,float4(texcoords,0,0));
	else if(Custom_Sidebars == 1)
		return tex2Dlod(BackBufferBORDER,float4(texcoords,0,0));
	else
		return tex2Dlod(BackBufferCLAMP,float4(texcoords,0,0));
}

float4 DelayFrame(in float4 position : SV_Position, in float2 texcoords : TEXCOORD) : SV_Target
{
	return tex2Dlod(SamplerCF,float4(texcoords,0,0));
}

float4 CSB(float2 texcoords)
{
	if(Depth_Map_View == 0)
		return tex2Dlod(SamplerDF,float4(texcoords,0,0));
	else
		return tex2D(SamplerzBufferN_P,texcoords).xxxx;
}
#else
float4 CSB(float2 texcoords)
{
	if(Custom_Sidebars == 0 && Depth_Map_View == 0)
		return tex2Dlod(BackBufferMIRROR,float4(texcoords,0,0));
	else if(Custom_Sidebars == 1 && Depth_Map_View == 0)
		return tex2Dlod(BackBufferBORDER,float4(texcoords,0,0));
	else if(Custom_Sidebars == 2 && Depth_Map_View == 0)
		return tex2Dlod(BackBufferCLAMP,float4(texcoords,0,0));
	else
		return tex2Dlod(SamplerzBufferN_P,float4(texcoords,0,0)).x;
}
#endif

#if LBC || LBM || LB_Correction || LetterBox_Masking
int LBSensitivity( float inVal )
{
	#if LBS
		return inVal < 0.005; //Less Sensitive
	#else
		return inVal == 0; //Sensitive
	#endif
}

float SLLTresh(float2 TCLocations, float MipLevel)
{ 
	return tex2Dlod(SamplerzBufferN_L,float4(TCLocations,0, MipLevel)).y;
}

float LBDetection()//Active RGB Detection
{   float MipLevel = 5,Center = SLLTresh(float2(0.5,0.5), 8) > 0, Top_Left = LBSensitivity(SLLTresh(float2(0.1,0.1), MipLevel));
	if ( LetterBox_Masking == 2 || LB_Correction == 2 || LBC == 2 || LBM == 2 )
		return Top_Left && LBSensitivity(SLLTresh(float2(0.1,0.5), MipLevel)) && LBSensitivity(SLLTresh(float2(0.9,0.5), MipLevel)) && Center ? 1 : 0; //Vert
	else                   //Left_Center                                  //Right_Center
		return Top_Left && LBSensitivity(SLLTresh(float2(0.5,0.9), MipLevel)) && Center ? 1 : 0; //Hoz
}			              //Bottom_Center
#endif

#if SDT || SD_Trigger
float TargetedDepth(float2 TC)
{
	return smoothstep(0,1,tex2Dlod(SamplerzBufferN_P,float4(TC,0,0)).y);
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
{ float TCoRF[1], Trigger_Fade, AA = Fade_Time_Adjust, PStoredfade = tex2D(SamplerLumN,float2(0,0.083)).z;
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
		  PStoredfade_A = tex2D(SamplerLumN,float2(0, 0.250)).z, PStoredfade_B = tex2D(SamplerLumN,float2(0, 0.416)).z;
	//Fade in toggle.
	return float2( PStoredfade_A + (Trigger_Fade_A - PStoredfade_A) * (1.0 - exp(-frametime/AA)), PStoredfade_B + (Trigger_Fade_B - PStoredfade_B) * (1.0 - exp(-frametime/AA)) ); ///exp2 would be even slower
}
#define FadeSpeed_AW 0.25
float AltWeapon_Fade()
{
	float  ExAd = (1-(FadeSpeed_AW * 2.0))*1000, Current =  min(0.75f,smoothstep(0,0.25f,PrepDepth(0.5f)[0][0])), Past = tex2D(SamplerLumN,float2(0,0.750)).z;
	return Past + (Current - Past) * (1.0 - exp(-frametime/ExAd));
}

float Weapon_ZPD_Fade(float Weapon_Con)
{
	float  ExAd = (1-(FadeSpeed_AW * 2.0))*1000, Current =  Weapon_Con, Past = tex2D(SamplerLumN,float2(0,0.916)).z;
	return Past + (Current - Past) * (1.0 - exp(-frametime/ExAd));
}
//////////////////////////////////////////////////////////Depth Map Alterations/////////////////////////////////////////////////////////////////////
float4 DepthMap(in float4 position : SV_Position,in float2 texcoord : TEXCOORD) : SV_Target
{
	float4 DM = float4(PrepDepth(texcoord)[0][0],PrepDepth(texcoord)[0][1],PrepDepth(texcoord)[0][2],PrepDepth(texcoord)[1][1]);
	float R = DM.x, G = DM.y, B = DM.z, Auto_Scale =  WZPD_and_WND.y > 0.0 ? tex2D(SamplerLumN,float2(0,0.750)).z : 1;

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
				WZPDB = 1 - (WZPD_and_WND.x * WZDPArray[i]) / tex2Dlod(SamplerDMN,float4(float2(WArray[i],0.9375),0,0)).z;
			else
			{
				if (Weapon_ZPD_Boundary < 0) //Code for Moving Weapon Hand stablity.
					WZPDB = 1 - WZPD_and_WND.x / tex2Dlod(SamplerDMN,float4(float2(MWArray[i],Distance_From_Bottom),0,0)).z;
				else //Normal
					WZPDB = 1 - WZPD_and_WND.x / tex2Dlod(SamplerDMN,float4(float2(WArray[i],Distance_From_Bottom),0,0)).z;
			}

			if (WZPDB < -DJ_W) // Default -0.1
				W_Convergence *= 1.0-abs(Weapon_ZPD_Boundary);
		}
	}
	//Store Weapon Convergence for Smoothing.
	Store_WC = W_Convergence;
	
	W_Convergence = 1 - tex2D(SamplerLumN,float2(0,0.916)).z / MD_WHD.y;// 1-W_Convergence/D
	float WD = MD_WHD.y; //Needed to seperate Depth for the  Weapon Hand. It was causing problems with Auto Depth Range below.

		if (Auto_Depth_Adjust > 0)
			D = AutoDepthRange(D,texcoord);

	#if Balance_Mode || BMT
			ZP = saturate(ZPD_Balance);
	#else
		if(Auto_Balance_Ex > 0 )
			ZP = saturate(ALC);
	#endif
		float DOoR = smoothstep(0,1,tex2D(SamplerLumN,float2(0, 0.416)).z), ZDP_Array[16] = { 0.0, 0.0125, 0.025, 0.0375, 0.04375, 0.05, 0.0625, 0.075, 0.0875, 0.09375, 0.1, 0.125, 0.150, 0.175, 0.20, 0.225};
		
		if(REF || RE_Fix)
		{
			if(RE_Fix)
				ZPD_Boundary = lerp(ZPD_Boundary,ZDP_Array[RE_Fix],DOoR);
			else
				ZPD_Boundary = lerp(ZPD_Boundary,ZDP_Array[REF],DOoR);
		}

		Z *= lerp( 1, ZPD_Boundary, smoothstep(0,1,tex2D(SamplerLumN,float2(0, 0.250)).z));
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
	float4 DM = float4(tex2Dlod(SamplerDMN,float4(texcoord,0,0)).xyz,PrepDepth( SDT || SD_Trigger ? TC_SP(texcoord) :  texcoord )[1][1]);
	//Hide Temporal passthrough
	if(texcoord.x < pix.x * 2 && texcoord.y < pix.y * 2)
		DM = PrepDepth(texcoord)[0][0];
	if(1-texcoord.x < pix.x * 2 && 1-texcoord.y < pix.y * 2)
		DM = PrepDepth(texcoord)[0][0];
	if(texcoord.x < pix.x * 2 && 1-texcoord.y < pix.y * 2)
		DM = PrepDepth(texcoord)[0][0];

	if (WP == 0 || WZPD_and_WND.x <= 0)
		DM.y = 0;

	float FadeIO = smoothstep(0,1,1-Fade_in_out(texcoord).x), FD_Adjust = Focus_Reduction_Type == 2 ? 0.125 : 0.1625;	

	if( Eye_Fade_Reduction_n_Power.y == 1)
		FD_Adjust = 0.25;
	else if( Eye_Fade_Reduction_n_Power.y == 2)
		FD_Adjust = 0.30;

	//Handle Convergence Here
	float3 HandleConvergence = Conv(DM.xz,texcoord).xyz;
		   HandleConvergence.y *= WA_XYZW().w;
		   HandleConvergence.y = lerp(HandleConvergence.y + FD_Adjust, HandleConvergence.y, FadeIO);
	DM.y = lerp( HandleConvergence.x, HandleConvergence.y, DM.y);
	//Better mixing for eye Comfort
	DM.z = DM.y;
	DM.y += lerp(DM.y,DM.x,DM.w);
	DM.y *= 0.5f;
	DM.y = lerp(DM.y,DM.z,0.9375f);
	#if Compatibility_DD
	if (Depth_Detection == 1 || Depth_Detection == 2)
	{ //Check Depth at 3 Point D_A Top_Center / Bottom_Center
		float2 DA_DB = float2(tex2Dlod(SamplerDMN,float4(float2(0.5,0.0),0,0)).x, tex2Dlod(SamplerDMN,float4(float2(0.0,1.0),0,0)).x);
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
		float2 DA_DB = float2(tex2Dlod(SamplerDMN,float4(float2(0.5,0.0),0,0)).x, tex2Dlod(SamplerDMN,float4(float2(0.0,1.0),0,0)).x);
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

float3 GetDB(float2 texcoord)
{  
	if(Vert_3D_Pinball)	
		texcoord.xy = texcoord.yx;
		
	float MIP_LvL = View_Mode == 1 ? 6 : 0, Seper_Am = View_Mode == 5 ? 50.0 : 6.0, 
		Depth_Blur = tex2Dlod(SamplerzBufferN_P, float4(texcoord,0, MIP_LvL) ).y;
		if(View_Mode == 1 || View_Mode == 5)
		{		
			Depth_Blur += tex2Dlod(SamplerzBufferN_P, float4(texcoord + float2(1,0) * pix * Seper_Am,0, MIP_LvL) ).y;
			Depth_Blur += tex2Dlod(SamplerzBufferN_P, float4(texcoord - float2(1,0) * pix * Seper_Am,0, MIP_LvL) ).y;		  
			Depth_Blur += tex2Dlod(SamplerzBufferN_P, float4(texcoord + float2(2,0) * pix * Seper_Am,0, MIP_LvL) ).y;
			Depth_Blur += tex2Dlod(SamplerzBufferN_P, float4(texcoord - float2(2,0) * pix * Seper_Am,0, MIP_LvL) ).y;
			Depth_Blur /= 5;
		} 

	float3 DepthBuffer_LP = float3(tex2Dlod(SamplerzBufferN_L, float4( texcoord, 0, 0) ).x,tex2Dlod(SamplerzBufferN_P, float4( texcoord, 0, 0) ).x, Depth_Blur );
	
	if(View_Mode <= 1 || View_Mode == 3)	
		DepthBuffer_LP.x = DepthBuffer_LP.y;
		
	//float DBLP_Mix = lerp(tex2Dlod(SamplerzBufferN_L, float4( texcoord, 0, TEST) ).x,DepthBuffer_LP.xy, saturate(1-PDepth) );

	float Separation = lerp(1.0,5.0,ZPD_Separation.y); 	
	return float3(Separation * DepthBuffer_LP.xy, DepthBuffer_LP.z);
}
//Perf Level selection                             X    Y      Z      W              X    Y      Z      W
static const float4 Performance_LvL[2] = { float4( 0.5, 0.5095, 0.679, 0.5 ), float4( 1.0, 1.019, 1.425, 1.0) };
//////////////////////////////////////////////////////////Parallax Generation///////////////////////////////////////////////////////////////////////
float2 Parallax(float Diverge, float2 Coordinates, float IO) // Horizontal parallax offset & Hole filling effect
{   float MS = Diverge * pix.x;   
	float2 ParallaxCoord = Coordinates, CBxy = floor( float2(Coordinates.x * BUFFER_WIDTH, Coordinates.y * BUFFER_HEIGHT));
	float GetDepth = smoothstep(0,1, GetDB(Coordinates).z ),// CM_Clamp = clamp(abs(Compatibility_Mode),0,4),
			   Details = View_Mode > 1 ? lerp(0.0375, 1.0,Compatibility_Power) : 0,//CM_Power[5] = { 0.0, 0.5, 1.0, 1.5, 2.0}, NS_CM = Compatibility_Mode < 0 ? -CM_Power[(int)CM_Clamp] : CM_Power[(int)CM_Clamp] ,
			   Perf = Performance_LvL[Performance_Level].x;
	//Would Use Switch....
	if( View_Mode == 2)
		Perf = Performance_LvL[Performance_Level].z;
	if( View_Mode == 3)
		Perf = Performance_LvL[Performance_Level].x;
	if( View_Mode == 4)
		Perf = fmod(CBxy.y,2) ? Performance_LvL[Performance_Level].z : fmod(CBxy.x+CBxy.y,2) ? 1.020f : 1.021f;
	if( View_Mode == 5)
	{
		if( GetDepth >= 0.999 )
			Perf = fmod(CBxy.x+CBxy.y,2) ? 1.016: 1.017;
		else if( GetDepth >= 0.875)
			Perf = fmod(CBxy.x+CBxy.y,2) ? 1.018: 1.019;
		else
			Perf = fmod(CBxy.x+CBxy.y,2) ? 1.020: 1.021;
	}
	//ParallaxSteps Calculations
	float D = abs(Diverge), Cal_Steps = D * Perf, Steps = clamp( Cal_Steps, Performance_Level || View_Mode == 1 ? 20 : lerp(20,D,saturate(GetDepth > 0.998) ), 200 );//Foveated Rendering Point of attack 16-256 limit samples.
	// Offset per step progress & Limit
	float LayerDepth = rcp(Steps), TP = lerp(0.0334, 0.05,Compatibility_Power);
		  D = Diverge < 0 ? -75 : 75;

	//Offsets listed here Max Seperation is 3% - 8% of screen space with Depth Offsets & Netto layer offset change based on MS.
	float deltaCoordinates = MS * LayerDepth, CurrentDepthMapValue = GetDB( ParallaxCoord).x, CurrentLayerDepth = 0.0f,
		  Offset_Switch = View_Mode > 1 ? 0.0 : 1.0, DB_Offset = D * TP * pix.x;
		  
	[loop] //Steep parallax mapping
	while ( CurrentDepthMapValue > CurrentLayerDepth )
	{   // Shift coordinates horizontally in linear fasion
	    ParallaxCoord.x -= deltaCoordinates; 
	    // Get depth value at current coordinates
	    CurrentDepthMapValue = GetDB(float2( ParallaxCoord.x, ParallaxCoord.y)).x;
	    // Get depth of next layer
	    CurrentLayerDepth += LayerDepth;
		continue;
	}
	
    ParallaxCoord.x += (DB_Offset * Offset_Switch);
	float2 PrevParallaxCoord = float2( ParallaxCoord.x + deltaCoordinates, ParallaxCoord.y);
	//Anti-Weapon Hand Fighting
	float Weapon_Mask = tex2Dlod(SamplerDMN,float4(Coordinates,0,0)).y, ZFighting_Mask = 1.0-(1.0-tex2Dlod(SamplerLumN,float4(Coordinates,0,1.400)).w - Weapon_Mask);
		  ZFighting_Mask = ZFighting_Mask * (1.0-Weapon_Mask);
		  float PCoord = (View_Mode > 1 ? ParallaxCoord.x : lerp( ParallaxCoord.x, lerp( ParallaxCoord.x, PrevParallaxCoord.x,0.5), saturate(GetDepth * 5.0))) - 0.004 * MS ;
	float Get_DB = GetDB(float2( PCoord, PrevParallaxCoord.y ) ).y, Get_DB_ZDP = WP > 0 ? lerp(Get_DB, abs(Get_DB), ZFighting_Mask) : Get_DB;
	// Parallax Occlusion Mapping
	float beforeDepthValue = Get_DB_ZDP, afterDepthValue = CurrentDepthMapValue - CurrentLayerDepth;
		  beforeDepthValue += LayerDepth - CurrentLayerDepth;
	// Depth Diffrence for Gap masking and depth scaling in Normal Mode.
	float DepthDiffrence = afterDepthValue - beforeDepthValue, DD_Map = abs(DepthDiffrence) > 0.064;//, Depth_Mask = distance(saturate(GetDepth * 10),saturate((1-GetDepth) * 10));
	float weight = afterDepthValue / min(-0.0125,DepthDiffrence), Weight = weight;

	if( View_Mode == 1 )
	{
		if(Diverge < 0)
			weight *= lerp( 1, 1-(0.0025 * saturate(GetDepth * 2.5)), DD_Map ); 
		else
			weight *= lerp( 1, 1+(0.0025 * saturate(GetDepth * 2.5)), DD_Map );  
	}
		ParallaxCoord.x = PrevParallaxCoord.x * weight + ParallaxCoord.x * (1 - Weight);
	//This is to limit artifacts.
	ParallaxCoord.x += DB_Offset.x * Details;
	if( View_Mode <= 1 )
	{
		if(Diverge < 0)
			ParallaxCoord.x += lerp(DepthDiffrence * 2.5, DepthDiffrence * (View_Mode == 1 ? 4.0 : 3.0), DD_Map) * pix.x;
		else
			ParallaxCoord.x -= lerp(DepthDiffrence * 2.5, DepthDiffrence * (View_Mode == 1 ? 4.0 : 3.0), DD_Map) * pix.x;
	}	
	
	if(Stereoscopic_Mode == 2)
		ParallaxCoord.y += IO * pix.y; //Optimization for line interlaced.
	else if(Stereoscopic_Mode == 3)
		ParallaxCoord.x += IO * pix.x; //Optimization for column interlaced.

	return ParallaxCoord;
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

float2 LensePitch(float2 TC)
{
	//Texture Rotation//
	/*
	Sacchan calculator http://z800.yokinihakarae.com/html5test/sachiicalc02.html
	Number of horizontal dots: 3840
	Number of vertical dots: 2160
	Sreen Size in inch: 27.9
	DPI obtained from resolution / inch : 157.9144924790178

	Answer from the DPI of the LCD panel :
	pitch = 12.683894978234363 Sacchan coefficient 12.45
	pitch = 12.633159398321425 Sacchan coefficient 12.5
	pitch = 12.582828085977514 Sacchan coefficient 12.55
	pitch = 12.532896228493478 Sacchan coefficient 12.6
	pitch = 12.483359089250419 Sacchan coefficient 12.65
	pitch = 12.434212006221875 Sacchan coefficient 12.7
	pitch = 12.3854503905112   Sacchan coefficient 12.75
	*/
	//Ended up using the Sacchan cofficient here as Degrees 12.55 CW......
	float Degrees = radians(Lens_Angle);//Converts the specified value from radians to degrees.

	float2 PivotPoint = 0.5;
	float2 Rotationtexcoord = TC;
	float sin_factor = sin(Degrees);
	float cos_factor = cos(Degrees);
	Rotationtexcoord = mul(Rotationtexcoord - PivotPoint, float2x2(float2(cos_factor, -sin_factor), float2(sin_factor, cos_factor)));
	Rotationtexcoord += PivotPoint + PivotPoint;

	return Rotationtexcoord.xy;
}
/*
float StereoViewMask( float2 coord )
{
    //Calculate Gradient distance to edge
    float edge = length((coord*2.0-1.0) * float2(0.0 + 2.25, 1.0));
    //Compute vignette gradient and intensity
    return saturate(smoothstep(0.499,0.5,edge)); 
}
*/
///////////////////////////////////////////////////////////Stereo Calculation///////////////////////////////////////////////////////////////////////
float3 PS_calcLR(float2 texcoord)
{
	float2 TCL, TCR, TCL_T, TCR_T, TexCoords = texcoord, TC = texcoord;

	[branch] if (Stereoscopic_Mode == 0)
	{
		TCL = float2(texcoord.x*2,texcoord.y);
		TCR = float2(texcoord.x*2-1,texcoord.y);
	}
	else if(Stereoscopic_Mode == 1)
	{
		TCL = float2(texcoord.x,texcoord.y*2);
		TCR = float2(texcoord.x,texcoord.y*2-1);
	}
	else if(Stereoscopic_Mode == 6)
	{
		TCL = float2(texcoord.x*2,texcoord.y*2);
		TCL_T = float2(texcoord.x*2-1,texcoord.y*2);
		TCR = float2(texcoord.x*2-1,texcoord.y*2-1);
		TCR_T = float2(texcoord.x*2,texcoord.y*2-1);
	}
	else
	{
		TCL = float2(texcoord.x,texcoord.y);
		TCR = float2(texcoord.x,texcoord.y);
	}

	TCL += Per;
	TCR -= Per;

	float D = Eye_Swap ? -Divergence : Divergence;

	float FadeIO = smoothstep(0,1,1-Fade_in_out(texcoord).x), FD = D, FD_Adjust = Focus_Reduction_Type == 2 ? 0.5 : 0.1;

	if( Eye_Fade_Reduction_n_Power.y == 1)
		FD_Adjust = Focus_Reduction_Type == 2 ? 0.625 : 0.2;
	else if( Eye_Fade_Reduction_n_Power.y == 2)
		FD_Adjust = Focus_Reduction_Type == 2 ? 0.75 : 0.3;

	if(Focus_Reduction_Type == 1)
		FD_Adjust = 1.0;		

	if (FPSDFIO >= 1)
		FD = lerp(FD * FD_Adjust,FD,FadeIO);

	float2 DLR = float2(FD,FD);
	if( Eye_Fade_Reduction_n_Power.x == 1)
			DLR = float2(D,FD);
	else if( Eye_Fade_Reduction_n_Power.x == 2)
			DLR = float2(FD,D);

	float4 image = 1, accum, color, Left_T, Right_T, L, R, Left, Right;
		
	if(Vert_3D_Pinball)
	{
		Left = MouseCursor(Parallax(-DLR.x, TCL.yx, AI).yx);
		Right= MouseCursor(Parallax(DLR.y, TCR.yx, -AI).yx);
	}
	else
	{
		Left = MouseCursor(Parallax(-DLR.x, TCL, AI));
		Right= MouseCursor(Parallax(DLR.y, TCR, -AI));
	}

	if(Stereoscopic_Mode == 6)
	{
		Left_T = MouseCursor(Parallax(-DLR.x * 0.33333333, TCL_T, AI));
		Right_T= MouseCursor(Parallax(DLR.y * 0.33333333, TCR_T, -AI));
	}

	#if HUD_MODE || HMT
	float HUD_Adjustment = ((0.5 - HUD_Adjust.y)*25.) * pix.x;
	Left.rgb = HUD(Left.rgb,float2(TCL.x - HUD_Adjustment,TCL.y)).rgb;
	Right.rgb = HUD(Right.rgb,float2(TCR.x + HUD_Adjustment,TCR.y)).rgb;
	if(Stereoscopic_Mode == 6)
	{
		Left_T.rgb = HUD(Left_T.rgb,float2(TCL_T.x - HUD_Adjustment,TCL_T.y)).rgb;
		Right_T.rgb = HUD(Right_T.rgb,float2(TCR_T.x + HUD_Adjustment,TCR_T.y)).rgb;
	}
	#endif
	//Auto Stereo Section C adjusting for eye tracking and distance. This also the point of where pitch, rotation, and other information is used.
	float Dist = Stereoscopic_Mode == 5 ? int(FP_IO_Pos().z) : 0, Distance_Ladder = 0.0;//This is the Distance calulation for adjusting the pitch based on what Tobii eye traker gives me.
	//Adjust for distance from screen here.
	if (Dist == 2)
		Distance_Ladder = 2;
	if (Dist == 3)
		Distance_Ladder = 1.5; // Swap
	if (Dist == 4)
		Distance_Ladder = 1.0;
	if (Dist == 5 || Dist == 6)
		Distance_Ladder = 0.5; // Swap
	if (Dist == 7)
		Distance_Ladder = 0.0;

	float IAC = saturate(Interlace_Anaglyph_Calibrate.z), LPI = Stereoscopic_Mode == 5 ? 1.267 + (Distance_Ladder * 0.001) : 1.0;//1.268
	// -4 to 4 is the scale 0 is center.
	TC += float2( ( FP_IO_Pos().x * 1.225 + lerp(0,4,IAC) ) * pix.x, 0.0);//0.0001????

	TC = Stereoscopic_Mode == 5 ? LensePitch(TC) * LPI : TC;

	float2 gridxy, GXYArray[9] = {
		float2(TC.x * BUFFER_WIDTH, TC.y * BUFFER_HEIGHT), //Native
		float2(TC.x * 3840.0, TC.y * 2160.0),
		float2(TC.x * 3841.0, TC.y * 2161.0),
		float2(TC.x * 1920.0, TC.y * 1080.0),
		float2(TC.x * 1921.0, TC.y * 1081.0),
		float2(TC.x * 1680.0, TC.y * 1050.0),
		float2(TC.x * 1681.0, TC.y * 1051.0),
		float2(TC.x * 1280.0, TC.y * 720.0),
		float2(TC.x * 1281.0, TC.y * 721.0)
	};

	gridxy = floor(GXYArray[Scaling_Support]);
	float DG = 0.950, Swap_Eye = Dist== 3 || Dist == 5 || Dist == 6 ? 1 : 0;
	const int Images = 4;

	if (Swap_Eye) { L = Right; R = Left; } else	{ L = Left; R = Right; }

	float3 Colors[Images] = {
	    float3(L.x     , R.y * DG, R.z     ), // L | R | R
	    float3(L.x * DG, L.y     , R.z * DG), // L | L | R
	    float3(R.x     , L.y * DG, L.z     ), // R | L | L
	    float3(R.x * DG, R.y     , L.z * DG)};// R | R | L

	if(Stereoscopic_Mode == 0)
		color = TexCoords.x < 0.5 ? L : R;
	else if(Stereoscopic_Mode == 1)
		color = TexCoords.y < 0.5 ? L : R;
	else if(Stereoscopic_Mode == 2)
		color = fmod(gridxy.y,2) ? R : L;
	else if(Stereoscopic_Mode == 3)
		color = fmod(gridxy.x,2) ? R : L;
	else if(Stereoscopic_Mode == 4)
		color = fmod(gridxy.x+gridxy.y,2) ? R : L;
	else if(Stereoscopic_Mode == 5)
		color = float4(Colors[int(fmod(gridxy.x,Images))],1.0);
	else if(Stereoscopic_Mode == 6)
		color = TexCoords.y < 0.5 ? TexCoords.x < 0.5 ? Left : Left_T : TexCoords.x < 0.5 ? Right_T : Right;
	else if(Stereoscopic_Mode >= 7)
	{
		float Contrast = 1.0, DeGhost = 0.06, LOne, ROne;
		float3 HalfLA = dot(L.rgb,float3(0.299, 0.587, 0.114)), HalfRA = dot(R.rgb,float3(0.299, 0.587, 0.114));
		float3 LMA = lerp(HalfLA,L.rgb,Interlace_Anaglyph_Calibrate.y), RMA = lerp(HalfRA,R.rgb,Interlace_Anaglyph_Calibrate.y);

		float contrast = (Contrast*0.5)+0.5;

		// Left/Right Image
		float4 cA = float4(LMA,1);
		float4 cB = float4(RMA,1);

		if (Stereoscopic_Mode == 7) // Anaglyph 3D Colors Red/Cyan
			color =  float4(cA.r,cB.g,cB.b,1.0);
		else if (Stereoscopic_Mode == 8) // Anaglyph 3D Dubois Red/Cyan
		{
			float red = 0.437 * cA.r + 0.449 * cA.g + 0.164 * cA.b - 0.011 * cB.r - 0.032 * cB.g - 0.007 * cB.b;

			if (red > 1) { red = 1; }   if (red < 0) { red = 0; }

			float green = -0.062 * cA.r -0.062 * cA.g -0.024 * cA.b + 0.377 * cB.r + 0.761 * cB.g + 0.009 * cB.b;

			if (green > 1) { green = 1; }   if (green < 0) { green = 0; }

			float blue = -0.048 * cA.r - 0.050 * cA.g - 0.017 * cA.b -0.026 * cB.r -0.093 * cB.g + 1.234  * cB.b;

			if (blue > 1) { blue = 1; }   if (blue < 0) { blue = 0; }

			color = float4(red, green, blue, 0);
		}
		else if (Stereoscopic_Mode == 9) // Anaglyph 3D Deghosted Red/Cyan Code From http://iaian7.com/quartz/AnaglyphCompositing & vectorform.com by John Einselen
		{
			LOne = contrast*0.45;
			ROne = contrast;
			DeGhost *= 0.1;

			accum = saturate(cA*float4(LOne,(1.0-LOne)*0.5,(1.0-LOne)*0.5,1.0));
			image.r = pow(accum.r+accum.g+accum.b, 1.00);
			image.a = accum.a;

			accum = saturate(cB*float4(1.0-ROne,ROne,0.0,1.0));
			image.g = pow(accum.r+accum.g+accum.b, 1.15);
			image.a = image.a+accum.a;

			accum = saturate(cB*float4(1.0-ROne,0.0,ROne,1.0));
			image.b = pow(accum.r+accum.g+accum.b, 1.15);
			image.a = (image.a+accum.a)/3.0;

			accum = image;
			image.r = (accum.r+(accum.r*DeGhost)+(accum.g*(DeGhost*-0.5))+(accum.b*(DeGhost*-0.5)));
			image.g = (accum.g+(accum.r*(DeGhost*-0.25))+(accum.g*(DeGhost*0.5))+(accum.b*(DeGhost*-0.25)));
			image.b = (accum.b+(accum.r*(DeGhost*-0.25))+(accum.g*(DeGhost*-0.25))+(accum.b*(DeGhost*0.5)));
			color = image;
		}
		else if (Stereoscopic_Mode == 10) // Anaglyph 3D Green/Magenta
			color = float4(cB.r,cA.g,cB.b,1.0);
		else if (Stereoscopic_Mode == 11) // Anaglyph 3D Dubois Green/Magenta
		{

			float red = -0.062 * cA.r -0.158 * cA.g -0.039 * cA.b + 0.529 * cB.r + 0.705 * cB.g + 0.024 * cB.b;

			if (red > 1) { red = 1; }   if (red < 0) { red = 0; }

			float green = 0.284 * cA.r + 0.668 * cA.g + 0.143 * cA.b - 0.016 * cB.r - 0.015 * cB.g + 0.065 * cB.b;

			if (green > 1) { green = 1; }   if (green < 0) { green = 0; }

			float blue = -0.015 * cA.r -0.027 * cA.g + 0.021 * cA.b + 0.009 * cB.r + 0.075 * cB.g + 0.937  * cB.b;

			if (blue > 1) { blue = 1; }   if (blue < 0) { blue = 0; }

			color = float4(red, green, blue, 0);
		}
		else if (Stereoscopic_Mode == 12)// Anaglyph 3D Deghosted Green/Magenta Code From http://iaian7.com/quartz/AnaglyphCompositing & vectorform.com by John Einselen
		{
			LOne = contrast*0.45;
			ROne = contrast*0.8;
			DeGhost *= 0.275;

			accum = saturate(cB*float4(ROne,1.0-ROne,0.0,1.0));
			image.r = pow(accum.r+accum.g+accum.b, 1.15);
			image.a = accum.a;

			accum = saturate(cA*float4((1.0-LOne)*0.5,LOne,(1.0-LOne)*0.5,1.0));
			image.g = pow(accum.r+accum.g+accum.b, 1.05);
			image.a = image.a+accum.a;

			accum = saturate(cB*float4(0.0,1.0-ROne,ROne,1.0));
			image.b = pow(accum.r+accum.g+accum.b, 1.15);
			image.a = (image.a+accum.a)*0.33333333;

			accum = image;
			image.r = accum.r+(accum.r*(DeGhost*0.5))+(accum.g*(DeGhost*-0.25))+(accum.b*(DeGhost*-0.25));
			image.g = accum.g+(accum.r*(DeGhost*-0.5))+(accum.g*(DeGhost*0.25))+(accum.b*(DeGhost*-0.5));
			image.b = accum.b+(accum.r*(DeGhost*-0.25))+(accum.g*(DeGhost*-0.25))+(accum.b*(DeGhost*0.5));
			color = image;
		}
		else if (Stereoscopic_Mode == 13) // Anaglyph 3D Blue/Amber Code From http://iaian7.com/quartz/AnaglyphCompositing & vectorform.com by John Einselen
		{
			LOne = contrast*0.45;
			ROne = contrast;
			float D[1];//The Chronicles of Riddick: Assault on Dark Athena FIX I don't know why it works.......
			DeGhost *= 0.275;

			accum = saturate(cA*float4(ROne,0.0,1.0-ROne,1.0));
			image.r = pow(accum.r+accum.g+accum.b, 1.05);
			image.a = accum.a;

			accum = saturate(cA*float4(0.0,ROne,1.0-ROne,1.0));
			image.g = pow(accum.r+accum.g+accum.b, 1.10);
			image.a = image.a+accum.a;

			accum = saturate(cB*float4((1.0-LOne)*0.5,(1.0-LOne)*0.5,LOne,1.0));
			image.b = pow(accum.r+accum.g+accum.b, 1.0);
			image.b = lerp(pow(image.b,(DeGhost*0.15)+1.0),1.0-pow(abs(1.0-image.b),(DeGhost*0.15)+1.0),image.b);
			image.a = (image.a+accum.a)*0.33333333;

			accum = image;
			image.r = accum.r+(accum.r*(DeGhost*1.5))+(accum.g*(DeGhost*-0.75))+(accum.b*(DeGhost*-0.75));
			image.g = accum.g+(accum.r*(DeGhost*-0.75))+(accum.g*(DeGhost*1.5))+(accum.b*(DeGhost*-0.75));
			image.b = accum.b+(accum.r*(DeGhost*-1.5))+(accum.g*(DeGhost*-1.5))+(accum.b*(DeGhost*3.0));
			color = saturate(image);
		}
	}

	if (Depth_Map_View == 2)
		color.rgb = tex2D(SamplerzBufferN_P,TexCoords).xxx;
	
	
	float DepthBlur, Alinement_Depth = tex2Dlod(SamplerzBufferN_P,float4(TexCoords,0,0)).x, Depth = Alinement_Depth;
	const float DBPower = 1.0, Con = 11, weight[11] = { 0.0,0.010,-0.010,0.020,-0.020,0.030,-0.030,0.040,-0.040,0.050,-0.050 };
	if(BD_Options == 2 || Alinement_View)
	{
		float2 dir = 0.5 - TexCoords; 
		[loop]
		for (int i = 0; i < 11; i++)
		{
			DepthBlur += tex2Dlod(SamplerzBufferN_L,float4(TexCoords + dir * weight[i] * DBPower,0,2) ).x;
		}
		
		Alinement_Depth = ( Alinement_Depth + DepthBlur ) * 0.08333;
	}

	if (BD_Options == 2 || Alinement_View)
		color.rgb = dot(tex2D(BackBufferBORDER,TexCoords).rgb,0.333) * float3((Depth/Alinement_Depth> 0.998),1,(Depth/Alinement_Depth > 0.998));

	return color.rgb;
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
	float Storage__Array[Num_of_Values] = { tex2D(SamplerDMN,0).x,    //0.083
                                tex2D(SamplerDMN,1).x,                //0.250
                                tex2D(SamplerDMN,int2(0,1)).x,        //0.416
                                1.0,                                  //0.583
								tex2D(SamplerzBufferN_P,1).y,         //0.750
								tex2D(SamplerzBufferN_P,int2(0,1)).y};//0.916

	//Set a avr size for the Number of lines needed in texture storage.
	float Grid = floor(texcoord.y * BUFFER_HEIGHT * BUFFER_RCP_HEIGHT * Num_of_Values);

	return float4(Average_Lum_ZPD,Average_Lum_Bottom,Storage__Array[int(fmod(Grid,Num_of_Values))],tex2Dlod(SamplerDMN,float4(texcoord,0,0)).y);
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
	float3 Color = PS_calcLR(texcoord).rgb; //Color = texcoord.x+texcoord.y > 1 ? Color : LBDetection();
		  
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
		if(Stereoscopic_Mode == 5)
		{
		//Auto Stereo Help
		charPos = float2( 0.4575, 0.49375);
		//calibrate
		ETC += drawChar( CH_C, charPos, charSize, TC); charPos.x += .01 * Size;
		ETC += drawChar( CH_A, charPos, charSize, TC); charPos.x += .01 * Size;
		ETC += drawChar( CH_L, charPos, charSize, TC); charPos.x += .01 * Size;
		ETC += drawChar( CH_I, charPos, charSize, TC); charPos.x += .01 * Size;
		ETC += drawChar( CH_B, charPos, charSize, TC); charPos.x += .01 * Size;
		ETC += drawChar( CH_R, charPos, charSize, TC); charPos.x += .01 * Size;
		ETC += drawChar( CH_A, charPos, charSize, TC); charPos.x += .01 * Size;
		ETC += drawChar( CH_T, charPos, charSize, TC); charPos.x += .01 * Size;
		ETC += drawChar( CH_E, charPos, charSize, TC); charPos.x;
		//Too Far
		charPos = float2( 0.4575, 0.49375);
		ETTF += drawChar( CH_T, charPos, charSize, TC); charPos.x += .01 * Size;
		ETTF += drawChar( CH_O, charPos, charSize, TC); charPos.x += .01 * Size;
		ETTF += drawChar( CH_O, charPos, charSize, TC); charPos.x += .01 * Size;
		ETTF += drawChar( CH_BLNK, charPos, charSize, TC); charPos.x += .01 * Size;
		ETTF += drawChar( CH_F, charPos, charSize, TC); charPos.x += .01 * Size;
		ETTF += drawChar( CH_A, charPos, charSize, TC); charPos.x += .01 * Size;
		ETTF += drawChar( CH_R, charPos, charSize, TC); charPos.x;
		//Too Close
		charPos = float2( 0.445, 0.49375);
		ETTC += drawChar( CH_T, charPos, charSize, TC); charPos.x += .01 * Size;
		ETTC += drawChar( CH_O, charPos, charSize, TC); charPos.x += .01 * Size;
		ETTC += drawChar( CH_O, charPos, charSize, TC); charPos.x += .01 * Size;
		ETTC += drawChar( CH_BLNK, charPos, charSize, TC); charPos.x += .01 * Size;
		ETTC += drawChar( CH_C, charPos, charSize, TC); charPos.x += .01 * Size;
		ETTC += drawChar( CH_L, charPos, charSize, TC); charPos.x += .01 * Size;
		ETTC += drawChar( CH_O, charPos, charSize, TC); charPos.x += .01 * Size;
		ETTC += drawChar( CH_S, charPos, charSize, TC); charPos.x += .01 * Size;
		ETTC += drawChar( CH_E, charPos, charSize, TC); charPos.x;
		//Eye Tracking for Auto Stereo
		#if Compatibility_FP
			if(FP_IO_Pos().x <= 0.5 && FP_IO_Pos().x >= -0.5)
				ET = ETC;
		#elif Compatibility_FP == 2
			if(FP_IO_Pos().x <= 0.5 && FP_IO_Pos().x >= -0.5)
				ET = ETC;
			if(FP_IO_Pos().z > 5)
				ET = ETTF;
			if(FP_IO_Pos().z < 4)
				ET = ETTC;
		#endif
		}
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

technique SuperDepth3D
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
		RenderTarget = texLumN;
	}
	#endif
		pass DepthBuffer
	{
		VertexShader = PostProcessVS;
		PixelShader = DepthMap;
		RenderTarget = texDMN;
	}	
		pass Modzbuffer
	{
		VertexShader = PostProcessVS;
		PixelShader = Mod_Z;
		RenderTarget0 = texzBufferN_P;
		RenderTarget1 = texzBufferN_L;
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
		RenderTarget = texLumN;
	}
	#endif
}
