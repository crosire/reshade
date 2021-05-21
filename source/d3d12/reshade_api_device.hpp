/*
 * Copyright (C) 2021 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include "com_tracking.hpp"
#include "addon_manager.hpp"
#include "descriptor_heap.hpp"
#include <dxgi1_5.h>
#include <unordered_map>

namespace reshade::d3d12
{
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

		bool is_resource_handle_valid(api::resource handle) const final;
		bool is_resource_view_handle_valid(api::resource_view handle) const final;

		bool create_sampler(const api::sampler_desc &desc, api::sampler *out) final;
		bool create_resource(const api::resource_desc &desc, const api::subresource_data *initial_data, api::resource_usage initial_state, api::resource *out) final;
		bool create_resource_view(api::resource resource, api::resource_usage usage_type, const api::resource_view_desc &desc, api::resource_view *out) final;

		bool create_pipeline(const api::pipeline_desc &desc, api::pipeline *out) final;
		bool create_pipeline_compute(const api::pipeline_desc &desc, api::pipeline *out);
		bool create_pipeline_graphics_all(const api::pipeline_desc &desc, api::pipeline *out);

		bool create_shader_module(api::shader_stage type, api::shader_format format, const void *code, size_t code_size, const char *entry_point, api::shader_module *out) final;
		bool create_pipeline_layout(uint32_t num_table_layouts, const api::descriptor_set_layout *table_layouts, uint32_t num_constant_ranges, const api::constant_range *constant_ranges, api::pipeline_layout *out) final;
		bool create_descriptor_set_layout(uint32_t num_ranges, const api::descriptor_range *ranges, bool push_descriptors, api::descriptor_set_layout *out) final;
		bool create_query_pool(api::query_type type, uint32_t count, api::query_pool *out) final;
		bool create_descriptor_sets(api::descriptor_set_layout layout, uint32_t count, api::descriptor_set *out) final;

		void destroy_sampler(api::sampler handle) final;
		void destroy_resource(api::resource handle) final;
		void destroy_resource_view(api::resource_view handle) final;

		void destroy_pipeline(api::pipeline_type type, api::pipeline handle) final;
		void destroy_shader_module(api::shader_module handle) final;
		void destroy_pipeline_layout(api::pipeline_layout handle) final;
		void destroy_descriptor_set_layout(api::descriptor_set_layout handle) final;
		void destroy_query_pool(api::query_pool handle) final;
		void destroy_descriptor_sets(api::descriptor_set_layout layout, uint32_t count, const api::descriptor_set *sets) final;

		void get_resource_from_view(api::resource_view view, api::resource *out_resource) const final;
		api::resource_desc get_resource_desc(api::resource resource) const final;

		bool map_resource(api::resource resource, uint32_t subresource, api::map_access access, void **data) final;
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
}
