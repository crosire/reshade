/*
 * Copyright (C) 2021 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause OR MIT
 */

// The subdirectory to read textures from
#define REPLACE_DIR "texreplace"

// The hash method to use
#define REPLACE_HASH REPLACE_HASH_TEXMOD

// The image file format to read
#define REPLACE_FORMAT REPLACE_FORMAT_PNG

// --- END OPTIONS --- //

#define REPLACE_HASH_FULL 0
#define REPLACE_HASH_TEXMOD 1

#define REPLACE_FORMAT_BMP 0
#define REPLACE_FORMAT_PNG 1

#define STB_IMAGE_IMPLEMENTATION

#include <reshade.hpp>
#include "crc32_hash.hpp"
#include <vector>
#include <filesystem>
#include <stb_image.h>

using namespace reshade::api;

bool replace_texture(const resource_desc &desc, subresource_data &data, std::vector<std::vector<uint8_t>> &data_to_delete)
{
	if (desc.texture.format != format::r8g8b8a8_typeless && desc.texture.format != format::r8g8b8a8_unorm && desc.texture.format != format::r8g8b8a8_unorm_srgb &&
		desc.texture.format != format::b8g8r8a8_typeless && desc.texture.format != format::b8g8r8a8_unorm && desc.texture.format != format::b8g8r8a8_unorm_srgb &&
		desc.texture.format != format::r8g8b8x8_typeless && desc.texture.format != format::r8g8b8x8_unorm && desc.texture.format != format::r8g8b8x8_unorm_srgb &&
		desc.texture.format != format::b8g8r8x8_typeless && desc.texture.format != format::b8g8r8x8_unorm && desc.texture.format != format::b8g8r8x8_unorm_srgb)
		return false;

#if REPLACE_HASH == REPLACE_HASH_FULL
	// Correct hash calculation using entire resource data
	const uint32_t hash = compute_crc32(
		static_cast<const uint8_t *>(data.data),
		format_slice_pitch(desc.texture.format, data.row_pitch, desc.texture.height));
#elif REPLACE_HASH == REPLACE_HASH_TEXMOD
	// Behavior of the original TexMod (see https://github.com/codemasher/texmod/blob/master/uMod_DX9/uMod_TextureFunction.cpp#L41)
	const uint32_t hash = ~compute_crc32(
		static_cast<const uint8_t *>(data.data),
		desc.texture.height * (
			(desc.texture.format >= format::bc1_typeless && desc.texture.format <= format::bc1_unorm_srgb) || (desc.texture.format >= format::bc4_typeless && desc.texture.format <= format::bc4_snorm) ? (desc.texture.width * 4) / 8 :
			(desc.texture.format >= format::bc2_typeless && desc.texture.format <= format::bc2_unorm_srgb) || (desc.texture.format >= format::bc3_typeless && desc.texture.format <= format::bc3_unorm_srgb) || (desc.texture.format >= format::bc5_typeless && desc.texture.format <= format::bc7_unorm_srgb) ? desc.texture.width :
			format_row_pitch(desc.texture.format, desc.texture.width)));
#endif

	// Prepend executable file name to image files
	wchar_t file_prefix[MAX_PATH] = L"";
	GetModuleFileNameW(nullptr, file_prefix, ARRAYSIZE(file_prefix));

	std::filesystem::path replace_path = file_prefix;
	replace_path  = replace_path.parent_path();
	replace_path /= REPLACE_DIR;

	wchar_t hash_string[11];
	swprintf_s(hash_string, L"0x%08X", hash);

	replace_path /= hash_string;

#if REPLACE_FORMAT == REPLACE_FORMAT_BMP
	replace_path += L".bmp";
#elif REPLACE_FORMAT == REPLACE_FORMAT_PNG
	replace_path += L".png";
#endif

	// Check if a replacement file for this texture hash exists and if so, overwrite the texture data with its contents
	if (std::filesystem::exists(replace_path))
	{
		int width = 0, height = 0, channels = 0;
		stbi_uc *const texture_data = stbi_load(replace_path.u8string().c_str(), &width, &height, &channels, STBI_rgb_alpha);
		if (texture_data != nullptr)
		{
			// Only support changing pixel data, but not texture dimensions
			if (desc.texture.width != static_cast<uint32_t>(width) ||
				desc.texture.height != static_cast<uint32_t>(height))
			{
				stbi_image_free(texture_data);
				return false;
			}

			data_to_delete.emplace_back(texture_data, texture_data + width * height * 4);

			stbi_image_free(texture_data);

			data.data = data_to_delete.back().data();
			data.row_pitch = 4 * width;
			data.slice_pitch = data.row_pitch * height;
			return true;
		}
	}

	return false;
}
