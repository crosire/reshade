/*
 * Copyright (C) 2021 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <glad/wgl.h>
#include "reshade_api_object_impl.hpp"
#include <unordered_map>

namespace reshade::opengl
{
	class device_impl;

	class device_context_impl : public api::api_object_impl<HGLRC, api::command_queue, api::command_list>
	{
	public:
		device_context_impl(device_impl *device, HGLRC hglrc);
		~device_context_impl();

		api::device *get_device() final;

		api::command_queue_type get_type() const final { return api::command_queue_type::graphics | api::command_queue_type::compute | api::command_queue_type::copy; }

		void wait_idle() const final;

		void flush_immediate_command_list() const final;

		api::command_list *get_immediate_command_list() final { return this; }

		void barrier(uint32_t count, const api::resource *resources, const api::resource_usage *old_states, const api::resource_usage *new_states) final;

		void begin_render_pass(uint32_t count, const api::render_pass_render_target_desc *rts, const api::render_pass_depth_stencil_desc *ds) final;
		void end_render_pass() final;
		void bind_render_targets_and_depth_stencil(uint32_t count, const api::resource_view *rtvs, api::resource_view dsv) final;

		void bind_framebuffer_with_resource(GLenum target, GLenum attachment, api::resource dest, uint32_t dest_subresource, const api::resource_desc &dest_desc);
		void bind_framebuffer_with_resource_views(GLenum target, uint32_t count, const api::resource_view *rtvs, api::resource_view dsv);

		void update_default_framebuffer(unsigned int width, unsigned int height);
		void update_current_window_height(api::resource_view default_attachment);

		void bind_pipeline(api::pipeline_stage stages, api::pipeline pipeline) final;
		void bind_pipeline_states(uint32_t count, const api::dynamic_state *states, const uint32_t *values) final;
		void bind_viewports(uint32_t first, uint32_t count, const api::viewport *viewports) final;
		void bind_scissor_rects(uint32_t first, uint32_t count, const api::rect *rects) final;

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
		void resolve_texture_region(api::resource source, uint32_t source_subresource, const api::subresource_box *source_box, api::resource dest, uint32_t dest_subresource, uint32_t dest_x, uint32_t dest_y, uint32_t dest_z, api::format format) final;

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
		void query_acceleration_structures(uint32_t count, const api::resource_view *acceleration_structures, api::query_heap heap, api::query_type type, uint32_t first) final;

		void update_buffer_region(const void *data, api::resource dest, uint64_t dest_offset, uint64_t size) final;
		void update_texture_region(const api::subresource_data &data, api::resource dest, uint32_t dest_subresource, const api::subresource_box *dest_box) final;

		void begin_debug_event(const char *label, const float color[4]) final;
		void end_debug_event() final;
		void insert_debug_marker(const char *label, const float color[4]) final;

		bool wait(api::fence fence, uint64_t value) final;
		bool signal(api::fence fence, uint64_t value) final;

		uint64_t get_timestamp_frequency() const final { return 1000000000; /* Assume nanoseconds */ }

		bool _current_vao_dirty = true;
		bool _current_ibo_dirty = true;
		bool _current_vbo_dirty = true;

		GLenum _current_prim_mode = GL_NONE;
		GLenum _current_index_type = GL_UNSIGNED_INT;
		GLuint _current_vertex_count = 0; // Used to calculate vertex count inside 'glBegin'/'glEnd' pairs
		GLuint _current_window_height = 0; // Current height of the window coordinate system

		// Each render context may be active with a different device context, corresponding to different dimensions (pixel format has to match, so texture format etc. are identical to '_default_fbo_desc' of the device)
		unsigned int _default_fbo_width = 0;
		unsigned int _default_fbo_height = 0;

	private:
		device_impl *const _device_impl;

		std::vector<GLuint> _push_constants;
		std::vector<GLuint> _push_constants_size;

		// Framebuffer and vertex array objects cannot be shared between render contexts, so have to create them for each one
		uint64_t _last_fbo_lookup_version = 0;
		std::unordered_map<size_t, GLuint> _fbo_lookup;
		uint64_t _last_vao_lookup_version = 0;
		std::unordered_map<size_t, GLuint> _vao_lookup;
	};
}
