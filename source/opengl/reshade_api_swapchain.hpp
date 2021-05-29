/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include "runtime.hpp"
#include "reshade_api_device.hpp"
#include "reshade_api_type_utils.hpp"
#include "state_block.hpp"

namespace reshade::opengl
{
	class swapchain_impl : public runtime, public device_impl
	{
	public:
		swapchain_impl(HDC hdc, HGLRC hglrc);
		~swapchain_impl();

		bool get_user_data(const uint8_t guid[16], void **ptr) const final { return device_impl::get_user_data(guid, ptr); }
		void set_user_data(const uint8_t guid[16], void *const ptr)  final { device_impl::set_user_data(guid, ptr); }

		uint64_t get_native_object() const final { return reinterpret_cast<uintptr_t>(*_hdcs.begin()); } // Simply return the first device context

		api::device *get_device() final { return this; }
		api::command_queue *get_command_queue() final { return this; }

		void get_current_back_buffer(api::resource *out) final
		{
#if 0
			*out = make_resource_handle(GL_FRAMEBUFFER_DEFAULT, GL_BACK);
#else
			*out = make_resource_handle(GL_RENDERBUFFER, _rbo);
#endif
		}
		void get_current_back_buffer_target(bool srgb, api::resource_view *out) final
		{
#if 0
			*out = make_resource_view_handle(GL_FRAMEBUFFER_DEFAULT, GL_BACK, srgb ? 0x2 : 0);
#else
			*out = make_resource_view_handle(GL_RENDERBUFFER, _rbo, srgb ? 0x2 : 0);
#endif
		}

		bool on_init(HWND hwnd, unsigned int width, unsigned int height);
		void on_reset();
		void on_present(bool default_fbo = true);
		bool on_layer_submit(uint32_t eye, GLuint source_object, bool is_rbo, bool is_array, const float bounds[4], GLuint *target_rbo);

	private:
		state_block _app_state;
		GLuint _rbo = 0;
		GLuint _fbo[2] = {}, _current_fbo = 0;
		std::vector<GLuint> _reserved_texture_names;
	};
}
