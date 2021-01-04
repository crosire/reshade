/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "dll_log.hpp"
#include "hook_manager.hpp"
#include "vulkan_hooks.hpp"
#include "runtime_vk.hpp"
#include "format_utils.hpp"
#include "lockfree_table.hpp"

struct device_data
{
	VkPhysicalDevice physical_device;
	VkLayerDispatchTable dispatch_table;
	uint32_t graphics_queue_family_index = std::numeric_limits<uint32_t>::max();
	reshade::vulkan::state_tracking_context state;
};

struct render_pass_data
{
	uint32_t depthstencil_attachment_index = std::numeric_limits<uint32_t>::max();
	VkImageLayout final_depthstencil_layout = VK_IMAGE_LAYOUT_UNDEFINED;
};

struct command_buffer_data
{
	// State tracking for render passes
	uint32_t current_subpass = std::numeric_limits<uint32_t>::max();
	VkRenderPass current_renderpass = VK_NULL_HANDLE;
	VkFramebuffer current_framebuffer = VK_NULL_HANDLE;
	reshade::vulkan::state_tracking state;
};

static lockfree_table<void *, device_data, 16> s_device_dispatch;
extern lockfree_table<void *, VkLayerInstanceDispatchTable, 16> s_instance_dispatch;
extern lockfree_table<VkSurfaceKHR, HWND, 16> s_surface_windows;
static lockfree_table<VkSwapchainKHR, reshade::vulkan::runtime_vk *, 16> s_vulkan_runtimes;
static lockfree_table<VkImage, VkImageCreateInfo, 4096> s_image_data;
static lockfree_table<VkImageView, VkImage, 4096> s_image_view_mapping;
static lockfree_table<VkFramebuffer, std::vector<VkImage>, 4096> s_framebuffer_data;
static lockfree_table<VkCommandBuffer, command_buffer_data, 4096> s_command_buffer_data;
static lockfree_table<VkRenderPass, std::vector<render_pass_data>, 4096> s_renderpass_data;

#define GET_DEVICE_DISPATCH_PTR(name, object) \
	PFN_vk##name trampoline = s_device_dispatch.at(dispatch_key_from_handle(object)).dispatch_table.name; \
	assert(trampoline != nullptr);

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

	auto enum_queue_families = s_instance_dispatch.at(dispatch_key_from_handle(physicalDevice)).GetPhysicalDeviceQueueFamilyProperties;
	assert(enum_queue_families != nullptr);
	auto enum_device_extensions = s_instance_dispatch.at(dispatch_key_from_handle(physicalDevice)).EnumerateDeviceExtensionProperties;
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

		graphics_queue_family_index  = std::numeric_limits<uint32_t>::max();
	}

	// Only have to enable additional features if there is a graphics queue, since ReShade will not run otherwise
	if (graphics_queue_family_index != std::numeric_limits<uint32_t>::max())
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
		add_extension(VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME, false); // This is optional, see imgui code in 'runtime_vk'
		add_extension(VK_KHR_IMAGE_FORMAT_LIST_EXTENSION_NAME, true);
		add_extension(VK_KHR_SWAPCHAIN_MUTABLE_FORMAT_EXTENSION_NAME, true);
#ifndef NDEBUG
		add_extension(VK_EXT_DEBUG_MARKER_EXTENSION_NAME, false);
#endif
	}
	else
	{
		LOG(WARN) << "Skipping device because it is not created with a graphics queue.";
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

	// Continue call down the chain
	const VkResult result = trampoline(physicalDevice, &create_info, pAllocator, pDevice);
	if (result != VK_SUCCESS)
	{
		LOG(WARN) << "vkCreateDevice" << " failed with error code " << result << '.';
		return result;
	}

	VkDevice device = *pDevice;
	// Initialize the device dispatch table
	VkLayerDispatchTable dispatch_table = { get_device_proc };

#define INIT_DEVICE_PROC(name) \
	dispatch_table.name = reinterpret_cast<PFN_vk##name>(get_device_proc(device, "vk" #name))

	// ---- Core 1_0 commands
	INIT_DEVICE_PROC(DestroyDevice);
	INIT_DEVICE_PROC(GetDeviceQueue);
	INIT_DEVICE_PROC(QueueSubmit);
	INIT_DEVICE_PROC(QueueWaitIdle);
	INIT_DEVICE_PROC(DeviceWaitIdle);
	INIT_DEVICE_PROC(AllocateMemory);
	INIT_DEVICE_PROC(FreeMemory);
	INIT_DEVICE_PROC(MapMemory);
	INIT_DEVICE_PROC(UnmapMemory);
	INIT_DEVICE_PROC(FlushMappedMemoryRanges);
	INIT_DEVICE_PROC(InvalidateMappedMemoryRanges);
	INIT_DEVICE_PROC(BindBufferMemory);
	INIT_DEVICE_PROC(BindImageMemory);
	INIT_DEVICE_PROC(GetBufferMemoryRequirements);
	INIT_DEVICE_PROC(GetImageMemoryRequirements);
	INIT_DEVICE_PROC(CreateFence);
	INIT_DEVICE_PROC(DestroyFence);
	INIT_DEVICE_PROC(ResetFences);
	INIT_DEVICE_PROC(GetFenceStatus);
	INIT_DEVICE_PROC(WaitForFences);
	INIT_DEVICE_PROC(CreateSemaphore);
	INIT_DEVICE_PROC(DestroySemaphore);
	INIT_DEVICE_PROC(CreateQueryPool);
	INIT_DEVICE_PROC(DestroyQueryPool);
	INIT_DEVICE_PROC(GetQueryPoolResults);
	INIT_DEVICE_PROC(CreateBuffer);
	INIT_DEVICE_PROC(DestroyBuffer);
	INIT_DEVICE_PROC(CreateImage);
	INIT_DEVICE_PROC(DestroyImage);
	INIT_DEVICE_PROC(GetImageSubresourceLayout);
	INIT_DEVICE_PROC(CreateImageView);
	INIT_DEVICE_PROC(DestroyImageView);
	INIT_DEVICE_PROC(CreateShaderModule);
	INIT_DEVICE_PROC(DestroyShaderModule);
	INIT_DEVICE_PROC(CreateGraphicsPipelines);
	INIT_DEVICE_PROC(CreateComputePipelines);
	INIT_DEVICE_PROC(DestroyPipeline);
	INIT_DEVICE_PROC(CreatePipelineLayout);
	INIT_DEVICE_PROC(DestroyPipelineLayout);
	INIT_DEVICE_PROC(CreateSampler);
	INIT_DEVICE_PROC(DestroySampler);
	INIT_DEVICE_PROC(CreateDescriptorSetLayout);
	INIT_DEVICE_PROC(DestroyDescriptorSetLayout);
	INIT_DEVICE_PROC(CreateDescriptorPool);
	INIT_DEVICE_PROC(DestroyDescriptorPool);
	INIT_DEVICE_PROC(ResetDescriptorPool);
	INIT_DEVICE_PROC(AllocateDescriptorSets);
	INIT_DEVICE_PROC(FreeDescriptorSets);
	INIT_DEVICE_PROC(UpdateDescriptorSets);
	INIT_DEVICE_PROC(CreateFramebuffer);
	INIT_DEVICE_PROC(DestroyFramebuffer);
	INIT_DEVICE_PROC(CreateRenderPass);
	INIT_DEVICE_PROC(DestroyRenderPass);
	INIT_DEVICE_PROC(CreateCommandPool);
	INIT_DEVICE_PROC(DestroyCommandPool);
	INIT_DEVICE_PROC(ResetCommandPool);
	INIT_DEVICE_PROC(AllocateCommandBuffers);
	INIT_DEVICE_PROC(FreeCommandBuffers);
	INIT_DEVICE_PROC(BeginCommandBuffer);
	INIT_DEVICE_PROC(EndCommandBuffer);
	INIT_DEVICE_PROC(ResetCommandBuffer);
	INIT_DEVICE_PROC(CmdBindPipeline);
	INIT_DEVICE_PROC(CmdSetViewport);
	INIT_DEVICE_PROC(CmdSetScissor);
	INIT_DEVICE_PROC(CmdBindDescriptorSets);
	INIT_DEVICE_PROC(CmdBindIndexBuffer);
	INIT_DEVICE_PROC(CmdBindVertexBuffers);
	INIT_DEVICE_PROC(CmdDraw);
	INIT_DEVICE_PROC(CmdDrawIndexed);
	INIT_DEVICE_PROC(CmdDispatch);
	INIT_DEVICE_PROC(CmdCopyBuffer);
	INIT_DEVICE_PROC(CmdCopyImage);
	INIT_DEVICE_PROC(CmdBlitImage);
	INIT_DEVICE_PROC(CmdCopyBufferToImage);
	INIT_DEVICE_PROC(CmdCopyImageToBuffer);
	INIT_DEVICE_PROC(CmdUpdateBuffer);
	INIT_DEVICE_PROC(CmdClearColorImage);
	INIT_DEVICE_PROC(CmdClearDepthStencilImage);
	INIT_DEVICE_PROC(CmdClearAttachments);
	INIT_DEVICE_PROC(CmdPipelineBarrier);
	INIT_DEVICE_PROC(CmdResetQueryPool);
	INIT_DEVICE_PROC(CmdWriteTimestamp);
	INIT_DEVICE_PROC(CmdPushConstants);
	INIT_DEVICE_PROC(CmdBeginRenderPass);
	INIT_DEVICE_PROC(CmdNextSubpass);
	INIT_DEVICE_PROC(CmdEndRenderPass);
	INIT_DEVICE_PROC(CmdExecuteCommands);
	// ---- Core 1_1 commands
	INIT_DEVICE_PROC(BindBufferMemory2);
	INIT_DEVICE_PROC(BindImageMemory2);
	INIT_DEVICE_PROC(GetBufferMemoryRequirements2);
	INIT_DEVICE_PROC(GetImageMemoryRequirements2);
	INIT_DEVICE_PROC(GetDeviceQueue2);
	// ---- Core 1_2 commands
	INIT_DEVICE_PROC(CreateRenderPass2);
	if (dispatch_table.CreateRenderPass2 == nullptr) // Try the KHR version if the core version does not exist
		dispatch_table.CreateRenderPass2  = reinterpret_cast<PFN_vkCreateRenderPass2KHR>(get_device_proc(device, "vkCreateRenderPass2KHR"));
	// ---- VK_KHR_swapchain extension commands
	INIT_DEVICE_PROC(CreateSwapchainKHR);
	INIT_DEVICE_PROC(DestroySwapchainKHR);
	INIT_DEVICE_PROC(GetSwapchainImagesKHR);
	INIT_DEVICE_PROC(QueuePresentKHR);
	// ---- VK_KHR_push_descriptor extension commands
	INIT_DEVICE_PROC(CmdPushDescriptorSetKHR);
	// ---- VK_EXT_debug_marker extension commands
	INIT_DEVICE_PROC(DebugMarkerSetObjectNameEXT);
	INIT_DEVICE_PROC(CmdDebugMarkerBeginEXT);
	INIT_DEVICE_PROC(CmdDebugMarkerEndEXT);

	// Initialize per-device data
	s_device_dispatch.emplace(dispatch_key_from_handle(device), device_data { physicalDevice, dispatch_table, graphics_queue_family_index });

#if RESHADE_VERBOSE_LOG
	LOG(INFO) << "Returning Vulkan device " << device << '.';
#endif
	return VK_SUCCESS;
}
void     VKAPI_CALL vkDestroyDevice(VkDevice device, const VkAllocationCallbacks *pAllocator)
{
	LOG(INFO) << "Redirecting " << "vkDestroyDevice" << '(' << "device = " << device << ", pAllocator = " << pAllocator << ')' << " ...";

	s_command_buffer_data.clear(); // Reset all command buffer data

	// Get function pointer before removing it next
	GET_DEVICE_DISPATCH_PTR(DestroyDevice, device);
	// Remove device dispatch table since this device is being destroyed
	s_device_dispatch.erase(dispatch_key_from_handle(device));

	trampoline(device, pAllocator);
}

VkResult VKAPI_CALL vkCreateSwapchainKHR(VkDevice device, const VkSwapchainCreateInfoKHR *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkSwapchainKHR *pSwapchain)
{
	LOG(INFO) << "Redirecting " << "vkCreateSwapchainKHR" << '(' << "device = " << device << ", pCreateInfo = " << pCreateInfo << ", pAllocator = " << pAllocator << ", pSwapchain = " << pSwapchain << ')' << " ...";

	assert(pCreateInfo != nullptr && pSwapchain != nullptr);

	auto &device_data = s_device_dispatch.at(dispatch_key_from_handle(device));

	VkSwapchainCreateInfoKHR create_info = *pCreateInfo;
	VkImageFormatListCreateInfoKHR format_list_info { VK_STRUCTURE_TYPE_IMAGE_FORMAT_LIST_CREATE_INFO_KHR };
	std::vector<VkFormat> format_list; std::vector<uint32_t> queue_family_list;

	// Only have to enable additional features if there is a graphics queue, since ReShade will not run otherwise
	if (device_data.graphics_queue_family_index != std::numeric_limits<uint32_t>::max())
	{
		// Add required usage flags to create info
		create_info.imageUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

		// Add required formats, so views with different formats can be created for the swap chain images
		format_list.push_back(make_format_srgb(pCreateInfo->imageFormat));
		format_list.push_back(make_format_normal(pCreateInfo->imageFormat));

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

			// This is evil, because writing into the application memory, but whatever
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
			queue_family_list.push_back(device_data.graphics_queue_family_index);

			for (uint32_t i = 0; i < create_info.queueFamilyIndexCount; ++i)
				if (create_info.pQueueFamilyIndices[i] != device_data.graphics_queue_family_index)
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
	if (const char *format_string = format_to_string(create_info.imageFormat); format_string != nullptr)
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

	const VkResult result = device_data.dispatch_table.CreateSwapchainKHR(device, &create_info, pAllocator, pSwapchain);
	if (result != VK_SUCCESS)
	{
		LOG(WARN) << "vkCreateSwapchainKHR" << " failed with error code " << result << '.';
		return result;
	}

	if (device_data.graphics_queue_family_index != std::numeric_limits<uint32_t>::max())
	{
		reshade::vulkan::runtime_vk *runtime;
		// Remove old swap chain from the list so that a call to 'vkDestroySwapchainKHR' won't reset the runtime again
		if (s_vulkan_runtimes.erase(pCreateInfo->oldSwapchain, runtime))
		{
			assert(pCreateInfo->oldSwapchain != VK_NULL_HANDLE);

			// Re-use the existing runtime if this swap chain was not created from scratch
			runtime->on_reset(); // But reset it before initializing again below
		}
		else
		{
			runtime = new reshade::vulkan::runtime_vk(
				device, device_data.physical_device, device_data.graphics_queue_family_index,
				s_instance_dispatch.at(dispatch_key_from_handle(device_data.physical_device)), device_data.dispatch_table,
				&device_data.state);
		}

		// Look up window handle from surface
		const HWND hwnd = s_surface_windows.at(pCreateInfo->surface);

		if (!runtime->on_init(*pSwapchain, create_info, hwnd))
			LOG(ERROR) << "Failed to initialize Vulkan runtime environment on runtime " << runtime << '.';

		s_vulkan_runtimes.emplace(*pSwapchain, runtime);
	}
	else
	{
		s_vulkan_runtimes.emplace(*pSwapchain, nullptr);
	}

#if RESHADE_VERBOSE_LOG
	LOG(INFO) << "Returning Vulkan swapchain " << *pSwapchain << '.';
#endif
	return VK_SUCCESS;
}
void     VKAPI_CALL vkDestroySwapchainKHR(VkDevice device, VkSwapchainKHR swapchain, const VkAllocationCallbacks *pAllocator)
{
	LOG(INFO) << "Redirecting " << "vkDestroySwapchainKHR" << '(' << device << ", " << swapchain << ", " << pAllocator << ')' << " ...";

	// Remove runtime from global list
	if (reshade::vulkan::runtime_vk *runtime;
		s_vulkan_runtimes.erase(swapchain, runtime))
	{
		delete runtime;
	}

	GET_DEVICE_DISPATCH_PTR(DestroySwapchainKHR, device);
	trampoline(device, swapchain, pAllocator);
}

VkResult VKAPI_CALL vkQueueSubmit(VkQueue queue, uint32_t submitCount, const VkSubmitInfo *pSubmits, VkFence fence)
{
	assert(pSubmits != nullptr);

	auto &device_data = s_device_dispatch.at(dispatch_key_from_handle(queue));

	for (uint32_t i = 0; i < submitCount; ++i)
	{
		for (uint32_t k = 0; k < pSubmits[i].commandBufferCount; ++k)
		{
			VkCommandBuffer cmd = pSubmits[i].pCommandBuffers[k];
			assert(cmd != VK_NULL_HANDLE);

			auto &command_buffer_data = s_command_buffer_data.at(cmd);

			// Merge command list trackers into device one
			device_data.state.merge(command_buffer_data.state);
		}
	}

	// The loader uses the same dispatch table pointer for queues and devices, so can use queue to perform lookup here
	return device_data.dispatch_table.QueueSubmit(queue, submitCount, pSubmits, fence);
}
VkResult VKAPI_CALL vkQueuePresentKHR(VkQueue queue, const VkPresentInfoKHR *pPresentInfo)
{
	assert(pPresentInfo != nullptr);

	auto &device_data = s_device_dispatch.at(dispatch_key_from_handle(queue));

	std::vector<VkSemaphore> wait_semaphores(
		pPresentInfo->pWaitSemaphores, pPresentInfo->pWaitSemaphores + pPresentInfo->waitSemaphoreCount);

	for (uint32_t i = 0; i < pPresentInfo->swapchainCount; ++i)
	{
		if (const auto runtime = s_vulkan_runtimes.at(pPresentInfo->pSwapchains[i]);
			runtime != nullptr)
			runtime->on_present(queue, pPresentInfo->pImageIndices[i], wait_semaphores);
	}

	device_data.state.reset();

	// Override wait semaphores based on the last queue submit from above
	VkPresentInfoKHR present_info = *pPresentInfo;
	present_info.waitSemaphoreCount = static_cast<uint32_t>(wait_semaphores.size());
	present_info.pWaitSemaphores = wait_semaphores.data();

	return device_data.dispatch_table.QueuePresentKHR(queue, &present_info);
}

VkResult VKAPI_CALL vkCreateImage(VkDevice device, const VkImageCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkImage *pImage)
{
	assert(pCreateInfo != nullptr && pImage != nullptr);

	VkImageCreateInfo create_info = *pCreateInfo;
	// Allow shader access to images that are used as depth-stencil attachments
	const bool is_depth_stencil_image = create_info.usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	if (is_depth_stencil_image)
		create_info.usage |= VK_IMAGE_USAGE_SAMPLED_BIT;

	GET_DEVICE_DISPATCH_PTR(CreateImage, device);
	const VkResult result = trampoline(device, &create_info, pAllocator, pImage);
	if (result != VK_SUCCESS)
	{
		LOG(WARN) << "vkCreateImage" << " failed with error code " << result << '.';
		return result;
	}

	// Keep track of image information (only care about depth-stencil images currently)
	if (aspect_flags_from_format(pCreateInfo->format) != VK_IMAGE_ASPECT_COLOR_BIT)
		s_image_data.emplace(*pImage, *pCreateInfo);

	return VK_SUCCESS;
}
void     VKAPI_CALL vkDestroyImage(VkDevice device, VkImage image, const VkAllocationCallbacks *pAllocator)
{
	s_image_data.erase(image);

	GET_DEVICE_DISPATCH_PTR(DestroyImage, device);
	trampoline(device, image, pAllocator);
}

VkResult VKAPI_CALL vkCreateImageView(VkDevice device, const VkImageViewCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkImageView *pView)
{
	assert(pCreateInfo != nullptr && pView != nullptr);

	GET_DEVICE_DISPATCH_PTR(CreateImageView, device);
	const VkResult result = trampoline(device, pCreateInfo, pAllocator, pView);
	if (result != VK_SUCCESS)
	{
		LOG(WARN) << "vkCreateImageView" << " failed with error code " << result << '.';
		return result;
	}

	// Keep track of image view information (only care about depth-stencil image views currently)
	if (aspect_flags_from_format(pCreateInfo->format) != VK_IMAGE_ASPECT_COLOR_BIT)
		s_image_view_mapping.emplace(*pView, pCreateInfo->image);

	return VK_SUCCESS;
}
void     VKAPI_CALL vkDestroyImageView(VkDevice device, VkImageView imageView, const VkAllocationCallbacks *pAllocator)
{
	s_image_view_mapping.erase(imageView);

	GET_DEVICE_DISPATCH_PTR(DestroyImageView, device);
	trampoline(device, imageView, pAllocator);
}

VkResult VKAPI_CALL vkCreateRenderPass(VkDevice device, const VkRenderPassCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkRenderPass *pRenderPass)
{
	assert(pCreateInfo != nullptr && pRenderPass != nullptr);

	GET_DEVICE_DISPATCH_PTR(CreateRenderPass, device);
	const VkResult result = trampoline(device, pCreateInfo, pAllocator, pRenderPass);
	if (result != VK_SUCCESS)
	{
		LOG(WARN) << "vkCreateRenderPass" << " failed with error code " << result << '.';
		return result;
	}

	auto &renderpass_data = s_renderpass_data.emplace(*pRenderPass);

	// Search for the first pass using a depth-stencil attachment
	for (uint32_t subpass = 0; subpass < pCreateInfo->subpassCount; ++subpass)
	{
		auto &subpass_data = renderpass_data.emplace_back();

		const VkAttachmentReference *const ds_reference = pCreateInfo->pSubpasses[subpass].pDepthStencilAttachment;
		if (ds_reference != nullptr && ds_reference->attachment != VK_ATTACHMENT_UNUSED)
		{
			const VkAttachmentDescription &ds_attachment = pCreateInfo->pAttachments[ds_reference->attachment];
			subpass_data.final_depthstencil_layout = ds_attachment.finalLayout;
			subpass_data.depthstencil_attachment_index = ds_reference->attachment;
		}
	}

	return VK_SUCCESS;
}
VkResult VKAPI_CALL vkCreateRenderPass2(VkDevice device, const VkRenderPassCreateInfo2 *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkRenderPass *pRenderPass)
{
	assert(pCreateInfo != nullptr && pRenderPass != nullptr);

	GET_DEVICE_DISPATCH_PTR(CreateRenderPass2, device);
	const VkResult result = trampoline(device, pCreateInfo, pAllocator, pRenderPass);
	if (result != VK_SUCCESS)
	{
		LOG(WARN) << "vkCreateRenderPass2" << " failed with error code " << result << '.';
		return result;
	}

	auto &renderpass_data = s_renderpass_data.emplace(*pRenderPass);

	// Search for the first pass using a depth-stencil attachment
	for (uint32_t subpass = 0; subpass < pCreateInfo->subpassCount; ++subpass)
	{
		auto &subpass_data = renderpass_data.emplace_back();

		const VkAttachmentReference2 *const ds_reference = pCreateInfo->pSubpasses[subpass].pDepthStencilAttachment;
		if (ds_reference != nullptr && ds_reference->attachment != VK_ATTACHMENT_UNUSED)
		{
			const VkAttachmentDescription2 &ds_attachment = pCreateInfo->pAttachments[ds_reference->attachment];
			subpass_data.final_depthstencil_layout = ds_attachment.finalLayout;
			subpass_data.depthstencil_attachment_index = ds_reference->attachment;
		}
	}

	return VK_SUCCESS;
}
void     VKAPI_CALL vkDestroyRenderPass(VkDevice device, VkRenderPass renderPass, const VkAllocationCallbacks *pAllocator)
{
	s_renderpass_data.erase(renderPass);

	GET_DEVICE_DISPATCH_PTR(DestroyRenderPass, device);
	trampoline(device, renderPass, pAllocator);
}

VkResult VKAPI_CALL vkCreateFramebuffer(VkDevice device, const VkFramebufferCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkFramebuffer *pFramebuffer)
{
	GET_DEVICE_DISPATCH_PTR(CreateFramebuffer, device);
	const VkResult result = trampoline(device, pCreateInfo, pAllocator, pFramebuffer);
	if (result != VK_SUCCESS)
	{
		LOG(WARN) << "vkCreateFramebuffer" << " failed with error code " << result << '.';
		return result;
	}

	// Look up the depth-stencil images associated with their image views
	std::vector<VkImage> attachment_images(pCreateInfo->attachmentCount);
	for (const auto &subpass_data : s_renderpass_data.at(pCreateInfo->renderPass))
		if (subpass_data.depthstencil_attachment_index < pCreateInfo->attachmentCount)
			attachment_images[subpass_data.depthstencil_attachment_index] = s_image_view_mapping.at(
				pCreateInfo->pAttachments[subpass_data.depthstencil_attachment_index]);

	// Keep track of depth-stencil image in this frame buffer
	s_framebuffer_data.emplace(*pFramebuffer, std::move(attachment_images));

	return VK_SUCCESS;
}
void     VKAPI_CALL vkDestroyFramebuffer(VkDevice device, VkFramebuffer framebuffer, const VkAllocationCallbacks *pAllocator)
{
	s_framebuffer_data.erase(framebuffer);

	GET_DEVICE_DISPATCH_PTR(DestroyFramebuffer, device);
	trampoline(device, framebuffer, pAllocator);
}

VkResult VKAPI_CALL vkAllocateCommandBuffers(VkDevice device, const VkCommandBufferAllocateInfo *pAllocateInfo, VkCommandBuffer *pCommandBuffers)
{
	GET_DEVICE_DISPATCH_PTR(AllocateCommandBuffers, device);
	const VkResult result = trampoline(device, pAllocateInfo, pCommandBuffers);
	if (result != VK_SUCCESS)
	{
		LOG(WARN) << "vkAllocateCommandBuffers" << " failed with error code " << result << '.';
		return result;
	}

	for (uint32_t i = 0; i < pAllocateInfo->commandBufferCount; ++i)
		s_command_buffer_data.emplace(pCommandBuffers[i]);

	return VK_SUCCESS;
}
void     VKAPI_CALL vkFreeCommandBuffers(VkDevice device, VkCommandPool commandPool, uint32_t commandBufferCount, const VkCommandBuffer *pCommandBuffers)
{
	for (uint32_t i = 0; i < commandBufferCount; ++i)
		s_command_buffer_data.erase(pCommandBuffers[i]);

	GET_DEVICE_DISPATCH_PTR(FreeCommandBuffers, device);
	trampoline(device, commandPool, commandBufferCount, pCommandBuffers);
}

VkResult VKAPI_CALL vkBeginCommandBuffer(VkCommandBuffer commandBuffer, const VkCommandBufferBeginInfo *pBeginInfo)
{
	// Begin does perform an implicit reset if command pool was created with VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT
	auto &data = s_command_buffer_data.at(commandBuffer);
	data.state.reset();

	GET_DEVICE_DISPATCH_PTR(BeginCommandBuffer, commandBuffer);
	return trampoline(commandBuffer, pBeginInfo);
}

void     VKAPI_CALL vkCmdBeginRenderPass(VkCommandBuffer commandBuffer, const VkRenderPassBeginInfo *pRenderPassBegin, VkSubpassContents contents)
{
#if RESHADE_DEPTH
	auto &data = s_command_buffer_data.at(commandBuffer);
	data.current_subpass = 0;
	data.current_renderpass = pRenderPassBegin->renderPass;
	data.current_framebuffer = pRenderPassBegin->framebuffer;

	const auto &renderpass_data = s_renderpass_data.at(data.current_renderpass)[data.current_subpass];
	const auto &framebuffer_data = s_framebuffer_data.at(data.current_framebuffer);
	if (renderpass_data.depthstencil_attachment_index < framebuffer_data.size())
	{
		const VkImage depthstencil = framebuffer_data[renderpass_data.depthstencil_attachment_index];
		VkImageLayout depthstencil_layout = renderpass_data.final_depthstencil_layout;

		data.state.on_set_depthstencil(
			depthstencil,
			depthstencil_layout,
			s_image_data.at(depthstencil));
	}
	else
	{
		data.state.on_set_depthstencil(VK_NULL_HANDLE, VK_IMAGE_LAYOUT_UNDEFINED, {});
	}
#endif

	GET_DEVICE_DISPATCH_PTR(CmdBeginRenderPass, commandBuffer);
	trampoline(commandBuffer, pRenderPassBegin, contents);
}
void     VKAPI_CALL vkCmdNextSubpass(VkCommandBuffer commandBuffer, VkSubpassContents contents)
{
	GET_DEVICE_DISPATCH_PTR(CmdNextSubpass, commandBuffer);
	trampoline(commandBuffer, contents);

#if RESHADE_DEPTH
	auto &data = s_command_buffer_data.at(commandBuffer);
	data.current_subpass++;
	assert(data.current_renderpass != VK_NULL_HANDLE);
	assert(data.current_framebuffer != VK_NULL_HANDLE);

	const auto &renderpass_data = s_renderpass_data.at(data.current_renderpass)[data.current_subpass];
	const auto &framebuffer_data = s_framebuffer_data.at(data.current_framebuffer);
	if (renderpass_data.depthstencil_attachment_index < framebuffer_data.size())
	{
		const VkImage depthstencil = framebuffer_data[renderpass_data.depthstencil_attachment_index];
		VkImageLayout depthstencil_layout = renderpass_data.final_depthstencil_layout;

		data.state.on_set_depthstencil(
			depthstencil,
			depthstencil_layout,
			s_image_data.at(depthstencil));
	}
	else
	{
		data.state.on_set_depthstencil(VK_NULL_HANDLE, VK_IMAGE_LAYOUT_UNDEFINED, {});
	}
#endif
}
void     VKAPI_CALL vkCmdEndRenderPass(VkCommandBuffer commandBuffer)
{
	GET_DEVICE_DISPATCH_PTR(CmdEndRenderPass, commandBuffer);
	trampoline(commandBuffer);

#if RESHADE_DEPTH
	auto &data = s_command_buffer_data.at(commandBuffer);
	data.current_subpass = std::numeric_limits<uint32_t>::max();
	data.current_renderpass = VK_NULL_HANDLE;
	data.current_framebuffer = VK_NULL_HANDLE;

	data.state.on_set_depthstencil(VK_NULL_HANDLE, VK_IMAGE_LAYOUT_UNDEFINED, {});
#endif
}

void     VKAPI_CALL vkCmdExecuteCommands(VkCommandBuffer commandBuffer, uint32_t commandBufferCount, const VkCommandBuffer *pCommandBuffers)
{
	auto &data = s_command_buffer_data.at(commandBuffer);

	for (uint32_t i = 0; i < commandBufferCount; ++i)
	{
		const auto &secondary_data = s_command_buffer_data.at(pCommandBuffers[i]);

		// Merge secondary command list trackers into the current primary one
		data.state.merge(secondary_data.state);
	}

	GET_DEVICE_DISPATCH_PTR(CmdExecuteCommands, commandBuffer);
	trampoline(commandBuffer, commandBufferCount, pCommandBuffers);
}

void     VKAPI_CALL vkCmdDraw(VkCommandBuffer commandBuffer, uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance)
{
	GET_DEVICE_DISPATCH_PTR(CmdDraw, commandBuffer);
	trampoline(commandBuffer, vertexCount, instanceCount, firstVertex, firstInstance);

	auto &data = s_command_buffer_data.at(commandBuffer);
	data.state.on_draw(vertexCount * instanceCount);
}
void     VKAPI_CALL vkCmdDrawIndexed(VkCommandBuffer commandBuffer, uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance)
{
	GET_DEVICE_DISPATCH_PTR(CmdDrawIndexed, commandBuffer);
	trampoline(commandBuffer, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);

	auto &data = s_command_buffer_data.at(commandBuffer);
	data.state.on_draw(indexCount * instanceCount);
}

VK_LAYER_EXPORT PFN_vkVoidFunction VKAPI_CALL vkGetDeviceProcAddr(VkDevice device, const char *pName)
{
#define CHECK_DEVICE_PROC(name) \
	if (0 == strcmp(pName, "vk" #name)) \
		return reinterpret_cast<PFN_vkVoidFunction>(vk##name)

	// The Vulkan loader gets the 'vkDestroyDevice' function from the device dispatch table
	CHECK_DEVICE_PROC(DestroyDevice);

	CHECK_DEVICE_PROC(CreateSwapchainKHR);
	CHECK_DEVICE_PROC(DestroySwapchainKHR);
	CHECK_DEVICE_PROC(QueueSubmit);
	CHECK_DEVICE_PROC(QueuePresentKHR);

	CHECK_DEVICE_PROC(CreateImage);
	CHECK_DEVICE_PROC(DestroyImage);
	CHECK_DEVICE_PROC(CreateImageView);
	CHECK_DEVICE_PROC(DestroyImageView);
	CHECK_DEVICE_PROC(CreateRenderPass);
	CHECK_DEVICE_PROC(CreateRenderPass2);
	if (0 == strcmp(pName, "vkCreateRenderPass2KHR"))
		return reinterpret_cast<PFN_vkVoidFunction>(vkCreateRenderPass2);
	CHECK_DEVICE_PROC(DestroyRenderPass);
	CHECK_DEVICE_PROC(CreateFramebuffer);
	CHECK_DEVICE_PROC(DestroyFramebuffer);

	CHECK_DEVICE_PROC(AllocateCommandBuffers);
	CHECK_DEVICE_PROC(FreeCommandBuffers);
	CHECK_DEVICE_PROC(BeginCommandBuffer);

	CHECK_DEVICE_PROC(CmdBeginRenderPass);
	CHECK_DEVICE_PROC(CmdNextSubpass);
	CHECK_DEVICE_PROC(CmdEndRenderPass);
	CHECK_DEVICE_PROC(CmdExecuteCommands);
	CHECK_DEVICE_PROC(CmdDraw);
	CHECK_DEVICE_PROC(CmdDrawIndexed);

	// Need to self-intercept as well, since some layers rely on this (e.g. Steam overlay)
	// See https://github.com/KhronosGroup/Vulkan-Loader/blob/master/loader/LoaderAndLayerInterface.md#layer-conventions-and-rules
	CHECK_DEVICE_PROC(GetDeviceProcAddr);

	if (device == VK_NULL_HANDLE)
		return nullptr;

	GET_DEVICE_DISPATCH_PTR(GetDeviceProcAddr, device);
	return trampoline(device, pName);
}
