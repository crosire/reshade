/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include "runtime.hpp"
#include "opengl_impl_device.hpp"
#include "opengl_impl_state_block.hpp"

namespace reshade::opengl
{
	class swapchain_impl : public device_impl, public runtime
	{
	public:
		swapchain_impl(HDC hdc, HGLRC hglrc, bool compatibility_context = false);
		~swapchain_impl();

		void get_private_data(const uint8_t guid[16], uint64_t *data) const final { device_impl::get_private_data(guid, data); }
		void set_private_data(const uint8_t guid[16], const uint64_t data)  final { device_impl::set_private_data(guid, data); }

		uint64_t get_native_object() const final { return reinterpret_cast<uintptr_t>(*_hdcs.begin()); } // Simply return the first device context

		api::resource get_back_buffer(uint32_t index) final;
		api::resource get_back_buffer_resolved(uint32_t index) final;

		uint32_t get_back_buffer_count() const final { return 1; }
		uint32_t get_current_back_buffer_index() const final { return 0; }

		bool on_init(HWND hwnd, unsigned int width, unsigned int height);
		void on_reset();

		void on_present();

#if RESHADE_FX
		void render_effects(api::command_list *cmd_list, api::resource_view rtv, api::resource_view rtv_srgb) final;
#endif

	private:
		state_block _app_state;
		GLuint _rbo = 0;
		GLuint _fbo = 0;
	};
}
