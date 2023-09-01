/*
 * Copyright (C) 2022 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "openvr_impl_swapchain.hpp"
#include "d3d10/d3d10_device.hpp"
#include "d3d10/d3d10_impl_state_block.hpp"
#include "d3d11/d3d11_device.hpp"
#include "d3d11/d3d11_device_context.hpp"
#include "d3d11/d3d11_impl_state_block.hpp"
#include "d3d12/d3d12_device.hpp"
#include "d3d12/d3d12_command_queue.hpp"
#include "opengl/opengl_impl_swapchain.hpp"
#include "opengl/opengl_impl_state_block.hpp"
#include "dll_log.hpp"

reshade::openvr::swapchain_impl::swapchain_impl(D3D10Device *device, vr::IVRCompositor *compositor) :
	swapchain_impl(device, device, compositor)
{
	_app_state = new d3d10::state_block(device->_orig);

	_direct3d_device = static_cast<ID3D10Device *>(device);
	// Explicitly add a reference to the device, to ensure it stays valid for the lifetime of this swap chain object
	static_cast<IUnknown *>(_direct3d_device)->AddRef();
}

reshade::openvr::swapchain_impl::swapchain_impl(D3D11Device *device, vr::IVRCompositor *compositor) :
	swapchain_impl(device, device->_immediate_context, compositor)
{
	_app_state = new d3d11::state_block(device->_orig);

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
	api_object_impl(compositor, device, graphics_queue)
{
	_is_vr = true;
	_renderer_id = static_cast<unsigned int>(device->get_api());

	if (static_cast<api::device_api>(_renderer_id) == api::device_api::opengl)
		_app_state = new opengl::state_block();
}

reshade::openvr::swapchain_impl::~swapchain_impl()
{
	extern thread_local reshade::opengl::render_context_impl *g_current_context;
	// Do not access '_device' object to check the device API, in case it was already destroyed
	if (static_cast<api::device_api>(_renderer_id) == api::device_api::opengl && g_current_context == nullptr)
	{
		delete static_cast<opengl::state_block *>(_app_state);
		return; // Cannot clean up if OpenGL context was already destroyed
	}

	on_reset();

	switch (_device->get_api())
	{
	case api::device_api::d3d10:
		delete static_cast<d3d10::state_block *>(_app_state);
		break;
	case api::device_api::d3d11:
		delete static_cast<d3d11::state_block *>(_app_state);
		break;
	case api::device_api::opengl:
		delete static_cast<opengl::state_block *>(_app_state);
		break;
	}

	// Release the explicit reference to the device now that the effect runtime was destroyed and is longer referencing it
	if (_direct3d_device != nullptr)
		static_cast<IUnknown *>(_direct3d_device)->Release();
}

reshade::api::resource reshade::openvr::swapchain_impl::get_back_buffer(uint32_t index)
{
	assert(index == 0);

	return _side_by_side_texture;
}

reshade::api::rect reshade::openvr::swapchain_impl::get_eye_rect(vr::EVREye eye) const
{
	return api::rect {
		static_cast<int32_t>(eye * (_width / 2)), 0,
		static_cast<int32_t>((eye + 1) * (_width / 2)), static_cast<int32_t>(_height)
	};
}
reshade::api::subresource_box reshade::openvr::swapchain_impl::get_eye_subresource_box(vr::EVREye eye) const
{
	return api::subresource_box {
		static_cast<int32_t>(eye * (_width / 2)), 0, 0,
		static_cast<int32_t>((eye + 1) * (_width / 2)), static_cast<int32_t>(_height), 1
	};
}

bool reshade::openvr::swapchain_impl::on_init()
{
	// Created in 'on_vr_submit' below
	assert(_side_by_side_texture != 0);

#if RESHADE_ADDON
	invoke_addon_event<addon_event::init_swapchain>(this);
#endif

	return runtime::on_init(nullptr);
}
void reshade::openvr::swapchain_impl::on_reset()
{
	if (_side_by_side_texture == 0)
		return;

	runtime::on_reset();

#if RESHADE_ADDON
	invoke_addon_event<addon_event::destroy_swapchain>(this);
#endif

	// Make sure none of the resources below are currently in use
	_graphics_queue->wait_idle();

	_device->destroy_resource(_side_by_side_texture);
	_side_by_side_texture = {};
}

void reshade::openvr::swapchain_impl::on_present()
{
	if (!is_initialized())
		return;

	switch (_device->get_api())
	{
	case api::device_api::d3d10:
		static_cast<d3d10::state_block *>(_app_state)->capture();
		break;
	case api::device_api::d3d11:
		static_cast<d3d11::state_block *>(_app_state)->capture(reinterpret_cast<ID3D11DeviceContext *>(_graphics_queue->get_native()));
		break;
	case api::device_api::opengl:
		static_cast<opengl::state_block *>(_app_state)->capture(false);
		break;
	}

	runtime::on_present();

	switch (_device->get_api())
	{
	case api::device_api::d3d10:
		static_cast<d3d10::state_block *>(_app_state)->apply_and_release();
		break;
	case api::device_api::d3d11:
		static_cast<d3d11::state_block *>(_app_state)->apply_and_release();
		break;
	case api::device_api::opengl:
		static_cast<opengl::state_block *>(_app_state)->apply(false);
		break;
	}
}

bool reshade::openvr::swapchain_impl::on_vr_submit(vr::EVREye eye, api::resource eye_texture, const vr::VRTextureBounds_t *bounds, uint32_t layer)
{
	assert(eye < 2 && eye_texture != 0);

	const api::resource_desc source_desc = _device->get_resource_desc(eye_texture);

	if (source_desc.texture.samples > 1 && !_device->check_capability(api::device_caps::resolve_region))
		return false; // Can only copy whole subresources when the resource is multisampled

	reshade::api::subresource_box source_box;
	if (bounds != nullptr)
	{
		source_box.left  = static_cast<int32_t>(std::floor(source_desc.texture.width * std::min(bounds->uMin, bounds->uMax)));
		source_box.top   = static_cast<int32_t>(std::floor(source_desc.texture.height * std::min(bounds->vMin, bounds->vMax)));
		source_box.front = 0;
		source_box.right  = static_cast<int32_t>(std::ceil(source_desc.texture.width * std::max(bounds->uMin, bounds->uMax)));
		source_box.bottom = static_cast<int32_t>(std::ceil(source_desc.texture.height * std::max(bounds->vMin, bounds->vMax)));
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

	// Due to rounding errors with the bounds we have to use a tolerance of 1 pixel per eye (2 pixels in total)
	const  int32_t width_difference = std::abs(static_cast<int32_t>(target_width) - static_cast<int32_t>(_width));
	const  int32_t height_difference = std::abs(static_cast<int32_t>(region_height) - static_cast<int32_t>(_height));

	if (width_difference > 2 || height_difference > 2 || api::format_to_typeless(source_desc.texture.format) != api::format_to_typeless(_back_buffer_format))
	{
		LOG(INFO) << "Resizing runtime " << this << " in VR to " << target_width << "x" << region_height << " ...";

		on_reset();

		if (!_device->create_resource(
				api::resource_desc(target_width, region_height, 1, 1, api::format_to_typeless(source_desc.texture.format), 1, api::memory_heap::gpu_only, api::resource_usage::render_target | api::resource_usage::copy_source | api::resource_usage::copy_dest),
				nullptr, api::resource_usage::general, &_side_by_side_texture))
		{
			LOG(ERROR) << "Failed to create region texture!";
			return false;
		}

		if (!on_init())
			return false;
	}

	api::command_list *const cmd_list = _graphics_queue->get_immediate_command_list();

	// Copy region of the source texture (in case of an array texture, copy from the layer corresponding to the current eye)
	const api::subresource_box dest_box = get_eye_subresource_box(eye);

	if (source_desc.texture.depth_or_layers <= 1)
		layer = 0;

	const bool is_d3d12 = _device->get_api() == api::device_api::d3d12;

	if (source_desc.texture.samples <= 1)
	{
		// In all but D3D12 the eye texture resource is already in copy source state at this point
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

	return true;
}

#if RESHADE_ADDON && RESHADE_FX
void reshade::openvr::swapchain_impl::render_effects(api::command_list *cmd_list, api::resource_view rtv, api::resource_view rtv_srgb)
{
	if (!_is_in_present_call)
	{
		switch (_device->get_api())
		{
		case api::device_api::d3d10:
			static_cast<d3d10::state_block *>(_app_state)->capture();
			break;
		case api::device_api::d3d11:
			static_cast<d3d11::state_block *>(_app_state)->capture(reinterpret_cast<ID3D11DeviceContext *>(_graphics_queue->get_native()));
			break;
		case api::device_api::opengl:
			static_cast<opengl::state_block *>(_app_state)->capture(false);
			break;
		}
	}

	runtime::render_effects(cmd_list, rtv, rtv_srgb);

	if (!_is_in_present_call)
	{
		switch (_device->get_api())
		{
		case api::device_api::d3d10:
			static_cast<d3d10::state_block *>(_app_state)->apply_and_release();
			break;
		case api::device_api::d3d11:
			static_cast<d3d11::state_block *>(_app_state)->apply_and_release();
			break;
		case api::device_api::opengl:
			static_cast<opengl::state_block *>(_app_state)->apply(false);
			break;
		}
	}
}
void reshade::openvr::swapchain_impl::render_technique(api::effect_technique handle, api::command_list *cmd_list, api::resource_view rtv, api::resource_view rtv_srgb)
{
	if (!_is_in_present_call)
	{
		switch (_device->get_api())
		{
		case api::device_api::d3d10:
			static_cast<d3d10::state_block *>(_app_state)->capture();
			break;
		case api::device_api::d3d11:
			static_cast<d3d11::state_block *>(_app_state)->capture(reinterpret_cast<ID3D11DeviceContext *>(_graphics_queue->get_native()));
			break;
		case api::device_api::opengl:
			static_cast<opengl::state_block *>(_app_state)->capture(false);
			break;
		}
	}

	runtime::render_technique(handle, cmd_list, rtv, rtv_srgb);

	if (!_is_in_present_call)
	{
		switch (_device->get_api())
		{
		case api::device_api::d3d10:
			static_cast<d3d10::state_block *>(_app_state)->apply_and_release();
			break;
		case api::device_api::d3d11:
			static_cast<d3d11::state_block *>(_app_state)->apply_and_release();
			break;
		case api::device_api::opengl:
			static_cast<opengl::state_block *>(_app_state)->apply(false);
			break;
		}
	}
}
#endif
