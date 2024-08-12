/*
 * Copyright (C) 2021 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "vulkan_impl_device.hpp"
#include "vulkan_impl_command_queue.hpp"
#include "vulkan_impl_type_convert.hpp"
#include "dll_log.hpp"
#include <cstring> // std::memcpy
#include <algorithm> // std::copy_n, std::max

#define vk _dispatch_table

reshade::vulkan::device_impl::device_impl(
	VkDevice device,
	VkPhysicalDevice physical_device,
	VkInstance instance,
	uint32_t api_version,
	const VkLayerInstanceDispatchTable &instance_table, const VkLayerDispatchTable &device_table, const VkPhysicalDeviceFeatures &enabled_features,
	bool push_descriptors_ext,
	bool dynamic_rendering_ext,
	bool timeline_semaphore_ext,
	bool custom_border_color_ext,
	bool extended_dynamic_state_ext,
	bool conservative_rasterization_ext,
	bool ray_tracing_ext) :
	api_object_impl(device),
	_physical_device(physical_device),
	_dispatch_table(device_table),
	_instance_dispatch_table(instance_table),
	_push_descriptor_ext(push_descriptors_ext),
	_dynamic_rendering_ext(dynamic_rendering_ext),
	_timeline_semaphore_ext(timeline_semaphore_ext),
	_custom_border_color_ext(custom_border_color_ext),
	_extended_dynamic_state_ext(extended_dynamic_state_ext),
	_conservative_rasterization_ext(conservative_rasterization_ext),
	_ray_tracing_ext(ray_tracing_ext),
	_enabled_features(enabled_features)
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
#if VMA_VULKAN_VERSION >= 1001000
		functions.vkGetBufferMemoryRequirements2KHR = device_table.GetBufferMemoryRequirements2;
		functions.vkGetImageMemoryRequirements2KHR = device_table.GetImageMemoryRequirements2;
		functions.vkBindBufferMemory2KHR = device_table.BindBufferMemory2;
		functions.vkBindImageMemory2KHR = device_table.BindImageMemory2;
		functions.vkGetPhysicalDeviceMemoryProperties2KHR = instance_table.GetPhysicalDeviceMemoryProperties2;
#endif
#if VMA_VULKAN_VERSION >= 1003000
		functions.vkGetDeviceBufferMemoryRequirements = device_table.GetDeviceBufferMemoryRequirements;
		functions.vkGetDeviceImageMemoryRequirements = device_table.GetDeviceImageMemoryRequirements;
#endif

		VmaAllocatorCreateInfo create_info = {};
		create_info.physicalDevice = physical_device;
		create_info.device = device;
		create_info.preferredLargeHeapBlockSize = 1920 * 1080 * 4 * 16; // Allocate blocks of memory that can comfortably contain 16 Full HD images
		create_info.pVulkanFunctions = &functions;
		create_info.instance = instance;
		create_info.vulkanApiVersion = api_version;

		if (ray_tracing_ext)
			create_info.flags |= VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;

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
		create_info.poolSizeCount = static_cast<uint32_t>(std::size(pool_sizes));
		create_info.pPoolSizes = pool_sizes;

		if (vk.CreateDescriptorPool(_orig, &create_info, nullptr, &_descriptor_pool) != VK_SUCCESS)
		{
			log::message(log::level::error, "Failed to create descriptor pool!");
		}
	}

	if (!_push_descriptor_ext)
	{
		for (uint32_t i = 0; i < 4; ++i)
		{
			VkDescriptorPoolCreateInfo create_info { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
			create_info.maxSets = 32;
			create_info.poolSizeCount = static_cast<uint32_t>(std::size(pool_sizes));
			create_info.pPoolSizes = pool_sizes;

			if (vk.CreateDescriptorPool(_orig, &create_info, nullptr, &_transient_descriptor_pool[i]) != VK_SUCCESS)
			{
				log::message(log::level::error, "Failed to create transient descriptor pool!");
			}
		}
	}

	{	VkPrivateDataSlotCreateInfo create_info { VK_STRUCTURE_TYPE_PRIVATE_DATA_SLOT_CREATE_INFO };

		if (vk.CreatePrivateDataSlot(_orig, &create_info, nullptr, &_private_data_slot) != VK_SUCCESS)
		{
			log::message(log::level::error, "Failed to create private data slot!");
		}
	}
}
reshade::vulkan::device_impl::~device_impl()
{
	assert(_queues.empty()); // All queues should have been unregistered and destroyed at this point

	for (const auto &render_pass_data : _render_pass_lookup)
	{
		vk.DestroyRenderPass(_orig, render_pass_data.second.renderPass, nullptr);
		vk.DestroyFramebuffer(_orig, render_pass_data.second.framebuffer, nullptr);
	}

	vk.DestroyPrivateDataSlot(_orig, _private_data_slot, nullptr);

	vk.DestroyDescriptorPool(_orig, _descriptor_pool, nullptr);
	for (uint32_t i = 0; i < 4; ++i)
		vk.DestroyDescriptorPool(_orig, _transient_descriptor_pool[i], nullptr);

	vmaDestroyAllocator(_alloc);
}

bool reshade::vulkan::device_impl::get_property(api::device_properties property, void *data) const
{
	VkPhysicalDeviceRayTracingPipelinePropertiesKHR ray_tracing_props { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR };
	VkPhysicalDeviceProperties2 device_props { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, &ray_tracing_props };
	_instance_dispatch_table.GetPhysicalDeviceProperties2(_physical_device, &device_props);

	switch (property)
	{
	case api::device_properties::api_version:
		*static_cast<uint32_t *>(data) =
			VK_VERSION_MAJOR(device_props.properties.apiVersion) << 12 |
			VK_VERSION_MINOR(device_props.properties.apiVersion) << 8;
		return true;
	case api::device_properties::driver_version:
	{
		const uint32_t driver_major_version = VK_VERSION_MAJOR(device_props.properties.driverVersion);
		// NVIDIA has a custom driver version scheme, so extract the proper minor version from it
		const uint32_t driver_minor_version = device_props.properties.vendorID == 0x10DE ?
			(device_props.properties.driverVersion >> 14) & 0xFF : VK_VERSION_MINOR(device_props.properties.driverVersion);
		*static_cast<uint32_t *>(data) = driver_major_version * 100 + driver_minor_version;
		return true;
	}
	case api::device_properties::vendor_id:
		*static_cast<uint32_t *>(data) = device_props.properties.vendorID;
		return true;
	case api::device_properties::device_id:
		*static_cast<uint32_t *>(data) = device_props.properties.deviceID;
		return true;
	case api::device_properties::description:
		static_assert(VK_MAX_PHYSICAL_DEVICE_NAME_SIZE <= 256);
		std::copy_n(device_props.properties.deviceName, 256, static_cast<char *>(data));
		return true;
	case api::device_properties::shader_group_handle_size:
		*static_cast<uint32_t *>(data) = ray_tracing_props.shaderGroupHandleSize;
		return true;
	case api::device_properties::shader_group_alignment:
		*static_cast<uint32_t *>(data) = ray_tracing_props.shaderGroupBaseAlignment;
		return true;
	case api::device_properties::shader_group_handle_alignment:
		*static_cast<uint32_t *>(data) = ray_tracing_props.shaderGroupHandleAlignment;
		return true;
	default:
		return false;
	}
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
	case api::device_caps::dual_source_blend:
		return _enabled_features.dualSrcBlend;
	case api::device_caps::independent_blend:
		return _enabled_features.independentBlend;
	case api::device_caps::fill_mode_non_solid:
		return _enabled_features.fillModeNonSolid;
	case api::device_caps::conservative_rasterization:
		return _conservative_rasterization_ext;
	case api::device_caps::bind_render_targets_and_depth_stencil:
		return false;
	case api::device_caps::multi_viewport:
		return _enabled_features.multiViewport;
	case api::device_caps::partial_push_constant_updates:
		return true;
	case api::device_caps::partial_push_descriptor_updates:
		return _push_descriptor_ext;
	case api::device_caps::draw_instanced:
		return true;
	case api::device_caps::draw_or_dispatch_indirect:
		// Technically this only specifies whether multi-draw indirect is supported, not draw indirect as a whole
		return _enabled_features.multiDrawIndirect;
	case api::device_caps::copy_buffer_region:
	case api::device_caps::copy_buffer_to_texture:
	case api::device_caps::blit:
	case api::device_caps::resolve_region:
	case api::device_caps::copy_query_heap_results:
	case api::device_caps::sampler_compare:
		return true;
	case api::device_caps::sampler_anisotropic:
		return _enabled_features.samplerAnisotropy;
	case api::device_caps::sampler_with_resource_view:
		return true;
	case api::device_caps::shared_resource:
	{
		VkPhysicalDeviceExternalBufferInfo external_buffer_info { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_BUFFER_INFO };
		external_buffer_info.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_KMT_BIT;

		VkExternalBufferProperties external_buffer_properties { VK_STRUCTURE_TYPE_EXTERNAL_BUFFER_PROPERTIES };
		_instance_dispatch_table.GetPhysicalDeviceExternalBufferProperties(_physical_device, &external_buffer_info, &external_buffer_properties);
		return (external_buffer_properties.externalMemoryProperties.externalMemoryFeatures & (VK_EXTERNAL_MEMORY_FEATURE_EXPORTABLE_BIT | VK_EXTERNAL_MEMORY_FEATURE_IMPORTABLE_BIT)) == (VK_EXTERNAL_MEMORY_FEATURE_EXPORTABLE_BIT | VK_EXTERNAL_MEMORY_FEATURE_IMPORTABLE_BIT);
	}
	case api::device_caps::shared_resource_nt_handle:
	{
		VkPhysicalDeviceExternalBufferInfo external_buffer_info { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_BUFFER_INFO };
		external_buffer_info.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT;

		VkExternalBufferProperties external_buffer_properties { VK_STRUCTURE_TYPE_EXTERNAL_BUFFER_PROPERTIES };
		_instance_dispatch_table.GetPhysicalDeviceExternalBufferProperties(_physical_device, &external_buffer_info, &external_buffer_properties);
		return (external_buffer_properties.externalMemoryProperties.externalMemoryFeatures & (VK_EXTERNAL_MEMORY_FEATURE_EXPORTABLE_BIT | VK_EXTERNAL_MEMORY_FEATURE_IMPORTABLE_BIT)) == (VK_EXTERNAL_MEMORY_FEATURE_EXPORTABLE_BIT | VK_EXTERNAL_MEMORY_FEATURE_IMPORTABLE_BIT);
	}
	case api::device_caps::resolve_depth_stencil:
		// VK_KHR_dynamic_rendering has VK_KHR_depth_stencil_resolve as a dependency
		return _dynamic_rendering_ext;
	case api::device_caps::shared_fence:
	{
		VkPhysicalDeviceExternalSemaphoreInfo external_semaphore_info { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_SEMAPHORE_INFO };
		external_semaphore_info.handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_KMT_BIT;

		VkExternalSemaphoreProperties external_semaphore_properties { VK_STRUCTURE_TYPE_EXTERNAL_SEMAPHORE_PROPERTIES };
		_instance_dispatch_table.GetPhysicalDeviceExternalSemaphoreProperties(_physical_device, &external_semaphore_info, &external_semaphore_properties);
		return (external_semaphore_properties.externalSemaphoreFeatures & (VK_EXTERNAL_SEMAPHORE_FEATURE_EXPORTABLE_BIT | VK_EXTERNAL_SEMAPHORE_FEATURE_IMPORTABLE_BIT)) == (VK_EXTERNAL_SEMAPHORE_FEATURE_EXPORTABLE_BIT | VK_EXTERNAL_SEMAPHORE_FEATURE_IMPORTABLE_BIT);
	}
	case api::device_caps::shared_fence_nt_handle:
	{
		VkPhysicalDeviceExternalSemaphoreInfo external_semaphore_info { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_SEMAPHORE_INFO };
		external_semaphore_info.handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_BIT;

		VkExternalSemaphoreProperties external_semaphore_properties { VK_STRUCTURE_TYPE_EXTERNAL_SEMAPHORE_PROPERTIES };
		_instance_dispatch_table.GetPhysicalDeviceExternalSemaphoreProperties(_physical_device, &external_semaphore_info, &external_semaphore_properties);
		return (external_semaphore_properties.externalSemaphoreFeatures & (VK_EXTERNAL_SEMAPHORE_FEATURE_EXPORTABLE_BIT | VK_EXTERNAL_SEMAPHORE_FEATURE_IMPORTABLE_BIT)) == (VK_EXTERNAL_SEMAPHORE_FEATURE_EXPORTABLE_BIT | VK_EXTERNAL_SEMAPHORE_FEATURE_IMPORTABLE_BIT);
	}
	case api::device_caps::amplification_and_mesh_shader:
		return vk.CmdDrawMeshTasksEXT != nullptr;
	case api::device_caps::ray_tracing:
		return _ray_tracing_ext;
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

	if ((usage & api::resource_usage::depth_stencil) != 0 && (props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) == 0)
		return false;
	if ((usage & api::resource_usage::render_target) != 0 && (props.optimalTilingFeatures & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT) == 0)
		return false;
	if ((usage & api::resource_usage::shader_resource) != 0 && (props.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) == 0)
		return false;
	if ((usage & api::resource_usage::unordered_access) != 0 && (props.optimalTilingFeatures & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT) == 0)
		return false;

	if ((usage & (api::resource_usage::copy_dest | api::resource_usage::resolve_dest)) != 0 && (props.optimalTilingFeatures & VK_FORMAT_FEATURE_TRANSFER_DST_BIT) == 0)
		return false;
	if ((usage & (api::resource_usage::copy_source | api::resource_usage::resolve_source)) != 0 && (props.optimalTilingFeatures & VK_FORMAT_FEATURE_TRANSFER_SRC_BIT) == 0)
		return false;

	return true;
}

bool reshade::vulkan::device_impl::create_sampler(const api::sampler_desc &desc, api::sampler *out_sampler)
{
	VkSamplerCreateInfo create_info { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
	create_info.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;

	VkSamplerCustomBorderColorCreateInfoEXT border_color_info;
	if (_custom_border_color_ext && (
		desc.border_color[0] != 0.0f || desc.border_color[1] != 0.0f || desc.border_color[2] != 0.0f || desc.border_color[3] != 0.0f))
	{
		border_color_info = { VK_STRUCTURE_TYPE_SAMPLER_CUSTOM_BORDER_COLOR_CREATE_INFO_EXT };

		create_info.pNext = &border_color_info;
		create_info.borderColor = VK_BORDER_COLOR_FLOAT_CUSTOM_EXT;
	}

	convert_sampler_desc(desc, create_info);

	if (VkSampler object = VK_NULL_HANDLE;
		vk.CreateSampler(_orig, &create_info, nullptr, &object) == VK_SUCCESS)
	{
		*out_sampler = { (uint64_t)object };
		return true;
	}
	else
	{
		*out_sampler = { 0 };
		return false;
	}
}
void reshade::vulkan::device_impl::destroy_sampler(api::sampler sampler)
{
	vk.DestroySampler(_orig, (VkSampler)sampler.handle, nullptr);
}

bool reshade::vulkan::device_impl::create_resource(const api::resource_desc &desc, const api::subresource_data *initial_data, api::resource_usage initial_state, api::resource *out_resource, HANDLE *shared_handle)
{
	*out_resource = { 0 };

	assert((desc.usage & initial_state) == initial_state || initial_state == api::resource_usage::general || initial_state == api::resource_usage::cpu_access);

	VmaAllocation allocation = VMA_NULL;
	VmaAllocationCreateInfo alloc_info = {};
	switch (desc.heap)
	{
	default:
	case api::memory_heap::unknown:
		alloc_info.usage = VMA_MEMORY_USAGE_UNKNOWN;
		break;
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

	VkExternalMemoryHandleTypeFlagBits handle_type = {};
	union
	{
		VkExportMemoryAllocateInfo export_info;
		VkImportMemoryWin32HandleInfoKHR import_info;
	} import_export_info;

	const bool is_shared = (desc.flags & api::resource_flags::shared) != 0;
	if (is_shared)
	{
		if (shared_handle == nullptr)
			return false;

		if ((desc.flags & api::resource_flags::shared_nt_handle) != 0)
			handle_type = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT;
		else
			handle_type = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_KMT_BIT;

		if (*shared_handle != nullptr)
		{
			assert(initial_data == nullptr);

			import_export_info.import_info = { VK_STRUCTURE_TYPE_IMPORT_MEMORY_WIN32_HANDLE_INFO_KHR };
			import_export_info.import_info.handleType = handle_type;
			import_export_info.import_info.handle = *shared_handle;
		}
		else
		{
			if (vk.GetMemoryWin32HandleKHR == nullptr)
				return false;

			import_export_info.export_info = { VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO };
			import_export_info.export_info.handleTypes = handle_type;
		}
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

			VkExternalMemoryBufferCreateInfo external_memory_info;
			if (is_shared)
			{
				external_memory_info = { VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO };
				external_memory_info.handleTypes = handle_type;
				create_info.pNext = &external_memory_info;
			}

			if (VkBuffer object = VK_NULL_HANDLE;
				(desc.heap == api::memory_heap::unknown || is_shared ?
					 vk.CreateBuffer(_orig, &create_info, nullptr, &object) :
					 vmaCreateBuffer(_alloc, &create_info, &alloc_info, &object, &allocation, nullptr)) == VK_SUCCESS)
			{
				object_data<VK_OBJECT_TYPE_BUFFER> data;
				data.allocation = allocation;
				data.create_info = create_info;
				data.create_info.pNext = nullptr; // Clear out structure chain pointer, since it becomes invalid once leaving the current scope

				if (is_shared)
				{
					VkMemoryRequirements reqs = {};
					vk.GetBufferMemoryRequirements(_orig, object, &reqs);

					VkMemoryAllocateInfo mem_alloc_info { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, &import_export_info };
					mem_alloc_info.allocationSize = reqs.size;
					vmaFindMemoryTypeIndex(_alloc, reqs.memoryTypeBits, &alloc_info, &mem_alloc_info.memoryTypeIndex);

					if (vk.AllocateMemory(_orig, &mem_alloc_info, nullptr, &data.memory) != VK_SUCCESS)
					{
						vk.DestroyBuffer(_orig, object, nullptr);
						break;
					}

					vk.BindBufferMemory(_orig, object, data.memory, data.memory_offset);

					if (*shared_handle == nullptr)
					{
						VkMemoryGetWin32HandleInfoKHR handle_info { VK_STRUCTURE_TYPE_MEMORY_GET_WIN32_HANDLE_INFO_KHR };
						handle_info.memory = data.memory;
						handle_info.handleType = handle_type;

						if (vk.GetMemoryWin32HandleKHR(_orig, &handle_info, shared_handle) != VK_SUCCESS)
						{
							vk.DestroyBuffer(_orig, object, nullptr);
							vk.FreeMemory(_orig, data.memory, nullptr);
							break;
						}
					}
				}
				else if (allocation != VMA_NULL)
				{
					VmaAllocationInfo allocation_info;
					vmaGetAllocationInfo(_alloc, allocation, &allocation_info);
					data.memory = allocation_info.deviceMemory;
					data.memory_offset = allocation_info.offset;
				}

				register_object<VK_OBJECT_TYPE_BUFFER>(object, std::move(data));

				*out_resource = { (uint64_t)object };

				if (initial_data != nullptr)
				{
					if (get_first_immediate_command_list())
					{
						update_buffer_region(initial_data->data, *out_resource, 0, desc.buffer.size);
					}
					else
					{
						// Cannot upload initial data without a command list
						destroy_resource(*out_resource);
						*out_resource = { 0 };
						break;
					}
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
			// Default view creation for resolving requires image to have a usage usable for view creation
			if (desc.heap != api::memory_heap::unknown && !is_shared && (desc.usage & (api::resource_usage::resolve_source | api::resource_usage::resolve_dest)) != 0)
				create_info.usage |= aspect_flags_from_format(create_info.format) & (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT) ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT : VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
			// Mapping images is only really useful with linear tiling
			if (desc.heap == api::memory_heap::gpu_to_cpu || desc.heap == api::memory_heap::cpu_only)
				create_info.tiling = VK_IMAGE_TILING_LINEAR;

			VkExternalMemoryImageCreateInfo external_memory_info;
			if (is_shared)
			{
				external_memory_info = { VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO };
				external_memory_info.handleTypes = handle_type;

				create_info.pNext = &external_memory_info;
			}

			if (VkImage object = VK_NULL_HANDLE;
				(desc.heap == api::memory_heap::unknown || is_shared ?
					 vk.CreateImage(_orig, &create_info, nullptr, &object) :
					 vmaCreateImage(_alloc, &create_info, &alloc_info, &object, &allocation, nullptr)) == VK_SUCCESS)
			{
				object_data<VK_OBJECT_TYPE_IMAGE> data;
				data.allocation = allocation;
				data.create_info = create_info;
				data.create_info.pNext = nullptr; // Clear out structure chain pointer, since it becomes invalid once leaving the current scope

				if (is_shared)
				{
					VkMemoryRequirements reqs = {};
					vk.GetImageMemoryRequirements(_orig, object, &reqs);

					VkMemoryAllocateInfo mem_alloc_info { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, &import_export_info };
					mem_alloc_info.allocationSize = reqs.size;
					vmaFindMemoryTypeIndex(_alloc, reqs.memoryTypeBits, &alloc_info, &mem_alloc_info.memoryTypeIndex);

					if (vk.AllocateMemory(_orig, &mem_alloc_info, nullptr, &data.memory) != VK_SUCCESS)
					{
						vk.DestroyImage(_orig, object, nullptr);
						break;
					}

					vk.BindImageMemory(_orig, object, data.memory, data.memory_offset);

					if (*shared_handle == nullptr)
					{
						VkMemoryGetWin32HandleInfoKHR handle_info { VK_STRUCTURE_TYPE_MEMORY_GET_WIN32_HANDLE_INFO_KHR };
						handle_info.memory = data.memory;
						handle_info.handleType = handle_type;

						if (vk.GetMemoryWin32HandleKHR(_orig, &handle_info, shared_handle) != VK_SUCCESS)
						{
							vk.DestroyImage(_orig, object, nullptr);
							vk.FreeMemory(_orig, data.memory, nullptr);
							break;
						}
					}
				}
				else if (allocation != VMA_NULL)
				{
					VmaAllocationInfo allocation_info;
					vmaGetAllocationInfo(_alloc, allocation, &allocation_info);
					data.memory = allocation_info.deviceMemory;
					data.memory_offset = allocation_info.offset;

					// Need to create a default view that is used in 'command_list_impl::resolve_texture_region'
					if ((desc.usage & (api::resource_usage::resolve_source | api::resource_usage::resolve_dest)) != 0)
					{
						VkImageViewCreateInfo default_view_info { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
						default_view_info.image = object;
						default_view_info.viewType = static_cast<VkImageViewType>(create_info.imageType); // Map 'VK_IMAGE_TYPE_1D' to VK_IMAGE_VIEW_TYPE_1D' and so on
						default_view_info.format = create_info.format;
						default_view_info.subresourceRange.aspectMask = aspect_flags_from_format(create_info.format);
						default_view_info.subresourceRange.baseMipLevel = 0;
						default_view_info.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
						default_view_info.subresourceRange.baseArrayLayer = 0;
						default_view_info.subresourceRange.layerCount = 1; // Non-array image view types can only contain a single layer

						vk.CreateImageView(_orig, &default_view_info, nullptr, &data.default_view);
					}
				}

				register_object<VK_OBJECT_TYPE_IMAGE>(object, std::move(data));

				*out_resource = { (uint64_t)object };

				// Only makes sense to upload initial data if it is not thrown away on the first layout transition
				assert(initial_data == nullptr || initial_state != api::resource_usage::undefined);

				if (initial_state != api::resource_usage::undefined)
				{
					// Transition resource into the initial state using the first available immediate command list
					if (const auto immediate_command_list = get_first_immediate_command_list())
					{
						if (initial_data != nullptr)
						{
							const api::resource_usage states_upload[2] = { api::resource_usage::undefined, api::resource_usage::copy_dest };
							immediate_command_list->barrier(1, out_resource, &states_upload[0], &states_upload[1]);

							for (uint32_t subresource = 0; subresource < (desc.type == api::resource_type::texture_3d ? 1u : static_cast<uint32_t>(desc.texture.depth_or_layers)) * desc.texture.levels; ++subresource)
								update_texture_region(initial_data[subresource], *out_resource, subresource, nullptr);

							const api::resource_usage states_finalize[2] = { api::resource_usage::copy_dest, initial_state };
							immediate_command_list->barrier(1, out_resource, &states_finalize[0], &states_finalize[1]);
						}
						else
						{
							const api::resource_usage states_finalize[2] = { api::resource_usage::undefined, initial_state };
							immediate_command_list->barrier(1, out_resource, &states_finalize[0], &states_finalize[1]);
						}

						// Always flush right away, in case resource is destroyed again before an explicit flush of the immediate command list
						VkSubmitInfo semaphore_info { VK_STRUCTURE_TYPE_SUBMIT_INFO };
						immediate_command_list->flush(semaphore_info);
					}
					else if (initial_data != nullptr)
					{
						// Cannot upload initial data without a command list
						destroy_resource(*out_resource);
						*out_resource = { 0 };
						break;
					}
				}
				return true;
			}
			break;
		}
	}

	return false;
}
void reshade::vulkan::device_impl::destroy_resource(api::resource resource)
{
	if (resource == 0)
		return;

	static_assert(
		offsetof(object_data<VK_OBJECT_TYPE_IMAGE>, allocation ) == offsetof(object_data<VK_OBJECT_TYPE_BUFFER>, allocation ) &&
		offsetof(object_data<VK_OBJECT_TYPE_IMAGE>, create_info) == offsetof(object_data<VK_OBJECT_TYPE_BUFFER>, create_info));

	const auto data = get_private_data_for_object<VK_OBJECT_TYPE_IMAGE>((VkImage)resource.handle);
	if (data->create_info.sType == VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO) // Structure type is at the same offset in both image and buffer object data structures
	{
		const VmaAllocation allocation = data->allocation;
		const VkDeviceMemory memory = data->memory;
		const VkImageView default_view = data->default_view;

		// Warning, the 'data' pointer must not be accessed after this call, since it frees that memory!
		unregister_object<VK_OBJECT_TYPE_IMAGE>((VkImage)resource.handle);

		if (allocation == VMA_NULL)
		{
			vk.DestroyImageView(_orig, default_view, nullptr);
			vk.DestroyImage(_orig, (VkImage)resource.handle, nullptr);
			vk.FreeMemory(_orig, memory, nullptr);
		}
		else
		{
			vmaDestroyImage(_alloc, (VkImage)resource.handle, allocation);
		}
	}
	else
	{
		const VmaAllocation allocation = data->allocation;
		const VkDeviceMemory memory = data->memory;

		// Warning, the 'data' pointer must not be accessed after this call, since it frees that memory!
		unregister_object<VK_OBJECT_TYPE_BUFFER>((VkBuffer)resource.handle);

		if (allocation == VMA_NULL)
		{
			vk.DestroyBuffer(_orig, (VkBuffer)resource.handle, nullptr);
			vk.FreeMemory(_orig, memory, nullptr);
		}
		else
		{
			vmaDestroyBuffer(_alloc, (VkBuffer)resource.handle, allocation);
		}
	}
}

reshade::api::resource_desc reshade::vulkan::device_impl::get_resource_desc(api::resource resource) const
{
	const auto data = get_private_data_for_object<VK_OBJECT_TYPE_IMAGE>((VkImage)resource.handle);
	if (data->create_info.sType == VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO)
		return convert_resource_desc(data->create_info);
	else
		return convert_resource_desc(reinterpret_cast<const object_data<VK_OBJECT_TYPE_BUFFER> *>(data)->create_info);
}

bool reshade::vulkan::device_impl::create_resource_view(api::resource resource, api::resource_usage usage_type, const api::resource_view_desc &desc, api::resource_view *out_view)
{
	*out_view = { 0 };

	if (resource == 0)
		return false;

	const auto resource_data = get_private_data_for_object<VK_OBJECT_TYPE_IMAGE>((VkImage)resource.handle);
	if (resource_data->create_info.sType == VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO)
	{
		VkImageViewCreateInfo create_info { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
		create_info.image = (VkImage)resource.handle;

		if (desc.type != api::resource_view_type::unknown)
		{
			convert_resource_view_desc(desc, create_info);
		}
		else
		{
			switch (resource_data->create_info.imageType)
			{
			case VK_IMAGE_TYPE_1D:
				create_info.viewType = VK_IMAGE_VIEW_TYPE_1D;
				break;
			case VK_IMAGE_TYPE_2D:
				create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
				break;
			case VK_IMAGE_TYPE_3D:
				create_info.viewType = VK_IMAGE_VIEW_TYPE_3D;
				break;
			}

			create_info.format = resource_data->create_info.format;
			create_info.subresourceRange.baseMipLevel = 0;
			create_info.subresourceRange.levelCount = (usage_type & api::resource_usage::render_target) != 0 ? 1 : VK_REMAINING_MIP_LEVELS;
			create_info.subresourceRange.baseArrayLayer = 0;
			create_info.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
		}

		create_info.subresourceRange.aspectMask = aspect_flags_from_format(create_info.format);

		// Shader resource views can never access stencil data (except for the explicit formats that do that), so remove that aspect flag for views created with a format that supports stencil
		if (desc.format == api::format::x24_unorm_g8_uint || desc.format == api::format::x32_float_g8_uint)
			create_info.subresourceRange.aspectMask &= ~VK_IMAGE_ASPECT_DEPTH_BIT;
		else if ((usage_type & api::resource_usage::shader_resource) != 0)
			create_info.subresourceRange.aspectMask &= ~VK_IMAGE_ASPECT_STENCIL_BIT;

		VkImageView image_view = VK_NULL_HANDLE;
		if (vk.CreateImageView(_orig, &create_info, nullptr, &image_view) == VK_SUCCESS)
		{
			object_data<VK_OBJECT_TYPE_IMAGE_VIEW> data;
			data.create_info = create_info;
			data.image_extent = resource_data->create_info.extent;
			if (VK_REMAINING_MIP_LEVELS == data.create_info.subresourceRange.levelCount)
				data.create_info.subresourceRange.levelCount = resource_data->create_info.mipLevels;
			if (VK_REMAINING_ARRAY_LAYERS == data.create_info.subresourceRange.layerCount)
				data.create_info.subresourceRange.layerCount = resource_data->create_info.arrayLayers;

			register_object<VK_OBJECT_TYPE_IMAGE_VIEW>(image_view, std::move(data));

			*out_view = { (uint64_t)image_view };
			return true;
		}
	}
	else if (usage_type != api::resource_usage::acceleration_structure && desc.type != api::resource_view_type::acceleration_structure)
	{
		VkBufferViewCreateInfo create_info { VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO };
		create_info.buffer = (VkBuffer)resource.handle;

		if (desc.type != api::resource_view_type::unknown)
		{
			convert_resource_view_desc(desc, create_info);
		}
		else
		{
			create_info.format = VK_FORMAT_UNDEFINED;
			create_info.offset = 0;
			create_info.range = VK_WHOLE_SIZE;
		}

		VkBufferView buffer_view = VK_NULL_HANDLE;
		if (vk.CreateBufferView(_orig, &create_info, nullptr, &buffer_view) == VK_SUCCESS)
		{
			object_data<VK_OBJECT_TYPE_BUFFER_VIEW> data;
			data.create_info = create_info;

			register_object<VK_OBJECT_TYPE_BUFFER_VIEW>(buffer_view, std::move(data));

			*out_view = { (uint64_t)buffer_view };
			return true;
		}
	}
	else
	{
		VkAccelerationStructureCreateInfoKHR create_info { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR };
		create_info.type = VK_ACCELERATION_STRUCTURE_TYPE_GENERIC_KHR;
		create_info.buffer = (VkBuffer)resource.handle;

		if (desc.type != api::resource_view_type::unknown)
		{
			convert_resource_view_desc(desc, create_info);
		}
		else
		{
			create_info.offset = 0;
			create_info.size = VK_WHOLE_SIZE;
		}

		if (VK_WHOLE_SIZE == create_info.size)
			create_info.size = reinterpret_cast<object_data<VK_OBJECT_TYPE_BUFFER> *>(resource_data)->create_info.size - create_info.offset;

		VkAccelerationStructureKHR buffer_view = VK_NULL_HANDLE;
		if (vk.CreateAccelerationStructureKHR(_orig, &create_info, nullptr, &buffer_view) == VK_SUCCESS)
		{
			object_data<VK_OBJECT_TYPE_ACCELERATION_STRUCTURE_KHR> data;
			data.create_info = create_info;

			register_object<VK_OBJECT_TYPE_ACCELERATION_STRUCTURE_KHR>(buffer_view, std::move(data));

			*out_view = { (uint64_t)buffer_view };
			return true;
		}
	}

	return false;
}
void reshade::vulkan::device_impl::destroy_resource_view(api::resource_view view)
{
	if (view == 0)
		return;

	static_assert(
		offsetof(object_data<VK_OBJECT_TYPE_IMAGE_VIEW>, create_info) == offsetof(object_data<VK_OBJECT_TYPE_BUFFER_VIEW>, create_info));

	const auto data = get_private_data_for_object<VK_OBJECT_TYPE_IMAGE_VIEW>((VkImageView)view.handle);
	if (data->create_info.sType == VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO) // Structure type is at the same offset in both image view and buffer view object data structures
	{
		unregister_object<VK_OBJECT_TYPE_IMAGE_VIEW>((VkImageView)view.handle);

		vk.DestroyImageView(_orig, (VkImageView)view.handle, nullptr);
	}
	else if(data->create_info.sType != VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR)
	{
		unregister_object<VK_OBJECT_TYPE_BUFFER_VIEW>((VkBufferView)view.handle);

		vk.DestroyBufferView(_orig, (VkBufferView)view.handle, nullptr);
	}
	else
	{
		unregister_object<VK_OBJECT_TYPE_ACCELERATION_STRUCTURE_KHR>((VkAccelerationStructureKHR)view.handle);

		vk.DestroyAccelerationStructureKHR(_orig, (VkAccelerationStructureKHR)view.handle, nullptr);
	}
}

reshade::api::resource reshade::vulkan::device_impl::get_resource_from_view(api::resource_view view) const
{
	const auto data = get_private_data_for_object<VK_OBJECT_TYPE_IMAGE_VIEW>((VkImageView)view.handle);
	if (data->create_info.sType == VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO)
		return { (uint64_t)data->create_info.image };
	else if (data->create_info.sType != VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR)
		return { (uint64_t)reinterpret_cast<const object_data<VK_OBJECT_TYPE_BUFFER_VIEW> *>(data)->create_info.buffer };
	else
		return { (uint64_t)reinterpret_cast<const object_data<VK_OBJECT_TYPE_ACCELERATION_STRUCTURE_KHR> *>(data)->create_info.buffer };
}
reshade::api::resource_view_desc reshade::vulkan::device_impl::get_resource_view_desc(api::resource_view view) const
{
	const auto data = get_private_data_for_object<VK_OBJECT_TYPE_IMAGE_VIEW>((VkImageView)view.handle);
	if (data->create_info.sType == VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO)
		return convert_resource_view_desc(data->create_info);
	else if (data->create_info.sType != VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR)
		return convert_resource_view_desc(reinterpret_cast<const object_data<VK_OBJECT_TYPE_BUFFER_VIEW> *>(data)->create_info);
	else
		return convert_resource_view_desc(reinterpret_cast<const object_data<VK_OBJECT_TYPE_ACCELERATION_STRUCTURE_KHR> *>(data)->create_info);
}

uint64_t reshade::vulkan::device_impl::get_resource_view_gpu_address(api::resource_view view) const
{
	const auto data = get_private_data_for_object<VK_OBJECT_TYPE_IMAGE_VIEW>((VkImageView)view.handle);
	if (data->create_info.sType == VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO)
	{
		return 0;
	}
	else if (data->create_info.sType != VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR)
	{
		VkBufferDeviceAddressInfo info { VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO };
		info.buffer = reinterpret_cast<const object_data<VK_OBJECT_TYPE_BUFFER_VIEW> *>(data)->create_info.buffer;

		return vk.GetBufferDeviceAddress(_orig, &info) + reinterpret_cast<const object_data<VK_OBJECT_TYPE_BUFFER_VIEW> *>(data)->create_info.offset;
	}
	else
	{
		VkAccelerationStructureDeviceAddressInfoKHR info{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR };
		info.accelerationStructure = (VkAccelerationStructureKHR)view.handle;

		return vk.GetAccelerationStructureDeviceAddressKHR(_orig, &info);
	}
}

bool reshade::vulkan::device_impl::map_buffer_region(api::resource resource, uint64_t offset, uint64_t size, api::map_access, void **out_data)
{
	if (out_data == nullptr)
		return false;

	assert(resource != 0);

	const auto resource_data = get_private_data_for_object<VK_OBJECT_TYPE_BUFFER>((VkBuffer)resource.handle);
	if (resource_data->allocation != VMA_NULL)
	{
		assert(size == UINT64_MAX || size <= resource_data->allocation->GetSize());

		if (vmaMapMemory(_alloc, resource_data->allocation, out_data) == VK_SUCCESS)
		{
			*out_data = static_cast<uint8_t *>(*out_data) + offset;
			return true;
		}
	}
	else if (resource_data->memory != VK_NULL_HANDLE)
	{
		return vk.MapMemory(_orig, resource_data->memory, resource_data->memory_offset + offset, size, 0, out_data) == VK_SUCCESS;
	}

	return false;
}
void reshade::vulkan::device_impl::unmap_buffer_region(api::resource resource)
{
	assert(resource != 0);

	const auto resource_data = get_private_data_for_object<VK_OBJECT_TYPE_BUFFER>((VkBuffer)resource.handle);
	if (resource_data->allocation != VMA_NULL)
	{
		vmaUnmapMemory(_alloc, resource_data->allocation);
	}
	else if (resource_data->memory != VK_NULL_HANDLE)
	{
		vk.UnmapMemory(_orig, resource_data->memory);
	}
}
bool reshade::vulkan::device_impl::map_texture_region(api::resource resource, uint32_t subresource, const api::subresource_box *box, api::map_access, api::subresource_data *out_data)
{
	if (out_data == nullptr)
		return false;

	out_data->data = nullptr;
	out_data->row_pitch = 0;
	out_data->slice_pitch = 0;

	// Mapping a subset of a texture is not supported
	if (box != nullptr)
		return false;

	assert(resource != 0);

	const auto resource_data = get_private_data_for_object<VK_OBJECT_TYPE_IMAGE>((VkImage)resource.handle);

	VkImageSubresource subresource_info;
	subresource_info.aspectMask = aspect_flags_from_format(resource_data->create_info.format);
	subresource_info.mipLevel = subresource % resource_data->create_info.mipLevels;
	subresource_info.arrayLayer = subresource / resource_data->create_info.mipLevels;

	VkSubresourceLayout subresource_layout = {};
	vk.GetImageSubresourceLayout(_orig, (VkImage)resource.handle, &subresource_info, &subresource_layout);

	if (resource_data->allocation != VMA_NULL)
	{
		if (vmaMapMemory(_alloc, resource_data->allocation, &out_data->data) == VK_SUCCESS)
		{
			out_data->data = static_cast<uint8_t *>(out_data->data) + subresource_layout.offset;
			out_data->row_pitch = static_cast<uint32_t>(subresource_layout.rowPitch);
			out_data->slice_pitch = static_cast<uint32_t>(std::max(subresource_layout.arrayPitch, subresource_layout.depthPitch));
			return true;
		}
	}
	else if (resource_data->memory != VK_NULL_HANDLE)
	{
		if (vk.MapMemory(_orig, resource_data->memory, resource_data->memory_offset, VK_WHOLE_SIZE, 0, &out_data->data) == VK_SUCCESS)
		{
			out_data->data = static_cast<uint8_t *>(out_data->data) + subresource_layout.offset;
			out_data->row_pitch = static_cast<uint32_t>(subresource_layout.rowPitch);
			out_data->slice_pitch = static_cast<uint32_t>(std::max(subresource_layout.arrayPitch, subresource_layout.depthPitch));
			return true;
		}
	}

	return false;
}
void reshade::vulkan::device_impl::unmap_texture_region(api::resource resource, uint32_t subresource)
{
	if (subresource != 0)
		return;

	assert(resource != 0);

	const auto resource_data = get_private_data_for_object<VK_OBJECT_TYPE_IMAGE>((VkImage)resource.handle);
	if (resource_data->allocation != VMA_NULL)
	{
		vmaUnmapMemory(_alloc, resource_data->allocation);
	}
	else if (resource_data->memory != VK_NULL_HANDLE)
	{
		vk.UnmapMemory(_orig, resource_data->memory);
	}
}

void reshade::vulkan::device_impl::update_buffer_region(const void *data, api::resource resource, uint64_t offset, uint64_t size)
{
	assert(resource != 0);
	assert(data != nullptr);

	if (const auto immediate_command_list = get_first_immediate_command_list())
	{
		immediate_command_list->_has_commands = true;

		vk.CmdUpdateBuffer(immediate_command_list->_orig, (VkBuffer)resource.handle, offset, size, data);

		immediate_command_list->flush_and_wait();
	}
}
void reshade::vulkan::device_impl::update_texture_region(const api::subresource_data &data, api::resource resource, uint32_t subresource, const api::subresource_box *box)
{
	assert(resource != 0);
	assert(data.data != nullptr);

	const auto resource_data = get_private_data_for_object<VK_OBJECT_TYPE_IMAGE>((VkImage)resource.handle);

	VkExtent3D extent = resource_data->create_info.extent;
	extent.depth *= resource_data->create_info.arrayLayers;

	if (box != nullptr)
	{
		extent.width  = box->width();
		extent.height = box->height();
		extent.depth  = box->depth();
	}

	const auto row_pitch = api::format_row_pitch(convert_format(resource_data->create_info.format), extent.width);
	const auto slice_pitch = api::format_slice_pitch(convert_format(resource_data->create_info.format), row_pitch, extent.height);
	const auto total_image_size = extent.depth * static_cast<size_t>(slice_pitch);

	// Allocate host memory for upload
	VkBuffer intermediate = VK_NULL_HANDLE;
	VmaAllocation intermediate_mem = VK_NULL_HANDLE;

	{   VkBufferCreateInfo create_info { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
		create_info.size = total_image_size;
		create_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

		VmaAllocationCreateInfo alloc_info = {};
		alloc_info.usage = VMA_MEMORY_USAGE_CPU_ONLY;

		if (vmaCreateBuffer(_alloc, &create_info, &alloc_info, &intermediate, &intermediate_mem, nullptr) != VK_SUCCESS)
		{
			log::message(log::level::error, "Failed to create upload buffer (width = %llu)!", create_info.size);
			return;
		}
	}

	// Fill upload buffer with pixel data
	uint8_t *mapped_data = nullptr;
	if (vmaMapMemory(_alloc, intermediate_mem, reinterpret_cast<void **>(&mapped_data)) == VK_SUCCESS)
	{
		if ((row_pitch == data.row_pitch || extent.height == 1) &&
			(slice_pitch == data.slice_pitch || extent.depth == 1))
		{
			std::memcpy(mapped_data, data.data, total_image_size);
		}
		else
		{
			const size_t row_size = data.row_pitch < row_pitch ? data.row_pitch : static_cast<size_t>(row_pitch);

			for (size_t z = 0; z < extent.depth; ++z)
				for (size_t y = 0; y < extent.height; ++y, mapped_data += row_pitch)
					std::memcpy(mapped_data, static_cast<const uint8_t *>(data.data) + z * data.slice_pitch + y * data.row_pitch, row_size);
		}

		vmaUnmapMemory(_alloc, intermediate_mem);

		// Copy data from upload buffer into target texture using the first available immediate command list
		if (const auto immediate_command_list = get_first_immediate_command_list())
		{
			immediate_command_list->copy_buffer_to_texture({ (uint64_t)intermediate }, 0, 0, 0, resource, subresource, box);

			// Wait for command to finish executing before destroying the upload buffer
			immediate_command_list->flush_and_wait();
		}
	}

	vmaDestroyBuffer(_alloc, intermediate, intermediate_mem);
}

bool reshade::vulkan::device_impl::create_shader_module(VkShaderStageFlagBits stage, const api::shader_desc &desc, VkPipelineShaderStageCreateInfo &stage_info, VkSpecializationInfo &spec_info, std::vector<VkSpecializationMapEntry> &spec_map)
{
	spec_map.reserve(desc.spec_constants);
	for (uint32_t i = 0; i < desc.spec_constants; ++i)
		spec_map.push_back(VkSpecializationMapEntry { desc.spec_constant_ids[i], i * 4, sizeof(uint32_t) });

	spec_info.mapEntryCount = desc.spec_constants;
	spec_info.pMapEntries = spec_map.data();
	spec_info.dataSize = desc.spec_constants * sizeof(uint32_t);
	spec_info.pData = desc.spec_constant_values;

	stage_info = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
	stage_info.stage = stage;
	stage_info.pName = desc.entry_point != nullptr ? desc.entry_point : "main";
	stage_info.pSpecializationInfo = &spec_info;

	VkShaderModuleCreateInfo create_info { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
	create_info.codeSize = desc.code_size;
	create_info.pCode = static_cast<const uint32_t *>(desc.code);

	return vk.CreateShaderModule(_orig, &create_info, nullptr, &stage_info.module) == VK_SUCCESS;
}

bool reshade::vulkan::device_impl::create_pipeline(api::pipeline_layout layout, uint32_t subobject_count, const api::pipeline_subobject *subobjects, api::pipeline *out_pipeline)
{
	VkRenderPass render_pass = VK_NULL_HANDLE;
	std::vector<VkShaderModule> shaders;

	api::shader_desc vs_desc = {};
	api::shader_desc hs_desc = {};
	api::shader_desc ds_desc = {};
	api::shader_desc gs_desc = {};
	api::shader_desc ps_desc = {};
	api::shader_desc cs_desc = {};
	api::shader_desc as_desc = {};
	api::shader_desc ms_desc = {};
	api::pipeline_subobject input_layout_desc = {};
	api::stream_output_desc stream_output_desc = {};
	api::blend_desc blend_desc = {};
	api::rasterizer_desc rasterizer_desc = {};
	api::depth_stencil_desc depth_stencil_desc = {};
	api::primitive_topology topology = api::primitive_topology::undefined;
	api::format depth_stencil_format = api::format::unknown;
	api::pipeline_subobject render_target_formats = {};
	api::pipeline_subobject dynamic_states_subobject = {};
	uint32_t sample_mask = UINT32_MAX;
	uint32_t sample_count = 1;
	uint32_t viewport_count = 1;
	std::vector<api::shader_desc> raygen_desc;
	std::vector<api::shader_desc> any_hit_desc;
	std::vector<api::shader_desc> closest_hit_desc;
	std::vector<api::shader_desc> miss_desc;
	std::vector<api::shader_desc> intersection_desc;
	std::vector<api::shader_desc> callable_desc;
	std::vector<api::pipeline> libraries;
	std::vector<api::shader_group> shader_groups;
	uint32_t max_payload_size = 0;
	uint32_t max_attribute_size = 2 * sizeof(float); // Default triangle attributes
	uint32_t max_recursion_depth = 1;
	api::pipeline_flags flags = api::pipeline_flags::none;

	for (uint32_t i = 0; i < subobject_count; ++i)
	{
		if (subobjects[i].count == 0)
			continue;

		switch (subobjects[i].type)
		{
		case api::pipeline_subobject_type::vertex_shader:
			assert(subobjects[i].count == 1);
			vs_desc = *static_cast<const api::shader_desc *>(subobjects[i].data);
			break;
		case api::pipeline_subobject_type::hull_shader:
			assert(subobjects[i].count == 1);
			hs_desc = *static_cast<const api::shader_desc *>(subobjects[i].data);
			break;
		case api::pipeline_subobject_type::domain_shader:
			assert(subobjects[i].count == 1);
			ds_desc = *static_cast<const api::shader_desc *>(subobjects[i].data);
			break;
		case api::pipeline_subobject_type::geometry_shader:
			assert(subobjects[i].count == 1);
			gs_desc = *static_cast<const api::shader_desc *>(subobjects[i].data);
			break;
		case api::pipeline_subobject_type::pixel_shader:
			assert(subobjects[i].count == 1);
			ps_desc = *static_cast<const api::shader_desc *>(subobjects[i].data);
			break;
		case api::pipeline_subobject_type::compute_shader:
			assert(subobjects[i].count == 1);
			cs_desc = *static_cast<const api::shader_desc *>(subobjects[i].data);
			break;
		case api::pipeline_subobject_type::input_layout:
			input_layout_desc = subobjects[i];
			break;
		case api::pipeline_subobject_type::stream_output_state:
			assert(subobjects[i].count == 1);
			stream_output_desc = *static_cast<const api::stream_output_desc *>(subobjects[i].data);
			break;
		case api::pipeline_subobject_type::blend_state:
			assert(subobjects[i].count == 1);
			blend_desc = *static_cast<const api::blend_desc *>(subobjects[i].data);
			break;
		case api::pipeline_subobject_type::rasterizer_state:
			assert(subobjects[i].count == 1);
			rasterizer_desc = *static_cast<const api::rasterizer_desc *>(subobjects[i].data);
			break;
		case api::pipeline_subobject_type::depth_stencil_state:
			assert(subobjects[i].count == 1);
			depth_stencil_desc = *static_cast<const api::depth_stencil_desc *>(subobjects[i].data);
			break;
		case api::pipeline_subobject_type::primitive_topology:
			assert(subobjects[i].count == 1);
			topology = *static_cast<const api::primitive_topology *>(subobjects[i].data);
			if (topology == api::primitive_topology::triangle_fan)
				goto exit_failure;
			break;
		case api::pipeline_subobject_type::depth_stencil_format:
			assert(subobjects[i].count == 1);
			depth_stencil_format = *static_cast<const api::format *>(subobjects[i].data);
			break;
		case api::pipeline_subobject_type::render_target_formats:
			assert(subobjects[i].count <= 8);
			render_target_formats = subobjects[i];
			break;
		case api::pipeline_subobject_type::sample_mask:
			assert(subobjects[i].count == 1);
			sample_mask = *static_cast<const uint32_t *>(subobjects[i].data);
			break;
		case api::pipeline_subobject_type::sample_count:
			assert(subobjects[i].count == 1);
			sample_count = *static_cast<const uint32_t *>(subobjects[i].data);
			break;
		case api::pipeline_subobject_type::viewport_count:
			assert(subobjects[i].count == 1);
			viewport_count = *static_cast<const uint32_t *>(subobjects[i].data);
			break;
		case api::pipeline_subobject_type::dynamic_pipeline_states:
			dynamic_states_subobject = subobjects[i];
			break;
		case api::pipeline_subobject_type::max_vertex_count:
			assert(subobjects[i].count == 1);
			break; // Ignored
		case api::pipeline_subobject_type::amplification_shader:
			assert(subobjects[i].count == 1);
			as_desc = *static_cast<const api::shader_desc *>(subobjects[i].data);
			break;
		case api::pipeline_subobject_type::mesh_shader:
			assert(subobjects[i].count == 1);
			ms_desc = *static_cast<const api::shader_desc *>(subobjects[i].data);
			break;
		case api::pipeline_subobject_type::raygen_shader:
			for (uint32_t k = 0; k < subobjects[i].count; ++k)
				raygen_desc.push_back(static_cast<const api::shader_desc *>(subobjects[i].data)[k]);
			break;
		case api::pipeline_subobject_type::any_hit_shader:
			for (uint32_t k = 0; k < subobjects[i].count; ++k)
				any_hit_desc.push_back(static_cast<const api::shader_desc *>(subobjects[i].data)[k]);
			break;
		case api::pipeline_subobject_type::closest_hit_shader:
			for (uint32_t k = 0; k < subobjects[i].count; ++k)
				closest_hit_desc.push_back(static_cast<const api::shader_desc *>(subobjects[i].data)[k]);
			break;
		case api::pipeline_subobject_type::miss_shader:
			for (uint32_t k = 0; k < subobjects[i].count; ++k)
				miss_desc.push_back(static_cast<const api::shader_desc *>(subobjects[i].data)[k]);
			break;
		case api::pipeline_subobject_type::intersection_shader:
			for (uint32_t k = 0; k < subobjects[i].count; ++k)
				intersection_desc.push_back(static_cast<const api::shader_desc *>(subobjects[i].data)[k]);
			break;
		case api::pipeline_subobject_type::callable_shader:
			for (uint32_t k = 0; k < subobjects[i].count; ++k)
				callable_desc.push_back(static_cast<const api::shader_desc *>(subobjects[i].data)[k]);
			break;
		case api::pipeline_subobject_type::libraries:
			for (uint32_t k = 0; k < subobjects[i].count; ++k)
				libraries.push_back(static_cast<const api::pipeline *>(subobjects[i].data)[k]);
			break;
		case api::pipeline_subobject_type::shader_groups:
			for (uint32_t k = 0; k < subobjects[i].count; ++k)
				shader_groups.push_back(static_cast<const api::shader_group *>(subobjects[i].data)[k]);
			break;
		case api::pipeline_subobject_type::max_payload_size:
			assert(subobjects[i].count == 1);
			max_payload_size = *static_cast<const uint32_t *>(subobjects[i].data);
			break;
		case api::pipeline_subobject_type::max_attribute_size:
			assert(subobjects[i].count == 1);
			max_attribute_size = *static_cast<const uint32_t *>(subobjects[i].data);
			break;
		case api::pipeline_subobject_type::max_recursion_depth:
			assert(subobjects[i].count == 1);
			max_recursion_depth = *static_cast<const uint32_t *>(subobjects[i].data);
			break;
		case api::pipeline_subobject_type::flags:
			assert(subobjects[i].count == 1);
			flags = *static_cast<const api::pipeline_flags *>(subobjects[i].data);
			break;
		default:
			assert(false);
			goto exit_failure;
		}
	}

	if (!raygen_desc.empty() || !shader_groups.empty())
	{
		VkRayTracingPipelineCreateInfoKHR create_info { VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR };
		create_info.flags = convert_pipeline_flags(flags);
		create_info.layout = (VkPipelineLayout)layout.handle;
		create_info.maxPipelineRayRecursionDepth = max_recursion_depth;

		const size_t max_shader_stage_count = raygen_desc.size() + any_hit_desc.size() + closest_hit_desc.size() + miss_desc.size() + intersection_desc.size() + callable_desc.size();
		std::vector<VkPipelineShaderStageCreateInfo> shader_stage_info;
		std::vector<VkSpecializationInfo> spec_info;
		std::vector<std::vector<VkSpecializationMapEntry>> spec_map;
		shader_stage_info.reserve(max_shader_stage_count);
		spec_info.reserve(max_shader_stage_count);
		spec_map.reserve(max_shader_stage_count);

		for (const api::shader_desc &shader_desc : raygen_desc)
		{
			if (!create_shader_module(VK_SHADER_STAGE_RAYGEN_BIT_KHR, shader_desc, shader_stage_info.emplace_back(), spec_info.emplace_back(), spec_map.emplace_back()))
				goto exit_failure;
			shaders.push_back(shader_stage_info.back().module);
		}
		for (const api::shader_desc &shader_desc : any_hit_desc)
		{
			if (!create_shader_module(VK_SHADER_STAGE_ANY_HIT_BIT_KHR, shader_desc, shader_stage_info.emplace_back(), spec_info.emplace_back(), spec_map.emplace_back()))
				goto exit_failure;
			shaders.push_back(shader_stage_info.back().module);
		}
		for (const api::shader_desc &shader_desc : closest_hit_desc)
		{
			if (!create_shader_module(VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, shader_desc, shader_stage_info.emplace_back(), spec_info.emplace_back(), spec_map.emplace_back()))
				goto exit_failure;
			shaders.push_back(shader_stage_info.back().module);
		}
		for (const api::shader_desc &shader_desc : miss_desc)
		{
			if (!create_shader_module(VK_SHADER_STAGE_MISS_BIT_KHR, shader_desc, shader_stage_info.emplace_back(), spec_info.emplace_back(), spec_map.emplace_back()))
				goto exit_failure;
			shaders.push_back(shader_stage_info.back().module);
		}
		for (const api::shader_desc &shader_desc : intersection_desc)
		{
			if (!create_shader_module(VK_SHADER_STAGE_INTERSECTION_BIT_KHR, shader_desc, shader_stage_info.emplace_back(), spec_info.emplace_back(), spec_map.emplace_back()))
				goto exit_failure;
			shaders.push_back(shader_stage_info.back().module);
		}
		for (const api::shader_desc &shader_desc : callable_desc)
		{
			if (!create_shader_module(VK_SHADER_STAGE_CALLABLE_BIT_KHR, shader_desc, shader_stage_info.emplace_back(), spec_info.emplace_back(), spec_map.emplace_back()))
				goto exit_failure;
			shaders.push_back(shader_stage_info.back().module);
		}

		create_info.stageCount = static_cast<uint32_t>(shader_stage_info.size());
		create_info.pStages = shader_stage_info.data();

		std::vector<VkRayTracingShaderGroupCreateInfoKHR> group_infos;
		group_infos.reserve(shader_groups.size());

		bool any_null_any_hit = false;
		bool any_null_closest_hit = false;
		bool any_null_miss = false;
		bool any_null_intersection = false;

		for (const api::shader_group &shader_group : shader_groups)
		{
			VkRayTracingShaderGroupCreateInfoKHR &group_info = group_infos.emplace_back(VkRayTracingShaderGroupCreateInfoKHR { VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR });
			group_info.type = convert_shader_group_type(shader_group.type);

			switch (shader_group.type)
			{
			case api::shader_group_type::raygen:
				if (shader_group.raygen.shader_index != UINT32_MAX)
				{
					group_info.generalShader = shader_group.raygen.shader_index;
				}
				else
				{
					group_info.generalShader = VK_SHADER_UNUSED_KHR;
				}
				group_info.closestHitShader = VK_SHADER_UNUSED_KHR;
				group_info.anyHitShader = VK_SHADER_UNUSED_KHR;
				group_info.intersectionShader = VK_SHADER_UNUSED_KHR;
				break;
			case api::shader_group_type::miss:
				if (shader_group.miss.shader_index != UINT32_MAX)
				{
					group_info.generalShader = static_cast<uint32_t>(raygen_desc.size() + any_hit_desc.size() + closest_hit_desc.size()) + shader_group.miss.shader_index;
				}
				else
				{
					group_info.generalShader = VK_SHADER_UNUSED_KHR;
					any_null_miss = true;
				}
				group_info.closestHitShader = VK_SHADER_UNUSED_KHR;
				group_info.anyHitShader = VK_SHADER_UNUSED_KHR;
				group_info.intersectionShader = VK_SHADER_UNUSED_KHR;
				break;
			case api::shader_group_type::callable:
				if (shader_group.callable.shader_index != UINT32_MAX)
				{
					group_info.generalShader = static_cast<uint32_t>(raygen_desc.size() + any_hit_desc.size() + closest_hit_desc.size() + miss_desc.size() + intersection_desc.size()) + shader_group.callable.shader_index;
				}
				else
				{
					group_info.generalShader = VK_SHADER_UNUSED_KHR;
				}
				group_info.closestHitShader = VK_SHADER_UNUSED_KHR;
				group_info.anyHitShader = VK_SHADER_UNUSED_KHR;
				group_info.intersectionShader = VK_SHADER_UNUSED_KHR;
				break;
			case api::shader_group_type::hit_group_triangles:
			case api::shader_group_type::hit_group_aabbs:
				group_info.generalShader = VK_SHADER_UNUSED_KHR;
				if (shader_group.hit_group.any_hit_shader_index != UINT32_MAX)
				{
					group_info.anyHitShader = static_cast<uint32_t>(raygen_desc.size()) + shader_group.hit_group.any_hit_shader_index;
				}
				else
				{
					group_info.anyHitShader = VK_SHADER_UNUSED_KHR;
					any_null_any_hit = true;
				}
				if (shader_group.hit_group.closest_hit_shader_index != UINT32_MAX)
				{
					group_info.closestHitShader = static_cast<uint32_t>(raygen_desc.size() + any_hit_desc.size()) + shader_group.hit_group.closest_hit_shader_index;
				}
				else
				{
					group_info.closestHitShader = VK_SHADER_UNUSED_KHR;
					any_null_closest_hit = true;
				}
				if (shader_group.hit_group.intersection_shader_index != UINT32_MAX && shader_group.type != api::shader_group_type::hit_group_triangles)
				{
					group_info.intersectionShader = static_cast<uint32_t>(raygen_desc.size() + any_hit_desc.size() + closest_hit_desc.size() + miss_desc.size()) + shader_group.hit_group.intersection_shader_index;
				}
				else
				{
					group_info.intersectionShader = VK_SHADER_UNUSED_KHR;
					any_null_intersection = true;
				}
				break;
			}
		}

		create_info.groupCount = static_cast<uint32_t>(group_infos.size());
		create_info.pGroups = group_infos.data();

		if (!any_null_any_hit)
			create_info.flags |= VK_PIPELINE_CREATE_RAY_TRACING_NO_NULL_ANY_HIT_SHADERS_BIT_KHR;
		if (!any_null_closest_hit)
			create_info.flags |= VK_PIPELINE_CREATE_RAY_TRACING_NO_NULL_CLOSEST_HIT_SHADERS_BIT_KHR;
		if (!any_null_miss)
			create_info.flags |= VK_PIPELINE_CREATE_RAY_TRACING_NO_NULL_MISS_SHADERS_BIT_KHR;
		if (!any_null_intersection)
			create_info.flags |= VK_PIPELINE_CREATE_RAY_TRACING_NO_NULL_INTERSECTION_SHADERS_BIT_KHR;

		VkPipelineLibraryCreateInfoKHR library_info;
		VkRayTracingPipelineInterfaceCreateInfoKHR interface_info;
		if (!libraries.empty())
		{
			library_info = { VK_STRUCTURE_TYPE_PIPELINE_LIBRARY_CREATE_INFO_KHR };
			library_info.libraryCount = static_cast<uint32_t>(libraries.size());
			library_info.pLibraries = reinterpret_cast<const VkPipeline *>(libraries.data());

			interface_info = { VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_INTERFACE_CREATE_INFO_KHR };
			interface_info.maxPipelineRayPayloadSize = max_payload_size;
			interface_info.maxPipelineRayHitAttributeSize = max_attribute_size;

			create_info.pLibraryInfo = &library_info;
			create_info.pLibraryInterface = &interface_info;
		}

		if (VkPipeline object = VK_NULL_HANDLE;
			vk.CreateRayTracingPipelinesKHR(_orig, VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &create_info, nullptr, &object) == VK_SUCCESS)
		{
			for (const VkShaderModule shader : shaders)
				vk.DestroyShaderModule(_orig, shader, nullptr);

			*out_pipeline = { (uint64_t)object };
			return true;
		}
	}
	else if (cs_desc.code_size != 0)
	{
		VkComputePipelineCreateInfo create_info { VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };
		create_info.flags = convert_pipeline_flags(flags);
		create_info.layout = (VkPipelineLayout)layout.handle;

		VkSpecializationInfo spec_info;
		std::vector<VkSpecializationMapEntry> spec_map;

		if (cs_desc.code_size != 0)
		{
			if (!create_shader_module(VK_SHADER_STAGE_COMPUTE_BIT, cs_desc, create_info.stage, spec_info, spec_map))
				goto exit_failure;
			shaders.push_back(create_info.stage.module);
		}

		if (VkPipeline object = VK_NULL_HANDLE;
			vk.CreateComputePipelines(_orig, VK_NULL_HANDLE, 1, &create_info, nullptr, &object) == VK_SUCCESS)
		{
			vk.DestroyShaderModule(_orig, create_info.stage.module, nullptr);

			*out_pipeline = { (uint64_t)object };
			return true;
		}
	}
	else
	{
		VkGraphicsPipelineCreateInfo create_info { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
		create_info.flags = convert_pipeline_flags(flags);
		create_info.layout = (VkPipelineLayout)layout.handle;

		VkPipelineShaderStageCreateInfo shader_stage_info[6];
		create_info.pStages = shader_stage_info;

		VkSpecializationInfo spec_info[6];
		std::vector<VkSpecializationMapEntry> spec_map[6];

		if (vs_desc.code_size != 0)
		{
			if (!create_shader_module(VK_SHADER_STAGE_VERTEX_BIT, vs_desc, shader_stage_info[create_info.stageCount], spec_info[create_info.stageCount], spec_map[create_info.stageCount]))
				goto exit_failure;
			shaders.push_back(shader_stage_info[create_info.stageCount++].module);
		}
		if (hs_desc.code_size != 0)
		{
			if (!create_shader_module(VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT, hs_desc, shader_stage_info[create_info.stageCount], spec_info[create_info.stageCount], spec_map[create_info.stageCount]))
				goto exit_failure;
			shaders.push_back(shader_stage_info[create_info.stageCount++].module);
		}
		if (ds_desc.code_size != 0)
		{
			if (!create_shader_module(VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, ds_desc, shader_stage_info[create_info.stageCount], spec_info[create_info.stageCount], spec_map[create_info.stageCount]))
				goto exit_failure;
			shaders.push_back(shader_stage_info[create_info.stageCount++].module);
		}
		if (gs_desc.code_size != 0)
		{
			if (!create_shader_module(VK_SHADER_STAGE_GEOMETRY_BIT, gs_desc, shader_stage_info[create_info.stageCount], spec_info[create_info.stageCount], spec_map[create_info.stageCount]))
				goto exit_failure;
			shaders.push_back(shader_stage_info[create_info.stageCount++].module);
		}
		if (ps_desc.code_size != 0)
		{
			if (!create_shader_module(VK_SHADER_STAGE_FRAGMENT_BIT, ps_desc, shader_stage_info[create_info.stageCount], spec_info[create_info.stageCount], spec_map[create_info.stageCount]))
				goto exit_failure;
			shaders.push_back(shader_stage_info[create_info.stageCount++].module);
		}
		if (as_desc.code_size != 0)
		{
			if (!create_shader_module(VK_SHADER_STAGE_TASK_BIT_EXT, as_desc, shader_stage_info[create_info.stageCount], spec_info[create_info.stageCount], spec_map[create_info.stageCount]))
				goto exit_failure;
			shaders.push_back(shader_stage_info[create_info.stageCount++].module);
		}
		if (ms_desc.code_size != 0)
		{
			if (!create_shader_module(VK_SHADER_STAGE_MESH_BIT_EXT, ms_desc, shader_stage_info[create_info.stageCount], spec_info[create_info.stageCount], spec_map[create_info.stageCount]))
				goto exit_failure;
			shaders.push_back(shader_stage_info[create_info.stageCount++].module);
		}

		std::vector<VkDynamicState> dyn_states;
		// Always make scissor rectangles and viewports dynamic
		dyn_states.push_back(VK_DYNAMIC_STATE_SCISSOR);
		dyn_states.push_back(VK_DYNAMIC_STATE_VIEWPORT);
		convert_dynamic_states(dynamic_states_subobject.count, static_cast<const api::dynamic_state *>(dynamic_states_subobject.data), dyn_states);

		VkPipelineDynamicStateCreateInfo dynamic_state_info { VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
		create_info.pDynamicState = &dynamic_state_info;
		dynamic_state_info.dynamicStateCount = static_cast<uint32_t>(dyn_states.size());
		dynamic_state_info.pDynamicStates = dyn_states.data();

		std::vector<VkVertexInputBindingDescription> vertex_bindings;
		std::vector<VkVertexInputAttributeDescription> vertex_attributes;
		convert_input_layout_desc(input_layout_desc.count, static_cast<const api::input_element *>(input_layout_desc.data), vertex_bindings, vertex_attributes);

		VkPipelineVertexInputStateCreateInfo vertex_input_state_info { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
		create_info.pVertexInputState = &vertex_input_state_info;
		vertex_input_state_info.vertexBindingDescriptionCount = static_cast<uint32_t>(vertex_bindings.size());
		vertex_input_state_info.pVertexBindingDescriptions = vertex_bindings.data();
		vertex_input_state_info.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertex_attributes.size());
		vertex_input_state_info.pVertexAttributeDescriptions = vertex_attributes.data();

		VkPipelineInputAssemblyStateCreateInfo input_assembly_state_info { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
		create_info.pInputAssemblyState = &input_assembly_state_info;
		input_assembly_state_info.primitiveRestartEnable = VK_FALSE;
		input_assembly_state_info.topology = convert_primitive_topology(topology);

		VkPipelineTessellationStateCreateInfo tessellation_state_info { VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO };
		create_info.pTessellationState = &tessellation_state_info;
		if (input_assembly_state_info.topology == VK_PRIMITIVE_TOPOLOGY_PATCH_LIST)
			tessellation_state_info.patchControlPoints = static_cast<uint32_t>(topology) - static_cast<uint32_t>(api::primitive_topology::patch_list_01_cp) + 1;

		VkPipelineViewportStateCreateInfo viewport_state_info { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
		create_info.pViewportState = &viewport_state_info;
		viewport_state_info.scissorCount = viewport_count;
		viewport_state_info.viewportCount = viewport_count;

		VkPipelineRasterizationStateCreateInfo rasterization_state_info { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
		create_info.pRasterizationState = &rasterization_state_info;

		VkPipelineRasterizationConservativeStateCreateInfoEXT conservative_rasterization_info;
		if (rasterizer_desc.conservative_rasterization != 0)
		{
			if (!_conservative_rasterization_ext)
				goto exit_failure;

			conservative_rasterization_info = { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_CONSERVATIVE_STATE_CREATE_INFO_EXT };
			conservative_rasterization_info.pNext = rasterization_state_info.pNext;

			rasterization_state_info.pNext = &conservative_rasterization_info;
		}

		convert_rasterizer_desc(rasterizer_desc, rasterization_state_info);
		rasterization_state_info.rasterizerDiscardEnable = VK_FALSE;
		rasterization_state_info.lineWidth = 1.0f;

		VkPipelineRasterizationStateStreamCreateInfoEXT stream_rasterization_info;
		if (stream_output_desc.rasterized_stream != 0)
		{
			stream_rasterization_info = { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_STREAM_CREATE_INFO_EXT };
			stream_rasterization_info.pNext = rasterization_state_info.pNext;

			rasterization_state_info.pNext = &stream_rasterization_info;

			convert_stream_output_desc(stream_output_desc, rasterization_state_info);
		}

		VkPipelineMultisampleStateCreateInfo multisample_state_info { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
		create_info.pMultisampleState = &multisample_state_info;
		multisample_state_info.rasterizationSamples = sample_count != 0 ? static_cast<VkSampleCountFlagBits>(sample_count) : VK_SAMPLE_COUNT_1_BIT;
		multisample_state_info.sampleShadingEnable = VK_FALSE;
		multisample_state_info.minSampleShading = 0.0f;
		multisample_state_info.alphaToOneEnable = VK_FALSE;
		multisample_state_info.pSampleMask = &sample_mask;

		VkPipelineDepthStencilStateCreateInfo depth_stencil_state_info { VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
		create_info.pDepthStencilState = &depth_stencil_state_info;
		convert_depth_stencil_desc(depth_stencil_desc, depth_stencil_state_info);
		depth_stencil_state_info.depthBoundsTestEnable = VK_FALSE;
		depth_stencil_state_info.minDepthBounds = 0.0f;
		depth_stencil_state_info.maxDepthBounds = 1.0f;

		VkPipelineColorBlendStateCreateInfo color_blend_state_info { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
		create_info.pColorBlendState = &color_blend_state_info;
		color_blend_state_info.attachmentCount = 8;
		temp_mem<VkPipelineColorBlendAttachmentState, 8> attachment_info(color_blend_state_info.attachmentCount);
		color_blend_state_info.pAttachments = attachment_info.p;
		convert_blend_desc(blend_desc, color_blend_state_info, multisample_state_info);

		temp_mem<VkFormat, 8> attachment_formats(render_target_formats.count);
		color_blend_state_info.attachmentCount = 0;
		for (uint32_t i = 0; i < render_target_formats.count; ++i, ++color_blend_state_info.attachmentCount)
		{
			attachment_formats[i] = convert_format(static_cast<const api::format *>(render_target_formats.data)[i]);
			assert(attachment_formats[i] != VK_FORMAT_UNDEFINED);
		}

		VkPipelineRenderingCreateInfo dynamic_rendering_info;
		if (_dynamic_rendering_ext)
		{
			dynamic_rendering_info = { VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
			dynamic_rendering_info.pNext = create_info.pNext;
			dynamic_rendering_info.colorAttachmentCount = color_blend_state_info.attachmentCount;
			dynamic_rendering_info.pColorAttachmentFormats = attachment_formats.p;

			const VkFormat depth_stencil_format_vk = convert_format(depth_stencil_format);
			if (aspect_flags_from_format(depth_stencil_format_vk) & VK_IMAGE_ASPECT_DEPTH_BIT)
				dynamic_rendering_info.depthAttachmentFormat = depth_stencil_format_vk;
			if (aspect_flags_from_format(depth_stencil_format_vk) & VK_IMAGE_ASPECT_STENCIL_BIT)
				dynamic_rendering_info.stencilAttachmentFormat = depth_stencil_format_vk;

			create_info.pNext = &dynamic_rendering_info;
		}
		else
		{
			const uint32_t max_attachments = color_blend_state_info.attachmentCount + 1;

			temp_mem<VkAttachmentReference, 9> attach_refs(max_attachments);
			temp_mem<VkAttachmentDescription, 9> attach_descs(max_attachments);

			VkSubpassDependency subdep = {};
			subdep.srcSubpass = VK_SUBPASS_EXTERNAL;
			subdep.dstSubpass = 0;
			subdep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			subdep.dstStageMask = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;
			subdep.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			subdep.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

			VkSubpassDescription subpass = {};
			subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
			subpass.colorAttachmentCount = color_blend_state_info.attachmentCount;
			subpass.pColorAttachments = attach_refs.p;
			subpass.pDepthStencilAttachment = (depth_stencil_format != api::format::unknown) ? &attach_refs[color_blend_state_info.attachmentCount] : nullptr;

			VkRenderPassCreateInfo render_pass_create_info { VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
			render_pass_create_info.attachmentCount = subpass.colorAttachmentCount + (subpass.pDepthStencilAttachment != nullptr ? 1 : 0);
			render_pass_create_info.pAttachments = attach_descs.p;
			render_pass_create_info.subpassCount = 1;
			render_pass_create_info.pSubpasses = &subpass;
			render_pass_create_info.dependencyCount = 1;
			render_pass_create_info.pDependencies = &subdep;

			for (uint32_t i = 0; i < subpass.colorAttachmentCount; ++i)
			{
				VkAttachmentReference &attach_ref = attach_refs[i];
				attach_ref.attachment = i;
				attach_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

				VkAttachmentDescription &attach_desc = attach_descs[i];
				attach_desc.flags = 0;
				attach_desc.format = attachment_formats[i];
				attach_desc.samples = multisample_state_info.rasterizationSamples;
				attach_desc.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
				attach_desc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
				attach_desc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
				attach_desc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
				attach_desc.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
				attach_desc.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			}

			if (subpass.pDepthStencilAttachment != nullptr)
			{
				VkAttachmentReference &attach_ref = attach_refs[subpass.colorAttachmentCount];
				attach_ref.attachment = subpass.colorAttachmentCount;
				attach_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

				VkAttachmentDescription &attach_desc = attach_descs[subpass.colorAttachmentCount];
				attach_desc.flags = 0;
				attach_desc.format = convert_format(depth_stencil_format);
				attach_desc.samples = multisample_state_info.rasterizationSamples;
				attach_desc.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
				attach_desc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
				attach_desc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
				attach_desc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
				attach_desc.initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
				attach_desc.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
			}

			if (vk.CreateRenderPass(_orig, &render_pass_create_info, nullptr, &render_pass) != VK_SUCCESS)
				goto exit_failure;

			create_info.renderPass = render_pass;
		}

		VkPipelineLibraryCreateInfoKHR library_info;
		if (!libraries.empty())
		{
			library_info = { VK_STRUCTURE_TYPE_PIPELINE_LIBRARY_CREATE_INFO_KHR };
			library_info.pNext = create_info.pNext;
			library_info.libraryCount = static_cast<uint32_t>(libraries.size());
			library_info.pLibraries = reinterpret_cast<const VkPipeline *>(libraries.data());

			create_info.pNext = &library_info;
		}

		if (VkPipeline object = VK_NULL_HANDLE;
			vk.CreateGraphicsPipelines(_orig, VK_NULL_HANDLE, 1, &create_info, nullptr, &object) == VK_SUCCESS)
		{
			if (render_pass != VK_NULL_HANDLE)
				vk.DestroyRenderPass(_orig, render_pass, nullptr);

			for (const VkShaderModule shader : shaders)
				vk.DestroyShaderModule(_orig, shader, nullptr);

			*out_pipeline = { (uint64_t)object };
			return true;
		}
	}

exit_failure:
	if (render_pass != VK_NULL_HANDLE)
		vk.DestroyRenderPass(_orig, render_pass, nullptr);

	for (const VkShaderModule shader : shaders)
		vk.DestroyShaderModule(_orig, shader, nullptr);

	*out_pipeline = { 0 };
	return false;
}
void reshade::vulkan::device_impl::destroy_pipeline(api::pipeline pipeline)
{
	vk.DestroyPipeline(_orig, (VkPipeline)pipeline.handle, nullptr);
}

bool reshade::vulkan::device_impl::create_pipeline_layout(uint32_t param_count, const api::pipeline_layout_param *params, api::pipeline_layout *out_layout)
{
	std::vector<VkPushConstantRange> push_constant_ranges;
	std::vector<VkDescriptorSetLayout> set_layouts;
	std::vector<VkSampler> embedded_samplers;
	set_layouts.reserve(param_count);
	push_constant_ranges.reserve(param_count);

	uint32_t i = 0;

	// Push constant ranges have to be at the end of the layout description
	for (; i < param_count && params[i].type != api::pipeline_layout_param_type::push_constants; ++i)
	{
		bool push_descriptors = (params[i].type == api::pipeline_layout_param_type::push_descriptors);
		const bool with_static_samplers = (params[i].type == api::pipeline_layout_param_type::descriptor_table_with_static_samplers || params[i].type == api::pipeline_layout_param_type::push_descriptors_with_static_samplers);
		const uint32_t range_count = push_descriptors ? 1 : with_static_samplers ? params[i].descriptor_table_with_static_samplers.count : params[i].descriptor_table.count;
		const api::descriptor_range_with_static_samplers *range = static_cast<const api::descriptor_range_with_static_samplers *>(push_descriptors ? &params[i].push_descriptors : with_static_samplers ? params[i].descriptor_table_with_static_samplers.ranges : params[i].descriptor_table.ranges);
		push_descriptors |= (params[i].type == api::pipeline_layout_param_type::push_descriptors_with_ranges || params[i].type == api::pipeline_layout_param_type::push_descriptors_with_static_samplers);

		object_data<VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT> data;
		data.ranges.reserve(range_count);
		data.binding_to_offset.reserve(range_count);

		std::vector<VkDescriptorSetLayoutBinding> internal_bindings;
		std::vector<std::vector<VkSampler>> internal_samplers;
		internal_bindings.reserve(range_count);
		internal_samplers.reserve(range_count);

		for (uint32_t k = 0, offset = 0; k < range_count; ++k, range = (with_static_samplers ? range + 1 : reinterpret_cast<const api::descriptor_range_with_static_samplers *>(reinterpret_cast<const api::descriptor_range *>(range) + 1)))
		{
			data.ranges.push_back(*static_cast<const api::descriptor_range *>(range));

			if (range->count == 0)
				continue;

			const uint32_t max_binding = range->binding + (range->count - range->array_size);
			if (max_binding >= data.binding_to_offset.size())
				data.binding_to_offset.resize(max_binding + 1);
			data.binding_to_offset[range->binding] = offset;

			VkDescriptorSetLayoutBinding &internal_binding = internal_bindings.emplace_back();
			internal_binding.binding = range->binding;
			internal_binding.descriptorType = convert_descriptor_type(range->type);
			internal_binding.descriptorCount = range->array_size;
			internal_binding.stageFlags = static_cast<VkShaderStageFlags>(range->visibility);

			offset += internal_binding.descriptorCount;

			if (with_static_samplers && (range->type == api::descriptor_type::sampler || range->type == api::descriptor_type::sampler_with_resource_view) && range->static_samplers != nullptr)
			{
				if (range->array_size != 1)
					goto exit_failure;

				std::vector<VkSampler> &internal_binding_samplers = internal_samplers.emplace_back();
				internal_binding_samplers.resize(range->count);

				for (uint32_t j = 0; j < range->count; ++j)
				{
					VkSamplerCreateInfo create_info { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
					// Cannot have custom border color in immutable samplers
					convert_sampler_desc(range->static_samplers[j], create_info);

					if (vk.CreateSampler(_orig, &create_info, nullptr, &embedded_samplers.emplace_back()) != VK_SUCCESS)
						goto exit_failure;

					internal_binding_samplers[j] = embedded_samplers.back();
				}

				internal_binding.pImmutableSamplers = internal_binding_samplers.data();
			}

			if (range->count == UINT32_MAX)
				continue; // Skip unbounded ranges

			// Add additional bindings if the total descriptor count exceeds the array size of the binding
			for (uint32_t j = 0; j < (range->count - range->array_size); ++j)
			{
				data.binding_to_offset[range->binding + 1 + j] = offset;

				VkDescriptorSetLayoutBinding &additional_binding = internal_bindings.emplace_back();
				additional_binding.binding = range->binding + 1 + j;
				additional_binding.descriptorType = convert_descriptor_type(range->type);
				additional_binding.descriptorCount = 1;
				additional_binding.stageFlags = static_cast<VkShaderStageFlags>(range->visibility);

				offset += additional_binding.descriptorCount;
			}
		}

		VkDescriptorSetLayoutCreateInfo create_info { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
		create_info.bindingCount = static_cast<uint32_t>(internal_bindings.size());
		create_info.pBindings = internal_bindings.data();

		if (push_descriptors && _push_descriptor_ext)
			create_info.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR;

		if (vk.CreateDescriptorSetLayout(_orig, &create_info, nullptr, &set_layouts.emplace_back()) == VK_SUCCESS)
		{
			register_object<VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT>(set_layouts.back(), std::move(data));
		}
		else
		{
			goto exit_failure;
		}
	}

	for (; i < param_count && params[i].type == api::pipeline_layout_param_type::push_constants; ++i)
	{
		VkPushConstantRange &push_constant_range = push_constant_ranges.emplace_back();
		push_constant_range.stageFlags = static_cast<VkShaderStageFlagBits>(params[i].push_constants.visibility);
		push_constant_range.offset = 0;
		push_constant_range.size = params[i].push_constants.count * 4;
	}

	if (i < param_count)
		goto exit_failure;

	{
		VkPipelineLayoutCreateInfo create_info { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
		create_info.setLayoutCount = static_cast<uint32_t>(set_layouts.size());
		create_info.pSetLayouts =  set_layouts.data();
		create_info.pushConstantRangeCount = static_cast<uint32_t>(push_constant_ranges.size());
		create_info.pPushConstantRanges = push_constant_ranges.data();

		if (VkPipelineLayout object = VK_NULL_HANDLE;
			vk.CreatePipelineLayout(_orig, &create_info, nullptr, &object) == VK_SUCCESS)
		{
			object_data<VK_OBJECT_TYPE_PIPELINE_LAYOUT> data;
			data.set_layouts = std::move(set_layouts);
			data.embedded_samplers = std::move(embedded_samplers);

			register_object<VK_OBJECT_TYPE_PIPELINE_LAYOUT>(object, std::move(data));

			*out_layout = { (uint64_t)object };
			return true;
		}
	}

exit_failure:
	for (const VkSampler sampler : embedded_samplers)
	{
		vk.DestroySampler(_orig, sampler, nullptr);
	}

	for (const VkDescriptorSetLayout set_layout : set_layouts)
	{
		unregister_object<VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT>(set_layout);

		vk.DestroyDescriptorSetLayout(_orig, set_layout, nullptr);
	}

	*out_layout = { 0 };
	return false;
}
void reshade::vulkan::device_impl::destroy_pipeline_layout(api::pipeline_layout layout)
{
	if (layout == 0)
		return;

	const auto layout_data = get_private_data_for_object<VK_OBJECT_TYPE_PIPELINE_LAYOUT>((VkPipelineLayout)layout.handle);

	for (const VkSampler sampler : layout_data->embedded_samplers)
	{
		vk.DestroySampler(_orig, sampler, nullptr);
	}

	for (const VkDescriptorSetLayout set_layout : layout_data->set_layouts)
	{
		unregister_object<VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT>(set_layout);

		vk.DestroyDescriptorSetLayout(_orig, set_layout, nullptr);
	}

	unregister_object<VK_OBJECT_TYPE_PIPELINE_LAYOUT>((VkPipelineLayout)layout.handle);

	vk.DestroyPipelineLayout(_orig, (VkPipelineLayout)layout.handle, nullptr);
}

bool reshade::vulkan::device_impl::allocate_descriptor_tables(uint32_t count, api::pipeline_layout layout, uint32_t layout_param, api::descriptor_table *out_tables)
{
	const auto layout_data = get_private_data_for_object<VK_OBJECT_TYPE_PIPELINE_LAYOUT>((VkPipelineLayout)layout.handle);

	std::vector<VkDescriptorSetLayout> set_layouts(count, layout_data->set_layouts[layout_param]);

	VkDescriptorSetAllocateInfo alloc_info { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
	alloc_info.descriptorPool = _descriptor_pool;
	alloc_info.descriptorSetCount = count;
	alloc_info.pSetLayouts = set_layouts.data();

	// Access to descriptor pools must be externally synchronized, so lock for the duration of allocation from the global descriptor pool
	if (std::unique_lock<std::shared_mutex> lock(_mutex);
		vk.AllocateDescriptorSets(_orig, &alloc_info, reinterpret_cast<VkDescriptorSet *>(out_tables)) == VK_SUCCESS)
	{
		lock.unlock();

		for (uint32_t i = 0; i < count; ++i)
		{
			object_data<VK_OBJECT_TYPE_DESCRIPTOR_SET> data;
			data.pool = VK_NULL_HANDLE; // 'get_descriptor_heap_offset' is not supported for the internal pool
			data.offset = 0;
			data.layout = layout_data->set_layouts[layout_param];

			register_object<VK_OBJECT_TYPE_DESCRIPTOR_SET>((VkDescriptorSet)out_tables[i].handle, std::move(data));
		}

		return true;
	}
	else
	{
		return false;
	}
}
void reshade::vulkan::device_impl::free_descriptor_tables(uint32_t count, const api::descriptor_table *tables)
{
	for (uint32_t i = 0; i < count; ++i)
		unregister_object<VK_OBJECT_TYPE_DESCRIPTOR_SET>((VkDescriptorSet)tables[i].handle);

	// Access to descriptor pools must be externally synchronized, so lock for the duration of freeing in the global descriptor pool
	const std::unique_lock<std::shared_mutex> lock(_mutex);

	vk.FreeDescriptorSets(_orig, _descriptor_pool, count, reinterpret_cast<const VkDescriptorSet *>(tables));
}

void reshade::vulkan::device_impl::get_descriptor_heap_offset(api::descriptor_table table, uint32_t binding, uint32_t array_offset, api::descriptor_heap *heap, uint32_t *offset) const
{
	assert(heap != nullptr && offset != nullptr);

	const auto set_data = get_private_data_for_object<VK_OBJECT_TYPE_DESCRIPTOR_SET>((VkDescriptorSet)table.handle);
	const auto set_layout_data = get_private_data_for_object<VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT>(set_data->layout);

	*heap = { (uint64_t)set_data->pool };
	*offset = set_data->offset + set_layout_data->binding_to_offset[binding] + array_offset;
}

void reshade::vulkan::device_impl::copy_descriptor_tables(uint32_t count, const api::descriptor_table_copy *copies)
{
	std::vector<VkCopyDescriptorSet> copies_internal;
	copies_internal.reserve(count);

	for (uint32_t i = 0; i < count; ++i)
	{
		const api::descriptor_table_copy &copy = copies[i];

		if (copy.count == 0)
			continue;

		VkCopyDescriptorSet &copy_internal = copies_internal.emplace_back();
		copy_internal = { VK_STRUCTURE_TYPE_COPY_DESCRIPTOR_SET };
		copy_internal.dstSet = (VkDescriptorSet)copy.dest_table.handle;
		copy_internal.dstBinding = copy.dest_binding;
		copy_internal.dstArrayElement = copy.dest_array_offset;
		copy_internal.srcSet = (VkDescriptorSet)copy.source_table.handle;
		copy_internal.srcBinding = copy.source_binding;
		copy_internal.srcArrayElement = copy.source_array_offset;
		copy_internal.descriptorCount = copy.count;
	}

	vk.UpdateDescriptorSets(_orig, 0, nullptr, static_cast<uint32_t>(copies_internal.size()), copies_internal.data());
}
void reshade::vulkan::device_impl::update_descriptor_tables(uint32_t count, const api::descriptor_table_update *updates)
{
	std::vector<VkWriteDescriptorSet> writes_internal;
	writes_internal.reserve(count);

	constexpr size_t extra_data_size = std::max(sizeof(VkDescriptorImageInfo), sizeof(VkWriteDescriptorSetAccelerationStructureKHR)) / sizeof(uint64_t);
	uint32_t max_descriptors = 0;
	for (uint32_t i = 0; i < count; ++i)
		max_descriptors += updates[i].count;
	temp_mem<uint64_t> extra_data(max_descriptors * extra_data_size);

	for (uint32_t i = 0, j = 0; i < count; ++i)
	{
		const api::descriptor_table_update &update = updates[i];

		if (update.count == 0)
			continue;

		VkWriteDescriptorSet &write = writes_internal.emplace_back();
		write = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
		write.dstSet = (VkDescriptorSet)update.table.handle;
		write.dstBinding = update.binding;
		write.dstArrayElement = update.array_offset;
		write.descriptorCount = update.count;
		write.descriptorType = convert_descriptor_type(update.type);

		switch (update.type)
		{
		case api::descriptor_type::sampler:
		{
			const auto image_info = reinterpret_cast<VkDescriptorImageInfo *>(extra_data.p + j * extra_data_size);
			write.pImageInfo = image_info;
			for (uint32_t k = 0; k < update.count; ++k, ++j)
			{
				const auto &descriptor = static_cast<const api::sampler *>(update.descriptors)[k];
				image_info[k].sampler = (VkSampler)descriptor.handle;
				image_info[k].imageView = VK_NULL_HANDLE;
				image_info[k].imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			}
			break;
		}
		case api::descriptor_type::sampler_with_resource_view:
		{
			const auto image_info = reinterpret_cast<VkDescriptorImageInfo *>(extra_data.p + j * extra_data_size);
			write.pImageInfo = image_info;
			for (uint32_t k = 0; k < update.count; ++k, ++j)
			{
				const auto &descriptor = static_cast<const api::sampler_with_resource_view *>(update.descriptors)[k];
				image_info[k].sampler = (VkSampler)descriptor.sampler.handle;
				image_info[k].imageView = (VkImageView)descriptor.view.handle;
				image_info[k].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			}
			break;
		}
		case api::descriptor_type::texture_shader_resource_view:
		case api::descriptor_type::texture_unordered_access_view:
		{
				const auto image_info = reinterpret_cast<VkDescriptorImageInfo *>(extra_data.p + j * extra_data_size);
				write.pImageInfo = image_info;
				for (uint32_t k = 0; k < update.count; ++k, ++j)
				{
					const auto &descriptor = static_cast<const api::resource_view *>(update.descriptors)[k];
					image_info[k].sampler = VK_NULL_HANDLE;
					image_info[k].imageView = (VkImageView)descriptor.handle;
					image_info[k].imageLayout = (update.type == api::descriptor_type::texture_unordered_access_view) ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				}
			break;
		}
		case api::descriptor_type::buffer_shader_resource_view:
		case api::descriptor_type::buffer_unordered_access_view:
			write.pTexelBufferView = reinterpret_cast<const VkBufferView *>(update.descriptors);
			break;
		case api::descriptor_type::constant_buffer:
		case api::descriptor_type::shader_storage_buffer:
			write.pBufferInfo = static_cast<const VkDescriptorBufferInfo *>(update.descriptors);
			break;
		case api::descriptor_type::acceleration_structure:
		{
			const auto as_info = reinterpret_cast<VkWriteDescriptorSetAccelerationStructureKHR *>(extra_data.p + j++ * extra_data_size);
			write.pNext = as_info;
			as_info[0] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR };
			as_info->accelerationStructureCount = update.count;
			as_info->pAccelerationStructures = static_cast<const VkAccelerationStructureKHR *>(update.descriptors);
			break;
		}
		default:
			assert(false);
			break;
		}
	}

	vk.UpdateDescriptorSets(_orig, static_cast<uint32_t>(writes_internal.size()), writes_internal.data(), 0, nullptr);
}

bool reshade::vulkan::device_impl::create_query_heap(api::query_type type, uint32_t count, api::query_heap *out_heap)
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
		object_data<VK_OBJECT_TYPE_QUERY_POOL> data;
		data.type = create_info.queryType;

		register_object<VK_OBJECT_TYPE_QUERY_POOL>(pool, std::move(data));

		// Reset all queries for initial use
#if 1
		if (const auto immediate_command_list = get_first_immediate_command_list())
		{
			vk.CmdResetQueryPool(immediate_command_list->_orig, pool, 0, count);

			immediate_command_list->_has_commands = true;

			VkSubmitInfo semaphore_info { VK_STRUCTURE_TYPE_SUBMIT_INFO };
			immediate_command_list->flush(semaphore_info);
		}
#else
		vk.ResetQueryPool(_orig, pool, 0, count);
#endif

		*out_heap = { (uint64_t)pool };
		return true;
	}
	else
	{
		*out_heap = { 0 };
		return false;
	}
}
void reshade::vulkan::device_impl::destroy_query_heap(api::query_heap heap)
{
	if (heap == 0)
		return;

	unregister_object<VK_OBJECT_TYPE_QUERY_POOL>((VkQueryPool)heap.handle);

	vk.DestroyQueryPool(_orig, (VkQueryPool)heap.handle, nullptr);
}

bool reshade::vulkan::device_impl::get_query_heap_results(api::query_heap heap, uint32_t first, uint32_t count, void *results, uint32_t stride)
{
	assert(heap != 0);
	assert(stride >= sizeof(uint64_t));

	return vk.GetQueryPoolResults(_orig, (VkQueryPool)heap.handle, first, count, static_cast<size_t>(count) * stride, results, stride, VK_QUERY_RESULT_64_BIT) == VK_SUCCESS;
}

void reshade::vulkan::device_impl::set_resource_name(api::resource resource, const char *name)
{
#ifdef VK_EXT_debug_utils
	if (vk.SetDebugUtilsObjectNameEXT == nullptr)
		return;

	const auto data = get_private_data_for_object<VK_OBJECT_TYPE_IMAGE>((VkImage)resource.handle);

	VkDebugUtilsObjectNameInfoEXT name_info { VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT };
	name_info.objectType = data->create_info.sType == VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO ? VK_OBJECT_TYPE_IMAGE : VK_OBJECT_TYPE_BUFFER;
	name_info.objectHandle = resource.handle;
	name_info.pObjectName = name;

	vk.SetDebugUtilsObjectNameEXT(_orig, &name_info);
#endif
}
void reshade::vulkan::device_impl::set_resource_view_name(api::resource_view view, const char *name)
{
#ifdef VK_EXT_debug_utils
	if (vk.SetDebugUtilsObjectNameEXT == nullptr)
		return;

	const auto data = get_private_data_for_object<VK_OBJECT_TYPE_IMAGE_VIEW>((VkImageView)view.handle);

	VkDebugUtilsObjectNameInfoEXT name_info { VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT };
	name_info.objectType = data->create_info.sType == VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO ? VK_OBJECT_TYPE_IMAGE_VIEW : VK_OBJECT_TYPE_BUFFER_VIEW;
	name_info.objectHandle = view.handle;
	name_info.pObjectName = name;

	vk.SetDebugUtilsObjectNameEXT(_orig, &name_info);
#endif
}

bool reshade::vulkan::device_impl::create_fence(uint64_t initial_value, api::fence_flags flags, api::fence *out_fence, HANDLE *shared_handle)
{
	*out_fence = { 0 };

	if (!_timeline_semaphore_ext)
		return false;

	VkSemaphoreTypeCreateInfo type_create_info { VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO };
	type_create_info.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
	type_create_info.initialValue = initial_value;

	VkExternalSemaphoreHandleTypeFlagBits handle_type = {};
	VkExportSemaphoreCreateInfo export_info;

	const bool is_shared = (flags & api::fence_flags::shared) != 0;
	if (is_shared)
	{
		if (shared_handle == nullptr)
			return false;

		if ((flags & api::fence_flags::shared_nt_handle) != 0)
			handle_type = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_BIT;
		else
			handle_type = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_KMT_BIT;

		if (*shared_handle != nullptr)
		{
			if (vk.ImportSemaphoreWin32HandleKHR == nullptr)
				return false;
		}
		else
		{
			if (vk.GetSemaphoreWin32HandleKHR == nullptr)
				return false;

			export_info = { VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_CREATE_INFO };
			export_info.handleTypes = handle_type;

			type_create_info.pNext = &export_info;
		}
	}

	VkSemaphoreCreateInfo create_info { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, &type_create_info };

	if (VkSemaphore semaphore = VK_NULL_HANDLE;
		vk.CreateSemaphore(_orig, &create_info, nullptr, &semaphore) == VK_SUCCESS)
	{
		if (is_shared)
		{
			if (*shared_handle != nullptr)
			{
				VkImportSemaphoreWin32HandleInfoKHR import_info { VK_STRUCTURE_TYPE_IMPORT_SEMAPHORE_WIN32_HANDLE_INFO_KHR };
				import_info.semaphore = semaphore;
				import_info.handleType = handle_type;
				import_info.handle = *shared_handle;

				if (vk.ImportSemaphoreWin32HandleKHR(_orig, &import_info) != VK_SUCCESS)
				{
					vk.DestroySemaphore(_orig, semaphore, nullptr);
					return false;
				}
			}
			else
			{
				VkSemaphoreGetWin32HandleInfoKHR handle_info { VK_STRUCTURE_TYPE_SEMAPHORE_GET_WIN32_HANDLE_INFO_KHR };
				handle_info.semaphore = semaphore;
				handle_info.handleType = handle_type;

				if (vk.GetSemaphoreWin32HandleKHR(_orig, &handle_info, shared_handle) != VK_SUCCESS)
				{
					vk.DestroySemaphore(_orig, semaphore, nullptr);
					return false;
				}
			}
		}

		*out_fence = { (uint64_t)semaphore };
		return true;
	}
	else
	{
		return false;
	}
}
void reshade::vulkan::device_impl::destroy_fence(api::fence fence)
{
	vk.DestroySemaphore(_orig, (VkSemaphore)fence.handle, nullptr);
}

uint64_t reshade::vulkan::device_impl::get_completed_fence_value(api::fence fence) const
{
	uint64_t value = 0;
	vk.GetSemaphoreCounterValue(_orig, (VkSemaphore)fence.handle, &value);
	return value;
}

bool reshade::vulkan::device_impl::wait(api::fence fence, uint64_t value, uint64_t timeout)
{
	const VkSemaphore wait_semaphore = (VkSemaphore)fence.handle;

	VkSemaphoreWaitInfo wait_info { VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO };
	wait_info.semaphoreCount = 1;
	wait_info.pSemaphores = &wait_semaphore;
	wait_info.pValues = &value;

	return vk.WaitSemaphores(_orig, &wait_info, timeout) == VK_SUCCESS;
}

bool reshade::vulkan::device_impl::signal(api::fence fence, uint64_t value)
{
	VkSemaphoreSignalInfo signal_info { VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO };
	signal_info.semaphore = (VkSemaphore)fence.handle;
	signal_info.value = value;

	return vk.SignalSemaphore(_orig, &signal_info) == VK_SUCCESS;
}

void reshade::vulkan::device_impl::get_acceleration_structure_size(api::acceleration_structure_type type, api::acceleration_structure_build_flags flags, uint32_t input_count, const api::acceleration_structure_build_input *inputs, uint64_t *out_size, uint64_t *out_build_scratch_size, uint64_t *out_update_scratch_size) const
{
	std::vector<VkAccelerationStructureGeometryKHR> geometries(input_count, { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR });
	std::vector<VkAccelerationStructureBuildRangeInfoKHR> range_infos(input_count);
	std::vector<uint32_t> max_primitive_counts(input_count);
	for (uint32_t i = 0; i < input_count; ++i)
	{
		convert_acceleration_structure_build_input(inputs[i], geometries[i], range_infos[i]);
		max_primitive_counts[i] = range_infos[i].primitiveCount;
	}

	VkAccelerationStructureBuildGeometryInfoKHR info { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR };
	info.type = convert_acceleration_structure_type(type);
	info.flags = convert_acceleration_structure_build_flags(flags);
	info.geometryCount = static_cast<uint32_t>(geometries.size());
	info.pGeometries = geometries.data();

	VkAccelerationStructureBuildSizesInfoKHR size_info { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };

	if (vk.GetAccelerationStructureBuildSizesKHR != nullptr)
		vk.GetAccelerationStructureBuildSizesKHR(_orig, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &info, max_primitive_counts.data(), &size_info);

	if (out_size != nullptr)
		*out_size = size_info.accelerationStructureSize;
	if (out_build_scratch_size != nullptr)
		*out_build_scratch_size = size_info.buildScratchSize;
	if (out_update_scratch_size != nullptr)
		*out_update_scratch_size = size_info.updateScratchSize;
}

bool reshade::vulkan::device_impl::get_pipeline_shader_group_handles(api::pipeline pipeline, uint32_t first, uint32_t count, void *out_handles)
{
	uint32_t handle_size = 0;
	get_property(api::device_properties::shader_group_handle_size, &handle_size);

	return vk.GetRayTracingShaderGroupHandlesKHR(_orig, (VkPipeline)pipeline.handle, first, count, static_cast<size_t>(count) * static_cast<size_t>(handle_size), out_handles) == VK_SUCCESS;
}

void reshade::vulkan::device_impl::advance_transient_descriptor_pool()
{
	if (_push_descriptor_ext)
		return;

	// This assumes that no other thread is currently allocating from the transient descriptor pool
	const VkDescriptorPool next_pool = _transient_descriptor_pool[++_transient_index % 4];
	vk.ResetDescriptorPool(_orig, next_pool, 0);
}

reshade::vulkan::command_list_immediate_impl *reshade::vulkan::device_impl::get_first_immediate_command_list()
{
	assert(!_queues.empty());

	for (command_queue_impl *const queue : _queues)
		if (const auto immediate_command_list = static_cast<command_list_immediate_impl *>(queue->get_immediate_command_list()))
			return immediate_command_list;
	return nullptr;
}
