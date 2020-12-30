/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include "runtime.hpp"
#include "state_tracking.hpp"

#pragma warning(push)
#pragma warning(disable: 4100 4127 4324 4703) // Disable a bunch of warnings thrown by VMA code
#include <vk_mem_alloc.h>
#pragma warning(pop)
#include <vk_layer_dispatch_table.h>

namespace reshade::vulkan
{
	class runtime_vk : public runtime
	{
		static const uint32_t NUM_IMGUI_BUFFERS = 4;
		static const uint32_t NUM_COMMAND_FRAMES = 4; // Use power of two so that modulo can be replaced with bitwise operation

	public:
		runtime_vk(VkDevice device, VkPhysicalDevice physical_device, uint32_t queue_family_index, const VkLayerInstanceDispatchTable &instance_table, const VkLayerDispatchTable &device_table, state_tracking_context *state_tracking);
		~runtime_vk();

		bool on_init(VkSwapchainKHR swapchain, const VkSwapchainCreateInfoKHR &desc, HWND hwnd);
		void on_reset();
		void on_present(VkQueue queue, uint32_t swapchain_image_index, std::vector<VkSemaphore> &wait);

		bool capture_screenshot(uint8_t *buffer) const override;

		const VkLayerDispatchTable vk;

	private:
		bool init_effect(size_t index) override;
		void unload_effect(size_t index) override;
		void unload_effects() override;

		bool init_texture(texture &texture) override;
		void upload_texture(const texture &texture, const uint8_t *pixels) override;
		void destroy_texture(texture &texture) override;
		void generate_mipmaps(const struct tex_data *impl);

		void render_technique(technique &technique) override;

		bool begin_command_buffer() const;
		void execute_command_buffer() const;
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

		VmaAllocator _alloc = VK_NULL_HANDLE;
		const VkDevice _device;
		VkQueue _queue = VK_NULL_HANDLE;
		const uint32_t _queue_family_index;
		VkPhysicalDeviceProperties _device_props = {};
		VkPhysicalDeviceMemoryProperties _memory_props = {};
		state_tracking_context &_state_tracking;

		VkFence _cmd_fences[NUM_COMMAND_FRAMES + 1] = {};
		VkSemaphore _cmd_semaphores[NUM_COMMAND_FRAMES * 2] = {};
		VkCommandPool _cmd_pool = VK_NULL_HANDLE;
		mutable std::pair<VkCommandBuffer, bool> _cmd_buffers[NUM_COMMAND_FRAMES] = {};
		uint32_t _cmd_index = 0;
		uint32_t _swap_index = 0;
#ifndef NDEBUG
		mutable bool _wait_for_idle_happened = false;
#endif

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

#if RESHADE_GUI
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

#if RESHADE_DEPTH
		void draw_depth_debug_menu();
		void update_depth_image_bindings(state_tracking::depthstencil_info info);

		VkImage _depth_image = VK_NULL_HANDLE;
		VkImage _depth_image_override = VK_NULL_HANDLE;
		VkImageView _depth_image_view = VK_NULL_HANDLE;
		VkImageLayout _depth_image_layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		VkImageAspectFlags _depth_image_aspect = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
#endif
	};
}
