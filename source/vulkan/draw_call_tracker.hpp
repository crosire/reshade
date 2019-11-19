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
		};
#if RESHADE_VULKAN_CAPTURE_DEPTH_BUFFERS
		struct depthstencil_info
		{
			draw_stats stats;
			VkImage image = VK_NULL_HANDLE;
			VkImageLayout image_layout = VK_IMAGE_LAYOUT_UNDEFINED;
			VkImageCreateInfo image_info = {};
		};

		static bool filter_aspect_ratio;
		static unsigned int filter_depth_texture_format;
#endif

		uint32_t total_vertices() const { return _stats.vertices; }
		uint32_t total_drawcalls() const { return _stats.drawcalls; }

#if RESHADE_VULKAN_CAPTURE_DEPTH_BUFFERS
		const auto &depth_buffer_counters() const { return _counters_per_used_depth_image; }
#endif

		void init(VkDevice device, VkPhysicalDevice physical_device, const VkLayerInstanceDispatchTable &instance_table, const VkLayerDispatchTable &dispatch_table, draw_call_tracker *tracker);
		void reset();

		void merge(const draw_call_tracker &source);

		void on_draw(uint32_t vertices);

#if RESHADE_VULKAN_CAPTURE_DEPTH_BUFFERS
		void track_depthstencil(VkImage depthstencil, VkImageLayout layout, const VkImageCreateInfo &create_info);

		static bool check_aspect_ratio(const VkImageCreateInfo &create_info, uint32_t width, uint32_t height);
		static bool check_texture_format(const VkImageCreateInfo &create_info);

		depthstencil_info find_best_depth_texture(uint32_t width, uint32_t height,
			VkImage override = VK_NULL_HANDLE);
#endif

	private:
		VkDevice _device = VK_NULL_HANDLE;
		draw_call_tracker *_device_tracker = nullptr;
		const VkLayerDispatchTable *_vk = nullptr;
		VkPhysicalDeviceMemoryProperties _memory_props = {};
		draw_stats _stats;
#if RESHADE_VULKAN_CAPTURE_DEPTH_BUFFERS
		VkImage _current_depthstencil = VK_NULL_HANDLE;
		// Use "std::map" instead of "std::unordered_map" so that the iteration order is guaranteed
		std::map<VkImage, depthstencil_info> _counters_per_used_depth_image;
#endif
	};
}
