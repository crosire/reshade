/*
 * Copyright (C) 2021 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

namespace reshade::vulkan
{
	class device_impl;

	class command_list_impl : public api::api_object_impl<VkCommandBuffer, api::command_list>
	{
		friend class device_impl;

	public:
		command_list_impl(device_impl *device, VkCommandBuffer cmd_list);

		api::device *get_device() final;

		void barrier(uint32_t count, const api::resource *resources, const api::resource_usage *old_states, const api::resource_usage *new_states) final;

		void begin_render_pass(uint32_t count, const api::render_pass_render_target_desc *rts, const api::render_pass_depth_stencil_desc *ds) final;
		void end_render_pass() final;
		void bind_render_targets_and_depth_stencil(uint32_t count, const api::resource_view *rtvs, api::resource_view dsv) final;

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

		void begin_debug_event(const char *label, const float color[4]) final;
		void end_debug_event() final;
		void insert_debug_marker(const char *label, const float color[4]) final;

	protected:
		device_impl *const _device_impl;
		bool _has_commands = false;
		bool _is_in_render_pass = false;
	};

	template <>
	struct object_data<VK_OBJECT_TYPE_COMMAND_BUFFER> : public command_list_impl
	{
		using Handle = VkCommandBuffer;

		object_data(device_impl *device, VkCommandBuffer cmd_list) : command_list_impl(device, cmd_list) {}

#if RESHADE_ADDON
		using command_list_impl::_is_in_render_pass;

		uint32_t current_subpass = std::numeric_limits<uint32_t>::max();
		VkRenderPass current_render_pass = VK_NULL_HANDLE;
		VkFramebuffer current_framebuffer = VK_NULL_HANDLE;
		VkImageView current_color_attachments[8] = {};
		VkImageView current_depth_stencil_attachment = VK_NULL_HANDLE;
#endif
	};
}
