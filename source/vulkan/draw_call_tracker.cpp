#include "draw_call_tracker.hpp"
#include "dxgi/format_utils.hpp"
#include <math.h>

namespace reshade::vulkan
{
	void draw_call_tracker::merge(const draw_call_tracker& source)
	{
		_global_counter.vertices += source.total_vertices();
		_global_counter.drawcalls += source.total_drawcalls();

#if RESHADE_VULKAN_CAPTURE_DEPTH_BUFFERS
		for (const auto &[depthstencil, snapshot] : source._counters_per_used_depthstencil)
		{
			_counters_per_used_depthstencil[depthstencil].stats.vertices += snapshot.stats.vertices;
			_counters_per_used_depthstencil[depthstencil].stats.drawcalls += snapshot.stats.drawcalls;
			_counters_per_used_depthstencil[depthstencil].depthstencil = snapshot.depthstencil;
			_counters_per_used_depthstencil[depthstencil].create_info = snapshot.create_info;
			_counters_per_used_depthstencil[depthstencil].layout = snapshot.layout;
		}

		for (const auto &[index, depth_texture_save_info] : source._cleared_depth_images)
			_cleared_depth_images[index] = depth_texture_save_info;
#endif
	}

	void draw_call_tracker::reset()
	{
		_global_counter.vertices = 0;
		_global_counter.drawcalls = 0;
		_current_depthstencil = VK_NULL_HANDLE;
#if RESHADE_VULKAN_CAPTURE_DEPTH_BUFFERS
		_counters_per_used_depthstencil.clear();
		_cleared_depth_images.clear();
#endif
	}

	void draw_call_tracker::on_draw(UINT vertices)
	{
		_global_counter.vertices += vertices;
		_global_counter.drawcalls += 1;

#if RESHADE_VULKAN_CAPTURE_DEPTH_BUFFERS
		if (_current_depthstencil == VK_NULL_HANDLE)
			// This is a draw call with no depth stencil
			return;

		if (const auto intermediate_snapshot = _counters_per_used_depthstencil.find(_current_depthstencil); intermediate_snapshot != _counters_per_used_depthstencil.end())
		{
			intermediate_snapshot->second.stats.vertices += vertices;
			intermediate_snapshot->second.stats.drawcalls += 1;
		}
#endif
	}

#if RESHADE_VULKAN_CAPTURE_DEPTH_BUFFERS
	bool draw_call_tracker::check_depthstencil(VkImage depthstencil) const
	{
		return _counters_per_used_depthstencil.find(depthstencil) != _counters_per_used_depthstencil.end();
	}
	bool draw_call_tracker::check_depth_texture_format(int format_index, VkFormat format)
	{
		assert(format != VK_NULL_HANDLE);

		// Do not check format if all formats are allowed (index zero is VK_FORMAT_UNDEFINED)
		if (format_index == VK_FORMAT_UNDEFINED)
			return true;

		// Retrieve texture associated with depth stencil
		const VkFormat depth_texture_formats[] = {
			VK_FORMAT_UNDEFINED,
			VK_FORMAT_D16_UNORM,
			VK_FORMAT_D16_UNORM_S8_UINT,
			VK_FORMAT_D24_UNORM_S8_UINT,
			VK_FORMAT_D32_SFLOAT,
			VK_FORMAT_D32_SFLOAT_S8_UINT
		};

		assert(format_index > VK_FORMAT_UNDEFINED && format_index < ARRAYSIZE(depth_texture_formats));

		return format == depth_texture_formats[format_index];
	}

	void draw_call_tracker::track_renderpass(int format_index, VkImage depthstencil, VkImageLayout layout, const VkImageCreateInfo &depthstencil_create_info)
	{
		assert(depthstencil != VK_NULL_HANDLE);
		assert(layout != VK_IMAGE_LAYOUT_UNDEFINED);

		if (!check_depth_texture_format(format_index, depthstencil_create_info.format))
			return;

		if (_counters_per_used_depthstencil[depthstencil].depthstencil == VK_NULL_HANDLE)
		{
			_counters_per_used_depthstencil[depthstencil].depthstencil = depthstencil;
			_counters_per_used_depthstencil[depthstencil].layout = layout;
			_counters_per_used_depthstencil[depthstencil].create_info = depthstencil_create_info;
		}
	}
	void draw_call_tracker::track_depth_image(int format_index, UINT index, VkImage src_image, const VkImageCreateInfo &src_image_info, VkImage dest_image, bool cleared)
	{
		// Function that keeps track of a cleared depth texture in an ordered map in order to retrieve it at the final rendering stage
		assert(src_image != VK_NULL_HANDLE);

		if (!check_depth_texture_format(format_index, src_image_info.format))
			return;

		// check if it is really a depth texture
		assert((src_image_info.usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0);

		// fill the ordered map with the saved depth texture
		_cleared_depth_images[index] = depth_texture_save_info { src_image, src_image_info, dest_image, cleared };
	}

	draw_call_tracker::intermediate_snapshot_info draw_call_tracker::find_best_snapshot(uint32_t width, uint32_t height)
	{
		const float aspect_ratio = float(width) / float(height);
		intermediate_snapshot_info best_snapshot;

		for (auto &[depthstencil, snapshot] : _counters_per_used_depthstencil)
		{
			if (snapshot.stats.drawcalls == 0 || snapshot.stats.vertices == 0)
				continue;

			assert(snapshot.depthstencil != VK_NULL_HANDLE);
			assert((snapshot.create_info.usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0);

			// Check aspect ratio
			const float width_factor = float(width) / float(snapshot.create_info.extent.width);
			const float height_factor = float(height) / float(snapshot.create_info.extent.height);
			const float texture_aspect_ratio = float(snapshot.create_info.extent.width) / float(snapshot.create_info.extent.height);

			if (fabs(texture_aspect_ratio - aspect_ratio) > 0.1f || width_factor > 1.85f || height_factor > 1.85f || width_factor < 0.5f || height_factor < 0.5f)
				continue; // No match, not a good fit

			if (snapshot.stats.vertices >= best_snapshot.stats.vertices)
				best_snapshot = snapshot;
		}

		return best_snapshot;
	}

	void draw_call_tracker::keep_cleared_depth_textures()
	{
		// Function that keeps only the depth textures that has been retrieved before the last depth stencil clearance
		std::map<uint32_t, depth_texture_save_info>::reverse_iterator it = _cleared_depth_images.rbegin();

		// Reverse loop on the cleared depth textures map
		while (it != _cleared_depth_images.rend())
		{
			// Exit if the last cleared depth stencil is found
			if (it->second.cleared)
				return;

			// Remove the depth texture if it was retrieved after the last clearance of the depth stencil
			it = std::map<uint32_t, depth_texture_save_info>::reverse_iterator(_cleared_depth_images.erase(std::next(it).base()));
		}
	}

	draw_call_tracker::intermediate_cleared_depthstencil_info draw_call_tracker::find_best_cleared_depth_buffer_image(uint32_t clear_index)
	{
		// Function that selects the best cleared depth texture according to the clearing number defined in the configuration settings
		intermediate_cleared_depthstencil_info best_match = { VK_NULL_HANDLE };

		// Ensure to work only on the depth textures retrieved before the last depth stencil clearance
		keep_cleared_depth_textures();

		for (const auto &it : _cleared_depth_images)
		{
			uint32_t i = it.first;
			auto &image_counter_info = it.second;

			VkImage image;
			if (image_counter_info.dest_image == VK_NULL_HANDLE)
				continue;
			image = image_counter_info.dest_image;

			if (clear_index != 0 && i > clear_index)
				continue;

			// The _cleared_dept_textures ordered map stores the depth textures, according to the order of clearing
			// if clear_index == 0, the auto select mode is defined, so the last cleared depth texture is retrieved
			// if the user selects a clearing number and the number of cleared depth textures is greater or equal than it, the texture corresponding to this number is retrieved
			// if the user selects a clearing number and the number of cleared depth textures is lower than it, the last cleared depth texture is retrieved
			best_match.image = image;
			best_match.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL; // This is transitioned in 'save_depth_image'
			best_match.image_info = image_counter_info.src_image_info;
		}

		return best_match;
	}
#endif
}
