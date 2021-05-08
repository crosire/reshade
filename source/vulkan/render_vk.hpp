/*
 * Copyright (C) 2021 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include "addon_manager.hpp"
#include "lockfree_table.hpp"
#pragma warning(push)
#pragma warning(disable: 4100 4127 4324 4703) // Disable a bunch of warnings thrown by VMA code
#include <vk_mem_alloc.h>
#pragma warning(pop)
#include <vk_layer_dispatch_table.h>
#include <unordered_map>

namespace reshade::vulkan
{
	struct pipeline_data
	{
		uint32_t values[35];
	};

	struct render_pass_data
	{
		struct subpass
		{
			std::vector<VkAttachmentReference> color_attachments;
			VkAttachmentReference depth_stencil_attachment;
		};
		struct cleared_attachment
		{
			uint32_t index;
			uint32_t clear_flags;
			VkImageLayout initial_layout;
		};

		std::vector<subpass> subpasses;
		std::vector<cleared_attachment> cleared_attachments;
	};

	struct resource_data
	{
		bool is_image() const { return type != VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO; }

		union
		{
			VkImage image;
			VkBuffer buffer;
		};
		union
		{
			VkStructureType type;
			VkImageCreateInfo image_create_info;
			VkBufferCreateInfo buffer_create_info;
		};

		VmaAllocation allocation;
	};

	struct resource_view_data
	{
		bool is_image_view() const { return type != VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO; }

		union
		{
			VkImageView image_view;
			VkBufferView buffer_view;
		};
		union
		{
			VkStructureType type;
			VkImageViewCreateInfo image_create_info;
			VkBufferViewCreateInfo buffer_create_info;
		};
	};

	class device_impl : public api::api_object_impl<VkDevice, api::device>
	{
		friend class command_queue_impl;

	public:
		device_impl(VkDevice device, VkPhysicalDevice physical_device, const VkLayerInstanceDispatchTable &instance_table, const VkLayerDispatchTable &device_table);
		~device_impl();

		api::render_api get_api() const final { return api::render_api::vulkan; }

		bool check_format_support(api::format format, api::resource_usage usage) const final;

		bool check_resource_handle_valid(api::resource resource) const final;
		bool check_resource_view_handle_valid(api::resource_view view) const final;

		bool create_sampler(const api::sampler_desc &desc, api::sampler *out) final;
		bool create_resource(const api::resource_desc &desc, const api::subresource_data *initial_data, api::resource_usage initial_state, api::resource *out) final;
		bool create_resource_view(api::resource resource, api::resource_usage usage_type, const api::resource_view_desc &desc, api::resource_view *out) final;

		bool create_pipeline(const api::pipeline_desc &desc, api::pipeline *out) final;
		bool create_pipeline_compute(const api::pipeline_desc &desc, api::pipeline *out);
		bool create_pipeline_graphics_all(const api::pipeline_desc &desc, api::pipeline *out);

		bool create_shader_module(api::shader_stage type, api::shader_format format, const char *entry_point, const void *data, size_t size, api::shader_module *out) final;
		bool create_pipeline_layout(uint32_t num_table_layouts, const api::descriptor_table_layout *table_layouts, uint32_t num_constant_ranges, const api::constant_range *constant_ranges, api::pipeline_layout *out) final;
		bool create_descriptor_heap(uint32_t max_tables, uint32_t num_sizes, const api::descriptor_heap_size *sizes, api::descriptor_heap *out) final;
		bool create_descriptor_table(api::descriptor_heap heap, api::descriptor_table_layout layout, api::descriptor_table *out) final;
		bool create_descriptor_table_layout(uint32_t num_ranges, const api::descriptor_range *ranges, bool push_descriptors, api::descriptor_table_layout *out) final;

		void destroy_sampler(api::sampler handle) final;
		void destroy_resource(api::resource handle) final;
		void destroy_resource_view(api::resource_view handle) final;

		void destroy_pipeline(api::pipeline_type type, api::pipeline handle) final;
		void destroy_shader_module(api::shader_module handle) final;
		void destroy_pipeline_layout(api::pipeline_layout handle) final;
		void destroy_descriptor_heap(api::descriptor_heap handle) final;
		void destroy_descriptor_table_layout(api::descriptor_table_layout handle) final;

		void update_descriptor_tables(uint32_t num_updates, const api::descriptor_update *updates) final;

		bool map_resource(api::resource resource, uint32_t subresource, api::map_access access, void **mapped_ptr) final;
		void unmap_resource(api::resource resource, uint32_t subresource) final;

		void upload_buffer_region(api::resource dst, uint64_t dst_offset, const void *data, uint64_t size) final;
		void upload_texture_region(api::resource dst, uint32_t dst_subresource, const int32_t dst_box[6], const void *data, uint32_t row_pitch, uint32_t depth_pitch) final;

		void get_resource_from_view(api::resource_view view, api::resource *out_resource) const final;
		api::resource_desc get_resource_desc(api::resource resource) const final;

		void wait_idle() const final;

		void set_debug_name(api::resource resource, const char *name) final;

#if RESHADE_ADDON
		uint32_t get_subresource_index(VkImage image, const VkImageSubresourceLayers &layers, uint32_t layer = 0) const
		{
			return layers.mipLevel + (layers.baseArrayLayer + layer) * _resources.at((uint64_t)image).image_create_info.mipLevels;
		}

		api::resource_view get_default_view(VkImage image)
		{
			VkImageViewCreateInfo create_info{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
			create_info.image = image;
			register_image_view((VkImageView)image, create_info); // Register fake image view for this image
			return { (uint64_t)image };
		}
#endif
		void register_image(VkImage image, const VkImageCreateInfo &create_info, VmaAllocation allocation = nullptr)
		{
			resource_data data;
			data.image = image;
			data.image_create_info = create_info;
			data.allocation = allocation;
			_resources.emplace((uint64_t)image, data);
		}
		void register_image_view(VkImageView image_view, const VkImageViewCreateInfo &create_info)
		{
			resource_view_data data;
			data.image_view = image_view;
			data.image_create_info = create_info;
			_views.emplace((uint64_t)image_view, data);
		}
		void register_buffer(VkBuffer buffer, const VkBufferCreateInfo &create_info, VmaAllocation allocation = nullptr)
		{
			resource_data data;
			data.buffer = buffer;
			data.buffer_create_info = create_info;
			data.allocation = allocation;
			_resources.emplace((uint64_t)buffer, data);
		}
		void register_buffer_view(VkBufferView buffer_view, const VkBufferViewCreateInfo &create_info)
		{
			resource_view_data data;
			data.buffer_view = buffer_view;
			data.buffer_create_info = create_info;
			_views.emplace((uint64_t)buffer_view, data);
		}

		void unregister_image(VkImage image) { _resources.erase((uint64_t)image); }
		void unregister_image_view(VkImageView image_view) { _views.erase((uint64_t)image_view); }
		void unregister_buffer(VkBuffer buffer) { _resources.erase((uint64_t)buffer); }
		void unregister_buffer_view(VkBufferView buffer_view) { _views.erase((uint64_t)buffer_view); }

		bool request_render_pass_and_framebuffer(uint32_t count, const reshade::api::resource_view *rtvs, reshade::api::resource_view dsv, VkRenderPass &out_pass, VkFramebuffer &out_fbo);

		const VkPhysicalDevice _physical_device;
		const VkLayerDispatchTable _dispatch_table;
		const VkLayerInstanceDispatchTable _instance_dispatch_table;

		uint32_t _graphics_queue_family_index = std::numeric_limits<uint32_t>::max();
		std::vector<command_queue_impl *> _queues;

		VmaAllocator _alloc = nullptr;
		lockfree_table<uint64_t, resource_data, 4096> _resources;
		lockfree_table<uint64_t, resource_view_data, 4096> _views;
#if RESHADE_ADDON
		lockfree_table<VkPipeline, pipeline_data, 4096> _pipeline_list;
		lockfree_table<VkRenderPass, render_pass_data, 4096> _render_pass_list;
		lockfree_table<VkFramebuffer, std::vector<api::resource_view>, 4096> _framebuffer_list;
#endif
		std::unordered_map<size_t, VkRenderPass> _render_pass_list_internal;
		std::unordered_map<size_t, VkFramebuffer> _framebuffer_list_internal;
	};

	class command_list_impl : public api::api_object_impl<VkCommandBuffer, api::command_list>
	{
	public:
		command_list_impl(device_impl *device, VkCommandBuffer cmd_list);
		~command_list_impl();

		api::device *get_device() final { return _device_impl; }

		void bind_index_buffer(api::resource buffer, uint64_t offset, uint32_t index_size) final;
		void bind_vertex_buffers(uint32_t first, uint32_t count, const api::resource *buffers, const uint64_t *offsets, const uint32_t *strides) final;

		void bind_pipeline(api::pipeline_type type, api::pipeline pipeline) final;
		void bind_pipeline_states(uint32_t count, const api::pipeline_state *states, const uint32_t *values) final;

		void push_constants(api::shader_stage stage, api::pipeline_layout layout, uint32_t layout_index, uint32_t first, uint32_t count, const uint32_t *values) final;
		void push_descriptors(api::shader_stage stage, api::pipeline_layout layout, uint32_t layout_index, api::descriptor_type type, uint32_t first, uint32_t count, const void *descriptors) final;
		void bind_descriptor_heaps(uint32_t count, const api::descriptor_heap *heaps) final;
		void bind_descriptor_tables(api::shader_stage stage, api::pipeline_layout layout, uint32_t first, uint32_t count, const api::descriptor_table *tables) final;

		void bind_viewports(uint32_t first, uint32_t count, const float *viewports) final;
		void bind_scissor_rects(uint32_t first, uint32_t count, const int32_t *rects) final;
		void bind_render_targets_and_depth_stencil(uint32_t count, const api::resource_view *rtvs, api::resource_view dsv) final;

		void draw(uint32_t vertices, uint32_t instances, uint32_t first_vertex, uint32_t first_instance) final;
		void draw_indexed(uint32_t indices, uint32_t instances, uint32_t first_index, int32_t vertex_offset, uint32_t first_instance) final;
		void dispatch(uint32_t num_groups_x, uint32_t num_groups_y, uint32_t num_groups_z) final;

		void blit(api::resource src, uint32_t src_subresource, const int32_t src_box[6], api::resource dst, uint32_t dst_subresource, const int32_t dst_box[6], api::texture_filter filter) final;
		void resolve(api::resource src, uint32_t src_subresource, const int32_t src_offset[3], api::resource dst, uint32_t dst_subresource, const int32_t dst_offset[3], const uint32_t size[3], uint32_t format) final;
		void copy_resource(api::resource src, api::resource dst) final;
		void copy_buffer_region(api::resource src, uint64_t src_offset, api::resource dst, uint64_t dst_offset, uint64_t size) final;
		void copy_buffer_to_texture(api::resource src, uint64_t src_offset, uint32_t row_length, uint32_t slice_height, api::resource dst, uint32_t dst_subresource, const int32_t dst_box[6]) final;
		void copy_texture_region(api::resource src, uint32_t src_subresource, const int32_t src_offset[3], api::resource dst, uint32_t dst_subresource, const int32_t dst_offset[3], const uint32_t size[3]) final;
		void copy_texture_to_buffer(api::resource src, uint32_t src_subresource, const int32_t src_box[6], api::resource dst, uint64_t dst_offset, uint32_t row_length, uint32_t slice_height) final;

		void clear_depth_stencil_view(api::resource_view dsv, uint32_t clear_flags, float depth, uint8_t stencil) final;
		void clear_render_target_views(uint32_t count, const api::resource_view *rtvs, const float color[4]) final;

		void transition_state(api::resource resource, api::resource_usage old_state, api::resource_usage new_state) final;

		// State tracking for render passes
		uint32_t current_subpass = std::numeric_limits<uint32_t>::max();
		VkViewport current_viewport = {};
		VkRenderPass current_renderpass = VK_NULL_HANDLE;
		VkFramebuffer current_framebuffer = VK_NULL_HANDLE;

	protected:
		device_impl *const _device_impl;
		bool _has_commands = false;
	};

	class command_list_immediate_impl : public command_list_impl
	{
		static const uint32_t NUM_COMMAND_FRAMES = 4; // Use power of two so that modulo can be replaced with bitwise operation

	public:
		command_list_immediate_impl(device_impl *device, uint32_t queue_family_index);
		~command_list_immediate_impl();

		bool flush(VkQueue queue, std::vector<VkSemaphore> &wait_semaphores);
		bool flush_and_wait(VkQueue queue);

		const VkCommandBuffer begin_commands() { _has_commands = true; return _orig; }

	private:
		uint32_t _cmd_index = 0;
		VkCommandPool _cmd_pool = VK_NULL_HANDLE;
		VkFence _cmd_fences[NUM_COMMAND_FRAMES] = {};
		VkSemaphore _cmd_semaphores[NUM_COMMAND_FRAMES] = {};
		VkCommandBuffer _cmd_buffers[NUM_COMMAND_FRAMES] = {};
	};

	class command_queue_impl : public api::api_object_impl<VkQueue, api::command_queue>
	{
	public:
		command_queue_impl(device_impl *device, uint32_t queue_family_index, const VkQueueFamilyProperties &queue_family, VkQueue queue);
		~command_queue_impl();

		api::device *get_device() final { return _device_impl; }

		api::command_list *get_immediate_command_list() final { return _immediate_cmd_list; }

		void flush_immediate_command_list() const final;

	private:
		device_impl *const _device_impl;
		command_list_immediate_impl *_immediate_cmd_list = nullptr;
	};
}
