/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include "runtime.hpp"
#include "state_block.hpp"

namespace reshade::d3d11
{
	class device_impl;
	class device_context_impl;

	class swapchain_impl : public api::api_object_impl<IDXGISwapChain *, runtime>
	{
	public:
		swapchain_impl(device_impl *device, device_context_impl *immediate_context, IDXGISwapChain *swapchain);
		~swapchain_impl();

		void get_current_back_buffer(api::resource *out) final;
		void get_current_back_buffer_target(bool srgb, api::resource_view *out) final;

		bool on_init();
		bool on_init(const DXGI_SWAP_CHAIN_DESC &desc);
		void on_reset();
		void on_present();
		bool on_layer_submit(UINT eye, ID3D11Texture2D *source, const float bounds[4], ID3D11Texture2D **target);

	private:
		state_block _app_state;

		com_ptr<ID3D11Texture2D> _backbuffer;
		com_ptr<ID3D11Texture2D> _backbuffer_resolved;
		com_ptr<ID3D11RenderTargetView> _backbuffer_rtv[3];
		com_ptr<ID3D11Texture2D> _backbuffer_texture;
		com_ptr<ID3D11ShaderResourceView> _backbuffer_texture_srv;
	};
}
