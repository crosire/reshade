#include "draw_call_tracker.hpp"
#include "format_utils.hpp"
#include "runtime_vk.hpp"
#include <mutex>
#include <math.h>

namespace reshade::vulkan
{
	static std::mutex s_global_mutex;

	void draw_call_tracker::merge(const draw_call_tracker &source)
	{
		const std::lock_guard<std::mutex> lock(s_global_mutex);

		_global_counter.vertices += source.total_vertices();
		_global_counter.drawcalls += source.total_drawcalls();

#if RESHADE_VULKAN_CAPTURE_DEPTH_BUFFERS
		for (const auto &[clear_index, snapshot] : source._cleared_depth_images)
		{
			_cleared_depth_images[clear_index] = snapshot;
		}

		for (const auto &[depthstencil, snapshot] : source._counters_per_used_depth_image)
		{
			_counters_per_used_depth_image[depthstencil].stats.vertices += snapshot.stats.vertices;
			_counters_per_used_depth_image[depthstencil].stats.drawcalls += snapshot.stats.drawcalls;
			_counters_per_used_depth_image[depthstencil].image = snapshot.image;
			_counters_per_used_depth_image[depthstencil].image_info = snapshot.image_info;
			_counters_per_used_depth_image[depthstencil].image_layout = snapshot.image_layout;
		}
#endif
	}

	void draw_call_tracker::reset()
	{
		const std::lock_guard<std::mutex> lock(s_global_mutex);

		_global_counter.vertices = 0;
		_global_counter.drawcalls = 0;
#if RESHADE_VULKAN_CAPTURE_DEPTH_BUFFERS
		_cleared_depth_images.clear();
		_counters_per_used_depth_image.clear();
#endif
	}

	void draw_call_tracker::on_draw(uint32_t vertices)
	{
		const std::lock_guard<std::mutex> lock(s_global_mutex);

		_global_counter.vertices += vertices;
		_global_counter.drawcalls += 1;

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
	bool draw_call_tracker::filter_aspect_ratio = true;
	bool draw_call_tracker::preserve_depth_buffers = false;
	bool draw_call_tracker::preserve_stencil_buffers = false;
	unsigned int draw_call_tracker::depth_stencil_clear_index = 0;
	unsigned int draw_call_tracker::filter_depth_texture_format = 0;

	void draw_call_tracker::track_render_pass(VkImage depthstencil, VkImageLayout layout, const VkImageCreateInfo &create_info)
	{
		track_next_depthstencil(depthstencil);

		if (depthstencil == VK_NULL_HANDLE)
			return;

		assert(layout != VK_IMAGE_LAYOUT_UNDEFINED);
		assert((create_info.usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0);

		const std::lock_guard<std::mutex> lock(s_global_mutex);

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
	void draw_call_tracker::track_cleared_depthstencil(VkCommandBuffer cmd_list, VkImageAspectFlags clear_flags, VkImage depthstencil, VkImageLayout layout, const VkImageCreateInfo &create_info, uint32_t clear_index, runtime_vk *runtime)
	{
		if (!(preserve_depth_buffers && (clear_flags & VK_IMAGE_ASPECT_DEPTH_BIT)) &&
			!(preserve_stencil_buffers && (clear_flags & VK_IMAGE_ASPECT_STENCIL_BIT)))
			return;

		const std::lock_guard<std::mutex> lock(s_global_mutex);

		assert(depthstencil != VK_NULL_HANDLE);
		assert(layout != VK_IMAGE_LAYOUT_UNDEFINED);
		assert((create_info.usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0);

		if (create_info.samples != VK_SAMPLE_COUNT_1_BIT)
			return; // Ignore MSAA textures
		if (!check_texture_format(create_info))
			return;

		VkImage backup_image = VK_NULL_HANDLE;

		// Make a backup copy of depth textures that are cleared by the application
		if ((depth_stencil_clear_index == 0) || (clear_index == depth_stencil_clear_index))
		{
			backup_image = runtime->create_compatible_image(create_info);
			if (backup_image == VK_NULL_HANDLE)
				return;

			assert(cmd_list != VK_NULL_HANDLE);

			const VkImageAspectFlags aspect_mask =
				aspect_flags_from_format(create_info.format);

			VkImageMemoryBarrier transition[2];
			transition[0] = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
			transition[0].image = backup_image;
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

			runtime->vk.CmdPipelineBarrier(cmd_list, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 2, transition);

			// TODO: It is invalid for the below to be called in a render pass (which is the case when called from vkCmdClearAttachments). Need to find a solution.
			const VkImageCopy copy_range = {
				{ aspect_mask, 0, 0, 1 }, { 0, 0, 0 },
				{ aspect_mask, 0, 0, 1 }, { 0, 0, 0 }, { create_info.extent.width, create_info.extent.height, 1 }
			};
			runtime->vk.CmdCopyImage(cmd_list, depthstencil, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, backup_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy_range);

			transition[0].srcAccessMask = transition[0].dstAccessMask;
			transition[0].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			transition[0].oldLayout = transition[0].newLayout;
			transition[0].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			transition[1].srcAccessMask = transition[1].dstAccessMask;
			transition[1].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			transition[1].oldLayout = transition[1].newLayout;
			transition[1].newLayout = layout;

			runtime->vk.CmdPipelineBarrier(cmd_list, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 2, transition);
		}

		_cleared_depth_images.insert({ clear_index, { depthstencil, backup_image, create_info } });
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

	draw_call_tracker::intermediate_snapshot_info draw_call_tracker::find_best_depth_texture(uint32_t width, uint32_t height)
	{
		intermediate_snapshot_info best_snapshot;

		const std::lock_guard<std::mutex> lock(s_global_mutex);

		if (preserve_depth_buffers || preserve_stencil_buffers)
		{
			if (const auto it = _cleared_depth_images.find(depth_stencil_clear_index);
				depth_stencil_clear_index != 0 && it != _cleared_depth_images.end())
			{
				best_snapshot.image = it->second.backup_image;
				best_snapshot.image_info = it->second.image_info;
				best_snapshot.image_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				return best_snapshot;
			}

			for (const auto &[clear_index, snapshot] : _cleared_depth_images)
			{
				assert(snapshot.src_image != VK_NULL_HANDLE);

				if (snapshot.backup_image == VK_NULL_HANDLE)
					continue;

				if (!check_aspect_ratio(snapshot.image_info, width, height))
					continue;

				best_snapshot.image = snapshot.backup_image;
				best_snapshot.image_info = snapshot.image_info;
				best_snapshot.image_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL; // This was transitioned in 'track_cleared_depthstencil'
			}
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
