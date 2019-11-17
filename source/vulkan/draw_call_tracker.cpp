/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "draw_call_tracker.hpp"
#include "format_utils.hpp"
#include "log.hpp"
#include <math.h>
#include <assert.h>

namespace reshade::vulkan
{
	extern uint32_t find_memory_type_index(const VkPhysicalDeviceMemoryProperties &props, VkMemoryPropertyFlags flags, uint32_t type_bits);

	void draw_call_tracker::init(VkDevice device, VkPhysicalDevice physical_device, const VkLayerInstanceDispatchTable &instance_table, const VkLayerDispatchTable &device_table, draw_call_tracker *tracker)
	{
		if (physical_device != VK_NULL_HANDLE)
			instance_table.GetPhysicalDeviceMemoryProperties(physical_device, &_memory_props);

		_vk = &device_table; // This works because 'lockfree_table' holds a stable pointer to its objects
		_device = device;
		_device_tracker = tracker;
	}
	void draw_call_tracker::reset(bool release_resources)
	{
		_stats.vertices = 0;
		_stats.drawcalls = 0;
#if RESHADE_VULKAN_CAPTURE_DEPTH_BUFFERS
		_clear_stats.vertices = 0;
		_clear_stats.drawcalls = 0;
		_counters_per_used_depth_image.clear();

		if (release_resources)
		{
			assert(_vk != nullptr && this == _device_tracker);

			_vk->FreeMemory(_device, _depthstencil_clear_image_mem, nullptr);
			_vk->DestroyImage(_device, _depthstencil_clear_image, nullptr);
			_depthstencil_clear_image = VK_NULL_HANDLE;
			_depthstencil_clear_image_mem = VK_NULL_HANDLE;
			_depthstencil_clear_image_format = VK_FORMAT_UNDEFINED;
			_depthstencil_clear_image_extent = { 0, 0 };
		}
#endif
	}

	void draw_call_tracker::merge(const draw_call_tracker &source)
	{
		_stats.vertices += source.total_vertices();
		_stats.drawcalls += source.total_drawcalls();


#if RESHADE_VULKAN_CAPTURE_DEPTH_BUFFERS
		_clear_stats.vertices += source._clear_stats.vertices;
		_clear_stats.drawcalls += source._clear_stats.drawcalls;

		for (const auto &[depthstencil, snapshot] : source._counters_per_used_depth_image)
		{
			auto &target_snapshot = _counters_per_used_depth_image[depthstencil];
			target_snapshot.stats.vertices += snapshot.stats.vertices;
			target_snapshot.stats.drawcalls += snapshot.stats.drawcalls;

			target_snapshot.image = snapshot.image;
			target_snapshot.image_info = snapshot.image_info;
			target_snapshot.image_layout = snapshot.image_layout;

			target_snapshot.clears.insert(target_snapshot.clears.end(), snapshot.clears.begin(), snapshot.clears.end());
		}
#endif
	}

	void draw_call_tracker::on_draw(uint32_t vertices)
	{
		_stats.vertices += vertices;
		_stats.drawcalls += 1;

#if RESHADE_VULKAN_CAPTURE_DEPTH_BUFFERS
		_clear_stats.vertices += vertices;
		_clear_stats.drawcalls += 1;

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
	bool draw_call_tracker::filter_aspect_ratio = true;
	bool draw_call_tracker::preserve_depth_buffers = false;
	unsigned int draw_call_tracker::filter_depth_texture_format = 0;

	void draw_call_tracker::track_render_pass(VkImage depthstencil, VkImageLayout layout, const VkImageCreateInfo &create_info)
	{
		track_next_depthstencil(depthstencil);

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
	void draw_call_tracker::track_next_depthstencil(VkImage depthstencil)
	{
		_current_depthstencil = depthstencil;
	}
	void draw_call_tracker::track_cleared_depthstencil(VkCommandBuffer cmd_list, VkImageAspectFlags clear_flags, VkImage depthstencil, VkImageLayout layout)
	{
		// Reset draw call stats for clears
		auto current_stats = _clear_stats;
		_clear_stats.vertices = 0;
		_clear_stats.drawcalls = 0;

		if (!(preserve_depth_buffers && (clear_flags & VK_IMAGE_ASPECT_DEPTH_BIT)))
			return;

		assert(_device_tracker != nullptr);
		assert(depthstencil != VK_NULL_HANDLE && layout != VK_IMAGE_LAYOUT_UNDEFINED);

		if (depthstencil != _device_tracker->_depthstencil_clear_index.first)
			return;

		auto &clears = _counters_per_used_depth_image[depthstencil].clears;
		clears.push_back(current_stats);

		// Make a backup copy of the depth texture before it is cleared
		if (clears.size() == _device_tracker->_depthstencil_clear_index.second)
		{
			assert(cmd_list != VK_NULL_HANDLE);
			assert(_device_tracker->_depthstencil_clear_image != VK_NULL_HANDLE);

			const VkImageAspectFlags aspect_mask =
				aspect_flags_from_format(_depthstencil_clear_image_format);

			VkImageMemoryBarrier transition[2];
			transition[0] = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
			transition[0].image = _device_tracker->_depthstencil_clear_image;
			transition[0].subresourceRange = { aspect_mask, 0, 1, 0, 1 };
			transition[0].dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			transition[0].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			transition[0].newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			transition[1] = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
			transition[1].image = depthstencil;
			transition[1].subresourceRange = { aspect_mask, 0, 1, 0, 1 };
			transition[1].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			transition[1].dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
			transition[1].oldLayout = layout;
			transition[1].newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

			_vk->CmdPipelineBarrier(cmd_list, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 2, transition);

			// TODO: It is invalid for the below to be called in a render pass (which is the case when called from vkCmdClearAttachments). Need to find a solution.
			const VkImageCopy copy_range = {
				{ aspect_mask, 0, 0, 1 }, { 0, 0, 0 },
				{ aspect_mask, 0, 0, 1 }, { 0, 0, 0 }, { _depthstencil_clear_image_extent.width, _depthstencil_clear_image_extent.height, 1 }
			};
			_vk->CmdCopyImage(cmd_list, depthstencil, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, _device_tracker->_depthstencil_clear_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy_range);

			transition[0].srcAccessMask = transition[0].dstAccessMask;
			transition[0].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			transition[0].oldLayout = transition[0].newLayout;
			transition[0].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			transition[1].srcAccessMask = transition[1].dstAccessMask;
			transition[1].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			transition[1].oldLayout = transition[1].newLayout;
			transition[1].newLayout = layout;

			_vk->CmdPipelineBarrier(cmd_list, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 2, transition);
		}
	}

	bool draw_call_tracker::update_depthstencil_clear_image(VkImageCreateInfo create_info)
	{
		// This cannot be called on a command list draw call tracker instance
		assert(_vk != nullptr && _device != VK_NULL_HANDLE && this == _device_tracker);
		assert(create_info.imageType == VK_IMAGE_TYPE_2D && create_info.extent.depth == 1);

		if (_depthstencil_clear_image != VK_NULL_HANDLE)
		{
			if (create_info.extent.width == _depthstencil_clear_image_extent.width &&
				create_info.extent.height == _depthstencil_clear_image_extent.height &&
				create_info.format == _depthstencil_clear_image_format)
				return true; // Texture already matches dimensions, so can re-use

			// Make sure memory is not currently in use before freeing it
			_vk->DeviceWaitIdle(_device);

			// Clean up existing image and memory
			_vk->FreeMemory(_device, _depthstencil_clear_image_mem, nullptr);
			_vk->DestroyImage(_device, _depthstencil_clear_image, nullptr);
			_depthstencil_clear_image = VK_NULL_HANDLE;
			_depthstencil_clear_image_mem = VK_NULL_HANDLE;
			_depthstencil_clear_image_format = VK_FORMAT_UNDEFINED;
			_depthstencil_clear_image_extent = { 0, 0 };
		}

		create_info.mipLevels = 1;
		create_info.arrayLayers = 1;
		create_info.samples = VK_SAMPLE_COUNT_1_BIT;
		create_info.tiling = VK_IMAGE_TILING_OPTIMAL;
		create_info.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		create_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

		if (_vk->CreateImage(_device, &create_info, nullptr, &_depthstencil_clear_image) != VK_SUCCESS)
		{
			LOG(ERROR) << "Failed to create depth image!";
			return false;
		}

		VkMemoryRequirements reqs = {};
		_vk->GetImageMemoryRequirements(_device, _depthstencil_clear_image, &reqs);

		VkMemoryAllocateInfo alloc_info { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
		alloc_info.allocationSize = reqs.size;
		alloc_info.memoryTypeIndex = find_memory_type_index(_memory_props, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, reqs.memoryTypeBits);

		if (alloc_info.memoryTypeIndex == std::numeric_limits<uint32_t>::max())
		{
			LOG(ERROR) << "Failed to find memory type for depth image!";

			_vk->DestroyImage(_device, _depthstencil_clear_image, nullptr);
			_depthstencil_clear_image = VK_NULL_HANDLE;
			return false;
		}

		VkMemoryDedicatedAllocateInfo dedicated_info { VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO };
		alloc_info.pNext = &dedicated_info;
		dedicated_info.image = _depthstencil_clear_image;

		if (_vk->AllocateMemory(_device, &alloc_info, nullptr, &_depthstencil_clear_image_mem) != VK_SUCCESS ||
			_vk->BindImageMemory(_device, _depthstencil_clear_image, _depthstencil_clear_image_mem, 0) != VK_SUCCESS)
		{
			LOG(ERROR) << "Failed to allocate or bind memory for depth image!";

			_vk->FreeMemory(_device, _depthstencil_clear_image_mem, nullptr);
			_vk->DestroyImage(_device, _depthstencil_clear_image, nullptr);
			_depthstencil_clear_image = VK_NULL_HANDLE;
			_depthstencil_clear_image_mem = VK_NULL_HANDLE;
			return false;
		}

		_depthstencil_clear_image_extent.width = create_info.extent.width;
		_depthstencil_clear_image_extent.height = create_info.extent.height;
		_depthstencil_clear_image_format = create_info.format;

		return true;
	}

	bool draw_call_tracker::check_aspect_ratio(const VkImageCreateInfo &create_info, uint32_t width, uint32_t height)
	{
		if (!filter_aspect_ratio)
			return true;

		const float aspect_ratio = float(width) / float(height);
		const float texture_aspect_ratio = float(create_info.extent.width) / float(create_info.extent.height);

		const float width_factor = float(width) / float(create_info.extent.width);
		const float height_factor = float(height) / float(create_info.extent.height);

		return !(fabs(texture_aspect_ratio - aspect_ratio) > 0.1f || width_factor > 1.85f || height_factor > 1.85f || width_factor < 0.5f || height_factor < 0.5f);
	}
	bool draw_call_tracker::check_texture_format(const VkImageCreateInfo &create_info)
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

	draw_call_tracker::depthstencil_info draw_call_tracker::find_best_depth_texture(uint32_t width, uint32_t height, VkImage override, uint32_t clear_index_override)
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

		if (preserve_depth_buffers && best_snapshot.image != VK_NULL_HANDLE)
		{
			_depthstencil_clear_index = { best_snapshot.image, std::numeric_limits<uint32_t>::max() };

			if (clear_index_override != 0 && clear_index_override <= best_snapshot.clears.size())
			{
				_depthstencil_clear_index.second = clear_index_override;
			}
			else
			{
				uint32_t last_vertices = 0;

				for (uint32_t clear_index = 0; clear_index < best_snapshot.clears.size(); ++clear_index)
				{
					const auto &snapshot = best_snapshot.clears[clear_index];

					if (snapshot.vertices >= last_vertices)
					{
						last_vertices = snapshot.vertices;
						_depthstencil_clear_index.second = clear_index + 1;
					}
				}
			}

			if (update_depthstencil_clear_image(best_snapshot.image_info))
			{
				best_snapshot.image = _depthstencil_clear_image;
				best_snapshot.image_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL; // This was transitioned in 'track_cleared_depthstencil'
			}
			else
			{
				_depthstencil_clear_index = { VK_NULL_HANDLE, std::numeric_limits<uint32_t>::max() };
			}
		}

		return best_snapshot;
	}
#endif
}
