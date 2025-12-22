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
#include "addon_manager.hpp"
#include "runtime_manager.hpp"
#include "lockfree_linear_map.hpp"
#include <algorithm> // std::fill_n, std::sort, std::unique

extern thread_local bool g_in_dxgi_runtime;

extern lockfree_linear_map<VkSurfaceKHR, HWND, 16> g_vulkan_surfaces;
extern lockfree_linear_map<void *, reshade::vulkan::device_impl *, 8> g_vulkan_devices;

#if RESHADE_ADDON
extern void create_default_view(reshade::vulkan::device_impl *device_impl, VkImage image);
extern void destroy_default_view(reshade::vulkan::device_impl *device_impl, VkImage image);
#endif

VkResult VKAPI_CALL vkCreateSwapchainKHR(VkDevice device, const VkSwapchainCreateInfoKHR *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkSwapchainKHR *pSwapchain)
{
	reshade::log::message(reshade::log::level::info, "Redirecting vkCreateSwapchainKHR(device = %p, pCreateInfo = %p, pAllocator = %p, pSwapchain = %p) ...", device, pCreateInfo, pAllocator, pSwapchain);

	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(device));
	RESHADE_VULKAN_GET_DEVICE_DISPATCH_PTR(CreateSwapchainKHR, device_impl);

	assert(pCreateInfo != nullptr && pSwapchain != nullptr);

	std::vector<VkFormat> format_list;
	std::vector<uint32_t> queue_family_list;
	VkSwapchainCreateInfoKHR create_info = *pCreateInfo;
	VkImageFormatListCreateInfo format_list_info;

	// Only have to enable additional features if there is a graphics queue, since ReShade will not run otherwise
	if (device_impl->_primary_graphics_queue != nullptr)
	{
		// Add required usage flags to create info
		create_info.imageUsage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

		// Add required format variants, so e.g. both linear and sRGB views can be created for the swap chain images
		format_list.push_back(reshade::vulkan::convert_format(
			reshade::api::format_to_default_typed(reshade::vulkan::convert_format(create_info.imageFormat), 0)));
		format_list.push_back(reshade::vulkan::convert_format(
			reshade::api::format_to_default_typed(reshade::vulkan::convert_format(create_info.imageFormat), 1)));

		// Only have to make format mutable if they are actually different
		if (format_list[0] != format_list[1])
			create_info.flags |= VK_SWAPCHAIN_CREATE_MUTABLE_FORMAT_BIT_KHR;

		// Patch the format list in the create info of the application
		if (const auto format_list_info2 = find_in_structure_chain<VkImageFormatListCreateInfo>(
				pCreateInfo->pNext, VK_STRUCTURE_TYPE_IMAGE_FORMAT_LIST_CREATE_INFO))
		{
			format_list.insert(format_list.end(),
				format_list_info2->pViewFormats, format_list_info2->pViewFormats + format_list_info2->viewFormatCount);

			// Remove duplicates from the list (since the new formats may have already been added by the application)
			std::sort(format_list.begin(), format_list.end());
			format_list.erase(std::unique(format_list.begin(), format_list.end()), format_list.end());

			// This is evil, because writing into application memory, but eh =)
			const_cast<VkImageFormatListCreateInfo *>(format_list_info2)->viewFormatCount = static_cast<uint32_t>(format_list.size());
			const_cast<VkImageFormatListCreateInfo *>(format_list_info2)->pViewFormats = format_list.data();
		}
		else if (format_list[0] != format_list[1])
		{
			format_list_info = { VK_STRUCTURE_TYPE_IMAGE_FORMAT_LIST_CREATE_INFO };
			format_list_info.pNext = create_info.pNext;
			format_list_info.viewFormatCount = static_cast<uint32_t>(format_list.size());
			format_list_info.pViewFormats = format_list.data();

			create_info.pNext = &format_list_info;
		}

		// Add required queue family indices, so images can be used on the graphics queue
		if (create_info.imageSharingMode == VK_SHARING_MODE_CONCURRENT)
		{
			queue_family_list.reserve(create_info.queueFamilyIndexCount + 1);
			queue_family_list.push_back(device_impl->_primary_graphics_queue_family_index);

			for (uint32_t i = 0; i < create_info.queueFamilyIndexCount; ++i)
				if (create_info.pQueueFamilyIndices[i] != device_impl->_primary_graphics_queue_family_index)
					queue_family_list.push_back(create_info.pQueueFamilyIndices[i]);

			create_info.queueFamilyIndexCount = static_cast<uint32_t>(queue_family_list.size());
			create_info.pQueueFamilyIndices = queue_family_list.data();
		}
	}

	// Dump swap chain description
	{
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
		case VK_FORMAT_A2B10G10R10_UNORM_PACK32:
			format_string = "VK_FORMAT_A2B10G10R10_UNORM_PACK32";
			break;
		case VK_FORMAT_A2R10G10B10_UNORM_PACK32:
			format_string = "VK_FORMAT_A2R10G10B10_UNORM_PACK32";
			break;
		case VK_FORMAT_R16G16B16A16_UNORM:
			format_string = "VK_FORMAT_R16G16B16A16_UNORM";
			break;
		case VK_FORMAT_R16G16B16A16_SFLOAT:
			format_string = "VK_FORMAT_R16G16B16A16_SFLOAT";
			break;
		}

		const char *color_space_string = nullptr;
		switch (create_info.imageColorSpace)
		{
		case VK_COLOR_SPACE_SRGB_NONLINEAR_KHR:
			color_space_string = "VK_COLOR_SPACE_SRGB_NONLINEAR_KHR";
			break;
		case VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT:
			color_space_string = "VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT";
			break;
		case VK_COLOR_SPACE_BT2020_LINEAR_EXT:
			color_space_string = "VK_COLOR_SPACE_BT2020_LINEAR_EXT";
			break;
		case VK_COLOR_SPACE_HDR10_ST2084_EXT:
			color_space_string = "VK_COLOR_SPACE_HDR10_ST2084_EXT";
			break;
		case VK_COLOR_SPACE_HDR10_HLG_EXT:
			color_space_string = "VK_COLOR_SPACE_HDR10_HLG_EXT";
			break;
		}

		reshade::log::message(reshade::log::level::info, "> Dumping swap chain description:");
		reshade::log::message(reshade::log::level::info, "  +-----------------------------------------+-----------------------------------------+");
		reshade::log::message(reshade::log::level::info, "  | Parameter                               | Value                                   |");
		reshade::log::message(reshade::log::level::info, "  +-----------------------------------------+-----------------------------------------+");
		reshade::log::message(reshade::log::level::info, "  | flags                                   |"                               " %-#39x |", static_cast<unsigned int>(create_info.flags));
		reshade::log::message(reshade::log::level::info, "  | surface                                 |"                                " %-39p |", create_info.surface);
		reshade::log::message(reshade::log::level::info, "  | minImageCount                           |"                                " %-39u |", create_info.minImageCount);
		if (format_string != nullptr)
		reshade::log::message(reshade::log::level::info, "  | imageFormat                             |"                                " %-39s |", format_string);
		else
		reshade::log::message(reshade::log::level::info, "  | imageFormat                             |"                                " %-39d |", static_cast<int>(create_info.imageFormat));
		if (color_space_string != nullptr)
		reshade::log::message(reshade::log::level::info, "  | imageColorSpace                         |"                                " %-39s |", color_space_string);
		else
		reshade::log::message(reshade::log::level::info, "  | imageColorSpace                         |"                                " %-39d |", static_cast<int>(create_info.imageColorSpace));
		reshade::log::message(reshade::log::level::info, "  | imageExtent                             |"            " %-19u"            " %-19u |", create_info.imageExtent.width, create_info.imageExtent.height);
		reshade::log::message(reshade::log::level::info, "  | imageArrayLayers                        |"                                " %-39u |", create_info.imageArrayLayers);
		reshade::log::message(reshade::log::level::info, "  | imageUsage                              |"                               " %-#39x |", static_cast<unsigned int>(create_info.imageUsage));
		reshade::log::message(reshade::log::level::info, "  | imageSharingMode                        |"                                " %-39d |", static_cast<int>(create_info.imageSharingMode));
		reshade::log::message(reshade::log::level::info, "  | queueFamilyIndexCount                   |"                                " %-39u |", create_info.queueFamilyIndexCount);
		reshade::log::message(reshade::log::level::info, "  | preTransform                            |"                               " %-#39x |", static_cast<unsigned int>(create_info.preTransform));
		reshade::log::message(reshade::log::level::info, "  | compositeAlpha                          |"                               " %-#39x |", static_cast<unsigned int>(create_info.compositeAlpha));
		reshade::log::message(reshade::log::level::info, "  | presentMode                             |"                                " %-39d |", static_cast<int>(create_info.presentMode));
		reshade::log::message(reshade::log::level::info, "  | clipped                                 |"                                " %-39s |", create_info.clipped ? "true" : "false");
		reshade::log::message(reshade::log::level::info, "  | oldSwapchain                            |"                                " %-39p |", create_info.oldSwapchain);
		reshade::log::message(reshade::log::level::info, "  +-----------------------------------------+-----------------------------------------+");
	}

	// Look up window handle from surface
	const HWND hwnd = g_vulkan_surfaces.at(create_info.surface);

#if RESHADE_ADDON
	reshade::api::swapchain_desc desc = {};
	desc.back_buffer.type = reshade::api::resource_type::texture_2d;
	desc.back_buffer.texture.width = create_info.imageExtent.width;
	desc.back_buffer.texture.height = create_info.imageExtent.height;
	assert(create_info.imageArrayLayers <= std::numeric_limits<uint16_t>::max());
	desc.back_buffer.texture.depth_or_layers = static_cast<uint16_t>(create_info.imageArrayLayers);
	desc.back_buffer.texture.levels = 1;
	desc.back_buffer.texture.format = reshade::vulkan::convert_format(create_info.imageFormat);
	desc.back_buffer.texture.samples = 1;
	desc.back_buffer.heap = reshade::api::memory_heap::gpu_only;
	reshade::vulkan::convert_image_usage_flags_to_usage(create_info.imageUsage, desc.back_buffer.usage);

	desc.back_buffer_count = create_info.minImageCount;
	desc.present_mode = static_cast<uint32_t>(create_info.presentMode);
	desc.present_flags = create_info.flags;
	desc.sync_interval = create_info.presentMode == VK_PRESENT_MODE_IMMEDIATE_KHR ? 0 : UINT32_MAX;

	// Optionally change fullscreen state
	VkSurfaceFullScreenExclusiveInfoEXT fullscreen_info;
	if (const auto existing_fullscreen_info = find_in_structure_chain<VkSurfaceFullScreenExclusiveInfoEXT>(
			pCreateInfo->pNext, VK_STRUCTURE_TYPE_SURFACE_FULL_SCREEN_EXCLUSIVE_INFO_EXT))
	{
		fullscreen_info = *existing_fullscreen_info;

		desc.fullscreen_state = existing_fullscreen_info->fullScreenExclusive == VK_FULL_SCREEN_EXCLUSIVE_ALLOWED_EXT;
	}
	else
	{
		fullscreen_info = { VK_STRUCTURE_TYPE_SURFACE_FULL_SCREEN_EXCLUSIVE_INFO_EXT, const_cast<void *>(create_info.pNext) };
		fullscreen_info.fullScreenExclusive = VK_FULL_SCREEN_EXCLUSIVE_DEFAULT_EXT;
	}

	if (reshade::invoke_addon_event<reshade::addon_event::create_swapchain>(reshade::api::device_api::vulkan, desc, hwnd))
	{
		create_info.imageFormat = reshade::vulkan::convert_format(desc.back_buffer.texture.format);
		create_info.imageExtent.width = desc.back_buffer.texture.width;
		create_info.imageExtent.height = desc.back_buffer.texture.height;
		create_info.imageArrayLayers = desc.back_buffer.texture.depth_or_layers;
		reshade::vulkan::convert_usage_to_image_usage_flags(desc.back_buffer.usage, create_info.imageUsage);

		create_info.minImageCount = desc.back_buffer_count;
		create_info.presentMode = static_cast<VkPresentModeKHR>(desc.present_mode);
		create_info.flags = static_cast<uint32_t>(desc.present_flags);

		if (desc.fullscreen_state)
		{
			if (fullscreen_info.fullScreenExclusive != VK_FULL_SCREEN_EXCLUSIVE_APPLICATION_CONTROLLED_EXT)
			{
				fullscreen_info.fullScreenExclusive = VK_FULL_SCREEN_EXCLUSIVE_ALLOWED_EXT;

				create_info.pNext = &fullscreen_info;
			}
		}
		else
		{
			if (fullscreen_info.fullScreenExclusive == VK_FULL_SCREEN_EXCLUSIVE_ALLOWED_EXT)
			{
				fullscreen_info.fullScreenExclusive = VK_FULL_SCREEN_EXCLUSIVE_APPLICATION_CONTROLLED_EXT;

				create_info.pNext = &fullscreen_info;
			}
		}

		if (desc.sync_interval == 0)
			create_info.presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
	}
#endif

	// Unregister object from old swap chain so that a call to 'vkDestroySwapchainKHR' won't reset the effect runtime again
	reshade::vulkan::object_data<VK_OBJECT_TYPE_SWAPCHAIN_KHR> *swapchain_impl = nullptr;
	if (create_info.oldSwapchain != VK_NULL_HANDLE)
		swapchain_impl = device_impl->get_private_data_for_object<VK_OBJECT_TYPE_SWAPCHAIN_KHR>(create_info.oldSwapchain);

	if (nullptr != swapchain_impl)
	{
		// Reuse the existing effect runtime if this swap chain was not created from scratch, but reset it before initializing again below
		reshade::reset_effect_runtime(swapchain_impl);

		// Get back buffer images of old swap chain
		uint32_t num_images = 0;
		device_impl->_dispatch_table.GetSwapchainImagesKHR(device, swapchain_impl->_orig, &num_images, nullptr);
		temp_mem<VkImage, 3> swapchain_images(num_images);
		device_impl->_dispatch_table.GetSwapchainImagesKHR(device, swapchain_impl->_orig, &num_images, swapchain_images.p);

#if RESHADE_ADDON
		reshade::invoke_addon_event<reshade::addon_event::destroy_swapchain>(swapchain_impl, false);
#endif

		for (uint32_t i = 0; i < num_images; ++i)
		{
#if RESHADE_ADDON
			destroy_default_view(device_impl, swapchain_images[i]);
#endif

			device_impl->unregister_object<VK_OBJECT_TYPE_IMAGE>(swapchain_images[i]);
		}

		device_impl->unregister_object<VK_OBJECT_TYPE_SWAPCHAIN_KHR, false>(swapchain_impl->_orig);
	}

	assert(!g_in_dxgi_runtime);
	g_in_dxgi_runtime = true;
	const VkResult result = trampoline(device, &create_info, pAllocator, pSwapchain);
	g_in_dxgi_runtime = false;
	if (result < VK_SUCCESS)
	{
		reshade::log::message(reshade::log::level::warning, "vkCreateSwapchainKHR failed with error code %d.", static_cast<int>(result));
		return result;
	}

	if (nullptr == swapchain_impl)
	{
		swapchain_impl = new reshade::vulkan::object_data<VK_OBJECT_TYPE_SWAPCHAIN_KHR>(device_impl, *pSwapchain, create_info, hwnd);

		reshade::create_effect_runtime(swapchain_impl, device_impl->_primary_graphics_queue);
	}
	else
	{
		swapchain_impl->_orig = *pSwapchain;
		swapchain_impl->_create_info = create_info;
		swapchain_impl->_create_info.pNext = nullptr; // Clear out structure chain pointer, since it becomes invalid once leaving the current scope
		swapchain_impl->_hwnd = hwnd;
	}

	device_impl->register_object<VK_OBJECT_TYPE_SWAPCHAIN_KHR>(swapchain_impl->_orig, swapchain_impl);

	// Get back buffer images of new swap chain
	uint32_t num_images = 0;
	device_impl->_dispatch_table.GetSwapchainImagesKHR(device, swapchain_impl->_orig, &num_images, nullptr);
	temp_mem<VkImage, 3> swapchain_images(num_images);
	device_impl->_dispatch_table.GetSwapchainImagesKHR(device, swapchain_impl->_orig, &num_images, swapchain_images.p);

	// Add swap chain images to the image list
	for (uint32_t i = 0; i < num_images; ++i)
	{
		reshade::vulkan::object_data<VK_OBJECT_TYPE_IMAGE> &image_data = *device_impl->register_object<VK_OBJECT_TYPE_IMAGE>(swapchain_images[i]);
		image_data.create_info = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
		image_data.create_info.imageType = VK_IMAGE_TYPE_2D;
		image_data.create_info.format = create_info.imageFormat;
		image_data.create_info.extent = { create_info.imageExtent.width, create_info.imageExtent.height, 1 };
		image_data.create_info.mipLevels = 1;
		image_data.create_info.arrayLayers = create_info.imageArrayLayers;
		image_data.create_info.samples = VK_SAMPLE_COUNT_1_BIT;
		image_data.create_info.usage = create_info.imageUsage;
		image_data.create_info.sharingMode = create_info.imageSharingMode;
		image_data.create_info.initialLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

		// See https://registry.khronos.org/vulkan/specs/latest/man/html/vkCreateSwapchainKHR.html#_description
		if ((create_info.flags & VK_SWAPCHAIN_CREATE_SPLIT_INSTANCE_BIND_REGIONS_BIT_KHR) != 0)
			image_data.create_info.flags |= VK_IMAGE_CREATE_SPLIT_INSTANCE_BIND_REGIONS_BIT;
		if ((create_info.flags & VK_SWAPCHAIN_CREATE_PROTECTED_BIT_KHR) != 0)
			image_data.create_info.flags |= VK_IMAGE_CREATE_PROTECTED_BIT;
		if ((create_info.flags & VK_SWAPCHAIN_CREATE_MUTABLE_FORMAT_BIT_KHR) != 0)
			image_data.create_info.flags |= VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT | VK_IMAGE_CREATE_EXTENDED_USAGE_BIT;
	}

#if RESHADE_ADDON
	reshade::invoke_addon_event<reshade::addon_event::init_swapchain>(swapchain_impl, false);

	// Create default views for swap chain images (do this after the 'init_swapchain' event, so that the images are known to add-ons)
	for (uint32_t i = 0; i < num_images; ++i)
		create_default_view(device_impl, swapchain_images[i]);

	if (fullscreen_info.fullScreenExclusive != VK_FULL_SCREEN_EXCLUSIVE_DEFAULT_EXT)
	{
		if (const auto fullscreen_win32_info = find_in_structure_chain<VkSurfaceFullScreenExclusiveWin32InfoEXT>(
				create_info.pNext, VK_STRUCTURE_TYPE_SURFACE_FULL_SCREEN_EXCLUSIVE_WIN32_INFO_EXT))
			swapchain_impl->hmonitor = fullscreen_win32_info->hmonitor;

		reshade::invoke_addon_event<reshade::addon_event::set_fullscreen_state>(swapchain_impl, fullscreen_info.fullScreenExclusive == VK_FULL_SCREEN_EXCLUSIVE_ALLOWED_EXT, swapchain_impl->hmonitor);
	}
#endif

	reshade::init_effect_runtime(swapchain_impl);

#if RESHADE_VERBOSE_LOG
	reshade::log::message(reshade::log::level::debug, "Returning Vulkan swap chain %p.", *pSwapchain);
#endif
	return result;
}
void     VKAPI_CALL vkDestroySwapchainKHR(VkDevice device, VkSwapchainKHR swapchain, const VkAllocationCallbacks *pAllocator)
{
	reshade::log::message(reshade::log::level::info, "Redirecting vkDestroySwapchainKHR(device = %p, swapchain = %p, pAllocator = %p) ...", device, swapchain, pAllocator);

	if (swapchain == VK_NULL_HANDLE)
		return;

	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(device));
	RESHADE_VULKAN_GET_DEVICE_DISPATCH_PTR(DestroySwapchainKHR, device_impl);

	// Remove swap chain from global list
	reshade::vulkan::object_data<VK_OBJECT_TYPE_SWAPCHAIN_KHR> *const swapchain_impl = device_impl->get_private_data_for_object<VK_OBJECT_TYPE_SWAPCHAIN_KHR, true>(swapchain);
	if (swapchain_impl != nullptr)
	{
		reshade::reset_effect_runtime(swapchain_impl);

		// Get back buffer images of old swap chain
		uint32_t num_images = 0;
		device_impl->_dispatch_table.GetSwapchainImagesKHR(device, swapchain, &num_images, nullptr);
		temp_mem<VkImage, 3> swapchain_images(num_images);
		device_impl->_dispatch_table.GetSwapchainImagesKHR(device, swapchain, &num_images, swapchain_images.p);

#if RESHADE_ADDON
		reshade::invoke_addon_event<reshade::addon_event::destroy_swapchain>(swapchain_impl, false);
#endif

		for (uint32_t i = 0; i < num_images; ++i)
		{
#if RESHADE_ADDON
			destroy_default_view(device_impl, swapchain_images[i]);
#endif

			device_impl->unregister_object<VK_OBJECT_TYPE_IMAGE>(swapchain_images[i]);
		}

		reshade::destroy_effect_runtime(swapchain_impl);
	}

	device_impl->unregister_object<VK_OBJECT_TYPE_SWAPCHAIN_KHR, false>(swapchain);

	delete swapchain_impl;

	trampoline(device, swapchain, pAllocator);
}

VkResult VKAPI_CALL vkAcquireNextImageKHR(VkDevice device, VkSwapchainKHR swapchain, uint64_t timeout, VkSemaphore semaphore, VkFence fence, uint32_t *pImageIndex)
{
	assert(pImageIndex != nullptr);

	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(device));
	RESHADE_VULKAN_GET_DEVICE_DISPATCH_PTR(AcquireNextImageKHR, device_impl);

	const VkResult result = trampoline(device, swapchain, timeout, semaphore, fence, pImageIndex);
	if (result == VK_SUCCESS)
	{
		if (reshade::vulkan::object_data<VK_OBJECT_TYPE_SWAPCHAIN_KHR> *const swapchain_impl = device_impl->get_private_data_for_object<VK_OBJECT_TYPE_SWAPCHAIN_KHR, true>(swapchain))
			swapchain_impl->_swap_index = *pImageIndex;
	}
#if RESHADE_VERBOSE_LOG
	else if (result < VK_SUCCESS)
	{
		reshade::log::message(reshade::log::level::warning, "vkAcquireNextImageKHR failed with error code %d.", static_cast<int>(result));
	}
#endif

	return result;
}
VkResult VKAPI_CALL vkAcquireNextImage2KHR(VkDevice device, const VkAcquireNextImageInfoKHR *pAcquireInfo, uint32_t *pImageIndex)
{
	assert(pAcquireInfo != nullptr && pImageIndex != nullptr);

	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(device));
	RESHADE_VULKAN_GET_DEVICE_DISPATCH_PTR(AcquireNextImage2KHR, device_impl);

	const VkResult result = trampoline(device, pAcquireInfo, pImageIndex);
	if (result == VK_SUCCESS)
	{
		if (reshade::vulkan::object_data<VK_OBJECT_TYPE_SWAPCHAIN_KHR> *const swapchain_impl = device_impl->get_private_data_for_object<VK_OBJECT_TYPE_SWAPCHAIN_KHR, true>(pAcquireInfo->swapchain))
			swapchain_impl->_swap_index = *pImageIndex;
	}
#if RESHADE_VERBOSE_LOG
	else if (result < VK_SUCCESS)
	{
		reshade::log::message(reshade::log::level::warning, "vkAcquireNextImage2KHR failed with error code %d.", static_cast<int>(result));
	}
#endif

	return result;
}

VkResult VKAPI_CALL vkQueuePresentKHR(VkQueue queue, const VkPresentInfoKHR *pPresentInfo)
{
	assert(pPresentInfo != nullptr);

	VkPresentInfoKHR present_info = *pPresentInfo;

	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(queue));
	reshade::vulkan::command_queue_impl *const queue_impl = device_impl->get_private_data_for_object<VK_OBJECT_TYPE_QUEUE>(queue);

	const bool present_from_secondary_queue = device_impl->_primary_graphics_queue != nullptr && device_impl->_primary_graphics_queue != queue_impl;
	if (present_from_secondary_queue)
		std::lock(queue_impl->_mutex, device_impl->_primary_graphics_queue->_mutex);
	else
		queue_impl->_mutex.lock();

	for (uint32_t i = 0; i < pPresentInfo->swapchainCount; ++i)
	{
		reshade::vulkan::swapchain_impl *const swapchain_impl = device_impl->get_private_data_for_object<VK_OBJECT_TYPE_SWAPCHAIN_KHR>(pPresentInfo->pSwapchains[i]);

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

			const VkRectLayerKHR *const rects = present_regions->pRegions[i].pRectangles;

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

		reshade::present_effect_runtime(swapchain_impl);
	}

	// Synchronize immediate command list flush
	{
		temp_mem<VkPipelineStageFlags> wait_stages(present_info.waitSemaphoreCount);
		std::fill_n(wait_stages.p, present_info.waitSemaphoreCount, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

		VkSubmitInfo submit_info { VK_STRUCTURE_TYPE_SUBMIT_INFO };
		submit_info.waitSemaphoreCount = present_info.waitSemaphoreCount;
		submit_info.pWaitSemaphores = present_info.pWaitSemaphores;
		submit_info.pWaitDstStageMask = wait_stages.p;

		queue_impl->flush_immediate_command_list(submit_info);

		// If the application is presenting with a different queue than rendering, synchronize these two queues
		if (present_from_secondary_queue)
		{
			queue_impl->wait_and_signal(submit_info);

			device_impl->_primary_graphics_queue->flush_immediate_command_list(submit_info);
		}

		// Override wait semaphores based on the last queue submit
		present_info.waitSemaphoreCount = submit_info.waitSemaphoreCount;
		present_info.pWaitSemaphores = submit_info.pWaitSemaphores;
	}

	device_impl->advance_transient_descriptor_pool();

	RESHADE_VULKAN_GET_DEVICE_DISPATCH_PTR(QueuePresentKHR, device_impl);
	assert(!g_in_dxgi_runtime);
	g_in_dxgi_runtime = true;
	const VkResult result = trampoline(queue, &present_info);
	g_in_dxgi_runtime = false;

#if RESHADE_ADDON
	if (result >= VK_SUCCESS && reshade::has_addon_event<reshade::addon_event::finish_present>())
	{
		for (uint32_t i = 0; i < pPresentInfo->swapchainCount; ++i)
		{
			reshade::vulkan::swapchain_impl *const swapchain_impl = device_impl->get_private_data_for_object<VK_OBJECT_TYPE_SWAPCHAIN_KHR>(pPresentInfo->pSwapchains[i]);

			reshade::invoke_addon_event<reshade::addon_event::finish_present>(queue_impl, swapchain_impl);
		}
	}
#endif

	if (present_from_secondary_queue)
		device_impl->_primary_graphics_queue->_mutex.unlock();
	queue_impl->_mutex.unlock();

	return result;
}

VkResult VKAPI_CALL vkAcquireFullScreenExclusiveModeEXT(VkDevice device, VkSwapchainKHR swapchain)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(device));
	RESHADE_VULKAN_GET_DEVICE_DISPATCH_PTR(AcquireFullScreenExclusiveModeEXT, device_impl);

#if RESHADE_ADDON
	if (reshade::vulkan::object_data<VK_OBJECT_TYPE_SWAPCHAIN_KHR> *const swapchain_impl = device_impl->get_private_data_for_object<VK_OBJECT_TYPE_SWAPCHAIN_KHR, true>(swapchain))
		if (reshade::invoke_addon_event<reshade::addon_event::set_fullscreen_state>(swapchain_impl, true, swapchain_impl->hmonitor))
			return VK_SUCCESS;
#endif

	return trampoline(device, swapchain);
}
VkResult VKAPI_CALL vkReleaseFullScreenExclusiveModeEXT(VkDevice device, VkSwapchainKHR swapchain)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(device));
	RESHADE_VULKAN_GET_DEVICE_DISPATCH_PTR(ReleaseFullScreenExclusiveModeEXT, device_impl);

#if RESHADE_ADDON
	if (reshade::vulkan::object_data<VK_OBJECT_TYPE_SWAPCHAIN_KHR> *const swapchain_impl = device_impl->get_private_data_for_object<VK_OBJECT_TYPE_SWAPCHAIN_KHR, true>(swapchain))
		if (reshade::invoke_addon_event<reshade::addon_event::set_fullscreen_state>(swapchain_impl, false, swapchain_impl->hmonitor))
			return VK_SUCCESS;
#endif

	return trampoline(device, swapchain);
}
