/*
 * Copyright (C) 2021 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "render_vk.hpp"
#include "render_vk_utils.hpp"
#include "format_utils.hpp"
#include <algorithm>

#define vk _dispatch_table

static inline VkImageSubresourceLayers convert_subresource(uint32_t subresource, const VkImageCreateInfo &create_info)
{
	VkImageSubresourceLayers result;
	result.aspectMask = aspect_flags_from_format(create_info.format);
	result.mipLevel = subresource % create_info.mipLevels;
	result.baseArrayLayer = subresource / create_info.mipLevels;
	result.layerCount = 1;
	return result;
}

reshade::vulkan::device_impl::device_impl(VkDevice device, VkPhysicalDevice physical_device, const VkLayerInstanceDispatchTable &instance_table, const VkLayerDispatchTable &device_table) :
	api_object_impl(device), _physical_device(physical_device), _dispatch_table(device_table), _instance_dispatch_table(instance_table)
{
	{	VmaVulkanFunctions functions;
		functions.vkGetPhysicalDeviceProperties = instance_table.GetPhysicalDeviceProperties;
		functions.vkGetPhysicalDeviceMemoryProperties = instance_table.GetPhysicalDeviceMemoryProperties;
		functions.vkAllocateMemory = device_table.AllocateMemory;
		functions.vkFreeMemory = device_table.FreeMemory;
		functions.vkMapMemory = device_table.MapMemory;
		functions.vkUnmapMemory = device_table.UnmapMemory;
		functions.vkFlushMappedMemoryRanges = device_table.FlushMappedMemoryRanges;
		functions.vkInvalidateMappedMemoryRanges = device_table.InvalidateMappedMemoryRanges;
		functions.vkBindBufferMemory = device_table.BindBufferMemory;
		functions.vkBindImageMemory = device_table.BindImageMemory;
		functions.vkGetBufferMemoryRequirements = device_table.GetBufferMemoryRequirements;
		functions.vkGetImageMemoryRequirements = device_table.GetImageMemoryRequirements;
		functions.vkCreateBuffer = device_table.CreateBuffer;
		functions.vkDestroyBuffer = device_table.DestroyBuffer;
		functions.vkCreateImage = device_table.CreateImage;
		functions.vkDestroyImage = device_table.DestroyImage;
		functions.vkCmdCopyBuffer = device_table.CmdCopyBuffer;
		functions.vkGetBufferMemoryRequirements2KHR = device_table.GetBufferMemoryRequirements2;
		functions.vkGetImageMemoryRequirements2KHR = device_table.GetImageMemoryRequirements2;
		functions.vkBindBufferMemory2KHR = device_table.BindBufferMemory2;
		functions.vkBindImageMemory2KHR = device_table.BindImageMemory2;
		functions.vkGetPhysicalDeviceMemoryProperties2KHR = instance_table.GetPhysicalDeviceMemoryProperties2;

		VmaAllocatorCreateInfo create_info = {};
		// The runtime runs in a single thread, so no synchronization necessary
		create_info.flags = VMA_ALLOCATOR_CREATE_EXTERNALLY_SYNCHRONIZED_BIT;
		create_info.physicalDevice = physical_device;
		create_info.device = device;
		create_info.preferredLargeHeapBlockSize = 1920 * 1080 * 4 * 16; // Allocate blocks of memory that can comfortably contain 16 Full HD images
		create_info.pVulkanFunctions = &functions;
		create_info.vulkanApiVersion = VK_API_VERSION_1_1; // Vulkan 1.1 is guaranteed by code in vulkan_hooks_instance.cpp

		vmaCreateAllocator(&create_info, &_alloc);
	}

#if RESHADE_ADDON
	addon::load_addons();
#endif

	invoke_addon_event<reshade::addon_event::init_device>(this);
}
reshade::vulkan::device_impl::~device_impl()
{
	assert(_queues.empty()); // All queues should have been unregistered and destroyed at this point

	invoke_addon_event<reshade::addon_event::destroy_device>(this);

#if RESHADE_ADDON
	addon::unload_addons();
#endif

	for (const auto &it : _render_pass_list_internal)
		vk.DestroyRenderPass(_orig, it.second, nullptr);
	for (const auto &it : _framebuffer_list_internal)
		vk.DestroyFramebuffer(_orig, it.second, nullptr);

	vmaDestroyAllocator(_alloc);
}

bool reshade::vulkan::device_impl::check_capability(api::device_caps capability) const
{
	VkPhysicalDeviceFeatures features = {};
	_instance_dispatch_table.GetPhysicalDeviceFeatures(_physical_device, &features);

	switch (capability)
	{
	case api::device_caps::compute_shader:
		return true;
	case api::device_caps::geometry_shader:
		return features.geometryShader;
	case api::device_caps::tessellation_shaders:
		return features.tessellationShader;
	case api::device_caps::dual_src_blend:
		return features.dualSrcBlend;
	case api::device_caps::independent_blend:
		return features.independentBlend;
	case api::device_caps::logic_op:
		return features.logicOp;
	case api::device_caps::draw_instanced:
		return true;
	case api::device_caps::draw_or_dispatch_indirect:
		// Technically this only specifies whether multi-draw indirect is supported, not draw indirect as a whole
		return features.multiDrawIndirect;
	case api::device_caps::fill_mode_non_solid:
		return features.fillModeNonSolid;
	case api::device_caps::multi_viewport:
		return features.multiViewport;
	case api::device_caps::sampler_anisotropy:
		return features.samplerAnisotropy;
	case api::device_caps::push_descriptors:
		return vk.CmdPushDescriptorSetKHR != nullptr;
	case api::device_caps::descriptor_tables:
	case api::device_caps::sampler_with_resource_view:
	case api::device_caps::blit:
	case api::device_caps::resolve_region:
	case api::device_caps::copy_buffer_region:
	case api::device_caps::copy_buffer_to_texture:
		return true;
	default:
		return false;
	}
}
bool reshade::vulkan::device_impl::check_format_support(api::format format, api::resource_usage usage) const
{
	const VkFormat vk_format = convert_format(format);
	if (vk_format == VK_FORMAT_UNDEFINED)
		return false;

	VkFormatProperties props = {};
	_instance_dispatch_table.GetPhysicalDeviceFormatProperties(_physical_device, vk_format, &props);

	if ((usage & api::resource_usage::render_target) != 0 &&
		(props.optimalTilingFeatures & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT) == 0)
		return false;
	if ((usage & api::resource_usage::depth_stencil) != 0 &&
		(props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) == 0)
		return false;
	if ((usage & api::resource_usage::shader_resource) != 0 &&
		(props.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) == 0)
		return false;
	if ((usage & api::resource_usage::unordered_access) != 0 &&
		(props.optimalTilingFeatures & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT) == 0)
		return false;
	if ((usage & api::resource_usage::copy_dest) != 0 &&
		(props.optimalTilingFeatures & VK_FORMAT_FEATURE_TRANSFER_DST_BIT) == 0)
		return false;
	if ((usage & api::resource_usage::copy_source) != 0 &&
		(props.optimalTilingFeatures & VK_FORMAT_FEATURE_TRANSFER_SRC_BIT) == 0)
		return false;

	return true;
}

bool reshade::vulkan::device_impl::check_resource_handle_valid(api::resource resource) const
{
	if (resource.handle == 0)
		return false;
	const resource_data &data = _resources.at(resource.handle);

	if (data.is_image())
		return data.image == (VkImage)resource.handle;
	else
		return data.buffer == (VkBuffer)resource.handle;
}
bool reshade::vulkan::device_impl::check_resource_view_handle_valid(api::resource_view view) const
{
	if (view.handle == 0)
		return false;
	const resource_view_data &data = _views.at(view.handle);

	if (data.is_image_view())
		return data.image_view == (VkImageView)view.handle;
	else
		return data.buffer_view == (VkBufferView)view.handle;
}

bool reshade::vulkan::device_impl::create_sampler(const api::sampler_desc &desc, api::sampler *out)
{
	VkSamplerCreateInfo create_info { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
	convert_sampler_desc(desc, create_info);

	if (VkSampler object = VK_NULL_HANDLE;
		vk.CreateSampler(_orig, &create_info, nullptr, &object) == VK_SUCCESS)
	{
		*out = { (uint64_t)object };
		return true;
	}
	else
	{
		*out = { 0 };
		return false;
	}
}
bool reshade::vulkan::device_impl::create_resource(const api::resource_desc &desc, const api::subresource_data *initial_data, api::resource_usage initial_state, api::resource *out)
{
	if (initial_data != nullptr)
		return false;

	assert((desc.usage & initial_state) == initial_state || initial_state == api::resource_usage::cpu_access);

	VmaAllocation allocation = VK_NULL_HANDLE;
	VmaAllocationCreateInfo alloc_info = {};
	switch (desc.heap)
	{
	default:
	case api::memory_heap::gpu_only:
		alloc_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;
		break;
	case api::memory_heap::cpu_to_gpu:
		alloc_info.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
		// Make sure host visible allocations are coherent, since no explicit flushing is performed
		alloc_info.requiredFlags |= VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
		break;
	case api::memory_heap::gpu_to_cpu:
		alloc_info.usage = VMA_MEMORY_USAGE_GPU_TO_CPU;
		break;
	case api::memory_heap::cpu_only:
		alloc_info.usage = VMA_MEMORY_USAGE_CPU_ONLY;
		break;
	}

	switch (desc.type)
	{
		case api::resource_type::buffer:
		{
			VkBufferCreateInfo create_info { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
			convert_resource_desc(desc, create_info);

			if (VkBuffer object = VK_NULL_HANDLE;
				vmaCreateBuffer(_alloc, &create_info, &alloc_info, &object, &allocation, nullptr) == VK_SUCCESS)
			{
				register_buffer(object, create_info, allocation);
				*out = { (uint64_t)object };
				return true;
			}
			break;
		}
		case api::resource_type::texture_1d:
		case api::resource_type::texture_2d:
		case api::resource_type::texture_3d:
		{
			VkImageCreateInfo create_info { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
			convert_resource_desc(desc, create_info);
			if ((desc.usage & (api::resource_usage::render_target | api::resource_usage::shader_resource)) != 0)
				create_info.flags |= VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;

			if (VkImage object = VK_NULL_HANDLE;
				vmaCreateImage(_alloc, &create_info, &alloc_info, &object, &allocation, nullptr) == VK_SUCCESS)
			{
				register_image(object, create_info, allocation);
				*out = { (uint64_t)object };

				if (initial_state != api::resource_usage::undefined)
				{
					// Transition resource into the initial state using the first available immediate command list
					for (command_queue_impl *const queue : _queues)
					{
						const auto immediate_command_list = static_cast<command_list_immediate_impl *>(queue->get_immediate_command_list());
						if (immediate_command_list != nullptr)
						{
							const api::resource_usage states[2] = { api::resource_usage::undefined, initial_state };
							immediate_command_list->insert_barrier(1, out, &states[0], &states[1]);
							immediate_command_list->flush_and_wait((VkQueue)queue->get_native_object());
							break;
						}
					}
				}
				return true;
			}
			break;
		}
	}

	*out = { 0 };
	return false;
}
bool reshade::vulkan::device_impl::create_resource_view(api::resource resource, api::resource_usage usage_type, const api::resource_view_desc &desc, api::resource_view *out)
{
	assert(resource.handle != 0);
	const resource_data &data = _resources.at(resource.handle);

	if (data.is_image())
	{
		VkImageViewCreateInfo create_info { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
		convert_resource_view_desc(desc, create_info);
		create_info.image = data.image;
		create_info.components = { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY };
		create_info.subresourceRange.aspectMask = aspect_flags_from_format(create_info.format);

		// Shader resource views can never access stencil data, so remove that aspect flag for views created with a format that supports stencil
		if ((usage_type & api::resource_usage::shader_resource) != 0)
			create_info.subresourceRange.aspectMask &= ~VK_IMAGE_ASPECT_STENCIL_BIT;

		VkImageView image_view = VK_NULL_HANDLE;
		if (vk.CreateImageView(_orig, &create_info, nullptr, &image_view) == VK_SUCCESS)
		{
			register_image_view(image_view, create_info);
			*out = { (uint64_t)image_view };
			return true;
		}
	}
	else
	{
		VkBufferViewCreateInfo create_info { VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO };
		convert_resource_view_desc(desc, create_info);
		create_info.buffer = data.buffer;

		VkBufferView buffer_view = VK_NULL_HANDLE;
		if (vk.CreateBufferView(_orig, &create_info, nullptr, &buffer_view) == VK_SUCCESS)
		{
			register_buffer_view(buffer_view, create_info);
			*out = { (uint64_t)buffer_view };
			return true;
		}
	}

	*out = { 0 };
	return false;
}

bool reshade::vulkan::device_impl::create_pipeline(const api::pipeline_desc &desc, api::pipeline *out)
{
	switch (desc.type)
	{
	default:
		*out = { 0 };
		return false;
	case api::pipeline_type::compute:
		return create_pipeline_compute(desc, out);
	case api::pipeline_type::graphics:
		return create_pipeline_graphics_all(desc, out);
	}
}
bool reshade::vulkan::device_impl::create_pipeline_compute(const api::pipeline_desc &desc, api::pipeline *out)
{
	VkComputePipelineCreateInfo create_info { VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };
	create_info.layout = (VkPipelineLayout)desc.layout.handle;

	{	VkPipelineShaderStageCreateInfo &stage = create_info.stage;
		stage = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
		stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
		stage.module = (VkShaderModule)desc.compute.shader.handle;
		stage.pName = "main";
	}

	if (VkPipeline object = VK_NULL_HANDLE;
		vk.CreateComputePipelines(_orig, VK_NULL_HANDLE, 1, &create_info, nullptr, &object) == VK_SUCCESS)
	{
		*out = { (uint64_t)object };
		return true;
	}
	else
	{
		*out = { 0 };
		return false;
	}
}
bool reshade::vulkan::device_impl::create_pipeline_graphics_all(const api::pipeline_desc &desc, api::pipeline *out)
{
	VkGraphicsPipelineCreateInfo create_info { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
	create_info.layout = (VkPipelineLayout)desc.layout.handle;

	VkPipelineShaderStageCreateInfo shader_stage_info[6];
	create_info.pStages = shader_stage_info;
	if (desc.graphics.vertex_shader.handle != 0)
	{
		VkPipelineShaderStageCreateInfo &stage = shader_stage_info[create_info.stageCount++];
		stage = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
		stage.stage = VK_SHADER_STAGE_VERTEX_BIT;
		stage.module = (VkShaderModule)desc.graphics.vertex_shader.handle;
		stage.pName = "main";
	}
	if (desc.graphics.hull_shader.handle != 0)
	{
		VkPipelineShaderStageCreateInfo &stage = shader_stage_info[create_info.stageCount++];
		stage = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
		stage.stage = VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
		stage.module = (VkShaderModule)desc.graphics.hull_shader.handle;
		stage.pName = "main";
	}
	if (desc.graphics.domain_shader.handle != 0)
	{
		VkPipelineShaderStageCreateInfo &stage = shader_stage_info[create_info.stageCount++];
		stage = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
		stage.stage = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
		stage.module = (VkShaderModule)desc.graphics.domain_shader.handle;
		stage.pName = "main";
	}
	if (desc.graphics.geometry_shader.handle != 0)
	{
		VkPipelineShaderStageCreateInfo &stage = shader_stage_info[create_info.stageCount++];
		stage = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
		stage.stage = VK_SHADER_STAGE_GEOMETRY_BIT;
		stage.module = (VkShaderModule)desc.graphics.geometry_shader.handle;
		stage.pName = "main";
	}
	if (desc.graphics.pixel_shader.handle != 0)
	{
		VkPipelineShaderStageCreateInfo &stage = shader_stage_info[create_info.stageCount++];
		stage = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
		stage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		stage.module = (VkShaderModule)desc.graphics.pixel_shader.handle;
		stage.pName = "main";
	}

	std::vector<VkDynamicState> dyn_states;
	dyn_states.reserve(2 + desc.graphics.num_dynamic_states);
	// Always make scissor rectangles and viewports dynamic
	dyn_states.push_back(VK_DYNAMIC_STATE_SCISSOR);
	dyn_states.push_back(VK_DYNAMIC_STATE_VIEWPORT);

	for (uint32_t i = 0; i < desc.graphics.num_dynamic_states; ++i)
	{
		switch (desc.graphics.dynamic_states[i])
		{
		case api::pipeline_state::blend_constant:
			dyn_states.push_back(VK_DYNAMIC_STATE_BLEND_CONSTANTS);
			break;
		case api::pipeline_state::stencil_read_mask:
			dyn_states.push_back(VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK);
			break;
		case api::pipeline_state::stencil_write_mask:
			dyn_states.push_back(VK_DYNAMIC_STATE_STENCIL_WRITE_MASK);
			break;
		case api::pipeline_state::stencil_reference_value:
			dyn_states.push_back(VK_DYNAMIC_STATE_STENCIL_REFERENCE);
			break;
		default:
			*out = { 0 };
			return false;
		}
	}

	VkPipelineDynamicStateCreateInfo dynamic_state_info { VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
	create_info.pDynamicState = &dynamic_state_info;
	dynamic_state_info.dynamicStateCount = static_cast<uint32_t>(dyn_states.size());
	dynamic_state_info.pDynamicStates = dyn_states.data();

	std::vector<VkVertexInputBindingDescription> vertex_bindings;
	std::vector<VkVertexInputAttributeDescription> vertex_attributes;
	vertex_attributes.reserve(16);

	for (uint32_t i = 0; i < 16 && desc.graphics.input_layout[i].format != api::format::unknown; ++i)
	{
		const auto &element = desc.graphics.input_layout[i];

		VkVertexInputAttributeDescription &attribute = vertex_attributes.emplace_back();
		attribute.location = element.location;
		attribute.binding = element.buffer_binding;
		attribute.format = convert_format(element.format);
		attribute.offset = element.offset;

		assert(element.instance_step_rate <= 1);
		const VkVertexInputRate input_rate = element.instance_step_rate > 0 ? VK_VERTEX_INPUT_RATE_INSTANCE : VK_VERTEX_INPUT_RATE_VERTEX;

		if (const auto it = std::find_if(vertex_bindings.begin(), vertex_bindings.end(),
			[&element](const VkVertexInputBindingDescription &input_binding) { return input_binding.binding == element.buffer_binding; });
			it != vertex_bindings.end())
		{
			if (it->inputRate != input_rate || it->stride != element.stride)
			{
				*out = { 0 };
				return false;
			}
		}
		else
		{
			VkVertexInputBindingDescription &binding = vertex_bindings.emplace_back();
			binding.binding = element.buffer_binding;
			binding.stride = element.stride;
			binding.inputRate = input_rate;
		}
	}

	VkPipelineVertexInputStateCreateInfo vertex_input_state_info { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
	create_info.pVertexInputState = &vertex_input_state_info;
	vertex_input_state_info.vertexBindingDescriptionCount = static_cast<uint32_t>(vertex_bindings.size());
	vertex_input_state_info.pVertexBindingDescriptions = vertex_bindings.data();
	vertex_input_state_info.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertex_attributes.size());
	vertex_input_state_info.pVertexAttributeDescriptions = vertex_attributes.data();

	VkPipelineInputAssemblyStateCreateInfo input_assembly_state_info { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
	create_info.pInputAssemblyState = &input_assembly_state_info;
	input_assembly_state_info.primitiveRestartEnable = VK_FALSE;
	input_assembly_state_info.topology = convert_primitive_topology(desc.graphics.rasterizer_state.topology);

	VkPipelineTessellationStateCreateInfo tessellation_state_info { VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO };
	create_info.pTessellationState = &tessellation_state_info;
	if (input_assembly_state_info.topology == VK_PRIMITIVE_TOPOLOGY_PATCH_LIST)
		tessellation_state_info.patchControlPoints = static_cast<uint32_t>(desc.graphics.rasterizer_state.topology) - static_cast<uint32_t>(api::primitive_topology::patch_list_01_cp);

	VkPipelineViewportStateCreateInfo viewport_state_info { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
	create_info.pViewportState = &viewport_state_info;
	viewport_state_info.scissorCount = desc.graphics.blend_state.num_viewports;
	viewport_state_info.viewportCount = desc.graphics.blend_state.num_viewports;

	VkPipelineRasterizationStateCreateInfo rasterization_state_info { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
	create_info.pRasterizationState = &rasterization_state_info;
	rasterization_state_info.depthClampEnable = !desc.graphics.rasterizer_state.depth_clip;
	rasterization_state_info.rasterizerDiscardEnable = VK_FALSE;
	rasterization_state_info.polygonMode = convert_fill_mode(desc.graphics.rasterizer_state.fill_mode);
	rasterization_state_info.cullMode = convert_cull_mode(desc.graphics.rasterizer_state.cull_mode);
	rasterization_state_info.frontFace = desc.graphics.rasterizer_state.front_counter_clockwise ? VK_FRONT_FACE_COUNTER_CLOCKWISE : VK_FRONT_FACE_CLOCKWISE;
	rasterization_state_info.depthBiasEnable = desc.graphics.rasterizer_state.depth_bias != 0 || desc.graphics.rasterizer_state.depth_bias_clamp != 0 || desc.graphics.rasterizer_state.slope_scaled_depth_bias != 0;
	rasterization_state_info.depthBiasConstantFactor = desc.graphics.rasterizer_state.depth_bias;
	rasterization_state_info.depthBiasClamp = desc.graphics.rasterizer_state.depth_bias_clamp;
	rasterization_state_info.depthBiasSlopeFactor = desc.graphics.rasterizer_state.slope_scaled_depth_bias;
	rasterization_state_info.lineWidth = 1.0f;

	VkPipelineMultisampleStateCreateInfo multisample_state_info { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
	create_info.pMultisampleState = &multisample_state_info;
	multisample_state_info.rasterizationSamples = static_cast<VkSampleCountFlagBits>(desc.graphics.multisample_state.sample_count);
	multisample_state_info.sampleShadingEnable = VK_FALSE;
	multisample_state_info.minSampleShading = 0.0f;
	multisample_state_info.alphaToCoverageEnable = desc.graphics.multisample_state.alpha_to_coverage;
	multisample_state_info.alphaToOneEnable = VK_FALSE;
	multisample_state_info.pSampleMask = &desc.graphics.multisample_state.sample_mask;

	VkPipelineDepthStencilStateCreateInfo depth_stencil_state_info { VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
	create_info.pDepthStencilState = &depth_stencil_state_info;
	depth_stencil_state_info.depthTestEnable = desc.graphics.depth_stencil_state.depth_test;
	depth_stencil_state_info.depthWriteEnable = desc.graphics.depth_stencil_state.depth_write_mask;
	depth_stencil_state_info.depthCompareOp = convert_compare_op(desc.graphics.depth_stencil_state.depth_func);
	depth_stencil_state_info.depthBoundsTestEnable = VK_FALSE;
	depth_stencil_state_info.stencilTestEnable = desc.graphics.depth_stencil_state.stencil_test;
	depth_stencil_state_info.back.failOp = convert_stencil_op(desc.graphics.depth_stencil_state.back_stencil_fail_op);
	depth_stencil_state_info.back.passOp = convert_stencil_op(desc.graphics.depth_stencil_state.back_stencil_pass_op);
	depth_stencil_state_info.back.depthFailOp = convert_stencil_op(desc.graphics.depth_stencil_state.back_stencil_depth_fail_op);
	depth_stencil_state_info.back.compareOp = convert_compare_op(desc.graphics.depth_stencil_state.back_stencil_func);
	depth_stencil_state_info.back.compareMask = desc.graphics.depth_stencil_state.stencil_read_mask;
	depth_stencil_state_info.back.writeMask = desc.graphics.depth_stencil_state.stencil_write_mask;
	depth_stencil_state_info.back.reference = desc.graphics.depth_stencil_state.stencil_reference_value;
	depth_stencil_state_info.front.failOp = convert_stencil_op(desc.graphics.depth_stencil_state.front_stencil_fail_op);
	depth_stencil_state_info.front.passOp = convert_stencil_op(desc.graphics.depth_stencil_state.front_stencil_pass_op);
	depth_stencil_state_info.front.depthFailOp = convert_stencil_op(desc.graphics.depth_stencil_state.front_stencil_depth_fail_op);
	depth_stencil_state_info.front.compareOp = convert_compare_op(desc.graphics.depth_stencil_state.front_stencil_func);
	depth_stencil_state_info.front.compareMask = desc.graphics.depth_stencil_state.stencil_read_mask;
	depth_stencil_state_info.front.writeMask = desc.graphics.depth_stencil_state.stencil_write_mask;
	depth_stencil_state_info.front.reference = desc.graphics.depth_stencil_state.stencil_reference_value;
	depth_stencil_state_info.minDepthBounds = 0.0f;
	depth_stencil_state_info.maxDepthBounds = 1.0f;

	VkPipelineColorBlendAttachmentState attachment_info[8];
	for (uint32_t i = 0; i < 8; ++i)
	{
		attachment_info[i].blendEnable = desc.graphics.blend_state.blend_enable[i];
		attachment_info[i].srcColorBlendFactor = convert_blend_factor(desc.graphics.blend_state.src_color_blend_factor[i]);
		attachment_info[i].dstColorBlendFactor = convert_blend_factor(desc.graphics.blend_state.dst_color_blend_factor[i]);
		attachment_info[i].colorBlendOp = convert_blend_op(desc.graphics.blend_state.color_blend_op[i]);
		attachment_info[i].srcAlphaBlendFactor = convert_blend_factor(desc.graphics.blend_state.src_alpha_blend_factor[i]);
		attachment_info[i].dstAlphaBlendFactor = convert_blend_factor(desc.graphics.blend_state.dst_alpha_blend_factor[i]);
		attachment_info[i].alphaBlendOp = convert_blend_op(desc.graphics.blend_state.alpha_blend_op[i]);
		attachment_info[i].colorWriteMask = desc.graphics.blend_state.render_target_write_mask[i];
	}

	VkPipelineColorBlendStateCreateInfo color_blend_state_info { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
	create_info.pColorBlendState = &color_blend_state_info;
	color_blend_state_info.logicOpEnable = VK_FALSE;
	color_blend_state_info.logicOp = VK_LOGIC_OP_CLEAR;
	color_blend_state_info.attachmentCount = desc.graphics.blend_state.num_render_targets;
	color_blend_state_info.pAttachments = attachment_info;
	color_blend_state_info.blendConstants[0] = ((desc.graphics.blend_state.blend_constant      ) & 0xFF) / 255.0f;
	color_blend_state_info.blendConstants[1] = ((desc.graphics.blend_state.blend_constant >>  4) & 0xFF) / 255.0f;
	color_blend_state_info.blendConstants[2] = ((desc.graphics.blend_state.blend_constant >>  8) & 0xFF) / 255.0f;
	color_blend_state_info.blendConstants[3] = ((desc.graphics.blend_state.blend_constant >> 12) & 0xFF) / 255.0f;

	uint32_t num_attachments = 0;
	VkAttachmentReference attachment_refs[8 + 1] = {};
	VkAttachmentDescription attachment_desc[8 + 1] = {};

	for (uint32_t i = 0; i < desc.graphics.blend_state.num_render_targets && i < 8; ++i)
	{
		attachment_desc[num_attachments].format = convert_format(desc.graphics.blend_state.render_target_format[i]);
		attachment_desc[num_attachments].samples = static_cast<VkSampleCountFlagBits>(desc.graphics.multisample_state.sample_count);
		attachment_desc[num_attachments].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
		attachment_desc[num_attachments].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachment_desc[num_attachments].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachment_desc[num_attachments].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachment_desc[num_attachments].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		attachment_desc[num_attachments].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		attachment_refs[i].attachment = num_attachments++;
		attachment_refs[i].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	}

	if (desc.graphics.depth_stencil_state.depth_stencil_format != api::format::unknown)
	{
		attachment_desc[num_attachments].format = convert_format(desc.graphics.depth_stencil_state.depth_stencil_format);
		attachment_desc[num_attachments].samples = static_cast<VkSampleCountFlagBits>(desc.graphics.multisample_state.sample_count);
		attachment_desc[num_attachments].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
		attachment_desc[num_attachments].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachment_desc[num_attachments].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
		attachment_desc[num_attachments].stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachment_desc[num_attachments].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		attachment_desc[num_attachments].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		attachment_refs[8].attachment = num_attachments++;
		attachment_refs[8].layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	}
	else
	{
		attachment_refs[8].attachment = VK_ATTACHMENT_UNUSED;
		attachment_refs[8].layout = VK_IMAGE_LAYOUT_UNDEFINED;
	}

	VkSubpassDescription subpass_desc = {};
	subpass_desc.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass_desc.colorAttachmentCount = desc.graphics.blend_state.num_render_targets;
	subpass_desc.pColorAttachments = attachment_refs;
	subpass_desc.pDepthStencilAttachment = &attachment_refs[8];

	VkRenderPassCreateInfo render_pass_info { VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
	render_pass_info.attachmentCount = num_attachments;
	render_pass_info.pAttachments = attachment_desc;
	render_pass_info.subpassCount = 1;
	render_pass_info.pSubpasses = &subpass_desc;

	if (VkPipeline object = VK_NULL_HANDLE;
		vk.CreateRenderPass(_orig, &render_pass_info, nullptr, &create_info.renderPass) == VK_SUCCESS &&
		vk.CreateGraphicsPipelines(_orig, VK_NULL_HANDLE, 1, &create_info, nullptr, &object) == VK_SUCCESS)
	{
		vk.DestroyRenderPass(_orig, create_info.renderPass, nullptr);

		*out = { (uint64_t)object };
		return true;
	}
	else
	{
		vk.DestroyRenderPass(_orig, create_info.renderPass, nullptr);

		*out = { 0 };
		return false;
	}
}

bool reshade::vulkan::device_impl::create_shader_module(api::shader_stage, api::shader_format format, const char *entry_point, const void *code, size_t code_size, api::shader_module *out)
{
	if (format != api::shader_format::spirv)
	{
		*out = { 0 };
		return false;
	}

	VkShaderModuleCreateInfo create_info { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
	create_info.codeSize = code_size;
	create_info.pCode = static_cast<const uint32_t *>(code);

	if (VkShaderModule object = VK_NULL_HANDLE;
		vk.CreateShaderModule(_orig, &create_info, nullptr, &object) == VK_SUCCESS)
	{
		*out = { (uint64_t)object };
		return true;
	}
	else
	{
		*out = { 0 };
		return false;
	}
}
bool reshade::vulkan::device_impl::create_pipeline_layout(uint32_t num_table_layouts, const api::descriptor_table_layout *table_layouts, uint32_t num_constant_ranges, const api::constant_range *constant_ranges, api::pipeline_layout *out)
{
	static_assert(sizeof(*table_layouts) == sizeof(VkDescriptorSetLayout));

	std::vector<VkPushConstantRange> push_constant_ranges(num_constant_ranges);
	for (uint32_t i = 0; i < num_constant_ranges; ++i)
	{
		push_constant_ranges[i].stageFlags = static_cast<VkShaderStageFlagBits>(constant_ranges[i].visibility);
		push_constant_ranges[i].offset = constant_ranges[i].offset * 4;
		push_constant_ranges[i].size = constant_ranges[i].count * 4;
	}

	VkPipelineLayoutCreateInfo create_info { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
	create_info.setLayoutCount = num_table_layouts;
	create_info.pSetLayouts = reinterpret_cast<const VkDescriptorSetLayout *>(table_layouts);
	create_info.pushConstantRangeCount = num_constant_ranges;
	create_info.pPushConstantRanges = push_constant_ranges.data();

	if (VkPipelineLayout object = VK_NULL_HANDLE;
		vk.CreatePipelineLayout(_orig, &create_info, nullptr, &object) == VK_SUCCESS)
	{
		*out = { (uint64_t)object };
		return true;
	}
	else
	{
		*out = { 0 };
		return false;
	}
}
bool reshade::vulkan::device_impl::create_descriptor_heap(uint32_t max_tables, uint32_t num_sizes, const api::descriptor_heap_size *sizes, api::descriptor_heap *out)
{
	static_assert(sizeof(*sizes) == sizeof(VkDescriptorPoolSize));

	VkDescriptorPoolCreateInfo create_info { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
	create_info.maxSets = max_tables;
	create_info.poolSizeCount = num_sizes;
	create_info.pPoolSizes = reinterpret_cast<const VkDescriptorPoolSize *>(sizes);

	if (VkDescriptorPool object = VK_NULL_HANDLE;
		vk.CreateDescriptorPool(_orig, &create_info, nullptr, &object) == VK_SUCCESS)
	{
		*out = { (uint64_t)object };
		return true;
	}
	else
	{
		*out = { 0 };
		return false;
	}
}
bool reshade::vulkan::device_impl::create_descriptor_tables(api::descriptor_heap heap, api::descriptor_table_layout layout, uint32_t count, api::descriptor_table *out)
{
	static_assert(sizeof(*out) == sizeof(VkDescriptorSet));

	std::vector<VkDescriptorSetLayout> set_layouts(count, (VkDescriptorSetLayout)layout.handle);

	VkDescriptorSetAllocateInfo alloc_info { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
	alloc_info.descriptorPool = (VkDescriptorPool)heap.handle;
	alloc_info.descriptorSetCount = count;
	alloc_info.pSetLayouts = set_layouts.data();

	if (vk.AllocateDescriptorSets(_orig, &alloc_info, reinterpret_cast<VkDescriptorSet *>(out)) == VK_SUCCESS)
	{
		return true;
	}
	else
	{
		for (uint32_t i = 0; i < count; ++i)
			out[i] = { 0 };
		return false;
	}
}
bool reshade::vulkan::device_impl::create_descriptor_table_layout(uint32_t num_ranges, const api::descriptor_range *ranges, bool push_descriptors, api::descriptor_table_layout *out)
{
	std::vector<VkDescriptorSetLayoutBinding> internal_bindings;
	internal_bindings.reserve(num_ranges);
	for (uint32_t i = 0; i < num_ranges; ++i)
	{
		for (uint32_t k = 0; k < ranges[i].count; ++k)
		{
			VkDescriptorSetLayoutBinding &internal_binding = internal_bindings.emplace_back();
			internal_binding.binding = ranges[i].binding + k;
			internal_binding.descriptorType = static_cast<VkDescriptorType>(ranges[i].type);
			internal_binding.descriptorCount = 1;
			internal_binding.stageFlags = static_cast<VkShaderStageFlags>(ranges[i].visibility);
			internal_binding.pImmutableSamplers = nullptr;
		}
	}

	VkDescriptorSetLayoutCreateInfo set_create_info { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
	set_create_info.bindingCount = static_cast<uint32_t>(internal_bindings.size());
	set_create_info.pBindings = internal_bindings.data();

	if (push_descriptors)
		set_create_info.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR;

	if (VkDescriptorSetLayout object = VK_NULL_HANDLE;
		vk.CreateDescriptorSetLayout(_orig, &set_create_info, nullptr, &object) == VK_SUCCESS)
	{
		*out = { (uint64_t)object };
		return true;
	}
	else
	{
		*out = { 0 };
		return false;
	}
}

void reshade::vulkan::device_impl::destroy_sampler(api::sampler handle)
{
	vk.DestroySampler(_orig, (VkSampler)handle.handle, nullptr);
}
void reshade::vulkan::device_impl::destroy_resource(api::resource handle)
{
	if (handle.handle == 0)
		return;
	const resource_data &data = _resources.at(handle.handle);

	// Can only destroy resources that were allocated via 'create_resource' previously
	assert(data.allocation != nullptr);

	if (data.is_image())
		vmaDestroyImage(_alloc, data.image, data.allocation);
	else
		vmaDestroyBuffer(_alloc, data.buffer, data.allocation);

	_resources.erase(handle.handle);
}
void reshade::vulkan::device_impl::destroy_resource_view(api::resource_view handle)
{
	if (handle.handle == 0)
		return;
	const resource_view_data &data = _views.at(handle.handle);

	if (data.is_image_view())
		vk.DestroyImageView(_orig, data.image_view, nullptr);
	else
		vk.DestroyBufferView(_orig, data.buffer_view, nullptr);

	_views.erase(handle.handle);
}

void reshade::vulkan::device_impl::destroy_pipeline(api::pipeline_type, api::pipeline handle)
{
	vk.DestroyPipeline(_orig, (VkPipeline)handle.handle, nullptr);
}
void reshade::vulkan::device_impl::destroy_shader_module(api::shader_module handle)
{
	vk.DestroyShaderModule(_orig, (VkShaderModule)handle.handle, nullptr);
}
void reshade::vulkan::device_impl::destroy_pipeline_layout(api::pipeline_layout handle)
{
	vk.DestroyPipelineLayout(_orig, (VkPipelineLayout)handle.handle, nullptr);
}
void reshade::vulkan::device_impl::destroy_descriptor_heap(api::descriptor_heap handle)
{
	vk.DestroyDescriptorPool(_orig, (VkDescriptorPool)handle.handle, nullptr);
}
void reshade::vulkan::device_impl::destroy_descriptor_table_layout(api::descriptor_table_layout handle)
{
	vk.DestroyDescriptorSetLayout(_orig, (VkDescriptorSetLayout)handle.handle, nullptr);
}

void reshade::vulkan::device_impl::update_descriptor_tables(uint32_t num_updates, const api::descriptor_update *updates)
{
	std::vector<VkWriteDescriptorSet> writes(num_updates);

	std::vector<VkDescriptorImageInfo> image_info(num_updates);
	std::vector<VkDescriptorBufferInfo> buffer_info(num_updates);

	for (uint32_t i = 0; i < num_updates; ++i)
	{
		writes[i] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
		writes[i].dstSet = (VkDescriptorSet)updates[i].table.handle;
		writes[i].dstBinding = updates[i].binding;
		writes[i].dstArrayElement = 0;
		writes[i].descriptorCount = 1;
		writes[i].descriptorType = static_cast<VkDescriptorType>(updates[i].type);

		if (updates[i].type == api::descriptor_type::constant_buffer)
		{
			writes[i].pBufferInfo = &buffer_info[i];

			buffer_info[i].buffer = (VkBuffer)updates[i].descriptor.resource.handle;
			buffer_info[i].offset = 0;
			buffer_info[i].range = VK_WHOLE_SIZE;
		}
		else
		{
			writes[i].pImageInfo = &image_info[i];

			image_info[i].sampler = (VkSampler)updates[i].descriptor.sampler.handle;
			image_info[i].imageView = (VkImageView)updates[i].descriptor.view.handle;
			image_info[i].imageLayout = updates[i].type == api::descriptor_type::unordered_access_view ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		}
	}

	vk.UpdateDescriptorSets(_orig, num_updates, writes.data(), 0, nullptr);
}

bool reshade::vulkan::device_impl::map_resource(api::resource resource, uint32_t subresource, api::map_access, void **mapped_ptr)
{
	assert(resource.handle != 0);
	const resource_data &data = _resources.at(resource.handle);

	if (data.allocation != nullptr)
	{
		assert(subresource == 0);
		return vmaMapMemory(_alloc, data.allocation, mapped_ptr) == VK_SUCCESS;
	}
	else
	{
		*mapped_ptr = nullptr;
		return false;
	}
}
void reshade::vulkan::device_impl::unmap_resource(api::resource resource, uint32_t subresource)
{
	assert(resource.handle != 0);
	const resource_data &data = _resources.at(resource.handle);

	if (data.allocation != nullptr)
	{
		assert(subresource == 0);
		vmaUnmapMemory(_alloc, data.allocation);
	}
}

void reshade::vulkan::device_impl::upload_buffer_region(api::resource dst, uint64_t dst_offset, const void *data, uint64_t size)
{
	assert(false); // TODO
}
void reshade::vulkan::device_impl::upload_texture_region(api::resource dst, uint32_t dst_subresource, const int32_t dst_box[6], const void *data, uint32_t row_pitch, uint32_t slice_pitch)
{
	assert(false); // TODO
}

void reshade::vulkan::device_impl::get_resource_from_view(api::resource_view view, api::resource *out_resource) const
{
	assert(view.handle != 0);
	const resource_view_data &data = _views.at(view.handle);

	if (data.is_image_view())
		*out_resource = { (uint64_t)data.image_create_info.image };
	else
		*out_resource = { (uint64_t)data.buffer_create_info.buffer };
}

reshade::api::resource_desc reshade::vulkan::device_impl::get_resource_desc(api::resource resource) const
{
	assert(resource.handle != 0);
	const resource_data &data = _resources.at(resource.handle);

	if (data.is_image())
		return convert_resource_desc(data.image_create_info);
	else
		return convert_resource_desc(data.buffer_create_info);
}

void reshade::vulkan::device_impl::wait_idle() const
{
	vk.DeviceWaitIdle(_orig);
}

void reshade::vulkan::device_impl::set_debug_name(api::resource resource, const char *name)
{
	if (vk.DebugMarkerSetObjectNameEXT != nullptr)
	{
		const resource_data &data = _resources.at(resource.handle);

		VkDebugMarkerObjectNameInfoEXT name_info { VK_STRUCTURE_TYPE_DEBUG_MARKER_OBJECT_NAME_INFO_EXT };
		name_info.object = resource.handle;
		name_info.objectType = data.is_image() ? VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT : VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT;
		name_info.pObjectName = name;

		vk.DebugMarkerSetObjectNameEXT(_orig, &name_info);
	}
}

bool reshade::vulkan::device_impl::request_render_pass_and_framebuffer(uint32_t count, const api::resource_view *rtvs, api::resource_view dsv, VkRenderPass &pass, VkFramebuffer &fbo)
{
	size_t hash = 0xFFFFFFFF;
	for (uint32_t i = 0; i < count; ++i)
		hash ^= std::hash<uint64_t>()(rtvs[i].handle);
	if (dsv.handle != 0)
		hash ^= std::hash<uint64_t>()(dsv.handle);

	if (const auto it = _render_pass_list_internal.find(hash);
		it != _render_pass_list_internal.end())
	{
		pass = it->second;
	}
	else
	{
		std::vector<VkAttachmentReference> attachment_refs;
		std::vector<VkAttachmentDescription> attachment_descs;

		for (uint32_t i = 0; i < count; ++i)
		{
			VkAttachmentReference &ref = attachment_refs.emplace_back();
			ref.attachment = i;
			ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

			const auto &rtv_info = _views.at(rtvs[i].handle);
			const auto &rt_resource_info = _resources.at((uint64_t)rtv_info.image_create_info.image);

			VkAttachmentDescription &attach = attachment_descs.emplace_back();
			attach.format = rtv_info.image_create_info.format;
			attach.samples = rt_resource_info.image_create_info.samples;
			attach.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
			attach.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			attach.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			attach.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		}

		if (dsv.handle != 0)
		{
			VkAttachmentReference &ref = attachment_refs.emplace_back();
			ref.attachment = count;
			ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

			const auto &dsv_info = _views.at(dsv.handle);
			const auto &ds_resource_info = _resources.at((uint64_t)dsv_info.image_create_info.image);

			VkAttachmentDescription &attach = attachment_descs.emplace_back();
			attach.format = dsv_info.image_create_info.format;
			attach.samples = ds_resource_info.image_create_info.samples;
			attach.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
			attach.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			attach.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
			attach.stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
			attach.initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
			attach.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		}

		// Synchronize any writes to render targets in previous passes with reads from them in this pass
		VkSubpassDependency subdep = {};
		subdep.srcSubpass = VK_SUBPASS_EXTERNAL;
		subdep.dstSubpass = 0;
		subdep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		subdep.dstStageMask = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;
		subdep.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		subdep.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

		VkSubpassDescription subpass = {};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = count;
		subpass.pColorAttachments = attachment_refs.data();
		subpass.pDepthStencilAttachment = dsv.handle != 0 ? &attachment_refs[count] : nullptr;

		VkRenderPassCreateInfo create_info { VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
		create_info.attachmentCount = static_cast<uint32_t>(attachment_descs.size());
		create_info.pAttachments = attachment_descs.data();
		create_info.subpassCount = 1;
		create_info.pSubpasses = &subpass;
		create_info.dependencyCount = 1;
		create_info.pDependencies = &subdep;

		if (vk.CreateRenderPass(_orig, &create_info, nullptr, &pass) != VK_SUCCESS)
			return false;

		_render_pass_list_internal.emplace(hash, pass);
	}

	if (const auto it = _framebuffer_list_internal.find(hash);
		it != _framebuffer_list_internal.end())
	{
		fbo = it->second;
	}
	else
	{
		const auto &rtv_info = _views.at(count != 0 ? rtvs[0].handle : dsv.handle);
		const auto &rt_resource_info = _resources.at((uint64_t)rtv_info.image_create_info.image);

		std::vector<VkImageView> views;
		views.reserve(count + 1);
		for (uint32_t i = 0; i < count; ++i)
			views.push_back((VkImageView)rtvs[i].handle);
		if (dsv.handle != 0)
			views.push_back((VkImageView)dsv.handle);

		VkFramebufferCreateInfo create_info { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
		create_info.renderPass = pass;
		create_info.attachmentCount = count + (dsv.handle != 0 ? 1 : 0);
		create_info.pAttachments = views.data();
		create_info.width = rt_resource_info.image_create_info.extent.width;
		create_info.height = rt_resource_info.image_create_info.extent.height;
		create_info.layers = rt_resource_info.image_create_info.arrayLayers;

		if (vk.CreateFramebuffer(_orig, &create_info, nullptr, &fbo) != VK_SUCCESS)
			return false;

		_framebuffer_list_internal.emplace(hash, fbo);
	}

	return true;
}

reshade::vulkan::command_list_impl::command_list_impl(device_impl *device, VkCommandBuffer cmd_list) :
	api_object_impl(cmd_list), _device_impl(device), _has_commands(cmd_list != VK_NULL_HANDLE)
{
	if (_has_commands) // Do not call add-on event for immediate command list
		invoke_addon_event<addon_event::init_command_list>(this);
}
reshade::vulkan::command_list_impl::~command_list_impl()
{
	if (_has_commands)
		invoke_addon_event<addon_event::destroy_command_list>(this);
}

void reshade::vulkan::command_list_impl::bind_pipeline(api::pipeline_type type, api::pipeline pipeline)
{
	_device_impl->vk.CmdBindPipeline(_orig,
		type == api::pipeline_type::compute ? VK_PIPELINE_BIND_POINT_COMPUTE : VK_PIPELINE_BIND_POINT_GRAPHICS,
		(VkPipeline)pipeline.handle);
}
void reshade::vulkan::command_list_impl::bind_pipeline_states(uint32_t count, const api::pipeline_state *states, const uint32_t *values)
{
	for (uint32_t i = 0; i < count; ++i)
	{
		switch (states[i])
		{
		case api::pipeline_state::blend_constant:
		{
			float blend_constant[4];
			blend_constant[0] = ((values[i]      ) & 0xFF) / 255.0f;
			blend_constant[1] = ((values[i] >>  4) & 0xFF) / 255.0f;
			blend_constant[2] = ((values[i] >>  8) & 0xFF) / 255.0f;
			blend_constant[3] = ((values[i] >> 12) & 0xFF) / 255.0f;

			_device_impl->vk.CmdSetBlendConstants(_orig, blend_constant);
			break;
		}
		case api::pipeline_state::stencil_read_mask:
			_device_impl->vk.CmdSetStencilCompareMask(_orig, VK_STENCIL_FACE_FRONT_AND_BACK, values[i]);
			break;
		case api::pipeline_state::stencil_write_mask:
			_device_impl->vk.CmdSetStencilWriteMask(_orig, VK_STENCIL_FACE_FRONT_AND_BACK, values[i]);
			break;
		case api::pipeline_state::stencil_reference_value:
			_device_impl->vk.CmdSetStencilReference(_orig, VK_STENCIL_FACE_FRONT_AND_BACK, values[i]);
			break;
		default:
			assert(false);
			break;
		}
	}
}
void reshade::vulkan::command_list_impl::bind_viewports(uint32_t first, uint32_t count, const float *viewports)
{
	_device_impl->vk.CmdSetViewport(_orig, first, count, reinterpret_cast<const VkViewport *>(viewports));
}
void reshade::vulkan::command_list_impl::bind_scissor_rects(uint32_t first, uint32_t count, const int32_t *rects)
{
	const auto rect_data = static_cast<VkRect2D *>(alloca(sizeof(VkRect2D) * count));
	for (uint32_t i = 0, k = 0; i < count; ++i, k += 4)
	{
		rect_data[i].offset.x = rects[k + 0];
		rect_data[i].offset.y = rects[k + 1];
		rect_data[i].extent.width = rects[k + 2] - rects[k + 0];
		rect_data[i].extent.height = rects[k + 3] - rects[k + 1];
	}

	_device_impl->vk.CmdSetScissor(_orig, first, count, rect_data);
}

void reshade::vulkan::command_list_impl::push_constants(api::shader_stage stage, api::pipeline_layout layout, uint32_t, uint32_t offset, uint32_t count, const uint32_t *values)
{
	assert(count != 0);

	_device_impl->vk.CmdPushConstants(_orig,
		(VkPipelineLayout)layout.handle,
		static_cast<VkShaderStageFlags>(stage),
		offset * 4, count * 4, values);
}
void reshade::vulkan::command_list_impl::push_descriptors(api::shader_stage stage, api::pipeline_layout layout, uint32_t layout_index, api::descriptor_type type, uint32_t first, uint32_t count, const void *descriptors)
{
	assert(count != 0);

	VkWriteDescriptorSet write { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
	write.dstBinding = first;
	write.descriptorCount = count;
	write.descriptorType = static_cast<VkDescriptorType>(type);

	std::vector<VkDescriptorImageInfo> image_info(count);
	std::vector<VkDescriptorBufferInfo> buffer_info(count);

	switch (type)
	{
	case api::descriptor_type::sampler:
		for (uint32_t i = 0; i < count; ++i)
		{
			const auto &descriptor = static_cast<const api::sampler *>(descriptors)[i];
			image_info[i].sampler = (VkSampler)descriptor.handle;
		}
		write.pImageInfo = image_info.data();
		break;
	case api::descriptor_type::sampler_with_resource_view:
		for (uint32_t i = 0; i < count; ++i)
		{
			const auto &descriptor = static_cast<const api::sampler_with_resource_view *>(descriptors)[i];
			image_info[i].sampler = (VkSampler)descriptor.sampler.handle;
			image_info[i].imageView = (VkImageView)descriptor.view.handle;
			image_info[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		}
		write.pImageInfo = image_info.data();
		break;
	case api::descriptor_type::shader_resource_view:
		for (uint32_t i = 0; i < count; ++i)
		{
			const auto &descriptor = static_cast<const api::resource_view *>(descriptors)[i];
			image_info[i].imageView = (VkImageView)descriptor.handle;
			image_info[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		}
		write.pImageInfo = image_info.data();
		break;
	case api::descriptor_type::unordered_access_view:
		for (uint32_t i = 0; i < count; ++i)
		{
			const auto &descriptor = static_cast<const api::resource_view *>(descriptors)[i];
			image_info[i].imageView = (VkImageView)descriptor.handle;
			image_info[i].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		}
		write.pImageInfo = image_info.data();
		break;
	case api::descriptor_type::constant_buffer:
		for (uint32_t i = 0; i < count; ++i)
		{
			const auto &descriptor = static_cast<const api::resource *>(descriptors)[i];
			buffer_info[i].buffer = (VkBuffer)descriptor.handle;
			buffer_info[i].offset = 0;
			buffer_info[i].range = VK_WHOLE_SIZE;
		}
		write.pBufferInfo = buffer_info.data();
		break;
	}

	_device_impl->vk.CmdPushDescriptorSetKHR(_orig,
		stage == api::shader_stage::compute ? VK_PIPELINE_BIND_POINT_COMPUTE : VK_PIPELINE_BIND_POINT_GRAPHICS,
		(VkPipelineLayout)layout.handle, layout_index,
		1, &write);
}
void reshade::vulkan::command_list_impl::bind_descriptor_heaps(uint32_t, const api::descriptor_heap *)
{
	// No need to bind descriptor pool in Vulkan
}
void reshade::vulkan::command_list_impl::bind_descriptor_tables(api::pipeline_type type, api::pipeline_layout layout, uint32_t first, uint32_t count, const api::descriptor_table *tables)
{
	_device_impl->vk.CmdBindDescriptorSets(_orig,
		type == api::pipeline_type::compute ? VK_PIPELINE_BIND_POINT_COMPUTE : VK_PIPELINE_BIND_POINT_GRAPHICS,
		(VkPipelineLayout)layout.handle,
		first, count, reinterpret_cast<const VkDescriptorSet *>(tables),
		0, nullptr);
}

void reshade::vulkan::command_list_impl::bind_index_buffer(api::resource buffer, uint64_t offset, uint32_t index_size)
{
	_device_impl->vk.CmdBindIndexBuffer(_orig, (VkBuffer)buffer.handle, offset, index_size == 1 ? VK_INDEX_TYPE_UINT8_EXT : index_size == 2 ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32);
}
void reshade::vulkan::command_list_impl::bind_vertex_buffers(uint32_t first, uint32_t count, const api::resource *buffers, const uint64_t *offsets, const uint32_t *)
{
	_device_impl->vk.CmdBindVertexBuffers(_orig, first, count, reinterpret_cast<const VkBuffer *>(buffers), offsets);
}

void reshade::vulkan::command_list_impl::draw(uint32_t vertices, uint32_t instances, uint32_t first_vertex, uint32_t first_instance)
{
	_has_commands = true;

	_device_impl->vk.CmdDraw(_orig, vertices, instances, first_vertex, first_instance);
}
void reshade::vulkan::command_list_impl::draw_indexed(uint32_t indices, uint32_t instances, uint32_t first_index, int32_t vertex_offset, uint32_t first_instance)
{
	_has_commands = true;

	_device_impl->vk.CmdDrawIndexed(_orig, indices, instances, first_index, vertex_offset, first_instance);
}
void reshade::vulkan::command_list_impl::dispatch(uint32_t num_groups_x, uint32_t num_groups_y, uint32_t num_groups_z)
{
	_has_commands = true;

	_device_impl->vk.CmdDispatch(_orig, num_groups_x, num_groups_y, num_groups_z);
}
void reshade::vulkan::command_list_impl::draw_or_dispatch_indirect(uint32_t type, api::resource buffer, uint64_t offset, uint32_t draw_count, uint32_t stride)
{
	_has_commands = true;

	switch (type)
	{
	case 1:
		_device_impl->vk.CmdDrawIndirect(_orig, (VkBuffer)buffer.handle, offset, draw_count, stride);
		break;
	case 2:
		_device_impl->vk.CmdDrawIndexedIndirect(_orig, (VkBuffer)buffer.handle, offset, draw_count, stride);
		break;
	case 3:
		for (uint32_t i = 0; i < draw_count; ++i)
			_device_impl->vk.CmdDispatchIndirect(_orig, (VkBuffer)buffer.handle, offset + i * stride);
		break;
	}
}

void reshade::vulkan::command_list_impl::begin_render_pass(uint32_t count, const api::resource_view *rtvs, api::resource_view dsv)
{
	_has_commands = true;

	VkRenderPassBeginInfo begin_info { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };

	if (!_device_impl->request_render_pass_and_framebuffer(count, rtvs, dsv, begin_info.renderPass, begin_info.framebuffer))
		return;

	assert(current_renderpass == VK_NULL_HANDLE);

	const auto &rtv_info = _device_impl->_views.at(count != 0 ? rtvs[0].handle : dsv.handle);
	const auto &rt_resource_info = _device_impl->_resources.at((uint64_t)rtv_info.image_create_info.image);

	begin_info.renderArea.extent.width = rt_resource_info.image_create_info.extent.width;
	begin_info.renderArea.extent.height = rt_resource_info.image_create_info.extent.height;

	_device_impl->vk.CmdBeginRenderPass(_orig, &begin_info, VK_SUBPASS_CONTENTS_INLINE);

	current_subpass = 0;
	current_renderpass = begin_info.renderPass;
	current_framebuffer = begin_info.framebuffer;
}
void reshade::vulkan::command_list_impl::end_render_pass()
{
	_device_impl->vk.CmdEndRenderPass(_orig);

	current_renderpass = VK_NULL_HANDLE;
	current_framebuffer = VK_NULL_HANDLE;
}

void reshade::vulkan::command_list_impl::blit(api::resource src, uint32_t src_subresource, const int32_t src_box[6], api::resource dst, uint32_t dst_subresource, const int32_t dst_box[6], api::texture_filter filter)
{
	_has_commands = true;

	const resource_data &src_data = _device_impl->_resources.at(src.handle);
	const resource_data &dst_data = _device_impl->_resources.at(dst.handle);

	assert(src_data.is_image() && dst_data.is_image());

	VkImageBlit region;

	region.srcSubresource = convert_subresource(src_subresource, src_data.image_create_info);
	if (src_box != nullptr)
	{
		std::copy_n(src_box, 6, &region.srcOffsets[0].x);
	}
	else
	{
		region.srcOffsets[0] = { 0, 0, 0 };
		region.srcOffsets[1] = {
			static_cast<int32_t>(std::max(1u, src_data.image_create_info.extent.width >> region.srcSubresource.mipLevel)),
			static_cast<int32_t>(std::max(1u, src_data.image_create_info.extent.height >> region.srcSubresource.mipLevel)),
			static_cast<int32_t>(std::max(1u, src_data.image_create_info.extent.depth >> region.srcSubresource.mipLevel)) };
	}

	region.dstSubresource = convert_subresource(dst_subresource, dst_data.image_create_info);
	if (dst_box != nullptr)
	{
		std::copy_n(dst_box, 6, &region.dstOffsets[0].x);
	}
	else
	{
		region.dstOffsets[0] = { 0, 0, 0 };
		region.dstOffsets[1] = {
			static_cast<int32_t>(std::max(1u, dst_data.image_create_info.extent.width >> region.dstSubresource.mipLevel)),
			static_cast<int32_t>(std::max(1u, dst_data.image_create_info.extent.height >> region.dstSubresource.mipLevel)),
			static_cast<int32_t>(std::max(1u, dst_data.image_create_info.extent.depth >> region.dstSubresource.mipLevel)) };
	}

	_device_impl->vk.CmdBlitImage(_orig,
		(VkImage)src.handle, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		(VkImage)dst.handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		1, &region,
		filter == api::texture_filter::min_mag_mip_linear || filter == api::texture_filter::min_mag_linear_mip_point ? VK_FILTER_LINEAR : VK_FILTER_NEAREST);
}
void reshade::vulkan::command_list_impl::resolve(api::resource src, uint32_t src_subresource, const int32_t src_offset[3], api::resource dst, uint32_t dst_subresource, const int32_t dst_offset[3], const uint32_t size[3], api::format)
{
	_has_commands = true;

	const resource_data &src_data = _device_impl->_resources.at(src.handle);
	const resource_data &dst_data = _device_impl->_resources.at(dst.handle);

	assert(src_data.is_image() && dst_data.is_image());

	VkImageResolve region;

	region.srcSubresource = convert_subresource(src_subresource, src_data.image_create_info);
	if (src_offset != nullptr)
		std::copy_n(src_offset, 3, &region.srcOffset.x);
	else
		region.srcOffset = { 0, 0, 0 };

	region.dstSubresource = convert_subresource(dst_subresource, dst_data.image_create_info);
	if (dst_offset != nullptr)
		std::copy_n(dst_offset, 3, &region.dstOffset.x);
	else
		region.dstOffset = { 0, 0, 0 };

	if (size != nullptr)
	{
		std::copy_n(size, 3, &region.extent.width);
	}
	else
	{
		region.extent.width = std::max(1u, src_data.image_create_info.extent.width >> region.srcSubresource.mipLevel);
		region.extent.height = std::max(1u, src_data.image_create_info.extent.height >> region.srcSubresource.mipLevel);
		region.extent.depth = std::max(1u, src_data.image_create_info.extent.depth >> region.srcSubresource.mipLevel);
	}

	_device_impl->vk.CmdResolveImage(_orig,
		(VkImage)src.handle, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		(VkImage)dst.handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		1, &region);
}
void reshade::vulkan::command_list_impl::copy_resource(api::resource src, api::resource dst)
{
	const api::resource_desc desc = _device_impl->get_resource_desc(src);

	if (desc.type == api::resource_type::buffer)
	{
		copy_buffer_region(src, 0, dst, 0, ~0llu);
	}
	else
	{
		for (uint32_t layer = 0, layers = (desc.type != api::resource_type::texture_3d) ? desc.texture.depth_or_layers : 1; layer < layers; ++layer)
		{
			for (uint32_t level = 0; level < desc.texture.levels; ++level)
			{
				const uint32_t subresource = level + layer * desc.texture.levels;

				copy_texture_region(src, subresource, nullptr, dst, subresource, nullptr, nullptr);
			}
		}
	}
}
void reshade::vulkan::command_list_impl::copy_buffer_region(api::resource src, uint64_t src_offset, api::resource dst, uint64_t dst_offset, uint64_t size)
{
	_has_commands = true;

	VkBufferCopy region;
	region.srcOffset = src_offset;
	region.dstOffset = dst_offset;
	region.size = size;

	_device_impl->vk.CmdCopyBuffer(_orig, (VkBuffer)src.handle, (VkBuffer)dst.handle, 1, &region);
}
void reshade::vulkan::command_list_impl::copy_buffer_to_texture(api::resource src, uint64_t src_offset, uint32_t row_length, uint32_t slice_height, api::resource dst, uint32_t dst_subresource, const int32_t dst_box[6])
{
	_has_commands = true;

	const resource_data &src_data = _device_impl->_resources.at(src.handle);
	const resource_data &dst_data = _device_impl->_resources.at(dst.handle);

	assert(!src_data.is_image() && dst_data.is_image());

	VkBufferImageCopy region;
	region.bufferOffset = src_offset;
	region.bufferRowLength = row_length;
	region.bufferImageHeight = slice_height;

	region.imageSubresource = convert_subresource(dst_subresource, dst_data.image_create_info);
	if (dst_box != nullptr)
	{
		std::copy_n(dst_box, 3, &region.imageOffset.x);
		region.imageExtent.width = dst_box[3] - dst_box[0];
		region.imageExtent.height = dst_box[4] - dst_box[1];
		region.imageExtent.depth = dst_box[5] - dst_box[2];
	}
	else
	{
		region.imageOffset = { 0, 0, 0 };
		region.imageExtent.width = std::max(1u, dst_data.image_create_info.extent.width >> region.imageSubresource.mipLevel);
		region.imageExtent.height = std::max(1u, dst_data.image_create_info.extent.height >> region.imageSubresource.mipLevel);
		region.imageExtent.depth = std::max(1u, dst_data.image_create_info.extent.depth >> region.imageSubresource.mipLevel);
	}

	_device_impl->vk.CmdCopyBufferToImage(_orig, (VkBuffer)src.handle, (VkImage)dst.handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
}
void reshade::vulkan::command_list_impl::copy_texture_region(api::resource src, uint32_t src_subresource, const int32_t src_offset[3], api::resource dst, uint32_t dst_subresource, const int32_t dst_offset[3], const uint32_t size[3])
{
	_has_commands = true;

	const resource_data &src_data = _device_impl->_resources.at(src.handle);
	const resource_data &dst_data = _device_impl->_resources.at(dst.handle);

	assert(src_data.is_image() && dst_data.is_image());

	VkImageCopy region;

	region.srcSubresource = convert_subresource(src_subresource, src_data.image_create_info);
	if (src_offset != nullptr)
		std::copy_n(src_offset, 3, &region.srcOffset.x);
	else
		region.srcOffset = { 0, 0, 0 };

	region.dstSubresource = convert_subresource(dst_subresource, dst_data.image_create_info);
	if (dst_offset != nullptr)
		std::copy_n(dst_offset, 3, &region.dstOffset.x);
	else
		region.dstOffset = { 0, 0, 0 };

	if (size != nullptr)
	{
		std::copy_n(size, 3, &region.extent.width);
	}
	else
	{
		region.extent.width = std::max(1u, src_data.image_create_info.extent.width >> region.srcSubresource.mipLevel);
		region.extent.height = std::max(1u, src_data.image_create_info.extent.height >> region.srcSubresource.mipLevel);
		region.extent.depth = std::max(1u, src_data.image_create_info.extent.depth >> region.srcSubresource.mipLevel);
	}

	_device_impl->vk.CmdCopyImage(_orig, (VkImage)src.handle, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, (VkImage)dst.handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
}
void reshade::vulkan::command_list_impl::copy_texture_to_buffer(api::resource src, uint32_t src_subresource, const int32_t src_box[6], api::resource dst, uint64_t dst_offset, uint32_t row_length, uint32_t slice_height)
{
	_has_commands = true;

	const resource_data &src_data = _device_impl->_resources.at(src.handle);
	const resource_data &dst_data = _device_impl->_resources.at(dst.handle);

	assert(src_data.is_image() && !dst_data.is_image());

	VkBufferImageCopy region;
	region.bufferOffset = dst_offset;
	region.bufferRowLength = row_length;
	region.bufferImageHeight = slice_height;

	region.imageSubresource = convert_subresource(src_subresource, src_data.image_create_info);
	if (src_box != nullptr)
	{
		std::copy_n(src_box, 3, &region.imageOffset.x);
		region.imageExtent.width = src_box[3] - src_box[0];
		region.imageExtent.height = src_box[4] - src_box[1];
		region.imageExtent.depth = src_box[5] - src_box[2];
	}
	else
	{
		region.imageOffset = { 0, 0, 0 };
		region.imageExtent.width = std::max(1u, src_data.image_create_info.extent.width >> region.imageSubresource.mipLevel);
		region.imageExtent.height = std::max(1u, src_data.image_create_info.extent.height >> region.imageSubresource.mipLevel);
		region.imageExtent.depth = std::max(1u, src_data.image_create_info.extent.depth >> region.imageSubresource.mipLevel);
	}

	_device_impl->vk.CmdCopyImageToBuffer(_orig, (VkImage)src.handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, (VkBuffer)dst.handle, 1, &region);
}

void reshade::vulkan::command_list_impl::clear_depth_stencil_view(api::resource_view dsv, uint32_t clear_flags, float depth, uint8_t stencil)
{
	_has_commands = true;

	const resource_view_data &dsv_data = _device_impl->_views.at(dsv.handle);
	assert(dsv_data.is_image_view()); // Has to be an image

	VkImageAspectFlags aspect_flags = 0;
	if ((clear_flags & 0x1) != 0)
		aspect_flags |= VK_IMAGE_ASPECT_DEPTH_BIT;
	if ((clear_flags & 0x2) != 0)
		aspect_flags |= VK_IMAGE_ASPECT_STENCIL_BIT;

	// Transition state to VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL (since it will be in 'resource_usage::depth_stencil_write' at this point)
	VkImageMemoryBarrier transition { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
	transition.oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	transition.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	transition.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	transition.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	transition.image = dsv_data.image_create_info.image;
	transition.subresourceRange = { aspect_flags, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS };
	_device_impl->vk.CmdPipelineBarrier(_orig, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &transition);

	const VkClearDepthStencilValue clear_value = { depth, stencil };
	_device_impl->vk.CmdClearDepthStencilImage(_orig, dsv_data.image_create_info.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clear_value, 1, &transition.subresourceRange);

	std::swap(transition.oldLayout, transition.newLayout);
	_device_impl->vk.CmdPipelineBarrier(_orig, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &transition);
}
void reshade::vulkan::command_list_impl::clear_render_target_views(uint32_t count, const api::resource_view *rtvs, const float color[4])
{
	_has_commands = true;

	for (uint32_t i = 0; i < count; ++i)
	{
		const resource_view_data &rtv_data = _device_impl->_views.at(rtvs[i].handle);
		assert(rtv_data.is_image_view()); // Has to be an image

		// Transition state to VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL (since it will be in 'resource_usage::render_target' at this point)
		VkImageMemoryBarrier transition { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
		transition.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		transition.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		transition.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		transition.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		transition.image = rtv_data.image_create_info.image;
		transition.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS };
		_device_impl->vk.CmdPipelineBarrier(_orig, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &transition);

		VkClearColorValue clear_value;
		std::memcpy(clear_value.float32, color, 4 * sizeof(float));

		_device_impl->vk.CmdClearColorImage(_orig, rtv_data.image_create_info.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clear_value, 1, &transition.subresourceRange);

		std::swap(transition.oldLayout, transition.newLayout);
		_device_impl->vk.CmdPipelineBarrier(_orig, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &transition);
	}
}
void reshade::vulkan::command_list_impl::clear_unordered_access_view_uint(api::resource_view uav, const uint32_t values[4])
{
	_has_commands = true;

	const resource_view_data &uav_data = _device_impl->_views.at(uav.handle);
	assert(uav_data.is_image_view()); // Has to be an image

	const VkImageSubresourceRange range = { VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS };

	VkClearColorValue clear_value;
	std::memcpy(clear_value.uint32, values, 4 * sizeof(uint32_t));

	_device_impl->vk.CmdClearColorImage(_orig, uav_data.image_create_info.image, VK_IMAGE_LAYOUT_GENERAL, &clear_value, 1, &range);
}
void reshade::vulkan::command_list_impl::clear_unordered_access_view_float(api::resource_view uav, const float values[4])
{
	_has_commands = true;

	const resource_view_data &uav_data = _device_impl->_views.at(uav.handle);
	assert(uav_data.is_image_view()); // Has to be an image

	const VkImageSubresourceRange range = { VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS };

	VkClearColorValue clear_value;
	std::memcpy(clear_value.float32, values, 4 * sizeof(float));

	_device_impl->vk.CmdClearColorImage(_orig, uav_data.image_create_info.image, VK_IMAGE_LAYOUT_GENERAL, &clear_value, 1, &range);
}

void reshade::vulkan::command_list_impl::insert_barrier(uint32_t count, const api::resource *resources, const api::resource_usage *old_states, const api::resource_usage *new_states)
{
	_has_commands = true;

	std::vector<VkImageMemoryBarrier> image_barriers;
	image_barriers.reserve(count);
	std::vector<VkBufferMemoryBarrier> buffer_barriers;
	buffer_barriers.reserve(count);

	VkPipelineStageFlags src_stage_mask = 0;
	VkPipelineStageFlags dst_stage_mask = 0;

	for (uint32_t i = 0; i < count; ++i)
	{
		const resource_data &data = _device_impl->_resources.at(resources[i].handle);

		if (data.is_image())
		{
			VkImageMemoryBarrier &transition = image_barriers.emplace_back();
			transition = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
			transition.srcAccessMask = convert_usage_to_access(old_states[i]);
			transition.dstAccessMask = convert_usage_to_access(new_states[i]);
			transition.oldLayout = convert_usage_to_image_layout(old_states[i]);
			transition.newLayout = convert_usage_to_image_layout(new_states[i]);
			transition.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			transition.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			transition.image = data.image;
			transition.subresourceRange = { aspect_flags_from_format(data.image_create_info.format), 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS };
		}
		else
		{
			VkBufferMemoryBarrier &transition = buffer_barriers.emplace_back();
			transition = { VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER };
			transition.srcAccessMask = convert_usage_to_access(old_states[i]);
			transition.dstAccessMask = convert_usage_to_access(new_states[i]);
			transition.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			transition.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			transition.buffer = data.buffer;
			transition.offset = 0;
			transition.size = VK_WHOLE_SIZE;
		}

		src_stage_mask |= convert_usage_to_pipeline_stage(old_states[i]);
		dst_stage_mask |= convert_usage_to_pipeline_stage(new_states[i]);
	}

	_device_impl->vk.CmdPipelineBarrier(_orig, src_stage_mask, dst_stage_mask, 0, 0, nullptr, static_cast<uint32_t>(buffer_barriers.size()), buffer_barriers.data(), static_cast<uint32_t>(image_barriers.size()), image_barriers.data());
}

void reshade::vulkan::command_list_impl::begin_debug_event(const char *label, const float color[4])
{
	if (_device_impl->vk.CmdDebugMarkerBeginEXT == nullptr)
		return;

	VkDebugMarkerMarkerInfoEXT marker_info { VK_STRUCTURE_TYPE_DEBUG_MARKER_MARKER_INFO_EXT };
	marker_info.pMarkerName = label;

	// The optional color value is ignored if all elements are set to zero
	if (color != nullptr)
	{
		marker_info.color[0] = color[0];
		marker_info.color[1] = color[1];
		marker_info.color[2] = color[2];
		marker_info.color[3] = color[3];
	}

	_device_impl->vk.CmdDebugMarkerBeginEXT(_orig, &marker_info);
}
void reshade::vulkan::command_list_impl::end_debug_event()
{
	if (_device_impl->vk.CmdDebugMarkerEndEXT == nullptr)
		return;

	_device_impl->vk.CmdDebugMarkerEndEXT(_orig);
}
void reshade::vulkan::command_list_impl::insert_debug_marker(const char *label, const float color[4])
{
	if (_device_impl->vk.CmdDebugMarkerInsertEXT == nullptr)
		return;

	VkDebugMarkerMarkerInfoEXT marker_info { VK_STRUCTURE_TYPE_DEBUG_MARKER_MARKER_INFO_EXT };
	marker_info.pMarkerName = label;

	if (color != nullptr)
	{
		marker_info.color[0] = color[0];
		marker_info.color[1] = color[1];
		marker_info.color[2] = color[2];
		marker_info.color[3] = color[3];
	}

	_device_impl->vk.CmdDebugMarkerInsertEXT(_orig, &marker_info);
}

reshade::vulkan::command_list_immediate_impl::command_list_immediate_impl(device_impl *device, uint32_t queue_family_index) :
	command_list_impl(device, VK_NULL_HANDLE)
{
	{	VkCommandPoolCreateInfo create_info { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
		create_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		create_info.queueFamilyIndex = queue_family_index;

		if (_device_impl->vk.CreateCommandPool(_device_impl->_orig, &create_info, nullptr, &_cmd_pool) != VK_SUCCESS)
			return;
	}

	{   VkCommandBufferAllocateInfo alloc_info { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
		alloc_info.commandPool = _cmd_pool;
		alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		alloc_info.commandBufferCount = NUM_COMMAND_FRAMES;

		if (_device_impl->vk.AllocateCommandBuffers(_device_impl->_orig, &alloc_info, _cmd_buffers) != VK_SUCCESS)
			return;
	}

	for (uint32_t i = 0; i < NUM_COMMAND_FRAMES; ++i)
	{
		// The validation layers expect the loader to have set the dispatch pointer, but this does not happen when calling down the layer chain from here, so fix it
		*reinterpret_cast<void **>(_cmd_buffers[i]) = *reinterpret_cast<void **>(device->_orig);

		VkFenceCreateInfo create_info { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
		create_info.flags = VK_FENCE_CREATE_SIGNALED_BIT; // Create signaled so waiting on it when no commands where submitted succeeds

		if (_device_impl->vk.CreateFence(_device_impl->_orig, &create_info, nullptr, &_cmd_fences[i]) != VK_SUCCESS)
			return;

		VkSemaphoreCreateInfo sem_create_info { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };

		if (_device_impl->vk.CreateSemaphore(_device_impl->_orig, &sem_create_info, nullptr, &_cmd_semaphores[i]) != VK_SUCCESS)
			return;
	}

	// Command buffer is in an invalid state and ready for a reset
	VkCommandBufferBeginInfo begin_info { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	if (_device_impl->vk.BeginCommandBuffer(_cmd_buffers[_cmd_index], &begin_info) != VK_SUCCESS)
		return;

	// Command buffer is now in the recording state
	_orig = _cmd_buffers[_cmd_index];
}
reshade::vulkan::command_list_immediate_impl::~command_list_immediate_impl()
{
	for (VkFence fence : _cmd_fences)
		_device_impl->vk.DestroyFence(_device_impl->_orig, fence, nullptr);
	for (VkSemaphore semaphore : _cmd_semaphores)
		_device_impl->vk.DestroySemaphore(_device_impl->_orig, semaphore, nullptr);

	_device_impl->vk.FreeCommandBuffers(_device_impl->_orig, _cmd_pool, NUM_COMMAND_FRAMES, _cmd_buffers);
	_device_impl->vk.DestroyCommandPool(_device_impl->_orig, _cmd_pool, nullptr);

	// Signal to 'command_list_impl' destructor that this is an immediate command list
	_has_commands = false;
}

bool reshade::vulkan::command_list_immediate_impl::flush(VkQueue queue, std::vector<VkSemaphore> &wait_semaphores)
{
	if (!_has_commands)
		return true;

	// Submit all asynchronous commands in one batch to the current queue
	if (_device_impl->vk.EndCommandBuffer(_cmd_buffers[_cmd_index]) != VK_SUCCESS)
		return false;

	VkSubmitInfo submit_info { VK_STRUCTURE_TYPE_SUBMIT_INFO };
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &_cmd_buffers[_cmd_index];

	std::vector<VkPipelineStageFlags> wait_stages(wait_semaphores.size(), VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
	if (!wait_semaphores.empty())
	{
		submit_info.waitSemaphoreCount = static_cast<uint32_t>(wait_semaphores.size());
		submit_info.pWaitSemaphores = wait_semaphores.data();
		submit_info.pWaitDstStageMask = wait_stages.data();
		submit_info.signalSemaphoreCount = 1;
		submit_info.pSignalSemaphores = &_cmd_semaphores[_cmd_index];
	}

	// Only reset fence before an actual submit which can signal it again
	_device_impl->vk.ResetFences(_device_impl->_orig, 1, &_cmd_fences[_cmd_index]);

	if (_device_impl->vk.QueueSubmit(queue, 1, &submit_info, _cmd_fences[_cmd_index]) != VK_SUCCESS)
		return false;

	// This queue submit now waits on the requested wait semaphores
	// The next queue submit should therefore wait on the semaphore that was signaled by this submit
	wait_semaphores.clear();
	wait_semaphores.push_back(_cmd_semaphores[_cmd_index]);

	// Continue with next command buffer now that the current one was submitted
	_cmd_index = (_cmd_index + 1) % NUM_COMMAND_FRAMES;

	// Make sure the next command buffer has finished executing before reusing it this frame
	if (_device_impl->vk.GetFenceStatus(_device_impl->_orig, _cmd_fences[_cmd_index]) == VK_NOT_READY)
	{
		_device_impl->vk.WaitForFences(_device_impl->_orig, 1, &_cmd_fences[_cmd_index], VK_TRUE, UINT64_MAX);
	}

	// Command buffer is now ready for a reset
	VkCommandBufferBeginInfo begin_info { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	if (_device_impl->vk.BeginCommandBuffer(_cmd_buffers[_cmd_index], &begin_info) != VK_SUCCESS)
		return false;

	// Command buffer is now in the recording state
	_orig = _cmd_buffers[_cmd_index];
	return true;
}
bool reshade::vulkan::command_list_immediate_impl::flush_and_wait(VkQueue queue)
{
	// Index is updated during flush below, so keep track of the current one to wait on
	const uint32_t cmd_index_to_wait_on = _cmd_index;

	std::vector<VkSemaphore> wait_semaphores; // No semaphores to wait on
	if (!flush(queue, wait_semaphores))
		return false;

	// Wait for the submitted work to finish and reset fence again for next use
	return _device_impl->vk.WaitForFences(_device_impl->_orig, 1, &_cmd_fences[cmd_index_to_wait_on], VK_TRUE, UINT64_MAX) == VK_SUCCESS;
}

reshade::vulkan::command_queue_impl::command_queue_impl(device_impl *device, uint32_t queue_family_index, const VkQueueFamilyProperties &queue_family, VkQueue queue) :
	api_object_impl(queue), _device_impl(device)
{
	// Register queue to device
	_device_impl->_queues.push_back(this);

	// Only create an immediate command list for graphics queues (since the implemented commands do not work on other queue types)
	if ((queue_family.queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0)
	{
		_immediate_cmd_list = new command_list_immediate_impl(device, queue_family_index);
	}

	invoke_addon_event<addon_event::init_command_queue>(this);
}
reshade::vulkan::command_queue_impl::~command_queue_impl()
{
	invoke_addon_event<addon_event::destroy_command_queue>(this);

	delete _immediate_cmd_list;

	// Unregister queue from device
	_device_impl->_queues.erase(std::find(_device_impl->_queues.begin(), _device_impl->_queues.end(), this));
}

void reshade::vulkan::command_queue_impl::flush_immediate_command_list() const
{
	std::vector<VkSemaphore> wait_semaphores; // No semaphores to wait on
	if (_immediate_cmd_list != nullptr)
		_immediate_cmd_list->flush(_orig, wait_semaphores);
}
