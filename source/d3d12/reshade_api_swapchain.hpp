/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include "runtime.hpp"
#include "reshade_api_device.hpp"
#include "reshade_api_command_queue.hpp"

namespace reshade::d3d12
{
	class swapchain_impl : public api::api_object_impl<IDXGISwapChain3 *, runtime>
	{
	public:
		swapchain_impl(device_impl *device, command_queue_impl *queue, IDXGISwapChain3 *swapchain);
		~swapchain_impl();

		api::device *get_device() final { return _device_impl; }
		api::command_queue *get_command_queue() final { return _cmd_queue_impl; }

		void get_current_back_buffer(api::resource *out) final
		{
			*out = { (uintptr_t)_backbuffers[_swap_index].get() };
		}
		void get_current_back_buffer_target(bool srgb, api::resource_view *out) final
		{
			*out = { _backbuffer_rtvs->GetCPUDescriptorHandleForHeapStart().ptr + (_swap_index * 2 + (srgb ? 1 : 0)) * _device_impl->_descriptor_handle_size[D3D12_DESCRIPTOR_HEAP_TYPE_RTV] };
		}

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
