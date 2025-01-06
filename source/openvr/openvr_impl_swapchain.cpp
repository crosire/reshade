/*
 * Copyright (C) 2022 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "openvr_impl_swapchain.hpp"
#include "d3d10/d3d10_device.hpp"
#include "d3d11/d3d11_device.hpp"
#include "d3d11/d3d11_device_context.hpp"
#include "d3d12/d3d12_device.hpp"
#include "d3d12/d3d12_command_queue.hpp"
#include "opengl/opengl_impl_device_context.hpp"
#include "dll_log.hpp"
#include "addon_manager.hpp"
#include "runtime_manager.hpp"
#include <cmath> // std::abs, std::ceil, std::floor
#include <algorithm> // std::max, std::min

reshade::openvr::swapchain_impl::swapchain_impl(D3D10Device *device, vr::IVRCompositor *compositor) :
	swapchain_impl(device, device, compositor)
{
	_direct3d_device = static_cast<ID3D10Device *>(device);
	// Explicitly add a reference to the device, to ensure it stays valid for the lifetime of this swap chain object
	static_cast<IUnknown *>(_direct3d_device)->AddRef();
}

reshade::openvr::swapchain_impl::swapchain_impl(D3D11Device *device, vr::IVRCompositor *compositor) :
	swapchain_impl(device, device->_immediate_context, compositor)
{
	_direct3d_device = static_cast<ID3D11Device *>(device);
	// Explicitly add a reference to the device, to ensure it stays valid for the lifetime of this swap chain object
	static_cast<IUnknown *>(_direct3d_device)->AddRef();
}

reshade::openvr::swapchain_impl::swapchain_impl(D3D12CommandQueue *queue, vr::IVRCompositor *compositor) :
	swapchain_impl(queue->_device, queue, compositor)
{
	_direct3d_device = queue;
	// Explicitly add a reference to the command queue, to ensure it stays valid for the lifetime of this swap chain object
	static_cast<IUnknown *>(_direct3d_device)->AddRef();
}

reshade::openvr::swapchain_impl::swapchain_impl(api::device *device, api::command_queue *graphics_queue, vr::IVRCompositor *compositor) :
	api_object_impl(compositor),
	_device(device)
{
	_is_opengl = device->get_api() == api::device_api::opengl;

	create_effect_runtime(this, graphics_queue, true);
}

reshade::openvr::swapchain_impl::~swapchain_impl()
{
	extern thread_local reshade::opengl::device_context_impl *g_opengl_context;
	// Do not access '_device' object to check the device API, in case it was already destroyed
	if (_is_opengl && g_opengl_context == nullptr)
	{
		return; // Cannot clean up if OpenGL context was already destroyed
	}

	on_reset();

	destroy_effect_runtime(this);

	// Release the explicit reference to the device now that the effect runtime was destroyed and is longer referencing it
	if (_direct3d_device != nullptr)
		static_cast<IUnknown *>(_direct3d_device)->Release();
}

reshade::api::device *reshade::openvr::swapchain_impl::get_device()
{
	return _device;
}

reshade::api::resource reshade::openvr::swapchain_impl::get_back_buffer(uint32_t index)
{
	assert(index == 0);

	return _side_by_side_texture;
}

void reshade::openvr::swapchain_impl::set_color_space(vr::EColorSpace color_space)
{
	switch (color_space)
	{
	default:
	case vr::ColorSpace_Auto:
		_back_buffer_color_space = api::color_space::unknown;
		break;
	case vr::ColorSpace_Gamma:
		_back_buffer_color_space = api::color_space::srgb_nonlinear;
		break;
	case vr::ColorSpace_Linear:
		_back_buffer_color_space = api::color_space::extended_srgb_linear;
		break;
	}
}

reshade::api::rect reshade::openvr::swapchain_impl::get_eye_rect(vr::EVREye eye) const
{
	const api::resource_desc desc = _device->get_resource_desc(_side_by_side_texture);

	return api::rect {
		static_cast<int32_t>(eye * (desc.texture.width / 2)), 0,
		static_cast<int32_t>((eye + 1) * (desc.texture.width / 2)), static_cast<int32_t>(desc.texture.height)
	};
}
reshade::api::subresource_box reshade::openvr::swapchain_impl::get_eye_subresource_box(vr::EVREye eye) const
{
	const api::resource_desc desc = _device->get_resource_desc(_side_by_side_texture);

	return api::subresource_box {
		eye * (desc.texture.width / 2), 0, 0,
		(eye + 1) * (desc.texture.width / 2), desc.texture.height, 1
	};
}

bool reshade::openvr::swapchain_impl::on_init()
{
	// Created in 'on_vr_submit' below
	assert(_side_by_side_texture != 0);

#if RESHADE_ADDON
	invoke_addon_event<addon_event::init_swapchain>(this);
#endif

	init_effect_runtime(this);

	return true;
}
void reshade::openvr::swapchain_impl::on_reset()
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

bool reshade::openvr::swapchain_impl::on_vr_submit(api::command_queue *queue, vr::EVREye eye, api::resource eye_texture, vr::EColorSpace color_space, const vr::VRTextureBounds_t *bounds, uint32_t layer)
{
	assert(eye < 2 && eye_texture != 0);

	const api::resource_desc source_desc = _device->get_resource_desc(eye_texture);

	if (source_desc.texture.samples > 1 && !_device->check_capability(api::device_caps::resolve_region))
		return false; // When the resource is multisampled, can only copy whole subresources 

	reshade::api::subresource_box source_box;
	if (bounds != nullptr)
	{
		source_box.left  = static_cast<uint32_t>(std::floor(source_desc.texture.width * std::min(bounds->uMin, bounds->uMax)));
		source_box.top   = static_cast<uint32_t>(std::floor(source_desc.texture.height * std::min(bounds->vMin, bounds->vMax)));
		source_box.front = 0;
		source_box.right  = static_cast<uint32_t>(std::ceil(source_desc.texture.width * std::max(bounds->uMin, bounds->uMax)));
		source_box.bottom = static_cast<uint32_t>(std::ceil(source_desc.texture.height * std::max(bounds->vMin, bounds->vMax)));
		source_box.back   = 1;
	}
	else
	{
		source_box.left  = 0;
		source_box.top   = 0;
		source_box.front = 0;
		source_box.right  = source_desc.texture.width;
		source_box.bottom = source_desc.texture.height;
		source_box.back   = 1;
	}

	const uint32_t region_width = source_box.width();
	const uint32_t target_width = region_width * 2;
	const uint32_t region_height = source_box.height();

	if (region_width == 0 || region_height == 0)
		return false;

	set_color_space(color_space);

	const api::resource_desc target_desc = _side_by_side_texture != 0 ? _device->get_resource_desc(_side_by_side_texture) : api::resource_desc();

	// Due to rounding errors with the bounds we have to use a tolerance of 1 pixel per eye (2 pixels in total)
	const auto width_difference = std::abs(static_cast<int32_t>(target_width) - static_cast<int32_t>(target_desc.texture.width));
	const auto height_difference = std::abs(static_cast<int32_t>(region_height) - static_cast<int32_t>(target_desc.texture.height));

	if (width_difference > 2 || height_difference > 2 || api::format_to_typeless(source_desc.texture.format) != api::format_to_typeless(target_desc.texture.format))
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
			return false;
		}

		_device->set_resource_name(_side_by_side_texture, "ReShade side-by-side texture");

		if (!on_init())
			return false;
	}

	api::command_list *const cmd_list = queue->get_immediate_command_list();

	// Copy region of the source texture (in case of an array texture, copy from the layer corresponding to the current eye)
	const api::subresource_box dest_box = get_eye_subresource_box(eye);

	if (source_desc.texture.depth_or_layers <= 1)
		layer = 0;

	const bool is_d3d12 = _device->get_api() == api::device_api::d3d12;

	if (source_desc.texture.samples <= 1)
	{
		// In all but D3D12 the eye texture resource is already in copy source state at this point
		// See https://github.com/ValveSoftware/openvr/wiki/Vulkan#image-layout
		if (is_d3d12)
			cmd_list->barrier(eye_texture, api::resource_usage::shader_resource_pixel, api::resource_usage::copy_source);
		cmd_list->barrier(_side_by_side_texture, api::resource_usage::general, api::resource_usage::copy_dest);

		cmd_list->copy_texture_region(eye_texture, layer, &source_box, _side_by_side_texture, 0, &dest_box, api::filter_mode::min_mag_mip_point);

		cmd_list->barrier(_side_by_side_texture, api::resource_usage::copy_dest, api::resource_usage::general);
		if (is_d3d12)
			cmd_list->barrier(eye_texture, api::resource_usage::copy_source, api::resource_usage::shader_resource_pixel);
	}
	else
	{
		if (is_d3d12)
			cmd_list->barrier(eye_texture, api::resource_usage::shader_resource_pixel, api::resource_usage::resolve_source);
		cmd_list->barrier(_side_by_side_texture, api::resource_usage::general, api::resource_usage::resolve_dest);

		cmd_list->resolve_texture_region(eye_texture, layer, &source_box, _side_by_side_texture, 0, dest_box.left, dest_box.top, dest_box.front, source_desc.texture.format);

		cmd_list->barrier(_side_by_side_texture, api::resource_usage::resolve_dest, api::resource_usage::general);
		if (is_d3d12)
			cmd_list->barrier(eye_texture, api::resource_usage::resolve_source, api::resource_usage::shader_resource_pixel);
	}

#if RESHADE_ADDON
	const reshade::api::rect eye_rect = get_eye_rect(eye);
	invoke_addon_event<reshade::addon_event::present>(queue, this, &eye_rect, &eye_rect, 0, nullptr);
#endif

	if (eye == vr::Eye_Right)
		reshade::present_effect_runtime(this, queue);

	return true;
}
