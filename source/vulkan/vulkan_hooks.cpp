/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "dll_log.hpp"
#include "hook_manager.hpp"
#include "runtime_vk.hpp"
#include "vk_layer.h"
#include "vk_layer_dispatch_table.h"
#include "format_utils.hpp"
#include "lockfree_table.hpp"

struct device_data
{
	VkPhysicalDevice physical_device;
	VkLayerDispatchTable dispatch_table;
	reshade::vulkan::buffer_detection_context buffer_detection;
	uint32_t graphics_queue_family_index = std::numeric_limits<uint32_t>::max();
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
	reshade::vulkan::buffer_detection buffer_detection;
};

static lockfree_table<void *, device_data, 16> s_device_dispatch;
static lockfree_table<void *, VkLayerInstanceDispatchTable, 16> s_instance_dispatch;
static lockfree_table<VkSurfaceKHR, HWND, 16> s_surface_windows;
static lockfree_table<VkSwapchainKHR, reshade::vulkan::runtime_vk *, 16> s_vulkan_runtimes;
static lockfree_table<VkImage, VkImageCreateInfo, 4096> s_image_data;
static lockfree_table<VkImageView, VkImage, 4096> s_image_view_mapping;
static lockfree_table<VkFramebuffer, std::vector<VkImage>, 256> s_framebuffer_data;
static lockfree_table<VkCommandBuffer, command_buffer_data, 4096> s_command_buffer_data;
static lockfree_table<VkRenderPass, std::vector<render_pass_data>, 4096> s_renderpass_data;

template <typename T>
static T *find_layer_info(const void *structure_chain, VkStructureType type, VkLayerFunction function)
{
	T *next = reinterpret_cast<T *>(const_cast<void *>(structure_chain));
	while (next != nullptr && !(next->sType == type && next->function == function))
		next = reinterpret_cast<T *>(const_cast<void *>(next->pNext));
	return next;
}
template <typename T>
static const T *find_in_structure_chain(const void *structure_chain, VkStructureType type)
{
	const T *next = reinterpret_cast<const T *>(structure_chain);
	while (next != nullptr && next->sType != type)
		next = reinterpret_cast<const T *>(next->pNext);
	return next;
}

static inline void *dispatch_key_from_handle(const void *dispatch_handle)
{
	// The Vulkan loader writes the dispatch table pointer right to the start of the object, so use that as a key for lookup
	// This ensures that all objects of a specific level (device or instance) will use the same dispatch table
	return *(void **)dispatch_handle;
}

#define GET_DEVICE_DISPATCH_PTR(name, object) \
	PFN_vk##name trampoline = s_device_dispatch.at(dispatch_key_from_handle(object)).dispatch_table.name; \
	assert(trampoline != nullptr);
#define GET_INSTANCE_DISPATCH_PTR(name, object) \
	PFN_vk##name trampoline = s_instance_dispatch.at(dispatch_key_from_handle(object)).name; \
	assert(trampoline != nullptr); \

VkResult VKAPI_CALL vkCreateInstance(const VkInstanceCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkInstance *pInstance)
{
	LOG(INFO) << "Redirecting vkCreateInstance" << '(' << "pCreateInfo = " << pCreateInfo << ", pAllocator = " << pAllocator << ", pInstance = " << pInstance << ')' << " ...";

	assert(pCreateInfo != nullptr && pInstance != nullptr);

	// Look for layer link info if installed as a layer (provided by the Vulkan loader)
	VkLayerInstanceCreateInfo *const link_info = find_layer_info<VkLayerInstanceCreateInfo>(
		pCreateInfo->pNext, VK_STRUCTURE_TYPE_LOADER_INSTANCE_CREATE_INFO, VK_LAYER_LINK_INFO);

	// Get trampoline function pointers
	auto gipa = static_cast<PFN_vkGetInstanceProcAddr>(nullptr);
	PFN_vkCreateInstance trampoline = nullptr;

	if (link_info != nullptr)
	{
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
		LOG(WARN) << "vkCreateInstance failed with error code " << result << '!';
		return result;
	}

	VkInstance instance = *pInstance;
	// Initialize the instance dispatch table
	VkLayerInstanceDispatchTable &dispatch_table = s_instance_dispatch.emplace(dispatch_key_from_handle(instance));
	dispatch_table.GetInstanceProcAddr = gipa;
#define INIT_INSTANCE_PROC(name) dispatch_table.name = (PFN_vk##name)gipa(instance, "vk" #name)
	// ---- Core 1_0 commands
	INIT_INSTANCE_PROC(DestroyInstance);
	INIT_INSTANCE_PROC(EnumeratePhysicalDevices);
	INIT_INSTANCE_PROC(GetPhysicalDeviceFormatProperties);
	INIT_INSTANCE_PROC(GetPhysicalDeviceProperties);
	INIT_INSTANCE_PROC(GetPhysicalDeviceMemoryProperties);
	INIT_INSTANCE_PROC(GetPhysicalDeviceQueueFamilyProperties);
	INIT_INSTANCE_PROC(EnumerateDeviceExtensionProperties);
	// ---- Core 1_1 commands
	INIT_INSTANCE_PROC(GetPhysicalDeviceMemoryProperties2);
	// ---- VK_KHR_surface extension commands
	INIT_INSTANCE_PROC(DestroySurfaceKHR);
	// ---- VK_KHR_win32_surface extension commands
	INIT_INSTANCE_PROC(CreateWin32SurfaceKHR);
#undef INIT_INSTANCE_PROC

#if RESHADE_VERBOSE_LOG
	LOG(INFO) << "Returning Vulkan instance " << instance << '.';
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
	VkLayerDeviceCreateInfo *const link_info = find_layer_info<VkLayerDeviceCreateInfo>(
		pCreateInfo->pNext, VK_STRUCTURE_TYPE_LOADER_DEVICE_CREATE_INFO, VK_LAYER_LINK_INFO);

	// Get trampoline function pointers
	auto gdpa = static_cast<PFN_vkGetDeviceProcAddr>(nullptr);
	auto gipa = static_cast<PFN_vkGetInstanceProcAddr>(nullptr);
	PFN_vkCreateDevice trampoline = nullptr;

	if (link_info != nullptr)
	{
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
		LOG(WARN) << "Skipping device because it was not created with the \"" VK_KHR_SWAPCHAIN_EXTENSION_NAME "\" extension.";

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

		// Enable extensions that ReShade requires
		add_extension(VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME, false); // This is optional, see imgui code in 'runtime_vk'
		add_extension(VK_KHR_IMAGE_FORMAT_LIST_EXTENSION_NAME, true);
		add_extension(VK_KHR_SWAPCHAIN_MUTABLE_FORMAT_EXTENSION_NAME, true);
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
		LOG(WARN) << "vkCreateDevice failed with error code " << result << '!';
		return result;
	}

	VkDevice device = *pDevice;
	// Initialize per-device data (safe to access here since nothing else can use it yet)
	auto &device_data = s_device_dispatch.emplace(dispatch_key_from_handle(device));
	device_data.physical_device = physicalDevice;
	device_data.graphics_queue_family_index = graphics_queue_family_index;

	// Initialize the device dispatch table
	VkLayerDispatchTable &dispatch_table = device_data.dispatch_table;
	dispatch_table.GetDeviceProcAddr = gdpa;
#define INIT_DEVICE_PROC(name) dispatch_table.name = (PFN_vk##name)gdpa(device, "vk" #name)
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
	INIT_DEVICE_PROC(CmdCopyBuffer);
	INIT_DEVICE_PROC(CmdCopyImage);
	INIT_DEVICE_PROC(CmdBlitImage);
	INIT_DEVICE_PROC(CmdCopyBufferToImage);
	INIT_DEVICE_PROC(CmdCopyImageToBuffer);
	INIT_DEVICE_PROC(CmdUpdateBuffer);
	INIT_DEVICE_PROC(CmdClearDepthStencilImage);
	INIT_DEVICE_PROC(CmdClearAttachments);
	INIT_DEVICE_PROC(CmdPipelineBarrier);
	INIT_DEVICE_PROC(CmdResetQueryPool);
	INIT_DEVICE_PROC(CmdWriteTimestamp);
	INIT_DEVICE_PROC(CmdPushConstants);
	INIT_DEVICE_PROC(CmdBeginRenderPass);
	INIT_DEVICE_PROC(CmdEndRenderPass);
	INIT_DEVICE_PROC(CmdExecuteCommands);
	// ---- Core 1_1 commands
	INIT_DEVICE_PROC(BindBufferMemory2);
	INIT_DEVICE_PROC(BindImageMemory2);
	INIT_DEVICE_PROC(GetBufferMemoryRequirements2);
	INIT_DEVICE_PROC(GetImageMemoryRequirements2);
	INIT_DEVICE_PROC(GetDeviceQueue2);
	// ---- VK_KHR_swapchain extension commands
	INIT_DEVICE_PROC(CreateSwapchainKHR);
	INIT_DEVICE_PROC(DestroySwapchainKHR);
	INIT_DEVICE_PROC(GetSwapchainImagesKHR);
	INIT_DEVICE_PROC(QueuePresentKHR);
	// ---- VK_KHR_push_descriptor extension commands
	INIT_DEVICE_PROC(CmdPushDescriptorSetKHR);
	// ---- VK_EXT_debug_marker extension commands
	INIT_DEVICE_PROC(DebugMarkerSetObjectNameEXT);
#undef INIT_DEVICE_PROC

#if RESHADE_VERBOSE_LOG
	LOG(INFO) << "Returning Vulkan device " << device << '.';
#endif
	return VK_SUCCESS;
}
void     VKAPI_CALL vkDestroyDevice(VkDevice device, const VkAllocationCallbacks *pAllocator)
{
	LOG(INFO) << "Redirecting vkDestroyDevice" << '(' << "device = " << device << ", pAllocator = " << pAllocator << ')' << " ...";

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

	auto &device_data = s_device_dispatch.at(dispatch_key_from_handle(device));

	VkSwapchainCreateInfoKHR create_info = *pCreateInfo;
	VkImageFormatListCreateInfoKHR format_list_info { VK_STRUCTURE_TYPE_IMAGE_FORMAT_LIST_CREATE_INFO_KHR };
	std::vector<VkFormat> format_list; std::vector<uint32_t> queue_family_list;

	// Only have to enable additional features if there is a graphics queue, since ReShade will not run otherwise
	if (device_data.graphics_queue_family_index != std::numeric_limits<uint32_t>::max())
	{
		// Add required usage flags to create info
		create_info.imageUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

		// Add required formats, so views with different formats can be created for the swapchain images
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
		LOG(WARN) << "vkCreateSwapchainKHR failed with error code " << result << '!';
		return result;
	}

	if (device_data.graphics_queue_family_index != std::numeric_limits<uint32_t>::max())
	{
		reshade::vulkan::runtime_vk *runtime;
		// Remove old swapchain from the list so that a call to 'vkDestroySwapchainKHR' won't reset the runtime again
		if (s_vulkan_runtimes.erase(pCreateInfo->oldSwapchain, runtime))
		{
			assert(pCreateInfo->oldSwapchain != VK_NULL_HANDLE);

			// Re-use the existing runtime if this swapchain was not created from scratch
			runtime->on_reset(); // But reset it before initializing again below
		}
		else
		{
			runtime = new reshade::vulkan::runtime_vk(
				device, device_data.physical_device, device_data.graphics_queue_family_index,
				s_instance_dispatch.at(dispatch_key_from_handle(device_data.physical_device)), device_data.dispatch_table);

			runtime->_buffer_detection = &device_data.buffer_detection;
		}

		// Look up window handle from surface
		const HWND hwnd = s_surface_windows.at(pCreateInfo->surface);

		if (!runtime->on_init(*pSwapchain, *pCreateInfo, hwnd))
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
	LOG(INFO) << "Redirecting vkDestroySwapchainKHR" << '(' << device << ", " << swapchain << ", " << pAllocator << ')' << " ...";

	// Remove runtime from global list
	if (reshade::vulkan::runtime_vk *runtime;
		s_vulkan_runtimes.erase(swapchain, runtime) && runtime != nullptr)
	{
		runtime->on_reset();

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
			device_data.buffer_detection.merge(command_buffer_data.buffer_detection);
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
		{
			VkSemaphore signal = VK_NULL_HANDLE;
			if (runtime->on_present(pPresentInfo->pImageIndices[i], wait_semaphores.data(), static_cast<uint32_t>(wait_semaphores.size()), signal); signal != VK_NULL_HANDLE)
			{
				// The queue submit in 'on_present' now waits on the requested wait semaphores
				// The next queue submit should therefore wait on the semaphore that was signaled by the last 'on_present' submit
				// This effectively builds a linear chain of submissions that each wait on the previous
				wait_semaphores.clear();
				wait_semaphores.push_back(signal);
			}
		}
	}

	device_data.buffer_detection.reset();

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
		LOG(WARN) << "vkCreateImage failed with error code " << result << '!';
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
		LOG(WARN) << "vkCreateImageView failed with error code " << result << '!';
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
		LOG(WARN) << "vkCreateRenderPass failed with error code " << result << '!';
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

#if RESHADE_DEPTH
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
