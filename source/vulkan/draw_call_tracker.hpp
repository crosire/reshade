#pragma once

#include <map>
#include "com_ptr.hpp"
#include <vulkan.h>

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

		struct depthstencil_infos
		{
			VkImage image = VK_NULL_HANDLE;
			VkImageCreateInfo image_info;
			VkImageViewCreateInfo image_view_info;
		};

#if RESHADE_VULKAN_CAPTURE_DEPTH_BUFFERS
		struct intermediate_snapshot_info
		{
			VkImageView depthstencil = VK_NULL_HANDLE;
			draw_stats stats;
			VkImage image;
			VkImageCreateInfo image_info;
			VkImageViewCreateInfo image_view_info;
			std::map<VkImageView, draw_stats> additional_views;
		};

		struct intermediate_cleared_depthstencil_info
		{
			VkImage image;
			VkImageView src_depthstencil;
			VkImageCreateInfo image_info;
			VkImageViewCreateInfo image_view_info;
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

		// void on_map(ID3D12Resource *pResource);
		void on_draw(VkImageView current_depthstencil, UINT vertices);

		VkImageView _depthstencil;

#if RESHADE_VULKAN_CAPTURE_DEPTH_BUFFERS
		void track_renderpasses(int formatIdx, VkImageView depthstencil, VkImage image, VkImageCreateInfo imageInfo, VkImageViewCreateInfo imageViewInfo);
		void track_depth_image(int formatIdx, UINT index, VkImage srcImage, VkImageCreateInfo srcImageInfo, VkImageView srcDepthstencil, VkImageViewCreateInfo srcDepthstencilInfo, VkImage destImage, bool cleared);

		void keep_cleared_depth_textures();

		intermediate_snapshot_info find_best_snapshot(UINT width, UINT height);
		intermediate_cleared_depthstencil_info find_best_cleared_depth_buffer_image(UINT clearIdx);
#endif

	private:
		struct depth_texture_save_info
		{
			VkImage src_image;
			VkImageView src_depthstencil;
			VkImageCreateInfo src_image_info;
			VkImageViewCreateInfo src_image_view_info;
			VkImage dest_image;
			bool cleared = false;
		};

#if RESHADE_VULKAN_CAPTURE_DEPTH_BUFFERS
		bool check_depthstencil(VkImageView) const;
		bool check_depth_texture_format(int formatIdx, VkFormat format);
#endif

		draw_stats _global_counter;
#if RESHADE_VULKAN_CAPTURE_DEPTH_BUFFERS

		// Use "std::map" instead of "std::unordered_map" so that the iteration order is guaranteed
		std::map<VkImageView, intermediate_snapshot_info> _counters_per_used_depthstencil;
		std::map<UINT, depth_texture_save_info> _cleared_depth_images;
#endif
	};
}
