/*
 * Copyright (C) 2022 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "reshade_api_object_impl.hpp"
#include <openvr.h>

struct D3D10Device;
struct D3D11Device;
struct D3D12CommandQueue;

namespace reshade::openvr
{
	class swapchain_impl : public api::api_object_impl<vr::IVRCompositor *, api::swapchain>
	{
	public:
		swapchain_impl(D3D10Device *device, vr::IVRCompositor *compositor);
		swapchain_impl(D3D11Device *device, vr::IVRCompositor *compositor);
		swapchain_impl(D3D12CommandQueue *queue, vr::IVRCompositor *compositor);
		swapchain_impl(api::device *device, api::command_queue *graphics_queue, vr::IVRCompositor *compositor);
		~swapchain_impl();

		api::device *get_device() final;
		api::command_queue *get_command_queue() { return _graphics_queue; }

		void *get_hwnd() const final { return nullptr; }

		api::resource get_back_buffer(uint32_t index = 0) final;

		uint32_t get_back_buffer_count() const final { return 1; }
		uint32_t get_current_back_buffer_index() const final { return 0; }

		bool check_color_space_support(api::color_space color_space) const final { return color_space == api::color_space::srgb || color_space == api::color_space::scrgb; }

		api::color_space get_color_space() const final { return _back_buffer_color_space; }
		void set_color_space(vr::EColorSpace color_space);

		api::rect get_eye_rect(vr::EVREye eye) const;
		api::subresource_box get_eye_subresource_box(vr::EVREye eye) const;

		void on_init();
		void on_reset();

		bool on_vr_submit(vr::EVREye eye, api::resource eye_texture, vr::EColorSpace color_space, const vr::VRTextureBounds_t *bounds, uint32_t layer);

	private:
		api::device *const _device;
		api::command_queue *const _graphics_queue;
		api::resource _side_by_side_texture = {};
		void *_direct3d_device = nullptr;
		api::color_space _back_buffer_color_space = api::color_space::unknown;
	};
}
