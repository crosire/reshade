#pragma once

/// CONSTANTS
// You shouldn't need to modify these if you're just editing options
#define REPLACE_HASH_FULL 0
#define REPLACE_HASH_TEXMOD 1

#define REPLACE_FMT_BMP 0
#define REPLACE_FMT_PNG 1


/// OPTIONS

// Which hash method to use
// Only supports FULL and TEXMOD
#define REPLACE_HASH REPLACE_HASH_TEXMOD

// The subdirectory to load textures from
// This directory should exist in the same directory as the executable
#define REPLACE_DIR "texreplace"

// Which texture format to read
// Only supports PNG and BMP
#define REPLACE_FMT REPLACE_FMT_PNG

