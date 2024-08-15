/*
 * Copyright (C) 2021 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause OR MIT
 */

#define STB_IMAGE_IMPLEMENTATION

#include <reshade.hpp>
#include "config.hpp"
#include "crc32_hash.hpp"
#include <vector>
#include <filesystem>
#include <stb_image.h>

using namespace reshade::api;

bool load_texture_image(const resource_desc &desc, subresource_data &data, std::vector<std::vector<uint8_t>> &data_to_delete)
{
#if RESHADE_ADDON_TEXTURE_LOAD_HASH_TEXMOD
	// Behavior of the original TexMod (see https://github.com/codemasher/texmod/blob/master/uMod_DX9/uMod_TextureFunction.cpp#L41)
	const uint32_t hash = ~compute_crc32(
		static_cast<const uint8_t *>(data.data),
		desc.texture.height * static_cast<size_t>(
			(desc.texture.format >= format::bc1_typeless && desc.texture.format <= format::bc1_unorm_srgb) || (desc.texture.format >= format::bc4_typeless && desc.texture.format <= format::bc4_snorm) ? (desc.texture.width * 4) / 8 :
			(desc.texture.format >= format::bc2_typeless && desc.texture.format <= format::bc2_unorm_srgb) || (desc.texture.format >= format::bc3_typeless && desc.texture.format <= format::bc3_unorm_srgb) || (desc.texture.format >= format::bc5_typeless && desc.texture.format <= format::bc7_unorm_srgb) ? desc.texture.width :
			format_row_pitch(desc.texture.format, desc.texture.width)));
#else
	// Correct hash calculation using entire resource data
	const uint32_t hash = compute_crc32(
		static_cast<const uint8_t *>(data.data),
		format_slice_pitch(desc.texture.format, data.row_pitch, desc.texture.height));
#endif

	// Prepend executable directory to image files
	wchar_t file_prefix[MAX_PATH] = L"";
	GetModuleFileNameW(nullptr, file_prefix, ARRAYSIZE(file_prefix));

	std::filesystem::path replace_path = file_prefix;
	replace_path  = replace_path.parent_path();
	replace_path /= RESHADE_ADDON_TEXTURE_LOAD_DIR;

	wchar_t hash_string[11];
	swprintf_s(hash_string, L"0x%08X", hash);

	replace_path /= hash_string;
	replace_path += RESHADE_ADDON_TEXTURE_LOAD_FORMAT;

	// Check if a replacement file for this texture hash exists and if so, overwrite the texture data with its contents
	if (!std::filesystem::exists(replace_path))
		return false;

	int width = 0, height = 0, channels = 0;
	stbi_uc *const rgba_pixel_data_p = stbi_load(replace_path.u8string().c_str(), &width, &height, &channels, STBI_rgb_alpha);
	if (rgba_pixel_data_p == nullptr)
		return false;

	std::vector<uint8_t> pixel_data(rgba_pixel_data_p, rgba_pixel_data_p + static_cast<size_t>(width) * static_cast<size_t>(height) * 4);

	stbi_image_free(rgba_pixel_data_p);

	// Only support changing pixel data, but not texture dimensions
	if (desc.texture.width != static_cast<uint32_t>(width) ||
		desc.texture.height != static_cast<uint32_t>(height))
	{
		reshade::log::message(reshade::log::level::error, "Failed to replace texture data because dimensions do not match!");
		return false;
	}

	switch (desc.texture.format)
	{
	case format::l8_unorm:
	case format::a8_unorm:
	case format::r8_typeless:
	case format::r8_unorm:
	case format::r8_snorm:
		for (size_t y = 0; y < static_cast<size_t>(height); ++y)
		{
			for (size_t x = 0; x < static_cast<size_t>(width); ++x)
			{
				const uint8_t *const src = pixel_data.data() + (y * width + x) * 4;
				uint8_t *const dst = pixel_data.data() + (y * width + x);

				dst[0] = src[0];
			}
		}
		pixel_data.resize(static_cast<size_t>(width) * static_cast<size_t>(height));
		data.data = pixel_data.data();
		data.row_pitch = width;
		data.slice_pitch = data.row_pitch * height;
		break;
	case format::l8a8_unorm:
	case format::r8g8_typeless:
	case format::r8g8_unorm:
	case format::r8g8_snorm:
		for (size_t y = 0; y < static_cast<size_t>(height); ++y)
		{
			for (size_t x = 0; x < static_cast<size_t>(width); ++x)
			{
				const uint8_t *const src = pixel_data.data() + (y * width + x) * 4;
				uint8_t *const dst = pixel_data.data() + (y * width + x) * 2;

				dst[0] = src[0];
				dst[1] = src[1];
			}
		}
		pixel_data.resize(static_cast<size_t>(width) * static_cast<size_t>(height) * 2);
		data.data = pixel_data.data();
		data.row_pitch = width * 2;
		data.slice_pitch = data.row_pitch * height;
		break;
	case format::r8g8b8a8_typeless:
	case format::r8g8b8a8_unorm:
	case format::r8g8b8a8_unorm_srgb:
	case format::r8g8b8x8_unorm:
	case format::r8g8b8x8_unorm_srgb:
		data.data = pixel_data.data();
		data.row_pitch = width * 4;
		data.slice_pitch = data.row_pitch * height;
		break;
	case format::b8g8r8a8_typeless:
	case format::b8g8r8a8_unorm:
	case format::b8g8r8a8_unorm_srgb:
	case format::b8g8r8x8_typeless:
	case format::b8g8r8x8_unorm:
	case format::b8g8r8x8_unorm_srgb:
		for (size_t y = 0; y < static_cast<size_t>(height); ++y)
		{
			for (size_t x = 0; x < static_cast<size_t>(width); ++x)
			{
				uint8_t *const dst = pixel_data.data() + (y * width + x) * 4;

				// Swap red and blue channel
				std::swap(dst[0], dst[2]);
			}
		}
		data.data = pixel_data.data();
		data.row_pitch = width * 4;
		data.slice_pitch = data.row_pitch * height;
		break;
	default:
		// Unsupported format
		reshade::log::message(reshade::log::level::error, "Failed to replace texture data because format is not supported!");
		return false;
	}

	data_to_delete.push_back(std::move(pixel_data));

	return true;
}
