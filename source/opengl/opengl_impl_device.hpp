/*
 * Copyright (C) 2021 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include "opengl.hpp"
#include "addon_manager.hpp"
#include <unordered_map>
#include <unordered_set>

namespace reshade::opengl
{
	inline auto make_resource_handle(GLenum target, GLuint object) -> api::resource
	{
		if (!object)
			return { 0 };
		return { (static_cast<uint64_t>(target) << 40) | object };
	}
	inline auto make_framebuffer_handle(GLuint object, uint32_t num_color_attachments = 8, uint8_t extra_bits = 0) -> api::framebuffer
	{
		if (object == 0)
			num_color_attachments = 1;
		return { (static_cast<uint64_t>(num_color_attachments) << 40) | (static_cast<uint64_t>(extra_bits) << 32) | object };
	}
	inline auto make_resource_view_handle(GLenum target, GLuint object, uint8_t extra_bits = 0) -> api::resource_view
	{
		return { (static_cast<uint64_t>(target) << 40) | (static_cast<uint64_t>(extra_bits) << 32) | object };
	}

	class device_impl : public api::api_object_impl<HGLRC, api::device, api::command_queue, api::command_list>
	{
	public:
		device_impl(HDC initial_hdc, HGLRC hglrc);
		~device_impl();

		api::device_api get_api() const final { return api::device_api::opengl; }

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

		bool create_pipeline_layout(const api::pipeline_layout_desc &desc, api::pipeline_layout *out) final;
		void destroy_pipeline_layout(api::pipeline_layout handle) final;

		bool create_query_pool(api::query_type type, uint32_t size, api::query_pool *out) final;
		void destroy_query_pool(api::query_pool handle) final;

		bool create_render_pass(const api::render_pass_desc &desc, api::render_pass *out) final;
		void destroy_render_pass(api::render_pass handle) final;

		bool create_framebuffer(const api::framebuffer_desc &desc, api::framebuffer *out) final;
		void destroy_framebuffer(api::framebuffer handle) final;

		bool map_resource(api::resource resource, uint32_t subresource, api::map_access access, void **data, uint32_t *row_pitch, uint32_t *slice_pitch) final;
		void unmap_resource(api::resource resource, uint32_t subresource) final;

		void upload_buffer_region(const void *data, api::resource dst, uint64_t dst_offset, uint64_t size) final;
		void upload_texture_region(const api::subresource_data &data, api::resource dst, uint32_t dst_subresource, const int32_t dst_box[6]) final;

		bool get_query_pool_results(api::query_pool pool, uint32_t first, uint32_t count, void *results, uint32_t stride) final;

		bool allocate_descriptor_sets(api::pipeline_layout layout, uint32_t param_index, uint32_t count, api::descriptor_set *out) final;
		void free_descriptor_sets(api::pipeline_layout layout, uint32_t param_index, uint32_t count, const api::descriptor_set *sets) final;

		void update_descriptor_sets(uint32_t num_writes, const api::write_descriptor_set *writes, uint32_t num_copies, const api::copy_descriptor_set *copies) final;

		void wait_idle() const final;

		void set_resource_name(api::resource resource, const char *name) final;

		api::resource_desc get_resource_desc(api::resource resource) const final;
		api::pipeline_layout_desc get_pipeline_layout_desc(api::pipeline_layout layout) const final;
		void get_resource_from_view(api::resource_view view, api::resource *out) const final;
		bool get_framebuffer_attachment(api::framebuffer framebuffer, api::attachment_type type, uint32_t index, api::resource_view *out) const final;

		api::device *get_device() override { return this; }

		api::command_list *get_immediate_command_list() final { return this; }

		void flush_immediate_command_list() const final;

		void barrier(uint32_t, const api::resource *, const api::resource_usage *, const api::resource_usage *) final { /* no-op */ }

		void begin_render_pass(api::render_pass pass, api::framebuffer framebuffer) final;
		void finish_render_pass() final;
		void bind_render_targets_and_depth_stencil(uint32_t count, const api::resource_view *rtvs, api::resource_view dsv) final;

		void bind_pipeline(api::pipeline_stage type, api::pipeline pipeline) final;
		void bind_pipeline_states(uint32_t count, const api::dynamic_state *states, const uint32_t *values) final;
		void bind_viewports(uint32_t first, uint32_t count, const float *viewports) final;
		void bind_scissor_rects(uint32_t first, uint32_t count, const int32_t *rects) final;

		void push_constants(api::shader_stage stages, api::pipeline_layout layout, uint32_t layout_index, uint32_t first, uint32_t count, const void *values) final;
		void push_descriptors(api::shader_stage stages, api::pipeline_layout layout, uint32_t layout_index, api::descriptor_type type, uint32_t first, uint32_t count, const void *descriptors) final;
		void bind_descriptor_sets(api::shader_stage stages, api::pipeline_layout layout, uint32_t first, uint32_t count, const api::descriptor_set *sets) final;

		void bind_index_buffer(api::resource buffer, uint64_t offset, uint32_t index_size) final;
		void bind_vertex_buffers(uint32_t first, uint32_t count, const api::resource *buffers, const uint64_t *offsets, const uint32_t *strides) final;

		void draw(uint32_t vertices, uint32_t instances, uint32_t first_vertex, uint32_t first_instance) final;
		void draw_indexed(uint32_t indices, uint32_t instances, uint32_t first_index, int32_t vertex_offset, uint32_t first_instance) final;
		void dispatch(uint32_t num_groups_x, uint32_t num_groups_y, uint32_t num_groups_z) final;
		void draw_or_dispatch_indirect(api::indirect_command type, api::resource buffer, uint64_t offset, uint32_t draw_count, uint32_t stride) final;

		void copy_resource(api::resource src, api::resource dst) final;
		void copy_buffer_region(api::resource src, uint64_t src_offset, api::resource dst, uint64_t dst_offset, uint64_t size) final;
		void copy_buffer_to_texture(api::resource src, uint64_t src_offset, uint32_t row_length, uint32_t slice_height, api::resource dst, uint32_t dst_subresource, const int32_t dst_box[6]) final;
		void copy_texture_region(api::resource src, uint32_t src_subresource, const int32_t src_box[6], api::resource dst, uint32_t dst_subresource, const int32_t dst_box[6], api::filter_type filter) final;
		void copy_texture_to_buffer(api::resource src, uint32_t src_subresource, const int32_t src_box[6], api::resource dst, uint64_t dst_offset, uint32_t row_length, uint32_t slice_height) final;
		void resolve_texture_region(api::resource src, uint32_t src_subresource, const int32_t src_box[6], api::resource dst, uint32_t dst_subresource, const int32_t dst_offset[3], api::format format) final;

		void clear_attachments(api::attachment_type clear_flags, const float color[4], float depth, uint8_t stencil, uint32_t num_rects, const int32_t *rects) final;
		void clear_depth_stencil_view(api::resource_view dsv, api::attachment_type clear_flags, float depth, uint8_t stencil, uint32_t num_rects, const int32_t *rects) final;
		void clear_render_target_view(api::resource_view rtv, const float color[4], uint32_t num_rects, const int32_t *rects) final;
		void clear_unordered_access_view_uint(api::resource_view uav, const uint32_t values[4], uint32_t num_rects, const int32_t *rects) final;
		void clear_unordered_access_view_float(api::resource_view uav, const float values[4], uint32_t num_rects, const int32_t *rects) final;

		void generate_mipmaps(api::resource_view srv) final;

		void begin_query(api::query_pool heap, api::query_type type, uint32_t index) final;
		void finish_query(api::query_pool heap, api::query_type type, uint32_t index) final;
		void copy_query_pool_results(api::query_pool heap, api::query_type type, uint32_t first, uint32_t count, api::resource dst, uint64_t dst_offset, uint32_t stride) final;

		void begin_debug_event(const char *label, const float color[4]) final;
		void finish_debug_event() final;
		void insert_debug_marker(const char *label, const float color[4]) final;

		bool _compatibility_context = false;
		std::unordered_set<HDC> _hdcs;
		api::pipeline_layout _global_pipeline_layout = { 0 };
		GLuint _current_fbo = 0;
		GLuint _current_ibo = 0;
		GLenum _current_prim_mode = GL_NONE;
		GLenum _current_index_type = GL_UNSIGNED_INT;
		GLuint _current_vertex_count = 0; // Used to calculate vertex count inside glBegin/glEnd pairs

	private:
		std::vector<GLuint> _reserved_texture_names;
		GLuint _copy_fbo[2] = {};
		GLuint _mipmap_program = 0;
		GLuint _push_constants = 0;
		GLuint _push_constants_size = 0;

	protected:
		// Cached context information for quick access
		GLuint _default_fbo_width = 0;
		GLuint _default_fbo_height = 0;
		GLenum _default_color_format = GL_NONE;
		GLenum _default_depth_format = GL_NONE;
	};
}
