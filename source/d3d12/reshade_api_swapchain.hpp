/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include "runtime.hpp"

namespace reshade::d3d12
{
	class device_impl;
	class command_queue_impl;

	class swapchain_impl : public api::api_object_impl<IDXGISwapChain3 *, runtime>
	{
	public:
		swapchain_impl(device_impl *device, command_queue_impl *queue, IDXGISwapChain3 *swapchain);
		~swapchain_impl();

		api::device *get_device() final;
		api::command_queue *get_command_queue() final;

		void get_current_back_buffer(api::resource *out) final;
		void get_current_back_buffer_target(bool srgb, api::resource_view *out) final;

		bool on_init();
		bool on_init(const DXGI_SWAP_CHAIN_DESC &desc);
		void on_reset();
		void on_present();
		bool on_present(ID3D12Resource *source, HWND hwnd);
		bool on_layer_submit(UINT eye, ID3D12Resource *source, const float bounds[4], ID3D12Resource **target);

	private:
		device_impl *const _device_impl;
		command_queue_impl *const _cmd_queue_impl;

		UINT _swap_index = 0;
		std::vector<com_ptr<ID3D12Resource>> _backbuffers;
		com_ptr<ID3D12DescriptorHeap> _backbuffer_rtvs;
	};
}
