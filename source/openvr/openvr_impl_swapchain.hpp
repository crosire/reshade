/*
 * Copyright (C) 2022 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include "runtime.hpp"
#include "addon_manager.hpp"
#include <openvr.h>

namespace reshade::openvr
{
	class swapchain_impl : public api::api_object_impl<vr::IVRCompositor *, runtime>
	{
	public:
		swapchain_impl(api::device *device, api::command_queue *graphics_queue, vr::IVRCompositor *compositor);
		~swapchain_impl();

		api::resource get_back_buffer(uint32_t index) final;

		uint32_t get_back_buffer_count() const final;
		uint32_t get_current_back_buffer_index() const final;

		bool on_init();
		void on_reset();

		void on_present();
		bool on_vr_submit(vr::EVREye eye, api::resource eye_texture, const vr::VRTextureBounds_t *bounds, uint32_t layer);

#if RESHADE_FX
		void render_effects(api::command_list *cmd_list, api::resource_view rtv, api::resource_view rtv_srgb) final;
#endif

	private:
		void *_app_state = nullptr;
		api::resource _side_by_side_texture = {};
	};
}
