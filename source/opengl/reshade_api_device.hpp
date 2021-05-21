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
	class device_impl : public api::api_object_impl<HGLRC, api::device, api::command_queue, api::command_list>
	{
	public:
		device_impl(HDC initial_hdc, HGLRC hglrc);
		~device_impl();

		api::device_api get_api() const final { return api::device_api::opengl; }

		bool check_capability(api::device_caps capability) const final;
		bool check_format_support(api::format format, api::resource_usage usage) const final;

		bool is_resource_handle_valid(api::resource handle) const final;
		bool is_resource_view_handle_valid(api::resource_view handle) const final;

		bool create_sampler(const api::sampler_desc &desc, api::sampler *out) final;
		bool create_resource(const api::resource_desc &desc, const api::subresource_data *initial_data, api::resource_usage initial_state, api::resource *out) final;
		bool create_resource_view(api::resource resource, api::resource_usage usage_type, const api::resource_view_desc &desc, api::resource_view *out) final;

		bool create_pipeline(const api::pipeline_desc &desc, api::pipeline *out) final;
		bool create_pipeline_compute(const api::pipeline_desc &desc, api::pipeline *out);
		bool create_pipeline_graphics(const api::pipeline_desc &desc, api::pipeline *out);

		bool create_shader_module(api::shader_stage type, api::shader_format format, const void *code, size_t code_size, const char *entry_point, api::shader_module *out) final;
		bool create_pipeline_layout(uint32_t num_set_layouts, const api::descriptor_set_layout *set_layouts, uint32_t num_constant_ranges, const api::constant_range *constant_ranges, api::pipeline_layout *out) final;
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

		bool get_query_results(api::query_pool heap, uint32_t first, uint32_t count, void *results, uint32_t stride) final;

		void wait_idle() const final;

		void set_debug_name(api::resource resource, const char *name) final;

		api::resource_view get_depth_stencil_from_fbo(GLuint fbo) const;
		api::resource_view get_render_target_from_fbo(GLuint fbo, GLuint drawbuffer) const;

		void request_framebuffer(uint32_t count, const api::resource_view *rtvs, api::resource_view dsv, GLuint &fbo);

		api::device *get_device() override { return this; }

		api::command_list *get_immediate_command_list() final { return this; }

		void flush_immediate_command_list() const final;

		void barrier(uint32_t, const api::resource *, const api::resource_usage *, const api::resource_usage *) final { /* no-op */ }

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
		void finish_render_pass() final;

		void copy_resource(api::resource src, api::resource dst) final;
		void copy_buffer_region(api::resource src, uint64_t src_offset, api::resource dst, uint64_t dst_offset, uint64_t size) final;
		void copy_buffer_to_texture(api::resource src, uint64_t src_offset, uint32_t row_length, uint32_t slice_height, api::resource dst, uint32_t dst_subresource, const int32_t dst_box[6]) final;
		void copy_texture_region(api::resource src, uint32_t src_subresource, const int32_t src_box[6], api::resource dst, uint32_t dst_subresource, const int32_t dst_box[6], api::texture_filter filter) final;
		void copy_texture_to_buffer(api::resource src, uint32_t src_subresource, const int32_t src_box[6], api::resource dst, uint64_t dst_offset, uint32_t row_length, uint32_t slice_height) final;
		void resolve_texture_region(api::resource src, uint32_t src_subresource, const int32_t src_offset[3], api::resource dst, uint32_t dst_subresource, const int32_t dst_offset[3], const uint32_t size[3], api::format format) final;

		void generate_mipmaps(api::resource_view srv) final;

		void clear_depth_stencil_view(api::resource_view dsv, uint32_t clear_flags, float depth, uint8_t stencil) final;
		void clear_render_target_views(uint32_t count, const api::resource_view *rtvs, const float color[4]) final;
		void clear_unordered_access_view_uint(api::resource_view uav, const uint32_t values[4]) final;
		void clear_unordered_access_view_float(api::resource_view uav, const float values[4]) final;

		void begin_query(api::query_pool heap, api::query_type type, uint32_t index) final;
		void finish_query(api::query_pool heap, api::query_type type, uint32_t index) final;
		void copy_query_results(api::query_pool heap, api::query_type type, uint32_t first, uint32_t count, api::resource dst, uint64_t dst_offset, uint32_t stride) final;

		void add_debug_marker(const char *label, const float color[4]) final;
		void begin_debug_marker(const char *label, const float color[4]) final;
		void finish_debug_marker() final;

	public:
		bool _compatibility_context = false;
		std::unordered_set<HDC> _hdcs;
		GLenum _current_prim_mode = GL_NONE;
		GLenum _current_index_type = GL_UNSIGNED_INT;
		GLuint _current_vertex_count = 0; // Used to calculate vertex count inside glBegin/glEnd pairs

	private:
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
		std::unordered_map<size_t, GLuint> _framebuffer_list_internal;
	};
}
