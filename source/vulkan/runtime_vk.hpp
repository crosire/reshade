/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include "runtime.hpp"
#include "vk_handle.hpp"
#include "buffer_detection.hpp"

namespace reshade::vulkan
{
	class runtime_vk : public runtime
	{
		friend struct vulkan_tex_data;
		friend struct vulkan_technique_data;
		friend struct vulkan_pass_data;
		friend struct vulkan_effect_data;

		static const uint32_t NUM_IMGUI_BUFFERS = 5;
		static const uint32_t NUM_COMMAND_FRAMES = 5;

	public:
		runtime_vk(VkDevice device, VkPhysicalDevice physical_device, const VkLayerInstanceDispatchTable &instance_table, const VkLayerDispatchTable &device_table);

		bool on_init(VkSwapchainKHR swapchain, const VkSwapchainCreateInfoKHR &desc, HWND hwnd);
		void on_reset();
		void on_present(VkQueue queue, uint32_t swapchain_image_index, buffer_detection_context &tracker);

		bool capture_screenshot(uint8_t *buffer) const override;

		const VkLayerDispatchTable vk;

	private:
		bool init_effect(size_t index) override;
		void unload_effect(size_t index) override;
		void unload_effects() override;

		bool init_texture(texture &texture) override;
		void upload_texture(texture &texture, const uint8_t *pixels) override;
		void generate_mipmaps(texture &texture);

		void render_technique(technique &technique) override;

		bool begin_command_buffer() const;
		void execute_command_buffer() const;
		void wait_for_command_buffers();

		VkImage create_image(uint32_t width, uint32_t height, uint32_t levels, VkFormat format, VkImageUsageFlags usage, VkMemoryPropertyFlags mem = 0, VkImageCreateFlags = 0);
		VkImageView create_image_view(VkImage image, VkFormat format, uint32_t levels, VkImageAspectFlags aspect);
		VkBuffer create_buffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags mem = 0);
		VkBufferView create_buffer_view(VkBuffer buffer, VkFormat format);

		const VkDevice _device;
		const VkPhysicalDevice _physical_device;
		VkSwapchainKHR _swapchain;
		VkQueue _main_queue = VK_NULL_HANDLE;
		uint32_t _queue_family_index = 0; // Default to first queue family index
		VkPhysicalDeviceMemoryProperties _memory_props = {};

		VkFence _cmd_fences[NUM_COMMAND_FRAMES];
		VkCommandPool _cmd_pool = VK_NULL_HANDLE;
		mutable std::pair<VkCommandBuffer, bool> _cmd_buffers[NUM_COMMAND_FRAMES];
		uint32_t _cmd_index = 0;
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

		std::vector<VkDeviceMemory> _allocations;

		VkImage _effect_depthstencil = VK_NULL_HANDLE;
		VkImageView _effect_depthstencil_view = VK_NULL_HANDLE;
		VkDescriptorPool _effect_descriptor_pool = VK_NULL_HANDLE;
		VkDescriptorSetLayout _effect_descriptor_layout = VK_NULL_HANDLE;
		std::vector<vulkan_effect_data> _effect_data;
		std::unordered_map<size_t, VkSampler> _effect_sampler_states;

#if RESHADE_GUI
		bool init_imgui_resources();
		void render_imgui_draw_data(ImDrawData *draw_data) override;

		VkDeviceSize _imgui_index_buffer_size = 0;
		VkBuffer _imgui_index_buffer = VK_NULL_HANDLE;
		VkDeviceMemory _imgui_index_mem = VK_NULL_HANDLE;
		VkDeviceSize _imgui_vertex_buffer_size = 0;
		VkBuffer _imgui_vertex_buffer = VK_NULL_HANDLE;
		VkDeviceMemory _imgui_vertex_mem = VK_NULL_HANDLE;
		VkSampler _imgui_font_sampler = VK_NULL_HANDLE;
		VkPipeline _imgui_pipeline = VK_NULL_HANDLE;
		VkPipelineLayout _imgui_pipeline_layout = VK_NULL_HANDLE;
		VkDescriptorSetLayout _imgui_descriptor_set_layout = VK_NULL_HANDLE;
#endif

#if RESHADE_VULKAN_CAPTURE_DEPTH_BUFFERS
		void draw_depth_debug_menu();
		void update_depthstencil_image(VkImage depthstencil, VkImageLayout layout, VkFormat image_format);

		bool _use_aspect_ratio_heuristics = true;
		VkImage _depth_image = VK_NULL_HANDLE;
		VkImage _depth_image_override = VK_NULL_HANDLE;
		VkImageView _depth_image_view = VK_NULL_HANDLE;
		VkImageLayout _depth_image_layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		VkImageAspectFlags _depth_image_aspect = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
		buffer_detection_context *_current_tracker = nullptr;
#endif
	};
}
