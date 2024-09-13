/*
 * Copyright (C) 2021 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "vulkan_impl_command_list.hpp"

namespace reshade::vulkan
{
	class command_list_immediate_impl : public command_list_impl
	{
		static constexpr uint32_t NUM_COMMAND_FRAMES = 4; // Use power of two so that modulo can be replaced with bitwise operation

	public:
		static thread_local command_list_immediate_impl *s_last_immediate_command_list;

		command_list_immediate_impl(device_impl *device, uint32_t queue_family_index, VkQueue queue);
		~command_list_immediate_impl();

		bool flush(VkSubmitInfo &semaphore_info);
		bool flush_and_wait();

	private:
		const VkQueue _parent_queue;
		uint32_t _cmd_index = 0;
		VkCommandPool _cmd_pool = VK_NULL_HANDLE;
		VkFence _cmd_fences[NUM_COMMAND_FRAMES] = {};
		VkSemaphore _cmd_semaphores[NUM_COMMAND_FRAMES] = {};
		VkCommandBuffer _cmd_buffers[NUM_COMMAND_FRAMES] = {};
	};
}
