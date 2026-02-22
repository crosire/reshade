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
		static constexpr uint32_t NUM_COMMAND_FRAMES = 8; // Use power of two so that modulo can be replaced with bitwise operation

	public:
		static thread_local command_list_immediate_impl *s_last_immediate_command_list;

		command_list_immediate_impl(device_impl *device, uint32_t queue_family_index, VkQueue queue);
		~command_list_immediate_impl();

		void push_descriptors(api::shader_stage stages, api::pipeline_layout layout, uint32_t layout_param, const api::descriptor_table_update &update) final;

		void update_buffer_region(const void *data, api::resource dest, uint64_t dest_offset, uint64_t size) final;
		void update_texture_region(const api::subresource_data &data, api::resource dest, uint32_t dest_subresource, const api::subresource_box *dest_box) final;

		bool flush(VkSubmitInfo *wait_semaphore_info);

	private:
		const VkQueue _parent_queue;

		uint32_t _cmd_index = 0;
		VkCommandPool _cmd_pool = VK_NULL_HANDLE;
		VkFence _cmd_fences[NUM_COMMAND_FRAMES] = {};
		VkSemaphore _cmd_semaphores[NUM_COMMAND_FRAMES] = {};
		VkCommandBuffer _cmd_buffers[NUM_COMMAND_FRAMES] = {};

		VkDescriptorPool _transient_descriptor_pool[NUM_COMMAND_FRAMES] = {};
	};
}
