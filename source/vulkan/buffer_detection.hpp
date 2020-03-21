/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include <map>
#include <vulkan.h>

namespace reshade::vulkan
{
	class buffer_detection
	{
	public:
		struct draw_stats
		{
			uint32_t vertices = 0;
			uint32_t drawcalls = 0;
		};
		struct depthstencil_info
		{
			draw_stats stats;
			VkImage image = VK_NULL_HANDLE;
			VkImageCreateInfo image_info = {};
		};

		void reset();

		void merge(const buffer_detection &source);

		void on_draw(uint32_t vertices);

#if RESHADE_DEPTH
		void on_set_depthstencil(VkImage depthstencil, VkImageLayout layout, const VkImageCreateInfo &create_info);
#endif

	protected:
#if RESHADE_DEPTH
		VkImage _current_depthstencil = VK_NULL_HANDLE;
		// Use "std::map" instead of "std::unordered_map" so that the iteration order is guaranteed
		std::map<VkImage, depthstencil_info> _counters_per_used_depth_image;
#endif
		draw_stats _stats;
	};

	class buffer_detection_context : public buffer_detection
	{
	public:
		uint32_t total_vertices() const { return _stats.vertices; }
		uint32_t total_drawcalls() const { return _stats.drawcalls; }

#if RESHADE_DEPTH
		const auto &depth_buffer_counters() const { return _counters_per_used_depth_image; }

		depthstencil_info find_best_depth_texture(VkExtent2D dimensions = {}, VkImage override = VK_NULL_HANDLE) const;
#endif
	};
}
