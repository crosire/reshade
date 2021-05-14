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
	class descriptor_heap_cpu
	{
		struct heap_info
		{
			com_ptr<ID3D12DescriptorHeap> heap;
			std::vector<bool> state;
			SIZE_T heap_base;
		};

		const UINT pool_size = 1024;

	public:
		descriptor_heap_cpu(ID3D12Device *device, D3D12_DESCRIPTOR_HEAP_TYPE type) :
			_device(device), _type(type)
		{
			_increment_size = device->GetDescriptorHandleIncrementSize(type);
		}

		bool allocate(D3D12_CPU_DESCRIPTOR_HANDLE &handle)
		{
			for (heap_info &heap_info : _heap_infos)
			{
				// Find free empty in the heap
				if (const auto it = std::find(heap_info.state.begin(), heap_info.state.end(), false);
					it != heap_info.state.end())
				{
					const size_t index = it - heap_info.state.begin();
					heap_info.state[index] = true; // Mark this entry as being in use

					handle.ptr = heap_info.heap_base + index * _increment_size;
					return true;
				}
			}

			// No more space available in the existing heaps, so create a new one and try again
			return allocate_heap() && allocate(handle);
		}

		void deallocate(D3D12_CPU_DESCRIPTOR_HANDLE handle)
		{
			for (heap_info &heap_info : _heap_infos)
			{
				const SIZE_T heap_beg = heap_info.heap_base;
				const SIZE_T heap_end = heap_info.heap_base + pool_size * _increment_size;

				if (handle.ptr >= heap_beg && handle.ptr < heap_end)
				{
					const SIZE_T index = (handle.ptr - heap_beg) / _increment_size;

					// Mark free slot in the descriptor heap
					heap_info.state[index] = false;
					break;
				}
			}
		}

	private:
		bool allocate_heap()
		{
			heap_info &heap_info = _heap_infos.emplace_back();

			D3D12_DESCRIPTOR_HEAP_DESC desc;
			desc.Type = _type;
			desc.NumDescriptors = pool_size;
			desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
			desc.NodeMask = 0;

			if (FAILED(_device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&heap_info.heap))))
			{
				_heap_infos.pop_back();
				return false;
			}

			heap_info.heap_base = heap_info.heap->GetCPUDescriptorHandleForHeapStart().ptr;
			heap_info.state.resize(pool_size);

			return true;
		}

		ID3D12Device *const _device;
		std::vector<heap_info> _heap_infos;
		SIZE_T _increment_size;
		D3D12_DESCRIPTOR_HEAP_TYPE _type;
	};

	template <D3D12_DESCRIPTOR_HEAP_TYPE type, UINT static_size, UINT transient_size>
	class descriptor_heap_gpu
	{
	public:
		explicit descriptor_heap_gpu(ID3D12Device *device, UINT node_mask = 0)
		{
			// Manage all descriptors in a single heap, to avoid costly descriptor heap switches during rendering
			// The lower portion of the heap is reserved for static bindings, the upper portion for transient bindings (which change frequently and are managed like a ring buffer)
			D3D12_DESCRIPTOR_HEAP_DESC desc;
			desc.Type = type;
			desc.NumDescriptors = static_size + transient_size;
			desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
			desc.NodeMask = node_mask;

			if (FAILED(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&_heap))))
				return;

			_increment_size = device->GetDescriptorHandleIncrementSize(type);
			_static_heap_base = _heap->GetCPUDescriptorHandleForHeapStart().ptr;
			_static_heap_base_gpu = _heap->GetGPUDescriptorHandleForHeapStart().ptr;
			_transient_heap_base = _static_heap_base + static_size * _increment_size;
			_transient_heap_base_gpu = _static_heap_base_gpu + static_size * _increment_size;
		}

		bool allocate_static(UINT count, D3D12_CPU_DESCRIPTOR_HANDLE &base_handle, D3D12_GPU_DESCRIPTOR_HANDLE &base_handle_gpu)
		{
			if (_heap == nullptr)
				return false;

			// First try to allocate from the list of freed blocks
			for (auto block = _free_list.begin(); block != _free_list.end(); ++block)
			{
				if (count <= (block->second - block->first) / _increment_size)
				{
					base_handle.ptr = _static_heap_base + (block->first - _static_heap_base_gpu);
					base_handle_gpu.ptr = block->first;

					// Remove the allocated range from the freed block and optionally remove it from the free list if no space is left afterwards
					block->first += count * _increment_size;
					if (block->first == block->second)
						_free_list.erase(block);

					return true;
				}
			}

			// Otherwise follow a linear allocation schema
			if (_current_static_index + count > static_size)
				return false; // The heap is full

			const SIZE_T offset = _current_static_index * _increment_size;
			base_handle.ptr = _static_heap_base + offset;
			base_handle_gpu.ptr = _static_heap_base_gpu + offset;

			_current_static_index += count;

			return true;
		}
		bool allocate_transient(UINT count, D3D12_CPU_DESCRIPTOR_HANDLE &base_handle, D3D12_GPU_DESCRIPTOR_HANDLE &base_handle_gpu)
		{
			if (_heap == nullptr)
				return false;

			SIZE_T index = static_cast<SIZE_T>(_current_transient_tail % transient_size);

			// Allocations need to be contiguous
			if (index + count > transient_size)
				_current_transient_tail += transient_size - index, index = 0;

			const SIZE_T offset = index * _increment_size;
			base_handle.ptr = _transient_heap_base + offset;
			base_handle_gpu.ptr = _transient_heap_base_gpu + offset;

			_current_transient_tail += count;

			return true;
		}

		void deallocate(D3D12_GPU_DESCRIPTOR_HANDLE handle, UINT count = 1)
		{
			// Ensure this handle falls into the static range of this heap
			if (handle.ptr < _static_heap_base_gpu || handle.ptr >= _transient_heap_base_gpu)
				return;

			// First try to append to an existing freed block
			for (auto block = _free_list.begin(); block != _free_list.end(); ++block)
			{
				if (handle.ptr == block->second)
				{
					block->second += count * _increment_size;

					// Try and merge with other blocks that are adjacent
					for (auto block_adj = _free_list.begin(); block_adj != _free_list.end(); ++block_adj)
					{
						if (block_adj->first == block->second)
						{
							block->second = block_adj->second;
							_free_list.erase(block_adj);
							break;
						}
					}
					return;
				}
				if (handle.ptr == (block->first - (count * _increment_size)))
				{
					block->first -= count * _increment_size;

					// Try and merge with other blocks that are adjacent
					for (auto block_adj = _free_list.begin(); block_adj != _free_list.end(); ++block_adj)
					{
						if (block_adj->second == block->first)
						{
							block->first = block_adj->first;
							_free_list.erase(block_adj);
							break;
						}
					}
					return;
				}
			}

			// Otherwise add a new block to the free list
			_free_list.emplace_back(handle.ptr, handle.ptr + count * _increment_size);
		}

		ID3D12DescriptorHeap *get() const { assert(_heap != nullptr); return _heap.get(); }

	private:
		com_ptr<ID3D12DescriptorHeap> _heap;
		SIZE_T _increment_size;
		SIZE_T _static_heap_base;
		SIZE_T _static_heap_base_gpu;
		SIZE_T _transient_heap_base;
		SIZE_T _transient_heap_base_gpu;
		SIZE_T _current_static_index = 0;
		UINT64 _current_transient_tail = 0;
		std::vector<std::pair<SIZE_T, SIZE_T>> _free_list;
	};

	class device_impl : public api::api_object_impl<ID3D12Device *, api::device>
	{
		friend class command_list_impl;
		friend class command_queue_impl;

	public:
		explicit device_impl(ID3D12Device *device);
		~device_impl();

		api::device_api get_api() const final { return api::device_api::d3d12; }

		bool check_capability(api::device_caps capability) const final;
		bool check_format_support(api::format format, api::resource_usage usage) const final;

		bool check_resource_handle_valid(api::resource handle) const final;
		bool check_resource_view_handle_valid(api::resource_view handle) const final;

		bool create_sampler(const api::sampler_desc &desc, api::sampler *out) final;
		bool create_resource(const api::resource_desc &desc, const api::subresource_data *initial_data, api::resource_usage initial_state, api::resource *out) final;
		bool create_resource_view(api::resource resource, api::resource_usage usage_type, const api::resource_view_desc &desc, api::resource_view *out) final;

		bool create_pipeline(const api::pipeline_desc &desc, api::pipeline *out) final;
		bool create_pipeline_compute(const api::pipeline_desc &desc, api::pipeline *out);
		bool create_pipeline_graphics_all(const api::pipeline_desc &desc, api::pipeline *out);

		bool create_shader_module(api::shader_stage type, api::shader_format format, const char *entry_point, const void *code, size_t code_size, api::shader_module *out) final;
		bool create_pipeline_layout(uint32_t num_table_layouts, const api::descriptor_set_layout *table_layouts, uint32_t num_constant_ranges, const api::constant_range *constant_ranges, api::pipeline_layout *out) final;
		bool create_query_pool(api::query_type type, uint32_t count, api::query_pool *out) final;
		bool create_descriptor_sets(api::descriptor_set_layout layout, uint32_t count, api::descriptor_set *out) final;
		bool create_descriptor_set_layout(uint32_t num_ranges, const api::descriptor_range *ranges, bool push_descriptors, api::descriptor_set_layout *out) final;

		void destroy_sampler(api::sampler handle) final;
		void destroy_resource(api::resource handle) final;
		void destroy_resource_view(api::resource_view handle) final;

		void destroy_pipeline(api::pipeline_type type, api::pipeline handle) final;
		void destroy_shader_module(api::shader_module handle) final;
		void destroy_pipeline_layout(api::pipeline_layout handle) final;
		void destroy_query_pool(api::query_pool handle) final;
		void destroy_descriptor_sets(api::descriptor_set_layout layout, uint32_t count, const api::descriptor_set *sets) final;
		void destroy_descriptor_set_layout(api::descriptor_set_layout handle) final;

		void get_resource_from_view(api::resource_view view, api::resource *out_resource) const final;
		api::resource_desc get_resource_desc(api::resource resource) const final;

		bool map_resource(api::resource resource, uint32_t subresource, api::map_access access, void **mapped_ptr) final;
		void unmap_resource(api::resource resource, uint32_t subresource) final;

		void upload_buffer_region(const void *data, api::resource dst, uint64_t dst_offset, uint64_t size) final;
		void upload_texture_region(const api::subresource_data &data, api::resource dst, uint32_t dst_subresource, const int32_t dst_box[6]) final;

		void update_descriptor_sets(uint32_t num_updates, const api::descriptor_update *updates) final;

		bool get_query_results(api::query_pool pool, uint32_t first, uint32_t count, void *results, uint32_t stride) final;

		void wait_idle() const final;

		void set_debug_name(api::resource resource, const char *name) final;

#if RESHADE_ADDON
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
#endif

		// Cached device capabilities for quick access
		UINT _descriptor_handle_size[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES];

	private:
		mutable std::mutex _mutex;
		std::vector<command_queue_impl *> _queues;
		std::vector<std::pair<ID3D12Resource *, D3D12_GPU_VIRTUAL_ADDRESS_RANGE>> _buffer_gpu_addresses;

		std::unordered_map<UINT64, D3D12_CPU_DESCRIPTOR_HANDLE> _descriptor_table_map;

		com_ptr<ID3D12PipelineState> _mipmap_pipeline;
		com_ptr<ID3D12RootSignature> _mipmap_signature;

		descriptor_heap_cpu _view_heaps[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES];
		descriptor_heap_gpu<D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, 128, 128> _gpu_sampler_heap;
		descriptor_heap_gpu<D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1024, 2048> _gpu_view_heap;


	protected:
		inline void register_resource_view(ID3D12Resource *resource, D3D12_CPU_DESCRIPTOR_HANDLE handle)
		{
			assert(resource != nullptr);
			const std::lock_guard<std::mutex> lock(_mutex);
			_views.emplace(handle.ptr, resource);
		}
#if RESHADE_ADDON
		inline void register_buffer_gpu_address(ID3D12Resource *resource, UINT64 size)
		{
			assert(resource != nullptr);
			const std::lock_guard<std::mutex> lock(_mutex);
			_buffer_gpu_addresses.emplace_back(resource, D3D12_GPU_VIRTUAL_ADDRESS_RANGE { resource->GetGPUVirtualAddress(), size });
		}
#endif

		com_object_list<ID3D12Resource> _resources;
		std::unordered_map<uint64_t, ID3D12Resource *> _views;
	};

	class command_list_impl : public api::api_object_impl<ID3D12GraphicsCommandList *, api::command_list>
	{
	public:
		command_list_impl(device_impl *device, ID3D12GraphicsCommandList *cmd_list);
		~command_list_impl();

		api::device *get_device() final { return _device_impl; }

		void bind_pipeline(api::pipeline_type type, api::pipeline pipeline) final;
		void bind_pipeline_states(uint32_t count, const api::pipeline_state *states, const uint32_t *values) final;
		void bind_viewports(uint32_t first, uint32_t count, const float *viewports) final;
		void bind_scissor_rects(uint32_t first, uint32_t count, const int32_t *rects) final;

		void push_constants(api::shader_stage stage, api::pipeline_layout layout, uint32_t layout_index, uint32_t first, uint32_t count, const void *values) final;
		void push_descriptors(api::shader_stage stage, api::pipeline_layout layout, uint32_t layout_index, api::descriptor_type type, uint32_t first, uint32_t count, const void *descriptors) final;
		void bind_descriptor_sets(api::pipeline_type type, api::pipeline_layout layout, uint32_t first, uint32_t count, const api::descriptor_set *sets) final;

		void bind_index_buffer(api::resource buffer, uint64_t offset, uint32_t index_size) final;
		void bind_vertex_buffers(uint32_t first, uint32_t count, const api::resource *buffers, const uint64_t *offsets, const uint32_t *strides) final;

		void draw(uint32_t vertices, uint32_t instances, uint32_t first_vertex, uint32_t first_instance) final;
		void draw_indexed(uint32_t indices, uint32_t instances, uint32_t first_index, int32_t vertex_offset, uint32_t first_instance) final;
		void dispatch(uint32_t num_groups_x, uint32_t num_groups_y, uint32_t num_groups_z) final;
		void draw_or_dispatch_indirect(uint32_t type, api::resource buffer, uint64_t offset, uint32_t draw_count, uint32_t stride) final;

		void begin_render_pass(uint32_t count, const api::resource_view *rtvs, api::resource_view dsv) final;
		void end_render_pass() final;

		void blit(api::resource src, uint32_t src_subresource, const int32_t src_box[6], api::resource dst, uint32_t dst_subresource, const int32_t dst_box[6], api::texture_filter filter) final;
		void resolve(api::resource src, uint32_t src_subresource, const int32_t src_offset[3], api::resource dst, uint32_t dst_subresource, const int32_t dst_offset[3], const uint32_t size[3], api::format format) final;
		void copy_resource(api::resource src, api::resource dst) final;
		void copy_buffer_region(api::resource src, uint64_t src_offset, api::resource dst, uint64_t dst_offset, uint64_t size) final;
		void copy_buffer_to_texture(api::resource src, uint64_t src_offset, uint32_t row_length, uint32_t slice_height, api::resource dst, uint32_t dst_subresource, const int32_t dst_box[6]) final;
		void copy_texture_region(api::resource src, uint32_t src_subresource, const int32_t src_offset[3], api::resource dst, uint32_t dst_subresource, const int32_t dst_offset[3], const uint32_t size[3]) final;
		void copy_texture_to_buffer(api::resource src, uint32_t src_subresource, const int32_t src_box[6], api::resource dst, uint64_t dst_offset, uint32_t row_length, uint32_t slice_height) final;

		void generate_mipmaps(api::resource_view srv) final;

		void clear_depth_stencil_view(api::resource_view dsv, uint32_t clear_flags, float depth, uint8_t stencil) final;
		void clear_render_target_views(uint32_t count, const api::resource_view *rtvs, const float color[4]) final;
		void clear_unordered_access_view_uint(api::resource_view uav, const uint32_t values[4]) final;
		void clear_unordered_access_view_float(api::resource_view uav, const float values[4]) final;

		void begin_query(api::query_pool pool, api::query_type type, uint32_t index) final;
		void end_query(api::query_pool pool, api::query_type type, uint32_t index) final;
		void copy_query_results(api::query_pool pool, api::query_type type, uint32_t first, uint32_t count, api::resource dst, uint64_t dst_offset, uint32_t stride) final;

		void insert_barrier(uint32_t count, const api::resource *resources, const api::resource_usage *old_states, const api::resource_usage *new_states) final;

		void begin_debug_marker(const char *label, const float color[4]) final;
		void end_debug_marker() final;
		void insert_debug_marker(const char *label, const float color[4]) final;

	protected:
		device_impl *const _device_impl;
		bool _has_commands = false;

		// Currently bound root signature (graphics at index 0, compute at index 1)
		ID3D12RootSignature *_current_root_signature[2] = {};
		// Currently bound descriptor heaps (there can only be one of each shader visible type, so a maximum of two)
		ID3D12DescriptorHeap *_current_descriptor_heaps[2] = {};
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

		void wait_idle() const final;

		void begin_debug_marker(const char *label, const float color[4]) final;
		void end_debug_marker() final;
		void insert_debug_marker(const char *label, const float color[4]) final;

	private:
		device_impl *const _device_impl;
		command_list_immediate_impl *_immediate_cmd_list = nullptr;
		HANDLE _wait_idle_fence_event = nullptr;
		mutable UINT64 _wait_idle_fence_value = 0;
		com_ptr<ID3D12Fence> _wait_idle_fence;
	};
}
