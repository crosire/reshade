/*
 * Copyright (C) 2021 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "reshade.hpp"
#include "crc32_hash.hpp"
#include <fstream>
#include <filesystem>
#include <stb_image.h>

#ifdef BUILTIN_ADDON
#include "ini_file.hpp"
#endif

using namespace reshade::api;

static bool replace_texture(const resource_desc &desc, subresource_data &data)
{
	const bool rgba_format = desc.texture.format == format::r8g8b8a8_unorm || desc.texture.format == format::r8g8b8a8_unorm_srgb || desc.texture.format == format::r8g8b8a8_typeless || desc.texture.format == format::b8g8r8a8_unorm;
	const bool rgbx_format = desc.texture.format == format::r8g8b8x8_unorm || desc.texture.format == format::r8g8b8x8_unorm_srgb || desc.texture.format == format::r8g8b8x8_typeless || desc.texture.format == format::b8g8r8x8_unorm;
	if (!rgba_format && !rgbx_format)
		return false;

	size_t bpp = format_bytes_per_pixel(desc.texture.format);
	size_t row_pitch = bpp * desc.texture.width;
	if (data.row_pitch < row_pitch && rgbx_format)
		row_pitch = 3 * desc.texture.width;

	// TODO: Handle row pitch properly
	const size_t size = row_pitch * desc.texture.height;
	const uint8_t *pixels = static_cast<const uint8_t *>(data.data);

	bool replace = false;
	std::filesystem::path replace_path;

#ifdef BUILTIN_ADDON
	ini_file &config = reshade::global_config();
	config.get("TEXTURE", "Replace", replace);
	config.get("TEXTURE", "ReplacePath", replace_path);
#else
	replace = true;
#endif

	if (replace)
	{
		const uint32_t hash = compute_crc32(pixels, size);

		char hash_string[11];
		sprintf_s(hash_string, "0x%08x", hash);

		replace_path /= L"texture_";
		replace_path += hash_string;
		replace_path += L".bmp";

		if (FILE *file; _wfopen_s(&file, replace_path.c_str(), L"rb") == 0)
		{
			// Read texture data into memory in one go since that is faster than reading chunk by chunk
			std::vector<uint8_t> mem(static_cast<size_t>(std::filesystem::file_size(replace_path)));
			fread(mem.data(), 1, mem.size(), file);
			fclose(file);

			int width = 0, height = 0, channels = 0;
			stbi_uc *const filedata = stbi_load_from_memory(mem.data(), static_cast<int>(mem.size()), &width, &height, &channels, STBI_rgb_alpha);
			if (filedata != nullptr)
			{
				// Only support changing pixel data, but not texture dimensions
				if (desc.texture.width != static_cast<uint32_t>(width) ||
					desc.texture.height != static_cast<uint32_t>(height))
				{
					stbi_image_free(filedata);
					return false;
				}

				data.data = filedata;
				data.row_pitch = 4 * width;
				data.slice_pitch = data.row_pitch * height;
				return true;
			}
		}
	}

	return false;
}

static thread_local bool replaced_last_texture = false;

static bool on_create_texture(device *device, resource_desc &desc, subresource_data *initial_data, resource_usage)
{
	if (desc.type != resource_type::texture_2d)
		return false; // Ignore resources that are not 2D textures
	if (device->get_api() != device_api::opengl && (desc.usage & (resource_usage::shader_resource | resource_usage::depth_stencil | resource_usage::render_target)) != resource_usage::shader_resource)
		return false; // Ignore textures that can be used as render targets (since this should only replace textures used as shader input)

	return replaced_last_texture = (initial_data != nullptr) && replace_texture(desc, *initial_data);
}
static void on_after_create_texture(device *, const resource_desc &, const subresource_data *initial_data, resource_usage, resource)
{
	// Free the memory allocated in 'replace_texture' above
	if (replaced_last_texture)
		stbi_image_free(initial_data->data);

	replaced_last_texture = false;
}

static bool on_copy_texture(command_list *cmd_list, resource src, uint32_t src_subresource, const int32_t /*src_box*/[6], resource dst, uint32_t dst_subresource, const int32_t dst_box[6], filter_mode)
{
	if (src_subresource != 0 || src_subresource != dst_subresource)
		return false; // Ignore copies to mipmap levels other than the base level

	device *const device = cmd_list->get_device();

	const resource_desc src_desc = device->get_resource_desc(src);
	if (src_desc.heap != memory_heap::cpu_to_gpu)
		return false; // Ignore copies that are not from a buffer in host memory

	const resource_desc dst_desc = device->get_resource_desc(dst);
	if (dst_desc.type != resource_type::texture_2d || (dst_desc.usage & resource_usage::shader_resource) == resource_usage::undefined)
		return false; // Ignore copies that do not target a 2D texture used as shader input

	if (dst_box != nullptr && (
		static_cast<uint32_t>(dst_box[3] - dst_box[0]) != dst_desc.texture.width ||
		static_cast<uint32_t>(dst_box[4] - dst_box[1]) != dst_desc.texture.height ||
		static_cast<uint32_t>(dst_box[5] - dst_box[2]) != dst_desc.texture.depth_or_layers))
		return false; // Ignore copies that do not update the entire texture

	subresource_data new_data;
	if (!device->map_resource(src, src_subresource, map_access::read_only, &new_data))
		return false;

	const bool replace = replace_texture(dst_desc, new_data);

	device->unmap_resource(src, src_subresource);

	if (replace)
	{
		// Update texture with the new data
		device->update_texture_region(new_data, dst, dst_subresource, dst_box);

		stbi_image_free(new_data.data);
		return true; // Texture was already updated now, so skip the original copy command from the application
	}

	return false;
}
static bool on_update_texture(device *device, const subresource_data &data, resource dst, uint32_t dst_subresource, const int32_t dst_box[6])
{
	if (dst_subresource != 0)
		return false; // Ignore updates to mipmap levels other than the base level

	const resource_desc dst_desc = device->get_resource_desc(dst);
	if (dst_desc.type != resource_type::texture_2d || (dst_desc.usage & resource_usage::shader_resource) == resource_usage::undefined)
		return false; // Ignore updates that do not target a 2D texture used as shader input

	if (dst_box != nullptr && (
		static_cast<uint32_t>(dst_box[3] - dst_box[0]) != dst_desc.texture.width ||
		static_cast<uint32_t>(dst_box[4] - dst_box[1]) != dst_desc.texture.height ||
		static_cast<uint32_t>(dst_box[5] - dst_box[2]) != dst_desc.texture.depth_or_layers))
		return false; // Ignore updates that do not update the entire texture

	subresource_data new_data = data;
	if (replace_texture(dst_desc, new_data))
	{
		// Update texture with the new data
		device->update_texture_region(new_data, dst, dst_subresource, dst_box);

		stbi_image_free(new_data.data);
		return true; // Texture was already updated now, so skip the original update command from the application
	}

	return false;
}

void register_addon_texmod_replace()
{
	reshade::register_event<reshade::addon_event::create_resource>(on_create_texture);
	reshade::register_event<reshade::addon_event::init_resource>(on_after_create_texture);

	reshade::register_event<reshade::addon_event::copy_texture_region>(on_copy_texture);
	reshade::register_event<reshade::addon_event::update_texture_region>(on_update_texture);
}
void unregister_addon_texmod_replace()
{
	reshade::unregister_event<reshade::addon_event::create_resource>(on_create_texture);
	reshade::unregister_event<reshade::addon_event::init_resource>(on_after_create_texture);

	reshade::unregister_event<reshade::addon_event::copy_texture_region>(on_copy_texture);
	reshade::unregister_event<reshade::addon_event::update_texture_region>(on_update_texture);
}

#ifdef _WINDLL

extern "C" __declspec(dllexport) const char *NAME = "TextureMod Replace";
extern "C" __declspec(dllexport) const char *DESCRIPTION = "Example add-on that replaces textures the application creates with image files loaded from disk.";

BOOL APIENTRY DllMain(HMODULE hModule, DWORD fdwReason, LPVOID)
{
	switch (fdwReason)
	{
	case DLL_PROCESS_ATTACH:
		if (!reshade::register_addon(hModule))
			return FALSE;
		register_addon_texmod_replace();
		break;
	case DLL_PROCESS_DETACH:
		unregister_addon_texmod_replace();
		reshade::unregister_addon(hModule);
		break;
	}

	return TRUE;
}

#endif
