/*
 * Copyright (C) 2021 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "vulkan_impl_command_list_immediate.hpp"
#include <mutex>

namespace reshade::vulkan
{
	class command_queue_impl : public api::api_object_impl<VkQueue, api::command_queue>
	{
	public:
		command_queue_impl(device_impl *device, uint32_t queue_family_index, const VkQueueFamilyProperties &queue_family, VkQueue queue);
		~command_queue_impl();

		api::device *get_device() final;

		api::command_queue_type get_type() const final;

		void wait_idle() const final;

		void flush_immediate_command_list() const final;
		void flush_immediate_command_list(VkSubmitInfo &semaphore_info) const;

		api::command_list *get_immediate_command_list() final { return _immediate_cmd_list; }

		void begin_debug_event(const char *label, const float color[4]) final;
		void end_debug_event() final;
		void insert_debug_marker(const char *label, const float color[4]) final;

		bool wait(api::fence fence, uint64_t value) final;
		bool signal(api::fence fence, uint64_t value) final;

		uint64_t get_timestamp_frequency() const final;

		mutable std::recursive_mutex _mutex;

	private:
		device_impl *const _device_impl;
		command_list_immediate_impl *_immediate_cmd_list = nullptr;
		VkQueueFamilyProperties _queue_family_props = {};
	};

	template <>
	struct object_data<VK_OBJECT_TYPE_QUEUE> : public command_queue_impl
	{
		using Handle = VkQueue;

		object_data(device_impl *device, uint32_t queue_family_index, const VkQueueFamilyProperties &queue_family, VkQueue queue) :
			command_queue_impl(device, queue_family_index, queue_family, queue) {}
	};
}
