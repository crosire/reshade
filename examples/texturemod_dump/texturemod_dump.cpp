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

static void unpack_r5g6b5(uint16_t data, uint8_t rgb[3])
{
	uint32_t temp;
	temp =  (data           >> 11) * 255 + 16;
	rgb[0] = static_cast<uint8_t>((temp / 32 + temp) / 32);
	temp = ((data & 0x07E0) >>  5) * 255 + 32;
	rgb[1] = static_cast<uint8_t>((temp / 64 + temp) / 64);
	temp =  (data & 0x001F)        * 255 + 16;
	rgb[2] = static_cast<uint8_t>((temp / 32 + temp) / 32);
}
static void unpack_bc1_value(const uint8_t color_0[3], const uint8_t color_1[3], uint32_t color_index, uint8_t result[4], bool not_degenerate = true)
{
	switch (color_index)
	{
	case 0:
		for (int c = 0; c < 3; ++c)
			result[c] = color_0[c];
		result[3] = 255;
		break;
	case 1:
		for (int c = 0; c < 3; ++c)
			result[c] = color_1[c];
		result[3] = 255;
		break;
	case 2:
		for (int c = 0; c < 3; ++c)
			result[c] = not_degenerate ? (2 * color_0[c] + color_1[c]) / 3 : (color_0[c] + color_1[c]) / 2;
		result[3] = 255;
		break;
	case 3:
		for (int c = 0; c < 3; ++c)
			result[c] = not_degenerate ? (color_0[c] + 2 * color_1[c]) / 3 : 0;
		result[3] = not_degenerate ? 255 : 0;
		break;
	}
}
static void unpack_bc4_value(uint8_t alpha_0, uint8_t alpha_1, uint32_t alpha_index, uint8_t *result)
{
	const bool interpolation_type = alpha_0 > alpha_1;

	switch (alpha_index)
	{
	case 0:
		*result = alpha_0;
		break;
	case 1:
		*result = alpha_1;
		break;
	case 2:
		*result = interpolation_type ? (6 * alpha_0 + 1 * alpha_1) / 7 : (4 * alpha_0 + 1 * alpha_1) / 5;
		break;
	case 3:
		*result = interpolation_type ? (5 * alpha_0 + 2 * alpha_1) / 7 : (3 * alpha_0 + 2 * alpha_1) / 5;
		break;
	case 4:
		*result = interpolation_type ? (4 * alpha_0 + 3 * alpha_1) / 7 : (2 * alpha_0 + 3 * alpha_1) / 5;
		break;
	case 5:
		*result = interpolation_type ? (3 * alpha_0 + 4 * alpha_1) / 7 : (1 * alpha_0 + 4 * alpha_1) / 5;
		break;
	case 6:
		*result = interpolation_type ? (2 * alpha_0 + 5 * alpha_1) / 7 : 0;
		break;
	case 7:
		*result = interpolation_type ? (1 * alpha_0 + 6 * alpha_1) / 7 : 255;
		break;
	}
}

bool dump_texture(const resource_desc &desc, const subresource_data &data, bool force_dump = false)
{
	if (desc.texture.height <= 8 || (desc.texture.width == 128 && desc.texture.height == 32))
		return false; // Filter out small textures, which are commonly just lookup tables that are not interesting to save

	uint8_t *data_p = static_cast<uint8_t *>(data.data);
	std::vector<uint8_t> rgba_pixel_data(desc.texture.width * desc.texture.height * 4);

	const uint32_t block_count_x = (desc.texture.width + 3) / 4;
	const uint32_t block_count_y = (desc.texture.height + 3) / 4;

	switch (desc.texture.format)
	{
	case format::l8_unorm:
		for (uint32_t y = 0; y < desc.texture.height; ++y, data_p += data.row_pitch)
		{
			for (uint32_t x = 0; x < desc.texture.width; ++x)
			{
				uint8_t *const rgba_pixel_data_p = rgba_pixel_data.data() + (y * desc.texture.width + x) * 4;
				rgba_pixel_data_p[0] = data_p[x];
				rgba_pixel_data_p[1] = data_p[x];
				rgba_pixel_data_p[2] = data_p[x];
				rgba_pixel_data_p[3] = 255;
			}
		}
		break;
	case format::a8_unorm:
		for (uint32_t y = 0; y < desc.texture.height; ++y, data_p += data.row_pitch)
		{
			for (uint32_t x = 0; x < desc.texture.width; ++x)
			{
				uint8_t *const rgba_pixel_data_p = rgba_pixel_data.data() + (y * desc.texture.width + x) * 4;
				rgba_pixel_data_p[0] = 0;
				rgba_pixel_data_p[1] = 0;
				rgba_pixel_data_p[2] = 0;
				rgba_pixel_data_p[3] = data_p[x];
			}
		}
		break;
	case format::r8_typeless:
	case format::r8_unorm:
	case format::r8_snorm:
		for (uint32_t y = 0; y < desc.texture.height; ++y, data_p += data.row_pitch)
		{
			for (uint32_t x = 0; x < desc.texture.width; ++x)
			{
				uint8_t *const rgba_pixel_data_p = rgba_pixel_data.data() + (y * desc.texture.width + x) * 4;
				rgba_pixel_data_p[0] = data_p[x];
				rgba_pixel_data_p[1] = 0;
				rgba_pixel_data_p[2] = 0;
				rgba_pixel_data_p[3] = 255;
			}
		}
		break;
	case format::l8a8_unorm:
		for (uint32_t y = 0; y < desc.texture.height; ++y, data_p += data.row_pitch)
		{
			for (uint32_t x = 0; x < desc.texture.width; ++x)
			{
				uint8_t *const rgba_pixel_data_p = rgba_pixel_data.data() + (y * desc.texture.width + x) * 4;
				rgba_pixel_data_p[0] = data_p[x * 2 + 0];
				rgba_pixel_data_p[1] = data_p[x * 2 + 0];
				rgba_pixel_data_p[2] = data_p[x * 2 + 0];
				rgba_pixel_data_p[3] = data_p[x * 2 + 1];
			}
		}
		break;
	case format::r8g8_typeless:
	case format::r8g8_unorm:
	case format::r8g8_snorm:
		for (uint32_t y = 0; y < desc.texture.height; ++y, data_p += data.row_pitch)
		{
			for (uint32_t x = 0; x < desc.texture.width; ++x)
			{
				uint8_t *const rgba_pixel_data_p = rgba_pixel_data.data() + (y * desc.texture.width + x) * 4;
				rgba_pixel_data_p[0] = data_p[x * 2 + 0];
				rgba_pixel_data_p[1] = data_p[x * 2 + 1];
				rgba_pixel_data_p[2] = 0;
				rgba_pixel_data_p[3] = 255;
			}
		}
		break;
	case format::r8g8b8a8_typeless:
	case format::r8g8b8a8_unorm:
	case format::r8g8b8a8_unorm_srgb:
	case format::r8g8b8x8_typeless:
	case format::r8g8b8x8_unorm:
	case format::r8g8b8x8_unorm_srgb:
		for (uint32_t y = 0; y < desc.texture.height; ++y, data_p += data.row_pitch)
		{
			for (uint32_t x = 0; x < desc.texture.width; ++x)
			{
				uint8_t *const rgba_pixel_data_p = rgba_pixel_data.data() + (y * desc.texture.width + x) * 4;
				rgba_pixel_data_p[0] = data_p[x * 4 + 0];
				rgba_pixel_data_p[1] = data_p[x * 4 + 1];
				rgba_pixel_data_p[2] = data_p[x * 4 + 2];
				rgba_pixel_data_p[3] = data_p[x * 4 + 3];
			}
		}
		break;
	case format::b8g8r8a8_typeless:
	case format::b8g8r8a8_unorm:
	case format::b8g8r8a8_unorm_srgb:
	case format::b8g8r8x8_typeless:
	case format::b8g8r8x8_unorm:
	case format::b8g8r8x8_unorm_srgb:
		for (uint32_t y = 0; y < desc.texture.height; ++y, data_p += data.row_pitch)
		{
			for (uint32_t x = 0; x < desc.texture.width; ++x)
			{
				uint8_t *const rgba_pixel_data_p = rgba_pixel_data.data() + (y * desc.texture.width + x) * 4;
				// Swap red and blue channel
				rgba_pixel_data_p[0] = data_p[x * 4 + 2];
				rgba_pixel_data_p[1] = data_p[x * 4 + 1];
				rgba_pixel_data_p[2] = data_p[x * 4 + 0];
				rgba_pixel_data_p[3] = data_p[x * 4 + 3];
			}
		}
		break;
	case format::bc1_typeless:
	case format::bc1_unorm:
	case format::bc1_unorm_srgb:
		// See https://docs.microsoft.com/windows/win32/direct3d10/d3d10-graphics-programming-guide-resources-block-compression#bc1
		for (uint32_t block_y = 0; block_y < block_count_y; ++block_y)
		{
			for (uint32_t block_x = 0; block_x < block_count_x; ++block_x)
			{
				const uint16_t color_0 = *reinterpret_cast<const uint16_t *>(data_p);
				const uint16_t color_1 = *reinterpret_cast<const uint16_t *>(data_p + 2);
				const uint32_t color_i = *reinterpret_cast<const uint32_t *>(data_p + 4);

				uint8_t color_0_rgb[3];
				unpack_r5g6b5(color_0, color_0_rgb);
				uint8_t color_1_rgb[3];
				unpack_r5g6b5(color_1, color_1_rgb);
				const bool degenerate = color_0 > color_1;

				for (int y = 0; y < 4; ++y)
				{
					for (int x = 0; x < 4; ++x)
					{
						uint8_t *const rgba_pixel_data_p = rgba_pixel_data.data() + ((block_y * 4 + y) * desc.texture.width + (block_x * 4 + x)) * 4;
						unpack_bc1_value(color_0_rgb, color_1_rgb, (color_i >> (2 * (y * 4 + x))) & 0x3, rgba_pixel_data_p, degenerate);
					}
				}

				data_p += 8;
			}
		}
		break;
	case format::bc3_typeless:
	case format::bc3_unorm:
	case format::bc3_unorm_srgb:
		// See https://docs.microsoft.com/windows/win32/direct3d10/d3d10-graphics-programming-guide-resources-block-compression#bc3
		for (uint32_t block_y = 0; block_y < block_count_y; ++block_y)
		{
			for (uint32_t block_x = 0; block_x < block_count_x; ++block_x)
			{
				const uint8_t  alpha_0 = *reinterpret_cast<const uint8_t  *>(data_p);
				const uint8_t  alpha_1 = *reinterpret_cast<const uint8_t  *>(data_p + 1);
				const uint64_t alpha_i = *reinterpret_cast<const uint64_t *>(data_p + 2);

				const uint16_t color_0 = *reinterpret_cast<const uint16_t *>(data_p + 8);
				const uint16_t color_1 = *reinterpret_cast<const uint16_t *>(data_p + 10);
				const uint32_t color_i = *reinterpret_cast<const uint32_t *>(data_p + 12);

				uint8_t color_0_rgb[3];
				unpack_r5g6b5(color_0, color_0_rgb);
				uint8_t color_1_rgb[3];
				unpack_r5g6b5(color_1, color_1_rgb);

				for (int y = 0; y < 4; ++y)
				{
					for (int x = 0; x < 4; ++x)
					{
						uint8_t *const rgba_pixel_data_p = rgba_pixel_data.data() + ((block_y * 4 + y) * desc.texture.width + (block_x * 4 + x)) * 4;
						unpack_bc1_value(color_0_rgb, color_1_rgb, (color_i >> (2 * (y * 4 + x))) & 0x3, rgba_pixel_data_p);
						unpack_bc4_value(alpha_0, alpha_1, (alpha_i >> (3 * (y * 4 + x))) & 0x7, rgba_pixel_data_p + 3);
					}
				}

				data_p += 16;
			}
		}
		break;
	case format::bc4_typeless:
	case format::bc4_unorm:
	case format::bc4_snorm:
		// See https://docs.microsoft.com/windows/win32/direct3d10/d3d10-graphics-programming-guide-resources-block-compression#bc4
		for (uint32_t block_y = 0; block_y < block_count_y; ++block_y)
		{
			for (uint32_t block_x = 0; block_x < block_count_x; ++block_x)
			{
				const uint8_t  red_0 = *reinterpret_cast<const uint8_t  *>(data_p);
				const uint8_t  red_1 = *reinterpret_cast<const uint8_t  *>(data_p + 1);
				const uint64_t red_i = *reinterpret_cast<const uint64_t *>(data_p + 2);

				for (int y = 0; y < 4; ++y)
				{
					for (int x = 0; x < 4; ++x)
					{
						uint8_t *const rgba_pixel_data_p = rgba_pixel_data.data() + ((block_y * 4 + y) * desc.texture.width + (block_x * 4 + x)) * 4;
						unpack_bc4_value(red_0, red_1, (red_i >> (3 * (y * 4 + x))) & 0x7, rgba_pixel_data_p);
						rgba_pixel_data_p[1] = rgba_pixel_data_p[0];
						rgba_pixel_data_p[2] = rgba_pixel_data_p[0];
						rgba_pixel_data_p[3] = 255;
					}
				}

				data_p += 8;
			}
		}
		break;
	case format::bc5_typeless:
	case format::bc5_unorm:
	case format::bc5_snorm:
		// See https://docs.microsoft.com/windows/win32/direct3d10/d3d10-graphics-programming-guide-resources-block-compression#bc5
		for (uint32_t block_y = 0; block_y < block_count_y; ++block_y)
		{
			for (uint32_t block_x = 0; block_x < block_count_x; ++block_x)
			{
				const uint8_t  red_0 = *reinterpret_cast<const uint8_t  *>(data_p);
				const uint8_t  red_1 = *reinterpret_cast<const uint8_t  *>(data_p + 1);
				const uint64_t red_i = *reinterpret_cast<const uint64_t *>(data_p + 2);

				const uint8_t  green_0 = *reinterpret_cast<const uint8_t  *>(data_p + 8);
				const uint8_t  green_1 = *reinterpret_cast<const uint8_t  *>(data_p + 9);
				const uint64_t green_i = *reinterpret_cast<const uint64_t *>(data_p + 10);

				for (int y = 0; y < 4; ++y)
				{
					for (int x = 0; x < 4; ++x)
					{
						uint8_t *const rgba_pixel_data_p = rgba_pixel_data.data() + ((block_y * 4 + y) * desc.texture.width + (block_x * 4 + x)) * 4;
						unpack_bc4_value(red_0, red_1, (red_i >> (3 * (y * 4 + x))) & 0x7, rgba_pixel_data_p);
						unpack_bc4_value(green_0, green_1, (green_i >> (3 * (y * 4 + x))) & 0x7, rgba_pixel_data_p + 1);
						rgba_pixel_data_p[2] = 0;
						rgba_pixel_data_p[3] = 255;
					}
				}

				data_p += 16;
			}
		}
		break;
	default:
		// Unsupported format
		return false;
	}

	bool dump_all = false;
	std::filesystem::path dump_path;

#ifdef BUILTIN_ADDON
	ini_file &config = reshade::global_config();
	config.get("TEXTURE", "DumpAll", dump_all);
	config.get("TEXTURE", "DumpPath", dump_path);
#else
	dump_all = true;
#endif

	if (dump_all || force_dump)
	{
		const uint32_t hash = compute_crc32(rgba_pixel_data.data(), rgba_pixel_data.size());

		char hash_string[11];
		sprintf_s(hash_string, "0x%08x", hash);

		dump_path /= L"texture_";
		dump_path += hash_string;
		dump_path += L".bmp";

		return stbi_write_bmp(dump_path.u8string().c_str(), desc.texture.width, desc.texture.height, 4, rgba_pixel_data.data()) != 0;
	}

	return false;
}
bool dump_texture(command_queue *queue, resource tex, const resource_desc &desc)
{
	device *const device = queue->get_device();

	uint32_t texture_pitch = format_row_pitch(desc.texture.format, desc.texture.width);
	if (device->get_api() == device_api::d3d12) // See D3D12_TEXTURE_DATA_PITCH_ALIGNMENT
		texture_pitch = (texture_pitch + 255) & ~255;

	resource intermediate;
	if (desc.heap != memory_heap::gpu_only)
	{
		// Avoid copying to temporary system memory resource if texture is accessible directly
		intermediate = tex;
	}
	else if (device->check_capability(device_caps::copy_buffer_to_texture))
	{
		if ((desc.usage & resource_usage::copy_source) != resource_usage::copy_source)
			return false;

		if (!device->create_resource(resource_desc(texture_pitch * desc.texture.height, memory_heap::gpu_to_cpu, resource_usage::copy_dest), nullptr, resource_usage::copy_dest, &intermediate))
		{
			reshade::log_message(1, "Failed to create system memory buffer for texture dumping!");
			return false;
		}

		command_list *const cmd_list = queue->get_immediate_command_list();
		cmd_list->barrier(tex, resource_usage::shader_resource, resource_usage::copy_source);
		cmd_list->copy_texture_to_buffer(tex, 0, nullptr, intermediate, 0, desc.texture.width, desc.texture.height);
		cmd_list->barrier(tex, resource_usage::copy_source, resource_usage::shader_resource);
	}
	else
	{
		if ((desc.usage & resource_usage::copy_source) != resource_usage::copy_source)
			return false;

		if (!device->create_resource(resource_desc(desc.texture.width, desc.texture.height, 1, 1, format_to_default_typed(desc.texture.format), 1, memory_heap::gpu_to_cpu, resource_usage::copy_dest), nullptr, resource_usage::copy_dest, &intermediate))
		{
			reshade::log_message(1, "Failed to create system memory texture for texture dumping!");
			return false;
		}

		command_list *const cmd_list = queue->get_immediate_command_list();
		cmd_list->barrier(tex, resource_usage::shader_resource, resource_usage::copy_source);
		cmd_list->copy_texture_region(tex, 0, nullptr, intermediate, 0, nullptr);
		cmd_list->barrier(tex, resource_usage::copy_source, resource_usage::shader_resource);
	}

	queue->wait_idle();

	if (subresource_data mapped_data;
		device->map_resource(intermediate, 0, nullptr, map_access::read_only, &mapped_data))
	{
		if (device->check_capability(device_caps::copy_buffer_to_texture))
			mapped_data.row_pitch = texture_pitch;

		dump_texture(desc, mapped_data, true);

		device->unmap_resource(intermediate, 0);
	}

	if (intermediate != tex)
		device->destroy_resource(intermediate);

	return true;
}

// There are multiple different ways textures can be initialized, so try and intercept them all
// - Via initial data provided during texture creation (e.g. for immutable textures, common in D3D11 and OpenGL): See 'on_init_texture' implementation below
// - Via a copy operation from a buffer in host memory to the texture (common in D3D12 and Vulkan): See 'on_copy_texture' implementation below
// - Via a direct update operation from host memory to the texture (common in D3D11): See 'on_update_texture' implementation below

static void on_init_texture(device *device, const resource_desc &desc, const subresource_data *initial_data, resource_usage, resource)
{
	if (desc.type != resource_type::texture_2d)
		return; // Ignore resources that are not 2D textures
	if (device->get_api() != device_api::opengl && (desc.usage & (resource_usage::shader_resource | resource_usage::depth_stencil | resource_usage::render_target)) != resource_usage::shader_resource)
		return; // Ignore textures that can be used as render targets (since this should only dump textures used as shader input)

	if (initial_data != nullptr)
		dump_texture(desc, *initial_data);
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

	// Map source buffer to get the contents that will be copied into the target texture (this should succeed, since it was already checked that the buffer is in host memory)
	if (subresource_data mapped_data;
		device->map_resource(src, src_subresource, nullptr, map_access::read_only, &mapped_data))
	{
		dump_texture(dst_desc, mapped_data);

		device->unmap_resource(src, src_subresource);
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
