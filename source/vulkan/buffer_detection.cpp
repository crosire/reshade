/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "log.hpp"
#include "buffer_detection.hpp"
#include "format_utils.hpp"
#include <math.h>
#include <assert.h>

namespace reshade::vulkan
{
	extern uint32_t find_memory_type_index(const VkPhysicalDeviceMemoryProperties &props, VkMemoryPropertyFlags flags, uint32_t type_bits);

	void buffer_detection::init(VkDevice device, VkPhysicalDevice physical_device, const VkLayerInstanceDispatchTable &instance_table, const VkLayerDispatchTable &device_table, buffer_detection *tracker)
	{
		if (physical_device != VK_NULL_HANDLE)
			instance_table.GetPhysicalDeviceMemoryProperties(physical_device, &_memory_props);

		_vk = &device_table; // This works because 'lockfree_table' holds a stable pointer to its objects
		_device = device;
		_device_tracker = tracker;
	}
	void buffer_detection::reset()
	{
		_stats.vertices = 0;
		_stats.drawcalls = 0;
#if RESHADE_VULKAN_CAPTURE_DEPTH_BUFFERS
		_counters_per_used_depth_image.clear();
#endif
	}

	void buffer_detection::merge(const buffer_detection &source)
	{
		_stats.vertices += source.total_vertices();
		_stats.drawcalls += source.total_drawcalls();

#if RESHADE_VULKAN_CAPTURE_DEPTH_BUFFERS
		for (const auto &[depthstencil, snapshot] : source._counters_per_used_depth_image)
		{
			auto &target_snapshot = _counters_per_used_depth_image[depthstencil];
			target_snapshot.stats.vertices += snapshot.stats.vertices;
			target_snapshot.stats.drawcalls += snapshot.stats.drawcalls;

			target_snapshot.image = snapshot.image;
			target_snapshot.image_info = snapshot.image_info;
			target_snapshot.image_layout = snapshot.image_layout;
		}
#endif
	}

	void buffer_detection::on_draw(uint32_t vertices)
	{
		_stats.vertices += vertices;
		_stats.drawcalls += 1;

#if RESHADE_VULKAN_CAPTURE_DEPTH_BUFFERS
		if (_current_depthstencil == VK_NULL_HANDLE)
			// This is a draw call with no depth stencil
			return;

		if (const auto intermediate_snapshot = _counters_per_used_depth_image.find(_current_depthstencil);
			intermediate_snapshot != _counters_per_used_depth_image.end())
		{
			intermediate_snapshot->second.stats.vertices += vertices;
			intermediate_snapshot->second.stats.drawcalls += 1;
		}
#endif
	}

#if RESHADE_VULKAN_CAPTURE_DEPTH_BUFFERS
	bool buffer_detection::filter_aspect_ratio = true;
	unsigned int buffer_detection::filter_depth_texture_format = 0;

	void buffer_detection::track_depthstencil(VkImage depthstencil, VkImageLayout layout, const VkImageCreateInfo &create_info)
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
			counters.image_layout = layout;
		}
	}

	bool buffer_detection::check_aspect_ratio(const VkImageCreateInfo &create_info, uint32_t width, uint32_t height)
	{
		if (!filter_aspect_ratio)
			return true;

		const float aspect_ratio = float(width) / float(height);
		const float texture_aspect_ratio = float(create_info.extent.width) / float(create_info.extent.height);

		const float width_factor = float(width) / float(create_info.extent.width);
		const float height_factor = float(height) / float(create_info.extent.height);

		return !(fabs(texture_aspect_ratio - aspect_ratio) > 0.1f || width_factor > 1.85f || height_factor > 1.85f || width_factor < 0.5f || height_factor < 0.5f);
	}
	bool buffer_detection::check_texture_format(const VkImageCreateInfo &create_info)
	{
		const VkFormat depth_texture_formats[] = {
			VK_FORMAT_UNDEFINED,
			VK_FORMAT_D16_UNORM,
			VK_FORMAT_D16_UNORM_S8_UINT,
			VK_FORMAT_D24_UNORM_S8_UINT,
			VK_FORMAT_D32_SFLOAT,
			VK_FORMAT_D32_SFLOAT_S8_UINT
		};

		if (filter_depth_texture_format == 0 ||
			filter_depth_texture_format >= _countof(depth_texture_formats))
			return true; // All formats are allowed

		return create_info.format == depth_texture_formats[filter_depth_texture_format];
	}

	buffer_detection::depthstencil_info buffer_detection::find_best_depth_texture(uint32_t width, uint32_t height, VkImage override)
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

				if (snapshot.image_info.samples != VK_SAMPLE_COUNT_1_BIT)
					continue; // Ignore MSAA textures, since they would need to be resolved first
				if (!check_aspect_ratio(snapshot.image_info, width, height))
					continue; // Not a good fit
				if (!check_texture_format(snapshot.image_info))
					continue;

				if (snapshot.stats.drawcalls >= best_snapshot.stats.drawcalls)
					best_snapshot = snapshot;
			}
		}

		return best_snapshot;
	}
#endif
}
