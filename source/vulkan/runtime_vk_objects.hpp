/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "runtime_objects.hpp"

namespace reshade::vulkan
{
	struct tex_data
	{
		VkImage image = VK_NULL_HANDLE;
		VkImageView view[4] = {};
		VmaAllocation image_mem = VK_NULL_HANDLE;
		VkFormat formats[2] = {};
#if RESHADE_GUI
		VkDescriptorSet descriptor_set = VK_NULL_HANDLE;
#endif
		uint32_t width = 0, height = 0, levels = 0;
	};

	struct pass_data
	{
		VkPipeline pipeline = VK_NULL_HANDLE;
		VkDescriptorSet set[2] = {};
		VkClearValue clear_values[8] = {};
		VkRenderPassBeginInfo begin_info = { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
		std::vector<const tex_data *> modified_resources;
	};

	struct effect_data
	{
		struct binding_data
		{
			std::string semantic;
			uint32_t index;
			std::vector<VkDescriptorSet> sets;
		};

		VkQueryPool query_pool = VK_NULL_HANDLE;
		VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
		VkDescriptorSetLayout sampler_layout = VK_NULL_HANDLE;
		VkDescriptorSetLayout storage_layout = VK_NULL_HANDLE;
		VkBuffer ubo = VK_NULL_HANDLE;
		VmaAllocation ubo_mem = VK_NULL_HANDLE;
		VkDescriptorSet ubo_set = VK_NULL_HANDLE;
		std::vector<binding_data> texture_semantic_to_binding;
		std::vector<VkDescriptorImageInfo> image_bindings;
	};

	struct technique_data
	{
		bool has_compute_passes = false;
		uint32_t query_base_index = 0;
		std::vector<pass_data> passes;
	};
}
