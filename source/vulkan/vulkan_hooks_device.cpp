/*
 * Copyright (C) 2014 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause OR MIT
 */

#include "vulkan_hooks.hpp"
#include "vulkan_impl_device.hpp"
#include "vulkan_impl_command_queue.hpp"
#include "vulkan_impl_swapchain.hpp"
#include "vulkan_impl_type_convert.hpp"
#include "dll_log.hpp"
#ifdef RESHADE_TEST_APPLICATION
#include "hook_manager.hpp"
#endif
#include "addon_manager.hpp"
#include "lockfree_linear_map.hpp"
#include <cstring> // std::strcmp, std::strncmp
#include <algorithm> // std::find_if, std::min

// Set during Vulkan device creation and presentation, to avoid hooking internal D3D devices created e.g. by NVIDIA Ansel, Optimus or layered DXGI swapchain
extern thread_local bool g_in_dxgi_runtime;

extern lockfree_linear_map<void *, vulkan_instance, 16> g_vulkan_instances;
lockfree_linear_map<void *, reshade::vulkan::device_impl *, 8> g_vulkan_devices;

#if RESHADE_ADDON
void create_default_view(reshade::vulkan::device_impl *device_impl, VkImage image)
{
	if (image == VK_NULL_HANDLE)
		return;

	const auto data = device_impl->get_private_data_for_object<VK_OBJECT_TYPE_IMAGE>(image);
	assert(data->default_view == VK_NULL_HANDLE);

	// Need to create a default view that is used in 'vkCmdClearColorImage' and 'vkCmdClearDepthStencilImage'
	if ((data->create_info.usage & (VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT)) != 0 &&
		(data->create_info.usage & (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)) != 0)
	{
		VkImageViewCreateInfo default_view_info { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
		default_view_info.image = image;
		default_view_info.viewType = static_cast<VkImageViewType>(data->create_info.imageType); // Map 'VK_IMAGE_TYPE_1D' to VK_IMAGE_VIEW_TYPE_1D' and so on
		default_view_info.format = data->create_info.format;
		default_view_info.subresourceRange.aspectMask = reshade::vulkan::aspect_flags_from_format(data->create_info.format);
		default_view_info.subresourceRange.baseMipLevel = 0;
		default_view_info.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
		default_view_info.subresourceRange.baseArrayLayer = 0;
		default_view_info.subresourceRange.layerCount = 1; // Non-array image view types can only contain a single layer

		vkCreateImageView(device_impl->_orig, &default_view_info, nullptr, &data->default_view);
	}
}
void destroy_default_view(reshade::vulkan::device_impl *device_impl, VkImage image)
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
	reshade::log::message(reshade::log::level::info, "Redirecting vkCreateDevice(physicalDevice = %p, pCreateInfo = %p, pAllocator = %p, pDevice = %p) ...", physicalDevice, pCreateInfo, pAllocator, pDevice);

	assert(pCreateInfo != nullptr && pDevice != nullptr);

	// Look for layer link info if installed as a layer (provided by the Vulkan loader)
	struct VkLayerDeviceLink
	{
		VkLayerDeviceLink *pNext;
		PFN_vkGetInstanceProcAddr pfnNextGetInstanceProcAddr;
		PFN_vkGetDeviceProcAddr pfnNextGetDeviceProcAddr;
	};
	struct VkLayerDeviceCreateInfo
	{
		VkStructureType sType; // VK_STRUCTURE_TYPE_LOADER_DEVICE_CREATE_INFO
		const void *pNext;
		VkLayerFunction function;
		union {
			VkLayerDeviceLink *pLayerInfo;
		} u;
	};

	const auto link_info = find_layer_info<VkLayerDeviceCreateInfo>(pCreateInfo->pNext, VK_STRUCTURE_TYPE_LOADER_DEVICE_CREATE_INFO, VK_LAYER_LINK_INFO);

	const vulkan_instance &instance = g_vulkan_instances.at(dispatch_key_from_handle(physicalDevice));
	assert(instance.handle != VK_NULL_HANDLE);

	// Get trampoline function pointers
	PFN_vkCreateDevice trampoline = nullptr;
	PFN_vkGetDeviceProcAddr get_device_proc_addr = nullptr;
	PFN_vkGetInstanceProcAddr get_instance_proc_addr = nullptr;

	if (link_info != nullptr)
	{
		assert(link_info->u.pLayerInfo != nullptr);
		assert(link_info->u.pLayerInfo->pfnNextGetDeviceProcAddr != nullptr);
		assert(link_info->u.pLayerInfo->pfnNextGetInstanceProcAddr != nullptr);

		// Look up functions in layer info
		get_device_proc_addr = link_info->u.pLayerInfo->pfnNextGetDeviceProcAddr;
		get_instance_proc_addr = link_info->u.pLayerInfo->pfnNextGetInstanceProcAddr;
		trampoline = reinterpret_cast<PFN_vkCreateDevice>(get_instance_proc_addr(instance.handle, "vkCreateDevice"));

		// Advance the link info for the next element on the chain
		link_info->u.pLayerInfo = link_info->u.pLayerInfo->pNext;
	}
#ifdef RESHADE_TEST_APPLICATION
	else
	{
		trampoline = reshade::hooks::call(vkCreateDevice);
		get_device_proc_addr = reshade::hooks::call(vkGetDeviceProcAddr);
		get_instance_proc_addr = reshade::hooks::call(vkGetInstanceProcAddr);
	}
#endif

	if (trampoline == nullptr) // Unable to resolve next 'vkCreateDevice' function in the call chain
		return VK_ERROR_INITIALIZATION_FAILED;

	reshade::log::message(reshade::log::level::info, "> Dumping enabled device extensions:");
	for (uint32_t i = 0; i < pCreateInfo->enabledExtensionCount; ++i)
		reshade::log::message(reshade::log::level::info, "  %s", pCreateInfo->ppEnabledExtensionNames[i]);

	const auto enum_queue_families = instance.dispatch_table.GetPhysicalDeviceQueueFamilyProperties;
	const auto enum_device_extensions = instance.dispatch_table.EnumerateDeviceExtensionProperties;
	if (enum_queue_families == nullptr || enum_device_extensions == nullptr)
		return VK_ERROR_INITIALIZATION_FAILED;

	uint32_t num_queue_families = 0;
	enum_queue_families(physicalDevice, &num_queue_families, nullptr);
	std::vector<VkQueueFamilyProperties> queue_families(num_queue_families);
	enum_queue_families(physicalDevice, &num_queue_families, queue_families.data());

	uint32_t graphics_queue_family_index = std::numeric_limits<uint32_t>::max();
	for (uint32_t i = 0; i < pCreateInfo->queueCreateInfoCount; ++i)
	{
		const uint32_t queue_family_index = pCreateInfo->pQueueCreateInfos[i].queueFamilyIndex;
		assert(queue_family_index < num_queue_families);

		// Find the first queue family which supports graphics and has at least one queue
		if (pCreateInfo->pQueueCreateInfos[i].queueCount > 0 && (queue_families[queue_family_index].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0)
		{
			if (pCreateInfo->pQueueCreateInfos[i].pQueuePriorities[0] < 1.0f)
				reshade::log::message(reshade::log::level::warning, "Vulkan queue used for rendering has a low priority (%f).", pCreateInfo->pQueueCreateInfos[i].pQueuePriorities[0]);

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
	bool timeline_semaphore_ext = false;
	bool custom_border_color_ext = false;
	bool extended_dynamic_state_ext = false;
	bool conservative_rasterization_ext = false;
	bool ray_tracing_ext = false;
	bool buffer_device_address_ext = false;

	{
		uint32_t num_extensions = 0;
		enum_device_extensions(physicalDevice, nullptr, &num_extensions, nullptr);
		std::vector<VkExtensionProperties> extensions(num_extensions);
		enum_device_extensions(physicalDevice, nullptr, &num_extensions, extensions.data());

		// Make sure the driver actually supports the requested extensions
		const auto add_extension = [&extensions, &enabled_extensions, &graphics_queue_family_index](const char *name, bool required) {
			if (const auto it = std::find_if(extensions.cbegin(), extensions.cend(),
					[name](const auto &props) { return std::strncmp(props.extensionName, name, VK_MAX_EXTENSION_NAME_SIZE) == 0; });
				it != extensions.cend())
			{
				enabled_extensions.push_back(name);
				return true;
			}

			if (required)
			{
				reshade::log::message(reshade::log::level::error, "Required extension \"%s\" is not supported on this device. Initialization failed.", name);

				// Reset queue family index to prevent ReShade initialization
				graphics_queue_family_index = std::numeric_limits<uint32_t>::max();
			}
			else
			{
				reshade::log::message(reshade::log::level::warning, "Optional extension \"%s\" is not supported on this device.", name);
			}

			return false;
		};

		// Enable features that ReShade requires
		enabled_features.samplerAnisotropy = VK_TRUE;
		enabled_features.shaderImageGatherExtended = VK_TRUE;
		enabled_features.shaderStorageImageReadWithoutFormat = VK_TRUE;
		enabled_features.shaderStorageImageWriteWithoutFormat = VK_TRUE;

		// Enable extensions that ReShade requires
		if (instance.api_version < VK_API_VERSION_1_3 && !add_extension(VK_EXT_PRIVATE_DATA_EXTENSION_NAME, true))
			return VK_ERROR_EXTENSION_NOT_PRESENT;

		push_descriptor_ext = add_extension(VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME, false);
		dynamic_rendering_ext = instance.api_version >= VK_API_VERSION_1_3 || add_extension(VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME, false);
		// Add extensions that are required by VK_KHR_dynamic_rendering when not using the core variant
		if (dynamic_rendering_ext && instance.api_version < VK_API_VERSION_1_3)
		{
			add_extension(VK_KHR_DEPTH_STENCIL_RESOLVE_EXTENSION_NAME, false);
			add_extension(VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME, false);
		}
		timeline_semaphore_ext = instance.api_version >= VK_API_VERSION_1_2 || add_extension(VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME, false);
		custom_border_color_ext = add_extension(VK_EXT_CUSTOM_BORDER_COLOR_EXTENSION_NAME, false);
		extended_dynamic_state_ext = instance.api_version >= VK_API_VERSION_1_3 || add_extension(VK_EXT_EXTENDED_DYNAMIC_STATE_EXTENSION_NAME, false);
		conservative_rasterization_ext = add_extension(VK_EXT_CONSERVATIVE_RASTERIZATION_EXTENSION_NAME, false);
		add_extension(VK_KHR_EXTERNAL_MEMORY_WIN32_EXTENSION_NAME, false);

#if 0
		ray_tracing_ext =
			add_extension(VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME, false) &&
			add_extension(VK_KHR_SPIRV_1_4_EXTENSION_NAME, false) &&
			add_extension(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME, false) &&
			add_extension(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME, false) &&
			add_extension(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME, false) &&
			add_extension(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME, false) &&
			add_extension(VK_KHR_RAY_TRACING_MAINTENANCE_1_EXTENSION_NAME, false);
		buffer_device_address_ext = ray_tracing_ext && add_extension(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME, false);
#endif

		// Check if the device is used for presenting
		if (std::find_if(enabled_extensions.cbegin(), enabled_extensions.cend(),
				[](const char *name) { return std::strcmp(name, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0; }) == enabled_extensions.cend())
		{
			reshade::log::message(reshade::log::level::warning, "Skipping device because it is not created with the \"" VK_KHR_SWAPCHAIN_EXTENSION_NAME "\" extension.");

			graphics_queue_family_index = std::numeric_limits<uint32_t>::max();
		}
		// Only have to enable additional features if there is a graphics queue, since ReShade will not run otherwise
		else if (graphics_queue_family_index == std::numeric_limits<uint32_t>::max())
		{
			reshade::log::message(reshade::log::level::warning, "Skipping device because it is not created with a graphics queue.");
		}
		else
		{
			// No Man's Sky initializes OpenVR before loading Vulkan (and therefore before loading ReShade), so need to manually install OpenVR hooks now when used
			extern void check_and_init_openvr_hooks();
			check_and_init_openvr_hooks();

			add_extension(VK_KHR_IMAGE_FORMAT_LIST_EXTENSION_NAME, true);
			add_extension(VK_KHR_SWAPCHAIN_MUTABLE_FORMAT_EXTENSION_NAME, true);
		}
	}

	VkDeviceCreateInfo create_info = *pCreateInfo;
	create_info.enabledExtensionCount = static_cast<uint32_t>(enabled_extensions.size());
	create_info.ppEnabledExtensionNames = enabled_extensions.data();

	#pragma region Patch the enabled features
	// Patch the enabled features
	if (features2 != nullptr)
		// This is evil, because overwriting application memory, but whatever (RenderDoc does this too)
		const_cast<VkPhysicalDeviceFeatures2 *>(features2)->features = enabled_features;
	else
		create_info.pEnabledFeatures = &enabled_features;

	// Enable private data feature
	VkDevicePrivateDataCreateInfo private_data_info { VK_STRUCTURE_TYPE_DEVICE_PRIVATE_DATA_CREATE_INFO, create_info.pNext };
	private_data_info.privateDataSlotRequestCount = 1;

	if (const auto existing_vulkan_14_features = find_in_structure_chain<VkPhysicalDeviceVulkan14Features>(
			pCreateInfo->pNext, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES))
	{
		assert(instance.api_version >= VK_API_VERSION_1_4);

		push_descriptor_ext = existing_vulkan_14_features->pushDescriptor;
	}

	VkPhysicalDevicePrivateDataFeatures private_data_features;
	VkPhysicalDeviceDynamicRenderingFeatures dynamic_rendering_features;
	if (const auto existing_vulkan_13_features = find_in_structure_chain<VkPhysicalDeviceVulkan13Features>(
			pCreateInfo->pNext, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES))
	{
		assert(instance.api_version >= VK_API_VERSION_1_3);

		create_info.pNext = &private_data_info;

		dynamic_rendering_ext = existing_vulkan_13_features->dynamicRendering;

		// Forcefully enable private data in Vulkan 1.3, again, evil =)
		const_cast<VkPhysicalDeviceVulkan13Features *>(existing_vulkan_13_features)->privateData = VK_TRUE;
	}
	else
	{
		if (const auto existing_private_data_features = find_in_structure_chain<VkPhysicalDevicePrivateDataFeatures>(
				pCreateInfo->pNext, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRIVATE_DATA_FEATURES))
		{
			create_info.pNext = &private_data_info;

			const_cast<VkPhysicalDevicePrivateDataFeatures *>(existing_private_data_features)->privateData = VK_TRUE;
		}
		else
		{
			private_data_features = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRIVATE_DATA_FEATURES, &private_data_info };
			private_data_features.privateData = VK_TRUE;

			create_info.pNext = &private_data_features;
		}

		if (const auto existing_dynamic_rendering_features = find_in_structure_chain<VkPhysicalDeviceDynamicRenderingFeatures>(
				pCreateInfo->pNext, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES))
		{
			dynamic_rendering_ext = existing_dynamic_rendering_features->dynamicRendering;
		}
		else if (dynamic_rendering_ext)
		{
			dynamic_rendering_features = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES, const_cast<void *>(create_info.pNext) };
			dynamic_rendering_features.dynamicRendering = VK_TRUE;

			create_info.pNext = &dynamic_rendering_features;
		}
	}

	VkPhysicalDeviceTimelineSemaphoreFeatures timeline_semaphore_features;
	if (const auto existing_vulkan_12_features = find_in_structure_chain<VkPhysicalDeviceVulkan12Features>(
			pCreateInfo->pNext, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES))
	{
		assert(instance.api_version >= VK_API_VERSION_1_2);

		// Force enable timeline semaphore support (used for effect runtime present/graphics queue synchronization in case of present from compute, e.g. in Indiana Jones and the Great Circle and DOOM Eternal)
		const_cast<VkPhysicalDeviceVulkan12Features *>(existing_vulkan_12_features)->timelineSemaphore = VK_TRUE;
	}
	else
	{
		if (const auto existing_timeline_semaphore_features = find_in_structure_chain<VkPhysicalDeviceTimelineSemaphoreFeatures>(
				pCreateInfo->pNext, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES))
		{
			timeline_semaphore_ext = existing_timeline_semaphore_features->timelineSemaphore;
		}
		else if (timeline_semaphore_ext)
		{
			timeline_semaphore_features = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES, const_cast<void *>(create_info.pNext) };
			timeline_semaphore_features.timelineSemaphore = VK_TRUE;

			create_info.pNext = &timeline_semaphore_features;
		}
	}

	// Enable Vulkan memory model device scope if it is not, since it is required by atomics in generated SPIR-V code for effects
	if (const auto existing_memory_model_features = find_in_structure_chain<VkPhysicalDeviceVulkanMemoryModelFeatures>(
			pCreateInfo->pNext, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_MEMORY_MODEL_FEATURES))
	{
		const_cast<VkPhysicalDeviceVulkanMemoryModelFeatures *>(existing_memory_model_features)->vulkanMemoryModel = VK_TRUE;
		const_cast<VkPhysicalDeviceVulkanMemoryModelFeatures *>(existing_memory_model_features)->vulkanMemoryModelDeviceScope = VK_TRUE;
	}

	// Optionally enable custom border color feature
	VkPhysicalDeviceCustomBorderColorFeaturesEXT custom_border_features;
	if (const auto existing_custom_border_features = find_in_structure_chain<VkPhysicalDeviceCustomBorderColorFeaturesEXT>(
			pCreateInfo->pNext, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CUSTOM_BORDER_COLOR_FEATURES_EXT))
	{
		custom_border_color_ext = existing_custom_border_features->customBorderColors;
	}
	else if (custom_border_color_ext)
	{
		custom_border_features = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CUSTOM_BORDER_COLOR_FEATURES_EXT, const_cast<void *>(create_info.pNext) };
		custom_border_features.customBorderColors = VK_TRUE;
		custom_border_features.customBorderColorWithoutFormat = VK_TRUE;

		create_info.pNext = &custom_border_features;
	}

	// Optionally enable extended dynamic state feature
	VkPhysicalDeviceExtendedDynamicStateFeaturesEXT extended_dynamic_state_features;
	if (const auto existing_extended_dynamic_state_features = find_in_structure_chain<VkPhysicalDeviceExtendedDynamicStateFeaturesEXT>(
			pCreateInfo->pNext, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_FEATURES_EXT))
	{
		extended_dynamic_state_ext = existing_extended_dynamic_state_features->extendedDynamicState;
	}
	else if (extended_dynamic_state_ext)
	{
		extended_dynamic_state_features = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_FEATURES_EXT, const_cast<void *>(create_info.pNext) };
		extended_dynamic_state_features.extendedDynamicState = VK_TRUE;

		create_info.pNext = &extended_dynamic_state_features;
	}

	// Optionally enable ray tracing feature
	VkPhysicalDeviceRayTracingPipelineFeaturesKHR ray_tracing_features;
	VkPhysicalDeviceAccelerationStructureFeaturesKHR acceleration_structure_features;
	if (const auto existing_ray_tracing_features = find_in_structure_chain<VkPhysicalDeviceRayTracingPipelineFeaturesKHR>(
			pCreateInfo->pNext, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR))
	{
		ray_tracing_ext = existing_ray_tracing_features->rayTracingPipeline;
	}
	else if (ray_tracing_ext)
	{
		ray_tracing_features = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR, const_cast<void *>(create_info.pNext) };
		ray_tracing_features.rayTracingPipeline = VK_TRUE;

		create_info.pNext = &ray_tracing_features;

		acceleration_structure_features = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR, const_cast<void *>(create_info.pNext) };
		acceleration_structure_features.accelerationStructure = VK_TRUE;

		create_info.pNext = &acceleration_structure_features;
	}

	VkPhysicalDeviceBufferDeviceAddressFeatures buffer_device_address_features;
	if (const auto existing_buffer_device_address_features = find_in_structure_chain<VkPhysicalDeviceBufferDeviceAddressFeatures>(
			pCreateInfo->pNext, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES))
	{
		buffer_device_address_ext = existing_buffer_device_address_features->bufferDeviceAddress;
	}
	else if (buffer_device_address_ext)
	{
		buffer_device_address_features = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES, const_cast<void *>(create_info.pNext) };
		buffer_device_address_features.bufferDeviceAddress = VK_TRUE;

		create_info.pNext = &buffer_device_address_features;
	}
	#pragma endregion

	// Continue calling down the chain
	assert(!g_in_dxgi_runtime);
	g_in_dxgi_runtime = true;
	const VkResult result = trampoline(physicalDevice, &create_info, pAllocator, pDevice);
	g_in_dxgi_runtime = false;
	if (result < VK_SUCCESS)
	{
		reshade::log::message(reshade::log::level::warning, "vkCreateDevice failed with error code %d.", static_cast<int>(result));
		return result;
	}

	// Initialize the device dispatch table
	vulkan_device device = { *pDevice, instance.handle, instance.dispatch_table };
	device.dispatch_table.GetDeviceProcAddr = get_device_proc_addr;
	device.dispatch_table.GetInstanceProcAddr = get_instance_proc_addr;

	gladLoadVulkanContextUserPtr(&device.dispatch_table, physicalDevice,
		[](void *user, const char *name) -> GLADapiproc {
			const vulkan_device &device = *static_cast<const vulkan_device *>(user);
			const char *name_without_prefix = name + 2; // Skip "vk" prefix

			// Do not load existing instance function pointers anew
			if (0 == std::strcmp(name_without_prefix, "GetInstanceProcAddr"))
				return reinterpret_cast<GLADapiproc>(device.dispatch_table.GetInstanceProcAddr);
			if (0 == std::strcmp(name_without_prefix, "CreateInstance"))
				return reinterpret_cast<GLADapiproc>(device.dispatch_table.CreateInstance);
			if (0 == std::strcmp(name_without_prefix, "EnumerateDeviceExtensionProperties"))
				return reinterpret_cast<GLADapiproc>(device.dispatch_table.EnumerateDeviceExtensionProperties);
			if (0 == std::strcmp(name_without_prefix, "EnumerateDeviceLayerProperties"))
				return reinterpret_cast<GLADapiproc>(device.dispatch_table.EnumerateDeviceLayerProperties);
			if (0 == std::strcmp(name_without_prefix, "EnumerateInstanceExtensionProperties"))
				return reinterpret_cast<GLADapiproc>(device.dispatch_table.EnumerateInstanceExtensionProperties);
			if (0 == std::strcmp(name_without_prefix, "EnumerateInstanceLayerProperties"))
				return reinterpret_cast<GLADapiproc>(device.dispatch_table.EnumerateInstanceLayerProperties);
			if (0 == std::strcmp(name_without_prefix, "EnumerateInstanceVersion"))
				return reinterpret_cast<GLADapiproc>(device.dispatch_table.EnumerateInstanceVersion);

			if (0 == std::strcmp(name_without_prefix, "EnumeratePhysicalDeviceGroups"))
				return reinterpret_cast<GLADapiproc>(device.dispatch_table.EnumeratePhysicalDeviceGroups);
			if (0 == std::strcmp(name_without_prefix, "EnumeratePhysicalDevices"))
				return reinterpret_cast<GLADapiproc>(device.dispatch_table.EnumeratePhysicalDevices);

			if (0 == std::strcmp(name_without_prefix, "GetDeviceProcAddr"))
				return reinterpret_cast<GLADapiproc>(device.dispatch_table.GetDeviceProcAddr);

			if (0 == std::strcmp(name_without_prefix, "DestroyInstance") ||
				0 == std::strcmp(name_without_prefix, "CreateDevice") ||
				0 == std::strcmp(name_without_prefix, "SubmitDebugUtilsMessageEXT") ||
				0 == std::strcmp(name_without_prefix, "CreateDebugUtilsMessengerEXT") ||
				0 == std::strcmp(name_without_prefix, "DestroyDebugUtilsMessengerEXT") ||
				(std::strstr(name_without_prefix, "Properties") != nullptr && std::strstr(name_without_prefix, "AccelerationStructures") == nullptr && std::strstr(name_without_prefix, "Handle") == nullptr) ||
				(std::strstr(name_without_prefix, "Surface") != nullptr && std::strstr(name_without_prefix, "DeviceGroupSurface") == nullptr) ||
				(std::strstr(name_without_prefix, "PhysicalDevice") != nullptr))
				return reinterpret_cast<GLADapiproc>(device.dispatch_table.GetInstanceProcAddr(device.instance_handle, name));

			const PFN_vkVoidFunction device_proc_address = device.dispatch_table.GetDeviceProcAddr(device.handle, name);
			return reinterpret_cast<GLADapiproc>(device_proc_address);
		}, &device);

	device.dispatch_table.KHR_push_descriptor &= push_descriptor_ext ? 1 : 0;
	device.dispatch_table.KHR_dynamic_rendering &= dynamic_rendering_ext ? 1 : 0;
	device.dispatch_table.KHR_timeline_semaphore &= timeline_semaphore_ext ? 1 : 0;
	device.dispatch_table.EXT_custom_border_color &= custom_border_color_ext ? 1 : 0;
	device.dispatch_table.EXT_extended_dynamic_state &= extended_dynamic_state_ext ? 1 : 0;
	device.dispatch_table.EXT_conservative_rasterization &= conservative_rasterization_ext ? 1 : 0;
	device.dispatch_table.KHR_ray_tracing_pipeline &= ray_tracing_ext ? 1 : 0;
	device.dispatch_table.KHR_acceleration_structure &= ray_tracing_ext ? 1 : 0;
	device.dispatch_table.KHR_buffer_device_address &= buffer_device_address_ext ? 1 : 0;

	if (instance.api_version < VK_API_VERSION_1_2)
	{
		device.dispatch_table.VERSION_1_2 = 0;

		device.dispatch_table.CreateRenderPass2 = device.dispatch_table.CreateRenderPass2KHR;
		device.dispatch_table.CmdBeginRenderPass2 = device.dispatch_table.CmdBeginRenderPass2KHR;
		device.dispatch_table.CmdNextSubpass2 = device.dispatch_table.CmdNextSubpass2KHR;
		device.dispatch_table.CmdEndRenderPass2 = device.dispatch_table.CmdEndRenderPass2KHR;

		device.dispatch_table.CmdDrawIndirectCount = device.dispatch_table.CmdDrawIndirectCountKHR;
		device.dispatch_table.CmdDrawIndexedIndirectCount = device.dispatch_table.CmdDrawIndexedIndirectCountKHR;

		device.dispatch_table.GetSemaphoreCounterValue = device.dispatch_table.GetSemaphoreCounterValueKHR;
		device.dispatch_table.WaitSemaphores = device.dispatch_table.WaitSemaphoresKHR;
		device.dispatch_table.SignalSemaphore = device.dispatch_table.SignalSemaphoreKHR;

		device.dispatch_table.GetBufferDeviceAddress = device.dispatch_table.GetBufferDeviceAddressKHR;
	}
	if (instance.api_version < VK_API_VERSION_1_3)
	{
		device.dispatch_table.VERSION_1_3 = 0;

		device.dispatch_table.CmdBeginRendering = device.dispatch_table.CmdBeginRenderingKHR;
		device.dispatch_table.CmdEndRendering = device.dispatch_table.CmdEndRenderingKHR;

		device.dispatch_table.CmdPipelineBarrier2 = device.dispatch_table.CmdPipelineBarrier2KHR;
		device.dispatch_table.CmdWriteTimestamp2 = device.dispatch_table.CmdWriteTimestamp2KHR;
		device.dispatch_table.QueueSubmit2 = device.dispatch_table.QueueSubmit2KHR;

		device.dispatch_table.CmdCopyBuffer2 = device.dispatch_table.CmdCopyBuffer2KHR;
		device.dispatch_table.CmdCopyImage2 = device.dispatch_table.CmdCopyImage2KHR;
		device.dispatch_table.CmdCopyBufferToImage2 = device.dispatch_table.CmdCopyBufferToImage2KHR;
		device.dispatch_table.CmdCopyImageToBuffer2 = device.dispatch_table.CmdCopyImageToBuffer2KHR;
		device.dispatch_table.CmdBlitImage2 = device.dispatch_table.CmdBlitImage2KHR;
		device.dispatch_table.CmdResolveImage2 = device.dispatch_table.CmdResolveImage2KHR;

		device.dispatch_table.CmdSetCullMode = device.dispatch_table.CmdSetCullModeEXT;
		device.dispatch_table.CmdSetFrontFace = device.dispatch_table.CmdSetFrontFaceEXT;
		device.dispatch_table.CmdSetPrimitiveTopology = device.dispatch_table.CmdSetPrimitiveTopologyEXT;
		device.dispatch_table.CmdSetViewportWithCount = device.dispatch_table.CmdSetViewportWithCountEXT;
		device.dispatch_table.CmdSetScissorWithCount = device.dispatch_table.CmdSetScissorWithCountEXT;
		device.dispatch_table.CmdBindVertexBuffers2 = device.dispatch_table.CmdBindVertexBuffers2EXT;
		device.dispatch_table.CmdSetDepthTestEnable = device.dispatch_table.CmdSetDepthTestEnableEXT;
		device.dispatch_table.CmdSetDepthWriteEnable = device.dispatch_table.CmdSetDepthWriteEnableEXT;
		device.dispatch_table.CmdSetDepthCompareOp = device.dispatch_table.CmdSetDepthCompareOpEXT;
		device.dispatch_table.CmdSetDepthBoundsTestEnable = device.dispatch_table.CmdSetDepthBoundsTestEnableEXT;
		device.dispatch_table.CmdSetStencilTestEnable = device.dispatch_table.CmdSetStencilTestEnableEXT;
		device.dispatch_table.CmdSetStencilOp = device.dispatch_table.CmdSetStencilOpEXT;

		device.dispatch_table.CreatePrivateDataSlot = device.dispatch_table.CreatePrivateDataSlotEXT;
		device.dispatch_table.DestroyPrivateDataSlot = device.dispatch_table.DestroyPrivateDataSlotEXT;
		device.dispatch_table.GetPrivateData = device.dispatch_table.GetPrivateDataEXT;
		device.dispatch_table.SetPrivateData = device.dispatch_table.SetPrivateDataEXT;
	}
	if (instance.api_version < VK_API_VERSION_1_4)
	{
		device.dispatch_table.VERSION_1_4 = 0;

		device.dispatch_table.CmdPushDescriptorSet = device.dispatch_table.CmdPushDescriptorSetKHR;
		device.dispatch_table.CmdPushDescriptorSetWithTemplate = device.dispatch_table.CmdPushDescriptorSetWithTemplateKHR;
	}

	// Initialize per-device data
	const auto device_impl = new reshade::vulkan::device_impl(
		device.handle,
		physicalDevice,
		instance.handle,
		instance.api_version,
		device.dispatch_table,
		enabled_features);

	if (!g_vulkan_devices.emplace(dispatch_key_from_handle(device.handle), device_impl))
	{
		reshade::log::message(reshade::log::level::warning, "Failed to register Vulkan device %p.", device.handle);
	}

#if RESHADE_ADDON
	reshade::load_addons();

	reshade::invoke_addon_event<reshade::addon_event::init_device>(device_impl);
#endif

	// Initialize all queues associated with this device
	for (uint32_t i = 0; i < pCreateInfo->queueCreateInfoCount; ++i)
	{
		const VkDeviceQueueCreateInfo &queue_create_info = pCreateInfo->pQueueCreateInfos[i];

		for (uint32_t queue_index = 0; queue_index < queue_create_info.queueCount; ++queue_index)
		{
			VkDeviceQueueInfo2 queue_info = { VK_STRUCTURE_TYPE_DEVICE_QUEUE_INFO_2 };
			queue_info.flags = queue_create_info.flags;
			queue_info.queueFamilyIndex = queue_create_info.queueFamilyIndex;
			queue_info.queueIndex = queue_index;

			VkQueue queue = VK_NULL_HANDLE;
			// According to the spec, 'vkGetDeviceQueue' must only be used to get queues where 'VkDeviceQueueCreateInfo::flags' is set to zero, so use 'vkGetDeviceQueue2' instead
			device.dispatch_table.GetDeviceQueue2(device.handle, &queue_info, &queue);
			assert(VK_NULL_HANDLE != queue);

			// Subsequent layers (like the validation layer or the Steam overlay) expect the loader to have set the dispatch pointer, but this does not happen when calling down the layer chain from here, so fix it
			// This applies to 'vkGetDeviceQueue', 'vkGetDeviceQueue2' and 'vkAllocateCommandBuffers' (functions that return dispatchable objects)
			*reinterpret_cast<void **>(queue) = *reinterpret_cast<void **>(device.handle);

			const auto queue_impl = new reshade::vulkan::object_data<VK_OBJECT_TYPE_QUEUE>(
				device_impl,
				queue_create_info.queueFamilyIndex,
				queue_families[queue_create_info.queueFamilyIndex],
				queue);

			device_impl->register_object<VK_OBJECT_TYPE_QUEUE>(queue, queue_impl);

#if RESHADE_ADDON
			reshade::invoke_addon_event<reshade::addon_event::init_command_queue>(queue_impl);
#endif

			if (queue_create_info.queueFamilyIndex == graphics_queue_family_index && queue_index == 0)
			{
				device_impl->_primary_graphics_queue = queue_impl;
				device_impl->_primary_graphics_queue_family_index = graphics_queue_family_index;
			}
		}
	}

#if RESHADE_VERBOSE_LOG
	reshade::log::message(reshade::log::level::debug, "Returning Vulkan device %p.", device.handle);
#endif
	return result;
}
void     VKAPI_CALL vkDestroyDevice(VkDevice device, const VkAllocationCallbacks *pAllocator)
{
	reshade::log::message(reshade::log::level::info, "Redirecting vkDestroyDevice(device = %p, pAllocator = %p) ...", device, pAllocator);

	if (device == VK_NULL_HANDLE)
		return;

	// Remove from device dispatch table since this device is being destroyed
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.erase(dispatch_key_from_handle(device));
	RESHADE_VULKAN_GET_DEVICE_DISPATCH_PTR(DestroyDevice, device_impl);

	// Destroy all queues associated with this device
	const std::vector<reshade::vulkan::command_queue_impl *> queues = device_impl->_queues;
	for (auto queue_it = queues.begin(); queue_it != queues.end(); ++queue_it)
	{
		const auto queue_impl = static_cast<reshade::vulkan::object_data<VK_OBJECT_TYPE_QUEUE> *>(*queue_it);

#if RESHADE_ADDON
		reshade::invoke_addon_event<reshade::addon_event::destroy_command_queue>(queue_impl);
#endif

		device_impl->unregister_object<VK_OBJECT_TYPE_QUEUE, false>(queue_impl->_orig);

		delete queue_impl; // This will remove the queue from the queue list of the device too (see 'command_queue_impl' destructor)
	}

#if RESHADE_ADDON
	reshade::invoke_addon_event<reshade::addon_event::destroy_device>(device_impl);

	reshade::unload_addons();
#endif

	// Finally destroy the device
	delete device_impl;

	trampoline(device, pAllocator);
}

VkResult VKAPI_CALL vkQueueSubmit(VkQueue queue, uint32_t submitCount, const VkSubmitInfo *pSubmits, VkFence fence)
{
	assert(pSubmits != nullptr || submitCount == 0);

	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(queue));

#if RESHADE_ADDON
	reshade::vulkan::command_queue_impl *const queue_impl = device_impl->get_private_data_for_object<VK_OBJECT_TYPE_QUEUE>(queue);

	const std::unique_lock<std::recursive_mutex> lock(queue_impl->_mutex);

	for (uint32_t i = 0; i < submitCount; ++i)
	{
		const VkSubmitInfo &submit_info = pSubmits[i];

		for (uint32_t k = 0; k < submit_info.commandBufferCount; ++k)
		{
			assert(submit_info.pCommandBuffers[k] != VK_NULL_HANDLE);

			reshade::vulkan::command_list_impl *const cmd_impl = device_impl->get_private_data_for_object<VK_OBJECT_TYPE_COMMAND_BUFFER>(submit_info.pCommandBuffers[k]);

			reshade::invoke_addon_event<reshade::addon_event::execute_command_list>(queue_impl, cmd_impl);
		}

		queue_impl->flush_immediate_command_list(const_cast<VkSubmitInfo &>(submit_info));
	}
#endif

	RESHADE_VULKAN_GET_DEVICE_DISPATCH_PTR(QueueSubmit, device_impl);
	return trampoline(queue, submitCount, pSubmits, fence);
}
VkResult VKAPI_CALL vkQueueSubmit2(VkQueue queue, uint32_t submitCount, const VkSubmitInfo2 *pSubmits, VkFence fence)
{
	assert(pSubmits != nullptr || submitCount == 0);

	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(queue));

#if RESHADE_ADDON
	reshade::vulkan::command_queue_impl *const queue_impl = device_impl->get_private_data_for_object<VK_OBJECT_TYPE_QUEUE>(queue);

	const std::unique_lock<std::recursive_mutex> lock(queue_impl->_mutex);

	for (uint32_t i = 0; i < submitCount; ++i)
	{
		const VkSubmitInfo2 &submit_info = pSubmits[i];

		for (uint32_t k = 0; k < submit_info.commandBufferInfoCount; ++k)
		{
			assert(submit_info.pCommandBufferInfos[k].commandBuffer != VK_NULL_HANDLE);

			reshade::vulkan::command_list_impl *const cmd_impl = device_impl->get_private_data_for_object<VK_OBJECT_TYPE_COMMAND_BUFFER>(submit_info.pCommandBufferInfos[k].commandBuffer);

			reshade::invoke_addon_event<reshade::addon_event::execute_command_list>(queue_impl, cmd_impl);
		}

		queue_impl->flush_immediate_command_list();
	}
#endif

	RESHADE_VULKAN_GET_DEVICE_DISPATCH_PTR(QueueSubmit2, device_impl);
	return trampoline(queue, submitCount, pSubmits, fence);
}

VkResult VKAPI_CALL vkBindBufferMemory(VkDevice device, VkBuffer buffer, VkDeviceMemory memory, VkDeviceSize memoryOffset)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(device));
	RESHADE_VULKAN_GET_DEVICE_DISPATCH_PTR(BindBufferMemory, device_impl);

	const VkResult result = trampoline(device, buffer, memory, memoryOffset);
	if (result < VK_SUCCESS)
	{
#if RESHADE_VERBOSE_LOG
		reshade::log::message(reshade::log::level::warning, "vkBindBufferMemory failed with error code %d.", static_cast<int>(result));
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
	RESHADE_VULKAN_GET_DEVICE_DISPATCH_PTR(BindBufferMemory2, device_impl);

	const VkResult result = trampoline(device, bindInfoCount, pBindInfos);
	if (result < VK_SUCCESS)
	{
#if RESHADE_VERBOSE_LOG
		reshade::log::message(reshade::log::level::warning, "vkBindBufferMemory2 failed with error code %d.", static_cast<int>(result));
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
	RESHADE_VULKAN_GET_DEVICE_DISPATCH_PTR(BindImageMemory, device_impl);

	const VkResult result = trampoline(device, image, memory, memoryOffset);
	if (result < VK_SUCCESS)
	{
#if RESHADE_VERBOSE_LOG
		reshade::log::message(reshade::log::level::warning, "vkBindImageMemory failed with error code %d.", static_cast<int>(result));
#endif
		return result;
	}

#if RESHADE_ADDON
	const auto image_data = device_impl->get_private_data_for_object<VK_OBJECT_TYPE_IMAGE>(image);
	image_data->memory = memory;
	image_data->memory_offset = memoryOffset;

	reshade::invoke_addon_event<reshade::addon_event::init_resource>(
		device_impl,
		reshade::vulkan::convert_resource_desc(image_data->create_info),
		nullptr,
		image_data->create_info.initialLayout == VK_IMAGE_LAYOUT_PREINITIALIZED ? reshade::api::resource_usage::cpu_access : reshade::api::resource_usage::undefined,
		reshade::api::resource { (uint64_t)image });

	// Create default view after the 'init_resource' event, so that the image is known to add-ons (since it calls 'init_resource_view' internally)
	create_default_view(device_impl, image);
#endif

	return result;
}
VkResult VKAPI_CALL vkBindImageMemory2(VkDevice device, uint32_t bindInfoCount, const VkBindImageMemoryInfo *pBindInfos)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(device));
	RESHADE_VULKAN_GET_DEVICE_DISPATCH_PTR(BindImageMemory2, device_impl);

	const VkResult result = trampoline(device, bindInfoCount, pBindInfos);
	if (result < VK_SUCCESS)
	{
#if RESHADE_VERBOSE_LOG
		reshade::log::message(reshade::log::level::warning, "vkBindImageMemory2 failed with error code %d.", static_cast<int>(result));
#endif
		return result;
	}

#if RESHADE_ADDON
	for (uint32_t i = 0; i < bindInfoCount; ++i)
	{
		const auto image_data = device_impl->get_private_data_for_object<VK_OBJECT_TYPE_IMAGE>(pBindInfos[i].image);
		image_data->memory = pBindInfos[i].memory;
		image_data->memory_offset = pBindInfos[i].memoryOffset;

		reshade::invoke_addon_event<reshade::addon_event::init_resource>(
			device_impl,
			reshade::vulkan::convert_resource_desc(image_data->create_info),
			nullptr,
			image_data->create_info.initialLayout == VK_IMAGE_LAYOUT_PREINITIALIZED ? reshade::api::resource_usage::cpu_access : reshade::api::resource_usage::undefined,
			reshade::api::resource { (uint64_t)pBindInfos[i].image });

		create_default_view(device_impl, pBindInfos[i].image);
	}
#endif

	return result;
}

VkResult VKAPI_CALL vkCreateQueryPool(VkDevice device, const VkQueryPoolCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkQueryPool *pQueryPool)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(device));
	RESHADE_VULKAN_GET_DEVICE_DISPATCH_PTR(CreateQueryPool, device_impl);

	assert(pCreateInfo != nullptr && pQueryPool != nullptr);

#if RESHADE_ADDON
	VkQueryPoolCreateInfo create_info = *pCreateInfo;

	if (reshade::invoke_addon_event<reshade::addon_event::create_query_heap>(device_impl, reshade::vulkan::convert_query_type(create_info.queryType), create_info.queryCount))
	{
		pCreateInfo = &create_info;
	}
#endif

	const VkResult result = trampoline(device, pCreateInfo, pAllocator, pQueryPool);
	if (result < VK_SUCCESS)
	{
#if RESHADE_VERBOSE_LOG
		reshade::log::message(reshade::log::level::warning, "vkCreateQueryPool failed with error code %d.", static_cast<int>(result));
#endif
		return result;
	}

#if RESHADE_ADDON
	reshade::vulkan::object_data<VK_OBJECT_TYPE_QUERY_POOL> &data = *device_impl->register_object<VK_OBJECT_TYPE_QUERY_POOL>(*pQueryPool);
	data.type = create_info.queryType;

	reshade::invoke_addon_event<reshade::addon_event::init_query_heap>(
		device_impl, reshade::vulkan::convert_query_type(create_info.queryType), create_info.queryCount, reshade::api::query_heap { (uint64_t)*pQueryPool });
#endif

	return result;
}
void     VKAPI_CALL vkDestroyQueryPool(VkDevice device, VkQueryPool queryPool, const VkAllocationCallbacks *pAllocator)
{
	if (queryPool == VK_NULL_HANDLE)
		return;

	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(device));
	RESHADE_VULKAN_GET_DEVICE_DISPATCH_PTR(DestroyQueryPool, device_impl);

#if RESHADE_ADDON
	reshade::invoke_addon_event<reshade::addon_event::destroy_query_heap>(device_impl, reshade::api::query_heap{ (uint64_t)queryPool });

	device_impl->unregister_object<VK_OBJECT_TYPE_QUERY_POOL>(queryPool);
#endif

	trampoline(device, queryPool, pAllocator);
}

VkResult VKAPI_CALL vkGetQueryPoolResults(VkDevice device, VkQueryPool queryPool, uint32_t firstQuery, uint32_t queryCount, size_t dataSize, void *pData, VkDeviceSize stride, VkQueryResultFlags flags)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(device));
	RESHADE_VULKAN_GET_DEVICE_DISPATCH_PTR(GetQueryPoolResults, device_impl);

#if RESHADE_ADDON >= 2
	assert(stride <= std::numeric_limits<uint32_t>::max());

	if (reshade::invoke_addon_event<reshade::addon_event::get_query_heap_results>(device_impl, reshade::api::query_heap { (uint64_t)queryPool }, firstQuery, queryCount, pData, static_cast<uint32_t>(stride)))
		return VK_SUCCESS;
#endif

	return trampoline(device, queryPool, firstQuery, queryCount, dataSize, pData, stride, flags);
}

VkResult VKAPI_CALL vkCreateBuffer(VkDevice device, const VkBufferCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkBuffer *pBuffer)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(device));
	RESHADE_VULKAN_GET_DEVICE_DISPATCH_PTR(CreateBuffer, device_impl);

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
		reshade::log::message(reshade::log::level::warning, "vkCreateBuffer failed with error code %d.", static_cast<int>(result));
#endif
		return result;
	}

#if RESHADE_ADDON
	reshade::vulkan::object_data<VK_OBJECT_TYPE_BUFFER> &data = *device_impl->register_object<VK_OBJECT_TYPE_BUFFER>(*pBuffer);
	data.create_info = create_info;
	data.create_info.pNext = nullptr; // Clear out structure chain pointer, since it becomes invalid once leaving the current scope
#endif

	return result;
}
void     VKAPI_CALL vkDestroyBuffer(VkDevice device, VkBuffer buffer, const VkAllocationCallbacks *pAllocator)
{
	if (buffer == VK_NULL_HANDLE)
		return;

	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(device));
	RESHADE_VULKAN_GET_DEVICE_DISPATCH_PTR(DestroyBuffer, device_impl);

#if RESHADE_ADDON
	reshade::invoke_addon_event<reshade::addon_event::destroy_resource>(device_impl, reshade::api::resource { (uint64_t)buffer });

	device_impl->unregister_object<VK_OBJECT_TYPE_BUFFER>(buffer);
#endif

	trampoline(device, buffer, pAllocator);
}

VkResult VKAPI_CALL vkCreateBufferView(VkDevice device, const VkBufferViewCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkBufferView *pView)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(device));
	RESHADE_VULKAN_GET_DEVICE_DISPATCH_PTR(CreateBufferView, device_impl);

	assert(pCreateInfo != nullptr && pView != nullptr);

#if RESHADE_ADDON
	VkBufferViewCreateInfo create_info = *pCreateInfo;
	auto desc = reshade::vulkan::convert_resource_view_desc(create_info);

	if (reshade::invoke_addon_event<reshade::addon_event::create_resource_view>(device_impl, reshade::api::resource { (uint64_t)create_info.buffer }, reshade::api::resource_usage::undefined, desc))
	{
		reshade::vulkan::convert_resource_view_desc(desc, create_info);
		pCreateInfo = &create_info;
	}
#endif

	const VkResult result = trampoline(device, pCreateInfo, pAllocator, pView);
	if (result < VK_SUCCESS)
	{
#if RESHADE_VERBOSE_LOG
		reshade::log::message(reshade::log::level::warning, "vkCreateBufferView failed with error code %d.", static_cast<int>(result));
#endif
		return result;
	}

#if RESHADE_ADDON
	reshade::vulkan::object_data<VK_OBJECT_TYPE_BUFFER_VIEW> &data = *device_impl->register_object<VK_OBJECT_TYPE_BUFFER_VIEW>(*pView);
	data.create_info = create_info;
	data.create_info.pNext = nullptr; // Clear out structure chain pointer, since it becomes invalid once leaving the current scope

	reshade::invoke_addon_event<reshade::addon_event::init_resource_view>(
		device_impl, reshade::api::resource { (uint64_t)create_info.buffer }, reshade::api::resource_usage::undefined, desc, reshade::api::resource_view { (uint64_t)*pView });
#endif

	return result;
}
void     VKAPI_CALL vkDestroyBufferView(VkDevice device, VkBufferView bufferView, const VkAllocationCallbacks *pAllocator)
{
	if (bufferView == VK_NULL_HANDLE)
		return;

	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(device));
	RESHADE_VULKAN_GET_DEVICE_DISPATCH_PTR(DestroyBufferView, device_impl);

#if RESHADE_ADDON
	reshade::invoke_addon_event<reshade::addon_event::destroy_resource_view>(device_impl, reshade::api::resource_view{ (uint64_t)bufferView });

	device_impl->unregister_object<VK_OBJECT_TYPE_BUFFER_VIEW>(bufferView);
#endif

	trampoline(device, bufferView, pAllocator);
}

VkResult VKAPI_CALL vkCreateImage(VkDevice device, const VkImageCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkImage *pImage)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(device));
	RESHADE_VULKAN_GET_DEVICE_DISPATCH_PTR(CreateImage, device_impl);

	assert(pCreateInfo != nullptr && pImage != nullptr);

#if RESHADE_ADDON
	VkImageCreateInfo create_info = *pCreateInfo;
	auto desc = reshade::vulkan::convert_resource_desc(create_info);
	assert(desc.heap == reshade::api::memory_heap::unknown);

	if (reshade::invoke_addon_event<reshade::addon_event::create_resource>(device_impl, desc, nullptr, pCreateInfo->initialLayout == VK_IMAGE_LAYOUT_PREINITIALIZED ? reshade::api::resource_usage::cpu_access : reshade::api::resource_usage::undefined))
	{
		reshade::vulkan::convert_resource_desc(desc, create_info);
		pCreateInfo = &create_info;

		if (const auto format_list_info = find_in_structure_chain<VkImageFormatListCreateInfo>(
				pCreateInfo->pNext, VK_STRUCTURE_TYPE_IMAGE_FORMAT_LIST_CREATE_INFO))
		{
			// Remove format list info if format was overriden
			if (std::find(format_list_info->pViewFormats, format_list_info->pViewFormats + format_list_info->viewFormatCount, create_info.format) == (format_list_info->pViewFormats + format_list_info->viewFormatCount))
				// This is evil, because writing into application memory, but it is what it is
				const_cast<VkImageFormatListCreateInfo *>(format_list_info)->viewFormatCount = 0;
		}
	}
#endif

	const VkResult result = trampoline(device, pCreateInfo, pAllocator, pImage);
	if (result < VK_SUCCESS)
	{
#if RESHADE_VERBOSE_LOG
		reshade::log::message(reshade::log::level::warning, "vkCreateImage failed with error code %d.", static_cast<int>(result));
#endif
		return result;
	}

#if RESHADE_ADDON
	reshade::vulkan::object_data<VK_OBJECT_TYPE_IMAGE> &data = *device_impl->register_object<VK_OBJECT_TYPE_IMAGE>(*pImage);
	data.create_info = create_info;
	data.create_info.pNext = nullptr; // Clear out structure chain pointer, since it becomes invalid once leaving the current scope
#endif

	return result;
}
void     VKAPI_CALL vkDestroyImage(VkDevice device, VkImage image, const VkAllocationCallbacks *pAllocator)
{
	if (image == VK_NULL_HANDLE)
		return;

	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(device));
	RESHADE_VULKAN_GET_DEVICE_DISPATCH_PTR(DestroyImage, device_impl);

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
	RESHADE_VULKAN_GET_DEVICE_DISPATCH_PTR(CreateImageView, device_impl);

	assert(pCreateInfo != nullptr && pView != nullptr);

#if RESHADE_ADDON
	VkImageViewCreateInfo create_info = *pCreateInfo;
	auto desc = reshade::vulkan::convert_resource_view_desc(create_info);

	if (reshade::invoke_addon_event<reshade::addon_event::create_resource_view>(device_impl, reshade::api::resource { (uint64_t)create_info.image }, reshade::api::resource_usage::undefined, desc))
	{
		reshade::vulkan::convert_resource_view_desc(desc, create_info);
		pCreateInfo = &create_info;
	}
#endif

	const VkResult result = trampoline(device, pCreateInfo, pAllocator, pView);
	if (result < VK_SUCCESS)
	{
#if RESHADE_VERBOSE_LOG
		reshade::log::message(reshade::log::level::warning, "vkCreateImageView failed with error code %d.", static_cast<int>(result));
#endif
		return result;
	}

#if RESHADE_ADDON
	reshade::vulkan::object_data<VK_OBJECT_TYPE_IMAGE_VIEW> &data = *device_impl->register_object<VK_OBJECT_TYPE_IMAGE_VIEW>(*pView);
	data.create_info = create_info;
	data.create_info.pNext = nullptr; // Clear out structure chain pointer, since it becomes invalid once leaving the current scope

	const auto resource_data = device_impl->get_private_data_for_object<VK_OBJECT_TYPE_IMAGE>(create_info.image);
	data.image_extent = resource_data->create_info.extent;
	// Update subresource range to the actual dimensions of the image
	if (VK_REMAINING_MIP_LEVELS == data.create_info.subresourceRange.levelCount)
		data.create_info.subresourceRange.levelCount = resource_data->create_info.mipLevels;
	if (VK_REMAINING_ARRAY_LAYERS == data.create_info.subresourceRange.layerCount)
		data.create_info.subresourceRange.layerCount = resource_data->create_info.arrayLayers;

	reshade::invoke_addon_event<reshade::addon_event::init_resource_view>(
		device_impl, reshade::api::resource { (uint64_t)create_info.image }, reshade::api::resource_usage::undefined, desc, reshade::api::resource_view { (uint64_t)*pView });
#endif

	return result;
}
void     VKAPI_CALL vkDestroyImageView(VkDevice device, VkImageView imageView, const VkAllocationCallbacks *pAllocator)
{
	if (imageView == VK_NULL_HANDLE)
		return;

	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(device));
	RESHADE_VULKAN_GET_DEVICE_DISPATCH_PTR(DestroyImageView, device_impl);

#if RESHADE_ADDON
	reshade::invoke_addon_event<reshade::addon_event::destroy_resource_view>(device_impl, reshade::api::resource_view { (uint64_t)imageView });

	device_impl->unregister_object<VK_OBJECT_TYPE_IMAGE_VIEW>(imageView);
#endif

	trampoline(device, imageView, pAllocator);
}

VkResult VKAPI_CALL vkCreateShaderModule(VkDevice device, const VkShaderModuleCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkShaderModule *pShaderModule)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(device));
	RESHADE_VULKAN_GET_DEVICE_DISPATCH_PTR(CreateShaderModule, device_impl);

	assert(pCreateInfo != nullptr && pShaderModule != nullptr);

	const VkResult result = trampoline(device, pCreateInfo, pAllocator, pShaderModule);
	if (result < VK_SUCCESS)
	{
#if RESHADE_VERBOSE_LOG
		reshade::log::message(reshade::log::level::warning, "vkCreateShaderModule failed with error code %d.", static_cast<int>(result));
#endif
		return result;
	}

#if RESHADE_ADDON >= 2
	reshade::vulkan::object_data<VK_OBJECT_TYPE_SHADER_MODULE> &data = *device_impl->register_object<VK_OBJECT_TYPE_SHADER_MODULE>(*pShaderModule);
	data.spirv.assign(reinterpret_cast<const uint8_t *>(pCreateInfo->pCode), reinterpret_cast<const uint8_t *>(pCreateInfo->pCode) + pCreateInfo->codeSize);
#endif

	return result;
}
void     VKAPI_CALL vkDestroyShaderModule(VkDevice device, VkShaderModule shaderModule, const VkAllocationCallbacks *pAllocator)
{
	if (shaderModule == VK_NULL_HANDLE)
		return;

	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(device));
	RESHADE_VULKAN_GET_DEVICE_DISPATCH_PTR(DestroyShaderModule, device_impl);

#if RESHADE_ADDON >= 2
	device_impl->unregister_object<VK_OBJECT_TYPE_SHADER_MODULE>(shaderModule);
#endif

	trampoline(device, shaderModule, pAllocator);
}

VkResult VKAPI_CALL vkCreateGraphicsPipelines(VkDevice device, VkPipelineCache pipelineCache, uint32_t createInfoCount, const VkGraphicsPipelineCreateInfo *pCreateInfos, const VkAllocationCallbacks *pAllocator, VkPipeline *pPipelines)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(device));
	RESHADE_VULKAN_GET_DEVICE_DISPATCH_PTR(CreateGraphicsPipelines, device_impl);

#if RESHADE_ADDON >= 2
	VkResult result = VK_SUCCESS;
	for (uint32_t i = 0; i < createInfoCount; ++i)
	{
		const VkGraphicsPipelineCreateInfo &create_info = pCreateInfos[i];

		reshade::api::pipeline_flags flags;
		if (const auto flags_info = find_in_structure_chain<VkPipelineCreateFlags2CreateInfo>(
				create_info.pNext, VK_STRUCTURE_TYPE_PIPELINE_CREATE_FLAGS_2_CREATE_INFO))
		{
			flags = reshade::vulkan::convert_pipeline_flags(flags_info->flags);
		}
		else
		{
			flags = reshade::vulkan::convert_pipeline_flags(create_info.flags);
		}

		reshade::api::shader_desc pixel_desc = {};
		reshade::api::shader_desc vertex_desc = {};
		reshade::api::shader_desc geometry_desc = {};
		reshade::api::shader_desc hull_desc = {};
		reshade::api::shader_desc domain_desc = {};
		reshade::api::shader_desc mesh_desc = {};
		reshade::api::shader_desc amplification_desc = {};

		reshade::api::stream_output_desc stream_output_desc;
		reshade::api::blend_desc blend_desc;
		reshade::api::rasterizer_desc rasterizer_desc;
		reshade::api::depth_stencil_desc depth_stencil_desc;
		std::vector<reshade::api::input_element> input_layout;
		reshade::api::primitive_topology topology;

		reshade::api::format depth_stencil_format;
		reshade::api::format render_target_formats[8] = {};

		uint32_t sample_mask;
		uint32_t sample_count;
		uint32_t viewport_count;

		std::vector<reshade::api::pipeline_subobject> subobjects;

		for (uint32_t k = 0; k < create_info.stageCount; ++k)
		{
			const VkPipelineShaderStageCreateInfo &stage = create_info.pStages[k];

			reshade::api::shader_desc *desc = nullptr;
			switch (stage.stage)
			{
			case VK_SHADER_STAGE_VERTEX_BIT:
				desc = &vertex_desc;
				subobjects.push_back({ reshade::api::pipeline_subobject_type::vertex_shader, 1, &vertex_desc });
				break;
			case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:
				desc = &hull_desc;
				subobjects.push_back({ reshade::api::pipeline_subobject_type::hull_shader, 1, &hull_desc });
				break;
			case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:
				desc = &domain_desc;
				subobjects.push_back({ reshade::api::pipeline_subobject_type::domain_shader, 1, &domain_desc });
				break;
			case VK_SHADER_STAGE_GEOMETRY_BIT:
				desc = &geometry_desc;
				subobjects.push_back({ reshade::api::pipeline_subobject_type::geometry_shader, 1, &geometry_desc });
				break;
			case VK_SHADER_STAGE_FRAGMENT_BIT:
				desc = &pixel_desc;
				subobjects.push_back({ reshade::api::pipeline_subobject_type::pixel_shader, 1, &pixel_desc });
				break;
			case VK_SHADER_STAGE_TASK_BIT_EXT:
				desc = &amplification_desc;
				subobjects.push_back({ reshade::api::pipeline_subobject_type::amplification_shader, 1, &amplification_desc });
				break;
			case VK_SHADER_STAGE_MESH_BIT_EXT:
				desc = &mesh_desc;
				subobjects.push_back({ reshade::api::pipeline_subobject_type::mesh_shader, 1, &mesh_desc });
				break;
			default:
				continue;
			}

			desc->entry_point = stage.pName;

			if (stage.module != VK_NULL_HANDLE)
			{
				const auto module_data = device_impl->get_private_data_for_object<VK_OBJECT_TYPE_SHADER_MODULE>(stage.module);

				desc->code = module_data->spirv.data();
				desc->code_size = module_data->spirv.size();
			}
			else if (const auto module_info = find_in_structure_chain<VkShaderModuleCreateInfo>(
				stage.pNext, VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO))
			{
				desc->code = module_info->pCode;
				desc->code_size = module_info->codeSize;
			}
		}

		subobjects.push_back({ reshade::api::pipeline_subobject_type::flags, 1, &flags });

		auto dynamic_states = reshade::vulkan::convert_dynamic_states(create_info.pDynamicState);
		subobjects.push_back({ reshade::api::pipeline_subobject_type::dynamic_pipeline_states, static_cast<uint32_t>(dynamic_states.size()), dynamic_states.data() });

		VkGraphicsPipelineLibraryFlagsEXT library_flags = (flags & reshade::api::pipeline_flags::library) == 0 ? VK_GRAPHICS_PIPELINE_LIBRARY_FLAG_BITS_MAX_ENUM_EXT : 0;

		if (const auto library_info = find_in_structure_chain<VkPipelineLibraryCreateInfoKHR>(
				create_info.pNext, VK_STRUCTURE_TYPE_PIPELINE_LIBRARY_CREATE_INFO_KHR))
		{
			subobjects.push_back({ reshade::api::pipeline_subobject_type::libraries, library_info->libraryCount, const_cast<reshade::api::pipeline *>(reinterpret_cast<const reshade::api::pipeline *>(library_info->pLibraries)) });

			if (library_info->libraryCount != 0)
				library_flags = 0;
		}

		if (const auto library_flags_info = find_in_structure_chain<VkGraphicsPipelineLibraryCreateInfoEXT>(
				create_info.pNext, VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_LIBRARY_CREATE_INFO_EXT))
		{
			library_flags = library_flags_info->flags;
		}

		if ((library_flags & VK_GRAPHICS_PIPELINE_LIBRARY_VERTEX_INPUT_INTERFACE_BIT_EXT) != 0)
		{
			input_layout = reshade::vulkan::convert_input_layout_desc(create_info.pVertexInputState);

			if ((hull_desc.code_size != 0 || domain_desc.code_size != 0) && create_info.pTessellationState != nullptr)
			{
				const VkPipelineTessellationStateCreateInfo &tessellation_state_info = *create_info.pTessellationState;

				topology = static_cast<reshade::api::primitive_topology>(static_cast<uint32_t>(reshade::api::primitive_topology::patch_list_01_cp) + tessellation_state_info.patchControlPoints - 1);
			}
			else if (create_info.pInputAssemblyState != nullptr)
			{
				topology = reshade::vulkan::convert_primitive_topology(create_info.pInputAssemblyState->topology);
			}
			else
			{
				topology = reshade::api::primitive_topology::undefined;
			}

			subobjects.push_back({ reshade::api::pipeline_subobject_type::input_layout, static_cast<uint32_t>(input_layout.size()), input_layout.data() });
			subobjects.push_back({ reshade::api::pipeline_subobject_type::primitive_topology, 1, &topology });
		}
		if ((library_flags & VK_GRAPHICS_PIPELINE_LIBRARY_PRE_RASTERIZATION_SHADERS_BIT_EXT) != 0)
		{
			stream_output_desc = reshade::vulkan::convert_stream_output_desc(create_info.pRasterizationState);
			rasterizer_desc = reshade::vulkan::convert_rasterizer_desc(create_info.pRasterizationState, create_info.pMultisampleState);
			viewport_count = (create_info.pViewportState != nullptr) ? create_info.pViewportState->viewportCount : 1;

			subobjects.push_back({ reshade::api::pipeline_subobject_type::stream_output_state, 1, &stream_output_desc });
			subobjects.push_back({ reshade::api::pipeline_subobject_type::rasterizer_state, 1, &rasterizer_desc });
			subobjects.push_back({ reshade::api::pipeline_subobject_type::viewport_count, 1, &viewport_count });
		}
		if ((library_flags & VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_SHADER_BIT_EXT) != 0)
		{
			depth_stencil_desc = reshade::vulkan::convert_depth_stencil_desc(create_info.pDepthStencilState);

			subobjects.push_back({ reshade::api::pipeline_subobject_type::depth_stencil_state, 1, &depth_stencil_desc });
		}
		if ((library_flags & VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_OUTPUT_INTERFACE_BIT_EXT) != 0)
		{
			blend_desc = reshade::vulkan::convert_blend_desc(create_info.pColorBlendState, create_info.pMultisampleState);

			uint32_t render_target_count = 0;
			depth_stencil_format = reshade::api::format::unknown;

			if (create_info.renderPass != VK_NULL_HANDLE)
			{
				const auto render_pass_data = device_impl->get_private_data_for_object<VK_OBJECT_TYPE_RENDER_PASS>(create_info.renderPass);

				const reshade::vulkan::object_data<VK_OBJECT_TYPE_RENDER_PASS>::subpass &subpass = render_pass_data->subpasses[create_info.subpass];

				render_target_count = subpass.num_color_attachments;

				for (uint32_t k = 0; k < render_target_count; ++k)
				{
					const uint32_t a = subpass.color_attachments[k];
					if (a != VK_ATTACHMENT_UNUSED)
						render_target_formats[k] = reshade::vulkan::convert_format(render_pass_data->attachments[a].format);
				}

				{
					const uint32_t a = subpass.depth_stencil_attachment;
					if (a != VK_ATTACHMENT_UNUSED)
						depth_stencil_format = reshade::vulkan::convert_format(render_pass_data->attachments[subpass.depth_stencil_attachment].format);
				}
			}
			else if (const auto dynamic_rendering_info = find_in_structure_chain<VkPipelineRenderingCreateInfo>(
				create_info.pNext, VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO))
			{
				assert(dynamic_rendering_info->colorAttachmentCount <= 8);
				render_target_count = std::min(dynamic_rendering_info->colorAttachmentCount, 8u);

				for (uint32_t k = 0; k < render_target_count; ++k)
					render_target_formats[k] = reshade::vulkan::convert_format(dynamic_rendering_info->pColorAttachmentFormats[k]);

				if (dynamic_rendering_info->depthAttachmentFormat != VK_FORMAT_UNDEFINED)
					depth_stencil_format = reshade::vulkan::convert_format(dynamic_rendering_info->depthAttachmentFormat);
				else
					depth_stencil_format = reshade::vulkan::convert_format(dynamic_rendering_info->stencilAttachmentFormat);
			}

			sample_mask = (create_info.pMultisampleState != nullptr && create_info.pMultisampleState->pSampleMask != nullptr) ? *create_info.pMultisampleState->pSampleMask : UINT32_MAX;
			sample_count = (create_info.pMultisampleState != nullptr) ? static_cast<uint32_t>(create_info.pMultisampleState->rasterizationSamples) : 1;

			subobjects.push_back({ reshade::api::pipeline_subobject_type::blend_state, 1, &blend_desc });
			subobjects.push_back({ reshade::api::pipeline_subobject_type::render_target_formats, render_target_count, render_target_formats });
			subobjects.push_back({ reshade::api::pipeline_subobject_type::depth_stencil_format, 1, &depth_stencil_format });
			subobjects.push_back({ reshade::api::pipeline_subobject_type::sample_mask, 1, &sample_mask });
			subobjects.push_back({ reshade::api::pipeline_subobject_type::sample_count, 1, &sample_count });
		}

		if (pAllocator == nullptr && // Cannot replace pipeline if custom allocator is used, since corresponding 'vkDestroyPipeline' would be called with mismatching allocator callbacks
			reshade::invoke_addon_event<reshade::addon_event::create_pipeline>(device_impl, reshade::api::pipeline_layout { (uint64_t)create_info.layout }, static_cast<uint32_t>(subobjects.size()), subobjects.data()))
		{
			static_assert(sizeof(*pPipelines) == sizeof(reshade::api::pipeline));

			assert(create_info.pNext == nullptr); // 'device_impl::create_pipeline' does not support extension structures apart from dynamic rendering

			result = device_impl->create_pipeline(
				reshade::api::pipeline_layout { (uint64_t)create_info.layout }, static_cast<uint32_t>(subobjects.size()), subobjects.data(), reinterpret_cast<reshade::api::pipeline *>(&pPipelines[i])) ? VK_SUCCESS : VK_ERROR_OUT_OF_HOST_MEMORY;
		}
		else
		{
			result = trampoline(device, pipelineCache, 1, &create_info, pAllocator, &pPipelines[i]);
		}

		if (result >= VK_SUCCESS)
		{
			reshade::invoke_addon_event<reshade::addon_event::init_pipeline>(
				device_impl, reshade::api::pipeline_layout { (uint64_t)create_info.layout }, static_cast<uint32_t>(subobjects.size()), subobjects.data(), reshade::api::pipeline { (uint64_t)pPipelines[i] });
		}
		else
		{
#if RESHADE_VERBOSE_LOG
			reshade::log::message(reshade::log::level::warning, "vkCreateGraphicsPipelines failed with error code %d.", static_cast<int>(result));
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
	RESHADE_VULKAN_GET_DEVICE_DISPATCH_PTR(CreateComputePipelines, device_impl);

#if RESHADE_ADDON >= 2
	VkResult result = VK_SUCCESS;
	for (uint32_t i = 0; i < createInfoCount; ++i)
	{
		const VkComputePipelineCreateInfo &create_info = pCreateInfos[i];

		reshade::api::pipeline_flags flags;
		if (const auto flags_info = find_in_structure_chain<VkPipelineCreateFlags2CreateInfo>(
				create_info.pNext, VK_STRUCTURE_TYPE_PIPELINE_CREATE_FLAGS_2_CREATE_INFO))
		{
			flags = reshade::vulkan::convert_pipeline_flags(flags_info->flags);
		}
		else
		{
			flags = reshade::vulkan::convert_pipeline_flags(create_info.flags);
		}

		assert(create_info.stage.stage == VK_SHADER_STAGE_COMPUTE_BIT);

		reshade::api::shader_desc compute_desc = {};
		compute_desc.entry_point = create_info.stage.pName;

		if (create_info.stage.module != VK_NULL_HANDLE)
		{
			const auto module_data = device_impl->get_private_data_for_object<VK_OBJECT_TYPE_SHADER_MODULE>(create_info.stage.module);

			compute_desc.code = module_data->spirv.data();
			compute_desc.code_size = module_data->spirv.size();
		}
		else if (const auto module_info = find_in_structure_chain<VkShaderModuleCreateInfo>(
			create_info.stage.pNext, VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO))
		{
			compute_desc.code = module_info->pCode;
			compute_desc.code_size = module_info->codeSize;
		}

		const reshade::api::pipeline_subobject subobjects[] = {
			{ reshade::api::pipeline_subobject_type::compute_shader, 1, &compute_desc },
			{ reshade::api::pipeline_subobject_type::flags, 1, &flags }
		};

		if (pAllocator == nullptr && // Cannot replace pipeline if custom allocator is used, since corresponding 'vkDestroyPipeline' would be called with mismatching allocator callbacks
			reshade::invoke_addon_event<reshade::addon_event::create_pipeline>(device_impl, reshade::api::pipeline_layout { (uint64_t)create_info.layout }, static_cast<uint32_t>(std::size(subobjects)), subobjects))
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
			reshade::log::message(reshade::log::level::warning, "vkCreateComputePipelines failed with error code %d.", static_cast<int>(result));
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
VkResult VKAPI_CALL vkCreateRayTracingPipelinesKHR(VkDevice device, VkDeferredOperationKHR deferredOperation, VkPipelineCache pipelineCache, uint32_t createInfoCount, const VkRayTracingPipelineCreateInfoKHR *pCreateInfos, const VkAllocationCallbacks *pAllocator, VkPipeline *pPipelines)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(device));
	RESHADE_VULKAN_GET_DEVICE_DISPATCH_PTR(CreateRayTracingPipelinesKHR, device_impl);

#if RESHADE_ADDON >= 2
	VkResult result = VK_SUCCESS;
	for (uint32_t i = 0; i < createInfoCount; ++i)
	{
		const VkRayTracingPipelineCreateInfoKHR &create_info = pCreateInfos[i];

		reshade::api::pipeline_flags flags;
		if (const auto flags_info = find_in_structure_chain<VkPipelineCreateFlags2CreateInfo>(
				create_info.pNext, VK_STRUCTURE_TYPE_PIPELINE_CREATE_FLAGS_2_CREATE_INFO))
		{
			flags = reshade::vulkan::convert_pipeline_flags(flags_info->flags);
		}
		else
		{
			flags = reshade::vulkan::convert_pipeline_flags(create_info.flags);
		}

		std::vector<reshade::api::shader_desc> raygen_desc;
		std::vector<reshade::api::shader_desc> any_hit_desc;
		std::vector<reshade::api::shader_desc> closest_hit_desc;
		std::vector<reshade::api::shader_desc> miss_desc;
		std::vector<reshade::api::shader_desc> intersection_desc;
		std::vector<reshade::api::shader_desc> callable_desc;

		std::vector<uint32_t> shader_stage_to_desc_index(create_info.stageCount);
		std::vector<reshade::api::shader_group> shader_groups;

		for (uint32_t k = 0; k < create_info.stageCount; ++k)
		{
			const VkPipelineShaderStageCreateInfo &stage = create_info.pStages[k];

			reshade::api::shader_desc *desc = nullptr;
			switch (stage.stage)
			{
			case VK_SHADER_STAGE_RAYGEN_BIT_KHR:
				desc = &raygen_desc.emplace_back();
				shader_stage_to_desc_index[k] = static_cast<uint32_t>(raygen_desc.size() - 1);
				break;
			case VK_SHADER_STAGE_ANY_HIT_BIT_KHR:
				desc = &any_hit_desc.emplace_back();
				shader_stage_to_desc_index[k] = static_cast<uint32_t>(any_hit_desc.size() - 1);
				break;
			case VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR:
				desc = &closest_hit_desc.emplace_back();
				shader_stage_to_desc_index[k] = static_cast<uint32_t>(closest_hit_desc.size() - 1);
				break;
			case VK_SHADER_STAGE_MISS_BIT_KHR:
				desc = &miss_desc.emplace_back();
				shader_stage_to_desc_index[k] = static_cast<uint32_t>(miss_desc.size() - 1);
				break;
			case VK_SHADER_STAGE_INTERSECTION_BIT_KHR:
				desc = &intersection_desc.emplace_back();
				shader_stage_to_desc_index[k] = static_cast<uint32_t>(intersection_desc.size() - 1);
				break;
			case VK_SHADER_STAGE_CALLABLE_BIT_KHR:
				desc = &callable_desc.emplace_back();
				shader_stage_to_desc_index[k] = static_cast<uint32_t>(callable_desc.size() - 1);
				break;
			default:
				assert(false);
				continue;
			}

			desc->entry_point = stage.pName;

			if (stage.module != VK_NULL_HANDLE)
			{
				const auto module_data = device_impl->get_private_data_for_object<VK_OBJECT_TYPE_SHADER_MODULE>(stage.module);

				desc->code = module_data->spirv.data();
				desc->code_size = module_data->spirv.size();
			}
			else if (const auto module_info = find_in_structure_chain<VkShaderModuleCreateInfo>(
				stage.pNext, VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO))
			{
				desc->code = module_info->pCode;
				desc->code_size = module_info->codeSize;
			}
		}

		shader_groups.reserve(create_info.groupCount);
		for (uint32_t k = 0; k < create_info.groupCount; ++k)
		{
			const VkRayTracingShaderGroupCreateInfoKHR &group_info = create_info.pGroups[k];

			reshade::api::shader_group &shader_group = shader_groups.emplace_back();

			if (group_info.type == VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR)
			{
				assert(group_info.generalShader != VK_SHADER_UNUSED_KHR);

				switch (create_info.pStages[group_info.generalShader].stage)
				{
				case VK_SHADER_STAGE_RAYGEN_BIT_KHR:
					shader_group.type = reshade::api::shader_group_type::raygen;
					shader_group.raygen.shader_index = shader_stage_to_desc_index[group_info.generalShader];
					break;
				case VK_SHADER_STAGE_MISS_BIT_KHR:
					shader_group.type = reshade::api::shader_group_type::miss;
					shader_group.miss.shader_index = shader_stage_to_desc_index[group_info.generalShader];
					break;
				case VK_SHADER_STAGE_CALLABLE_BIT_KHR:
					shader_group.type = reshade::api::shader_group_type::callable;
					shader_group.callable.shader_index = shader_stage_to_desc_index[group_info.generalShader];
					break;
				default:
					assert(false);
					break;
				}
			}
			else
			{
				assert(group_info.generalShader == VK_SHADER_UNUSED_KHR);

				shader_group.type = reshade::vulkan::convert_shader_group_type(group_info.type);
				shader_group.hit_group.closest_hit_shader_index = (group_info.closestHitShader != VK_SHADER_UNUSED_KHR) ? shader_stage_to_desc_index[group_info.closestHitShader] : UINT32_MAX;
				shader_group.hit_group.any_hit_shader_index = (group_info.anyHitShader != VK_SHADER_UNUSED_KHR) ? shader_stage_to_desc_index[group_info.anyHitShader] : UINT32_MAX;
				shader_group.hit_group.intersection_shader_index = (group_info.intersectionShader != VK_SHADER_UNUSED_KHR) ? shader_stage_to_desc_index[group_info.intersectionShader] : UINT32_MAX;
			}
		}

		auto dynamic_states = reshade::vulkan::convert_dynamic_states(create_info.pDynamicState);

		std::vector<reshade::api::pipeline_subobject> subobjects = {
			{ reshade::api::pipeline_subobject_type::raygen_shader, static_cast<uint32_t>(raygen_desc.size()), raygen_desc.data() },
			{ reshade::api::pipeline_subobject_type::any_hit_shader, static_cast<uint32_t>(any_hit_desc.size()), any_hit_desc.data() },
			{ reshade::api::pipeline_subobject_type::closest_hit_shader, static_cast<uint32_t>(closest_hit_desc.size()), closest_hit_desc.data() },
			{ reshade::api::pipeline_subobject_type::miss_shader, static_cast<uint32_t>(miss_desc.size()), miss_desc.data() },
			{ reshade::api::pipeline_subobject_type::callable_shader, static_cast<uint32_t>(callable_desc.size()), callable_desc.data() },
			{ reshade::api::pipeline_subobject_type::shader_groups, static_cast<uint32_t>(shader_groups.size()), shader_groups.data() },
			{ reshade::api::pipeline_subobject_type::max_recursion_depth, create_info.groupCount, const_cast<uint32_t *>(&create_info.maxPipelineRayRecursionDepth) },
			{ reshade::api::pipeline_subobject_type::flags, 1, &flags },
			{ reshade::api::pipeline_subobject_type::dynamic_pipeline_states, static_cast<uint32_t>(dynamic_states.size()), dynamic_states.data() },
		};

		if (create_info.pLibraryInfo != nullptr)
		{
			subobjects.push_back({ reshade::api::pipeline_subobject_type::libraries, create_info.pLibraryInfo->libraryCount, const_cast<reshade::api::pipeline *>(reinterpret_cast<const reshade::api::pipeline *>(create_info.pLibraryInfo->pLibraries)) });
		}

		if (create_info.pLibraryInterface != nullptr)
		{
			subobjects.push_back({ reshade::api::pipeline_subobject_type::max_payload_size, 1, const_cast<uint32_t *>(&create_info.pLibraryInterface->maxPipelineRayPayloadSize) });
			subobjects.push_back({ reshade::api::pipeline_subobject_type::max_attribute_size, 1, const_cast<uint32_t *>(&create_info.pLibraryInterface->maxPipelineRayHitAttributeSize) });
		}

		if (pAllocator == nullptr && // Cannot replace pipeline if custom allocator is used, since corresponding 'vkDestroyPipeline' would be called with mismatching allocator callbacks
			reshade::invoke_addon_event<reshade::addon_event::create_pipeline>(device_impl, reshade::api::pipeline_layout { (uint64_t)create_info.layout }, static_cast<uint32_t>(subobjects.size()), subobjects.data()))
		{
			static_assert(sizeof(*pPipelines) == sizeof(reshade::api::pipeline));

			assert(deferredOperation == VK_NULL_HANDLE);
			assert(create_info.pNext == nullptr); // 'device_impl::create_pipeline' does not support extension structures

			result = device_impl->create_pipeline(
				reshade::api::pipeline_layout { (uint64_t)create_info.layout }, static_cast<uint32_t>(subobjects.size()), subobjects.data(), reinterpret_cast<reshade::api::pipeline *>(&pPipelines[i])) ? VK_SUCCESS : VK_ERROR_OUT_OF_HOST_MEMORY;
		}
		else
		{
			result = trampoline(device, deferredOperation, pipelineCache, 1, &create_info, pAllocator, &pPipelines[i]);
		}

		if (result >= VK_SUCCESS)
		{
			reshade::invoke_addon_event<reshade::addon_event::init_pipeline>(
				device_impl, reshade::api::pipeline_layout { (uint64_t)create_info.layout }, static_cast<uint32_t>(subobjects.size()), subobjects.data(), reshade::api::pipeline {(uint64_t)pPipelines[i]});
		}
		else
		{
#if RESHADE_VERBOSE_LOG
			reshade::log::message(reshade::log::level::warning, "vkCreateRayTracingPipelinesKHR failed with error code %d.", static_cast<int>(result));
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
	return trampoline(device, deferredOperation, pipelineCache, createInfoCount, pCreateInfos, pAllocator, pPipelines);
#endif
}
void     VKAPI_CALL vkDestroyPipeline(VkDevice device, VkPipeline pipeline, const VkAllocationCallbacks *pAllocator)
{
	if (pipeline == VK_NULL_HANDLE)
		return;

	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(device));
	RESHADE_VULKAN_GET_DEVICE_DISPATCH_PTR(DestroyPipeline, device_impl);

#if RESHADE_ADDON >= 2
	reshade::invoke_addon_event<reshade::addon_event::destroy_pipeline>(device_impl, reshade::api::pipeline { (uint64_t)pipeline });
#endif

	trampoline(device, pipeline, pAllocator);
}

VkResult VKAPI_CALL vkCreatePipelineLayout(VkDevice device, const VkPipelineLayoutCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkPipelineLayout *pPipelineLayout)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(device));
	RESHADE_VULKAN_GET_DEVICE_DISPATCH_PTR(CreatePipelineLayout, device_impl);

	assert(pCreateInfo != nullptr && pPipelineLayout != nullptr);

	VkResult result = VK_SUCCESS;
#if RESHADE_ADDON >= 2
	const uint32_t set_desc_count = pCreateInfo->setLayoutCount;
	uint32_t param_count = set_desc_count + pCreateInfo->pushConstantRangeCount;

	std::vector<reshade::api::pipeline_layout_param> params(param_count);

	for (uint32_t i = 0; i < set_desc_count; ++i)
	{
		if (pCreateInfo->pSetLayouts[i] == VK_NULL_HANDLE)
			continue; // This is optional when the pipeline layout is created with 'VK_PIPELINE_LAYOUT_CREATE_INDEPENDENT_SETS_BIT_EXT' flag

		const auto set_layout_impl = device_impl->get_private_data_for_object<VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT>(pCreateInfo->pSetLayouts[i]);

		if (set_layout_impl->push_descriptors)
		{
			if (!set_layout_impl->ranges_with_static_samplers.empty())
			{
				params[i].type = reshade::api::pipeline_layout_param_type::push_descriptors_with_static_samplers;
				params[i].descriptor_table_with_static_samplers.count = static_cast<uint32_t>(set_layout_impl->ranges_with_static_samplers.size());
				params[i].descriptor_table_with_static_samplers.ranges = set_layout_impl->ranges_with_static_samplers.data();
			}
			else if (set_layout_impl->ranges.size() == 1)
			{
				params[i].type = reshade::api::pipeline_layout_param_type::push_descriptors;
				params[i].push_descriptors = set_layout_impl->ranges[0];
			}
			else
			{
				params[i].type = reshade::api::pipeline_layout_param_type::push_descriptors_with_ranges;
				params[i].descriptor_table.count = static_cast<uint32_t>(set_layout_impl->ranges.size());
				params[i].descriptor_table.ranges = set_layout_impl->ranges.data();
			}
		}
		else
		{
			if (!set_layout_impl->ranges_with_static_samplers.empty())
			{
				params[i].type = reshade::api::pipeline_layout_param_type::descriptor_table_with_static_samplers;
				params[i].descriptor_table_with_static_samplers.count = static_cast<uint32_t>(set_layout_impl->ranges_with_static_samplers.size());
				params[i].descriptor_table_with_static_samplers.ranges = set_layout_impl->ranges_with_static_samplers.data();
			}
			else
			{
				params[i].type = reshade::api::pipeline_layout_param_type::descriptor_table;
				params[i].descriptor_table.count = static_cast<uint32_t>(set_layout_impl->ranges.size());
				params[i].descriptor_table.ranges = set_layout_impl->ranges.data();
			}
		}
	}

	for (uint32_t i = set_desc_count; i < param_count; ++i)
	{
		const VkPushConstantRange &push_constant_range = pCreateInfo->pPushConstantRanges[i - set_desc_count];

		params[i].type = reshade::api::pipeline_layout_param_type::push_constants;
		params[i].push_constants.count = push_constant_range.offset + push_constant_range.size;
		params[i].push_constants.visibility = static_cast<reshade::api::shader_stage>(push_constant_range.stageFlags);
	}

	reshade::api::pipeline_layout_param *param_data = params.data();

	if (pAllocator == nullptr && // Cannot replace pipeline layout if custom allocator is used, since corresponding 'vkDestroyPipelineLayout' would be called with mismatching allocator callbacks
		reshade::invoke_addon_event<reshade::addon_event::create_pipeline_layout>(device_impl, param_count, param_data))
	{
		static_assert(sizeof(*pPipelineLayout) == sizeof(reshade::api::pipeline_layout));

		assert(pCreateInfo->pNext == nullptr); // 'device_impl::create_pipeline_layout' does not support extension structures

		result = device_impl->create_pipeline_layout(param_count, param_data, reinterpret_cast<reshade::api::pipeline_layout *>(pPipelineLayout)) ? VK_SUCCESS : VK_ERROR_OUT_OF_HOST_MEMORY;
	}
	else
#endif
	{
		result = trampoline(device, pCreateInfo, pAllocator, pPipelineLayout);
	}

	if (result < VK_SUCCESS)
	{
#if RESHADE_VERBOSE_LOG
		reshade::log::message(reshade::log::level::warning, "vkCreatePipelineLayout failed with error code %d.", static_cast<int>(result));
#endif
		return result;
	}

#if RESHADE_ADDON >= 2
	reshade::vulkan::object_data<VK_OBJECT_TYPE_PIPELINE_LAYOUT> &data = *device_impl->register_object<VK_OBJECT_TYPE_PIPELINE_LAYOUT>(*pPipelineLayout);
	data.set_layouts.assign(pCreateInfo->pSetLayouts, pCreateInfo->pSetLayouts + pCreateInfo->setLayoutCount);

	reshade::invoke_addon_event<reshade::addon_event::init_pipeline_layout>(device_impl, param_count, param_data, reshade::api::pipeline_layout { (uint64_t)*pPipelineLayout });
#endif

	return result;
}
void     VKAPI_CALL vkDestroyPipelineLayout(VkDevice device, VkPipelineLayout pipelineLayout, const VkAllocationCallbacks *pAllocator)
{
	if (pipelineLayout == VK_NULL_HANDLE)
		return;

	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(device));
	RESHADE_VULKAN_GET_DEVICE_DISPATCH_PTR(DestroyPipelineLayout, device_impl);

#if RESHADE_ADDON >= 2
	reshade::invoke_addon_event<reshade::addon_event::destroy_pipeline_layout>(device_impl, reshade::api::pipeline_layout { (uint64_t)pipelineLayout });

	reshade::vulkan::object_data<VK_OBJECT_TYPE_PIPELINE_LAYOUT> &data = *device_impl->get_private_data_for_object<VK_OBJECT_TYPE_PIPELINE_LAYOUT>(pipelineLayout);

	// Clean up any samplers that may have been created when an add-on modified the creation of the pipeline layout
	for (const VkSampler sampler : data.embedded_samplers)
		device_impl->destroy_sampler({ (uint64_t)sampler });

	device_impl->unregister_object<VK_OBJECT_TYPE_PIPELINE_LAYOUT>(pipelineLayout);
#endif

	trampoline(device, pipelineLayout, pAllocator);
}

VkResult VKAPI_CALL vkCreateSampler(VkDevice device, const VkSamplerCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkSampler *pSampler)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(device));
	RESHADE_VULKAN_GET_DEVICE_DISPATCH_PTR(CreateSampler, device_impl);

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
		reshade::log::message(reshade::log::level::warning, "vkCreateSampler failed with error code %d.", static_cast<int>(result));
#endif
		return result;
	}

#if RESHADE_ADDON
	reshade::vulkan::object_data<VK_OBJECT_TYPE_SAMPLER> &data = *device_impl->register_object<VK_OBJECT_TYPE_SAMPLER>(*pSampler);
	data.create_info = create_info;
	data.create_info.pNext = nullptr; // Clear out structure chain pointer, since it becomes invalid once leaving the current scope

	reshade::invoke_addon_event<reshade::addon_event::init_sampler>(device_impl, desc, reshade::api::sampler { (uint64_t)*pSampler });
#endif

	return result;
}
void     VKAPI_CALL vkDestroySampler(VkDevice device, VkSampler sampler, const VkAllocationCallbacks *pAllocator)
{
	if (sampler == VK_NULL_HANDLE)
		return;

	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(device));
	RESHADE_VULKAN_GET_DEVICE_DISPATCH_PTR(DestroySampler, device_impl);

#if RESHADE_ADDON
	reshade::invoke_addon_event<reshade::addon_event::destroy_sampler>(device_impl, reshade::api::sampler { (uint64_t)sampler });

	device_impl->unregister_object<VK_OBJECT_TYPE_SAMPLER>(sampler);
#endif

	trampoline(device, sampler, pAllocator);
}

VkResult VKAPI_CALL vkCreateDescriptorSetLayout(VkDevice device, const VkDescriptorSetLayoutCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkDescriptorSetLayout *pSetLayout)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(device));
	RESHADE_VULKAN_GET_DEVICE_DISPATCH_PTR(CreateDescriptorSetLayout, device_impl);

	assert(pCreateInfo != nullptr && pSetLayout != nullptr);

	const VkResult result = trampoline(device, pCreateInfo, pAllocator, pSetLayout);
	if (result < VK_SUCCESS)
	{
#if RESHADE_VERBOSE_LOG
		reshade::log::message(reshade::log::level::warning, "vkCreateDescriptorSetLayout failed with error code %d.", static_cast<int>(result));
#endif
		return result;
	}

#if RESHADE_ADDON >= 2
	reshade::vulkan::object_data<VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT> &data = *device_impl->register_object<VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT>(*pSetLayout);
	data.num_descriptors = 0;
	data.push_descriptors = (pCreateInfo->flags & VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR) != 0;

	if (pCreateInfo->bindingCount != 0)
	{
		assert(pCreateInfo->pBindings != nullptr);

		const auto binding_flags_info = find_in_structure_chain<VkDescriptorSetLayoutBindingFlagsCreateInfo>(
				pCreateInfo->pNext, VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO);

		bool has_static_samplers = false;

		data.ranges_with_static_samplers.resize(pCreateInfo->bindingCount);
		data.static_samplers.resize(pCreateInfo->bindingCount);
		data.binding_to_offset.reserve(pCreateInfo->bindingCount);

		for (uint32_t i = 0; i < pCreateInfo->bindingCount; ++i)
		{
			const VkDescriptorSetLayoutBinding &binding = pCreateInfo->pBindings[i];

			data.binding_to_offset.resize(std::max(data.binding_to_offset.size(), static_cast<size_t>(binding.binding) + 1));
			data.binding_to_offset[binding.binding] = binding.descriptorCount;

			if ((binding.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLER || binding.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER) && binding.pImmutableSamplers != nullptr)
			{
				has_static_samplers = true;

				for (uint32_t k = 0; k < binding.descriptorCount; ++k)
					data.static_samplers[i].push_back(reshade::vulkan::convert_sampler_desc(
						device_impl->get_private_data_for_object<VK_OBJECT_TYPE_SAMPLER>(binding.pImmutableSamplers[k])->create_info));
			}

			reshade::api::descriptor_range_with_static_samplers &range = data.ranges_with_static_samplers[i];
			range.binding = binding.binding;
			range.dx_register_index = 0;
			range.dx_register_space = 0;
			range.count = binding.descriptorCount;
			range.visibility = static_cast<reshade::api::shader_stage>(binding.stageFlags);
			range.array_size = binding.descriptorCount;
			range.type = reshade::vulkan::convert_descriptor_type(binding.descriptorType);
			range.static_samplers = data.static_samplers[i].data();

			if (binding_flags_info != nullptr && i < binding_flags_info->bindingCount)
			{
				const VkDescriptorBindingFlags binding_flags = binding_flags_info->pBindingFlags[i];

				if ((binding_flags & VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT) != 0)
					range.count = UINT32_MAX;
			}

			data.num_descriptors += binding.descriptorCount;
		}

		for (size_t i = 1; i < data.binding_to_offset.size(); ++i)
			data.binding_to_offset[i] += data.binding_to_offset[i - 1];

		if (!has_static_samplers)
		{
			data.ranges.assign(data.ranges_with_static_samplers.begin(), data.ranges_with_static_samplers.end());
			data.ranges_with_static_samplers.clear();
			data.static_samplers.clear();
		}
	}
#endif

	return result;
}
void     VKAPI_CALL vkDestroyDescriptorSetLayout(VkDevice device, VkDescriptorSetLayout descriptorSetLayout, const VkAllocationCallbacks *pAllocator)
{
	if (descriptorSetLayout == VK_NULL_HANDLE)
		return;

	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(device));
	RESHADE_VULKAN_GET_DEVICE_DISPATCH_PTR(DestroyDescriptorSetLayout, device_impl);

#if RESHADE_ADDON >= 2
	device_impl->unregister_object<VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT>(descriptorSetLayout);
#endif

	trampoline(device, descriptorSetLayout, pAllocator);
}

VkResult VKAPI_CALL vkCreateDescriptorPool(VkDevice device, const VkDescriptorPoolCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkDescriptorPool *pDescriptorPool)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(device));
	RESHADE_VULKAN_GET_DEVICE_DISPATCH_PTR(CreateDescriptorPool, device_impl);

	assert(pCreateInfo != nullptr && pDescriptorPool != nullptr);

	const VkResult result = trampoline(device, pCreateInfo, pAllocator, pDescriptorPool);
	if (result < VK_SUCCESS)
	{
#if RESHADE_VERBOSE_LOG
		reshade::log::message(reshade::log::level::warning, "vkCreateDescriptorPool failed with error code %d.", static_cast<int>(result));
#endif
		return result;
	}

#if RESHADE_ADDON >= 2
	reshade::vulkan::object_data<VK_OBJECT_TYPE_DESCRIPTOR_POOL> &data = *device_impl->register_object<VK_OBJECT_TYPE_DESCRIPTOR_POOL>(*pDescriptorPool);
	data.max_sets = pCreateInfo->maxSets;
	data.max_descriptors = 0;
	data.next_set = 0;
	data.next_offset = 0;
	data.sets.resize(data.max_sets);

	for (uint32_t i = 0; i < pCreateInfo->poolSizeCount; ++i)
		data.max_descriptors += pCreateInfo->pPoolSizes[i].descriptorCount;
#endif

	return result;
}
void     VKAPI_CALL vkDestroyDescriptorPool(VkDevice device, VkDescriptorPool descriptorPool, const VkAllocationCallbacks *pAllocator)
{
	if (descriptorPool == VK_NULL_HANDLE)
		return;

	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(device));
	RESHADE_VULKAN_GET_DEVICE_DISPATCH_PTR(DestroyDescriptorPool, device_impl);

#if RESHADE_ADDON >= 2
	device_impl->unregister_object<VK_OBJECT_TYPE_DESCRIPTOR_POOL>(descriptorPool);
#endif

	trampoline(device, descriptorPool, pAllocator);
}

VkResult VKAPI_CALL vkResetDescriptorPool(VkDevice device, VkDescriptorPool descriptorPool, VkDescriptorPoolResetFlags flags)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(device));
	RESHADE_VULKAN_GET_DEVICE_DISPATCH_PTR(ResetDescriptorPool, device_impl);

#if RESHADE_ADDON >= 2
	const auto pool_data = device_impl->get_private_data_for_object<VK_OBJECT_TYPE_DESCRIPTOR_POOL>(descriptorPool);

	pool_data->next_set = 0;
	pool_data->next_offset = 0;
#endif

	return trampoline(device, descriptorPool, flags);
}
VkResult VKAPI_CALL vkAllocateDescriptorSets(VkDevice device, const VkDescriptorSetAllocateInfo *pAllocateInfo, VkDescriptorSet *pDescriptorSets)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(device));
	RESHADE_VULKAN_GET_DEVICE_DISPATCH_PTR(AllocateDescriptorSets, device_impl);

	assert(pAllocateInfo != nullptr && pDescriptorSets != nullptr);

	const VkResult result = trampoline(device, pAllocateInfo, pDescriptorSets);
	if (result < VK_SUCCESS)
	{
#if RESHADE_VERBOSE_LOG
		reshade::log::message(reshade::log::level::warning, "vkAllocateDescriptorSets failed with error code %d.", static_cast<int>(result));
#endif
		return result;
	}

#if RESHADE_ADDON >= 2
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

		device_impl->register_object<VK_OBJECT_TYPE_DESCRIPTOR_SET>(pDescriptorSets[i], data);
	}
#endif

	return result;
}
VkResult VKAPI_CALL vkFreeDescriptorSets(VkDevice device, VkDescriptorPool descriptorPool, uint32_t descriptorSetCount, const VkDescriptorSet *pDescriptorSets)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(device));
	RESHADE_VULKAN_GET_DEVICE_DISPATCH_PTR(FreeDescriptorSets, device_impl);

	assert(pDescriptorSets != nullptr);

#if RESHADE_ADDON >= 2
	for (uint32_t i = 0; i < descriptorSetCount; ++i)
	{
		if (pDescriptorSets[i] == VK_NULL_HANDLE)
			continue;

		device_impl->unregister_object<VK_OBJECT_TYPE_DESCRIPTOR_SET, false>(pDescriptorSets[i]);
	}
#endif

	return trampoline(device, descriptorPool, descriptorSetCount, pDescriptorSets);
}

void     VKAPI_CALL vkUpdateDescriptorSets(VkDevice device, uint32_t descriptorWriteCount, const VkWriteDescriptorSet *pDescriptorWrites, uint32_t descriptorCopyCount, const VkCopyDescriptorSet *pDescriptorCopies)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(device));
	RESHADE_VULKAN_GET_DEVICE_DISPATCH_PTR(UpdateDescriptorSets, device_impl);

#if RESHADE_ADDON >= 2
	if (descriptorWriteCount != 0 && reshade::has_addon_event<reshade::addon_event::update_descriptor_tables>())
	{
		temp_mem<reshade::api::descriptor_table_update> updates(descriptorWriteCount);

		uint32_t max_descriptors = 0;
		for (uint32_t i = 0; i < descriptorWriteCount; ++i)
			max_descriptors += pDescriptorWrites[i].descriptorCount;
		temp_mem<uint64_t> descriptors(max_descriptors * 2);

		for (uint32_t i = 0, j = 0; i < descriptorWriteCount; ++i)
		{
			const VkWriteDescriptorSet &write = pDescriptorWrites[i];

			reshade::api::descriptor_table_update &update = updates[i];
			update.table = { (uint64_t)write.dstSet };
			update.binding = write.dstBinding;
			update.array_offset = write.dstArrayElement;
			update.count = write.descriptorCount;
			update.type = reshade::vulkan::convert_descriptor_type(write.descriptorType);
			update.descriptors = descriptors.p + j;

			switch (write.descriptorType)
			{
			case VK_DESCRIPTOR_TYPE_SAMPLER:
				for (uint32_t k = 0; k < write.descriptorCount; ++k, ++j)
					descriptors[j] = (uint64_t)write.pImageInfo[k].sampler;
				break;
			case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
				for (uint32_t k = 0; k < write.descriptorCount; ++k, j += 2)
					descriptors[j + 0] = (uint64_t)write.pImageInfo[k].sampler,
					descriptors[j + 1] = (uint64_t)write.pImageInfo[k].imageView;
				break;
			case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
			case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
				for (uint32_t k = 0; k < write.descriptorCount; ++k, ++j)
					descriptors[j] = (uint64_t)write.pImageInfo[k].imageView;
				break;
			case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
			case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
				for (uint32_t k = 0; k < write.descriptorCount; ++k, ++j)
					descriptors[j] = (uint64_t)write.pTexelBufferView[k];
				break;
			case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
			case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
				static_assert(sizeof(reshade::api::buffer_range) == sizeof(VkDescriptorBufferInfo));
				update.descriptors = write.pBufferInfo;
				break;
			case VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR:
				if (const auto write_acceleration_structure = find_in_structure_chain<VkWriteDescriptorSetAccelerationStructureKHR>(write.pNext, VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR))
				{
					assert(update.count == write_acceleration_structure->accelerationStructureCount);
					update.descriptors = write_acceleration_structure->pAccelerationStructures;
					break;
				}
				[[fallthrough]];
			default:
				update.count = 0;
				update.descriptors = nullptr;
				break;
			}
		}

		if (reshade::invoke_addon_event<reshade::addon_event::update_descriptor_tables>(device_impl, descriptorWriteCount, updates.p))
			descriptorWriteCount = 0;
	}

	if (descriptorCopyCount != 0 && reshade::has_addon_event<reshade::addon_event::copy_descriptor_tables>())
	{
		temp_mem<reshade::api::descriptor_table_copy> copies(descriptorCopyCount);

		for (uint32_t i = 0; i < descriptorCopyCount; ++i)
		{
			const VkCopyDescriptorSet &internal_copy = pDescriptorCopies[i];

			reshade::api::descriptor_table_copy &copy = copies[i];
			copy.source_table = { (uint64_t)internal_copy.srcSet };
			copy.source_binding = internal_copy.srcBinding;
			copy.source_array_offset = internal_copy.srcArrayElement;
			copy.dest_table = { (uint64_t)internal_copy.dstSet };
			copy.dest_binding = internal_copy.dstBinding;
			copy.dest_array_offset = internal_copy.dstArrayElement;
			copy.count = internal_copy.descriptorCount;
		}

		if (reshade::invoke_addon_event<reshade::addon_event::copy_descriptor_tables>(device_impl, descriptorCopyCount, copies.p))
			descriptorCopyCount = 0;
	}
#endif

	trampoline(device, descriptorWriteCount, pDescriptorWrites, descriptorCopyCount, pDescriptorCopies);
}

VkResult VKAPI_CALL vkCreateDescriptorUpdateTemplate(VkDevice device, const VkDescriptorUpdateTemplateCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkDescriptorUpdateTemplate *pDescriptorUpdateTemplate)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(device));
	RESHADE_VULKAN_GET_DEVICE_DISPATCH_PTR(CreateDescriptorUpdateTemplate, device_impl);

	const VkResult result = trampoline(device, pCreateInfo, pAllocator, pDescriptorUpdateTemplate);
	if (result < VK_SUCCESS)
	{
#if RESHADE_VERBOSE_LOG
		reshade::log::message(reshade::log::level::warning, "vkCreateDescriptorUpdateTemplate failed with error code %d.", static_cast<int>(result));
#endif
		return result;
	}

#if RESHADE_ADDON >= 2
	reshade::vulkan::object_data<VK_OBJECT_TYPE_DESCRIPTOR_UPDATE_TEMPLATE> &data = *device_impl->register_object<VK_OBJECT_TYPE_DESCRIPTOR_UPDATE_TEMPLATE>(*pDescriptorUpdateTemplate);
	data.bind_point = pCreateInfo->pipelineBindPoint;
	data.entries.assign(pCreateInfo->pDescriptorUpdateEntries, pCreateInfo->pDescriptorUpdateEntries + pCreateInfo->descriptorUpdateEntryCount);
#endif

	return result;
}
void     VKAPI_CALL vkDestroyDescriptorUpdateTemplate(VkDevice device, VkDescriptorUpdateTemplate descriptorUpdateTemplate, const VkAllocationCallbacks *pAllocator)
{
	if (descriptorUpdateTemplate == VK_NULL_HANDLE)
		return;

	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(device));
	RESHADE_VULKAN_GET_DEVICE_DISPATCH_PTR(DestroyDescriptorUpdateTemplate, device_impl);

#if RESHADE_ADDON >= 2
	device_impl->unregister_object<VK_OBJECT_TYPE_DESCRIPTOR_UPDATE_TEMPLATE>(descriptorUpdateTemplate);
#endif

	trampoline(device, descriptorUpdateTemplate, pAllocator);
}

void     VKAPI_CALL vkUpdateDescriptorSetWithTemplate(VkDevice device, VkDescriptorSet descriptorSet, VkDescriptorUpdateTemplate descriptorUpdateTemplate, const void *pData)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(device));
	RESHADE_VULKAN_GET_DEVICE_DISPATCH_PTR(UpdateDescriptorSetWithTemplate, device_impl);

#if RESHADE_ADDON >= 2
	if (reshade::has_addon_event<reshade::addon_event::update_descriptor_tables>())
	{
		const auto template_data = device_impl->get_private_data_for_object<VK_OBJECT_TYPE_DESCRIPTOR_UPDATE_TEMPLATE>(descriptorUpdateTemplate);

		temp_mem<reshade::api::descriptor_table_update> updates(template_data->entries.size());

		uint32_t max_descriptors = 0;
		for (const VkDescriptorUpdateTemplateEntry &entry : template_data->entries)
			max_descriptors += entry.descriptorCount;
		temp_mem<uint64_t> descriptors(max_descriptors * 2);

		for (uint32_t i = 0, j = 0; i < static_cast<uint32_t>(template_data->entries.size()); ++i)
		{
			const VkDescriptorUpdateTemplateEntry &entry = template_data->entries[i];

			reshade::api::descriptor_table_update &update = updates[i];
			update.table = { (uint64_t)descriptorSet };
			update.binding = entry.dstBinding;
			update.array_offset = entry.dstArrayElement;
			update.count = entry.descriptorCount;
			update.type = reshade::vulkan::convert_descriptor_type(entry.descriptorType);
			update.descriptors = descriptors.p + j;

			const void *const base = static_cast<const uint8_t *>(pData) + entry.offset;

			switch (entry.descriptorType)
			{
			case VK_DESCRIPTOR_TYPE_SAMPLER:
				for (uint32_t k = 0; k < entry.descriptorCount; ++k, ++j)
					descriptors[j] = (uint64_t)static_cast<const VkDescriptorImageInfo *>(base)[k].sampler;
				break;
			case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
				for (uint32_t k = 0; k < entry.descriptorCount; ++k, j += 2)
					descriptors[j + 0] = (uint64_t)static_cast<const VkDescriptorImageInfo *>(base)[k].sampler,
					descriptors[j + 1] = (uint64_t)static_cast<const VkDescriptorImageInfo *>(base)[k].imageView;
				break;
			case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
			case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
				for (uint32_t k = 0; k < entry.descriptorCount; ++k, ++j)
					descriptors[j] = (uint64_t)static_cast<const VkDescriptorImageInfo *>(base)[k].imageView;
				break;
			case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
			case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
				for (uint32_t k = 0; k < entry.descriptorCount; ++k, ++j)
					descriptors[j] = (uint64_t)static_cast<const VkBufferView *>(base)[k];
				break;
			case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
			case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
				update.descriptors = static_cast<const VkDescriptorBufferInfo *>(base);
				break;
			case VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR:
				update.descriptors = static_cast<const VkAccelerationStructureKHR *>(base);
				break;
			default:
				update.count = 0;
				update.descriptors = nullptr;
				break;
			}
		}

		if (reshade::invoke_addon_event<reshade::addon_event::update_descriptor_tables>(device_impl, static_cast<uint32_t>(template_data->entries.size()), updates.p))
			return;
	}
#endif

	trampoline(device, descriptorSet, descriptorUpdateTemplate, pData);
}

VkResult VKAPI_CALL vkCreateFramebuffer(VkDevice device, const VkFramebufferCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkFramebuffer *pFramebuffer)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(device));
	RESHADE_VULKAN_GET_DEVICE_DISPATCH_PTR(CreateFramebuffer, device_impl);

	assert(pCreateInfo != nullptr && pFramebuffer != nullptr);

	const VkResult result = trampoline(device, pCreateInfo, pAllocator, pFramebuffer);
	if (result < VK_SUCCESS)
	{
#if RESHADE_VERBOSE_LOG
		reshade::log::message(reshade::log::level::warning, "vkCreateFramebuffer failed with error code %d.", static_cast<int>(result));
#endif
		return result;
	}

#if RESHADE_ADDON
	// Keep track of the frame buffer attachments
	reshade::vulkan::object_data<VK_OBJECT_TYPE_FRAMEBUFFER> &data = *device_impl->register_object<VK_OBJECT_TYPE_FRAMEBUFFER>(*pFramebuffer);
	if ((pCreateInfo->flags & VK_FRAMEBUFFER_CREATE_IMAGELESS_BIT) != 0)
	{
		data.attachments.resize(pCreateInfo->attachmentCount);
	}
	else
	{
		assert(pCreateInfo->pAttachments != nullptr);
		data.attachments.assign(pCreateInfo->pAttachments, pCreateInfo->pAttachments + pCreateInfo->attachmentCount);
	}
#endif

	return result;
}
void     VKAPI_CALL vkDestroyFramebuffer(VkDevice device, VkFramebuffer framebuffer, const VkAllocationCallbacks *pAllocator)
{
	if (framebuffer == VK_NULL_HANDLE)
		return;

	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(device));
	RESHADE_VULKAN_GET_DEVICE_DISPATCH_PTR(DestroyFramebuffer, device_impl);

#if RESHADE_ADDON
	device_impl->unregister_object<VK_OBJECT_TYPE_FRAMEBUFFER>(framebuffer);
#endif

	trampoline(device, framebuffer, pAllocator);
}

VkResult VKAPI_CALL vkCreateRenderPass(VkDevice device, const VkRenderPassCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkRenderPass *pRenderPass)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(device));
	RESHADE_VULKAN_GET_DEVICE_DISPATCH_PTR(CreateRenderPass, device_impl);

	assert(pCreateInfo != nullptr && pRenderPass != nullptr);

#if 0
	// Hack that prevents artifacts when trying to use depth buffer after render pass finished (see also comment in 'on_begin_render_pass_with_depth_stencil' in generic depth add-on)
	for (uint32_t a = 0; a < pCreateInfo->attachmentCount; ++a)
		if (pCreateInfo->pAttachments[a].storeOp == VK_ATTACHMENT_STORE_OP_DONT_CARE)
			const_cast<VkAttachmentDescription &>(pCreateInfo->pAttachments[a]).storeOp = VK_ATTACHMENT_STORE_OP_STORE;
#endif

	const VkResult result = trampoline(device, pCreateInfo, pAllocator, pRenderPass);
	if (result < VK_SUCCESS)
	{
#if RESHADE_VERBOSE_LOG
		reshade::log::message(reshade::log::level::warning, "vkCreateRenderPass failed with error code %d.", static_cast<int>(result));
#endif
		return result;
	}

#if RESHADE_ADDON
	reshade::vulkan::object_data<VK_OBJECT_TYPE_RENDER_PASS> &data = *device_impl->register_object<VK_OBJECT_TYPE_RENDER_PASS>(*pRenderPass);
	data.subpasses.resize(pCreateInfo->subpassCount);
	data.attachments.assign(pCreateInfo->pAttachments, pCreateInfo->pAttachments + pCreateInfo->attachmentCount);

	for (uint32_t i = 0; i < pCreateInfo->subpassCount; ++i)
	{
		reshade::vulkan::object_data<VK_OBJECT_TYPE_RENDER_PASS>::subpass &dst_subpass = data.subpasses[i];
		const VkSubpassDescription &src_subpass = pCreateInfo->pSubpasses[i];

		assert(src_subpass.colorAttachmentCount <= 8);
		dst_subpass.num_color_attachments = std::min(src_subpass.colorAttachmentCount, 8u);

		for (uint32_t a = 0; a < dst_subpass.num_color_attachments; ++a)
			dst_subpass.color_attachments[a] = src_subpass.pColorAttachments[a].attachment;

		dst_subpass.depth_stencil_attachment = (src_subpass.pDepthStencilAttachment != nullptr) ? src_subpass.pDepthStencilAttachment->attachment : VK_ATTACHMENT_UNUSED;
	}
#endif

	return result;
}
VkResult VKAPI_CALL vkCreateRenderPass2(VkDevice device, const VkRenderPassCreateInfo2 *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkRenderPass *pRenderPass)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(device));
	RESHADE_VULKAN_GET_DEVICE_DISPATCH_PTR(CreateRenderPass2, device_impl);

	assert(pCreateInfo != nullptr && pRenderPass != nullptr);

	const VkResult result = trampoline(device, pCreateInfo, pAllocator, pRenderPass);
	if (result < VK_SUCCESS)
	{
#if RESHADE_VERBOSE_LOG
		reshade::log::message(reshade::log::level::warning, "vkCreateRenderPass2 failed with error code %d.", static_cast<int>(result));
#endif
		return result;
	}

#if RESHADE_ADDON
	reshade::vulkan::object_data<VK_OBJECT_TYPE_RENDER_PASS> &data = *device_impl->register_object<VK_OBJECT_TYPE_RENDER_PASS>(*pRenderPass);
	data.subpasses.resize(pCreateInfo->subpassCount);
	data.attachments.resize(pCreateInfo->attachmentCount);

	for (uint32_t i = 0; i < pCreateInfo->subpassCount; ++i)
	{
		reshade::vulkan::object_data<VK_OBJECT_TYPE_RENDER_PASS>::subpass &dst_subpass = data.subpasses[i];
		const VkSubpassDescription2 &src_subpass = pCreateInfo->pSubpasses[i];

		assert(src_subpass.colorAttachmentCount <= 8);
		dst_subpass.num_color_attachments = std::min(src_subpass.colorAttachmentCount, 8u);

		for (uint32_t a = 0; a < dst_subpass.num_color_attachments; ++a)
			dst_subpass.color_attachments[a] = src_subpass.pColorAttachments[a].attachment;

		dst_subpass.depth_stencil_attachment = (src_subpass.pDepthStencilAttachment != nullptr) ? src_subpass.pDepthStencilAttachment->attachment : VK_ATTACHMENT_UNUSED;
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
#endif

	return result;
}
void     VKAPI_CALL vkDestroyRenderPass(VkDevice device, VkRenderPass renderPass, const VkAllocationCallbacks *pAllocator)
{
	if (renderPass == VK_NULL_HANDLE)
		return;

	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(device));
	RESHADE_VULKAN_GET_DEVICE_DISPATCH_PTR(DestroyRenderPass, device_impl);

#if RESHADE_ADDON
	device_impl->unregister_object<VK_OBJECT_TYPE_RENDER_PASS>(renderPass);
#endif

	trampoline(device, renderPass, pAllocator);
}

VkResult VKAPI_CALL vkAllocateCommandBuffers(VkDevice device, const VkCommandBufferAllocateInfo *pAllocateInfo, VkCommandBuffer *pCommandBuffers)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(device));
	RESHADE_VULKAN_GET_DEVICE_DISPATCH_PTR(AllocateCommandBuffers, device_impl);

	const VkResult result = trampoline(device, pAllocateInfo, pCommandBuffers);
	if (result < VK_SUCCESS)
	{
#if RESHADE_VERBOSE_LOG
		reshade::log::message(reshade::log::level::warning, "vkAllocateCommandBuffers failed with error code %d.", static_cast<int>(result));
#endif
		return result;
	}

#if RESHADE_ADDON
	for (uint32_t i = 0; i < pAllocateInfo->commandBufferCount; ++i)
	{
		const auto cmd_impl = new reshade::vulkan::object_data<VK_OBJECT_TYPE_COMMAND_BUFFER>(device_impl, pCommandBuffers[i]);

		device_impl->register_object<VK_OBJECT_TYPE_COMMAND_BUFFER>(pCommandBuffers[i], cmd_impl);

		reshade::invoke_addon_event<reshade::addon_event::init_command_list>(cmd_impl);
	}
#endif

	return result;
}
void     VKAPI_CALL vkFreeCommandBuffers(VkDevice device, VkCommandPool commandPool, uint32_t commandBufferCount, const VkCommandBuffer *pCommandBuffers)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(device));
	RESHADE_VULKAN_GET_DEVICE_DISPATCH_PTR(FreeCommandBuffers, device_impl);

	assert(pCommandBuffers != nullptr);

#if RESHADE_ADDON
	for (uint32_t i = 0; i < commandBufferCount; ++i)
	{
		if (pCommandBuffers[i] == VK_NULL_HANDLE)
			continue;

		reshade::vulkan::object_data<VK_OBJECT_TYPE_COMMAND_BUFFER> *const cmd_impl = device_impl->get_private_data_for_object<VK_OBJECT_TYPE_COMMAND_BUFFER>(pCommandBuffers[i]);

		reshade::invoke_addon_event<reshade::addon_event::destroy_command_list>(cmd_impl);

		device_impl->unregister_object<VK_OBJECT_TYPE_COMMAND_BUFFER, false>(pCommandBuffers[i]);

		delete cmd_impl;
	}
#endif

	trampoline(device, commandPool, commandBufferCount, pCommandBuffers);
}

VkResult VKAPI_CALL vkCreateAccelerationStructureKHR(VkDevice device, const VkAccelerationStructureCreateInfoKHR *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkAccelerationStructureKHR *pAccelerationStructure)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(device));
	RESHADE_VULKAN_GET_DEVICE_DISPATCH_PTR(CreateAccelerationStructureKHR, device_impl);

	assert(pCreateInfo != nullptr && pAccelerationStructure != nullptr);

#if RESHADE_ADDON
	VkAccelerationStructureCreateInfoKHR create_info = *pCreateInfo;
	auto desc = reshade::vulkan::convert_resource_view_desc(create_info);

	if (reshade::invoke_addon_event<reshade::addon_event::create_resource_view>(device_impl, reshade::api::resource { (uint64_t)create_info.buffer }, reshade::api::resource_usage::acceleration_structure, desc))
	{
		reshade::vulkan::convert_resource_view_desc(desc, create_info);
		pCreateInfo = &create_info;
	}
#endif

	const VkResult result = trampoline(device, pCreateInfo, pAllocator, pAccelerationStructure);
	if (result < VK_SUCCESS)
	{
#if RESHADE_VERBOSE_LOG
		reshade::log::message(reshade::log::level::warning, "vkCreateAccelerationStructureKHR failed with error code %d.", static_cast<int>(result));
#endif
		return result;
	}

#if RESHADE_ADDON
	reshade::invoke_addon_event<reshade::addon_event::init_resource_view>(
		device_impl, reshade::api::resource { (uint64_t)create_info.buffer }, reshade::api::resource_usage::acceleration_structure, desc, reshade::api::resource_view { (uint64_t)*pAccelerationStructure });
#endif

	return result;
}
void     VKAPI_CALL vkDestroyAccelerationStructureKHR(VkDevice device, VkAccelerationStructureKHR accelerationStructure, const VkAllocationCallbacks *pAllocator)
{
	if (accelerationStructure == VK_NULL_HANDLE)
		return;

	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(device));
	RESHADE_VULKAN_GET_DEVICE_DISPATCH_PTR(DestroyAccelerationStructureKHR, device_impl);

#if RESHADE_ADDON
	reshade::invoke_addon_event<reshade::addon_event::destroy_resource_view>(device_impl, reshade::api::resource_view { (uint64_t)accelerationStructure });
#endif

	trampoline(device, accelerationStructure, pAllocator);
}
