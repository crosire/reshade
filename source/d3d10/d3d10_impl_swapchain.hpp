/*
 * Copyright (C) 2014 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "runtime.hpp"

namespace reshade::d3d10
{
	class device_impl;

	class swapchain_impl : public api::api_object_impl<IDXGISwapChain *, runtime>
	{
	public:
		swapchain_impl(device_impl *device, IDXGISwapChain *swapchain);
		~swapchain_impl();

		api::resource get_back_buffer(uint32_t index = 0) final;

		uint32_t get_back_buffer_count() const final { return 1; }
		uint32_t get_current_back_buffer_index() const final { return 0; }

		bool check_color_space_support(api::color_space color_space) const final { return color_space == api::color_space::srgb_nonlinear || color_space == api::color_space::extended_srgb_linear; }

		api::color_space get_color_space() const final { return api::color_space::unknown; }

		bool on_init();
		void on_reset();

		using runtime::on_present;

	private:
		com_ptr<ID3D10Texture2D> _back_buffer;
	};
}
