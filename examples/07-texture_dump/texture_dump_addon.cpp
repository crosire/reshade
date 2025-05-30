/*
 * Copyright (C) 2021 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause OR MIT
 */

#include <reshade.hpp>
#include "config.hpp"

using namespace reshade::api;

// See implementation in 'utils\save_texture_image.cpp'
extern bool save_texture_image(const resource_desc &desc, const subresource_data &data);

// There are multiple different ways textures can be initialized, so try and intercept them all:
// - via initial data provided during texture creation (e.g. for immutable textures, common in D3D11 and OpenGL): See 'on_init_texture' implementation below
// - via a direct update operation from host memory to the texture (common in D3D11): See 'on_update_texture' implementation below
// - via a copy operation from a buffer in host memory to the texture (common in D3D12 and Vulkan): See 'on_copy_buffer_to_texture' implementation below
// - via mapping and writing to texture that is accessible in host memory (common in D3D9): See 'on_map_texture' and 'on_unmap_texture' implementation below

static inline bool filter_texture(device *device, const resource_desc &desc, const subresource_box *box)
{
	if (desc.type != resource_type::texture_2d || (desc.usage & resource_usage::shader_resource) == resource_usage::undefined || (desc.heap != memory_heap::gpu_only && desc.heap != memory_heap::unknown) || (desc.flags & resource_flags::dynamic) == resource_flags::dynamic)
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

	if (desc.texture.height <= 8 || (desc.texture.width == 128 && desc.texture.height == 32))
		return false; // Filter out small textures, which are commonly just lookup tables that are not interesting to save

	if (desc.texture.format == format::r8_unorm || desc.texture.format == format::a8_unorm || desc.texture.format == format::l8_unorm)
		return false; // Filter out single component textures, since they are commonly used for video processing

	return true;
}

static void on_init_texture(device *device, const resource_desc &desc, const subresource_data *initial_data, resource_usage, resource)
{
	if (initial_data == nullptr || !filter_texture(device, desc, nullptr))
		return;

	save_texture_image(desc, *initial_data);
}
static bool on_update_texture(device *device, const subresource_data &data, resource dest, uint32_t dest_subresource, const subresource_box *dest_box)
{
	if (dest_subresource != 0)
		return false; // Ignore updates to mipmap levels other than the base level

	const resource_desc dest_desc = device->get_resource_desc(dest);
	if (!filter_texture(device, dest_desc, dest_box))
		return false;

	save_texture_image(dest_desc, data);

	return false;
}

static bool on_copy_buffer_to_texture(command_list *cmd_list, resource source, uint64_t source_offset, uint32_t row_length, uint32_t slice_height, resource dest, uint32_t dest_subresource, const subresource_box *dest_box)
{
	if (dest_subresource != 0)
		return false; // Ignore copies to mipmap levels other than the base level

	device *const device = cmd_list->get_device();

	const resource_desc source_desc = device->get_resource_desc(source);
	if (source_desc.heap != memory_heap::cpu_to_gpu && source_desc.heap != memory_heap::unknown)
		return false; // Ignore copies that are not from a buffer in host memory

	const resource_desc dest_desc = device->get_resource_desc(dest);
	if (!filter_texture(device, dest_desc, dest_box))
		return false;

	// Map source buffer to get the contents that will be copied into the target texture (this should succeed, since it was already checked that the buffer is in host memory)
	if (void *mapped_ptr;
		device->map_buffer_region(source, source_offset, ~0ull, map_access::read_only, &mapped_ptr))
	{
		subresource_data mapped_data;
		mapped_data.data = mapped_ptr;
		mapped_data.row_pitch = format_row_pitch(dest_desc.texture.format, row_length != 0 ? row_length : dest_desc.texture.width);
		if (device->get_api() == device_api::d3d12) // Align row pitch to D3D12_TEXTURE_DATA_PITCH_ALIGNMENT (256)
			mapped_data.row_pitch = (mapped_data.row_pitch + 255) & ~255;
		mapped_data.slice_pitch = format_slice_pitch(dest_desc.texture.format, mapped_data.row_pitch, slice_height != 0 ? slice_height : dest_desc.texture.height);

		save_texture_image(dest_desc, mapped_data);

		device->unmap_buffer_region(source);
	}

	return false;
}

// Keep track of current resource between 'map_texture_region' and 'unmap_texture_region' event invocations
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

	save_texture_image(s_current_mapping.desc, s_current_mapping.data);
}

extern "C" __declspec(dllexport) const char *NAME = "Texture Dump";
extern "C" __declspec(dllexport) const char *DESCRIPTION = "Example add-on that dumps all textures used by the application to image files on disk (\"" RESHADE_ADDON_TEXTURE_SAVE_DIR "\" directory).";

BOOL APIENTRY DllMain(HMODULE hModule, DWORD fdwReason, LPVOID)
{
	switch (fdwReason)
	{
	case DLL_PROCESS_ATTACH:
		if (!reshade::register_addon(hModule))
			return FALSE;
		reshade::register_event<reshade::addon_event::init_resource>(on_init_texture);
		reshade::register_event<reshade::addon_event::update_texture_region>(on_update_texture);
		reshade::register_event<reshade::addon_event::copy_buffer_to_texture>(on_copy_buffer_to_texture);
		reshade::register_event<reshade::addon_event::map_texture_region>(on_map_texture);
		reshade::register_event<reshade::addon_event::unmap_texture_region>(on_unmap_texture);
		break;
	case DLL_PROCESS_DETACH:
		reshade::unregister_addon(hModule);
		break;
	}

	return TRUE;
}
