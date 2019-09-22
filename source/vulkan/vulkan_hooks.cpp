/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "log.hpp"
#include "hook_manager.hpp"
#include "runtime_vk.hpp"
#include "vk_layer.h"
#include "vk_layer_dispatch_table.h"
#include <mutex>
#include <memory>
#include <unordered_map>

struct image_data
{
	VkImageCreateInfo image_info;
	VkDevice device;
};

struct image_view_data
{
	VkImage image;
	VkImageViewCreateInfo image_view_info;
	image_data image_data;
	VkImageView replacement;
};

struct attachment_data
{
	VkImageView imageView;
	image_view_data image_view_data;
};

static std::mutex s_mutex;
static std::mutex s_trackers_per_command_buffer_mutex;
static std::unordered_map<void *, VkLayerDispatchTable> s_command_buffer_dispatch;
static std::unordered_map<void *, VkLayerDispatchTable> s_device_dispatch;
static std::unordered_map<void *, VkLayerInstanceDispatchTable> s_instance_dispatch;
static std::unordered_map<VkSurfaceKHR, HWND> s_surface_windows;
static std::unordered_map<VkDevice, VkPhysicalDevice> s_device_mapping;
static std::unordered_map<VkDevice, reshade::vulkan::draw_call_tracker> s_device_tracker;
static std::unordered_map<VkCommandBuffer, reshade::vulkan::draw_call_tracker> s_trackers_per_command_buffer;
static std::unordered_map<VkCommandBuffer, VkFramebuffer> s_framebuffer_per_command_buffer;
static std::unordered_map<VkPhysicalDevice, VkInstance> s_instance_mapping;
static std::unordered_map<VkQueue, VkDevice> s_queue_mapping;
static std::unordered_map<VkCommandBuffer, VkDevice> s_command_buffer_mapping;
static std::unordered_map<VkDevice, std::shared_ptr<reshade::vulkan::runtime_vk>> s_device_runtimes;
static std::unordered_map<VkSwapchainKHR, std::shared_ptr<reshade::vulkan::runtime_vk>> s_runtimes;
static std::unordered_map<VkImage, image_data> s_depth_stencil_buffer_images;
static std::unordered_map<VkImageView, image_view_data> s_depth_stencil_buffer_image_views;
static std::unordered_map<VkFramebuffer, attachment_data> s_depth_stencil_buffer_frameBuffers;

inline void *get_dispatch_key(const void *dispatchable_handle)
{
	return *(void **)dispatchable_handle;
}

static void add_command_buffer_trackers(VkCommandBuffer command_buffer, reshade::vulkan::draw_call_tracker &tracker_source)
{
	assert(command_buffer != VK_NULL_HANDLE);

	const std::lock_guard<std::mutex> lock(s_trackers_per_command_buffer_mutex);

	// Merges the counters in counters_source in the s_counters_per_command_buffer for the command buffer specified
	const auto it = s_trackers_per_command_buffer.find(command_buffer);
	if (it == s_trackers_per_command_buffer.end())
		s_trackers_per_command_buffer.emplace(command_buffer, tracker_source);
	else
		it->second.merge(tracker_source);
}
static void merge_command_buffer_trackers(VkCommandBuffer command_buffer, reshade::vulkan::draw_call_tracker &tracker_destination)
{
	assert(command_buffer != VK_NULL_HANDLE);

	const std::lock_guard<std::mutex> lock(s_trackers_per_command_buffer_mutex);

	// Merges the counters logged for the specified command buffer in the counters destination tracker specified
	const auto it = s_trackers_per_command_buffer.find(command_buffer);
	if (it != s_trackers_per_command_buffer.end())
		tracker_destination.merge(it->second);

	it->second.reset();
}

#if RESHADE_VULKAN_CAPTURE_DEPTH_BUFFERS
static bool save_depth_image(VkCommandBuffer, VkDevice device, reshade::vulkan::draw_call_tracker &draw_call_tracker, VkImageView pDepthStencilView, image_view_data imageViewData, bool cleared)
{
	if (const auto it = s_device_runtimes.find(device); it == s_device_runtimes.end())
		return false;

	std::shared_ptr<reshade::vulkan::runtime_vk> runtime = s_device_runtimes.at(device);

	if (!runtime->depth_buffer_before_clear)
		return false;
	if (!cleared && !runtime->extended_depth_buffer_detection)
		return false;

	VkImage depthstencil = imageViewData.image;

	assert(pDepthStencilView != VK_NULL_HANDLE);

	// Check if aspect ratio is similar to the back buffer one
	const float width_factor = float(runtime->frame_width()) / float(imageViewData.image_data.image_info.extent.width);
	const float height_factor = float(runtime->frame_height()) / float(imageViewData.image_data.image_info.extent.height);
	const float aspect_ratio = float(runtime->frame_width()) / float(runtime->frame_height());
	const float texture_aspect_ratio = float(imageViewData.image_data.image_info.extent.width) / float(imageViewData.image_data.image_info.extent.height);

	if (fabs(texture_aspect_ratio - aspect_ratio) > 0.1f || width_factor > 1.85f || height_factor > 1.85f || width_factor < 0.5f || height_factor < 0.5f)
		return false; // No match, not a good fit

	// In case the depth texture is retrieved, we make a copy of it and store it in an ordered map to use it later in the final rendering stage.
	// TODO: revert to this test when bugs will be fixed and depth buffer swithc will no longer require reloading of the effects
	// if ((runtime->cleared_depth_buffer_index == 0 && cleared) || (runtime->clear_DSV_iter <= runtime->cleared_depth_buffer_index))
	if (runtime->clear_DSV_iter == runtime->cleared_depth_buffer_index)
	{
		VkDevice imageDevice = imageViewData.image_data.device;
		VkImageView newdepthstencilView = VK_NULL_HANDLE;
		const VkCommandBuffer cmd_list = runtime->create_command_list();
		// Select an appropriate destination texture
		// VkImage depth_image_save = runtime->select_depth_image_save(imageViewData.image_data.image_info);
		VkImage depth_image_save;
		s_device_dispatch.at(get_dispatch_key(imageDevice)).CreateImage(imageDevice, &imageViewData.image_data.image_info, nullptr, &depth_image_save);
		if (depth_image_save == VK_NULL_HANDLE)
			return false;

		s_device_dispatch.at(get_dispatch_key(imageDevice)).CreateImageView(imageDevice, &imageViewData.image_view_info, nullptr, &newdepthstencilView);

		// Save depth buffer
		const VkImageCopy copy_range = {
			{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 }, { 0, 0, 0 },
			{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 }, { 0, 0, 0 }, { imageViewData.image_data.image_info.extent.width, imageViewData.image_data.image_info.extent.height, 1 }
		};

		// Copy the depth texture. This is necessary because the content of the depth texture is cleared.
		// This way, we can retrieve this content in the final rendering stage
		// TODO: keep_track of the device
		runtime->transition_layout(cmd_list, depth_image_save, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
		runtime->transition_layout(cmd_list, depthstencil, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
		s_device_dispatch.at(get_dispatch_key(imageDevice)).CmdCopyImage(cmd_list, depthstencil, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, depth_image_save, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy_range);
		runtime->transition_layout(cmd_list, depth_image_save, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		runtime->transition_layout(cmd_list, depthstencil, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

		runtime->execute_command_list_async(cmd_list);

		// Store the saved texture in the ordered map.
		draw_call_tracker.track_depth_image(runtime->depth_buffer_texture_format, runtime->clear_DSV_iter, depthstencil, imageViewData.image_data.image_info, newdepthstencilView, imageViewData.image_view_info, depth_image_save, cleared);
	}
	else
	{
		// Store a null depth texture in the ordered map in order to display it even if the user chose a previous cleared texture.
		// This way the texture will still be visible in the depth buffer selection window and the user can choose it.
		draw_call_tracker.track_depth_image(runtime->depth_buffer_texture_format, runtime->clear_DSV_iter, depthstencil, imageViewData.image_data.image_info, pDepthStencilView, imageViewData.image_view_info, VK_NULL_HANDLE, cleared);
	}

	runtime->clear_DSV_iter++;
	return true;
}
static void track_active_renderpass(VkCommandBuffer commandBuffer, VkDevice device, attachment_data attachmentData)
{
	if (const auto it = s_device_runtimes.find(device); it == s_device_runtimes.end())
		return;

	std::shared_ptr<reshade::vulkan::runtime_vk> runtime = s_device_runtimes.at(device);

	// Check if aspect ratio is similar to the back buffer one
	const float width_factor = float(runtime->frame_width()) / float(attachmentData.image_view_data.image_data.image_info.extent.width);
	const float height_factor = float(runtime->frame_height()) / float(attachmentData.image_view_data.image_data.image_info.extent.height);
	const float aspect_ratio = float(runtime->frame_width()) / float(runtime->frame_height());
	const float texture_aspect_ratio = float(attachmentData.image_view_data.image_data.image_info.extent.width) / float(attachmentData.image_view_data.image_data.image_info.extent.height);

	// TODO: restore this dimension filter when all is OK
	// if (fabs(texture_aspect_ratio - aspect_ratio) > 0.1f || width_factor > 1.85f || height_factor > 1.85f || width_factor < 0.5f || height_factor < 0.5f)
		// return; // No match, not a good fit

	{ const std::lock_guard<std::mutex> lock(s_trackers_per_command_buffer_mutex);
		reshade::vulkan::draw_call_tracker *command_buffer_tracker = &s_trackers_per_command_buffer.at(commandBuffer);
		command_buffer_tracker->track_renderpasses(runtime->depth_buffer_texture_format, attachmentData.imageView, attachmentData.image_view_data.image, attachmentData.image_view_data.image_data.image_info, attachmentData.image_view_data.image_view_info, attachmentData.image_view_data.replacement);

		save_depth_image(commandBuffer, device, *command_buffer_tracker, attachmentData.imageView, attachmentData.image_view_data, false);

		command_buffer_tracker->_depthstencil = attachmentData.imageView;
	}
}
static void track_cleared_depthstencil(VkCommandBuffer commandBuffer, VkDevice device, reshade::vulkan::draw_call_tracker &draw_call_tracker, VkImageView pDepthStencilView, image_view_data imageViewData)
{
	if (const auto it = s_device_runtimes.find(device); it == s_device_runtimes.end())
		return;

	std::shared_ptr<reshade::vulkan::runtime_vk> runtime = s_device_runtimes.at(device);

	save_depth_image(commandBuffer, device, draw_call_tracker, pDepthStencilView, imageViewData, true);
}
#endif

VkResult VKAPI_CALL vkCreateInstance(const VkInstanceCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkInstance *pInstance)
{
	PFN_vkCreateInstance trampoline = nullptr;
	PFN_vkGetInstanceProcAddr get_instance_proc = nullptr;

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
		trampoline = (PFN_vkCreateInstance)get_instance_proc(nullptr, "vkCreateInstance");

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

	// Continue call down the chain
	const VkResult result = trampoline(pCreateInfo, pAllocator, pInstance);
	if (result != VK_SUCCESS)
	{
		LOG(WARN) << "> vkCreateInstance failed with error code " << result << '!';
		return result;
	}

	VkInstance instance = *pInstance;
	// Initialize the instance dispatch table
	VkLayerInstanceDispatchTable dispatch_table = {};
	dispatch_table.GetInstanceProcAddr = get_instance_proc;
	dispatch_table.DestroyInstance = (PFN_vkDestroyInstance)get_instance_proc(instance, "vkDestroyInstance");
	dispatch_table.EnumeratePhysicalDevices = (PFN_vkEnumeratePhysicalDevices)get_instance_proc(instance, "vkEnumeratePhysicalDevices");
	dispatch_table.CreateWin32SurfaceKHR = (PFN_vkCreateWin32SurfaceKHR)get_instance_proc(instance, "vkCreateWin32SurfaceKHR");
	dispatch_table.DestroySurfaceKHR = (PFN_vkDestroySurfaceKHR)get_instance_proc(instance, "vkDestroySurfaceKHR");
	dispatch_table.GetPhysicalDeviceMemoryProperties = (PFN_vkGetPhysicalDeviceMemoryProperties)get_instance_proc(instance, "vkGetPhysicalDeviceMemoryProperties");

	{ const std::lock_guard<std::mutex> lock(s_mutex);
		s_instance_dispatch[get_dispatch_key(instance)] = dispatch_table;
	}

	return VK_SUCCESS;
}
void     VKAPI_CALL vkDestroyInstance(VkInstance instance, const VkAllocationCallbacks *pAllocator)
{
	PFN_vkDestroyInstance trampoline;

	LOG(INFO) << "Redirecting vkDestroyInstance" << '(' << instance << ", " << pAllocator << ')' << " ...";

	{ const std::lock_guard<std::mutex> lock(s_mutex);
		trampoline = s_instance_dispatch.at(get_dispatch_key(instance)).DestroyInstance;
		assert(trampoline != nullptr);

		// Remove instance dispatch table since this instance is being destroyed
		s_instance_dispatch.erase(get_dispatch_key(instance));
	}

	trampoline(instance, pAllocator);
}

VkResult VKAPI_CALL vkEnumeratePhysicalDevices(VkInstance instance, uint32_t *pPhysicalDeviceCount, VkPhysicalDevice *pPhysicalDevices)
{
	PFN_vkEnumeratePhysicalDevices trampoline;

	LOG(INFO) << "Redirecting vkEnumeratePhysicalDevices" << '(' << instance << ", " << pPhysicalDeviceCount << ", " << pPhysicalDevices << ')' << " ...";

	{ const std::lock_guard<std::mutex> lock(s_mutex);
		trampoline = s_instance_dispatch.at(get_dispatch_key(instance)).EnumeratePhysicalDevices;
		assert(trampoline != nullptr);
	}

	const VkResult result = trampoline(instance, pPhysicalDeviceCount, pPhysicalDevices);
	if (result != VK_SUCCESS && result != VK_INCOMPLETE)
	{
		LOG(WARN) << "> vkEnumeratePhysicalDevices failed with error code " << result << '!';
		return result;
	}

	if (pPhysicalDevices != nullptr)
	{
		const std::lock_guard<std::mutex> lock(s_mutex);
		for (uint32_t i = 0; i < *pPhysicalDeviceCount; ++i)
			s_instance_mapping[pPhysicalDevices[i]] = instance;
	}

	return result;
}

VkResult VKAPI_CALL vkCreateWin32SurfaceKHR(VkInstance instance, const VkWin32SurfaceCreateInfoKHR *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkSurfaceKHR *pSurface)
{
	PFN_vkCreateWin32SurfaceKHR trampoline;

	LOG(INFO) << "Redirecting vkCreateWin32SurfaceKHR" << '(' << instance << ", " << pCreateInfo << ", " << pAllocator << ", " << pSurface << ')' << " ...";

	{ const std::lock_guard<std::mutex> lock(s_mutex);
		trampoline = s_instance_dispatch.at(get_dispatch_key(instance)).CreateWin32SurfaceKHR;
		assert(trampoline != nullptr);
	}

	const VkResult result = trampoline(instance, pCreateInfo, pAllocator, pSurface);
	if (result != VK_SUCCESS)
	{
		LOG(WARN) << "> vkCreateWin32SurfaceKHR failed with error code " << result << '!';
		return result;
	}

	{ const std::lock_guard<std::mutex> lock(s_mutex);
		s_surface_windows[*pSurface] = pCreateInfo->hwnd;
	}

	return VK_SUCCESS;
}
void     VKAPI_CALL vkDestroySurfaceKHR(VkInstance instance, VkSurfaceKHR surface, const VkAllocationCallbacks *pAllocator)
{
	PFN_vkDestroySurfaceKHR trampoline;

	LOG(INFO) << "Redirecting vkDestroySurfaceKHR" << '(' << instance << ", " << surface << ", " << pAllocator << ')' << " ...";

	{ const std::lock_guard<std::mutex> lock(s_mutex);
		trampoline = s_instance_dispatch.at(get_dispatch_key(instance)).DestroySurfaceKHR;
		assert(trampoline != nullptr);

		// Remove surface since this surface is being destroyed
		s_surface_windows.erase(surface);
	}

	trampoline(instance, surface, pAllocator);
}

VkResult VKAPI_CALL vkCreateDevice(VkPhysicalDevice physicalDevice, const VkDeviceCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkDevice *pDevice)
{
	PFN_vkCreateDevice trampoline = nullptr;
	PFN_vkGetDeviceProcAddr get_device_proc = nullptr;
	PFN_vkGetInstanceProcAddr get_instance_proc = nullptr;

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
		trampoline = (PFN_vkCreateDevice)get_instance_proc(nullptr, "vkCreateDevice");

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
	const VkResult result = trampoline(physicalDevice, &create_info, pAllocator, pDevice);
	if (result != VK_SUCCESS)
	{
		LOG(WARN) << "> vkCreateDevice failed with error code " << result << '!';
		return result;
	}

	VkDevice device = *pDevice;
	// Initialize the device dispatch table
	VkLayerDispatchTable dispatch_table = {};
	dispatch_table.GetDeviceProcAddr = get_device_proc;
	dispatch_table.DestroyDevice = (PFN_vkDestroyDevice)get_device_proc(device, "vkDestroyDevice");
	dispatch_table.GetDeviceQueue = (PFN_vkGetDeviceQueue)get_device_proc(device, "vkGetDeviceQueue");
	dispatch_table.GetDeviceQueue2 = (PFN_vkGetDeviceQueue2)get_device_proc(device, "vkGetDeviceQueue2");
	dispatch_table.CreateSwapchainKHR = (PFN_vkCreateSwapchainKHR)get_device_proc(device, "vkCreateSwapchainKHR");
	dispatch_table.DestroySwapchainKHR = (PFN_vkDestroySwapchainKHR)get_device_proc(device, "vkDestroySwapchainKHR");
	dispatch_table.QueuePresentKHR = (PFN_vkQueuePresentKHR)get_device_proc(device, "vkQueuePresentKHR");
	dispatch_table.QueueWaitIdle = (PFN_vkQueueWaitIdle)get_device_proc(device, "vkQueueWaitIdle");
	dispatch_table.DeviceWaitIdle = (PFN_vkDeviceWaitIdle)get_device_proc(device, "vkDeviceWaitIdle");
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
	dispatch_table.CmdClearAttachments = (PFN_vkCmdClearAttachments)get_device_proc(device, "vkCmdClearAttachments");
	dispatch_table.CmdEndRenderPass = (PFN_vkCmdEndRenderPass)get_device_proc(device, "vkCmdEndRenderPass");
	dispatch_table.CmdBeginRenderPass = (PFN_vkCmdBeginRenderPass)get_device_proc(device, "vkCmdBeginRenderPass");
	dispatch_table.CmdBindPipeline = (PFN_vkCmdBindPipeline)get_device_proc(device, "vkCmdBindPipeline");
	dispatch_table.CmdBindIndexBuffer = (PFN_vkCmdBindIndexBuffer)get_device_proc(device, "vkCmdBindIndexBuffer");
	dispatch_table.CmdBindVertexBuffers = (PFN_vkCmdBindVertexBuffers)get_device_proc(device, "vkCmdBindVertexBuffers");
	dispatch_table.CmdSetScissor = (PFN_vkCmdSetScissor)get_device_proc(device, "vkCmdSetScissor");
	dispatch_table.CmdSetViewport = (PFN_vkCmdSetViewport)get_device_proc(device, "vkCmdSetViewport");
	dispatch_table.CmdDraw = (PFN_vkCmdDraw)get_device_proc(device, "vkCmdDraw");
	dispatch_table.CmdDrawIndexed = (PFN_vkCmdDrawIndexed)get_device_proc(device, "vkCmdDrawIndexed");
	dispatch_table.CmdDrawIndexedIndirect = (PFN_vkCmdDrawIndexedIndirect)get_device_proc(device, "vkCmdDrawIndexedIndirect");
	dispatch_table.CmdPipelineBarrier = (PFN_vkCmdPipelineBarrier)get_device_proc(device, "vkCmdPipelineBarrier");
	dispatch_table.DebugMarkerSetObjectNameEXT = (PFN_vkDebugMarkerSetObjectNameEXT)get_device_proc(device, "vkDebugMarkerSetObjectNameEXT");

	{ const std::lock_guard<std::mutex> lock(s_mutex);
		s_device_mapping[device] = physicalDevice;
		s_device_dispatch[get_dispatch_key(device)] = dispatch_table;
		reshade::vulkan::draw_call_tracker device_tracker;
		s_device_tracker[device] = device_tracker;
	}

	// retrieve the associated command queues
	for (uint32_t i = 0; i < pCreateInfo->queueCreateInfoCount; i++)
	{
		VkDeviceQueueCreateInfo queueInfo = pCreateInfo->pQueueCreateInfos[i];

		for (uint32_t j = 0; j < queueInfo.queueCount; j++)
		{
			VkQueue queue = VK_NULL_HANDLE;
			PFN_vkGetDeviceQueue get_device_queue;
			
			{ const std::lock_guard<std::mutex> lock(s_mutex);
				get_device_queue = s_device_dispatch.at(get_dispatch_key(device)).GetDeviceQueue;
				assert(get_device_queue != nullptr);
			}

			get_device_queue(device, queueInfo.queueFamilyIndex, j, &queue);
			s_queue_mapping[queue] = device;
		}
	}

	return VK_SUCCESS;
}
void     VKAPI_CALL vkDestroyDevice(VkDevice device, const VkAllocationCallbacks *pAllocator)
{
	PFN_vkDestroyDevice trampoline;

	LOG(INFO) << "Redirecting vkDestroyDevice" << '(' << device << ", " << pAllocator << ')' << " ...";

	{ const std::lock_guard<std::mutex> lock(s_mutex);
		trampoline = s_device_dispatch.at(get_dispatch_key(device)).DestroyDevice;
		assert(trampoline != nullptr);

		if (const auto it = s_device_runtimes.find(device); it != s_device_runtimes.end())
		{
			it->second->on_reset();

			s_device_runtimes.erase(it);
		}

		s_device_mapping.erase(device);
		s_device_tracker.erase(device);

		for (auto it = s_queue_mapping.begin(); it != s_queue_mapping.end();)
		{
			if (it->second == device)
			{
				s_queue_mapping.erase(it->first);
				continue;
			}

			++it;
		}

		for (auto it = s_command_buffer_mapping.begin(); it != s_command_buffer_mapping.end();)
		{
			if (it->second == device)
			{
				// command buffer found
				s_command_buffer_mapping.erase(it->first);

				if (const auto it2 = s_trackers_per_command_buffer.find(it->first); it2 != s_trackers_per_command_buffer.end())
				{
					// draw call tracker found. Reset it
					it2->second.reset();
					s_trackers_per_command_buffer.erase(it->first);
					s_framebuffer_per_command_buffer.erase(it->first);
				}
				continue;
			}

			++it;
		}

		// Remove device dispatch table since this device is being destroyed
		s_device_dispatch.erase(get_dispatch_key(device));
	}

	trampoline(device, pAllocator);
}

void     VKAPI_CALL vkGetDeviceQueue(VkDevice device, uint32_t queueFamilyIndex, uint32_t queueIndex, VkQueue *pQueue)
{
	PFN_vkGetDeviceQueue trampoline;

	{ const std::lock_guard<std::mutex> lock(s_mutex);
		trampoline = s_device_dispatch.at(get_dispatch_key(device)).GetDeviceQueue;
		assert(trampoline != nullptr);
	}

	trampoline(device, queueFamilyIndex, queueIndex, pQueue);

	{ const std::lock_guard<std::mutex> lock(s_mutex);
		// Copy dispatch table for queue
		VkLayerDispatchTable dispatch_table = s_device_dispatch.at(get_dispatch_key(device));
		s_device_dispatch[get_dispatch_key(*pQueue)] = dispatch_table;
	}
}
void     VKAPI_CALL vkGetDeviceQueue2(VkDevice device, const VkDeviceQueueInfo2 *pQueueInfo, VkQueue *pQueue)
{
	PFN_vkGetDeviceQueue2 trampoline;

	{ const std::lock_guard<std::mutex> lock(s_mutex);
		trampoline = s_device_dispatch.at(get_dispatch_key(device)).GetDeviceQueue2;
		assert(trampoline != nullptr);
	}

	trampoline(device, pQueueInfo, pQueue);

	{ const std::lock_guard<std::mutex> lock(s_mutex);
		// Copy dispatch table for queue
		VkLayerDispatchTable device_data = s_device_dispatch.at(get_dispatch_key(device));
		s_device_dispatch[get_dispatch_key(*pQueue)] = device_data;
	}
}

VkResult VKAPI_CALL vkCreateSwapchainKHR(VkDevice device, const VkSwapchainCreateInfoKHR *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkSwapchainKHR *pSwapchain)
{
	PFN_vkCreateSwapchainKHR trampoline;

	LOG(INFO) << "Redirecting vkCreateSwapchainKHR" << '(' << device << ", " << pCreateInfo << ", " << pAllocator << ", " << pSwapchain << ')' << " ...";

	// Add required usage flags to create info
	VkSwapchainCreateInfoKHR create_info = *pCreateInfo;
	create_info.imageUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

	{ const std::lock_guard<std::mutex> lock(s_mutex);
		trampoline = s_device_dispatch.at(get_dispatch_key(device)).CreateSwapchainKHR;
		assert(trampoline != nullptr);
	}

	const VkResult result = trampoline(device, &create_info, pAllocator, pSwapchain);
	if (result != VK_SUCCESS)
	{
		LOG(WARN) << "> vkCreateSwapchainKHR failed with error code " << result << '!';
		return result;
	}

	// Guard access to runtime map
	const std::lock_guard<std::mutex> lock(s_mutex);

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
		const VkPhysicalDevice physical_device = s_device_mapping.at(device);
		const VkInstance instance = s_instance_mapping.at(physical_device);

		runtime = std::make_shared<reshade::vulkan::runtime_vk>(
			device, physical_device,
			s_instance_dispatch.at(get_dispatch_key(instance)), s_device_dispatch.at(get_dispatch_key(device)));
	}

	if (!runtime->on_init(*pSwapchain, *pCreateInfo, s_surface_windows.at(pCreateInfo->surface)))
		LOG(ERROR) << "Failed to initialize Vulkan runtime environment on runtime " << runtime.get() << '.';

	s_device_runtimes[device] = runtime;
	s_runtimes[*pSwapchain] = runtime;

	return VK_SUCCESS;
}
void     VKAPI_CALL vkDestroySwapchainKHR(VkDevice device, VkSwapchainKHR swapchain, const VkAllocationCallbacks *pAllocator)
{
	PFN_vkDestroySwapchainKHR trampoline;

	LOG(INFO) << "Redirecting vkDestroySwapchainKHR" << '(' << device << ", " << swapchain << ", " << pAllocator << ')' << " ...";

	{ const std::lock_guard<std::mutex> lock(s_mutex);
		trampoline = s_device_dispatch.at(get_dispatch_key(device)).DestroySwapchainKHR;
		assert(trampoline != nullptr);

		if (const auto it = s_runtimes.find(swapchain); it != s_runtimes.end())
		{
			it->second->on_reset();

			s_runtimes.erase(it);
		}
	}

	trampoline(device, swapchain, pAllocator);
}

VkResult VKAPI_CALL vkQueuePresentKHR(VkQueue queue, const VkPresentInfoKHR *pPresentInfo)
{
	PFN_vkQueuePresentKHR trampoline;

	{ const std::lock_guard<std::mutex> lock(s_mutex);
		trampoline = s_device_dispatch.at(get_dispatch_key(queue)).QueuePresentKHR;
		assert(trampoline != nullptr);
		assert(pPresentInfo != nullptr);

		for (uint32_t i = 0; i < pPresentInfo->swapchainCount; ++i)
		{
			const auto it = s_runtimes.find(pPresentInfo->pSwapchains[i]);
			const auto it2 = s_queue_mapping.find(queue);
			if (it != s_runtimes.end() && it2 != s_queue_mapping.end())
			{
				it->second->on_present(pPresentInfo->pImageIndices[i], s_device_tracker.at(it2->second));
				s_device_tracker.at(it2->second).reset();
			}
		}
	}

	return trampoline(queue, pPresentInfo);
}

VkResult VKAPI_CALL vkQueueSubmit(VkQueue queue, uint32_t submitCount, const VkSubmitInfo* pSubmits, VkFence fence)
{
	PFN_vkQueueSubmit trampoline;

	{ const std::lock_guard<std::mutex> lock(s_mutex);
		trampoline = s_device_dispatch.at(get_dispatch_key(queue)).QueueSubmit;
		assert(trampoline != nullptr);
		assert(pSubmits != nullptr);

		for (uint32_t i = 0; i < submitCount; ++i)
		{
			VkSubmitInfo pSubmit = pSubmits[i];
			for (uint32_t j = 0; j < pSubmit.commandBufferCount; ++j)
			{
				VkCommandBuffer commandBuffer = pSubmits->pCommandBuffers[j];
				const auto it = s_trackers_per_command_buffer.find(commandBuffer);
				const auto it2 = s_queue_mapping.find(queue);
				if (it != s_trackers_per_command_buffer.end() && it2 != s_queue_mapping.end())
					merge_command_buffer_trackers(commandBuffer, s_device_tracker.at(it2->second));
			}
		}
	}

	return trampoline(queue, submitCount, pSubmits, fence);
}

VkResult VKAPI_CALL vkCreateImage(VkDevice device, const VkImageCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkImage* pImage)
{
	PFN_vkCreateImage trampoline = nullptr;

	{ const std::lock_guard<std::mutex> lock(s_mutex);
		trampoline = s_device_dispatch.at(get_dispatch_key(device)).CreateImage;
		assert(trampoline != nullptr);
	}

	VkImageCreateInfo createInfo = *pCreateInfo;

	// check if it is a depth stencil buffer
	if (pCreateInfo->usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
		createInfo.usage |= VK_IMAGE_USAGE_SAMPLED_BIT;

	const VkResult result = trampoline(device, &createInfo, pAllocator, pImage);
	if (result != VK_SUCCESS)
	{
		LOG(WARN) << "> vkCreateImage failed with error code " << result << '!';
		return result;
	}

	// Guard access to the map
	const std::lock_guard<std::mutex> lock(s_mutex);

	// check if it is a depth stencil buffer
	if (pCreateInfo->usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT && pCreateInfo->usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
		s_depth_stencil_buffer_images[*pImage] = { *pCreateInfo, device };

	return result;
}

VkResult VKAPI_CALL vkCreateImageView(VkDevice device, const VkImageViewCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkImageView* pView)
{
	PFN_vkCreateImageView trampoline = nullptr;

	{ const std::lock_guard<std::mutex> lock(s_mutex);
		trampoline = s_device_dispatch.at(get_dispatch_key(device)).CreateImageView;
		assert(trampoline != nullptr);
	}
	
	// create the original image view
	VkResult result = trampoline(device, pCreateInfo, pAllocator, pView);
	if (result != VK_SUCCESS)
	{
		LOG(WARN) << "> vkCreateImageView failed with error code " << result << '!';
		return result;
	}

#if RESHADE_VULKAN_CAPTURE_DEPTH_BUFFERS
	// Guard access to the map
	const std::lock_guard<std::mutex> lock(s_mutex);

	const auto it = s_depth_stencil_buffer_images.find(pCreateInfo->image);
	if (it != s_depth_stencil_buffer_images.end())
	{
		// create an image view where depth buffer is displayable in the shaders
		VkImageViewCreateInfo create_info = *pCreateInfo;
		create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

		VkImageView view_replacement = VK_NULL_HANDLE;
		result = trampoline(device, &create_info, nullptr, &view_replacement);
		if (result != VK_SUCCESS)
		{
			LOG(WARN) << "> vkCreateImageView failed with error code " << result << '!';
			return result;
		}

		s_depth_stencil_buffer_image_views[*pView] = { it->first, create_info, it->second, view_replacement };
	}
#endif

	return result;
}

VkResult VKAPI_CALL vkCreateFramebuffer(VkDevice device, const VkFramebufferCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkFramebuffer* pFramebuffer)
{
	PFN_vkCreateFramebuffer trampoline = nullptr;

	{ const std::lock_guard<std::mutex> lock(s_mutex);
		trampoline = s_device_dispatch.at(get_dispatch_key(device)).CreateFramebuffer;
		assert(trampoline != nullptr);
	}

	const VkResult result = trampoline(device, pCreateInfo, pAllocator, pFramebuffer);
	if (result != VK_SUCCESS)
	{
		LOG(WARN) << "> vkCreateFramebuffer failed with error code " << result << '!';
		return result;
	}

	// Guard access to the map
	const std::lock_guard<std::mutex> lock(s_mutex);

	const VkImageView *pAttachments = pCreateInfo->pAttachments;
	for (uint32_t i = 0; i < pCreateInfo->attachmentCount; i++)
	{
		VkImageView imageView = pAttachments[i];

		const auto it = s_depth_stencil_buffer_image_views.find(imageView);
		if (it != s_depth_stencil_buffer_image_views.end())
			s_depth_stencil_buffer_frameBuffers[*pFramebuffer] = { it->first, it->second };
	}

	return result;
}

VkResult VKAPI_CALL vkAllocateCommandBuffers(VkDevice device, const VkCommandBufferAllocateInfo* pAllocateInfo, VkCommandBuffer* pCommandBuffers)
{
	PFN_vkAllocateCommandBuffers trampoline = nullptr;

	{ const std::lock_guard<std::mutex> lock(s_mutex);
		trampoline = s_device_dispatch.at(get_dispatch_key(device)).AllocateCommandBuffers;
		assert(trampoline != nullptr);
	}

	const VkResult result = trampoline(device, pAllocateInfo, pCommandBuffers);
	if (result != VK_SUCCESS)
	{
		LOG(WARN) << "> vkAllocateCommandBuffers failed with error code " << result << '!';
		return result;
	}

	// Guard access to the map
	const std::lock_guard<std::mutex> lock(s_mutex);

	for (uint32_t i = 0; i < pAllocateInfo->commandBufferCount; i++)
	{
		VkCommandBuffer commandBuffer = pCommandBuffers[i];
		s_command_buffer_mapping[commandBuffer] = device;

		reshade::vulkan::draw_call_tracker command_buffer_tracker;
		add_command_buffer_trackers(commandBuffer, command_buffer_tracker);
	}

	return result;
}

void VKAPI_CALL vkCmdBeginRenderPass(VkCommandBuffer commandBuffer, const VkRenderPassBeginInfo* pRenderPassBegin, VkSubpassContents contents)
{
	PFN_vkCmdBeginRenderPass trampoline = nullptr;

	{ const std::lock_guard<std::mutex> lock(s_mutex);
		trampoline = s_device_dispatch.at(get_dispatch_key(commandBuffer)).CmdBeginRenderPass;
		assert(trampoline != nullptr);
	}

	trampoline(commandBuffer, pRenderPassBegin, contents);

	// no device associated (this cannot happen normally)
	if (const auto it = s_command_buffer_mapping.find(commandBuffer); it == s_command_buffer_mapping.end())
		return;

	VkDevice device = s_command_buffer_mapping.at(commandBuffer);

	// Guard access to the map
	const std::lock_guard<std::mutex> lock(s_mutex);

	s_framebuffer_per_command_buffer[commandBuffer] = pRenderPassBegin->framebuffer;
	const auto it = s_depth_stencil_buffer_frameBuffers.find(pRenderPassBegin->framebuffer);
	if (it != s_depth_stencil_buffer_frameBuffers.end())
		track_active_renderpass(commandBuffer, device, it->second);

	bool depthstencilCleared = false;

	for (uint32_t i = 0; i < pRenderPassBegin->clearValueCount; i++)
	{
		VkClearValue clearValue = pRenderPassBegin->pClearValues[i];
		if (clearValue.depthStencil.depth > 0)
			depthstencilCleared = true;
	}

	if (!depthstencilCleared)
		return;

	if (it != s_depth_stencil_buffer_frameBuffers.end())
	{
		// retrieve the current depth stencil image view
		reshade::vulkan::draw_call_tracker *command_buffer_tracker = &s_trackers_per_command_buffer.at(commandBuffer);
		image_view_data image_view_data = it->second.image_view_data;
		VkImageView depthstencil = it->second.imageView;
		if (image_view_data.image_data.image_info.usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
			track_cleared_depthstencil(commandBuffer, device, *command_buffer_tracker, depthstencil, image_view_data);
	}
}

void     VKAPI_CALL vkCmdDraw(VkCommandBuffer commandBuffer, uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance)
{
	PFN_vkCmdDraw trampoline;

	trampoline = s_device_dispatch.at(get_dispatch_key(commandBuffer)).CmdDraw;
	assert(trampoline != nullptr);

	trampoline(commandBuffer, vertexCount, instanceCount, firstVertex, firstInstance);

	// retrieve the current depth stencil image view
	reshade::vulkan::draw_call_tracker *command_buffer_tracker = &s_trackers_per_command_buffer.at(commandBuffer);
	command_buffer_tracker->on_draw(command_buffer_tracker->_depthstencil, vertexCount * instanceCount);
}

void     VKAPI_CALL vkCmdDrawIndexed(VkCommandBuffer commandBuffer, uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance)
{
	PFN_vkCmdDrawIndexed trampoline;

	trampoline = s_device_dispatch.at(get_dispatch_key(commandBuffer)).CmdDrawIndexed;
	assert(trampoline != nullptr);

	trampoline(commandBuffer, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);

	// retrieve the current depth stencil image view
	reshade::vulkan::draw_call_tracker *command_buffer_tracker = &s_trackers_per_command_buffer.at(commandBuffer);
	command_buffer_tracker->on_draw(command_buffer_tracker->_depthstencil, indexCount * instanceCount);
}

void     VKAPI_CALL vkCmdClearDepthStencilImage(VkCommandBuffer commandBuffer, VkImage image, VkImageLayout imageLayout, const VkClearDepthStencilValue* pDepthStencil, uint32_t rangeCount, const VkImageSubresourceRange* pRanges)
{
	PFN_vkCmdClearDepthStencilImage trampoline;

	{ const std::lock_guard<std::mutex> lock(s_mutex);
		trampoline = s_device_dispatch.at(get_dispatch_key(commandBuffer)).CmdClearDepthStencilImage;
		assert(trampoline != nullptr);
	}

	// no device associated (this cannot happen normally)
	if (const auto it = s_command_buffer_mapping.find(commandBuffer); it != s_command_buffer_mapping.end())
	{
		VkDevice device = s_command_buffer_mapping.at(commandBuffer);

		for (auto &[depthstencil, image_view_data] : s_depth_stencil_buffer_image_views)
		{
			if (image == image_view_data.image)
			{
				{ const std::lock_guard<std::mutex> lock(s_mutex);
					// retrieve the current depth stencil image view
					reshade::vulkan::draw_call_tracker *command_buffer_tracker = &s_trackers_per_command_buffer.at(commandBuffer);
					if (image_view_data.image_data.image_info.usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
						track_cleared_depthstencil(commandBuffer, device, *command_buffer_tracker, depthstencil, image_view_data);
					}
			}
		}
	}

	// important: the trampoline method must be executed after the depth buffer image saving
	trampoline(commandBuffer, image, imageLayout, pDepthStencil, rangeCount, pRanges);
}

void     VKAPI_CALL vkCmdClearAttachments(VkCommandBuffer commandBuffer, uint32_t attachmentCount, const VkClearAttachment* pAttachments, uint32_t rectCount, const VkClearRect* pRects)
{
	PFN_vkCmdClearAttachments trampoline;

	{ const std::lock_guard<std::mutex> lock(s_mutex);
		trampoline = s_device_dispatch.at(get_dispatch_key(commandBuffer)).CmdClearAttachments;
		assert(trampoline != nullptr);
	}

	// no device associated (this cannot happen normally)
	if (const auto it = s_command_buffer_mapping.find(commandBuffer); it != s_command_buffer_mapping.end())
	{
		VkDevice device = s_command_buffer_mapping.at(commandBuffer);
		VkFramebuffer framebuffer = s_framebuffer_per_command_buffer.at(commandBuffer);

		const auto it2 = s_depth_stencil_buffer_frameBuffers.find(framebuffer);
		if (it2 != s_depth_stencil_buffer_frameBuffers.end())
		{
			{ const std::lock_guard<std::mutex> lock(s_mutex);
				// retrieve the current depth stencil image view
				reshade::vulkan::draw_call_tracker *command_buffer_tracker = &s_trackers_per_command_buffer.at(commandBuffer);
				image_view_data image_view_data = it2->second.image_view_data;
				VkImageView depthstencil = it2->second.imageView;
				if (image_view_data.image_data.image_info.usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
					track_cleared_depthstencil(commandBuffer, device, *command_buffer_tracker, depthstencil, image_view_data);
			}
		}
	}

	// important: the trampoline method must be executed after the depth buffer image saving
	trampoline(commandBuffer, attachmentCount, pAttachments, rectCount, pRects);
}

void     VKAPI_CALL vkDestroyImage(VkDevice device, VkImage image, const VkAllocationCallbacks* pAllocator)
{
	PFN_vkDestroyImage trampoline;

	// LOG(INFO) << "Redirecting vkDestroyImage" << '(' << device << ", " << image << ", " << pAllocator << ')' << " ...";

	{ const std::lock_guard<std::mutex> lock(s_mutex);
		trampoline = s_device_dispatch.at(get_dispatch_key(device)).DestroyImage;
		assert(trampoline != nullptr);

		// Remove surface since this surface is being destroyed
		s_depth_stencil_buffer_images.erase(image);
	}

	trampoline(device, image, pAllocator);
}

void     VKAPI_CALL vkDestroyImageView(VkDevice device, VkImageView imageView, const VkAllocationCallbacks* pAllocator)
{
	PFN_vkDestroyImageView trampoline;

	{ const std::lock_guard<std::mutex> lock(s_mutex);
		trampoline = s_device_dispatch.at(get_dispatch_key(device)).DestroyImageView;
		assert(trampoline != nullptr);

		// Remove surface since this surface is being destroyed
		s_depth_stencil_buffer_image_views.erase(imageView);
	}		
		
	trampoline(device, imageView, pAllocator);
}

void     VKAPI_CALL vkDestroyFramebuffer(VkDevice device, VkFramebuffer framebuffer, const VkAllocationCallbacks* pAllocator)
{
	PFN_vkDestroyFramebuffer trampoline;

	{ const std::lock_guard<std::mutex> lock(s_mutex);
		trampoline = s_device_dispatch.at(get_dispatch_key(device)).DestroyFramebuffer;
		assert(trampoline != nullptr);

		// Remove surface since this surface is being destroyed
		s_depth_stencil_buffer_frameBuffers.erase(framebuffer);
	}

	trampoline(device, framebuffer, pAllocator);
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
	if (0 == strcmp(pName, "vkQueueSubmit"))
		return reinterpret_cast<PFN_vkVoidFunction>(vkQueueSubmit);
	if (0 == strcmp(pName, "vkAllocateCommandBuffers"))
		return reinterpret_cast<PFN_vkVoidFunction>(vkAllocateCommandBuffers);
	if (0 == strcmp(pName, "vkCmdBeginRenderPass"))
		return reinterpret_cast<PFN_vkVoidFunction>(vkCmdBeginRenderPass);
	if (0 == strcmp(pName, "vkCmdDraw"))
		return reinterpret_cast<PFN_vkVoidFunction>(vkCmdDraw);
	if (0 == strcmp(pName, "vkCmdDrawIndexed"))
		return reinterpret_cast<PFN_vkVoidFunction>(vkCmdDrawIndexed);
	if (0 == strcmp(pName, "vkCmdClearDepthStencilImage"))
		return reinterpret_cast<PFN_vkVoidFunction>(vkCmdClearDepthStencilImage);
	if (0 == strcmp(pName, "vkCmdClearAttachments"))
		return reinterpret_cast<PFN_vkVoidFunction>(vkCmdClearAttachments);
	if (0 == strcmp(pName, "vkCreateImage"))
		return reinterpret_cast<PFN_vkVoidFunction>(vkCreateImage);
	if (0 == strcmp(pName, "vkCreateImageView"))
		return reinterpret_cast<PFN_vkVoidFunction>(vkCreateImageView);
	if (0 == strcmp(pName, "vkCreateFramebuffer"))
		return reinterpret_cast<PFN_vkVoidFunction>(vkCreateFramebuffer);
	if (0 == strcmp(pName, "vkDestroyImage"))
		return reinterpret_cast<PFN_vkVoidFunction>(vkDestroyImage);
	if (0 == strcmp(pName, "vkDestroyImageView"))
		return reinterpret_cast<PFN_vkVoidFunction>(vkDestroyImageView);
	if (0 == strcmp(pName, "vkDestroyFramebuffer"))
		return reinterpret_cast<PFN_vkVoidFunction>(vkDestroyFramebuffer);

	if (device == VK_NULL_HANDLE)
		return nullptr;

	// Guard access to device dispatch table
	const std::lock_guard<std::mutex> lock(s_mutex);

	PFN_vkGetDeviceProcAddr trampoline = s_device_dispatch.at(
		get_dispatch_key(device)).GetDeviceProcAddr;

	assert(trampoline != nullptr);
	return trampoline(device, pName);
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

	// Guard access to instance dispatch table
	const std::lock_guard<std::mutex> lock(s_mutex);

	PFN_vkGetInstanceProcAddr trampoline = s_instance_dispatch.at(
		get_dispatch_key(instance)).GetInstanceProcAddr;

	assert(trampoline != nullptr);
	return trampoline(instance, pName);
}
