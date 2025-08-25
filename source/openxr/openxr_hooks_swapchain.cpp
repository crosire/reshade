/*
 * Copyright (C) 2023 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause OR MIT
 */

#include "openxr_hooks.hpp"
#include "openxr_impl_swapchain.hpp"
#include "d3d11/d3d11_device.hpp"
#include "d3d11/d3d11_device_context.hpp"
#include "d3d12/d3d12_device.hpp"
#include "d3d12/d3d12_command_queue.hpp"
#include "opengl/opengl_impl_device_context.hpp"
#include "opengl/opengl_impl_type_convert.hpp"
#include "vulkan/vulkan_hooks.hpp"
#include "vulkan/vulkan_impl_device.hpp"
#include "vulkan/vulkan_impl_command_queue.hpp"
#include "dll_log.hpp"
#include "com_utils.hpp"
#include "hook_manager.hpp"
#include "lockfree_linear_map.hpp"
#include <deque>
#include <algorithm> // std::find, std::find_if

#define XR_USE_GRAPHICS_API_D3D11
#define XR_USE_GRAPHICS_API_D3D12
#define XR_USE_GRAPHICS_API_OPENGL
#define XR_USE_GRAPHICS_API_VULKAN
#include <openxr/openxr_platform.h>

struct openxr_session
{
	const XrGeneratedDispatchTable *dispatch_table = nullptr;
	reshade::openxr::swapchain_impl *swapchain_impl = nullptr;
};
struct openxr_swapchain
{
	const XrGeneratedDispatchTable *dispatch_table = nullptr;
	XrSwapchainCreateFlags create_flags = 0;
	std::vector<reshade::api::resource> surface_images;
	std::deque<uint32_t> acquired_index;
	uint32_t last_released_index = 0;
};

static lockfree_linear_map<XrSession, openxr_session, 16> s_openxr_sessions;
extern lockfree_linear_map<XrInstance, openxr_instance, 16> g_openxr_instances;
static lockfree_linear_map<XrSwapchain, openxr_swapchain, 16> s_openxr_swapchains;

XrResult XRAPI_CALL xrCreateSession(XrInstance instance, const XrSessionCreateInfo *pCreateInfo, XrSession *pSession)
{
	reshade::log::message(reshade::log::level::info, "Redirecting xrCreateSession(instance = %" PRIx64 ", pCreateInfo = %p, pSession = %p) ...", instance, pCreateInfo, pSession);

	assert(pCreateInfo != nullptr && pSession != nullptr);

	const XrGeneratedDispatchTable &dispatch_table = g_openxr_instances.at(instance).dispatch_table;
	RESHADE_OPENXR_GET_DISPATCH_PTR_FROM(CreateSession, &dispatch_table);

	const XrResult result = trampoline(instance, pCreateInfo, pSession);
	if (XR_FAILED(result))
	{
		reshade::log::message(reshade::log::level::warning, "xrCreateSession failed with error code %d.", static_cast<int>(result));
		return result;
	}

	auto enum_view_configurations = dispatch_table.EnumerateViewConfigurations;
	assert(enum_view_configurations != nullptr);

	uint32_t num_view_configurations = 0;
	enum_view_configurations(instance, pCreateInfo->systemId, num_view_configurations, &num_view_configurations, nullptr);
	std::vector<XrViewConfigurationType> view_configurations(num_view_configurations);
	enum_view_configurations(instance, pCreateInfo->systemId, num_view_configurations, &num_view_configurations, view_configurations.data());

	reshade::openxr::swapchain_impl *swapchain_impl = nullptr;

	if (std::find(view_configurations.begin(), view_configurations.end(), XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO) != view_configurations.end())
	{
		// Initialize OpenVR hooks, in case this OpenXR instance is using the SteamVR runtime, so that the VR dashboard overlay can be added
		extern void check_and_init_openvr_hooks();
		check_and_init_openvr_hooks();

		if (const auto binding_d3d11 = find_in_structure_chain<XrGraphicsBindingD3D11KHR>(pCreateInfo, XR_TYPE_GRAPHICS_BINDING_D3D11_KHR))
		{
			if (const auto device_proxy = get_private_pointer_d3dx<D3D11Device>(binding_d3d11->device))
			{
				swapchain_impl = new reshade::openxr::swapchain_impl(device_proxy, device_proxy->_immediate_context, *pSession);
			}
			else
			{
				reshade::log::message(reshade::log::level::warning, "Skipping OpenXR session because it was created without a proxy Direct3D 11 device.");
			}
		}
		else
		if (const auto binding_d3d12 = find_in_structure_chain<XrGraphicsBindingD3D12KHR>(pCreateInfo, XR_TYPE_GRAPHICS_BINDING_D3D12_KHR))
		{
			if (const auto device_proxy = get_private_pointer_d3dx<D3D12Device>(binding_d3d12->device))
			{
				if (com_ptr<D3D12CommandQueue> command_queue_proxy;
					SUCCEEDED(binding_d3d12->queue->QueryInterface(&command_queue_proxy)))
				{
					swapchain_impl = new reshade::openxr::swapchain_impl(device_proxy, command_queue_proxy.get(), *pSession);
				}
			}
			else
			{
				reshade::log::message(reshade::log::level::warning, "Skipping OpenXR session because it was created without a proxy Direct3D 12 device.");
			}
		}
		else
		if (const auto binding_opengl = find_in_structure_chain<XrGraphicsBindingOpenGLWin32KHR>(pCreateInfo, XR_TYPE_GRAPHICS_BINDING_OPENGL_WIN32_KHR))
		{
			extern thread_local reshade::opengl::device_context_impl *g_opengl_context;

			if (g_opengl_context != nullptr)
			{
				assert(reinterpret_cast<HGLRC>(g_opengl_context->get_native()) == binding_opengl->hGLRC);

				swapchain_impl = new reshade::openxr::swapchain_impl(g_opengl_context->get_device(), g_opengl_context, *pSession);
			}
			else
			{
				reshade::log::message(reshade::log::level::warning, "Skipping OpenXR session because it was created without an OpenGL context current.");
			}
		}
		else
		if (const auto binding_vulkan = find_in_structure_chain<XrGraphicsBindingVulkanKHR>(pCreateInfo, XR_TYPE_GRAPHICS_BINDING_VULKAN_KHR))
		{
			extern lockfree_linear_map<void *, reshade::vulkan::device_impl *, 8> g_vulkan_devices;

			if (reshade::vulkan::device_impl *const device = g_vulkan_devices.at(dispatch_key_from_handle(binding_vulkan->device)))
			{
				VkQueue queue_handle = VK_NULL_HANDLE;
				device->_dispatch_table.GetDeviceQueue(binding_vulkan->device, binding_vulkan->queueFamilyIndex, binding_vulkan->queueIndex, &queue_handle);
				assert(queue_handle != VK_NULL_HANDLE);

				if (const auto queue_it = std::find_if(device->_queues.cbegin(), device->_queues.cend(),
						[queue_handle](reshade::vulkan::command_queue_impl *queue) { return queue->_orig == queue_handle; });
					queue_it != device->_queues.cend())
				{
					swapchain_impl = new reshade::openxr::swapchain_impl(device, *queue_it, *pSession);
				}
			}
			else
			{
				reshade::log::message(reshade::log::level::warning, "Skipping OpenXR session because it was created without a known Vulkan device.");
			}
		}
	}
	else
	{
		reshade::log::message(reshade::log::level::warning, "Skipping OpenXR session because the system does not support the stereo view configuration.");
	}

	s_openxr_sessions.emplace(*pSession, openxr_session { &dispatch_table, swapchain_impl });

#if RESHADE_VERBOSE_LOG
	reshade::log::message(reshade::log::level::debug, "Returning OpenXR session %" PRIx64 ".", *pSession);
#endif
	return result;
}

XrResult XRAPI_CALL xrDestroySession(XrSession session)
{
	reshade::log::message(reshade::log::level::info, "Redirecting xrDestroySession(session = %" PRIx64 ") ...", session);

	assert(session != XR_NULL_HANDLE);

	const openxr_session &data = s_openxr_sessions.at(session);
	RESHADE_OPENXR_GET_DISPATCH_PTR_FROM(DestroySession, data.dispatch_table);

	delete data.swapchain_impl;

	s_openxr_sessions.erase(session);

	return trampoline(session);
}

XrResult XRAPI_CALL xrCreateSwapchain(XrSession session, const XrSwapchainCreateInfo *pCreateInfo, XrSwapchain *pSwapchain)
{
	reshade::log::message(reshade::log::level::info, "Redirecting xrCreateSwapchain(session = %" PRIx64 ", pCreateInfo = %p, pSwapchain = %p) ...", session, pCreateInfo, pSwapchain);

	const openxr_session &data = s_openxr_sessions.at(session);
	RESHADE_OPENXR_GET_DISPATCH_PTR_FROM(CreateSwapchain, data.dispatch_table);

	assert(pCreateInfo != nullptr && pSwapchain != nullptr);

	XrSwapchainCreateInfo create_info = *pCreateInfo;
	// Add required usage flags to create info
	create_info.usageFlags |= XR_SWAPCHAIN_USAGE_TRANSFER_SRC_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT;

	const XrResult result = trampoline(session, &create_info, pSwapchain);
	if (XR_FAILED(result))
	{
		reshade::log::message(reshade::log::level::warning, "xrCreateSwapchain failed with error code %d.", static_cast<int>(result));
		return result;
	}

	std::vector<reshade::api::resource> images;

	if (data.swapchain_impl != nullptr)
	{
		auto enum_swapchain_images = data.dispatch_table->EnumerateSwapchainImages;
		assert(enum_swapchain_images != nullptr);

		uint32_t num_images = 0;
		enum_swapchain_images(*pSwapchain, num_images, &num_images, nullptr);
		images.reserve(num_images);

		switch (data.swapchain_impl->get_device()->get_api())
		{
		case reshade::api::device_api::d3d11:
		{
			std::vector<XrSwapchainImageD3D11KHR> images_d3d11(num_images, { XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR });
			enum_swapchain_images(*pSwapchain, num_images, &num_images, reinterpret_cast<XrSwapchainImageBaseHeader *>(images_d3d11.data()));
			for (const XrSwapchainImageD3D11KHR &image_d3d11 : images_d3d11)
				images.push_back({ reinterpret_cast<uintptr_t>(image_d3d11.texture) });
			break;
		}
		case reshade::api::device_api::d3d12:
		{
			std::vector<XrSwapchainImageD3D12KHR> images_d3d12(num_images, { XR_TYPE_SWAPCHAIN_IMAGE_D3D12_KHR });
			enum_swapchain_images(*pSwapchain, num_images, &num_images, reinterpret_cast<XrSwapchainImageBaseHeader *>(images_d3d12.data()));
			for (const XrSwapchainImageD3D12KHR &image_d3d12 : images_d3d12)
				images.push_back({ reinterpret_cast<uintptr_t>(image_d3d12.texture) });
			break;
		}
		case reshade::api::device_api::opengl:
		{
			std::vector<XrSwapchainImageOpenGLKHR> images_opengl(num_images, { XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_KHR });
			enum_swapchain_images(*pSwapchain, num_images, &num_images, reinterpret_cast<XrSwapchainImageBaseHeader *>(images_opengl.data()));
			for (const XrSwapchainImageOpenGLKHR &image_opengl : images_opengl)
				images.push_back(reshade::opengl::make_resource_handle(GL_TEXTURE_2D, image_opengl.image));
			break;
		}
		case reshade::api::device_api::vulkan:
		{
			std::vector<XrSwapchainImageVulkanKHR> images_vulkan(num_images, { XR_TYPE_SWAPCHAIN_IMAGE_VULKAN_KHR });
			enum_swapchain_images(*pSwapchain, num_images, &num_images, reinterpret_cast<XrSwapchainImageBaseHeader *>(images_vulkan.data()));
			for (const XrSwapchainImageVulkanKHR &image_vulkan : images_vulkan)
				images.push_back({ (uint64_t)image_vulkan.image });
			break;
		}
		}
	}

	s_openxr_swapchains.emplace(*pSwapchain, openxr_swapchain { data.dispatch_table, create_info.createFlags, std::move(images) });

#if RESHADE_VERBOSE_LOG
	reshade::log::message(reshade::log::level::debug, "Returning OpenXR swap chain %" PRIx64 ".", *pSwapchain);
#endif
	return result;
}

XrResult XRAPI_CALL xrDestroySwapchain(XrSwapchain swapchain)
{
	reshade::log::message(reshade::log::level::info, "Redirecting xrDestroySwapchain(swapchain = %" PRIx64 ") ...", swapchain);

	const openxr_swapchain &data = s_openxr_swapchains.at(swapchain);
	RESHADE_OPENXR_GET_DISPATCH_PTR_FROM(DestroySwapchain, data.dispatch_table);

	s_openxr_swapchains.erase(swapchain);

	return trampoline(swapchain);
}

XrResult XRAPI_CALL xrAcquireSwapchainImage(XrSwapchain swapchain, const XrSwapchainImageAcquireInfo *pAcquireInfo, uint32_t *pIndex)
{
	openxr_swapchain &data = s_openxr_swapchains.at(swapchain);
	RESHADE_OPENXR_GET_DISPATCH_PTR_FROM(AcquireSwapchainImage, data.dispatch_table);

	const XrResult result = trampoline(swapchain, pAcquireInfo, pIndex);
	if (XR_FAILED(result))
		return result;

	data.acquired_index.push_back(*pIndex);

	return result;
}

XrResult XRAPI_CALL xrReleaseSwapchainImage(XrSwapchain swapchain, const XrSwapchainImageReleaseInfo *pReleaseInfo)
{
	openxr_swapchain &data = s_openxr_swapchains.at(swapchain);
	RESHADE_OPENXR_GET_DISPATCH_PTR_FROM(ReleaseSwapchainImage, data.dispatch_table);

	const XrResult result = trampoline(swapchain, pReleaseInfo);
	if (XR_FAILED(result))
		return result;

	// 'xrReleaseSwapchainImage' releases the last image from 'xrWaitSwapchainImage', which will implicitly wait on the oldest acquired swapchain image which has not yet been successfully waited on
	data.last_released_index = data.acquired_index.front();
	data.acquired_index.pop_front();

	return result;
}

XrResult XRAPI_CALL xrEndFrame(XrSession session, const XrFrameEndInfo *frameEndInfo)
{
	const openxr_session &data = s_openxr_sessions.at(session);

	if (data.swapchain_impl != nullptr)
	{
		for (uint32_t layer_index = 0; layer_index < frameEndInfo->layerCount; ++layer_index)
		{
			if (frameEndInfo->layers[layer_index]->type == XR_TYPE_COMPOSITION_LAYER_PROJECTION)
			{
				const XrCompositionLayerProjection *const layer = reinterpret_cast<const XrCompositionLayerProjection *>(frameEndInfo->layers[layer_index]);

				uint32_t view_count = 0;
				temp_mem<reshade::api::resource, 2> view_textures(layer->viewCount);
				temp_mem<reshade::api::subresource_box, 2> view_boxes(layer->viewCount);
				temp_mem<uint32_t, 2> view_layers(layer->viewCount);
				const std::vector<reshade::api::resource> *swapchain_images = nullptr;
				uint32_t swap_index = 0;

				assert(layer->viewCount != 0);

				for (; view_count < layer->viewCount; ++view_count)
				{
					XrSwapchainSubImage const &sub_image = layer->views[view_count].subImage;

					const openxr_swapchain &swapchain = s_openxr_swapchains.at(sub_image.swapchain);

					if ((swapchain.create_flags & XR_SWAPCHAIN_CREATE_STATIC_IMAGE_BIT) != 0)
						break; // Cannot apply effects to a static image, since it would just stack on top of the previous result every frame

					view_textures[view_count] = swapchain.surface_images[swapchain.last_released_index];

					assert(sub_image.imageRect.offset.x >= 0 && sub_image.imageRect.offset.y >= 0 && sub_image.imageRect.extent.width >= 0 && sub_image.imageRect.extent.height >= 0);

					reshade::api::subresource_box &view_box = view_boxes[view_count];
					view_box.left = static_cast<uint32_t>(sub_image.imageRect.offset.x);
					view_box.top = static_cast<uint32_t>(sub_image.imageRect.offset.y);
					view_box.front = 0;
					view_box.right = static_cast<uint32_t>(sub_image.imageRect.offset.x + sub_image.imageRect.extent.width);
					view_box.bottom = static_cast<uint32_t>(sub_image.imageRect.offset.y + sub_image.imageRect.extent.height);
					view_box.back = 1;

					view_layers[view_count] = sub_image.imageArrayIndex;

					swapchain_images = &swapchain.surface_images;
					if (view_textures[view_count] != view_textures[0] || view_layers[view_count] != view_layers[0])
						swapchain_images = nullptr;
					swap_index = swapchain.last_released_index;
				}

				if (view_count == layer->viewCount)
				{
					data.swapchain_impl->on_present(view_count, view_textures.p, view_boxes.p, view_layers.p, swapchain_images, swap_index);
					break;
				}
			}
		}
	}

	RESHADE_OPENXR_GET_DISPATCH_PTR_FROM(EndFrame, data.dispatch_table);
	return trampoline(session, frameEndInfo);
}
