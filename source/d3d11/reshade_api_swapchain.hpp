/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include "runtime.hpp"
#include "reshade_api_device.hpp"
#include "reshade_api_device_context.hpp"
#include "state_block.hpp"

namespace reshade::d3d11
{
	class swapchain_impl : public api::api_object_impl<IDXGISwapChain *, runtime>
	{
	public:
		swapchain_impl(device_impl *device, device_context_impl *immediate_context, IDXGISwapChain *swapchain);
		~swapchain_impl();

		api::device *get_device() final { return _device_impl; }
		api::command_queue *get_command_queue() final { return _immediate_context_impl; }

		void get_current_back_buffer(api::resource *out) final
		{
			*out = { (uintptr_t)_backbuffer_resolved.get() };
		}
		void get_current_back_buffer_target(bool srgb, api::resource_view *out) final
		{
			*out = { reinterpret_cast<uintptr_t>(_backbuffer_rtv[srgb ? 1 : 0].get()) };
		}

		bool on_init();
		bool on_init(const DXGI_SWAP_CHAIN_DESC &desc);
		void on_reset();
		void on_present();
		bool on_layer_submit(UINT eye, ID3D11Texture2D *source, const float bounds[4], ID3D11Texture2D **target);

	private:
		device_impl *const _device_impl;
		device_context_impl *const _immediate_context_impl;

		state_block _app_state;

		com_ptr<ID3D11Texture2D> _backbuffer;
		com_ptr<ID3D11Texture2D> _backbuffer_resolved;
		com_ptr<ID3D11RenderTargetView> _backbuffer_rtv[3];
		com_ptr<ID3D11Texture2D> _backbuffer_texture;
		com_ptr<ID3D11ShaderResourceView> _backbuffer_texture_srv;
	};
}
