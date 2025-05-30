/*
 * Copyright (C) 2021 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause OR MIT
 */

#include <reshade.hpp>
#include "config.hpp"
#include <vector>

using namespace reshade::api;

// When a resource is being created, ReShade will first call any registered 'create_resource' callbacks, then actually create the resource and finally call any registered 'init_resource' callbacks.
// So to modify the initial data passed to resource creation, have to allocate and fill it in a 'create_resource' callback, which is then passed along to the actual resource creation and then have to free it again in a 'init_resource' callback.
// The problem is that multiple threads can create resources simultaneously and as such the order in which these three things happen across multiple threads is not guaranteed (e.g. 'init_resource' may be called on one thread while another thread has only just called 'create_resource').
// To ensure there is no race condition between the data being freed on one thread and another thread potentially still creating a different resource also referencing initial data, this variable is made thread-local.
// This way each thread gets its own copy and the order it is being used on each thread is always correct, from it being set in the 'create_resource' callback, to it being used during resource creation, to it finally being freed in the 'init_resource' callback.
static thread_local std::vector<std::vector<uint8_t>> s_data_to_delete;

// See implementation in 'utils\load_texture_image.cpp'
extern bool load_texture_image(const resource_desc &desc, subresource_data &data, std::vector<std::vector<uint8_t>> &data_to_delete);

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

	return initial_data != nullptr && load_texture_image(desc, *initial_data, s_data_to_delete);
}
static void on_after_create_texture(device *, const resource_desc &, const subresource_data *, resource_usage, resource)
{
	// Free the memory allocated via the 'load_texture_image' call above during the preceding 'create_resource' event that called in the 'on_create_texture' callback
	s_data_to_delete.clear();
}

static bool on_copy_texture(command_list *cmd_list, resource source, uint32_t source_subresource, const subresource_box *, resource dest, uint32_t dest_subresource, const subresource_box *dest_box, filter_mode)
{
	if (source_subresource != 0 || source_subresource != dest_subresource)
		return false; // Ignore copies to mipmap levels other than the base level

	device *const device = cmd_list->get_device();

	const resource_desc source_desc = device->get_resource_desc(source);
	if (source_desc.heap != memory_heap::cpu_to_gpu)
		return false; // Ignore copies that are not from a buffer in host memory

	const resource_desc dest_desc = device->get_resource_desc(dest);
	if (!filter_texture(device, dest_desc, dest_box))
		return false;

	bool replace = false;

	subresource_data new_data;
	if (device->map_texture_region(source, source_subresource, nullptr, map_access::read_only, &new_data))
	{
		replace = load_texture_image(dest_desc, new_data, s_data_to_delete);

		device->unmap_texture_region(source, source_subresource);
	}

	if (replace)
	{
		// Update texture with the new data
		device->update_texture_region(new_data, dest, dest_subresource, dest_box);

		// Free the memory allocated via the 'load_texture_image' call above
		s_data_to_delete.clear();

		return true; // Texture was already updated now, so skip the original copy command from the application
	}

	return false;
}
static bool on_update_texture(device *device, const subresource_data &data, resource dst, uint32_t dst_subresource, const subresource_box *dst_box)
{
	if (dst_subresource != 0)
		return false; // Ignore updates to mipmap levels other than the base level

	const resource_desc dest_desc = device->get_resource_desc(dst);
	if (!filter_texture(device, dest_desc, dst_box))
		return false;

	subresource_data new_data = data;
	if (load_texture_image(dest_desc, new_data, s_data_to_delete))
	{
		// Update texture with the new data
		device->update_texture_region(new_data, dst, dst_subresource, dst_box);

		// Free the memory allocated via the 'load_texture_image' call above
		s_data_to_delete.clear();

		return true; // Texture was already updated now, so skip the original update command from the application
	}

	return false;
}

// Keep track of current resource between 'map_resource' and 'unmap_resource' event invocations
// It's also thread-local, for similar reasons as 's_data_to_delete' above, just that the two events that could interact with multiple threads here are 'map_texture_region' and 'unmap_texture_region'.
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

	if (load_texture_image(s_current_mapping.desc, s_current_mapping.data, s_data_to_delete))
	{
		std::memcpy(mapped_data, s_current_mapping.data.data, s_current_mapping.data.slice_pitch);

		// Free the memory allocated via the 'load_texture_image' call above
		s_data_to_delete.clear();
	}
}

extern "C" __declspec(dllexport) const char *NAME = "Texture Replace";
extern "C" __declspec(dllexport) const char *DESCRIPTION = "Example add-on that replaces textures before they are used by the application with image files from disk (\"" RESHADE_ADDON_TEXTURE_LOAD_DIR "\" directory).";

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
