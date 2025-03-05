/*
 * Copyright (C) 2021 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "descriptor_heap.hpp"
#include "reshade_api_object_impl.hpp"
#include <map>
#include <unordered_map>
#include <concurrent_vector.h>

struct D3D12DescriptorHeap;

namespace reshade::d3d12
{
	class device_impl : public api::api_object_impl<ID3D12Device *, api::device>
	{
		friend class command_list_impl;
		friend class command_list_immediate_impl;
		friend class command_queue_impl;

	public:
		explicit device_impl(ID3D12Device *device);
		~device_impl();

		api::device_api get_api() const final { return api::device_api::d3d12; }

		bool get_property(api::device_properties property, void *data) const final;

		bool check_capability(api::device_caps capability) const final;
		bool check_format_support(api::format format, api::resource_usage usage) const final;

		bool create_sampler(const api::sampler_desc &desc, api::sampler *out_sampler) final;
		void destroy_sampler(api::sampler sampler) final;

		bool create_resource(const api::resource_desc &desc, const api::subresource_data *initial_data, api::resource_usage initial_state, api::resource *out_resource, HANDLE *shared_handle = nullptr) final;
		void destroy_resource(api::resource resource) final;

		api::resource_desc get_resource_desc(api::resource resource) const final;

		bool create_resource_view(api::resource resource, api::resource_usage usage_type, const api::resource_view_desc &desc, api::resource_view *out_view) final;
		void destroy_resource_view(api::resource_view view) final;

		api::resource get_resource_from_view(api::resource_view view) const final;
		api::resource_view_desc get_resource_view_desc(api::resource_view view) const final;

		uint64_t get_resource_view_gpu_address(api::resource_view view) const final;

		bool map_buffer_region(api::resource resource, uint64_t offset, uint64_t size, api::map_access access, void **out_data) final;
		void unmap_buffer_region(api::resource resource) final;
		bool map_texture_region(api::resource resource, uint32_t subresource, const api::subresource_box *box, api::map_access access, api::subresource_data *out_data) final;
		void unmap_texture_region(api::resource resource, uint32_t subresource) final;

		void update_buffer_region(const void *data, api::resource resource, uint64_t offset, uint64_t size) final;
		void update_texture_region(const api::subresource_data &data, api::resource resource, uint32_t subresource, const api::subresource_box *box) final;

		bool create_pipeline(api::pipeline_layout layout, uint32_t subobject_count, const api::pipeline_subobject *subobjects, api::pipeline *out_pipeline) final;
		void destroy_pipeline(api::pipeline pipeline) final;

		bool create_pipeline_layout(uint32_t param_count, const api::pipeline_layout_param *params, api::pipeline_layout *out_layout, D3D12_ROOT_SIGNATURE_FLAGS flags);
		bool create_pipeline_layout(uint32_t param_count, const api::pipeline_layout_param *params, api::pipeline_layout *out_layout) final;
		void destroy_pipeline_layout(api::pipeline_layout layout) final;

		bool allocate_descriptor_tables(uint32_t count, api::pipeline_layout layout, uint32_t layout_param, api::descriptor_table *out_tables) final;
		void free_descriptor_tables(uint32_t count, const api::descriptor_table *tables) final;

		void get_descriptor_heap_offset(api::descriptor_table table, uint32_t binding, uint32_t array_offset, api::descriptor_heap *out_heap, uint32_t *out_offset) const final;

		__forceinline ID3D12DescriptorHeap *get_descriptor_heap(api::descriptor_table table) const
		{
			api::descriptor_heap heap;
			get_descriptor_heap_offset(table, 0, 0, &heap, nullptr);
			return reinterpret_cast<ID3D12DescriptorHeap *>(heap.handle);
		}

		void copy_descriptor_tables(uint32_t count, const api::descriptor_table_copy *copies) final;
		void update_descriptor_tables(uint32_t count, const api::descriptor_table_update *updates) final;

		bool create_query_heap(api::query_type type, uint32_t count, api::query_heap *out_heap) final;
		void destroy_query_heap(api::query_heap heap) final;

		bool get_query_heap_results(api::query_heap heap, uint32_t first, uint32_t count, void *results, uint32_t stride) final;

		void set_resource_name(api::resource resource, const char *name) final;
		void set_resource_view_name(api::resource_view, const char * ) final {}

		bool create_fence(uint64_t initial_value, api::fence_flags flags, api::fence *out_fence, HANDLE *shared_handle = nullptr) final;
		void destroy_fence(api::fence fence) final;

		uint64_t get_completed_fence_value(api::fence fence) const final;

		bool wait(api::fence fence, uint64_t value, uint64_t timeout) final;
		bool signal(api::fence fence, uint64_t value) final;

		void get_acceleration_structure_size(api::acceleration_structure_type type, api::acceleration_structure_build_flags flags, uint32_t input_count, const api::acceleration_structure_build_input *inputs, uint64_t *out_size, uint64_t *out_build_scratch_size, uint64_t *out_update_scratch_size) const final;

		bool get_pipeline_shader_group_handles(api::pipeline pipeline, uint32_t first, uint32_t count, void *out_handles) final;

		command_list_immediate_impl *get_immediate_command_list();

#if RESHADE_ADDON >= 2
		bool resolve_gpu_address(D3D12_GPU_VIRTUAL_ADDRESS address, api::resource *out_resource, uint64_t *out_offset, bool *out_acceleration_structure = nullptr) const;

		static __forceinline api::descriptor_table convert_to_descriptor_table(D3D12_CPU_DESCRIPTOR_HANDLE handle)
		{
			assert((handle.ptr & 0xF000000000000000ull) == 0);
			return { 0xF000000000000000ull | handle.ptr }; // Add bit to be able to distinguish this handle CPU and GPU descriptor handles
		}
#endif
		static __forceinline api::descriptor_table convert_to_descriptor_table(D3D12_GPU_DESCRIPTOR_HANDLE handle)
		{
			assert((handle.ptr & 0xF000000000000000ull) != 0xF000000000000000ull);
			return { handle.ptr };
		}

		D3D12_CPU_DESCRIPTOR_HANDLE convert_to_original_cpu_descriptor_handle(api::descriptor_table set, D3D12_DESCRIPTOR_HEAP_TYPE *type = nullptr) const;
		D3D12_GPU_DESCRIPTOR_HANDLE convert_to_original_gpu_descriptor_handle(api::descriptor_table set) const;

		__forceinline D3D12_CPU_DESCRIPTOR_HANDLE offset_descriptor_handle(D3D12_CPU_DESCRIPTOR_HANDLE handle, SIZE_T offset, D3D12_DESCRIPTOR_HEAP_TYPE type) const
		{
			handle.ptr += offset * _descriptor_handle_size[type];
			return handle;
		}
		__forceinline D3D12_GPU_DESCRIPTOR_HANDLE offset_descriptor_handle(D3D12_GPU_DESCRIPTOR_HANDLE handle, SIZE_T offset, D3D12_DESCRIPTOR_HEAP_TYPE type) const
		{
			handle.ptr += offset * _descriptor_handle_size[type];
			return handle;
		}

	protected:
		void register_resource(ID3D12Resource *resource, bool acceleration_structure);
		void unregister_resource(ID3D12Resource *resource);

		void register_resource_view(D3D12_CPU_DESCRIPTOR_HANDLE handle, ID3D12Resource *resource, api::resource_view_desc desc);
		void register_resource_view(D3D12_CPU_DESCRIPTOR_HANDLE handle, D3D12_CPU_DESCRIPTOR_HANDLE source_handle);

#if RESHADE_ADDON >= 2
		void register_descriptor_heap(D3D12DescriptorHeap *heap);
		void unregister_descriptor_heap(D3D12DescriptorHeap *heap);
#endif

	private:
		std::vector<command_queue_impl *> _queues;

		UINT _descriptor_handle_size[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES];

		descriptor_heap_cpu _view_heaps[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES];
		descriptor_heap_gpu<D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, 128, 128> _gpu_sampler_heap;
		descriptor_heap_gpu<D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 50000, 2048> _gpu_view_heap;

		mutable std::shared_mutex _resource_mutex;
#if RESHADE_ADDON >= 2
		concurrency::concurrent_vector<D3D12DescriptorHeap *> _descriptor_heaps;
		std::map<D3D12_GPU_VIRTUAL_ADDRESS, std::tuple<UINT64, ID3D12Resource *, bool>> _buffer_gpu_addresses;
#endif
		std::unordered_map<SIZE_T, std::pair<ID3D12Resource *, api::resource_view_desc>> _views;

		com_ptr<ID3D12PipelineState> _mipmap_pipeline;
		com_ptr<ID3D12RootSignature> _mipmap_signature;
	};
}
