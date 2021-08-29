/*
 * Copyright (C) 2021 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "dll_log.hpp"
#include "vulkan_impl_device.hpp"
#include "vulkan_impl_command_queue.hpp"
#include "vulkan_impl_type_convert.hpp"
#include <algorithm>

#define vk _dispatch_table

inline VkImageAspectFlags aspect_flags_from_format(VkFormat format)
{
	if (format >= VK_FORMAT_D16_UNORM && format <= VK_FORMAT_D32_SFLOAT)
		return VK_IMAGE_ASPECT_DEPTH_BIT;
	if (format == VK_FORMAT_S8_UINT)
		return VK_IMAGE_ASPECT_STENCIL_BIT;
	if (format >= VK_FORMAT_D16_UNORM_S8_UINT && format <= VK_FORMAT_D32_SFLOAT_S8_UINT)
		return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
	return VK_IMAGE_ASPECT_COLOR_BIT;
}

reshade::vulkan::device_impl::device_impl(VkDevice device, VkPhysicalDevice physical_device, const VkLayerInstanceDispatchTable &instance_table, const VkLayerDispatchTable &device_table, const VkPhysicalDeviceFeatures &enabled_features) :
	api_object_impl(device), _physical_device(physical_device), _dispatch_table(device_table), _instance_dispatch_table(instance_table), _enabled_features(enabled_features)
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
		// The effect runtime runs in a single thread, so no synchronization necessary
		create_info.flags = VMA_ALLOCATOR_CREATE_EXTERNALLY_SYNCHRONIZED_BIT;
		create_info.physicalDevice = physical_device;
		create_info.device = device;
		create_info.preferredLargeHeapBlockSize = 1920 * 1080 * 4 * 16; // Allocate blocks of memory that can comfortably contain 16 Full HD images
		create_info.pVulkanFunctions = &functions;
		create_info.vulkanApiVersion = VK_API_VERSION_1_1; // Vulkan 1.1 is guaranteed by code in vulkan_hooks_instance.cpp

		vmaCreateAllocator(&create_info, &_alloc);
	}

	const VkDescriptorPoolSize pool_sizes[] = {
		{ VK_DESCRIPTOR_TYPE_SAMPLER, 128 },
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1024 },
		{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1024 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 512 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 128 }
	};

	{	VkDescriptorPoolCreateInfo create_info { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
		create_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
		create_info.maxSets = 512;
		create_info.poolSizeCount = 5;
		create_info.pPoolSizes = pool_sizes;

		if (vk.CreateDescriptorPool(_orig, &create_info, nullptr, &_descriptor_pool) != VK_SUCCESS)
		{
			LOG(ERROR) << "Failed to create descriptor pool!";
		}
	}

	if (vk.CmdPushDescriptorSetKHR == nullptr)
	{
		for (uint32_t i = 0; i < 4; ++i)
		{
			VkDescriptorPoolCreateInfo create_info { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
			create_info.maxSets = 32;
			create_info.poolSizeCount = 5;
			create_info.pPoolSizes = pool_sizes;

			if (vk.CreateDescriptorPool(_orig, &create_info, nullptr, &_transient_descriptor_pool[i]) != VK_SUCCESS)
			{
				LOG(ERROR) << "Failed to create transient descriptor pool!";
			}
		}
	}

#if RESHADE_ADDON
	load_addons();

	invoke_addon_event<reshade::addon_event::init_device>(this);
#endif
}
reshade::vulkan::device_impl::~device_impl()
{
	assert(_queues.empty()); // All queues should have been unregistered and destroyed at this point

#if RESHADE_ADDON
	invoke_addon_event<reshade::addon_event::destroy_device>(this);

	unload_addons();
#endif

	vk.DestroyDescriptorPool(_orig, _descriptor_pool, nullptr);
	for (uint32_t i = 0; i < 4; ++i)
		vk.DestroyDescriptorPool(_orig, _transient_descriptor_pool[i], nullptr);

	vmaDestroyAllocator(_alloc);
}

bool reshade::vulkan::device_impl::check_capability(api::device_caps capability) const
{
	switch (capability)
	{
	case api::device_caps::compute_shader:
		return true;
	case api::device_caps::geometry_shader:
		return _enabled_features.geometryShader;
	case api::device_caps::hull_and_domain_shader:
		return _enabled_features.tessellationShader;
	case api::device_caps::logic_op:
		return _enabled_features.logicOp;
	case api::device_caps::dual_src_blend:
		return _enabled_features.dualSrcBlend;
	case api::device_caps::independent_blend:
		return _enabled_features.independentBlend;
	case api::device_caps::fill_mode_non_solid:
		return _enabled_features.fillModeNonSolid;
	case api::device_caps::bind_render_targets_and_depth_stencil:
		return false;
	case api::device_caps::multi_viewport:
		return _enabled_features.multiViewport;
	case api::device_caps::partial_push_constant_updates:
		return true;
	case api::device_caps::partial_push_descriptor_updates:
		return vk.CmdPushDescriptorSetKHR != nullptr;
	case api::device_caps::draw_instanced:
		return true;
	case api::device_caps::draw_or_dispatch_indirect:
		// Technically this only specifies whether multi-draw indirect is supported, not draw indirect as a whole
		return _enabled_features.multiDrawIndirect;
	case api::device_caps::copy_buffer_region:
	case api::device_caps::copy_buffer_to_texture:
	case api::device_caps::blit:
	case api::device_caps::resolve_region:
	case api::device_caps::copy_query_pool_results:
	case api::device_caps::sampler_compare:
		return true;
	case api::device_caps::sampler_anisotropic:
		return _enabled_features.samplerAnisotropy;
	case api::device_caps::sampler_with_resource_view:
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

	VkFormatProperties props;
	props.optimalTilingFeatures = 0;
	_instance_dispatch_table.GetPhysicalDeviceFormatProperties(_physical_device, vk_format, &props);

	if ((usage & api::resource_usage::depth_stencil) != api::resource_usage::undefined &&
		(props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) == 0)
		return false;
	if ((usage & api::resource_usage::render_target) != api::resource_usage::undefined &&
		(props.optimalTilingFeatures & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT) == 0)
		return false;
	if ((usage & api::resource_usage::shader_resource) != api::resource_usage::undefined &&
		(props.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) == 0)
		return false;
	if ((usage & api::resource_usage::unordered_access) != api::resource_usage::undefined &&
		(props.optimalTilingFeatures & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT) == 0)
		return false;

	if ((usage & (api::resource_usage::copy_dest | api::resource_usage::resolve_dest)) != api::resource_usage::undefined &&
		(props.optimalTilingFeatures & VK_FORMAT_FEATURE_TRANSFER_DST_BIT) == 0)
		return false;
	if ((usage & (api::resource_usage::copy_source | api::resource_usage::resolve_source)) != api::resource_usage::undefined &&
		(props.optimalTilingFeatures & VK_FORMAT_FEATURE_TRANSFER_SRC_BIT) == 0)
		return false;

	return true;
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
void reshade::vulkan::device_impl::destroy_sampler(api::sampler handle)
{
	vk.DestroySampler(_orig, (VkSampler)handle.handle, nullptr);
}

bool reshade::vulkan::device_impl::create_resource(const api::resource_desc &desc, const api::subresource_data *initial_data, api::resource_usage initial_state, api::resource *out)
{
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

			// Initial data upload requires the buffer to be transferable to
			if (create_info.usage == 0 || initial_data != nullptr)
				create_info.usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;

			if (VkBuffer object = VK_NULL_HANDLE;
				(desc.heap == api::memory_heap::unknown ?
				 vk.CreateBuffer(_orig, &create_info, nullptr, &object) :
				 vmaCreateBuffer(_alloc, &create_info, &alloc_info, &object, &allocation, nullptr)) == VK_SUCCESS)
			{
				resource_data data;
				data.allocation = allocation;
				data.buffer_create_info = create_info;

				register_object<resource_data>((uint64_t)object, std::move(data));

				*out = { (uint64_t)object };

				if (initial_data != nullptr)
				{
					upload_buffer_region(initial_data->data, *out, 0, desc.buffer.size);
				}
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

			// Initial data upload requires the image to be transferable to
			if (create_info.usage == 0 || initial_data != nullptr)
				create_info.usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;

			if (VkImage object = VK_NULL_HANDLE;
				(desc.heap == api::memory_heap::unknown ?
				 vk.CreateImage(_orig, &create_info, nullptr, &object) :
				 vmaCreateImage(_alloc, &create_info, &alloc_info, &object, &allocation, nullptr)) == VK_SUCCESS)
			{
				resource_data data;
				data.allocation = allocation;
				data.image_create_info = create_info;

				register_object<resource_data>((uint64_t)object, std::move(data));

				*out = { (uint64_t)object };

				if (initial_data != nullptr)
				{
					// Only makes sense to upload initial data if it is not thrown away on the first layout transition
					assert(initial_state != api::resource_usage::undefined);
				}

				if (initial_state != api::resource_usage::undefined)
				{
					// Transition resource into the initial state using the first available immediate command list
					for (command_queue_impl *const queue : _queues)
					{
						const auto immediate_command_list = static_cast<command_list_immediate_impl *>(queue->get_immediate_command_list());
						if (immediate_command_list != nullptr)
						{
							if (initial_data != nullptr)
							{
								const api::resource_usage states_upload[2] = { api::resource_usage::undefined, api::resource_usage::copy_dest };
								immediate_command_list->barrier(1, out, &states_upload[0], &states_upload[1]);

								for (uint32_t subresource = 0; subresource < static_cast<uint32_t>(desc.texture.depth_or_layers) * desc.texture.levels; ++subresource)
									upload_texture_region(initial_data[subresource], *out, subresource, nullptr);

								const api::resource_usage states_finalize[2] = { api::resource_usage::copy_dest, initial_state };
								immediate_command_list->barrier(1, out, &states_finalize[0], &states_finalize[1]);
							}
							else
							{
								const api::resource_usage states_finalize[2] = { api::resource_usage::undefined, initial_state };
								immediate_command_list->barrier(1, out, &states_finalize[0], &states_finalize[1]);
							}

							queue->flush_immediate_command_list();
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
void reshade::vulkan::device_impl::destroy_resource(api::resource handle)
{
	if (handle.handle == 0)
		return;

	const resource_data data = get_native_object_data<resource_data>(handle.handle);

	unregister_object<resource_data>(handle.handle);

	if (data.allocation == VK_NULL_HANDLE)
	{
		if (data.is_image())
			vk.DestroyImage(_orig, (VkImage)handle.handle, nullptr);
		else
			vk.DestroyBuffer(_orig, (VkBuffer)handle.handle, nullptr);
	}
	else
	{
		if (data.is_image())
			vmaDestroyImage(_alloc, (VkImage)handle.handle, data.allocation);
		else
			vmaDestroyBuffer(_alloc, (VkBuffer)handle.handle, data.allocation);
	}
}

bool reshade::vulkan::device_impl::create_resource_view(api::resource resource, api::resource_usage usage_type, const api::resource_view_desc &desc, api::resource_view *out)
{
	if (resource.handle == 0)
	{
		*out = { 0 };
		return false;
	}

	const resource_data data = get_native_object_data<resource_data>(resource.handle);

	if (data.is_image())
	{
		VkImageViewCreateInfo create_info { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
		convert_resource_view_desc(desc, create_info);
		create_info.image = (VkImage)resource.handle;
		create_info.subresourceRange.aspectMask = aspect_flags_from_format(create_info.format);

		if (desc.format == api::format::a8_unorm)
			create_info.components = { VK_COMPONENT_SWIZZLE_ZERO, VK_COMPONENT_SWIZZLE_ZERO, VK_COMPONENT_SWIZZLE_ZERO, VK_COMPONENT_SWIZZLE_R };
		else if (
			desc.format == api::format::r8g8b8x8_unorm || desc.format == api::format::r8g8b8x8_unorm_srgb ||
			desc.format == api::format::b8g8r8x8_unorm || desc.format == api::format::b8g8r8x8_unorm_srgb || desc.format == api::format::b5g5r5x1_unorm)
			create_info.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_ONE };
		else
			create_info.components = { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY };

		// Shader resource views can never access stencil data (except for the explicit formats that do that), so remove that aspect flag for views created with a format that supports stencil
		if (desc.format == api::format::x24_unorm_g8_uint || desc.format == api::format::x32_float_g8_uint)
			create_info.subresourceRange.aspectMask &= ~VK_IMAGE_ASPECT_DEPTH_BIT;
		else if ((usage_type & api::resource_usage::shader_resource) != api::resource_usage::undefined)
			create_info.subresourceRange.aspectMask &= ~VK_IMAGE_ASPECT_STENCIL_BIT;

		VkImageView image_view = VK_NULL_HANDLE;
		if (vk.CreateImageView(_orig, &create_info, nullptr, &image_view) == VK_SUCCESS)
		{
			resource_view_data view_data;
			view_data.image_create_info = create_info;

			register_object<resource_view_data>((uint64_t)image_view, std::move(view_data));

			*out = { (uint64_t)image_view };
			return true;
		}
	}
	else
	{
		VkBufferViewCreateInfo create_info { VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO };
		convert_resource_view_desc(desc, create_info);
		create_info.buffer = (VkBuffer)resource.handle;

		VkBufferView buffer_view = VK_NULL_HANDLE;
		if (vk.CreateBufferView(_orig, &create_info, nullptr, &buffer_view) == VK_SUCCESS)
		{
			resource_view_data view_data;
			view_data.buffer_create_info = create_info;

			register_object<resource_view_data>((uint64_t)buffer_view, std::move(view_data));

			*out = { (uint64_t)buffer_view };
			return true;
		}
	}

	*out = { 0 };
	return false;
}
void reshade::vulkan::device_impl::destroy_resource_view(api::resource_view handle)
{
	if (handle.handle == 0)
		return;

	const resource_view_data data = get_native_object_data<resource_view_data>(handle.handle);

	unregister_object<resource_view_data>(handle.handle);

	if (data.is_image_view())
		vk.DestroyImageView(_orig, (VkImageView)handle.handle, nullptr);
	else
		vk.DestroyBufferView(_orig, (VkBufferView)handle.handle, nullptr);
}

bool reshade::vulkan::device_impl::create_shader_module(VkShaderStageFlagBits stage, const api::shader_desc &desc, VkPipelineShaderStageCreateInfo &stage_info, VkSpecializationInfo &spec_info, std::vector<VkSpecializationMapEntry> &spec_map)
{
	spec_map.reserve(desc.num_spec_constants);
	for (uint32_t i = 0; i < desc.num_spec_constants; ++i)
		spec_map.push_back(VkSpecializationMapEntry { desc.spec_constant_ids[i], i * sizeof(uint32_t), sizeof(uint32_t) });

	spec_info.mapEntryCount = desc.num_spec_constants;
	spec_info.pMapEntries = spec_map.data();
	spec_info.dataSize = desc.num_spec_constants * sizeof(uint32_t);
	spec_info.pData = desc.spec_constant_values;

	stage_info = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
	stage_info.stage = stage;
	stage_info.pName = desc.entry_point;
	stage_info.pSpecializationInfo = &spec_info;

	VkShaderModuleCreateInfo create_info { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
	create_info.codeSize = desc.code_size;
	create_info.pCode = static_cast<const uint32_t *>(desc.code);

	return vk.CreateShaderModule(_orig, &create_info, nullptr, &stage_info.module) == VK_SUCCESS;
}

bool reshade::vulkan::device_impl::create_pipeline(const api::pipeline_desc &desc, api::pipeline *out)
{
	switch (desc.type)
	{
	default:
		*out = { 0 };
		return false;
	case api::pipeline_stage::all_compute:
		return create_compute_pipeline(desc, out);
	case api::pipeline_stage::all_graphics:
		return create_graphics_pipeline(desc, out);
	}
}
bool reshade::vulkan::device_impl::create_compute_pipeline(const api::pipeline_desc &desc, api::pipeline *out)
{
	VkComputePipelineCreateInfo create_info { VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };
	create_info.layout = (VkPipelineLayout)desc.layout.handle;

	VkSpecializationInfo spec_info;
	std::vector<VkSpecializationMapEntry> spec_map;

	if (desc.compute.shader.code_size != 0)
	{
		if (!create_shader_module(VK_SHADER_STAGE_COMPUTE_BIT, desc.compute.shader, create_info.stage, spec_info, spec_map))
			goto exit_failure;
	}

	if (VkPipeline object = VK_NULL_HANDLE;
		vk.CreateComputePipelines(_orig, VK_NULL_HANDLE, 1, &create_info, nullptr, &object) == VK_SUCCESS)
	{
		vk.DestroyShaderModule(_orig, create_info.stage.module, nullptr);

		*out = { (uint64_t)object };
		return true;
	}

exit_failure:
	vk.DestroyShaderModule(_orig, create_info.stage.module, nullptr);

	*out = { 0 };
	return false;
}
bool reshade::vulkan::device_impl::create_graphics_pipeline(const api::pipeline_desc &desc, api::pipeline *out)
{
	if (desc.graphics.render_pass_template.handle == 0)
	{
		*out = { 0 };
		return false;
	}

	VkGraphicsPipelineCreateInfo create_info { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
	create_info.layout = (VkPipelineLayout)desc.layout.handle;

	VkPipelineShaderStageCreateInfo shader_stage_info[6];
	create_info.pStages = shader_stage_info;

	VkSpecializationInfo spec_info[6];
	std::vector<VkSpecializationMapEntry> spec_map[6];

	if (desc.graphics.vertex_shader.code_size != 0)
	{
		if (!create_shader_module(VK_SHADER_STAGE_VERTEX_BIT, desc.graphics.vertex_shader,
			shader_stage_info[create_info.stageCount], spec_info[create_info.stageCount], spec_map[create_info.stageCount]))
			goto exit_failure;
		create_info.stageCount++;
	}
	if (desc.graphics.hull_shader.code_size != 0)
	{
		if (!create_shader_module(VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT, desc.graphics.hull_shader,
			shader_stage_info[create_info.stageCount], spec_info[create_info.stageCount], spec_map[create_info.stageCount]))
			goto exit_failure;
		create_info.stageCount++;
	}
	if (desc.graphics.domain_shader.code_size != 0)
	{
		if (!create_shader_module(VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, desc.graphics.domain_shader,
			shader_stage_info[create_info.stageCount], spec_info[create_info.stageCount], spec_map[create_info.stageCount]))
			goto exit_failure;
		create_info.stageCount++;
	}
	if (desc.graphics.geometry_shader.code_size != 0)
	{
		if (!create_shader_module(VK_SHADER_STAGE_GEOMETRY_BIT, desc.graphics.geometry_shader,
			shader_stage_info[create_info.stageCount], spec_info[create_info.stageCount], spec_map[create_info.stageCount]))
			goto exit_failure;
		create_info.stageCount++;
	}
	if (desc.graphics.pixel_shader.code_size != 0)
	{
		if (!create_shader_module(VK_SHADER_STAGE_FRAGMENT_BIT, desc.graphics.pixel_shader,
			shader_stage_info[create_info.stageCount], spec_info[create_info.stageCount], spec_map[create_info.stageCount]))
			goto exit_failure;
		create_info.stageCount++;
	}

	{
		std::vector<VkDynamicState> dyn_states;
		dyn_states.reserve(2 + ARRAYSIZE(desc.graphics.dynamic_states));
		// Always make scissor rectangles and viewports dynamic
		dyn_states.push_back(VK_DYNAMIC_STATE_SCISSOR);
		dyn_states.push_back(VK_DYNAMIC_STATE_VIEWPORT);

		for (uint32_t i = 0; i < ARRAYSIZE(desc.graphics.dynamic_states) && desc.graphics.dynamic_states[i] != api::dynamic_state::unknown; ++i)
		{
			switch (desc.graphics.dynamic_states[i])
			{
			case api::dynamic_state::blend_constant:
				dyn_states.push_back(VK_DYNAMIC_STATE_BLEND_CONSTANTS);
				break;
			case api::dynamic_state::stencil_read_mask:
				dyn_states.push_back(VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK);
				break;
			case api::dynamic_state::stencil_write_mask:
				dyn_states.push_back(VK_DYNAMIC_STATE_STENCIL_WRITE_MASK);
				break;
			case api::dynamic_state::stencil_reference_value:
				dyn_states.push_back(VK_DYNAMIC_STATE_STENCIL_REFERENCE);
				break;
			default:
				goto exit_failure;
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
			const api::input_layout_element &element = desc.graphics.input_layout[i];

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
					goto exit_failure;
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
		input_assembly_state_info.topology = convert_primitive_topology(desc.graphics.topology);

		VkPipelineTessellationStateCreateInfo tessellation_state_info { VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO };
		create_info.pTessellationState = &tessellation_state_info;
		if (input_assembly_state_info.topology == VK_PRIMITIVE_TOPOLOGY_PATCH_LIST)
			tessellation_state_info.patchControlPoints = static_cast<uint32_t>(desc.graphics.topology) - static_cast<uint32_t>(api::primitive_topology::patch_list_01_cp) + 1;

		VkPipelineViewportStateCreateInfo viewport_state_info { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
		create_info.pViewportState = &viewport_state_info;
		viewport_state_info.scissorCount = desc.graphics.viewport_count;
		viewport_state_info.viewportCount = desc.graphics.viewport_count;

		VkPipelineRasterizationStateCreateInfo rasterization_state_info { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
		create_info.pRasterizationState = &rasterization_state_info;
		rasterization_state_info.depthClampEnable = !desc.graphics.rasterizer_state.depth_clip_enable;
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
		multisample_state_info.rasterizationSamples = static_cast<VkSampleCountFlagBits>(desc.graphics.sample_count);
		multisample_state_info.sampleShadingEnable = VK_FALSE;
		multisample_state_info.minSampleShading = 0.0f;
		multisample_state_info.alphaToCoverageEnable = desc.graphics.blend_state.alpha_to_coverage_enable;
		multisample_state_info.alphaToOneEnable = VK_FALSE;
		multisample_state_info.pSampleMask = &desc.graphics.sample_mask;

		VkPipelineDepthStencilStateCreateInfo depth_stencil_state_info { VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
		create_info.pDepthStencilState = &depth_stencil_state_info;
		depth_stencil_state_info.depthTestEnable = desc.graphics.depth_stencil_state.depth_enable;
		depth_stencil_state_info.depthWriteEnable = desc.graphics.depth_stencil_state.depth_write_mask;
		depth_stencil_state_info.depthCompareOp = convert_compare_op(desc.graphics.depth_stencil_state.depth_func);
		depth_stencil_state_info.depthBoundsTestEnable = VK_FALSE;
		depth_stencil_state_info.stencilTestEnable = desc.graphics.depth_stencil_state.stencil_enable;
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

		uint32_t num_color_attachments = 0;
		for (const auto &attachment : get_native_object_data<render_pass_data>(desc.graphics.render_pass_template.handle).attachments)
			if (attachment.format_flags == VK_IMAGE_ASPECT_COLOR_BIT)
				num_color_attachments++;

		VkPipelineColorBlendStateCreateInfo color_blend_state_info { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
		create_info.pColorBlendState = &color_blend_state_info;
		color_blend_state_info.logicOpEnable = desc.graphics.blend_state.logic_op_enable[0];
		color_blend_state_info.logicOp = convert_logic_op(desc.graphics.blend_state.logic_op[0]);
		color_blend_state_info.attachmentCount = num_color_attachments;
		color_blend_state_info.pAttachments = attachment_info;
		color_blend_state_info.blendConstants[0] = ((desc.graphics.blend_state.blend_constant) & 0xFF) / 255.0f;
		color_blend_state_info.blendConstants[1] = ((desc.graphics.blend_state.blend_constant >> 4) & 0xFF) / 255.0f;
		color_blend_state_info.blendConstants[2] = ((desc.graphics.blend_state.blend_constant >> 8) & 0xFF) / 255.0f;
		color_blend_state_info.blendConstants[3] = ((desc.graphics.blend_state.blend_constant >> 12) & 0xFF) / 255.0f;

		create_info.renderPass = (VkRenderPass)desc.graphics.render_pass_template.handle;

		if (VkPipeline object = VK_NULL_HANDLE;
			vk.CreateGraphicsPipelines(_orig, VK_NULL_HANDLE, 1, &create_info, nullptr, &object) == VK_SUCCESS)
		{
			for (uint32_t stage_index = 0; stage_index < create_info.stageCount; ++stage_index)
				vk.DestroyShaderModule(_orig, create_info.pStages[stage_index].module, nullptr);

			*out = { (uint64_t)object };
			return true;
		}
	}

exit_failure:
	for (uint32_t stage_index = 0; stage_index < create_info.stageCount; ++stage_index)
		vk.DestroyShaderModule(_orig, create_info.pStages[stage_index].module, nullptr);

	*out = { 0 };
	return false;
}
void reshade::vulkan::device_impl::destroy_pipeline(api::pipeline_stage, api::pipeline handle)
{
	vk.DestroyPipeline(_orig, (VkPipeline)handle.handle, nullptr);
}

bool reshade::vulkan::device_impl::create_pipeline_layout(uint32_t count, const api::pipeline_layout_param *params, api::pipeline_layout *out)
{
	pipeline_layout_data data;
	data.desc.assign(params, params + count);

	std::vector<VkPushConstantRange> push_constant_ranges;
	std::vector<VkDescriptorSetLayout> set_layouts;
	set_layouts.reserve(count);
	push_constant_ranges.reserve(count);

	uint32_t i = 0;

	// Push constant ranges have to be at the end of the layout description
	for (; i < count && params[i].type != api::pipeline_layout_param_type::push_constants; ++i)
	{
		set_layouts.push_back((VkDescriptorSetLayout)params[i].descriptor_layout.handle);
	}

	for (; i < count && params[i].type == api::pipeline_layout_param_type::push_constants; ++i)
	{
		VkPushConstantRange &push_constant_range = push_constant_ranges.emplace_back();
		push_constant_range.stageFlags = static_cast<VkShaderStageFlagBits>(params[i].push_constants.visibility);
		push_constant_range.offset = params[i].push_constants.offset * 4;
		push_constant_range.size = params[i].push_constants.count * 4;
	}

	if (i < count)
	{
		*out = { 0 };
		return false;
	}

	VkPipelineLayoutCreateInfo create_info { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
	create_info.setLayoutCount = static_cast<uint32_t>(set_layouts.size());
	create_info.pSetLayouts =  set_layouts.data();
	create_info.pushConstantRangeCount = static_cast<uint32_t>(push_constant_ranges.size());
	create_info.pPushConstantRanges = push_constant_ranges.data();

	if (VkPipelineLayout object = VK_NULL_HANDLE;
		vk.CreatePipelineLayout(_orig, &create_info, nullptr, &object) == VK_SUCCESS)
	{
		register_object<pipeline_layout_data>((uint64_t)object, std::move(data));

		*out = { (uint64_t)object };
		return true;
	}
	else
	{
		*out = { 0 };
		return false;
	}
}
void reshade::vulkan::device_impl::destroy_pipeline_layout(api::pipeline_layout handle)
{
	unregister_object<pipeline_layout_data>(handle.handle);

	vk.DestroyPipelineLayout(_orig, (VkPipelineLayout)handle.handle, nullptr);
}

bool reshade::vulkan::device_impl::create_descriptor_set_layout(uint32_t count, const api::descriptor_range *bindings, bool push_descriptors, api::descriptor_set_layout *out)
{
	descriptor_set_layout_data data;
	data.desc.assign(bindings, bindings + count);
	data.binding_to_offset.reserve(count);

	std::vector<VkDescriptorSetLayoutBinding> internal_bindings(count);

	for (uint32_t i = 0; i < count; ++i)
	{
		const api::descriptor_range &range = bindings[i];

		data.binding_to_offset[range.binding] = range.offset;

		internal_bindings[i].binding = range.binding;
		internal_bindings[i].descriptorType = static_cast<VkDescriptorType>(range.type);
		internal_bindings[i].descriptorCount = range.array_size;
		internal_bindings[i].stageFlags = static_cast<VkShaderStageFlags>(range.visibility);
	}

	VkDescriptorSetLayoutCreateInfo create_info { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
	create_info.bindingCount = count;
	create_info.pBindings = internal_bindings.data();

	if (push_descriptors && vk.CmdPushDescriptorSetKHR != nullptr)
		create_info.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR;

	if (VkDescriptorSetLayout object = VK_NULL_HANDLE;
		vk.CreateDescriptorSetLayout(_orig, &create_info, nullptr, &object) == VK_SUCCESS)
	{
		register_object<descriptor_set_layout_data>((uint64_t)object, std::move(data));

		*out = { (uint64_t)object };
		return true;
	}
	else
	{
		*out = { 0 };
		return false;
	}
}
void reshade::vulkan::device_impl::destroy_descriptor_set_layout(api::descriptor_set_layout handle)
{
	unregister_object<descriptor_set_layout_data>(handle.handle);

	vk.DestroyDescriptorSetLayout(_orig, (VkDescriptorSetLayout)handle.handle, nullptr);
}

bool reshade::vulkan::device_impl::create_query_pool(api::query_type type, uint32_t count, api::query_pool *out)
{
	VkQueryPoolCreateInfo create_info { VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO };
	create_info.queryType = convert_query_type(type);
	create_info.queryCount = count;

	if (type == api::query_type::pipeline_statistics)
	{
		create_info.pipelineStatistics =
			VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_VERTICES_BIT |
			VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT |
			VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT |
			VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_INVOCATIONS_BIT |
			VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_PRIMITIVES_BIT |
			VK_QUERY_PIPELINE_STATISTIC_CLIPPING_INVOCATIONS_BIT |
			VK_QUERY_PIPELINE_STATISTIC_CLIPPING_PRIMITIVES_BIT |
			VK_QUERY_PIPELINE_STATISTIC_FRAGMENT_SHADER_INVOCATIONS_BIT |
			VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_CONTROL_SHADER_PATCHES_BIT |
			VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_EVALUATION_SHADER_INVOCATIONS_BIT |
			VK_QUERY_PIPELINE_STATISTIC_COMPUTE_SHADER_INVOCATIONS_BIT;
	}

	if (VkQueryPool pool = VK_NULL_HANDLE;
		vk.CreateQueryPool(_orig, &create_info, nullptr, &pool) == VK_SUCCESS)
	{
		// Reset all queries for initial use
#if 0
		vk.ResetQueryPool(_orig, pool, 0, count);
#else
		for (command_queue_impl *const queue : _queues)
		{
			const auto immediate_command_list = static_cast<command_list_immediate_impl *>(queue->get_immediate_command_list());
			if (immediate_command_list != nullptr)
			{
				vk.CmdResetQueryPool(immediate_command_list->_orig, pool, 0, count);

				immediate_command_list->_has_commands = true;
				queue->flush_immediate_command_list();
				break;
			}
		}
#endif

		*out = { (uint64_t)pool };
		return true;
	}
	else
	{
		*out = { 0 };
		return false;
	}
}
void reshade::vulkan::device_impl::destroy_query_pool(api::query_pool handle)
{
	vk.DestroyQueryPool(_orig, (VkQueryPool)handle.handle, nullptr);
}

bool reshade::vulkan::device_impl::create_render_pass(const api::render_pass_desc &desc, api::render_pass *out)
{
	render_pass_data data;
	data.attachments.reserve(8 + 1);

	uint32_t num_color_attachments = 0;
	std::vector<VkAttachmentReference> attachment_refs;
	attachment_refs.reserve(8 + 1);
	std::vector<VkAttachmentDescription> attachment_descs;
	attachment_descs.reserve(8 + 1);

	for (uint32_t i = 0; i < 8 && desc.render_targets_format[i] != api::format::unknown; ++i, ++num_color_attachments)
	{
		VkAttachmentReference &ref = attachment_refs.emplace_back();
		ref.attachment = i;
		ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkAttachmentDescription &attach = attachment_descs.emplace_back();
		attach.format = convert_format(desc.render_targets_format[i]);
		attach.samples = static_cast<VkSampleCountFlagBits>(desc.samples);
		attach.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
		attach.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attach.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		attach.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		data.attachments.push_back({ attach.initialLayout, 0, aspect_flags_from_format(attach.format) });
	}

	if (desc.depth_stencil_format != api::format::unknown)
	{
		VkAttachmentReference &ref = attachment_refs.emplace_back();
		ref.attachment = num_color_attachments;
		ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		VkAttachmentDescription &attach = attachment_descs.emplace_back();
		attach.format = convert_format(desc.depth_stencil_format);
		attach.samples = static_cast<VkSampleCountFlagBits>(desc.samples);
		attach.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
		attach.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attach.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
		attach.stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
		attach.initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		attach.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		data.attachments.push_back({ attach.initialLayout, 0, aspect_flags_from_format(attach.format) });
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
	subpass.colorAttachmentCount = num_color_attachments;
	subpass.pColorAttachments = attachment_refs.data();
	subpass.pDepthStencilAttachment = desc.depth_stencil_format != api::format::unknown ? &attachment_refs.back() : nullptr;

	VkRenderPassCreateInfo create_info { VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
	create_info.attachmentCount = static_cast<uint32_t>(attachment_descs.size());
	create_info.pAttachments = attachment_descs.data();
	create_info.subpassCount = 1;
	create_info.pSubpasses = &subpass;
	create_info.dependencyCount = 1;
	create_info.pDependencies = &subdep;

	if (VkRenderPass object = VK_NULL_HANDLE;
		vk.CreateRenderPass(_orig, &create_info, nullptr, &object) == VK_SUCCESS)
	{
		register_object<render_pass_data>((uint64_t)object, std::move(data));

		*out = { (uint64_t)object };
		return true;
	}
	else
	{
		*out = { 0 };
		return false;
	}
}
void reshade::vulkan::device_impl::destroy_render_pass(api::render_pass handle)
{
	unregister_object<render_pass_data>(handle.handle);

	vk.DestroyRenderPass(_orig, (VkRenderPass)handle.handle, nullptr);
}

bool reshade::vulkan::device_impl::create_framebuffer(const api::framebuffer_desc &desc, api::framebuffer *out)
{
	framebuffer_data data;
	data.attachments.reserve(8 + 1);
	data.attachment_types.reserve(8 + 1);

	uint32_t width = std::numeric_limits<uint32_t>::max();
	uint32_t height = std::numeric_limits<uint32_t>::max();
	uint32_t num_layers = std::numeric_limits<uint32_t>::max();

	for (uint32_t i = 0; i < 8 && desc.render_targets[i].handle != 0; ++i)
	{
		const auto rtv_data = get_native_object_data<resource_view_data>(desc.render_targets[i].handle);
		const auto rt_resource_data = get_native_object_data<resource_data>((uint64_t)rtv_data.image_create_info.image);

		width = std::min(rt_resource_data.image_create_info.extent.width, width);
		height = std::min(rt_resource_data.image_create_info.extent.height, height);
		num_layers = std::min(rt_resource_data.image_create_info.arrayLayers, num_layers);

		data.attachments.push_back((VkImageView)desc.render_targets[i].handle);
		data.attachment_types.push_back(VK_IMAGE_ASPECT_COLOR_BIT);
	}

	if (desc.depth_stencil.handle != 0)
	{
		const auto dsv_data = get_native_object_data<resource_view_data>(desc.depth_stencil.handle);
		const auto ds_resource_data = get_native_object_data<resource_data>((uint64_t)dsv_data.image_create_info.image);

		width = std::min(ds_resource_data.image_create_info.extent.width, width);
		height = std::min(ds_resource_data.image_create_info.extent.height, height);
		num_layers = std::min(ds_resource_data.image_create_info.arrayLayers, num_layers);

		data.attachments.push_back((VkImageView)desc.depth_stencil.handle);
		data.attachment_types.push_back(aspect_flags_from_format(ds_resource_data.image_create_info.format));
	}

	VkFramebufferCreateInfo create_info { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
	create_info.renderPass = (VkRenderPass)desc.render_pass_template.handle;
	create_info.attachmentCount = static_cast<uint32_t>(data.attachments.size());
	create_info.pAttachments = data.attachments.data();
	create_info.width = width;
	create_info.height = height;
	create_info.layers = num_layers;

	data.area.width = width;
	data.area.height = height;

	if (VkFramebuffer object = VK_NULL_HANDLE;
		vk.CreateFramebuffer(_orig, &create_info, nullptr, &object) == VK_SUCCESS)
	{
		register_object<framebuffer_data>((uint64_t)object, std::move(data));

		*out = { (uint64_t)object };
		return true;
	}
	else
	{
		*out = { 0 };
		return false;
	}
}
void reshade::vulkan::device_impl::destroy_framebuffer(api::framebuffer handle)
{
	unregister_object<framebuffer_data>(handle.handle);

	vk.DestroyFramebuffer(_orig, (VkFramebuffer)handle.handle, nullptr);
}

bool reshade::vulkan::device_impl::map_resource(api::resource resource, uint32_t subresource, api::map_access, api::subresource_data *out_data)
{
	assert(out_data != nullptr);

	const resource_data res_data = get_native_object_data<resource_data>(resource.handle);

	if (res_data.allocation != nullptr)
	{
		assert(subresource == 0);

		if (vmaMapMemory(_alloc, res_data.allocation, &out_data->data) == VK_SUCCESS)
		{
			out_data->row_pitch = static_cast<uint32_t>(res_data.allocation->GetSize());
			out_data->slice_pitch = out_data->row_pitch;
			return true;
		}
	}

	out_data->data = nullptr;
	out_data->row_pitch = 0;
	out_data->slice_pitch = 0;
	return false;
}
void reshade::vulkan::device_impl::unmap_resource(api::resource resource, uint32_t subresource)
{
	const resource_data res_data = get_native_object_data<resource_data>(resource.handle);

	if (res_data.allocation != nullptr)
	{
		assert(subresource == 0);
		vmaUnmapMemory(_alloc, res_data.allocation);
	}
}

void reshade::vulkan::device_impl::upload_buffer_region(const void *data, api::resource dst, uint64_t dst_offset, uint64_t size)
{
	assert(dst.handle != 0);
	assert(!_queues.empty());

	for (command_queue_impl *const queue : _queues)
	{
		const auto immediate_command_list = static_cast<command_list_immediate_impl *>(queue->get_immediate_command_list());
		if (immediate_command_list != nullptr)
		{
			immediate_command_list->_has_commands = true;

			vk.CmdUpdateBuffer(immediate_command_list->_orig, (VkBuffer)dst.handle, dst_offset, size, data);

			immediate_command_list->flush_and_wait((VkQueue)queue->get_native_object());
			break;
		}
	}
}
void reshade::vulkan::device_impl::upload_texture_region(const api::subresource_data &data, api::resource dst, uint32_t dst_subresource, const int32_t dst_box[6])
{
	assert(!_queues.empty());

	const resource_data dst_data = get_native_object_data<resource_data>(dst.handle);
	assert(dst_data.is_image());

	VkExtent3D extent = dst_data.image_create_info.extent;
	extent.depth *= dst_data.image_create_info.arrayLayers;

	if (dst_box != nullptr)
	{
		extent.width  = dst_box[3] - dst_box[0];
		extent.height = dst_box[4] - dst_box[1];
		extent.depth  = dst_box[5] - dst_box[2];
	}

	const auto row_size_packed = extent.width * api::format_bytes_per_pixel(convert_format(dst_data.image_create_info.format));
	const auto slice_size_packed = extent.height * row_size_packed;
	const auto total_size = extent.depth * slice_size_packed;

	// Allocate host memory for upload
	VkBuffer intermediate = VK_NULL_HANDLE;
	VmaAllocation intermediate_mem = VK_NULL_HANDLE;

	{   VkBufferCreateInfo create_info { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
		create_info.size = total_size;
		create_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

		VmaAllocationCreateInfo alloc_info = {};
		alloc_info.usage = VMA_MEMORY_USAGE_CPU_ONLY;

		if (vmaCreateBuffer(_alloc, &create_info, &alloc_info, &intermediate, &intermediate_mem, nullptr) != VK_SUCCESS)
		{
			LOG(ERROR) << "Failed to create upload buffer!";
			LOG(DEBUG) << "> Details: Width = " << create_info.size;
			return;
		}
	}

	// Fill upload buffer with pixel data
	uint8_t *mapped_data = nullptr;
	if (vmaMapMemory(_alloc, intermediate_mem, reinterpret_cast<void **>(&mapped_data)) == VK_SUCCESS)
	{
		if ((row_size_packed == data.row_pitch || extent.height == 1) &&
			(slice_size_packed == data.slice_pitch || extent.depth == 1))
		{
			std::memcpy(mapped_data, data.data, total_size);
		}
		else
		{
			for (uint32_t z = 0; z < extent.depth; ++z)
				for (uint32_t y = 0; y < extent.height; ++y, mapped_data += row_size_packed)
					std::memcpy(mapped_data, static_cast<const uint8_t *>(data.data) + z * data.slice_pitch + y * data.row_pitch, row_size_packed);
		}

		vmaUnmapMemory(_alloc, intermediate_mem);

		// Copy data from upload buffer into target texture using the first available immediate command list
		for (command_queue_impl *const queue : _queues)
		{
			const auto immediate_command_list = static_cast<command_list_immediate_impl *>(queue->get_immediate_command_list());
			if (immediate_command_list != nullptr)
			{
				immediate_command_list->copy_buffer_to_texture({ (uint64_t)intermediate }, 0, 0, 0, dst, dst_subresource, dst_box);

				// Wait for command to finish executing before destroying the upload buffer
				immediate_command_list->flush_and_wait((VkQueue)queue->get_native_object());
				break;
			}
		}
	}

	vmaDestroyBuffer(_alloc, intermediate, intermediate_mem);
}

bool reshade::vulkan::device_impl::get_query_pool_results(api::query_pool pool, uint32_t first, uint32_t count, void *results, uint32_t stride)
{
	assert(pool.handle != 0);
	assert(stride >= sizeof(uint64_t));

	return vk.GetQueryPoolResults(_orig, (VkQueryPool)pool.handle, first, count, count * stride, results, stride, VK_QUERY_RESULT_64_BIT) == VK_SUCCESS;
}

bool reshade::vulkan::device_impl::allocate_descriptor_sets(uint32_t count, const api::descriptor_set_layout *layouts, api::descriptor_set *out)
{
	static_assert(sizeof(*layouts) == sizeof(VkDescriptorSetLayout) && sizeof(*out) == sizeof(VkDescriptorSet));

	VkDescriptorSetAllocateInfo alloc_info { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
	alloc_info.descriptorPool = _descriptor_pool;
	alloc_info.descriptorSetCount = count;
	alloc_info.pSetLayouts = reinterpret_cast<const VkDescriptorSetLayout *>(layouts);

	if (vk.AllocateDescriptorSets(_orig, &alloc_info, reinterpret_cast<VkDescriptorSet *>(out)) == VK_SUCCESS)
	{
		for (uint32_t i = 0; i < count; ++i)
			register_object<descriptor_set_data>(out[i].handle, { (VkDescriptorSetLayout)layouts[i].handle });

		return true;
	}
	else
	{
		return false;
	}
}
void reshade::vulkan::device_impl::free_descriptor_sets(uint32_t count, const api::descriptor_set_layout *, const api::descriptor_set *sets)
{
	for (uint32_t i = 0; i < count; ++i)
		unregister_object<descriptor_set_data>(sets[i].handle);

	vk.FreeDescriptorSets(_orig, _descriptor_pool, count, reinterpret_cast<const VkDescriptorSet *>(sets));
}
void reshade::vulkan::device_impl::update_descriptor_sets(uint32_t count, const api::write_descriptor_set *updates)
{
	std::vector<VkWriteDescriptorSet> writes_internal(count);

	uint32_t max_descriptors = 0;
	for (uint32_t i = 0; i < count; ++i)
		max_descriptors += updates[i].count;
	std::vector<VkDescriptorImageInfo> image_info(max_descriptors);

	for (uint32_t i = 0, j = 0; i < count; ++i)
	{
		const api::write_descriptor_set &update = updates[i];

		writes_internal[i] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
		writes_internal[i].dstSet = (VkDescriptorSet)update.set.handle;
		get_native_object_data<descriptor_set_layout_data>((uint64_t)get_native_object_data<descriptor_set_data>(update.set.handle).layout).calc_binding_from_offset(update.offset, writes_internal[i].dstBinding, writes_internal[i].dstArrayElement);
		writes_internal[i].descriptorCount = update.count;
		writes_internal[i].descriptorType = static_cast<VkDescriptorType>(update.type);

		switch (update.type)
		{
		case api::descriptor_type::sampler:
			writes_internal[i].pImageInfo = image_info.data() + j;
			for (uint32_t k = 0; k < update.count; ++k, ++j)
			{
				image_info[j].sampler = (VkSampler)static_cast<const api::sampler *>(update.descriptors)[k].handle;
			}
			break;
		case api::descriptor_type::sampler_with_resource_view:
			writes_internal[i].pImageInfo = image_info.data() + j;
			for (uint32_t k = 0; k < update.count; ++k, ++j)
			{
				image_info[j].sampler = (VkSampler)static_cast<const api::sampler_with_resource_view *>(update.descriptors)[k].sampler.handle;
				image_info[j].imageView = (VkImageView)static_cast<const api::sampler_with_resource_view *>(update.descriptors)[k].view.handle;
				image_info[j].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			}
			break;
		case api::descriptor_type::shader_resource_view:
		case api::descriptor_type::unordered_access_view:
			writes_internal[i].pImageInfo = image_info.data() + j;
			for (uint32_t k = 0; k < update.count; ++k, ++j)
			{
				image_info[j].imageView = (VkImageView)static_cast<const api::resource_view *>(update.descriptors)[k].handle;
				image_info[j].imageLayout = update.type == api::descriptor_type::unordered_access_view ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			}
			break;
		case api::descriptor_type::constant_buffer:
			writes_internal[i].pBufferInfo = static_cast<const VkDescriptorBufferInfo *>(update.descriptors);
			break;
		}
	}

	vk.UpdateDescriptorSets(_orig, count, writes_internal.data(), 0, nullptr);
}

void reshade::vulkan::device_impl::wait_idle() const
{
	vk.DeviceWaitIdle(_orig);

	// Make sure any pending work gets executed here, so it is not enqueued later (at which point the referenced objects may have been destroyed by the code calling this)
	// Do this after waiting for idle, since it should run after all work by the application is done and is synchronous anyway
	for (command_queue_impl *const queue : _queues)
	{
		const auto immediate_command_list = static_cast<command_list_immediate_impl *>(queue->get_immediate_command_list());
		if (immediate_command_list != nullptr)
			immediate_command_list->flush_and_wait(queue->_orig);
	}

#ifndef NDEBUG
	_wait_for_idle_happened = true;
#endif
}

void reshade::vulkan::device_impl::set_resource_name(api::resource resource, const char *name)
{
	if (vk.SetDebugUtilsObjectNameEXT == nullptr)
		return;

	const resource_data data = get_native_object_data<resource_data>(resource.handle);

	VkDebugUtilsObjectNameInfoEXT name_info { VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT };
	name_info.objectType = data.is_image() ? VK_OBJECT_TYPE_IMAGE : VK_OBJECT_TYPE_BUFFER;
	name_info.objectHandle = resource.handle;
	name_info.pObjectName = name;

	vk.SetDebugUtilsObjectNameEXT(_orig, &name_info);
}

void reshade::vulkan::device_impl::get_pipeline_layout_desc(api::pipeline_layout layout, uint32_t *count, api::pipeline_layout_param *params) const
{
	assert(count != nullptr);

	const pipeline_layout_data data = get_native_object_data<pipeline_layout_data>(layout.handle);

	if (params != nullptr)
	{
		*count = std::min(*count, static_cast<uint32_t>(data.desc.size()));
		std::memcpy(params, data.desc.data(), *count * sizeof(api::pipeline_layout_param));
	}
	else
	{
		*count = static_cast<uint32_t>(data.desc.size());
	}
}
void reshade::vulkan::device_impl::get_descriptor_set_layout_desc(api::descriptor_set_layout layout, uint32_t *count, api::descriptor_range *bindings) const
{
	assert(count != nullptr);

	const descriptor_set_layout_data data = get_native_object_data<descriptor_set_layout_data>(layout.handle);

	if (bindings != nullptr)
	{
		*count = std::min(*count, static_cast<uint32_t>(data.desc.size()));
		std::memcpy(bindings, data.desc.data(), *count * sizeof(api::descriptor_range));
	}
	else
	{
		*count = static_cast<uint32_t>(data.desc.size());
	}
}

reshade::api::resource_desc reshade::vulkan::device_impl::get_resource_desc(api::resource resource) const
{
	const resource_data data = get_native_object_data<resource_data>(resource.handle);

	if (data.is_image())
		return convert_resource_desc(data.image_create_info);
	else
		return convert_resource_desc(data.buffer_create_info);
}

void reshade::vulkan::device_impl::get_resource_from_view(api::resource_view view, api::resource *out) const
{
	const resource_view_data data = get_native_object_data<resource_view_data>(view.handle);

	if (data.is_image_view())
		*out = { (uint64_t)data.image_create_info.image };
	else
		*out = { (uint64_t)data.buffer_create_info.buffer };
}

bool reshade::vulkan::device_impl::get_framebuffer_attachment(api::framebuffer fbo, api::attachment_type type, uint32_t index, api::resource_view *out) const
{
	assert(fbo.handle != 0);

	const framebuffer_data data = get_native_object_data<framebuffer_data>(fbo.handle);
	assert(index <= data.attachments.size());

	for (uint32_t i = 0; i < data.attachments.size(); ++i)
	{
		if (data.attachment_types[i] & static_cast<VkImageAspectFlags>(type))
		{
			if (index == 0)
			{
				*out = { (uint64_t)data.attachments[i] };
				return true;
			}
			else
			{
				index -= 1;
			}
		}
	}

	*out = { 0 };
	return false;
}

void reshade::vulkan::device_impl::advance_transient_descriptor_pool()
{
	if (vk.CmdPushDescriptorSetKHR != nullptr)
		return;

	const VkDescriptorPool next_pool = _transient_descriptor_pool[++_transient_index % 4];
	vk.ResetDescriptorPool(_orig, next_pool, 0);
}
