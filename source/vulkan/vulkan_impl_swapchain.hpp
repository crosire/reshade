/*
 * Copyright (C) 2014 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

namespace reshade::vulkan
{
	class device_impl;

	class swapchain_impl : public api::api_object_impl<VkSwapchainKHR, api::swapchain>
	{
	public:
		swapchain_impl(device_impl *device);
		~swapchain_impl();

		api::device *get_device() final;

		void *get_hwnd() const final { return _hwnd; }

		api::resource get_back_buffer(uint32_t index) final;

		uint32_t get_back_buffer_count() const final;
		uint32_t get_current_back_buffer_index() const final;
		void set_current_back_buffer_index(uint32_t index);

		bool check_color_space_support(api::color_space color_space) const final;

		api::color_space get_color_space() const final;

		void on_init(VkSwapchainKHR swapchain, const VkSwapchainCreateInfoKHR &create_info, HWND hwnd);
		void on_reset();

	private:
		device_impl *const _device_impl;
		uint32_t _swap_index = 0;
		std::vector<VkImage> _swapchain_images;
		VkSwapchainCreateInfoKHR _create_info = { VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
		HWND _hwnd = nullptr;
	};

	template <>
	struct object_data<VK_OBJECT_TYPE_SWAPCHAIN_KHR> : public swapchain_impl
	{
		using Handle = VkSwapchainKHR;

		object_data(device_impl *device) : swapchain_impl(device) {}
	};
}
