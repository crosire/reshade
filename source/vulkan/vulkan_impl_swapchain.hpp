/*
 * Copyright (C) 2014 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "runtime.hpp"

namespace reshade::vulkan
{
	class device_impl;
	class command_queue_impl;

	class swapchain_impl : public api::api_object_impl<VkSwapchainKHR, runtime>
	{
		static const uint32_t NUM_SYNC_SEMAPHORES = 4;

	public:
		swapchain_impl(device_impl *device, command_queue_impl *graphics_queue);
		~swapchain_impl();

		api::resource get_back_buffer(uint32_t index) final;

		uint32_t get_back_buffer_count() const final;
		uint32_t get_current_back_buffer_index() const final;

		void set_current_back_buffer_index(uint32_t index);

		bool on_init(VkSwapchainKHR swapchain, const VkSwapchainCreateInfoKHR &desc, HWND hwnd);
		void on_reset();

		void on_present(VkQueue queue, VkSemaphore *wait_semaphores, uint32_t &num_wait_semaphores);

	private:
		uint32_t _swap_index = 0;
		std::vector<VkImage> _swapchain_images;
		uint32_t _queue_sync_index = 0;
		VkSemaphore _queue_sync_semaphores[NUM_SYNC_SEMAPHORES] = {};
	};

	template <>
	struct object_data<VK_OBJECT_TYPE_SWAPCHAIN_KHR> : public swapchain_impl
	{
		using Handle = VkSwapchainKHR;

		object_data(device_impl *device, command_queue_impl *graphics_queue) : swapchain_impl(device, graphics_queue) {}
	};
}
