/*
 * Copyright (C) 2014 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

namespace reshade::d3d11
{
	class device_impl;

	class swapchain_impl : public api::api_object_impl<IDXGISwapChain *, api::swapchain>
	{
	public:
		swapchain_impl(device_impl *device, IDXGISwapChain *swapchain);

		api::device *get_device() final;

		void *get_hwnd() const final;

		api::resource get_back_buffer(uint32_t index = 0) final;

		uint32_t get_back_buffer_count() const final { return 1; }
		uint32_t get_current_back_buffer_index() const final { return 0; }

		bool check_color_space_support(api::color_space color_space) const final;

		api::color_space get_color_space() const final;
		DXGI_COLOR_SPACE_TYPE get_color_space_native() const { return _color_space; }
		void set_color_space_native(DXGI_COLOR_SPACE_TYPE type) { _color_space = type; }

	private:
		device_impl *const _device_impl;
		DXGI_COLOR_SPACE_TYPE _color_space = DXGI_COLOR_SPACE_CUSTOM;
	};
}
