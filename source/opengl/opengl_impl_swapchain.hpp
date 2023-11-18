/*
 * Copyright (C) 2014 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "opengl_impl_device.hpp"
#include "opengl_impl_render_context.hpp"
#include <unordered_set>

namespace reshade::opengl
{
	class swapchain_base : public api::swapchain
	{
	public:
		swapchain_base(device_impl *device) : _device_impl(device) {}

		uint64_t get_native() const final { return reinterpret_cast<uintptr_t>(*_hdcs.begin()); } // Simply return the first device context

		void get_private_data(const uint8_t guid[16], uint64_t *data) const final { _device_impl->get_private_data(guid, data); }
		void set_private_data(const uint8_t guid[16], const uint64_t data)  final { _device_impl->set_private_data(guid, data); }

		api::device *get_device() final { return _device_impl; }

		std::unordered_set<HDC> _hdcs;

	private:
		device_impl *const _device_impl;
	};

	class swapchain_impl : public device_impl, public render_context_impl, public swapchain_base
	{
	public:
		static inline swapchain_impl *from_context(render_context_impl *render_context) { return render_context != nullptr ? static_cast<swapchain_impl *>(render_context->get_device()) : nullptr; }

		swapchain_impl(HDC hdc, HGLRC initial_hglrc, bool compatibility_context = false);

		void *get_hwnd() const final { return _hwnd; }

		inline int get_pixel_format() const { return _pixel_format; }

		api::resource get_back_buffer(uint32_t index = 0) final;

		uint32_t get_back_buffer_count() const final { return 1; }
		uint32_t get_current_back_buffer_index() const final { return 0; }

		bool check_color_space_support(api::color_space color_space) const final { return color_space == api::color_space::srgb_nonlinear || color_space == api::color_space::extended_srgb_linear; }

		api::color_space get_color_space() const final { return api::color_space::unknown; }

		void on_init(HWND hwnd, unsigned int width, unsigned int height);
		void on_reset();
		void on_present(HDC hdc);

		void destroy_resource_view(api::resource_view handle) final;

	private:
		HWND _hwnd = nullptr;
	};
}
