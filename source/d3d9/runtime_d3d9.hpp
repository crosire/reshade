/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include "runtime.hpp"
#include "reshade_api_device.hpp"
#include "reshade_api_type_utils.hpp"
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

		bool on_init();
		void on_reset();
		void on_present();

		bool compile_effect(effect &effect, api::shader_stage type, const std::string &entry_point, std::vector<char> &cso) final;

		api::resource_view get_backbuffer(bool srgb) final { return { reinterpret_cast<uintptr_t>(_backbuffer_resolved.get()) | (srgb ? 1 : 0) }; }
		api::resource get_backbuffer_resource() final { return { reinterpret_cast<uintptr_t>(_backbuffer_resolved.get()) }; }
		api::format get_backbuffer_format() final { return convert_format(_backbuffer_format); }

	private:
		const com_ptr<IDirect3DDevice9> _device;
		device_impl *const _device_impl;

		state_block _app_state;

		D3DFORMAT _backbuffer_format = D3DFMT_UNKNOWN;
		com_ptr<IDirect3DSurface9> _backbuffer;
		com_ptr<IDirect3DSurface9> _backbuffer_resolved;

		HMODULE _d3d_compiler = nullptr;
	};
}
