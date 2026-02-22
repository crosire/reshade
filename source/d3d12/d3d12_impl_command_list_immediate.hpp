/*
 * Copyright (C) 2021 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "d3d12_impl_command_list.hpp"

namespace reshade::d3d12
{
	class command_list_immediate_impl : public command_list_impl
	{
		static constexpr uint32_t NUM_COMMAND_FRAMES = 4; // Use power of two so that modulo can be replaced with bitwise operation

	public:
		static thread_local command_list_immediate_impl *s_last_immediate_command_list;

		command_list_immediate_impl(device_impl *device, ID3D12CommandQueue *queue);
		~command_list_immediate_impl();

		void end_query(api::query_heap heap, api::query_type type, uint32_t index) final;
		void query_acceleration_structures(uint32_t count, const api::resource_view *acceleration_structures, api::query_heap heap, api::query_type type, uint32_t first) final;

		void update_buffer_region(const void *data, api::resource dest, uint64_t dest_offset, uint64_t size) final;
		void update_texture_region(const api::subresource_data &data, api::resource dest, uint32_t dest_subresource, const api::subresource_box *dest_box) final;

		bool flush(bool wait);

	private:
		ID3D12CommandQueue *const _parent_queue;
		UINT32 _cmd_index = 0;
		HANDLE _fence_event = nullptr;
		UINT64 _fence_value[NUM_COMMAND_FRAMES] = {};
		com_ptr<ID3D12Fence> _fence[NUM_COMMAND_FRAMES];
		com_ptr<ID3D12CommandAllocator> _cmd_alloc[NUM_COMMAND_FRAMES];

		// List of query fences scheduled for signaling during next flush
		std::vector<std::pair<ID3D12Fence *, UINT64>> _current_query_fences;
	};
}
