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
		swapchain_impl(HDC hdc, HGLRC hglrc);
		~swapchain_impl();

		bool get_user_data(const uint8_t guid[16], void **ptr) const final { return device_impl::get_user_data(guid, ptr); }
		void set_user_data(const uint8_t guid[16], void *const ptr)  final { device_impl::set_user_data(guid, ptr); }

		uint64_t get_native_object() const final { return reinterpret_cast<uintptr_t>(*_hdcs.begin()); } // Simply return the first device context

		void get_back_buffer(uint32_t index, api::resource *out) final;
		void get_back_buffer_resolved(uint32_t index, api::resource *out) final;

		uint32_t get_back_buffer_count() const final { return 1; }
		uint32_t get_current_back_buffer_index() const final { return 0; }

		bool on_init(HWND hwnd, unsigned int width, unsigned int height);
		void on_reset();

		void on_present(bool default_fbo = true);
		bool on_layer_submit(uint32_t eye, GLuint source_object, bool is_rbo, bool is_array, const float bounds[4], GLuint *target_rbo);

	private:
		state_block _app_state;
		GLuint _rbo = 0;
		GLuint _fbo[2] = {};
	};
}
