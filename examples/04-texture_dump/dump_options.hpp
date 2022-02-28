#pragma once

/// CONSTANTS
// You shouldn't need to modify these if you're just editing options
#define DUMP_HASH_FULL 0
#define DUMP_HASH_TEXMOD 1

#define DUMP_FMT_BMP 0
#define DUMP_FMT_PNG 1


/// OPTIONS

// Which hash method to use
// Only supports FULL and TEXMOD
#define DUMP_HASH DUMP_HASH_TEXMOD

// The subdirectory to place textures in
// This directory should exist in the same directory as Reshade and the game's executable
#define DUMP_DIR "texdump"

// Which texture format to output
// Only supports PNG and BMP
#define DUMP_FMT DUMP_FMT_PNG

// This allows the addon to skip any textures it already dumped this session
// This may reduce lag, but will increase memory usage
// Comment out the line below to disable
#define DUMP_ENABLE_HASH_SET

