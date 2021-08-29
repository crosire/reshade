/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "dll_log.hpp"
#include "hook_manager.hpp"
#include "lockfree_linear_map.hpp"
#include "vulkan_hooks.hpp"
#include "vulkan_impl_device.hpp"
#include "vulkan_impl_command_queue.hpp"
#include "vulkan_impl_swapchain.hpp"
#include "vulkan_impl_type_convert.hpp"

lockfree_linear_map<void *, reshade::vulkan::device_impl *, 4> g_vulkan_devices;
static lockfree_linear_map<VkQueue, reshade::vulkan::command_queue_impl *, 16> s_vulkan_queues;
extern locked_ptr_hash_map<reshade::vulkan::command_list_impl *> g_vulkan_command_buffers;
extern lockfree_linear_map<void *, VkLayerInstanceDispatchTable, 4> g_instance_dispatch;
extern lockfree_linear_map<VkSurfaceKHR, HWND, 16> g_surface_windows;
static lockfree_linear_map<VkSwapchainKHR, reshade::vulkan::swapchain_impl *, 16> s_vulkan_swapchains;

#define GET_DISPATCH_PTR(name, object) \
	GET_DISPATCH_PTR_FROM(name, g_vulkan_devices.at(dispatch_key_from_handle(object)))
#define GET_DISPATCH_PTR_FROM(name, data) \
	assert((data) != nullptr); \
	PFN_vk##name trampoline = (data)->_dispatch_table.name; \
	assert(trampoline != nullptr)
#define INIT_DISPATCH_PTR(name) \
	dispatch_table.name = reinterpret_cast<PFN_vk##name>(get_device_proc(device, "vk" #name))

static inline const char *vk_format_to_string(VkFormat format)
{
	switch (format)
	{
	case VK_FORMAT_UNDEFINED:
		return "VK_FORMAT_UNDEFINED";
	case VK_FORMAT_R8G8B8A8_UNORM:
		return "VK_FORMAT_R8G8B8A8_UNORM";
	case VK_FORMAT_R8G8B8A8_SRGB:
		return "VK_FORMAT_R8G8B8A8_SRGB";
	case VK_FORMAT_B8G8R8A8_UNORM:
		return "VK_FORMAT_B8G8R8A8_UNORM";
	case VK_FORMAT_B8G8R8A8_SRGB:
		return "VK_FORMAT_B8G8R8A8_SRGB";
	case VK_FORMAT_A2R10G10B10_UNORM_PACK32:
		return "VK_FORMAT_A2R10G10B10_UNORM_PACK32";
	case VK_FORMAT_R16G16B16A16_SFLOAT:
		return "VK_FORMAT_R16G16B16A16_SFLOAT";
	default:
		return nullptr;
	}
}

VkResult VKAPI_CALL vkCreateDevice(VkPhysicalDevice physicalDevice, const VkDeviceCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkDevice *pDevice)
{
	LOG(INFO) << "Redirecting " << "vkCreateDevice" << '(' << "physicalDevice = " << physicalDevice << ", pCreateInfo = " << pCreateInfo << ", pAllocator = " << pAllocator << ", pDevice = " << pDevice << ')' << " ...";

	assert(pCreateInfo != nullptr && pDevice != nullptr);

	// Look for layer link info if installed as a layer (provided by the Vulkan loader)
	VkLayerDeviceCreateInfo *const link_info = find_layer_info<VkLayerDeviceCreateInfo>(
		pCreateInfo->pNext, VK_STRUCTURE_TYPE_LOADER_DEVICE_CREATE_INFO, VK_LAYER_LINK_INFO);

	// Get trampoline function pointers
	PFN_vkCreateDevice trampoline = nullptr;
	PFN_vkGetDeviceProcAddr get_device_proc = nullptr;
	PFN_vkGetInstanceProcAddr get_instance_proc = nullptr;

	if (link_info != nullptr)
	{
		assert(link_info->u.pLayerInfo != nullptr);
		assert(link_info->u.pLayerInfo->pfnNextGetDeviceProcAddr != nullptr);
		assert(link_info->u.pLayerInfo->pfnNextGetInstanceProcAddr != nullptr);

		// Look up functions in layer info
		get_device_proc = link_info->u.pLayerInfo->pfnNextGetDeviceProcAddr;
		get_instance_proc = link_info->u.pLayerInfo->pfnNextGetInstanceProcAddr;
		trampoline = reinterpret_cast<PFN_vkCreateDevice>(get_instance_proc(nullptr, "vkCreateDevice"));

		// Advance the link info for the next element on the chain
		link_info->u.pLayerInfo = link_info->u.pLayerInfo->pNext;
	}
#ifdef RESHADE_TEST_APPLICATION
	else
	{
		trampoline = reshade::hooks::call(vkCreateDevice);
		get_device_proc = reshade::hooks::call(vkGetDeviceProcAddr);
		get_instance_proc = reshade::hooks::call(vkGetInstanceProcAddr);
	}
#endif

	if (trampoline == nullptr) // Unable to resolve next 'vkCreateDevice' function in the call chain
		return VK_ERROR_INITIALIZATION_FAILED;

	LOG(INFO) << "> Dumping enabled device extensions:";
	for (uint32_t i = 0; i < pCreateInfo->enabledExtensionCount; ++i)
		LOG(INFO) << "  " << pCreateInfo->ppEnabledExtensionNames[i];

	auto enum_queue_families = g_instance_dispatch.at(dispatch_key_from_handle(physicalDevice)).GetPhysicalDeviceQueueFamilyProperties;
	assert(enum_queue_families != nullptr);
	auto enum_device_extensions = g_instance_dispatch.at(dispatch_key_from_handle(physicalDevice)).EnumerateDeviceExtensionProperties;
	assert(enum_device_extensions != nullptr);

	uint32_t num_queue_families = 0;
	enum_queue_families(physicalDevice, &num_queue_families, nullptr);
	std::vector<VkQueueFamilyProperties> queue_families(num_queue_families);
	enum_queue_families(physicalDevice, &num_queue_families, queue_families.data());

	uint32_t graphics_queue_family_index = std::numeric_limits<uint32_t>::max();
	for (uint32_t i = 0, queue_family_index; i < pCreateInfo->queueCreateInfoCount; ++i)
	{
		queue_family_index = pCreateInfo->pQueueCreateInfos[i].queueFamilyIndex;
		assert(queue_family_index < num_queue_families);

		// Find the first queue family which supports graphics and has at least one queue
		if (pCreateInfo->pQueueCreateInfos[i].queueCount > 0 && (queue_families[queue_family_index].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0)
		{
			if (pCreateInfo->pQueueCreateInfos[i].pQueuePriorities[0] < 1.0f)
				LOG(WARN) << "Vulkan queue used for rendering has a low priority (" << pCreateInfo->pQueueCreateInfos[i].pQueuePriorities[0] << ").";

			graphics_queue_family_index = queue_family_index;
			break;
		}
	}

	VkPhysicalDeviceFeatures enabled_features = {};
	const VkPhysicalDeviceFeatures2 *features2 = find_in_structure_chain<VkPhysicalDeviceFeatures2>(
		pCreateInfo->pNext, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2);
	if (features2 != nullptr) // The features from the structure chain take precedence
		enabled_features = features2->features;
	else if (pCreateInfo->pEnabledFeatures != nullptr)
		enabled_features = *pCreateInfo->pEnabledFeatures;

	std::vector<const char *> enabled_extensions;
	enabled_extensions.reserve(pCreateInfo->enabledExtensionCount);
	for (uint32_t i = 0; i < pCreateInfo->enabledExtensionCount; ++i)
		enabled_extensions.push_back(pCreateInfo->ppEnabledExtensionNames[i]);

	// Check if the device is used for presenting
	if (std::find_if(enabled_extensions.begin(), enabled_extensions.end(),
		[](const char *name) { return strcmp(name, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0; }) == enabled_extensions.end())
	{
		LOG(WARN) << "Skipping device because it is not created with the \"" VK_KHR_SWAPCHAIN_EXTENSION_NAME "\" extension.";

		graphics_queue_family_index = std::numeric_limits<uint32_t>::max();
	}
	// Only have to enable additional features if there is a graphics queue, since ReShade will not run otherwise
	else if (graphics_queue_family_index == std::numeric_limits<uint32_t>::max())
	{
		LOG(WARN) << "Skipping device because it is not created with a graphics queue.";
	}
	else
	{
		uint32_t num_extensions = 0;
		enum_device_extensions(physicalDevice, nullptr, &num_extensions, nullptr);
		std::vector<VkExtensionProperties> extensions(num_extensions);
		enum_device_extensions(physicalDevice, nullptr, &num_extensions, extensions.data());

		// Make sure the driver actually supports the requested extensions
		const auto add_extension = [&extensions, &enabled_extensions, &graphics_queue_family_index](const char *name, bool required) {
			if (const auto it = std::find_if(extensions.begin(), extensions.end(),
				[name](const auto &props) { return strncmp(props.extensionName, name, VK_MAX_EXTENSION_NAME_SIZE) == 0; });
				it != extensions.end())
			{
				enabled_extensions.push_back(name);
				return true;
			}

			if (required)
			{
				LOG(ERROR) << "Required extension \"" << name << "\" is not supported on this device. Initialization failed.";

				// Reset queue family index to prevent ReShade initialization
				graphics_queue_family_index = std::numeric_limits<uint32_t>::max();
			}
			else
			{
				LOG(WARN)  << "Optional extension \"" << name << "\" is not supported on this device.";
			}

			return false;
		};

		// Enable features that ReShade requires
		enabled_features.shaderImageGatherExtended = true;
		enabled_features.shaderStorageImageWriteWithoutFormat = true;

		// Enable extensions that ReShade requires
		add_extension(VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME, false); // This is optional, see imgui code in 'swapchain_impl'
		add_extension(VK_KHR_IMAGE_FORMAT_LIST_EXTENSION_NAME, true);
		add_extension(VK_KHR_SWAPCHAIN_MUTABLE_FORMAT_EXTENSION_NAME, true);
	}

	VkDeviceCreateInfo create_info = *pCreateInfo;
	create_info.enabledExtensionCount = uint32_t(enabled_extensions.size());
	create_info.ppEnabledExtensionNames = enabled_extensions.data();

	// Patch the enabled features
	if (features2 != nullptr)
		// This is evil, because overwriting application memory, but whatever (RenderDoc does this too)
		const_cast<VkPhysicalDeviceFeatures2 *>(features2)->features = enabled_features;
	else
		create_info.pEnabledFeatures = &enabled_features;

	// Continue calling down the chain
	const VkResult result = trampoline(physicalDevice, &create_info, pAllocator, pDevice);
	if (result < VK_SUCCESS)
	{
		LOG(WARN) << "vkCreateDevice" << " failed with error code " << result << '.';
		return result;
	}

	VkDevice device = *pDevice;
	// Initialize the device dispatch table
	VkLayerDispatchTable dispatch_table = { get_device_proc };

	// ---- Core 1_0 commands
	INIT_DISPATCH_PTR(DestroyDevice);
	INIT_DISPATCH_PTR(GetDeviceQueue);
	INIT_DISPATCH_PTR(QueueSubmit);
	INIT_DISPATCH_PTR(QueueWaitIdle);
	INIT_DISPATCH_PTR(DeviceWaitIdle);
	INIT_DISPATCH_PTR(AllocateMemory);
	INIT_DISPATCH_PTR(FreeMemory);
	INIT_DISPATCH_PTR(MapMemory);
	INIT_DISPATCH_PTR(UnmapMemory);
	INIT_DISPATCH_PTR(FlushMappedMemoryRanges);
	INIT_DISPATCH_PTR(InvalidateMappedMemoryRanges);
	INIT_DISPATCH_PTR(BindBufferMemory);
	INIT_DISPATCH_PTR(BindImageMemory);
	INIT_DISPATCH_PTR(GetBufferMemoryRequirements);
	INIT_DISPATCH_PTR(GetImageMemoryRequirements);
	INIT_DISPATCH_PTR(CreateFence);
	INIT_DISPATCH_PTR(DestroyFence);
	INIT_DISPATCH_PTR(ResetFences);
	INIT_DISPATCH_PTR(GetFenceStatus);
	INIT_DISPATCH_PTR(WaitForFences);
	INIT_DISPATCH_PTR(CreateSemaphore);
	INIT_DISPATCH_PTR(DestroySemaphore);
	INIT_DISPATCH_PTR(CreateQueryPool);
	INIT_DISPATCH_PTR(DestroyQueryPool);
	INIT_DISPATCH_PTR(GetQueryPoolResults);
	INIT_DISPATCH_PTR(CreateBuffer);
	INIT_DISPATCH_PTR(DestroyBuffer);
	INIT_DISPATCH_PTR(CreateBufferView);
	INIT_DISPATCH_PTR(DestroyBufferView);
	INIT_DISPATCH_PTR(CreateImage);
	INIT_DISPATCH_PTR(DestroyImage);
	INIT_DISPATCH_PTR(GetImageSubresourceLayout);
	INIT_DISPATCH_PTR(CreateImageView);
	INIT_DISPATCH_PTR(DestroyImageView);
	INIT_DISPATCH_PTR(CreateShaderModule);
	INIT_DISPATCH_PTR(DestroyShaderModule);
	INIT_DISPATCH_PTR(CreateGraphicsPipelines);
	INIT_DISPATCH_PTR(CreateComputePipelines);
	INIT_DISPATCH_PTR(DestroyPipeline);
	INIT_DISPATCH_PTR(CreatePipelineLayout);
	INIT_DISPATCH_PTR(DestroyPipelineLayout);
	INIT_DISPATCH_PTR(CreateSampler);
	INIT_DISPATCH_PTR(DestroySampler);
	INIT_DISPATCH_PTR(CreateDescriptorSetLayout);
	INIT_DISPATCH_PTR(DestroyDescriptorSetLayout);
	INIT_DISPATCH_PTR(CreateDescriptorPool);
	INIT_DISPATCH_PTR(DestroyDescriptorPool);
	INIT_DISPATCH_PTR(ResetDescriptorPool);
	INIT_DISPATCH_PTR(AllocateDescriptorSets);
	INIT_DISPATCH_PTR(FreeDescriptorSets);
	INIT_DISPATCH_PTR(UpdateDescriptorSets);
	INIT_DISPATCH_PTR(CreateFramebuffer);
	INIT_DISPATCH_PTR(DestroyFramebuffer);
	INIT_DISPATCH_PTR(CreateRenderPass);
	INIT_DISPATCH_PTR(DestroyRenderPass);
	INIT_DISPATCH_PTR(CreateCommandPool);
	INIT_DISPATCH_PTR(DestroyCommandPool);
	INIT_DISPATCH_PTR(ResetCommandPool);
	INIT_DISPATCH_PTR(AllocateCommandBuffers);
	INIT_DISPATCH_PTR(FreeCommandBuffers);
	INIT_DISPATCH_PTR(BeginCommandBuffer);
	INIT_DISPATCH_PTR(EndCommandBuffer);
	INIT_DISPATCH_PTR(ResetCommandBuffer);
	INIT_DISPATCH_PTR(CmdBindPipeline);
	INIT_DISPATCH_PTR(CmdSetViewport);
	INIT_DISPATCH_PTR(CmdSetScissor);
	INIT_DISPATCH_PTR(CmdSetDepthBias);
	INIT_DISPATCH_PTR(CmdSetBlendConstants);
	INIT_DISPATCH_PTR(CmdSetStencilCompareMask);
	INIT_DISPATCH_PTR(CmdSetStencilWriteMask);
	INIT_DISPATCH_PTR(CmdSetStencilReference);
	INIT_DISPATCH_PTR(CmdBindDescriptorSets);
	INIT_DISPATCH_PTR(CmdBindIndexBuffer);
	INIT_DISPATCH_PTR(CmdBindVertexBuffers);
	INIT_DISPATCH_PTR(CmdDraw);
	INIT_DISPATCH_PTR(CmdDrawIndexed);
	INIT_DISPATCH_PTR(CmdDrawIndirect);
	INIT_DISPATCH_PTR(CmdDrawIndexedIndirect);
	INIT_DISPATCH_PTR(CmdDispatch);
	INIT_DISPATCH_PTR(CmdDispatchIndirect);
	INIT_DISPATCH_PTR(CmdCopyBuffer);
	INIT_DISPATCH_PTR(CmdCopyImage);
	INIT_DISPATCH_PTR(CmdBlitImage);
	INIT_DISPATCH_PTR(CmdCopyBufferToImage);
	INIT_DISPATCH_PTR(CmdCopyImageToBuffer);
	INIT_DISPATCH_PTR(CmdUpdateBuffer);
	INIT_DISPATCH_PTR(CmdClearColorImage);
	INIT_DISPATCH_PTR(CmdClearDepthStencilImage);
	INIT_DISPATCH_PTR(CmdClearAttachments);
	INIT_DISPATCH_PTR(CmdResolveImage);
	INIT_DISPATCH_PTR(CmdPipelineBarrier);
	INIT_DISPATCH_PTR(CmdBeginQuery);
	INIT_DISPATCH_PTR(CmdEndQuery);
	INIT_DISPATCH_PTR(CmdResetQueryPool);
	INIT_DISPATCH_PTR(CmdWriteTimestamp);
	INIT_DISPATCH_PTR(CmdCopyQueryPoolResults);
	INIT_DISPATCH_PTR(CmdPushConstants);
	INIT_DISPATCH_PTR(CmdBeginRenderPass);
	INIT_DISPATCH_PTR(CmdNextSubpass);
	INIT_DISPATCH_PTR(CmdEndRenderPass);
	INIT_DISPATCH_PTR(CmdExecuteCommands);
	// ---- Core 1_1 commands
	INIT_DISPATCH_PTR(BindBufferMemory2);
	INIT_DISPATCH_PTR(BindImageMemory2);
	INIT_DISPATCH_PTR(GetBufferMemoryRequirements2);
	INIT_DISPATCH_PTR(GetImageMemoryRequirements2);
	INIT_DISPATCH_PTR(GetDeviceQueue2);
	// ---- Core 1_2 commands
	INIT_DISPATCH_PTR(CreateRenderPass2);
	if (dispatch_table.CreateRenderPass2 == nullptr) // Try the KHR version if the core version does not exist
		dispatch_table.CreateRenderPass2  = reinterpret_cast<PFN_vkCreateRenderPass2KHR>(get_device_proc(device, "vkCreateRenderPass2KHR"));
	// ---- VK_KHR_swapchain extension commands
	INIT_DISPATCH_PTR(CreateSwapchainKHR);
	INIT_DISPATCH_PTR(DestroySwapchainKHR);
	INIT_DISPATCH_PTR(GetSwapchainImagesKHR);
	INIT_DISPATCH_PTR(QueuePresentKHR);
	// ---- VK_KHR_push_descriptor extension commands
	INIT_DISPATCH_PTR(CmdPushDescriptorSetKHR);
	// ---- VK_EXT_debug_utils extension commands
	INIT_DISPATCH_PTR(SetDebugUtilsObjectNameEXT);
	INIT_DISPATCH_PTR(QueueBeginDebugUtilsLabelEXT);
	INIT_DISPATCH_PTR(QueueEndDebugUtilsLabelEXT);
	INIT_DISPATCH_PTR(QueueInsertDebugUtilsLabelEXT);
	INIT_DISPATCH_PTR(CmdBeginDebugUtilsLabelEXT);
	INIT_DISPATCH_PTR(CmdEndDebugUtilsLabelEXT);
	INIT_DISPATCH_PTR(CmdInsertDebugUtilsLabelEXT);

	// Initialize per-device data
	const auto device_impl = new reshade::vulkan::device_impl(
		device,
		physicalDevice,
		g_instance_dispatch.at(dispatch_key_from_handle(physicalDevice)),
		dispatch_table,
		enabled_features);

	device_impl->_graphics_queue_family_index = graphics_queue_family_index;

	g_vulkan_devices.emplace(dispatch_key_from_handle(device), device_impl);

	// Initialize all queues associated with this device
	for (uint32_t i = 0; i < pCreateInfo->queueCreateInfoCount; ++i)
	{
		const VkDeviceQueueCreateInfo &queue_create_info = pCreateInfo->pQueueCreateInfos[i];

		for (uint32_t queue_index = 0; queue_index < queue_create_info.queueCount; ++queue_index)
		{
			VkQueue queue = VK_NULL_HANDLE;
			dispatch_table.GetDeviceQueue(device, queue_create_info.queueFamilyIndex, queue_index, &queue);
			assert(VK_NULL_HANDLE != queue);

			const auto queue_impl = new reshade::vulkan::command_queue_impl(
				device_impl,
				queue_create_info.queueFamilyIndex,
				queue_families[queue_create_info.queueFamilyIndex],
				queue);

			s_vulkan_queues.emplace(queue, queue_impl);
		}
	}

#if RESHADE_VERBOSE_LOG
	LOG(INFO) << "Returning Vulkan device " << device << '.';
#endif
	return result;
}
void     VKAPI_CALL vkDestroyDevice(VkDevice device, const VkAllocationCallbacks *pAllocator)
{
	LOG(INFO) << "Redirecting " << "vkDestroyDevice" << '(' << "device = " << device << ", pAllocator = " << pAllocator << ')' << " ...";

	// Remove from device dispatch table since this device is being destroyed
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.erase(dispatch_key_from_handle(device));
	GET_DISPATCH_PTR_FROM(DestroyDevice, device_impl);

	// Destroy all queues associated with this device
	const std::vector<reshade::vulkan::command_queue_impl *> queues = device_impl->_queues;
	for (reshade::vulkan::command_queue_impl *queue_impl : queues)
	{
		s_vulkan_queues.erase(queue_impl->_orig);
		delete queue_impl; // This will remove the queue from the queue list of the device too (see 'command_queue_impl' destructor)
	}
	assert(device_impl->_queues.empty());

	// Finally destroy the device
	delete device_impl;

	trampoline(device, pAllocator);
}

VkResult VKAPI_CALL vkCreateSwapchainKHR(VkDevice device, const VkSwapchainCreateInfoKHR *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkSwapchainKHR *pSwapchain)
{
	LOG(INFO) << "Redirecting " << "vkCreateSwapchainKHR" << '(' << "device = " << device << ", pCreateInfo = " << pCreateInfo << ", pAllocator = " << pAllocator << ", pSwapchain = " << pSwapchain << ')' << " ...";

	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(device));
	GET_DISPATCH_PTR_FROM(CreateSwapchainKHR, device_impl);

	assert(pCreateInfo != nullptr && pSwapchain != nullptr);

	VkSwapchainCreateInfoKHR create_info = *pCreateInfo;
	VkImageFormatListCreateInfoKHR format_list_info { VK_STRUCTURE_TYPE_IMAGE_FORMAT_LIST_CREATE_INFO_KHR };
	std::vector<VkFormat> format_list; std::vector<uint32_t> queue_family_list;

	// Only have to enable additional features if there is a graphics queue, since ReShade will not run otherwise
	if (device_impl->_graphics_queue_family_index != std::numeric_limits<uint32_t>::max())
	{
		// Add required usage flags to create info
		create_info.imageUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

		// Add required formats, so views with different formats can be created for the swap chain images
		format_list.push_back(reshade::vulkan::convert_format(
			reshade::api::format_to_default_typed(reshade::vulkan::convert_format(create_info.imageFormat), 0)));
		format_list.push_back(reshade::vulkan::convert_format(
			reshade::api::format_to_default_typed(reshade::vulkan::convert_format(create_info.imageFormat), 1)));

		// Only have to make format mutable if they are actually different
		if (format_list[0] != format_list[1])
			create_info.flags |= VK_SWAPCHAIN_CREATE_MUTABLE_FORMAT_BIT_KHR;

		// Patch the format list in the create info of the application
		if (const VkImageFormatListCreateInfoKHR *format_list_info2 = find_in_structure_chain<VkImageFormatListCreateInfoKHR>(
			pCreateInfo->pNext, VK_STRUCTURE_TYPE_IMAGE_FORMAT_LIST_CREATE_INFO_KHR); format_list_info2 != nullptr)
		{
			format_list.insert(format_list.end(),
				format_list_info2->pViewFormats, format_list_info2->pViewFormats + format_list_info2->viewFormatCount);

			// Remove duplicates from the list (since the new formats may have already been added by the application)
			std::sort(format_list.begin(), format_list.end());
			format_list.erase(std::unique(format_list.begin(), format_list.end()), format_list.end());

			// This is evil, because writing into the application memory, but eh =)
			const_cast<VkImageFormatListCreateInfoKHR *>(format_list_info2)->viewFormatCount = static_cast<uint32_t>(format_list.size());
			const_cast<VkImageFormatListCreateInfoKHR *>(format_list_info2)->pViewFormats = format_list.data();
		}
		else if (format_list[0] != format_list[1])
		{
			format_list_info.pNext = create_info.pNext;
			format_list_info.viewFormatCount = static_cast<uint32_t>(format_list.size());
			format_list_info.pViewFormats = format_list.data();

			create_info.pNext = &format_list_info;
		}

		// Add required queue family indices, so images can be used on the graphics queue
		if (create_info.imageSharingMode == VK_SHARING_MODE_CONCURRENT)
		{
			queue_family_list.reserve(create_info.queueFamilyIndexCount + 1);
			queue_family_list.push_back(device_impl->_graphics_queue_family_index);

			for (uint32_t i = 0; i < create_info.queueFamilyIndexCount; ++i)
				if (create_info.pQueueFamilyIndices[i] != device_impl->_graphics_queue_family_index)
					queue_family_list.push_back(create_info.pQueueFamilyIndices[i]);

			create_info.queueFamilyIndexCount = static_cast<uint32_t>(queue_family_list.size());
			create_info.pQueueFamilyIndices = queue_family_list.data();
		}
	}

	LOG(INFO) << "> Dumping swap chain description:";
	LOG(INFO) << "  +-----------------------------------------+-----------------------------------------+";
	LOG(INFO) << "  | Parameter                               | Value                                   |";
	LOG(INFO) << "  +-----------------------------------------+-----------------------------------------+";
	LOG(INFO) << "  | flags                                   | " << std::setw(39) << std::hex << create_info.flags << std::dec << " |";
	LOG(INFO) << "  | surface                                 | " << std::setw(39) << create_info.surface << " |";
	LOG(INFO) << "  | minImageCount                           | " << std::setw(39) << create_info.minImageCount << " |";
	if (const char *format_string = vk_format_to_string(create_info.imageFormat); format_string != nullptr)
		LOG(INFO) << "  | imageFormat                             | " << std::setw(39) << format_string << " |";
	else
		LOG(INFO) << "  | imageFormat                             | " << std::setw(39) << create_info.imageFormat << " |";
	LOG(INFO) << "  | imageColorSpace                         | " << std::setw(39) << create_info.imageColorSpace << " |";
	LOG(INFO) << "  | imageExtent                             | " << std::setw(19) << create_info.imageExtent.width << ' ' << std::setw(19) << create_info.imageExtent.height << " |";
	LOG(INFO) << "  | imageArrayLayers                        | " << std::setw(39) << create_info.imageArrayLayers << " |";
	LOG(INFO) << "  | imageUsage                              | " << std::setw(39) << std::hex << create_info.imageUsage << std::dec << " |";
	LOG(INFO) << "  | imageSharingMode                        | " << std::setw(39) << create_info.imageSharingMode << " |";
	LOG(INFO) << "  | queueFamilyIndexCount                   | " << std::setw(39) << create_info.queueFamilyIndexCount << " |";
	LOG(INFO) << "  | preTransform                            | " << std::setw(39) << std::hex << create_info.preTransform << std::dec << " |";
	LOG(INFO) << "  | compositeAlpha                          | " << std::setw(39) << std::hex << create_info.compositeAlpha << std::dec << " |";
	LOG(INFO) << "  | presentMode                             | " << std::setw(39) << create_info.presentMode << " |";
	LOG(INFO) << "  | clipped                                 | " << std::setw(39) << (create_info.clipped ? "true" : "false") << " |";
	LOG(INFO) << "  | oldSwapchain                            | " << std::setw(39) << create_info.oldSwapchain << " |";
	LOG(INFO) << "  +-----------------------------------------+-----------------------------------------+";

	// Look up window handle from surface
	const HWND hwnd = g_surface_windows.at(create_info.surface);

#if RESHADE_ADDON
	reshade::api::resource_desc buffer_desc = {};
	buffer_desc.type = reshade::api::resource_type::texture_2d;
	buffer_desc.texture.width = create_info.imageExtent.width;
	buffer_desc.texture.height = create_info.imageExtent.height;
	assert(create_info.imageArrayLayers <= std::numeric_limits<uint16_t>::max());
	buffer_desc.texture.depth_or_layers = static_cast<uint16_t>(create_info.imageArrayLayers);
	buffer_desc.texture.levels = 1;
	buffer_desc.texture.format = reshade::vulkan::convert_format(create_info.imageFormat);
	buffer_desc.texture.samples = 1;
	buffer_desc.heap = reshade::api::memory_heap::gpu_only;
	reshade::vulkan::convert_image_usage_flags_to_usage(create_info.imageUsage, buffer_desc.usage);

	if (reshade::invoke_addon_event<reshade::addon_event::create_swapchain>(buffer_desc, hwnd))
	{
		create_info.imageFormat = reshade::vulkan::convert_format(buffer_desc.texture.format);
		create_info.imageExtent.width = buffer_desc.texture.width;
		create_info.imageExtent.height = buffer_desc.texture.height;
		create_info.imageArrayLayers = buffer_desc.texture.depth_or_layers;
		reshade::vulkan::convert_usage_to_image_usage_flags(buffer_desc.usage, create_info.imageUsage);
	}
#endif

	const VkResult result = trampoline(device, &create_info, pAllocator, pSwapchain);
	if (result < VK_SUCCESS)
	{
		LOG(WARN) << "vkCreateSwapchainKHR" << " failed with error code " << result << '.';
		return result;
	}

	reshade::vulkan::command_queue_impl *queue_impl = nullptr;
	if (device_impl->_graphics_queue_family_index != std::numeric_limits<uint32_t>::max())
	{
		// Get the main graphics queue for command submission
		// There has to be at least one queue, or else this effect runtime would not have been created with this queue family index, so it is safe to get the first one here
		VkQueue graphics_queue = VK_NULL_HANDLE;
		device_impl->_dispatch_table.GetDeviceQueue(device, device_impl->_graphics_queue_family_index, 0, &graphics_queue);
		assert(VK_NULL_HANDLE != graphics_queue);

		queue_impl = s_vulkan_queues.at(graphics_queue);
	}

	if (queue_impl != nullptr)
	{
		// Remove old swap chain from the list so that a call to 'vkDestroySwapchainKHR' won't reset the effect runtime again
		reshade::vulkan::swapchain_impl *swapchain_impl = s_vulkan_swapchains.erase(create_info.oldSwapchain);
		if (swapchain_impl != nullptr)
		{
			assert(create_info.oldSwapchain != VK_NULL_HANDLE);

			// Re-use the existing effect runtime if this swap chain was not created from scratch, but reset it before initializing again below
			swapchain_impl->on_reset();
		}
		else
		{
			swapchain_impl = new reshade::vulkan::swapchain_impl(device_impl, queue_impl);
		}

		swapchain_impl->on_init(*pSwapchain, create_info, hwnd);

		if (!s_vulkan_swapchains.emplace(*pSwapchain, swapchain_impl))
			delete swapchain_impl;
	}
	else
	{
		s_vulkan_swapchains.emplace(*pSwapchain, nullptr);
	}

#if RESHADE_VERBOSE_LOG
	LOG(INFO) << "Returning Vulkan swapchain " << *pSwapchain << '.';
#endif
	return result;
}
void     VKAPI_CALL vkDestroySwapchainKHR(VkDevice device, VkSwapchainKHR swapchain, const VkAllocationCallbacks *pAllocator)
{
	LOG(INFO) << "Redirecting " << "vkDestroySwapchainKHR" << '(' << device << ", " << swapchain << ", " << pAllocator << ')' << " ...";

	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(device));
	GET_DISPATCH_PTR_FROM(DestroySwapchainKHR, device_impl);

	// Remove swap chain from global list
	delete s_vulkan_swapchains.erase(swapchain);

	trampoline(device, swapchain, pAllocator);
}

VkResult VKAPI_CALL vkQueueSubmit(VkQueue queue, uint32_t submitCount, const VkSubmitInfo *pSubmits, VkFence fence)
{
	assert(pSubmits != nullptr);

#if RESHADE_ADDON
	if (reshade::vulkan::command_queue_impl *const queue_impl = s_vulkan_queues.at(queue); queue_impl != nullptr)
	{
		for (uint32_t i = 0; i < submitCount; ++i)
		{
			for (uint32_t k = 0; k < pSubmits[i].commandBufferCount; ++k)
			{
				assert(pSubmits[i].pCommandBuffers[k] != VK_NULL_HANDLE);

				reshade::vulkan::command_list_impl *const cmd_impl = g_vulkan_command_buffers.at((uint64_t)pSubmits[i].pCommandBuffers[k]);

				reshade::invoke_addon_event<reshade::addon_event::execute_command_list>(queue_impl, cmd_impl);
			}
		}

		queue_impl->flush_immediate_command_list();
	}
#endif

	// The loader uses the same dispatch table pointer for queues and devices, so can use queue to perform lookup here
	GET_DISPATCH_PTR(QueueSubmit, queue);
	return trampoline(queue, submitCount, pSubmits, fence);
}
VkResult VKAPI_CALL vkQueuePresentKHR(VkQueue queue, const VkPresentInfoKHR *pPresentInfo)
{
	assert(pPresentInfo != nullptr);

	std::vector<VkSemaphore> wait_semaphores(
		pPresentInfo->pWaitSemaphores, pPresentInfo->pWaitSemaphores + pPresentInfo->waitSemaphoreCount);

	if (reshade::vulkan::command_queue_impl *const queue_impl = s_vulkan_queues.at(queue); queue_impl != nullptr)
	{
		for (uint32_t i = 0; i < pPresentInfo->swapchainCount; ++i)
		{
			reshade::vulkan::swapchain_impl *const swapchain_impl = s_vulkan_swapchains.at(pPresentInfo->pSwapchains[i]);
			if (swapchain_impl != nullptr)
			{
#if RESHADE_ADDON
				reshade::invoke_addon_event<reshade::addon_event::present>(queue_impl, swapchain_impl);
#endif
				swapchain_impl->on_present(queue, pPresentInfo->pImageIndices[i], wait_semaphores);
			}
		}

		queue_impl->flush_immediate_command_list(wait_semaphores);

		static_cast<reshade::vulkan::device_impl *>(queue_impl->get_device())->advance_transient_descriptor_pool();
	}

	// Override wait semaphores based on the last queue submit from above
	VkPresentInfoKHR present_info = *pPresentInfo;
	present_info.waitSemaphoreCount = static_cast<uint32_t>(wait_semaphores.size());
	present_info.pWaitSemaphores = wait_semaphores.data();

	GET_DISPATCH_PTR(QueuePresentKHR, queue);
	return trampoline(queue, &present_info);
}

VkResult VKAPI_CALL vkCreateBuffer(VkDevice device, const VkBufferCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkBuffer *pBuffer)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(device));
	GET_DISPATCH_PTR_FROM(CreateBuffer, device_impl);

	assert(pCreateInfo != nullptr && pBuffer != nullptr);

#if RESHADE_ADDON
	VkBufferCreateInfo create_info = *pCreateInfo;
	auto desc = reshade::vulkan::convert_resource_desc(create_info);
	assert(desc.heap == reshade::api::memory_heap::unknown);

	if (reshade::invoke_addon_event<reshade::addon_event::create_resource>(device_impl, desc, nullptr, reshade::api::resource_usage::undefined))
	{
		reshade::vulkan::convert_resource_desc(desc, create_info);
		pCreateInfo = &create_info;
	}
#endif

	const VkResult result = trampoline(device, pCreateInfo, pAllocator, pBuffer);
	if (result >= VK_SUCCESS)
	{
#if RESHADE_ADDON
		reshade::vulkan::resource_data data;
		data.allocation = VK_NULL_HANDLE;
		data.buffer_create_info = create_info;

		device_impl->register_object<reshade::vulkan::resource_data>((uint64_t)*pBuffer, std::move(data));

		reshade::invoke_addon_event<reshade::addon_event::init_resource>(
			device_impl, desc, nullptr, reshade::api::resource_usage::undefined, reshade::api::resource { (uint64_t)*pBuffer });
#endif
	}
	else
	{
#if RESHADE_VERBOSE_LOG
		LOG(WARN) << "vkCreateBuffer" << " failed with error code " << result << '.';
#endif
	}

	return result;
}
void     VKAPI_CALL vkDestroyBuffer(VkDevice device, VkBuffer buffer, const VkAllocationCallbacks *pAllocator)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(device));
	GET_DISPATCH_PTR_FROM(DestroyBuffer, device_impl);

#if RESHADE_ADDON
	reshade::invoke_addon_event<reshade::addon_event::destroy_resource>(device_impl, reshade::api::resource { (uint64_t)buffer });

	device_impl->unregister_object<reshade::vulkan::resource_data>((uint64_t)buffer);
#endif

	trampoline(device, buffer, pAllocator);
}

VkResult VKAPI_CALL vkCreateBufferView(VkDevice device, const VkBufferViewCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkBufferView *pView)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(device));
	GET_DISPATCH_PTR_FROM(CreateBufferView, device_impl);

	assert(pCreateInfo != nullptr && pView != nullptr);

#if RESHADE_ADDON
	VkBufferViewCreateInfo create_info = *pCreateInfo;
	auto desc = reshade::vulkan::convert_resource_view_desc(create_info);

	if (reshade::invoke_addon_event<reshade::addon_event::create_resource_view>(device_impl, reshade::api::resource { (uint64_t)pCreateInfo->buffer }, reshade::api::resource_usage::undefined, desc))
	{
		reshade::vulkan::convert_resource_view_desc(desc, create_info);
		pCreateInfo = &create_info;
	}
#endif

	const VkResult result = trampoline(device, pCreateInfo, pAllocator, pView);
	if (result >= VK_SUCCESS)
	{
#if RESHADE_ADDON
		reshade::vulkan::resource_view_data data;
		data.buffer_create_info = create_info;

		device_impl->register_object<reshade::vulkan::resource_view_data>((uint64_t)*pView, std::move(data));

		reshade::invoke_addon_event<reshade::addon_event::init_resource_view>(
			device_impl, reshade::api::resource { (uint64_t)pCreateInfo->buffer }, reshade::api::resource_usage::undefined, desc, reshade::api::resource_view { (uint64_t)*pView });
#endif
	}
	else
	{
#if RESHADE_VERBOSE_LOG
		LOG(WARN) << "vkCreateBufferView" << " failed with error code " << result << '.';
#endif
	}

	return result;
}
void     VKAPI_CALL vkDestroyBufferView(VkDevice device, VkBufferView bufferView, const VkAllocationCallbacks *pAllocator)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(device));
	GET_DISPATCH_PTR_FROM(DestroyBufferView, device_impl);

#if RESHADE_ADDON
	reshade::invoke_addon_event<reshade::addon_event::destroy_resource_view>(device_impl, reshade::api::resource_view { (uint64_t)bufferView });

	device_impl->unregister_object<reshade::vulkan::resource_view_data>((uint64_t)bufferView);
#endif

	trampoline(device, bufferView, pAllocator);
}

VkResult VKAPI_CALL vkCreateImage(VkDevice device, const VkImageCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkImage *pImage)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(device));
	GET_DISPATCH_PTR_FROM(CreateImage, device_impl);

	assert(pCreateInfo != nullptr && pImage != nullptr);

#if RESHADE_ADDON
	VkImageCreateInfo create_info = *pCreateInfo;
	auto desc = reshade::vulkan::convert_resource_desc(create_info);
	assert(desc.heap == reshade::api::memory_heap::unknown);

	if (reshade::invoke_addon_event<reshade::addon_event::create_resource>(device_impl, desc, nullptr, pCreateInfo->initialLayout == VK_IMAGE_LAYOUT_PREINITIALIZED ? reshade::api::resource_usage::cpu_access : reshade::api::resource_usage::undefined))
	{
		reshade::vulkan::convert_resource_desc(desc, create_info);
		pCreateInfo = &create_info;
	}
#endif

	const VkResult result = trampoline(device, pCreateInfo, pAllocator, pImage);
	if (result >= VK_SUCCESS)
	{
#if RESHADE_ADDON
		reshade::vulkan::resource_data data;
		data.allocation = VK_NULL_HANDLE;
		data.image_create_info = create_info;

		device_impl->register_object<reshade::vulkan::resource_data>((uint64_t)*pImage, std::move(data));

		reshade::invoke_addon_event<reshade::addon_event::init_resource>(
			device_impl, desc, nullptr, pCreateInfo->initialLayout == VK_IMAGE_LAYOUT_PREINITIALIZED ? reshade::api::resource_usage::cpu_access : reshade::api::resource_usage::undefined, reshade::api::resource { (uint64_t)*pImage });
#endif
	}
	else
	{
#if RESHADE_VERBOSE_LOG
		LOG(WARN) << "vkCreateImage" << " failed with error code " << result << '.';
#endif
	}

	return result;
}
void     VKAPI_CALL vkDestroyImage(VkDevice device, VkImage image, const VkAllocationCallbacks *pAllocator)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(device));
	GET_DISPATCH_PTR_FROM(DestroyImage, device_impl);

#if RESHADE_ADDON
	reshade::invoke_addon_event<reshade::addon_event::destroy_resource>(device_impl, reshade::api::resource { (uint64_t)image });

	device_impl->unregister_object<reshade::vulkan::resource_data>((uint64_t)image);
#endif

	trampoline(device, image, pAllocator);
}

VkResult VKAPI_CALL vkCreateImageView(VkDevice device, const VkImageViewCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkImageView *pView)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(device));
	GET_DISPATCH_PTR_FROM(CreateImageView, device_impl);

	assert(pCreateInfo != nullptr && pView != nullptr);

#if RESHADE_ADDON
	VkImageViewCreateInfo create_info = *pCreateInfo;
	auto desc = reshade::vulkan::convert_resource_view_desc(create_info);

	if (reshade::invoke_addon_event<reshade::addon_event::create_resource_view>(device_impl, reshade::api::resource { (uint64_t)pCreateInfo->image }, reshade::api::resource_usage::undefined, desc))
	{
		reshade::vulkan::convert_resource_view_desc(desc, create_info);
		pCreateInfo = &create_info;
	}
#endif

	const VkResult result = trampoline(device, pCreateInfo, pAllocator, pView);
	if (result >= VK_SUCCESS)
	{
#if RESHADE_ADDON
		reshade::vulkan::resource_view_data data;
		data.image_create_info = create_info;

		device_impl->register_object<reshade::vulkan::resource_view_data>((uint64_t)*pView, std::move(data));

		reshade::invoke_addon_event<reshade::addon_event::init_resource_view>(
			device_impl, reshade::api::resource { (uint64_t)pCreateInfo->image }, reshade::api::resource_usage::undefined, desc, reshade::api::resource_view { (uint64_t)*pView });
#endif
	}
	else
	{
#if RESHADE_VERBOSE_LOG
		LOG(WARN) << "vkCreateImageView" << " failed with error code " << result << '.';
#endif
	}

	return result;
}
void     VKAPI_CALL vkDestroyImageView(VkDevice device, VkImageView imageView, const VkAllocationCallbacks *pAllocator)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(device));
	GET_DISPATCH_PTR_FROM(DestroyImageView, device_impl);

#if RESHADE_ADDON
	reshade::invoke_addon_event<reshade::addon_event::destroy_resource_view>(device_impl, reshade::api::resource_view { (uint64_t)imageView });

	device_impl->unregister_object<reshade::vulkan::resource_view_data>((uint64_t)imageView);
#endif

	trampoline(device, imageView, pAllocator);
}

VkResult VKAPI_CALL vkCreateShaderModule(VkDevice device, const VkShaderModuleCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkShaderModule *pShaderModule)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(device));
	GET_DISPATCH_PTR_FROM(CreateShaderModule, device_impl);

	assert(pCreateInfo != nullptr && pShaderModule != nullptr);

	const VkResult result = trampoline(device, pCreateInfo, pAllocator, pShaderModule);
	if (result >= VK_SUCCESS)
	{
#if RESHADE_ADDON
		reshade::vulkan::shader_module_data data;
		data.spirv = new uint8_t[pCreateInfo->codeSize];
		std::memcpy(const_cast<uint8_t *>(data.spirv), pCreateInfo->pCode, pCreateInfo->codeSize);
		data.spirv_size = pCreateInfo->codeSize;

		device_impl->register_object<reshade::vulkan::shader_module_data>((uint64_t)*pShaderModule, std::move(data));
#endif
	}
	else
	{
#if RESHADE_VERBOSE_LOG
		LOG(WARN) << "vkCreateShaderModule" << " failed with error code " << result << '.';
#endif
	}

	return result;
}
void     VKAPI_CALL vkDestroyShaderModule(VkDevice device, VkShaderModule shaderModule, const VkAllocationCallbacks *pAllocator)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(device));
	GET_DISPATCH_PTR_FROM(DestroyShaderModule, device_impl);

#if RESHADE_ADDON
	delete[] device_impl->get_native_object_data<reshade::vulkan::shader_module_data>((uint64_t)shaderModule).spirv;

	device_impl->unregister_object<reshade::vulkan::shader_module_data>((uint64_t)shaderModule);
#endif

	trampoline(device, shaderModule, pAllocator);
}

VkResult VKAPI_CALL vkCreateGraphicsPipelines(VkDevice device, VkPipelineCache pipelineCache, uint32_t createInfoCount, const VkGraphicsPipelineCreateInfo *pCreateInfos, const VkAllocationCallbacks *pAllocator, VkPipeline *pPipelines)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(device));
	GET_DISPATCH_PTR_FROM(CreateGraphicsPipelines, device_impl);

#if RESHADE_ADDON
	VkResult result = VK_SUCCESS;
	for (uint32_t i = 0; i < createInfoCount; ++i)
	{
		VkGraphicsPipelineCreateInfo create_info = pCreateInfos[i];
		auto desc = device_impl->convert_pipeline_desc(create_info);

		if (reshade::invoke_addon_event<reshade::addon_event::create_pipeline>(device_impl, desc))
		{
			static_assert(sizeof(*pPipelines) == sizeof(reshade::api::pipeline));

			result = device_impl->create_graphics_pipeline(
				desc, reinterpret_cast<reshade::api::pipeline *>(&pPipelines[i])) ? VK_SUCCESS : VK_ERROR_OUT_OF_HOST_MEMORY;
		}
		else
		{
			result = trampoline(device, pipelineCache, 1, &create_info, pAllocator, &pPipelines[i]);
		}

		if (result >= VK_SUCCESS)
		{
			reshade::invoke_addon_event<reshade::addon_event::init_pipeline>(device_impl, desc, reshade::api::pipeline { (uint64_t)pPipelines[i] });
		}
		else
		{
#if RESHADE_VERBOSE_LOG
			LOG(WARN) << "vkCreateGraphicsPipelines" << " failed with error code " << result << '.';
#endif

			for (uint32_t k = 0; k < i; ++k)
				vkDestroyPipeline(device, pPipelines[k], pAllocator);
			for (uint32_t k = 0; k < createInfoCount; ++k)
				pPipelines[k] = VK_NULL_HANDLE;
			break;
		}
	}

	return result;
#else
	return trampoline(device, pipelineCache, createInfoCount, pCreateInfos, pAllocator, pPipelines);
#endif
}
VkResult VKAPI_CALL vkCreateComputePipelines(VkDevice device, VkPipelineCache pipelineCache, uint32_t createInfoCount, const VkComputePipelineCreateInfo *pCreateInfos, const VkAllocationCallbacks *pAllocator, VkPipeline *pPipelines)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(device));
	GET_DISPATCH_PTR_FROM(CreateComputePipelines, device_impl);

#if RESHADE_ADDON
	VkResult result = VK_SUCCESS;
	for (uint32_t i = 0; i < createInfoCount; ++i)
	{
		VkComputePipelineCreateInfo create_info = pCreateInfos[i];
		auto desc = device_impl->convert_pipeline_desc(pCreateInfos[i]);

		if (reshade::invoke_addon_event<reshade::addon_event::create_pipeline>(device_impl, desc))
		{
			result = device_impl->create_compute_pipeline(
				desc, reinterpret_cast<reshade::api::pipeline *>(&pPipelines[i])) ? VK_SUCCESS : VK_ERROR_OUT_OF_HOST_MEMORY;
		}
		else
		{
			result = trampoline(device, pipelineCache, 1, &create_info, pAllocator, &pPipelines[i]);
		}

		if (result >= VK_SUCCESS)
		{
			reshade::invoke_addon_event<reshade::addon_event::init_pipeline>(device_impl, desc, reshade::api::pipeline { (uint64_t)pPipelines[i] });
		}
		else
		{
#if RESHADE_VERBOSE_LOG
			LOG(WARN) << "vkCreateComputePipelines" << " failed with error code " << result << '.';
#endif

			for (uint32_t k = 0; k < i; ++k)
				vkDestroyPipeline(device, pPipelines[k], pAllocator);
			for (uint32_t k = 0; k < createInfoCount; ++k)
				pPipelines[k] = VK_NULL_HANDLE;
			break;
		}
	}

	return result;
#else
	return trampoline(device, pipelineCache, createInfoCount, pCreateInfos, pAllocator, pPipelines);
#endif
}
void     VKAPI_CALL vkDestroyPipeline(VkDevice device, VkPipeline pipeline, const VkAllocationCallbacks *pAllocator)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(device));
	GET_DISPATCH_PTR_FROM(DestroyPipeline, device_impl);

#if RESHADE_ADDON
	reshade::invoke_addon_event<reshade::addon_event::destroy_pipeline>(device_impl, reshade::api::pipeline { (uint64_t)pipeline });
#endif

	trampoline(device, pipeline, pAllocator);
}

VkResult VKAPI_CALL vkCreatePipelineLayout(VkDevice device, const VkPipelineLayoutCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkPipelineLayout *pPipelineLayout)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(device));
	GET_DISPATCH_PTR_FROM(CreatePipelineLayout, device_impl);

	assert(pCreateInfo != nullptr && pPipelineLayout != nullptr);

	const VkResult result = trampoline(device, pCreateInfo, pAllocator, pPipelineLayout);
	if (result >= VK_SUCCESS)
	{
#if RESHADE_ADDON
		std::vector<reshade::api::pipeline_layout_param> layout_desc(
			pCreateInfo->setLayoutCount + pCreateInfo->pushConstantRangeCount);

		for (uint32_t i = 0; i < pCreateInfo->setLayoutCount; ++i)
		{
			const bool push_descriptors = device_impl->get_native_object_data<reshade::vulkan::descriptor_set_layout_data>((uint64_t)pCreateInfo->pSetLayouts[i]).push_descriptors;

			layout_desc[i].type = push_descriptors ? reshade::api::pipeline_layout_param_type::push_descriptors : reshade::api::pipeline_layout_param_type::descriptor_set;
			layout_desc[i].descriptor_layout = { (uint64_t)pCreateInfo->pSetLayouts[i] };
		}

		for (uint32_t i = pCreateInfo->setLayoutCount; i < pCreateInfo->setLayoutCount + pCreateInfo->pushConstantRangeCount; ++i)
		{
			const VkPushConstantRange &push_constant_range = pCreateInfo->pPushConstantRanges[i];

			layout_desc[i].type = reshade::api::pipeline_layout_param_type::push_constants;
			layout_desc[i].push_constants.offset = push_constant_range.offset;
			layout_desc[i].push_constants.count = push_constant_range.size;
			layout_desc[i].push_constants.visibility = static_cast<reshade::api::shader_stage>(push_constant_range.stageFlags);
		}

		reshade::vulkan::pipeline_layout_data data;
		data.desc = std::move(layout_desc);

		device_impl->register_object<reshade::vulkan::pipeline_layout_data>((uint64_t)*pPipelineLayout, std::move(data));
#endif
	}
	else
	{
#if RESHADE_VERBOSE_LOG
		LOG(WARN) << "vkCreatePipelineLayout" << " failed with error code " << result << '.';
#endif
	}

	return result;
}
void     VKAPI_CALL vkDestroyPipelineLayout(VkDevice device, VkPipelineLayout pipelineLayout, const VkAllocationCallbacks *pAllocator)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(device));
	GET_DISPATCH_PTR_FROM(DestroyPipelineLayout, device_impl);

#if RESHADE_ADDON
	device_impl->unregister_object<reshade::vulkan::pipeline_layout_data>((uint64_t)pipelineLayout);
#endif

	trampoline(device, pipelineLayout, pAllocator);
}

VkResult VKAPI_CALL vkCreateSampler(VkDevice device, const VkSamplerCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkSampler *pSampler)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(device));
	GET_DISPATCH_PTR_FROM(CreateSampler, device_impl);

	assert(pCreateInfo != nullptr && pSampler != nullptr);

#if RESHADE_ADDON
	VkSamplerCreateInfo create_info = *pCreateInfo;
	auto desc = reshade::vulkan::convert_sampler_desc(create_info);

	if (reshade::invoke_addon_event<reshade::addon_event::create_sampler>(device_impl, desc))
	{
		reshade::vulkan::convert_sampler_desc(desc, create_info);
		pCreateInfo = &create_info;
	}
#endif

	const VkResult result = trampoline(device, pCreateInfo, pAllocator, pSampler);
	if (result >= VK_SUCCESS)
	{
#if RESHADE_ADDON
		reshade::invoke_addon_event<reshade::addon_event::init_sampler>(
			device_impl, desc, reshade::api::sampler { (uint64_t)*pSampler });
#endif
	}
	else
	{
#if RESHADE_VERBOSE_LOG
		LOG(WARN) << "vkCreateSampler" << " failed with error code " << result << '.';
#endif
	}

	return result;
}
void     VKAPI_CALL vkDestroySampler(VkDevice device, VkSampler sampler, const VkAllocationCallbacks *pAllocator)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(device));
	GET_DISPATCH_PTR_FROM(DestroySampler, device_impl);

#if RESHADE_ADDON
	reshade::invoke_addon_event<reshade::addon_event::destroy_sampler>(device_impl, reshade::api::sampler { (uint64_t)sampler });
#endif

	trampoline(device, sampler, pAllocator);
}

VkResult VKAPI_CALL vkCreateDescriptorSetLayout(VkDevice device, const VkDescriptorSetLayoutCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkDescriptorSetLayout *pSetLayout)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(device));
	GET_DISPATCH_PTR_FROM(CreateDescriptorSetLayout, device_impl);

	assert(pCreateInfo != nullptr && pSetLayout != nullptr);

	const VkResult result = trampoline(device, pCreateInfo, pAllocator, pSetLayout);
	if (result >= VK_SUCCESS)
	{
#if RESHADE_ADDON
		std::vector<uint32_t> descriptor_counts_per_binding;
		descriptor_counts_per_binding.reserve(pCreateInfo->bindingCount);
		std::vector<reshade::api::descriptor_range> layout_desc(pCreateInfo->bindingCount);

		for (uint32_t i = 0; i < pCreateInfo->bindingCount; ++i)
		{
			const VkDescriptorSetLayoutBinding &binding = pCreateInfo->pBindings[i];

			if (binding.binding >= descriptor_counts_per_binding.size())
				descriptor_counts_per_binding.resize(binding.binding + 1);
			descriptor_counts_per_binding[binding.binding] = binding.descriptorCount;

			layout_desc[i].binding = binding.binding;
			layout_desc[i].dx_register_index = 0;
			layout_desc[i].dx_register_space = 0;
			layout_desc[i].type = static_cast<reshade::api::descriptor_type>(binding.descriptorType);
			layout_desc[i].array_size = binding.descriptorCount;
			layout_desc[i].visibility = static_cast<reshade::api::shader_stage>(binding.stageFlags);
		}

		for (size_t i = 1; i < descriptor_counts_per_binding.size(); ++i)
			descriptor_counts_per_binding[i] += descriptor_counts_per_binding[i - 1];

		std::unordered_map<uint32_t, uint32_t> binding_to_offset;
		binding_to_offset.reserve(descriptor_counts_per_binding.back());

		for (uint32_t i = 0, offset = 0; i < pCreateInfo->bindingCount; ++i)
		{
			const uint32_t binding = layout_desc[i].binding;
			if (binding != 0)
			{
				offset = descriptor_counts_per_binding[binding - 1];
				layout_desc[i].offset = offset;
			}

			binding_to_offset[binding] = offset;
		}

		reshade::vulkan::descriptor_set_layout_data data;
		data.desc = std::move(layout_desc);
		data.binding_to_offset = std::move(binding_to_offset);
		data.push_descriptors = (pCreateInfo->flags & VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR) != 0;

		device_impl->register_object<reshade::vulkan::descriptor_set_layout_data>((uint64_t)*pSetLayout, std::move(data));
#endif
	}
	else
	{
#if RESHADE_VERBOSE_LOG
		LOG(WARN) << "vkCreateDescriptorSetLayout" << " failed with error code " << result << '.';
#endif
	}

	return result;
}
void     VKAPI_CALL vkDestroyDescriptorSetLayout(VkDevice device, VkDescriptorSetLayout descriptorSetLayout, const VkAllocationCallbacks *pAllocator)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(device));
	GET_DISPATCH_PTR_FROM(DestroyDescriptorSetLayout, device_impl);

#if RESHADE_ADDON
	device_impl->unregister_object<reshade::vulkan::descriptor_set_layout_data>((uint64_t)descriptorSetLayout);
#endif

	trampoline(device, descriptorSetLayout, pAllocator);
}

void     VKAPI_CALL vkUpdateDescriptorSets(VkDevice device, uint32_t descriptorWriteCount, const VkWriteDescriptorSet *pDescriptorWrites, uint32_t descriptorCopyCount, const VkCopyDescriptorSet *pDescriptorCopies)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(device));
	GET_DISPATCH_PTR_FROM(UpdateDescriptorSets, device_impl);

#if RESHADE_ADDON
	if (reshade::has_addon_event<reshade::addon_event::update_descriptor_sets>())
	{
		std::vector<reshade::api::copy_descriptor_set> copies;
		copies.reserve(descriptorCopyCount);
		std::vector<reshade::api::write_descriptor_set> writes;
		writes.reserve(descriptorWriteCount);

		uint32_t max_descriptors = 0;
		for (uint32_t i = 0; i < descriptorWriteCount; ++i)
			max_descriptors += pDescriptorWrites[i].descriptorCount;
		std::vector<uint64_t> descriptors(max_descriptors * 2);

		for (uint32_t i = 0, j = 0; i < descriptorWriteCount; ++i)
		{
			const VkWriteDescriptorSet &write = pDescriptorWrites[i];

			reshade::api::write_descriptor_set &new_write = writes.emplace_back();
			new_write.set = { (uint64_t)write.dstSet };
			new_write.offset = device_impl->get_native_object_data<reshade::vulkan::descriptor_set_layout_data>((uint64_t)device_impl->get_native_object_data<reshade::vulkan::descriptor_set_data>((uint64_t)write.dstSet).layout).calc_offset_from_binding(write.dstBinding, write.dstArrayElement);
			new_write.count = write.descriptorCount;
			new_write.type = static_cast<reshade::api::descriptor_type>(write.descriptorType);

			switch (write.descriptorType)
			{
			case VK_DESCRIPTOR_TYPE_SAMPLER:
				new_write.descriptors = descriptors.data() + j;
				for (uint32_t k = 0; k < write.descriptorCount; ++k, ++j)
					descriptors[j] = (uint64_t)write.pImageInfo[k].sampler;
				break;
			case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
				new_write.descriptors = descriptors.data() + j;
				for (uint32_t k = 0; k < write.descriptorCount; ++k, j += 2)
				{
					descriptors[j + 0] = (uint64_t)write.pImageInfo[k].sampler;
					descriptors[j + 1] = (uint64_t)write.pImageInfo[k].imageView;
				}
				break;
			case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
			case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
				new_write.descriptors = descriptors.data() + j;
				for (uint32_t k = 0; k < write.descriptorCount; ++k, ++j)
					descriptors[j] = (uint64_t)write.pImageInfo[k].imageView;
				break;
			case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
			case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
				static_assert(sizeof(reshade::api::buffer_range) == sizeof(VkDescriptorBufferInfo));
				new_write.descriptors = write.pBufferInfo;
				break;
			}
		}
		for (uint32_t i = 0; i < descriptorCopyCount; ++i)
		{
			const VkCopyDescriptorSet &copy = pDescriptorCopies[i];

			reshade::api::copy_descriptor_set &new_copy = copies.emplace_back();
			new_copy.src_set = { (uint64_t)copy.srcSet };
			new_copy.src_offset = device_impl->get_native_object_data<reshade::vulkan::descriptor_set_layout_data>((uint64_t)device_impl->get_native_object_data<reshade::vulkan::descriptor_set_data>((uint64_t)copy.srcSet).layout).calc_offset_from_binding(copy.srcBinding, copy.srcArrayElement);
			new_copy.dst_set = { (uint64_t)copy.dstSet };
			new_copy.dst_offset = device_impl->get_native_object_data<reshade::vulkan::descriptor_set_layout_data>((uint64_t)device_impl->get_native_object_data<reshade::vulkan::descriptor_set_data>((uint64_t)copy.dstSet).layout).calc_offset_from_binding(copy.dstBinding, copy.dstArrayElement);
			new_copy.count = copy.descriptorCount;
		}

		if (reshade::invoke_addon_event<reshade::addon_event::update_descriptor_sets>(device_impl, descriptorWriteCount, writes.data(), descriptorCopyCount, copies.data()))
			return;
	}
#endif

	trampoline(device, descriptorWriteCount, pDescriptorWrites, descriptorCopyCount, pDescriptorCopies);
}

VkResult VKAPI_CALL vkCreateFramebuffer(VkDevice device, const VkFramebufferCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkFramebuffer *pFramebuffer)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(device));
	GET_DISPATCH_PTR_FROM(CreateFramebuffer, device_impl);

	assert(pCreateInfo != nullptr && pFramebuffer != nullptr);

#if RESHADE_ADDON
	const auto pass_attachments = device_impl->get_native_object_data<reshade::vulkan::render_pass_data>((uint64_t)pCreateInfo->renderPass).attachments;

	// Keep track of the frame buffer attachments
	reshade::vulkan::framebuffer_data data;
	data.area.width = pCreateInfo->width;
	data.area.height = pCreateInfo->height;
	data.attachments.resize(pCreateInfo->attachmentCount);
	data.attachment_types.resize(pCreateInfo->attachmentCount);
	for (uint32_t i = 0; i < pCreateInfo->attachmentCount; ++i)
	{
		data.attachments[i] = pCreateInfo->pAttachments[i];
		data.attachment_types[i] = pass_attachments[i].format_flags;
	}

	reshade::api::framebuffer_desc desc = {};
	desc.render_pass_template = { (uint64_t)pCreateInfo->renderPass };
	for (uint32_t i = 0, k = 0; i < data.attachments.size(); ++i)
	{
		if (data.attachment_types[i] & (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT))
			desc.depth_stencil = { (uint64_t)data.attachments[i] };
		else if (data.attachment_types[i] & (VK_IMAGE_ASPECT_COLOR_BIT) && k < 8)
			desc.render_targets[k++] = { (uint64_t)data.attachments[i] };
	}

	VkFramebufferCreateInfo create_info;

	if (reshade::invoke_addon_event<reshade::addon_event::create_framebuffer>(device_impl, desc))
	{
		for (uint32_t i = 0, k = 0; i < data.attachments.size(); ++i)
		{
			if (data.attachment_types[i] & (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT))
				data.attachments[i] = (VkImageView)desc.depth_stencil.handle;
			else if (data.attachment_types[i] & (VK_IMAGE_ASPECT_COLOR_BIT) && k < 8)
				data.attachments[i] = (VkImageView)desc.render_targets[k++].handle;
		}

		create_info = *pCreateInfo;
		create_info.pAttachments = data.attachments.data();
		pCreateInfo = &create_info;
	}
#endif

	const VkResult result = trampoline(device, pCreateInfo, pAllocator, pFramebuffer);
	if (result >= VK_SUCCESS)
	{
#if RESHADE_ADDON
		device_impl->register_object<reshade::vulkan::framebuffer_data>((uint64_t)*pFramebuffer, std::move(data));

		reshade::invoke_addon_event<reshade::addon_event::init_framebuffer>(
			device_impl, desc, reshade::api::framebuffer { (uint64_t)*pFramebuffer });
#endif
	}
	else
	{
#if RESHADE_VERBOSE_LOG
		LOG(WARN) << "vkCreateFramebuffer" << " failed with error code " << result << '.';
#endif
	}

	return result;
}
void     VKAPI_CALL vkDestroyFramebuffer(VkDevice device, VkFramebuffer framebuffer, const VkAllocationCallbacks *pAllocator)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(device));
	GET_DISPATCH_PTR_FROM(DestroyFramebuffer, device_impl);

#if RESHADE_ADDON
	reshade::invoke_addon_event<reshade::addon_event::destroy_framebuffer>(device_impl, reshade::api::framebuffer { (uint64_t)framebuffer });

	device_impl->unregister_object<reshade::vulkan::framebuffer_data>((uint64_t)framebuffer);
#endif

	trampoline(device, framebuffer, pAllocator);
}

VkResult VKAPI_CALL vkCreateRenderPass(VkDevice device, const VkRenderPassCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkRenderPass *pRenderPass)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(device));
	GET_DISPATCH_PTR_FROM(CreateRenderPass, device_impl);

	assert(pCreateInfo != nullptr && pRenderPass != nullptr);

#if RESHADE_ADDON
	std::vector<VkAttachmentDescription> attachments;

	reshade::api::render_pass_desc desc = {};
	for (uint32_t i = 0; i < pCreateInfo->attachmentCount && i < 8; ++i)
	{
		desc.samples = static_cast<uint16_t>(pCreateInfo->pAttachments[i].samples);
		desc.render_targets_format[i] = reshade::vulkan::convert_format(pCreateInfo->pAttachments[i].format);
	}

	VkRenderPassCreateInfo create_info;

	if (reshade::invoke_addon_event<reshade::addon_event::create_render_pass>(device_impl, desc))
	{
		attachments.resize(pCreateInfo->attachmentCount);
		for (uint32_t i = 0; i < pCreateInfo->attachmentCount; ++i)
		{
			attachments[i] = pCreateInfo->pAttachments[i];
			attachments[i].samples = static_cast<VkSampleCountFlagBits>(desc.samples);
			attachments[i].format = reshade::vulkan::convert_format(desc.render_targets_format[i]);
		}

		create_info = *pCreateInfo;
		create_info.pAttachments = attachments.data();
		pCreateInfo = &create_info;
	}
#endif

	const VkResult result = trampoline(device, pCreateInfo, pAllocator, pRenderPass);
	if (result >= VK_SUCCESS)
	{
#if RESHADE_ADDON
		reshade::vulkan::render_pass_data data;
		data.attachments.reserve(pCreateInfo->attachmentCount);

		for (uint32_t attachment = 0; attachment < pCreateInfo->attachmentCount; ++attachment)
		{
			extern VkImageAspectFlags aspect_flags_from_format(VkFormat format);
			VkImageAspectFlags clear_flags = aspect_flags_from_format(pCreateInfo->pAttachments[attachment].format);
			const VkImageAspectFlags format_flags = clear_flags;

			if (pCreateInfo->pAttachments[attachment].loadOp != VK_ATTACHMENT_LOAD_OP_CLEAR)
				clear_flags &= ~(VK_IMAGE_ASPECT_COLOR_BIT | VK_IMAGE_ASPECT_DEPTH_BIT);
			if (pCreateInfo->pAttachments[attachment].stencilLoadOp != VK_ATTACHMENT_LOAD_OP_CLEAR)
				clear_flags &= ~(VK_IMAGE_ASPECT_STENCIL_BIT);

			data.attachments.push_back({ pCreateInfo->pAttachments[attachment].initialLayout, clear_flags, format_flags });
		}

		device_impl->register_object<reshade::vulkan::render_pass_data>((uint64_t)*pRenderPass, std::move(data));

		reshade::invoke_addon_event<reshade::addon_event::init_render_pass>(
			device_impl, desc, reshade::api::render_pass { (uint64_t)*pRenderPass });
#endif
	}
	else
	{
#if RESHADE_VERBOSE_LOG
		LOG(WARN) << "vkCreateRenderPass" << " failed with error code " << result << '.';
#endif
	}

	return result;
}
VkResult VKAPI_CALL vkCreateRenderPass2(VkDevice device, const VkRenderPassCreateInfo2 *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkRenderPass *pRenderPass)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(device));
	GET_DISPATCH_PTR_FROM(CreateRenderPass2, device_impl);

	assert(pCreateInfo != nullptr && pRenderPass != nullptr);

#if RESHADE_ADDON
	std::vector<VkAttachmentDescription2> attachments;

	reshade::api::render_pass_desc desc = {};
	for (uint32_t i = 0; i < pCreateInfo->attachmentCount && i < 8; ++i)
	{
		desc.samples = static_cast<uint16_t>(pCreateInfo->pAttachments[i].samples);
		desc.render_targets_format[i] = reshade::vulkan::convert_format(pCreateInfo->pAttachments[i].format);
	}

	VkRenderPassCreateInfo2 create_info;

	if (reshade::invoke_addon_event<reshade::addon_event::create_render_pass>(device_impl, desc))
	{
		attachments.resize(pCreateInfo->attachmentCount);
		for (uint32_t i = 0; i < pCreateInfo->attachmentCount; ++i)
		{
			attachments[i] = pCreateInfo->pAttachments[i];
			attachments[i].samples = static_cast<VkSampleCountFlagBits>(desc.samples);
			attachments[i].format = reshade::vulkan::convert_format(desc.render_targets_format[i]);
		}

		create_info = *pCreateInfo;
		create_info.pAttachments = attachments.data();
		pCreateInfo = &create_info;
	}
#endif

	const VkResult result = trampoline(device, pCreateInfo, pAllocator, pRenderPass);
	if (result >= VK_SUCCESS)
	{
#if RESHADE_ADDON
		reshade::vulkan::render_pass_data data;
		data.attachments.reserve(pCreateInfo->attachmentCount);

		for (uint32_t attachment = 0; attachment < pCreateInfo->attachmentCount; ++attachment)
		{
			extern VkImageAspectFlags aspect_flags_from_format(VkFormat format);
			VkImageAspectFlags clear_flags = aspect_flags_from_format(pCreateInfo->pAttachments[attachment].format);
			const VkImageAspectFlags format_flags = clear_flags;

			if (pCreateInfo->pAttachments[attachment].loadOp != VK_ATTACHMENT_LOAD_OP_CLEAR)
				clear_flags &= ~(VK_IMAGE_ASPECT_COLOR_BIT | VK_IMAGE_ASPECT_DEPTH_BIT);
			if (pCreateInfo->pAttachments[attachment].stencilLoadOp != VK_ATTACHMENT_LOAD_OP_CLEAR)
				clear_flags &= ~(VK_IMAGE_ASPECT_STENCIL_BIT);

			data.attachments.push_back({ pCreateInfo->pAttachments[attachment].initialLayout, clear_flags, format_flags });
		}

		device_impl->register_object<reshade::vulkan::render_pass_data>((uint64_t)*pRenderPass, std::move(data));

		reshade::invoke_addon_event<reshade::addon_event::init_render_pass>(
			device_impl, desc, reshade::api::render_pass { (uint64_t)*pRenderPass });
#endif
	}
	else
	{
#if RESHADE_VERBOSE_LOG
		LOG(WARN) << "vkCreateRenderPass2" << " failed with error code " << result << '.';
#endif
	}

	return result;
}
void     VKAPI_CALL vkDestroyRenderPass(VkDevice device, VkRenderPass renderPass, const VkAllocationCallbacks *pAllocator)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(device));
	GET_DISPATCH_PTR_FROM(DestroyRenderPass, device_impl);

#if RESHADE_ADDON
	reshade::invoke_addon_event<reshade::addon_event::destroy_render_pass>(device_impl, reshade::api::render_pass { (uint64_t)renderPass });

	device_impl->unregister_object<reshade::vulkan::render_pass_data>((uint64_t)renderPass);
#endif

	trampoline(device, renderPass, pAllocator);
}

VkResult VKAPI_CALL vkAllocateCommandBuffers(VkDevice device, const VkCommandBufferAllocateInfo *pAllocateInfo, VkCommandBuffer *pCommandBuffers)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(device));
	GET_DISPATCH_PTR_FROM(AllocateCommandBuffers, device_impl);

	const VkResult result = trampoline(device, pAllocateInfo, pCommandBuffers);
	if (result >= VK_SUCCESS)
	{
#if RESHADE_ADDON
		for (uint32_t i = 0; i < pAllocateInfo->commandBufferCount; ++i)
		{
			const auto cmd_impl = new reshade::vulkan::command_list_impl(device_impl, pCommandBuffers[i]);
			g_vulkan_command_buffers.emplace((uint64_t)pCommandBuffers[i], cmd_impl);
		}
#endif
	}
	else
	{
#if RESHADE_VERBOSE_LOG
		LOG(WARN) << "vkAllocateCommandBuffers" << " failed with error code " << result << '.';
#endif
	}

	return result;
}
void     VKAPI_CALL vkFreeCommandBuffers(VkDevice device, VkCommandPool commandPool, uint32_t commandBufferCount, const VkCommandBuffer *pCommandBuffers)
{
#if RESHADE_ADDON
	for (uint32_t i = 0; i < commandBufferCount; ++i)
	{
		delete g_vulkan_command_buffers.at((uint64_t)pCommandBuffers[i]);
		g_vulkan_command_buffers.erase((uint64_t)pCommandBuffers[i]);
	}
#endif

	GET_DISPATCH_PTR(FreeCommandBuffers, device);
	trampoline(device, commandPool, commandBufferCount, pCommandBuffers);
}
