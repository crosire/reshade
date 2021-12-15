/*
 * Copyright (C) 2021 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

namespace reshade::d3d11
{
	class device_impl;

	class command_list_impl : public api::api_object_impl<ID3D11CommandList *, api::command_list>
	{
	public:
		command_list_impl(device_impl *device, ID3D11CommandList *cmd_list);
		~command_list_impl();

		api::device *get_device() final;

		void barrier(uint32_t, const api::resource *, const api::resource_usage *, const api::resource_usage *) final { assert(false); }

		void begin_render_pass(uint32_t, const api::render_pass_render_target_desc *, const api::render_pass_depth_stencil_desc *) final { assert(false); }
		void end_render_pass() final { assert(false); }
		void bind_render_targets_and_depth_stencil(uint32_t, const api::resource_view *, api::resource_view) final { assert(false); }

		void bind_pipeline(api::pipeline_stage, api::pipeline) final { assert(false); }
		void bind_pipeline_states(uint32_t, const api::dynamic_state *, const uint32_t *) final { assert(false); }
		void bind_viewports(uint32_t, uint32_t, const api::viewport *) final { assert(false); }
		void bind_scissor_rects(uint32_t, uint32_t, const api::rect *) final { assert(false); }

		void push_constants(api::shader_stage, api::pipeline_layout, uint32_t, uint32_t, uint32_t, const void *) final { assert(false); }
		void push_descriptors(api::shader_stage, api::pipeline_layout, uint32_t, const api::descriptor_set_update &) final { assert(false); }
		void bind_descriptor_sets(api::shader_stage, api::pipeline_layout, uint32_t, uint32_t, const api::descriptor_set *) final { assert(false); }

		void bind_index_buffer(api::resource, uint64_t, uint32_t) final { assert(false); }
		void bind_vertex_buffers(uint32_t, uint32_t, const api::resource *, const uint64_t *, const uint32_t *) final { assert(false); }

		void draw(uint32_t, uint32_t, uint32_t, uint32_t) final { assert(false); }
		void draw_indexed(uint32_t, uint32_t, uint32_t, int32_t, uint32_t) final { assert(false); }
		void dispatch(uint32_t, uint32_t, uint32_t) final { assert(false); }
		void draw_or_dispatch_indirect(api::indirect_command, api::resource, uint64_t, uint32_t, uint32_t) final { assert(false); }

		void copy_resource(api::resource, api::resource) final { assert(false); }
		void copy_buffer_region(api::resource, uint64_t, api::resource, uint64_t, uint64_t) final { assert(false); }
		void copy_buffer_to_texture(api::resource, uint64_t, uint32_t, uint32_t, api::resource, uint32_t, const api::subresource_box *) final { assert(false); }
		void copy_texture_region(api::resource, uint32_t, const api::subresource_box *, api::resource, uint32_t, const api::subresource_box *, api::filter_mode) final { assert(false); }
		void copy_texture_to_buffer(api::resource, uint32_t, const api::subresource_box *, api::resource, uint64_t, uint32_t, uint32_t) final { assert(false); }
		void resolve_texture_region(api::resource, uint32_t, const api::rect *, api::resource, uint32_t, int32_t, int32_t, api::format) final { assert(false); }

		void clear_depth_stencil_view(api::resource_view, const float *, const uint8_t *, uint32_t, const api::rect *) final { assert(false); }
		void clear_render_target_view(api::resource_view, const float[4], uint32_t, const api::rect *) final { assert(false); }
		void clear_unordered_access_view_uint(api::resource_view, const uint32_t[4], uint32_t, const api::rect *) final { assert(false); }
		void clear_unordered_access_view_float(api::resource_view, const float[4], uint32_t, const api::rect *) final { assert(false); }

		void generate_mipmaps(api::resource_view) final { assert(false); }

		void begin_query(api::query_pool, api::query_type, uint32_t) final { assert(false); }
		void end_query(api::query_pool, api::query_type, uint32_t) final { assert(false); }
		void copy_query_pool_results(api::query_pool, api::query_type, uint32_t, uint32_t, api::resource, uint64_t, uint32_t) final { assert(false); }

		void begin_debug_event(const char *, const float[4]) final { assert(false); }
		void end_debug_event() final { assert(false); }
		void insert_debug_marker(const char *, const float[4]) final { assert(false); }

	private:
		device_impl *const _device_impl;
	};

	class device_context_impl : public api::api_object_impl<ID3D11DeviceContext *, api::command_queue, api::command_list>
	{
		friend class swapchain_impl;

	public:
		device_context_impl(device_impl *device, ID3D11DeviceContext *context);
		~device_context_impl();

		api::device *get_device() final;

		api::command_list *get_immediate_command_list() final;

		void wait_idle() const final { /* no-op */ }

		void flush_immediate_command_list() const final;

		void barrier(uint32_t count, const api::resource *resources, const api::resource_usage *old_states, const api::resource_usage *new_states) final;

		void begin_render_pass(uint32_t count, const api::render_pass_render_target_desc *rts, const api::render_pass_depth_stencil_desc *ds) final;
		void end_render_pass() final;
		void bind_render_targets_and_depth_stencil(uint32_t count, const api::resource_view *rtvs, api::resource_view dsv) final;

		void bind_pipeline(api::pipeline_stage type, api::pipeline pipeline) final;
		void bind_pipeline_states(uint32_t count, const api::dynamic_state *states, const uint32_t *values) final;
		void bind_viewports(uint32_t first, uint32_t count, const api::viewport *viewports) final;
		void bind_scissor_rects(uint32_t first, uint32_t count, const api::rect *rects) final;

		void bind_samplers(api::shader_stage stages, uint32_t first, uint32_t count, const api::sampler *samplers);
		void bind_shader_resource_views(api::shader_stage stages, uint32_t first, uint32_t count, const api::resource_view *views);
		void bind_unordered_access_views(api::shader_stage stages, uint32_t first, uint32_t count, const api::resource_view *views);
		void bind_constant_buffers(api::shader_stage stages, uint32_t first, uint32_t count, const api::buffer_range *buffer_ranges);

		void push_constants(api::shader_stage stages, api::pipeline_layout layout, uint32_t layout_param, uint32_t first, uint32_t count, const void *values) final;
		void push_descriptors(api::shader_stage stages, api::pipeline_layout layout, uint32_t layout_param, const api::descriptor_set_update &update) final;
		void bind_descriptor_sets(api::shader_stage stages, api::pipeline_layout layout, uint32_t first, uint32_t count, const api::descriptor_set *sets) final;

		void bind_index_buffer(api::resource buffer, uint64_t offset, uint32_t index_size) final;
		void bind_vertex_buffers(uint32_t first, uint32_t count, const api::resource *buffers, const uint64_t *offsets, const uint32_t *strides) final;

		void draw(uint32_t vertex_count, uint32_t instance_count, uint32_t first_vertex, uint32_t first_instance) final;
		void draw_indexed(uint32_t index_count, uint32_t instance_count, uint32_t first_index, int32_t vertex_offset, uint32_t first_instance) final;
		void dispatch(uint32_t group_count_x, uint32_t group_count_y, uint32_t group_count_z) final;
		void draw_or_dispatch_indirect(api::indirect_command type, api::resource buffer, uint64_t offset, uint32_t draw_count, uint32_t stride) final;

		void copy_resource(api::resource source, api::resource dest) final;
		void copy_buffer_region(api::resource source, uint64_t source_offset, api::resource dest, uint64_t dest_offset, uint64_t size) final;
		void copy_buffer_to_texture(api::resource source, uint64_t source_offset, uint32_t row_length, uint32_t slice_height, api::resource dest, uint32_t dest_subresource, const api::subresource_box *dest_box) final;
		void copy_texture_region(api::resource source, uint32_t source_subresource, const api::subresource_box *source_box, api::resource dest, uint32_t dest_subresource, const api::subresource_box *dest_box, api::filter_mode filter) final;
		void copy_texture_to_buffer(api::resource source, uint32_t source_subresource, const api::subresource_box *source_box, api::resource dest, uint64_t dest_offset, uint32_t row_length, uint32_t slice_height) final;
		void resolve_texture_region(api::resource source, uint32_t source_subresource, const api::rect *source_rect, api::resource dest, uint32_t dest_subresource, int32_t dest_x, int32_t dest_y, api::format format) final;

		void clear_depth_stencil_view(api::resource_view dsv, const float *depth, const uint8_t *stencil, uint32_t rect_count, const api::rect *rects) final;
		void clear_render_target_view(api::resource_view rtv, const float color[4], uint32_t rect_count, const api::rect *rects) final;
		void clear_unordered_access_view_uint(api::resource_view uav, const uint32_t values[4], uint32_t rect_count, const api::rect *rects) final;
		void clear_unordered_access_view_float(api::resource_view uav, const float values[4], uint32_t rect_count, const api::rect *rects) final;

		void generate_mipmaps(api::resource_view srv) final;

		void begin_query(api::query_pool pool, api::query_type type, uint32_t index) final;
		void end_query(api::query_pool pool, api::query_type type, uint32_t index) final;
		void copy_query_pool_results(api::query_pool pool, api::query_type type, uint32_t first, uint32_t count, api::resource dest, uint64_t dest_offset, uint32_t stride) final;

		void begin_debug_event(const char *label, const float color[4]) final;
		void end_debug_event() final;
		void insert_debug_marker(const char *label, const float color[4]) final;

	private:
		device_impl *const _device_impl;
		com_ptr<ID3DUserDefinedAnnotation> _annotations;

		UINT _push_constants_size = 0;
		com_ptr<ID3D11Buffer> _push_constants;
	};
}
