/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "log.hpp"
#include "hook_manager.hpp"
#include "runtime_vk.hpp"
#include "vk_layer.h"
#include "vk_layer_dispatch_table.h"
#include <memory>
#include <unordered_map>

struct vk_layer_device_data
{
	VkInstance instance;
	VkPhysicalDevice physical_device;
	VkLayerDispatchTable dispatch_table;
};
struct vk_layer_instance_data
{
	VkLayerInstanceDispatchTable dispatch_table;
};

static std::unordered_map<void *, vk_layer_device_data> s_device_data;
static std::unordered_map<void *, vk_layer_instance_data> s_instance_data;
static std::unordered_map<VkSurfaceKHR, HWND> s_surface_windows;
static std::unordered_map<VkPhysicalDevice, VkInstance> s_instance_mapping;
static std::unordered_map<VkSwapchainKHR, std::shared_ptr<reshade::vulkan::runtime_vk>> s_runtimes;

inline void *get_dispatch_key(const void *dispatchable_handle)
{
	return *(void **)dispatchable_handle;
}

VkResult VKAPI_CALL vkCreateInstance(const VkInstanceCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkInstance *pInstance)
{
	PFN_vkCreateInstance create_func;
	PFN_vkGetInstanceProcAddr get_instance_proc;

	LOG(INFO) << "Redirecting vkCreateInstance" << '(' << pCreateInfo << ", " << pAllocator << ", " << pInstance << ')' << " ...";

	// Look for layer link info if installed as a layer (provided by the Vulkan loader)
	VkLayerInstanceCreateInfo *link_info = (VkLayerInstanceCreateInfo *)pCreateInfo->pNext;
	while (link_info != nullptr && !(link_info->sType == VK_STRUCTURE_TYPE_LOADER_INSTANCE_CREATE_INFO && link_info->function == VK_LAYER_LINK_INFO))
		link_info = (VkLayerInstanceCreateInfo *)link_info->pNext;

	if (link_info != nullptr)
	{
		assert(link_info->u.pLayerInfo != nullptr);
		// Look up functions in layer info
		get_instance_proc = link_info->u.pLayerInfo->pfnNextGetInstanceProcAddr;
		create_func = (PFN_vkCreateInstance)get_instance_proc(nullptr, "vkCreateInstance");

		// Advance the link info for the next element of the chain
		link_info->u.pLayerInfo = link_info->u.pLayerInfo->pNext;
	}
	else
	{
		create_func = reshade::hooks::call(vkCreateInstance);
		get_instance_proc = reshade::hooks::call(vkGetInstanceProcAddr);
	}

	if (create_func == nullptr) // Unable to resolve next 'vkCreateInstance' function in the call chain
		return VK_ERROR_INITIALIZATION_FAILED;

	// Continue call down the chain
	const VkResult result = create_func(pCreateInfo, pAllocator, pInstance);
	if (result != VK_SUCCESS)
	{
		LOG(WARN) << "> vkCreateInstance failed with error code " << result << '!';
		return result;
	}

	VkInstance instance = *pInstance;
	vk_layer_instance_data &instance_data = s_instance_data[get_dispatch_key(instance)];

	// Initialize the instance dispatch table
	VkLayerInstanceDispatchTable &dispatch_table = instance_data.dispatch_table;
	dispatch_table.GetInstanceProcAddr = get_instance_proc;

	dispatch_table.DestroyInstance = (PFN_vkDestroyInstance)get_instance_proc(instance, "vkDestroyInstance");
	dispatch_table.EnumeratePhysicalDevices = (PFN_vkEnumeratePhysicalDevices)get_instance_proc(instance, "vkEnumeratePhysicalDevices");
	dispatch_table.CreateWin32SurfaceKHR = (PFN_vkCreateWin32SurfaceKHR)get_instance_proc(instance, "vkCreateWin32SurfaceKHR");
	dispatch_table.DestroySurfaceKHR = (PFN_vkDestroySurfaceKHR)get_instance_proc(instance, "vkDestroySurfaceKHR");
	dispatch_table.GetPhysicalDeviceMemoryProperties = (PFN_vkGetPhysicalDeviceMemoryProperties)get_instance_proc(instance, "vkGetPhysicalDeviceMemoryProperties");

	return VK_SUCCESS;
}
void     VKAPI_CALL vkDestroyInstance(VkInstance instance, const VkAllocationCallbacks *pAllocator)
{
	LOG(INFO) << "Redirecting vkDestroyInstance" << '(' << instance << ", " << pAllocator << ')' << " ...";

	const vk_layer_instance_data &instance_data = s_instance_data.at(get_dispatch_key(instance));

	// Continue call down the chain
	assert(instance_data.dispatch_table.DestroyInstance != nullptr);
	instance_data.dispatch_table.DestroyInstance(instance, pAllocator);

	s_instance_data.erase(get_dispatch_key(instance));
}

VkResult VKAPI_CALL vkEnumeratePhysicalDevices(VkInstance instance, uint32_t *pPhysicalDeviceCount, VkPhysicalDevice *pPhysicalDevices)
{
	LOG(INFO) << "Redirecting vkEnumeratePhysicalDevices" << '(' << instance << ", " << pPhysicalDeviceCount << ", " << pPhysicalDevices << ')' << " ...";

	const vk_layer_instance_data &instance_data = s_instance_data.at(get_dispatch_key(instance));

	// Continue call down the chain
	assert(instance_data.dispatch_table.EnumeratePhysicalDevices != nullptr);
	const VkResult result = instance_data.dispatch_table.EnumeratePhysicalDevices(instance, pPhysicalDeviceCount, pPhysicalDevices);
	if (result != VK_SUCCESS && result != VK_INCOMPLETE)
	{
		LOG(WARN) << "> vkEnumeratePhysicalDevices failed with error code " << result << '!';
		return result;
	}

	if (pPhysicalDevices != nullptr)
		for (uint32_t i = 0; i < *pPhysicalDeviceCount; ++i)
			s_instance_mapping[pPhysicalDevices[i]] = instance;

	return result;
}

VkResult VKAPI_CALL vkCreateWin32SurfaceKHR(VkInstance instance, const VkWin32SurfaceCreateInfoKHR *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkSurfaceKHR *pSurface)
{
	LOG(INFO) << "Redirecting vkCreateWin32SurfaceKHR" << '(' << instance << ", " << pCreateInfo << ", " << pAllocator << ", " << pSurface << ')' << " ...";

	const vk_layer_instance_data &instance_data = s_instance_data.at(get_dispatch_key(instance));

	// Continue call down the chain
	assert(instance_data.dispatch_table.CreateWin32SurfaceKHR != nullptr);
	const VkResult result = instance_data.dispatch_table.CreateWin32SurfaceKHR(instance, pCreateInfo, pAllocator, pSurface);
	if (result != VK_SUCCESS)
	{
		LOG(WARN) << "> vkCreateWin32SurfaceKHR failed with error code " << result << '!';
		return result;
	}

	s_surface_windows[*pSurface] = pCreateInfo->hwnd;

	return VK_SUCCESS;
}
void     VKAPI_CALL vkDestroySurfaceKHR(VkInstance instance, VkSurfaceKHR surface, const VkAllocationCallbacks *pAllocator)
{
	LOG(INFO) << "Redirecting vkDestroySurfaceKHR" << '(' << instance << ", " << surface << ", " << pAllocator << ')' << " ...";

	const vk_layer_instance_data &instance_data = s_instance_data.at(get_dispatch_key(instance));

	s_surface_windows.erase(surface);

	// Continue call down the chain
	assert(instance_data.dispatch_table.DestroySurfaceKHR != nullptr);
	instance_data.dispatch_table.DestroySurfaceKHR(instance, surface, pAllocator);
}

VkResult VKAPI_CALL vkCreateDevice(VkPhysicalDevice physicalDevice, const VkDeviceCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkDevice *pDevice)
{
	PFN_vkCreateDevice create_func;
	PFN_vkGetDeviceProcAddr get_device_proc;
	PFN_vkGetInstanceProcAddr get_instance_proc;

	LOG(INFO) << "Redirecting vkCreateDevice" << '(' << physicalDevice << ", " << pCreateInfo << ", " << pAllocator << ", " << pDevice << ')' << " ...";

	// Look for layer link info if installed as a layer (provided by the Vulkan loader)
	VkLayerDeviceCreateInfo *link_info = (VkLayerDeviceCreateInfo *)pCreateInfo->pNext;
	while (link_info != nullptr && !(link_info->sType == VK_STRUCTURE_TYPE_LOADER_DEVICE_CREATE_INFO && link_info->function == VK_LAYER_LINK_INFO))
		link_info = (VkLayerDeviceCreateInfo *)link_info->pNext;

	if (link_info != nullptr)
	{
		assert(link_info->u.pLayerInfo != nullptr);
		// Look up functions in layer info
		get_device_proc = link_info->u.pLayerInfo->pfnNextGetDeviceProcAddr;
		get_instance_proc = link_info->u.pLayerInfo->pfnNextGetInstanceProcAddr;
		create_func = (PFN_vkCreateDevice)get_instance_proc(nullptr, "vkCreateDevice");

		// Advance the link info for the next element on the chain
		link_info->u.pLayerInfo = link_info->u.pLayerInfo->pNext;
	}
	else
	{
		get_device_proc = reshade::hooks::call(vkGetDeviceProcAddr);
		get_instance_proc = reshade::hooks::call(vkGetInstanceProcAddr);
		create_func = reshade::hooks::call(vkCreateDevice);
	}

	if (create_func == nullptr) // Unable to resolve next 'vkCreateDevice' function in the call chain
		return VK_ERROR_INITIALIZATION_FAILED;

	std::vector<const char *> enabled_extensions;
	enabled_extensions.reserve(pCreateInfo->enabledExtensionCount);
	for (uint32_t i = 0; i < pCreateInfo->enabledExtensionCount; ++i)
		enabled_extensions.push_back(pCreateInfo->ppEnabledExtensionNames[i]);

	// Add extensions that ReShade requires
	enabled_extensions.push_back(VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME);

	VkDeviceCreateInfo create_info = *pCreateInfo;
	create_info.enabledExtensionCount = uint32_t(enabled_extensions.size());
	create_info.ppEnabledExtensionNames = enabled_extensions.data();

	// Continue call down the chain
	const VkResult result = create_func(physicalDevice, &create_info, pAllocator, pDevice);
	if (result != VK_SUCCESS)
	{
		LOG(WARN) << "> vkCreateDevice failed with error code " << result << '!';
		return result;
	}

	VkDevice device = *pDevice;
	vk_layer_device_data &device_data = s_device_data[get_dispatch_key(device)];
	device_data.instance = s_instance_mapping.at(physicalDevice);
	device_data.physical_device = physicalDevice;

	// Initialize the device dispatch table
	VkLayerDispatchTable &dispatch_table = device_data.dispatch_table;
	dispatch_table.GetDeviceProcAddr = get_device_proc;

	dispatch_table.DestroyDevice = (PFN_vkDestroyDevice)get_device_proc(device, "vkDestroyDevice");
	dispatch_table.GetDeviceQueue = (PFN_vkGetDeviceQueue)get_device_proc(device, "vkGetDeviceQueue");
	dispatch_table.GetDeviceQueue2 = (PFN_vkGetDeviceQueue2)get_device_proc(device, "vkGetDeviceQueue2");
	dispatch_table.CreateSwapchainKHR = (PFN_vkCreateSwapchainKHR)get_device_proc(device, "vkCreateSwapchainKHR");
	dispatch_table.DestroySwapchainKHR = (PFN_vkDestroySwapchainKHR)get_device_proc(device, "vkDestroySwapchainKHR");
	dispatch_table.QueuePresentKHR = (PFN_vkQueuePresentKHR)get_device_proc(device, "vkQueuePresentKHR");

	dispatch_table.QueueWaitIdle = (PFN_vkQueueWaitIdle)get_device_proc(device, "vkQueueWaitIdle");
	dispatch_table.DeviceWaitIdle = (PFN_vkDeviceWaitIdle)get_device_proc(device, "vkDeviceWaitIdle");
	dispatch_table.GetDeviceQueue = (PFN_vkGetDeviceQueue)get_device_proc(device, "vkGetDeviceQueue");
	dispatch_table.GetSwapchainImagesKHR = (PFN_vkGetSwapchainImagesKHR)get_device_proc(device, "vkGetSwapchainImagesKHR");
	dispatch_table.QueueSubmit = (PFN_vkQueueSubmit)get_device_proc(device, "vkQueueSubmit");
	dispatch_table.CreateFence = (PFN_vkCreateFence)get_device_proc(device, "vkCreateFence");
	dispatch_table.DestroyFence = (PFN_vkDestroyFence)get_device_proc(device, "vkDestroyFence");
	dispatch_table.WaitForFences = (PFN_vkWaitForFences)get_device_proc(device, "vkWaitForFences");
	dispatch_table.ResetFences = (PFN_vkResetFences)get_device_proc(device, "vkResetFences");
	dispatch_table.MapMemory = (PFN_vkMapMemory)get_device_proc(device, "vkMapMemory");
	dispatch_table.FreeMemory = (PFN_vkFreeMemory)get_device_proc(device, "vkFreeMemory");
	dispatch_table.UnmapMemory = (PFN_vkUnmapMemory)get_device_proc(device, "vkUnmapMemory");
	dispatch_table.AllocateMemory = (PFN_vkAllocateMemory)get_device_proc(device, "vkAllocateMemory");
	dispatch_table.CreateImage = (PFN_vkCreateImage)get_device_proc(device, "vkCreateImage");
	dispatch_table.DestroyImage = (PFN_vkDestroyImage)get_device_proc(device, "vkDestroyImage");
	dispatch_table.GetImageSubresourceLayout = (PFN_vkGetImageSubresourceLayout)get_device_proc(device, "vkGetImageSubresourceLayout");
	dispatch_table.GetImageMemoryRequirements = (PFN_vkGetImageMemoryRequirements)get_device_proc(device, "vkGetImageMemoryRequirements");
	dispatch_table.BindImageMemory = (PFN_vkBindImageMemory)get_device_proc(device, "vkBindImageMemory");
	dispatch_table.CreateBuffer = (PFN_vkCreateBuffer)get_device_proc(device, "vkCreateBuffer");
	dispatch_table.DestroyBuffer = (PFN_vkDestroyBuffer)get_device_proc(device, "vkDestroyBuffer");
	dispatch_table.GetBufferMemoryRequirements = (PFN_vkGetBufferMemoryRequirements)get_device_proc(device, "vkGetBufferMemoryRequirements");
	dispatch_table.BindBufferMemory = (PFN_vkBindBufferMemory)get_device_proc(device, "vkBindBufferMemory");
	dispatch_table.BindBufferMemory2 = (PFN_vkBindBufferMemory2)get_device_proc(device, "vkBindBufferMemory2");
	dispatch_table.CreateImageView = (PFN_vkCreateImageView)get_device_proc(device, "vkCreateImageView");
	dispatch_table.DestroyImageView = (PFN_vkDestroyImageView)get_device_proc(device, "vkDestroyImageView");
	dispatch_table.CreateRenderPass = (PFN_vkCreateRenderPass)get_device_proc(device, "vkCreateRenderPass");
	dispatch_table.DestroyRenderPass = (PFN_vkDestroyRenderPass)get_device_proc(device, "vkDestroyRenderPass");
	dispatch_table.CreateFramebuffer = (PFN_vkCreateFramebuffer)get_device_proc(device, "vkCreateFramebuffer");
	dispatch_table.DestroyFramebuffer = (PFN_vkDestroyFramebuffer)get_device_proc(device, "vkDestroyFramebuffer");
	dispatch_table.ResetCommandPool = (PFN_vkResetCommandPool)get_device_proc(device, "vkResetCommandPool");
	dispatch_table.CreateCommandPool = (PFN_vkCreateCommandPool)get_device_proc(device, "vkCreateCommandPool");
	dispatch_table.DestroyCommandPool = (PFN_vkDestroyCommandPool)get_device_proc(device, "vkDestroyCommandPool");
	dispatch_table.AllocateCommandBuffers = (PFN_vkAllocateCommandBuffers)get_device_proc(device, "vkAllocateCommandBuffers");
	dispatch_table.EndCommandBuffer = (PFN_vkEndCommandBuffer)get_device_proc(device, "vkEndCommandBuffer");
	dispatch_table.BeginCommandBuffer = (PFN_vkBeginCommandBuffer)get_device_proc(device, "vkBeginCommandBuffer");
	dispatch_table.ResetDescriptorPool = (PFN_vkResetDescriptorPool)get_device_proc(device, "vkResetDescriptorPool");
	dispatch_table.CreateDescriptorPool = (PFN_vkCreateDescriptorPool)get_device_proc(device, "vkCreateDescriptorPool");
	dispatch_table.DestroyDescriptorPool = (PFN_vkDestroyDescriptorPool)get_device_proc(device, "vkDestroyDescriptorPool");
	dispatch_table.AllocateDescriptorSets = (PFN_vkAllocateDescriptorSets)get_device_proc(device, "vkAllocateDescriptorSets");
	dispatch_table.UpdateDescriptorSets = (PFN_vkUpdateDescriptorSets)get_device_proc(device, "vkUpdateDescriptorSets");
	dispatch_table.CreateDescriptorSetLayout = (PFN_vkCreateDescriptorSetLayout)get_device_proc(device, "vkCreateDescriptorSetLayout");
	dispatch_table.DestroyDescriptorSetLayout = (PFN_vkDestroyDescriptorSetLayout)get_device_proc(device, "vkDestroyDescriptorSetLayout");
	dispatch_table.CreateGraphicsPipelines = (PFN_vkCreateGraphicsPipelines)get_device_proc(device, "vkCreateGraphicsPipelines");
	dispatch_table.DestroyPipeline = (PFN_vkDestroyPipeline)get_device_proc(device, "vkDestroyPipeline");
	dispatch_table.CreatePipelineLayout = (PFN_vkCreatePipelineLayout)get_device_proc(device, "vkCreatePipelineLayout");
	dispatch_table.DestroyPipelineLayout = (PFN_vkDestroyPipelineLayout)get_device_proc(device, "vkDestroyPipelineLayout");
	dispatch_table.CreateSampler = (PFN_vkCreateSampler)get_device_proc(device, "vkCreateSampler");
	dispatch_table.DestroySampler = (PFN_vkDestroySampler)get_device_proc(device, "vkDestroySampler");
	dispatch_table.CreateShaderModule = (PFN_vkCreateShaderModule)get_device_proc(device, "vkCreateShaderModule");
	dispatch_table.DestroyShaderModule = (PFN_vkDestroyShaderModule)get_device_proc(device, "vkDestroyShaderModule");
	dispatch_table.CmdBlitImage = (PFN_vkCmdBlitImage)get_device_proc(device, "vkCmdBlitImage");
	dispatch_table.CmdCopyImage = (PFN_vkCmdCopyImage)get_device_proc(device, "vkCmdCopyImage");
	dispatch_table.CmdCopyBufferToImage = (PFN_vkCmdCopyBufferToImage)get_device_proc(device, "vkCmdCopyBufferToImage");
	dispatch_table.CmdUpdateBuffer = (PFN_vkCmdUpdateBuffer)get_device_proc(device, "vkCmdUpdateBuffer");
	dispatch_table.CmdBindDescriptorSets = (PFN_vkCmdBindDescriptorSets)get_device_proc(device, "vkCmdBindDescriptorSets");
	dispatch_table.CmdPushConstants = (PFN_vkCmdPushConstants)get_device_proc(device, "vkCmdPushConstants");
	dispatch_table.CmdPushDescriptorSetKHR = (PFN_vkCmdPushDescriptorSetKHR)get_device_proc(device, "vkCmdPushDescriptorSetKHR");
	dispatch_table.CmdClearDepthStencilImage = (PFN_vkCmdClearDepthStencilImage)get_device_proc(device, "vkCmdClearDepthStencilImage");
	dispatch_table.CmdEndRenderPass = (PFN_vkCmdEndRenderPass)get_device_proc(device, "vkCmdEndRenderPass");
	dispatch_table.CmdBeginRenderPass = (PFN_vkCmdBeginRenderPass)get_device_proc(device, "vkCmdBeginRenderPass");
	dispatch_table.CmdBindPipeline = (PFN_vkCmdBindPipeline)get_device_proc(device, "vkCmdBindPipeline");
	dispatch_table.CmdBindIndexBuffer = (PFN_vkCmdBindIndexBuffer)get_device_proc(device, "vkCmdBindIndexBuffer");
	dispatch_table.CmdBindVertexBuffers = (PFN_vkCmdBindVertexBuffers)get_device_proc(device, "vkCmdBindVertexBuffers");
	dispatch_table.CmdSetScissor = (PFN_vkCmdSetScissor)get_device_proc(device, "vkCmdSetScissor");
	dispatch_table.CmdSetViewport = (PFN_vkCmdSetViewport)get_device_proc(device, "vkCmdSetViewport");
	dispatch_table.CmdDraw = (PFN_vkCmdDraw)get_device_proc(device, "vkCmdDraw");
	dispatch_table.CmdDrawIndexed = (PFN_vkCmdDrawIndexed)get_device_proc(device, "vkCmdDrawIndexed");
	dispatch_table.CmdPipelineBarrier = (PFN_vkCmdPipelineBarrier)get_device_proc(device, "vkCmdPipelineBarrier");
	dispatch_table.DebugMarkerSetObjectNameEXT = (PFN_vkDebugMarkerSetObjectNameEXT)get_device_proc(device, "vkDebugMarkerSetObjectNameEXT");

	return VK_SUCCESS;
}
void     VKAPI_CALL vkDestroyDevice(VkDevice device, const VkAllocationCallbacks *pAllocator)
{
	LOG(INFO) << "Redirecting vkDestroyDevice" << '(' << device << ", " << pAllocator << ')' << " ...";

	const vk_layer_device_data &device_data = s_device_data.at(get_dispatch_key(device));

	// Continue call down the chain
	assert(device_data.dispatch_table.DestroyDevice != nullptr);
	device_data.dispatch_table.DestroyDevice(device, pAllocator);

	s_device_data.erase(get_dispatch_key(device));
}

void     VKAPI_CALL vkGetDeviceQueue(VkDevice device, uint32_t queueFamilyIndex, uint32_t queueIndex, VkQueue *pQueue)
{
	const vk_layer_device_data &device_data = s_device_data.at(get_dispatch_key(device));

	// Continue call down the chain
	assert(device_data.dispatch_table.GetDeviceQueue != nullptr);
	device_data.dispatch_table.GetDeviceQueue(device, queueFamilyIndex, queueIndex, pQueue);

	s_device_data[get_dispatch_key(*pQueue)] = device_data;
}
void     VKAPI_CALL vkGetDeviceQueue2(VkDevice device, const VkDeviceQueueInfo2 *pQueueInfo, VkQueue *pQueue)
{
	const vk_layer_device_data &device_data = s_device_data.at(get_dispatch_key(device));

	// Continue call down the chain
	assert(device_data.dispatch_table.GetDeviceQueue2 != nullptr);
	device_data.dispatch_table.GetDeviceQueue2(device, pQueueInfo, pQueue);

	s_device_data[get_dispatch_key(*pQueue)] = device_data;
}

VkResult VKAPI_CALL vkCreateSwapchainKHR(VkDevice device, const VkSwapchainCreateInfoKHR *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkSwapchainKHR *pSwapchain)
{
	LOG(INFO) << "Redirecting vkCreateSwapchainKHR" << '(' << device << ", " << pCreateInfo << ", " << pAllocator << ", " << pSwapchain << ')' << " ...";

	const vk_layer_device_data &device_data = s_device_data.at(get_dispatch_key(device));

	// Add required usage flags to create info
	VkSwapchainCreateInfoKHR create_info = *pCreateInfo;
	create_info.imageUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

	// Continue call down the chain
	assert(device_data.dispatch_table.CreateSwapchainKHR != nullptr);
	const VkResult result = device_data.dispatch_table.CreateSwapchainKHR(device, &create_info, pAllocator, pSwapchain);
	if (result != VK_SUCCESS)
	{
		LOG(WARN) << "> vkCreateSwapchainKHR failed with error code " << result << '!';
		return result;
	}

	std::shared_ptr<reshade::vulkan::runtime_vk> runtime;
	if (const auto it = s_runtimes.find(pCreateInfo->oldSwapchain); it != s_runtimes.end())
	{
		assert(pCreateInfo->oldSwapchain != VK_NULL_HANDLE);

		// Re-use an existing runtime if this swapchain was not created from scratch
		runtime = it->second;

		// Reset the old runtime before initializing it again below
		runtime->on_reset();

		// Remove it from the list so that a call to 'vkDestroySwapchainKHR' won't reset the runtime again
		s_runtimes.erase(it);
	}
	else
	{
		runtime = std::make_shared<reshade::vulkan::runtime_vk>(
			device,
			device_data.physical_device,
			s_instance_data.at(get_dispatch_key(device_data.instance)).dispatch_table,
			device_data.dispatch_table);
	}

	if (!runtime->on_init(*pSwapchain, *pCreateInfo, s_surface_windows.at(pCreateInfo->surface)))
		LOG(ERROR) << "Failed to initialize Vulkan runtime environment on runtime " << runtime.get() << '.';

	s_runtimes[*pSwapchain] = runtime;

	return VK_SUCCESS;
}
void     VKAPI_CALL vkDestroySwapchainKHR(VkDevice device, VkSwapchainKHR swapchain, const VkAllocationCallbacks *pAllocator)
{
	LOG(INFO) << "Redirecting vkDestroySwapchainKHR" << '(' << device << ", " << swapchain << ", " << pAllocator << ')' << " ...";

	const vk_layer_device_data &device_data = s_device_data.at(get_dispatch_key(device));

	if (const auto it = s_runtimes.find(swapchain); it != s_runtimes.end())
	{
		it->second->on_reset();

		s_runtimes.erase(it);
	}

	// Continue call down the chain
	assert(device_data.dispatch_table.DestroySwapchainKHR != nullptr);
	device_data.dispatch_table.DestroySwapchainKHR(device, swapchain, pAllocator);
}

VkResult VKAPI_CALL vkQueuePresentKHR(VkQueue queue, const VkPresentInfoKHR *pPresentInfo)
{
	const vk_layer_device_data &device_data = s_device_data.at(get_dispatch_key(queue));

	assert(pPresentInfo != nullptr);

	for (uint32_t i = 0; i < pPresentInfo->swapchainCount; ++i)
	{
		const auto it = s_runtimes.find(pPresentInfo->pSwapchains[i]);
		if (it != s_runtimes.end())
			it->second->on_present(pPresentInfo->pImageIndices[i]);
	}

	// Continue call down the chain
	assert(device_data.dispatch_table.QueuePresentKHR != nullptr);
	return device_data.dispatch_table.QueuePresentKHR(queue, pPresentInfo);
}

VK_LAYER_EXPORT PFN_vkVoidFunction VKAPI_CALL vkGetDeviceProcAddr(VkDevice device, const char *pName)
{
	if (0 == strcmp(pName, "vkGetDeviceQueue"))
		return reinterpret_cast<PFN_vkVoidFunction>(vkGetDeviceQueue);
	if (0 == strcmp(pName, "vkGetDeviceQueue2"))
		return reinterpret_cast<PFN_vkVoidFunction>(vkGetDeviceQueue2);
	if (0 == strcmp(pName, "vkCreateSwapchainKHR"))
		return reinterpret_cast<PFN_vkVoidFunction>(vkCreateSwapchainKHR);
	if (0 == strcmp(pName, "vkDestroySwapchainKHR"))
		return reinterpret_cast<PFN_vkVoidFunction>(vkDestroySwapchainKHR);
	if (0 == strcmp(pName, "vkQueuePresentKHR"))
		return reinterpret_cast<PFN_vkVoidFunction>(vkQueuePresentKHR);

	if (device == VK_NULL_HANDLE)
		return nullptr;

	const vk_layer_device_data &device_data = s_device_data.at(get_dispatch_key(device));

	assert(device_data.dispatch_table.GetDeviceProcAddr != nullptr);
	return device_data.dispatch_table.GetDeviceProcAddr(device, pName);
}
VK_LAYER_EXPORT PFN_vkVoidFunction VKAPI_CALL vkGetInstanceProcAddr(VkInstance instance, const char *pName)
{
	if (0 == strcmp(pName, "vkCreateInstance"))
		return reinterpret_cast<PFN_vkVoidFunction>(vkCreateInstance);
	if (0 == strcmp(pName, "vkDestroyInstance"))
		return reinterpret_cast<PFN_vkVoidFunction>(vkDestroyInstance);
	if (0 == strcmp(pName, "vkEnumeratePhysicalDevices"))
		return reinterpret_cast<PFN_vkVoidFunction>(vkEnumeratePhysicalDevices);
	if (0 == strcmp(pName, "vkCreateDevice"))
		return reinterpret_cast<PFN_vkVoidFunction>(vkCreateDevice);
	if (0 == strcmp(pName, "vkDestroyDevice"))
		return reinterpret_cast<PFN_vkVoidFunction>(vkDestroyDevice);
	if (0 == strcmp(pName, "vkCreateWin32SurfaceKHR"))
		return reinterpret_cast<PFN_vkVoidFunction>(vkCreateWin32SurfaceKHR);
	if (0 == strcmp(pName, "vkDestroySurfaceKHR"))
		return reinterpret_cast<PFN_vkVoidFunction>(vkDestroySurfaceKHR);

	if (instance == VK_NULL_HANDLE)
		return nullptr;

	const vk_layer_instance_data &instance_data = s_instance_data.at(get_dispatch_key(instance));

	assert(instance_data.dispatch_table.GetInstanceProcAddr != nullptr);
	return instance_data.dispatch_table.GetInstanceProcAddr(instance, pName);
}
