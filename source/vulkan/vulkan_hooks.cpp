/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "log.hpp"
#include "hook_manager.hpp"
#include "runtime_vk.hpp"
#include "vk_layer.h"
#include "vk_layer_dispatch_table.h"
#include "format_utils.hpp"
#include "lockfree_table.hpp"
#include <memory>

struct device_data
{
	VkPhysicalDevice physical_device;
	reshade::vulkan::buffer_detection_context buffer_detection;
};

struct render_pass_data
{
	uint32_t depthstencil_attachment_index = std::numeric_limits<uint32_t>::max();
	VkImageLayout initial_depthstencil_layout = VK_IMAGE_LAYOUT_UNDEFINED;
};

struct command_buffer_data
{
	// State tracking for render passes
	uint32_t current_subpass = std::numeric_limits<uint32_t>::max();
	VkRenderPass current_renderpass = VK_NULL_HANDLE;
	VkFramebuffer current_framebuffer = VK_NULL_HANDLE;
	reshade::vulkan::buffer_detection buffer_detection;
};

static lockfree_table<void *, device_data, 16> s_device_data;
static lockfree_table<void *, VkLayerDispatchTable, 16> s_device_dispatch;
static lockfree_table<void *, VkLayerInstanceDispatchTable, 16> s_instance_dispatch;
static lockfree_table<VkSurfaceKHR, HWND, 16> s_surface_windows;
static lockfree_table<VkSwapchainKHR, std::shared_ptr<reshade::vulkan::runtime_vk>, 16> s_runtimes;
static lockfree_table<VkImage, VkImageCreateInfo, 4096> s_image_data;
static lockfree_table<VkImageView, VkImage, 4096> s_image_view_mapping;
static lockfree_table<VkFramebuffer, std::vector<VkImage>, 256> s_framebuffer_data;
static lockfree_table<VkCommandBuffer, command_buffer_data, 4096> s_command_buffer_data;
static lockfree_table<VkRenderPass, std::vector<render_pass_data>, 4096> s_renderpass_data;

static inline void *dispatch_key_from_handle(const void *dispatch_handle)
{
	// The Vulkan loader writes the dispatch table pointer right to the start of the object, so use that as a key for lookup
	// This ensures that all objects of a specific level (device or instance) will use the same dispatch table
	return *(void **)dispatch_handle;
}

#define GET_DEVICE_DISPATCH_PTR(name, object) \
	PFN_vk##name trampoline = s_device_dispatch.at(dispatch_key_from_handle(object)).name; \
	assert(trampoline != nullptr);
#define GET_INSTANCE_DISPATCH_PTR(name, object) \
	PFN_vk##name trampoline = s_instance_dispatch.at(dispatch_key_from_handle(object)).name; \
	assert(trampoline != nullptr); \

VkResult VKAPI_CALL vkCreateInstance(const VkInstanceCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkInstance *pInstance)
{
	LOG(INFO) << "Redirecting vkCreateInstance" << '(' << "pCreateInfo = " << pCreateInfo << ", pAllocator = " << pAllocator << ", pInstance = " << pInstance << ')' << " ...";

	assert(pCreateInfo != nullptr && pInstance != nullptr);

	// Look for layer link info if installed as a layer (provided by the Vulkan loader)
	VkLayerInstanceCreateInfo *link_info = (VkLayerInstanceCreateInfo *)pCreateInfo->pNext;
	while (link_info != nullptr && !(link_info->sType == VK_STRUCTURE_TYPE_LOADER_INSTANCE_CREATE_INFO && link_info->function == VK_LAYER_LINK_INFO))
		link_info = (VkLayerInstanceCreateInfo *)link_info->pNext;

	// Get trampoline function pointers
	PFN_vkGetInstanceProcAddr gipa = nullptr;
	PFN_vkCreateInstance trampoline = nullptr;

	if (link_info != nullptr) {
		assert(link_info->u.pLayerInfo != nullptr);
		assert(link_info->u.pLayerInfo->pfnNextGetInstanceProcAddr != nullptr);

		// Look up functions in layer info
		gipa = link_info->u.pLayerInfo->pfnNextGetInstanceProcAddr;
		trampoline = reinterpret_cast<PFN_vkCreateInstance>(gipa(nullptr, "vkCreateInstance"));

		// Advance the link info for the next element of the chain
		link_info->u.pLayerInfo = link_info->u.pLayerInfo->pNext;
	}
#ifdef RESHADE_TEST_APPLICATION
	else
	{
		gipa = reshade::hooks::call(vkGetInstanceProcAddr);
		trampoline = reshade::hooks::call(vkCreateInstance);
	}
#endif

	if (trampoline == nullptr) // Unable to resolve next 'vkCreateInstance' function in the call chain
		return VK_ERROR_INITIALIZATION_FAILED;

	// Continue call down the chain
	const VkResult result = trampoline(pCreateInfo, pAllocator, pInstance);
	if (result != VK_SUCCESS)
	{
		LOG(WARN) << "vkCreateInstance failed with error code " << result << '!';
		return result;
	}

	VkInstance instance = *pInstance;
	// Initialize the instance dispatch table
	VkLayerInstanceDispatchTable &dispatch_table = s_instance_dispatch.emplace(dispatch_key_from_handle(instance));
	// ---- Core 1_0 commands
	dispatch_table.DestroyInstance = (PFN_vkDestroyInstance)gipa(instance, "vkDestroyInstance");
	dispatch_table.EnumeratePhysicalDevices = (PFN_vkEnumeratePhysicalDevices)gipa(instance, "vkEnumeratePhysicalDevices");
	dispatch_table.GetPhysicalDeviceMemoryProperties = (PFN_vkGetPhysicalDeviceMemoryProperties)gipa(instance, "vkGetPhysicalDeviceMemoryProperties");
	dispatch_table.GetPhysicalDeviceQueueFamilyProperties = (PFN_vkGetPhysicalDeviceQueueFamilyProperties)gipa(instance, "vkGetPhysicalDeviceQueueFamilyProperties");
	dispatch_table.GetInstanceProcAddr = gipa;
	// ---- VK_KHR_surface extension commands
	dispatch_table.DestroySurfaceKHR = (PFN_vkDestroySurfaceKHR)gipa(instance, "vkDestroySurfaceKHR");
	// ---- VK_KHR_win32_surface extension commands
	dispatch_table.CreateWin32SurfaceKHR = (PFN_vkCreateWin32SurfaceKHR)gipa(instance, "vkCreateWin32SurfaceKHR");

#if RESHADE_VERBOSE_LOG
	LOG(INFO) << "> Returning Vulkan instance " << instance << '.';
#endif
	return VK_SUCCESS;
}
void     VKAPI_CALL vkDestroyInstance(VkInstance instance, const VkAllocationCallbacks *pAllocator)
{
	LOG(INFO) << "Redirecting vkDestroyInstance" << '(' << "instance = " << instance << ", pAllocator = " << pAllocator << ')' << " ...";

	// Get function pointer before removing it next
	GET_INSTANCE_DISPATCH_PTR(DestroyInstance, instance);
	// Remove instance dispatch table since this instance is being destroyed
	s_instance_dispatch.erase(dispatch_key_from_handle(instance));

	trampoline(instance, pAllocator);
}

VkResult VKAPI_CALL vkCreateWin32SurfaceKHR(VkInstance instance, const VkWin32SurfaceCreateInfoKHR *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkSurfaceKHR *pSurface)
{
	LOG(INFO) << "Redirecting vkCreateWin32SurfaceKHR" << '(' << "instance = " << instance << ", pCreateInfo = " << pCreateInfo << ", pAllocator = " << pAllocator << ", pSurface = " << pSurface << ')' << " ...";

	GET_INSTANCE_DISPATCH_PTR(CreateWin32SurfaceKHR, instance);
	const VkResult result = trampoline(instance, pCreateInfo, pAllocator, pSurface);
	if (result != VK_SUCCESS)
	{
		LOG(WARN) << "vkCreateWin32SurfaceKHR failed with error code " << result << '!';
		return result;
	}

	s_surface_windows.emplace(*pSurface, pCreateInfo->hwnd);

	return VK_SUCCESS;
}
void     VKAPI_CALL vkDestroySurfaceKHR(VkInstance instance, VkSurfaceKHR surface, const VkAllocationCallbacks *pAllocator)
{
	LOG(INFO) << "Redirecting vkDestroySurfaceKHR" << '(' << "instance = " << instance << ", surface = " << surface << ", pAllocator = " << pAllocator << ')' << " ...";

	s_surface_windows.erase(surface);

	GET_INSTANCE_DISPATCH_PTR(DestroySurfaceKHR, instance);
	trampoline(instance, surface, pAllocator);
}

VkResult VKAPI_CALL vkCreateDevice(VkPhysicalDevice physicalDevice, const VkDeviceCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkDevice *pDevice)
{
	LOG(INFO) << "Redirecting vkCreateDevice" << '(' << "physicalDevice = " << physicalDevice << ", pCreateInfo = " << pCreateInfo << ", pAllocator = " << pAllocator << ", pDevice = " << pDevice << ')' << " ...";

	assert(pCreateInfo != nullptr && pDevice != nullptr);

	// Look for layer link info if installed as a layer (provided by the Vulkan loader)
	VkLayerDeviceCreateInfo *link_info = (VkLayerDeviceCreateInfo *)pCreateInfo->pNext;
	while (link_info != nullptr && !(link_info->sType == VK_STRUCTURE_TYPE_LOADER_DEVICE_CREATE_INFO && link_info->function == VK_LAYER_LINK_INFO))
		link_info = (VkLayerDeviceCreateInfo *)link_info->pNext;

	// Get trampoline function pointers
	PFN_vkGetDeviceProcAddr gdpa = nullptr;
	PFN_vkGetInstanceProcAddr gipa = nullptr;
	PFN_vkCreateDevice trampoline = nullptr;

	if (link_info != nullptr) {
		assert(link_info->u.pLayerInfo != nullptr);
		assert(link_info->u.pLayerInfo->pfnNextGetDeviceProcAddr != nullptr);
		assert(link_info->u.pLayerInfo->pfnNextGetInstanceProcAddr != nullptr);

		// Look up functions in layer info
		gdpa = link_info->u.pLayerInfo->pfnNextGetDeviceProcAddr;
		gipa = link_info->u.pLayerInfo->pfnNextGetInstanceProcAddr;
		trampoline = reinterpret_cast<PFN_vkCreateDevice>(gipa(nullptr, "vkCreateDevice"));

		// Advance the link info for the next element on the chain
		link_info->u.pLayerInfo = link_info->u.pLayerInfo->pNext;
	}
#ifdef RESHADE_TEST_APPLICATION
	else
	{
		gdpa = reshade::hooks::call(vkGetDeviceProcAddr);
		gipa = reshade::hooks::call(vkGetInstanceProcAddr);
		trampoline = reshade::hooks::call(vkCreateDevice);
	}
#endif

	if (trampoline == nullptr) // Unable to resolve next 'vkCreateDevice' function in the call chain
		return VK_ERROR_INITIALIZATION_FAILED;

	// TODO: Search pNext chain for VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2
	VkPhysicalDeviceFeatures enabled_features = {};
	if (pCreateInfo->pEnabledFeatures != nullptr)
		enabled_features = *pCreateInfo->pEnabledFeatures;

	std::vector<const char *> enabled_extensions;
	enabled_extensions.reserve(pCreateInfo->enabledExtensionCount);
	for (uint32_t i = 0; i < pCreateInfo->enabledExtensionCount; ++i)
		enabled_extensions.push_back(pCreateInfo->ppEnabledExtensionNames[i]);

	// Enable features that ReShade requires
	enabled_features.shaderImageGatherExtended = true;

	// Enable extensions that ReShade requires
	enabled_extensions.push_back(VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME);

	// TODO: Make sure a graphics queue exists

	VkDeviceCreateInfo create_info = *pCreateInfo;
	create_info.enabledExtensionCount = uint32_t(enabled_extensions.size());
	create_info.ppEnabledExtensionNames = enabled_extensions.data();
	create_info.pEnabledFeatures = &enabled_features;

	// Continue call down the chain
	const VkResult result = trampoline(physicalDevice, &create_info, pAllocator, pDevice);
	if (result != VK_SUCCESS)
	{
		LOG(WARN) << "vkCreateDevice failed with error code " << result << '!';
		return result;
	}

	VkDevice device = *pDevice;
	// Initialize the device dispatch table
	VkLayerDispatchTable &dispatch_table = s_device_dispatch.emplace(dispatch_key_from_handle(device));
	// ---- Core 1_0 commands
	dispatch_table.GetDeviceProcAddr = gdpa;
	dispatch_table.DestroyDevice = (PFN_vkDestroyDevice)gdpa(device, "vkDestroyDevice");
	dispatch_table.GetDeviceQueue = (PFN_vkGetDeviceQueue)gdpa(device, "vkGetDeviceQueue");
	dispatch_table.QueueSubmit = (PFN_vkQueueSubmit)gdpa(device, "vkQueueSubmit");
	dispatch_table.QueueWaitIdle = (PFN_vkQueueWaitIdle)gdpa(device, "vkQueueWaitIdle");
	dispatch_table.DeviceWaitIdle = (PFN_vkDeviceWaitIdle)gdpa(device, "vkDeviceWaitIdle");
	dispatch_table.AllocateMemory = (PFN_vkAllocateMemory)gdpa(device, "vkAllocateMemory");
	dispatch_table.FreeMemory = (PFN_vkFreeMemory)gdpa(device, "vkFreeMemory");
	dispatch_table.MapMemory = (PFN_vkMapMemory)gdpa(device, "vkMapMemory");
	dispatch_table.UnmapMemory = (PFN_vkUnmapMemory)gdpa(device, "vkUnmapMemory");
	dispatch_table.BindBufferMemory = (PFN_vkBindBufferMemory)gdpa(device, "vkBindBufferMemory");
	dispatch_table.BindImageMemory = (PFN_vkBindImageMemory)gdpa(device, "vkBindImageMemory");
	dispatch_table.GetBufferMemoryRequirements = (PFN_vkGetBufferMemoryRequirements)gdpa(device, "vkGetBufferMemoryRequirements");
	dispatch_table.GetImageMemoryRequirements = (PFN_vkGetImageMemoryRequirements)gdpa(device, "vkGetImageMemoryRequirements");
	dispatch_table.CreateFence = (PFN_vkCreateFence)gdpa(device, "vkCreateFence");
	dispatch_table.DestroyFence = (PFN_vkDestroyFence)gdpa(device, "vkDestroyFence");
	dispatch_table.ResetFences = (PFN_vkResetFences)gdpa(device, "vkResetFences");
	dispatch_table.GetFenceStatus = (PFN_vkGetFenceStatus)gdpa(device, "vkGetFenceStatus");
	dispatch_table.WaitForFences = (PFN_vkWaitForFences)gdpa(device, "vkWaitForFences");
	dispatch_table.CreateQueryPool = (PFN_vkCreateQueryPool)gdpa(device, "vkCreateQueryPool");
	dispatch_table.DestroyQueryPool = (PFN_vkDestroyQueryPool)gdpa(device, "vkDestroyQueryPool");
	dispatch_table.GetQueryPoolResults = (PFN_vkGetQueryPoolResults)gdpa(device, "vkGetQueryPoolResults");
	dispatch_table.CreateBuffer = (PFN_vkCreateBuffer)gdpa(device, "vkCreateBuffer");
	dispatch_table.DestroyBuffer = (PFN_vkDestroyBuffer)gdpa(device, "vkDestroyBuffer");
	dispatch_table.CreateImage = (PFN_vkCreateImage)gdpa(device, "vkCreateImage");
	dispatch_table.DestroyImage = (PFN_vkDestroyImage)gdpa(device, "vkDestroyImage");
	dispatch_table.GetImageSubresourceLayout = (PFN_vkGetImageSubresourceLayout)gdpa(device, "vkGetImageSubresourceLayout");
	dispatch_table.CreateImageView = (PFN_vkCreateImageView)gdpa(device, "vkCreateImageView");
	dispatch_table.DestroyImageView = (PFN_vkDestroyImageView)gdpa(device, "vkDestroyImageView");
	dispatch_table.CreateShaderModule = (PFN_vkCreateShaderModule)gdpa(device, "vkCreateShaderModule");
	dispatch_table.DestroyShaderModule = (PFN_vkDestroyShaderModule)gdpa(device, "vkDestroyShaderModule");
	dispatch_table.CreateGraphicsPipelines = (PFN_vkCreateGraphicsPipelines)gdpa(device, "vkCreateGraphicsPipelines");
	dispatch_table.DestroyPipeline = (PFN_vkDestroyPipeline)gdpa(device, "vkDestroyPipeline");
	dispatch_table.CreatePipelineLayout = (PFN_vkCreatePipelineLayout)gdpa(device, "vkCreatePipelineLayout");
	dispatch_table.DestroyPipelineLayout = (PFN_vkDestroyPipelineLayout)gdpa(device, "vkDestroyPipelineLayout");
	dispatch_table.CreateSampler = (PFN_vkCreateSampler)gdpa(device, "vkCreateSampler");
	dispatch_table.DestroySampler = (PFN_vkDestroySampler)gdpa(device, "vkDestroySampler");
	dispatch_table.CreateDescriptorSetLayout = (PFN_vkCreateDescriptorSetLayout)gdpa(device, "vkCreateDescriptorSetLayout");
	dispatch_table.DestroyDescriptorSetLayout = (PFN_vkDestroyDescriptorSetLayout)gdpa(device, "vkDestroyDescriptorSetLayout");
	dispatch_table.CreateDescriptorPool = (PFN_vkCreateDescriptorPool)gdpa(device, "vkCreateDescriptorPool");
	dispatch_table.DestroyDescriptorPool = (PFN_vkDestroyDescriptorPool)gdpa(device, "vkDestroyDescriptorPool");
	dispatch_table.ResetDescriptorPool = (PFN_vkResetDescriptorPool)gdpa(device, "vkResetDescriptorPool");
	dispatch_table.AllocateDescriptorSets = (PFN_vkAllocateDescriptorSets)gdpa(device, "vkAllocateDescriptorSets");
	dispatch_table.FreeDescriptorSets = (PFN_vkFreeDescriptorSets)gdpa(device, "vkFreeDescriptorSets");
	dispatch_table.UpdateDescriptorSets = (PFN_vkUpdateDescriptorSets)gdpa(device, "vkUpdateDescriptorSets");
	dispatch_table.CreateFramebuffer = (PFN_vkCreateFramebuffer)gdpa(device, "vkCreateFramebuffer");
	dispatch_table.DestroyFramebuffer = (PFN_vkDestroyFramebuffer)gdpa(device, "vkDestroyFramebuffer");
	dispatch_table.CreateRenderPass = (PFN_vkCreateRenderPass)gdpa(device, "vkCreateRenderPass");
	dispatch_table.DestroyRenderPass = (PFN_vkDestroyRenderPass)gdpa(device, "vkDestroyRenderPass");
	dispatch_table.CreateCommandPool = (PFN_vkCreateCommandPool)gdpa(device, "vkCreateCommandPool");
	dispatch_table.DestroyCommandPool = (PFN_vkDestroyCommandPool)gdpa(device, "vkDestroyCommandPool");
	dispatch_table.ResetCommandPool = (PFN_vkResetCommandPool)gdpa(device, "vkResetCommandPool");
	dispatch_table.AllocateCommandBuffers = (PFN_vkAllocateCommandBuffers)gdpa(device, "vkAllocateCommandBuffers");
	dispatch_table.FreeCommandBuffers = (PFN_vkFreeCommandBuffers)gdpa(device, "vkFreeCommandBuffers");
	dispatch_table.BeginCommandBuffer = (PFN_vkBeginCommandBuffer)gdpa(device, "vkBeginCommandBuffer");
	dispatch_table.EndCommandBuffer = (PFN_vkEndCommandBuffer)gdpa(device, "vkEndCommandBuffer");
	dispatch_table.ResetCommandBuffer = (PFN_vkResetCommandBuffer)gdpa(device, "vkResetCommandBuffer");
	dispatch_table.CmdBindPipeline = (PFN_vkCmdBindPipeline)gdpa(device, "vkCmdBindPipeline");
	dispatch_table.CmdSetViewport = (PFN_vkCmdSetViewport)gdpa(device, "vkCmdSetViewport");
	dispatch_table.CmdSetScissor = (PFN_vkCmdSetScissor)gdpa(device, "vkCmdSetScissor");
	dispatch_table.CmdBindDescriptorSets = (PFN_vkCmdBindDescriptorSets)gdpa(device, "vkCmdBindDescriptorSets");
	dispatch_table.CmdBindIndexBuffer = (PFN_vkCmdBindIndexBuffer)gdpa(device, "vkCmdBindIndexBuffer");
	dispatch_table.CmdBindVertexBuffers = (PFN_vkCmdBindVertexBuffers)gdpa(device, "vkCmdBindVertexBuffers");
	dispatch_table.CmdDraw = (PFN_vkCmdDraw)gdpa(device, "vkCmdDraw");
	dispatch_table.CmdDrawIndexed = (PFN_vkCmdDrawIndexed)gdpa(device, "vkCmdDrawIndexed");
	dispatch_table.CmdCopyImage = (PFN_vkCmdCopyImage)gdpa(device, "vkCmdCopyImage");
	dispatch_table.CmdBlitImage = (PFN_vkCmdBlitImage)gdpa(device, "vkCmdBlitImage");
	dispatch_table.CmdCopyBufferToImage = (PFN_vkCmdCopyBufferToImage)gdpa(device, "vkCmdCopyBufferToImage");
	dispatch_table.CmdCopyImageToBuffer = (PFN_vkCmdCopyImageToBuffer)gdpa(device, "vkCmdCopyImageToBuffer");
	dispatch_table.CmdUpdateBuffer = (PFN_vkCmdUpdateBuffer)gdpa(device, "vkCmdUpdateBuffer");
	dispatch_table.CmdClearDepthStencilImage = (PFN_vkCmdClearDepthStencilImage)gdpa(device, "vkCmdClearDepthStencilImage");
	dispatch_table.CmdClearAttachments = (PFN_vkCmdClearAttachments)gdpa(device, "vkCmdClearAttachments");
	dispatch_table.CmdPipelineBarrier = (PFN_vkCmdPipelineBarrier)gdpa(device, "vkCmdPipelineBarrier");
	dispatch_table.CmdResetQueryPool = (PFN_vkCmdResetQueryPool)gdpa(device, "vkCmdResetQueryPool");
	dispatch_table.CmdWriteTimestamp = (PFN_vkCmdWriteTimestamp)gdpa(device, "vkCmdWriteTimestamp");
	dispatch_table.CmdPushConstants = (PFN_vkCmdPushConstants)gdpa(device, "vkCmdPushConstants");
	dispatch_table.CmdBeginRenderPass = (PFN_vkCmdBeginRenderPass)gdpa(device, "vkCmdBeginRenderPass");
	dispatch_table.CmdEndRenderPass = (PFN_vkCmdEndRenderPass)gdpa(device, "vkCmdEndRenderPass");
	dispatch_table.CmdExecuteCommands = (PFN_vkCmdExecuteCommands)gdpa(device, "vkCmdExecuteCommands");
	// ---- Core 1_1 commands
	dispatch_table.BindBufferMemory2 = (PFN_vkBindBufferMemory2)gdpa(device, "vkBindBufferMemory2");
	dispatch_table.GetDeviceQueue2 = (PFN_vkGetDeviceQueue2)gdpa(device, "vkGetDeviceQueue2");
	// ---- VK_KHR_swapchain extension commands
	dispatch_table.CreateSwapchainKHR = (PFN_vkCreateSwapchainKHR)gdpa(device, "vkCreateSwapchainKHR");
	dispatch_table.DestroySwapchainKHR = (PFN_vkDestroySwapchainKHR)gdpa(device, "vkDestroySwapchainKHR");
	dispatch_table.GetSwapchainImagesKHR = (PFN_vkGetSwapchainImagesKHR)gdpa(device, "vkGetSwapchainImagesKHR");
	dispatch_table.QueuePresentKHR = (PFN_vkQueuePresentKHR)gdpa(device, "vkQueuePresentKHR");
	// ---- VK_KHR_push_descriptor extension commands
	dispatch_table.CmdPushDescriptorSetKHR = (PFN_vkCmdPushDescriptorSetKHR)gdpa(device, "vkCmdPushDescriptorSetKHR");
	// ---- VK_EXT_debug_marker extension commands
	dispatch_table.DebugMarkerSetObjectNameEXT = (PFN_vkDebugMarkerSetObjectNameEXT)gdpa(device, "vkDebugMarkerSetObjectNameEXT");

	// Initialize per-device data (safe to access here since nothing else can use it yet)
	auto &device_data = s_device_data.emplace(dispatch_key_from_handle(device));
	device_data.physical_device = physicalDevice;

#if RESHADE_VERBOSE_LOG
	LOG(INFO) << "> Returning Vulkan device " << device << '.';
#endif
	return VK_SUCCESS;
}
void     VKAPI_CALL vkDestroyDevice(VkDevice device, const VkAllocationCallbacks *pAllocator)
{
	LOG(INFO) << "Redirecting vkDestroyDevice" << '(' << "device = " << device << ", pAllocator = " << pAllocator << ')' << " ...";

	s_device_data.erase(dispatch_key_from_handle(device));
	s_command_buffer_data.clear(); // Reset all command buffer data

	// Get function pointer before removing it next
	GET_DEVICE_DISPATCH_PTR(DestroyDevice, device);
	// Remove device dispatch table since this device is being destroyed
	s_device_dispatch.erase(dispatch_key_from_handle(device));

	trampoline(device, pAllocator);
}

VkResult VKAPI_CALL vkCreateSwapchainKHR(VkDevice device, const VkSwapchainCreateInfoKHR *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkSwapchainKHR *pSwapchain)
{
	LOG(INFO) << "Redirecting vkCreateSwapchainKHR" << '(' << "device = " << device << ", pCreateInfo = " << pCreateInfo << ", pAllocator = " << pAllocator << ", pSwapchain = " << pSwapchain << ')' << " ...";

	assert(pCreateInfo != nullptr && pSwapchain != nullptr);

	// Add required usage flags to create info
	VkSwapchainCreateInfoKHR create_info = *pCreateInfo;
	create_info.imageUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

	LOG(INFO) << "> Dumping swap chain description:";
	LOG(INFO) << "  +-----------------------------------------+-----------------------------------------+";
	LOG(INFO) << "  | Parameter                               | Value                                   |";
	LOG(INFO) << "  +-----------------------------------------+-----------------------------------------+";
	LOG(INFO) << "  | flags                                   | " << std::setw(39) << std::hex << create_info.flags << std::dec << " |";
	LOG(INFO) << "  | surface                                 | " << std::setw(39) << create_info.surface << " |";
	LOG(INFO) << "  | minImageCount                           | " << std::setw(39) << create_info.minImageCount << " |";
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

	GET_DEVICE_DISPATCH_PTR(CreateSwapchainKHR, device);
	const VkResult result = trampoline(device, &create_info, pAllocator, pSwapchain);
	if (result != VK_SUCCESS)
	{
		LOG(WARN) << "vkCreateSwapchainKHR failed with error code " << result << '!';
		return result;
	}

	auto &device_data = s_device_data.at(dispatch_key_from_handle(device));

	std::shared_ptr<reshade::vulkan::runtime_vk> runtime;
	// Remove old swapchain from the list so that a call to 'vkDestroySwapchainKHR' won't reset the runtime again
	if (s_runtimes.erase(pCreateInfo->oldSwapchain, runtime))
	{
		assert(pCreateInfo->oldSwapchain != VK_NULL_HANDLE);

		// Re-use the existing runtime if this swapchain was not created from scratch
		runtime->on_reset(); // But reset it before initializing again below
	}
	else
	{
		runtime = std::make_shared<reshade::vulkan::runtime_vk>(
			device, device_data.physical_device,
			s_instance_dispatch.at(dispatch_key_from_handle(device_data.physical_device)), s_device_dispatch.at(dispatch_key_from_handle(device)));
	}

	// Look up window handle from surface
	const HWND hwnd = s_surface_windows.at(pCreateInfo->surface);

	if (!runtime->on_init(*pSwapchain, *pCreateInfo, hwnd))
		LOG(ERROR) << "Failed to initialize Vulkan runtime environment on runtime " << runtime.get() << '.';

	s_runtimes.emplace(*pSwapchain, runtime);

#if RESHADE_VERBOSE_LOG
	LOG(INFO) << "> Returning Vulkan swapchain " << *pSwapchain << '.';
#endif
	return VK_SUCCESS;
}
void     VKAPI_CALL vkDestroySwapchainKHR(VkDevice device, VkSwapchainKHR swapchain, const VkAllocationCallbacks *pAllocator)
{
	LOG(INFO) << "Redirecting vkDestroySwapchainKHR" << '(' << device << ", " << swapchain << ", " << pAllocator << ')' << " ...";

	// Remove runtime from global list
	if (std::shared_ptr<reshade::vulkan::runtime_vk> runtime;
		s_runtimes.erase(swapchain, runtime))
	{
		runtime->on_reset();
	}

	GET_DEVICE_DISPATCH_PTR(DestroySwapchainKHR, device);
	trampoline(device, swapchain, pAllocator);
}

VkResult VKAPI_CALL vkQueueSubmit(VkQueue queue, uint32_t submitCount, const VkSubmitInfo *pSubmits, VkFence fence)
{
	assert(pSubmits != nullptr);

	auto &device_data = s_device_data.at(dispatch_key_from_handle(queue));

	for (uint32_t i = 0; i < submitCount; ++i)
	{
		for (uint32_t k = 0; k < pSubmits[i].commandBufferCount; ++k)
		{
			VkCommandBuffer cmd = pSubmits->pCommandBuffers[k];
			assert(cmd != VK_NULL_HANDLE);

			auto &command_buffer_data = s_command_buffer_data.at(cmd);

			// Merge command list trackers into device one
			device_data.buffer_detection.merge(command_buffer_data.buffer_detection);
		}
	}

	// The loader uses the same dispatch table pointer for queues and devices, so can use queue to perform lookup here
	GET_DEVICE_DISPATCH_PTR(QueueSubmit, queue);
	return trampoline(queue, submitCount, pSubmits, fence);
}
VkResult VKAPI_CALL vkQueuePresentKHR(VkQueue queue, const VkPresentInfoKHR *pPresentInfo)
{
	assert(pPresentInfo != nullptr);

	auto &device_data = s_device_data.at(dispatch_key_from_handle(queue));

	for (uint32_t i = 0; i < pPresentInfo->swapchainCount; ++i)
	{
		if (const auto runtime = s_runtimes.at(pPresentInfo->pSwapchains[i]);
			runtime != nullptr)
		{
			runtime->on_present(queue, pPresentInfo->pImageIndices[i], device_data.buffer_detection);
		}
	}

	device_data.buffer_detection.reset();

	// TODO: It may be necessary to add a wait semaphore to the present info
	GET_DEVICE_DISPATCH_PTR(QueuePresentKHR, queue);
	return trampoline(queue, pPresentInfo);
}


VkResult VKAPI_CALL vkCreateImage(VkDevice device, const VkImageCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkImage *pImage)
{
	assert(pCreateInfo != nullptr && pImage != nullptr);

	VkImageCreateInfo create_info = *pCreateInfo;
	// Allow shader access to images that are used as depth stencil attachments
	const bool is_depth_stencil_image = create_info.usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	if (is_depth_stencil_image)
		create_info.usage |= VK_IMAGE_USAGE_SAMPLED_BIT;

	GET_DEVICE_DISPATCH_PTR(CreateImage, device);
	const VkResult result = trampoline(device, &create_info, pAllocator, pImage);
	if (result != VK_SUCCESS)
	{
		LOG(WARN) << "vkCreateImage failed with error code " << result << '!';
		return result;
	}

	// Keep track of image information (only care about depth stencil images currently)
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
		LOG(WARN) << "vkCreateImageView failed with error code " << result << '!';
		return result;
	}

	// Keep track of image view information (only care about depth stencil image views currently)
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
		LOG(WARN) << "vkCreateRenderPass failed with error code " << result << '!';
		return result;
	}

	auto &renderpass_data = s_renderpass_data.emplace(*pRenderPass);

	// Search for the first pass using a depth stencil attachment
	for (uint32_t subpass = 0; subpass < pCreateInfo->subpassCount; ++subpass)
	{
		auto &subpass_data = renderpass_data.emplace_back();

		const VkAttachmentReference *const ds_reference = pCreateInfo->pSubpasses[subpass].pDepthStencilAttachment;
		if (ds_reference != nullptr && ds_reference->attachment != VK_ATTACHMENT_UNUSED)
		{
			const VkAttachmentDescription &ds_attachment = pCreateInfo->pAttachments[ds_reference->attachment];
			subpass_data.initial_depthstencil_layout = ds_attachment.initialLayout;
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
		LOG(WARN) << "vkCreateFramebuffer failed with error code " << result << '!';
		return result;
	}

	// Look up the depth stencil images associated with their image views
	std::vector<VkImage> attachment_images(pCreateInfo->attachmentCount);
	for (const auto &subpass_data : s_renderpass_data.at(pCreateInfo->renderPass))
		if (subpass_data.depthstencil_attachment_index < pCreateInfo->attachmentCount)
			attachment_images[subpass_data.depthstencil_attachment_index] = s_image_view_mapping.at(
				pCreateInfo->pAttachments[subpass_data.depthstencil_attachment_index]);

	// Keep track of depth stencil image in this frame buffer
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
		LOG(WARN) << "vkAllocateCommandBuffers failed with error code " << result << '!';
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
	data.buffer_detection.reset();

	GET_DEVICE_DISPATCH_PTR(BeginCommandBuffer, commandBuffer);
	return trampoline(commandBuffer, pBeginInfo);
}

void     VKAPI_CALL vkCmdBeginRenderPass(VkCommandBuffer commandBuffer, const VkRenderPassBeginInfo *pRenderPassBegin, VkSubpassContents contents)
{
#if RESHADE_VULKAN_CAPTURE_DEPTH_BUFFERS
	auto &data = s_command_buffer_data.at(commandBuffer);
	data.current_subpass = 0;
	data.current_renderpass = pRenderPassBegin->renderPass;
	data.current_framebuffer = pRenderPassBegin->framebuffer;

	const auto &renderpass_data = s_renderpass_data.at(data.current_renderpass)[data.current_subpass];
	const auto &framebuffer_data = s_framebuffer_data.at(data.current_framebuffer);
	if (renderpass_data.depthstencil_attachment_index < framebuffer_data.size())
	{
		const VkImage depthstencil = framebuffer_data[renderpass_data.depthstencil_attachment_index];
		VkImageLayout depthstencil_layout = renderpass_data.initial_depthstencil_layout;

		// TODO: Technically the image layout stored here is not the one the image ends up in, but the likelihood is high
		if (VK_IMAGE_LAYOUT_UNDEFINED == depthstencil_layout) // Do some trickery to make sure it is valid
			depthstencil_layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		data.buffer_detection.on_set_depthstencil(
			depthstencil,
			depthstencil_layout,
			s_image_data.at(depthstencil));
	}
	else
	{
		data.buffer_detection.on_set_depthstencil(VK_NULL_HANDLE, VK_IMAGE_LAYOUT_UNDEFINED, {});
	}
#endif

	GET_DEVICE_DISPATCH_PTR(CmdBeginRenderPass, commandBuffer);
	trampoline(commandBuffer, pRenderPassBegin, contents);
}
void     VKAPI_CALL vkCmdNextSubpass(VkCommandBuffer commandBuffer, VkSubpassContents contents)
{
	GET_DEVICE_DISPATCH_PTR(CmdNextSubpass, commandBuffer);
	trampoline(commandBuffer, contents);

#if RESHADE_VULKAN_CAPTURE_DEPTH_BUFFERS
	auto &data = s_command_buffer_data.at(commandBuffer);
	data.current_subpass++;
	assert(data.current_renderpass != VK_NULL_HANDLE);
	assert(data.current_framebuffer != VK_NULL_HANDLE);

	const auto &renderpass_data = s_renderpass_data.at(data.current_renderpass)[data.current_subpass];
	const auto &framebuffer_data = s_framebuffer_data.at(data.current_framebuffer);
	if (renderpass_data.depthstencil_attachment_index < framebuffer_data.size())
	{
		const VkImage depthstencil = framebuffer_data[renderpass_data.depthstencil_attachment_index];
		VkImageLayout depthstencil_layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		data.buffer_detection.on_set_depthstencil(
			depthstencil,
			depthstencil_layout,
			s_image_data.at(depthstencil));
	}
	else
	{
		data.buffer_detection.on_set_depthstencil(VK_NULL_HANDLE, VK_IMAGE_LAYOUT_UNDEFINED, {});
	}
#endif
}
void     VKAPI_CALL vkCmdEndRenderPass(VkCommandBuffer commandBuffer)
{
	GET_DEVICE_DISPATCH_PTR(CmdEndRenderPass, commandBuffer);
	trampoline(commandBuffer);

#if RESHADE_VULKAN_CAPTURE_DEPTH_BUFFERS
	auto &data = s_command_buffer_data.at(commandBuffer);
	data.current_subpass = std::numeric_limits<uint32_t>::max();
	data.current_renderpass = VK_NULL_HANDLE;
	data.current_framebuffer = VK_NULL_HANDLE;

	data.buffer_detection.on_set_depthstencil(VK_NULL_HANDLE, VK_IMAGE_LAYOUT_UNDEFINED, {});
#endif
}

void     VKAPI_CALL vkCmdExecuteCommands(VkCommandBuffer commandBuffer, uint32_t commandBufferCount, const VkCommandBuffer *pCommandBuffers)
{
	auto &data = s_command_buffer_data.at(commandBuffer);

	for (uint32_t i = 0; i < commandBufferCount; ++i)
	{
		const auto &secondary_data = s_command_buffer_data.at(pCommandBuffers[i]);

		// Merge secondary command list trackers into the current primary one
		data.buffer_detection.merge(secondary_data.buffer_detection);
	}

	GET_DEVICE_DISPATCH_PTR(CmdExecuteCommands, commandBuffer);
	trampoline(commandBuffer, commandBufferCount, pCommandBuffers);
}

void     VKAPI_CALL vkCmdDraw(VkCommandBuffer commandBuffer, uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance)
{
	GET_DEVICE_DISPATCH_PTR(CmdDraw, commandBuffer);
	trampoline(commandBuffer, vertexCount, instanceCount, firstVertex, firstInstance);

	auto &data = s_command_buffer_data.at(commandBuffer);
	data.buffer_detection.on_draw(vertexCount * instanceCount);
}
void     VKAPI_CALL vkCmdDrawIndexed(VkCommandBuffer commandBuffer, uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance)
{
	GET_DEVICE_DISPATCH_PTR(CmdDrawIndexed, commandBuffer);
	trampoline(commandBuffer, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);

	auto &data = s_command_buffer_data.at(commandBuffer);
	data.buffer_detection.on_draw(indexCount * instanceCount);
}


VK_LAYER_EXPORT PFN_vkVoidFunction VKAPI_CALL vkGetDeviceProcAddr(VkDevice device, const char *pName)
{
	if (0 == strcmp(pName, "vkCreateSwapchainKHR"))
		return reinterpret_cast<PFN_vkVoidFunction>(vkCreateSwapchainKHR);
	if (0 == strcmp(pName, "vkDestroySwapchainKHR"))
		return reinterpret_cast<PFN_vkVoidFunction>(vkDestroySwapchainKHR);
	if (0 == strcmp(pName, "vkQueueSubmit"))
		return reinterpret_cast<PFN_vkVoidFunction>(vkQueueSubmit);
	if (0 == strcmp(pName, "vkQueuePresentKHR"))
		return reinterpret_cast<PFN_vkVoidFunction>(vkQueuePresentKHR);

	if (0 == strcmp(pName, "vkCreateImage"))
		return reinterpret_cast<PFN_vkVoidFunction>(vkCreateImage);
	if (0 == strcmp(pName, "vkDestroyImage"))
		return reinterpret_cast<PFN_vkVoidFunction>(vkDestroyImage);
	if (0 == strcmp(pName, "vkCreateImageView"))
		return reinterpret_cast<PFN_vkVoidFunction>(vkCreateImageView);
	if (0 == strcmp(pName, "vkDestroyImageView"))
		return reinterpret_cast<PFN_vkVoidFunction>(vkDestroyImageView);
	if (0 == strcmp(pName, "vkCreateRenderPass"))
		return reinterpret_cast<PFN_vkVoidFunction>(vkCreateRenderPass);
	if (0 == strcmp(pName, "vkDestroyRenderPass"))
		return reinterpret_cast<PFN_vkVoidFunction>(vkDestroyRenderPass);
	if (0 == strcmp(pName, "vkCreateFramebuffer"))
		return reinterpret_cast<PFN_vkVoidFunction>(vkCreateFramebuffer);
	if (0 == strcmp(pName, "vkDestroyFramebuffer"))
		return reinterpret_cast<PFN_vkVoidFunction>(vkDestroyFramebuffer);

	if (0 == strcmp(pName, "vkAllocateCommandBuffers"))
		return reinterpret_cast<PFN_vkVoidFunction>(vkAllocateCommandBuffers);
	if (0 == strcmp(pName, "vkFreeCommandBuffers"))
		return reinterpret_cast<PFN_vkVoidFunction>(vkFreeCommandBuffers);
	if (0 == strcmp(pName, "vkBeginCommandBuffer"))
		return reinterpret_cast<PFN_vkVoidFunction>(vkBeginCommandBuffer);

	if (0 == strcmp(pName, "vkCmdBeginRenderPass"))
		return reinterpret_cast<PFN_vkVoidFunction>(vkCmdBeginRenderPass);
	if (0 == strcmp(pName, "vkCmdNextSubpass"))
		return reinterpret_cast<PFN_vkVoidFunction>(vkCmdNextSubpass);
	if (0 == strcmp(pName, "vkCmdEndRenderPass"))
		return reinterpret_cast<PFN_vkVoidFunction>(vkCmdEndRenderPass);
	if (0 == strcmp(pName, "vkCmdExecuteCommands"))
		return reinterpret_cast<PFN_vkVoidFunction>(vkCmdExecuteCommands);
	if (0 == strcmp(pName, "vkCmdDraw"))
		return reinterpret_cast<PFN_vkVoidFunction>(vkCmdDraw);
	if (0 == strcmp(pName, "vkCmdDrawIndexed"))
		return reinterpret_cast<PFN_vkVoidFunction>(vkCmdDrawIndexed);

	// Need to self-intercept as well, since some layers rely on this (e.g. Steam overlay)
	// See also https://github.com/KhronosGroup/Vulkan-Loader/blob/master/loader/LoaderAndLayerInterface.md#layer-conventions-and-rules
	if (0 == strcmp(pName, "vkGetDeviceProcAddr"))
		return reinterpret_cast<PFN_vkVoidFunction>(vkGetDeviceProcAddr);

	if (device == VK_NULL_HANDLE)
		return nullptr;

	GET_DEVICE_DISPATCH_PTR(GetDeviceProcAddr, device);
	return trampoline(device, pName);
}
VK_LAYER_EXPORT PFN_vkVoidFunction VKAPI_CALL vkGetInstanceProcAddr(VkInstance instance, const char *pName)
{
	if (0 == strcmp(pName, "vkCreateInstance"))
		return reinterpret_cast<PFN_vkVoidFunction>(vkCreateInstance);
	if (0 == strcmp(pName, "vkDestroyInstance"))
		return reinterpret_cast<PFN_vkVoidFunction>(vkDestroyInstance);
	if (0 == strcmp(pName, "vkCreateDevice"))
		return reinterpret_cast<PFN_vkVoidFunction>(vkCreateDevice);
	if (0 == strcmp(pName, "vkDestroyDevice"))
		return reinterpret_cast<PFN_vkVoidFunction>(vkDestroyDevice);
	if (0 == strcmp(pName, "vkCreateWin32SurfaceKHR"))
		return reinterpret_cast<PFN_vkVoidFunction>(vkCreateWin32SurfaceKHR);
	if (0 == strcmp(pName, "vkDestroySurfaceKHR"))
		return reinterpret_cast<PFN_vkVoidFunction>(vkDestroySurfaceKHR);

	// Self-intercept here as well to stay consistent with 'vkGetDeviceProcAddr' implementation
	if (0 == strcmp(pName, "vkGetInstanceProcAddr"))
		return reinterpret_cast<PFN_vkVoidFunction>(vkGetInstanceProcAddr);

	if (instance == VK_NULL_HANDLE)
		return nullptr;

	GET_INSTANCE_DISPATCH_PTR(GetInstanceProcAddr, instance);
	return trampoline(instance, pName);
}
