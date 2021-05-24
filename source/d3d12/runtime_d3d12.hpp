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
	class runtime_impl : public api::api_object_impl<IDXGISwapChain3 *, runtime>
	{
	public:
		runtime_impl(device_impl *device, command_queue_impl *queue, IDXGISwapChain3 *swapchain);
		~runtime_impl();

		api::device *get_device() final { return _device_impl; }
		api::command_queue *get_command_queue() final { return _cmd_queue_impl; }

		bool on_init();
		bool on_init(const DXGI_SWAP_CHAIN_DESC &desc);
		void on_reset();
		void on_present();
		bool on_present(ID3D12Resource *source, HWND hwnd);
		bool on_layer_submit(UINT eye, ID3D12Resource *source, const float bounds[4], ID3D12Resource **target);

		bool compile_effect(effect &effect, api::shader_stage type, const std::string &entry_point, std::vector<char> &cso) final;

		api::resource_view get_backbuffer(bool srgb) final { return { _backbuffer_rtvs->GetCPUDescriptorHandleForHeapStart().ptr + (_swap_index * 2 + (srgb ? 1 : 0)) * _device_impl->_descriptor_handle_size[D3D12_DESCRIPTOR_HEAP_TYPE_RTV] }; }
		api::resource get_backbuffer_resource() final { return { (uintptr_t)_backbuffers[_swap_index].get() }; }
		api::format get_backbuffer_format() final { return (api::format)_backbuffer_format; }

	private:
		const com_ptr<ID3D12Device> _device;
		const com_ptr<ID3D12CommandQueue> _cmd_queue;
		device_impl *const _device_impl;
		command_queue_impl *const _cmd_queue_impl;
		command_list_immediate_impl *const _cmd_impl;

		UINT _swap_index = 0;
		DXGI_FORMAT _backbuffer_format = DXGI_FORMAT_UNKNOWN;
		std::vector<com_ptr<ID3D12Resource>> _backbuffers;
		com_ptr<ID3D12DescriptorHeap> _backbuffer_rtvs;

		HMODULE _d3d_compiler = nullptr;
	};
}
