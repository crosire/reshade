/*
 * Copyright (C) 2021 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

namespace reshade::opengl
{
	class device_impl;

	class render_context_impl : public api::api_object_impl<HGLRC, api::command_queue, api::command_list>
	{
	public:
		render_context_impl(device_impl *device, HGLRC hglrc);
		~render_context_impl();

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

		api::resource_view get_framebuffer_attachment(GLuint framebuffer, GLenum type, uint32_t index) const;

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

		void begin_debug_event(const char *label, const float color[4]) final;
		void end_debug_event() final;
		void insert_debug_marker(const char *label, const float color[4]) final;

		bool wait(api::fence fence, uint64_t value) final;
		bool signal(api::fence fence, uint64_t value) final;

		uint64_t get_timestamp_frequency() const final { return 1000000000; /* Assume nanoseconds */ }

		GLuint _current_ibo = 0;
		GLenum _current_prim_mode = GL_NONE;
		GLenum _current_index_type = GL_UNSIGNED_INT;
		GLuint _current_vertex_count = 0; // Used to calculate vertex count inside 'glBegin'/'glEnd' pairs
		GLuint _current_window_height = 0; // Current height of the window coordinate system

	private:
		device_impl *const _device_impl;

		// Programs and framebuffer objects cannot be shared between render contexts, so have to create them for each one
		GLuint _mipmap_program = 0;
		GLuint _mipmap_sampler = 0;

		GLuint _push_constants = 0;
		GLuint _push_constants_size = 0;

	protected:
		std::unordered_map<size_t, GLuint> _fbo_lookup;
	};
}
