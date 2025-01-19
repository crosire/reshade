/*
 * Copyright (C) 2021 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <d3d11_4.h>
#include "com_ptr.hpp"
#include "reshade_api_object_impl.hpp"

namespace reshade::d3d11
{
	class device_impl : public api::api_object_impl<ID3D11Device *, api::device>
	{
	public:
		explicit device_impl(ID3D11Device *device);

		api::device_api get_api() const final { return api::device_api::d3d11; }

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

		uint64_t get_resource_view_gpu_address(api::resource_view) const final { return 0; }

		bool map_buffer_region(api::resource resource, uint64_t offset, uint64_t size, api::map_access access, void **out_data) final;
		void unmap_buffer_region(api::resource resource) final;
		bool map_texture_region(api::resource resource, uint32_t subresource, const api::subresource_box *box, api::map_access access, api::subresource_data *out_data) final;
		void unmap_texture_region(api::resource resource, uint32_t subresource) final;

		void update_buffer_region(const void *data, api::resource resource, uint64_t offset, uint64_t size) final;
		void update_texture_region(const api::subresource_data &data, api::resource resource, uint32_t subresource, const api::subresource_box *box) final;

		bool create_input_layout(uint32_t count, const api::input_element *desc, const api::shader_desc &signature, api::pipeline *out_pipeline);
		bool create_vertex_shader(const api::shader_desc &desc, api::pipeline *out_pipeline);
		bool create_hull_shader(const api::shader_desc &desc, api::pipeline *out_pipeline);
		bool create_domain_shader(const api::shader_desc &desc, api::pipeline *out_pipeline);
		bool create_geometry_shader(const api::shader_desc &desc, api::pipeline *out_pipeline);
		bool create_pixel_shader(const api::shader_desc &desc, api::pipeline *out_pipeline);
		bool create_compute_shader(const api::shader_desc &desc, api::pipeline *out_pipeline);
		bool create_rasterizer_state(const api::rasterizer_desc &desc, api::pipeline *out_pipeline);
		bool create_blend_state(const api::blend_desc &desc, api::pipeline *out_pipeline);
		bool create_depth_stencil_state(const api::depth_stencil_desc &desc, api::pipeline *out_pipeline);

		bool create_pipeline(api::pipeline_layout layout, uint32_t subobject_count, const api::pipeline_subobject *subobjects, api::pipeline *out_pipeline) final;
		void destroy_pipeline(api::pipeline pipeline) final;

		bool create_pipeline_layout(uint32_t param_count, const api::pipeline_layout_param *params, api::pipeline_layout *out_layout) final;
		void destroy_pipeline_layout(api::pipeline_layout layout) final;

		bool allocate_descriptor_tables(uint32_t count, api::pipeline_layout layout, uint32_t layout_param, api::descriptor_table *out_tables) final;
		void free_descriptor_tables(uint32_t count, const api::descriptor_table *tables) final;

		void get_descriptor_heap_offset(api::descriptor_table table, uint32_t binding, uint32_t array_offset, api::descriptor_heap *out_heap, uint32_t *out_offset) const final;

		void copy_descriptor_tables(uint32_t count, const api::descriptor_table_copy *copies) final;
		void update_descriptor_tables(uint32_t count, const api::descriptor_table_update *updates) final;

		bool create_query_heap(api::query_type type, uint32_t count, api::query_heap *out_heap) final;
		void destroy_query_heap(api::query_heap heap) final;

		bool get_query_heap_results(api::query_heap heap, uint32_t first, uint32_t count, void *results, uint32_t stride) final;

		void set_resource_name(api::resource resource, const char *name) final;
		void set_resource_view_name(api::resource_view view, const char *name) final;

		bool create_fence(uint64_t initial_value, api::fence_flags flags, api::fence *out_fence, HANDLE *shared_handle = nullptr) final;
		void destroy_fence(api::fence fence) final;

		uint64_t get_completed_fence_value(api::fence fence) const final;

		bool wait(api::fence fence, uint64_t value, uint64_t timeout) final;
		bool signal(api::fence fence, uint64_t value) final;

		void get_acceleration_structure_size(api::acceleration_structure_type type, api::acceleration_structure_build_flags flags, uint32_t input_count, const api::acceleration_structure_build_input *inputs, uint64_t *out_size, uint64_t *out_build_scratch_size, uint64_t *out_update_scratch_size) const final;

		bool get_pipeline_shader_group_handles(api::pipeline pipeline, uint32_t first, uint32_t count, void *out_handles) final;
	};
}
