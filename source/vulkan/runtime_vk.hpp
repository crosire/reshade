/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include "runtime.hpp"
#include "render_vk.hpp"

namespace reshade::vulkan
{
	class runtime_impl : public api::api_object_impl<VkSwapchainKHR, runtime>
	{
		static const uint32_t NUM_QUERY_FRAMES = 4;
		static const uint32_t MAX_IMAGE_DESCRIPTOR_SETS = 128; // TODO: Check if these limits are enough
		static const uint32_t MAX_EFFECT_DESCRIPTOR_SETS = 50 * 2 * 4; // 50 resources, 4 passes

	public:
		runtime_impl(device_impl *device, command_queue_impl *graphics_queue);
		~runtime_impl();

		api::device *get_device() override { return _device_impl; }
		api::command_queue *get_command_queue() override { return _queue_impl; }

		bool on_init(VkSwapchainKHR swapchain, const VkSwapchainCreateInfoKHR &desc, HWND hwnd);
		void on_reset();
		void on_present(VkQueue queue, uint32_t swapchain_image_index, std::vector<VkSemaphore> &wait);

		bool capture_screenshot(uint8_t *buffer) const override;

		void update_texture_bindings(const char *semantic, api::resource_view_handle srv) override;

	private:
		bool init_effect(size_t index) override;
		void unload_effect(size_t index) override;
		void unload_effects() override;

		bool init_texture(texture &texture) override;
		void upload_texture(const texture &texture, const uint8_t *pixels) override;
		void destroy_texture(texture &texture) override;
		void generate_mipmaps(const struct tex_data *impl);

		void render_technique(technique &technique) override;

		void wait_for_command_buffers();

		void set_debug_name(uint64_t object, VkDebugReportObjectTypeEXT type, const char *name) const;
		inline void set_debug_name_image(VkImage image, const char *name) const { set_debug_name((uint64_t)image, VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT, name); }
		inline void set_debug_name_buffer(VkBuffer buffer, const char *name) const { set_debug_name((uint64_t)buffer, VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT, name); }

		VkImage create_image(uint32_t width, uint32_t height, uint32_t levels, VkFormat format,
			VkImageUsageFlags usage, VmaMemoryUsage mem_usage,
			VkImageCreateFlags flags = 0, VmaAllocationCreateFlags mem_flags = 0, VmaAllocation *out_mem = nullptr);
		VkBuffer create_buffer(VkDeviceSize size,
			VkBufferUsageFlags usage, VmaMemoryUsage mem_usage,
			VkBufferCreateFlags flags = 0, VmaAllocationCreateFlags mem_flags = 0, VmaAllocation *out_mem = nullptr);
		VkImageView create_image_view(VkImage image, VkFormat format, uint32_t levels, VkImageAspectFlags aspect);

		device_impl *const _device_impl;
		const VkDevice _device;
		command_queue_impl *const _queue_impl;
		VkQueue _queue = VK_NULL_HANDLE;
		command_list_immediate_impl *const _cmd_impl;

		uint32_t _queue_sync_index = 0;
		VkSemaphore _queue_sync_semaphores[NUM_QUERY_FRAMES] = {};
#ifndef NDEBUG
		mutable bool _wait_for_idle_happened = false;
#endif

		uint32_t _swap_index = 0;
		VkFormat _backbuffer_format = VK_FORMAT_UNDEFINED;
		VkExtent2D _render_area = {};
		VkRenderPass _default_render_pass[2] = {};
		std::vector<VkImage> _swapchain_images;
		std::vector<VkImageView> _swapchain_views;
		std::vector<VkFramebuffer> _swapchain_frames;
		VkImage _backbuffer_image = VK_NULL_HANDLE;
		VkImageView _backbuffer_image_view[2] = {};

		VkImage _empty_depth_image = VK_NULL_HANDLE;
		VkImageView _empty_depth_image_view = VK_NULL_HANDLE;

		std::vector<VmaAllocation> _allocations;

		VkImage _effect_stencil = VK_NULL_HANDLE;
		VkFormat _effect_stencil_format = VK_FORMAT_UNDEFINED;
		VkImageView _effect_stencil_view = VK_NULL_HANDLE;
		std::vector<struct effect_data> _effect_data;
		VkDescriptorPool _effect_descriptor_pool = VK_NULL_HANDLE;
		VkDescriptorSetLayout _effect_descriptor_layout = VK_NULL_HANDLE;
		std::unordered_map<size_t, VkSampler> _effect_sampler_states;

		std::unordered_map<std::string, VkImageView> _texture_semantic_bindings;

#if RESHADE_GUI
		static const uint32_t NUM_IMGUI_BUFFERS = 4;

		bool init_imgui_resources();
		void render_imgui_draw_data(ImDrawData *draw_data) override;

		struct imgui_resources
		{
			VkSampler sampler = VK_NULL_HANDLE;
			VkPipeline pipeline = VK_NULL_HANDLE;
			VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
			VkDescriptorPool descriptor_pool = VK_NULL_HANDLE;
			VkDescriptorSetLayout descriptor_layout = VK_NULL_HANDLE;

			VkBuffer indices[NUM_IMGUI_BUFFERS] = {};
			VkBuffer vertices[NUM_IMGUI_BUFFERS] = {};
			VmaAllocation indices_mem[NUM_IMGUI_BUFFERS] = {};
			VmaAllocation vertices_mem[NUM_IMGUI_BUFFERS] = {};
			int num_indices[NUM_IMGUI_BUFFERS] = {};
			int num_vertices[NUM_IMGUI_BUFFERS] = {};
		} _imgui;
#endif
	};
}
