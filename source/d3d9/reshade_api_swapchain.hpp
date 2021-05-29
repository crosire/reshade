/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include "runtime.hpp"
#include "state_block.hpp"

namespace reshade::d3d9
{
	class device_impl;

	class swapchain_impl : public api::api_object_impl<IDirect3DSwapChain9 *, runtime>
	{
	public:
		swapchain_impl(device_impl *device, IDirect3DSwapChain9 *swapchain);
		~swapchain_impl();

		void get_current_back_buffer(api::resource *out) final;
		void get_current_back_buffer_target(bool srgb, api::resource_view *out) final;

		bool on_init();
		void on_reset();
		void on_present();

	private:
		state_block _app_state;

		com_ptr<IDirect3DSurface9> _backbuffer;
		com_ptr<IDirect3DSurface9> _backbuffer_resolved;
	};
}
