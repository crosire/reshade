#pragma once

#include <map>
#include <vulkan.h>
#include "com_ptr.hpp"

#define RESHADE_VULKAN_CAPTURE_DEPTH_BUFFERS 1

namespace reshade::vulkan
{
	class draw_call_tracker
	{
	public:
		struct draw_stats
		{
			UINT vertices = 0;
			UINT drawcalls = 0;
			UINT mapped = 0;
			UINT vs_uses = 0;
			UINT ps_uses = 0;
		};

#if RESHADE_VULKAN_CAPTURE_DEPTH_BUFFERS
		struct intermediate_snapshot_info
		{
			VkImage depthstencil = VK_NULL_HANDLE;
			draw_stats stats;
			VkImageLayout layout;
			VkImageCreateInfo create_info;
		};

		struct intermediate_cleared_depthstencil_info
		{
			VkImage image;
			VkImageLayout layout;
			VkImageCreateInfo image_info;
		};
#endif

		UINT total_vertices() const { return _global_counter.vertices; }
		UINT total_drawcalls() const { return _global_counter.drawcalls; }

#if RESHADE_VULKAN_CAPTURE_DEPTH_BUFFERS
		const auto &depth_buffer_counters() const { return _counters_per_used_depthstencil; }
		const auto &cleared_depth_textures() const { return _cleared_depth_images; }
#endif

		void merge(const draw_call_tracker &source);
		void reset();

		void on_draw(UINT vertices);

#if RESHADE_VULKAN_CAPTURE_DEPTH_BUFFERS
		void track_renderpass(int format_index, VkImage depthstencil, VkImageLayout layout, const VkImageCreateInfo &depthstencil_create_info);
		void track_depth_image(int format_index, UINT index, VkImage src_image, const VkImageCreateInfo &src_image_info, VkImage dest_image, bool cleared);

		void keep_cleared_depth_textures();

		intermediate_snapshot_info find_best_snapshot(uint32_t width, uint32_t height);
		intermediate_cleared_depthstencil_info find_best_cleared_depth_buffer_image(uint32_t clear_index);
#endif

		VkImage _current_depthstencil;

	private:
		struct depth_texture_save_info
		{
			VkImage src_image;
			VkImageCreateInfo src_image_info;
			VkImage dest_image;
			bool cleared = false;
		};

#if RESHADE_VULKAN_CAPTURE_DEPTH_BUFFERS
		bool check_depthstencil(VkImage depthstencil) const;
		bool check_depth_texture_format(int format_index, VkFormat format);
#endif

		draw_stats _global_counter;
#if RESHADE_VULKAN_CAPTURE_DEPTH_BUFFERS

		// Use "std::map" instead of "std::unordered_map" so that the iteration order is guaranteed
		std::map<VkImage, intermediate_snapshot_info> _counters_per_used_depthstencil;
		std::map<uint32_t, depth_texture_save_info> _cleared_depth_images;
#endif
	};
}
