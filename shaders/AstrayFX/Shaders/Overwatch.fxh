////----------------------------------------//
///SuperDepth3D Overwatch Automation Shader///
//----------------------------------------////
// Version 2.1.7
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

//Weapon Setting are at the bottom of this file.

//SuperDepth3D Defaults
static const float ZPD_D = 0.025;                       //ZPD
static const float Depth_Adjust_D = 7.5;                //Depth Adjust
static const float Offset_D = 0.0;                      //Offset
static const int Depth_Linearization_D = 0;             //Linearization
static const int Depth_Flip_D = 0;                      //Depth Flip
static const int Auto_Balance_D = 0;                    //Auto Balance
static const float Auto_Depth_D = 0.1;                  //Auto Depth Range
static const int Weapon_Hand_D = 0;                     //Weapon Profile
static const float BD_K1_D = 0.0;                       //Barrel Distortion K1
static const float BD_K2_D = 0.0;                       //Barrel Distortion K2
static const float BD_K3_D = 0.0;                       //Barrel Distortion K3
static const float BD_Zoom_D = 0.0;                     //Barrel Distortion Zoom
static const float HVS_X_D = 1.0;                       //Horizontal Size
static const float HVS_Y_D = 1.0;                       //Vertical Size
static const float HVP_X_D = 0;                         //Horizontal Position
static const float HVP_Y_D = 0;                         //Vertical Position
static const int ZPD_Boundary_Type_D = 0;               //ZPD Boundary Type
static const float ZPD_Boundary_Scaling_D = 0.5;        //ZPD Boundary Scaling
static const float ZPD_Boundary_Fade_Time_D = 0.25;     //ZPD Boundary Fade Time
static const float Weapon_Near_Depth_Max_D = 0.0;       //Weapon Near Depth              Max
static const float ZPD_Weapon_Boundary_Adjust = 0.0;    //ZPD Weapon Boundary Adjust
static const float Separation = 0.0;                    //ZPD Separation
static const float Null_Z = 0.0;                        //
static const float HUDX_D = 0.0;                        //Heads Up Display Cut Off Point
static const float Manual_ZPD_Balance = 0.5;            //Manual Balance Mode Adjustment
static const float Null_Y = 0.0;                        //
static const float Weapon_Near_Depth_Min_D = 0.0;       //Weapon Near Depth              Min
static const float Check_Depth_Limit = 0.0;             //Check Depth Limit

//Special Toggles Defaults
static const int REF = 0;                               //Resident Evil Fix
static const int HMT = 0;                               //HUD Mode Trigger
static const int IDF = 0;                               //Inverted Depth Fix
static const int SPF = 0;                               //Size & Position Fix
static const int BDF = 0;                               //Barrel Distortion Fix
static const int DFW = 0;                               //Delay Frame Workaround
static const int ALB = 0;                               //Auto Letter Box
static const int LBD = 0;                               //Letter Box Depth
static const int STD = 0;                               //Specialized Depth Trigger
static const int BMT = 0;                               //Balance Mode Toggle
//Special Toggles Generic
static const int RHW = 0;                               //Read Help Warning
static const int EDW = 0;                               //Emulator Detected Warning

//Special Toggles Warnings
static const int NCW = 0;                               //Not Compatible Warning
static const int NPW = 0;                               //No Profile Warning
static const int NFM = 0;                               //Needs Fix/Mod
static const int DSW = 0;                               //Depth Selection Warning
static const int DAA = 0;                               //Disable Anti-Aliasing
static const int NWW = 0;                               //Network Warning
static const int PEW = 0;                               //Disable Post Effect Warning
static const int WPW = 0;                               //Weapon Profile Warning
static const int FOV = 0;                               //Set Game FoV

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
	#define RH 1
	#define SP 2
	#define HM 1
	#define DS 1
	#define ED 1
#elif (App == 0xC753DADB )	//ES: Oblivion
	#define DB_W 2
	#define DB_Y 3
#elif (App == 0x7B81CCAB || App == 0xFB9A99AB )	//BorderLands 2 & Pre-Sequel
	#define DA_Y 25.0
	#define DA_Z 0.00025
	#define DA_X 0.03750
	#define DB_Y 2
	#define DB_W 4
	#define DE_X 4
	#define DE_Y 0.625
	#define DE_Z 0.300
	#define DF_X 0.300
	#define NW 1
#elif (App == 0x2D950D30 )	//Fallout 4
	#define DA_X 0.05
	//#define DA_Y 7.8
	#define DB_Y 3
	#define DB_W 6
	#define DF_Y 0.012
	#define DE_X 3
	#define DE_Y 0.750
	#define DE_Z 0.375
	#define FV 1
	#define RH 1
	#define DS 1
#elif (App == 0x3950D04E )	//Skyrim: SE
	#define DA_Y 6.25
	#define DB_Y 2
	#define DB_W 7
#elif (App == 0x142EDFD6 || App == 0x2A0ECCC9 || App == 0x8B0C2031 )	//DOOM 2016
	#define DA_Y 23.125
	#define DA_Z -0.00010
	#define DA_X 0.071
	#define DB_Z 0.1125
	#define DF_Y 0.01875
	#define DB_Y 1 //Auto Mode Works But this game is better locked.
	#define DE_X 3
	#define DE_Y 0.500
	#define DE_Z 0.4375
	#define DB_W 8
    #define DG_Z 0.001
	#define BM 1
	#define DG_X 0.145
	#define PE 1
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
	#define RH 1
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
	#define DA_Y 12.5
	#define DB_Y 2
	#define DB_W 21
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
	#define DE_X 3
	#define DE_Y 0.5
	#define DE_Z 0.300
    #define DF_Y 0.05
	#define WSM 4
	#define OW_WP "Read Help & Change Me\0Custom WP\0Prey High Settings and <\0Prey 2017 Very High\0"
	#define RH 1
	#define PE 1
	#define WW 1
#elif (App == 0xBF757E3A )	//Return to Castle Wolfenstein
	#define DA_Y 8.75
	#define DB_Y 2
	#define DB_W 31
#elif (App == 0xC770832 || App == 0x3E42619F )	//Wolfenstein: The New Order | The Old Blood
	#define DA_Y 25.0
	#define DB_Y 5
	#define DA_Z 0.00125
	#define DB_W 33
#elif (App == 0x6FC1FF71 ) //Black Mesa
	#define DA_Y 8.75
	#define DA_Z 0.000125
	#define DA_X 0.0325
	#define DB_Y 2
	#define DB_W 35
	#define DB_Z 0.08625
#elif (App == 0x6D3CD99E ) //Blood 2
	#define DA_X 0.105
	#define DB_Y 2
	#define DE_X 3
	//#define DE_Y 0.50
	#define DE_Z 0.475
	#define WSM 5
	#define DB_W 2
	#define OW_WP "Read Help & Change Me\0Custom WP\0Blood 2 All Weapons\0Blood 2 Bonus Weapons\0Blood 2 Former\0"
	#define WW 1
	#define NF 1
	#define RH 1
#elif (App == 0xF22A9C7D || App == 0x5416A79D ) //SOMA
	#define DA_Y 23.125 //21.25 //25.0
	#define DA_X 0.1025 //0.110 //0.095
	#define DB_Y 5
	#define BM 1
	#define DG_X 0.15625
	#define DA_Z -0.00025
	#define DG_W 0.1
	#define DB_W 38
	#define DG_Z 0.341
	#define DE_X 3
	#define DE_Y 0.5
	#define DE_Z 0.375
	#define DF_X 0.25
	#define FV 1
	#define RH 1
#elif (App == 0x6FB6410B ) //Cryostasis
	#define DA_Y 13.75
	#define DB_Y 3
	#define DB_W 39
#elif (App == 0x16B8D61A) //Unreal Gold with v227
	#define DA_Y 17.5
	#define DB_Y 1
	#define DB_W 40
	#define DF_W 0.534
	#define HM 1
#elif (App == 0xEB9EEB74 || App == 0x8238E9CA ) //Serious Sam Revolution | Serious Sam 2
	#define DA_X 0.075
	#define DA_Y 10.0
	#define DB_Y 1
	#define DE_X 4
	#define DE_Y 0.85
	#define DE_Z 0.375
	#define DB_Z 0.150
	#define DA_Z 0.1111
	#define DF_X 0.1125
	#define DB_W 41
	#define DF_W 0.5
	#define HM 1
#elif (App == 0x308AEBEA ) //TitanFall 2
	#define DB_Y 4
	#define DB_W 44
#elif (App == 0x5FCFB1E5 ) //Project Warlock
	#define DA_Y 17.5
	#define DB_Y 2
	#define DA_W 1
	#define DB_W 45
#elif (App == 0x7DCCBBBD ) //Kingpin Life of Crime
	#define DA_Y 10.0
	#define DB_Y 4
	#define DB_W 46
	#define RH 1
#elif (App == 0x9C5C946E ) //EuroTruckSim2
	#define DB_W 47
#elif (App == 0xB302EC7 || App == 0x91D9EBAF ) //F.E.A.R | F.E.A.R 2: Project Origin
	#define DA_X 0.110
	#define DA_Y 12.0
	#define DA_Z 0.00025
	#define DB_Y 5
	#define DE_X 3
	//#define DE_Y 0.625
	#define DE_Z 0.375
	//#define DG_W 0.25
	#define DB_W 48
	//#define DF_X 0.225
	#define DS 1 //?
	#define FV 1
	#define RH 1
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
	#define DB_W 53
#elif (App == 0x44BD41E1 ) //Bioshock Remaster
	#define DA_Z 0.001
	#define DB_Y 3
	#define DB_W 55
#elif (App == 0x7CF5A01 ) //Bioshock 2 Remaster
	#define DA_Z 0.001
	#define DB_W 56
	#define DB_Y 3
	#define DF_W 0.5034
	#define HM 1
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
	#define HM 1
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
	#define RH 1
#elif (App == 0x9E7AA0C4 ) //Shadow Tactics: Blades of the Shogun
	#define DA_Y 7.0
	#define DA_Z 0.001
	#define DA_X 0.150
	#define DB_Y 5
	#define DB_Z 0.305
	#define DB_X 1
	#define RH 1
#elif (App == 0xE63BF4A4 ) //World of Warcraft DX12
	#define DA_Y 7.5
	#define DA_W 1
	#define DB_Y 3
	#define DB_Z 0.1375
	#define NW 1
	#define RH 1
#elif (App == 0x5961D1CC ) //Requiem: Avenging Angel
	#define DA_Y 37.5
	#define DA_X 0.0375
	#define DA_Z 0.8
	#define DF_W 0.501
	#define HM 1
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
	#define DB_Y 3
	#define DB_Z 0.015
	#define DA_Y 51.25
	#define DA_W 1
	#define DA_Z 0.00015
	#define RE 1
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
	#define RE 1
#elif (App == 0xAAA18268 ) //Hellblade
	#define DB_Y 1
	#define DA_Y 25.0
	#define DA_W 1
	#define DA_Z 0.0005
	#define DB_Z 0.25 //Under Review
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
	#define RH 1
#elif (App == 0xA100000 ) //Lego Batman 1 & 2
	#define DA_Y 27.5
	#define DA_X 0.125
	#define DA_Z 0.001
	#define DB_Y 2
	#define DB_Z 0.025
	#define RE 1
#elif (App == 0x5F2CA572 ) //Lego Batman 3
	#define DA_X 0.03
	#define DA_Z 0.001
	#define DB_Y 4
	#define RH 1
#elif (App == 0xA200000 ) //Batman BlackGate
	#define DA_Y 12.5
	#define DA_X 0.0375
	#define DA_Z 0.00025
	#define DB_Y 3
#elif (App == 0xCB1CCDC ) //BATMAN TTS
	#define NC 1 //Not Compatible
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
	#define RE 1
	#define RH 1
#elif (App == 0x1335BAB8 ) //BattleField 1
	#define DA_W 1
	#define DA_Y 8.125
	#define DA_X 0.04
	#define DB_Y 5
	#define RE 2
#elif (App == 0xA0762A98 ) //Assassin's Creed Unity
	#define DA_W 1
	#define DA_Y 25.0
	#define DA_Z 0.00025
	#define DA_X 0.04375
	#define DB_Z 0.2
#elif (App == 0xC990B77C ) //Assassin's Origins
	#define DA_W 1
	#define DA_Y 50.0
	#define DA_X 0.0475
	#define DB_Y 1
	#define DE_X 2
	#define DE_Y 0.4
	#define DE_Z 0.375
	#define DB_Z 0.1
#elif (App == 0xBF222C03 ) //Among The Sleep
	#define DA_X 0.05
	#define DA_Y 15.0
	#define DA_Z 0.0005
	#define DB_Y 4
	#define DB_X 1
	#define ID 1
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
#elif (App == 0x8B0F15E7 ) //Alan Wake
	#define DA_X 0.03
	#define DA_Y 32.5
	#define DB_Y 1
	#define RH 1
#elif (App == 0xCFE885A2 ) //Alan Wake's American Nightmare
	#define DA_X 0.03
	#define DA_Y 32.5
	#define DB_Y 1
	#define RH 1
#elif (App == 0x56D8243B ) //Agony Unrated
	#define DA_W 1
	#define DA_X 0.04375
	#define DA_Y 43.75
	#define DB_Y 5
	#define RH 1
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
	#define DC 1
	#define DC_X 0.22
	#define DC_Y -0.1
	#define DC_W -0.022
	#define RH 1
	#define PE 1
#elif (App == 0x5839915F ) //35MM
	#define DA_Y 35.00
	#define DB_X 1
	#define DB_Y 2
	#define RH 1
#elif (App == 0x578862 ) //Condemned Criminal Origins
	#define DA_Y 162.5
	#define DA_Z 0.00025
	#define DA_X 0.040
	#define DB_Y 4
	#define DB_W 49
	#define RH 1
#elif (App == 0xA67FA4BC ) //Outlast
	#define DA_Y 30.0
	#define DA_Z 0.0004
	#define DA_X 0.043750
	#define DB_Y 5
	#define RH 1
#elif (App == 0xDCC7F877 ) //Outlast II
	#define DA_W 1
	#define DA_Y 50.0
	#define DA_Z 0.0004
	#define DA_X 0.056250
	#define DB_Y 4
	#define RH 1
#elif (App == 0x60F43F45 ) //Resident Evil 7
	#define DA_W 1
	#define DA_Y 30.0
	#define DA_Z 0.0002
	#define DA_X 0.0625
	#define DB_Y 3
	#define DE_X 1
	#define DE_Y 0.5
	#define DE_Z 0.375
	#define DC 1
	#define DC_X 0.24
	#define DC_Y 0.1
	#define DC_Z -0.024
	#define DC_W -0.05
	#define RH 1
#elif (App == 0x1B8B9F54 ) //TheEvilWithin
	#define DA_Y 40.0
	#define DA_Z 0.0001
	#define DA_X 0.1
	#define DB_Y 4
#elif (App == 0x7D9B7A37 ) //TheEvilWithin II
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
	#define RH 1
#elif (App == 0x706C8618 ) //Layer of Fear
	#define DB_X 1
	#define DA_Y 17.50
	#define DA_X 0.035
	#define DB_Y 5
	#define RH 1
#elif (App == 0x2F0BD376 ) //Minecraft / BuildGDX
	#define DA_Y 22.5
	#define DA_X 0.0625
	#define DB_W 25
	#define DB_Y 3
	#define DE_X 4
	#define DE_Y 0.500
	#define DE_Z 0.375
	#define DF_Y 0.005
	#define RH 1
#elif (App == 0x84D341E3 || App == 0x15A08799) //Little Nightmares & Little Nightmares II
	#define DA_W 1
	#define DA_Y 33.75
	#define DA_X 0.25
	#define DF_Y 0.125
	#define DA_Z 0.0015
	#define DB_Y 5 		//ZPD Boundary Scaling
	#define DB_Z 0.325	//Auto Depth Adjust
	#define PE 1
#elif (App == 0xC0AC5174 ) //Observer
	#define DA_W 1
	#define DA_Y 20.0
	#define DA_X 0.05
	#define DF_Y 0.01
	#define DA_Z 0.0005
	#define DB_Y 5 //Fall Back
	#define DE_X 2
	#define DE_Y 0.275
	#define DE_Z 0.400
	#define DG_W 0.1875 // Slight adjustment to the ZPD Boundary
	#define BM 1 // Had to use this mode since Auto Mode was not cutting it.
	#define DG_X 0.1
	#define RH 1
	#define PE 1
#elif (App == 0xABAA2255 ) //The Forest
	#define DA_W 1
	#define DB_X 1
	#define DA_Y 7.5
	#define DA_X 0.04375
	#define DB_Y 3
	#define RH 1
#elif (App == 0x67A4A23A ) //Crash Bandicoot N.Saine Trilogy
	#define DA_Y 7.5
    #define DF_Y 0.0625
	#define DA_Z -0.250
	#define DA_X 0.1
	#define DB_Y 4
	#define DF_W 0.580
	#define HM 1
#elif (App == 0xE160AE14 ) //Spyro Reignited Trilogy
	#define DA_W 1
	#define DA_Y 12.5
	#define DA_Z 0.0001
	#define DA_X 0.05625
	#define DB_Y 3
#elif (App == 0x5833F81C ) //Dying Light
	#define DA_W 1
	#define DF_Y 0.045
	#define DA_X 0.05
	#define DA_Y 17.5
	#define DA_Z -0.5
	#define DB_Y 4
	#define NW 1
	#define PE 1
    #define FV 1
    #define DG_Z 0.070 //Min
    #define DE_W 0.100 //Max
	//#define DB_W 62
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
	#define RH 1
	#define NF 1
#elif (App == 0x88004DC9 || App == 0x1DDA9341) //Strange Brigade DX12 & Vulkan
	#define DA_X 0.0625
	#define DF_Y 0.02
	#define DA_Y 20.0
	#define DA_Z 0.0001
	#define DB_Y 5
	#define DE_X 2
	#define DE_Y 0.3
	#define DE_Z 0.475
	#define RH 1
#elif (App == 0xC0052CC4) //Halo The Master Chief Collection
	#define DA_X 0.037
	#define DA_W 1
	#define DA_Y 75.0
	#define DB_Y 5
	#define DE_X 3
	#define DE_Y 0.375
	#define DE_Z 0.375
	#define WSM 3
	#define OW_WP "Read Help & Change Me\0Custom WP\0Halo: Reach\0Halo: CE Anniversary\0Halo 2: Anniversary\0Halo 3\0Halo 3: ODST\0Halo 4\0"
	#define RH 1
#elif (App == 0x2AB9ECF9) //System ReShock
	#define DA_X 0.05
	#define DA_W 1
	#define DA_Y 11.25
	#define DA_Z 0.00125
	#define DB_Y 4
	#define DE_X 4
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
	#define RH 1
#elif (App == 0xA640659C) //MegaMan 2.5D in 3D
	#define DA_X 0.150
	#define DA_Y 8.75
	#define DA_Z -1005.0
	#define DE_X 1
	#define DE_Y 0.275
	#define DE_Z 0.375
	#define RH 1
#elif (App == 0x49654776) //Paratopic
	#define DA_X 0.05
	#define DA_W 1
	#define DA_Y 8.75
	#define DA_Z 0.000250
	#define DB_Y 3
	#define DB_X 1
	#define RH 1
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
	#define NC 1
	#define RH 1
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
	#define RH 1
#elif (App == 0xB7C22840) //Strife
	#define DA_X 0.1
	#define DA_Y 250.0
	#define DB_W 59
	#define RH 1
#elif (App == 0x21DC397E || App == 0x653AF1E1) //Gold Source
	#define DA_X 0.045
	#define DA_Y 21.25
	#define DA_Z 0.0003
	#define DB_Y 3
	#define DB_W 60
#elif (App == 0xC2E621A5) //No Man Sky
	#define DA_X 0.04375
	#define DA_W 1
	#define DA_Y 72.5
	#define DB_Y 5
  #define DB_Z 0.0
  #define DE_X 1
	#define DE_Y 0.375
	#define DE_Z 0.4
	#define RH 1
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
	#define HM 1
#elif (App == 0x242D82C4 ) //Okami HD
	#define DA_X 0.200
	#define DA_W 1
	#define DA_Z 0.001
	#define DB_Y 1
	#define DE_X 2
  #define DE_Y 0.125
  #define DE_Z 0.375
	#define DF_W 0.5
	#define HM 1
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
	#define RH 1
#elif (App == 0x5925FCC8 ) //Dusk
	#define DA_Y 25.0
	#define DA_X 0.05
	#define DB_Y 5
	#define DA_W 1
	#define DB_X 1
	#define DE_X 3
	#define DE_Z 0.375
#elif (App == 0xDDA80A38 ) //Deus Ex Rev DX9
	#define DA_X 0.04375
	#define DA_Y 20
	#define DB_Y 3
	#define DB_W 23
	#define DF_W 0.534
	#define HM 1
	#define DF_X 0.025
#elif (App == 0x1714C977) //Deus Ex DX9
	#define DA_X 0.05
	#define DA_Y 125.0
	#define DB_Y 3
	#define DB_W 24
	#define DF_W 1.0
	#define HM 1
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
	#define NW 1
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
	#define DE_X 4
	#define DE_Z 0.375
#elif (App == 0x56301DED ) //ShadowWarrior 2
	#define DA_X 0.035
	#define DA_W 1
	#define DB_Y 4
	#define DE_X 4
	#define DE_Z 0.375
	#define NW 1
#elif (App == 0x892CA092 ) //Farcry
	#define DA_Y 7.0
	#define DA_Z 0.000375
	#define DB_Z 0.105
	#define DA_X 0.055
	#define DB_Y 4
	#define DB_W 63
	#define DF_X 0.13875
#elif (App == 0x9140DBE0 ) //Farcry 2
	#define DA_X 0.05
	#define DB_Y 4
	#define DB_W 64
	#define DE_X 4
	#define DE_Z 0.375
	#define RH 1
#elif (App == 0xA4B66433 ) //Farcry 3
	#define DA_X 0.05
	#define DB_Y 4
	#define DE_X 4
	#define DE_Z 0.375
	#define DE_W 0.350
	#define RH 1
#elif (App == 0xC150B652 ) //Farcry 4
	#define DA_Y 8.75
	#define DA_W 1
	#define DA_X 0.0375
	#define DB_Y 4
	#define DE_X 4
	#define DE_Z 0.375
	#define DE_W 0.360
#elif (App == 0x2EB82B07 ) //Farcry Primal
	#define DA_Y 8.75
	#define DA_W 1
	#define DA_X 0.0375
	#define DB_Y 4
	#define DE_X 4
	#define DE_Z 0.375
	#define DE_W 0.360
#elif (App == 0xC150B805 ) //Farcry 5
	#define DA_Y 8.75
	#define DA_W 1
	#define DA_X 0.0375
	#define DB_Y 4
	#define DE_X 4
	#define DE_Z 0.375
	#define DE_W 0.360
	#define RH 1
#elif (App == 0xE3AD2F05 ) //Sauerbraten
	#define DA_Y 25.0
	#define DA_X 0.05
	#define DB_Y 5
	#define DB_W 73
	#define DF_X 0.150
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
	#define DA_X 0.125
	#define DB_Y 4
	#define DE_X 2
	#define DE_Y 0.162
	#define DE_Z 0.4375
	#define RH 1
#elif (App == 0xCD0E316F ) //Sonic Adventure DX Modded with BetterSADX
	#define DA_Y 8.75
	#define DA_X 0.1125
	#define DB_Y 3
	#define DE_X 2
	#define DE_Y 0.375
	#define DE_Z 0.4375
	#define DB_Z 0.250
	#define RH 1
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
	#define BM 1
	#define DG_X 0.275
	#define DB_Y 4
	#define DE_X 1
	#define DE_Y 0.375
	#define DE_Z 0.375
	#define DG_W 0.25
  #define DS 1
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
	#define DB_W 66
	#define DE_X 1
	#define DE_Z 0.375
	#define DF_X 0.175
	#define RH 1
#elif (App == 0x905631F2 ) //Crysis DX10 64bit
	#define DA_X 0.0375
	#define DB_Y 5
	#define DB_W 44
	#define DE_X 3
	#define DE_Z 0.375
	#define DF_X -0.175
#elif (App == 0x6061750E ) //Mirror's Edge
	#define DA_Y 12.25
	#define DF_Y 0.020
	#define DA_X 0.040
	#define DB_Y 5
    #define DB_Z 0.01
	#define DE_X 1
	#define DE_Y 0.50
	#define DE_Z 0.375
	#define DB_W 70
	#define DS 1
#elif (App == 0xC3AF1228 || App == 0x95A994C8 ) //Spellforce
	#define DA_Y 145.0
	#define DA_Z 0.001
	#define DB_Z 0.05
	#define DA_X 0.05
	#define DB_Y 3
	#define DE_X 1
	#define DE_Y 0.235
	#define DE_Z 0.375
	#define HM 1
	#define DF_W 0.5
#elif (App == 0xD372612E ) //Raft
	#define DA_W 1
	#define DB_X 1
	#define DA_X 0.04375
	#define DB_Y 4
	#define NW 1
#elif (App == 0xC06FE818 ) //BorderLands 3
	#define DA_Y 18.0
	#define DA_Z 0.0001375
	#define DA_X 0.04
	#define DB_Z 0.05
	#define DA_W 1
	#define DB_Y 4
	#define DB_W 5
	#define DE_X 4
	#define DE_Y 0.425
	#define DE_Z 0.300
	#define DF_X 0.085
	#define NW 1
	#define DA 1
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
	#define DE_X 4
	#define DE_Y 0.550
	#define DE_Z 0.333
	#define DB_W 68
	//#define DG_Z 0.080 //Min
    #define DE_W 0.125 //Max
	//#define DE_W 0.09375
	#define DA_X 0.03125
	#define DF_Y 0.03125
	//#define DA_X 0.0375 //Alternet settings Not used.
	#define PE 1
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
	#define RH 1
#elif (App == 0x21CB998 ) //.Hack//G.U.
	#define DA_Y 22.5
	#define DA_X 0.125
	#define DB_Y 3
	#define DE_X 1
	#define DE_Y 0.500
	#define DE_Z 0.3
	#define RH 1
	#define NF 1
#elif (App == 0x9CC5C8E0 ) //GTA V
	#define DA_Y 18.75
	#define DA_W 1
	#define DA_X 0.0475
	#define DB_Y 4
	#define DE_X 1
	#define DE_Y 0.325
	#define DE_Z 0.375
	#define DB_Z 0.05
	#define RH 1
	#define PE 1
	#define NF 1
#elif (App == 0x8CD23575 ) //Dark Souls: Remastered
	#define DA_Y 75.0
	//#define DA_Z 0.001
	#define DA_X 0.05625
	#define DF_Y 0.05
	#define DB_Y 2
	#define DE_X 2
	#define DE_Y 0.250
	#define DE_Z 0.375
	//#define DE_W 0.0625
	#define PE 1
	#define FV 1
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
	#define PE 1
	#define FV 1
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
	#define DA_Y 68.75
	#define DA_X 0.05
	#define DF_Y 0.05
	#define DB_Y 2
	#define DE_X 2
	#define DE_Y 0.250
	#define DE_Z 0.375
	#define DG_Z 0.025 //Min
  //#define DE_W 0.125 //Max
	#define PE 1
	#define FV 1
#elif (App == 0xCE5313C2 ) //BorderLands Enhanced
	#define DA_Y 18.75
	#define DA_Z 0.0005
	#define DA_X 0.055
	#define DF_Y 0.02
	#define DB_Z 0.075
	#define DB_Y 1
	#define DB_W 3
	#define DE_X 3
	#define DE_Y 0.425
	#define DE_Z 0.375
	#define DG_Z 0.3975
	#define DF_X 0.225
	#define NW 1
#elif (App == 0x2ECAAF29 || App == 0xE19E4830 || App == 0xE19E4830  ) //Half-Life 2 | Left 4 Dead 2
	#define DA_Y 8.75
	#define DA_X 0.04
	#define DB_Z 0.115
	#define DB_Y 3
	#define DB_W 20
	#define DE_X 4
	#define DE_Y 0.5
	#define DE_Z 0.375
	#define DF_X 0.105
	#define RH 1
#elif (App == 0x68EF1B4E || App == 0xC103D998 ) //Serious Sam Fusion | Serious Sam 4: Planet Badass
	#define DA_W 1
	#define DA_X 0.075
	#define DA_Y 10.0
	#define DA_Z 0.1
	#define DB_Y 1
	#define DB_W 42
	#define DE_X 4
	#define DE_Y 0.5
	#define DE_Z 0.375
	#define DB_Z 0.150
	#define NW 1
	#define RH 1
	#define PE 1
#elif (App == 0xEACB4D0D ) //Final Fantasy XV Windows Edition
	#define DA_X 0.0375
	#define DA_Y 30.0
	#define DB_Y 3
	#define DE_X 2
	#define DE_Y 0.5
	#define DE_Z 0.375
	#define RH 1
#elif (App == 0xAC4DF2C4 ) //Mafia II Definitive Edition
	#define DA_X 0.05
	#define DA_Y 37.5
	#define DA_Z 0.0007
	#define DB_Y 4
	#define DE_X 1
	#define DE_Y 0.5
	#define DE_Z 0.375
	#define DF 1
	#define RH 1
#elif (App == 0xEA75DEDE ) //Lost Planet Colonies
	#define DA_X 0.05
	#define DA_Y 37.5
	#define DB_Y 4
	#define DE_X 2
	#define DE_Y 0.5
	#define DE_Z 0.375
	#define RH 1
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
	#define RH 1
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
	#define DS 1
#elif (App == 0xE4F6014F ) //Shovel Knight
	#define DB_X 1
	#define DA_X 0.035
	#define DA_Y 22.5
	#define DA_Z 0.483
	#define RH 1
#elif (App == 0x94EFD213 ) //Chex Quest HD
	#define DA_W 1
	#define DA_X 0.1
	#define DA_Y 112.5
	#define DA_Z 0.0000125
	#define DB_Y 4
	#define DE_X 3
	#define DE_Y 0.5
	#define DE_Z 0.375
	#define DB_Z 0.125
	#define DB_W 74
#elif (App == 0xF6F3C763 ) //WRATH
	#define DA_X 0.065
	#define DA_Y 75.0
	#define DA_Z 0.00005
	#define DB_Y 2
	#define DE_X 3
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
	#define NC 1
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
	#define NC 1
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
	//#define BM 1
	//#define DG_X 0.1375
	#define DS 1
	#define LBC 1
#elif (App == 0x89351FC4 ) //3DSen Games
	#define DA_W 1
	#define DB_X 1
	#define DA_X 0.1
	#define DA_Y 375.0
	#define DB_Y 5
	#define DE_X 1
	#define DE_Y 0.5
	#define DE_Z 0.250
	#define DF 1
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
	#define DA 1
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
	#define RH 1
	#define NF 1
	#define DA_X 0.06
	#define DA_Y 17.5
	#define DA_Z 0.0003
	#define DB_Y 4
	#define DB_W 22
#elif (App == 0x12C96DB0 ) //Hexen 2 Hammer of Thyrion
	#define RH 1
	#define NF 1
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
	#define DB_W 75
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
	#define DG_Z 0.450
	#define DG_W 0.125
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
	#define DS 1
#elif (App == 0xBF53A67A ) //The Bureau: XCOM Declassified
	#define DA_X 0.04375
	#define DA_Y 29.0
	#define DA_Z 0.0003
	#define DB_Y 2
	#define DE_X 2
	#define DE_Y 0.5
	#define DE_Z 0.375
	#define DS 1
#elif (App == 0x1764D88A ) //X-Com 2
	#define DA_X 0.24
	#define DA_Y 29.0
	#define DA_Z 0.0001
	#define DB_Z 0.130
	#define DB_Y 3
	#define DE_X 2
	#define DE_Y 0.25
	#define DE_Z 0.375
	#define DE_W 0.075
#elif (App == 0xC60A845F ) //My Friend Pedro
	#define DB_X 1
	#define DA_W 1
	#define DA_X 0.075
	#define DA_Y 50.0
	#define DA_Z 0.000375
	#define DB_Y 4
	#define DB_Z 0.13
#elif (App == 0xD45ACB4B ) //Murdered Soul Suspect
	#define DA_X 0.05
	#define DA_Y 37.5
	#define DA_Z 0.001
	#define DB_Y 1
	#define DE_X 1
	#define DE_Y 0.5
	#define DE_Z 0.375
	#define DS 1
#elif (App == 0x4FF5CF63 ) //Lords of the Fallen
	#define DA_X 0.049
	#define DA_Y 70.0
	#define DA_Z 0.001
	#define DB_Y 2
	#define DE_X 1
	#define DE_Y 0.5
	#define DE_Z 0.375
	#define DE_W 0.415
	#define PE 1
#elif (App == 0xD0AA19 ) //The Bards Tale 4
	#define DA_W 1
	#define DA_X 0.0375
	#define DA_Y 20.0
	#define DB_Y 3
	#define DE_X 1
	#define DE_Y 0.5
	#define DE_Z 0.375
	#define DA 1
	#define PE 1
	#define RH 1
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
	#define DA 1
	#define PE 1
#elif (App == 0x36ECE27F ) //Supraland
	#define DA_W 1
	#define DA_Y 22.5
	#define DB_Y 2
	#define DE_X 2
	#define DE_Y 0.8
	#define DE_Z 0.375
	#define PE 1
	#define DA 1
#elif (App == 0x3604DCE6 ) //Remnant: From the Ashes
	#define DA_W 1
	#define DA_X 0.07
	//#define DF_Y 0.038
	#define DA_Y 16.25
	#define DB_Y 2
	#define DE_X 1
	#define DE_Y 0.5
	#define DE_Z 0.375
	#define DA_Z -0.050
	//#define DF_Z -0.125
	//#define BM 1
	//#define DG_X 0.1
	#define NW 1
	#define PE 1
#elif (App == 0x621202BC ) //Vanquish DGVoodoo2
	#define DA_X 0.05
	#define DA_Y 15.0
	#define DB_Y 3
	#define DE_X 1
	#define DE_Y 0.5
	#define DE_Z 0.375
	#define RH 1
	#define NF 1
	#define PE 1
#elif (App == 0xA1214CD1 ) //Life is Strange
	#define DA_X 0.125
	#define DA_Y 8.0
	#define DA_Z 0.0005
	#define DB_Y 4
	#define DE_X 1
	#define DE_Y 0.5
	#define DE_Z 0.375
	#define PE 1
	#define DS 1
	#define DA 1
#elif (App == 0x913AD2D ) //SpaceHulk DeathWing Enhanced Edition
	#define DA_W 1
	#define DA_X 0.05
	#define DA_Y 7.5
	#define DB_Y 4
	#define DE_X 1
	#define DE_Y 0.5
	#define DE_Z 0.375
	#define DE_W 0.065
	#define PE 1
	#define NW 1
#elif (App == 0xEB5CDE17 ) //Man Of Medan
	#define DA_W 1
	#define DA_X 0.04375
	#define DA_Y 20.0
	#define DB_Z 0.3
	#define DB_Y 4
	#define DE_X 1
	#define DE_Y 0.300
	#define DE_Z 0.300
	#define LBM 1
	#define RH 1
	#define NW 1
	#define SP 1
	#define DD_W -0.240
#elif (App == 0x62454263 ) //Red Dead Redemption 2
	#define DA_W 1
	#define DA_X 0.05
	#define DF_Y 0.11875
	#define DA_Y 35.5
	#define DB_Y 2
	#define DE_X 1
	#define DE_Y 0.5
	#define DE_Z 0.375
	#define PE 1
	#define DA 1
	#define LBM 2
#elif (App == 0xB5AE6CBA ) //Rage 2
	#define DA_W 1
	#define DA_X 0.04
	#define DA_Y 125.0
	#define DB_Y 2
	#define DE_W 0.06
	#define PE 1
	#define DA 1
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
	#define PE 1
	#define NW 1
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
	#define DB_W 65
	#define DA 1
#elif (App == 0x6EC76A83 ) //Watch Dogs 2
	#define DA_W 1
	#define DA_X 0.070
	#define DA_Y 12.0
	#define DB_Y 1
	#define DE_X 2
	#define DE_Y 0.500
	#define DE_Z 0.375
	#define PE 1
	#define DA 1
#elif (App == 0xD9691F81 ) //Destroy All Humans!
	#define DA_W 1
	#define DA_X 0.050
	#define DA_Y 66.0
	#define DB_Y 4
	#define DE_X 1
	#define DE_Y 0.500
	#define DE_Z 0.375
	#define DF_Y 0.025
	#define NF 1
	#define PE 1
	#define RH 1
#elif (App == 0x9C5C8E4D ) //INSIDE
	#define DA_X 0.050
	#define DB_Y 3
	#define DE_X 1
	#define DE_Y 0.500
	#define DE_Z 0.375
	#define DC 1
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
	#define DA 1
#elif (App == 0x76CD4369 ) //Resident Evil
	#define DA_X 0.06875
	#define DA_Y 24.0
	#define DA_Z 0.00025
	#define DB_Y 2
	#define DE_X 2
	#define DE_Y 0.500
	#define DE_Z 0.375
	#define DF_Y 0.024
	#define DA 1
	#define SP 1
	#define DD_X 1.333
	#define DD_Y 0.933
	#define LBM 4
	#define DS 1
	#define RH 1
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
	#define DA 1
#elif (App == 0xD86799B9 ) //Devil May Cry 5
	#define DA_W 1
	#define DA_X 0.0375
	#define DA_Y 40.0
	#define DB_Y 3
	#define DE_X 1
	#define DE_Y 0.300
	#define DE_Z 0.350
	#define DG_W 0.277 //0.5625 //I Think Erroring on the safe side is needed here.
	#define DC 1
	#define DC_X 0.025
	#define DC_Y 0.025
	#define DC_W -0.012
	#define DF_Y 0.02375
	#define PE 1
	#define RH 1
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
	#define SP 1
	//#define DD_Y 0.675
	//#define DD_W -0.4815
	#define DD_Y 0.9
	#define DD_W -0.111
	#define DB_W 72
	#define DS 1
#elif (App == 0x5C0EBBE9 ) //A Plague Tale Innocence
	#define DA_W 1
	#define DA_X 0.050
    #define DF_Y 0.025
	#define DA_Y 33.00
	#define DB_Y 3
	#define DE_X 2
	#define DE_Y 0.500
	#define DE_Z 0.375
	#define PE 1
	#define RH 1
	#define NF 1
	#define DS 1
#elif (App == 0xB2B11A3C ) //Catherine with a K
	#define DA_X 0.05
	#define DF_Y 0.0125
	#define DA_Y 50.0
	#define DB_Y 3
	#define DE_X 1
	#define DE_Y 0.500
	#define DE_Z 0.375
	#define DG_W 0.4375
	#define PE 1
	#define DA 1
#elif (App == 0x7ABE98F0 ) //Samurai Jack
	#define DA_W 1
	#define DA_X 0.1375
	#define DF_Y 0.0125
	#define DA_Y 20.0
	#define DB_Y 5
	#define DE_X 2
	#define DE_Y 0.500
	#define DE_Z 0.375
	#define DA 1
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
	#define DA_X 0.05625
	#define DF_Y 0.0625
	#define DA_Y 18.75
	#define DB_Y 3
	#define DE_X 1
	#define DE_Y 0.500
	#define DE_Z 0.375
	#define PE 1
	#define DA 1
#elif (App == 0x5A7B540A ) //We Where Here Too
	#define DA_W 1
	#define DB_X 1
	#define DA_X 0.05625
	#define DA_Y 42.5
	#define DB_Y 1
	#define NW 1
#elif (App == 0x6AB553A ) //We Where here Together
	#define DA_W 1
	#define DB_X 1
	#define DF_Y 0.05625
	#define DA_X 0.05625
	#define DA_Y 56.25
	#define DB_Y 3
	#define DE_X 3
	#define DE_Y 0.500
	#define DE_Z 0.375
	#define DB_W 30
	#define FV 1
	#define DA 1
	#define NW 1
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
	#define DS 1
	#define RH 1
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
	#define DS 1
	#define RH 1
	#define NF 1
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
	//#define SP 1
	#define DS 1
	#define RH 1
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
	#define DS 1
#elif (App == 0xCF0046B7 ) //SpecOps The Line
	#define DA_Y 15.0
	#define DA_X 0.05
	#define DF_Y 0.01
	#define DB_Y 4
	#define DA_Z -0.00125
	#define DE_X 1
	#define DE_Y 0.500
	#define DE_Z 0.375
	#define DS 1
	#define PE 1
	#define FV 1
#elif (App == 0x6C433D70 ) //Q.U.B.E 2
	#define DA_W 1
	#define DA_Y 75.0
	#define DA_X 0.05625
	#define DF_Y 0.01
	#define DB_Y 2
	#define DB_Z 0.05625
	#define DA_Z 0.00025
	#define DE_X 3
	#define DE_Y 0.450
	#define DE_Z 0.375
	#define PE 1
	#define FV 1
	#define DB_W 69
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
	#define PE 1
	#define FV 1
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
	#define PE 1
	#define DS 1
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
	#define PE 1
	#define FV 1
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
	#define PE 1
	#define DA 1
#elif (App == 0x8C8F544C ) //Witcher 3
	#define DA_W 1
    //#define DB_X 1
	#define DA_Y 12.5
	#define DA_X 0.075
	#define DF_Y 0.0125
	//#define DF_Y 0.1
	#define DB_Y 4
	//#define DB_Z 0.125
	//#define DA_Z 0.0005
	#define DE_X 2
	#define DE_Y 0.500
	#define DE_Z 0.300
	#define PE 1
	#define DA 1
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
	#define DS 1
	#define RH 1
	#define NF 1
	#define SP 1
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
	#define PE 1
	#define FV 1
    #define DA 1
	//#define DS 1
	//#define RH 1
	//#define NF 1
	//#define SP 1
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
	#define PE 1
	#define FV 1
    //#define DA 1
	#define DS 1
	//#define RH 1
	//#define NF 1
	//#define SP 1
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
	#define PE 1
	#define FV 1
	//#define DA 1
	//#define DS 1
	//#define RH 1
	//#define NF 1
	//#define SP 1
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
	#define DE_X 3
	//#define DE_Y 0.450
	#define DE_Z 0.375
	#define PE 1
	#define FV 1
	#define DB_W 67
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
	#define PE 1
	#define DS 1
	#define RH 1
#elif (App == 0xAA5644F9 || App == 0x1981FECC ) //Need For Speed: Heat | Payback
	#define DA_W 1
	#define DA_Y 12.5
	#define DA_X 0.1
	#define DF_Y 0.1
	#define DB_Y 4
	//#define DB_Z 0.1
	//#define DA_Z 0.00025
	//#define DE_X 2
	//#define DE_Y 0.450
	//#define DE_Z 0.375
	#define PE 1
	#define DA 1
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
	#define PE 1
	#define FV 1
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
	#define DE_X 3
	#define DE_Z 0.375
	#define DB_W 76
#elif (App == 0xC54A173B ) //Dead or Alive 6
	#define DA_W 1
	#define DA_Y 60.0
	#define DA_X 0.06
	#define DB_Y 5
	#define RH 1
	#define NW 1
	#define NF 1
#elif (App == 0x934DC835 || App == 0xD063D305 || App == 0xE29F2D4 ) //Dead Rising | Dead Rising 2 | Dead Rising 2 Off The Record
	#define DA_Y 25.0
	#define DA_X 0.125
	#define DB_Y 5
	#define DE_X 1
	#define DE_Z 0.375
	#define NW 1
#elif (App == 0xF28EC9C2 || App == 0xF28EC143 ) //Dead Rising 3 | Dead Rising 4
	#define DA_Y 20.0
	#define DA_X 0.1
	#define DB_Y 5
	#define DE_X 2
	#define DE_Z 0.375
	#define RH 1
	#define NW 1
	#define NF 1
#elif (App == 0xC402F6B8 ) //Iron Harvest
	#define DA_W 1
	#define DA_Y 125.0
	#define DA_Z 0.0015
	#define DA_X 0.250
	#define DB_Y 3
	#define DE_X 1
	#define DE_Y 0.1
	#define DE_Z 0.4
	#define PE 1
	#define NW 1
	#define DA 1
#elif (App == 0xA867FE21 ) //Senran Kagura Peach Ball
	#define DA_Y 72.5
	#define DA_Z -0.0011
	#define DA_X 0.200
	#define DF_Y 0.200
	#define DB_Y 2
	#define DE_X 2
	#define DE_Y 0.075
	#define DE_Z 0.375
	#define SDT 1
	#define HM 1
	#define DF_W 0.5
#elif (App == 0x1BDC0C4C ) //Quake Enhanced Edition
	#define DA_X 0.07
	#define DA_Y 13.0
	#define DB_Y 1
	#define DE_X 3
	#define DE_Y 0.5
	#define DE_Z 0.375
	#define NW 1
	#define PE 1
	#define DB_W 71
	#define DF_X 0.250
#elif (App == 0xB3729F40 ) //Rocket League Steam
	#define DA_Y 50.0
	#define DA_X 0.100
	#define DB_Y 5
	#define DS 1
	#define NW 1
	#define PE 1
#elif (App == 0x1BB6E62A ) //AMIN EVIL RTX
	#define DA_W 1
	#define DA_X 0.07
	#define DA_Y 12.5
	#define DA_Z 0.000125
	#define DB_Y 5
	#define DE_X 3
	#define DE_Y 0.5
	#define DE_Z 0.45
	#define PE 1
	#define DA 1
	#define DB_W 27
#elif (App == 0xFBC55DDE ) //Tormented Shouls Demo - Add to real game if Application ID is given.
	#define DA_W 1
	#define DA_Y 15.0
	#define DA_Z 0.00125
	#define DA_X 0.06125
	#define DB_X 1
	#define DB_Y 2
	#define DE_X 2
	//#define DG_Z 0.288 // This works. But, may be a bit overboard. Use this if users complain about edge pop out issues. I don't think it's needed.
	#define PE 1
#elif (App == 0x920D5D88 ) //Graven
	#define DA_W 1
	#define DA_X 0.1
	#define DA_Y 50.0
	#define DA_Z 0.00005
	#define DB_Y 5
	#define DE_X 3
	#define DE_Y 0.5
	#define DE_Z 0.375
	#define PE 1
	#define DA 1
	//#define DB_W 54 //Graven WP Not used Due to Clipping on world. Even if it looks good. Maybe Give people the option???
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
	#define NW 1
	#define DS 1
#elif (App == 0xD0F69E54 ) //Yooka-Laylee
	#define DA_Y 12.75
	#define DA_Z 0.001
	#define DA_X 0.09125
	#define DF_Y 0.00625
	#define DB_X 1
	#define DB_Y 4
	#define DE_X 2
	#define DE_Y 0.325
	#define DG_W 0.25 //allowed popout
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
	#define DE_X 3
	#define DE_Y 0.500
	#define DE_Z 0.4375
	#define DB_W 34
	#define DF_X 0.20
	#define DG_W 0.08
	#define BM 1
	#define DG_X 0.130
	#define PE 1
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
	#define PE 1
	#define DA 1
#elif (App == 0x87AC1510 ) //Ghostrunner
	#define DA_W 1
	#define DA_Y 245.0
	#define DA_Z 0.0000025 // Magic
	#define DA_X 0.050
	#define DF_Y 0.047
	#define DB_Z 0.0625
	#define DB_Y 1
	#define BM 1
	#define DG_X 0.180
	#define DE_X 3
	#define DE_Y 0.650
	#define DE_Z 0.400
	#define DB_W 43
	#define DF_X 0.1
	#define PE 1
	#define DA 1
#elif (App == 0x11E6C55E ) //The Suicide of Rachel Foster
	#define DA_W 1
	#define DA_Y 35.0
	#define DA_X 0.030
	#define DF_Y 0.020
	#define DB_Z 0.050
	#define DB_Y 5
	#define DE_X 3
	#define DE_Y 0.250
	#define DE_Z 0.425
	#define DG_Z 0.449
	#define DG_W 0.37 //Allow much popout "Please don't abuse this."
	#define PE 1
	#define DA 1
	#define RH 1
	#define WSM 2
	#define DB_W 2
#elif (App == 0xFC960068 ) //Devolverland Expo
	#define DA_W 1
	#define DA_Y 30.0
	#define DA_X 0.050
	#define DB_Z 0.050
	#define DB_Y 5
	#define DE_X 3
	#define DE_Y 0.50
	#define DE_Z 0.375
	#define WSM 2
	#define DB_W 3
	#define PE 1
	#define DA 1
#elif (App == 0x59DA13F1 ) //Conarium
	#define DA_W 1
	#define DA_Y 18.75
	#define DA_X 0.0875
	#define DF_Y 0.0328125
	#define DB_Z 0.075
	#define DB_Y 3
	#define DE_X 3
	#define DE_Y 0.525
	#define DE_Z 0.400
	#define DG_Z 0.305
	#define DG_W 0.0875 //Allow much popout "Please don't abuse this."
	#define PE 1
	#define DA 1
	#define RH 1
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
	#define BM 1
	#define DG_X 0.1375
	#define DS 1
	#define FV 1
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
	#define PE 1
	#define DA 1
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
	#define BM 1
	#define DG_X 0.165
	#define PE 1
	#define DA 1
	#define DS 1
	#define NW 1
#elif (App == 0x98C69E31 || App == 0xA8778B7D ) // F1 2019 | F1 2020 DX12
	#define DA_W 1
	#define DG_W 0.25
	#define DA_Y 14.5
	#define DA_X 0.07
	#define DB_Y 5
	#define DE_X 2
	#define DE_Z 0.400
	#define DG_Z 0.400
	#define PE 1
	#define NW 1
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
	#define BM 1
	#define DG_X 0.1625
	#define PE 1
	#define DA 1
	//#define DS 1
	#define NW 1
#elif (App == 0x54568EA ) //Assetto Corsa
	#define DA_X 0.05
	#define DB_Y 4
	#define DE_X 1
	#define DE_Y 0.750
	#define DE_Z 0.375
	#define DG_W 0.2
	#define PE 1
	#define DA 1
	#define NW 1
	#define FV 1
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
	#define DS 1
	#define PE 1
#elif (App == 0xC57720A6 ) //Crysis 2 DX11 1.9
	#define DA_X 0.07
	//#define DA_Y 11.5
	#define DA_Z 0.00025
	#define DB_Y 2
	#define DE_X 3
	//#define DE_Y 0.625
	//#define DE_Z 0.375
	//#define DG_W 0.25
	#define WSM 2
	#define DB_W 7
	#define DF_X 0.225
	#define DS 1 //?
	#define PE 1
	#define RH 1
#elif (App == 0xDB778A3B ) //Portal 2
	#define DA_X 0.05
	#define DA_Y 20.5
	#define DA_Z 0.001
    #define DB_Z 0.105
	#define DB_Y 3
	#define DE_X 3
	#define DE_Y 0.7
	#define DE_Z 0.375
	#define DG_W 0.125
	#define DB_W 36
    #define DG_Z 0.430
	#define DS 1
	#define PE 1
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
	#define PE 1	
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
	#define BM 1
	#define DG_X 0.165
	#define PE 1
	#define NW 1
#elif (App == 0x19D0F410 ) //Zombie Army Trilogy
	#define DA_Y 12.5
	#define DA_X 0.155
	#define DA_Z -0.00025
	#define DB_Y 5
	#define DE_X 1
	#define DE_Y 0.375
	#define DE_Z 0.375
	#define DG_W 0.7
	//#define BM 1
	//#define DG_X 0.165
	#define PE 1
	#define NW 1
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
	#define NW 1
#elif (App == 0xEEAF4DE ) //Guardians of the galaxy
    #define DA_W 1
	#define DA_Y 54.0
	#define DA_X 0.05
	#define DB_Y 1
	#define DB_Z 0.12
	#define DE_X 1
	#define DE_Y 0.375
	#define DE_Z 0.400
	#define DF_Y 0.0375
	#define DG_W 0.2
	#define DA 1
	#define PE 1
#elif (App == 0x967BB1CC ) //HROT
	#define DA_X 0.055
	#define DF_Y 0.025
	#define DA_Y 150.0
	#define DB_Z 0.0875
	#define DB_Y 2
	#define DE_X 3
	#define DE_Y 0.550
	#define DE_Z 0.375
	#define WSM 2 //Weapon Settings Mode
	#define DB_W 11
	#define DS 1 
	#define PE 1
#elif (App == 0x11763BB7 ) //FATAL Frame Maiden of the Black Water.... Too Damn spooky....
	#define DA_X 0.0825
	#define DF_Y 0.04125
	#define DA_Y 16.25
	#define DB_Z 0.275
	#define DB_Y 4
	#define DE_X 1
	#define DE_Y 0.250
	#define DE_Z 0.375
	#define PE 1
	#define DA 1
#else
	#define NP 1 //No Profile
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
    #define DE_W Weapon_Near_Depth_Max_D
#endif
// X = [ZPD Weapon Boundary] Y = [Separation] Z = [Null] W = [HUD]
#ifndef DF_X
    #define DF_X ZPD_Weapon_Boundary_Adjust
#endif
#ifndef DF_Y
    #define DF_Y Separation
#endif
#ifndef DF_Z
    #define DF_Z Null_Z
#endif
#ifndef DF_W
    #define DF_W HUDX_D
#endif
// X = [ZPD Balance] Y = [Null] Z = [Weapon NearDepth Min] W = [Check Depth Limit]
#ifndef DG_X
    #define DG_X Manual_ZPD_Balance
#endif
#ifndef DG_Y
    #define DG_Y Null_Y
#endif
#ifndef DG_Z
    #define DG_Z Weapon_Near_Depth_Min_D
#endif
#ifndef DG_W
	#define DG_W Check_Depth_Limit
#endif

//Special Toggles
#ifndef RE
    #define RE REF //Resident Evil Fix
#endif
#ifndef NC
    #define NC NCW //Not Compatible Warning
#endif
#ifndef RH
    #define RH RHW //Read Help Warning
#endif
#ifndef ED
    #define ED EDW //Emulator Detected Warning
#endif
#ifndef NP
    #define NP NPW //No Profile Warning
#endif
#ifndef ID
    #define NI IDF //Inverted Depth Fix
#endif
#ifndef SP
    #define SP SPF //Size & Position Fix
#endif
#ifndef DC
    #define DC BDF //Barrel Distortion Fix
#endif
#ifndef HM
    #define HM HMT //HUD Mode Trigger
#endif
#ifndef DF
    #define DF DFW //Delay Frame Workaround
#endif
#ifndef NF
    #define NF NFM //Needs Fix and/or Modding
#endif
#ifndef DS
    #define DS DSW //Depth Selection Warning
#endif
#ifndef LBC
    #define LBC ALB //Auto Letter Box
#endif
#ifndef LBM
    #define LBM LBD //Letter Box Depth
#endif
#ifndef SDT
    #define SDT STD //Specialized Depth Trigger
#endif
#ifndef DA
    #define DA DAA //Disable Anti-Aliasing
#endif
#ifndef NW
    #define NW NWW //Network Warning
#endif
#ifndef PE
    #define PE PEW //Disable Post Effect Warning
#endif
#ifndef WW
    #define WW WPW //Weapon Profile Warning
#endif
#ifndef FV
    #define FV FOV //Set Game FoV
#endif
#ifndef BM
    #define BM BMT //Balance Mode Toggle
#endif
//Weapon Settings "Use #define WSM 1 or 2"
#ifndef OW_WP     //This is used if OW_WP is not called in the Above Profile
    #define OW_WP "WP Off\0Custom WP\0WP 0\0WP 1\0WP 2\0WP 3\0WP 4\0WP 5\0WP 6\0WP 7\0WP 8\0WP 9\0WP 10\0WP 11\0WP 12\0WP 13\0WP 14\0WP 15\0WP 16\0WP 17\0WP 18\0WP 19\0WP 20\0WP 21\0WP 22\0WP 23\0WP 24\0WP 25\0WP 26\0WP 27\0WP 28\0WP 29\0WP 30\0WP 31\0WP 32\0WP 33\0WP 34\0WP 35\0WP 36\0WP 37\0WP 38\0WP 39\0WP 40\0WP 41\0WP 42\0WP 43\0WP 44\0WP 45\0WP 46\0WP 47\0WP 48\0WP 49\0WP 50\0WP 51\0WP 52\0WP 53\0WP 54\0WP 55\0WP 56\0WP 57\0WP 58\0WP 59\0WP 60\0WP 61\0WP 62\0WP 63\0WP 64\0WP 65\0WP 66\0WP 67\0WP 68\0WP 69\0WP 70\0WP 71\0WP 72\0WP 73\0WP 74\0"
#endif
#ifndef WSM //One is Profiles List A | Two is Profiles List B | Three is MCC | Four is Prey | Five is Blood 2
    #define WSM 1 //Weapon Setting Mode
#endif

#if WSM == 1
float3 Weapon_Profiles(float WP ,float3 Weapon_Adjust) //Tried Switch But, can't compile in some older versions of ReShade.
{   if (WP == 2)
        Weapon_Adjust = float3(0.425,5.0,1.125);      //WP 0  | ES: Oblivion
    if (WP == 3)
        Weapon_Adjust = float3(0.276,16.25,9.15);      //WP 1  | BorderLands
    if (WP == 4)
        Weapon_Adjust = float3(0.5,32.5,7.15);        //WP 2  | BorderLands 2
    if (WP == 5)
        Weapon_Adjust = float3(0.284,10.5,0.8725);    //WP 3  | BorderLands 3
    if (WP == 6)
        Weapon_Adjust = float3(0.253,39.0,97.5);      //WP 4  | Fallout 4
    if (WP == 7)
        Weapon_Adjust = float3(0.276,20.0,9.5625);    //WP 5  | Skyrim: SE
    if (WP == 8)
        Weapon_Adjust = float3(0.338,21.0,9.1375);    //WP 6  | DOOM 2016
    if (WP == 9)
        Weapon_Adjust = float3(0.255,177.5,63.025);   //WP 7  | CoD:Black Ops | CoD:MW2 | CoD:MW3
    if (WP == 10)
        Weapon_Adjust = float3(0.254,100.0,0.9843);   //WP 8  | CoD:Black Ops II
    if (WP == 11)
        Weapon_Adjust = float3(0.254,203.125,0.98435);//WP 9  | CoD:Ghost
    if (WP == 12)
        Weapon_Adjust = float3(0.254,203.125,0.98433);//WP 10 | CoD:AW | CoD:MW Re
    if (WP == 13)
        Weapon_Adjust = float3(0.254,125.0,0.9843);   //WP 11 | CoD:IW
    if (WP == 14)
        Weapon_Adjust = float3(0.255,200.0,63.0);     //WP 12 | CoD:WaW
    if (WP == 15)
        Weapon_Adjust = float3(0.510,162.5,3.975);    //WP 13 | CoD | CoD:UO | CoD:2
    if (WP == 16)
        Weapon_Adjust = float3(0.254,23.75,0.98425);  //WP 14 | CoD: Black Ops IIII
    if (WP == 17)
        Weapon_Adjust = float3(0.375,60.0,15.15625);  //WP 15 | Quake DarkPlaces
    if (WP == 18)
        Weapon_Adjust = float3(0.7,14.375,2.5);       //WP 16 | Quake 2 XP
    if (WP == 19)
        Weapon_Adjust = float3(0.750,30.0,1.050);     //WP 17 | Quake 4
    if (WP == 20)
        Weapon_Adjust = float3(0.278,62.5,9.1);       //WP 18 | Half-Life 2
    if (WP == 21)
        Weapon_Adjust = float3(0.450,12.0,23.75);     //WP 19 | Metro Redux Games
    if (WP == 22)
        Weapon_Adjust = float3(0.350,12.5,2.0);       //WP 20 | Soldier of Fortune
    if (WP == 23)
        Weapon_Adjust = float3(0.286,1500.0,7.0);     //WP 21 | Deus Ex rev
    if (WP == 24)
        Weapon_Adjust = float3(35.0,250.0,0);         //WP 21 | Deus Ex
    if (WP == 25)
        Weapon_Adjust = float3(0.625,350.0,0.785);    //WP 23 | Minecraft
    if (WP == 26)
        Weapon_Adjust = float3(0.255,6.375,53.75);    //WP 24 | S.T.A.L.K.E.R: Games
    if (WP == 27)
        Weapon_Adjust = float3(0.450,5.5625,0.0);     //WP 25 | AMID EVIL RTX
    if (WP == 28)
        Weapon_Adjust = float3(0.750,30.0,1.025);     //WP 26 | Prey 2006
    if (WP == 29)
        Weapon_Adjust = float3(0.266,30.0,14.0);      //WP 27 | Wrath
    if (WP == 30)
        Weapon_Adjust = float3(3.625,20.0,0);         //WP 28 | We Where Here Together
    if (WP == 31)
        Weapon_Adjust = float3(0.7,9.0,2.3625);       //WP 29 | Return to Castle Wolfenstine
    if (WP == 32)
        Weapon_Adjust = float3(0.4894,62.50,0.98875); //WP 30 | Wolfenstein
    if (WP == 33)
        Weapon_Adjust = float3(1.0,93.75,0.81875);    //WP 31 | Wolfenstein: The New Order #C770832 / The Old Blood #3E42619F
    if (WP == 34)
        Weapon_Adjust = float3(1.150,55.0,0.9);       //WP 32 | Cyberpunk 2077
    if (WP == 35)
        Weapon_Adjust = float3(0.278,37.50,9.1);      //WP 33 | Black Mesa
    if (WP == 36)
        Weapon_Adjust = float3(0.277,105.0,8.8625);   //WP 34 | Portal 2
    if (WP == 37)
        Weapon_Adjust = float3(0.0,0.0,0.0);          //WP 35 | Game
    if (WP == 38)
        Weapon_Adjust = float3(0.78,21.25,0.1875);    //WP 36 | SOMA
    if (WP == 39)
        Weapon_Adjust = float3(0.444,20.0,1.1875);    //WP 37 | Cryostasis
    if (WP == 40)
        Weapon_Adjust = float3(0.286,80.0,7.0);       //WP 38 | Unreal Gold with v227
    if (WP == 41)
        Weapon_Adjust = float3(0.280,18.75,9.03);     //WP 39 | Serious Sam Revolution #EB9EEB74/Serious Sam HD: The First Encounter /The Second Encounter /Serious Sam 2 #8238E9CA/ Serious Sam 3: BFE*
    if (WP == 42)
        Weapon_Adjust = float3(0.3,17.5,0.9015);      //WP 40 | Serious Sam Fusion
    if (WP == 43)
        Weapon_Adjust = float3(1.2,12.5,0.3);         //WP 41 | GhostRunner DX12
    if (WP == 44)
        Weapon_Adjust = float3(0.277,20.0,8.8);       //WP 42 | TitanFall 2
    if (WP == 45)
        Weapon_Adjust = float3(0.7,16.250,0.300);     //WP 43 | Project Warlock
    if (WP == 46)
        Weapon_Adjust = float3(0.625,9.0,2.375);      //WP 44 | Kingpin Life of Crime
    if (WP == 47)
        Weapon_Adjust = float3(0.28,20.0,9.0);        //WP 45 | EuroTruckSim2
    if (WP == 48)
        Weapon_Adjust = float3(0.460,12.5,1.0);       //WP 46 | F.E.A.R #B302EC7 & F.E.A.R 2: Project Origin #91D9EBAF
    if (WP == 49)
        Weapon_Adjust = float3(1.5,37.5,0.99875);     //WP 47 | Condemned Criminal Origins
    if (WP == 50)
        Weapon_Adjust = float3(2.0,16.25,0.09);       //WP 48 | Immortal Redneck CP alt 1.9375
    if (WP == 51)
        Weapon_Adjust = float3(0.485,62.5,0.9625);    //WP 49 | Dementium 2
    if (WP == 52)
        Weapon_Adjust = float3(0.489,68.75,1.02);     //WP 50 | NecroVisioN & NecroVisioN: Lost Company #663E66FE
    if (WP == 53)
        Weapon_Adjust = float3(1.0,237.5,0.83625);    //WP 51 | Rage64
    if (WP == 54)
        Weapon_Adjust = float3(13.870,50.0,0.0);      //WP 52 | Graven
    if (WP == 55)
        Weapon_Adjust = float3(0.425,15.0,99.0);      //WP 53 | Bioshock Remastred
    if (WP == 56)
        Weapon_Adjust = float3(0.425,21.25,99.5);     //WP 54 | Bioshock 2 Remastred
    if (WP == 57)
        Weapon_Adjust = float3(0.425,5.25,1.0);       //WP 55 | No One Lives Forever
    if (WP == 58)
        Weapon_Adjust = float3(0.519,31.25,8.875);    //WP 56 | No One Lives Forever 2
    if (WP == 59)
        Weapon_Adjust = float3(0.5,8.0,0);            //WP 57 | Strife
    if (WP == 60)
        Weapon_Adjust = float3(0.350,9.0,1.8);        //WP 58 | Gold Source
    if (WP == 61) //Unity Limit if using else if
        Weapon_Adjust = float3(1.825,13.75,0);        //WP 59 | No Man Sky FPS Mode
    if (WP == 62)
        Weapon_Adjust = float3(1.953,5.25,0);         //WP 60 | Dying Light
    if (WP == 63)
        Weapon_Adjust = float3(0.287,180.0,9.0);      //WP 61 | Farcry
    if (WP == 64)
        Weapon_Adjust = float3(0.2503,55.0,1000.0);   //WP 62 | Farcry 2
    if (WP == 65)
        Weapon_Adjust = float3(0.279,100.0,0.905);    //WP 63 | Talos Principle
    if (WP == 66)
        Weapon_Adjust = float3(0.2503,52.5,987.5);    //WP 64 | Singularity
    if (WP == 67)
        Weapon_Adjust = float3(0.251,12.5,925.0);     //WP 65 | Betrayer
    if (WP == 68)
        Weapon_Adjust = float3(1.035,16.0,0.185);     //WP 66 | Doom Eternal
    if (WP == 69)
        Weapon_Adjust = float3(1.553,16.875,0.0);     //WP 67 | Q.U.B.E 2
    if (WP == 70)
        Weapon_Adjust = float3(0.251,5.6875,950.0);   //WP 68 | Mirror Edge
    if (WP == 71)
        Weapon_Adjust = float3(0.345,10.125,1.825);   //WP 69 | Quake Enhanced Edition
    if (WP == 72)
        Weapon_Adjust = float3(0.430,6.250,0.100);    //WP 70 | The Citadel 186
    if (WP == 73)
        Weapon_Adjust = float3(0.800,15.0,0.3);       //WP 71 | Sauerbraten 2
    if (WP == 74)
        Weapon_Adjust = float3(13.3,62.5,0.0);        //WP 72 | Chex Quest HD
    if (WP == 75)
        Weapon_Adjust = float3(0.75,112.5,0.5);       //WP 73 | Hexen 2
    if (WP == 76) //DX 9 Temp Registers Limit
        Weapon_Adjust = float3(0.350,17.5,2.050);     //WP 74 | Star Trek EliteForce II

		return Weapon_Adjust;
}
#elif WSM == 2
float3 Weapon_Profiles(float WP ,float3 Weapon_Adjust) //Could reduce from 76 to 57 to save on compiling time.
{   if (WP == 2)
        Weapon_Adjust = float3(0.6,6.5,0.0);          //WP 0  | The Suicide of Rachel Foster
    if (WP == 3)
        Weapon_Adjust = float3(1.653,17.5,0.0);       //WP 1  | Devolverland Expo
    if (WP == 4)
        Weapon_Adjust = float3(1.489,16.875,0.0);     //WP 2  | Conarium
    if (WP == 5)
        Weapon_Adjust = float3(0.270,25.0,0.951);     //WP 3  | WRC 10
    if (WP == 6)
        Weapon_Adjust = float3(0.0,0.0,0.0);          //WP 4  | Game
    if (WP == 7)
        Weapon_Adjust = float3(0.275,11.0,10.0);      //WP 5  | Crysis 2 DX11 1.9
    if (WP == 8)
        Weapon_Adjust = float3(0.0,0.0,0.0);          //WP 6  | Game
    if (WP == 9)
        Weapon_Adjust = float3(0.0,0.0,0.0);          //WP 7  | Game
    if (WP == 10)
        Weapon_Adjust = float3(0.0,0.0,0.0);          //WP 8  | Game
    if (WP == 11)
        Weapon_Adjust = float3(4.100,25.0,0.0);       //WP 9  | HROT
    if (WP == 12)
        Weapon_Adjust = float3(0.0,0.0,0.0);          //WP 10 | Game
    if (WP == 13)
        Weapon_Adjust = float3(0.0,0.0,0.0);          //WP 11 | Game
    if (WP == 14)
        Weapon_Adjust = float3(0.0,0.0,0.0);          //WP 12 | Game
    if (WP == 15)
        Weapon_Adjust = float3(0.0,0.0,0.0);          //WP 13 | Game
    if (WP == 16)
        Weapon_Adjust = float3(0.0,0.0,0.0);          //WP 14 | Game
    if (WP == 17)
        Weapon_Adjust = float3(0.0,0.0,0.0);          //WP 15 | Game
    if (WP == 18)
        Weapon_Adjust = float3(0.0,0.0,0.0);          //WP 16 | Game
    if (WP == 19)
        Weapon_Adjust = float3(0.0,0.0,0.0);          //WP 17 | Game
    if (WP == 20)
        Weapon_Adjust = float3(0.0,0.0,0.0);          //WP 18 | Game
    if (WP == 21)
        Weapon_Adjust = float3(0.0,0.0,0.0);          //WP 19 | Game
    if (WP == 22)
        Weapon_Adjust = float3(0.0,0.0,0.0);          //WP 20 | Game
    if (WP == 23)
        Weapon_Adjust = float3(0.0,0.0,0.0);          //WP 21 | Game
    if (WP == 24)
        Weapon_Adjust = float3(0.0,0.0,0.0);          //WP 21 | Game
    if (WP == 25)
        Weapon_Adjust = float3(0.0,0.0,0.0);          //WP 23 | Game
    if (WP == 26)
        Weapon_Adjust = float3(0.0,0.0,0.0);          //WP 24 | Game
    if (WP == 27)
        Weapon_Adjust = float3(0.0,0.0,0.0);          //WP 25 | Game
    if (WP == 28)
        Weapon_Adjust = float3(0.0,0.0,0.0);          //WP 26 | Game
    if (WP == 29)
        Weapon_Adjust = float3(0.0,0.0,0.0);          //WP 27 | Game
    if (WP == 30)
        Weapon_Adjust = float3(0.0,0.0,0.0);          //WP 28 | Game
    if (WP == 31)
        Weapon_Adjust = float3(0.0,0.0,0.0);          //WP 29 | Game
    if (WP == 32)
        Weapon_Adjust = float3(0.0,0.0,0.0);          //WP 30 | Game
    if (WP == 33)
        Weapon_Adjust = float3(0.0,0.0,0.0);          //WP 31 | Game
    if (WP == 34)
        Weapon_Adjust = float3(0.0,0.0,0.0);          //WP 32 | Game
    if (WP == 35)
        Weapon_Adjust = float3(0.0,0.0,0.0);          //WP 33 | Game
    if (WP == 36)
        Weapon_Adjust = float3(0.0,0.0,0.0);          //WP 34 | Game
    if (WP == 37)
        Weapon_Adjust = float3(0.0,0.0,0.0);          //WP 35 | Game
    if (WP == 38)
        Weapon_Adjust = float3(0.0,0.0,0.0);          //WP 36 | Game
    if (WP == 39)
        Weapon_Adjust = float3(0.0,0.0,0.0);          //WP 37 | Game
    if (WP == 40)
        Weapon_Adjust = float3(0.0,0.0,0.0);          //WP 38 | Game
    if (WP == 41)
        Weapon_Adjust = float3(0.0,0.0,0.0);          //WP 39 | Game
    if (WP == 42)
        Weapon_Adjust = float3(0.0,0.0,0.0);          //WP 40 | Game
    if (WP == 43)
        Weapon_Adjust = float3(0.0,0.0,0.0);          //WP 41 | Game
    if (WP == 44)
        Weapon_Adjust = float3(0.0,0.0,0.0);          //WP 42 | Game
    if (WP == 45)
        Weapon_Adjust = float3(0.0,0.0,0.0);          //WP 43 | Game
    if (WP == 46)
        Weapon_Adjust = float3(0.0,0.0,0.0);          //WP 44 | Game
    if (WP == 47)
        Weapon_Adjust = float3(0.0,0.0,0.0);          //WP 45 | Game
    if (WP == 48)
        Weapon_Adjust = float3(0.0,0.0,0.0);          //WP 46 | Game
    if (WP == 49)
        Weapon_Adjust = float3(0.0,0.0,0.0);          //WP 47 | Game
    if (WP == 50)
        Weapon_Adjust = float3(0.0,0.0,0.0);          //WP 48 | Game
    if (WP == 51)
        Weapon_Adjust = float3(0.0,0.0,0.0);          //WP 49 | Game
    if (WP == 52)
        Weapon_Adjust = float3(0.0,0.0,0.0);          //WP 50 | Game
    if (WP == 53)
        Weapon_Adjust = float3(0.0,0.0,0.0);          //WP 51 | Game
    if (WP == 54)
        Weapon_Adjust = float3(0.0,0.0,0.0);          //WP 52 | Game
    if (WP == 55)
        Weapon_Adjust = float3(0.0,0.0,0.0);          //WP 53 | Game
    if (WP == 56)
        Weapon_Adjust = float3(0.0,0.0,0.0);          //WP 54 | Game
    if (WP == 57)
        Weapon_Adjust = float3(0.0,0.0,0.0);          //WP 55 | Game
    if (WP == 58)
        Weapon_Adjust = float3(0.0,0.0,0.0);          //WP 56 | Game
    if (WP == 59)
        Weapon_Adjust = float3(0.0,0.0,0.0);          //WP 57 | Game
    if (WP == 60)
        Weapon_Adjust = float3(0.0,0.0,0.0);          //WP 58 | Game
    if (WP == 61) //Unity Limit if using else if
        Weapon_Adjust = float3(0.0,0.0,0.0);          //WP 59 | Game
    if (WP == 62)
        Weapon_Adjust = float3(0.0,0.0,0.0);          //WP 60 | Game
    if (WP == 63)
        Weapon_Adjust = float3(0.0,0.0,0.0);          //WP 61 | Game
    if (WP == 64)
        Weapon_Adjust = float3(0.0,0.0,0.0);          //WP 62 | Game
    if (WP == 65)
        Weapon_Adjust = float3(0.0,0.0,0.0);          //WP 63 | Game
    if (WP == 66)
        Weapon_Adjust = float3(0.0,0.0,0.0);          //WP 64 | Game
    if (WP == 67)
        Weapon_Adjust = float3(0.0,0.0,0.0);          //WP 65 | Game
    if (WP == 68)
        Weapon_Adjust = float3(0.0,0.0,0.0);          //WP 66 | Game
    if (WP == 69)
        Weapon_Adjust = float3(0.0,0.0,0.0);          //WP 67 | Game
    if (WP == 70)
        Weapon_Adjust = float3(0.0,0.0,0.0);          //WP 68 | Game
    if (WP == 71)
        Weapon_Adjust = float3(0.0,0.0,0.0);          //WP 69 | Game
    if (WP == 72)
        Weapon_Adjust = float3(0.0,0.0,0.0);          //WP 70 | Game
    if (WP == 73)
        Weapon_Adjust = float3(0.0,0.0,0.0);          //WP 71 | Game
    if (WP == 74)
        Weapon_Adjust = float3(0.0,0.0,0.0);          //WP 72 | Game
    if (WP == 75)
        Weapon_Adjust = float3(0.0,0.0,0.0);          //WP 73 | Game
    if (WP == 76) //DX 9 Temp Registers Limit
        Weapon_Adjust = float3(0.0,0.0,0.0);          //WP 74 | Game

		return Weapon_Adjust;
}
#elif WSM == 3
float3 Weapon_Profiles(float WP ,float3 Weapon_Adjust) // MCC
{
	if (WP == 2)
        Weapon_Adjust = float3(0,0,0);                //WP 0  | Halo: Reach
    if (WP == 3)
        Weapon_Adjust = float3(1.5,26.25,0.2);        //WP 1  | Halo: CE Anniversary
    if (WP == 4)
        Weapon_Adjust = float3(0.615,70.0,0.3955);    //WP 2  | Halo 2: Anniversary
    if (WP == 5)
        Weapon_Adjust = float3(5.750,24.0,0);         //WP 3  | Halo 3
    if (WP == 6)
        Weapon_Adjust = float3(0,0,0);                //WP 4  | Halo 3: ODST
    if (WP == 7)
        Weapon_Adjust = float3(0,0,0);                //WP 5  | Halo 4

		return Weapon_Adjust;
}
#elif WSM == 4
float3 Weapon_Profiles(float WP ,float3 Weapon_Adjust) // Prey 2017
{
	if (WP == 2)
		Weapon_Adjust = float3(0.2832,31.25,0.8775); //WP 0 | Prey 2017 High Settings and <
	if (WP == 3)
		Weapon_Adjust = float3(0.2832,31.25,0.91875);//WP 1 | Prey 2017 Very High

	return Weapon_Adjust;
}
#elif WSM == 5
float3 Weapon_Profiles(float WP ,float3 Weapon_Adjust) // Blood 2
{
    if (WP == 2)
        Weapon_Adjust = float3(0.4213,5.0,0.5);        //WP 0 | Blood 2 All Weapons
    if (WP == 3)
        Weapon_Adjust = float3(0.484,5.0,0.5);         //WP 1 | Blood 2 Bonus weapons
    if (WP == 4)
        Weapon_Adjust = float3(0.4213,5.0,0.8);        //WP 2 | Blood 2 Former

	return Weapon_Adjust;
}
#endif
