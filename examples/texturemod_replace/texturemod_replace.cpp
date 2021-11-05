/*
 * Copyright (C) 2021 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#define STB_IMAGE_IMPLEMENTATION

#include <reshade.hpp>
#include "crc32_hash.hpp"
#include <fstream>
#include <filesystem>
#include <stb_image.h>

using namespace reshade::api;

static bool replace_texture(const resource_desc &desc, subresource_data &data, const std::filesystem::path &file_prefix)
{
	if (desc.texture.format != format::r8g8b8a8_typeless && desc.texture.format != format::r8g8b8a8_unorm && desc.texture.format != format::r8g8b8a8_unorm_srgb &&
		desc.texture.format != format::b8g8r8a8_typeless && desc.texture.format != format::b8g8r8a8_unorm && desc.texture.format != format::b8g8r8a8_unorm_srgb &&
		desc.texture.format != format::r8g8b8x8_typeless && desc.texture.format != format::r8g8b8x8_unorm && desc.texture.format != format::r8g8b8x8_unorm_srgb &&
		desc.texture.format != format::b8g8r8x8_typeless && desc.texture.format != format::b8g8r8x8_unorm && desc.texture.format != format::b8g8r8x8_unorm_srgb)
		return false;

#if 0
	// Correct hash calculation using entire resource data
	const uint32_t hash = compute_crc32(
		static_cast<const uint8_t *>(data.data),
		format_slice_pitch(desc.texture.format, data.row_pitch, desc.texture.height));
#else
	// Behavior of the original TexMod (see https://github.com/codemasher/texmod/blob/master/uMod_DX9/uMod_TextureFunction.cpp#L41)
	const uint32_t hash = ~compute_crc32(
		static_cast<const uint8_t *>(data.data),
		desc.texture.height * (
			(desc.texture.format >= format::bc1_typeless && desc.texture.format <= format::bc1_unorm_srgb) || (desc.texture.format >= format::bc4_typeless && desc.texture.format <= format::bc4_snorm) ? (desc.texture.width * 4) / 8 :
			(desc.texture.format >= format::bc2_typeless && desc.texture.format <= format::bc2_unorm_srgb) || (desc.texture.format >= format::bc3_typeless && desc.texture.format <= format::bc3_unorm_srgb) || (desc.texture.format >= format::bc5_typeless && desc.texture.format <= format::bc7_unorm_srgb) ? desc.texture.width :
			format_row_pitch(desc.texture.format, desc.texture.width)));
#endif

	char hash_string[11];
	sprintf_s(hash_string, "0x%08X", hash);

	std::filesystem::path replace_path = file_prefix;
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

	return false;
}

static bool replace_texture(const resource_desc &desc, subresource_data &data)
{
	// Prepend executable file name to image files
	WCHAR file_prefix[MAX_PATH] = {};
	GetModuleFileNameW(nullptr, file_prefix, ARRAYSIZE(file_prefix));

	return replace_texture(desc, data, std::wstring(file_prefix) + L'_');
}

static inline bool filter_texture(device *device, const resource_desc &desc, const int32_t box[6])
{
	if (desc.type != resource_type::texture_2d || (desc.usage & resource_usage::shader_resource) == resource_usage::undefined || (desc.heap != memory_heap::unknown && desc.heap != memory_heap::gpu_only) || (desc.flags & resource_flags::dynamic) == resource_flags::dynamic)
		return false; // Ignore resources that are not static 2D textures that can be used as shader input

	if (device->get_api() != device_api::opengl && (desc.usage & (resource_usage::shader_resource | resource_usage::depth_stencil | resource_usage::render_target)) != resource_usage::shader_resource)
		return false; // Ignore resources that can be used as render targets (except in OpenGL, since all textures have the render target usage flag there)

	if (box != nullptr && (
		static_cast<uint32_t>(box[3] - box[0]) != desc.texture.width ||
		static_cast<uint32_t>(box[4] - box[1]) != desc.texture.height ||
		static_cast<uint32_t>(box[5] - box[2]) != desc.texture.depth_or_layers))
		return false; // Ignore updates that do not update the entire texture

	if (desc.texture.samples != 1)
		return false;

	return true;
}

static thread_local bool replaced_last_texture = false;

static bool on_create_texture(device *device, resource_desc &desc, subresource_data *initial_data, resource_usage)
{
	if (!filter_texture(device, desc, nullptr))
		return false;

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
	if (!filter_texture(device, dst_desc, dst_box))
		return false;

	bool replace = false;

	subresource_data new_data;
	if (device->map_texture_region(src, src_subresource, nullptr, map_access::read_only, &new_data))
	{
		replace = replace_texture(dst_desc, new_data);

		device->unmap_texture_region(src, src_subresource);
	}

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
	if (!filter_texture(device, dst_desc, dst_box))
		return false;

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

// Keep track of current resource between 'map_resource' and 'unmap_resource' event invocations
static thread_local struct {
	resource res = { 0 };
	resource_desc desc;
	subresource_data data;
} s_current_mapping;

static void on_map_texture(device *device, resource resource, uint32_t subresource, const int32_t box[6], map_access access, subresource_data *data)
{
	if (subresource != 0 || access == map_access::read_only || data == nullptr)
		return;

	const resource_desc desc = device->get_resource_desc(resource);
	if (!filter_texture(device, desc, box))
		return;

	s_current_mapping.res = resource;
	s_current_mapping.desc = desc;
	s_current_mapping.data = *data;
}
static void on_unmap_texture(device *, resource resource, uint32_t subresource)
{
	if (subresource != 0 || resource != s_current_mapping.res)
		return;

	s_current_mapping.res = { 0 };

	void *mapped_data = s_current_mapping.data.data;

	if (replace_texture(s_current_mapping.desc, s_current_mapping.data))
	{
		std::memcpy(mapped_data, s_current_mapping.data.data, s_current_mapping.data.slice_pitch);

		stbi_image_free(s_current_mapping.data.data);
	}
}

void register_addon_texmod_replace()
{
	reshade::register_event<reshade::addon_event::create_resource>(on_create_texture);
	reshade::register_event<reshade::addon_event::init_resource>(on_after_create_texture);

	reshade::register_event<reshade::addon_event::copy_texture_region>(on_copy_texture);
	reshade::register_event<reshade::addon_event::update_texture_region>(on_update_texture);
	reshade::register_event<reshade::addon_event::map_texture_region>(on_map_texture);
	reshade::register_event<reshade::addon_event::unmap_texture_region>(on_unmap_texture);
}
void unregister_addon_texmod_replace()
{
	reshade::unregister_event<reshade::addon_event::create_resource>(on_create_texture);
	reshade::unregister_event<reshade::addon_event::init_resource>(on_after_create_texture);

	reshade::unregister_event<reshade::addon_event::copy_texture_region>(on_copy_texture);
	reshade::unregister_event<reshade::addon_event::update_texture_region>(on_update_texture);
	reshade::unregister_event<reshade::addon_event::map_texture_region>(on_map_texture);
	reshade::unregister_event<reshade::addon_event::unmap_texture_region>(on_unmap_texture);
}

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
