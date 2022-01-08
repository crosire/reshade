/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include "runtime.hpp"
#include "d3d11_impl_state_block.hpp"

namespace reshade::d3d11
{
	class device_impl;
	class device_context_impl;

	class swapchain_impl : public api::api_object_impl<IDXGISwapChain *, runtime>
	{
	public:
		swapchain_impl(device_impl *device, device_context_impl *immediate_context, IDXGISwapChain *swapchain);
		~swapchain_impl();

		api::resource get_back_buffer(uint32_t index) final;
		api::resource get_back_buffer_resolved(uint32_t index) final;

		uint32_t get_back_buffer_count() const final { return 1; }
		uint32_t get_current_back_buffer_index() const final { return 0; }

		bool on_init();
		void on_reset();

		void on_present();
		bool on_vr_submit(UINT eye, ID3D11Texture2D *source, const float bounds[4], ID3D11Texture2D **target);

#if RESHADE_FX
		void render_effects(api::command_list *cmd_list, api::resource_view rtv, api::resource_view rtv_srgb) final;
#endif

	private:
		state_block _app_state;
		com_ptr<ID3D11Texture2D> _backbuffer;
		com_ptr<ID3D11Texture2D> _backbuffer_resolved;
		com_ptr<ID3D11RenderTargetView> _backbuffer_rtv;
		com_ptr<ID3D11ShaderResourceView> _backbuffer_resolved_srv;
	};
}
