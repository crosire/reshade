/*
 * Copyright (C) 2021 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include "com_ptr.hpp"
#include "com_tracking.hpp"
#include "addon_manager.hpp"
#include <d3d12.h>
#include <dxgi1_5.h>
#include <unordered_map>

namespace reshade::d3d12
{
	class device_impl : public api::api_object_impl<ID3D12Device *, api::device>
	{
		friend class command_queue_impl;

	public:
		explicit device_impl(ID3D12Device *device);
		~device_impl();

		api::render_api get_api() const final { return api::render_api::d3d12; }

		bool check_format_support(uint32_t format, api::resource_usage usage) const final;

		bool check_resource_handle_valid(api::resource_handle resource) const final;
		bool check_resource_view_handle_valid(api::resource_view_handle view) const final;

		bool create_resource(api::resource_type type, const api::resource_desc &desc, api::resource_usage initial_state, api::resource_handle *out_resource) final;
		bool create_resource_view(api::resource_handle resource, api::resource_view_type type, const api::resource_view_desc &desc, api::resource_view_handle *out_view) final;

		void destroy_resource(api::resource_handle resource) final;
		void destroy_resource_view(api::resource_view_handle view) final;

		void get_resource_from_view(api::resource_view_handle view, api::resource_handle *out_resource) const final;

		api::resource_desc get_resource_desc(api::resource_handle resource) const final;

		void wait_idle() const final;

		bool resolve_gpu_address(D3D12_GPU_VIRTUAL_ADDRESS address, ID3D12Resource **out_resource, UINT64 *out_offset)
		{
			const std::lock_guard<std::mutex> lock(_mutex);
			for (const auto &buffer_info : _buffer_gpu_addresses)
			{
				if (address < buffer_info.second.StartAddress)
					continue;
				const UINT64 address_offset = address - buffer_info.second.StartAddress;
				if (address_offset < buffer_info.second.SizeInBytes)
				{
					*out_offset = address_offset;
					*out_resource = buffer_info.first;
					return true;
				}
			}
			return false;
		}

		// Cached device capabilities for quick access
		UINT _descriptor_handle_size[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES];

	private:
		D3D12_CPU_DESCRIPTOR_HANDLE allocate_descriptor_handle(D3D12_DESCRIPTOR_HEAP_TYPE type);

		mutable std::mutex _mutex;
		std::vector<ID3D12CommandQueue *> _queues;
		std::vector<bool> _resource_view_pool_state[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES];
		com_ptr<ID3D12DescriptorHeap> _resource_view_pool[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES];
		std::vector<std::pair<ID3D12Resource *, D3D12_GPU_VIRTUAL_ADDRESS_RANGE>> _buffer_gpu_addresses;

	protected:
		inline void register_resource_view(ID3D12Resource *resource, D3D12_CPU_DESCRIPTOR_HANDLE handle)
		{
			assert(resource != nullptr);
			const std::lock_guard<std::mutex> lock(_mutex);
			_views.emplace(handle.ptr, resource);
		}
		inline void register_buffer_gpu_address(ID3D12Resource *resource, UINT64 size)
		{
			assert(resource != nullptr);
			const std::lock_guard<std::mutex> lock(_mutex);
			_buffer_gpu_addresses.emplace_back(resource, D3D12_GPU_VIRTUAL_ADDRESS_RANGE { resource->GetGPUVirtualAddress(), size });
		}

		com_object_list<ID3D12Resource> _resources;
		std::unordered_map<uint64_t, ID3D12Resource *> _views;
	};

	class command_list_impl : public api::api_object_impl<ID3D12GraphicsCommandList *, api::command_list>
	{
	public:
		command_list_impl(device_impl *device, ID3D12GraphicsCommandList *cmd_list);
		~command_list_impl();

		api::device *get_device() final { return _device_impl; }

		void draw(uint32_t vertices, uint32_t instances, uint32_t first_vertex, uint32_t first_instance) final;
		void draw_indexed(uint32_t indices, uint32_t instances, uint32_t first_index, int32_t vertex_offset, uint32_t first_instance) final;

		void copy_resource(api::resource_handle source, api::resource_handle destination) final;

		void transition_state(api::resource_handle resource, api::resource_usage old_state, api::resource_usage new_state) final;

		void clear_depth_stencil_view(api::resource_view_handle dsv, uint32_t clear_flags, float depth, uint8_t stencil) final;
		void clear_render_target_view(api::resource_view_handle rtv, const float color[4]) final;

	protected:
		device_impl *const _device_impl;
		bool _has_commands = false;
	};

	class command_list_immediate_impl : public command_list_impl
	{
		static const UINT NUM_COMMAND_FRAMES = 4;

	public:
		command_list_immediate_impl(device_impl *device);
		~command_list_immediate_impl();

		bool flush(ID3D12CommandQueue *queue);
		bool flush_and_wait(ID3D12CommandQueue *queue);

		ID3D12GraphicsCommandList *const begin_commands() { _has_commands = true; return _orig; }

	private:
		UINT _cmd_index = 0;
		HANDLE _fence_event = nullptr;
		UINT64 _fence_value[NUM_COMMAND_FRAMES] = {};
		com_ptr<ID3D12Fence> _fence[NUM_COMMAND_FRAMES];
		com_ptr<ID3D12CommandAllocator> _cmd_alloc[NUM_COMMAND_FRAMES];
	};

	class command_queue_impl : public api::api_object_impl<ID3D12CommandQueue *, api::command_queue>
	{
	public:
		command_queue_impl(device_impl *device, ID3D12CommandQueue *queue);
		~command_queue_impl();

		api::device *get_device() final { return _device_impl; }

		api::command_list *get_immediate_command_list() final { return _immediate_cmd_list; }

		void flush_immediate_command_list() const final;

	private:
		device_impl *const _device_impl;
		command_list_immediate_impl *_immediate_cmd_list = nullptr;
	};
}
