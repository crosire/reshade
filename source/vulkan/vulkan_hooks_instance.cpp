/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "dll_log.hpp"
#include "hook_manager.hpp"
#include "vulkan_hooks.hpp"
#include "lockfree_table.hpp"

lockfree_table<void *, VkLayerInstanceDispatchTable, 16> g_instance_dispatch;
lockfree_table<VkSurfaceKHR, HWND, 16> g_surface_windows;

#define GET_DISPATCH_PTR(name, object) \
	PFN_vk##name trampoline = g_instance_dispatch.at(dispatch_key_from_handle(object)).name; \
	assert(trampoline != nullptr)
#define INIT_DISPATCH_PTR(name) \
	dispatch_table.name = reinterpret_cast<PFN_vk##name>(get_instance_proc(instance, "vk" #name))

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

	auto enum_instance_extensions = reinterpret_cast<PFN_vkEnumerateInstanceExtensionProperties>(get_instance_proc(nullptr, "vkEnumerateInstanceExtensionProperties"));
	assert(enum_instance_extensions != nullptr);

	std::vector<const char *> enabled_extensions;
	enabled_extensions.reserve(pCreateInfo->enabledExtensionCount);
	for (uint32_t i = 0; i < pCreateInfo->enabledExtensionCount; ++i)
		enabled_extensions.push_back(pCreateInfo->ppEnabledExtensionNames[i]);

	{
		uint32_t num_extensions = 0;
		enum_instance_extensions(nullptr, &num_extensions, nullptr);
		std::vector<VkExtensionProperties> extensions(num_extensions);
		enum_instance_extensions(nullptr, &num_extensions, extensions.data());

		// Make sure the driver actually supports the requested extensions
		const auto add_extension = [&extensions, &enabled_extensions](const char *name, bool required) {
			if (const auto it = std::find_if(extensions.begin(), extensions.end(),
				[name](const auto &props) { return strncmp(props.extensionName, name, VK_MAX_EXTENSION_NAME_SIZE) == 0; });
				it != extensions.end())
			{
				enabled_extensions.push_back(name);
				return true;
			}

			if (required)
			{
				LOG(ERROR) << "Required extension \"" << name << "\" is not supported on this instance. Initialization failed.";
			}
			else
			{
				LOG(WARN) << "Optional extension \"" << name << "\" is not supported on this instance.";
			}

			return false;
		};

		// Enable extensions that ReShade requires
#ifndef NDEBUG
		add_extension(VK_EXT_DEBUG_UTILS_EXTENSION_NAME, false);
#endif
	}

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
	create_info.enabledExtensionCount = uint32_t(enabled_extensions.size());
	create_info.ppEnabledExtensionNames = enabled_extensions.data();

	// Continue calling down the chain
	const VkResult result = trampoline(&create_info, pAllocator, pInstance);
	if (result != VK_SUCCESS)
	{
		LOG(WARN) << "vkCreateInstance" << " failed with error code " << result << '.';
		return result;
	}

	VkInstance instance = *pInstance;
	// Initialize the instance dispatch table
	VkLayerInstanceDispatchTable dispatch_table = { get_physical_device_proc };

	// ---- Core 1_0 commands
	INIT_DISPATCH_PTR(DestroyInstance);
	INIT_DISPATCH_PTR(EnumeratePhysicalDevices);
	INIT_DISPATCH_PTR(GetPhysicalDeviceFeatures);
	INIT_DISPATCH_PTR(GetPhysicalDeviceFormatProperties);
	INIT_DISPATCH_PTR(GetPhysicalDeviceProperties);
	INIT_DISPATCH_PTR(GetPhysicalDeviceMemoryProperties);
	INIT_DISPATCH_PTR(GetPhysicalDeviceQueueFamilyProperties);
	dispatch_table.GetInstanceProcAddr = get_instance_proc;
	INIT_DISPATCH_PTR(EnumerateDeviceExtensionProperties);
	// ---- Core 1_1 commands
	INIT_DISPATCH_PTR(GetPhysicalDeviceMemoryProperties2);
	// ---- VK_KHR_surface extension commands
	INIT_DISPATCH_PTR(DestroySurfaceKHR);
	// ---- VK_KHR_win32_surface extension commands
	INIT_DISPATCH_PTR(CreateWin32SurfaceKHR);

	g_instance_dispatch.emplace(dispatch_key_from_handle(instance), dispatch_table);

#if RESHADE_VERBOSE_LOG
	LOG(INFO) << "Returning Vulkan instance " << instance << '.';
#endif
	return VK_SUCCESS;
}

void     VKAPI_CALL vkDestroyInstance(VkInstance instance, const VkAllocationCallbacks *pAllocator)
{
	LOG(INFO) << "Redirecting " << "vkDestroyInstance" << '(' << "instance = " << instance << ", pAllocator = " << pAllocator << ')' << " ...";

	// Get function pointer before removing it next
	GET_DISPATCH_PTR(DestroyInstance, instance);
	// Remove instance dispatch table since this instance is being destroyed
	g_instance_dispatch.erase(dispatch_key_from_handle(instance));

	trampoline(instance, pAllocator);
}

VkResult VKAPI_CALL vkCreateWin32SurfaceKHR(VkInstance instance, const VkWin32SurfaceCreateInfoKHR *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkSurfaceKHR *pSurface)
{
	LOG(INFO) << "Redirecting " << "vkCreateWin32SurfaceKHR" << '(' << "instance = " << instance << ", pCreateInfo = " << pCreateInfo << ", pAllocator = " << pAllocator << ", pSurface = " << pSurface << ')' << " ...";

	GET_DISPATCH_PTR(CreateWin32SurfaceKHR, instance);
	const VkResult result = trampoline(instance, pCreateInfo, pAllocator, pSurface);
	if (result != VK_SUCCESS)
	{
		LOG(WARN) << "vkCreateWin32SurfaceKHR" << " failed with error code " << result << '.';
		return result;
	}

	g_surface_windows.emplace(*pSurface, pCreateInfo->hwnd);

	return VK_SUCCESS;
}

void     VKAPI_CALL vkDestroySurfaceKHR(VkInstance instance, VkSurfaceKHR surface, const VkAllocationCallbacks *pAllocator)
{
	LOG(INFO) << "Redirecting " << "vkDestroySurfaceKHR" << '(' << "instance = " << instance << ", surface = " << surface << ", pAllocator = " << pAllocator << ')' << " ...";

	g_surface_windows.erase(surface);

	GET_DISPATCH_PTR(DestroySurfaceKHR, instance);
	trampoline(instance, surface, pAllocator);
}
