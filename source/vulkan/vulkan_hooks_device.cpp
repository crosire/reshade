/*
 * Copyright (C) 2014 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause OR MIT
 */

#include "dll_log.hpp"
#include "hook_manager.hpp"
#include "lockfree_linear_map.hpp"
#include "vulkan_hooks.hpp"
#include "vulkan_impl_device.hpp"
#include "vulkan_impl_command_queue.hpp"
#include "vulkan_impl_swapchain.hpp"
#include "vulkan_impl_type_convert.hpp"

// Set during Vulkan device creation and presentation, to avoid hooking internal D3D devices created e.g. by NVIDIA Ansel and Optimus
extern thread_local bool g_in_dxgi_runtime;

lockfree_linear_map<void *, reshade::vulkan::device_impl *, 8> g_vulkan_devices;
static lockfree_linear_map<VkQueue, reshade::vulkan::command_queue_impl *, 16> s_vulkan_queues;
extern lockfree_linear_map<void *, instance_dispatch_table, 4> g_instance_dispatch;
extern lockfree_linear_map<VkSurfaceKHR, HWND, 16> g_surface_windows;
static lockfree_linear_map<VkSwapchainKHR, reshade::vulkan::swapchain_impl *, 16> s_vulkan_swapchains;

#define GET_DISPATCH_PTR(name, object) \
	GET_DISPATCH_PTR_FROM(name, g_vulkan_devices.at(dispatch_key_from_handle(object)))
#define GET_DISPATCH_PTR_FROM(name, data) \
	assert((data) != nullptr); \
	PFN_vk##name trampoline = (data)->_dispatch_table.name; \
	assert(trampoline != nullptr)
#define INIT_DISPATCH_PTR(name) \
	dispatch_table.name = reinterpret_cast<PFN_vk##name>(get_device_proc(device, "vk" #name))
#define INIT_DISPATCH_PTR_EXTENSION(name, suffix) \
	if (dispatch_table.name == nullptr) \
		dispatch_table.name  = reinterpret_cast<PFN_vk##name##suffix>(get_device_proc(device, "vk" #name #suffix))

#if RESHADE_ADDON
extern VkImageAspectFlags aspect_flags_from_format(VkFormat format);

static void create_default_view(reshade::vulkan::device_impl *device_impl, VkImage image)
{
	if (image == VK_NULL_HANDLE)
		return;

	const auto data = device_impl->get_private_data_for_object<VK_OBJECT_TYPE_IMAGE>(image);
	assert(data->default_view == VK_NULL_HANDLE);

	// Need to create a default view that is used in 'vkCmdClearColorImage' and 'vkCmdClearDepthStencilImage'
	if ((data->create_info.usage & (VK_IMAGE_USAGE_TRANSFER_DST_BIT)) == VK_IMAGE_USAGE_TRANSFER_DST_BIT &&
		(data->create_info.usage & (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)) != 0)
	{
		VkImageViewCreateInfo default_view_info { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
		default_view_info.image = image;
		default_view_info.viewType = static_cast<VkImageViewType>(data->create_info.imageType); // Map 'VK_IMAGE_TYPE_1D' to VK_IMAGE_VIEW_TYPE_1D' and so on
		default_view_info.format = data->create_info.format;
		default_view_info.subresourceRange.aspectMask = aspect_flags_from_format(data->create_info.format);
		default_view_info.subresourceRange.baseMipLevel = 0;
		default_view_info.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
		default_view_info.subresourceRange.baseArrayLayer = 0;
		default_view_info.subresourceRange.layerCount = 1; // Non-array image view types can only contain a single layer

		vkCreateImageView(device_impl->_orig, &default_view_info, nullptr, &data->default_view);
	}
}
static void destroy_default_view(reshade::vulkan::device_impl *device_impl, VkImage image)
{
	if (image == VK_NULL_HANDLE)
		return;

	const VkImageView default_view = device_impl->get_private_data_for_object<VK_OBJECT_TYPE_IMAGE>(image)->default_view;
	if (default_view != VK_NULL_HANDLE)
	{
		vkDestroyImageView(device_impl->_orig, default_view, nullptr);
	}
}
#endif

VkResult VKAPI_CALL vkCreateDevice(VkPhysicalDevice physicalDevice, const VkDeviceCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkDevice *pDevice)
{
	LOG(INFO) << "Redirecting " << "vkCreateDevice" << '(' << "physicalDevice = " << physicalDevice << ", pCreateInfo = " << pCreateInfo << ", pAllocator = " << pAllocator << ", pDevice = " << pDevice << ')' << " ...";

	assert(pCreateInfo != nullptr && pDevice != nullptr);

	const instance_dispatch_table &instance_dispatch = g_instance_dispatch.at(dispatch_key_from_handle(physicalDevice));

	// Look for layer link info if installed as a layer (provided by the Vulkan loader)
	VkLayerDeviceCreateInfo *const link_info = find_layer_info<VkLayerDeviceCreateInfo>(
		pCreateInfo->pNext, VK_STRUCTURE_TYPE_LOADER_DEVICE_CREATE_INFO, VK_LAYER_LINK_INFO);

	// Get trampoline function pointers
	PFN_vkCreateDevice trampoline = nullptr;
	PFN_vkGetDeviceProcAddr get_device_proc = nullptr;
	PFN_vkGetInstanceProcAddr get_instance_proc = nullptr;

	if (link_info != nullptr)
	{
		assert(link_info->u.pLayerInfo != nullptr);
		assert(link_info->u.pLayerInfo->pfnNextGetDeviceProcAddr != nullptr);
		assert(link_info->u.pLayerInfo->pfnNextGetInstanceProcAddr != nullptr);

		// Look up functions in layer info
		get_device_proc = link_info->u.pLayerInfo->pfnNextGetDeviceProcAddr;
		get_instance_proc = link_info->u.pLayerInfo->pfnNextGetInstanceProcAddr;
		trampoline = reinterpret_cast<PFN_vkCreateDevice>(get_instance_proc(instance_dispatch.instance, "vkCreateDevice"));

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

	LOG(INFO) << "> Dumping enabled device extensions:";
	for (uint32_t i = 0; i < pCreateInfo->enabledExtensionCount; ++i)
		LOG(INFO) << "  " << pCreateInfo->ppEnabledExtensionNames[i];

	auto enum_queue_families = instance_dispatch.GetPhysicalDeviceQueueFamilyProperties;
	assert(enum_queue_families != nullptr);
	auto enum_device_extensions = instance_dispatch.EnumerateDeviceExtensionProperties;
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
	const VkPhysicalDeviceFeatures2 *const features2 = find_in_structure_chain<VkPhysicalDeviceFeatures2>(
		pCreateInfo->pNext, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2);
	if (features2 != nullptr) // The features from the structure chain take precedence
		enabled_features = features2->features;
	else if (pCreateInfo->pEnabledFeatures != nullptr)
		enabled_features = *pCreateInfo->pEnabledFeatures;

	std::vector<const char *> enabled_extensions;
	enabled_extensions.reserve(pCreateInfo->enabledExtensionCount);
	for (uint32_t i = 0; i < pCreateInfo->enabledExtensionCount; ++i)
		enabled_extensions.push_back(pCreateInfo->ppEnabledExtensionNames[i]);

	bool push_descriptor_ext = false;
	bool dynamic_rendering_ext = false;
	bool custom_border_color_ext = false;
	bool extended_dynamic_state_ext = false;
	bool conservative_rasterization_ext = false;

	// Check if the device is used for presenting
	if (std::find_if(enabled_extensions.begin(), enabled_extensions.end(),
		[](const char *name) { return std::strcmp(name, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0; }) == enabled_extensions.end())
	{
		LOG(WARN) << "Skipping device because it is not created with the \"" VK_KHR_SWAPCHAIN_EXTENSION_NAME "\" extension.";

		graphics_queue_family_index = std::numeric_limits<uint32_t>::max();
	}
	// Only have to enable additional features if there is a graphics queue, since ReShade will not run otherwise
	else if (graphics_queue_family_index == std::numeric_limits<uint32_t>::max())
	{
		LOG(WARN) << "Skipping device because it is not created with a graphics queue.";
	}
	else
	{
		uint32_t num_extensions = 0;
		enum_device_extensions(physicalDevice, nullptr, &num_extensions, nullptr);
		std::vector<VkExtensionProperties> extensions(num_extensions);
		enum_device_extensions(physicalDevice, nullptr, &num_extensions, extensions.data());

		// Make sure the driver actually supports the requested extensions
		const auto add_extension = [&extensions, &enabled_extensions, &graphics_queue_family_index](const char *name, bool required) {
			if (const auto it = std::find_if(extensions.begin(), extensions.end(),
				[name](const auto &props) { return std::strncmp(props.extensionName, name, VK_MAX_EXTENSION_NAME_SIZE) == 0; });
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
		enabled_features.shaderStorageImageWriteWithoutFormat = true;

		// Enable extensions that ReShade requires
		if (instance_dispatch.api_version < VK_API_VERSION_1_3 && !add_extension(VK_EXT_PRIVATE_DATA_EXTENSION_NAME, true))
			return VK_ERROR_EXTENSION_NOT_PRESENT;

		add_extension(VK_KHR_IMAGE_FORMAT_LIST_EXTENSION_NAME, true);
		add_extension(VK_KHR_SWAPCHAIN_MUTABLE_FORMAT_EXTENSION_NAME, true);

#ifdef VK_KHR_push_descriptor
		push_descriptor_ext = add_extension(VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME, false);
#endif
		dynamic_rendering_ext = instance_dispatch.api_version >= VK_API_VERSION_1_3 || add_extension(VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME, false);
#ifdef VK_EXT_custom_border_color
		custom_border_color_ext = add_extension(VK_EXT_CUSTOM_BORDER_COLOR_EXTENSION_NAME, false);
#endif
#ifdef VK_EXT_extended_dynamic_state
		extended_dynamic_state_ext = instance_dispatch.api_version >= VK_API_VERSION_1_3 || add_extension(VK_EXT_EXTENDED_DYNAMIC_STATE_EXTENSION_NAME, false);
#endif
#ifdef VK_EXT_conservative_rasterization
		conservative_rasterization_ext = add_extension(VK_EXT_CONSERVATIVE_RASTERIZATION_EXTENSION_NAME, false);
#endif
#ifdef VK_KHR_external_memory_win32
		add_extension(VK_KHR_EXTERNAL_MEMORY_WIN32_EXTENSION_NAME, false);
#endif
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

	// Enable private data feature
	VkDevicePrivateDataCreateInfo private_data_info { VK_STRUCTURE_TYPE_DEVICE_PRIVATE_DATA_CREATE_INFO };
	private_data_info.pNext = create_info.pNext;
	private_data_info.privateDataSlotRequestCount = 1;

	VkPhysicalDevicePrivateDataFeatures private_data_feature { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRIVATE_DATA_FEATURES };
	private_data_feature.pNext = &private_data_info;
	private_data_feature.privateData = VK_TRUE;

	create_info.pNext = &private_data_feature;

	VkPhysicalDeviceDynamicRenderingFeatures dynamic_rendering_feature { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES };
	if (const auto existing_dynamic_rendering_feature = find_in_structure_chain<VkPhysicalDeviceDynamicRenderingFeatures>(
			pCreateInfo->pNext, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES))
	{
		dynamic_rendering_ext = existing_dynamic_rendering_feature->dynamicRendering;
	}
	else if (dynamic_rendering_ext)
	{
		dynamic_rendering_feature.pNext = const_cast<void *>(create_info.pNext);
		dynamic_rendering_feature.dynamicRendering = VK_TRUE;

		create_info.pNext = &dynamic_rendering_feature;
	}

#ifdef VK_EXT_custom_border_color
	// Optionally enable custom border color feature
	VkPhysicalDeviceCustomBorderColorFeaturesEXT custom_border_feature { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CUSTOM_BORDER_COLOR_FEATURES_EXT };
	if (const auto existing_custom_border_feature = find_in_structure_chain<VkPhysicalDeviceCustomBorderColorFeaturesEXT>(
			pCreateInfo->pNext, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CUSTOM_BORDER_COLOR_FEATURES_EXT))
	{
		custom_border_color_ext = existing_custom_border_feature->customBorderColors;
	}
	else if (custom_border_color_ext)
	{
		custom_border_feature.pNext = const_cast<void *>(create_info.pNext);
		custom_border_feature.customBorderColors = VK_TRUE;
		custom_border_feature.customBorderColorWithoutFormat = VK_TRUE;

		create_info.pNext = &custom_border_feature;
	}
#endif

#ifdef VK_EXT_extended_dynamic_state
	// Optionally enable extended dynamic state feature
	VkPhysicalDeviceExtendedDynamicStateFeaturesEXT extended_dynamic_state_feature { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_FEATURES_EXT };
	if (const auto existing_extended_dynamic_state_feature = find_in_structure_chain<VkPhysicalDeviceExtendedDynamicStateFeaturesEXT>(
			pCreateInfo->pNext, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_FEATURES_EXT))
	{
		extended_dynamic_state_ext = existing_extended_dynamic_state_feature->extendedDynamicState;
	}
	else if (extended_dynamic_state_ext)
	{
		extended_dynamic_state_feature.pNext = const_cast<void *>(create_info.pNext);
		extended_dynamic_state_feature.extendedDynamicState = VK_TRUE;

		create_info.pNext = &extended_dynamic_state_feature;
	}
#endif

	// Continue calling down the chain
	g_in_dxgi_runtime = true;
	const VkResult result = trampoline(physicalDevice, &create_info, pAllocator, pDevice);
	g_in_dxgi_runtime = false;
	if (result < VK_SUCCESS)
	{
		LOG(WARN) << "vkCreateDevice" << " failed with error code " << result << '.';
		return result;
	}

	VkDevice device = *pDevice;
	// Initialize the device dispatch table
	VkLayerDispatchTable dispatch_table = {};
	dispatch_table.GetDeviceProcAddr = get_device_proc;

	#pragma region Core 1_0
	INIT_DISPATCH_PTR(DestroyDevice);
	INIT_DISPATCH_PTR(GetDeviceQueue);
	INIT_DISPATCH_PTR(QueueSubmit);
	INIT_DISPATCH_PTR(QueueWaitIdle);
	INIT_DISPATCH_PTR(DeviceWaitIdle);
	INIT_DISPATCH_PTR(AllocateMemory);
	INIT_DISPATCH_PTR(FreeMemory);
	INIT_DISPATCH_PTR(MapMemory);
	INIT_DISPATCH_PTR(UnmapMemory);
	INIT_DISPATCH_PTR(FlushMappedMemoryRanges);
	INIT_DISPATCH_PTR(InvalidateMappedMemoryRanges);
	INIT_DISPATCH_PTR(BindBufferMemory);
	INIT_DISPATCH_PTR(BindImageMemory);
	INIT_DISPATCH_PTR(GetBufferMemoryRequirements);
	INIT_DISPATCH_PTR(GetImageMemoryRequirements);
	INIT_DISPATCH_PTR(CreateFence);
	INIT_DISPATCH_PTR(DestroyFence);
	INIT_DISPATCH_PTR(ResetFences);
	INIT_DISPATCH_PTR(GetFenceStatus);
	INIT_DISPATCH_PTR(WaitForFences);
	INIT_DISPATCH_PTR(CreateSemaphore);
	INIT_DISPATCH_PTR(DestroySemaphore);
	INIT_DISPATCH_PTR(CreateQueryPool);
	INIT_DISPATCH_PTR(DestroyQueryPool);
	INIT_DISPATCH_PTR(GetQueryPoolResults);
	INIT_DISPATCH_PTR(CreateBuffer);
	INIT_DISPATCH_PTR(DestroyBuffer);
	INIT_DISPATCH_PTR(CreateBufferView);
	INIT_DISPATCH_PTR(DestroyBufferView);
	INIT_DISPATCH_PTR(CreateImage);
	INIT_DISPATCH_PTR(DestroyImage);
	INIT_DISPATCH_PTR(GetImageSubresourceLayout);
	INIT_DISPATCH_PTR(CreateImageView);
	INIT_DISPATCH_PTR(DestroyImageView);
	INIT_DISPATCH_PTR(CreateShaderModule);
	INIT_DISPATCH_PTR(DestroyShaderModule);
	INIT_DISPATCH_PTR(CreateGraphicsPipelines);
	INIT_DISPATCH_PTR(CreateComputePipelines);
	INIT_DISPATCH_PTR(DestroyPipeline);
	INIT_DISPATCH_PTR(CreatePipelineLayout);
	INIT_DISPATCH_PTR(DestroyPipelineLayout);
	INIT_DISPATCH_PTR(CreateSampler);
	INIT_DISPATCH_PTR(DestroySampler);
	INIT_DISPATCH_PTR(CreateDescriptorSetLayout);
	INIT_DISPATCH_PTR(DestroyDescriptorSetLayout);
	INIT_DISPATCH_PTR(CreateDescriptorPool);
	INIT_DISPATCH_PTR(DestroyDescriptorPool);
	INIT_DISPATCH_PTR(ResetDescriptorPool);
	INIT_DISPATCH_PTR(AllocateDescriptorSets);
	INIT_DISPATCH_PTR(FreeDescriptorSets);
	INIT_DISPATCH_PTR(UpdateDescriptorSets);
	INIT_DISPATCH_PTR(CreateFramebuffer);
	INIT_DISPATCH_PTR(DestroyFramebuffer);
	INIT_DISPATCH_PTR(CreateRenderPass);
	INIT_DISPATCH_PTR(DestroyRenderPass);
	INIT_DISPATCH_PTR(CreateCommandPool);
	INIT_DISPATCH_PTR(DestroyCommandPool);
	INIT_DISPATCH_PTR(ResetCommandPool);
	INIT_DISPATCH_PTR(AllocateCommandBuffers);
	INIT_DISPATCH_PTR(FreeCommandBuffers);
	INIT_DISPATCH_PTR(BeginCommandBuffer);
	INIT_DISPATCH_PTR(EndCommandBuffer);
	INIT_DISPATCH_PTR(ResetCommandBuffer);
	INIT_DISPATCH_PTR(CmdBindPipeline);
	INIT_DISPATCH_PTR(CmdSetViewport);
	INIT_DISPATCH_PTR(CmdSetScissor);
	INIT_DISPATCH_PTR(CmdSetDepthBias);
	INIT_DISPATCH_PTR(CmdSetBlendConstants);
	INIT_DISPATCH_PTR(CmdSetStencilCompareMask);
	INIT_DISPATCH_PTR(CmdSetStencilWriteMask);
	INIT_DISPATCH_PTR(CmdSetStencilReference);
	INIT_DISPATCH_PTR(CmdBindDescriptorSets);
	INIT_DISPATCH_PTR(CmdBindIndexBuffer);
	INIT_DISPATCH_PTR(CmdBindVertexBuffers);
	INIT_DISPATCH_PTR(CmdDraw);
	INIT_DISPATCH_PTR(CmdDrawIndexed);
	INIT_DISPATCH_PTR(CmdDrawIndirect);
	INIT_DISPATCH_PTR(CmdDrawIndexedIndirect);
	INIT_DISPATCH_PTR(CmdDispatch);
	INIT_DISPATCH_PTR(CmdDispatchIndirect);
	INIT_DISPATCH_PTR(CmdCopyBuffer);
	INIT_DISPATCH_PTR(CmdCopyImage);
	INIT_DISPATCH_PTR(CmdBlitImage);
	INIT_DISPATCH_PTR(CmdCopyBufferToImage);
	INIT_DISPATCH_PTR(CmdCopyImageToBuffer);
	INIT_DISPATCH_PTR(CmdUpdateBuffer);
	INIT_DISPATCH_PTR(CmdClearColorImage);
	INIT_DISPATCH_PTR(CmdClearDepthStencilImage);
	INIT_DISPATCH_PTR(CmdClearAttachments);
	INIT_DISPATCH_PTR(CmdResolveImage);
	INIT_DISPATCH_PTR(CmdPipelineBarrier);
	INIT_DISPATCH_PTR(CmdBeginQuery);
	INIT_DISPATCH_PTR(CmdEndQuery);
	INIT_DISPATCH_PTR(CmdResetQueryPool);
	INIT_DISPATCH_PTR(CmdWriteTimestamp);
	INIT_DISPATCH_PTR(CmdCopyQueryPoolResults);
	INIT_DISPATCH_PTR(CmdPushConstants);
	INIT_DISPATCH_PTR(CmdBeginRenderPass);
	INIT_DISPATCH_PTR(CmdNextSubpass);
	INIT_DISPATCH_PTR(CmdEndRenderPass);
	INIT_DISPATCH_PTR(CmdExecuteCommands);
	#pragma endregion
	#pragma region Core 1_1
	if (instance_dispatch.api_version >= VK_API_VERSION_1_1)
	{
		INIT_DISPATCH_PTR(BindBufferMemory2);
		INIT_DISPATCH_PTR(BindImageMemory2);
		INIT_DISPATCH_PTR(GetBufferMemoryRequirements2);
		INIT_DISPATCH_PTR(GetImageMemoryRequirements2);
		INIT_DISPATCH_PTR(GetDeviceQueue2);
	}
	#pragma endregion
	#pragma region Core 1_2
	if (instance_dispatch.api_version >= VK_API_VERSION_1_2)
	{
		INIT_DISPATCH_PTR(CreateRenderPass2);
		INIT_DISPATCH_PTR(CmdBeginRenderPass2);
		INIT_DISPATCH_PTR(CmdNextSubpass2);
		INIT_DISPATCH_PTR(CmdEndRenderPass2);
	}
	#pragma endregion
	#pragma region Core 1_3
	if (instance_dispatch.api_version >= VK_API_VERSION_1_3)
	{
		INIT_DISPATCH_PTR(CreatePrivateDataSlot);
		INIT_DISPATCH_PTR(DestroyPrivateDataSlot);
		INIT_DISPATCH_PTR(GetPrivateData);
		INIT_DISPATCH_PTR(SetPrivateData);
		INIT_DISPATCH_PTR(CmdPipelineBarrier2);
		INIT_DISPATCH_PTR(CmdWriteTimestamp2);
		INIT_DISPATCH_PTR(QueueSubmit2);
		INIT_DISPATCH_PTR(CmdCopyBuffer2);
		INIT_DISPATCH_PTR(CmdCopyImage2);
		INIT_DISPATCH_PTR(CmdBlitImage2);
		INIT_DISPATCH_PTR(CmdCopyBufferToImage2);
		INIT_DISPATCH_PTR(CmdCopyImageToBuffer2);
		INIT_DISPATCH_PTR(CmdResolveImage2);
		INIT_DISPATCH_PTR(CmdBeginRendering);
		INIT_DISPATCH_PTR(CmdEndRendering);
		INIT_DISPATCH_PTR(CmdSetCullMode);
		INIT_DISPATCH_PTR(CmdSetFrontFace);
		INIT_DISPATCH_PTR(CmdSetPrimitiveTopology);
		INIT_DISPATCH_PTR(CmdSetViewportWithCount);
		INIT_DISPATCH_PTR(CmdSetScissorWithCount);
		INIT_DISPATCH_PTR(CmdBindVertexBuffers2);
		INIT_DISPATCH_PTR(CmdSetDepthTestEnable);
		INIT_DISPATCH_PTR(CmdSetDepthWriteEnable);
		INIT_DISPATCH_PTR(CmdSetDepthCompareOp);
		INIT_DISPATCH_PTR(CmdSetDepthBoundsTestEnable);
		INIT_DISPATCH_PTR(CmdSetStencilTestEnable);
		INIT_DISPATCH_PTR(CmdSetStencilOp);
		INIT_DISPATCH_PTR(CmdSetRasterizerDiscardEnable);
		INIT_DISPATCH_PTR(CmdSetDepthBiasEnable);
		INIT_DISPATCH_PTR(CmdSetPrimitiveRestartEnable);
	}
	#pragma endregion
	#pragma region VK_KHR_swapchain
	INIT_DISPATCH_PTR(CreateSwapchainKHR);
	INIT_DISPATCH_PTR(DestroySwapchainKHR);
	INIT_DISPATCH_PTR(GetSwapchainImagesKHR);
	INIT_DISPATCH_PTR(QueuePresentKHR);
	#pragma endregion
	#pragma region VK_KHR_dynamic_rendering
	INIT_DISPATCH_PTR_EXTENSION(CmdBeginRendering, KHR);
	INIT_DISPATCH_PTR_EXTENSION(CmdEndRendering, KHR);
	#pragma endregion
	#pragma region VK_KHR_push_descriptor
	INIT_DISPATCH_PTR(CmdPushDescriptorSetKHR);
	#pragma endregion
	#pragma region VK_KHR_create_renderpass2
	// Try the KHR version if the core version does not exist
	INIT_DISPATCH_PTR_EXTENSION(CreateRenderPass2, KHR);
	INIT_DISPATCH_PTR_EXTENSION(CmdBeginRenderPass2, KHR);
	INIT_DISPATCH_PTR_EXTENSION(CmdNextSubpass2, KHR);
	INIT_DISPATCH_PTR_EXTENSION(CmdEndRenderPass2, KHR);
	#pragma endregion
	#pragma region VK_KHR_copy_commands2
	INIT_DISPATCH_PTR_EXTENSION(CmdCopyBuffer2, KHR);
	INIT_DISPATCH_PTR_EXTENSION(CmdCopyImage2, KHR);
	INIT_DISPATCH_PTR_EXTENSION(CmdBlitImage2, KHR);
	INIT_DISPATCH_PTR_EXTENSION(CmdCopyBufferToImage2, KHR);
	INIT_DISPATCH_PTR_EXTENSION(CmdCopyImageToBuffer2, KHR);
	INIT_DISPATCH_PTR_EXTENSION(CmdResolveImage2, KHR);
	#pragma endregion
	#pragma region VK_EXT_transform_feedback
	INIT_DISPATCH_PTR(CmdBindTransformFeedbackBuffersEXT);
	INIT_DISPATCH_PTR(CmdBeginQueryIndexedEXT);
	INIT_DISPATCH_PTR(CmdEndQueryIndexedEXT);
	#pragma endregion
	#pragma region VK_EXT_debug_utils
	INIT_DISPATCH_PTR(SetDebugUtilsObjectNameEXT);
	INIT_DISPATCH_PTR(QueueBeginDebugUtilsLabelEXT);
	INIT_DISPATCH_PTR(QueueEndDebugUtilsLabelEXT);
	INIT_DISPATCH_PTR(QueueInsertDebugUtilsLabelEXT);
	INIT_DISPATCH_PTR(CmdBeginDebugUtilsLabelEXT);
	INIT_DISPATCH_PTR(CmdEndDebugUtilsLabelEXT);
	INIT_DISPATCH_PTR(CmdInsertDebugUtilsLabelEXT);
	#pragma endregion
	#pragma region VK_EXT_extended_dynamic_state
#ifdef VK_EXT_extended_dynamic_state
	INIT_DISPATCH_PTR_EXTENSION(CmdSetCullMode, EXT);
	INIT_DISPATCH_PTR_EXTENSION(CmdSetFrontFace, EXT);
	INIT_DISPATCH_PTR_EXTENSION(CmdSetPrimitiveTopology, EXT);
	INIT_DISPATCH_PTR_EXTENSION(CmdSetViewportWithCount, EXT);
	INIT_DISPATCH_PTR_EXTENSION(CmdSetScissorWithCount, EXT);
	INIT_DISPATCH_PTR_EXTENSION(CmdBindVertexBuffers2, EXT);
	INIT_DISPATCH_PTR_EXTENSION(CmdSetDepthTestEnable, EXT);
	INIT_DISPATCH_PTR_EXTENSION(CmdSetDepthWriteEnable, EXT);
	INIT_DISPATCH_PTR_EXTENSION(CmdSetDepthCompareOp, EXT);
	INIT_DISPATCH_PTR_EXTENSION(CmdSetDepthBoundsTestEnable, EXT);
	INIT_DISPATCH_PTR_EXTENSION(CmdSetStencilTestEnable, EXT);
	INIT_DISPATCH_PTR_EXTENSION(CmdSetStencilOp, EXT);
#endif
	#pragma endregion
	#pragma region VK_EXT_private_data
	// Try the EXT version if the core version does not exist
	INIT_DISPATCH_PTR_EXTENSION(CreatePrivateDataSlot, EXT);
	INIT_DISPATCH_PTR_EXTENSION(DestroyPrivateDataSlot, EXT);
	INIT_DISPATCH_PTR_EXTENSION(GetPrivateData, EXT);
	INIT_DISPATCH_PTR_EXTENSION(SetPrivateData, EXT);
	#pragma endregion
	#pragma region VK_KHR_external_memory_win32
	INIT_DISPATCH_PTR(GetMemoryWin32HandleKHR);
	INIT_DISPATCH_PTR(GetMemoryWin32HandlePropertiesKHR);
	#pragma endregion

	// Initialize per-device data
	const auto device_impl = new reshade::vulkan::device_impl(
		device,
		physicalDevice,
		g_instance_dispatch.at(dispatch_key_from_handle(physicalDevice)),
		dispatch_table,
		enabled_features,
		push_descriptor_ext,
		dynamic_rendering_ext,
		custom_border_color_ext,
		extended_dynamic_state_ext,
		conservative_rasterization_ext);

	device_impl->_graphics_queue_family_index = graphics_queue_family_index;

	g_vulkan_devices.emplace(dispatch_key_from_handle(device), device_impl);

	// Initialize all queues associated with this device
	for (uint32_t i = 0; i < pCreateInfo->queueCreateInfoCount; ++i)
	{
		const VkDeviceQueueCreateInfo &queue_create_info = pCreateInfo->pQueueCreateInfos[i];

		for (uint32_t queue_index = 0; queue_index < queue_create_info.queueCount; ++queue_index)
		{
			VkQueue queue = VK_NULL_HANDLE;
			dispatch_table.GetDeviceQueue(device, queue_create_info.queueFamilyIndex, queue_index, &queue);
			assert(VK_NULL_HANDLE != queue);

			const auto queue_impl = new reshade::vulkan::command_queue_impl(
				device_impl,
				queue_create_info.queueFamilyIndex,
				queue_families[queue_create_info.queueFamilyIndex],
				queue);

			s_vulkan_queues.emplace(queue, queue_impl);
		}
	}

#if RESHADE_VERBOSE_LOG
	LOG(INFO) << "Returning Vulkan device " << device << '.';
#endif
	return result;
}
void     VKAPI_CALL vkDestroyDevice(VkDevice device, const VkAllocationCallbacks *pAllocator)
{
	LOG(INFO) << "Redirecting " << "vkDestroyDevice" << '(' << "device = " << device << ", pAllocator = " << pAllocator << ')' << " ...";

	// Remove from device dispatch table since this device is being destroyed
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.erase(dispatch_key_from_handle(device));
	GET_DISPATCH_PTR_FROM(DestroyDevice, device_impl);

	// Destroy all queues associated with this device
	const std::vector<reshade::vulkan::command_queue_impl *> queues = device_impl->_queues;
	for (reshade::vulkan::command_queue_impl *queue_impl : queues)
	{
		s_vulkan_queues.erase(queue_impl->_orig);
		delete queue_impl; // This will remove the queue from the queue list of the device too (see 'command_queue_impl' destructor)
	}

	assert(device_impl->_queues.empty());

	// Finally destroy the device
	delete device_impl;

	trampoline(device, pAllocator);
}

VkResult VKAPI_CALL vkCreateSwapchainKHR(VkDevice device, const VkSwapchainCreateInfoKHR *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkSwapchainKHR *pSwapchain)
{
	LOG(INFO) << "Redirecting " << "vkCreateSwapchainKHR" << '(' << "device = " << device << ", pCreateInfo = " << pCreateInfo << ", pAllocator = " << pAllocator << ", pSwapchain = " << pSwapchain << ')' << " ...";

	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(device));
	GET_DISPATCH_PTR_FROM(CreateSwapchainKHR, device_impl);

	assert(pCreateInfo != nullptr && pSwapchain != nullptr);

	VkSwapchainCreateInfoKHR create_info = *pCreateInfo;
	VkImageFormatListCreateInfoKHR format_list_info { VK_STRUCTURE_TYPE_IMAGE_FORMAT_LIST_CREATE_INFO_KHR };
	std::vector<VkFormat> format_list; std::vector<uint32_t> queue_family_list;

	// Only have to enable additional features if there is a graphics queue, since ReShade will not run otherwise
	if (device_impl->_graphics_queue_family_index != std::numeric_limits<uint32_t>::max())
	{
		// Add required usage flags to create info
		create_info.imageUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

		// Add required formats, so views with different formats can be created for the swap chain images
		format_list.push_back(reshade::vulkan::convert_format(
			reshade::api::format_to_default_typed(reshade::vulkan::convert_format(create_info.imageFormat), 0)));
		format_list.push_back(reshade::vulkan::convert_format(
			reshade::api::format_to_default_typed(reshade::vulkan::convert_format(create_info.imageFormat), 1)));

		// Only have to make format mutable if they are actually different
		if (format_list[0] != format_list[1])
			create_info.flags |= VK_SWAPCHAIN_CREATE_MUTABLE_FORMAT_BIT_KHR;

		// Patch the format list in the create info of the application
		if (const auto format_list_info2 = find_in_structure_chain<VkImageFormatListCreateInfoKHR>(pCreateInfo->pNext, VK_STRUCTURE_TYPE_IMAGE_FORMAT_LIST_CREATE_INFO_KHR))
		{
			format_list.insert(format_list.end(),
				format_list_info2->pViewFormats, format_list_info2->pViewFormats + format_list_info2->viewFormatCount);

			// Remove duplicates from the list (since the new formats may have already been added by the application)
			std::sort(format_list.begin(), format_list.end());
			format_list.erase(std::unique(format_list.begin(), format_list.end()), format_list.end());

			// This is evil, because writing into the application memory, but eh =)
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
			queue_family_list.push_back(device_impl->_graphics_queue_family_index);

			for (uint32_t i = 0; i < create_info.queueFamilyIndexCount; ++i)
				if (create_info.pQueueFamilyIndices[i] != device_impl->_graphics_queue_family_index)
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

	const char *format_string = nullptr;
	switch (create_info.imageFormat)
	{
	case VK_FORMAT_UNDEFINED:
		format_string = "VK_FORMAT_UNDEFINED";
		break;
	case VK_FORMAT_R8G8B8A8_UNORM:
		format_string = "VK_FORMAT_R8G8B8A8_UNORM";
		break;
	case VK_FORMAT_R8G8B8A8_SRGB:
		format_string = "VK_FORMAT_R8G8B8A8_SRGB";
		break;
	case VK_FORMAT_B8G8R8A8_UNORM:
		format_string = "VK_FORMAT_B8G8R8A8_UNORM";
		break;
	case VK_FORMAT_B8G8R8A8_SRGB:
		format_string = "VK_FORMAT_B8G8R8A8_SRGB";
		break;
	case VK_FORMAT_A2R10G10B10_UNORM_PACK32:
		format_string = "VK_FORMAT_A2R10G10B10_UNORM_PACK32";
		break;
	case VK_FORMAT_R16G16B16A16_SFLOAT:
		format_string = "VK_FORMAT_R16G16B16A16_SFLOAT";
		break;
	}

	if (format_string != nullptr)
		LOG(INFO) << "  | imageFormat                             | " << std::setw(39) << format_string << " |";
	else
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

	// Look up window handle from surface
	const HWND hwnd = g_surface_windows.at(create_info.surface);

#if RESHADE_ADDON
	reshade::api::resource_desc buffer_desc = {};
	buffer_desc.type = reshade::api::resource_type::texture_2d;
	buffer_desc.texture.width = create_info.imageExtent.width;
	buffer_desc.texture.height = create_info.imageExtent.height;
	assert(create_info.imageArrayLayers <= std::numeric_limits<uint16_t>::max());
	buffer_desc.texture.depth_or_layers = static_cast<uint16_t>(create_info.imageArrayLayers);
	buffer_desc.texture.levels = 1;
	buffer_desc.texture.format = reshade::vulkan::convert_format(create_info.imageFormat);
	buffer_desc.texture.samples = 1;
	buffer_desc.heap = reshade::api::memory_heap::gpu_only;
	reshade::vulkan::convert_image_usage_flags_to_usage(create_info.imageUsage, buffer_desc.usage);

	if (reshade::invoke_addon_event<reshade::addon_event::create_swapchain>(buffer_desc, hwnd))
	{
		create_info.imageFormat = reshade::vulkan::convert_format(buffer_desc.texture.format);
		create_info.imageExtent.width = buffer_desc.texture.width;
		create_info.imageExtent.height = buffer_desc.texture.height;
		create_info.imageArrayLayers = buffer_desc.texture.depth_or_layers;
		reshade::vulkan::convert_usage_to_image_usage_flags(buffer_desc.usage, create_info.imageUsage);
	}
#endif

	// Remove old swap chain from the list so that a call to 'vkDestroySwapchainKHR' won't reset the effect runtime again
	reshade::vulkan::swapchain_impl *swapchain_impl = s_vulkan_swapchains.erase(create_info.oldSwapchain);
	if (swapchain_impl != nullptr)
	{
		assert(create_info.oldSwapchain != VK_NULL_HANDLE);

#if RESHADE_ADDON
		for (uint32_t i = 0; i < swapchain_impl->get_back_buffer_count(); ++i)
			destroy_default_view(device_impl, (VkImage)swapchain_impl->get_back_buffer(i).handle);
#endif

		// Re-use the existing effect runtime if this swap chain was not created from scratch, but reset it before initializing again below
		swapchain_impl->on_reset();
	}

	const VkResult result = trampoline(device, &create_info, pAllocator, pSwapchain);
	if (result < VK_SUCCESS)
	{
		LOG(WARN) << "vkCreateSwapchainKHR" << " failed with error code " << result << '.';
		return result;
	}

	reshade::vulkan::command_queue_impl *queue_impl = nullptr;
	if (device_impl->_graphics_queue_family_index != std::numeric_limits<uint32_t>::max())
	{
		// Get the main graphics queue for command submission
		// There has to be at least one queue, or else this effect runtime would not have been created with this queue family index, so it is safe to get the first one here
		VkQueue graphics_queue = VK_NULL_HANDLE;
		device_impl->_dispatch_table.GetDeviceQueue(device, device_impl->_graphics_queue_family_index, 0, &graphics_queue);
		assert(VK_NULL_HANDLE != graphics_queue);

		queue_impl = s_vulkan_queues.at(graphics_queue);
	}

	if (queue_impl != nullptr)
	{
		if (nullptr == swapchain_impl)
			swapchain_impl = new reshade::vulkan::swapchain_impl(device_impl, queue_impl);

		swapchain_impl->on_init(*pSwapchain, create_info, hwnd);

#if RESHADE_ADDON
		// Create default views for swap chain images
		for (uint32_t i = 0; i < swapchain_impl->get_back_buffer_count(); ++i)
			create_default_view(device_impl, (VkImage)swapchain_impl->get_back_buffer(i).handle);
#endif

		if (!s_vulkan_swapchains.emplace(*pSwapchain, swapchain_impl))
			delete swapchain_impl;
	}
	else
	{
		delete swapchain_impl;

		s_vulkan_swapchains.emplace(*pSwapchain, nullptr);
	}

#if RESHADE_VERBOSE_LOG
	LOG(INFO) << "Returning Vulkan swap chain " << *pSwapchain << '.';
#endif
	return result;
}
void     VKAPI_CALL vkDestroySwapchainKHR(VkDevice device, VkSwapchainKHR swapchain, const VkAllocationCallbacks *pAllocator)
{
	LOG(INFO) << "Redirecting " << "vkDestroySwapchainKHR" << '(' << device << ", " << swapchain << ", " << pAllocator << ')' << " ...";

	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(device));
	GET_DISPATCH_PTR_FROM(DestroySwapchainKHR, device_impl);

	// Remove swap chain from global list
	reshade::vulkan::swapchain_impl *const swapchain_impl = s_vulkan_swapchains.erase(swapchain);

#if RESHADE_ADDON
	if (swapchain_impl != nullptr)
	{
		for (uint32_t i = 0; i < swapchain_impl->get_back_buffer_count(); ++i)
			destroy_default_view(device_impl, (VkImage)swapchain_impl->get_back_buffer(i).handle);
	}
#endif

	delete swapchain_impl;

	trampoline(device, swapchain, pAllocator);
}

VkResult VKAPI_CALL vkQueueSubmit(VkQueue queue, uint32_t submitCount, const VkSubmitInfo *pSubmits, VkFence fence)
{
	assert(pSubmits != nullptr);

#if RESHADE_ADDON
	if (reshade::vulkan::command_queue_impl *const queue_impl = s_vulkan_queues.at(queue); queue_impl != nullptr)
	{
		const auto device_impl = static_cast<reshade::vulkan::device_impl *>(queue_impl->get_device());

		for (uint32_t i = 0; i < submitCount; ++i)
		{
			for (uint32_t k = 0; k < pSubmits[i].commandBufferCount; ++k)
			{
				assert(pSubmits[i].pCommandBuffers[k] != VK_NULL_HANDLE);

				reshade::vulkan::command_list_impl *const cmd_impl = device_impl->get_private_data_for_object<VK_OBJECT_TYPE_COMMAND_BUFFER>(pSubmits[i].pCommandBuffers[k]);

				reshade::invoke_addon_event<reshade::addon_event::execute_command_list>(queue_impl, cmd_impl);
			}
		}

		queue_impl->flush_immediate_command_list();
	}
#endif

	// The loader uses the same dispatch table pointer for queues and devices, so can use queue to perform lookup here
	GET_DISPATCH_PTR(QueueSubmit, queue);
	return trampoline(queue, submitCount, pSubmits, fence);
}
VkResult VKAPI_CALL vkQueueSubmit2(VkQueue queue, uint32_t submitCount, const VkSubmitInfo2 *pSubmits, VkFence fence)
{
	assert(pSubmits != nullptr);

#if RESHADE_ADDON
	if (reshade::vulkan::command_queue_impl *const queue_impl = s_vulkan_queues.at(queue); queue_impl != nullptr)
	{
		const auto device_impl = static_cast<reshade::vulkan::device_impl *>(queue_impl->get_device());

		for (uint32_t i = 0; i < submitCount; ++i)
		{
			for (uint32_t k = 0; k < pSubmits[i].commandBufferInfoCount; ++k)
			{
				assert(pSubmits[i].pCommandBufferInfos[k].commandBuffer != VK_NULL_HANDLE);

				reshade::vulkan::command_list_impl *const cmd_impl = device_impl->get_private_data_for_object<VK_OBJECT_TYPE_COMMAND_BUFFER>(pSubmits[i].pCommandBufferInfos[k].commandBuffer);

				reshade::invoke_addon_event<reshade::addon_event::execute_command_list>(queue_impl, cmd_impl);
			}
		}

		queue_impl->flush_immediate_command_list();
	}
#endif

	GET_DISPATCH_PTR(QueueSubmit2, queue);
	return trampoline(queue, submitCount, pSubmits, fence);
}

VkResult VKAPI_CALL vkQueuePresentKHR(VkQueue queue, const VkPresentInfoKHR *pPresentInfo)
{
	assert(pPresentInfo != nullptr);

	std::vector<VkSemaphore> wait_semaphores(
		pPresentInfo->pWaitSemaphores, pPresentInfo->pWaitSemaphores + pPresentInfo->waitSemaphoreCount);

	if (reshade::vulkan::command_queue_impl *const queue_impl = s_vulkan_queues.at(queue); queue_impl != nullptr)
	{
		for (uint32_t i = 0; i < pPresentInfo->swapchainCount; ++i)
		{
			reshade::vulkan::swapchain_impl *const swapchain_impl = s_vulkan_swapchains.at(pPresentInfo->pSwapchains[i]);
			if (swapchain_impl != nullptr)
			{
#if RESHADE_ADDON
				uint32_t dirty_rect_count = 0;
				temp_mem<reshade::api::rect, 16> dirty_rects;

				const auto present_regions = find_in_structure_chain<VkPresentRegionsKHR>(pPresentInfo->pNext, VK_STRUCTURE_TYPE_PRESENT_REGIONS_KHR);
				if (present_regions != nullptr)
				{
					assert(present_regions->swapchainCount == pPresentInfo->swapchainCount);

					dirty_rect_count = present_regions->pRegions[i].rectangleCount;
					if (dirty_rect_count > 16)
						dirty_rects.p = new reshade::api::rect[dirty_rect_count];

					const VkRectLayerKHR *rects = present_regions->pRegions[i].pRectangles;
					for (uint32_t k = 0; k < dirty_rect_count; ++k)
					{
						dirty_rects[k] = {
							rects[k].offset.x,
							rects[k].offset.y,
							rects[k].offset.x + static_cast<int32_t>(rects[k].extent.width),
							rects[k].offset.y + static_cast<int32_t>(rects[k].extent.height)
						};
					}
				}

				reshade::api::rect source_rect, dest_rect;

				const auto display_present_info = find_in_structure_chain<VkDisplayPresentInfoKHR>(pPresentInfo->pNext, VK_STRUCTURE_TYPE_DISPLAY_PRESENT_INFO_KHR);
				if (display_present_info != nullptr)
				{
					source_rect = {
						display_present_info->srcRect.offset.x,
						display_present_info->srcRect.offset.y,
						display_present_info->srcRect.offset.x + static_cast<int32_t>(display_present_info->srcRect.extent.width),
						display_present_info->srcRect.offset.y + static_cast<int32_t>(display_present_info->srcRect.extent.height)
					};
					dest_rect = {
						display_present_info->dstRect.offset.x,
						display_present_info->dstRect.offset.y,
						display_present_info->dstRect.offset.x + static_cast<int32_t>(display_present_info->dstRect.extent.width),
						display_present_info->dstRect.offset.y + static_cast<int32_t>(display_present_info->dstRect.extent.height)
					};
				}

				reshade::invoke_addon_event<reshade::addon_event::present>(
					queue_impl,
					swapchain_impl,
					display_present_info != nullptr ? &source_rect : nullptr,
					display_present_info != nullptr ? &dest_rect : nullptr,
					dirty_rect_count,
					dirty_rect_count != 0 ? dirty_rects.p : nullptr);
#endif
				swapchain_impl->on_present(queue, pPresentInfo->pImageIndices[i], wait_semaphores);
			}
		}

		queue_impl->flush_immediate_command_list(wait_semaphores);

		static_cast<reshade::vulkan::device_impl *>(queue_impl->get_device())->advance_transient_descriptor_pool();
	}

	// Override wait semaphores based on the last queue submit from above
	VkPresentInfoKHR present_info = *pPresentInfo;
	present_info.waitSemaphoreCount = static_cast<uint32_t>(wait_semaphores.size());
	present_info.pWaitSemaphores = wait_semaphores.data();

	GET_DISPATCH_PTR(QueuePresentKHR, queue);
	g_in_dxgi_runtime = true;
	const VkResult result = trampoline(queue, &present_info);
	g_in_dxgi_runtime = false;
	return result;
}

VkResult VKAPI_CALL vkBindBufferMemory(VkDevice device, VkBuffer buffer, VkDeviceMemory memory, VkDeviceSize memoryOffset)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(device));
	GET_DISPATCH_PTR_FROM(BindBufferMemory, device_impl);

	const VkResult result = trampoline(device, buffer, memory, memoryOffset);
	if (result < VK_SUCCESS)
	{
#if RESHADE_VERBOSE_LOG
		LOG(WARN) << "vkBindBufferMemory" << " failed with error code " << result << '.';
#endif
		return result;
	}

#if RESHADE_ADDON
	const auto buffer_data = device_impl->get_private_data_for_object<VK_OBJECT_TYPE_BUFFER>(buffer);
	buffer_data->memory = memory;
	buffer_data->memory_offset = memoryOffset;

	reshade::invoke_addon_event<reshade::addon_event::init_resource>(
		device_impl,
		reshade::vulkan::convert_resource_desc(buffer_data->create_info),
		nullptr,
		reshade::api::resource_usage::undefined,
		reshade::api::resource { (uint64_t)buffer });
#endif

	return result;
}
VkResult VKAPI_CALL vkBindBufferMemory2(VkDevice device, uint32_t bindInfoCount, const VkBindBufferMemoryInfo *pBindInfos)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(device));
	GET_DISPATCH_PTR_FROM(BindBufferMemory2, device_impl);

	const VkResult result = trampoline(device, bindInfoCount, pBindInfos);
	if (result < VK_SUCCESS)
	{
#if RESHADE_VERBOSE_LOG
		LOG(WARN) << "vkBindBufferMemory2" << " failed with error code " << result << '.';
#endif
		return result;
	}

#if RESHADE_ADDON
	for (uint32_t i = 0; i < bindInfoCount; ++i)
	{
		const auto buffer_data = device_impl->get_private_data_for_object<VK_OBJECT_TYPE_BUFFER>(pBindInfos[i].buffer);
		buffer_data->memory = pBindInfos[i].memory;
		buffer_data->memory_offset = pBindInfos[i].memoryOffset;

		reshade::invoke_addon_event<reshade::addon_event::init_resource>(
			device_impl,
			reshade::vulkan::convert_resource_desc(buffer_data->create_info),
			nullptr,
			reshade::api::resource_usage::undefined,
			reshade::api::resource { (uint64_t)pBindInfos[i].buffer });
	}
#endif

	return result;
}

VkResult VKAPI_CALL vkBindImageMemory(VkDevice device, VkImage image, VkDeviceMemory memory, VkDeviceSize memoryOffset)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(device));
	GET_DISPATCH_PTR_FROM(BindImageMemory, device_impl);

	const VkResult result = trampoline(device, image, memory, memoryOffset);
	if (result < VK_SUCCESS)
	{
#if RESHADE_VERBOSE_LOG
		LOG(WARN) << "vkBindImageMemory" << " failed with error code " << result << '.';
#endif
		return result;
	}

#if RESHADE_ADDON
	create_default_view(device_impl, image);

	const auto image_data = device_impl->get_private_data_for_object<VK_OBJECT_TYPE_IMAGE>(image);
	image_data->memory = memory;
	image_data->memory_offset = memoryOffset;

	reshade::invoke_addon_event<reshade::addon_event::init_resource>(
		device_impl,
		reshade::vulkan::convert_resource_desc(image_data->create_info),
		nullptr,
		image_data->create_info.initialLayout == VK_IMAGE_LAYOUT_PREINITIALIZED ? reshade::api::resource_usage::cpu_access : reshade::api::resource_usage::undefined,
		reshade::api::resource { (uint64_t)image });
#endif

	return result;
}
VkResult VKAPI_CALL vkBindImageMemory2(VkDevice device, uint32_t bindInfoCount, const VkBindImageMemoryInfo *pBindInfos)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(device));
	GET_DISPATCH_PTR_FROM(BindImageMemory2, device_impl);

	const VkResult result = trampoline(device, bindInfoCount, pBindInfos);
	if (result < VK_SUCCESS)
	{
#if RESHADE_VERBOSE_LOG
		LOG(WARN) << "vkBindImageMemory2" << " failed with error code " << result << '.';
#endif
		return result;
	}

#if RESHADE_ADDON
	for (uint32_t i = 0; i < bindInfoCount; ++i)
	{
		create_default_view(device_impl, pBindInfos[i].image);

		const auto image_data = device_impl->get_private_data_for_object<VK_OBJECT_TYPE_IMAGE>(pBindInfos[i].image);
		image_data->memory = pBindInfos[i].memory;
		image_data->memory_offset = pBindInfos[i].memoryOffset;

		reshade::invoke_addon_event<reshade::addon_event::init_resource>(
			device_impl,
			reshade::vulkan::convert_resource_desc(image_data->create_info),
			nullptr,
			image_data->create_info.initialLayout == VK_IMAGE_LAYOUT_PREINITIALIZED ? reshade::api::resource_usage::cpu_access : reshade::api::resource_usage::undefined,
			reshade::api::resource { (uint64_t)pBindInfos[i].image });
	}
#endif

	return result;
}

VkResult VKAPI_CALL vkCreateQueryPool(VkDevice device, const VkQueryPoolCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkQueryPool *pQueryPool)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(device));
	GET_DISPATCH_PTR_FROM(CreateQueryPool, device_impl);

	assert(pCreateInfo != nullptr && pQueryPool != nullptr);

#if RESHADE_ADDON
	VkQueryPoolCreateInfo create_info = *pCreateInfo;

	if (reshade::invoke_addon_event<reshade::addon_event::create_query_pool>(device_impl, reshade::vulkan::convert_query_type(create_info.queryType), create_info.queryCount))
	{
		pCreateInfo = &create_info;
	}
#endif

	const VkResult result = trampoline(device, pCreateInfo, pAllocator, pQueryPool);
	if (result < VK_SUCCESS)
	{
#if RESHADE_VERBOSE_LOG
		LOG(WARN) << "vkCreateQueryPool" << " failed with error code " << result << '.';
#endif
		return result;
	}

#if RESHADE_ADDON
	reshade::vulkan::object_data<VK_OBJECT_TYPE_QUERY_POOL> data;
	data.type = create_info.queryType;

	device_impl->register_object<VK_OBJECT_TYPE_QUERY_POOL>(*pQueryPool, std::move(data));

	reshade::invoke_addon_event<reshade::addon_event::init_query_pool>(
		device_impl, reshade::vulkan::convert_query_type(create_info.queryType), create_info.queryCount, reshade::api::query_pool { (uint64_t)*pQueryPool });
#endif

	return result;
}
void     VKAPI_CALL vkDestroyQueryPool(VkDevice device, VkQueryPool queryPool, const VkAllocationCallbacks *pAllocator)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(device));
	GET_DISPATCH_PTR_FROM(DestroyQueryPool, device_impl);

#if RESHADE_ADDON
	reshade::invoke_addon_event<reshade::addon_event::destroy_query_pool>(device_impl, reshade::api::query_pool { (uint64_t)queryPool });

	device_impl->unregister_object<VK_OBJECT_TYPE_QUERY_POOL>(queryPool);
#endif

	trampoline(device, queryPool, pAllocator);
}

VkResult VKAPI_CALL vkGetQueryPoolResults(VkDevice device, VkQueryPool queryPool, uint32_t firstQuery, uint32_t queryCount, size_t dataSize, void *pData, VkDeviceSize stride, VkQueryResultFlags flags)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(device));
	GET_DISPATCH_PTR_FROM(GetQueryPoolResults, device_impl);

#if RESHADE_ADDON
	assert(stride <= std::numeric_limits<uint32_t>::max());

	if (reshade::invoke_addon_event<reshade::addon_event::get_query_pool_results>(device_impl, reshade::api::query_pool { (uint64_t)queryPool }, firstQuery, queryCount, pData, static_cast<uint32_t>(stride)))
		return VK_SUCCESS;
#endif

	return trampoline(device, queryPool, firstQuery, queryCount, dataSize, pData, stride, flags);
}

VkResult VKAPI_CALL vkCreateBuffer(VkDevice device, const VkBufferCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkBuffer *pBuffer)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(device));
	GET_DISPATCH_PTR_FROM(CreateBuffer, device_impl);

	assert(pCreateInfo != nullptr && pBuffer != nullptr);

#if RESHADE_ADDON
	VkBufferCreateInfo create_info = *pCreateInfo;
	auto desc = reshade::vulkan::convert_resource_desc(create_info);
	assert(desc.heap == reshade::api::memory_heap::unknown);

	if (reshade::invoke_addon_event<reshade::addon_event::create_resource>(device_impl, desc, nullptr, reshade::api::resource_usage::undefined))
	{
		reshade::vulkan::convert_resource_desc(desc, create_info);
		pCreateInfo = &create_info;
	}
#endif

	const VkResult result = trampoline(device, pCreateInfo, pAllocator, pBuffer);
	if (result < VK_SUCCESS)
	{
#if RESHADE_VERBOSE_LOG
		LOG(WARN) << "vkCreateBuffer" << " failed with error code " << result << '.';
#endif
		return result;
	}

#if RESHADE_ADDON
	reshade::vulkan::object_data<VK_OBJECT_TYPE_BUFFER> data;
	data.allocation = VK_NULL_HANDLE;
	data.create_info = create_info;
	data.create_info.pNext = nullptr; // Clear out structure chain pointer, since it becomes invalid once leaving the current scope

	device_impl->register_object<VK_OBJECT_TYPE_BUFFER>(*pBuffer, std::move(data));
#endif

	return result;
}
void     VKAPI_CALL vkDestroyBuffer(VkDevice device, VkBuffer buffer, const VkAllocationCallbacks *pAllocator)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(device));
	GET_DISPATCH_PTR_FROM(DestroyBuffer, device_impl);

#if RESHADE_ADDON
	reshade::invoke_addon_event<reshade::addon_event::destroy_resource>(device_impl, reshade::api::resource { (uint64_t)buffer });

	device_impl->unregister_object<VK_OBJECT_TYPE_BUFFER>(buffer);
#endif

	trampoline(device, buffer, pAllocator);
}

VkResult VKAPI_CALL vkCreateBufferView(VkDevice device, const VkBufferViewCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkBufferView *pView)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(device));
	GET_DISPATCH_PTR_FROM(CreateBufferView, device_impl);

	assert(pCreateInfo != nullptr && pView != nullptr);

#if RESHADE_ADDON
	VkBufferViewCreateInfo create_info = *pCreateInfo;
	auto desc = reshade::vulkan::convert_resource_view_desc(create_info);

	if (reshade::invoke_addon_event<reshade::addon_event::create_resource_view>(device_impl, reshade::api::resource { (uint64_t)pCreateInfo->buffer }, reshade::api::resource_usage::undefined, desc))
	{
		reshade::vulkan::convert_resource_view_desc(desc, create_info);
		pCreateInfo = &create_info;
	}
#endif

	const VkResult result = trampoline(device, pCreateInfo, pAllocator, pView);
	if (result < VK_SUCCESS)
	{
#if RESHADE_VERBOSE_LOG
		LOG(WARN) << "vkCreateBufferView" << " failed with error code " << result << '.';
#endif
		return result;
	}

#if RESHADE_ADDON
	reshade::vulkan::object_data<VK_OBJECT_TYPE_BUFFER_VIEW> data;
	data.create_info = create_info;
	data.create_info.pNext = nullptr; // Clear out structure chain pointer, since it becomes invalid once leaving the current scope

	device_impl->register_object<VK_OBJECT_TYPE_BUFFER_VIEW>(*pView, std::move(data));

	reshade::invoke_addon_event<reshade::addon_event::init_resource_view>(
		device_impl, reshade::api::resource { (uint64_t)pCreateInfo->buffer }, reshade::api::resource_usage::undefined, desc, reshade::api::resource_view { (uint64_t)*pView });
#endif

	return result;
}
void     VKAPI_CALL vkDestroyBufferView(VkDevice device, VkBufferView bufferView, const VkAllocationCallbacks *pAllocator)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(device));
	GET_DISPATCH_PTR_FROM(DestroyBufferView, device_impl);

#if RESHADE_ADDON
	reshade::invoke_addon_event<reshade::addon_event::destroy_resource_view>(device_impl, reshade::api::resource_view { (uint64_t)bufferView });

	device_impl->unregister_object<VK_OBJECT_TYPE_BUFFER_VIEW>(bufferView);
#endif

	trampoline(device, bufferView, pAllocator);
}

VkResult VKAPI_CALL vkCreateImage(VkDevice device, const VkImageCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkImage *pImage)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(device));
	GET_DISPATCH_PTR_FROM(CreateImage, device_impl);

	assert(pCreateInfo != nullptr && pImage != nullptr);

#if RESHADE_ADDON
	VkImageCreateInfo create_info = *pCreateInfo;
	auto desc = reshade::vulkan::convert_resource_desc(create_info);
	assert(desc.heap == reshade::api::memory_heap::unknown);

	if (reshade::invoke_addon_event<reshade::addon_event::create_resource>(device_impl, desc, nullptr, pCreateInfo->initialLayout == VK_IMAGE_LAYOUT_PREINITIALIZED ? reshade::api::resource_usage::cpu_access : reshade::api::resource_usage::undefined))
	{
		reshade::vulkan::convert_resource_desc(desc, create_info);
		pCreateInfo = &create_info;
	}
#endif

	const VkResult result = trampoline(device, pCreateInfo, pAllocator, pImage);
	if (result < VK_SUCCESS)
	{
#if RESHADE_VERBOSE_LOG
		LOG(WARN) << "vkCreateImage" << " failed with error code " << result << '.';
#endif
		return result;
	}

#if RESHADE_ADDON
	reshade::vulkan::object_data<VK_OBJECT_TYPE_IMAGE> data;
	data.allocation = VK_NULL_HANDLE;
	data.create_info = create_info;
	data.create_info.pNext = nullptr; // Clear out structure chain pointer, since it becomes invalid once leaving the current scope

	device_impl->register_object<VK_OBJECT_TYPE_IMAGE>(*pImage, std::move(data));
#endif

	return result;
}
void     VKAPI_CALL vkDestroyImage(VkDevice device, VkImage image, const VkAllocationCallbacks *pAllocator)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(device));
	GET_DISPATCH_PTR_FROM(DestroyImage, device_impl);

#if RESHADE_ADDON
	reshade::invoke_addon_event<reshade::addon_event::destroy_resource>(device_impl, reshade::api::resource { (uint64_t)image });

	destroy_default_view(device_impl, image);

	device_impl->unregister_object<VK_OBJECT_TYPE_IMAGE>(image);
#endif

	trampoline(device, image, pAllocator);
}

VkResult VKAPI_CALL vkCreateImageView(VkDevice device, const VkImageViewCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkImageView *pView)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(device));
	GET_DISPATCH_PTR_FROM(CreateImageView, device_impl);

	assert(pCreateInfo != nullptr && pView != nullptr);

#if RESHADE_ADDON
	VkImageViewCreateInfo create_info = *pCreateInfo;
	auto desc = reshade::vulkan::convert_resource_view_desc(create_info);

	if (reshade::invoke_addon_event<reshade::addon_event::create_resource_view>(device_impl, reshade::api::resource { (uint64_t)pCreateInfo->image }, reshade::api::resource_usage::undefined, desc))
	{
		reshade::vulkan::convert_resource_view_desc(desc, create_info);
		pCreateInfo = &create_info;
	}
#endif

	const VkResult result = trampoline(device, pCreateInfo, pAllocator, pView);
	if (result < VK_SUCCESS)
	{
#if RESHADE_VERBOSE_LOG
		LOG(WARN) << "vkCreateImageView" << " failed with error code " << result << '.';
#endif
		return result;
	}

#if RESHADE_ADDON
	reshade::vulkan::object_data<VK_OBJECT_TYPE_IMAGE_VIEW> data;
	data.create_info = create_info;
	data.create_info.pNext = nullptr; // Clear out structure chain pointer, since it becomes invalid once leaving the current scope

	device_impl->register_object<VK_OBJECT_TYPE_IMAGE_VIEW>(*pView, std::move(data));

	reshade::invoke_addon_event<reshade::addon_event::init_resource_view>(
		device_impl, reshade::api::resource { (uint64_t)pCreateInfo->image }, reshade::api::resource_usage::undefined, desc, reshade::api::resource_view { (uint64_t)*pView });
#endif

	return result;
}
void     VKAPI_CALL vkDestroyImageView(VkDevice device, VkImageView imageView, const VkAllocationCallbacks *pAllocator)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(device));
	GET_DISPATCH_PTR_FROM(DestroyImageView, device_impl);

#if RESHADE_ADDON
	reshade::invoke_addon_event<reshade::addon_event::destroy_resource_view>(device_impl, reshade::api::resource_view { (uint64_t)imageView });

	device_impl->unregister_object<VK_OBJECT_TYPE_IMAGE_VIEW>(imageView);
#endif

	trampoline(device, imageView, pAllocator);
}

VkResult VKAPI_CALL vkCreateShaderModule(VkDevice device, const VkShaderModuleCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkShaderModule *pShaderModule)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(device));
	GET_DISPATCH_PTR_FROM(CreateShaderModule, device_impl);

	assert(pCreateInfo != nullptr && pShaderModule != nullptr);

	const VkResult result = trampoline(device, pCreateInfo, pAllocator, pShaderModule);
	if (result < VK_SUCCESS)
	{
#if RESHADE_VERBOSE_LOG
		LOG(WARN) << "vkCreateShaderModule" << " failed with error code " << result << '.';
#endif
		return result;
	}

#if RESHADE_ADDON
	reshade::vulkan::object_data<VK_OBJECT_TYPE_SHADER_MODULE> data;
	data.spirv.assign(reinterpret_cast<const uint8_t *>(pCreateInfo->pCode), reinterpret_cast<const uint8_t *>(pCreateInfo->pCode) + pCreateInfo->codeSize);

	device_impl->register_object<VK_OBJECT_TYPE_SHADER_MODULE>(*pShaderModule, std::move(data));
#endif

	return result;
}
void     VKAPI_CALL vkDestroyShaderModule(VkDevice device, VkShaderModule shaderModule, const VkAllocationCallbacks *pAllocator)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(device));
	GET_DISPATCH_PTR_FROM(DestroyShaderModule, device_impl);

#if RESHADE_ADDON
	device_impl->unregister_object<VK_OBJECT_TYPE_SHADER_MODULE>(shaderModule);
#endif

	trampoline(device, shaderModule, pAllocator);
}

VkResult VKAPI_CALL vkCreateGraphicsPipelines(VkDevice device, VkPipelineCache pipelineCache, uint32_t createInfoCount, const VkGraphicsPipelineCreateInfo *pCreateInfos, const VkAllocationCallbacks *pAllocator, VkPipeline *pPipelines)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(device));
	GET_DISPATCH_PTR_FROM(CreateGraphicsPipelines, device_impl);

#if RESHADE_ADDON && !RESHADE_ADDON_LITE
	VkResult result = VK_SUCCESS;
	for (uint32_t i = 0; i < createInfoCount; ++i)
	{
		const VkGraphicsPipelineCreateInfo &create_info = pCreateInfos[i];

		reshade::api::shader_desc vs_desc = {};
		reshade::api::shader_desc hs_desc = {};
		reshade::api::shader_desc ds_desc = {};
		reshade::api::shader_desc gs_desc = {};
		reshade::api::shader_desc ps_desc = {};
		auto stream_output_desc = reshade::vulkan::convert_stream_output_desc(create_info.pRasterizationState);
		auto blend_desc = reshade::vulkan::convert_blend_desc(create_info.pColorBlendState, create_info.pMultisampleState);
		auto rasterizer_desc = reshade::vulkan::convert_rasterizer_desc(create_info.pRasterizationState, create_info.pMultisampleState);
		auto depth_stencil_desc = reshade::vulkan::convert_depth_stencil_desc(create_info.pDepthStencilState);
		auto input_layout = reshade::vulkan::convert_input_layout_desc(create_info.pVertexInputState);
		reshade::api::primitive_topology topology = (create_info.pInputAssemblyState != nullptr) ? reshade::vulkan::convert_primitive_topology(create_info.pInputAssemblyState->topology) : reshade::api::primitive_topology::undefined;
		reshade::api::format depth_stencil_format = reshade::api::format::unknown;
		reshade::api::format render_target_formats[8] = {};
		uint32_t sample_mask = (create_info.pMultisampleState != nullptr && create_info.pMultisampleState->pSampleMask != nullptr) ? *create_info.pMultisampleState->pSampleMask : UINT32_MAX;
		uint32_t sample_count = (create_info.pMultisampleState != nullptr) ? static_cast<uint32_t>(create_info.pMultisampleState->rasterizationSamples) : 1;
		uint32_t viewport_count = (create_info.pViewportState != nullptr) ? create_info.pViewportState->viewportCount : 1;
		auto dynamic_states = reshade::vulkan::convert_dynamic_states(create_info.pDynamicState);

		for (uint32_t k = 0; k < create_info.stageCount; ++k)
		{
			const VkPipelineShaderStageCreateInfo &stage = create_info.pStages[k];

			const auto module_data = device_impl->get_private_data_for_object<VK_OBJECT_TYPE_SHADER_MODULE>(stage.module);

			switch (stage.stage)
			{
			case VK_SHADER_STAGE_VERTEX_BIT:
				vs_desc.code = module_data->spirv.data();
				vs_desc.code_size = module_data->spirv.size();
				vs_desc.entry_point = stage.pName;
				break;
			case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:
				hs_desc.code = module_data->spirv.data();
				hs_desc.code_size = module_data->spirv.size();
				hs_desc.entry_point = stage.pName;
				break;
			case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:
				ds_desc.code = module_data->spirv.data();
				ds_desc.code_size = module_data->spirv.size();
				ds_desc.entry_point = stage.pName;
				break;
			case VK_SHADER_STAGE_GEOMETRY_BIT:
				gs_desc.code = module_data->spirv.data();
				gs_desc.code_size = module_data->spirv.size();
				gs_desc.entry_point = stage.pName;
				break;
			case VK_SHADER_STAGE_FRAGMENT_BIT:
				ps_desc.code = module_data->spirv.data();
				ps_desc.code_size = module_data->spirv.size();
				ps_desc.entry_point = stage.pName;
				break;
			}
		}

		if ((hs_desc.code_size != 0 || ds_desc.code_size != 0) && create_info.pTessellationState != nullptr)
		{
			const VkPipelineTessellationStateCreateInfo &tessellation_state_info = *create_info.pTessellationState;

			assert(topology == reshade::api::primitive_topology::patch_list_01_cp);
			topology = static_cast<reshade::api::primitive_topology>(static_cast<uint32_t>(reshade::api::primitive_topology::patch_list_01_cp) + tessellation_state_info.patchControlPoints - 1);
		}

		uint32_t render_target_count = 0;

		if (create_info.renderPass != VK_NULL_HANDLE)
		{
			const auto render_pass_data = device_impl->get_private_data_for_object<VK_OBJECT_TYPE_RENDER_PASS>(create_info.renderPass);

			const auto &subpass = render_pass_data->subpasses[create_info.subpass];

			if (subpass.pDepthStencilAttachment != nullptr)
			{
				const uint32_t a = subpass.pDepthStencilAttachment->attachment;
				if (a != VK_ATTACHMENT_UNUSED)
					depth_stencil_format = reshade::vulkan::convert_format(render_pass_data->attachments[a].format);
			}

			render_target_count = std::min(subpass.colorAttachmentCount, 8u);

			for (uint32_t k = 0; k < render_target_count; ++k)
			{
				const uint32_t a = subpass.pColorAttachments[k].attachment;
				if (a != VK_ATTACHMENT_UNUSED)
					render_target_formats[i] = reshade::vulkan::convert_format(render_pass_data->attachments[a].format);
			}
		}
		else
		{
			if (const auto dynamic_rendering_info = find_in_structure_chain<VkPipelineRenderingCreateInfo>(create_info.pNext, VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO))
			{
				if (dynamic_rendering_info->depthAttachmentFormat != VK_FORMAT_UNDEFINED)
					depth_stencil_format = reshade::vulkan::convert_format(dynamic_rendering_info->depthAttachmentFormat);
				else
					depth_stencil_format = reshade::vulkan::convert_format(dynamic_rendering_info->stencilAttachmentFormat);

				render_target_count = std::min(dynamic_rendering_info->colorAttachmentCount, 8u);

				for (uint32_t k = 0; k < render_target_count; ++k)
					render_target_formats[k] = reshade::vulkan::convert_format(dynamic_rendering_info->pColorAttachmentFormats[k]);
			}
		}

		const reshade::api::pipeline_subobject subobjects[] = {
			{ reshade::api::pipeline_subobject_type::vertex_shader, 1, &vs_desc },
			{ reshade::api::pipeline_subobject_type::pixel_shader, 1, &ps_desc },
			{ reshade::api::pipeline_subobject_type::domain_shader, 1, &ds_desc },
			{ reshade::api::pipeline_subobject_type::hull_shader, 1, &hs_desc },
			{ reshade::api::pipeline_subobject_type::geometry_shader, 1, &gs_desc },
			{ reshade::api::pipeline_subobject_type::stream_output_state, 1, &stream_output_desc },
			{ reshade::api::pipeline_subobject_type::blend_state, 1, &blend_desc },
			{ reshade::api::pipeline_subobject_type::sample_mask, 1, &sample_mask },
			{ reshade::api::pipeline_subobject_type::rasterizer_state, 1, &rasterizer_desc },
			{ reshade::api::pipeline_subobject_type::depth_stencil_state, 1, &depth_stencil_desc },
			{ reshade::api::pipeline_subobject_type::input_layout, static_cast<uint32_t>(input_layout.size()), input_layout.data() },
			{ reshade::api::pipeline_subobject_type::primitive_topology, 1, &topology },
			{ reshade::api::pipeline_subobject_type::render_target_formats, render_target_count, render_target_formats },
			{ reshade::api::pipeline_subobject_type::depth_stencil_format, 1, &depth_stencil_format },
			{ reshade::api::pipeline_subobject_type::sample_count, 1, &sample_count },
			{ reshade::api::pipeline_subobject_type::viewport_count, 1, &viewport_count },
			{ reshade::api::pipeline_subobject_type::dynamic_pipeline_states, static_cast<uint32_t>(dynamic_states.size()), dynamic_states.data() },
		};

		if (reshade::invoke_addon_event<reshade::addon_event::create_pipeline>(device_impl, reshade::api::pipeline_layout { (uint64_t)create_info.layout }, static_cast<uint32_t>(std::size(subobjects)), subobjects))
		{
			static_assert(sizeof(*pPipelines) == sizeof(reshade::api::pipeline));

			result = device_impl->create_pipeline(
				reshade::api::pipeline_layout { (uint64_t)create_info.layout }, static_cast<uint32_t>(std::size(subobjects)), subobjects, reinterpret_cast<reshade::api::pipeline *>(&pPipelines[i])) ? VK_SUCCESS : VK_ERROR_OUT_OF_HOST_MEMORY;
		}
		else
		{
			result = trampoline(device, pipelineCache, 1, &create_info, pAllocator, &pPipelines[i]);
		}

		if (result >= VK_SUCCESS)
		{
			reshade::invoke_addon_event<reshade::addon_event::init_pipeline>(
				device_impl, reshade::api::pipeline_layout { (uint64_t)create_info.layout }, static_cast<uint32_t>(std::size(subobjects)), subobjects, reshade::api::pipeline{ (uint64_t)pPipelines[i] });
		}
		else
		{
#if RESHADE_VERBOSE_LOG
			LOG(WARN) << "vkCreateGraphicsPipelines" << " failed with error code " << result << '.';
#endif

			for (uint32_t k = 0; k < i; ++k)
				vkDestroyPipeline(device, pPipelines[k], pAllocator);
			for (uint32_t k = 0; k < createInfoCount; ++k)
				pPipelines[k] = VK_NULL_HANDLE;
			break;
		}
	}

	return result;
#else
	return trampoline(device, pipelineCache, createInfoCount, pCreateInfos, pAllocator, pPipelines);
#endif
}
VkResult VKAPI_CALL vkCreateComputePipelines(VkDevice device, VkPipelineCache pipelineCache, uint32_t createInfoCount, const VkComputePipelineCreateInfo *pCreateInfos, const VkAllocationCallbacks *pAllocator, VkPipeline *pPipelines)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(device));
	GET_DISPATCH_PTR_FROM(CreateComputePipelines, device_impl);

#if RESHADE_ADDON && !RESHADE_ADDON_LITE
	VkResult result = VK_SUCCESS;
	for (uint32_t i = 0; i < createInfoCount; ++i)
	{
		const VkComputePipelineCreateInfo &create_info = pCreateInfos[i];

		assert(create_info.stage.stage == VK_SHADER_STAGE_COMPUTE_BIT);
		const auto module_data = device_impl->get_private_data_for_object<VK_OBJECT_TYPE_SHADER_MODULE>(create_info.stage.module);

		reshade::api::shader_desc cs_desc = {};
		cs_desc.code = module_data->spirv.data();
		cs_desc.code_size = module_data->spirv.size();
		cs_desc.entry_point = create_info.stage.pName;

		const reshade::api::pipeline_subobject subobjects[] = {
			{ reshade::api::pipeline_subobject_type::compute_shader, 1, &cs_desc }
		};

		if (reshade::invoke_addon_event<reshade::addon_event::create_pipeline>(device_impl, reshade::api::pipeline_layout { (uint64_t)create_info.layout }, static_cast<uint32_t>(std::size(subobjects)), subobjects))
		{
			result = device_impl->create_pipeline(
				reshade::api::pipeline_layout { (uint64_t)create_info.layout }, static_cast<uint32_t>(std::size(subobjects)), subobjects, reinterpret_cast<reshade::api::pipeline *>(&pPipelines[i])) ? VK_SUCCESS : VK_ERROR_OUT_OF_HOST_MEMORY;
		}
		else
		{
			result = trampoline(device, pipelineCache, 1, &create_info, pAllocator, &pPipelines[i]);
		}

		if (result >= VK_SUCCESS)
		{
			reshade::invoke_addon_event<reshade::addon_event::init_pipeline>(
				device_impl, reshade::api::pipeline_layout { (uint64_t)create_info.layout }, static_cast<uint32_t>(std::size(subobjects)), subobjects, reshade::api::pipeline { (uint64_t)pPipelines[i] });
		}
		else
		{
#if RESHADE_VERBOSE_LOG
			LOG(WARN) << "vkCreateComputePipelines" << " failed with error code " << result << '.';
#endif

			for (uint32_t k = 0; k < i; ++k)
				vkDestroyPipeline(device, pPipelines[k], pAllocator);
			for (uint32_t k = 0; k < createInfoCount; ++k)
				pPipelines[k] = VK_NULL_HANDLE;
			break;
		}
	}

	return result;
#else
	return trampoline(device, pipelineCache, createInfoCount, pCreateInfos, pAllocator, pPipelines);
#endif
}
void     VKAPI_CALL vkDestroyPipeline(VkDevice device, VkPipeline pipeline, const VkAllocationCallbacks *pAllocator)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(device));
	GET_DISPATCH_PTR_FROM(DestroyPipeline, device_impl);

#if RESHADE_ADDON && !RESHADE_ADDON_LITE
	reshade::invoke_addon_event<reshade::addon_event::destroy_pipeline>(device_impl, reshade::api::pipeline { (uint64_t)pipeline });
#endif

	trampoline(device, pipeline, pAllocator);
}

VkResult VKAPI_CALL vkCreatePipelineLayout(VkDevice device, const VkPipelineLayoutCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkPipelineLayout *pPipelineLayout)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(device));
	GET_DISPATCH_PTR_FROM(CreatePipelineLayout, device_impl);

	assert(pCreateInfo != nullptr && pPipelineLayout != nullptr);

	const VkResult result = trampoline(device, pCreateInfo, pAllocator, pPipelineLayout);
	if (result < VK_SUCCESS)
	{
#if RESHADE_VERBOSE_LOG
		LOG(WARN) << "vkCreatePipelineLayout" << " failed with error code " << result << '.';
#endif
		return result;
	}

#if RESHADE_ADDON && !RESHADE_ADDON_LITE
	const uint32_t set_desc_count = pCreateInfo->setLayoutCount;
	const uint32_t total_param_count = set_desc_count + pCreateInfo->pushConstantRangeCount;

	reshade::vulkan::object_data<VK_OBJECT_TYPE_PIPELINE_LAYOUT> data;
	data.set_layouts.assign(pCreateInfo->pSetLayouts, pCreateInfo->pSetLayouts + pCreateInfo->setLayoutCount);

	std::vector<reshade::api::pipeline_layout_param> params(total_param_count);

	for (uint32_t i = 0; i < set_desc_count; ++i)
	{
		const auto set_layout_impl = device_impl->get_private_data_for_object<VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT>(pCreateInfo->pSetLayouts[i]);

		if (set_layout_impl->push_descriptors)
		{
			assert(set_layout_impl->ranges.size() == 1); // TODO

			params[i].type = reshade::api::pipeline_layout_param_type::push_descriptors;
			params[i].push_descriptors = set_layout_impl->ranges[0];
		}
		else
		{
			params[i].type = reshade::api::pipeline_layout_param_type::descriptor_set;
			params[i].descriptor_set.count = static_cast<uint32_t>(set_layout_impl->ranges.size());
			params[i].descriptor_set.ranges = set_layout_impl->ranges.data();
		}
	}

	for (uint32_t i = set_desc_count; i < total_param_count; ++i)
	{
		const VkPushConstantRange &push_constant_range = pCreateInfo->pPushConstantRanges[i];

		params[i].type = reshade::api::pipeline_layout_param_type::push_constants;
		params[i].push_constants.count = push_constant_range.offset + push_constant_range.size;
		params[i].push_constants.visibility = static_cast<reshade::api::shader_stage>(push_constant_range.stageFlags);
	}

	device_impl->register_object<VK_OBJECT_TYPE_PIPELINE_LAYOUT>(*pPipelineLayout, data);

	reshade::invoke_addon_event<reshade::addon_event::init_pipeline_layout>(device_impl, total_param_count, params.data(), reshade::api::pipeline_layout { (uint64_t)*pPipelineLayout });
#endif

	return result;
}
void     VKAPI_CALL vkDestroyPipelineLayout(VkDevice device, VkPipelineLayout pipelineLayout, const VkAllocationCallbacks *pAllocator)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(device));
	GET_DISPATCH_PTR_FROM(DestroyPipelineLayout, device_impl);

#if RESHADE_ADDON && !RESHADE_ADDON_LITE
	reshade::invoke_addon_event<reshade::addon_event::destroy_pipeline_layout>(device_impl, reshade::api::pipeline_layout { (uint64_t)pipelineLayout });

	device_impl->unregister_object<VK_OBJECT_TYPE_PIPELINE_LAYOUT>(pipelineLayout);
#endif

	trampoline(device, pipelineLayout, pAllocator);
}

VkResult VKAPI_CALL vkCreateSampler(VkDevice device, const VkSamplerCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkSampler *pSampler)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(device));
	GET_DISPATCH_PTR_FROM(CreateSampler, device_impl);

	assert(pCreateInfo != nullptr && pSampler != nullptr);

#if RESHADE_ADDON
	VkSamplerCreateInfo create_info = *pCreateInfo;
	auto desc = reshade::vulkan::convert_sampler_desc(create_info);

	if (reshade::invoke_addon_event<reshade::addon_event::create_sampler>(device_impl, desc))
	{
		reshade::vulkan::convert_sampler_desc(desc, create_info);
		pCreateInfo = &create_info;
	}
#endif

	const VkResult result = trampoline(device, pCreateInfo, pAllocator, pSampler);
	if (result < VK_SUCCESS)
	{
#if RESHADE_VERBOSE_LOG
		LOG(WARN) << "vkCreateSampler" << " failed with error code " << result << '.';
#endif
		return result;
	}

#if RESHADE_ADDON
	reshade::invoke_addon_event<reshade::addon_event::init_sampler>(device_impl, desc, reshade::api::sampler { (uint64_t)*pSampler });
#endif

	return result;
}
void     VKAPI_CALL vkDestroySampler(VkDevice device, VkSampler sampler, const VkAllocationCallbacks *pAllocator)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(device));
	GET_DISPATCH_PTR_FROM(DestroySampler, device_impl);

#if RESHADE_ADDON
	reshade::invoke_addon_event<reshade::addon_event::destroy_sampler>(device_impl, reshade::api::sampler { (uint64_t)sampler });
#endif

	trampoline(device, sampler, pAllocator);
}

VkResult VKAPI_CALL vkCreateDescriptorSetLayout(VkDevice device, const VkDescriptorSetLayoutCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkDescriptorSetLayout *pSetLayout)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(device));
	GET_DISPATCH_PTR_FROM(CreateDescriptorSetLayout, device_impl);

	assert(pCreateInfo != nullptr && pSetLayout != nullptr);

	const VkResult result = trampoline(device, pCreateInfo, pAllocator, pSetLayout);
	if (result < VK_SUCCESS)
	{
#if RESHADE_VERBOSE_LOG
		LOG(WARN) << "vkCreateDescriptorSetLayout" << " failed with error code " << result << '.';
#endif
		return result;
	}

#if RESHADE_ADDON && !RESHADE_ADDON_LITE
	reshade::vulkan::object_data<VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT> data;
	data.ranges.resize(pCreateInfo->bindingCount);
	data.num_descriptors = 0;
	data.push_descriptors = (pCreateInfo->flags & VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR) != 0;

	if (pCreateInfo->bindingCount != 0)
	{
		assert(pCreateInfo->pBindings != nullptr);

		data.binding_to_offset.reserve(pCreateInfo->bindingCount);

		for (uint32_t i = 0; i < pCreateInfo->bindingCount; ++i)
		{
			const VkDescriptorSetLayoutBinding &binding = pCreateInfo->pBindings[i];

			if (binding.binding >= data.binding_to_offset.size())
				data.binding_to_offset.resize(binding.binding + 1);
			data.binding_to_offset[binding.binding] = binding.descriptorCount;

			data.ranges[i].binding = binding.binding;
			data.ranges[i].dx_register_index = 0;
			data.ranges[i].dx_register_space = 0;
			data.ranges[i].count = binding.descriptorCount;
			data.ranges[i].array_size = binding.descriptorCount;
			data.ranges[i].type = reshade::vulkan::convert_descriptor_type(binding.descriptorType);
			data.ranges[i].visibility = static_cast<reshade::api::shader_stage>(binding.stageFlags);

			data.num_descriptors += binding.descriptorCount;
		}

		for (size_t i = 1; i < data.binding_to_offset.size(); ++i)
			data.binding_to_offset[i] += data.binding_to_offset[i - 1];
	}

	device_impl->register_object<VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT>(*pSetLayout, std::move(data));
#endif

	return result;
}
void     VKAPI_CALL vkDestroyDescriptorSetLayout(VkDevice device, VkDescriptorSetLayout descriptorSetLayout, const VkAllocationCallbacks *pAllocator)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(device));
	GET_DISPATCH_PTR_FROM(DestroyDescriptorSetLayout, device_impl);

#if RESHADE_ADDON && !RESHADE_ADDON_LITE
	device_impl->unregister_object<VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT>(descriptorSetLayout);
#endif

	trampoline(device, descriptorSetLayout, pAllocator);
}

VkResult VKAPI_CALL vkCreateDescriptorPool(VkDevice device, const VkDescriptorPoolCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkDescriptorPool *pDescriptorPool)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(device));
	GET_DISPATCH_PTR_FROM(CreateDescriptorPool, device_impl);

	assert(pCreateInfo != nullptr && pDescriptorPool != nullptr);

	const VkResult result = trampoline(device, pCreateInfo, pAllocator, pDescriptorPool);
	if (result < VK_SUCCESS)
	{
#if RESHADE_VERBOSE_LOG
		LOG(WARN) << "vkCreateDescriptorPool" << " failed with error code " << result << '.';
#endif
		return result;
	}

#if RESHADE_ADDON
	reshade::vulkan::object_data<VK_OBJECT_TYPE_DESCRIPTOR_POOL> data;
	data.max_sets = pCreateInfo->maxSets;
	data.max_descriptors = 0;
	data.next_set = 0;
	data.next_offset = 0;
	data.sets.resize(data.max_sets);

	for (uint32_t i = 0; i < pCreateInfo->poolSizeCount; ++i)
		data.max_descriptors += pCreateInfo->pPoolSizes[i].descriptorCount;

	device_impl->register_object<VK_OBJECT_TYPE_DESCRIPTOR_POOL>(*pDescriptorPool, std::move(data));
#endif

	return result;
}
void     VKAPI_CALL vkDestroyDescriptorPool(VkDevice device, VkDescriptorPool descriptorPool, const VkAllocationCallbacks *pAllocator)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(device));
	GET_DISPATCH_PTR_FROM(DestroyDescriptorPool, device_impl);

#if RESHADE_ADDON
	device_impl->unregister_object<VK_OBJECT_TYPE_DESCRIPTOR_POOL>(descriptorPool);
#endif

	trampoline(device, descriptorPool, pAllocator);
}

VkResult VKAPI_CALL vkResetDescriptorPool(VkDevice device, VkDescriptorPool descriptorPool, VkDescriptorPoolResetFlags flags)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(device));
	GET_DISPATCH_PTR_FROM(ResetDescriptorPool, device_impl);

#if RESHADE_ADDON
	const auto pool_data = device_impl->get_private_data_for_object<VK_OBJECT_TYPE_DESCRIPTOR_POOL>(descriptorPool);

	pool_data->next_set = 0;
	pool_data->next_offset = 0;
#endif

	return trampoline(device, descriptorPool, flags);
}
VkResult VKAPI_CALL vkAllocateDescriptorSets(VkDevice device, const VkDescriptorSetAllocateInfo *pAllocateInfo, VkDescriptorSet *pDescriptorSets)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(device));
	GET_DISPATCH_PTR_FROM(AllocateDescriptorSets, device_impl);

	assert(pAllocateInfo != nullptr && pDescriptorSets != nullptr);

	const VkResult result = trampoline(device, pAllocateInfo, pDescriptorSets);
	if (result < VK_SUCCESS)
	{
#if RESHADE_VERBOSE_LOG
		LOG(WARN) << "vkAllocateDescriptorSets" << " failed with error code " << result << '.';
#endif
		return result;
	}

#if RESHADE_ADDON && !RESHADE_ADDON_LITE
	const auto pool_data = device_impl->get_private_data_for_object<VK_OBJECT_TYPE_DESCRIPTOR_POOL>(pAllocateInfo->descriptorPool);

	for (uint32_t i = 0; i < pAllocateInfo->descriptorSetCount; ++i)
	{
		const auto layout_data = device_impl->get_private_data_for_object<VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT>(pAllocateInfo->pSetLayouts[i]);

		reshade::vulkan::object_data<VK_OBJECT_TYPE_DESCRIPTOR_SET> *data = &pool_data->sets[pool_data->next_set++];
		data->pool = pAllocateInfo->descriptorPool;
		data->layout = pAllocateInfo->pSetLayouts[i];

		data->offset = pool_data->next_offset;
		pool_data->next_offset += layout_data->num_descriptors;

		if (pool_data->next_set >= pool_data->max_sets)
		{
			pool_data->next_set = 0;
		}
		if (pool_data->next_offset >= pool_data->max_descriptors)
		{
			// Out of pool memory, simply wrap around
			data->offset = 0;
			pool_data->next_offset = layout_data->num_descriptors;
		}

		device_impl->register_object(VK_OBJECT_TYPE_DESCRIPTOR_SET, (uint64_t)pDescriptorSets[i], data);
	}
#endif

	return result;
}
VkResult VKAPI_CALL vkFreeDescriptorSets(VkDevice device, VkDescriptorPool descriptorPool, uint32_t descriptorSetCount, const VkDescriptorSet *pDescriptorSets)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(device));
	GET_DISPATCH_PTR_FROM(FreeDescriptorSets, device_impl);

	assert(pDescriptorSets != nullptr);

	const VkResult result = trampoline(device, descriptorPool, descriptorSetCount, pDescriptorSets);
	if (result < VK_SUCCESS)
	{
#if RESHADE_VERBOSE_LOG
		LOG(WARN) << "vkFreeDescriptorSets" << " failed with error code " << result << '.';
#endif
		return result;
	}

#if RESHADE_ADDON && !RESHADE_ADDON_LITE
	for (uint32_t i = 0; i < descriptorSetCount; ++i)
	{
		device_impl->unregister_object(VK_OBJECT_TYPE_DESCRIPTOR_SET, (uint64_t)pDescriptorSets[i]);
	}
#endif

	return result;
}

void     VKAPI_CALL vkUpdateDescriptorSets(VkDevice device, uint32_t descriptorWriteCount, const VkWriteDescriptorSet *pDescriptorWrites, uint32_t descriptorCopyCount, const VkCopyDescriptorSet *pDescriptorCopies)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(device));
	GET_DISPATCH_PTR_FROM(UpdateDescriptorSets, device_impl);

#if RESHADE_ADDON && !RESHADE_ADDON_LITE
	if (descriptorWriteCount != 0 && reshade::has_addon_event<reshade::addon_event::update_descriptor_sets>())
	{
		temp_mem<reshade::api::descriptor_set_update> updates(descriptorWriteCount);

		uint32_t max_descriptors = 0;
		for (uint32_t i = 0; i < descriptorWriteCount; ++i)
			max_descriptors += pDescriptorWrites[i].descriptorCount;
		temp_mem<uint64_t> descriptors(max_descriptors * 2);

		for (uint32_t i = 0, j = 0; i < descriptorWriteCount; ++i)
		{
			const VkWriteDescriptorSet &write = pDescriptorWrites[i];

			reshade::api::descriptor_set_update &update = updates[i];
			update.set = { (uint64_t)write.dstSet };
			update.binding = write.dstBinding;
			update.array_offset = write.dstArrayElement;
			update.count = write.descriptorCount;
			update.type = reshade::vulkan::convert_descriptor_type(write.descriptorType);
			update.descriptors = descriptors.p + j;

			switch (write.descriptorType)
			{
			case VK_DESCRIPTOR_TYPE_SAMPLER:
				for (uint32_t k = 0; k < write.descriptorCount; ++k, ++j)
					descriptors[j + 0] = (uint64_t)write.pImageInfo[k].sampler;
				break;
			case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
				for (uint32_t k = 0; k < write.descriptorCount; ++k, j += 2)
					descriptors[j + 0] = (uint64_t)write.pImageInfo[k].sampler,
					descriptors[j + 1] = (uint64_t)write.pImageInfo[k].imageView;
				break;
			case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
			case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
				for (uint32_t k = 0; k < write.descriptorCount; ++k, ++j)
					descriptors[j + 0] = (uint64_t)write.pImageInfo[k].imageView;
				break;
			case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
			case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
				static_assert(sizeof(reshade::api::buffer_range) == sizeof(VkDescriptorBufferInfo));
				update.descriptors = write.pBufferInfo;
				break;
			}
		}

		if (reshade::invoke_addon_event<reshade::addon_event::update_descriptor_sets>(device_impl, descriptorWriteCount, updates.p))
			descriptorWriteCount = 0;
	}

	if (descriptorCopyCount != 0 && reshade::has_addon_event<reshade::addon_event::copy_descriptor_sets>())
	{
		temp_mem<reshade::api::descriptor_set_copy> copies(descriptorCopyCount);

		for (uint32_t i = 0; i < descriptorCopyCount; ++i)
		{
			const VkCopyDescriptorSet &internal_copy = pDescriptorCopies[i];

			reshade::api::descriptor_set_copy &copy = copies[i];
			copy.source_set = { (uint64_t)internal_copy.srcSet };
			copy.source_binding = internal_copy.srcBinding;
			copy.source_array_offset = internal_copy.srcArrayElement;
			copy.dest_set = { (uint64_t)internal_copy.dstSet };
			copy.dest_binding = internal_copy.dstBinding;
			copy.dest_array_offset = internal_copy.dstArrayElement;
			copy.count = internal_copy.descriptorCount;
		}

		if (reshade::invoke_addon_event<reshade::addon_event::copy_descriptor_sets>(device_impl, descriptorCopyCount, copies.p))
			descriptorCopyCount = 0;
	}
#endif

	trampoline(device, descriptorWriteCount, pDescriptorWrites, descriptorCopyCount, pDescriptorCopies);
}

VkResult VKAPI_CALL vkCreateFramebuffer(VkDevice device, const VkFramebufferCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkFramebuffer *pFramebuffer)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(device));
	GET_DISPATCH_PTR_FROM(CreateFramebuffer, device_impl);

	assert(pCreateInfo != nullptr && pFramebuffer != nullptr);

	const VkResult result = trampoline(device, pCreateInfo, pAllocator, pFramebuffer);
	if (result < VK_SUCCESS)
	{
#if RESHADE_VERBOSE_LOG
		LOG(WARN) << "vkCreateFramebuffer" << " failed with error code " << result << '.';
#endif
		return result;
	}

#if RESHADE_ADDON
	// Keep track of the frame buffer attachments
	reshade::vulkan::object_data<VK_OBJECT_TYPE_FRAMEBUFFER> data;
	data.attachments.assign(pCreateInfo->pAttachments, pCreateInfo->pAttachments + pCreateInfo->attachmentCount);

	device_impl->register_object<VK_OBJECT_TYPE_FRAMEBUFFER>(*pFramebuffer, std::move(data));
#endif

	return result;
}
void     VKAPI_CALL vkDestroyFramebuffer(VkDevice device, VkFramebuffer framebuffer, const VkAllocationCallbacks *pAllocator)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(device));
	GET_DISPATCH_PTR_FROM(DestroyFramebuffer, device_impl);

#if RESHADE_ADDON
	device_impl->unregister_object<VK_OBJECT_TYPE_FRAMEBUFFER>(framebuffer);
#endif

	trampoline(device, framebuffer, pAllocator);
}

VkResult VKAPI_CALL vkCreateRenderPass(VkDevice device, const VkRenderPassCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkRenderPass *pRenderPass)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(device));
	GET_DISPATCH_PTR_FROM(CreateRenderPass, device_impl);

	assert(pCreateInfo != nullptr && pRenderPass != nullptr);

	const VkResult result = trampoline(device, pCreateInfo, pAllocator, pRenderPass);
	if (result < VK_SUCCESS)
	{
#if RESHADE_VERBOSE_LOG
		LOG(WARN) << "vkCreateRenderPass" << " failed with error code " << result << '.';
#endif
		return result;
	}

#if RESHADE_ADDON
	reshade::vulkan::object_data<VK_OBJECT_TYPE_RENDER_PASS> data;
	data.subpasses.assign(pCreateInfo->pSubpasses, pCreateInfo->pSubpasses + pCreateInfo->subpassCount);
	data.attachments.assign(pCreateInfo->pAttachments, pCreateInfo->pAttachments + pCreateInfo->attachmentCount);

	for (VkSubpassDescription &subpass : data.subpasses)
	{
		const auto color_attachments = new VkAttachmentReference[subpass.colorAttachmentCount];
		std::memcpy(color_attachments, subpass.pColorAttachments, subpass.colorAttachmentCount * sizeof(VkAttachmentReference));
		subpass.pColorAttachments = color_attachments;

		if (subpass.pDepthStencilAttachment != nullptr)
		{
			const auto depth_stencil_attachment = new VkAttachmentReference;
			std::memcpy(depth_stencil_attachment, subpass.pDepthStencilAttachment, sizeof(VkAttachmentReference));
			subpass.pDepthStencilAttachment = depth_stencil_attachment;
		}
	}

	device_impl->register_object<VK_OBJECT_TYPE_RENDER_PASS>(*pRenderPass, std::move(data));
#endif

	return result;
}
VkResult VKAPI_CALL vkCreateRenderPass2(VkDevice device, const VkRenderPassCreateInfo2 *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkRenderPass *pRenderPass)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(device));
	GET_DISPATCH_PTR_FROM(CreateRenderPass2, device_impl);

	assert(pCreateInfo != nullptr && pRenderPass != nullptr);

	const VkResult result = trampoline(device, pCreateInfo, pAllocator, pRenderPass);
	if (result < VK_SUCCESS)
	{
#if RESHADE_VERBOSE_LOG
		LOG(WARN) << "vkCreateRenderPass2" << " failed with error code " << result << '.';
#endif
		return result;
	}

#if RESHADE_ADDON
	reshade::vulkan::object_data<VK_OBJECT_TYPE_RENDER_PASS> data;
	data.subpasses.resize(pCreateInfo->subpassCount);
	data.attachments.resize(pCreateInfo->attachmentCount);

	for (uint32_t i = 0; i < pCreateInfo->subpassCount; ++i)
	{
		VkSubpassDescription &dst_subpass = data.subpasses[i];
		const VkSubpassDescription2 &src_subpass = pCreateInfo->pSubpasses[i];

		const auto color_attachments = new VkAttachmentReference[src_subpass.colorAttachmentCount];
		for (uint32_t a = 0; a < src_subpass.colorAttachmentCount; ++a)
		{
			color_attachments[a].attachment = src_subpass.pColorAttachments[a].attachment;
			color_attachments[a].layout = src_subpass.pColorAttachments[a].layout;
		}

		dst_subpass.colorAttachmentCount = src_subpass.colorAttachmentCount;
		dst_subpass.pColorAttachments = color_attachments;

		if (src_subpass.pDepthStencilAttachment != nullptr)
		{
			const auto depth_stencil_attachment = new VkAttachmentReference;
			depth_stencil_attachment->attachment = src_subpass.pDepthStencilAttachment->attachment;
			depth_stencil_attachment->layout = src_subpass.pDepthStencilAttachment->layout;

			dst_subpass.pDepthStencilAttachment = depth_stencil_attachment;
		}
	}
	for (uint32_t i = 0; i < pCreateInfo->attachmentCount; ++i)
	{
		VkAttachmentDescription &dst_attachment = data.attachments[i];
		const VkAttachmentDescription2 &src_attachment = pCreateInfo->pAttachments[i];

		dst_attachment.flags = src_attachment.flags;
		dst_attachment.format = src_attachment.format;
		dst_attachment.samples = src_attachment.samples;
		dst_attachment.loadOp = src_attachment.loadOp;
		dst_attachment.storeOp = src_attachment.storeOp;
		dst_attachment.stencilLoadOp = src_attachment.stencilLoadOp;
		dst_attachment.stencilStoreOp = src_attachment.stencilStoreOp;
		dst_attachment.initialLayout = src_attachment.initialLayout;
		dst_attachment.finalLayout = src_attachment.finalLayout;
	}

	device_impl->register_object<VK_OBJECT_TYPE_RENDER_PASS>(*pRenderPass, std::move(data));
#endif

	return result;
}
void     VKAPI_CALL vkDestroyRenderPass(VkDevice device, VkRenderPass renderPass, const VkAllocationCallbacks *pAllocator)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(device));
	GET_DISPATCH_PTR_FROM(DestroyRenderPass, device_impl);

#if RESHADE_ADDON
	const auto data = device_impl->get_private_data_for_object<VK_OBJECT_TYPE_RENDER_PASS>(renderPass);

	for (VkSubpassDescription &subpass : data->subpasses)
	{
		delete subpass.pDepthStencilAttachment;
		delete[] subpass.pColorAttachments;
	}

	device_impl->unregister_object<VK_OBJECT_TYPE_RENDER_PASS>(renderPass);
#endif

	trampoline(device, renderPass, pAllocator);
}

VkResult VKAPI_CALL vkAllocateCommandBuffers(VkDevice device, const VkCommandBufferAllocateInfo *pAllocateInfo, VkCommandBuffer *pCommandBuffers)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(device));
	GET_DISPATCH_PTR_FROM(AllocateCommandBuffers, device_impl);

	const VkResult result = trampoline(device, pAllocateInfo, pCommandBuffers);
	if (result < VK_SUCCESS)
	{
#if RESHADE_VERBOSE_LOG
		LOG(WARN) << "vkAllocateCommandBuffers" << " failed with error code " << result << '.';
#endif
		return result;
	}

#if RESHADE_ADDON
	for (uint32_t i = 0; i < pAllocateInfo->commandBufferCount; ++i)
		device_impl->register_object<VK_OBJECT_TYPE_COMMAND_BUFFER>(pCommandBuffers[i], device_impl, pCommandBuffers[i]);
#endif

	return result;
}
void     VKAPI_CALL vkFreeCommandBuffers(VkDevice device, VkCommandPool commandPool, uint32_t commandBufferCount, const VkCommandBuffer *pCommandBuffers)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(device));
	GET_DISPATCH_PTR_FROM(FreeCommandBuffers, device_impl);

#if RESHADE_ADDON
	for (uint32_t i = 0; i < commandBufferCount; ++i)
		device_impl->unregister_object<VK_OBJECT_TYPE_COMMAND_BUFFER>(pCommandBuffers[i]);
#endif

	trampoline(device, commandPool, commandBufferCount, pCommandBuffers);
}
