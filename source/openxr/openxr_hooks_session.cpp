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

struct session_data : public reshade::openxr::swapchain_impl
{
	session_data(reshade::api::device *device, reshade::api::command_queue *graphics_queue, XrSession session) : swapchain_impl(device, graphics_queue, session) {}

	XrInstance instance = XR_NULL_HANDLE;
};
struct swapchain_data
{
	XrInstance instance;
	// TODO_OXR: why deque?
	std::deque<uint32_t> acquired_index;
	uint32_t last_released_index = 0;
	std::vector<XrSwapchainImageVulkanKHR> surface_images;
};

static lockfree_linear_map<XrSession, session_data *, 16> s_openxr_sessions;
static lockfree_linear_map<XrSwapchain, swapchain_data, 16> s_openxr_swapchains;
extern lockfree_linear_map<XrInstance, openxr_dispatch_table, 16> g_openxr_instances;

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

	const XrSession session = *pSession;

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
				const auto swapchain_impl = new session_data(device, queue, session);
				swapchain_impl->instance = instance;

				s_openxr_sessions.emplace(session, swapchain_impl);
			}
		}
		else
		{
			LOG(WARN) << "Skipping swapchain because it it not created with a known Vulkan device.";
		}
	}

#if RESHADE_VERBOSE_LOG
	LOG(DEBUG) << "Returning OpenXR session " << session << '.';
#endif
	return result;
}

XrResult XRAPI_CALL xrDestroySession(XrSession session)
{
	LOG(INFO) << "Redirecting " << "xrDestroySession" << '(' << "session = " << session << ')' << " ...";

	assert(session != XR_NULL_HANDLE);

	session_data *const swapchain_impl = s_openxr_sessions.erase(session);
	GET_DISPATCH_PTR_FROM(DestroySession, swapchain_impl->instance);

	delete swapchain_impl;

	return trampoline(session);
}

XrResult XRAPI_CALL xrCreateSwapchain(XrSession session, const XrSwapchainCreateInfo *pCreateInfo, XrSwapchain *pSwapchain)
{
	LOG(INFO) << "Redirecting " << "xrCreateSwapchain" << '(' << "session = " << session << ", pCreateInfo = " << pCreateInfo << ", pSwapchain = " << pSwapchain << ')' << " ...";

	XrSwapchainCreateInfo create_info = *pCreateInfo;
	create_info.usageFlags |= XR_SWAPCHAIN_USAGE_TRANSFER_SRC_BIT;

	session_data *const swapchain_impl = s_openxr_sessions.at(session);
	openxr_dispatch_table &dispatch_table = g_openxr_instances.at(swapchain_impl->instance);

	GET_DISPATCH_PTR_FROM(CreateSwapchain, swapchain_impl->instance);

	const XrResult result = trampoline(session, &create_info, pSwapchain);
	if (XR_FAILED(result))
	{
		LOG(WARN) << "xrCreateSwapchain" << " failed with error code " << result << '.';
		return result;
	}

	auto enum_swapchain_images = dispatch_table.EnumerateSwapchainImages;
	assert(enum_swapchain_images != nullptr);

	uint32_t num_images = 0;
	enum_swapchain_images(*pSwapchain, num_images, &num_images, nullptr);
	std::vector<XrSwapchainImageVulkanKHR> images(num_images, { XR_TYPE_SWAPCHAIN_IMAGE_VULKAN_KHR });
	enum_swapchain_images(*pSwapchain, num_images, &num_images, reinterpret_cast<XrSwapchainImageBaseHeader *>(images.data()));

	// Track our own information about the swapchain, so we can draw stuff onto it.
	swapchain_data data;
	data.instance = swapchain_impl->instance;
	data.surface_images = std::move(images);

	s_openxr_swapchains.emplace(*pSwapchain, std::move(data));

#if RESHADE_VERBOSE_LOG
	LOG(DEBUG) << "Returning OpenXR swap chain " << *pSwapchain << '.';
#endif
	return result;
}

XrResult XRAPI_CALL xrDestroySwapchain(XrSwapchain swapchain)
{
	LOG(INFO) << "Redirecting " << "xrDestroySwapchain" << '(' << "swapchain = " << swapchain << ')' << " ...";

	swapchain_data &data = s_openxr_swapchains.at(swapchain);

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

	session_data *const swapchain_impl = s_openxr_sessions.at(session);

	if (left_texture != 0 && right_texture != 0)
		swapchain_impl->on_present(left_texture, left_rect, left_layer, right_texture, right_rect, right_layer);

	GET_DISPATCH_PTR_FROM(EndFrame, swapchain_impl->instance);
	return trampoline(session, frameEndInfo);
}
