/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include "runtime.hpp"
#include "reshade_api_device.hpp"
#include "reshade_api_command_queue.hpp"
#include "reshade_api_type_utils.hpp"

namespace reshade::vulkan
{
	class runtime_impl : public api::api_object_impl<VkSwapchainKHR, runtime>
	{
		static const uint32_t NUM_QUERY_FRAMES = 4;
		static const uint32_t MAX_IMAGE_DESCRIPTOR_SETS = 128; // TODO: Check if these limits are enough
		static const uint32_t MAX_EFFECT_DESCRIPTOR_SETS = 50 * 2 * 4; // 50 resources, 4 passes

	public:
		runtime_impl(device_impl *device, command_queue_impl *graphics_queue);
		~runtime_impl();

		api::device *get_device() final { return _device_impl; }
		api::command_queue *get_command_queue() final { return _queue_impl; }

		bool on_init(VkSwapchainKHR swapchain, const VkSwapchainCreateInfoKHR &desc, HWND hwnd);
		void on_reset();
		void on_present(VkQueue queue, const uint32_t swapchain_image_index, std::vector<VkSemaphore> &wait);
		bool on_layer_submit(uint32_t eye, VkImage source, const VkExtent2D &source_extent, VkFormat source_format, VkSampleCountFlags source_samples, uint32_t source_layer_index, const float bounds[4], VkImage *target_image);

		bool capture_screenshot(uint8_t *buffer) const final;

		bool compile_effect(effect &effect, api::shader_stage type, const std::string &entry_point, api::shader_module &out) final;

		api::resource_view get_backbuffer(bool srgb) final { return { (uint64_t)_swapchain_views[_swap_index * 2 + (srgb ? 1 : 0)] }; }
		api::resource get_backbuffer_resource() final { return { (uint64_t)_swapchain_images[_swap_index] }; }
		api::format get_backbuffer_format() final { return convert_format(_backbuffer_format); }

	private:
		device_impl *const _device_impl;
		const VkDevice _device;
		command_queue_impl *const _queue_impl;
		VkQueue _queue = VK_NULL_HANDLE;
		command_list_immediate_impl *const _cmd_impl;

		uint32_t _queue_sync_index = 0;
		VkSemaphore _queue_sync_semaphores[NUM_QUERY_FRAMES] = {};

		uint32_t _swap_index = 0;
		VkFormat _backbuffer_format = VK_FORMAT_UNDEFINED;
		std::vector<VkImage> _swapchain_images;
		std::vector<VkImageView> _swapchain_views;
	};
}
