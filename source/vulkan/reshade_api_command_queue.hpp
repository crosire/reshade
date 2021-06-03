/*
 * Copyright (C) 2021 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include "reshade_api_command_list_immediate.hpp"

namespace reshade::vulkan
{
	class command_queue_impl : public api::api_object_impl<VkQueue, api::command_queue>
	{
	public:
		command_queue_impl(device_impl *device, uint32_t queue_family_index, const VkQueueFamilyProperties &queue_family, VkQueue queue);
		~command_queue_impl();

		api::device *get_device() final;

		api::command_list *get_immediate_command_list() final { return _immediate_cmd_list; }

		void flush_immediate_command_list() const final;
		void flush_immediate_command_list(std::vector<VkSemaphore> &wait_semaphores) const;

		void wait_idle() const final;

		void begin_debug_event(const char *label, const float color[4]) final;
		void finish_debug_event() final;
		void insert_debug_marker(const char *label, const float color[4]) final;

	private:
		device_impl *const _device_impl;
		command_list_immediate_impl *_immediate_cmd_list = nullptr;
	};
}
