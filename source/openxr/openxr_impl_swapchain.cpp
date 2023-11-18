/*
 * Copyright (C) 2022 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "openxr_impl_swapchain.hpp"
#include "dll_log.hpp"
#include "addon_manager.hpp"

reshade::openxr::swapchain_impl::swapchain_impl(api::device *device, api::command_queue *graphics_queue, XrSession session) :
	api_object_impl(session),
	_device(device)
{
	// TODO_OXR: this calls init_vr_gui.  UI is not implemented yet.
	create_effect_runtime(this, graphics_queue, false);
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

void reshade::openxr::swapchain_impl::on_present(api::command_queue *queue, api::resource left_xr_swapchain_image, api::resource right_xr_swapchain_image)
{
	const api::resource_desc source_desc = _device->get_resource_desc(left_xr_swapchain_image);

	if (source_desc.texture.samples > 1 && !_device->check_capability(api::device_caps::resolve_region))
		return; // Can only copy whole subresources when the resource is multisampled

	reshade::api::subresource_box source_box;
	source_box.left = 0;
	source_box.top = 0;
	source_box.front = 0;
	source_box.right = source_desc.texture.width;
	source_box.bottom = source_desc.texture.height;
	source_box.back = 1;

	auto const region_width = source_box.width();
	auto const target_width = region_width * 2;
	auto const region_height = source_box.height();

	if (region_width == 0 || region_height == 0)
		return;

	const api::resource_desc target_desc = _side_by_side_texture != 0 ? _device->get_resource_desc(_side_by_side_texture) : api::resource_desc();

	// Due to rounding errors with the bounds we have to use a tolerance of 1 pixel per eye (2 pixels in total)
	auto const width_difference = std::abs(static_cast<int32_t>(target_width) - static_cast<int32_t>(target_desc.texture.width));
	auto const height_difference = std::abs(static_cast<int32_t>(region_height) - static_cast<int32_t>(target_desc.texture.height));

	if (width_difference > 2 || height_difference > 2 || api::format_to_typeless(source_desc.texture.format) != api::format_to_typeless(target_desc.texture.format))
	{
		LOG(INFO) << "Resizing runtime " << this << " in VR to " << target_width << "x" << region_height << " ...";

		on_reset();

		if (!_device->create_resource(
				api::resource_desc(target_width, region_height, 1, 1, api::format_to_typeless(source_desc.texture.format), 1, api::memory_heap::gpu_only, api::resource_usage::render_target | api::resource_usage::copy_source | api::resource_usage::copy_dest),
				nullptr, api::resource_usage::general, &_side_by_side_texture))
		{
			LOG(ERROR) << "Failed to create region texture!";
			return;
		}

		if (!on_init())
			return;
	}

	api::command_list *const cmd_list = queue->get_immediate_command_list();

	// Copy region of the source texture (in case of an array texture, copy from the layer corresponding to the current eye)
	auto const left_dest_box = get_eye_subresource_box(eye::left);
	auto const right_dest_box = get_eye_subresource_box(eye::right);

	cmd_list->barrier(_side_by_side_texture, api::resource_usage::undefined, api::resource_usage::copy_dest);
	// OXR end frame images are in VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL.
	cmd_list->barrier(left_xr_swapchain_image, api::resource_usage::render_target, api::resource_usage::copy_source);
	cmd_list->barrier(right_xr_swapchain_image, api::resource_usage::render_target, api::resource_usage::copy_source);
	if (source_desc.texture.samples <= 1)
	{
		cmd_list->copy_texture_region(left_xr_swapchain_image,
									  0,
									  &source_box,
									  _side_by_side_texture,
									  0,
									  &left_dest_box,
									  api::filter_mode::min_mag_mip_point);
		cmd_list->copy_texture_region(right_xr_swapchain_image,
									  0,
									  &source_box,
									  _side_by_side_texture,
									  0,
									  &right_dest_box,
									  api::filter_mode::min_mag_mip_point);
	}
	else
	{
		cmd_list->resolve_texture_region(left_xr_swapchain_image,
										 0,
										 &source_box,
										 _side_by_side_texture,
										 0,
										 left_dest_box.left,
										 left_dest_box.top,
										 left_dest_box.front,
										 source_desc.texture.format);

		cmd_list->resolve_texture_region(right_xr_swapchain_image,
										 0,
										 &source_box,
										 _side_by_side_texture,
										 0,
										 right_dest_box.left,
										 right_dest_box.top,
										 right_dest_box.front,
										 source_desc.texture.format);
	}
	cmd_list->barrier(_side_by_side_texture, api::resource_usage::copy_dest, api::resource_usage::present);

	reshade::present_effect_runtime(this, queue);

	reshade::api::subresource_box left_source_box;
	left_source_box.left = 0;
	left_source_box.top = 0;
	left_source_box.front = 0;
	left_source_box.right = source_desc.texture.width;
	left_source_box.bottom = source_desc.texture.height;
	left_source_box.back = 1;

	reshade::api::subresource_box right_source_box;
	right_source_box.left = source_desc.texture.width;
	right_source_box.top = 0;
	right_source_box.front = 0;
	right_source_box.right = source_desc.texture.width * 2;
	right_source_box.bottom = source_desc.texture.height;
	right_source_box.back = 1;

	// Copy region of the source texture (in case of an array texture, copy from the layer corresponding to the current eye)
	auto const dest_box = get_eye_subresource_box(reshade::openxr::eye::left);

	cmd_list->barrier(left_xr_swapchain_image, api::resource_usage::copy_source, api::resource_usage::copy_dest);
	cmd_list->barrier(right_xr_swapchain_image, api::resource_usage::copy_source, api::resource_usage::copy_dest);
	cmd_list->barrier(_side_by_side_texture, api::resource_usage::present, api::resource_usage::copy_source);

	if (source_desc.texture.samples <= 1)
	{
		cmd_list->copy_texture_region(_side_by_side_texture,
									  0,
									  &left_source_box,
									  left_xr_swapchain_image,
									  0,
									  &dest_box,
									  api::filter_mode::min_mag_mip_point);
		cmd_list->copy_texture_region(_side_by_side_texture,
									  0,
									  &right_source_box,
									  right_xr_swapchain_image,
									  0,
									  &dest_box,
									  api::filter_mode::min_mag_mip_point);
	}

	cmd_list->barrier(left_xr_swapchain_image, api::resource_usage::copy_dest, api::resource_usage::render_target);
	cmd_list->barrier(right_xr_swapchain_image, api::resource_usage::copy_dest, api::resource_usage::render_target);
}
