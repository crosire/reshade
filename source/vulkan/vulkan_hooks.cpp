/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "log.hpp"
#include "hook_manager.hpp"
#include "runtime_vk.hpp"
#include <assert.h>
#include <memory>
#include <unordered_map>

static std::unordered_map<VkSurfaceKHR, HWND> s_surface_windows;
static std::unordered_map<VkDevice, VkPhysicalDevice> s_device_mapping;
static std::unordered_map<VkPhysicalDevice, VkInstance> s_instance_mapping;
static std::unordered_map<VkSwapchainKHR, std::shared_ptr<reshade::vulkan::runtime_vk>> s_runtimes;

HOOK_EXPORT VkResult VKAPI_CALL vkEnumeratePhysicalDevices(VkInstance instance, uint32_t *pPhysicalDeviceCount, VkPhysicalDevice *pPhysicalDevices)
{
	LOG(INFO) << "Redirecting vkEnumeratePhysicalDevices" << '(' << instance << ", " << pPhysicalDeviceCount << ", " << pPhysicalDevices << ')' << " ...";

	const VkResult result = reshade::hooks::call(vkEnumeratePhysicalDevices)(instance, pPhysicalDeviceCount, pPhysicalDevices);
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

HOOK_EXPORT VkResult VKAPI_CALL vkCreateDevice(VkPhysicalDevice physicalDevice, const VkDeviceCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkDevice *pDevice)
{
	LOG(INFO) << "Redirecting vkCreateDevice" << '(' << physicalDevice << ", " << pCreateInfo << ", " << pAllocator << ", " << pDevice << ')' << " ...";

	std::vector<const char *> enabled_extensions;
	enabled_extensions.reserve(pCreateInfo->enabledExtensionCount);
	for (uint32_t i = 0; i < pCreateInfo->enabledExtensionCount; ++i)
		enabled_extensions.push_back(pCreateInfo->ppEnabledExtensionNames[i]);

	// Add extensions that ReShade requires
	enabled_extensions.push_back(VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME);

	VkDeviceCreateInfo create_info = *pCreateInfo;
	create_info.enabledExtensionCount = uint32_t(enabled_extensions.size());
	create_info.ppEnabledExtensionNames = enabled_extensions.data();

	const VkResult result = reshade::hooks::call(vkCreateDevice)(physicalDevice, &create_info, pAllocator, pDevice);
	if (result != VK_SUCCESS)
	{
		LOG(WARN) << "> vkCreateDevice failed with error code " << result << '!';
		return result;
	}

	s_device_mapping[*pDevice] = physicalDevice;

	return VK_SUCCESS;
}
HOOK_EXPORT void     VKAPI_CALL vkDestroyDevice(VkDevice device, const VkAllocationCallbacks *pAllocator)
{
	LOG(INFO) << "Redirecting vkDestroyDevice" << '(' << device << ", " << pAllocator << ')' << " ...";

	s_device_mapping.erase(device);

	return reshade::hooks::call(vkDestroyDevice)(device, pAllocator);
}

HOOK_EXPORT VkResult VKAPI_CALL vkCreateWin32SurfaceKHR(VkInstance instance, const VkWin32SurfaceCreateInfoKHR *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkSurfaceKHR *pSurface)
{
	LOG(INFO) << "Redirecting vkCreateWin32SurfaceKHR" << '(' << instance << ", " << pCreateInfo << ", " << pAllocator << ", " << pSurface << ')' << " ...";

	const VkResult result = reshade::hooks::call(vkCreateWin32SurfaceKHR)(instance, pCreateInfo, pAllocator, pSurface);
	if (result != VK_SUCCESS)
	{
		LOG(WARN) << "> vkCreateWin32SurfaceKHR failed with error code " << result << '!';
		return result;
	}

	s_surface_windows[*pSurface] = pCreateInfo->hwnd;

	return VK_SUCCESS;
}
HOOK_EXPORT void     VKAPI_CALL vkDestroySurfaceKHR(VkInstance instance, VkSurfaceKHR surface, const VkAllocationCallbacks *pAllocator)
{
	LOG(INFO) << "Redirecting vkDestroySurfaceKHR" << '(' << instance << ", " << surface << ", " << pAllocator << ')' << " ...";

	s_surface_windows.erase(surface);

	return reshade::hooks::call(vkDestroySurfaceKHR)(instance, surface, pAllocator);
}

HOOK_EXPORT VkResult VKAPI_CALL vkCreateSwapchainKHR(VkDevice device, const VkSwapchainCreateInfoKHR *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkSwapchainKHR *pSwapchain)
{
	LOG(INFO) << "Redirecting vkCreateSwapchainKHR" << '(' << device << ", " << pCreateInfo << ", " << pAllocator << ", " << pSwapchain << ')' << " ...";

	// Add required usage flags to create info
	VkSwapchainCreateInfoKHR create_info = *pCreateInfo;
	create_info.imageUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

	VkResult result = reshade::hooks::call(vkCreateSwapchainKHR)(device, &create_info, pAllocator, pSwapchain);
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
		const auto get_device_proc_addr = reshade::hooks::call(vkGetDeviceProcAddr);
		const auto get_instance_proc_addr = reshade::hooks::call(vkGetInstanceProcAddr);

		const VkPhysicalDevice physical_device = s_device_mapping.at(device);
		const VkInstance instance = s_instance_mapping.at(physical_device);

		// Load original Vulkan functions instead of using the hooked ones
		vk_device_table table = {};
		table.vkQueueWaitIdle = reinterpret_cast<PFN_vkQueueWaitIdle>(get_device_proc_addr(device, "vkQueueWaitIdle"));
		table.vkDeviceWaitIdle = reinterpret_cast<PFN_vkDeviceWaitIdle>(get_device_proc_addr(device, "vkDeviceWaitIdle"));
		table.vkGetDeviceQueue = reinterpret_cast<PFN_vkGetDeviceQueue>(get_device_proc_addr(device, "vkGetDeviceQueue"));
		table.vkGetSwapchainImagesKHR = reinterpret_cast<PFN_vkGetSwapchainImagesKHR>(get_device_proc_addr(device, "vkGetSwapchainImagesKHR"));
		table.vkQueueSubmit = reinterpret_cast<PFN_vkQueueSubmit>(get_device_proc_addr(device, "vkQueueSubmit"));
		table.vkCreateFence = reinterpret_cast<PFN_vkCreateFence>(get_device_proc_addr(device, "vkCreateFence"));
		table.vkDestroyFence = reinterpret_cast<PFN_vkDestroyFence>(get_device_proc_addr(device, "vkDestroyFence"));
		table.vkWaitForFences = reinterpret_cast<PFN_vkWaitForFences>(get_device_proc_addr(device, "vkWaitForFences"));
		table.vkResetFences = reinterpret_cast<PFN_vkResetFences>(get_device_proc_addr(device, "vkResetFences"));
		table.vkMapMemory = reinterpret_cast<PFN_vkMapMemory>(get_device_proc_addr(device, "vkMapMemory"));
		table.vkFreeMemory = reinterpret_cast<PFN_vkFreeMemory>(get_device_proc_addr(device, "vkFreeMemory"));
		table.vkUnmapMemory = reinterpret_cast<PFN_vkUnmapMemory>(get_device_proc_addr(device, "vkUnmapMemory"));
		table.vkAllocateMemory = reinterpret_cast<PFN_vkAllocateMemory>(get_device_proc_addr(device, "vkAllocateMemory"));
		table.vkCreateImage = reinterpret_cast<PFN_vkCreateImage>(get_device_proc_addr(device, "vkCreateImage"));
		table.vkDestroyImage = reinterpret_cast<PFN_vkDestroyImage>(get_device_proc_addr(device, "vkDestroyImage"));
		table.vkGetImageSubresourceLayout = reinterpret_cast<PFN_vkGetImageSubresourceLayout>(get_device_proc_addr(device, "vkGetImageSubresourceLayout"));
		table.vkGetImageMemoryRequirements = reinterpret_cast<PFN_vkGetImageMemoryRequirements>(get_device_proc_addr(device, "vkGetImageMemoryRequirements"));
		table.vkBindImageMemory = reinterpret_cast<PFN_vkBindImageMemory>(get_device_proc_addr(device, "vkBindImageMemory"));
		table.vkCreateBuffer = reinterpret_cast<PFN_vkCreateBuffer>(get_device_proc_addr(device, "vkCreateBuffer"));
		table.vkDestroyBuffer = reinterpret_cast<PFN_vkDestroyBuffer>(get_device_proc_addr(device, "vkDestroyBuffer"));
		table.vkGetBufferMemoryRequirements = reinterpret_cast<PFN_vkGetBufferMemoryRequirements>(get_device_proc_addr(device, "vkGetBufferMemoryRequirements"));
		table.vkBindBufferMemory = reinterpret_cast<PFN_vkBindBufferMemory>(get_device_proc_addr(device, "vkBindBufferMemory"));
		table.vkBindBufferMemory2 = reinterpret_cast<PFN_vkBindBufferMemory2>(get_device_proc_addr(device, "vkBindBufferMemory2"));
		table.vkCreateImageView = reinterpret_cast<PFN_vkCreateImageView>(get_device_proc_addr(device, "vkCreateImageView"));
		table.vkDestroyImageView = reinterpret_cast<PFN_vkDestroyImageView>(get_device_proc_addr(device, "vkDestroyImageView"));
		table.vkCreateRenderPass = reinterpret_cast<PFN_vkCreateRenderPass>(get_device_proc_addr(device, "vkCreateRenderPass"));
		table.vkDestroyRenderPass = reinterpret_cast<PFN_vkDestroyRenderPass>(get_device_proc_addr(device, "vkDestroyRenderPass"));
		table.vkCreateFramebuffer = reinterpret_cast<PFN_vkCreateFramebuffer>(get_device_proc_addr(device, "vkCreateFramebuffer"));
		table.vkDestroyFramebuffer = reinterpret_cast<PFN_vkDestroyFramebuffer>(get_device_proc_addr(device, "vkDestroyFramebuffer"));
		table.vkResetCommandPool = reinterpret_cast<PFN_vkResetCommandPool>(get_device_proc_addr(device, "vkResetCommandPool"));
		table.vkCreateCommandPool = reinterpret_cast<PFN_vkCreateCommandPool>(get_device_proc_addr(device, "vkCreateCommandPool"));
		table.vkDestroyCommandPool = reinterpret_cast<PFN_vkDestroyCommandPool>(get_device_proc_addr(device, "vkDestroyCommandPool"));
		table.vkAllocateCommandBuffers = reinterpret_cast<PFN_vkAllocateCommandBuffers>(get_device_proc_addr(device, "vkAllocateCommandBuffers"));
		table.vkEndCommandBuffer = reinterpret_cast<PFN_vkEndCommandBuffer>(get_device_proc_addr(device, "vkEndCommandBuffer"));
		table.vkBeginCommandBuffer = reinterpret_cast<PFN_vkBeginCommandBuffer>(get_device_proc_addr(device, "vkBeginCommandBuffer"));
		table.vkResetDescriptorPool = reinterpret_cast<PFN_vkResetDescriptorPool>(get_device_proc_addr(device, "vkResetDescriptorPool"));
		table.vkCreateDescriptorPool = reinterpret_cast<PFN_vkCreateDescriptorPool>(get_device_proc_addr(device, "vkCreateDescriptorPool"));
		table.vkDestroyDescriptorPool = reinterpret_cast<PFN_vkDestroyDescriptorPool>(get_device_proc_addr(device, "vkDestroyDescriptorPool"));
		table.vkAllocateDescriptorSets = reinterpret_cast<PFN_vkAllocateDescriptorSets>(get_device_proc_addr(device, "vkAllocateDescriptorSets"));
		table.vkUpdateDescriptorSets = reinterpret_cast<PFN_vkUpdateDescriptorSets>(get_device_proc_addr(device, "vkUpdateDescriptorSets"));
		table.vkCreateDescriptorSetLayout = reinterpret_cast<PFN_vkCreateDescriptorSetLayout>(get_device_proc_addr(device, "vkCreateDescriptorSetLayout"));
		table.vkDestroyDescriptorSetLayout = reinterpret_cast<PFN_vkDestroyDescriptorSetLayout>(get_device_proc_addr(device, "vkDestroyDescriptorSetLayout"));
		table.vkCreateGraphicsPipelines = reinterpret_cast<PFN_vkCreateGraphicsPipelines>(get_device_proc_addr(device, "vkCreateGraphicsPipelines"));
		table.vkDestroyPipeline = reinterpret_cast<PFN_vkDestroyPipeline>(get_device_proc_addr(device, "vkDestroyPipeline"));
		table.vkCreatePipelineLayout = reinterpret_cast<PFN_vkCreatePipelineLayout>(get_device_proc_addr(device, "vkCreatePipelineLayout"));
		table.vkDestroyPipelineLayout = reinterpret_cast<PFN_vkDestroyPipelineLayout>(get_device_proc_addr(device, "vkDestroyPipelineLayout"));
		table.vkCreateSampler = reinterpret_cast<PFN_vkCreateSampler>(get_device_proc_addr(device, "vkCreateSampler"));
		table.vkDestroySampler = reinterpret_cast<PFN_vkDestroySampler>(get_device_proc_addr(device, "vkDestroySampler"));
		table.vkCreateShaderModule = reinterpret_cast<PFN_vkCreateShaderModule>(get_device_proc_addr(device, "vkCreateShaderModule"));
		table.vkDestroyShaderModule = reinterpret_cast<PFN_vkDestroyShaderModule>(get_device_proc_addr(device, "vkDestroyShaderModule"));
		table.vkCmdBlitImage = reinterpret_cast<PFN_vkCmdBlitImage>(get_device_proc_addr(device, "vkCmdBlitImage"));
		table.vkCmdCopyImage = reinterpret_cast<PFN_vkCmdCopyImage>(get_device_proc_addr(device, "vkCmdCopyImage"));
		table.vkCmdCopyBufferToImage = reinterpret_cast<PFN_vkCmdCopyBufferToImage>(get_device_proc_addr(device, "vkCmdCopyBufferToImage"));
		table.vkCmdUpdateBuffer = reinterpret_cast<PFN_vkCmdUpdateBuffer>(get_device_proc_addr(device, "vkCmdUpdateBuffer"));
		table.vkCmdBindDescriptorSets = reinterpret_cast<PFN_vkCmdBindDescriptorSets>(get_device_proc_addr(device, "vkCmdBindDescriptorSets"));
		table.vkCmdPushConstants = reinterpret_cast<PFN_vkCmdPushConstants>(get_device_proc_addr(device, "vkCmdPushConstants"));
		table.vkCmdPushDescriptorSetKHR = reinterpret_cast<PFN_vkCmdPushDescriptorSetKHR>(get_device_proc_addr(device, "vkCmdPushDescriptorSetKHR"));
		table.vkCmdClearDepthStencilImage = reinterpret_cast<PFN_vkCmdClearDepthStencilImage>(get_device_proc_addr(device, "vkCmdClearDepthStencilImage"));
		table.vkCmdEndRenderPass = reinterpret_cast<PFN_vkCmdEndRenderPass>(get_device_proc_addr(device, "vkCmdEndRenderPass"));
		table.vkCmdBeginRenderPass = reinterpret_cast<PFN_vkCmdBeginRenderPass>(get_device_proc_addr(device, "vkCmdBeginRenderPass"));
		table.vkCmdBindPipeline = reinterpret_cast<PFN_vkCmdBindPipeline>(get_device_proc_addr(device, "vkCmdBindPipeline"));
		table.vkCmdBindIndexBuffer = reinterpret_cast<PFN_vkCmdBindIndexBuffer>(get_device_proc_addr(device, "vkCmdBindIndexBuffer"));
		table.vkCmdBindVertexBuffers = reinterpret_cast<PFN_vkCmdBindVertexBuffers>(get_device_proc_addr(device, "vkCmdBindVertexBuffers"));
		table.vkCmdSetScissor = reinterpret_cast<PFN_vkCmdSetScissor>(get_device_proc_addr(device, "vkCmdSetScissor"));
		table.vkCmdSetViewport = reinterpret_cast<PFN_vkCmdSetViewport>(get_device_proc_addr(device, "vkCmdSetViewport"));
		table.vkCmdDraw = reinterpret_cast<PFN_vkCmdDraw>(get_device_proc_addr(device, "vkCmdDraw"));
		table.vkCmdDrawIndexed = reinterpret_cast<PFN_vkCmdDrawIndexed>(get_device_proc_addr(device, "vkCmdDrawIndexed"));
		table.vkCmdPipelineBarrier = reinterpret_cast<PFN_vkCmdPipelineBarrier>(get_device_proc_addr(device, "vkCmdPipelineBarrier"));
		table.vkDebugMarkerSetObjectNameEXT = reinterpret_cast<PFN_vkDebugMarkerSetObjectNameEXT>(get_device_proc_addr(device, "vkDebugMarkerSetObjectNameEXT"));
		table.vkGetPhysicalDeviceMemoryProperties = reinterpret_cast<PFN_vkGetPhysicalDeviceMemoryProperties>(get_instance_proc_addr(instance, "vkGetPhysicalDeviceMemoryProperties"));

		runtime = std::make_shared<reshade::vulkan::runtime_vk>(device, physical_device, table);
	}

	if (!runtime->on_init(*pSwapchain, *pCreateInfo, s_surface_windows.at(pCreateInfo->surface)))
		LOG(ERROR) << "Failed to initialize Vulkan runtime environment on runtime " << runtime.get() << '.';

	s_runtimes[*pSwapchain] = runtime;

	return VK_SUCCESS;
}
HOOK_EXPORT void     VKAPI_CALL vkDestroySwapchainKHR(VkDevice device, VkSwapchainKHR swapchain, const VkAllocationCallbacks *pAllocator)
{
	LOG(INFO) << "Redirecting vkDestroySwapchainKHR" << '(' << device << ", " << swapchain << ", " << pAllocator << ')' << " ...";

	const auto it = s_runtimes.find(swapchain);

	if (it != s_runtimes.end())
	{
		it->second->on_reset();

		s_runtimes.erase(it);
	}

	return reshade::hooks::call(vkDestroySwapchainKHR)(device, swapchain, pAllocator);
}

HOOK_EXPORT VkResult VKAPI_CALL vkQueuePresentKHR(VkQueue queue, const VkPresentInfoKHR *pPresentInfo)
{
	assert(pPresentInfo != nullptr);

	for (uint32_t i = 0; i < pPresentInfo->swapchainCount; ++i)
	{
		const auto it = s_runtimes.find(pPresentInfo->pSwapchains[i]);
		if (it != s_runtimes.end())
			it->second->on_present(pPresentInfo->pImageIndices[i]);
	}

	static const auto trampoline = reshade::hooks::call(vkQueuePresentKHR);
	return trampoline(queue, pPresentInfo);
}

HOOK_EXPORT PFN_vkVoidFunction VKAPI_CALL vkGetDeviceProcAddr(VkDevice device, const char *pName)
{
	static const auto trampoline = reshade::hooks::call(vkGetDeviceProcAddr);
	const PFN_vkVoidFunction address = trampoline(device, pName);

	if (address == nullptr || pName == nullptr)
		return nullptr;
	else if (0 == strcmp(pName, "vkCreateSwapchainKHR"))
		return reinterpret_cast<PFN_vkVoidFunction>(vkCreateSwapchainKHR);
	else if (0 == strcmp(pName, "vkDestroySwapchainKHR"))
		return reinterpret_cast<PFN_vkVoidFunction>(vkDestroySwapchainKHR);
	else if (0 == strcmp(pName, "vkQueuePresentKHR"))
		return reinterpret_cast<PFN_vkVoidFunction>(vkQueuePresentKHR);

	return address;
}
HOOK_EXPORT PFN_vkVoidFunction VKAPI_CALL vkGetInstanceProcAddr(VkInstance instance, const char *pName)
{
	static const auto trampoline = reshade::hooks::call(vkGetInstanceProcAddr);
	const PFN_vkVoidFunction address = trampoline(instance, pName);

	if (address == nullptr || pName == nullptr)
		return nullptr;
	else if (0 == strcmp(pName, "vkEnumeratePhysicalDevices"))
		return reinterpret_cast<PFN_vkVoidFunction>(vkEnumeratePhysicalDevices);
	else if (0 == strcmp(pName, "vkCreateDevice"))
		return reinterpret_cast<PFN_vkVoidFunction>(vkCreateDevice);
	else if (0 == strcmp(pName, "vkDestroyDevice"))
		return reinterpret_cast<PFN_vkVoidFunction>(vkDestroyDevice);
	else if (0 == strcmp(pName, "vkCreateWin32SurfaceKHR"))
		return reinterpret_cast<PFN_vkVoidFunction>(vkCreateWin32SurfaceKHR);
	else if (0 == strcmp(pName, "vkDestroySurfaceKHR"))
		return reinterpret_cast<PFN_vkVoidFunction>(vkDestroySurfaceKHR);

	return address;
}
