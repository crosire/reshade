/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include "runtime.hpp"
#include "reshade_api_device.hpp"
#include "state_block_d3d9.hpp"

namespace reshade::d3d9
{
	class runtime_impl : public api::api_object_impl<IDirect3DSwapChain9 *, runtime>
	{
	public:
		runtime_impl(device_impl *device, IDirect3DSwapChain9 *swapchain);
		~runtime_impl();

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

		bool compile_effect(effect &effect, api::shader_stage type, const std::string &entry_point, std::vector<char> &cso) final;

	private:
		device_impl *const _device_impl;

		state_block _app_state;

		com_ptr<IDirect3DSurface9> _backbuffer;
		com_ptr<IDirect3DSurface9> _backbuffer_resolved;

		HMODULE _d3d_compiler = nullptr;
	};
}
