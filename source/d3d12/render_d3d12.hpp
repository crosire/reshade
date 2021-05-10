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

		api::device_api get_api() const final { return api::device_api::d3d12; }

		bool check_capability(api::device_caps capability) const final;
		bool check_format_support(api::format format, api::resource_usage usage) const final;

		bool check_resource_handle_valid(api::resource resource) const final;
		bool check_resource_view_handle_valid(api::resource_view view) const final;

		bool create_sampler(const api::sampler_desc &desc, api::sampler *out) final;
		bool create_resource(const api::resource_desc &desc, const api::subresource_data *initial_data, api::resource_usage initial_state, api::resource *out) final;
		bool create_resource_view(api::resource resource, api::resource_usage usage_type, const api::resource_view_desc &desc, api::resource_view *out) final;

		bool create_pipeline(const api::pipeline_desc &desc, api::pipeline *out) final;
		bool create_pipeline_compute(const api::pipeline_desc &desc, api::pipeline *out);
		bool create_pipeline_graphics_all(const api::pipeline_desc &desc, api::pipeline *out);

		bool create_shader_module(api::shader_stage type, api::shader_format format, const char *entry_point, const void *code, size_t code_size, api::shader_module *out) final;
		bool create_pipeline_layout(uint32_t num_table_layouts, const api::descriptor_table_layout *table_layouts, uint32_t num_constant_ranges, const api::constant_range *constant_ranges, api::pipeline_layout *out) final;
		bool create_descriptor_heap(uint32_t max_tables, uint32_t num_sizes, const api::descriptor_heap_size *sizes, api::descriptor_heap *out) final;
		bool create_descriptor_tables(api::descriptor_heap heap, api::descriptor_table_layout layout, uint32_t count, api::descriptor_table *out) final;
		bool create_descriptor_table_layout(uint32_t num_ranges, const api::descriptor_range *ranges, bool push_descriptors, api::descriptor_table_layout *out) final;

		void destroy_sampler(api::sampler handle) final;
		void destroy_resource(api::resource handle) final;
		void destroy_resource_view(api::resource_view handle) final;

		void destroy_pipeline(api::pipeline_type type, api::pipeline handle) final;
		void destroy_shader_module(api::shader_module handle) final;
		void destroy_pipeline_layout(api::pipeline_layout handle) final;
		void destroy_descriptor_heap(api::descriptor_heap handle) final;
		void destroy_descriptor_table_layout(api::descriptor_table_layout handle) final;

		void update_descriptor_tables(uint32_t num_updates, const api::descriptor_update *updates) final;

		bool map_resource(api::resource resource, uint32_t subresource, api::map_access access, void **mapped_ptr) final;
		void unmap_resource(api::resource resource, uint32_t subresource) final;

		void upload_buffer_region(api::resource dst, uint64_t dst_offset, const void *data, uint64_t size) final;
		void upload_texture_region(api::resource dst, uint32_t dst_subresource, const int32_t dst_box[6], const void *data, uint32_t row_pitch, uint32_t slice_pitch) final;

		void get_resource_from_view(api::resource_view view, api::resource *out_resource) const final;
		api::resource_desc get_resource_desc(api::resource resource) const final;

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
		D3D12_CPU_DESCRIPTOR_HANDLE allocate_descriptor_handle(D3D12_DESCRIPTOR_HEAP_TYPE type);

		mutable std::mutex _mutex;
		std::vector<ID3D12CommandQueue *> _queues;
		std::vector<bool> _resource_view_pool_state[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES];
		com_ptr<ID3D12DescriptorHeap> _resource_view_pool[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES];
		std::vector<std::pair<ID3D12Resource *, D3D12_GPU_VIRTUAL_ADDRESS_RANGE>> _buffer_gpu_addresses;

		std::unordered_map<UINT64, D3D12_CPU_DESCRIPTOR_HANDLE> _descriptor_table_map;
		std::unordered_map<ID3D12DescriptorHeap *, UINT> _descriptor_heap_offset;

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

		void push_constants(api::shader_stage stage, api::pipeline_layout layout, uint32_t layout_index, uint32_t first, uint32_t count, const uint32_t *values) final;
		void push_descriptors(api::shader_stage stage, api::pipeline_layout layout, uint32_t layout_index, api::descriptor_type type, uint32_t first, uint32_t count, const void *descriptors) final;
		void bind_descriptor_heaps(uint32_t count, const api::descriptor_heap *heaps) final;
		void bind_descriptor_tables(api::pipeline_type type, api::pipeline_layout layout, uint32_t first, uint32_t count, const api::descriptor_table *tables) final;

		void bind_index_buffer(api::resource buffer, uint64_t offset, uint32_t index_size) final;
		void bind_vertex_buffers(uint32_t first, uint32_t count, const api::resource *buffers, const uint64_t *offsets, const uint32_t *strides) final;

		void draw(uint32_t vertices, uint32_t instances, uint32_t first_vertex, uint32_t first_instance) final;
		void draw_indexed(uint32_t indices, uint32_t instances, uint32_t first_index, int32_t vertex_offset, uint32_t first_instance) final;
		void dispatch(uint32_t num_groups_x, uint32_t num_groups_y, uint32_t num_groups_z) final;
		void draw_or_dispatch_indirect(uint32_t type, api::resource buffer, uint64_t offset, uint32_t draw_count, uint32_t stride) final;

		void begin_render_pass(uint32_t count, const api::resource_view *rtvs, api::resource_view dsv) final;
		void   end_render_pass() final;

		void blit(api::resource src, uint32_t src_subresource, const int32_t src_box[6], api::resource dst, uint32_t dst_subresource, const int32_t dst_box[6], api::texture_filter filter) final;
		void resolve(api::resource src, uint32_t src_subresource, const int32_t src_offset[3], api::resource dst, uint32_t dst_subresource, const int32_t dst_offset[3], const uint32_t size[3], uint32_t format) final;
		void copy_resource(api::resource src, api::resource dst) final;
		void copy_buffer_region(api::resource src, uint64_t src_offset, api::resource dst, uint64_t dst_offset, uint64_t size) final;
		void copy_buffer_to_texture(api::resource src, uint64_t src_offset, uint32_t row_length, uint32_t slice_height, api::resource dst, uint32_t dst_subresource, const int32_t dst_box[6]) final;
		void copy_texture_region(api::resource src, uint32_t src_subresource, const int32_t src_offset[3], api::resource dst, uint32_t dst_subresource, const int32_t dst_offset[3], const uint32_t size[3]) final;
		void copy_texture_to_buffer(api::resource src, uint32_t src_subresource, const int32_t src_box[6], api::resource dst, uint64_t dst_offset, uint32_t row_length, uint32_t slice_height) final;

		void clear_depth_stencil_view(api::resource_view dsv, uint32_t clear_flags, float depth, uint8_t stencil) final;
		void clear_render_target_views(uint32_t count, const api::resource_view *rtvs, const float color[4]) final;
		void clear_unordered_access_view_uint(api::resource_view uav, const uint32_t values[4]) final;
		void clear_unordered_access_view_float(api::resource_view uav, const float values[4]) final;

		void transition_state(api::resource resource, api::resource_usage old_state, api::resource_usage new_state) final;

		void begin_debug_event(const char *label, const float color[4]) final;
		void   end_debug_event() final;
		void insert_debug_marker(const char *label, const float color[4]) final;

	protected:
		device_impl *const _device_impl;
		bool _has_commands = false;

		ID3D12RootSignature *_current_root_signature[2] = {};
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
