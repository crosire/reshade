/*
 * Copyright (C) 2022 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "openxr_impl_swapchain.hpp"
#include "dll_log.hpp"
#include "addon_manager.hpp"
#include "runtime_manager.hpp"

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

reshade::api::rect reshade::openxr::swapchain_impl::get_eye_rect(eye eye) const
{
	const api::resource_desc desc = _device->get_resource_desc(_side_by_side_texture);

	return api::rect {
		static_cast<int32_t>(static_cast<int>(eye) * (desc.texture.width / 2)), 0,
		static_cast<int32_t>((static_cast<int>(eye) + 1) * (desc.texture.width / 2)), static_cast<int32_t>(desc.texture.height)
	};
}
reshade::api::subresource_box reshade::openxr::swapchain_impl::get_eye_subresource_box(eye eye) const
{
	const api::resource_desc desc = _device->get_resource_desc(_side_by_side_texture);

	return api::subresource_box {
		static_cast<int32_t>(static_cast<int>(eye) * (desc.texture.width / 2)), 0, 0,
		static_cast<int32_t>((static_cast<int>(eye) + 1) * (desc.texture.width / 2)), static_cast<int32_t>(desc.texture.height), 1
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

void reshade::openxr::swapchain_impl::on_present(api::resource left_xr_swapchain_image, const api::rect &left_rect, uint32_t left_layer, api::resource right_xr_swapchain_image, const api::rect &right_rect, uint32_t right_layer)
{
	const api::resource_desc source_desc = _device->get_resource_desc(left_xr_swapchain_image);

	if (source_desc.texture.samples > 1 && !_device->check_capability(api::device_caps::resolve_region))
		return; // Can only copy whole subresources when the resource is multisampled

	reshade::api::subresource_box left_source_box;
	left_source_box.left = left_rect.left;
	left_source_box.top = left_rect.top;
	left_source_box.front = 0;
	left_source_box.right = left_rect.right;
	left_source_box.bottom = left_rect.bottom;
	left_source_box.back = 1;
	reshade::api::subresource_box right_source_box;
	right_source_box.left = right_rect.left;
	right_source_box.top = right_rect.top;
	right_source_box.front = 0;
	right_source_box.right = right_rect.right;
	right_source_box.bottom = right_rect.bottom;
	right_source_box.back = 1;

	const uint32_t target_width = left_source_box.width() + right_source_box.width();
	const uint32_t region_height = std::max(left_source_box.height(), right_source_box.height());

	if (target_width == 0 || region_height == 0)
		return;

	const api::resource_desc target_desc = _side_by_side_texture != 0 ? _device->get_resource_desc(_side_by_side_texture) : api::resource_desc();

	// Due to rounding errors with the bounds we have to use a tolerance of 1 pixel per eye (2 pixels in total)
	const auto width_difference = std::abs(static_cast<int32_t>(target_width) - static_cast<int32_t>(target_desc.texture.width));
	const auto height_difference = std::abs(static_cast<int32_t>(region_height) - static_cast<int32_t>(target_desc.texture.height));

	if (width_difference > 2 || height_difference > 2 || api::format_to_typeless(source_desc.texture.format) != api::format_to_typeless(target_desc.texture.format))
	{
		LOG(INFO) << "Resizing runtime " << this << " in VR to " << target_width << "x" << region_height << " ...";

		on_reset();

		// Only make format typeless for format variants that support sRGB, so to not break format variants that can be either unorm or float
		const api::format format = (source_desc.texture.format == api::format::r8g8b8a8_unorm || source_desc.texture.format == api::format::r8g8b8a8_unorm_srgb || source_desc.texture.format == api::format::b8g8r8a8_unorm || source_desc.texture.format == api::format::b8g8r8a8_unorm_srgb) ?
			api::format_to_typeless(source_desc.texture.format) : source_desc.texture.format;

		if (!_device->create_resource(
				api::resource_desc(target_width, region_height, 1, 1, format, 1, api::memory_heap::gpu_only, api::resource_usage::render_target | api::resource_usage::copy_source | api::resource_usage::copy_dest),
				nullptr, api::resource_usage::general, &_side_by_side_texture))
		{
			LOG(ERROR) << "Failed to create region texture!";
			return;
		}

		_device->set_resource_name(_side_by_side_texture, "ReShade side-by-side texture");

		if (!on_init())
			return;
	}

	api::command_list *const cmd_list = _graphics_queue->get_immediate_command_list();

	// Copy source textures into side-by-side texture
	const api::subresource_box left_dest_box = get_eye_subresource_box(eye::left);
	const api::subresource_box right_dest_box = get_eye_subresource_box(eye::right);

	const auto before_state = _device->get_api() == api::device_api::d3d12 ? api::resource_usage::shader_resource_pixel : api::resource_usage::copy_source;
	const api::resource resources[3] = { _side_by_side_texture, left_xr_swapchain_image, right_xr_swapchain_image };

	if (source_desc.texture.samples <= 1)
	{
		const api::resource_usage state_old[3] = { api::resource_usage::general, before_state, before_state };
		const api::resource_usage state_new[3] = { api::resource_usage::copy_dest, api::resource_usage::copy_source, api::resource_usage::copy_source };
		cmd_list->barrier(3, resources, state_old, state_new);

		cmd_list->copy_texture_region(left_xr_swapchain_image, left_layer, &left_source_box, _side_by_side_texture, 0, &left_dest_box, api::filter_mode::min_mag_mip_point);
		cmd_list->copy_texture_region(right_xr_swapchain_image, right_layer, &right_source_box, _side_by_side_texture, 0, &right_dest_box, api::filter_mode::min_mag_mip_point);

		cmd_list->barrier(_side_by_side_texture, api::resource_usage::copy_dest, api::resource_usage::present);
	}
	else
	{
		const api::resource_usage state_old[3] = { api::resource_usage::general, before_state, before_state };
		const api::resource_usage state_new[3] = { api::resource_usage::resolve_dest, api::resource_usage::resolve_source, api::resource_usage::resolve_source };
		cmd_list->barrier(3, resources, state_old, state_new);

		cmd_list->resolve_texture_region(left_xr_swapchain_image, left_layer, &left_source_box, _side_by_side_texture, 0, left_dest_box.left, left_dest_box.top, left_dest_box.front, source_desc.texture.format);
		cmd_list->resolve_texture_region(right_xr_swapchain_image, right_layer, &right_source_box, _side_by_side_texture, 0, right_dest_box.left, right_dest_box.top, right_dest_box.front, source_desc.texture.format);

		cmd_list->barrier(_side_by_side_texture, api::resource_usage::resolve_dest, api::resource_usage::present);
	}

#if RESHADE_ADDON
	invoke_addon_event<addon_event::present>(_graphics_queue, this, nullptr, nullptr, 0, nullptr);
#endif

	present_effect_runtime(this, _graphics_queue);

	if (source_desc.texture.samples <= 1)
	{
		const api::resource_usage state_old[3] = { api::resource_usage::present, api::resource_usage::copy_source, api::resource_usage::copy_source };
		const api::resource_usage state_new[3] = { api::resource_usage::copy_source, api::resource_usage::copy_dest, api::resource_usage::copy_dest };
		cmd_list->barrier(3, resources, state_old, state_new);

		cmd_list->copy_texture_region(_side_by_side_texture, 0, &left_dest_box, left_xr_swapchain_image, left_layer, &left_source_box, api::filter_mode::min_mag_mip_point);
		cmd_list->copy_texture_region(_side_by_side_texture, 0, &right_dest_box, right_xr_swapchain_image, right_layer, &right_source_box, api::filter_mode::min_mag_mip_point);

		const api::resource_usage state_final[3] = { api::resource_usage::general, before_state, before_state };
		cmd_list->barrier(3, resources, state_new, state_final);
	}
	else
	{
		const api::resource_usage state_old[3] = { api::resource_usage::present, api::resource_usage::resolve_source, api::resource_usage::resolve_source };

		// TODO

		const api::resource_usage state_final[3] = { api::resource_usage::general, before_state, before_state };
		cmd_list->barrier(3, resources, state_old, state_final);
	}

	_graphics_queue->flush_immediate_command_list();
}
