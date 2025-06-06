/*
 * Copyright (C) 2014 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause OR MIT
 */

#include "vulkan_hooks.hpp"
#include "dll_log.hpp"
#include "hook_manager.hpp"
#include "addon_manager.hpp"
#include "lockfree_linear_map.hpp"
#include <cstring> // std::strncmp, std::strncpy
#include <algorithm> // std::find_if

lockfree_linear_map<void *, instance_dispatch_table, 16> g_vulkan_instances;
lockfree_linear_map<VkSurfaceKHR, HWND, 16> g_vulkan_surface_windows;

#define GET_DISPATCH_PTR(name, object) \
	PFN_vk##name trampoline = g_vulkan_instances.at(dispatch_key_from_handle(object)).name; \
	assert(trampoline != nullptr)
#define INIT_DISPATCH_PTR(name) \
	dispatch_table.name = reinterpret_cast<PFN_vk##name>(get_instance_proc(instance, "vk" #name))

VkResult VKAPI_CALL vkCreateInstance(const VkInstanceCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkInstance *pInstance)
{
	reshade::log::message(reshade::log::level::info, "Redirecting vkCreateInstance(pCreateInfo = %p, pAllocator = %p, pInstance = %p) ...", pCreateInfo, pAllocator, pInstance);

	assert(pCreateInfo != nullptr && pInstance != nullptr);

	// Look for layer link info if installed as a layer (provided by the Vulkan loader)
	VkLayerInstanceCreateInfo *const link_info = find_layer_info<VkLayerInstanceCreateInfo>(pCreateInfo->pNext, VK_STRUCTURE_TYPE_LOADER_INSTANCE_CREATE_INFO, VK_LAYER_LINK_INFO);

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

	if (trampoline == nullptr || get_instance_proc == nullptr) // Unable to resolve next 'vkCreateInstance' function in the call chain
		return VK_ERROR_INITIALIZATION_FAILED;

	reshade::log::message(reshade::log::level::info, "> Dumping enabled instance extensions:");
	for (uint32_t i = 0; i < pCreateInfo->enabledExtensionCount; ++i)
		reshade::log::message(reshade::log::level::info, "  %s", pCreateInfo->ppEnabledExtensionNames[i]);

	VkApplicationInfo app_info { VK_STRUCTURE_TYPE_APPLICATION_INFO };
	if (pCreateInfo->pApplicationInfo != nullptr)
		app_info = *pCreateInfo->pApplicationInfo;

#if RESHADE_ADDON >= 2
	reshade::load_addons();

	uint32_t api_version =
		VK_API_VERSION_MAJOR(app_info.apiVersion) << 12 |
		VK_API_VERSION_MINOR(app_info.apiVersion) <<  8;
	if (reshade::invoke_addon_event<reshade::addon_event::create_device>(reshade::api::device_api::vulkan, api_version))
	{
		app_info.apiVersion = VK_MAKE_API_VERSION(0, (api_version >> 12) & 0xF, (api_version >> 8) & 0xF, api_version & 0xFF);
	}
#endif

	reshade::log::message(reshade::log::level::info, "Requesting new Vulkan instance for API version %u.%u.", VK_API_VERSION_MAJOR(app_info.apiVersion), VK_API_VERSION_MINOR(app_info.apiVersion));

	// ReShade requires at least Vulkan 1.1 (for SPIR-V 1.3 compatibility)
	if (app_info.apiVersion < VK_API_VERSION_1_1)
	{
		reshade::log::message(reshade::log::level::info, "> Replacing requested version with 1.1.");

		app_info.apiVersion = VK_API_VERSION_1_1;
	}

	// 'vkEnumerateInstanceExtensionProperties' is not included in the next 'vkGetInstanceProcAddr' from the call chain, so use global one instead
	const auto vulkan_module = GetModuleHandleW(L"vulkan-1.dll");
	assert(vulkan_module != nullptr);
	const auto enum_instance_extensions = reinterpret_cast<PFN_vkEnumerateInstanceExtensionProperties>(GetProcAddress(vulkan_module, "vkEnumerateInstanceExtensionProperties"));
	assert(enum_instance_extensions != nullptr);

	std::vector<const char *> enabled_extensions;
	enabled_extensions.reserve(pCreateInfo->enabledExtensionCount);
	for (uint32_t i = 0; i < pCreateInfo->enabledExtensionCount; ++i)
		enabled_extensions.push_back(pCreateInfo->ppEnabledExtensionNames[i]);

	if (enum_instance_extensions != nullptr)
	{
		uint32_t num_extensions = 0;
		enum_instance_extensions(nullptr, &num_extensions, nullptr);
		std::vector<VkExtensionProperties> extensions(num_extensions);
		enum_instance_extensions(nullptr, &num_extensions, extensions.data());

		// Make sure the driver actually supports the requested extensions
		const auto add_extension = [&extensions, &enabled_extensions](const char *name, bool required) {
			if (const auto it = std::find_if(extensions.begin(), extensions.end(),
					[name](const auto &props) { return std::strncmp(props.extensionName, name, VK_MAX_EXTENSION_NAME_SIZE) == 0; });
				it != extensions.end())
			{
				enabled_extensions.push_back(name);
				return true;
			}

			if (required)
			{
				reshade::log::message(reshade::log::level::error, "Required extension \"%s\" is not supported on this instance. Initialization failed.", name);
			}
			else
			{
				reshade::log::message(reshade::log::level::warning, "Optional extension \"%s\" is not supported on this instance.", name);
			}

			return false;
		};

		// Enable extensions that ReShade requires
		add_extension(VK_EXT_DEBUG_UTILS_EXTENSION_NAME, false);
	}

	VkInstanceCreateInfo create_info = *pCreateInfo;
	create_info.pApplicationInfo = &app_info;
	create_info.enabledExtensionCount = static_cast<uint32_t>(enabled_extensions.size());
	create_info.ppEnabledExtensionNames = enabled_extensions.data();

	// Continue calling down the chain
	const VkResult result = trampoline(&create_info, pAllocator, pInstance);
	if (result != VK_SUCCESS)
	{
#if RESHADE_ADDON >= 2
		reshade::unload_addons();
#endif
		reshade::log::message(reshade::log::level::warning, "vkCreateInstance failed with error code %d.", static_cast<int>(result));
		return result;
	}

	VkInstance instance = *pInstance;
	// Initialize the instance dispatch table
	VkLayerInstanceDispatchTable dispatch_table = {};
	dispatch_table.GetInstanceProcAddr = get_instance_proc;
	dispatch_table.GetPhysicalDeviceProcAddr = get_physical_device_proc;

	// Core 1_0
	INIT_DISPATCH_PTR(DestroyInstance);
	INIT_DISPATCH_PTR(EnumeratePhysicalDevices);
	INIT_DISPATCH_PTR(GetPhysicalDeviceFeatures);
	INIT_DISPATCH_PTR(GetPhysicalDeviceFormatProperties);
	INIT_DISPATCH_PTR(GetPhysicalDeviceProperties);
	INIT_DISPATCH_PTR(GetPhysicalDeviceMemoryProperties);
	INIT_DISPATCH_PTR(GetPhysicalDeviceQueueFamilyProperties);
	INIT_DISPATCH_PTR(EnumerateDeviceExtensionProperties);

	// Core 1_1
	INIT_DISPATCH_PTR(GetPhysicalDeviceProperties2);
	INIT_DISPATCH_PTR(GetPhysicalDeviceMemoryProperties2);
	INIT_DISPATCH_PTR(GetPhysicalDeviceExternalBufferProperties);
	INIT_DISPATCH_PTR(GetPhysicalDeviceExternalSemaphoreProperties);

	// Core 1_3
	INIT_DISPATCH_PTR(GetPhysicalDeviceToolProperties);

	// VK_KHR_surface
	INIT_DISPATCH_PTR(DestroySurfaceKHR);

	// VK_KHR_win32_surface
	INIT_DISPATCH_PTR(CreateWin32SurfaceKHR);

	// VK_EXT_tooling_info
	INIT_DISPATCH_PTR(GetPhysicalDeviceToolPropertiesEXT);

	g_vulkan_instances.emplace(dispatch_key_from_handle(instance), instance_dispatch_table { dispatch_table, instance, app_info.apiVersion });

#if RESHADE_VERBOSE_LOG
	reshade::log::message(reshade::log::level::debug, "Returning Vulkan instance %p.", instance);
#endif
	return VK_SUCCESS;
}

void     VKAPI_CALL vkDestroyInstance(VkInstance instance, const VkAllocationCallbacks *pAllocator)
{
	reshade::log::message(reshade::log::level::info, "Redirecting vkDestroyInstance(instance = %p, pAllocator = %p) ...", instance, pAllocator);

	// Get function pointer before removing it next
	GET_DISPATCH_PTR(DestroyInstance, instance);

	// Remove instance dispatch table since this instance is being destroyed
	g_vulkan_instances.erase(dispatch_key_from_handle(instance));

#if RESHADE_ADDON >= 2
	reshade::unload_addons();
#endif

	trampoline(instance, pAllocator);
}

VkResult VKAPI_CALL vkCreateWin32SurfaceKHR(VkInstance instance, const VkWin32SurfaceCreateInfoKHR *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkSurfaceKHR *pSurface)
{
	reshade::log::message(reshade::log::level::info, "Redirecting vkCreateWin32SurfaceKHR(instance = %p, pCreateInfo = %p, pAllocator = %p, pSurface = %p) ...", instance, pCreateInfo, pAllocator, pSurface);

	GET_DISPATCH_PTR(CreateWin32SurfaceKHR, instance);
	const VkResult result = trampoline(instance, pCreateInfo, pAllocator, pSurface);
	if (result != VK_SUCCESS)
	{
		reshade::log::message(reshade::log::level::warning, "vkCreateWin32SurfaceKHR failed with error code %d.", static_cast<int>(result));
		return result;
	}

	g_vulkan_surface_windows.emplace(*pSurface, pCreateInfo->hwnd);

	return VK_SUCCESS;
}

void     VKAPI_CALL vkDestroySurfaceKHR(VkInstance instance, VkSurfaceKHR surface, const VkAllocationCallbacks *pAllocator)
{
	reshade::log::message(reshade::log::level::info, "Redirecting vkDestroySurfaceKHR(instance = %p, surface = %p, pAllocator = %) ...", instance, surface, pAllocator);

	g_vulkan_surface_windows.erase(surface);

	GET_DISPATCH_PTR(DestroySurfaceKHR, instance);
	trampoline(instance, surface, pAllocator);
}

#include "version.h"

static VkResult get_physical_device_tool_properties(VkPhysicalDevice physicalDevice, uint32_t *pToolCount, VkPhysicalDeviceToolProperties *pToolProperties, VkResult(VKAPI_CALL *trampoline)(VkPhysicalDevice, uint32_t *, VkPhysicalDeviceToolProperties *))
{
	assert(pToolCount != nullptr);

	VkResult result = VK_SUCCESS;
	const uint32_t available_tool_count = *pToolCount;

	// First get any tools that are downstream (provided this extension is supported downstream as well)
	if (trampoline != nullptr)
		result = trampoline(physicalDevice, pToolCount, pToolProperties); // Sets the number written in "pToolCount"
	else
		*pToolCount = 0;

	// Check if application is only enumerating and increase reported tool count in that case
	if (pToolProperties == nullptr)
	{
		*pToolCount += 1;
		return result;
	}

	if (available_tool_count < *pToolCount + 1 && result >= VK_SUCCESS)
		result = VK_INCOMPLETE;
	if (VK_SUCCESS != result)
		return result;

	VkPhysicalDeviceToolPropertiesEXT &tool_props = pToolProperties[(*pToolCount)++];
	std::strncpy(tool_props.name, "ReShade", VK_MAX_EXTENSION_NAME_SIZE);
	std::strncpy(tool_props.version, VERSION_STRING_PRODUCT, VK_MAX_EXTENSION_NAME_SIZE);
	tool_props.purposes = VK_TOOL_PURPOSE_ADDITIONAL_FEATURES_BIT_EXT | VK_TOOL_PURPOSE_MODIFYING_FEATURES_BIT_EXT;
	std::strncpy(tool_props.description, "crosire's ReShade post-processing injector", VK_MAX_DESCRIPTION_SIZE);
	std::strncpy(tool_props.layer, "VK_LAYER_reshade", VK_MAX_EXTENSION_NAME_SIZE);

	return VK_SUCCESS;
}

VkResult VKAPI_CALL vkGetPhysicalDeviceToolProperties(VkPhysicalDevice physicalDevice, uint32_t *pToolCount, VkPhysicalDeviceToolProperties *pToolProperties)
{
	GET_DISPATCH_PTR(GetPhysicalDeviceToolProperties, physicalDevice);

	return get_physical_device_tool_properties(physicalDevice, pToolCount, pToolProperties, trampoline);
}
VkResult VKAPI_CALL vkGetPhysicalDeviceToolPropertiesEXT(VkPhysicalDevice physicalDevice, uint32_t *pToolCount, VkPhysicalDeviceToolPropertiesEXT *pToolProperties)
{
	GET_DISPATCH_PTR(GetPhysicalDeviceToolPropertiesEXT, physicalDevice);

	return get_physical_device_tool_properties(physicalDevice, pToolCount, pToolProperties, trampoline);
}
