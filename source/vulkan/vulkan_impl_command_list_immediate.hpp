/*
 * Copyright (C) 2021 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include "vulkan_impl_command_list.hpp"

namespace reshade::vulkan
{
	class command_list_immediate_impl : public command_list_impl
	{
		static const uint32_t NUM_COMMAND_FRAMES = 4; // Use power of two so that modulo can be replaced with bitwise operation

	public:
		command_list_immediate_impl(device_impl *device, uint32_t queue_family_index);
		~command_list_immediate_impl();

		bool flush(VkQueue queue, std::vector<VkSemaphore> &wait_semaphores);
		bool flush_and_wait(VkQueue queue);

		const VkCommandBuffer begin_commands() { _has_commands = true; return _orig; }

	private:
		uint32_t _cmd_index = 0;
		VkCommandPool _cmd_pool = VK_NULL_HANDLE;
		VkFence _cmd_fences[NUM_COMMAND_FRAMES] = {};
		VkSemaphore _cmd_semaphores[NUM_COMMAND_FRAMES] = {};
		VkCommandBuffer _cmd_buffers[NUM_COMMAND_FRAMES] = {};
	};
}
