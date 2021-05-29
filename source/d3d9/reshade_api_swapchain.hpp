/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include "runtime.hpp"
#include "reshade_api_device.hpp"
#include "state_block.hpp"

namespace reshade::d3d9
{
	class swapchain_impl : public api::api_object_impl<IDirect3DSwapChain9 *, runtime>
	{
	public:
		swapchain_impl(device_impl *device, IDirect3DSwapChain9 *swapchain);
		~swapchain_impl();

		api::device *get_device() final { return _device_impl; }
		api::command_queue *get_command_queue() final { return _device_impl; }

		void get_current_back_buffer(api::resource *out) final
		{
			*out = { reinterpret_cast<uintptr_t>(_backbuffer_resolved.get()) };
		}
		void get_current_back_buffer_target(bool srgb, api::resource_view *out) final
		{
			*out = { reinterpret_cast<uintptr_t>(_backbuffer_resolved.get()) | (srgb ? 1 : 0) };
		}

		bool on_init();
		void on_reset();
		void on_present();

	private:
		device_impl *const _device_impl;

		state_block _app_state;

		com_ptr<IDirect3DSurface9> _backbuffer;
		com_ptr<IDirect3DSurface9> _backbuffer_resolved;
	};
}
