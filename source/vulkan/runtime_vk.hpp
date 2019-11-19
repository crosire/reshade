/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include "runtime.hpp"
#include "vk_handle.hpp"
#include "draw_call_tracker.hpp"

namespace reshadefx { struct sampler_info; }

namespace reshade::vulkan
{
	class runtime_vk : public runtime
	{
		friend struct vulkan_tex_data;
		friend struct vulkan_technique_data;
		friend struct vulkan_pass_data;
		friend struct vulkan_effect_data;

	public:
		runtime_vk(VkDevice device, VkPhysicalDevice physical_device, const VkLayerInstanceDispatchTable &instance_table, const VkLayerDispatchTable &device_table);

		bool on_init(VkSwapchainKHR swapchain, const VkSwapchainCreateInfoKHR &desc, HWND hwnd);
		void on_reset();
		void on_present(VkQueue queue, uint32_t swapchain_image_index, draw_call_tracker &tracker);

		bool capture_screenshot(uint8_t *buffer) const override;

		auto create_image(uint32_t width, uint32_t height, uint32_t levels, VkFormat format, VkImageUsageFlags usage, VkMemoryPropertyFlags mem = 0, VkImageCreateFlags = 0) -> VkImage;
		auto create_image_view(VkImage image, VkFormat format, uint32_t levels, VkImageAspectFlags aspect) -> VkImageView;
		auto create_buffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags mem = 0) -> VkBuffer;

		const VkLayerDispatchTable vk;

	private:
		bool init_texture(texture &texture);
		void upload_texture(texture &texture, const uint8_t *pixels);

		void generate_mipmaps(const VkCommandBuffer cmd_list, texture &texture);

		void transition_layout(VkCommandBuffer cmd_list, VkImage image, VkImageLayout old_layout, VkImageLayout new_layout,
			VkImageSubresourceRange subresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS }) const;

		bool begin_command_buffer() const;
		void execute_command_buffer() const;
		void wait_for_command_buffers();

		bool compile_effect(effect_data &effect);
		void unload_effect(size_t id);
		void unload_effects();

		bool init_technique(technique &info, VkShaderModule module, const VkSpecializationInfo &spec_info);

		void render_technique(technique &technique) override;

#if RESHADE_GUI
		bool init_imgui_resources();
		void render_imgui_draw_data(ImDrawData *draw_data) override;
		void draw_debug_menu();
#endif

#if RESHADE_VULKAN_CAPTURE_DEPTH_BUFFERS
		void update_depthstencil_image(VkImage depthstencil, VkImageLayout layout, VkFormat image_format);

		VkImage _depth_image_override = VK_NULL_HANDLE;
#endif

		const VkDevice _device;
		const VkPhysicalDevice _physical_device;
		VkSwapchainKHR _swapchain;

		static const uint32_t NUM_IMGUI_BUFFERS = 5;
		static const uint32_t NUM_COMMAND_FRAMES = 5;

		VkQueue _main_queue = VK_NULL_HANDLE;
		uint32_t _queue_family_index = 0; // Default to first queue family index
		VkPhysicalDeviceMemoryProperties _memory_props = {};

		uint32_t _cmd_index = 0;
		uint32_t _swap_index = 0;

		VkCommandPool _cmd_pool = VK_NULL_HANDLE;
		VkFence _cmd_fences[NUM_COMMAND_FRAMES];
		mutable std::pair<VkCommandBuffer, bool> _cmd_buffers[NUM_COMMAND_FRAMES];

		std::vector<VkImage> _swapchain_images;
		std::vector<VkImageView> _swapchain_views;
		std::vector<VkFramebuffer> _swapchain_frames;
		VkRenderPass _default_render_pass[2] = {};
		VkExtent2D _render_area = {};
		VkFormat _backbuffer_format = VK_FORMAT_UNDEFINED;

		std::vector<VkDeviceMemory> _allocations;

		VkImage _backbuffer_texture = VK_NULL_HANDLE;
		VkImageView _backbuffer_texture_view[2] = {};
		VkImage _empty_depth_image = VK_NULL_HANDLE;
		VkImageView _empty_depth_image_view = VK_NULL_HANDLE;
		VkImage _default_depthstencil = VK_NULL_HANDLE;
		VkImageView _default_depthstencil_view = VK_NULL_HANDLE;

		VkImage _depth_image = VK_NULL_HANDLE;
		VkImageView _depth_image_view = VK_NULL_HANDLE;
		VkImageLayout _depth_image_layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		VkImageAspectFlags _depth_image_aspect = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;

		VkDescriptorPool _effect_descriptor_pool = VK_NULL_HANDLE;
		VkDescriptorSetLayout _effect_ubo_layout = VK_NULL_HANDLE;
		std::vector<struct vulkan_effect_data> _effect_data;
		std::unordered_map<size_t, VkSampler> _effect_sampler_states;

		draw_call_tracker *_current_tracker = nullptr;

#if RESHADE_GUI
		unsigned int _imgui_index_buffer_size = 0;
		VkBuffer _imgui_index_buffer = VK_NULL_HANDLE;
		VkDeviceMemory _imgui_index_mem = VK_NULL_HANDLE;
		unsigned int _imgui_vertex_buffer_size = 0;
		VkBuffer _imgui_vertex_buffer = VK_NULL_HANDLE;
		VkDeviceMemory _imgui_vertex_mem = VK_NULL_HANDLE;
		VkSampler _imgui_font_sampler = VK_NULL_HANDLE;
		VkPipeline _imgui_pipeline = VK_NULL_HANDLE;
		VkPipelineLayout _imgui_pipeline_layout = VK_NULL_HANDLE;
		VkDescriptorSetLayout _imgui_descriptor_set_layout = VK_NULL_HANDLE;
#endif
	};
}
