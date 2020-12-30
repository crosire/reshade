/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "dll_log.hpp"
#include "hook_manager.hpp"
#include "vulkan_hooks.hpp"
#include "lockfree_table.hpp"

lockfree_table<void *, VkLayerInstanceDispatchTable, 16> s_instance_dispatch;
lockfree_table<VkSurfaceKHR, HWND, 16> s_surface_windows;

#define GET_INSTANCE_DISPATCH_PTR(name, object) \
	PFN_vk##name trampoline = s_instance_dispatch.at(dispatch_key_from_handle(object)).name; \
	assert(trampoline != nullptr)

VkResult VKAPI_CALL vkCreateInstance(const VkInstanceCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkInstance *pInstance)
{
	LOG(INFO) << "Redirecting " << "vkCreateInstance" << '(' << "pCreateInfo = " << pCreateInfo << ", pAllocator = " << pAllocator << ", pInstance = " << pInstance << ')' << " ...";

	assert(pCreateInfo != nullptr && pInstance != nullptr);

	// Look for layer link info if installed as a layer (provided by the Vulkan loader)
	VkLayerInstanceCreateInfo *const link_info = find_layer_info<VkLayerInstanceCreateInfo>(
		pCreateInfo->pNext, VK_STRUCTURE_TYPE_LOADER_INSTANCE_CREATE_INFO, VK_LAYER_LINK_INFO);

	// Get trampoline function pointers
	PFN_vkCreateInstance trampoline = nullptr;
	PFN_vkGetInstanceProcAddr get_instance_proc = nullptr;
	PFN_GetPhysicalDeviceProcAddr get_physical_device_proc = nullptr;

	if (link_info != nullptr)
	{
		assert(link_info->u.pLayerInfo != nullptr);
		assert(link_info->u.pLayerInfo->pfnNextGetInstanceProcAddr != nullptr);

		// Look up functions in layer info
		get_instance_proc = link_info->u.pLayerInfo->pfnNextGetInstanceProcAddr;
		get_physical_device_proc = link_info->u.pLayerInfo->pfnNextGetPhysicalDeviceProcAddr;
		trampoline = reinterpret_cast<PFN_vkCreateInstance>(get_instance_proc(nullptr, "vkCreateInstance"));

		// Advance the link info for the next element of the chain
		link_info->u.pLayerInfo = link_info->u.pLayerInfo->pNext;
	}
#ifdef RESHADE_TEST_APPLICATION
	else
	{
		trampoline = reshade::hooks::call(vkCreateInstance);
		get_instance_proc = reshade::hooks::call(vkGetInstanceProcAddr);
	}
#endif

	if (trampoline == nullptr) // Unable to resolve next 'vkCreateInstance' function in the call chain
		return VK_ERROR_INITIALIZATION_FAILED;

	LOG(INFO) << "> Dumping enabled instance extensions:";
	for (uint32_t i = 0; i < pCreateInfo->enabledExtensionCount; ++i)
		LOG(INFO) << "  " << pCreateInfo->ppEnabledExtensionNames[i];

	VkApplicationInfo app_info { VK_STRUCTURE_TYPE_APPLICATION_INFO };
	if (pCreateInfo->pApplicationInfo != nullptr)
		app_info = *pCreateInfo->pApplicationInfo;

	LOG(INFO) << "> Requesting new Vulkan instance for API version " << VK_VERSION_MAJOR(app_info.apiVersion) << '.' << VK_VERSION_MINOR(app_info.apiVersion) << " ...";

	// ReShade requires at least Vulkan 1.1 (for SPIR-V 1.3 compatibility)
	if (app_info.apiVersion < VK_API_VERSION_1_1)
	{
		LOG(INFO) << "> Replacing requested version with 1.1 ...";

		app_info.apiVersion = VK_API_VERSION_1_1;
	}

	VkInstanceCreateInfo create_info = *pCreateInfo;
	create_info.pApplicationInfo = &app_info;

	// Continue call down the chain
	const VkResult result = trampoline(&create_info, pAllocator, pInstance);
	if (result != VK_SUCCESS)
	{
		LOG(WARN) << "vkCreateInstance" << " failed with error code " << result << '.';
		return result;
	}

	VkInstance instance = *pInstance;
	// Initialize the instance dispatch table
	VkLayerInstanceDispatchTable dispatch_table = { get_physical_device_proc };

#define INIT_INSTANCE_PROC(name) \
	dispatch_table.name = reinterpret_cast<PFN_vk##name>(get_instance_proc(instance, "vk" #name))

	// ---- Core 1_0 commands
	INIT_INSTANCE_PROC(DestroyInstance);
	INIT_INSTANCE_PROC(EnumeratePhysicalDevices);
	INIT_INSTANCE_PROC(GetPhysicalDeviceFormatProperties);
	INIT_INSTANCE_PROC(GetPhysicalDeviceProperties);
	INIT_INSTANCE_PROC(GetPhysicalDeviceMemoryProperties);
	INIT_INSTANCE_PROC(GetPhysicalDeviceQueueFamilyProperties);
	dispatch_table.GetInstanceProcAddr = get_instance_proc;
	INIT_INSTANCE_PROC(EnumerateDeviceExtensionProperties);
	// ---- Core 1_1 commands
	INIT_INSTANCE_PROC(GetPhysicalDeviceMemoryProperties2);
	// ---- VK_KHR_surface extension commands
	INIT_INSTANCE_PROC(DestroySurfaceKHR);
	// ---- VK_KHR_win32_surface extension commands
	INIT_INSTANCE_PROC(CreateWin32SurfaceKHR);

	s_instance_dispatch.emplace(dispatch_key_from_handle(instance), dispatch_table);

#if RESHADE_VERBOSE_LOG
	LOG(INFO) << "Returning Vulkan instance " << instance << '.';
#endif
	return VK_SUCCESS;
}

void     VKAPI_CALL vkDestroyInstance(VkInstance instance, const VkAllocationCallbacks *pAllocator)
{
	LOG(INFO) << "Redirecting " << "vkDestroyInstance" << '(' << "instance = " << instance << ", pAllocator = " << pAllocator << ')' << " ...";

	// Get function pointer before removing it next
	GET_INSTANCE_DISPATCH_PTR(DestroyInstance, instance);
	// Remove instance dispatch table since this instance is being destroyed
	s_instance_dispatch.erase(dispatch_key_from_handle(instance));

	trampoline(instance, pAllocator);
}

VkResult VKAPI_CALL vkCreateWin32SurfaceKHR(VkInstance instance, const VkWin32SurfaceCreateInfoKHR *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkSurfaceKHR *pSurface)
{
	LOG(INFO) << "Redirecting " << "vkCreateWin32SurfaceKHR" << '(' << "instance = " << instance << ", pCreateInfo = " << pCreateInfo << ", pAllocator = " << pAllocator << ", pSurface = " << pSurface << ')' << " ...";

	GET_INSTANCE_DISPATCH_PTR(CreateWin32SurfaceKHR, instance);
	const VkResult result = trampoline(instance, pCreateInfo, pAllocator, pSurface);
	if (result != VK_SUCCESS)
	{
		LOG(WARN) << "vkCreateWin32SurfaceKHR" << " failed with error code " << result << '.';
		return result;
	}

	s_surface_windows.emplace(*pSurface, pCreateInfo->hwnd);

	return VK_SUCCESS;
}

void     VKAPI_CALL vkDestroySurfaceKHR(VkInstance instance, VkSurfaceKHR surface, const VkAllocationCallbacks *pAllocator)
{
	LOG(INFO) << "Redirecting " << "vkDestroySurfaceKHR" << '(' << "instance = " << instance << ", surface = " << surface << ", pAllocator = " << pAllocator << ')' << " ...";

	s_surface_windows.erase(surface);

	GET_INSTANCE_DISPATCH_PTR(DestroySurfaceKHR, instance);
	trampoline(instance, surface, pAllocator);
}

VK_LAYER_EXPORT PFN_vkVoidFunction VKAPI_CALL vkGetInstanceProcAddr(VkInstance instance, const char *pName)
{
#define CHECK_INSTANCE_PROC(name) \
	if (0 == strcmp(pName, "vk" #name)) \
		return reinterpret_cast<PFN_vkVoidFunction>(vk##name)

	CHECK_INSTANCE_PROC(CreateInstance);
	CHECK_INSTANCE_PROC(DestroyInstance);
	CHECK_INSTANCE_PROC(CreateDevice);
	CHECK_INSTANCE_PROC(DestroyDevice);
	CHECK_INSTANCE_PROC(CreateWin32SurfaceKHR);
	CHECK_INSTANCE_PROC(DestroySurfaceKHR);

	// Self-intercept here as well to stay consistent with 'vkGetDeviceProcAddr' implementation
	CHECK_INSTANCE_PROC(GetInstanceProcAddr);

	if (instance == VK_NULL_HANDLE)
		return nullptr;

	GET_INSTANCE_DISPATCH_PTR(GetInstanceProcAddr, instance);
	return trampoline(instance, pName);
}
