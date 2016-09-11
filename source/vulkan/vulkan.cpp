#include "log.hpp"
#include "hook_manager.hpp"
#include "vulkan_runtime.hpp"
#include <assert.h>
#include <memory>
#include <unordered_map>

std::unordered_map<VkDevice, VkPhysicalDevice> g_vulkan_device_mapping;
std::unordered_map<VkSwapchainKHR, std::vector<VkImage>> g_vulkan_swapchain_images;
static std::unordered_map<VkSurfaceKHR, HWND> s_surface_windows;
static std::unordered_map<VkSwapchainKHR, std::shared_ptr<reshade::vulkan::vulkan_runtime>> s_runtimes;

HOOK_EXPORT VkResult VKAPI_CALL vkCreateDevice(VkPhysicalDevice physicalDevice, const VkDeviceCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkDevice *pDevice)
{
	const VkResult result = reshade::hooks::call(&vkCreateDevice)(physicalDevice, pCreateInfo, pAllocator, pDevice);

	if (result != VK_SUCCESS)
	{
		return result;
	}

	g_vulkan_device_mapping[*pDevice] = physicalDevice;

	return VK_SUCCESS;
}
HOOK_EXPORT VkResult VKAPI_CALL vkCreateSwapchainKHR(VkDevice device, const VkSwapchainCreateInfoKHR *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkSwapchainKHR *pSwapchain)
{
	LOG(INFO) << "Redirecting '" << "vkCreateSwapchainKHR" << "(" << device << ", " << pCreateInfo << ", " << pAllocator << ", " << pSwapchain << ")' ...";

	VkResult result = reshade::hooks::call(&vkCreateSwapchainKHR)(device, pCreateInfo, pAllocator, pSwapchain);

	if (result != VK_SUCCESS || pCreateInfo == nullptr || pSwapchain == nullptr)
	{
		LOG(WARNING) << "> 'vkCreateSwapchainKHR' failed with error code " << result << "!";

		return result;
	}

	const VkSwapchainKHR swapchain = *pSwapchain;

	uint32_t swapchain_image_count = 0;
	result = reshade::hooks::call(&vkGetSwapchainImagesKHR)(device, swapchain, &swapchain_image_count, nullptr);
	std::vector<VkImage> swapchain_images(swapchain_image_count);
	result = reshade::hooks::call(&vkGetSwapchainImagesKHR)(device, swapchain, &swapchain_image_count, swapchain_images.data());

	g_vulkan_swapchain_images[swapchain] = swapchain_images;

	const auto runtime = std::make_shared<reshade::vulkan::vulkan_runtime>(device, swapchain);

	if (!runtime->on_init(*pCreateInfo, s_surface_windows[pCreateInfo->surface]))
	{
		LOG(ERROR) << "Failed to initialize Vulkan runtime environment on runtime " << runtime.get() << ".";
	}

	s_runtimes[swapchain] = runtime;

	return VK_SUCCESS;
}
HOOK_EXPORT VkResult VKAPI_CALL vkCreateWin32SurfaceKHR(VkInstance instance, const VkWin32SurfaceCreateInfoKHR *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkSurfaceKHR *pSurface)
{
	LOG(INFO) << "Redirecting '" << "vkCreateWin32SurfaceKHR" << "(" << instance << ", " << pCreateInfo << ", " << pAllocator << ", " << pSurface << ")' ...";

	const VkResult result = reshade::hooks::call(&vkCreateWin32SurfaceKHR)(instance, pCreateInfo, pAllocator, pSurface);

	if (result != VK_SUCCESS || pCreateInfo == nullptr || pSurface == nullptr)
	{
		LOG(WARNING) << "> 'vkCreateWin32SurfaceKHR' failed with error code " << result << "!";

		return result;
	}

	s_surface_windows[*pSurface] = pCreateInfo->hwnd;

	return VK_SUCCESS;
}
HOOK_EXPORT void VKAPI_CALL vkDestroySurfaceKHR(VkInstance instance, VkSurfaceKHR surface, const VkAllocationCallbacks *pAllocator)
{
	LOG(INFO) << "Redirecting '" << "vkDestroySurfaceKHR" << "(" << instance << ", " << surface << ", " << pAllocator << ")' ...";

	s_surface_windows.erase(surface);

	return reshade::hooks::call(&vkDestroySurfaceKHR)(instance, surface, pAllocator);
}
HOOK_EXPORT void VKAPI_CALL vkDestroySwapchainKHR(VkDevice device, VkSwapchainKHR swapchain, const VkAllocationCallbacks *pAllocator)
{
	LOG(INFO) << "Redirecting '" << "vkDestroySwapchainKHR" << "(" << device << ", " << swapchain << ", " << pAllocator << ")' ...";

	const auto it = s_runtimes.find(swapchain);

	if (it != s_runtimes.end())
	{
		it->second->on_reset();

		s_runtimes.erase(it);
	}

	g_vulkan_swapchain_images.erase(swapchain);

	return reshade::hooks::call(&vkDestroySwapchainKHR)(device, swapchain, pAllocator);
}

HOOK_EXPORT VkResult VKAPI_CALL vkGetSwapchainImagesKHR(VkDevice device, VkSwapchainKHR swapchain, uint32_t *pSwapchainImageCount, VkImage *pSwapchainImages)
{
	UNREFERENCED_PARAMETER(device);

	const auto &images = g_vulkan_swapchain_images[swapchain];

	if (pSwapchainImages != nullptr)
	{
		size_t count = *pSwapchainImageCount;

		if (count > images.size())
		{
			count = images.size();
		}

		for (size_t i = 0; i < count; i++)
		{
			pSwapchainImages[i] = images[i];
		}

		if (count < images.size())
		{
			return VK_INCOMPLETE;
		}
	}

	*pSwapchainImageCount = static_cast<uint32_t>(images.size());

	return VK_SUCCESS;
}
HOOK_EXPORT VkResult VKAPI_CALL vkQueuePresentKHR(VkQueue queue, const VkPresentInfoKHR *pPresentInfo)
{
	assert(pPresentInfo != nullptr);

	for (uint32_t i = 0; i < pPresentInfo->swapchainCount; i++)
	{
		const auto it = s_runtimes.find(pPresentInfo->pSwapchains[i]);

		if (it != s_runtimes.end())
		{
			it->second->on_present(pPresentInfo->pImageIndices[i]);
		}
	}

	static const auto trampoline = reshade::hooks::call(&vkQueuePresentKHR);

	return trampoline(queue, pPresentInfo);
}

HOOK_EXPORT PFN_vkVoidFunction VKAPI_CALL vkGetDeviceProcAddr(VkDevice device, const char *pName)
{
	static const auto trampoline = reshade::hooks::call(&vkGetDeviceProcAddr);

	const PFN_vkVoidFunction address = trampoline(device, pName);

	if (address == nullptr || pName == nullptr)
		return nullptr;
	else if (strcmp(pName, "vkCreateSwapchainKHR") == 0)
		return reinterpret_cast<PFN_vkVoidFunction>(&vkCreateSwapchainKHR);
	else if (strcmp(pName, "vkDestroySwapchainKHR") == 0)
		return reinterpret_cast<PFN_vkVoidFunction>(&vkDestroySwapchainKHR);
	else if (strcmp(pName, "vkGetSwapchainImagesKHR") == 0)
		return reinterpret_cast<PFN_vkVoidFunction>(&vkGetSwapchainImagesKHR);
	else if (strcmp(pName, "vkQueuePresentKHR") == 0)
		return reinterpret_cast<PFN_vkVoidFunction>(&vkQueuePresentKHR);

	return address;
}
