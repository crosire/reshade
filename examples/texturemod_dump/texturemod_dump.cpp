/*
 * Copyright (C) 2021 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include <imgui.h> // Include here as well, so that 'register_addon' initializes the Dear ImGui function table
#include "reshade.hpp"
#include "crc32_hash.hpp"
#include <vector>
#include <filesystem>
#include <stb_image_write.h>

#ifdef BUILTIN_ADDON
#include "ini_file.hpp"
#endif

using namespace reshade::api;

void dump_texture(const resource_desc &desc, const subresource_data &data, bool force_dump = false)
{
	if (desc.texture.height <= 8 || (desc.texture.width == 128 && desc.texture.height == 32))
		return; // Filter out small textures, which are commonly just lookup tables that are not interesting to save

	const bool rgba_format = desc.texture.format == format::r8g8b8a8_unorm || desc.texture.format == format::r8g8b8a8_unorm_srgb || desc.texture.format == format::r8g8b8a8_typeless || desc.texture.format == format::b8g8r8a8_unorm;
	const bool rgbx_format = desc.texture.format == format::r8g8b8x8_unorm || desc.texture.format == format::r8g8b8x8_unorm_srgb || desc.texture.format == format::r8g8b8x8_typeless || desc.texture.format == format::b8g8r8x8_unorm;
	if (!rgba_format && !rgbx_format)
		return;

	size_t bpp = format_bytes_per_pixel(desc.texture.format);
	size_t row_pitch = bpp * desc.texture.width;
	if (data.row_pitch < row_pitch && rgbx_format)
		row_pitch = 3 * desc.texture.width;

	// TODO: Handle row pitch properly
	const size_t size = row_pitch * desc.texture.height;
	const uint8_t *pixels = static_cast<const uint8_t *>(data.data);

	bool dump = false;
	std::filesystem::path dump_path;

#ifdef BUILTIN_ADDON
	ini_file &config = reshade::global_config();
	config.get("TEXTURE", "DumpAll", dump);
	config.get("TEXTURE", "DumpPath", dump_path);
#else
	dump = true;
#endif

	if (dump || force_dump)
	{
		const uint32_t hash = compute_crc32(pixels, size);

		char hash_string[11];
		sprintf_s(hash_string, "0x%08x", hash);

		dump_path /= L"texture_";
		dump_path += hash_string;
		dump_path += L".bmp";

		assert(bpp <= 4);
		stbi_write_bmp(dump_path.u8string().c_str(), desc.texture.width, desc.texture.height, static_cast<int>(bpp), pixels);
	}
}

static void on_init_texture(device *device, const resource_desc &desc, const subresource_data *initial_data, resource_usage, resource)
{
	if (desc.type != resource_type::texture_2d)
		return;
	if (device->get_api() != device_api::opengl && (desc.usage & (resource_usage::shader_resource | resource_usage::depth_stencil | resource_usage::render_target)) != resource_usage::shader_resource)
		return;

	if (initial_data != nullptr)
		dump_texture(desc, *initial_data);
}

static bool on_copy_texture(command_list *cmd_list, resource src, uint32_t src_subresource, const int32_t /*src_box*/[6], resource dst, uint32_t dst_subresource, const int32_t dst_box[6], filter_mode)
{
	if (src_subresource != 0 || src_subresource != dst_subresource)
		return false;

	device *const device = cmd_list->get_device();

	const resource_desc src_desc = device->get_resource_desc(src);
	if (src_desc.heap != memory_heap::cpu_to_gpu)
		return false;

	const resource_desc dst_desc = device->get_resource_desc(dst);
	if ((dst_desc.usage & resource_usage::shader_resource) == resource_usage::undefined)
		return false;

	if (dst_box != nullptr && (
		static_cast<uint32_t>(dst_box[3] - dst_box[0]) != dst_desc.texture.width ||
		static_cast<uint32_t>(dst_box[4] - dst_box[1]) != dst_desc.texture.height ||
		static_cast<uint32_t>(dst_box[5] - dst_box[2]) != dst_desc.texture.depth_or_layers))
		return false;

	if (subresource_data mapped_data;
		device->map_resource(src, src_subresource, map_access::read_only, &mapped_data))
	{
		dump_texture(dst_desc, mapped_data);

		device->unmap_resource(src, src_subresource);
	}

	return false;
}
static bool on_update_texture(device *device, const subresource_data &data, resource dst, uint32_t dst_subresource, const int32_t dst_box[6])
{
	if (dst_subresource != 0)
		return false;

	const resource_desc dst_desc = device->get_resource_desc(dst);
	if ((dst_desc.usage & resource_usage::shader_resource) == resource_usage::undefined)
		return false;

	if (dst_box != nullptr && (
		static_cast<uint32_t>(dst_box[3] - dst_box[0]) != dst_desc.texture.width ||
		static_cast<uint32_t>(dst_box[4] - dst_box[1]) != dst_desc.texture.height ||
		static_cast<uint32_t>(dst_box[5] - dst_box[2]) != dst_desc.texture.depth_or_layers))
		return false;

	dump_texture(dst_desc, data);

	return false;
}

void register_addon_texmod_dump()
{
	reshade::register_event<reshade::addon_event::init_resource>(on_init_texture);

	reshade::register_event<reshade::addon_event::copy_texture_region>(on_copy_texture);
	reshade::register_event<reshade::addon_event::update_texture_region>(on_update_texture);
}
void unregister_addon_texmod_dump()
{
	reshade::unregister_event<reshade::addon_event::init_resource>(on_init_texture);

	reshade::unregister_event<reshade::addon_event::copy_texture_region>(on_copy_texture);
	reshade::unregister_event<reshade::addon_event::update_texture_region>(on_update_texture);
}

#ifdef _WINDLL

extern void register_addon_texmod_overlay(); // See implementation in 'texturemod_overlay.cpp'
extern void unregister_addon_texmod_overlay();

extern "C" __declspec(dllexport) const char *NAME = "TextureMod Dump";
extern "C" __declspec(dllexport) const char *DESCRIPTION = "Example add-on that dumps all textures used by the application to disk.";

BOOL APIENTRY DllMain(HMODULE hModule, DWORD fdwReason, LPVOID)
{
	switch (fdwReason)
	{
	case DLL_PROCESS_ATTACH:
		if (!reshade::register_addon(hModule))
			return FALSE;
		register_addon_texmod_dump();
		register_addon_texmod_overlay();
		break;
	case DLL_PROCESS_DETACH:
		unregister_addon_texmod_overlay();
		unregister_addon_texmod_dump();
		reshade::unregister_addon(hModule);
		break;
	}

	return TRUE;
}

#endif
