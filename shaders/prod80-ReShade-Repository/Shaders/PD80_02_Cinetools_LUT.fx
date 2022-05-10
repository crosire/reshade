// Easy LUT config
// Name which will display in the UI. Should be without spaces
#define PD80_Technique_Name     prod80_02_Cinetools_LUT

// Texture name which contains the LUT(s) and the Tile Sizes, Amounts, etc.
#define PD80_LUT_File_Name      "pd80_cinelut.png"
#define PD80_Tile_SizeXY        64
#define PD80_Tile_Amount        64
#define PD80_LUT_Amount         31

// Drop down menu which gives the names of the LUTs, each menu option should be followed by \0
#define PD80_Drop_Down_Menu     "FilmicGold\0FilmicGold_Contrast\0FilmicBlue\0FilmicBlue_Contrast\0TealOrangeNeutral\0TealOrangeYCSplit\0TealOrangeWarmMatte\0CinematicColors\0UltraWarmMatte\0UltraMatte\0BW-Max\0BW-MaxSepia\0BW-MatteLooks\0AlternativeProcess-01\0AlternativeProcess-02\0SuperGreens\0ColorHarmony-01\0ColorHarmony-02\0GoingBrownAgain\0Cyanolyte\0DustyOldMan\0DustyOldMatte\0DrankAllTheRedWine\0OldEleganceClean\0OldEleganceMatte\0HundredDollarStripper\0HundredDollarStripperMatte\0ClassicTealOrange\0ClassicTealOrangeMatte\0ClassicTealOrangeYHL\0ClassicTealOrangeYHLMatte\0"

// Final pass to the shader
#include "PD80_LUT_v2.fxh"