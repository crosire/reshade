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

	private:
		device_impl *const _device_impl;
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

	private:
		device_impl *const _device_impl;

	protected:
		std::vector<com_ptr<ID3D12Resource>> _back_buffers;
		HWND _hwnd = nullptr;
		UINT _swap_index = 0;
	};
}
