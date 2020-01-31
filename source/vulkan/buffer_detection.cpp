/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "dll_log.hpp"
#include "buffer_detection.hpp"
#include <cmath>
#include <cassert>

void reshade::vulkan::buffer_detection::reset()
{
	_stats.vertices = 0;
	_stats.drawcalls = 0;
#if RESHADE_DEPTH
	_counters_per_used_depth_image.clear();
#endif
}

void reshade::vulkan::buffer_detection::merge(const buffer_detection &source)
{
	_stats.vertices += source._stats.vertices;
	_stats.drawcalls += source._stats.drawcalls;

#if RESHADE_DEPTH
	for (const auto &[depthstencil, snapshot] : source._counters_per_used_depth_image)
	{
		auto &target_snapshot = _counters_per_used_depth_image[depthstencil];
		target_snapshot.stats.vertices += snapshot.stats.vertices;
		target_snapshot.stats.drawcalls += snapshot.stats.drawcalls;

		target_snapshot.image = snapshot.image;
		target_snapshot.image_info = snapshot.image_info;
	}
#endif
}

void reshade::vulkan::buffer_detection::on_draw(uint32_t vertices)
{
	_stats.vertices += vertices;
	_stats.drawcalls += 1;

#if RESHADE_DEPTH
	if (_current_depthstencil == VK_NULL_HANDLE)
		// This is a draw call with no depth-stencil bound
		return;

	auto &counters = _counters_per_used_depth_image[_current_depthstencil];
	counters.stats.vertices += vertices;
	counters.stats.drawcalls += 1;
#endif
}

#if RESHADE_DEPTH
void reshade::vulkan::buffer_detection::on_set_depthstencil(VkImage depthstencil, VkImageLayout layout, const VkImageCreateInfo &create_info)
{
	_current_depthstencil = depthstencil;

	if (depthstencil == VK_NULL_HANDLE)
		return;

	assert(layout != VK_IMAGE_LAYOUT_UNDEFINED);
	assert((create_info.usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0);

	auto &counters = _counters_per_used_depth_image[depthstencil];

	if (VK_NULL_HANDLE == counters.image)
	{
		// This is a new entry in the map, so update data
		counters.image = depthstencil;
		counters.image_info = create_info;
		// Keep track of the layout this image likely ends up in
		counters.image_info.initialLayout = layout;
	}
}

reshade::vulkan::buffer_detection::depthstencil_info reshade::vulkan::buffer_detection_context::find_best_depth_texture(uint32_t width, uint32_t height, VkImage override)
{
	depthstencil_info best_snapshot;

	if (override != VK_NULL_HANDLE)
	{
		best_snapshot = _counters_per_used_depth_image[override];
	}
	else
	{
		for (auto &[depthstencil, snapshot] : _counters_per_used_depth_image)
		{
			if (snapshot.stats.drawcalls == 0 || snapshot.stats.vertices == 0)
				continue; // Skip unused

			assert(snapshot.image != VK_NULL_HANDLE);

			if (snapshot.image_info.samples != VK_SAMPLE_COUNT_1_BIT)
				continue; // Ignore MSAA textures, since they would need to be resolved first

			if (width != 0 && height != 0)
			{
				const float aspect_ratio = float(width) / float(height);
				const float texture_aspect_ratio = float(snapshot.image_info.extent.width) / float(snapshot.image_info.extent.height);

				const float width_factor = float(width) / float(snapshot.image_info.extent.width);
				const float height_factor = float(height) / float(snapshot.image_info.extent.height);

				if (std::fabs(texture_aspect_ratio - aspect_ratio) > 0.1f || width_factor > 1.85f || height_factor > 1.85f || width_factor < 0.5f || height_factor < 0.5f)
					continue; // Not a good fit
			}

			if (snapshot.stats.drawcalls >= best_snapshot.stats.drawcalls)
				best_snapshot = snapshot;
		}
	}

	return best_snapshot;
}
#endif
