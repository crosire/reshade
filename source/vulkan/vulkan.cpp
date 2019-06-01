/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "log.hpp"
#include "hook_manager.hpp"
#include "runtime_vulkan.hpp"
#include <assert.h>
#include <memory>
#include <unordered_map>

static std::unordered_map<VkSurfaceKHR, HWND> s_surface_windows;
static std::unordered_map<VkSwapchainKHR, std::shared_ptr<reshade::vulkan::runtime_vulkan>> s_runtimes;

HOOK_EXPORT VkResult VKAPI_CALL vkCreateWin32SurfaceKHR(VkInstance instance, const VkWin32SurfaceCreateInfoKHR *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkSurfaceKHR *pSurface)
{
	LOG(INFO) << "Redirecting vkCreateWin32SurfaceKHR" << '(' << instance << ", " << pCreateInfo << ", " << pAllocator << ", " << pSurface << ')' << " ...";

	const VkResult result = reshade::hooks::call(vkCreateWin32SurfaceKHR)(instance, pCreateInfo, pAllocator, pSurface);

	if (result != VK_SUCCESS || pCreateInfo == nullptr || pSurface == nullptr)
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

	VkResult result = reshade::hooks::call(vkCreateSwapchainKHR)(device, pCreateInfo, pAllocator, pSwapchain);

	if (result != VK_SUCCESS || pCreateInfo == nullptr || pSwapchain == nullptr)
	{
		LOG(WARN) << "> vkCreateSwapchainKHR failed with error code " << result << '!';
		return result;
	}

	const auto runtime = std::make_shared<reshade::vulkan::runtime_vulkan>(device, *pSwapchain);

	if (!runtime->on_init(*pCreateInfo, s_surface_windows[pCreateInfo->surface]))
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
	else if (0 == strcmp(pName, "vkCreateWin32SurfaceKHR"))
		return reinterpret_cast<PFN_vkVoidFunction>(vkCreateWin32SurfaceKHR);
	else if (0 == strcmp(pName, "vkDestroySurfaceKHR"))
		return reinterpret_cast<PFN_vkVoidFunction>(vkDestroySurfaceKHR);

	return address;
}
