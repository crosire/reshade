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

static thread_local std::vector<std::vector<uint8_t>> data_to_delete;

static bool replace_texture(const resource_desc &desc, subresource_data &data)
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

	// Prepend executable file name to image files
	WCHAR file_prefix[MAX_PATH] = L"";
	GetModuleFileNameW(nullptr, file_prefix, ARRAYSIZE(file_prefix));

	std::filesystem::path replace_path = file_prefix;
	replace_path += L'_';
	replace_path += hash_string;
	replace_path += L".bmp";

	// Check if a replacement file for this texture hash exists and if so, overwrite the texture data with its contents
	if (std::filesystem::exists(replace_path))
	{
		std::ifstream file(replace_path, std::ios::binary);
		file.seekg(0, std::ios::end);
		std::vector<uint8_t> file_data(static_cast<size_t>(file.tellg()));
		file.seekg(0, std::ios::beg).read(reinterpret_cast<char *>(file_data.data()), file_data.size());

		int width = 0, height = 0, channels = 0;
		stbi_uc *const texture_data = stbi_load_from_memory(file_data.data(), static_cast<int>(file_data.size()), &width, &height, &channels, STBI_rgb_alpha);
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

static inline bool filter_texture(device *device, const resource_desc &desc, const subresource_box *box)
{
	if (desc.type != resource_type::texture_2d || (desc.usage & resource_usage::shader_resource) == resource_usage::undefined || (desc.heap != memory_heap::unknown && desc.heap != memory_heap::gpu_only) || (desc.flags & resource_flags::dynamic) == resource_flags::dynamic)
		return false; // Ignore resources that are not static 2D textures that can be used as shader input

	if (device->get_api() != device_api::opengl && (desc.usage & (resource_usage::shader_resource | resource_usage::depth_stencil | resource_usage::render_target)) != resource_usage::shader_resource)
		return false; // Ignore resources that can be used as render targets (except in OpenGL, since all textures have the render target usage flag there)

	if (box != nullptr && (
		static_cast<uint32_t>(box->right - box->left) != desc.texture.width ||
		static_cast<uint32_t>(box->bottom - box->top) != desc.texture.height ||
		static_cast<uint32_t>(box->back - box->front) != desc.texture.depth_or_layers))
		return false; // Ignore updates that do not update the entire texture

	if (desc.texture.samples != 1)
		return false;

	return true;
}

static bool on_create_texture(device *device, resource_desc &desc, subresource_data *initial_data, resource_usage)
{
	if (!filter_texture(device, desc, nullptr))
		return false;

	return initial_data != nullptr && replace_texture(desc, *initial_data);
}
static void on_after_create_texture(device *, const resource_desc &, const subresource_data *, resource_usage, resource)
{
	// Free the memory allocated in 'replace_texture' above
	data_to_delete.clear();
}

static bool on_copy_texture(command_list *cmd_list, resource src, uint32_t src_subresource, const subresource_box *, resource dst, uint32_t dst_subresource, const subresource_box *dst_box, filter_mode)
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

		// Free the memory allocated in 'replace_texture' above
		data_to_delete.clear();

		return true; // Texture was already updated now, so skip the original copy command from the application
	}

	return false;
}
static bool on_update_texture(device *device, const subresource_data &data, resource dst, uint32_t dst_subresource, const subresource_box *dst_box)
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

		// Free the memory allocated in 'replace_texture' above
		data_to_delete.clear();

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

static void on_map_texture(device *device, resource resource, uint32_t subresource, const subresource_box *box, map_access access, subresource_data *data)
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

		// Free the memory allocated in 'replace_texture' above
		data_to_delete.clear();
	}
}

extern "C" __declspec(dllexport) const char *NAME = "TextureMod Replace";
extern "C" __declspec(dllexport) const char *DESCRIPTION = "Example add-on that replaces textures before they are used by the application with image files from disk.";

BOOL APIENTRY DllMain(HMODULE hModule, DWORD fdwReason, LPVOID)
{
	switch (fdwReason)
	{
	case DLL_PROCESS_ATTACH:
		if (!reshade::register_addon(hModule))
			return FALSE;
		reshade::register_event<reshade::addon_event::create_resource>(on_create_texture);
		reshade::register_event<reshade::addon_event::init_resource>(on_after_create_texture);
		reshade::register_event<reshade::addon_event::copy_texture_region>(on_copy_texture);
		reshade::register_event<reshade::addon_event::update_texture_region>(on_update_texture);
		reshade::register_event<reshade::addon_event::map_texture_region>(on_map_texture);
		reshade::register_event<reshade::addon_event::unmap_texture_region>(on_unmap_texture);
		break;
	case DLL_PROCESS_DETACH:
		reshade::unregister_addon(hModule);
		break;
	}

	return TRUE;
}
