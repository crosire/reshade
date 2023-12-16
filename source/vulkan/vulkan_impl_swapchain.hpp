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
		friend VkResult VKAPI_CALL ::vkCreateSwapchainKHR(VkDevice device, const VkSwapchainCreateInfoKHR *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkSwapchainKHR *pSwapchain);
		friend VkResult VKAPI_CALL ::vkAcquireNextImageKHR(VkDevice device, VkSwapchainKHR swapchain, uint64_t timeout, VkSemaphore semaphore, VkFence fence, uint32_t *pImageIndex);
		friend VkResult VKAPI_CALL ::vkAcquireNextImage2KHR(VkDevice device, const VkAcquireNextImageInfoKHR *pAcquireInfo, uint32_t *pImageIndex);

	public:
		swapchain_impl(device_impl *device, VkSwapchainKHR swapchain, const VkSwapchainCreateInfoKHR &create_info, HWND hwnd);

		api::device *get_device() final;

		void *get_hwnd() const final;

		api::resource get_back_buffer(uint32_t index) final;

		uint32_t get_back_buffer_count() const final;
		uint32_t get_current_back_buffer_index() const final;

		bool check_color_space_support(api::color_space color_space) const final;

		api::color_space get_color_space() const final;

	private:
		device_impl *const _device_impl;

	protected:
		VkSwapchainCreateInfoKHR _create_info = { VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
		HWND _hwnd = nullptr;
		uint32_t _swap_index = 0;
	};

	template <>
	struct object_data<VK_OBJECT_TYPE_SWAPCHAIN_KHR> : public swapchain_impl
	{
		using Handle = VkSwapchainKHR;

		object_data(device_impl *device, VkSwapchainKHR swapchain, const VkSwapchainCreateInfoKHR &create_info, HWND hwnd) : swapchain_impl(device, swapchain, create_info, hwnd) {}
	};
}
