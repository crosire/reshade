/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include <map>
#include <vector>
#include <vulkan.h>
#include "vk_layer_dispatch_table.h"

#define RESHADE_VULKAN_CAPTURE_DEPTH_BUFFERS 1

namespace reshade::vulkan
{
	class draw_call_tracker
	{
	public:
		struct draw_stats
		{
			uint32_t vertices = 0;
			uint32_t drawcalls = 0;
			uint32_t mapped = 0;
			uint32_t vs_uses = 0;
			uint32_t ps_uses = 0;
		};
#if RESHADE_VULKAN_CAPTURE_DEPTH_BUFFERS
		struct depthstencil_info
		{
			draw_stats stats;
			VkImage image = VK_NULL_HANDLE;
			VkImageLayout image_layout = VK_IMAGE_LAYOUT_UNDEFINED;
			VkImageCreateInfo image_info = {};
			std::vector<draw_stats> clears;
		};

		static bool filter_aspect_ratio;
		static bool preserve_depth_buffers;
		static unsigned int filter_depth_texture_format;
#endif

		uint32_t total_vertices() const { return _stats.vertices; }
		uint32_t total_drawcalls() const { return _stats.drawcalls; }

#if RESHADE_VULKAN_CAPTURE_DEPTH_BUFFERS
		VkImage current_depth_image() const { return _depthstencil_clear_index.first; }
		uint32_t current_clear_index() const { return _depthstencil_clear_index.second; }
		const auto &depth_buffer_counters() const { return _counters_per_used_depth_image; }
#endif

		void init(VkDevice device, VkPhysicalDevice physical_device, const VkLayerInstanceDispatchTable &instance_table, const VkLayerDispatchTable &dispatch_table, draw_call_tracker *tracker);
		void reset(bool release_resources);

		void merge(const draw_call_tracker &source);

		void on_draw(uint32_t vertices);

#if RESHADE_VULKAN_CAPTURE_DEPTH_BUFFERS
		void track_render_pass(VkImage depthstencil, VkImageLayout layout, const VkImageCreateInfo &create_info);
		void track_next_depthstencil(VkImage depthstencil);
		void track_cleared_depthstencil(VkCommandBuffer cmd_list, VkImageAspectFlags clear_flags, VkImage depthstencil, VkImageLayout layout);

		bool update_depthstencil_clear_image(VkImageCreateInfo create_info);

		static bool check_aspect_ratio(const VkImageCreateInfo &create_info, uint32_t width, uint32_t height);
		static bool check_texture_format(const VkImageCreateInfo &create_info);

		depthstencil_info find_best_depth_texture(uint32_t width, uint32_t height,
			VkImage override = VK_NULL_HANDLE, uint32_t clear_index_override = 0);
#endif

	private:
		VkDevice _device = VK_NULL_HANDLE;
		draw_call_tracker *_device_tracker = nullptr;
		const VkLayerDispatchTable *_vk = nullptr;
		VkPhysicalDeviceMemoryProperties _memory_props = {};
		draw_stats _stats;
#if RESHADE_VULKAN_CAPTURE_DEPTH_BUFFERS
		draw_stats _clear_stats;
		VkImage _current_depthstencil = VK_NULL_HANDLE;
		VkImage _depthstencil_clear_image = VK_NULL_HANDLE;
		VkDeviceMemory _depthstencil_clear_image_mem = VK_NULL_HANDLE;
		VkFormat _depthstencil_clear_image_format = VK_FORMAT_UNDEFINED;
		VkExtent2D _depthstencil_clear_image_extent = { 0, 0 };
		std::pair<VkImage, uint32_t> _depthstencil_clear_index = { VK_NULL_HANDLE, std::numeric_limits<uint32_t>::max() };
		// Use "std::map" instead of "std::unordered_map" so that the iteration order is guaranteed
		std::map<VkImage, depthstencil_info> _counters_per_used_depth_image;
#endif
	};
}
