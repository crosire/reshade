/*
 * Copyright (C) 2022 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "runtime.hpp"
#include "addon_manager.hpp"
#include <openvr.h>

struct D3D10Device;
struct D3D11Device;
struct D3D12CommandQueue;

namespace reshade::openvr
{
	class swapchain_impl : public api::api_object_impl<vr::IVRCompositor *, runtime>
	{
	public:
		swapchain_impl(D3D10Device *device, vr::IVRCompositor *compositor);
		swapchain_impl(D3D11Device *device, vr::IVRCompositor *compositor);
		swapchain_impl(D3D12CommandQueue *queue, vr::IVRCompositor *compositor);
		swapchain_impl(api::device *device, api::command_queue *graphics_queue, vr::IVRCompositor *compositor);
		~swapchain_impl();

		api::resource get_back_buffer(uint32_t index = 0) final;

		uint32_t get_back_buffer_count() const final { return 1; }
		uint32_t get_current_back_buffer_index() const final { return 0; }

		api::rect get_eye_rect(vr::EVREye eye) const;
		api::subresource_box get_eye_subresource_box(vr::EVREye eye) const;

		bool on_init();
		void on_reset();

		void on_present();
		bool on_vr_submit(vr::EVREye eye, api::resource eye_texture, const vr::VRTextureBounds_t *bounds, uint32_t layer);

#if RESHADE_ADDON && RESHADE_FX
		void render_effects(api::command_list *cmd_list, api::resource_view rtv, api::resource_view rtv_srgb) final;
		void render_technique(api::effect_technique handle, api::command_list *cmd_list, api::resource_view rtv, api::resource_view rtv_srgb) final;
#endif

	private:
		api::resource _side_by_side_texture = {};
		void *_app_state = nullptr;
		void *_direct3d_device = nullptr;
	};
}
