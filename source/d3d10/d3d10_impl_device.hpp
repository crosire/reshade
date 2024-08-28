/*
 * Copyright (C) 2021 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <d3d10_1.h>
#include "com_ptr.hpp"
#include "reshade_api_object_impl.hpp"

namespace reshade::d3d10
{
	class device_impl : public api::api_object_impl<ID3D10Device1 *, api::device, api::command_queue, api::command_list>
	{
	public:
		explicit device_impl(ID3D10Device1 *device);

		api::device_api get_api() const final { return api::device_api::d3d10; }

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
		bool create_geometry_shader(const api::shader_desc &desc, api::pipeline *out_pipeline);
		bool create_pixel_shader(const api::shader_desc &desc, api::pipeline *out_pipeline);
		bool create_rasterizer_state(const api::rasterizer_desc &desc, api::pipeline *out_pipeline);
		bool create_blend_state(const api::blend_desc &desc, api::pipeline *out_pipeline);
		bool create_depth_stencil_state(const api::depth_stencil_desc &desc, api::pipeline *out_pipeline);

		bool create_pipeline(api::pipeline_layout layout, uint32_t subobject_count, const api::pipeline_subobject *subobjects, api::pipeline *out_pipeline) final;
		void destroy_pipeline(api::pipeline pipeline) final;

		bool create_pipeline_layout(uint32_t param_count, const api::pipeline_layout_param *params, api::pipeline_layout *out_layout) final;
		void destroy_pipeline_layout(api::pipeline_layout layout) final;

		bool allocate_descriptor_tables(uint32_t count, api::pipeline_layout layout, uint32_t layout_param, api::descriptor_table *out_tables) final;
		void free_descriptor_tables(uint32_t count, const api::descriptor_table *tables) final;

		void get_descriptor_heap_offset(api::descriptor_table table, uint32_t binding, uint32_t array_offset, api::descriptor_heap *out_heap, uint32_t *offset) const final;

		void copy_descriptor_tables(uint32_t count, const api::descriptor_table_copy *copies) final;
		void update_descriptor_tables(uint32_t count, const api::descriptor_table_update *updates) final;

		bool create_query_heap(api::query_type type, uint32_t size, api::query_heap *out_heap) final;
		void destroy_query_heap(api::query_heap heap) final;

		bool get_query_heap_results(api::query_heap heap, uint32_t first, uint32_t count, void *results, uint32_t stride) final;

		void set_resource_name(api::resource resource, const char *name) final;
		void set_resource_view_name(api::resource_view view, const char *name) final;

		bool create_fence(uint64_t initial_value, api::fence_flags flags, api::fence *out_fence, HANDLE *shared_handle = nullptr) final;
		void destroy_fence(api::fence fence) final;

		uint64_t get_completed_fence_value(api::fence fence) const final;

		bool wait(api::fence fence, uint64_t value, uint64_t timeout) final;
		bool wait(api::fence fence, uint64_t value) final { return wait(fence, value, UINT64_MAX); }
		bool signal(api::fence fence, uint64_t value) final;

		void get_acceleration_structure_size(api::acceleration_structure_type type, api::acceleration_structure_build_flags flags, uint32_t input_count, const api::acceleration_structure_build_input *inputs, uint64_t *out_size, uint64_t *out_build_scratch_size, uint64_t *out_update_scratch_size) const final;

		bool get_pipeline_shader_group_handles(api::pipeline pipeline, uint32_t first, uint32_t count, void *out_handles) final;

		uint64_t get_timestamp_frequency() const final;

		api::device *get_device() final { return this; }

		api::command_queue_type get_type() const final { return api::command_queue_type::graphics | api::command_queue_type::copy; }

		void wait_idle() const final;

		void flush_immediate_command_list() const final;

		api::command_list *get_immediate_command_list() final { return this; }

		void barrier(uint32_t count, const api::resource *resources, const api::resource_usage *old_states, const api::resource_usage *new_states) final;

		void begin_render_pass(uint32_t count, const api::render_pass_render_target_desc *rts, const api::render_pass_depth_stencil_desc *ds) final;
		void end_render_pass() final;
		void bind_render_targets_and_depth_stencil(uint32_t count, const api::resource_view *rtvs, api::resource_view dsv) final;

		void bind_pipeline(api::pipeline_stage stages, api::pipeline pipeline) final;
		void bind_pipeline_states(uint32_t count, const api::dynamic_state *states, const uint32_t *values) final;
		void bind_viewports(uint32_t first, uint32_t count, const api::viewport *viewports) final;
		void bind_scissor_rects(uint32_t first, uint32_t count, const api::rect *rects) final;

		void bind_samplers(api::shader_stage stages, uint32_t first, uint32_t count, const api::sampler *samplers);
		void bind_shader_resource_views(api::shader_stage stages, uint32_t first, uint32_t count, const api::resource_view *views);
		void bind_constant_buffers(api::shader_stage stages, uint32_t first, uint32_t count, const api::buffer_range *buffer_ranges);

		void push_constants(api::shader_stage stages, api::pipeline_layout layout, uint32_t layout_param, uint32_t first, uint32_t count, const void *values) final;
		void push_descriptors(api::shader_stage stages, api::pipeline_layout layout, uint32_t layout_param, const api::descriptor_table_update &update) final;
		void bind_descriptor_tables(api::shader_stage stages, api::pipeline_layout layout, uint32_t first, uint32_t count, const api::descriptor_table *tables) final;

		void bind_index_buffer(api::resource buffer, uint64_t offset, uint32_t index_size) final;
		void bind_vertex_buffers(uint32_t first, uint32_t count, const api::resource *buffers, const uint64_t *offsets, const uint32_t *strides) final;
		void bind_stream_output_buffers(uint32_t first, uint32_t count, const api::resource *buffers, const uint64_t *offsets, const uint64_t *max_sizes, const api::resource *counter_buffers, const uint64_t *counter_offsets) final;

		void draw(uint32_t vertex_count, uint32_t instance_count, uint32_t first_vertex, uint32_t first_instance) final;
		void draw_indexed(uint32_t index_count, uint32_t instance_count, uint32_t first_index, int32_t vertex_offset, uint32_t first_instance) final;
		void dispatch(uint32_t group_count_x, uint32_t group_count_y, uint32_t group_count_z) final;
		void dispatch_mesh(uint32_t group_count_x, uint32_t group_count_y, uint32_t group_count_z) final;
		void dispatch_rays(api::resource raygen, uint64_t raygen_offset, uint64_t raygen_size, api::resource miss, uint64_t miss_offset, uint64_t miss_size, uint64_t miss_stride, api::resource hit_group, uint64_t hit_group_offset, uint64_t hit_group_size, uint64_t hit_group_stride, api::resource callable, uint64_t callable_offset, uint64_t callable_size, uint64_t callable_stride, uint32_t width, uint32_t height, uint32_t depth) final;
		void draw_or_dispatch_indirect(api::indirect_command type, api::resource buffer, uint64_t offset, uint32_t draw_count, uint32_t stride) final;

		void copy_resource(api::resource source, api::resource dest) final;
		void copy_buffer_region(api::resource source, uint64_t source_offset, api::resource dest, uint64_t dest_offset, uint64_t size) final;
		void copy_buffer_to_texture(api::resource source, uint64_t source_offset, uint32_t row_length, uint32_t slice_height, api::resource dest, uint32_t dest_subresource, const api::subresource_box *dest_box) final;
		void copy_texture_region(api::resource source, uint32_t source_subresource, const api::subresource_box *source_box, api::resource dest, uint32_t dest_subresource, const api::subresource_box *dest_box, api::filter_mode filter) final;
		void copy_texture_to_buffer(api::resource source, uint32_t source_subresource, const api::subresource_box *source_box, api::resource dest, uint64_t dest_offset, uint32_t row_length, uint32_t slice_height) final;
		void resolve_texture_region(api::resource source, uint32_t source_subresource, const api::subresource_box *source_box, api::resource dest, uint32_t dest_subresource, int32_t dest_x, int32_t dest_y, int32_t dest_z, api::format format) final;

		void clear_depth_stencil_view(api::resource_view dsv, const float *depth, const uint8_t *stencil, uint32_t rect_count, const api::rect *rects) final;
		void clear_render_target_view(api::resource_view rtv, const float color[4], uint32_t rect_count, const api::rect *rects) final;
		void clear_unordered_access_view_uint(api::resource_view uav, const uint32_t values[4], uint32_t rect_count, const api::rect *rects) final;
		void clear_unordered_access_view_float(api::resource_view uav, const float values[4], uint32_t rect_count, const api::rect *rects) final;

		void generate_mipmaps(api::resource_view srv) final;

		void begin_query(api::query_heap heap, api::query_type type, uint32_t index) final;
		void end_query(api::query_heap heap, api::query_type type, uint32_t index) final;
		void copy_query_heap_results(api::query_heap heap, api::query_type type, uint32_t first, uint32_t count, api::resource dest, uint64_t dest_offset, uint32_t stride) final;

		void copy_acceleration_structure(api::resource_view source, api::resource_view dest, api::acceleration_structure_copy_mode mode) final;
		void build_acceleration_structure(api::acceleration_structure_type type, api::acceleration_structure_build_flags flags, uint32_t input_count, const api::acceleration_structure_build_input *inputs, api::resource scratch, uint64_t scratch_offset, api::resource_view source, api::resource_view dest, api::acceleration_structure_build_mode mode) final;

		void begin_debug_event(const char *, const float[4]) final {}
		void end_debug_event() final {}
		void insert_debug_marker(const char *, const float[4]) final {}

	private:
		com_ptr<ID3D10Buffer> _push_constants[D3D10_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];
		std::vector<uint32_t> _push_constants_data[D3D10_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];
	};
}
