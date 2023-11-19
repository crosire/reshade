/*
 * OpenXR Vulkan wiring by The Iron Wolf.
 *
 * I tried creating a general solution using OXR API Layer (xr-vk-layer branch), but no matter what I do game would
 * crash on call to ::xrCreateSession. So instead use good old hooks.
 *
 * Special thanks to creators of ReShade, OpenVR wiring and Matthieu Bucchianeri for guidance and samples:
 * https://github.com/mbucchia/_ARCHIVE_XR_APILAYER_NOVENDOR_fov_modifier
 * https://github.com/mbucchia/Quad-Views-Foveated
 *
 * Copyright (C) 2021 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

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

struct swapchain_data
{
	// TODO_OXR: why deque?
	std::deque<uint32_t> acquired_index;
	uint32_t last_released_index = 0;
	std::vector<XrSwapchainImageVulkanKHR> surface_images;
};

static lockfree_linear_map<XrSession, reshade::openxr::swapchain_impl *, 16> s_openxr_sessions;
static lockfree_linear_map<XrSwapchain, swapchain_data, 16> s_openxr_swapchains;

template <typename T>
static const T *find_in_structure_chain(const void *structure_chain, XrStructureType type)
{
	const T *next = reinterpret_cast<const T *>(structure_chain);
	while (next != nullptr && next->type != type)
		next = reinterpret_cast<const T *>(next->next);
	return next;
}

XrResult XRAPI_CALL xrCreateSession(XrInstance instance, const XrSessionCreateInfo *createInfo, XrSession *session)
{
	static const auto trampoline = reshade::hooks::call(xrCreateSession);

	const XrResult result = trampoline(instance, createInfo, session);
	if (XR_SUCCEEDED(result))
	{
		if (const auto binding_vulkan = find_in_structure_chain<XrGraphicsBindingVulkanKHR>(createInfo, XR_TYPE_GRAPHICS_BINDING_VULKAN_KHR))
		{
			extern lockfree_linear_map<void *, reshade::vulkan::device_impl *, 8> g_vulkan_devices;
			reshade::vulkan::device_impl *const device = g_vulkan_devices.at(dispatch_key_from_handle(binding_vulkan->device));

			VkQueue queue_handle = VK_NULL_HANDLE;
			device->_dispatch_table.GetDeviceQueue(binding_vulkan->device, binding_vulkan->queueFamilyIndex, binding_vulkan->queueIndex, &queue_handle);

			reshade::vulkan::command_queue_impl *queue = nullptr;
			if (const auto queue_it = std::find_if(device->_queues.cbegin(), device->_queues.cend(),
					[queue_handle](reshade::vulkan::command_queue_impl *queue) { return queue->_orig == queue_handle; });
				queue_it != device->_queues.cend())
				queue = *queue_it;

			if (queue != nullptr)
			{
				const auto swapchain_impl = new reshade::openxr::swapchain_impl(device, queue, *session);

				s_openxr_sessions.emplace(*session, swapchain_impl);
			}
		}
	}

	return result;
}

XrResult XRAPI_CALL xrDestroySession(XrSession session)
{
	reshade::openxr::swapchain_impl *const swapchain_impl = s_openxr_sessions.erase(session);

	delete swapchain_impl;

	static const auto trampoline = reshade::hooks::call(xrDestroySession);
	return trampoline(session);
}

XrResult XRAPI_CALL xrCreateSwapchain(XrSession session, const XrSwapchainCreateInfo *createInfo, XrSwapchain *swapchain)
{
	XrSwapchainCreateInfo create_info = *createInfo;
	create_info.usageFlags |= XR_SWAPCHAIN_USAGE_TRANSFER_SRC_BIT;

	static const auto trampoline = reshade::hooks::call(xrCreateSwapchain);

	const XrResult result = trampoline(session, &create_info, swapchain);
	if (XR_SUCCEEDED(result))
	{
#ifndef _WIN64
		const HMODULE module = GetModuleHandleW(L"openxr_loader.dll");
#else
		const HMODULE module = GetModuleHandleW(L"openxr_loader64.dll");
#endif
		assert(module != nullptr);
		auto enum_swapchain_images = reinterpret_cast<PFN_xrEnumerateSwapchainImages>(GetProcAddress(module, "xrEnumerateSwapchainImages"));
		assert(enum_swapchain_images != nullptr);

		uint32_t num_images = 0;
		enum_swapchain_images(*swapchain, num_images, &num_images, nullptr);
		std::vector<XrSwapchainImageVulkanKHR> images(num_images);
		enum_swapchain_images(*swapchain, num_images, &num_images, reinterpret_cast<XrSwapchainImageBaseHeader *>(images.data()));

		// Track our own information about the swapchain, so we can draw stuff onto it.
		swapchain_data data;
		data.surface_images = std::move(images);

		s_openxr_swapchains.emplace(*swapchain, std::move(data));
	}

	return result;
}

XrResult XRAPI_CALL xrDestroySwapchain(XrSwapchain swapchain)
{
	s_openxr_swapchains.erase(swapchain);

	static const auto trampoline = reshade::hooks::call(xrDestroySwapchain);
	return trampoline(swapchain);
}

XrResult XRAPI_CALL xrAcquireSwapchainImage(XrSwapchain swapchain, const XrSwapchainImageAcquireInfo *acquireInfo, uint32_t *index)
{
	static const auto trampoline = reshade::hooks::call(xrAcquireSwapchainImage);

	const XrResult result = trampoline(swapchain, acquireInfo, index);
	if (XR_SUCCEEDED(result))
	{
		swapchain_data &data = s_openxr_swapchains.at(swapchain);
		data.acquired_index.push_back(*index);
	}

	return result;
}

XrResult XRAPI_CALL xrReleaseSwapchainImage(XrSwapchain swapchain, const XrSwapchainImageReleaseInfo *releaseInfo)
{
	static const auto trampoline = reshade::hooks::call(xrReleaseSwapchainImage);

	const XrResult result = trampoline(swapchain, releaseInfo);
	if (XR_SUCCEEDED(result))
	{
		swapchain_data &data = s_openxr_swapchains.at(swapchain);
		data.last_released_index = data.acquired_index.front();
		data.acquired_index.pop_front();
	}

	return result;
}

XrResult XRAPI_CALL xrEndFrame(XrSession session, const XrFrameEndInfo *frameEndInfo)
{
	reshade::api::resource left_texture = {};
	reshade::api::resource right_texture = {};

	for (uint32_t i = 0; i < frameEndInfo->layerCount; ++i)
	{
		if (frameEndInfo->layers[i]->type == XR_TYPE_COMPOSITION_LAYER_PROJECTION)
		{
			const XrCompositionLayerProjection *const layer = reinterpret_cast<const XrCompositionLayerProjection *>(frameEndInfo->layers[i]);

			swapchain_data const &left_data = s_openxr_swapchains.at(layer->views[static_cast<int>(reshade::openxr::eye::left)].subImage.swapchain);
			left_texture.handle = (uint64_t)left_data.surface_images[left_data.last_released_index].image;

			swapchain_data const &right_data = s_openxr_swapchains.at(layer->views[static_cast<int>(reshade::openxr::eye::right)].subImage.swapchain);
			right_texture.handle = (uint64_t)right_data.surface_images[right_data.last_released_index].image;
			break;
		}
	}

	if (left_texture != 0 && right_texture != 0)
	{
		if (reshade::openxr::swapchain_impl *const swapchain_impl = s_openxr_sessions.at(session))
		{
			swapchain_impl->on_present(left_texture, right_texture);
		}
	}

	static const auto trampoline = reshade::hooks::call(xrEndFrame);
	return trampoline(session, frameEndInfo);
}

void check_and_init_openxr_hooks()
{
	static bool s_hooks_installed = false;
	if (s_hooks_installed)
		return;

#ifndef _WIN64
	const HMODULE module = GetModuleHandleW(L"openxr_loader.dll");
#else
	const HMODULE module = GetModuleHandleW(L"openxr_loader64.dll");
#endif
	if (module == nullptr)
		return;

	reshade::hooks::install("xrCreateSession", GetProcAddress(module, "xrCreateSession"), xrCreateSession);
	reshade::hooks::install("xrDestroySession", GetProcAddress(module, "xrDestroySession"), xrDestroySession);
	reshade::hooks::install("xrAcquireSwapchainImage", GetProcAddress(module, "xrAcquireSwapchainImage"), xrAcquireSwapchainImage);
	reshade::hooks::install("xrReleaseSwapchainImage", GetProcAddress(module, "xrReleaseSwapchainImage"), xrReleaseSwapchainImage);
	reshade::hooks::install("xrCreateSwapchain", GetProcAddress(module, "xrCreateSwapchain"), xrCreateSwapchain);
	reshade::hooks::install("xrDestroySwapchain", GetProcAddress(module, "xrDestroySwapchain"), xrDestroySwapchain);
	reshade::hooks::install("xrEndFrame", GetProcAddress(module, "xrEndFrame"), xrEndFrame);

	s_hooks_installed = true;
}
