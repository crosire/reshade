/*
 * Copyright (C) 2021 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include "addon_manager.hpp"
#include "descriptor_heap.hpp"
#include <dxgi1_5.h>
#include <shared_mutex>

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

		bool create_sampler(const api::sampler_desc &desc, api::sampler *out) final;
		void destroy_sampler(api::sampler handle) final;

		bool create_resource(const api::resource_desc &desc, const api::subresource_data *initial_data, api::resource_usage initial_state, api::resource *out) final;
		void destroy_resource(api::resource handle) final;

		bool create_resource_view(api::resource resource, api::resource_usage usage_type, const api::resource_view_desc &desc, api::resource_view *out) final;
		void destroy_resource_view(api::resource_view handle) final;

		bool create_pipeline(const api::pipeline_desc &desc, api::pipeline *out) final;
		bool create_compute_pipeline(const api::pipeline_desc &desc, api::pipeline *out);
		bool create_graphics_pipeline(const api::pipeline_desc &desc, api::pipeline *out);
		void destroy_pipeline(api::pipeline_stage type, api::pipeline handle) final;

		bool create_pipeline_layout(uint32_t count, const api::pipeline_layout_param *params, api::pipeline_layout *out) final;
		void destroy_pipeline_layout(api::pipeline_layout handle) final;

		bool create_descriptor_set_layout(uint32_t count, const api::descriptor_range *bindings, bool push_descriptors, api::descriptor_set_layout *out) final;
		void destroy_descriptor_set_layout(api::descriptor_set_layout handle) final;

		bool create_query_pool(api::query_type type, uint32_t size, api::query_pool *out) final;
		void destroy_query_pool(api::query_pool handle) final;

		bool create_render_pass(const api::render_pass_desc &desc, api::render_pass *out) final;
		void destroy_render_pass(api::render_pass handle) final;

		bool create_framebuffer(const api::framebuffer_desc &desc, api::framebuffer *out) final;
		void destroy_framebuffer(api::framebuffer handle) final;

		bool create_descriptor_sets(uint32_t count, const api::descriptor_set_layout *layouts, api::descriptor_set *out) final;
		void destroy_descriptor_sets(uint32_t count, const api::descriptor_set *sets) final;

		bool map_resource(api::resource resource, uint32_t subresource, api::map_access access, api::subresource_data *out_data) final;
		void unmap_resource(api::resource resource, uint32_t subresource) final;

		void update_buffer_region(const void *data, api::resource dst, uint64_t dst_offset, uint64_t size) final;
		void update_texture_region(const api::subresource_data &data, api::resource dst, uint32_t dst_subresource, const int32_t dst_box[6]) final;

		void update_descriptor_sets(uint32_t count, const api::descriptor_set_update *updates) final;

		bool get_query_pool_results(api::query_pool pool, uint32_t first, uint32_t count, void *results, uint32_t stride) final;

		void wait_idle() const final;

		void set_resource_name(api::resource resource, const char *name) final;

		void get_pipeline_layout_desc(api::pipeline_layout layout, uint32_t *count, api::pipeline_layout_param *params) const final;
		void get_descriptor_pool_offset(api::descriptor_set set, api::descriptor_pool *pool, uint32_t *offset) const final;
		void get_descriptor_set_layout_desc(api::descriptor_set_layout layout, uint32_t *count, api::descriptor_range *bindings) const final;

		api::resource_desc get_resource_desc(api::resource resource) const final;
		api::resource      get_resource_from_view(api::resource_view view) const final;

		api::resource_view get_framebuffer_attachment(api::framebuffer framebuffer, api::attachment_type type, uint32_t index) const final;

		bool resolve_gpu_address(D3D12_GPU_VIRTUAL_ADDRESS address, api::resource *out_resource, uint64_t *out_offset) const;
		bool resolve_descriptor_handle(D3D12_CPU_DESCRIPTOR_HANDLE handle, D3D12_DESCRIPTOR_HEAP_TYPE type, api::descriptor_set *out_set) const;
		bool resolve_descriptor_handle(api::descriptor_set set, D3D12_CPU_DESCRIPTOR_HANDLE *handle, api::descriptor_pool *out_pool = nullptr, uint32_t *out_offset = nullptr) const;

		inline D3D12_CPU_DESCRIPTOR_HANDLE offset_descriptor_handle(D3D12_CPU_DESCRIPTOR_HANDLE handle, UINT offset, D3D12_DESCRIPTOR_HEAP_TYPE type) const
		{
			return { handle.ptr + static_cast<SIZE_T>(offset) * _descriptor_handle_size[type] };
		}

	protected:
		void register_resource(ID3D12Resource *resource);
		void unregister_resource(ID3D12Resource *resource);

#if RESHADE_ADDON
		void register_descriptor_heap(ID3D12DescriptorHeap *heap);
		void unregister_descriptor_heap(ID3D12DescriptorHeap *heap);
#endif

		inline bool is_resource_view(D3D12_CPU_DESCRIPTOR_HANDLE handle) const
		{
			const std::shared_lock<std::shared_mutex> lock(_resource_mutex);
			return _views.find(handle.ptr) != _views.end();
		}

		inline void register_resource_view(D3D12_CPU_DESCRIPTOR_HANDLE handle, ID3D12Resource *resource)
		{
			const std::unique_lock<std::shared_mutex> lock(_resource_mutex);
			_views[handle.ptr] = resource;
		}

	private:
		std::vector<command_queue_impl *> _queues;

		UINT _descriptor_handle_size[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES];

		descriptor_heap_cpu _view_heaps[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES];
		descriptor_heap_gpu<D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, 128, 128> _gpu_sampler_heap;
		descriptor_heap_gpu<D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1024, 2048> _gpu_view_heap;

		mutable std::shared_mutex _heap_mutex;
		mutable std::shared_mutex _resource_mutex;
		std::unordered_map<UINT64, UINT> _sets;
		std::unordered_map<SIZE_T, ID3D12Resource *> _views;
#if RESHADE_ADDON
		std::vector<ID3D12DescriptorHeap *> _descriptor_heaps;
		std::vector<std::pair<ID3D12Resource *, D3D12_GPU_VIRTUAL_ADDRESS_RANGE>> _buffer_gpu_addresses;
#endif

		com_ptr<ID3D12PipelineState> _mipmap_pipeline;
		com_ptr<ID3D12RootSignature> _mipmap_signature;
	};
}
