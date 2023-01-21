/*
 * Copyright (C) 2014 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "runtime.hpp"
#include "opengl_impl_device.hpp"
#include "opengl_impl_render_context.hpp"
#include "opengl_impl_state_block.hpp"

namespace reshade::opengl
{
	class swapchain_base : public runtime
	{
	public:
		swapchain_base(device_impl *device, render_context_impl *render_context) : runtime(device, render_context) {}

		uint64_t get_native() const final { return reinterpret_cast<uintptr_t>(*_hdcs.begin()); } // Simply return the first device context

		void get_private_data(const uint8_t guid[16], uint64_t *data) const final { _device->get_private_data(guid, data); }
		void set_private_data(const uint8_t guid[16], const uint64_t data)  final { _device->set_private_data(guid, data); }

		std::unordered_set<HDC> _hdcs;
	};

	class swapchain_impl : public device_impl, public render_context_impl, public swapchain_base
	{
	public:
		static inline swapchain_impl *from_context(render_context_impl *render_context) { return render_context != nullptr ? static_cast<swapchain_impl *>(render_context->get_device()) : nullptr; }

		swapchain_impl(HDC hdc, HGLRC initial_hglrc, bool compatibility_context = false);
		~swapchain_impl();

		int get_pixel_format() const { return _pixel_format; }

		api::resource get_back_buffer(uint32_t index = 0) final;

		uint32_t get_back_buffer_count() const final { return 1; }
		uint32_t get_current_back_buffer_index() const final { return 0; }

		bool on_init(HWND hwnd, unsigned int width, unsigned int height);
		void on_reset();

		void on_present();

#if RESHADE_ADDON && RESHADE_FX
		void render_effects(api::command_list *cmd_list, api::resource_view rtv, api::resource_view rtv_srgb) final;
		void render_technique(api::effect_technique handle, api::command_list *cmd_list, api::resource_view rtv, api::resource_view rtv_srgb) final;
#endif

		void destroy_resource_view(api::resource_view handle) final;

	private:
		state_block _app_state;
	};
}
