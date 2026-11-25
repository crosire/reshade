/*
 * Copyright (C) 2014 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <dxgi1_5.h>

struct ID3D12CommandQueueDownlevel;

namespace reshade::d3d12
{
	class device_impl;

	class swapchain_impl : public api::api_object_impl<IDXGISwapChain3 *, api::swapchain>
	{
	public:
		swapchain_impl(device_impl *device, IDXGISwapChain3 *swapchain);

		api::device *get_device() final;

		void *get_hwnd() const final;

		api::resource get_back_buffer(uint32_t index) final;

		uint32_t get_back_buffer_count() const final;
		uint32_t get_current_back_buffer_index() const final;

		bool check_color_space_support(api::color_space color_space) const final;

		api::color_space get_color_space() const final;

	protected:
		device_impl *const _device;
	};

	class swapchain_d3d12on7_impl : public api::api_object_impl<ID3D12CommandQueueDownlevel *, api::swapchain>
	{
	public:
		swapchain_d3d12on7_impl(device_impl *device, ID3D12CommandQueueDownlevel *command_queue_downlevel);

		api::device *get_device() final;

		void *get_hwnd() const final;

		api::resource get_back_buffer(uint32_t index) final;

		uint32_t get_back_buffer_count() const final;
		uint32_t get_current_back_buffer_index() const final;

		bool check_color_space_support(api::color_space color_space) const final { return color_space == api::color_space::srgb; }

		api::color_space get_color_space() const final { return api::color_space::unknown; }

	protected:
		device_impl *const _device;

		std::vector<com_ptr<ID3D12Resource>> _back_buffers;
		HWND _hwnd = nullptr;
		UINT _swap_index = 0;
	};
}

// SpecialK uses this private data GUID to track the current swap chain color space, so do the same
inline constexpr GUID SKID_SwapChainColorSpace = { 0x18b57e4, 0x1493, 0x4953, { 0xad, 0xf2, 0xde, 0x6d, 0x99, 0xcc, 0x5, 0xe5 } }; // {018B57E4-1493-4953-ADF2-DE6D99CC05E5}
