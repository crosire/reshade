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
#include "com_utils.hpp"
#include "hook_manager.hpp"
#include "lockfree_linear_map.hpp"
#include <functional>
#include <deque>

#define XR_USE_GRAPHICS_API_VULKAN
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

struct captured_swapchain
{
	// TODO_OXR: why deque?
	std::deque<uint32_t> acquired_index;
	uint32_t last_released_index { 0 };

	XrSwapchainCreateInfo create_info {};
	std::vector<XrSwapchainImageVulkanKHR> surface_images;
};

static std::unordered_map<XrSwapchain, captured_swapchain> g_captured_swapchains;
static VkDevice g_oxr_vulkan_device = VK_NULL_HANDLE;
static uint32_t g_oxr_vulkan_queue_index = 0u;

static reshade::openxr::swapchain_impl *s_vr_swapchain = nullptr;

XrResult XRAPI_CALL xrCreateSession(XrInstance instance, const XrSessionCreateInfo *createInfo, XrSession *session)
{
	static const auto trampoline = reshade::hooks::call(xrCreateSession);

	const XrResult result = trampoline(instance, createInfo, session);
	if (XR_SUCCEEDED(result))
	{
		auto const gbv = reinterpret_cast<XrGraphicsBindingVulkanKHR const *>(createInfo->next);
		g_oxr_vulkan_device = gbv->device;
		g_oxr_vulkan_queue_index = gbv->queueIndex;
	}
	return result;
}

XrResult XRAPI_CALL xrDestroySession(XrSession session)
{
	delete s_vr_swapchain;

	static const auto trampoline = reshade::hooks::call(xrDestroySession);
	return trampoline(session);
}

XrResult XRAPI_CALL xrCreateSwapchain(XrSession session, const XrSwapchainCreateInfo *createInfo, XrSwapchain *swapchain)
{
	const_cast<XrSwapchainCreateInfo *>(createInfo)->usageFlags |= XR_SWAPCHAIN_USAGE_TRANSFER_SRC_BIT;

	static const auto trampoline = reshade::hooks::call(xrCreateSwapchain);

	const XrResult result = trampoline(session, createInfo, swapchain);
	if (XR_SUCCEEDED(result))
	{
		captured_swapchain csd {};
		csd.create_info = *createInfo;

		static auto const xrEnumerateSwapchainImages = reinterpret_cast<PFN_xrEnumerateSwapchainImages>(
		  ::GetProcAddress(::GetModuleHandle(L"openxr_loader.dll"), "xrEnumerateSwapchainImages"));
		assert(xrEnumerateSwapchainImages != nullptr);

		uint32_t numSurfaces = 0u;
		auto rv = xrEnumerateSwapchainImages(*swapchain, 0u, &numSurfaces, nullptr);
		assert(XR_SUCCEEDED(rv));
		if (!XR_SUCCEEDED(rv))
			return result;

		// Track our own information about the swapchain, so we can draw stuff onto it.
		csd.surface_images.resize(numSurfaces, { XR_TYPE_SWAPCHAIN_IMAGE_VULKAN_KHR });
		rv = xrEnumerateSwapchainImages(*swapchain, numSurfaces, &numSurfaces, reinterpret_cast<XrSwapchainImageBaseHeader *>(csd.surface_images.data()));
		assert(XR_SUCCEEDED(rv));
		if (!XR_SUCCEEDED(rv))
			return result;

		g_captured_swapchains.insert_or_assign(*swapchain, std::move(csd));
	}
	return result;
}

XrResult XRAPI_CALL xrDestroySwapchain(XrSwapchain swapchain)
{
	static const auto trampoline = reshade::hooks::call(xrDestroySwapchain);

	const XrResult result = trampoline(swapchain);
	if (XR_SUCCEEDED(result))
	{
		if (const auto it = g_captured_swapchains.find(swapchain);
			it != g_captured_swapchains.end())
		{
			g_captured_swapchains.erase(it);
		}
	}
	return result;
}

XrResult XRAPI_CALL xrAcquireSwapchainImage(XrSwapchain swapchain, const XrSwapchainImageAcquireInfo *acquireInfo, uint32_t *index)
{
	static const auto trampoline = reshade::hooks::call(xrAcquireSwapchainImage);

	const XrResult result = trampoline(swapchain, acquireInfo, index);
	if (XR_SUCCEEDED(result))
	{
		if (const auto it = g_captured_swapchains.find(swapchain);
			it != g_captured_swapchains.end())
		{
			captured_swapchain &csd = it->second;
			csd.acquired_index.push_back(*index);
		}
	}
	return result;
}

XrResult XRAPI_CALL xrReleaseSwapchainImage(XrSwapchain swapchain, const XrSwapchainImageReleaseInfo *releaseInfo)
{
	static const auto trampoline = reshade::hooks::call(xrReleaseSwapchainImage);

	const XrResult result = trampoline(swapchain, releaseInfo);
	if (XR_SUCCEEDED(result))
	{
		if (const auto it = g_captured_swapchains.find(swapchain);
			it != g_captured_swapchains.end())
		{
			captured_swapchain &csd = it->second;
			csd.last_released_index = csd.acquired_index.front();
			csd.acquired_index.pop_front();
		}
	}
	return result;
}

XrResult XRAPI_CALL xrEndFrame(XrSession session, const XrFrameEndInfo *frameEndInfo)
{
	XrSwapchainImageVulkanKHR left_image { XR_TYPE_SWAPCHAIN_IMAGE_VULKAN_KHR };
	XrSwapchainImageVulkanKHR right_image { XR_TYPE_SWAPCHAIN_IMAGE_VULKAN_KHR };

	for (uint32_t i = 0; i < frameEndInfo->layerCount; ++i)
	{
		// We don't care about overlays.
		if (frameEndInfo->layers[i]->type == XR_TYPE_COMPOSITION_LAYER_PROJECTION)
		{
			auto const proj = reinterpret_cast<const XrCompositionLayerProjection *>(frameEndInfo->layers[i]);

			auto const itLeft = g_captured_swapchains.find(proj->views[static_cast<int>(reshade::openxr::eye::left)].subImage.swapchain);
			assert(itLeft != g_captured_swapchains.end());
			auto const llri = itLeft->second.last_released_index;
			left_image = itLeft->second.surface_images[llri];

			auto const itRight = g_captured_swapchains.find(proj->views[static_cast<int>(reshade::openxr::eye::right)].subImage.swapchain);
			assert(itRight != g_captured_swapchains.end());
			auto const rlri = itRight->second.last_released_index;
			right_image = itRight->second.surface_images[rlri];
			break;
		}
	}

	if (left_image.image != VK_NULL_HANDLE && right_image.image != VK_NULL_HANDLE)
	{
		extern lockfree_linear_map<void *, reshade::vulkan::device_impl *, 8> g_vulkan_devices;
		reshade::vulkan::device_impl *const device = g_vulkan_devices.at(dispatch_key_from_handle(g_oxr_vulkan_device));

		VkQueue graphicsQueue = VK_NULL_HANDLE;
		device->_dispatch_table.GetDeviceQueue(g_oxr_vulkan_device, device->_graphics_queue_family_index, 0, &graphicsQueue);

		reshade::vulkan::command_queue_impl *queue = nullptr;
		if (const auto queue_it = std::find_if(device->_queues.cbegin(), device->_queues.cend(),
				[graphicsQueue](reshade::vulkan::command_queue_impl *queue) { return queue->_orig == graphicsQueue; });
			queue_it != device->_queues.cend())
			queue = *queue_it;

		if (nullptr == s_vr_swapchain)
			s_vr_swapchain = new reshade::openxr::swapchain_impl(device, queue, session);

		s_vr_swapchain->on_present(queue, { (uint64_t)left_image.image }, { (uint64_t)right_image.image });

		queue->flush_immediate_command_list();
	}

	static const auto trampoline = reshade::hooks::call(xrEndFrame);
	return trampoline(session, frameEndInfo);
}

static bool g_oxr_hooks_init_attempted = false;

void check_and_init_openxr_hooks()
{
	if (g_oxr_hooks_init_attempted)
		return;

#ifndef _WIN64
	const HMODULE module = GetModuleHandleW(L"openxr_loader.dll");
#else
	// TODO_OXR: not tested
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

	g_oxr_hooks_init_attempted = true;
}
