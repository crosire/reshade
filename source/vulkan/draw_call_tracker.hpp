#pragma once

#include <map>
#include <vulkan.h>

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
		struct cleared_depthstencil_info
		{
			VkImage src_image = VK_NULL_HANDLE;
			VkImage backup_image = VK_NULL_HANDLE;
			VkImageCreateInfo image_info = {};
		};
		struct intermediate_snapshot_info
		{
			draw_stats stats;
			VkImage image = VK_NULL_HANDLE;
			VkImageLayout image_layout = VK_IMAGE_LAYOUT_UNDEFINED;
			VkImageCreateInfo image_info = {};
		};

		static bool filter_aspect_ratio;
		static bool preserve_depth_buffers;
		static bool preserve_stencil_buffers;
		static unsigned int depth_stencil_clear_index;
		static unsigned int filter_depth_texture_format;
#endif

		uint32_t total_vertices() const { return _global_counter.vertices; }
		uint32_t total_drawcalls() const { return _global_counter.drawcalls; }

#if RESHADE_VULKAN_CAPTURE_DEPTH_BUFFERS
		const auto &cleared_depth_images() const { return _cleared_depth_images; }
		const auto &depth_buffer_counters() const { return _counters_per_used_depth_image; }
#endif

		void merge(const draw_call_tracker &source);
		void reset();

		void on_draw(uint32_t vertices);

#if RESHADE_VULKAN_CAPTURE_DEPTH_BUFFERS
		void track_render_pass(VkImage depthstencil, VkImageLayout layout, const VkImageCreateInfo &create_info);
		void track_next_depthstencil(VkImage depthstencil);
		void track_cleared_depthstencil(VkCommandBuffer cmd_list, VkImageAspectFlags clear_flags, VkImage depthstencil, VkImageLayout layout, const VkImageCreateInfo &create_info, uint32_t clear_index, class runtime_vk *runtime);

		static bool check_aspect_ratio(const VkImageCreateInfo &create_info, uint32_t width, uint32_t height);
		static bool check_texture_format(const VkImageCreateInfo &create_info);

		intermediate_snapshot_info find_best_depth_texture(uint32_t width, uint32_t height);
#endif

	private:
		draw_stats _global_counter;
#if RESHADE_VULKAN_CAPTURE_DEPTH_BUFFERS
		VkImage _current_depthstencil = VK_NULL_HANDLE;
		std::map<uint32_t, cleared_depthstencil_info> _cleared_depth_images;
		// Use "std::map" instead of "std::unordered_map" so that the iteration order is guaranteed
		std::map<VkImage, intermediate_snapshot_info> _counters_per_used_depth_image;
#endif
	};
}
