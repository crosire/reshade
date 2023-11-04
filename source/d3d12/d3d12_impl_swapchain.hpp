/*
 * Copyright (C) 2014 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <dxgi1_5.h>

namespace reshade::d3d12
{
	class device_impl;
	class command_queue_impl;

	class swapchain_impl : public api::api_object_impl<IDXGISwapChain3 *, api::swapchain>
	{
	public:
		swapchain_impl(device_impl *device, command_queue_impl *queue, IDXGISwapChain3 *swapchain);
		~swapchain_impl();

		api::device *get_device() final;

		void *get_hwnd() const override;

		api::resource get_back_buffer(uint32_t index) final;

		uint32_t get_back_buffer_count() const final;
		uint32_t get_current_back_buffer_index() const override;

		bool check_color_space_support(api::color_space color_space) const final;

		api::color_space get_color_space() const final { return _back_buffer_color_space; }
		void set_color_space(DXGI_COLOR_SPACE_TYPE type);

		void on_init();
		void on_reset();

	protected:
		device_impl *const _device_impl;
		std::vector<com_ptr<ID3D12Resource>> _back_buffers;
		api::color_space _back_buffer_color_space = api::color_space::unknown;
	};

	class swapchain_d3d12on7_impl : public swapchain_impl
	{
	public:
		swapchain_d3d12on7_impl(device_impl *device, command_queue_impl *queue);

		void *get_hwnd() const final { return _hwnd; }

		uint32_t get_current_back_buffer_index() const final { return _swap_index; }

		bool on_present(ID3D12Resource *source, HWND hwnd);

	private:
		HWND _hwnd = nullptr;
		UINT _swap_index = 0;
	};
}
