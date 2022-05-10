////----------------------------------------//
///SuperDepth3D Overwatch Automation Shader///
//----------------------------------------////
// Version 2.5.4
//---------------------------------------OVERWATCH---------------------------------------//
// If you are reading this stop. Go away and never look back. From this point on if you  //
// still think it's is worth looking at this..... Then no one can save you or your soul. //
// You will be cursed with never enjoying any memes to their fullest potential.......... //
// Ya that's it. JK                                                                      //
// The name comes from this.                                                             //
// https://en.wikipedia.org/wiki/Overwatch_(military_tactic)                             //
// Since this File looks ahead and sends information the Main shader to prepare it self. //
//---------------------------------------------------------------------------------------//
// Special Thanks to CeeJay.dk for code simplification and guidance.                     //
// You can contact him here https://github.com/CeeJayDK                                  //
//----------------------------------------LICENSE----------------------------------------//
// ===================================================================================== //
// Overwatch is licenses under: Copyright (C) Depth3D - All Rights Reserved              //
//                                                                                       //
// Unauthorized copying of this file, via any medium is strictly prohibited              //
// Proprietary and confidential.                                                         //
//                                                                                       //
// You are allowed to obviously download this and use this for your personal use.        //
// Just don't redistribute this file unless I authorize it.                              //
//                                                                                       //
// Written by Jose Negrete <UntouchableBlueSky@gmail.com>, December 2019                 //
// ===================================================================================== //
//--------------------------------------Code Start---------------------------------------//

//SuperDepth3D Defaults                                 [Names]                                         [Key]
static const float ZPD_D = 0.025;                       //ZPD                                           | DA_X
static const float Depth_Adjust_D = 7.5;                //Depth Adjust                                  | DA_Y
static const float Offset_D = 0.0;                      //Offset                                        | DA_Z
static const int Depth_Linearization_D = 0;             //Linearization                                 | DA_W

static const int Depth_Flip_D = 0;                      //Depth Flip                                    | DB_X
static const int Auto_Balance_D = 0;                    //Auto Balance                                  | DB_Y
static const float Auto_Depth_D = 0.1;                  //Auto Depth Range                              | DB_Z
static const int Weapon_Hand_D = 0;                     //Weapon Profile                                | DB_W

//Barrel Distortion Fix
static const int Barrel_Distortion_Fix_D = 0;           // 0 | 1 : Off | On                             | BDF
static const float BD_K1_D = 0.0;                       //Barrel Distortion K1                          | DC_X
static const float BD_K2_D = 0.0;                       //Barrel Distortion K2                          | DC_Y
static const float BD_K3_D = 0.0;                       //Barrel Distortion K3                          | DC_Z
static const float BD_Zoom_D = 0.0;                     //Barrel Distortion Zoom                        | DC_W

//Size & Position Fix
static const int Size_Position_Fix_D = 0;               // 0 | 1 : Off | On                             | SPF
static const float HVS_X_D = 1.0;                       //Horizontal Size                               | DD_X
static const float HVS_Y_D = 1.0;                       //Vertical Size                                 | DD_Y
static const float HVP_X_D = 0;                         //Horizontal Position                           | DD_Z
static const float HVP_Y_D = 0;                         //Vertical Position                             | DD_W

static const int ZPD_Boundary_Type_D = 0;               //ZPD Boundary Type                             | DE_X
static const float ZPD_Boundary_Scaling_D = 0.5;        //ZPD Boundary Scaling                          | DE_Y 
static const float ZPD_Boundary_Fade_Time_D = 0.25;     //ZPD Boundary Fade Time                        | DE_Z
static const float Weapon_Near_Depth_Max_D = 0.0;       //Weapon Near Depth                     Max     | DE_W

//Balance Mode Toggle
static const int Balance_Mode_Toggle_D = 0;             // 0 | 1 : Off | On                             | BMT
static const float ZPD_Weapon_Boundary_Adjust_D = 0.0;  //ZPD Weapon Boundary Adjust                    | DF_X
static const float Separation_D = 0.0;                  //ZPD Separation                                | DF_Y
static const float Manual_ZPD_Balance_D = 0.5;          //Manual Balance Mode Adjustment                | DF_Z
static const float HUDX_D = 0.0;                        //Heads Up Display Cut Off Point                | DF_W

//Specialized Depth Trigger
static const int Specialized_Depth_Trigger_D = 0;       // 0 | 1                                        | SDT
static const float SDC_Offset_X_D = 0.0;                //Special Depth Correction Offset X             | DG_X
static const float SDC_Offset_Y_D = 0.0;                //Special Depth Correction Offset Y             | DG_Y
static const float Weapon_Near_Depth_Min_D = 0.0;       //Weapon Near Depth                     Min     | DG_Z
static const float Check_Depth_Limit_D = 0.0;           //Check Depth Limit                             | DG_W

//Auto Letter Box Correction
static const int Auto_Letter_Box_Correction_D = 0;      // 0 | 1 | 2 : Off | Hoz | Vert                 | LBC
static const float LB_Depth_Size_Offset_X_D = 1.0;      //Letter Box Depth Size Correction Offset X     | DH_X
static const float LB_Depth_Size_Offset_Y_D = 1.0;      //Letter Box Depth Size Correction Offset Y     | DH_Y
static const float LB_Depth_Pos_Offset_X_D = 0.0;       //Letter Box Depth Position Correction Offset X | DH_Z
static const float LB_Depth_Pos_Offset_Y_D = 0.0;       //Letter Box Depth Position Correction Offset Y | DH_W
//Letter Box Sensitivity
static const int LB_Sensitivity_D = 0;                  // 0 | 1 : Off / On                             | LBS 
//Auto Letter Box Masking
static const int Auto_Letter_Box_Masking_D = 0;         // 0 | 1 | 2 : Off | Hoz | Vert                 | LBM                                                                         
static const float LB_Masking_Offset_X_D = 1.0;         //LetterBox Masking Offset X                    | DI_X
static const float LB_Masking_Offset_Y_D = 1.0;         //LetterBox Masking Offset Y                    | DI_Y
static const float Weapon_Near_Depth_Trim_D = 0.25;     //Weapon Near Depth                     Trim    | DI_Z
static const float REF_Check_Depth_Limit_D = 0.0;       //Resident Evil Fix Check Depth Limit           | DI_W

static const float NULL_X_D = 0.0;                      //Null X                                        | DJ_X
static const float NULL_Y_D = 0.0;                      //Null Y                                        | DJ_Y
static const float NULL_Z_D = 0.0;                      //Null Z                                        | DJ_Z
static const float Check_Depth_Limit_Weapon_D = -0.100; //Check Depth Limit Weapon                      | DJ_W

//FPS Focus
static const int FPS_Focus_Method_D = 0;                //FPS Focus Method: Off | Switch | Hold         | DK_X
static const int EFO_Eye_Selection_D = 0;               //Eye Eye Selection: Both | Right Eye | Left Eye| DK_Y
static const int EFO_Fade_Selection_D = 0;              //Eye Fade Options: 0.1% | 0.2% | 0.3%          | DK_Z
static const int EFO_Fade_Speed_Selection_D = 0;        //Eye Fade Speed Options: +0% | +50% | +100%    | DK_W

//SM Values
static const int SM_Toggle_Sparation_D = 1;             // 0 | 1 | 2 | 3                                | SMS
static const float SM_Tune_D = 0.5;                     //SM Tune                                       | DL_X
static const int SM_Weapon_D = 1;                       //SM Weapon                                     | DL_Y
static const float SM_Local_Smoothing_D = 0.0;          //SM Local Smoothing                            | DL_Z
static const float SM_Perspective_D  = 0.05;            //SM Perspective                                | DL_W
//SM HQ Values
static const int HQ_Tune_D = 4;                         //HQ Tune                                       | DM_X
static const int HQ_Boost_D = 4;                        //HQ Boost                                      | DM_Y
static const int HQ_Smooth_D = 1;                       //HQ Smooth                                     | DM_Z
static const float NULL_W_D = 0.0;                      //Null W                                        | DM_W

//Special Toggles Defaults
static const int Resident_Evil_Fix_D = 0;               //Resident Evil Fix                             | REF
static const int HUD_Mode_Trigger_D = 0;                //HUD Mode Trigger                              | HMT
static const int Inverted_Depth_Fix_D = 0;              //Inverted Depth Fix                            | IDF 
static const int Delay_Frame_Workaround_D = 0;          //Delay Frame Workaround                        | DFW

//Special Toggles Warnings
static const int No_Profile_Warning_D = 0;              //No Profile Warning                            | NPW
static const int Needs_Fix_Mod_D = 0;                   //Needs Fix/Mod                                 | NFM
static const int Depth_Selection_Warning_D = 0;         //Depth Selection Warning                       | DSW
static const int Disable_Anti_Aliasing_D = 0;           //Disable Anti-Aliasing                         | DAA
static const int Network_Warning_D = 0;                 //Network Warning                               | NDW
static const int Disable_Post_Effects_Warning_D = 0;    //Disable Post Effects Warning                  | PEW
static const int Weapon_Profile_Warning_D = 0;          //Weapon Profile Warning                        | WPW
static const int Set_Game_FoV_D = 0;                    //Set Game FoV                                  | FOV 

//Special Toggles Generic
static const int Read_Help_Warning_D = 0;               //Read Help Warning                             | RHW
static const int Emulator_Detected_Warning_D = 0;       //Emulator Detected Warning                     | EDW
static const int Not_Compatible_Warning_D = 0;          //Not Compatible Warning                        | NCW

//Weapon Setting are at the bottom of this file.

//Special Handling
#if exists "LEGOBatman.exe"                             //Lego Batman
	#define sApp 0xA100000
#elif exists "LEGOBatman2.exe"                          //LEGO Batman 2
	#define sApp 0xA100000
#elif exists "GameComponentsOzzy_Win32Steam_Release.dll"//Batman BlackGate
	#define sApp 0xA200000
#else
	#define sApp __APPLICATION__
#endif

//Check for ReShade Version for 64bit game Bug.
#if !defined(__RESHADE__) || __RESHADE__ < 43000
	#if exists "ACU.exe"                                //Assassin's Creed Unity
		#define App 0xA0762A98
	#elif exists "BatmanAK.exe"                         //Batman Arkham Knight
		#define App 0x4A2297E4
	#elif exists "DOOMx64.exe"                          //DOOM 2016
		#define App 0x142EDFD6
	#elif exists "RED-Win64-Shipping.exe"               //DragonBall Fighters Z
		#define App 0x31BF8AF6
	#elif exists "HellbladeGame-Win64-Shipping.exe"     //Hellblade Senua's Sacrifice
		#define App 0xAAA18268
	#elif exists "TheForest.exe"                        //The Forest
		#define App 0xABAA2255
	#elif exists "MonsterHunterWorld.exe"               //Monster Hunter World
		#define App 0xDB3A28BD
	#elif exists "FarCry5.exe"                          //Farcry 5
		#define App 0xC150B805
	#else
		#define App sApp
	#endif
#else
	#define App sApp
#endif

//Game Hashes//
#if (App == 0xC19572DDF || App == 0xFBEE8027 ) //PCSX2 | CEMU
	#define RHW 1
	#define SPF 2
	#define HMT 1
	#define DSW 1
	#define EDW 1
#elif (App == 0xC753DADB )	//ES: Oblivion
    #define DA_X 0.0605
    #define DF_Y 0.025
    #define DA_Y 12.50
    //#define DE_X 1
    //#define DE_Y 0.8125
    //#define DE_Z 0.375	
	#define BMT 1    
	#define DF_Z 0.125 
	#define DG_Z 0.130//Min
    #define DI_Z 0.250//0.160//Trim
	#define FOV 1
#elif (App == 0x7B81CCAB || App == 0xFB9A99AB )	//BorderLands 2 & Pre-Sequel
	#define DA_Y 25.0
	#define DA_Z 0.00025
	#define DA_X 0.03750
	#define DB_Y 2 //Note This would require Lagacy Mode Once the trasition is made.
	#define DB_W 4
	#define DE_X 5
	#define DE_Y 0.625
	#define DE_Z 0.300
	#define DF_X 0.300
	#define SMS 0      //SM Toggle Separation
	#define DL_X 0.600 //SM Tune
	#define DL_Y 2 //SM Weapon Smooth
	#define DL_Z 0.75 //SM Local Smooth
	#define DL_W 0.050 //SM Perspective
	#define NDW 1
#elif (App == 0x2D950D30 )	//Fallout 4
	#define DA_X 0.05
	#define DF_Y 0.012
	#define DA_Y 10.5
//	#define DB_Y 3
	#define DE_X 4
	//#define DE_Y 0.500
	#define DE_Z 0.375
	#define DG_W -0.0875 //Neg Pop out
	#define BMT 1    
	#define DF_Z 0.012
	#define DB_W 6
	#define FOV 1
	#define RHW 1
	#define DSW 1
#elif (App == 0x3950D04E )	//Skyrim: SE
	#define DA_Y 13.75 //12.5 - 17.5
	#define DA_Z -0.000375
	#define DA_X 0.0480
	#define DB_Z 0.1125
	#define DF_Y 0.0525 //0.0575 //0.0465
	#define DB_Y 2 //Auto Mode Works But this game is better locked.
	#define DE_X 6
	#define DE_Y 0.375
	#define DE_Z 0.4375
	#define DG_Z 0.025 //Min
    #define DI_Z 0.025 //Trim
	#define BMT 1
	#define DF_Z 0.13
	#define DB_W 7
#elif (App == 0x142EDFD6 || App == 0x2A0ECCC9 || App == 0x8B0C2031 )	//DOOM 2016
	#define DA_Y 23.125
	#define DA_Z -0.00010
	#define DA_X 0.071
	#define DB_Z 0.1125
	#define DF_Y 0.01875
	#define DB_Y 1 //Auto Mode Works But this game is better locked.
	#define DE_X 4
	#define DE_Y 0.500
	#define DE_Z 0.4375
	#define DB_W 8
    #define DG_Z 0.001
	#define BMT 1
	#define DF_Z 0.145
	#define PEW 1
#elif (App == 0x17232880 || App == 0x9D77A7C4 || App == 0x22EF526F )	//CoD:Black Ops | CoD:MW2 |CoD:MW3
	#define DA_Y 12.5
	#define DB_Y 3
	#define DB_W 9
#elif (App == 0xD691718C )	//CoD:Black Ops II
	#define DA_Y 13.75
	#define DA_W 1
	#define DB_W 10
#elif (App == 0x7448721B )	//CoD:Ghost
	#define DA_Y 13.75
	#define DB_Y 2
	#define DA_W 1
	#define DB_W 11
#elif (App == 0x23AB8876 || App == 0xBF4D4A41 )	//CoD:AW | CoD:MW Re
	#define DA_Y 13.75
	#define DB_Y 2
	#define DA_W 1
	#define DB_W 12
#elif (App == 0x1544075 )	//CoD:IW
	#define DA_Y 13.75
	#define DB_Y 2
	#define DA_W 1
	#define DB_W 13
#elif (App == 0x697CDA52 )	//CoD:WaW
	#define DA_Y 12.5
	#define DB_Y 3
	#define DB_W 14
#elif (App == 0x4383C12A || App == 0x239E5522 || App == 0x3591DE9C )	//CoD | CoD:UO | CoD:2
	#define DB_W 15
	#define RHW 1
#elif (App == 0x73FA91DC )	//CoD: Black Ops IIII
	#define DA_Y 22.5
	#define DA_W 1
	#define DB_W 16
#elif (App == 0x37BD797D )	//Quake DarkPlaces
	#define DA_Y 15.0
	#define DB_Y 2
	#define DB_W 17
#elif (App == 0x34F4B6C )	//Quake 2 XP
	#define DB_Y 2
	#define DB_W 18
#elif (App == 0xED7B83DE )	//Quake 4 #ED7B83DE
	#define DA_Y 15.0
	#define DB_W 19
#elif (App == 0x886386A )	//Metro Redux Games
	#define DA_Y 10.50 //11.0 // 12.5
	#define DA_X 0.05
    #define DF_Y 0.05 //0.03
    #define DB_Z 0.125
	//#define DB_Y 2
	#define DE_X 4
	//#define DE_Y 0.5
	#define DE_Z 0.375
	#define BMT 1    
	#define DF_Z 0.150 
	#define DG_Z 0.0125 //Min
    #define DI_Z 0.1000 //Trim	
	#define DB_W 21
    #define DF_X 0.250 
	#define DJ_W 0.250
	#define SMS 2      //SM Toggle Separation
	#define DL_X 0.550 //SM Tune
	#define DL_Y 1     //SM Weapon Tune
	#define DL_Z 1.000 //SM Local Smooth
	#define DL_W 0.050 //SM Perspective
	#define FOV 1
#elif (App == 0xF5C7AA92 || App == 0x493B5C71 )	//S.T.A.L.K.E.R: Games
	#define DA_Y 10.0
	#define DB_Y 4
	#define DB_W 26
#elif (App == 0xDE2F0F4D )	//Prey 2006
	#define DB_W 28
	#define DB_Y 3
#elif (App == 0x36976F6D )	//Prey 2017
	#define DA_W 1
	#define DA_X 0.04625
	#define DA_Y 21.25
	#define DB_Y 2
	#define DE_X 4
	#define DE_Y 0.5
	#define DE_Z 0.300
    #define DF_Y 0.05
	#define WSM 7
	#define OW_WP "Read Help & Change Me\0Custom WP\0Prey High Settings and <\0Prey 2017 Very High\0"
	#define RHW 1
	#define PEW 1
	#define WPW 1
#elif (App == 0xBF757E3A )	//Return to Castle Wolfenstein
	#define DA_Y 8.75
	#define DB_Y 2
	#define DB_W 31
#elif (App == 0xC770832 || App == 0x3E42619F )	//Wolfenstein: The New Order | The Old Blood
	#define DA_Y 25.0
	#define DB_Y 5
	#define DA_Z 0.00125
	#define DB_W 33
#elif (App == 0x6FC1FF71 ) //Black Mesa**
	#define DA_Y 10.0
	#define DA_Z 0.000125
	#define DA_X 0.0400
    #define DF_Y 0.0125
	//#define DB_Y 2
	#define DB_Z 0.08625
    #define DE_X 5
    #define DE_Y 0.625
    #define DE_Z 0.375
    #define DG_W -0.225 //Pop out
	#define DB_W 35
	#define BMT 1
	#define DF_Z 0.1375
	#define SMS 1     //SM Toggle Separation
	#define DL_X 0.6375//SM Tune //0.5125 //0.560 
	#define DL_Y 1    //SM Weapon Tune 
	#define DL_W 0.0  //SM Perspective	
#elif (App == 0x6D3CD99E ) //Blood 2
	#define DA_X 0.105
	#define DB_Y 2
	#define DE_X 4
	//#define DE_Y 0.50
	#define DE_Z 0.475
	#define WSM 5
	#define DB_W 8
	#define OW_WP "Read Help & Change Me\0Custom WP\0Blood 2 All Weapons\0Blood 2 Bonus Weapons\0Blood 2 Former\0"
	#define WPW 1
	#define NFM 1
	#define RHW 1
#elif (App == 0xF22A9C7D || App == 0x5416A79D ) //SOMA
	#define DA_Y 35.00 //23.125 //21.25 //25.0
	#define DA_X 0.075 //0.1025 //0.110 //0.095
    #define DF_Y 0.055
	#define DB_Y 5
	#define BMT 1
	#define DF_Z 0.15625
	#define DA_Z -0.000125
	#define DG_W 0.1
	#define DB_W 38
	#define DG_Z 0.350 //0.130
	//#define DI_Z 0.130
	#define DE_X 4
	//#define DE_Y 0.375
	#define DE_Z 0.375
	#define DF_X 0.25
	#define DJ_W 0.40 //TEMP
	#define FOV 1
	#define RHW 1
#elif (App == 0x6FB6410B ) //Cryostasis
	#define DA_Y 13.75
	#define DB_Y 3
	#define DB_W 39
#elif (App == 0x16B8D61A) //Unreal Gold with v227
	#define DA_Y 17.5
	#define DB_Y 1
	#define DB_W 40
	#define DF_W 0.534
	#define HMT 1
#elif (App == 0x68EF1B4E || App == 0xC103D998 || App == 0xFAB47970 || App == 0x539E792B ) //Serious Sam Fusion | Serious Sam 4: Planet Badass | Serious Sam Siberian Mayhem/Unrestricted
	#define DA_W 1
	#define DA_X 0.0825 //0.0875
	#define DF_Y 0.0125
	#define DA_Y 10.0
	#define DA_Z 0.1
	#define DB_Y 1
	#define DB_W 42
    #define DF_X 0.25
    #define DJ_W 0.6
	#define DE_X 5
	#define DE_Y 0.5
	#define DE_Z 0.375
	#define DB_Z 0.150
	#define BMT 1    
	#define DF_Z 0.190 //0.170
	#define DG_Z 0.075 //Min
    #define DI_Z 0.125//Trim
	#define NDW 1
	#define RHW 1
	#define PEW 1
	#define LBC 2  //Letter Box Correction Offsets With X & Y
	#define DH_Z 0.256
	#define DH_W 0.0
#elif (App == 0x308AEBEA ) //TitanFall 2
	#define DB_Y 4
	#define DB_W 44
#elif (App == 0x5FCFB1E5 ) //Project Warlock
	#define DA_W 1
	#define DA_X 0.0475
    #define DF_Y 0.02375
	#define DA_Y 50.0
    #define DB_X 1
	#define DB_Y 4
	#define DE_X 4
	//#define DE_Y 0.500
	#define DE_Z 0.400
	#define DG_W 0.125
	#define DB_W 45
	#define DSW 1
#elif (App == 0x7DCCBBBD ) //Kingpin Life of Crime
	#define DA_Y 10.0
	#define DB_Y 4
	#define DB_W 46
	#define RHW 1
#elif (App == 0x9C5C946E ) //EuroTruckSim2
    #define DB_X 1
	#define DA_X 0.06
    #define DF_Y 0.05
	#define DA_Y 7.0
	#define DA_Z -0.007
	#define DB_W 47
	#define DB_Y 3 //1 or 5
#elif (App == 0xB302EC7 || App == 0x91D9EBAF ) //F.E.A.R | F.E.A.R 2: Project Origin
	#define DA_X 0.110
	#define DA_Y 12.0
	#define DA_Z 0.00025
	#define DB_Y 5
	#define DE_X 4
	//#define DE_Y 0.625
	#define DE_Z 0.375
	//#define DG_W 0.25
	#define DB_W 48
	//#define DF_X 0.225
	#define DSW 1 //?
	#define FOV 1
	#define RHW 1
#elif (App == 0x2C742D7C ) //Immortal Redneck CP alt 1.9375
	#define DA_Y 20.0
	#define DB_Y 5
	#define DB_W 50
#elif (App == 0x663E66FE ) //NecroVisioN & NecroVisioN: Lost Company
	#define DA_Y 10.0
	#define DB_Y 2
	#define DB_W 52
#elif (App == 0xAA6B948E ) //Rage64
	#define DA_Y 20.0
	#define DB_Y 2
	#define WSM 3
	#define DB_W 2
#elif (App == 0x44BD41E1 ) //Bioshock Remaster
	#define DA_Z 0.001
	#define DB_Y 3
	#define WSM 3
	#define DB_W 4
#elif (App == 0x7CF5A01 ) //Bioshock 2 Remaster
	#define DA_Z 0.001
	#define DB_Y 3
	#define DF_W 0.5034
	#define WSM 3
	#define DB_W 5
	#define HMT 1
#elif (App == 0x22BA110F ) //Turok: DH 2017
	#define DA_X 0.002
	#define DA_Y 250.0
#elif (App == 0x5F1DBD3B ) //Turok2: SoE 2017
	#define DA_X 0.002
	#define DA_Y 250.0
#elif (App == 0x3FDD232A ) //FEZ
	#define DA_X 0.2125
	#define DB_Y 4
	#define DA_Z -0.901
#elif (App == 0x619964A3 ) //What Remains of Edith Finch
	#define DA_Y 50.0
	#define DA_Z 0.000025
	#define DA_W 1
	#define DB_Y 2
#elif (App == 0x941D8A46 ) //Tomb Raider Anniversary :)
	#define DA_Y 75.0
	#define DA_Z 0.0206
	#define DB_Y 2
#elif (App == 0xF0100C34 ) //Two Worlds Epic Edition
	#define DA_Y 43.75
	#define DA_Z 0.07575
#elif (App == 0xA4C82737 ) //Silent Hill: Homecoming
	#define DA_Y 25.0
	#define DA_X 0.0375
	#define DA_Z 0.11625
	#define DB_Y 4
	#define DF_W 0.5
	#define HMT 1
#elif (App == 0x61243AED ) //Shadow Warrior Classic source port
	#define DA_Y 10.0
	#define DA_X 0.05
	#define DA_Z 1.0
	#define DB_Y 5
#elif (App == 0x5AE8FA62 ) //Shadow Warrior Classic Redux
	#define DA_Y 10.0
	#define DA_X 0.05
	#define DA_Z 1.0
	#define DB_Y 5
#elif (App == 0xFE54BF56 ) //No One Lives Forever and 2
	#define DA_X 0.0375
	#define WSM 9
	#define OW_WP "Read Help & Change Me\0Custom WP\0No One Lives Forever\0No One Lives Forever 2\0"
	#define WPW 1
	#define RHW 1
#elif (App == 0x9E7AA0C4 ) //Shadow Tactics: Blades of the Shogun
	#define DA_X 0.150
	#define DF_Y 0.050
	#define DA_Y 7.0
	#define DA_Z 0.0005
	#define DB_Y 5
	#define DB_Z 0.305
	#define DB_X 1
	#define BMT 1
	#define DF_Z 0.125 //0.1875
    //#define DG_W 0.100 //Pop
	#define SMS 1      //SM Toggle Separation
	#define DL_X 0.825 //SM Tune
	#define DL_Z 0.000 //SM Local Smooth
	#define DL_W 0.000 //SM Perspective
	#define DM_X 12    //HQ Tune
	#define DM_Y 4     //HQ Boost
	#define DM_Z 1     //HQ Smooth
	#define RHW 1
#elif (App == 0xE63BF4A4 ) //World of Warcraft DX12
	#define DA_Y 7.5
	#define DA_W 1
	#define DB_Y 3
	#define DB_Z 0.1375
	#define NDW 1
	#define RHW 1
#elif (App == 0x5961D1CC ) //Requiem: Avenging Angel
	#define DA_Y 37.5
	#define DA_X 0.0375
	#define DA_Z 0.8
	#define DF_W 0.501
	#define HMT 1
#elif (App == 0x86D33094 || App == 0x19019D10 ) //Rise of the TombRaider | TombRaider 2013
	#define DA_X 0.0725
	#define DB_Y 3
	#define DA_Y 25.0
	#define DA_Z 0.0200375
	#define DE_X 2
	#define DE_Y 0.375
	#define DE_Z 0.375
#elif (App == 0x60F436C6 ) //RESIDENT EVIL 2  BIOHAZARD RE2
	#define DA_X 0.1375
	#define DF_Y 0.025
	#define DB_Y 3
	#define DB_Z 0.015
	#define DA_Y 50
    #define DA_Z -0.625
	#define DA_W 1
	#define DE_X 1 //REF only works now with this. Since this system works in tandom.
	#define DE_Y 0.275
	#define DE_Z 0.4375
	#define REF 2 //Fix can go from 1 - 10 and 10 is low 1 is High
	#define DI_W 3.75 //Adjustment for REF
    #define DG_W 0.2
    #define PEW 1
    #define DAA 1    
#elif (App == 0xF0D4DB3D ) //Never Alone
	#define DA_X 0.1375
	#define DB_Y 2
	#define DA_Y 31.25
	#define DA_Z 0.004
#elif (App == 0x3EB1D73A ) //Magica 2
	#define DA_X 0.2
	#define DB_Y 5
	#define DA_Y 27.5
	#define DA_Z 0.007
#elif (App == 0x6D35D4BE ) //Lara Croft and the Temple of Osiris
	#define DA_X 0.15
	#define DB_Y 4
	#define DB_Z 0.4
	#define DA_Y 75.0
	#define DA_Z 0.021
	#define REF 1
#elif (App == 0xAAA18268 ) //Hellblade **
    #define DA_W 1
    #define DA_Y 20
    #define DA_X 0.050
    #define DF_Y 0.070
    #define DB_Y 3
    #define DE_X 1 
    #define DE_Y 0.375
    #define DE_Z 0.300
    #define DG_W 0.375 //Detection was moved back.
    #define DG_Z 0.050 //Min  New system from RE7 was used
    #define DI_Z 0.0625//Trim New system from RE7 was used
    #define BMT 1      //Disables Auto ZPD
	#define DF_Z 0.060 //Sets Manual Mode power.
	#define SMS 1      //SM Toggle Separation
	#define DL_X 0.500 //SM Tune
	#define DL_Z 0.000 //SM Local Smooth ???
	#define DL_W 0.050 //SM Perspective
	#define PEW 1
	#define DAA 1
#elif (App == 0x287BBA4C || App == 0x59BFE7AC ) //Grim Dawn 64bit/32bit
	#define DB_Y 2
	#define DA_Y 125.0
	#define DA_Z 0.003
#elif (App == 0x8EAF7114 ) //Firewatch
	#define DB_Y 3
	#define DA_Y 5.0
	#define DA_X 0.0375
	#define DB_X 1
	#define DA_W 1
#elif (App == 0x6BDF0098 ) //Dungeons 2
	#define DA_X 0.100
	#define DB_Y 3
	#define DA_Z 0.005
	#define DB_X 1
#elif (App == 0x56E482C9 ) //DreamFall Chapters
	#define DA_Y 10.0
	#define DA_X 0.0375
	#define DB_Y 2
	#define DA_Z 0.001
	#define DB_X 1
#elif (App == 0x31BF8AF6 ) //DragonBall Fighters Z
	#define DA_Y 10.0
	#define DA_W 1
	#define DA_X 0.130
	#define DB_Y 3
	#define DB_Z 0.625 //I know, I know........
#elif (App == 0x3F017CF ) //Call of Cthulhu
	#define DA_W 1
	#define DA_X 0.0375
	#define DB_Y 3
#elif (App == 0x874318FE || App == 0x7CBA2E8C || App == 0x69277DAF ) //Batman Arkham Asylum / City / Origins
	#define DA_Y 18.75
	#define DA_X 0.0375
	#define DA_Z 0.00025
	#define DB_Y 4
	#define DB_Z 0.15
	#define RHW 1
#elif (App == 0xA100000 ) //Lego Batman 1 & 2
	#define DA_Y 27.5
	#define DA_X 0.125
	#define DA_Z 0.001
	#define DB_Y 2
	#define DB_Z 0.025
	#define REF 1
#elif (App == 0x5F2CA572 ) //Lego Batman 3
	#define DA_X 0.03
	#define DA_Z 0.001
	#define DB_Y 4
	#define RHW 1
#elif (App == 0xA200000 ) //Batman BlackGate
	#define DA_Y 12.5
	#define DA_X 0.0375
	#define DA_Z 0.00025
	#define DB_Y 3
#elif (App == 0xCB1CCDC ) //BATMAN TTS
	#define NCW 1 //Not Compatible
#elif (App == 0x4A2297E4 ) //Batman Arkham Knight
	#define DA_Y 22.500
	#define DA_X 0.04375
	#define DB_Y 4
#elif (App == 0xE9A02687 ) //BattleTech
	#define DA_W 1
	#define DB_X 1
	#define DA_Y 75.0
	#define DA_X 0.250
	#define DB_Y 1
	#define REF 1
	#define RHW 1
#elif (App == 0x1335BAB8 ) //BattleField 1
	#define DA_W 1
	#define DA_Y 8.125
	#define DA_X 0.04
	#define DB_Y 5
	#define REF 2
#elif (App == 0xA0762A98 ) //Assassin's Creed Unity
	#define DA_W 1
	#define DA_Y 25.0
	#define DA_Z 0.00025
	#define DA_X 0.04375
	#define DB_Z 0.2
#elif (App == 0xC990B77C ) //Assassin's Origins
	#define DA_W 1
	#define DA_Y 50.0
	#define DA_X 0.050
	#define DB_Y 1
	#define DE_X 2
	#define DE_Y 0.375
	#define DE_Z 0.375
	#define DB_Z 0.1
	#define DF_Y 0.005  
	#define BMT 1    
	#define DF_Z 0.130
	#define DG_Z 0.07  
	#define DI_Z 0.120
#elif (App == 0xBF222C03 ) //Among The Sleep
	#define DA_X 0.05
	#define DA_Y 15.0
	#define DA_Z 0.0005
	#define DB_Y 4
	#define DB_X 1
	#define IDF 1
#elif (App == 0xB75F3C89 ) //Amnesia: The Dark Descent
	#define DA_X 0.05
	#define DA_Y 45.0
	#define DA_Z 0.0005
	#define DB_Y 3
#elif (App == 0x91FF5778 ) //Amnesia: Machine for Pigs
	#define DA_X 0.05
	#define DA_Y 45.0
	#define DA_Z 0.0005
	#define DB_Y 3
#elif (App == 0x8B0F15E7 || App == 0xCFE885A2 || App == 0xCADE8051 ) //Alan Wake | Alan Wake's American Nightmare | Alan Wake Remaster
	#define DA_X 0.04375
	#define DF_Y 0.01	
	#define DA_Y 25.0
   //#define DA_Z 0.00025 //-1.0
	#define DB_Y 1
	#define DE_X 1
	//#define DE_Y 0.325
	#define DE_Z 0.375
	#define BMT 1
	#define DF_Z 0.125
    //#define DG_W 0.100 //Pop
    #define DG_Z 0.002 //Min
	#define RHW 1
	#define PEW 1
	#define DAA 1
#elif (App == 0x56D8243B ) //Agony Unrated
	#define DA_W 1
	#define DA_X 0.04375
	#define DA_Y 43.75
	#define DB_Y 5
	#define RHW 1
#elif (App == 0x23D5135F ) //Alien Isolation
	#define DA_X 0.07
    #define DF_Y 0.0125
	#define DA_Y 22.5 // or 23.0
	#define DA_Z 0.0005
	#define DB_Y 2 
	#define DE_X 2
	#define DE_Y 0.7
	#define DE_Z 0.375
	#define DG_Z 0.0325
	#define DE_W 0.1
	#define BDF 1
	#define DC_X 0.22
	#define DC_Y -0.1
	#define DC_W -0.022
	#define RHW 1
	#define PEW 1
#elif (App == 0x5839915F ) //35MM
	#define DA_Y 35.00
	#define DB_X 1
	#define DB_Y 2
	#define RHW 1
#elif (App == 0x578862 ) //Condemned Criminal Origins
	#define DA_Y 162.5
	#define DA_Z 0.00025
	#define DA_X 0.040
	#define DB_Y 4
	#define DB_W 49
	#define RHW 1
#elif (App == 0xA67FA4BC ) //Outlast
	#define DA_Y 30.0
	#define DA_Z 0.0004
	#define DA_X 0.043750
	#define DB_Y 5
	#define RHW 1
#elif (App == 0xDCC7F877 ) //Outlast II
	#define DA_W 1
	#define DA_Y 50.0
	#define DA_Z 0.0004
	#define DA_X 0.056250
	#define DB_Y 4
	#define RHW 1
#elif (App == 0x60F43F45 ) //Resident Evil 7
	#define DA_W 1
	#define DA_Y 19.0
	#define DA_Z 0.0004
	#define DA_X 0.075
    #define DF_Y 0.1
	#define DB_Y 1
	#define DE_X 2
	#define DE_Y 0.375
	#define DE_Z 0.400
	#define DG_W 0.725 //Only Detect stuff past the screen.
    #define DG_Z 0.0425//Min
    #define DE_W 0.150 //Max
    #define DI_Z 0.180 //Trim	
	#define BDF 1
	#define DC_X 0.25
	#define DC_Y 0.1
	#define DC_Z -0.0625
	#define DC_W -0.049
	#define RHW 1
	#define PEW 1
	#define DAA 1
#elif (App == 0x60F440F8 ) //Resident Evil Village
	#define DA_W 1
	#define DA_Y 50.0 //36.0  //19.0
	//#define DA_Z 0.0004 //-0.65
	#define DA_X 0.070 //0.071 //0.075
    #define DF_Y 0.100
	#define DB_Y 1
	#define DE_X 3
	#define DE_Y 0.375
	#define DE_Z 0.400
	#define BMT 1    
	#define DF_Z 0.275 //0.300 //0.250 //0.500
	#define DG_W 0.725 //Only Detect stuff past the screen.
    #define DG_Z 0.040//Min
    //#define DE_W 0.100 //Max
    #define DI_Z 0.1375 //Trim
	#define SMS 3      //SM Toggle Separation
	#define DL_X 0.600 //SM Tune
	//#define DL_Z 0.050 //SM Local Smooth
	#define DL_W 0.050 //SM Perspective	
	#define RHW 1
	#define PEW 1
	#define DAA 1
#elif (App == 0x1B8B9F54 ) //The Evil Within
	#define DA_Y 62.5 //45.0
	#define DA_Z 0.000125
	#define DA_X 0.075 //0.100
    #define DF_Y 0.075 
	//#define DB_Y 4
	#define DE_X 3
	#define DE_Y 0.350
	#define DE_Z 0.375
	#define BMT 1 // Had to use this mode since Auto Mode was not cutting it.
	#define DF_Z 0.100
    #define DG_Z 0.050 //Min
    #define DI_Z 0.100 //Trim
#elif (App == 0x7D9B7A37 ) //The Evil Within II
	#define DA_Y 30.0
	#define DA_Z 0.0001
	#define DA_X 0.04
	#define DB_Y 5
#elif (App == 0xBF49B12E ) //Vampyr
	#define DA_W 1
	#define DA_Y 23.0
	#define DA_X 0.05
	#define DB_Y 5
#elif (App == 0x85F0A0FF ) //LUST for Darkness
	#define DA_W 1
	#define DB_X 1
	#define DA_Y 13.75
	#define DA_Z 0.001
	#define DA_X 0.04
	#define DB_Y 4
	#define DB_Z 0.125
	#define RHW 1
#elif (App == 0x706C8618 ) //Layer of Fear
	#define DB_X 1
	#define DA_Y 17.50
	#define DA_X 0.035
	#define DB_Y 5
	#define RHW 1
#elif (App == 0x2F0BD376 ) //Minecraft / BuildGDX
	#define DA_Y 22.5
	#define DA_X 0.0625
	#define DB_W 25
	#define DB_Y 3
	#define DE_X 5
	#define DE_Y 0.500
	#define DE_Z 0.375
	#define DF_Y 0.005
	#define RHW 1
#elif (App == 0x84D341E3 || App == 0x15A08799) //Little Nightmares & Little Nightmares II
	#define DA_W 1
	#define DA_Y 32.5
	#define DA_X 0.225
	#define DF_Y 0.125
	//#define DA_Z 0.0015
	#define DB_Y 5 		//ZPD Boundary Scaling
	#define DB_Z 0.325	//Auto Depth Adjust
	#define BMT 1 // Had to use this mode since Auto Mode was not cutting it.
	#define DF_Z 0.225
	#define SMS 2      //SM Toggle Separation
	#define DL_X 0.700 //SM Tune
	#define DL_Z 0.000 //SM Local Smooth
	#define DL_W 0.000 //SM Perspective
	#define PEW 1
#elif (App == 0xC282C520 ) //Observer System Redux
	#define DA_W 1
	#define DA_Y 13.5
	#define DA_X 0.05
	#define DF_Y 0.2250
	#define DA_Z -0.005
	#define DB_Z 0.0325
	#define DB_Y 5
	#define DE_X 1
	#define DE_Y 0.500
	#define DE_Z 0.400
	#define DG_W 0.100 // Slight adjustment to the ZPD Boundary
	#define BMT 1 // Had to use this mode since Auto Mode was not cutting it.
	#define DF_Z 0.1625
    #define DG_Z 0.05 //Min
    #define DI_Z 0.07 //Trim
	#define RHW 1
	#define PEW 1
#elif (App == 0xC0AC5174 ) //Observer
	#define DA_W 1
	#define DA_Y 13.5
	#define DA_X 0.05
	#define DF_Y 0.180
	#define DA_Z -0.005
	#define DB_Z 0.0325
	#define DB_Y 5
	#define DE_X 1
	#define DE_Y 0.500
	#define DE_Z 0.400
	#define DG_W 0.100 // Slight adjustment to the ZPD Boundary
	#define BMT 1 // Had to use this mode since Auto Mode was not cutting it.
	#define DF_Z 0.1625
    #define DG_Z 0.05 //Min
    #define DI_Z 0.07 //Trim
	#define RHW 1
	#define PEW 1
#elif (App == 0xABAA2255 ) //The Forest
	#define DA_W 1
	#define DB_X 1
	#define DA_X 0.038//0.0525//0.0525//0.04 //0.040 //0.038 //0.04375
    #define DF_Y 0.0075
	#define DA_Y 17.5 //10.5  //10.5  //15.5 //17.5  //20.5  //7.5 
    #define DA_Z 0.00075
	#define DB_Y 3
    #define DG_Z 0.091 //Min
    //#define DE_W 0.1625 //Max
    #define DI_Z 0.1625 //0.200 //Trim
	#define BMT 1
	#define DF_Z 0.1375
	//#define DB_W 62
	#define RHW 1
#elif (App == 0x67A4A23A ) //Crash Bandicoot N.Saine Trilogy
	#define DA_Y 7.5
    #define DF_Y 0.0625
	#define DA_Z -0.250
	#define DA_X 0.1
	#define DB_Y 4
	#define DF_W 0.580
	#define HMT 1
#elif (App == 0xE160AE14 ) //Spyro Reignited Trilogy**
    #define DA_W 1
    #define DA_Y 12.5
    #define DA_Z -0.25
    #define DA_X 0.100
    #define DF_Y 0.040
    #define DB_Y 4
    #define DE_X 2
    #define DE_Y 0.500
    #define DE_Z 0.375
    #define DG_Z 0.150 //0.050 //0.100 //Min
    #define DI_Z 0.150 //0.125 //0.250 //Trim
    #define BMT 1
	#define DF_Z 0.300
	#define SMS 1      //SM Toggle Separation
	#define DL_X 0.625 //SM Tune
	#define DL_Z 1.000 //SM Local Smooth
	#define DL_W 0.100 //SM Perspective
#elif (App == 0x5833F81C ) //Dying Light
	#define DA_W 1
	#define DA_X 0.040 //0.0375//0.036
	#define DF_Y 0.0375//0.050 //0.0525	
	#define DA_Y 11.00 //10.0//12.5
	//#define DA_Z -0.175//-0.200 //-0.375 //-0.05
	#define DB_Y 4
	#define DE_X 2
	#define DE_Y 0.7000
	#define DE_Z 0.3000
	#define NDW 1
	#define PEW 1
    #define FOV 1
    #define DG_Z 0.090 //0.0875//0.080 //Min
    #define DE_W 0.105 //Max
    #define DI_Z 0.200 //Trim
	#define BMT 1
	#define DF_Z 0.1375
	#define SMS 1      //SM Toggle Separation
	#define DL_X 0.650 //SM Tune
	#define DL_Z 1.000 //SM Local Smooth
	#define DL_W 0.050 //SM Perspective
	#define WSM 2 //Weapon Settings Mode
	//#define DB_W 51
#elif (App == 0x42C1A2B ) //CoD: WWII
	#define DA_X 0.04
	#define DA_W 1
	#define DB_Y 4
	#define DB_W 12
#elif (App == 0x86562CC2 ) //STARWARS Jedi Fallen Order
	#define DA_X 0.140
	#define DA_W 1
	#define DA_Y 13.75
	#define DA_Z 0.00025
	#define DB_Y 5
	#define DE_X 1
	#define DE_Y 0.1875
	#define DE_Z 0.475
	#define DB_Z 0.375
	#define RHW 1
	#define NFM 1
#elif (App == 0x88004DC9 || App == 0x1DDA9341) //Strange Brigade DX12 & Vulkan
	#define DA_X 0.0625
	#define DF_Y 0.02
	#define DA_Y 20.0
	#define DA_Z 0.0001
	#define DB_Y 5
	#define DE_X 2
	#define DE_Y 0.3
	#define DE_Z 0.475
	#define RHW 1
#elif (App == 0xC0052CC4) //Halo The Master Chief Collection
	#define DA_X 0.037
	#define DA_W 1
	#define DA_Y 75.0
	#define DB_Y 5
	#define DE_X 4
	#define DE_Y 0.375
	#define DE_Z 0.375
	#define WSM 6
	#define OW_WP "Read Help & Change Me\0Custom WP\0Halo: Reach\0Halo: CE Anniversary\0Halo 2: Anniversary\0Halo 3\0Halo 3: ODST\0Halo 4\0"
	#define RHW 1
	#define WPW 1
#elif (App == 0x2AB9ECF9) //System ReShock
	#define DA_X 0.05
	#define DA_W 1
	#define DA_Y 11.25
	#define DA_Z 0.00125
	#define DB_Y 4
	#define DE_X 5
	#define DE_Y 0.4375
	#define DE_Z 0.375
	#define DE_W 0.055
#elif (App == 0xC50993EC) //COD: Modern Warfare 2019
	#define DA_X 0.0375
	#define DA_W 1
	#define DA_Y 37.5
	#define DA_Z 0.000125
	#define DB_Y 5
	#define DB_W 12
	#define RHW 1
#elif (App == 0xA640659C) //MegaMan 2.5D in 3D
	#define DA_X 0.150
	#define DA_Y 8.75
	#define DA_Z -1005.0
	#define DE_X 1
	#define DE_Y 0.275
	#define DE_Z 0.375
	#define RHW 1
#elif (App == 0x49654776) //Paratopic
	#define DA_X 0.05
	#define DA_W 1
	#define DA_Y 8.75
	#define DA_Z 0.000250
	#define DB_Y 3
	#define DB_X 1
	#define RHW 1
#elif (App == 0xF7590C95) //Yume Nikki -Dream Diary-
	#define DA_X 0.0625
	#define DA_W 1
	#define DA_Y 20.0
	#define DA_Z 0.002
	#define DB_X 1
	#define DB_Y 1
	#define DE_X 1
	#define DE_Y 0.35
	#define DE_Z 0.25
	#define NCW 1
	#define RHW 1
#elif (App == 0x65F37CDF) //American Truck Simulator
	#define DA_X 0.05375
	#define DA_Y 15.0
	#define DA_Z 0.005
	#define DB_X 1
	#define DB_Y 2
	#define DB_W 47
#elif (App == 0xB5789234) //The Park
	#define DA_X 0.05625
	#define DA_W 1
	#define DA_Y 12.5
	#define DB_Y 1
	#define RHW 1
#elif (App == 0xB7C22840) //Strife
	#define DA_X 0.1
	#define DA_Y 250.0
	#define WSM 3
	#define DB_W 8
	#define RHW 1
#elif (App == 0x21DC397E || App == 0x653AF1E1) //Gold Source
	#define DA_X 0.045
    #define DF_Y 0.125
	#define DA_Y 32.0//21.25
	#define DA_Z 0.0003
	#define DB_Y 3
	#define DE_X 7
	#define DE_Y 0.500
	#define DE_Z 0.375
	#define DG_W -0.1375
	#define BMT 1    
	#define DF_Z 0.123 
	#define WSM 3
	#define DB_W 9
#elif (App == 0xC2E621A5) //No Man Sky
	#define DA_X 0.045
    #define DF_Y 0.010
	#define DA_W 1
	#define DA_Y 70.0
	#define DB_Y 5
	#define DE_X 3
	#define DE_Y 0.5
	#define DE_Z 0.375
	#define BMT 1    
	#define DF_Z 0.125
	#define DG_Z 0.070//Min
//	#define DE_W 0.0 //Max
    #define DI_Z 0.100 //Trim
	#define WSM 3
	#define DB_W 10
	#define RHW 1
#elif (App == 0x1E9DCD00) //Witch it
	#define DA_X 0.0475
	#define DA_W 1
	#define DA_Y 47.5
	#define DB_Y 5
	#define DE_W 0.325
#elif (App == 0x6B58D180) //Outlaws
	#define DA_X 0.14375
	#define DA_Y 30
	#define DB_Y 5
	#define DB_Z 0.225
#elif (App == 0x5AC0F7E3) //SoF
	#define DA_X 0.04375
	#define DA_Y 17.5
	#define DB_Y 5
	#define DB_W 22
	#define DF_X 0.5
#elif (App == 0xEA8E05B6) //Only If
	#define DA_X 0.100
	#define DB_Y 1
	#define DE_X 1
#elif (App == 0xFDE0387 ) //Stranger Odd Worlds
	#define DA_X 0.100
	#define DA_Y 7.5
	#define DB_X 1
	#define DB_Y 1
	#define DE_X 2
	#define DF_W 0.5
	#define HMT 1
#elif (App == 0x242D82C4 ) //Okami HD
	#define DA_X 0.200
	#define DA_W 1
	#define DA_Z 0.001
	#define DB_Y 1
	#define DE_X 2
	#define DE_Y 0.125
	#define DE_Z 0.375
	#define DF_W 0.5
	#define HMT 1
#elif (App == 0x75B36B20 ) //Eldritch
	#define DA_Y 125.0
	#define DA_X 0.05
	#define DB_Y 4
	#define DE_X 1
	#define DB_Z 0.05
#elif (App == 0x97CBF34C ) //Dementium 2
	#define DA_Y 18.75
	#define DA_X 0.04125
	#define DB_Y 5
	#define DB_W 51
	#define DB_X 1
	#define RHW 1
#elif (App == 0x5925FCC8 ) //Dusk
	#define DA_Y 25.0
	#define DA_X 0.05
	#define DB_Y 5
	#define DA_W 1
	#define DB_X 1
	#define DE_X 4
	#define DE_Z 0.375
#elif (App == 0xDDA80A38 ) //Deus Ex Rev DX9
	#define DA_X 0.04375
	#define DA_Y 20
	#define DB_Y 3
	#define DB_W 23
	#define DF_W 0.534
	#define HMT 1
	#define DF_X 0.025
#elif (App == 0x1714C977) //Deus Ex DX9
	#define DA_X 0.05
	#define DA_Y 125.0
	#define DB_Y 3
	#define DB_W 24
	#define DF_W 1.0
	#define HMT 1
	#define DF_X 0.05
#elif (App == 0x92583CDD ) //Legend of Dungeon
	#define DA_Y 12.5
	#define DA_Z 0.185
	#define DA_X 0.075
	#define DB_Y 4
	#define DB_X 1
#elif (App == 0xDB3A28BD ) //Monster Hunter World
	#define DA_Y 17.5
	#define DA_X 0.075
	#define DA_W 1
	#define DB_Y 5
	#define DE_X 1
	#define DE_Y 0.300
	#define DE_Z 0.4375
	#define NDW 1
#elif (App == 0xC073C2BB ) //StreetFighter V
	#define DA_Y 14.0
	#define DA_X 0.250
	#define DA_W 1
	#define DB_Y 4
	#define DB_Z 0.550
	#define DE_X 1
	#define DE_Y 0.375
	#define DE_Z 0.375
#elif (App == 0xCFB8DD02 ) //DIRT RALLY 2.0
	#define DA_Y 11.25
	#define DA_X 0.040
	#define DE_W 0.350
#elif (App == 0x2F55D5A3 || App == 0x4A5220AF ) //ShadowWarrior 2013 DX11 & DX9
	#define DA_X 0.035
	#define DB_Y 4
	#define DE_X 5
	#define DE_Z 0.375
#elif (App == 0x56301DED ) //ShadowWarrior 2
	#define DA_W 1
	#define DA_X 0.045
    #define DF_Y 0.025
	#define DA_Y 12.5
	#define DE_X 2
	#define DE_Y 0.375
	#define DE_Z 0.4375
	#define DG_W 0.6 //Pop out
	#define BMT 1    
	#define DF_Z 0.1
	#define DG_Z 0.09  
	#define DI_Z 0.170
	#define SMS 1      //SM Toggle Separation
	#define DL_X 0.610 //SM Tune
	#define DL_Z 1.000 //SM Local Smooth
	#define DL_W 0.050 //SM Perspective
	#define DSW 1
	#define NDW 1
#elif (App == 0x892CA092 ) //Farcry
	#define DA_Y 7.0
	#define DA_Z 0.000375
	#define DB_Z 0.105
	#define DA_X 0.055
	#define DB_Y 4
	#define WSM 3
	#define DB_W 12
	#define DF_X 0.13875
#elif (App == 0x9140DBE0 ) //Farcry 2
	#define DA_X 0.05
	#define DB_Y 4
	#define DE_X 5
	#define DE_Z 0.375
	#define WSM 3
	#define DB_W 13
	#define RHW 1
#elif (App == 0xA4B66433 ) //Farcry 3
	#define DA_X 0.05
	#define DB_Y 4
	#define DE_X 5
	#define DE_Z 0.375
	#define DE_W 0.350
	#define RHW 1
#elif (App == 0xC150B652 ) //Farcry 4
	#define DA_Y 8.75
	#define DA_W 1
	#define DA_X 0.0375
	#define DB_Y 4
	#define DE_X 5
	#define DE_Z 0.375
	#define DE_W 0.360
#elif (App == 0x2EB82B07 ) //Farcry Primal
	#define DA_Y 8.75
	#define DA_W 1
	#define DA_X 0.0375
	#define DB_Y 4
	#define DE_X 5
	#define DE_Z 0.375
	#define DE_W 0.360
#elif (App == 0xC150B805 ) //Farcry 5
	#define DA_Y 8.75
	#define DA_W 1
	#define DA_X 0.0375
	#define DB_Y 4
	#define DE_X 5
	#define DE_Z 0.375
	#define DE_W 0.360
	#define RHW 1
#elif (App == 0xE3AD2F05 ) //Sauerbraten
	#define DA_Y 25.0
	#define DA_X 0.05
	#define DB_Y 5
	#define DF_X 0.150
	#define WSM 3
	#define DB_W 22
#elif (App == 0xF0F2CF6A ) //Dragon Ball Z: Kakarot
	#define DA_W 1
	#define DA_Y 24.0
	#define DA_X 0.250
	#define DB_Y 3
	#define DE_X 1
	#define DE_Y 0.375
	#define DE_Z 0.400
	#define DB_Z 0.500 //Yay I know
#elif (App == 0xFA2C0106 ) //Hat in Time
	#define DA_X 0.250
	#define DB_Y 4
	#define DE_X 1
	#define DE_Z 0.4375
    #define DF_Y 0.045
    #define DG_Z 0.075
	#define RHW 1
	#define DSW 1
#elif (App == 0xCD0E316F ) //Sonic Adventure DX Modded with BetterSADX
	#define DA_Y 8.75
	#define DA_X 0.1125
	#define DB_Y 3
	#define DE_X 2
	#define DE_Y 0.375
	#define DE_Z 0.4375
	#define DB_Z 0.250
	#define RHW 1
#elif (App == 0xF9B1845A ) //Rime
	#define DA_W 1
	#define DA_Y 15.0
	#define DA_X 0.145
	#define DB_Y 4
	#define DE_X 1
	#define DE_Y 0.299
	#define DE_Z 0.400
#elif (App == 0x71170B42 ) //Blood: Fresh Suppy
	#define DA_Y 212.5
	#define DA_X 0.175
	#define BMT 1
	#define DF_Z 0.275
	#define DB_Y 4
	#define DE_X 1
	#define DE_Y 0.375
	#define DE_Z 0.375
	#define DG_W 0.25
	#define DSW 1
#elif (App == 0x8F615A99 ) //Frostpunk
	#define DA_Y 9.375
	#define DA_X 0.250
	#define DB_Y 4
	#define DE_X 1
	#define DE_Y 0.5
	#define DE_Z 0.400
#elif (App == 0x29B47A0A ) //KingMaker
	#define DB_X 1
	#define DA_W 1
	#define DA_Y 18.75
	#define DA_Z 0.0075
	#define DA_X 0.150
	#define DB_Y 5
	#define DE_X 1
	#define DE_Y 0.5
	#define DE_Z 0.300
#elif (App == 0xBF70711C ) //Singularity
	#define DA_Y 15.0
	#define DA_X 0.0375
	#define DE_X 1
	#define DE_Z 0.375
	#define DF_X 0.175
	#define WSM 3
	#define DB_W 15
	#define RHW 1
#elif (App == 0x905631F2 || App == 0x76F4DCB0 ) //Crysis DX10 32bit / 64bit
    #define DA_X 0.0375
    #define DF_Y 0.120
    #define DA_Y 7.0
    //#define DA_Z -0.0005
    #define DE_X 4
    #define DE_Y 0.500
    #define DE_Z 0.375
	//#fine DG_W 0.325 //Pop out
    #define BMT 1    
    #define DF_Z 0.110
	#define DB_W 44
	#define DF_X 0.3625
#elif (App == 0x6061750E ) //Mirror's Edge
	#define DA_Y 12.25
	#define DF_Y 0.020
	#define DA_X 0.040
	#define DB_Y 5
    #define DB_Z 0.01
	#define DE_X 1
	#define DE_Y 0.50
	#define DE_Z 0.375
	#define WSM 3
	#define DB_W 19
	#define DSW 1
#elif (App == 0xC3AF1228 || App == 0x95A994C8 ) //Spellforce
	#define DA_Y 145.0
	#define DA_Z 0.001
	#define DB_Z 0.05
	#define DA_X 0.05
	#define DB_Y 3
	#define DE_X 1
	#define DE_Y 0.235
	#define DE_Z 0.375
	#define HMT 1
	#define DF_W 0.5
#elif (App == 0xD372612E ) //Raft
	#define DA_W 1
	#define DB_X 1
	#define DA_X 0.04375
	#define DB_Y 4
	#define NDW 1
#elif (App == 0xC06FE818 ) //BorderLands 3
	#define DA_Y 16.25
	#define DA_Z 0.0001375
	#define DA_X 0.0415
	#define DF_Y 0.041
	#define DB_Z 0.05
	#define DA_W 1
	#define DB_Y 4
	#define DB_W 5
	#define DE_X 5
	#define DE_Y 0.425
	#define DE_Z 0.300
	#define DG_W 0.210   //Pop out
	#define DF_X 0.085
	#define BMT 1    
	#define DF_Z 0.170
	#define DG_Z 0.075   //Min
	#define DI_Z 0.185 //Trim
	#define SMS 1 //SM Toggle Separation
	#define DL_X 0.5625//SM Tune
	#define DL_Z 0.250 //SM Local Smooth
	#define DL_W 0.050 //SM Perspective
	#define NDW 1
	#define DAA 1
#elif (App == 0x3C8DE8E8 ) //Metro Exodus
	#define DA_Y 12.5 // What A mess
	//#define DA_X 0.05
	#define DA_Z 0.000375
	#define DA_W 1
	#define DB_Y 4
	//#define DE_W 0.08
	#define DE_W 0.0275
#elif (App == 0x7FC671B6 ) //Doom Eternal
	#define DA_Y 50.0
	#define DA_Z 0.00009375
	#define DA_W 1
	#define DB_Y 3
	#define DE_X 5
	#define DE_Y 0.550
	#define DE_Z 0.333
	//#define DG_Z 0.080 //Min
    #define DE_W 0.125 //Max
	//#define DE_W 0.09375
	#define DA_X 0.03125
	#define DF_Y 0.03125
	//#define DA_X 0.0375 //Alternet settings Not used.
	#define WSM 3
	#define DB_W 17
	#define PEW 1
#elif (App == 0x47F294E9 ) //Octopath Traveler
	#define DA_Y 250.0
	#define DA_Z 0.000375
	#define DA_X 0.1175
	#define DA_W 1
	#define DB_Y 2
	#define DE_X 1
	#define DE_Y 0.5625
	#define DE_Z 0.375
	#define DG_W 0.2125
	#define RHW 1
#elif (App == 0x21CB998 ) //.Hack//G.U.
	#define DA_Y 22.5
	#define DA_X 0.125
	#define DB_Y 3
	#define DE_X 1
	#define DE_Y 0.500
	#define DE_Z 0.3
	#define RHW 1
	#define NFM 1
#elif (App == 0x9CC5C8E0 ) //GTA V
	#define DA_Y 18.75
	#define DA_W 1
	#define DA_X 0.0475
	#define DB_Y 4
	#define DE_X 1
	#define DE_Y 0.325
	#define DE_Z 0.375
	#define DB_Z 0.05
	#define RHW 1
	#define PEW 1
	#define NFM 1
#elif (App == 0x8CD23575 ) //Dark Souls: Remastered
	#define DA_Y 68.0
	#define DA_Z -0.001
	#define DA_X 0.072
	#define DF_Y 0.1
	#define DB_Y 2
	#define DE_X 2
	#define DE_Y 0.350
	#define DE_Z 0.375
	#define DG_W 0.0625
	#define DG_Z 0.075  
	#define DI_Z 0.100
	#define BMT 1    
	#define DF_Z 0.125
	#define PEW 1
	#define FOV 1
#elif (App == 0x9E071BC0 ) //Dark Souls III
	#define DA_Y 25.0
	#define DA_Z 0.000125
	#define DA_X 0.1
	#define DF_Y 0.05625
	#define DB_Y 3
	#define DE_X 2
	#define DE_Y 0.5000
	#define DE_Z 0.4375
	#define DG_Z 0.1125 //Min
	//#define DE_W 0.225
	#define PEW 1
	#define FOV 1
#elif (App == 0x5D4939C9 ) //Dark Souls II
	#define DA_Y 22.5
	#define DA_Z 0.00025
	#define DA_X 0.110
	#define DB_Y 3
	#define DE_X 2
	#define DE_Y 0.50
	#define DE_Z 0.40
	#define DE_W 0.1
#elif (App == 0xFB111509 ) //Dark Souls II Scholar of the First Sin
	#define DA_Y 46.5
	#define DA_X 0.045
	#define DF_Y 0.0425
	#define DB_Y 3
	#define DE_X 1
	#define DE_Y 0.550
	#define DE_Z 0.350
	#define DG_W 0.127 //Pop out
	#define DG_Z 0.050 //Min
	#define DI_Z 0.100 //Trim
	#define DAA 1
	#define PEW 1
	#define FOV 1
#elif (App == 0xCE5313C2 ) //BorderLands Enhanced
	#define DA_Y 18.75
	#define DA_Z 0.0005
	#define DA_X 0.055
	#define DF_Y 0.02
	#define DB_Z 0.075
	#define DB_Y 1
	#define DB_W 3
	#define DE_X 4
	#define DE_Y 0.425
	#define DE_Z 0.375
	#define DG_Z 0.3975
	#define DF_X 0.225
	#define NDW 1
#elif (App == 0x2ECAAF29 || App == 0xE19E4830 || App == 0xE19E4830  ) //Half-Life 2 | Left 4 Dead 2
	#define DA_Y 15.0
	#define DA_X 0.045
    #define DF_Y 0.020
	#define DB_Z 0.115
	#define DB_Y 3
	#define DB_W 20
	#define DE_X 7
	#define DE_Y 0.5
	#define DE_Z 0.375
	#define DF_X 0.105
	#define DJ_W 0.175
	#define BMT 1
	#define DF_Z 0.105
	#define DSW 1
	#define RHW 1
#elif (App == 0xEACB4D0D ) //Final Fantasy XV Windows Edition
	#define DA_X 0.0375
	#define DA_Y 30.0
	#define DB_Y 3
	#define DE_X 2
	#define DE_Y 0.5
	#define DE_Z 0.375
	#define RHW 1
#elif (App == 0xAC4DF2C4 ) //Mafia II Definitive Edition
	#define DA_X 0.05
	#define DA_Y 37.5
	#define DA_Z 0.0007
	#define DB_Y 4
	#define DE_X 1
	#define DE_Y 0.5
	#define DE_Z 0.375
	#define DFW 1
	#define RHW 1
#elif (App == 0xEA75DEDE ) //Lost Planet Colonies
	#define DA_X 0.05
	#define DA_Y 37.5
	#define DB_Y 4
	#define DE_X 2
	#define DE_Y 0.5
	#define DE_Z 0.375
	#define RHW 1
#elif (App == 0xEFC486AF ) //Lost Planet 2 DX11
	#define DA_X 0.04375
	#define DA_Y 37.5
	#define DA_Z 0.001
	#define DB_Y 4
	#define DE_X 2
	#define DE_Y 0.5
	#define DE_Z 0.375
#elif (App == 0x97937D77 ) //Lost Planet 3 Dx9
	#define DA_X 0.05
	#define DA_Y 20.0
	#define DA_Z 0.001
	#define DB_Y 4
	#define DE_X 2
	#define DE_Y 0.5
	#define DE_Z 0.375
	//#define DE_W 0.3875
	#define RHW 1
#elif (App == 0x9896B9F5 ) //Old City: Leviathan
	#define DA_X 0.030
	#define DA_Y 82.5
	#define DA_Z 0.0015
	#define DB_Y 4
	#define DE_X 1
	//#define DE_Y 0.250
	#define DE_Z 0.375
	#define DB_Z 0.075
	//#define DG_Z 0.5
	#define DSW 1
#elif (App == 0xE4F6014F ) //Shovel Knight
	#define DB_X 1
	#define DA_X 0.035
	#define DA_Y 22.5
	#define DA_Z 0.483
	#define RHW 1
#elif (App == 0x94EFD213 ) //Chex Quest HD
	#define DA_W 1
	#define DA_X 0.1
	#define DA_Y 112.5
	#define DA_Z 0.0000125
	#define DB_Y 4
	#define DE_X 4
	#define DE_Y 0.5
	#define DE_Z 0.375
	#define DB_Z 0.125
	#define WSM 3
	#define DB_W 23
#elif (App == 0xF6F3C763 ) //WRATH
	#define DA_X 0.065
	#define DA_Y 75.0
	#define DA_Z 0.00005
	#define DB_Y 2
	#define DE_X 4
	#define DE_Y 0.5
	#define DE_Z 0.375
	#define DB_Z 0.090
	#define DB_W 29
	#define DF_X 0.1
#elif (App == 0xB05C57BC ) //HellBound
	#define DA_W 1
	#define DA_X 0.0325
	#define DA_Y 25.0
	#define DB_Y 5
	#define DE_W 0.025
#elif (App == 0x9FC060AE ) //STRIFE: Gold Edition
	#define DA_X 0.0375
	#define DA_Y 23.75
	#define DA_Z 0.00025
	#define DB_X 1
	#define DB_Y 4
	#define NCW 1
#elif (App == 0x6DDCD106 ) //The Town of Light
	#define DA_X 0.100
	#define DA_Y 10.0
	#define DB_X 1
	#define DB_Y 4
	#define DE_X 1
	#define DE_Y 0.5
	#define DE_Z 0.375
#elif (App == 0x6367B705 ) //Transference
	#define DA_W 1
	#define DA_X 0.09375
	#define DA_Y 111.0
	#define DA_Z 0.00025
	#define DB_X 1
	#define DB_Y 4
	#define DE_X 2
	#define DE_Y 0.5
	#define DE_Z 0.375
	#define DB_Z 0.05
	#define DE_W 0.05625
#elif (App == 0x38ED56AE ) //Heavy Rain
	#define DA_X 0.0325
	#define DA_Y 50.0
	//#define DA_Z 0.001
	#define DB_Y 5
	#define DE_X 1
	#define DE_Y 0.5
	#define DE_Z 0.375
	#define DB_Z 0.0675
	#define NCW 1
#elif (App == 0x2F1ABF4A ) //Detroit Become Human
	#define DA_W 1
	#define DB_X 1
	#define DA_X 0.0375
	#define DA_Y 50.0
	//#define DA_Z 0.001
	#define DB_Y 4
	#define DE_X 1
	#define DE_Y 0.5
	#define DE_Z 0.375
#elif (App == 0x9DA6C947 ) //Beyond Two Souls
	#define DA_X 0.0625
    #define DF_Y 0.0275
	#define DA_Y 20.00
	#define DB_Y 3
	#define DE_X 1
	//#define DE_Y 0.5
	#define DE_Z 0.375
	#define DG_W 0.175
	//#define BMT 1
	//#define DF_Z 0.1375
	#define DSW 1
	#define LBC 1 //Letter Box Correction With X & Y
    #define DH_X 1.0
    #define DH_Y 1.315
#elif (App == 0x89351FC4 ) //3DSen Games
	#define DA_W 1
	#define DB_X 1
	#define DA_X 0.1
	#define DA_Y 375.0
	#define DB_Y 5
	#define DE_X 1
	#define DE_Y 0.5
	#define DE_Z 0.250
	#define DFW 1
#elif (App == 0xF55F26A1 ) //Tekken 7
	#define DA_W 1
    #define DF_Y 0.025
	#define DA_Y 100.0
	#define DA_Z 0.0001
	#define DB_Y 1
	#define DE_X 1
	#define DE_Y 0.5
	#define DE_Z 0.375
	#define DE_W 0.440
	#define DAA 1
#elif (App == 0x9BD7A4FD ) //Starwars Battle Front II
	#define DA_W 1
	#define DA_X 0.05
	#define DA_Y 10.0
	#define DB_Y 3
	#define DE_X 2
	#define DE_Y 0.5
	#define DE_Z 0.375
	#define DE_W 0.075
#elif (App == 0x14E41902 ) //jsHexen II
	#define RHW 1
	#define NFM 1
	#define DA_X 0.06
	#define DA_Y 17.5
	#define DA_Z 0.0003
	#define DB_Y 4
	#define DB_W 22
#elif (App == 0x12C96DB0 ) //Hexen 2 Hammer of Thyrion
	#define RHW 1
	#define NFM 1
	#define DA_X 0.06
	#define DA_Y 17.5
	#define DA_Z 0.0003
	#define DB_Y 4
	#define DB_W 22
#elif (App == 0x54A39BDC ) //Hexen 2 FTEQW 64
	#define DA_X 0.06
	#define DA_Y 30.0
	#define DA_Z 0.0003
	#define DB_Y 4
	#define WSM 3
	#define DB_W 24 //???
#elif (App == 0x6281C1AC ) //DarkSiders Warmastered Edition
	#define DA_X 0.05
	#define DA_Y 30.0
	#define DB_Y 3
	#define DE_X 2
	#define DE_Y 0.5
	#define DE_Z 0.375
#elif (App == 0x763E5FA5 ) //DarkSiders 2 Depthinitve Edition
	#define DA_X 0.05
	#define DA_Y 15.0
	#define DB_Y 3
	#define DE_X 1
	#define DE_Y 0.65
	#define DE_Z 0.375
	#define DB_Z 0.025
#elif (App == 0x7F1A5568 ) //DarkSiders III
	#define DA_W 1
	#define DA_X 0.0625
	#define DA_Y 50.0
	#define DA_Z 0.0001
	#define DB_Y 4
	#define DE_X 1
	#define DE_Y 0.250
	#define DE_Z 0.4
	#define DG_W 0.125
    #define BMT 1    
    #define DF_Z 0.125
	#define DG_Z 0.450 //Min
	//#define DE_W 0.0 //Max
    #define DI_Z 0.250 //Trim
	#define SMS 1      //SM Toggle Separation
	#define DL_X 0.5125//SM Tune
	#define DL_Z 0.0   //SM Local Smooth ???
	#define DL_W 0.0   //SM Perspective
#elif (App == 0xB4C116F7 ) //Nioh
	#define DA_W 1
	#define DA_X 0.1
	#define DA_Y 162.5
	#define DA_Z 0.000125
	#define DB_Z 0.200
	#define DB_Y 1
	#define DE_X 1
	#define DE_Y 0.5
	#define DE_Z 0.4
	#define DE_W 0.05
#elif (App == 0xD30783B6 ) //X-Com
	#define DA_X 0.03
	#define DA_Y 200.0
	#define DA_Z 0.000125
	#define DB_Z 0.050
	#define DB_Y 3
	#define DE_X 1
	#define DE_Y 0.5
	#define DE_Z 0.375
	#define DE_W 0.025
	#define DSW 1
#elif (App == 0xBF53A67A ) //The Bureau: XCOM Declassified
	#define DA_X 0.04375
	#define DA_Y 29.0
	#define DA_Z 0.0003
	#define DB_Y 2
	#define DE_X 2
	#define DE_Y 0.5
	#define DE_Z 0.375
	#define DSW 1
#elif (App == 0x1764D88A || App == 0x18D54C6A ) //X-Com 2
	#define DA_X 0.20
	#define DF_Y 0.102
	#define DA_Y 28.0
	#define DB_Z 0.130
	#define DB_Y 1
	#define DE_X 2
	#define DE_Y 0.3
	#define DE_Z 0.375
	#define DG_Z 0.070
    #define DI_Z 0.125
#elif (App == 0xC60A845F ) //My Friend Pedro**
    #define DB_X 1
    #define DA_W 1
    #define DA_X 0.075
    #define DF_Y 0.030
    #define DA_Y 50.0
    #define DA_Z 0.000375
    #define DB_Z 0.13
    #define BMT 1
    #define DF_Z 0.10
#elif (App == 0xD45ACB4B ) //Murdered Soul Suspect
	#define DA_X 0.05
	#define DA_Y 37.5
	#define DA_Z 0.001
	#define DB_Y 1
	#define DE_X 1
	#define DE_Y 0.5
	#define DE_Z 0.375
	#define DSW 1
#elif (App == 0x4FF5CF63 ) //Lords of the Fallen
	#define DA_X 0.049
	#define DA_Y 70.0
	#define DA_Z 0.001
	#define DB_Y 2
	#define DE_X 1
	#define DE_Y 0.5
	#define DE_Z 0.375
	#define DE_W 0.415
	#define PEW 1
#elif (App == 0xD0AA19 ) //The Bards Tale 4
	#define DA_W 1
	#define DA_X 0.0375
	#define DA_Y 20.0
	#define DB_Y 3
	#define DE_X 1
	#define DE_Y 0.5
	#define DE_Z 0.375
	#define DAA 1
	#define PEW 1
	#define RHW 1
#elif (App == 0x54D4EAFA) //Sekiro Shadows Die Twice
	#define DA_W 1
    #define DF_Y 0.025
	#define DA_X 0.10
	#define DA_Y 31.25
	#define DA_Z -0.0525
	#define DG_W 0.25 //Pop out
	//#define DB_Z 0.125
	#define DB_Y 1
	#define DE_X 2
	#define DE_Y 0.275
	#define DE_Z 0.375
   // #define DF_Z -0.125
	#define DAA 1
	#define PEW 1
#elif (App == 0x36ECE27F ) //Supraland
	#define DA_W 1
	#define DA_Y 22.5
	#define DB_Y 2
	#define DE_X 2
	#define DE_Y 0.8
	#define DE_Z 0.375
	#define PEW 1
	#define DAA 1
#elif (App == 0x3604DCE6 ) //Remnant: From the Ashes
	#define DA_W 1
	#define DA_X 0.060
	#define DF_Y 0.05
	#define DA_Y 19.375
	#define DB_Y 2
	#define DE_X 2
	//#define DE_Y 0.450
	#define DE_Z 0.375
	#define DA_Z 0.000125 //-0.0375 //-0.050
	//#define DG_W 0.25
	//#define DF_Z -0.125
	#define DG_Z 0.0175  //Added to fix super close to cam issues.
	#define DI_Z 0.125  //and cutoff for cam
	#define BMT 1
	#define DF_Z 0.1375
	#define LBC 2  //Letter Box Correction Offsets With X & Y
	#define DH_Z 0.256
	#define DH_W 0.0
	#define SMS 2      //SM Toggle Separation
	#define DL_X 0.450 //SM Tune
	#define DL_Z 1.000 //SM Local Smooth
	#define DL_W 0.050 //SM Perspective
	#define NDW 1
	#define PEW 1
#elif (App == 0x621202BC ) //Vanquish DGVoodoo2
	#define DA_X 0.05
	#define DA_Y 15.0
	#define DB_Y 3
	#define DE_X 1La
	#define DE_Y 0.5
	#define DE_Z 0.375
	#define RHW 1
	#define NFM 1
	#define PEW 1
#elif (App == 0xA1214CD1 ) //Life is Strange
	#define DA_X 0.125
	#define DA_Y 8.0
	#define DA_Z 0.0005
	#define DB_Y 4
	#define DE_X 1
	#define DE_Y 0.5
	#define DE_Z 0.375
	#define PEW 1
	#define DSW 1
	#define DAA 1
#elif (App == 0x913AD2D ) //SpaceHulk DeathWing Enhanced Edition
	#define DA_W 1
	#define DA_X 0.04375
	#define DA_Y 9.25
    #define DF_Y 0.125
	#define BMT 1    
	#define DF_Z 0.123
	#define DG_Z 0.065//Min
    #define DI_Z 0.250 //Trim
	#define HMT 1
	#define DF_W 0.503
	#define PEW 1
	#define NDW 1
#elif (App == 0xEB5CDE17 ) //Man Of Medan
	#define DA_W 1
	#define DA_X 0.04375
	#define DA_Y 20.0
	#define DB_Z 0.3
	#define DF_Y 0.010
	#define DB_Y 4
	#define DE_X 1
	#define DE_Y 0.300
	#define DE_Z 0.300
	#define RHW 1
	#define NDW 1
	#define SPF 1
	#define DD_W -0.240
	#define LBM 1
	#define DI_X 0.879
	#define DI_Y 0.120
#elif (App == 0x62454263 ) //Red Dead Redemption 2
	#define DA_W 1
	#define DA_X 0.05
	#define DF_Y 0.11875
	#define DA_Y 35.5
	#define DB_Y 2
	#define DE_X 1
	#define DE_Y 0.5
	#define DE_Z 0.375
	#define BMT 1
	#define DF_Z 0.1525
	#define DG_Z 0.0375 //Min
    #define DI_Z 0.070 //Trim
	#define PEW 1
	#define DAA 1
	#define LBM 1
	#define DI_X 0.879
	#define DI_Y 0.120
#elif (App == 0xB5AE6CBA ) //Rage 2
	#define DA_W 1
	#define DA_X 0.04
	#define DA_Y 125.0
	#define DB_Y 2
	#define DE_W 0.06
	#define PEW 1
	#define DAA 1
#elif (App == 0x8CEACA5C ) //Dead Island
	#define DA_X 0.0475
	#define DA_Y 8.75
	#define DB_Y 2
	#define DF_Y 0.0875
#elif (App == 0xBE9001DC ) //Dead Island DE
  #define DA_W 1
	#define DA_X 0.0475
	#define DA_Y 8.75
	#define DB_Y 2
	#define DF_Y 0.0875
#elif (App == 0xCDD5E6CF ) //Legend of Grimrock
	#define DA_X 0.120
	#define DF_Y 0.135
	#define DA_Y 12.5
	#define DB_Y 3
#elif (App == 0x7C0F0E77 ) //Soulcaliber VI
	#define DA_W 1
	#define DA_X 0.070
	#define DA_Y 70.0
	#define DB_Y 5
	#define DE_X 1
	#define DE_Y 0.300
	#define DE_Z 0.375
	#define PEW 1
	#define NDW 1
#elif (App == 0x424052D0 ) //Talos Principle
	#define DA_W 1
	#define DA_X 0.10
	#define DF_Y 0.05
	#define DA_Y 20.5
	#define DA_Z 0.100
	#define DB_Z 0.200
	#define DB_Y 1
	#define DE_X 1
	#define DE_Y 0.5
	#define DE_Z 0.375
	#define WSM 3
	#define DB_W 14
	#define DAA 1
#elif (App == 0x6EC76A83 ) //Watch Dogs 2
	#define DA_W 1
	#define DA_X 0.070
	#define DA_Y 12.0
	#define DB_Y 1
	#define DE_X 2
	#define DE_Y 0.500
	#define DE_Z 0.375
	#define PEW 1
	#define DAA 1
#elif (App == 0xD9691F81 ) //Destroy All Humans!
	#define DA_W 1
	#define DA_X 0.050
	#define DA_Y 66.0
	#define DB_Y 4
	#define DE_X 1
	#define DE_Y 0.500
	#define DE_Z 0.375
	#define DF_Y 0.025
	#define NFM 1
	#define PEW 1
	#define RHW 1
#elif (App == 0x9C5C8E4D ) //INSIDE
	#define DA_X 0.050
	#define DB_Y 3
	#define DE_X 1
	#define DE_Y 0.500
	#define DE_Z 0.375
	#define BDF 1
	#define DC_X 0.6
	#define DC_Y -0.4
	#define DC_Z 0.087
	#define DC_W -0.03
	#define DB_X 1
#elif (App == 0xABADF9C2 ) //Sonic Robo Blast 2
	#define DA_X 0.1
	#define DA_Y 27.5
	#define DB_Z 0.155
	#define DB_Y 2
	#define DF_Y 0.1
#elif (App == 0x3867F04A ) //Castle of Illusion
	#define DA_X 0.1
	#define DA_Y 40.0
	#define DA_Z 0.0005
	#define DB_Y 1
	#define DF_Y 0.01
	#define DAA 1
#elif (App == 0x76CD4369 ) //Resident Evil
	#define DA_X 0.06875
	#define DA_Y 24.0
	#define DA_Z 0.00025
	#define DB_Y 2
	#define DE_X 2
	#define DE_Y 0.500
	#define DE_Z 0.375
	#define DF_Y 0.024
	#define DAA 1
	#define SPF 1
	#define DD_X 1.333
	#define DD_Y 0.933
	#define DSW 1
	#define RHW 1
	#define LBM 1
	#define DI_X 0.879
	#define DI_Y 0.125
#elif (App == 0x1AB66F8F ) //Dawn Of War III
	#define DA_X 0.125
	#define DA_Y 25.0
	#define DB_Z 0.125
	#define DA_Z 0.002
	#define DB_Y 4
	#define DE_X 2
	#define DE_Y 0.200
	#define DE_Z 0.375
	#define DF_Y 0.250
	#define DAA 1
#elif (App == 0xD86799B9 ) //Devil May Cry 5
	#define DA_W 1
	#define DA_X 0.0625
	#define DF_Y 0.020
	#define DA_Y 20.00
 //   #define DA_Z -0.375
	#define DB_Y 3
	#define DE_X 1
	#define DE_Y 0.375
	#define DE_Z 0.375
	#define DG_W 0.125
	#define BMT 1    
	#define DF_Z 0.1111 //0.100 //0.125 
	#define DG_Z 0.0325//0.0275//Min
    #define DI_Z 0.0625//Trim
	#define SMS 0      //SM Toggle Separation
	#define DL_X 0.500 //SM Tune
	#define DL_Z 0.625 //SM Local Smooth
	#define DL_W 0.050 //SM Perspective
	#define BDF 1
	#define DC_X 0.025
	#define DC_Y 0.025
	#define DC_W -0.012
	#define PEW 1
	#define DSW 1
	#define RHW 1
#elif (App == 0x5BC45541 ) //Contrast
	#define DA_X 0.075
	#define DA_Y 20.0
	#define DB_Y 1
	#define DE_X 1
	#define DE_Y 0.500
	#define DE_Z 0.375
	#define DF_Y 0.051
#elif (App == 0xEA61D579 || App == 0xCFCF902B ) //The Citadel
	#define DA_W 1
	#define DA_X 0.035
	#define DA_Y 18.6
	#define DB_Y 4
	#define DF_Y 0.05625
	#define SPF 1
	//#define DD_Y 0.675
	//#define DD_W -0.4815
	#define DD_Y 0.9
	#define DD_W -0.111
	#define WSM 3
	#define DB_W 21
	#define DSW 1
#elif (App == 0x5C0EBBE9 ) //A Plague Tale Innocence
	#define DA_W 1
	#define DA_X 0.050
    #define DF_Y 0.025
	#define DA_Y 33.00
	#define DB_Y 3
	#define DE_X 2
	#define DE_Y 0.500
	#define DE_Z 0.375
	#define PEW 1
	#define RHW 1
	#define NFM 1
	#define DSW 1
#elif (App == 0xB2B11A3C ) //Catherine with a K
	#define DA_X 0.05
	#define DF_Y 0.0125
	#define DA_Y 50.0
	#define DB_Y 3
	#define DE_X 1
	#define DE_Y 0.500
	#define DE_Z 0.375
	#define DG_W 0.4375
	#define PEW 1
	#define DAA 1
#elif (App == 0x7ABE98F0 ) //Samurai Jack
	#define DA_W 1
	#define DA_X 0.1375
	#define DF_Y 0.0125
	#define DA_Y 20.0
	#define DB_Y 5
	#define DE_X 2
	#define DE_Y 0.500
	#define DE_Z 0.375
	#define DAA 1
#elif (App == 0xC4B4435F ) //Night Cry
	#define DA_X 0.06  //ZPD
	#define DF_Y 0.025 //Separation
	#define DA_Y 50.0  //Depth
	#define DB_X 1
	#define DB_Y 3
	#define DE_X 1
	#define DE_Y 0.500
	#define DE_Z 0.375
#elif (App == 0x49F7B9C0 || App == 0x837F12C9 ) //Control DX12 | QuantumBreak DX11
	#define DA_X 0.051
	#define DF_Y 0.051
	#define DA_Y 17.5
	#define DB_Y 3
	#define DE_X 1
	#define DE_Y 0.500
	#define DE_Z 0.375
	#define BMT 1
	#define DF_Z 0.150
	#define PEW 1
	#define DAA 1
#elif (App == 0x5A7B540A ) //We Where Here Too
	#define DA_W 1
	#define DB_X 1
	#define DA_X 0.05625
	#define DA_Y 42.5
	#define DB_Y 1
	#define NDW 1
#elif (App == 0x6AB553A ) //We Where here Together
	#define DA_W 1
	#define DB_X 1
	#define DF_Y 0.05625
	#define DA_X 0.05625
	#define DA_Y 56.25
	#define DB_Y 3
	#define DE_X 4
	#define DE_Y 0.500
	#define DE_Z 0.375
	#define DB_W 30
	#define FOV 1
	#define DAA 1
	#define NDW 1
#elif (App == 0x75930301 ) //Void Bastards
	#define DA_W 1
	#define DB_X 1
	#define DF_Y 0.02625
	#define DA_X 0.0875
	#define DA_Y 56.25
	#define DB_Y 2
	#define DE_X 1
	#define DE_Y 0.625
	#define DE_Z 0.375
	#define DSW 1
	#define RHW 1
#elif (App == 0xDF3F2AB6 ) //Cloud Punk
	#define DA_W 1
	#define DB_X 1
	#define DF_Y 0.1
	#define DA_X 0.0325
	#define DA_Y 72.5
	#define DB_Y 4
	#define DE_X 1
	#define DE_Y 0.500
	#define DE_Z 0.375
	#define DSW 1
	#define RHW 1
	#define NFM 1
#elif (App == 0x4551A746 ) //The Swapper
	//#define DB_X 1
	#define DF_Y 0.0125
	#define DA_X 0.05
	#define DA_Y 99.0
	#define DA_Z -0.010
	#define DB_Y 4
	#define DE_X 1
	#define DE_Y 0.500
	#define DE_Z 0.375
	//#define DF_Z 1.0
	//#define DD_X 0.800
	//#define DD_Y 0.705
	//#define DD_Z 0.250
	//#define DD_W 0.4125
	//#define SPF 1
	#define DSW 1
	#define RHW 1
#elif ( App == 0xE0B7AF16 || App == 0xB84E12B6 ) //Horizon Chase Turbo
	#define DA_W 1
	#define DA_Y 12.5
	#define DA_X 0.175
	#define DF_Y 0.0625
	#define DB_Y 1
	#define DA_Z -0.125
	#define DE_X 1
	#define DE_Y 0.500
	#define DE_Z 0.375
	#define DSW 1
#elif (App == 0xCF0046B7 ) //SpecOps The Line
	#define DA_Y 15.0
	#define DA_X 0.05
	#define DF_Y 0.01
	#define DB_Y 4
	#define DA_Z -0.00125
	#define DE_X 1
	#define DE_Y 0.500
	#define DE_Z 0.375
	#define DSW 1
	#define PEW 1
	#define FOV 1
#elif (App == 0x6C433D70 ) //Q.U.B.E 2
	#define DA_W 1
	#define DA_Y 75.0
	#define DA_X 0.05625
	#define DF_Y 0.01
	#define DB_Y 2
	#define DB_Z 0.05625
	#define DA_Z 0.00025
	#define DE_X 4
	#define DE_Y 0.450
	#define DE_Z 0.375
	#define PEW 1
	#define FOV 1
	#define WSM 3
	#define DB_W 18
#elif (App == 0xD87951C4 ) //Horizon Zero Dawn
	//#define DA_W 1
	#define DA_Y 12.5
	#define DA_X 0.05
	//#define DF_Y 0.01
	#define DB_Y 2
	#define DB_Z 0.05
	#define DA_Z 0.0005
	#define DE_X 2
	#define DE_Y 0.3
	#define DE_Z 0.375
	#define PEW 1
	#define FOV 1
#elif (App == 0xF1BFCA91 ) //ELEX
	//#define DA_W 1
	//#define DA_Y 12.5
	#define DA_X 0.1
	//#define DF_Y 0.1
	#define DB_Y 2
	//#define DB_Z 0.1
	//#define DA_Z 0.0005
	#define DE_X 2
	#define DE_Y 0.500
	#define DE_Z 0.300
	#define PEW 1
	#define DSW 1
#elif (App == 0x2E63D83A ) //Kingdom Come Diliverance
	#define DA_W 1
	#define DA_Y 25.0
	#define DA_X 0.060
	//#define DF_Y 0.1
	#define DB_Y 1
	//#define DB_Z 0.1
	//#define DA_Z 0.0005
	#define DE_X 1
	#define DE_Y 0.500
	#define DE_Z 0.300
	#define PEW 1
	#define FOV 1
#elif (App == 0xF9341C1 ) //Valheim
	#define DA_W 1
    #define DB_X 1
	#define DA_Y 10.0
	#define DA_X 0.05125
	#define DF_Y 0.005
	#define DB_Y 2
	//#define DB_Z 0.125
	#define DA_Z 0.001
	#define DE_X 2
	#define DE_Y 0.500
	#define DE_Z 0.375
	#define DG_W 0.15
	#define PEW 1
	#define DAA 1
#elif (App == 0x8C8F544C ) //Witcher 3
	#define DA_W 1
    //#define DB_X 1
	#define DA_Y 12.5
	#define DA_X 0.0600
	#define DF_Y 0.0125
	//#define DF_Y 0.1
	#define DB_Y 4
	//#define DB_Z 0.125
	//#define DA_Z 0.0005
	#define DE_X 1
	#define DE_Y 0.450
	#define DE_Z 0.375
	#define DG_W 0.25  //Pop out allowed
	#define BMT 1
	#define DF_Z 0.150
	#define REF 15     //Fix can go from 1 - 15 and 15 is low 1 is High
	#define DI_W 1.7   //Adjustment for REF
	#define SMS 1      //SM Toggle Separation
	#define DL_X 0.675 //SM Tune
	#define DL_Z 0.000 //SM Local Smooth
	#define DL_W 0.050 //SM Perspective
	#define DM_X 8     //HQ Tune
	#define DM_Y 2     //HQ Boost
	#define DM_Z 1     //HQ Perspective
	#define PEW 1
	#define DAA 1
#elif (App == 0xA05A15C4 ) //Spooky's House of Jump Scares
	//#define DA_W 1
    //#define DB_X 1
	//#define DA_Y 12.5
	#define DA_X 0.150
	//#define DF_Y 0.1
	#define DB_Y 1
	//#define DB_Z 0.125
	//#define DA_Z 0.0005
	#define DE_X 1
	//#define DE_Y 0.500
	//#define DE_Z 0.300
	#define DSW 1
	#define RHW 1
	#define NFM 1
	#define SPF 1
	#define DD_X 0.625
	#define DD_Y 0.700
	#define DD_Z 0.600
	#define DD_W -0.425
#elif (App == 0x483A8BD9 ) //Song of Horror
	#define DA_W 1
    //#define DB_X 1
	#define DA_Y 112.5
	#define DA_X 0.1125
	#define DF_Y 0.1
	#define DB_Y 4
	//#define DB_Z 0.125
	//#define DA_Z 0.0005
	#define DE_X 2
	//#define DE_Y 0.500
	//#define DE_Z 0.300
	#define PEW 1
	#define FOV 1
    #define DAA 1
	//#define DSW 1
	//#define RHW 1
	//#define NFM 1
	//#define SPF 1
	//#define DD_X 0.625
	//#define DD_Y 0.700
	//#define DD_Z 0.600
	//#define DD_W -0.425
#elif (App == 0xB43B3B36 ) //Bendy and the Ink Machine
	#define DA_W 1
    #define DB_X 1
	#define DA_Y 12.5
	#define DA_X 0.120
	#define DF_Y 0.025
	#define DB_Y 1
	//#define DB_Z 0.125
	//#define DA_Z 0.0005
	#define DE_X 1
	//#define DE_Y 0.500
	#define DE_Z 0.300
	#define PEW 1
	#define FOV 1
    //#define DAA 1
	#define DSW 1
	//#define RHW 1
	//#define NFM 1
	//#define SPF 1
	//#define DD_X 0.625
	//#define DD_Y 0.700
	//#define DD_Z 0.600
	//#define DD_W -0.425
#elif (App == 0xEFB2EF28 ) //Remothered: Tormented Fathers
	#define DA_W 1
	//#define DB_X 1
	#define DA_Y 50.0
	#define DA_X 0.125
	#define DF_Y 0.050
	#define DB_Y 3
	//#define DB_Z 0.125
	#define DA_Z -1.250
	#define DE_X 2
	#define DE_Y 0.250
	#define DE_Z 0.375
	#define PEW 1
	#define FOV 1
	//#define DAA 1
	//#define DSW 1
	//#define RHW 1
	//#define NFM 1
	//#define SPF 1
	//#define DD_X 0.625
	//#define DD_Y 0.700
	//#define DD_Z 0.600
	//#define DD_W -0.425
#elif (App == 0x2EFA1BAF ) //Betrayer
	//#define DA_W 1
	#define DA_Y 15.0
	#define DA_X 0.05
	//#define DF_Y 0.01
	#define DB_Y 4
	//#define DB_Z 0.05625
	//#define DA_Z 0.00025
	#define DE_X 4
	//#define DE_Y 0.450
	#define DE_Z 0.375
	#define PEW 1
	#define FOV 1
	#define WSM 3
	#define DB_W 16
#elif (App == 0x75CE6926 ) //Chronicle of Riddick Assault on Dark Athena
	//#define DA_W 1
	//#define DA_Y 15.0
	//#define DA_X 0.1325
	#define DA_X 0.0666
	//#define DF_Y 0.01
	#define DB_Y 1
	//#define DB_Z 0.05625
	//#define DA_Z 0.00025
	#define DE_X 2
	//#define DE_Y 0.450
	//#define DE_Z 0.375
	#define PEW 1
	#define DSW 1
	#define RHW 1
#elif (App == 0xAA5644F9 || App == 0x1981FECC ) //Need For Speed: Heat | Payback
	#define DA_W 1
	#define DA_Y 10.0
	#define DA_X 0.1
	#define DF_Y 0.1
	#define DB_Y 4
	//#define DB_Z 0.1
	//#define DA_Z 0.00025
	//#define DE_X 2
	//#define DE_Y 0.450
	//#define DE_Z 0.375
    #define BMT 1    
    #define DF_Z 0.125
	//#define DG_Z 0.100 //Min
	//#define DE_W 0.0 //Max
    //#define DI_Z 0.150 //Trim
	#define SMS 1      //SM Toggle Separation
	#define DL_X 0.750 //SM Tune
	#define DL_Z 0.750 //SM Local Smooth
	#define DL_W 0.050 //SM Perspective
	#define PEW 1
	#define DAA 1
#elif (App == 0xBD8B2F39 ) //Assassin's Creed Odyssey
	#define DA_W 1
	#define DA_Y 30.0
	#define DA_X 0.05
	#define DF_Y 0.025
	#define DB_Y 4
	//#define DB_Z 0.125
	#define DA_Z -0.3
	#define DE_X 2
	#define DE_Y 0.250
	#define DE_Z 0.375
	#define DG_W 0.4 //Pop out allowed
	#define PEW 1
	#define FOV 1
#elif (App == 0x3D00A2BC ) //SM64 us f3dx2e
	#define DA_Y 12.5
	#define DA_X 0.050
	#define DB_Y 4
	#define DE_X 2
#elif (App == 0x892FCE80 ) //Star Trek EliteForce II
	#define DA_Y 30.0
	#define DA_X 0.100
	#define DB_Z 0.250
	#define DB_Y 1
	#define DE_X 4
	#define DE_Z 0.375
	#define WSM 3
	#define DB_W 25
#elif (App == 0xC54A173B ) //Dead or Alive 6
	#define DA_W 1
	#define DA_Y 60.0
	#define DA_X 0.06
	#define DB_Y 5
	#define RHW 1
	#define NDW 1
	#define NFM 1
#elif (App == 0x934DC835 || App == 0xD063D305 || App == 0xE29F2D4 ) //Dead Rising | Dead Rising 2 | Dead Rising 2 Off The Record
	#define DA_Y 25.0
	#define DA_X 0.125
	#define DB_Y 5
	#define DE_X 1
	#define DE_Z 0.375
	#define NDW 1
#elif (App == 0xF28EC9C2 || App == 0xF28EC143 ) //Dead Rising 3 | Dead Rising 4
	#define DA_Y 20.0
	#define DA_X 0.1
	#define DB_Y 5
	#define DE_X 2
	#define DE_Z 0.375
	#define RHW 1
	#define NDW 1
	#define NFM 1
#elif (App == 0xC402F6B8 ) //Iron Harvest
	#define DA_W 1
	#define DA_Y 125.0
	#define DA_Z 0.0015
	#define DA_X 0.250
	#define DB_Y 3
	#define DE_X 1
	#define DE_Y 0.1
	#define DE_Z 0.4
	#define PEW 1
	#define NDW 1
	#define DAA 1
#elif (App == 0xA867FE21 ) //Senran Kagura Peach Ball
	#define DA_Y 72.5
	#define DA_Z -0.0011
	#define DA_X 0.200
	#define DF_Y 0.200
	#define DB_Y 2
	#define DE_X 2
	#define DE_Y 0.075
	#define DE_Z 0.375
	#define HMT 1
	#define DF_W 0.5
	#define SDT 1 //Spcial Depth Trigger With X & Y Offsets
    #define DG_X -0.190
    #define DG_Y 0.0 
#elif (App == 0x1BDC0C4C ) //Quake Enhanced Edition
	#define DA_X 0.075 //0.07
	#define DA_Y 14.0 //13.0
	//#define DB_Y 1
	#define DE_X 4
	#define DE_Y 0.325 //0.4375
	#define DE_Z 0.375
	#define NDW 1
	#define PEW 1
	#define DF_X 0.250
	#define BMT 1
	#define DF_Z 0.225
	#define WSM 3
	#define DB_W 20
#elif (App == 0xB3729F40 ) //Rocket League Steam
	#define DA_Y 50.0
	#define DA_X 0.100
	#define DB_Y 5
	#define DSW 1
	#define NDW 1
	#define PEW 1
#elif (App == 0x1BB6E62A ) //AMID EVIL RTX
	#define DA_W 1
	#define DA_X 0.05
    #define DF_Y 0.0125
	#define DA_Y 15.0
	//#define DA_Z 0.000125
	#define DB_Y 5
	#define DE_X 4
	#define DE_Y 0.5
	#define DE_Z 0.45
	#define BMT 1    
	#define DF_Z 0.1125
	#define SMS 0      //SM Toggle Separation
	#define DL_X 0.825 //SM Tune
	#define DL_Y 0     //SM Weapon Tune
	#define DL_Z 0.500 //SM Local Smooth
	#define DL_W 0.050 //SM Perspective
	#define PEW 1
	#define DAA 1
	#define DB_W 27
#elif (App == 0xFBC55DDE || App == 0x836AD72D ) //Tormented Shouls & Demo 
	#define DA_W 1
	#define DA_Y 15.0
	#define DA_Z 0.00125
	#define DA_X 0.06125
	#define DB_X 1
	#define DB_Y 2
	#define DE_X 2
//#define DG_Z 0.288 // This works. But, may be a bit overboard. Use this if users complain about edge pop out issues. I don't think it's needed.
	#define PEW 1
#elif (App == 0x920D5D88 ) //Graven
	#define DA_W 1
	#define DA_X 0.1
	#define DA_Y 50.0
	#define DA_Z 0.00005
	#define DB_Y 5
	#define DE_X 4
	#define DE_Y 0.5
	#define DE_Z 0.375
	#define PEW 1
	#define DAA 1
	#define WSM 3
	//#define DB_W 3 //Graven WP Not used Due to Clipping on world. Even if it looks good. Maybe Give people the option???
	#define DG_Z 0.125
#elif (App == 0x6B2D15D6 ) //Rec Room Non VR
	#define DA_W 1
	#define DA_Y 11.25
	//#define DA_Z 0.00125
	#define DA_X 0.100
	#define DF_Y 0.108
	#define DB_X 1
	#define DB_Y 5
	#define DE_X 1
	#define DE_Y 0.6
	#define DE_Z 0.375
	#define DG_Z 0.075
	#define NDW 1
	#define DSW 1
#elif (App == 0xD0F69E54 ) //Yooka-Laylee
	#define DA_Y 12.50
	#define DA_Z 0.001
	#define DA_X 0.091
	#define DF_Y 0.005
	#define DB_X 1
	#define DB_Y 4
	#define DE_X 2
	#define DE_Y 0.325
    #define BMT 1    
    #define DF_Z 0.150
	#define DG_W 0.15 //allowed popout
	//#define DE_Z 0.375
#elif (App == 0x755C7E43 ) //Yooka-Laylee and the Impossible Lair
	#define DA_W 1
	#define DA_Y 80.0
	#define DA_Z 0.00025
	#define DA_X 0.0725
	#define DF_Y 0.010
	#define DB_X 1
	#define DB_Y 4
	#define DE_X 2
	#define DE_Y 0.225
	#define DG_Z 0.41125
#elif (App == 0x9FAEA815 ) //Amnesia Rebirth
	#define DA_Y 15.0
	#define DA_Z 0.0002
	#define DA_X 0.100
	//#define DF_Y 0.005
	#define DB_Y 4
	#define DE_X 1
	#define DE_Y 0.525
	#define DE_Z 0.400
#elif (App == 0x491EA19E ) //Cyberpunk 2077
	#define DA_W 1
	#define DA_Y 72.5
	#define DA_Z -0.00010
	#define DA_X 0.05125
	#define DB_Z 0.150
	#define DF_Y 0.05
	#define DB_Y 2 //?? Auto Mode didn't work well in this game.
	#define DE_X 4
	#define DE_Y 0.500
	#define DE_Z 0.4375
	#define DB_W 34
	#define DF_X 0.20
	#define DG_W 0.08
	#define BMT 1
	#define DF_Z 0.130
	#define PEW 1
#elif (App == 0xB53B8500 ) //DEATH STRANDING
	#define DA_W 1
	#define DA_Y 20.0
	#define DA_Z 0.000375
	#define DA_X 0.05
	//#define DB_Z 0.125
	#define DF_Y 0.01
	#define DB_Y 4
	#define DE_X 2
	#define DE_Y 0.375
	//#define DE_Z 0.375
	#define DG_Z 0.425
	#define DG_W 0.3 //Allow some popout
	#define PEW 1
	#define DAA 1
#elif (App == 0x87AC1510 ) //Ghostrunner
	#define DA_W 1
	#define DA_Y 245.0
	#define DA_Z 0.0000025 // Magic
	#define DA_X 0.050
	#define DF_Y 0.047
	#define DB_Z 0.0625
	#define DB_Y 1
	#define BMT 1
	#define DF_Z 0.115
	#define DE_X 4
	#define DE_Y 0.650
	#define DE_Z 0.400
	#define DB_W 43
	#define DF_X 0.1
	#define DG_Z 0.100 //Min
	//#define DE_W 0.0 //Max
    #define DI_Z 0.100 //Trim
	#define SMS 1      //SM Toggle Separation
	#define DL_X 0.810 //SM Tune
	#define DL_Y 0     //SM Weapon Tune
	#define DL_Z 0.0   //SM Local Smooth
	#define DL_W 0.050 //SM Perspective
	#define PEW 1
	#define DAA 1
#elif (App == 0x11E6C55E ) //The Suicide of Rachel Foster
	#define DA_W 1
	//#define DA_X 0.030
	#define DF_Y 0.020
	#define DA_Y 25.0 //25.0
	#define DA_Z -0.25
	#define DB_Z 0.050
	//#define DB_Y 5
	#define DE_X 4
	#define DE_Y 0.500
	#define DE_Z 0.425
    #define DG_Z 0.050 //Min
    //#define DE_W 0.105 //Max
    #define DI_Z 0.025 //Trim
	#define DG_W 0.365 //Allow much popout "Please don't abuse this."
    #define BMT 1    
    #define DF_Z 0.130 //This had to be adjusted
	#define PEW 1
	#define DAA 1
	#define RHW 1
	#define WSM 2
	#define DB_W 2
#elif (App == 0xFC960068 ) //Devolverland Expo
	#define DA_W 1
	#define DA_Y 30.0
	#define DA_X 0.050
	#define DB_Z 0.050
	#define DB_Y 5
	#define DE_X 4
	#define DE_Y 0.50
	#define DE_Z 0.375
	#define WSM 2
	#define DB_W 3
	#define PEW 1
	#define DAA 1
#elif (App == 0x59DA13F1 ) //Conarium
	#define DA_W 1
	#define DA_Y 18.75
	#define DA_X 0.0875
	#define DF_Y 0.0328125
	#define DB_Z 0.075
	#define DB_Y 3
	#define DE_X 4
	#define DE_Y 0.525
	#define DE_Z 0.400
	#define DG_Z 0.305
	#define DG_W 0.0875 //Allow much popout "Please don't abuse this."
	#define PEW 1
	#define DAA 1
	#define RHW 1
	#define WSM 2
	#define DB_W 4
	#define DF_X 0.150
#elif (App == 0xCE21A723 ) //Bully Scholarship Edition
	#define DA_Y 13.0
	#define DA_X 0.070
	#define DE_X 1
	#define DE_Y 0.5
	#define DE_Z 0.375
	#define DG_Z 0.360
	#define BMT 1
	#define DF_Z 0.1375
	#define DSW 1
	#define FOV 1
#elif (App == 0x289ABD5C ) //World Rally Championship 10
	#define DA_W 1
	#define DA_Y 20.5
	//#define DA_Z 0.000075
	#define DA_X 0.1375
	#define DF_Y 0.03
	#define DB_Y 4
	#define DE_X 1
	#define DE_Y 0.055
	#define DE_Z 0.4875
	#define WSM 2
	#define DB_W 5
	#define DF_X 0.125
	#define PEW 1
	#define DAA 1
#elif (App == 0xBCCAD1AE || App == 0x3D2B24D7 ) //Project Cars | Project Cars 2
	#define DA_Y 7.0
	//#define DA_Z 0.000075
	#define DA_X 0.16875
	//#define DF_Y 0.01
	//#define DB_Y 4
	#define DE_X 1
	#define DE_Y 0.375
	#define DE_Z 0.400
	#define DG_Z 0.125 //0.125//0.3125
	#define DE_W 0.275//0.275//0.35625
	#define DG_W 0.20 // Needed for old car.
	#define BMT 1
	#define DF_Z 0.165
	#define PEW 1
	#define DAA 1
	#define DSW 1
	#define NDW 1
#elif (App == 0x98C69E31 || App == 0xA8778B7D ) // F1 2019 | F1 2020 DX12
	#define DA_W 1
	#define DG_W 0.25
	#define DA_Y 14.5
	#define DA_X 0.07
	#define DB_Y 5
	#define DE_X 2
	#define DE_Z 0.400
	#define DG_Z 0.400
	#define PEW 1
	#define NDW 1
#elif (App == 0x164EF6B5 ) //Grid 2019
	#define DA_W 1
	#define DA_Y 11.5
	#define DA_X 0.15875
	#define DE_X 1
	//#define DE_Y 0.50
	#define DE_Z 0.400
	//#define DG_Z 0.125
	//#define DE_W 0.275
	#define DG_W 0.25
	#define BMT 1
	#define DF_Z 0.1625
	#define PEW 1
	#define DAA 1
	//#define DSW 1
	#define NDW 1
#elif (App == 0x54568EA ) //Assetto Corsa
	#define DA_X 0.05
	#define DB_Y 4
	#define DE_X 1
	#define DE_Y 0.750
	#define DE_Z 0.375
	#define DG_W 0.2
	#define PEW 1
	#define DAA 1
	#define NDW 1
	#define FOV 1
#elif (App == 0x7658447E ) //Dagon*
	#define DA_W 1
	#define DB_X 1
	#define DA_X 0.0625
	#define DA_Y 11.5
	#define DA_Z 0.00025
	#define DB_Y 5
	#define DE_X 1
	#define DE_Y 0.625
	#define DE_Z 0.375
	#define DB_Z 0
	#define DG_W 0.25
	#define DSW 1
	#define PEW 1
#elif (App == 0xC57720A6 ) //Crysis 2 DX11 1.9
	#define DA_X 0.07
	//#define DA_Y 11.5
	#define DA_Z 0.00025
	#define DB_Y 2
	#define DE_X 4
	//#define DE_Y 0.625
	//#define DE_Z 0.375
	//#define DG_W 0.25
	#define WSM 2
	#define DB_W 7
	#define DF_X 0.225
	#define DSW 1 //?
	#define PEW 1
	#define RHW 1
#elif (App == 0xDB778A3B ) //Portal 2
	#define DA_X 0.05
	#define DA_Y 20.5
	#define DA_Z 0.001
    #define DB_Z 0.105
	#define DB_Y 3
	#define DE_X 4
	#define DE_Y 0.7
	#define DE_Z 0.375
	#define DG_W 0.125
	#define DB_W 36
    #define DG_Z 0.430
	#define DSW 1
	#define PEW 1
#elif (App == 0x194A6354 ) //The Medium
    #define DA_W 1
	#define DA_X 0.125
    #define DF_Y 0.0225
	#define DA_Y 55.0
	#define DA_Z -0.025 // This can be still adjusted.
    #define DB_Z 0.145
	#define DB_Y 2
	#define DE_X 1
	#define DE_Y 0.375
	#define DE_Z 0.375
	#define DG_W 0.6125
	#define PEW 1	
#elif (App == 0xD829EFC1 ) //Ride 4
	#define DA_W 1
	#define DA_Y 16.25
	#define DA_X 0.13
	#define DE_X 2
	#define DE_Y 0.16875
	#define DE_Z 0.4875
	//#define DG_Z 0.125 //Near
	//#define DE_W 0.275 //Max
	#define DG_W 0.7
	#define BMT 1
	#define DF_Z 0.165
	#define PEW 1
	#define NDW 1
#elif (App == 0x19D0F410 ) //Zombie Army Trilogy
	#define DA_Y 12.5
	#define DA_X 0.155
	#define DA_Z -0.00025
	#define DB_Y 5
	#define DE_X 1
	#define DE_Y 0.375
	#define DE_Z 0.375
	#define DG_W 0.7
	//#define BMT 1
	//#define DF_Z 0.165
	#define PEW 1
	#define NDW 1
#elif (App == 0x8842D13 ) //Genshin Impact
	#define DA_W 1
    #define DB_X 1
	#define DA_Y 9.375
	#define DA_X 0.1
	#define DA_Z -0.01
	#define DB_Y 5
	#define DE_X 1
	//#define DE_Y 0.4375
	#define DE_Z 0.375
	#define DF_Y 0.01
	#define DG_W 0.1
	#define NDW 1
#elif (App == 0xEEAF4DE ) //Guardians of the galaxy
    #define DA_W 1
	#define DA_Y 26.00 //31.25//52
	#define DA_X 0.050
	#define DF_Y 0.0375
	#define DB_Z 0.110
	#define DB_Y 2
	#define DE_X 1
	#define DE_Y 0.375
	#define DE_Z 0.420
	#define DG_W 0.300 //0.25 //0.1625//Pop
	#define DG_Z 0.025 //0.040
	#define DI_Z 0.030
	#define BMT 1
	#define DF_Z 0.155
	#define REF 11    //Fix can go from 1 - 15 and 15 is low 1 is High
	#define DI_W 2.75 //3.25 //Adjustment for REF
	#define SMS 0 //SM Toggle Separation
	#define DL_X 0.500 //0.325 //SM Tune
	#define DL_W 0.050 //SM Perspective
	#define DAA 1
	#define PEW 1
#elif (App == 0x967BB1CC ) //HROT
	#define DA_X 0.055
	#define DF_Y 0.025
	#define DA_Y 150.0
	#define DB_Z 0.0875
	#define DB_Y 2
	#define DE_X 4
	#define DE_Y 0.550
	#define DE_Z 0.375
	#define WSM 2 //Weapon Settings Mode
	#define DB_W 11
	#define DSW 1 
	#define PEW 1
#elif (App == 0x11763BB7 ) //FATAL Frame Maiden of the Black Water.... Too Damn spooky....
	#define DA_X 0.0825
	#define DF_Y 0.040
	#define DA_Y 16.25
	#define DB_Z 0.275
	#define DB_Y 4
	#define DE_X 1
	#define DE_Y 0.250
	#define DE_Z 0.375
	#define BMT 1
	#define DF_Z 0.150
	#define PEW 1
	#define DAA 1
#elif (App == 0x88C50B03 ) //League of Legends
	#define DA_X 0.2
	#define DF_Y 0.2
	#define DA_Y 12.5
    #define DA_Z 0.004
	#define DB_Z 0.175
	#define DB_Y 2
	#define DE_X 1
	#define DE_Y 0.60
	#define DE_Z 0.375
	#define DG_W 0.1 //Allow some popout
	#define PEW 1
	#define DAA 1
#elif (App == 0x808ABB25 ) //BioShock Infinite 
	#define DA_X 0.050 // or 0.0525
    #define DF_Y 0.050 // 0.050
	#define DA_Y 12.50
    #define DA_Z -0.00125
    #define DB_Z 0.075
    #define BMT 1
	#define DF_Z 0.3375
    #define DG_Z 0.090 //Min
    #define DI_Z 0.300 //Trim
	#define SMS 3      //SM Toggle Separation
	#define DL_X 0.800 //SM Tune
	#define DL_Z 0.750 //SM Local Smooth
	#define DL_W 0.050 //SM Perspective
	#define DM_X 8     //HQ Tune
	#define DM_Y 4     //HQ Boost
	#define DM_Z 3     //HQ Smooth
	#define DSW 1
    #define FOV 1
#elif (App == 0x22CA259A ) //Kena Bridge of Spirits
    #define DA_W 1
	#define DA_X 0.225
    #define DF_Y 0.025
	//#define DA_Y 20.00
	#define DB_Y 5
	#define DE_X 1
	#define DE_Y 0.400
	#define DE_Z 0.375
	#define DG_W 0.275
	#define DSW 1
	#define LBC 1 //Letter Box Correction With X & Y
    #define DH_Z 0.0
    #define DH_W -0.05
    #define PEW 1
#elif (App == 0x1551DBDA ) //The Fogotten City
	#define DA_W 1
	#define DA_X 0.05//0.09
	#define DF_Y 0.015//0.0425
	#define DA_Z 0.000375
	#define DA_Y 16.25 //8.75
	#define DB_Z 0.2
	#define DB_Y 2
	#define DE_X 1
	#define DE_Y 0.315
	#define DE_Z 0.375
	#define DG_W 0.875 //0.9 if 0.1 zdp
	#define DG_Z 0.015
	#define PEW 1
	#define FOV 1
#elif (App == 0xD698BDD3 ) //Call of Juarez Gunslinger** [Steam]
	#define DA_X 0.045
	#define DF_Y 0.025
	#define DA_Y 12.5
	#define DB_Y 3
	#define DB_Z 0.175 
	//#define DA_Z -0.000125
	//#define BDF  1 //This didn't seem needed anymore for the steam version.
	//#define DC_X 0.22
	//#define DC_Y -0.37
	//#define DC_Z 0.16
	#define SPF 1 //This offset is still the same. Wonder what changed here.
	//#define DD_X 1
	#define DD_Y 1.067
	#define DG_Z 0.09375 //Min
	#define PEW 1
	//#define FOV 1
#elif (App == 0xA4F3EEC3 ) //Godfall
	#define DA_W 1
	#define DA_X 0.050
	#define DA_Z -0.025
	#define DA_Y 35.0
	#define DB_Y 2
	#define DE_X 1
	//#define DE_Y 0.5
	#define DE_Z 0.375
	#define PEW 1
	#define NDW 1    
#elif (App == 0x2B22A265 ) //Immortals Fenyx Rising
	#define DA_W 1
	#define DA_X 0.20
	#define DA_Z -0.15
	#define DA_Y 16.25
	#define DB_Y 3
	#define DE_X 1
	#define DE_Y 0.375
	#define DE_Z 0.375
	#define DB_Z 0.250
	#define DSW 1
	#define PEW 1
	#define NDW 1
#elif (App == 0xBCF34171 ) //Raji An Ancoent Epic
	#define DA_W 1
	#define DA_X 0.175
	#define DF_Y 0.05    
	#define DA_Z -0.1
	#define DA_Y 32.5
	#define DB_Y 5
	#define DE_X 2
	#define DE_Y 0.300
	#define DE_Z 0.375
	//#define DB_Z 0.250
	#define DG_W 0.15
	#define PEW 1
#elif (App == 0xED560119 ) //DarkSiders Genisis
	#define DA_W 1
	#define DA_X 0.075
	#define DF_Y 0.03
	#define DA_Y 102.5
	#define DA_Z -1.0  
	#define DB_Y 3
	#define DE_X 3
	#define DE_Y 0.400
	#define DE_Z 0.300
	#define DG_W 0.125
	#define DG_Z 0.375
	#define BMT 1    
	#define DF_Z 0.125
	#define SMS 1      //SM Toggle Separation
	#define DL_X 0.750 //SM Tune
	#define DL_Z 0.500 //SM Local Smooth
	#define DL_W 0.050 //SM Perspective
	#define NDW 1
#elif (App == 0x921BC951 ) //SpongeBob SquarePants: Battle for Bikini Bottom - Rehydrated
	#define DA_W 1
	#define DA_X 0.120
	#define DF_Y 0.05
	#define DA_Y 22.0
	#define DA_Z 0.000120  
	#define DB_Y 3
	#define DE_X 1
	#define DE_Y 0.400
	#define DE_Z 0.375
	#define DG_W 0.120
	#define DG_Z 0.35
	#define NDW 1
#elif (App == 0xEDC64E2B ) //The Patheless**
	#define DA_W 1
	#define DA_Y 75.0
	#define DA_X 0.025
	#define DF_Y 0.025
	#define DB_Y 5
	#define DE_X 1
	#define DE_Y 0.300
	#define DE_Z 0.375
	#define PEW 1
#elif (App == 0xFC113D8A ) //PsychoNauts 2
	#define DA_W 1
	#define DA_Y 25.0
	#define DA_X 0.1
	#define DF_Y 0.07
	#define DA_Z 0.001//-0.02
	//#define DB_Y 5
	#define DE_X 1
	//#define DE_Y 0.525
	#define DE_Z 0.375
	#define REF 6    //Fix can go from 1 - 15 and 15 is low 1 is High
	#define DI_W 2.0 //1.75 //Adjustment for REF
	#define BMT 1    
	#define DF_Z 0.100
	#define DG_Z 0.075//Min
	//#define DE_W 0.160 //Max
    #define DI_Z 0.20 //Trim
	#define LBC 2  //Letter Box Correction Offsets With X & Y
	#define DH_Z 0.255
	#define DH_W 0.0
	#define PEW 1
	#define DAA 1
#elif (App == 0xA34503F1 || App == 0xE0CAB4F3 ) //[Gothic 1.08k_mod - Gothic 2: Night of the Raven] with Kirides GD3D11 Mod
	#define DA_Y 25.0
	#define DA_X 0.05
	#define DF_Y 0.0125
	#define DB_Y 3
	#define DE_X 1
	#define DE_Y 0.375
	#define DE_Z 0.375
	#define DG_Z 0.411
	#define NDW 1
	#define RHW 1
	#define NFM 1
#elif (App == 0x509F5AD3 ) //Mortal Shell
	#define DA_W 1
	#define DA_X 0.225
	//#define DF_Y 0.025
	#define DA_Y 15.0
	//#define DA_Z -0.5
	#define DB_Z 0.320
	#define DB_Y 5
	#define DE_X 1
	#define DE_Y 0.45
	#define DE_Z 0.375
	#define DG_Z 0.375
	//#define NDW 1
	#define PEW 1
	#define DAA 1
	#define LBC 2  //Letter Box Correction Offsets With X & Y
	#define DH_Z 0.255
	#define DH_W 0.0
#elif (App == 0x3C982FAC ) //Forza Horizon 4  
	//#define DA_W 1
	#define DA_X 0.1225
	//#define DF_Y 0.05
	//#define DA_Y 10
	#define DB_Z 0.25
	//#define DA_Z 0.000120  
	#define DB_Y 4
	#define DE_X 1
	#define DE_Y 0.500
	#define DE_Z 0.375
	#define DG_W 0.25
	//#define DG_Z 0.35
	#define NDW 1
	#define PEW 1
	#define DAA 1
#elif (App == 0x3303C19A ) //Mafia Definitive Edition
	#define DA_W 1
	#define DA_X 0.175
	#define DF_Y 0.025
	#define DA_Y 10.0
	#define DB_Z 0.125
	#define DA_Z -0.125  
	#define DB_Y 5
	#define DE_X 1
	#define DE_Y 0.325
	#define DE_Z 0.375
	#define DG_W 0.125
	#define DG_Z 0.06
	#define PEW 1
#elif (App == 0x19237E38 ) //The Witness
	#define DA_W 1
	#define DA_X 0.2
	#define DF_Y 0.02
	#define DA_Y 15.0
	//#define DB_Z 0.100
	//#define DA_Z -0.125  
	#define DB_Y 4
	#define DE_X 1
	//#define DE_Y 0.5
	#define DE_Z 0.375
	#define DG_W 0.3
	//#define DG_Z 0.06
	#define DAA 1
#elif (App == 0x9255C26F ) //Crash Bandicoot 4 It's About Time**
	#define DA_W 1
	#define DA_Y 10.0
	#define DB_Y 4
	#define DA_Z -0.125
	#define DA_X 0.125
	#define DE_X 1
	//#define DE_Y 0.500
	#define DE_Z 0.250
	#define DF_Y 0.0625
	#define DF_W 0.580
	#define DG_W 0.125
	#define HMT 1
	#define PEW 1
#elif (App == 0x4F255CDB ) //Mortal Kombat 11 DX11** Note needs dx12
	#define DA_W 1
	#define DA_Y 22.5  //adjusted
	//#define DA_Z 0.004 //This was the issue
	#define DB_Y 2 //adjusted
	#define DA_X 0.125 //adjusted
	#define DF_Y 0.140 //adjusted
	#define DE_X 1
	#define DE_Y 0.300 //adjusted
	#define DE_Z 0.400
	#define DG_W 0.300 //Added as a buffer to allow little bit of popout.
	#define DB_Z 0.300 //Added To counteract issues with close to cam animations.
	#define PEW 1 
#elif (App == 0xA80D9183 ) //LEGO Marvel Super Heros
	#define DA_X 0.175
	#define DF_Y 0.025
	#define DE_X 5
	#define DE_Y 0.500
	#define DE_Z 0.375
	#define PEW 1
	#define DSW 1  
#elif (App == 0xC7A1F6B8 ) //NieR Replicant ver.1.22474487139
	#define DA_X 0.1375
	#define DF_Y 0.012
	#define DA_Y 25.0    
	//#define DB_Z 0.125
	#define DB_Y 3
	#define DE_X 1
	#define DE_Y 0.400
	#define DE_Z 0.375
	#define DG_Z 0.01
	#define DI_Z 0.225
	#define BMT 1
	#define DF_Z 0.2125
	#define DG_W 0.1375
	#define DAA 1  
#elif (App == 0xDE4C92BB ) //Halo Infinite
	#define DA_W 1
	#define DA_X 0.07 //0.06 //0.075
	#define DF_Y 0.02 //0.018
	#define DA_Y 80.0 //90.0  
	//#define DB_Z 0.125
	#define DB_Y 1
	#define DE_X 4
	#define DE_Y 0.475
	#define DE_Z 0.4
	//#define DG_Z 0.01
	//#define DI_Z 0.225
	#define BMT 1
	#define DF_Z 0.110
	#define DG_W -0.075
	#define WSM 2 //Weapon Settings Mode
	#define DB_W 19//Weapon Selection
	#define DF_X 0.13
	#define DJ_W 0.7 //1.0
	#define DG_Z 0.06
	#define DI_Z 0.070
	#define SMS 0      //SM Toggle Separation
	#define DL_X 0.640 //SM Tune
	#define DL_Y 2 //SM Tune Weapon
	//#define DL_Z 0.500 //SM Local Smooth
	#define DL_W 0.100   //SM Perspective
	#define NDW 1  
#elif (App == 0x312862CF ) //Aliens: Fireteam Elite**
	#define DA_W 1
	#define DA_X 0.125  //Adjusted
	#define DF_Y 0.025
	#define DA_Y 15.00
	#define DA_Z -0.0125
	#define DB_Y 5      //Not Good enough for this game.
	#define DE_X 1
	#define DE_Y 0.375
	#define DE_Z 0.400 //adjusted to make it react faster.
	#define DG_W 0.250
	#define DG_Z 0.07  //Added to fix super close to cam issues.
	#define DI_Z 0.10  //The cutoff for above value.
	#define BMT 1      //Added to override auto depth
	#define DF_Z 0.125 //Locked adjusted value.
	#define PEW 1
	#define NDW 1
#elif (App == 0x98746774 ) //God of War
	#define DA_W 1
	#define DA_X 0.14
	#define DF_Y 0.070
	#define DB_Y 5
	#define DE_X 1
	#define DE_Y 0.475
	#define DE_Z 0.300
	#define DG_W 0.150
	#define DG_Z 0.0375  
	#define DI_Z 0.10  
	#define BMT 1    
	#define DF_Z 0.140
	#define SMS 1      //SM Toggle Separation
	#define DL_X 0.500 //SM Tune
	//#define DL_Y 0     //SM Weapon Tune
	#define DL_Z 0.000 //SM Local Smooth
	#define DL_W 0.100 //SM Perspective
	#define PEW 1
#elif (App == 0x1A2B216E ) //Crysis Remastered
	#define DA_W 1
	#define DA_X 0.050
	#define DF_Y 0.01
	#define DA_Y 11.25
	//#define DB_Z 0.100
	//#define DB_Y 5
	#define DE_X 4
	#define DE_Y 0.500
	#define DE_Z 0.300
	#define BMT 1    
	#define DF_Z 0.125
	#define WSM 2 //Weapon Settings Mode
	#define DB_W 12
    #define DF_X 0.350 
	#define DJ_W 0.330
	#define PEW 1
#elif (App == 0x130E0740 ) //Crysis 2 Remastered
	#define DA_X 0.050
	#define DF_Y 0.005
	#define DA_Y 13.75
	//#define DB_Z 0.100
	//#define DB_Y 5
	#define DE_X 4
	#define DE_Y 0.375
	#define DE_Z 0.375
	#define BMT 1    
	#define DF_Z 0.1375
	#define WSM 2 //Weapon Settings Mode
	#define DB_W 13
    #define DF_X 0.300 
	#define DJ_W 0.330
	#define PEW 1 
	#define DAA 1
#elif (App == 0x2EFB1B0B ) //Crysis 3 Remastered
	#define DA_X 0.0375
	#define DF_Y 0.0040
	#define DA_Y 17.5
	//#define DB_Z 0.100
	//#define DB_Y 5
	#define DE_X 4
	#define DE_Y 0.375
	#define DE_Z 0.375
	#define BMT 1    
	#define DF_Z 0.180
	#define WSM 2 //Weapon Settings Mode
	#define DB_W 14
    #define DF_X 0.225 
	#define DJ_W 0.300
	#define SMS 3      //SM Toggle Separation
	#define DL_X 0.45 //SM Tune
	#define DL_Y 1     //SM Weapon Tune
	#define DL_Z 0.80  //SM Local Smooth
	#define DL_W 0.07  //SM Perspective
	#define PEW 1 
	#define DAA 1 
#elif (App == 0x16848B0F ) // HITMAN 3**
    #define DA_W 1
    #define DA_X 0.140
    #define DF_Y 0.21    
    #define DB_Y 5
    #define DE_X 1
    #define DE_Y 0.4375
    #define DE_Z 0.300
	#define PEW 1
#elif (App == 0xB5F52BDD ) // Abzu
    #define DA_W 1
    #define DA_X 0.075
    #define DF_Y 0.010
	#define DA_Y 20.00    
    #define DB_Y 5
    #define DE_X 1
    //#define DE_Y 0.500
    #define DE_Z 0.300
	#define BMT 1    
	#define DF_Z 0.150 
	#define DG_Z 0.100//Min
    #define DI_Z 0.125 //Trim
#elif (App == 0x2D1A3028 ) //Bright Memory: Infinite**
    #define DA_W 1
    #define DA_X 0.130 //0.125 //0.140 
    #define DF_Y 0.100
    #define DA_Y 53.00  //55.00 //52.5
    #define DB_Z 0.250
    #define DE_X 2
    #define DE_Y 0.750 //.0500 
    #define DE_Z 0.375
    #define DG_W 1.25 //disallow some popout
	#define BMT 1    
	#define DF_Z 0.130
	//#define WSM 2 //Weapon Settings Mode
	//#define DB_W 24//Weapon Selection
	#define DG_Z 0.2125//Min //0.200 //0.225
    #define DI_Z 0.750 //Trim
	#define SMS 2      //SM Toggle Separation
	#define DL_X 0.500 //SM Tune
	#define DL_Z 0.750 //SM Local Smooth
	#define DL_W 0.050 //SM Perspective
	#define PEW 1
#elif (App == 0x2ECE874 ) //Roblox Games
    #define DA_W 1
    #define DA_X 0.050//0.044//0.071
    #define DF_Y 0.025
    #define DA_Y 57.5 //70.0 //35.0 //37.5
    #define DE_X 1
    #define DE_Y 0.500
    #define DE_Z 0.4375
    #define DG_W 1.5
	#define BMT 1    
	#define DF_Z 0.180 
	#define DG_Z 0.075 //0.0625 //Min
    #define DI_Z 0.200 //0.180 //0.175 //0.2125
    //#define DK_X 2 //FPS Focus Method
    #define DK_Y 0 //Eye Eye Selection
    #define DK_Z 2 //Eye Fade Selection
    #define DK_W 1 //Eye Fade Speed Selection
#elif (App == 0x3C98315F ) //Forza Horizon 5  
	//#define DA_W 1
	#define DA_X 0.2
	//#define DF_Y 0.05
	#define DA_Y 11.0 //7.75 //10.0 //12.5
	#define DB_Z 0.225
	#define DA_Z -0.00025  
	#define DB_Y 4
	#define DE_X 1
	#define DE_Y 0.250
	#define DE_Z 0.475
	#define DG_W 0.375 //Allow popout
    #define DG_Z 0.025
	#define BMT 1    
	#define DF_Z 0.120 
	#define NDW 1
	#define PEW 1
	#define DAA 1
#elif (App == 0x4698602A ) // It takes two* WIP
    #define DA_W 1
    #define DA_Y 36.25 //Needs to be stronger since we zoom out a lot
    #define DA_X 0.060
    #define DF_Y 0.130//This was set too high. I noticed in you profiles you like to use this a lot. Try to keep it lower then what you set it too. Eye Strain can be caused by this.   
    //#define DB_Y 4  //This option getting phased out and replaced with BMT: Balance Mode for 3D profile creation in most instances.
    #define DE_X 1
    #define DE_Y 0.250
    #define DE_Z 0.4375
    #define DG_W 0.350 //Allow popout   
	#define BMT 1     //BMT 1 needs DF_Z set to a value from 0.0-0.250
	#define DF_Z 0.120 
	#define DG_Z 0.060//Min
    #define DI_Z 0.200//Trim
    #define LBC 1     //Letter Box Correction
	#define DH_Z 0.0  //Pos offset X    
	#define DH_W -0.25//Pos offset Y
	#define NDW 1  
	#define DAA 1
#elif (App == 0x822AF64D ) //The Outer Worlds**
    #define DA_W 1
    #define DA_X 0.050
    #define DF_Y 0.040
	#define DA_Z -0.0375//0.0000375  
    #define DA_Y 50.0
    #define DB_Z 0.075
    #define DB_Y 5
    #define DE_X 6
    #define DE_Y 0.500
    #define DE_Z 0.375  //This value needs to be low cause you climb ladders like in F.E.A.R, so it jitters like crazy with fast updates when moving
    #define DG_W -0.150 //Disallow popout 
    //#define DK_X 2
    #define BMT 1     //BMT 1 needs DF_Z set to a value from 0.0-0.250
	#define DF_Z 0.123 
	#define DG_Z 0.070//Min
    #define DE_W 0.170
    #define DI_Z 0.070//Trim
    #define WSM 2
    #define DB_W 6
    #define DF_X 0.250 
	#define DJ_W 0.125
    #define DK_W 2 //Set speed
    #define PEW 1  
    #define FOV 1
#elif (App == 0x3B03D773 ) //HardReset Redux
    #define DA_X 0.0625
    #define DF_Y 0.0425
    #define DA_Y 11.0
    #define HMT 1
	#define DF_W 0.5
	#define BMT 1    
	#define DF_Z 0.100 
	#define DG_Z 0.075 //Min
    #define DI_Z 0.225 //Trim
#elif (App == 0x51C8FDAA ) //Assassin's Valhalla
	#define DA_W 1
	#define DA_Y 50.0
    #define DA_Z -0.05
	#define DA_X 0.045
	#define DB_Y 1
	#define DE_X 1
	#define DE_Y 0.375
	#define DE_Z 0.375
	#define DB_Z 0.1
	#define DF_Y 0.0025 
    //#define DG_W -0.100 //Disallow popout  
	#define BMT 1    
	#define DF_Z 0.1375
	#define DG_Z 0.07  
	#define DI_Z 0.120
	#define SMS 1      //SM Toggle Separation
	#define DL_X 0.500 //SM Tune
	#define DL_Z 0.500 //SM Local Smooth ???
	#define DL_W 0.050 //SM Perspective
    #define PEW 1 
#elif (App == 0xB4403655 ) //Elden Ring
	#define DA_W 1
    #define DA_X 0.123
    #define DF_Y 0.025
	#define DA_Y 30.00
    //#define DA_Z -0.300
	#define DE_X 1
	#define DE_Y 0.500
	#define DE_Z 0.375
    #define DG_W -0.050 //Disallow popout  
	#define BMT 1    
	#define DF_Z 0.125
	#define DG_Z 0.025//Min
    #define LBC 2     //Letter Box Correction
    #define DH_X 1.340
    #define PEW 1 
    #define DAA 1
    #define DSW 1
#elif (App == 0xC5A76A71 ) //The Turing Test
	#define DA_W 1
    #define DA_X 0.045
    #define DF_Y 0.045
	#define DA_Y 37.5
    #define DA_Z 0.0015
	#define BMT 1    
	#define DF_Z 0.110
	#define DG_Z 0.0875//Min
	#define DE_W 0.160 //Max
    #define DI_Z 0.150 //Trim
#elif (App == 0x548BD6AD ) //Lost Ark
    #define DA_X 0.075
    #define DF_Y 0.225
	#define DA_Y 55
    //#define DA_Z 0.0015
	#define DE_X 1
	#define DE_Y 0.4
	#define DE_Z 0.4
	#define BMT 1    
	#define DF_Z 0.100
	#define DG_Z 0.070//Min
    #define DI_Z 0.070 //Trim
//    #define SPF 1
//    #define DD_Z 0.007 
#elif (App == 0xD360643 ) //BeamNG
	#define DA_W 1
    #define DA_X 0.2
    #define DF_Y 0.01375
	#define DA_Y 8.75
    //#define DA_Z 0.0015
    #define DE_X 1
	#define DE_Y 0.3
	#define DE_Z 0.4
	#define BMT 1    
	#define DF_Z 0.1375
	#define DG_Z 0.050//Min
//	#define DE_W 0.160 //Max
    #define DI_Z 0.100 //Trim
//    #define DD_Z 0.007 
#elif (App == 0x28900EB9 ) //Planet Zoo
	#define DA_W 1
    #define DA_X 0.1
    #define DF_Y 0.025
	#define DA_Y 77.5
    //#define DA_Z 0.0001
    #define DB_Z 0.250
    #define DE_X 1
	#define DE_Y 0.325
	#define DE_Z 0.350
	#define BMT 1    
	#define DF_Z 0.130
	#define DG_Z 0.100//Min
//	#define DE_W 0.0 //Max
    #define DI_Z 0.100 //Trim
#elif (App == 0x6EA7BBB ) //Tales from Borderlands
    #define DA_X 0.075
    #define DF_Y 0.005
	#define DA_Y 25.0
    #define DA_Z 0.0005
    //#define DB_Z 0.250
    #define DE_X 1
	#define DE_Y 0.400
	#define DE_Z 0.375
	#define BMT 1    
	#define DF_Z 0.1
	#define DG_Z 0.100//Min
//	#define DE_W 0.0 //Max
    #define DI_Z 0.125 //Trim
#elif (App == 0x8FDE4FCF ) //Ni No Kuni II: Revenant Kingdom**
    #define DA_X 0.075
    #define DF_Y 0.050
    #define DA_Y 12.50
    //#define DA_Z 0.0005
    #define DB_Y 2
    #define DE_X 1
    #define DE_Y 0.375
    #define DE_Z 0.400
	#define BMT 1    
	#define DF_Z 0.125
	#define SMS 0      //SM Toggle Separation
	#define DL_X 0.600 //SM Tune
	#define DL_Z 1.000 //SM Local Smooth
	#define DL_W 0.000 //SM Perspective
    #define PEW 1
#elif (App == 0xFA6649D4 ) //Shadow Warrior 3
	#define DA_W 1
    #define DA_X 0.070
    #define DF_Y 0.007
	#define DA_Y 40.0
    #define DA_Z 0.0001
    //#define DB_Z 0.250
	#define BMT 1    
	#define DF_Z 0.1375
	#define DG_Z 0.160//Min
//	#define DE_W 0.0 //Max
    #define DI_Z 0.300 //Trim
	#define SMS 1      //SM Toggle Separation
	#define DL_X 0.700 //SM Tune
	#define DL_Z 0.700 //SM Local Smooth
	#define DL_W 0.025 //SM Perspective
    #define FOV 1
    #define PEW 1
#elif (App == 0x45C250C6 ) //Dying Light 2
	#define DA_W 1
	#define DA_X 0.0425
	#define DF_Y 0.05125  //0.0525	
	#define DA_Y 10.0
	#define DA_Z -0.200
	#define DB_Y 4
	#define DE_X 2
	#define DE_Y 0.825
	//#define DE_Z 0.25
    #define DG_Z 0.090 //0.0875//0.080 //Min
    #define DE_W 0.105 //Max
    #define DI_Z 0.150 //0.215 //Trim
	#define BMT 1
	#define DF_Z 0.200
	#define WSM 2 //Weapon Settings Mode
	//#define DB_W 52
	#define BDF 1    //Barrel Distortion Fix k1 k2 k3 and Zoom
	#define DC_X 0.00
	#define DC_Y 0.150
	#define DC_Z -0.030
	#define DC_W -0.0200
	#define NDW 1
	#define PEW 1
    #define FOV 1
	#define RHW 1
    #define NFM 1
#elif (App == 0xBCFD90CA ) //FFVII Remake Intergrade 
	#define DA_W 1
	#define DA_X 0.090
	#define DF_Y 0.050	
	#define DA_Y 21.0 //15
    //#define DA_Z 0.00025 //-1.0
	#define DE_X 1
	#define DE_Y 0.325
	#define DE_Z 0.400
    #define DG_Z 0.002 //Min
    //#define DE_W 0.105 //Max
    #define DI_Z 0.200 //Trim
	#define BMT 1
	#define DF_Z 0.1125 //0.1875
    #define DG_W 0.100 //Pop
	#define SMS 2      //SM Toggle Separation
	#define DL_X 0.550 //SM Tune
	#define DL_Z 0.000 //SM Local Smooth
	#define DL_W 0.000 //SM Perspective
	#define DM_X 10    //HQ Tune
	#define DM_Y 2     //HQ Boost
	#define DM_Z 2     //HQ Smooth
#elif (App == 0xC150B2EC ) //FarCry 6 
	#define DA_W 1
	#define DA_X 0.045
	#define DF_Y 0.050	
	#define DA_Y 16.0
    //#define DA_Z 0.0002
	#define DE_X 1
	#define DE_Y 0.500
	#define DE_Z 0.400
    #define DG_Z 0.077 //Min
    //#define DE_W 0.105 //Max
    #define DI_Z 0.175 //Trim
	#define BMT 1
	#define DF_Z 0.130
    #define DG_W 0.550 //Pop
	#define REF 4 //2 //Fix can go from 1 - 15 and 15 is low 1 is High
	#define DI_W 2.5 //Adjustment for REF    
	#define DK_X 2 //FPS Hold 
	#define DK_Y 0 //FPS Both Eyes
	#define DK_Z 2 //FPS 0.3%
	#define DK_W 2 //FPS Speed 100%
	#define PEW 1
	#define NDW 1
#elif (App == 0xF844D5C3 ) //Tony Hawk's Pro Skater 1+2** 
    #define DA_W 1
    #define DA_X 0.1125
    #define DF_Y 0.0125 
    #define DA_Y 150.0
    #define DE_X 1
    #define DE_Y 0.500
    #define DE_Z 0.400
    #define BMT 1    
    #define DF_Z 0.125 //Try to keep this in the lower end. The profile is really Good!!!!!
    #define FOV 1
#elif (App == 0x42BC6574 ) //Sleeping Doggs: Definitinve Edition**
    #define DA_X 0.075
    #define DF_Y 0.0025 
    #define DA_Y 7.25
    #define DE_X 1
    #define DE_Y 0.500
    #define DE_Z 0.400
    #define BMT 1    
    #define DF_Z 0.138 //This had to be adjusted
	#define LBC 2  //Letter Box Correction Offsets With X & Y
	#define DH_X 1.345
	#define PEW 1
#elif (App == 0x72632818 ) //Matrix Demo UE 5
    #define DA_W 1
    #define DA_X 0.025
    #define DF_Y 0.005 
    #define DA_Y 47.5
    #define BMT 1    
    #define DF_Z 0.030
	#define PEW 1
#elif (App == 0xCBE94135 ) //Anno: Mutationem
    #define DA_W 1
    #define DB_X 1
    #define DA_X 0.0375
    #define DF_Y 0.01375
    #define DA_Y 36.25
    //#define DA_Z -0.025
    #define DE_X 2
    #define DE_Y 0.250
    #define DE_Z 0.375
    #define BMT 1    
    #define DF_Z 0.100
	#define SMS 2      //SM Toggle Separation
	#define DL_X 0.600 //SM Tune
	#define DL_Z 0.500 //SM Local Smooth ???
	#define DL_W 0.050 //SM Perspective
	#define PEW 1
#elif (App == 0x98E46BDC ) //Forgive Me Father
    #define DA_W 1
    #define DA_X 0.1875
    #define DF_Y 0.0125
    #define DA_Y 250.0
    #define DA_Z -1.0
    #define DE_X 1
    #define DE_Y 0.750
    #define DE_Z 0.375
    #define BMT 1    
    #define DF_Z 0.125
	#define PEW 1
#elif (App == 0xAB93A702 ) //Nightmare Reaper
    #define DA_W 1
    #define DA_X 0.050
    #define DF_Y 0.020
    #define DA_Y 24.0
    //#define DA_Z 0.0
    #define DE_X 4
    #define DE_Y 0.500
    #define DE_Z 0.375
    #define BMT 1    
    #define DF_Z 0.125
    #define WSM 2
    #define DB_W 30
	#define DG_Z 0.0300//Min
//	#define DE_W 0.0 //Max
    #define DI_Z 0.0525 //Trim
	#define PEW 1
#elif (App == 0xBE557C30 ) //Golden Light
    #define DA_W 1
    #define DB_X 1
    #define DA_X 0.100
    #define DF_Y 0.0125
    #define DA_Y 187.5
    #define DB_Z 0.200
    //#define DA_Z 0.0
    #define DE_X 1
    #define DE_Y 0.500
    #define DE_Z 0.375
    #define BMT 1    
    #define DF_Z 0.125
    #define DG_W 0.125 //Pop
	#define PEW 1
#elif (App == 0x70858D6 ) //PowerSlave Exhumed
    #define DA_X 0.250
    #define DF_Y 0.0125
    #define DA_Y 300.0
    #define BMT 1    
    #define DF_Z 0.500
    #define WSM 2
    #define DB_W 31
#elif (App == 0xB7097475 ) //God Damn The Garden
    #define DA_W 1
    #define DB_X 1
    #define DA_X 0.045
    #define DF_Y 0.045
    #define DA_Y 45.0
    //#define DA_Z 0.0
    //#define DE_X 4
    //#define DE_Y 0.500
    //#define DE_Z 0.375
    #define BMT 1    
    #define DF_Z 0.1375
	#define DG_Z 0.110//Min
	//#define DE_W 0.0 //Max
    #define DI_Z 0.160 //Trim
	#define PEW 1
	#define DSW 1
#elif (App == 0x3E2F4D65 ) //Inscryption
    #define DA_W 1
    #define DB_X 1
    #define DA_X 0.045
    #define DF_Y 0.0425
    #define DA_Y 17.0
    //#define DA_Z 0.0
    //#define DE_X 4
    //#define DE_Y 0.500
    //#define DE_Z 0.375
    #define BMT 1    
    #define DF_Z 0.110
	//#define DG_Z 0.110//Min
	//#define DE_W 0.0 //Max
    //#define DI_Z 0.160 //Trim
	#define PEW 1
	#define DSW 1
#elif ( App == 0xDA05BA00 ) //Tiny Tina's wonderlands
	#define DA_Y 15.0
	#define DA_Z 0.0001375
	#define DA_X 0.040
	#define DF_Y 0.0425
	#define DB_Z 0.050
	#define DA_W 1
	#define DB_Y 4
	#define DE_X 5
	#define DE_Y 0.425
	#define DE_Z 0.300
	#define DG_W 0.210 //Pop out
	#define BMT 1    
	#define DF_Z 0.175
	#define DG_Z 0.0375//Min
	#define DI_Z 0.250 //Trim
	#define DB_W 5
	#define NDW 1
	#define DAA 1
	#define SMS 0 //SM Toggle Separation
	#define DL_X 0.600 //SM Tune
	#define DL_Z 0.500 //0.125 //SM Local Smooth
	#define DL_W 0.050 //SM Perspective	
#elif ( App == 0x4F4E231E ) //Hob
	#define DA_Y 20.0
	#define DA_X 0.040
	#define DF_Y 0.040
	#define DB_Y 4
	#define DE_X 1
	#define DE_Y 0.500
	#define DE_Z 0.375
	#define DG_W 0.125 //Pop out
	#define BMT 1    
	#define DF_Z 0.100
	#define DG_Z 0.050 //Min
	#define DI_Z 0.070 //Trim
	#define DAA 1
	#define SMS 0      //SM Toggle Separation
	#define DL_X 0.625 //SM Tune
	#define DL_W 0.050 //SM Perspective	
#elif ( App == 0x1C2203BC ) //TUNIC
    #define DA_W 1
    #define DB_X 1
	#define DA_Y 2.5
	#define DA_X 0.060
	#define DF_Y 0.160
	#define DE_X 1
	#define DE_Y 0.500
	#define DE_Z 0.375
	#define DG_W 0.125 //Pop out
	#define BMT 1    
	#define DF_Z 0.100
	#define DSW 1
	#define PEW 1
	#define SMS 0      //SM Toggle Separation
	#define DL_X 0.5625//SM Tune
	#define DL_W 0.000 //SM Perspective
#elif ( App == 0x42F66404 ) //Bayonetta
	#define DA_Y 39.0
	#define DA_X 0.037
	#define DF_Y 0.0125
	#define DE_X 1
	#define DE_Y 0.500
	#define DE_Z 0.375
	#define BMT 1    
	#define DF_Z 0.125
	#define PEW 1
	#define SMS 1      //SM Toggle Separation
	#define DL_X 0.725 //SM Tune
	#define DL_Z 0.500 //SM Local Smooth
	#define DL_W 0.050 //SM Perspective
#elif ( App == 0x8CF29E7A ) //Maneater
    #define DA_W 1
	#define DA_Y 75.0
	#define DA_X 0.05
	#define DF_Y 0.02
	#define DE_X 1
	#define DE_Y 0.500
	#define DE_Z 0.375
	#define BMT 1    
	#define DF_Z 0.500
	#define PEW 1
	#define SMS 1      //SM Toggle Separation
	#define DL_X 0.600 //SM Tune
	#define DL_Z 1.000 //SM Local Smooth
	#define DL_W 0.050 //SM Perspective
    #define LBC 1      //Letter Box Correction
	#define DH_Z 0.0   //Pos offset X    
	#define DH_W -0.236//Pos offset Y
#elif (App == 0xF14D1D3B ) //SniperElite 4 
    #define DA_X 0.05
    #define DF_Y 0.05
    #define DA_Y 20.0
    #define DE_X 1
    #define DE_Y 0.500
    #define DE_Z 0.375
    #define BMT 1    
    #define DF_Z 0.125
	#define DG_Z 0.0625//Min
	//#define DE_W 0.0 //Max
    #define DI_Z 0.0625 //Trim
	#define SMS 1      //SM Toggle Separation
	#define DL_X 0.625 //SM Tune
	#define DL_Z 0.300 //SM Local Smooth
	#define DL_W 0.100 //SM Perspective
	#define PEW 1
#elif (App == 0xF4901178 ) //The Surge 2
    #define DA_W 1 
    #define DA_X 0.125
    #define DF_Y 0.0225
    #define DA_Y 25.00
    #define DE_X 1
    #define DE_Y 0.375
    #define DE_Z 0.375
    #define BMT 1    
    #define DF_Z 0.125 //0.125 //0.150
	#define DG_Z 0.100 //Min
	//#define DE_W 0.0 //Max
    #define DI_Z 0.150 //Trim
	#define SMS 2      //SM Toggle Separation
	#define DL_X 0.600 //SM Tune
	#define DL_Z 0.400 //SM Local Smooth
	#define DL_W 0.100 //SM Perspective
	#define PEW 1
#elif (App == 0xBAC3D546 ) //Wolfenstine New Colossus II 
    #define DA_X 0.0404
    #define DF_Y 0.1125
    #define DA_Y 15.0
    //#define DA_Z -0.0005
    #define DE_X 2
    #define DE_Y 0.500
    #define DE_Z 0.375
	#define DG_W 0.325 //Pop out
    #define BMT 1    
    #define DF_Z 0.100
	#define DG_Z 0.070 //Min
	//#define DE_W 0.0 //Max
    #define DI_Z 0.225 //Trim
	#define SMS 2      //SM Toggle Separation
	#define DL_X 0.625 //SM Tune
	//#define DL_Z 0.250 //SM Local Smooth ???
	#define DL_W 0.05 //SM Perspective
	#define PEW 1
#elif (App == 0x132AB11B ) //Wolfenstine Youngblood 
    #define DA_X 0.040
    #define DF_Y 0.116
    #define DA_Y 15.5
    //#define DA_Z -0.0005
    #define DE_X 2
    #define DE_Y 0.500
    #define DE_Z 0.375
	//#fine DG_W 0.325 //Pop out
    #define BMT 1    
    #define DF_Z 0.116
	#define DG_Z 0.070 //Min
	//#define DE_W 0.0 //Max
    #define DI_Z 0.225 //Trim
	#define SMS 2      //SM Toggle Separation
	#define DL_X 0.75 //SM Tune
	#define DL_Z 1.0   //SM Local Smooth
	#define DL_W 0.050 //SM Perspective
    #define LBC 1      //Letter Box Correction
    #define LBS 1      //Letter Box Sensitivity
	#define DH_Z 0.0   //Pos offset X    
	#define DH_W -0.240//Pos offset Y
	#define PEW 1
	#define NDW 1
	#define DAA 1
#elif (App == 0xE44B25B ) //Injustice 2
    #define DA_X 0.100
    #define DF_Y 0.0125
    #define DA_Y 15.00
    #define DE_X 1
    #define DE_Y 0.500
    #define DE_Z 0.375
    #define BMT 1    
    #define DF_Z 0.125
	//#define DG_Z 0.100 //Min
	//#define DE_W 0.0 //Max
    //#define DI_Z 0.150 //Trim
	#define SMS 2      //SM Toggle Separation
	#define DL_X 0.750 //SM Tune
	#define DL_Z 0.500 //SM Local Smooth
	#define DL_W 0.050 //SM Perspective
	#define PEW 1
#elif (App == 0xE1D149FD ) //Max Payne 3 
	//#define DA_W 1
	#define DA_X 0.125
	#define DF_Y 0.050	
	#define DA_Y 25.0 //15
    //#define DA_Z 0.00025 //-1.0
	#define DE_X 1
	#define DE_Y 0.500
	#define DE_Z 0.375
    //#define DG_Z 0.002 //Min
    //#define DE_W 0.105 //Max
    //#define DI_Z 0.200 //Trim
	#define BMT 1
	#define DF_Z 0.125 //0.1875
    //#define DG_W 0.100 //Pop
	#define SMS 2      //SM Toggle Separation
	#define DL_X 0.800 //SM Tune
	#define DL_Z 0.000 //SM Local Smooth
	#define DL_W 0.050 //SM Perspective
	#define DM_X 5     //HQ Tune
	#define DM_Y 4     //HQ Boost
	#define DM_Z 3     //HQ Smooth
	#define PEW 1
	#define DAA 1
#else
	#define NPW 1 //No Profile
#endif

//Change Output
//#ifndef checks whether the given token has been #defined earlier in the file or in an included file
// X = [ZPD] Y = [Depth Adjust] Z = [Offset] W = [Depth Linearization]
#ifndef DA_X
    #define DA_X ZPD_D
#endif
#ifndef DA_Y
    #define DA_Y Depth_Adjust_D
#endif
#ifndef DA_Z
    #define DA_Z Offset_D
#endif
#ifndef DA_W
    #define DA_W Depth_Linearization_D
#endif

// X = [Depth Flip] Y = [Auto Balance] Z = [Auto Depth] W = [Weapon Hand]
#ifndef DB_X
    #define DB_X Depth_Flip_D
#endif
#ifndef DB_Y
    #define DB_Y Auto_Balance_D
#endif
#ifndef DB_Z
    #define DB_Z Auto_Depth_D
#endif
#ifndef DB_W
    #define DB_W Weapon_Hand_D
#endif

// X = [HUD] Y = [Barrel Distortion K1] Z = [Barrel Distortion K2] W = [Barrel Distortion Zoom]
#ifndef DC_X
    #define DC_X BD_K1_D
#endif
#ifndef DC_Y
    #define DC_Y BD_K2_D
#endif
#ifndef DC_Z
    #define DC_Z BD_K3_D
#endif
#ifndef DC_W
    #define DC_W BD_Zoom_D
#endif

// X = [Horizontal Size] Y = [Vertical Size] Z = [Horizontal Position] W = [Vertical Position]
#ifndef DD_X
    #define DD_X HVS_X_D
#endif
#ifndef DD_Y
    #define DD_Y HVS_Y_D
#endif
#ifndef DD_Z
    #define DD_Z HVP_X_D
#endif
#ifndef DD_W
    #define DD_W HVP_Y_D
#endif

// X = [ZPD Boundary Type] Y = [ZPD Boundary Scaling] Z = [ZPD Boundary Fade Time] W = [Weapon NearDepth Max]
#ifndef DE_X
    #define DE_X ZPD_Boundary_Type_D
#endif
#ifndef DE_Y
    #define DE_Y ZPD_Boundary_Scaling_D
#endif
#ifndef DE_Z
    #define DE_Z ZPD_Boundary_Fade_Time_D
#endif
#ifndef DE_W
    #define DE_W Weapon_Near_Depth_Max_D          //Max
#endif

// X = [ZPD Weapon Boundary] Y = [Separation] Z = [ZPD Balance] W = [HUD]
#ifndef DF_X
    #define DF_X ZPD_Weapon_Boundary_Adjust_D
#endif
#ifndef DF_Y
    #define DF_Y Separation_D
#endif
#ifndef DF_Z
    #define DF_Z Manual_ZPD_Balance_D
#endif
#ifndef DF_W
    #define DF_W HUDX_D
#endif

// X = [Special Depth Correction X] Y = [Special Depth Correction Y] Z = [Weapon NearDepth Min] W = [Check Depth Limit]
#ifndef DG_X
    #define DG_X SDC_Offset_X_D
#endif
#ifndef DG_Y
    #define DG_Y SDC_Offset_Y_D
#endif
#ifndef DG_Z
    #define DG_Z Weapon_Near_Depth_Min_D         //Min
#endif
#ifndef DG_W
	#define DG_W Check_Depth_Limit_D
#endif

// X = [LBD Size Correction Offset X] Y = [LBD Size Correction Offset Y] Z = [LBD Pos Correction Offset X] W = [LBD Pos Correction Offset Y]
#ifndef DH_X
    #define DH_X LB_Depth_Size_Offset_X_D
#endif
#ifndef DH_Y
    #define DH_Y LB_Depth_Size_Offset_Y_D
#endif
#ifndef DH_Z
    #define DH_Z LB_Depth_Pos_Offset_X_D
#endif
#ifndef DH_W
	#define DH_W LB_Depth_Pos_Offset_Y_D
#endif

// X = [LBM Offset X] Y = [LBM Offset Y] Z = [Weapon Near Depth Trim] W = [REF Check Depth Limit]
#ifndef DI_X
    #define DI_X LB_Masking_Offset_X_D
#endif
#ifndef DI_Y
    #define DI_Y LB_Masking_Offset_Y_D
#endif
#ifndef DI_Z
    #define DI_Z Weapon_Near_Depth_Trim_D       //Trim
#endif 
#ifndef DI_W
	#define DI_W REF_Check_Depth_Limit_D
#endif

// X = [NULL X] Y = [NULL Y] Z = [NULL Z] W = [Check Depth Limit Weapon]
#ifndef DJ_X
    #define DJ_X NULL_X_D
#endif
#ifndef DJ_Y
    #define DJ_Y NULL_Y_D
#endif
#ifndef DJ_Z
    #define DJ_Z NULL_Z_D
#endif 
#ifndef DJ_W
	#define DJ_W Check_Depth_Limit_Weapon_D
#endif

// X = [FPS Focus Method] Y = [Eye Eye Selection] Z = [Eye Fade Selection] W = [Eye Fade Speed Selection]
#ifndef DK_X
    #define DK_X FPS_Focus_Method_D
#endif
#ifndef DK_Y
    #define DK_Y EFO_Eye_Selection_D
#endif
#ifndef DK_Z
    #define DK_Z EFO_Fade_Selection_D
#endif 
#ifndef DK_W
	#define DK_W EFO_Fade_Speed_Selection_D
#endif
///////////////////////////////////////////////////////////////////////////////////////////////////////////
// X = [SM Tune] Y = [SM Weapon] Z = [SM Local Smoothing] W = [SM Perspective]
#ifndef DL_X
    #define DL_X SM_Tune_D
#endif
#ifndef DL_Y
    #define DL_Y SM_Weapon_D
#endif
#ifndef DL_Z
    #define DL_Z SM_Local_Smoothing_D
#endif 
#ifndef DL_W
	#define DL_W SM_Perspective_D
#endif

// X = [HQ Tune] Y = [HQ Boost] Z = [HQ Smooth] W = [NULL W]
#ifndef DM_X
    #define DM_X HQ_Tune_D
#endif
#ifndef DM_Y
    #define DM_Y HQ_Boost_D
#endif
#ifndef DM_Z
    #define DM_Z HQ_Smooth_D
#endif 
#ifndef DM_W
	#define DM_W NULL_W_D
#endif
////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Special Toggles
#ifndef REF
    #define REF Resident_Evil_Fix_D            //Resident Evil Fix
#endif
#ifndef IDF
    #define IDF Inverted_Depth_Fix_D           //Inverted Depth Fix
#endif
#ifndef SPF
    #define SPF Size_Position_Fix_D            //Size & Position Fix
#endif
#ifndef BDF
    #define BDF Barrel_Distortion_Fix_D        //Barrel Distortion Fix
#endif
#ifndef HMT
    #define HMT HUD_Mode_Trigger_D             //HUD Mode Trigger
#endif
#ifndef DFW
    #define DFW Delay_Frame_Workaround_D       //Delay Frame Workaround
#endif
#ifndef LBC
    #define LBC Auto_Letter_Box_Correction_D   //Auto Letter Box Correction
#endif
#ifndef LBS
    #define LBS LB_Sensitivity_D               //Letter Box Sensitivity
#endif
#ifndef LBM
    #define LBM Auto_Letter_Box_Masking_D      //Auto Letter Box Depth Masking
#endif
#ifndef SDT
    #define SDT Specialized_Depth_Trigger_D    //Specialized Depth Trigger
#endif
#ifndef BMT
    #define BMT Balance_Mode_Toggle_D          //Balance Mode Toggle
#endif
#ifndef SMS
    #define SMS SM_Toggle_Sparation_D          //Smooth Mode Toggle Sparation
#endif

#ifndef NPW
    #define NPW No_Profile_Warning_D           //No Profile Warning
#endif
#ifndef NFM
    #define NFM Needs_Fix_Mod_D                //Needs Fix and/or Modding
#endif
#ifndef DSW
    #define DSW Depth_Selection_Warning_D      //Depth Selection Warning
#endif
#ifndef DAA
    #define DAA Disable_Anti_Aliasing_D        //Disable Anti-Aliasing
#endif
#ifndef NDW
    #define NDW Network_Warning_D              //Network Detection Warning
#endif
#ifndef PEW
    #define PEW Disable_Post_Effects_Warning_D //Disable Post Effect Warning
#endif
#ifndef WPW
    #define WPW Weapon_Profile_Warning_D       //Weapon Profile Warning
#endif
#ifndef FOV
    #define FOV Set_Game_FoV_D                 //Set Game FoV
#endif

#ifndef RHW
    #define RHW Read_Help_Warning_D            //Read Help Warning
#endif
#ifndef EDW
    #define EDW Emulator_Detected_Warning_D    //Emulator Detected Warning
#endif
#ifndef NCW
    #define NCW Not_Compatible_Warning_D       //Not Compatible Warning
#endif
//Weapon Settings "Use #define WSM | 2 | 3 | 4 | 5 | One is default"
//Expanded Settings "Use #define WSM 6+ if Games have Multi Weapon Profiles."
#ifndef OW_WP     //This is used if OW_WP is not called in the Above Profile
    #define OW_WP "WP Off\0Custom WP\0WP 0\0WP 1\0WP 2\0WP 3\0WP 4\0WP 5\0WP 6\0WP 7\0WP 8\0WP 9\0WP 10\0WP 11\0WP 12\0WP 13\0WP 14\0WP 15\0WP 16\0WP 17\0WP 18\0WP 19\0WP 20\0WP 21\0WP 22\0WP 23\0WP 24\0WP 25\0WP 26\0WP 27\0WP 28\0WP 29\0WP 30\0WP 31\0WP 32\0WP 33\0WP 34\0WP 35\0WP 36\0WP 37\0WP 38\0WP 39\0WP 40\0WP 41\0WP 42\0WP 43\0WP 44\0WP 45\0WP 46\0WP 47\0WP 48\0WP 49\0WP 50\0"
#endif
#ifndef WSM //Profiles List One | Profiles List Two | Profiles List Three | Profiles List Four | Profiles List Five | Six is MCC | Seven is Prey | Eight is Blood 2 | Nine No One Lives Forever |
    #define WSM 1 //Weapon Setting Mode
#endif

#if WSM == 1
float4 Weapon_Profiles(float WP ,float4 Weapon_Adjust) //Tried Switch But, can't compile in some older versions of ReShade.
{   if (WP == 2)
        Weapon_Adjust = float4(0.425,5.0,1.125,0.0);      //WP 0  | ES: Oblivion
    if (WP == 3)
        Weapon_Adjust = float4(0.276,16.25,9.15,0.0);     //WP 1  | BorderLands
    if (WP == 4)
        Weapon_Adjust = float4(0.5,32.5,7.15,0.0);        //WP 2  | BorderLands 2
    if (WP == 5)
        Weapon_Adjust = float4(0.284,10.5,0.8725,0.0);    //WP 3  | BorderLands 3
    if (WP == 6)
        Weapon_Adjust = float4(0.253,39.0,97.5,0.0);      //WP 4  | Fallout 4
    if (WP == 7)
        Weapon_Adjust = float4(0.276,22.0,9.50,0.200);    //WP 5  | Skyrim: SE
    if (WP == 8)
        Weapon_Adjust = float4(0.338,21.0,9.1375,0.0);    //WP 6  | DOOM 2016
    if (WP == 9)
        Weapon_Adjust = float4(0.255,177.5,63.025,0.0);   //WP 7  | CoD:Black Ops | CoD:MW2 | CoD:MW3
    if (WP == 10)
        Weapon_Adjust = float4(0.254,100.0,0.9843,0.0);   //WP 8  | CoD:Black Ops II
    if (WP == 11)
        Weapon_Adjust = float4(0.254,203.125,0.98435,0.0);//WP 9  | CoD:Ghost
    if (WP == 12)
        Weapon_Adjust = float4(0.254,203.125,0.98433,0.0);//WP 10 | CoD:AW | CoD:MW Re
    if (WP == 13)
        Weapon_Adjust = float4(0.254,125.0,0.9843,0.0);   //WP 11 | CoD:IW
    if (WP == 14)
        Weapon_Adjust = float4(0.255,200.0,63.0,0.0);     //WP 12 | CoD:WaW
    if (WP == 15)
        Weapon_Adjust = float4(0.510,162.5,3.975,0.0);    //WP 13 | CoD | CoD:UO | CoD:2
    if (WP == 16)
        Weapon_Adjust = float4(0.254,23.75,0.98425,0.0);  //WP 14 | CoD: Black Ops IIII
    if (WP == 17)
        Weapon_Adjust = float4(0.375,60.0,15.15625,0.0);  //WP 15 | Quake DarkPlaces
    if (WP == 18)
        Weapon_Adjust = float4(0.7,14.375,2.5,0.0);       //WP 16 | Quake 2 XP
    if (WP == 19)
        Weapon_Adjust = float4(0.750,30.0,1.050,0.0);     //WP 17 | Quake 4
    if (WP == 20)
        Weapon_Adjust = float4(0.278,90.0,9.1,0.050);     //WP 18 | Half-Life 2
    if (WP == 21)
        Weapon_Adjust = float4(0.400,11.0,23.750,0.025);  //WP 19 | Metro Redux Games
    if (WP == 22)
        Weapon_Adjust = float4(0.350,12.5,2.0,0.0);       //WP 20 | Soldier of Fortune
    if (WP == 23)
        Weapon_Adjust = float4(0.286,1500.0,7.0,0.0);     //WP 21 | Deus Ex rev
    if (WP == 24)
        Weapon_Adjust = float4(35.0,250.0,0,0.0);         //WP 21 | Deus Ex
    if (WP == 25)
        Weapon_Adjust = float4(0.625,350.0,0.785,0.0);    //WP 23 | Minecraft
    if (WP == 26)
        Weapon_Adjust = float4(0.255,6.375,53.75,0.0);    //WP 24 | S.T.A.L.K.E.R: Games
    if (WP == 27)
        Weapon_Adjust = float4(0.400,5.5625,0.0,0.0);     //WP 25 | AMID EVIL RTX
    if (WP == 28)
        Weapon_Adjust = float4(0.750,30.0,1.025,0.0);     //WP 26 | Prey 2006
    if (WP == 29)
        Weapon_Adjust = float4(0.266,30.0,14.0,0.0);      //WP 27 | Wrath
    if (WP == 30)
        Weapon_Adjust = float4(3.625,20.0,0,0.0);         //WP 28 | We Where Here Together
    if (WP == 31)
        Weapon_Adjust = float4(0.7,9.0,2.3625,0.0);       //WP 29 | Return to Castle Wolfenstine
    if (WP == 32)
        Weapon_Adjust = float4(0.4894,62.50,0.98875,0.0); //WP 30 | Wolfenstein
    if (WP == 33)
        Weapon_Adjust = float4(1.0,93.75,0.81875,0.0);    //WP 31 | Wolfenstein: The New Order #C770832 / The Old Blood #3E42619F
    if (WP == 34)
        Weapon_Adjust = float4(1.150,55.0,0.9,0.0);       //WP 32 | Cyberpunk 2077
    if (WP == 35)
        Weapon_Adjust = float4(0.278,42.50,9.0,0.0);      //WP 33 | Black Mesa
    if (WP == 36)
        Weapon_Adjust = float4(0.277,105.0,8.8625,0.0);   //WP 34 | Portal 2
    if (WP == 37)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 35 | Game
    if (WP == 38)
        Weapon_Adjust = float4(0.78,20.0,0.180,0.0);      //WP 36 | SOMA
    if (WP == 39)
        Weapon_Adjust = float4(0.444,20.0,1.1875,0.0);    //WP 37 | Cryostasis
    if (WP == 40)
        Weapon_Adjust = float4(0.286,80.0,7.0,0.0);       //WP 38 | Unreal Gold with v227
    if (WP == 41)
        Weapon_Adjust = float4(0.280,18.75,9.03,0.0);     //WP 39 | Serious Sam Revolution #EB9EEB74/Serious Sam HD: The First Encounter /The Second Encounter /Serious Sam 2 #8238E9CA/ Serious Sam 3: BFE*
    if (WP == 42)
        Weapon_Adjust = float4(0.3,12.5,0.901,0.0);       //WP 40 | Serious Sam Fusion
    if (WP == 43)
        Weapon_Adjust = float4(1.2,12.5,0.3,0.05);        //WP 41 | GhostRunner DX12
    if (WP == 44)
        Weapon_Adjust = float4(0.277,20.0,8.8,0.0);       //WP 42 | TitanFall 2
    if (WP == 45)
        Weapon_Adjust = float4(1.300,17.50,0.0,0.0);      //WP 43 | Project Warlock
    if (WP == 46)
        Weapon_Adjust = float4(0.625,9.0,2.375,0.0);      //WP 44 | Kingpin Life of Crime
    if (WP == 47)
        Weapon_Adjust = float4(0.28,20.0,9.0,0.0);        //WP 45 | EuroTruckSim2
    if (WP == 48)
        Weapon_Adjust = float4(0.460,12.5,1.0,0.0);       //WP 46 | F.E.A.R #B302EC7 & F.E.A.R 2: Project Origin #91D9EBAF
    if (WP == 49)
        Weapon_Adjust = float4(1.5,37.5,0.99875,0.0);     //WP 47 | Condemned Criminal Origins
    if (WP == 50)
        Weapon_Adjust = float4(2.0,16.25,0.09,0.0);       //WP 48 | Immortal Redneck CP alt 1.9375
    if (WP == 51)
        Weapon_Adjust = float4(0.485,62.5,0.9625,0.0);    //WP 49 | Dementium 2
    if (WP == 52)
        Weapon_Adjust = float4(0.489,68.75,1.02,0.0);     //WP 50 | NecroVisioN & NecroVisioN: Lost Company #663E66FE
	//Do Not Add more Profiles
	//61 Profiles is Unity's Limit if using else if
	//76 Profiles reaches DX 9's Temp Registers Limit 
	//Will be cliping it off at 52 so 50 Profiles will be the limit so that I have more room to grow and faster compile time. 
		return Weapon_Adjust;
}
#elif WSM == 2
float4 Weapon_Profiles(float WP ,float4 Weapon_Adjust) //Could reduce from 76 to 57 to save on compiling time.
{   if (WP == 2)
        Weapon_Adjust = float4(0.600,6.5,0.0,0.0);        //WP 0  | The Suicide of Rachel Foster
    if (WP == 3)
        Weapon_Adjust = float4(1.653,17.5,0.0,0.0);       //WP 1  | Devolverland Expo
    if (WP == 4)
        Weapon_Adjust = float4(1.489,16.875,0.0,0.0);     //WP 2  | Conarium
    if (WP == 5)
        Weapon_Adjust = float4(0.270,25.0,0.951,0.0);     //WP 3  | WRC 10
    if (WP == 6)
        Weapon_Adjust = float4(0.850,42.5,0.9990125,0.0); //WP 4  | The Outer Worlds
    if (WP == 7)
        Weapon_Adjust = float4(0.275,11.0,10.0,0.0);      //WP 5  | Crysis 2 DX11 1.9
    if (WP == 8)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 6  | Game
    if (WP == 9)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 7  | Game
    if (WP == 10)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 8  | Game
    if (WP == 11)
        Weapon_Adjust = float4(4.100,25.0,0.0,0.0);       //WP 9  | HROT
    if (WP == 12)
        Weapon_Adjust = float4(0.284,25.0,0.8745,0.0);    //WP 10 | Crysis Remastered
    if (WP == 13)
        Weapon_Adjust = float4(0.284,15.0,7.200,0.0);     //WP 11 | Crysis 2 Remastered
    if (WP == 14)
        Weapon_Adjust = float4(0.284,12.5,10.4375,0.125); //WP 12 | Crysis 3 Remastered
    if (WP == 15)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 13 | Game
    if (WP == 16)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 14 | Game
    if (WP == 17)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 15 | Game
    if (WP == 18)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 16 | Game
    if (WP == 19)
        Weapon_Adjust = float4(1.550,100.0,0.130,0.130);  //WP 17 | Halo Infinite //float4(1.550,117.5,0.125,0.125);
    if (WP == 20)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 18 | Game
    if (WP == 21)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 19 | Game
    if (WP == 22)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 20 | Game
    if (WP == 23)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 21 | Game
    if (WP == 24)
        Weapon_Adjust = float4(15.500,60.0,0.0,0.075);    //WP 22 | Bright Memory: infinite
    if (WP == 25)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 23 | Game
    if (WP == 26)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 24 | Game
    if (WP == 27)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 25 | Game
    if (WP == 28)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 26 | Game
    if (WP == 29)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 27 | Game
    if (WP == 30)
        Weapon_Adjust = float4(0.725,7.75,0.0,0.0);       //WP 28 | Nightmare Reaper
    if (WP == 31)
        Weapon_Adjust = float4(0.5,7.0,0.0,0.0);          //WP 29 | Powerslave Exhumed
    if (WP == 32)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 30 | Game
    if (WP == 33)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 31 | Game
    if (WP == 34)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 32 | Game
    if (WP == 35)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 33 | Game
    if (WP == 36)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 34 | Game
    if (WP == 37)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 35 | Game
    if (WP == 38)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 36 | Game
    if (WP == 39)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 37 | Game
    if (WP == 40)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 38 | Game
    if (WP == 41)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 39 | Game
    if (WP == 42)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 40 | Game
    if (WP == 43)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 41 | Game
    if (WP == 44)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 42 | Game
    if (WP == 45)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 43 | Game
    if (WP == 46)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 44 | Game
    if (WP == 47)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 45 | Game
    if (WP == 48)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 46 | Game
    if (WP == 49)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 47 | Game
    if (WP == 50)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 48 | Game
    if (WP == 51)
        Weapon_Adjust = float4(1.960,5.25,0,0.0);         //WP 60 | Dying Light
    if (WP == 52)
        Weapon_Adjust = float4(2.196,1.750,0.0,0.0);      //WP 61 | Dying Light 2
	//Do Not Add more Profiles
	//61 Profiles is Unity's Limit if using else if
	//76 Profiles reaches DX 9's Temp Registers Limit 
	//Will be cliping it off at 52 so 50 Profiles will be the limit so that I have more room to grow and faster compile time. 
		return Weapon_Adjust;
}
#elif WSM == 3
float4 Weapon_Profiles(float WP ,float4 Weapon_Adjust)
{   if (WP == 2)
        Weapon_Adjust = float4(1.0,237.5,0.83625,0.0);    //WP 0 | Rage64
    if (WP == 3)
        Weapon_Adjust = float4(13.870,50.0,0.0,0.0);      //WP 1 | Graven
    if (WP == 4)
        Weapon_Adjust = float4(0.425,15.0,99.0,0.0);      //WP 2 | Bioshock Remastred
    if (WP == 5)
        Weapon_Adjust = float4(0.425,21.25,99.5,0.0);     //WP 3 | Bioshock 2 Remastred
    if (WP == 6)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 4 | Game
    if (WP == 7)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 5 | Game
    if (WP == 8)
        Weapon_Adjust = float4(0.5,8.0,0,0.0);            //WP 6 | Strife
    if (WP == 9)
        Weapon_Adjust = float4(0.350,11.50,2.0,0.0);      //WP 7 | Gold Source
    if (WP == 10) 
        Weapon_Adjust = float4(1.825,13.75,0,0.0);        //WP 8 | No Man Sky FPS Mode
    if (WP == 11)
        Weapon_Adjust = float4(0.0,0.0,0,0.0);            //WP 9 | Game
    if (WP == 12)
        Weapon_Adjust = float4(0.287,180.0,9.0,0.0);      //WP 10 | Farcry
    if (WP == 13)
        Weapon_Adjust = float4(0.2503,55.0,1000.0,0.0);   //WP 11 | Farcry 2
    if (WP == 14)
        Weapon_Adjust = float4(0.279,100.0,0.905,0.0);    //WP 12 | Talos Principle
    if (WP == 15)
        Weapon_Adjust = float4(0.2503,52.5,987.5,0.0);    //WP 13 | Singularity
    if (WP == 16)
        Weapon_Adjust = float4(0.251,12.5,925.0,0.0);     //WP 14 | Betrayer
    if (WP == 17)
        Weapon_Adjust = float4(1.035,16.0,0.185,0.0);     //WP 15 | Doom Eternal
    if (WP == 18)
        Weapon_Adjust = float4(1.553,16.875,0.0,0.0);     //WP 16 | Q.U.B.E 2
    if (WP == 19)
        Weapon_Adjust = float4(0.251,5.6875,950.0,0.0);   //WP 17 | Mirror Edge
    if (WP == 20)
        Weapon_Adjust = float4(0.345,10.125,1.825,0.0);   //WP 18 | Quake Enhanced Edition
    if (WP == 21)
        Weapon_Adjust = float4(0.430,6.250,0.100,0.0);    //WP 19 | The Citadel 186
    if (WP == 22)
        Weapon_Adjust = float4(0.800,15.0,0.3,0.0);       //WP 20 | Sauerbraten 2
    if (WP == 23)
        Weapon_Adjust = float4(13.3,62.5,0.0,0.0);        //WP 21 | Chex Quest HD
    if (WP == 24)
        Weapon_Adjust = float4(0.75,112.5,0.5,0.0);       //WP 22 | Hexen 2
    if (WP == 25)
        Weapon_Adjust = float4(0.350,17.5,2.050,0.0);     //WP 23 | Star Trek EliteForce II
    if (WP == 26)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 24 | Game
    if (WP == 27)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 25 | Game
    if (WP == 28)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 26 | Game
    if (WP == 29)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 27 | Game
    if (WP == 30)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 28 | Game
    if (WP == 31)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 29 | Game
    if (WP == 32)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 30 | Game
    if (WP == 33)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 31 | Game
    if (WP == 34)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 32 | Game
    if (WP == 35)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 33 | Game
    if (WP == 36)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 34 | Game
    if (WP == 37)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 35 | Game
    if (WP == 38)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 36 | Game
    if (WP == 39)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 37 | Game
    if (WP == 40)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 38 | Game
    if (WP == 41)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 39 | Game
    if (WP == 42)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 40 | Game
    if (WP == 43)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 41 | Game
    if (WP == 44)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 42 | Game
    if (WP == 45)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 43 | Game
    if (WP == 46)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 44 | Game
    if (WP == 47)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 45 | Game
    if (WP == 48)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 46 | Game
    if (WP == 49)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 47 | Game
    if (WP == 50)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 48 | Game
    if (WP == 51)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 49 | Game
    if (WP == 52)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 50 | Game
	//Do Not Add more Profiles
	//61 Profiles is Unity's Limit if using else if
	//76 Profiles reaches DX 9's Temp Registers Limit 
	//Will be cliping it off at 52 so 50 Profiles will be the limit so that I have more room to grow and faster compile time. 
		return Weapon_Adjust;
}
#elif WSM == 4
float4 Weapon_Profiles(float WP ,float4 Weapon_Adjust)
{   if (WP == 2)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 0  | Game
    if (WP == 3)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 1  | Game
    if (WP == 4)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 2  | Game
    if (WP == 5)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 3  | Game
    if (WP == 6)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 4  | Game
    if (WP == 7)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 5  | Game
    if (WP == 8)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 6  | Game
    if (WP == 9)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 7  | Game
    if (WP == 10)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 8  | Game
    if (WP == 11)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 9  | Game
    if (WP == 12)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 10 | Game
    if (WP == 13)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 11 | Game
    if (WP == 14)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 12 | Game
    if (WP == 15)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 13 | Game
    if (WP == 16)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 14 | Game
    if (WP == 17)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 15 | Game
    if (WP == 18)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 16 | Game
    if (WP == 19)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 17 | Game
    if (WP == 20)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 18 | Game
    if (WP == 21)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 19 | Game
    if (WP == 22)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 20 | Game
    if (WP == 23)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 21 | Game
    if (WP == 24)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 22 | Game
    if (WP == 25)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 23 | Game
    if (WP == 26)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 24 | Game
    if (WP == 27)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 25 | Game
    if (WP == 28)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 26 | Game
    if (WP == 29)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 27 | Game
    if (WP == 30)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 28 | Game
    if (WP == 31)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 29 | Game
    if (WP == 32)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 30 | Game
    if (WP == 33)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 31 | Game
    if (WP == 34)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 32 | Game
    if (WP == 35)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 33 | Game
    if (WP == 36)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 34 | Game
    if (WP == 37)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 35 | Game
    if (WP == 38)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 36 | Game
    if (WP == 39)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 37 | Game
    if (WP == 40)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 38 | Game
    if (WP == 41)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 39 | Game
    if (WP == 42)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 40 | Game
    if (WP == 43)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 41 | Game
    if (WP == 44)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 42 | Game
    if (WP == 45)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 43 | Game
    if (WP == 46)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 44 | Game
    if (WP == 47)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 45 | Game
    if (WP == 48)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 46 | Game
    if (WP == 49)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 47 | Game
    if (WP == 50)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 48 | Game
    if (WP == 51)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 60 | Game
    if (WP == 52)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 61 | Game
	//Do Not Add more Profiles
	//61 Profiles is Unity's Limit if using else if
	//76 Profiles reaches DX 9's Temp Registers Limit 
	//Will be cliping it off at 52 so 50 Profiles will be the limit so that I have more room to grow and faster compile time. 
		return Weapon_Adjust;
}
#elif WSM == 5
float4 Weapon_Profiles(float WP ,float4 Weapon_Adjust)
{   if (WP == 2)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 0  | Game
    if (WP == 3)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 1  | Game
    if (WP == 4)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 2  | Game
    if (WP == 5)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 3  | Game
    if (WP == 6)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 4  | Game
    if (WP == 7)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 5  | Game
    if (WP == 8)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 6  | Game
    if (WP == 9)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 7  | Game
    if (WP == 10)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 8  | Game
    if (WP == 11)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 9  | Game
    if (WP == 12)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 10 | Game
    if (WP == 13)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 11 | Game
    if (WP == 14)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 12 | Game
    if (WP == 15)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 13 | Game
    if (WP == 16)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 14 | Game
    if (WP == 17)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 15 | Game
    if (WP == 18)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 16 | Game
    if (WP == 19)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 17 | Game
    if (WP == 20)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 18 | Game
    if (WP == 21)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 19 | Game
    if (WP == 22)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 20 | Game
    if (WP == 23)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 21 | Game
    if (WP == 24)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 22 | Game
    if (WP == 25)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 23 | Game
    if (WP == 26)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 24 | Game
    if (WP == 27)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 25 | Game
    if (WP == 28)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 26 | Game
    if (WP == 29)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 27 | Game
    if (WP == 30)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 28 | Game
    if (WP == 31)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 29 | Game
    if (WP == 32)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 30 | Game
    if (WP == 33)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 31 | Game
    if (WP == 34)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 32 | Game
    if (WP == 35)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 33 | Game
    if (WP == 36)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 34 | Game
    if (WP == 37)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 35 | Game
    if (WP == 38)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 36 | Game
    if (WP == 39)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 37 | Game
    if (WP == 40)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 38 | Game
    if (WP == 41)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 39 | Game
    if (WP == 42)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 40 | Game
    if (WP == 43)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 41 | Game
    if (WP == 44)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 42 | Game
    if (WP == 45)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 43 | Game
    if (WP == 46)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 44 | Game
    if (WP == 47)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 45 | Game
    if (WP == 48)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 46 | Game
    if (WP == 49)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 47 | Game
    if (WP == 50)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 48 | Game
    if (WP == 51)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 60 | Game
    if (WP == 52)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 61 | Game
	//Do Not Add more Profiles
	//61 Profiles is Unity's Limit if using else if
	//76 Profiles reaches DX 9's Temp Registers Limit 
	//Will be cliping it off at 52 so 50 Profiles will be the limit so that I have more room to grow and faster compile time. 
		return Weapon_Adjust;
}
#elif WSM == 6
float3 Weapon_Profiles(float WP ,float3 Weapon_Adjust) // MCC
{
	if (WP == 2)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 0  | Halo: Reach
    if (WP == 3)
        Weapon_Adjust = float4(1.5,26.25,0.2,0.0);        //WP 1  | Halo: CE Anniversary
    if (WP == 4)
        Weapon_Adjust = float4(0.615,70.0,0.3955,0.0);    //WP 2  | Halo 2: Anniversary
    if (WP == 5)
        Weapon_Adjust = float4(5.750,24.0,0,0.0);         //WP 3  | Halo 3
    if (WP == 6)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 4  | Halo 3: ODST
    if (WP == 7)
        Weapon_Adjust = float4(0.0,0.0,0.0,0.0);          //WP 5  | Halo 4

		return Weapon_Adjust;
}
#elif WSM == 7
float3 Weapon_Profiles(float WP ,float3 Weapon_Adjust) // Prey 2017
{
	if (WP == 2)
		Weapon_Adjust = float4(0.2832,31.25,0.8775,0.0);   //WP 0 | Prey 2017 High Settings and <
	if (WP == 3)
		Weapon_Adjust = float4(0.2832,31.25,0.91875,0.0);  //WP 1 | Prey 2017 Very High

	return Weapon_Adjust;
}
#elif WSM == 8
float3 Weapon_Profiles(float WP ,float3 Weapon_Adjust) // Blood 2
{
    if (WP == 2)
        Weapon_Adjust = float4(0.4213,5.0,0.5,0.0);        //WP 0 | Blood 2 All Weapons
    if (WP == 3)
        Weapon_Adjust = float4(0.484,5.0,0.5,0.0);         //WP 1 | Blood 2 Bonus weapons
    if (WP == 4)
        Weapon_Adjust = float4(0.4213,5.0,0.8,0.0);        //WP 2 | Blood 2 Former

	return Weapon_Adjust;
}
#elif WSM == 9
float3 Weapon_Profiles(float WP ,float3 Weapon_Adjust) // No One Lives Forever
{
    if (WP == 2)
        Weapon_Adjust = float4(0.425,5.25,1.0,0.0);       //WP 4 | No One Lives Forever
    if (WP == 3)
        Weapon_Adjust = float4(0.519,31.25,8.875,0.0);    //WP 5 | No One Lives Forever 2

	return Weapon_Adjust;
}
//Can expand here for games with multi weapon profiles.
#endif
