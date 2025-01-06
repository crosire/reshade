/*
 * Copyright (C) 2022 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "openxr_impl_swapchain.hpp"
#include "dll_log.hpp"
#include "addon_manager.hpp"
#include "runtime_manager.hpp"
#include <algorithm> // std::max

reshade::openxr::swapchain_impl::swapchain_impl(api::device *device, api::command_queue *graphics_queue, XrSession session) :
	api_object_impl(session),
	_device(device),
	_graphics_queue(graphics_queue)
{
	create_effect_runtime(this, graphics_queue, true);
}

reshade::openxr::swapchain_impl::~swapchain_impl()
{
	on_reset();

	destroy_effect_runtime(this);
}

reshade::api::device *reshade::openxr::swapchain_impl::get_device()
{
	return _device;
}

reshade::api::resource reshade::openxr::swapchain_impl::get_back_buffer(uint32_t index)
{
	assert(index == 0);

	return _side_by_side_texture;
}

reshade::api::rect reshade::openxr::swapchain_impl::get_view_rect(uint32_t index, uint32_t view_count) const
{
	const api::resource_desc desc = _device->get_resource_desc(_side_by_side_texture);

	return api::rect {
		static_cast<int32_t>(index * (desc.texture.width / view_count)), 0,
		static_cast<int32_t>((index + 1) * (desc.texture.width / view_count)), static_cast<int32_t>(desc.texture.height)
	};
}
reshade::api::subresource_box reshade::openxr::swapchain_impl::get_view_subresource_box(uint32_t index, uint32_t view_count) const
{
	const api::resource_desc desc = _device->get_resource_desc(_side_by_side_texture);

	return api::subresource_box {
		index * (desc.texture.width / view_count), 0, 0,
		(index + 1) * (desc.texture.width / view_count), desc.texture.height, 1
	};
}

bool reshade::openxr::swapchain_impl::on_init()
{
	// Created in 'on_vr_submit' below
	assert(_side_by_side_texture != 0);

#if RESHADE_ADDON
	invoke_addon_event<addon_event::init_swapchain>(this);
#endif

	init_effect_runtime(this);

	return true;
}
void reshade::openxr::swapchain_impl::on_reset()
{
	if (_side_by_side_texture == 0)
		return;

	reset_effect_runtime(this);

#if RESHADE_ADDON
	invoke_addon_event<addon_event::destroy_swapchain>(this);
#endif

	_device->destroy_resource(_side_by_side_texture);
	_side_by_side_texture = {};
}

void reshade::openxr::swapchain_impl::on_present(uint32_t view_count, const api::resource *view_textures, const api::subresource_box *view_boxes, const uint32_t *view_layers)
{
	const api::resource_desc source_desc = _device->get_resource_desc(view_textures[0]);

	if (source_desc.texture.samples > 1)
		return;

	uint32_t target_width = 0;
	uint32_t region_height = 0;
	for (uint32_t i = 0; i < view_count; ++i)
	{
		target_width += view_boxes[i].width();
		region_height = std::max(region_height, view_boxes[i].height());
	}

	if (target_width == 0 || region_height == 0)
		return;

	const api::resource_desc target_desc = _side_by_side_texture != 0 ? _device->get_resource_desc(_side_by_side_texture) : api::resource_desc();

	if (target_width != target_desc.texture.width || region_height != target_desc.texture.height || api::format_to_typeless(source_desc.texture.format) != api::format_to_typeless(target_desc.texture.format))
	{
		reshade::log::message(reshade::log::level::info, "Resizing runtime %p in VR to %ux%u ...", this, target_width, region_height);

		on_reset();

		// Only make format typeless for format variants that support sRGB, so to not break format variants that can be either unorm or float
		const api::format format = (source_desc.texture.format == api::format::r8g8b8a8_unorm || source_desc.texture.format == api::format::r8g8b8a8_unorm_srgb || source_desc.texture.format == api::format::b8g8r8a8_unorm || source_desc.texture.format == api::format::b8g8r8a8_unorm_srgb) ?
			api::format_to_typeless(source_desc.texture.format) : source_desc.texture.format;

		if (!_device->create_resource(
				api::resource_desc(target_width, region_height, 1, 1, format, 1, api::memory_heap::gpu_only, api::resource_usage::render_target | api::resource_usage::copy_source | api::resource_usage::copy_dest),
				nullptr, api::resource_usage::general, &_side_by_side_texture))
		{
			reshade::log::message(reshade::log::level::error, "Failed to create region texture!");
			return;
		}

		_device->set_resource_name(_side_by_side_texture, "ReShade side-by-side texture");

		if (!on_init())
			return;
	}

	api::command_list *const cmd_list = _graphics_queue->get_immediate_command_list();

	// Copy source textures into side-by-side texture
	const auto before_state = _device->get_api() == api::device_api::d3d12 ? api::resource_usage::shader_resource_pixel : api::resource_usage::copy_source;

	cmd_list->barrier(_side_by_side_texture, api::resource_usage::general, api::resource_usage::copy_dest);

	for (uint32_t i = 0; i < view_count; ++i)
	{
		cmd_list->barrier(view_textures[i], before_state, api::resource_usage::copy_source);

		const api::subresource_box dest_box = get_view_subresource_box(i, view_count);
		cmd_list->copy_texture_region(view_textures[i], view_layers[i], &view_boxes[i], _side_by_side_texture, 0, &dest_box, api::filter_mode::min_mag_mip_point);
	}

	cmd_list->barrier(_side_by_side_texture, api::resource_usage::copy_dest, api::resource_usage::present);

#if RESHADE_ADDON
	invoke_addon_event<addon_event::present>(_graphics_queue, this, nullptr, nullptr, 0, nullptr);
#endif

	present_effect_runtime(this, _graphics_queue);

	cmd_list->barrier(_side_by_side_texture, api::resource_usage::present, api::resource_usage::copy_source);

	for (uint32_t i = 0; i < view_count; ++i)
	{
		cmd_list->barrier(view_textures[i], api::resource_usage::copy_source, api::resource_usage::copy_dest);

		const api::subresource_box dest_box = get_view_subresource_box(i, view_count);
		cmd_list->copy_texture_region(_side_by_side_texture, 0, &dest_box, view_textures[i], view_layers[i], &view_boxes[i], api::filter_mode::min_mag_mip_point);

		cmd_list->barrier(view_textures[i], api::resource_usage::copy_dest, before_state);
	}

	cmd_list->barrier(_side_by_side_texture, api::resource_usage::copy_source, api::resource_usage::general);

	_graphics_queue->flush_immediate_command_list();
}
