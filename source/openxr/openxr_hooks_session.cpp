/*
 * Copyright (C) 2023 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause OR MIT
 */

#include "openxr_hooks.hpp"
#include "openxr_impl_swapchain.hpp"
#include "vulkan/vulkan_hooks.hpp"
#include "vulkan/vulkan_impl_device.hpp"
#include "vulkan/vulkan_impl_command_queue.hpp"
#include "dll_log.hpp"
#include "hook_manager.hpp"
#include "lockfree_linear_map.hpp"
#include <deque>

#define XR_USE_GRAPHICS_API_VULKAN
#include <openxr/openxr_platform.h>

struct session_data
{
	XrInstance instance = XR_NULL_HANDLE;
	reshade::openxr::swapchain_impl *swapchain_impl = nullptr;
};
struct swapchain_data
{
	XrInstance instance = XR_NULL_HANDLE;
	std::vector<XrSwapchainImageVulkanKHR> surface_images;
	std::deque<uint32_t> acquired_index;
	uint32_t last_released_index = 0;
};

extern lockfree_linear_map<XrInstance, openxr_dispatch_table, 16> g_openxr_instances;
static lockfree_linear_map<XrSession, session_data, 16> s_openxr_sessions;
static lockfree_linear_map<XrSwapchain, swapchain_data, 16> s_openxr_swapchains;

#define GET_DISPATCH_PTR(name) \
	GET_DISPATCH_PTR_FROM(name, instance)
#define GET_DISPATCH_PTR_FROM(name, instance) \
	assert((instance) != XR_NULL_HANDLE); \
	PFN_xr##name trampoline = g_openxr_instances.at(instance).name; \
	assert(trampoline != nullptr)

XrResult XRAPI_CALL xrCreateSession(XrInstance instance, const XrSessionCreateInfo *pCreateInfo, XrSession *pSession)
{
	LOG(INFO) << "Redirecting " << "xrCreateSession" << '(' << "instance = " << instance << ", pCreateInfo = " << pCreateInfo << ", pSession = " << pSession << ')' << " ...";

	assert(pCreateInfo != nullptr && pSession != nullptr);

	GET_DISPATCH_PTR(CreateSession);

	const XrResult result = trampoline(instance, pCreateInfo, pSession);
	if (XR_FAILED(result))
	{
		LOG(WARN) << "xrCreateSession" << " failed with error code " << result << '.';
		return result;
	}

	reshade::openxr::swapchain_impl *swapchain_impl = nullptr;

	if (const auto binding_vulkan = find_in_structure_chain<XrGraphicsBindingVulkanKHR>(pCreateInfo, XR_TYPE_GRAPHICS_BINDING_VULKAN_KHR))
	{
		extern lockfree_linear_map<void *, reshade::vulkan::device_impl *, 8> g_vulkan_devices;
		reshade::vulkan::device_impl *const device = g_vulkan_devices.at(dispatch_key_from_handle(binding_vulkan->device));

		if (device != nullptr)
		{
			VkQueue queue_handle = VK_NULL_HANDLE;
			device->_dispatch_table.GetDeviceQueue(binding_vulkan->device, binding_vulkan->queueFamilyIndex, binding_vulkan->queueIndex, &queue_handle);

			reshade::vulkan::command_queue_impl *queue = nullptr;
			if (const auto queue_it = std::find_if(device->_queues.cbegin(), device->_queues.cend(),
					[queue_handle](reshade::vulkan::command_queue_impl *queue) { return queue->_orig == queue_handle; });
				queue_it != device->_queues.cend())
				queue = *queue_it;

			if (queue != nullptr)
			{
				swapchain_impl = new reshade::openxr::swapchain_impl(device, queue, *pSession);
			}
		}
		else
		{
			LOG(WARN) << "Skipping OpenXR session because it was created without a known Vulkan device.";
		}
	}

	s_openxr_sessions.emplace(*pSession, session_data { instance, swapchain_impl });

#if RESHADE_VERBOSE_LOG
	LOG(DEBUG) << "Returning OpenXR session " << *pSession << '.';
#endif
	return result;
}

XrResult XRAPI_CALL xrDestroySession(XrSession session)
{
	LOG(INFO) << "Redirecting " << "xrDestroySession" << '(' << "session = " << session << ')' << " ...";

	assert(session != XR_NULL_HANDLE);

	const session_data &data = s_openxr_sessions.at(session);
	GET_DISPATCH_PTR_FROM(DestroySession, data.instance);

	delete data.swapchain_impl;

	s_openxr_sessions.erase(session);

	return trampoline(session);
}

XrResult XRAPI_CALL xrCreateSwapchain(XrSession session, const XrSwapchainCreateInfo *pCreateInfo, XrSwapchain *pSwapchain)
{
	LOG(INFO) << "Redirecting " << "xrCreateSwapchain" << '(' << "session = " << session << ", pCreateInfo = " << pCreateInfo << ", pSwapchain = " << pSwapchain << ')' << " ...";

	const session_data &data = s_openxr_sessions.at(session);
	GET_DISPATCH_PTR_FROM(CreateSwapchain, data.instance);

	assert(pCreateInfo != nullptr && pSwapchain != nullptr);

	XrSwapchainCreateInfo create_info = *pCreateInfo;
	// Add required usage flags to create info
	create_info.usageFlags |= XR_SWAPCHAIN_USAGE_TRANSFER_SRC_BIT;

	const XrResult result = trampoline(session, &create_info, pSwapchain);
	if (XR_FAILED(result))
	{
		LOG(WARN) << "xrCreateSwapchain" << " failed with error code " << result << '.';
		return result;
	}

	auto enum_swapchain_images = g_openxr_instances.at(data.instance).EnumerateSwapchainImages;
	assert(enum_swapchain_images != nullptr);

	uint32_t num_images = 0;
	enum_swapchain_images(*pSwapchain, num_images, &num_images, nullptr);
	std::vector<XrSwapchainImageVulkanKHR> images(num_images, { XR_TYPE_SWAPCHAIN_IMAGE_VULKAN_KHR });
	enum_swapchain_images(*pSwapchain, num_images, &num_images, reinterpret_cast<XrSwapchainImageBaseHeader *>(images.data()));

	s_openxr_swapchains.emplace(*pSwapchain, swapchain_data { data.instance, std::move(images) });

#if RESHADE_VERBOSE_LOG
	LOG(DEBUG) << "Returning OpenXR swap chain " << *pSwapchain << '.';
#endif
	return result;
}

XrResult XRAPI_CALL xrDestroySwapchain(XrSwapchain swapchain)
{
	LOG(INFO) << "Redirecting " << "xrDestroySwapchain" << '(' << "swapchain = " << swapchain << ')' << " ...";

	const swapchain_data &data = s_openxr_swapchains.at(swapchain);
	GET_DISPATCH_PTR_FROM(DestroySwapchain, data.instance);

	s_openxr_swapchains.erase(swapchain);

	return trampoline(swapchain);
}

XrResult XRAPI_CALL xrAcquireSwapchainImage(XrSwapchain swapchain, const XrSwapchainImageAcquireInfo *pAcquireInfo, uint32_t *pIndex)
{
	swapchain_data &data = s_openxr_swapchains.at(swapchain);
	GET_DISPATCH_PTR_FROM(AcquireSwapchainImage, data.instance);

	const XrResult result = trampoline(swapchain, pAcquireInfo, pIndex);
	if (XR_FAILED(result))
		return result;

	data.acquired_index.push_back(*pIndex);

	return result;
}

XrResult XRAPI_CALL xrReleaseSwapchainImage(XrSwapchain swapchain, const XrSwapchainImageReleaseInfo *pReleaseInfo)
{
	swapchain_data &data = s_openxr_swapchains.at(swapchain);
	GET_DISPATCH_PTR_FROM(ReleaseSwapchainImage, data.instance);

	const XrResult result = trampoline(swapchain, pReleaseInfo);
	if (XR_FAILED(result))
		return result;

	data.last_released_index = data.acquired_index.front();
	data.acquired_index.pop_front();

	return result;
}

XrResult XRAPI_CALL xrEndFrame(XrSession session, const XrFrameEndInfo *frameEndInfo)
{
	reshade::api::resource left_texture = {};
	reshade::api::rect left_rect;
	uint32_t left_layer = 0;
	reshade::api::resource right_texture = {};
	reshade::api::rect right_rect;
	uint32_t right_layer = 0;

	for (uint32_t i = 0; i < frameEndInfo->layerCount; ++i)
	{
		if (frameEndInfo->layers[i]->type == XR_TYPE_COMPOSITION_LAYER_PROJECTION)
		{
			const XrCompositionLayerProjection *const layer = reinterpret_cast<const XrCompositionLayerProjection *>(frameEndInfo->layers[i]);

			XrSwapchainSubImage const &left_sub_image = layer->views[0].subImage;

			swapchain_data const &left_data = s_openxr_swapchains.at(left_sub_image.swapchain);
			left_texture.handle = (uint64_t)left_data.surface_images[left_data.last_released_index].image;
			left_rect.left = left_sub_image.imageRect.offset.x;
			left_rect.top = left_sub_image.imageRect.offset.y;
			left_rect.right = left_sub_image.imageRect.offset.x + left_sub_image.imageRect.extent.width;
			left_rect.bottom = left_sub_image.imageRect.offset.y + left_sub_image.imageRect.extent.height;
			left_layer = left_sub_image.imageArrayIndex;

			XrSwapchainSubImage const &right_sub_image = layer->views[1].subImage;

			swapchain_data const &right_data = s_openxr_swapchains.at(right_sub_image.swapchain);
			right_texture.handle = (uint64_t)right_data.surface_images[right_data.last_released_index].image;
			right_rect.left = right_sub_image.imageRect.offset.x;
			right_rect.top = right_sub_image.imageRect.offset.y;
			right_rect.right = right_sub_image.imageRect.offset.x + right_sub_image.imageRect.extent.width;
			right_rect.bottom = right_sub_image.imageRect.offset.y + right_sub_image.imageRect.extent.height;
			right_layer = right_sub_image.imageArrayIndex;
			break;
		}
	}

	const session_data &data = s_openxr_sessions.at(session);

	if (left_texture != 0 && right_texture != 0)
		data.swapchain_impl->on_present(left_texture, left_rect, left_layer, right_texture, right_rect, right_layer);

	GET_DISPATCH_PTR_FROM(EndFrame, data.instance);
	return trampoline(session, frameEndInfo);
}
