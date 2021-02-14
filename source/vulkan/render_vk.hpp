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

namespace reshade::vulkan
{
	void convert_resource_desc(const api::resource_desc &desc, VkBufferCreateInfo &create_info);
	api::resource_desc convert_resource_desc(const VkBufferCreateInfo &create_info);
	void convert_resource_desc(api::resource_type type, const api::resource_desc &desc, VkImageCreateInfo &create_info);
	std::pair<api::resource_type, api::resource_desc> convert_resource_desc(const VkImageCreateInfo &create_info);

	void convert_resource_view_desc(const api::resource_view_desc &desc, VkImageViewCreateInfo &create_info);
	api::resource_view_desc convert_resource_view_desc(const VkImageViewCreateInfo &create_info);
	void convert_resource_view_desc(const api::resource_view_desc &desc, VkBufferViewCreateInfo &create_info);
	api::resource_view_desc convert_resource_view_desc(const VkBufferViewCreateInfo &create_info);

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
		bool type; // true => image, false => buffer

		union
		{
			VkImage image;
			VkBuffer buffer;
		};
		union
		{
			VkImageCreateInfo image_create_info;
			VkBufferCreateInfo buffer_create_info;
		};

		VmaAllocation allocation;
	};

	struct resource_view_data
	{
		bool type; // true => image view, false => buffer view

		union
		{
			VkImageView image_view;
			VkBufferView buffer_view;
		};
		union
		{
			VkImageViewCreateInfo image_create_info;
			VkBufferViewCreateInfo buffer_create_info;
		};
	};

	class device_impl : public api::device, api::api_data
	{
		friend class runtime_vk;
		friend class command_list_impl;

	public:
		device_impl(VkDevice device, VkPhysicalDevice physical_device, const VkLayerInstanceDispatchTable &instance_table, const VkLayerDispatchTable &device_table);
		~device_impl();

		bool get_data(const uint8_t guid[16], uint32_t size, void *data) override { return api_data::get_data(guid, size, data); }
		void set_data(const uint8_t guid[16], uint32_t size, const void *data) override  { api_data::set_data(guid, size, data); }

		void *get_native_object() override { return _device; }

		api::render_api get_api() override { return api::render_api::vulkan; }

		bool check_format_support(uint32_t format, api::resource_usage usage) override;

		bool check_resource_handle_valid(api::resource_handle resource) override;
		bool check_resource_view_handle_valid(api::resource_view_handle view) override;

		bool create_resource(api::resource_type type, const api::resource_desc &desc, api::resource_usage initial_state, api::resource_handle *out_resource) override;
		bool create_resource_view(api::resource_handle resource, api::resource_view_type type, const api::resource_view_desc &desc, api::resource_view_handle *out_view) override;

		void destroy_resource(api::resource_handle resource) override;
		void destroy_resource_view(api::resource_view_handle view) override;

		void get_resource_from_view(api::resource_view_handle view, api::resource_handle *out_resource) override;

		api::resource_desc get_resource_desc(api::resource_handle resource) override;

		void wait_idle() override;

		inline operator VkDevice() const { return _device; }

#if RESHADE_ADDON
		api::resource_view_handle get_default_view(VkImage image)
		{
			VkImageViewCreateInfo create_info{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
			create_info.image = image;
			register_image_view((VkImageView)image, create_info); // Register fake image view for this image
			return { (uint64_t)image };
		}
#endif
		void register_image(VkImage image, const VkImageCreateInfo &create_info, VmaAllocation allocation = nullptr)
		{
			resource_data data { true };
			data.image = image;
			data.image_create_info = create_info;
			data.allocation = allocation;
			_resources.emplace((uint64_t)image, data);
		}
		void register_image_view(VkImageView image_view, const VkImageViewCreateInfo &create_info)
		{
			resource_view_data data{ true };
			data.image_view = image_view;
			data.image_create_info = create_info;
			_views.emplace((uint64_t)image_view, data);
		}
		void register_buffer(VkBuffer buffer, const VkBufferCreateInfo &create_info, VmaAllocation allocation = nullptr)
		{
			resource_data data { true };
			data.buffer = buffer;
			data.buffer_create_info = create_info;
			data.allocation = allocation;
			_resources.emplace((uint64_t)buffer, data);
		}
		void register_buffer_view(VkBufferView buffer_view, const VkBufferViewCreateInfo &create_info)
		{
			resource_view_data data{ false };
			data.buffer_view = buffer_view;
			data.buffer_create_info = create_info;
			_views.emplace((uint64_t)buffer_view, data);
		}

		void unregister_image(VkImage image) { _resources.erase((uint64_t)image); }
		void unregister_image_view(VkImageView image_view) { _views.erase((uint64_t)image_view); }
		void unregister_buffer(VkBuffer buffer) { _resources.erase((uint64_t)buffer); }
		void unregister_buffer_view(VkBufferView buffer_view) { _views.erase((uint64_t)buffer_view); }

		const VkLayerDispatchTable vk;

		uint32_t graphics_queue_family_index = std::numeric_limits<uint32_t>::max();
		std::vector<std::pair<VkQueue, api::command_queue *>> queues;
#if RESHADE_ADDON
		lockfree_table<VkRenderPass, render_pass_data, 4096> render_pass_list;
		lockfree_table<VkFramebuffer, std::vector<api::resource_view_handle>, 4096> framebuffer_list;
#endif

	private:
		const VkDevice _device;
		const VkPhysicalDevice _physical_device;
		VkLayerInstanceDispatchTable _instance_dispatch_table;
		lockfree_table<uint64_t, resource_data, 4096> _resources;
		lockfree_table<uint64_t, resource_view_data, 4096> _views;

		VmaAllocator _alloc = nullptr;
	};

	class command_list_impl : public api::command_list, api::api_data
	{
	public:
		command_list_impl(device_impl *device, VkCommandBuffer cmd_list);
		~command_list_impl();

		bool get_data(const uint8_t guid[16], uint32_t size, void *data) override { return api_data::get_data(guid, size, data); }
		void set_data(const uint8_t guid[16], uint32_t size, const void *data) override  { api_data::set_data(guid, size, data); }

		void *get_native_object() override { return _cmd_list; }

		api::device *get_device() override { return _device_impl; }

		void draw(uint32_t vertices, uint32_t instances, uint32_t first_vertex, uint32_t first_instance) override;
		void draw_indexed(uint32_t indices, uint32_t instances, uint32_t first_index, int32_t vertex_offset, uint32_t first_instance) override;

		void copy_resource(api::resource_handle source, api::resource_handle destination) override;

		void transition_state(api::resource_handle resource, api::resource_usage old_state, api::resource_usage new_state) override;

		void clear_depth_stencil_view(api::resource_view_handle dsv, uint32_t clear_flags, float depth, uint8_t stencil) override;
		void clear_render_target_view(api::resource_view_handle rtv, const float color[4]) override;

		// State tracking for render passes
		uint32_t current_subpass = std::numeric_limits<uint32_t>::max();
		VkViewport current_viewport = {};
		VkRenderPass current_renderpass = VK_NULL_HANDLE;
		VkFramebuffer current_framebuffer = VK_NULL_HANDLE;

	protected:
		device_impl *const _device_impl;
		VkCommandBuffer _cmd_list;
		bool _has_commands = false;
	};

	class command_list_immediate_impl : public command_list_impl
	{
	public:
		static const uint32_t NUM_COMMAND_FRAMES = 4; // Use power of two so that modulo can be replaced with bitwise operation

		command_list_immediate_impl(device_impl *device, uint32_t queue_family_index);
		~command_list_immediate_impl();

		inline uint32_t current_index() const { return _cmd_index; }

		bool flush(VkQueue queue, std::vector<VkSemaphore> &wait_semaphores);
		bool flush_and_wait(VkQueue queue);

		VkCommandBuffer get() { _has_commands = true; return _cmd_list; }

	private:
		uint32_t _cmd_index = 0;
		VkCommandPool _cmd_pool = VK_NULL_HANDLE;
		VkFence _cmd_fences[NUM_COMMAND_FRAMES] = {};
		VkSemaphore _cmd_semaphores[NUM_COMMAND_FRAMES] = {};
		VkCommandBuffer _cmd_buffers[NUM_COMMAND_FRAMES] = {};
	};

	class command_queue_impl : public api::command_queue, api::api_data
	{
		friend class runtime_vk;

	public:
		command_queue_impl(device_impl *device, uint32_t queue_family_index, const VkQueueFamilyProperties &queue_family, VkQueue queue);
		~command_queue_impl();

		bool get_data(const uint8_t guid[16], uint32_t size, void *data) override { return api_data::get_data(guid, size, data); }
		void set_data(const uint8_t guid[16], uint32_t size, const void *data) override  { api_data::set_data(guid, size, data); }

		void *get_native_object() override { return _queue; }

		api::device *get_device() override { return _device_impl; }
		api::command_list *get_immediate_command_list() override { return _immediate_cmd_list; }

	private:
		device_impl *const _device_impl;
		const VkQueue _queue;
		command_list_immediate_impl *_immediate_cmd_list = nullptr;
	};
}
