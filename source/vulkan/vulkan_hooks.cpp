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
#include <mutex>
#include <memory>
#include <unordered_map>

struct device_data
{
	VkPhysicalDevice physical_device;
	reshade::vulkan::draw_call_tracker draw_call_tracker;
	std::vector<std::shared_ptr<reshade::vulkan::runtime_vk>> runtimes;
};

struct render_pass_data
{
	uint32_t depthstencil_attachment_index = std::numeric_limits<uint32_t>::max();
	VkImageAspectFlags clear_flags = 0;
	VkImageLayout initial_depthstencil_layout = VK_IMAGE_LAYOUT_UNDEFINED;
};

struct command_buffer_data
{
	std::weak_ptr<reshade::vulkan::runtime_vk> main_runtime;

	// State tracking
	uint32_t current_subpass = std::numeric_limits<uint32_t>::max();
	VkRenderPass current_renderpass = VK_NULL_HANDLE;
	VkFramebuffer current_framebuffer = VK_NULL_HANDLE;
	reshade::vulkan::draw_call_tracker draw_call_tracker;

#if RESHADE_VULKAN_CAPTURE_DEPTH_BUFFERS
	bool save_depth_image(VkCommandBuffer cmd_list, VkImage depthstencil, VkImageLayout layout, const VkImageCreateInfo &create_info, bool cleared)
	{
		if (main_runtime.expired())
			return false;

		const auto runtime = main_runtime.lock();

		if (!runtime->depth_buffer_before_clear)
			return false;
		if (!cleared && !runtime->extended_depth_buffer_detection)
			return false;

		assert(depthstencil != VK_NULL_HANDLE);

		// Check if aspect ratio is similar to the back buffer one
		const float width_factor = float(runtime->frame_width()) / float(create_info.extent.width);
		const float height_factor = float(runtime->frame_height()) / float(create_info.extent.height);
		const float aspect_ratio = float(runtime->frame_width()) / float(runtime->frame_height());
		const float texture_aspect_ratio = float(create_info.extent.width) / float(create_info.extent.height);

		if (fabs(texture_aspect_ratio - aspect_ratio) > 0.1f || width_factor > 1.85f || height_factor > 1.85f || width_factor < 0.5f || height_factor < 0.5f)
			return false; // No match, not a good fit

		// In case the depth texture is retrieved, we make a copy of it and store it in an ordered map to use it later in the final rendering stage.
		if ((runtime->cleared_depth_buffer_index == 0 && cleared) || (runtime->clear_DSV_iter <= runtime->cleared_depth_buffer_index))
		{
			VkImage depth_image_save = runtime->select_depth_texture_save(create_info);
			if (depth_image_save == VK_NULL_HANDLE)
				return false;

			assert(cmd_list != VK_NULL_HANDLE);

			const VkImageAspectFlags aspect_mask = aspect_flags_from_format(create_info.format);

			// TODO: It is invalid for the below to be called in a render pass (which is the case when called from vkCmdClearAttachments).
			// Need to find a solution.

			// Copy the depth texture. This is necessary because the content of the depth texture is cleared.
			// This way, we can retrieve this content in the final rendering stage
			runtime->transition_layout(cmd_list, depth_image_save, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, { aspect_mask, 0, 1, 0, 1 });
			runtime->transition_layout(cmd_list, depthstencil, layout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, { aspect_mask, 0, 1, 0, 1 });

			const VkImageCopy copy_range = {
				{ aspect_mask, 0, 0, 1 }, { 0, 0, 0 },
				{ aspect_mask, 0, 0, 1 }, { 0, 0, 0 }, { create_info.extent.width, create_info.extent.height, 1 }
			};
			runtime->vk.CmdCopyImage(cmd_list, depthstencil, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, depth_image_save, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy_range);

			runtime->transition_layout(cmd_list, depth_image_save, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, { aspect_mask, 0, 1, 0, 1 });
			runtime->transition_layout(cmd_list, depthstencil, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, layout, { aspect_mask, 0, 1, 0, 1 });

			// Store the saved texture in the ordered map.
			draw_call_tracker.track_depth_image(runtime->depth_buffer_texture_format, runtime->clear_DSV_iter, depthstencil, create_info, depth_image_save, cleared);
		}
		else
		{
			// Store a null depth texture in the ordered map in order to display it even if the user chose a previous cleared texture.
			// This way the texture will still be visible in the depth buffer selection window and the user can choose it.
			draw_call_tracker.track_depth_image(runtime->depth_buffer_texture_format, runtime->clear_DSV_iter, depthstencil, create_info, VK_NULL_HANDLE, cleared);
		}

		runtime->clear_DSV_iter++;

		return true;
	}

	void track_active_renderpass(VkCommandBuffer cmd_list, VkImage depthstencil, VkImageLayout layout, const VkImageCreateInfo &create_info)
	{
		// Reset current depth stencil binding
		draw_call_tracker._current_depthstencil = VK_NULL_HANDLE;

		if (main_runtime.expired())
			return;

		// Update current depth stencil binding
		draw_call_tracker._current_depthstencil = depthstencil;

		// TODO: Technically the image layout stored here is not the one the image ends up in, but the likelihood is high
		if (layout == VK_IMAGE_LAYOUT_UNDEFINED) // Do some trickery to make sure it is valid
			layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		draw_call_tracker.track_renderpass(main_runtime.lock()->depth_buffer_texture_format, depthstencil, layout, create_info);

		save_depth_image(cmd_list, depthstencil, layout, create_info, false);
	}
	void track_cleared_depthstencil(VkCommandBuffer cmd_list, VkImageAspectFlags clear_flags, VkImage depthstencil, VkImageLayout layout, const VkImageCreateInfo &create_info)
	{
		if (main_runtime.expired())
			return;

		if (clear_flags & VK_IMAGE_ASPECT_DEPTH_BIT || (main_runtime.lock()->depth_buffer_more_copies && clear_flags & VK_IMAGE_ASPECT_STENCIL_BIT))
			save_depth_image(cmd_list, depthstencil, layout, create_info, true);
	}
#endif
};

static std::unordered_map<void *, VkLayerDispatchTable> s_device_dispatch;
static std::unordered_map<void *, VkLayerInstanceDispatchTable> s_instance_dispatch;

static std::mutex s_device_global_mutex;
static std::unordered_map<VkSurfaceKHR, std::pair<VkInstance, HWND>> s_surface_windows;
static std::unordered_map<VkSwapchainKHR, std::shared_ptr<reshade::vulkan::runtime_vk>> s_runtimes;
static std::unordered_map<void *, device_data> s_device_data;
static std::unordered_map<VkImage, VkImageCreateInfo> s_image_data;
static std::unordered_map<VkImageView, VkImage> s_image_view_mapping;
static std::unordered_map<VkFramebuffer, std::vector<VkImage>> s_framebuffer_data;
static std::unordered_map<VkCommandBuffer, command_buffer_data> s_command_buffer_data;
static std::unordered_map<VkRenderPass, std::vector<render_pass_data>> s_renderpass_data;

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
	LOG(INFO) << "Redirecting vkCreateInstance" << '(' << pCreateInfo << ", " << pAllocator << ", " << pInstance << ')' << " ...";

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
		LOG(WARN) << "> vkCreateInstance failed with error code " << result << '!';
		return result;
	}

	VkInstance instance = *pInstance;
	// Initialize the instance dispatch table
	VkLayerInstanceDispatchTable &dispatch_table = s_instance_dispatch[dispatch_key_from_handle(instance)];
	// ---- Core 1_0 commands
	dispatch_table.DestroyInstance = (PFN_vkDestroyInstance)gipa(instance, "vkDestroyInstance");
	dispatch_table.EnumeratePhysicalDevices = (PFN_vkEnumeratePhysicalDevices)gipa(instance, "vkEnumeratePhysicalDevices");
	dispatch_table.GetPhysicalDeviceMemoryProperties = (PFN_vkGetPhysicalDeviceMemoryProperties)gipa(instance, "vkGetPhysicalDeviceMemoryProperties");
	dispatch_table.GetInstanceProcAddr = gipa;
	// ---- VK_KHR_surface extension commands
	dispatch_table.DestroySurfaceKHR = (PFN_vkDestroySurfaceKHR)gipa(instance, "vkDestroySurfaceKHR");
	// ---- VK_KHR_win32_surface extension commands
	dispatch_table.CreateWin32SurfaceKHR = (PFN_vkCreateWin32SurfaceKHR)gipa(instance, "vkCreateWin32SurfaceKHR");

	return VK_SUCCESS;
}
void     VKAPI_CALL vkDestroyInstance(VkInstance instance, const VkAllocationCallbacks *pAllocator)
{
	LOG(INFO) << "Redirecting vkDestroyInstance" << '(' << instance << ", " << pAllocator << ')' << " ...";

	// Get function pointer before removing it next
	GET_INSTANCE_DISPATCH_PTR(DestroyInstance, instance);
	// Remove instance dispatch table since this instance is being destroyed
	s_instance_dispatch.erase(dispatch_key_from_handle(instance));

	trampoline(instance, pAllocator);
}

VkResult VKAPI_CALL vkCreateWin32SurfaceKHR(VkInstance instance, const VkWin32SurfaceCreateInfoKHR *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkSurfaceKHR *pSurface)
{
	LOG(INFO) << "Redirecting vkCreateWin32SurfaceKHR" << '(' << instance << ", " << pCreateInfo << ", " << pAllocator << ", " << pSurface << ')' << " ...";

	GET_INSTANCE_DISPATCH_PTR(CreateWin32SurfaceKHR, instance);
	const VkResult result = trampoline(instance, pCreateInfo, pAllocator, pSurface);
	if (result != VK_SUCCESS)
	{
		LOG(WARN) << "> vkCreateWin32SurfaceKHR failed with error code " << result << '!';
		return result;
	}

	{ const std::lock_guard<std::mutex> lock(s_device_global_mutex);
		s_surface_windows.insert({ *pSurface, { instance, pCreateInfo->hwnd } });
	}

	return VK_SUCCESS;
}
void     VKAPI_CALL vkDestroySurfaceKHR(VkInstance instance, VkSurfaceKHR surface, const VkAllocationCallbacks *pAllocator)
{
	LOG(INFO) << "Redirecting vkDestroySurfaceKHR" << '(' << instance << ", " << surface << ", " << pAllocator << ')' << " ...";

	{ const std::lock_guard<std::mutex> lock(s_device_global_mutex);
		s_surface_windows.erase(surface);
	}

	GET_INSTANCE_DISPATCH_PTR(DestroySurfaceKHR, instance);
	trampoline(instance, surface, pAllocator);
}

VkResult VKAPI_CALL vkCreateDevice(VkPhysicalDevice physicalDevice, const VkDeviceCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkDevice *pDevice)
{
	LOG(INFO) << "Redirecting vkCreateDevice" << '(' << physicalDevice << ", " << pCreateInfo << ", " << pAllocator << ", " << pDevice << ')' << " ...";

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

	VkDeviceCreateInfo create_info = *pCreateInfo;
	create_info.enabledExtensionCount = uint32_t(enabled_extensions.size());
	create_info.ppEnabledExtensionNames = enabled_extensions.data();
	create_info.pEnabledFeatures = &enabled_features;

	// Continue call down the chain
	const VkResult result = trampoline(physicalDevice, &create_info, pAllocator, pDevice);
	if (result != VK_SUCCESS)
	{
		LOG(WARN) << "> vkCreateDevice failed with error code " << result << '!';
		return result;
	}

	VkDevice device = *pDevice;
	// Initialize the device dispatch table
	VkLayerDispatchTable &dispatch_table = s_device_dispatch[dispatch_key_from_handle(device)];
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
	dispatch_table.WaitForFences = (PFN_vkWaitForFences)gdpa(device, "vkWaitForFences");
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
	dispatch_table.CmdPushConstants = (PFN_vkCmdPushConstants)gdpa(device, "vkCmdPushConstants");
	dispatch_table.CmdBeginRenderPass = (PFN_vkCmdBeginRenderPass)gdpa(device, "vkCmdBeginRenderPass");
	dispatch_table.CmdEndRenderPass = (PFN_vkCmdEndRenderPass)gdpa(device, "vkCmdEndRenderPass");
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

	{ const std::lock_guard<std::mutex> lock(s_device_global_mutex);
		s_device_data[dispatch_key_from_handle(device)].physical_device = physicalDevice;
	}

	return VK_SUCCESS;
}
void     VKAPI_CALL vkDestroyDevice(VkDevice device, const VkAllocationCallbacks *pAllocator)
{
	LOG(INFO) << "Redirecting vkDestroyDevice" << '(' << device << ", " << pAllocator << ')' << " ...";

	{ const std::lock_guard<std::mutex> lock(s_device_global_mutex);
		s_device_data.erase(dispatch_key_from_handle(device));
		s_command_buffer_data.clear(); // Reset all command buffer data
	}

	// Get function pointer before removing it next
	GET_DEVICE_DISPATCH_PTR(DestroyDevice, device);
	// Remove device dispatch table since this device is being destroyed
	s_device_dispatch.erase(dispatch_key_from_handle(device));

	trampoline(device, pAllocator);
}

VkResult VKAPI_CALL vkCreateSwapchainKHR(VkDevice device, const VkSwapchainCreateInfoKHR *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkSwapchainKHR *pSwapchain)
{
	LOG(INFO) << "Redirecting vkCreateSwapchainKHR" << '(' << device << ", " << pCreateInfo << ", " << pAllocator << ", " << pSwapchain << ')' << " ...";

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
		LOG(WARN) << "> vkCreateSwapchainKHR failed with error code " << result << '!';
		return result;
	}

	{ const std::lock_guard<std::mutex> lock(s_device_global_mutex);

		std::shared_ptr<reshade::vulkan::runtime_vk> runtime;
		if (const auto it = s_runtimes.find(pCreateInfo->oldSwapchain);
			it != s_runtimes.end())
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
			const VkInstance instance = s_surface_windows.at(pCreateInfo->surface).first;
			const VkPhysicalDevice physical_device = s_device_data.at(dispatch_key_from_handle(device)).physical_device;

			runtime = std::make_shared<reshade::vulkan::runtime_vk>(
				device, physical_device,
				s_instance_dispatch.at(dispatch_key_from_handle(instance)), s_device_dispatch.at(dispatch_key_from_handle(device)));
		}

		if (!runtime->on_init(*pSwapchain, *pCreateInfo, s_surface_windows.at(pCreateInfo->surface).second))
			LOG(ERROR) << "Failed to initialize Vulkan runtime environment on runtime " << runtime.get() << '.';

		s_runtimes[*pSwapchain] = runtime;

		// Add runtime to device list
		auto &runtimes = s_device_data.at(dispatch_key_from_handle(device)).runtimes;
		runtimes.push_back(runtime);
	}

	return VK_SUCCESS;
}
void     VKAPI_CALL vkDestroySwapchainKHR(VkDevice device, VkSwapchainKHR swapchain, const VkAllocationCallbacks *pAllocator)
{
	LOG(INFO) << "Redirecting vkDestroySwapchainKHR" << '(' << device << ", " << swapchain << ", " << pAllocator << ')' << " ...";

	{ const std::lock_guard<std::mutex> lock(s_device_global_mutex);

		if (const auto it = s_runtimes.find(swapchain);
			it != s_runtimes.end())
		{
			it->second->on_reset();

			// Remove runtime from device list
			auto &runtimes = s_device_data.at(dispatch_key_from_handle(device)).runtimes;
			runtimes.erase(std::remove(runtimes.begin(), runtimes.end(), it->second), runtimes.end());

			// Remove runtime from global list
			s_runtimes.erase(it);
		}
	}

	GET_DEVICE_DISPATCH_PTR(DestroySwapchainKHR, device);
	trampoline(device, swapchain, pAllocator);
}

VkResult VKAPI_CALL vkQueueSubmit(VkQueue queue, uint32_t submitCount, const VkSubmitInfo *pSubmits, VkFence fence)
{
	assert(pSubmits != nullptr);

	{ const std::lock_guard<std::mutex> lock(s_device_global_mutex);

		auto &device_data = s_device_data.at(dispatch_key_from_handle(queue));

		for (uint32_t i = 0; i < submitCount; ++i)
		{
			for (uint32_t k = 0; k < pSubmits[i].commandBufferCount; ++k)
			{
				const VkCommandBuffer cmd = pSubmits->pCommandBuffers[k];
				assert(cmd != VK_NULL_HANDLE);

				auto &command_buffer_data = s_command_buffer_data[cmd];

				// Merge command list trackers into device one
				device_data.draw_call_tracker.merge(command_buffer_data.draw_call_tracker);

				command_buffer_data.draw_call_tracker.reset();
			}
		}
	}

	// The loader uses the same dispatch table for queues and devices, so can use queue to perform lookup here
	GET_DEVICE_DISPATCH_PTR(QueueSubmit, queue);
	return trampoline(queue, submitCount, pSubmits, fence);
}
VkResult VKAPI_CALL vkQueuePresentKHR(VkQueue queue, const VkPresentInfoKHR *pPresentInfo)
{
	assert(pPresentInfo != nullptr);

	{ const std::lock_guard<std::mutex> lock(s_device_global_mutex);

		auto &device_data = s_device_data.at(dispatch_key_from_handle(queue));

		for (uint32_t i = 0; i < pPresentInfo->swapchainCount; ++i)
		{
			if (const auto it = s_runtimes.find(pPresentInfo->pSwapchains[i]);
				it != s_runtimes.end())
			{
				it->second->on_present(pPresentInfo->pImageIndices[i], device_data.draw_call_tracker);

				device_data.draw_call_tracker.reset();
			}
		}

		// Reset command buffer data like draw call trackers
		s_command_buffer_data.clear();
	}

	GET_DEVICE_DISPATCH_PTR(QueuePresentKHR, queue);
	return trampoline(queue, pPresentInfo);
}


VkResult VKAPI_CALL vkCreateImage(VkDevice device, const VkImageCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkImage *pImage)
{
	assert(pImage != nullptr);
	assert(pCreateInfo != nullptr);

	VkImageCreateInfo create_info = *pCreateInfo;
	// Allow shader access to images that are used as depth stencil attachments
	const bool is_depth_stencil_image = create_info.usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	if (is_depth_stencil_image)
		create_info.usage |= VK_IMAGE_USAGE_SAMPLED_BIT;

	GET_DEVICE_DISPATCH_PTR(CreateImage, device);
	const VkResult result = trampoline(device, &create_info, pAllocator, pImage);
	if (result != VK_SUCCESS)
	{
		LOG(WARN) << "> vkCreateImage failed with error code " << result << '!';
		return result;
	}

	// Keep track of image information (only care about depth-stencil images though)
	if (is_depth_stencil_image)
	{ const std::lock_guard<std::mutex> lock(s_device_global_mutex);
		s_image_data.insert({ *pImage, *pCreateInfo });
	}

	return VK_SUCCESS;
}
void     VKAPI_CALL vkDestroyImage(VkDevice device, VkImage image, const VkAllocationCallbacks *pAllocator)
{
	{ const std::lock_guard<std::mutex> lock(s_device_global_mutex);
		s_image_data.erase(image);
	}

	GET_DEVICE_DISPATCH_PTR(DestroyImage, device);
	trampoline(device, image, pAllocator);
}

VkResult VKAPI_CALL vkCreateImageView(VkDevice device, const VkImageViewCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkImageView *pView)
{
	assert(pView != nullptr);
	assert(pCreateInfo != nullptr);

	GET_DEVICE_DISPATCH_PTR(CreateImageView, device);
	const VkResult result = trampoline(device, pCreateInfo, pAllocator, pView);
	if (result != VK_SUCCESS)
	{
		LOG(WARN) << "> vkCreateImageView failed with error code " << result << '!';
		return result;
	}

	// Keep track of image view information
	{ const std::lock_guard<std::mutex> lock(s_device_global_mutex);
		s_image_view_mapping.insert({ *pView, pCreateInfo->image });
	}


	return VK_SUCCESS;
}
void     VKAPI_CALL vkDestroyImageView(VkDevice device, VkImageView imageView, const VkAllocationCallbacks *pAllocator)
{
	{ const std::lock_guard<std::mutex> lock(s_device_global_mutex);
		s_image_view_mapping.erase(imageView);
	}

	GET_DEVICE_DISPATCH_PTR(DestroyImageView, device);
	trampoline(device, imageView, pAllocator);
}

VkResult VKAPI_CALL vkCreateRenderPass(VkDevice device, const VkRenderPassCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkRenderPass *pRenderPass)
{
	GET_DEVICE_DISPATCH_PTR(CreateRenderPass, device);
	const VkResult result = trampoline(device, pCreateInfo, pAllocator, pRenderPass);
	if (result != VK_SUCCESS)
	{
		LOG(WARN) << "> vkCreateRenderPass failed with error code " << result << '!';
		return result;
	}

	{ const std::lock_guard<std::mutex> lock(s_device_global_mutex);

		auto &renderpass_data = s_renderpass_data[*pRenderPass];

		// Search for the first pass using a depth-stencil attachment
		for (uint32_t subpass = 0; subpass < pCreateInfo->subpassCount; ++subpass)
		{
			render_pass_data data;

			const VkAttachmentReference *const depthstencil_reference = pCreateInfo->pSubpasses[subpass].pDepthStencilAttachment;
			if (depthstencil_reference != nullptr && depthstencil_reference->attachment != VK_ATTACHMENT_UNUSED)
			{
				const VkAttachmentDescription &depthstencil_attachment = pCreateInfo->pAttachments[depthstencil_reference->attachment];
				if (depthstencil_attachment.loadOp == VK_ATTACHMENT_LOAD_OP_CLEAR)
					data.clear_flags |= VK_IMAGE_ASPECT_DEPTH_BIT;
				if (depthstencil_attachment.stencilLoadOp == VK_ATTACHMENT_LOAD_OP_CLEAR)
					data.clear_flags |= VK_IMAGE_ASPECT_STENCIL_BIT;
				data.initial_depthstencil_layout = depthstencil_attachment.initialLayout;
				data.depthstencil_attachment_index = depthstencil_reference->attachment;
			}

			renderpass_data.push_back(std::move(data));
		}
	}

	return VK_SUCCESS;
}
void     VKAPI_CALL vkDestroyRenderPass(VkDevice device, VkRenderPass renderPass, const VkAllocationCallbacks *pAllocator)
{
	{ const std::lock_guard<std::mutex> lock(s_device_global_mutex);
		s_renderpass_data.erase(renderPass);
	}

	GET_DEVICE_DISPATCH_PTR(DestroyRenderPass, device);
	trampoline(device, renderPass, pAllocator);
}

VkResult VKAPI_CALL vkCreateFramebuffer(VkDevice device, const VkFramebufferCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkFramebuffer *pFramebuffer)
{
	GET_DEVICE_DISPATCH_PTR(CreateFramebuffer, device);
	const VkResult result = trampoline(device, pCreateInfo, pAllocator, pFramebuffer);
	if (result != VK_SUCCESS)
	{
		LOG(WARN) << "> vkCreateFramebuffer failed with error code " << result << '!';
		return result;
	}

	{ const std::lock_guard<std::mutex> lock(s_device_global_mutex);

		auto &framebuffer_data = s_framebuffer_data[*pFramebuffer];

		framebuffer_data.reserve(pCreateInfo->attachmentCount);
		for (uint32_t i = 0; i < pCreateInfo->attachmentCount; i++)
			framebuffer_data.push_back(s_image_view_mapping[pCreateInfo->pAttachments[i]]);
	}

	return VK_SUCCESS;
}
void     VKAPI_CALL vkDestroyFramebuffer(VkDevice device, VkFramebuffer framebuffer, const VkAllocationCallbacks *pAllocator)
{
	{ const std::lock_guard<std::mutex> lock(s_device_global_mutex);
		s_framebuffer_data.erase(framebuffer);
	}

	GET_DEVICE_DISPATCH_PTR(DestroyFramebuffer, device);
	trampoline(device, framebuffer, pAllocator);
}


void     VKAPI_CALL vkCmdBeginRenderPass(VkCommandBuffer commandBuffer, const VkRenderPassBeginInfo *pRenderPassBegin, VkSubpassContents contents)
{
	{ const std::lock_guard<std::mutex> lock(s_device_global_mutex);

		auto &data = s_command_buffer_data[commandBuffer];
		data.current_subpass = 0;
		data.current_renderpass = pRenderPassBegin->renderPass;
		data.current_framebuffer = pRenderPassBegin->framebuffer;

		const auto &renderpass_data = s_renderpass_data[data.current_renderpass][data.current_subpass];
		const auto &framebuffer_data = s_framebuffer_data[data.current_framebuffer];
		if (renderpass_data.depthstencil_attachment_index < framebuffer_data.size())
		{
			const VkImage depthstencil = framebuffer_data[renderpass_data.depthstencil_attachment_index];
			const VkImageCreateInfo create_info = s_image_data[depthstencil];

			// Update runtime reference
			if (const auto it = s_device_data.find(dispatch_key_from_handle(commandBuffer)); it != s_device_data.end() && !it->second.runtimes.empty())
				data.main_runtime = it->second.runtimes.front();

			data.track_active_renderpass(commandBuffer, depthstencil, renderpass_data.initial_depthstencil_layout, create_info);

			if (renderpass_data.clear_flags)
				data.track_cleared_depthstencil(commandBuffer, renderpass_data.clear_flags, depthstencil, renderpass_data.initial_depthstencil_layout, create_info);
		}
		else
		{
			data.draw_call_tracker._current_depthstencil = VK_NULL_HANDLE;
		}
	}

	GET_DEVICE_DISPATCH_PTR(CmdBeginRenderPass, commandBuffer);
	trampoline(commandBuffer, pRenderPassBegin, contents);
}
void     VKAPI_CALL vkCmdNextSubpass(VkCommandBuffer commandBuffer, VkSubpassContents contents)
{
	GET_DEVICE_DISPATCH_PTR(CmdNextSubpass, commandBuffer);
	trampoline(commandBuffer, contents);

	{ const std::lock_guard<std::mutex> lock(s_device_global_mutex);

		auto &data = s_command_buffer_data[commandBuffer];
		data.current_subpass++;

		const auto &renderpass_data = s_renderpass_data[data.current_renderpass][data.current_subpass];
		const auto &framebuffer_data = s_framebuffer_data[data.current_framebuffer];
		if (renderpass_data.depthstencil_attachment_index < framebuffer_data.size())
		{
			const VkImage depthstencil = framebuffer_data[renderpass_data.depthstencil_attachment_index];

			data.draw_call_tracker._current_depthstencil = depthstencil;
		}
		else
		{
			data.draw_call_tracker._current_depthstencil = VK_NULL_HANDLE;
		}
	}
}
void     VKAPI_CALL vkCmdEndRenderPass(VkCommandBuffer commandBuffer)
{
	GET_DEVICE_DISPATCH_PTR(CmdEndRenderPass, commandBuffer);
	trampoline(commandBuffer);

	{ const std::lock_guard<std::mutex> lock(s_device_global_mutex);

		auto &data = s_command_buffer_data[commandBuffer];
		data.current_subpass = std::numeric_limits<uint32_t>::max();
		data.current_renderpass = VK_NULL_HANDLE;
		data.current_framebuffer = VK_NULL_HANDLE;
		data.draw_call_tracker._current_depthstencil = VK_NULL_HANDLE;
	}
}

void     VKAPI_CALL vkCmdDraw(VkCommandBuffer commandBuffer, uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance)
{
	GET_DEVICE_DISPATCH_PTR(CmdDraw, commandBuffer);
	trampoline(commandBuffer, vertexCount, instanceCount, firstVertex, firstInstance);

	{ const std::lock_guard<std::mutex> lock(s_device_global_mutex);
		s_command_buffer_data[commandBuffer].draw_call_tracker.on_draw(vertexCount * instanceCount);
	}
}
void     VKAPI_CALL vkCmdDrawIndexed(VkCommandBuffer commandBuffer, uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance)
{
	GET_DEVICE_DISPATCH_PTR(CmdDrawIndexed, commandBuffer);
	trampoline(commandBuffer, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);

	{ const std::lock_guard<std::mutex> lock(s_device_global_mutex);
		s_command_buffer_data[commandBuffer].draw_call_tracker.on_draw(indexCount * instanceCount);
	}
}

void     VKAPI_CALL vkCmdClearAttachments(VkCommandBuffer commandBuffer, uint32_t attachmentCount, const VkClearAttachment *pAttachments, uint32_t rectCount, const VkClearRect *pRects)
{
	{ const std::lock_guard<std::mutex> lock(s_device_global_mutex);

		// Find the depth-stencil clear attachment
		for (uint32_t i = 0; i < attachmentCount; ++i)
		{
			if (pAttachments[i].aspectMask & (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT))
			{
				auto &data = s_command_buffer_data[commandBuffer];
				const auto &renderpass_data = s_renderpass_data[data.current_renderpass][data.current_subpass];
				const auto &framebuffer_data = s_framebuffer_data[data.current_framebuffer];
				assert(renderpass_data.depthstencil_attachment_index < framebuffer_data.size());

				const VkImage depthstencil = framebuffer_data[renderpass_data.depthstencil_attachment_index];

				data.track_cleared_depthstencil(commandBuffer, pAttachments[i].aspectMask, depthstencil, renderpass_data.initial_depthstencil_layout, s_image_data[depthstencil]);
				break; // There can only be a single depth-stencil attachment
			}
		}
	}

	GET_DEVICE_DISPATCH_PTR(CmdClearAttachments, commandBuffer);
	trampoline(commandBuffer, attachmentCount, pAttachments, rectCount, pRects);
}
void     VKAPI_CALL vkCmdClearDepthStencilImage(VkCommandBuffer commandBuffer, VkImage image, VkImageLayout imageLayout, const VkClearDepthStencilValue *pDepthStencil, uint32_t rangeCount, const VkImageSubresourceRange *pRanges)
{
	{ const std::lock_guard<std::mutex> lock(s_device_global_mutex);

		// Merge clear flags from all ranges
		VkImageAspectFlags clear_flags = 0;
		for (uint32_t i = 0; i < rangeCount; ++i)
			clear_flags |= pRanges[i].aspectMask;

		s_command_buffer_data[commandBuffer].track_cleared_depthstencil(commandBuffer, clear_flags, image, imageLayout, s_image_data[image]);
	}

	GET_DEVICE_DISPATCH_PTR(CmdClearDepthStencilImage, commandBuffer);
	trampoline(commandBuffer, image, imageLayout, pDepthStencil, rangeCount, pRanges);
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
	if (0 == strcmp(pName, "vkCmdBeginRenderPass"))
		return reinterpret_cast<PFN_vkVoidFunction>(vkCmdBeginRenderPass);
	if (0 == strcmp(pName, "vkCmdNextSubpass"))
		return reinterpret_cast<PFN_vkVoidFunction>(vkCmdNextSubpass);
	if (0 == strcmp(pName, "vkCmdEndRenderPass"))
		return reinterpret_cast<PFN_vkVoidFunction>(vkCmdEndRenderPass);
	if (0 == strcmp(pName, "vkCmdDraw"))
		return reinterpret_cast<PFN_vkVoidFunction>(vkCmdDraw);
	if (0 == strcmp(pName, "vkCmdDrawIndexed"))
		return reinterpret_cast<PFN_vkVoidFunction>(vkCmdDrawIndexed);
	if (0 == strcmp(pName, "vkCmdClearAttachments"))
		return reinterpret_cast<PFN_vkVoidFunction>(vkCmdClearAttachments);
	if (0 == strcmp(pName, "vkCmdClearDepthStencilImage"))
		return reinterpret_cast<PFN_vkVoidFunction>(vkCmdClearDepthStencilImage);

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
