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
	void convert_resource_desc(api::resource_type type, const api::resource_desc &desc, VkImageCreateInfo &create_info);
	std::pair<api::resource_type, api::resource_desc> convert_resource_desc(const VkImageCreateInfo &create_info);

	void convert_resource_view_desc(const api::resource_view_desc &desc, VkImageViewCreateInfo &create_info);
	api::resource_view_desc convert_resource_view_desc(const VkImageViewCreateInfo &create_info);

	api::resource_usage convert_image_layout(VkImageLayout layout);

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
		};

		std::vector<subpass> subpasses;
		std::vector<cleared_attachment> cleared_attachments;
	};

	struct resource_data
	{
		VkImage image;
		VkImageCreateInfo create_info;
		VmaAllocation allocation;
	};

	struct resource_view_data
	{
		VkImageView view;
		VkImageViewCreateInfo create_info;
	};

	class device_impl : public api::device, api::api_data
	{
		friend class runtime_vk;
		friend class command_list_impl;

	public:
		device_impl(VkDevice device, VkPhysicalDevice physical_device, uint32_t graphics_queue_family_index, const VkLayerInstanceDispatchTable &instance_table, const VkLayerDispatchTable &device_table);
		~device_impl();

		bool get_data(const uint8_t guid[16], uint32_t size, void *data) override { return api_data::get_data(guid, size, data); }
		void set_data(const uint8_t guid[16], uint32_t size, const void *data) override  { api_data::set_data(guid, size, data); }

		api::render_api get_api() override { return api::render_api::vulkan; }

		bool check_format_support(uint32_t format, api::resource_usage usage) override;

		bool is_resource_valid(api::resource_handle resource) override;
		bool is_resource_view_valid(api::resource_view_handle view) override;

		bool create_resource(api::resource_type type, const api::resource_desc &desc, api::resource_usage initial_state, api::resource_handle *out_resource) override;
		bool create_resource_view(api::resource_handle resource, api::resource_view_type type, const api::resource_view_desc &desc, api::resource_view_handle *out_view) override;

		void destroy_resource(api::resource_handle resource) override;
		void destroy_resource_view(api::resource_view_handle view) override;

		void get_resource_from_view(api::resource_view_handle view, api::resource_handle *out_resource) override;

		api::resource_desc get_resource_desc(api::resource_handle resource) override;

		void wait_idle() override;

		void register_resource(VkImage image, const VkImageCreateInfo &create_info) { _resources.emplace(image, resource_data { image, create_info }); }
		void register_resource(VkImage image, const VkImageCreateInfo &create_info, VmaAllocation allocation) { _resources.emplace(image, resource_data { image, create_info, allocation }); }
		void register_resource_view(VkImageView image_view, const VkImageViewCreateInfo &create_info) { _views.emplace(image_view, resource_view_data { image_view, create_info }); }
		void unregister_resource(VkImage image) { _resources.erase(image); }
		void unregister_resource_view(VkImageView image_view) { _views.erase(image_view); }
		api::resource_view_handle get_default_view(VkImage image);

		const VkLayerDispatchTable vk;

		uint32_t graphics_queue_family_index = std::numeric_limits<uint32_t>::max();
#if RESHADE_ADDON
		std::vector<VkQueue> queues;
		lockfree_table<VkRenderPass, render_pass_data, 4096> render_pass_list;
		lockfree_table<VkFramebuffer, std::vector<api::resource_view_handle>, 4096> framebuffer_list;
#endif

	private:
		const VkDevice _device;
		const VkPhysicalDevice _physical_device;
		VkLayerInstanceDispatchTable _instance_dispatch_table;
		lockfree_table<VkImage, resource_data, 4096> _resources;
		lockfree_table<VkImageView, resource_view_data, 4096> _views;

		VmaAllocator _alloc = nullptr;
		VkCommandPool _cmd_pool = VK_NULL_HANDLE;
	};

	class command_list_impl : public api::command_list, api::api_data
	{
	public:
		command_list_impl(device_impl *device, VkCommandBuffer cmd_list);
		~command_list_impl();

		bool get_data(const uint8_t guid[16], uint32_t size, void *data) override { return api_data::get_data(guid, size, data); }
		void set_data(const uint8_t guid[16], uint32_t size, const void *data) override  { api_data::set_data(guid, size, data); }

		api::device *get_device() override { return _device_impl; }

		void transition_state(api::resource_handle resource, api::resource_usage old_state, api::resource_usage new_state) override;

		void clear_depth_stencil_view(api::resource_view_handle dsv, uint32_t clear_flags, float depth, uint8_t stencil) override;
		void clear_render_target_view(api::resource_view_handle rtv, const float color[4]) override;

		void copy_resource(api::resource_handle source, api::resource_handle dest) override;

		// State tracking for render passes
		uint32_t current_subpass = std::numeric_limits<uint32_t>::max();
		VkViewport current_viewport = {};
		VkRenderPass current_renderpass = VK_NULL_HANDLE;
		VkFramebuffer current_framebuffer = VK_NULL_HANDLE;

	private:
		device_impl *const _device_impl;
		const VkCommandBuffer _cmd_list;
	};

	class command_queue_impl : public api::command_queue, api::api_data
	{
	public:
		command_queue_impl(device_impl *device, VkQueue queue);
		~command_queue_impl();

		bool get_data(const uint8_t guid[16], uint32_t size, void *data) override { return api_data::get_data(guid, size, data); }
		void set_data(const uint8_t guid[16], uint32_t size, const void *data) override  { api_data::set_data(guid, size, data); }

		api::device *get_device() override { return _device_impl; }

		api::command_list *get_immediate_command_list() override { return nullptr; }

	private:
		device_impl *const _device_impl;
		const VkQueue _queue;
	};
}
