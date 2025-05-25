/*
 * Copyright (C) 2021 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "openvr_impl_swapchain.hpp"
#include "d3d10/d3d10_device.hpp"
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
#include <cstdio> // std::sscanf
#include <cstring> // std::memcpy
#include <algorithm> // std::find_if, std::swap
#include <functional>
#include <ivrclientcore.h>

// There can only be a single global effect runtime in OpenVR (since its API is based on singletons)
static reshade::openvr::swapchain_impl *s_vr_swapchain = nullptr;

static const vr::VRTextureBounds_t calc_side_by_side_bounds(vr::EVREye eye, const vr::VRTextureBounds_t *orig_bounds)
{
	auto bounds = (eye != vr::Eye_Right) ?
		vr::VRTextureBounds_t { 0.0f, 0.0f, 0.5f, 1.0f } : // Left half of the texture
		vr::VRTextureBounds_t { 0.5f, 0.0f, 1.0f, 1.0f };  // Right half of the texture

	if (orig_bounds != nullptr && orig_bounds->uMin > orig_bounds->uMax)
		std::swap(bounds.uMin, bounds.uMax);
	if (orig_bounds != nullptr && orig_bounds->vMin > orig_bounds->vMax)
		std::swap(bounds.vMin, bounds.vMax);

	return bounds;
}

static vr::EVRCompositorError on_vr_submit_d3d10(vr::IVRCompositor *compositor, vr::EVREye eye, const vr::Texture_t *texture, const vr::VRTextureBounds_t *bounds, uint32_t layer, vr::EVRSubmitFlags flags,
	std::function<vr::EVRCompositorError(vr::EVREye eye, const vr::Texture_t *texture, const vr::VRTextureBounds_t *bounds, uint32_t layer, vr::EVRSubmitFlags flags)> submit, D3D10Device *device_proxy)
{
	if (device_proxy == nullptr)
		return submit(eye, texture, bounds, layer, flags); // No proxy device found, so just submit normally

	if (nullptr == s_vr_swapchain)
		s_vr_swapchain = new reshade::openvr::swapchain_impl(device_proxy, compositor);
	// It is not valid to switch the texture type once submitted for the first time
	else if (s_vr_swapchain->get_device() != device_proxy)
		return vr::VRCompositorError_TextureIsOnWrongDevice;

	const auto eye_texture = static_cast<ID3D10Texture2D *>(texture->handle);

	if (!s_vr_swapchain->on_vr_submit(eye, { reinterpret_cast<uintptr_t>(eye_texture) }, texture->eColorSpace, bounds, eye))
	{
		// Failed to initialize effect runtime or copy the eye texture, so submit normally without applying effects
#if RESHADE_VERBOSE_LOG
		reshade::log::message(reshade::log::level::error, "Failed to initialize effect runtime or copy the eye texture for eye %d!", static_cast<int>(eye));
#endif
		return submit(eye, texture, bounds, layer, flags);
	}

	// Skip submission of the first eye and instead submit both left and right eye in one step after application submitted both
	if (eye != vr::Eye_Right)
		return vr::VRCompositorError_None;

	vr::VRTextureWithPose_t target_texture;
	target_texture.handle = reinterpret_cast<ID3D10Texture2D *>(s_vr_swapchain->get_back_buffer().handle);
	target_texture.eType = vr::ETextureType::TextureType_DirectX;
	target_texture.eColorSpace = texture->eColorSpace;
	if ((flags & vr::Submit_TextureWithPose) != 0)
		target_texture.mDeviceToAbsoluteTracking = static_cast<const vr::VRTextureWithPose_t *>(texture)->mDeviceToAbsoluteTracking;

	assert((flags & (vr::Submit_GlRenderBuffer | vr::Submit_GlArrayTexture | vr::Submit_VulkanTextureWithArrayData)) == 0 && target_texture.eType == texture->eType);
	// Cannot deal with depth texture from multiple submits, so skip it
	flags = static_cast<vr::EVRSubmitFlags>(flags & ~vr::Submit_TextureWithDepth);

	// The left and right eye were copied side-by-side to a single texture in 'on_vr_submit', so set bounds accordingly
	const vr::VRTextureBounds_t left_bounds = calc_side_by_side_bounds(vr::Eye_Left, bounds);
	const vr::VRTextureBounds_t right_bounds = calc_side_by_side_bounds(vr::Eye_Right, bounds);
	return
		submit(vr::Eye_Left, &target_texture, &left_bounds, 0, flags),
		submit(vr::Eye_Right, &target_texture, &right_bounds, 0, flags);
}
static vr::EVRCompositorError on_vr_submit_d3d11(vr::IVRCompositor *compositor, vr::EVREye eye, const vr::Texture_t *texture, const vr::VRTextureBounds_t *bounds, uint32_t layer, vr::EVRSubmitFlags flags,
	std::function<vr::EVRCompositorError(vr::EVREye eye, const vr::Texture_t *texture, const vr::VRTextureBounds_t *bounds, uint32_t layer, vr::EVRSubmitFlags flags)> submit)
{
	const auto eye_texture = static_cast<ID3D11Texture2D *>(texture->handle);

	com_ptr<ID3D11Device> device;
	eye_texture->GetDevice(&device); // 'GetDevice' is at the same virtual function table index for 'ID3D10Texture2D' and 'ID3D11Texture2D', so this happens to work for either case

	// Check if the passed texture is actually a D3D10 texture
	// Cannot just 'QueryInterface' with 'ID3D10Texture2D', since that is supported on D3D11 textures too for some reason, so instead check the device
	if (com_ptr<ID3D10Device> device10;
		device->QueryInterface(&device10) == S_OK)
	{
		// Also check that the device has a proxy 'D3D10Device' interface, otherwise it is likely still a D3D11 device exposing the 'ID3D10Device' interface, after 'ID3D11Device::CreateDeviceContextState' was called
		if (const auto device10_proxy = get_private_pointer_d3dx<D3D10Device>(device10.get()))
		{
			// Whoops, this is actually a D3D10 texture, redirect ...
			return on_vr_submit_d3d10(compositor, eye, texture, bounds, layer, flags, submit, device10_proxy);
		}
	}

	const auto device_proxy = get_private_pointer_d3dx<D3D11Device>(device.get());
	if (device_proxy == nullptr)
		return submit(eye, texture, bounds, layer, flags); // No proxy device found, so just submit normally

	if (nullptr == s_vr_swapchain)
		s_vr_swapchain = new reshade::openvr::swapchain_impl(device_proxy, compositor);
	// It is not valid to switch the texture type once submitted for the first time
	else if (s_vr_swapchain->get_device() != device_proxy)
		return vr::VRCompositorError_TextureIsOnWrongDevice;

	if (!s_vr_swapchain->on_vr_submit(eye, { reinterpret_cast<uintptr_t>(eye_texture) }, texture->eColorSpace, bounds, eye))
	{
		// Failed to initialize effect runtime or copy the eye texture, so submit normally without applying effects
#if RESHADE_VERBOSE_LOG
		reshade::log::message(reshade::log::level::error, "Failed to initialize effect runtime or copy the eye texture for eye %d!", static_cast<int>(eye));
#endif
		return submit(eye, texture, bounds, layer, flags);
	}

	// Skip submission of the first eye and instead submit both left and right eye in one step after application submitted both
	if (eye != vr::Eye_Right)
		return vr::VRCompositorError_None;

	vr::VRTextureWithPose_t target_texture;
	target_texture.handle = reinterpret_cast<ID3D11Texture2D *>(s_vr_swapchain->get_back_buffer().handle);
	target_texture.eType = vr::ETextureType::TextureType_DirectX;
	target_texture.eColorSpace = texture->eColorSpace;
	if ((flags & vr::Submit_TextureWithPose) != 0)
		target_texture.mDeviceToAbsoluteTracking = static_cast<const vr::VRTextureWithPose_t *>(texture)->mDeviceToAbsoluteTracking;

	assert((flags & (vr::Submit_GlRenderBuffer | vr::Submit_GlArrayTexture | vr::Submit_VulkanTextureWithArrayData)) == 0 && target_texture.eType == texture->eType);
	// Cannot deal with depth texture from multiple submits, so skip it
	flags = static_cast<vr::EVRSubmitFlags>(flags & ~vr::Submit_TextureWithDepth);

	// The left and right eye were copied side-by-side to a single texture in 'on_vr_submit', so set bounds accordingly
	const vr::VRTextureBounds_t left_bounds = calc_side_by_side_bounds(vr::Eye_Left, bounds);
	const vr::VRTextureBounds_t right_bounds = calc_side_by_side_bounds(vr::Eye_Right, bounds);
	return
		submit(vr::Eye_Left, &target_texture, &left_bounds, 0, flags),
		submit(vr::Eye_Right, &target_texture, &right_bounds, 0, flags);
}
static vr::EVRCompositorError on_vr_submit_d3d12(vr::IVRCompositor *compositor, vr::EVREye eye, const vr::Texture_t *texture, const vr::VRTextureBounds_t *bounds, uint32_t layer, vr::EVRSubmitFlags flags,
	std::function<vr::EVRCompositorError(vr::EVREye eye, const vr::Texture_t *texture, const vr::VRTextureBounds_t *bounds, uint32_t layer, vr::EVRSubmitFlags flags)> submit)
{
	const auto eye_texture = static_cast<const vr::D3D12TextureData_t * >(texture->handle);

	com_ptr<D3D12CommandQueue> command_queue_proxy;
	if (FAILED(eye_texture->m_pCommandQueue->QueryInterface(IID_PPV_ARGS(&command_queue_proxy))))
		return submit(eye, texture, bounds, layer, flags); // No proxy command queue found, so just submit normally

	if (nullptr == s_vr_swapchain)
		s_vr_swapchain = new reshade::openvr::swapchain_impl(command_queue_proxy.get(), compositor);
	else if (s_vr_swapchain->get_command_queue() != command_queue_proxy.get())
		return vr::VRCompositorError_TextureIsOnWrongDevice;

	// Synchronize access to the command queue while events are invoked and the immediate command list may be accessed
	std::unique_lock<std::recursive_mutex> lock(command_queue_proxy->_mutex);

	// Resource should be in D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE state at this point
	if (!s_vr_swapchain->on_vr_submit(eye, { reinterpret_cast<uintptr_t>(eye_texture->m_pResource) }, texture->eColorSpace, bounds, layer))
	{
		// Failed to initialize effect runtime or copy the eye texture, so submit normally without applying effects
#if RESHADE_VERBOSE_LOG
		reshade::log::message(reshade::log::level::error, "Failed to initialize effect runtime or copy the eye texture for eye %d!", static_cast<int>(eye));
#endif
		return submit(eye, texture, bounds, layer, flags);
	}

	// Skip submission of the first eye and instead submit both left and right eye in one step after application submitted both
	if (eye != vr::Eye_Right)
		return vr::VRCompositorError_None;

	command_queue_proxy->flush_immediate_command_list();

	lock.unlock();

	vr::D3D12TextureData_t target_texture_data = *eye_texture;
	target_texture_data.m_pResource = reinterpret_cast<ID3D12Resource *>(s_vr_swapchain->get_back_buffer().handle);

	vr::VRTextureWithPose_t target_texture;
	target_texture.handle = &target_texture_data;
	target_texture.eType = vr::ETextureType::TextureType_DirectX12;
	target_texture.eColorSpace = texture->eColorSpace;
	if ((flags & vr::Submit_TextureWithPose) != 0)
		target_texture.mDeviceToAbsoluteTracking = static_cast<const vr::VRTextureWithPose_t *>(texture)->mDeviceToAbsoluteTracking;

	assert((flags & (vr::Submit_GlRenderBuffer | vr::Submit_GlArrayTexture | vr::Submit_VulkanTextureWithArrayData)) == 0 && target_texture.eType == texture->eType);
	// Cannot deal with depth texture from multiple submits, so skip it
	flags = static_cast<vr::EVRSubmitFlags>(flags & ~vr::Submit_TextureWithDepth);

	// The left and right eye were copied side-by-side to a single texture in 'on_vr_submit', so set bounds accordingly
	const vr::VRTextureBounds_t left_bounds = calc_side_by_side_bounds(vr::Eye_Left, bounds);
	const vr::VRTextureBounds_t right_bounds = calc_side_by_side_bounds(vr::Eye_Right, bounds);
	return
		submit(vr::Eye_Left, &target_texture, &left_bounds, 0, flags),
		submit(vr::Eye_Right, &target_texture, &right_bounds, 0, flags);
}
static vr::EVRCompositorError on_vr_submit_opengl(vr::IVRCompositor *compositor, vr::EVREye eye, const vr::Texture_t *texture, const vr::VRTextureBounds_t *bounds, uint32_t layer, vr::EVRSubmitFlags flags,
	std::function<vr::EVRCompositorError(vr::EVREye eye, const vr::Texture_t *texture, const vr::VRTextureBounds_t *bounds, uint32_t layer, vr::EVRSubmitFlags flags)> submit)
{
	extern thread_local reshade::opengl::device_context_impl *g_opengl_context;

	if (g_opengl_context == nullptr)
		return submit(eye, texture, bounds, layer, flags);

	if (nullptr == s_vr_swapchain)
		s_vr_swapchain = new reshade::openvr::swapchain_impl(g_opengl_context->get_device(), g_opengl_context, compositor);
	else if (s_vr_swapchain->get_command_queue() != g_opengl_context)
		return vr::VRCompositorError_TextureIsOnWrongDevice;

	const reshade::api::resource eye_texture = reshade::opengl::make_resource_handle(
		(flags & vr::Submit_GlRenderBuffer) != 0 ? GL_RENDERBUFFER : ((flags & vr::Submit_GlArrayTexture) != 0 ? GL_TEXTURE_2D_ARRAY : GL_TEXTURE_2D),
		static_cast<GLuint>(reinterpret_cast<uintptr_t>(texture->handle)));

	if (!s_vr_swapchain->on_vr_submit(eye, eye_texture, texture->eColorSpace, bounds, layer))
	{
		// Failed to initialize effect runtime or copy the eye texture, so submit normally without applying effects
#if RESHADE_VERBOSE_LOG
		reshade::log::message(reshade::log::level::error, "Failed to initialize effect runtime or copy the eye texture for eye %d!", static_cast<int>(eye));
#endif
		return submit(eye, texture, bounds, layer, flags);
	}

	// Skip submission of the first eye and instead submit both left and right eye in one step after application submitted both
	if (eye != vr::Eye_Right)
		return vr::VRCompositorError_None;

	vr::VRTextureWithPose_t target_texture;
	target_texture.handle = reinterpret_cast<void *>(static_cast<uintptr_t>(s_vr_swapchain->get_back_buffer().handle & 0xFFFFFFFF));
	target_texture.eType = vr::ETextureType::TextureType_OpenGL;
	target_texture.eColorSpace = texture->eColorSpace;
	if ((flags & vr::Submit_TextureWithPose) != 0)
		target_texture.mDeviceToAbsoluteTracking = static_cast<const vr::VRTextureWithPose_t *>(texture)->mDeviceToAbsoluteTracking;

	assert((flags & (vr::Submit_VulkanTextureWithArrayData)) == 0 && target_texture.eType == texture->eType);
	// The side-by-side texture created in 'on_vr_submit' is a 2D texture
	flags = static_cast<vr::EVRSubmitFlags>(flags & ~(vr::Submit_GlRenderBuffer | vr::Submit_GlArrayTexture | vr::Submit_TextureWithDepth));

	// The left and right eye were copied side-by-side to a single texture in 'on_vr_submit', so set bounds accordingly
	const vr::VRTextureBounds_t left_bounds = calc_side_by_side_bounds(vr::Eye_Left, bounds);
	const vr::VRTextureBounds_t right_bounds = calc_side_by_side_bounds(vr::Eye_Right, bounds);
	return
		submit(vr::Eye_Left, &target_texture, &left_bounds, 0, flags),
		submit(vr::Eye_Right, &target_texture, &right_bounds, 0, flags);
}
static vr::EVRCompositorError on_vr_submit_vulkan(vr::IVRCompositor *compositor, vr::EVREye eye, const vr::Texture_t *texture, const vr::VRTextureBounds_t *bounds, uint32_t layer, vr::EVRSubmitFlags flags,
	std::function<vr::EVRCompositorError(vr::EVREye eye, const vr::Texture_t *texture, const vr::VRTextureBounds_t *bounds, uint32_t layer, vr::EVRSubmitFlags flags)> submit)
{
	extern lockfree_linear_map<void *, reshade::vulkan::device_impl *, 8> g_vulkan_devices;

	const auto eye_texture = static_cast<const vr::VRVulkanTextureData_t *>(texture->handle);

	reshade::vulkan::device_impl *device = g_vulkan_devices.at(dispatch_key_from_handle(eye_texture->m_pDevice));
	if (device == nullptr)
		return submit(eye, texture, bounds, layer, flags);

	reshade::vulkan::command_queue_impl *queue = nullptr;
	if (const auto queue_it = std::find_if(device->_queues.cbegin(), device->_queues.cend(),
			[eye_texture](reshade::vulkan::command_queue_impl *queue) { return queue->_orig == eye_texture->m_pQueue; });
		queue_it != device->_queues.cend())
		queue = *queue_it;
	else
		return submit(eye, texture, bounds, layer, flags);

	if (nullptr == s_vr_swapchain)
		// OpenVR requires the passed in queue to be a graphics queue, so can safely use it
		s_vr_swapchain = new reshade::openvr::swapchain_impl(device, queue, compositor);
	else if (s_vr_swapchain->get_command_queue() != queue)
		return vr::VRCompositorError_TextureIsOnWrongDevice;

	// Image should be in VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL layout at this point
	if (!s_vr_swapchain->on_vr_submit(eye, { (uint64_t)(VkImage)eye_texture->m_nImage }, texture->eColorSpace, bounds, (flags & vr::Submit_VulkanTextureWithArrayData) != 0 ? static_cast<const vr::VRVulkanTextureArrayData_t *>(eye_texture)->m_unArrayIndex : layer))
	{
		// Failed to initialize effect runtime or copy the eye texture, so submit normally without applying effects
#if RESHADE_VERBOSE_LOG
		reshade::log::message(reshade::log::level::error, "Failed to initialize effect runtime or copy the eye texture for eye %d!", static_cast<int>(eye));
#endif
		return submit(eye, texture, bounds, layer, flags);
	}

	// Skip submission of the first eye and instead submit both left and right eye in one step after application submitted both
	if (eye != vr::Eye_Right)
		return vr::VRCompositorError_None;

	queue->flush_immediate_command_list();

	const reshade::api::resource target = s_vr_swapchain->get_current_back_buffer();
	const reshade::api::resource_desc target_desc = device->get_resource_desc(target);

	vr::VRVulkanTextureData_t target_texture_data = *eye_texture;
	target_texture_data.m_nImage = (uint64_t)(VkImage)target.handle;
	target_texture_data.m_nWidth = target_desc.texture.width;
	target_texture_data.m_nHeight = target_desc.texture.height;
	// Multisampled source textures were already resolved, so sample count is always one at this point
	target_texture_data.m_nSampleCount = target_desc.texture.samples;

	vr::VRTextureWithPose_t target_texture;
	target_texture.handle = &target_texture_data;
	target_texture.eType = vr::ETextureType::TextureType_Vulkan;
	target_texture.eColorSpace = texture->eColorSpace;
	if ((flags & vr::Submit_TextureWithPose) != 0)
		target_texture.mDeviceToAbsoluteTracking = static_cast<const vr::VRTextureWithPose_t *>(texture)->mDeviceToAbsoluteTracking;

	assert((flags & (vr::Submit_GlRenderBuffer | vr::Submit_GlArrayTexture)) == 0 && target_texture.eType == texture->eType);
	// The side-by-side texture created in 'on_vr_submit' is not an array texture
	flags = static_cast<vr::EVRSubmitFlags>(flags & ~(vr::Submit_VulkanTextureWithArrayData | vr::Submit_TextureWithDepth));

	// The left and right eye were copied side-by-side to a single texture in 'on_vr_submit', so set bounds accordingly
	const vr::VRTextureBounds_t left_bounds = calc_side_by_side_bounds(vr::Eye_Left, bounds);
	const vr::VRTextureBounds_t right_bounds = calc_side_by_side_bounds(vr::Eye_Right, bounds);
	return
		submit(vr::Eye_Left, &target_texture, &left_bounds, 0, flags),
		submit(vr::Eye_Right, &target_texture, &right_bounds, 0, flags);
}

#ifdef _WIN64
	#define VR_Interface_Impl(type, method_name, vtable_index, interface_version, impl, return_type, ...) \
		static return_type type##_##method_name##_##interface_version(vr::type *pThis, ##__VA_ARGS__) \
		{ \
			static const auto trampoline = reshade::hooks::call(type##_##method_name##_##interface_version, reshade::hooks::vtable_from_instance(pThis) + vtable_index); \
			impl \
		}
	#define VR_Interface_Call(...) trampoline(pThis, ##__VA_ARGS__)
#else
	// The OpenVR interface functions use the __thiscall calling convention on x86, so need to emulate that with __fastcall and a dummy second argument which passes along the EDX register value
	#define VR_Interface_Impl(type, method_name, vtable_index, interface_version, impl, return_type, ...) \
		static return_type __fastcall type##_##method_name##_##interface_version(vr::type *pThis, void *EDX, ##__VA_ARGS__) \
		{ \
			static const auto trampoline = reshade::hooks::call(type##_##method_name##_##interface_version, reshade::hooks::vtable_from_instance(pThis) + vtable_index); \
			impl \
		}
	#define VR_Interface_Call(...) trampoline(pThis, EDX, ##__VA_ARGS__)
#endif

VR_Interface_Impl(IVRCompositor, Submit, 6, 007, {
	if (pTexture == nullptr)
		return vr::VRCompositorError_InvalidTexture;

	vr::Texture_t texture;
	texture.handle = pTexture;
	texture.eType = static_cast<vr::ETextureType>(eTextureType);
	texture.eColorSpace = vr::ColorSpace_Auto;

	const auto submit_lambda = [&](vr::EVREye eye, const vr::Texture_t *texture, const vr::VRTextureBounds_t *bounds, uint32_t, vr::EVRSubmitFlags flags) {
		assert(flags == vr::Submit_Default);
		return VR_Interface_Call(eye, static_cast<unsigned int>(texture->eType), texture->handle, bounds);
	};

	switch (eTextureType)
	{
	case vr::TextureType_DirectX: // API_DirectX
		return on_vr_submit_d3d11(pThis, eEye, &texture, pBounds, static_cast<uint32_t>(eEye), vr::Submit_Default, submit_lambda);
	case vr::TextureType_OpenGL:  // API_OpenGL
		return on_vr_submit_opengl(pThis, eEye, &texture, pBounds, static_cast<uint32_t>(eEye), vr::Submit_Default, submit_lambda);
	default:
		assert(false);
		return vr::VRCompositorError_InvalidTexture;
	}
}, vr::EVRCompositorError, /* vr::Hmd_Eye */ vr::EVREye eEye, /* vr::GraphicsAPIConvention */ unsigned int eTextureType, void *pTexture, const vr::VRTextureBounds_t *pBounds)

VR_Interface_Impl(IVRCompositor, Submit, 6, 008, {
	if (pTexture == nullptr)
		return vr::VRCompositorError_InvalidTexture;

	vr::Texture_t texture;
	texture.handle = pTexture;
	texture.eType = static_cast<vr::ETextureType>(eTextureType);
	texture.eColorSpace = vr::ColorSpace_Auto;

	const auto submit_lambda = [&](vr::EVREye eye, const vr::Texture_t *texture, const vr::VRTextureBounds_t *bounds, uint32_t, vr::EVRSubmitFlags flags) {
		return VR_Interface_Call(eye, static_cast<unsigned int>(texture->eType), texture->handle, bounds, flags);
	};

	switch (eTextureType)
	{
	case vr::TextureType_DirectX: // API_DirectX
		return on_vr_submit_d3d11(pThis, eEye, &texture, pBounds, static_cast<uint32_t>(eEye), nSubmitFlags, submit_lambda);
	case vr::TextureType_OpenGL:  // API_OpenGL
		return on_vr_submit_opengl(pThis, eEye, &texture, pBounds, static_cast<uint32_t>(eEye), nSubmitFlags, submit_lambda);
	default:
		assert(false);
		return vr::VRCompositorError_InvalidTexture;
	}
}, vr::EVRCompositorError, /* vr::Hmd_Eye */ vr::EVREye eEye, /* vr::GraphicsAPIConvention */ unsigned int eTextureType, void *pTexture, const vr::VRTextureBounds_t *pBounds, /* vr::VRSubmitFlags_t */ vr::EVRSubmitFlags nSubmitFlags)

VR_Interface_Impl(IVRCompositor, Submit, 4, 009, {
	if (pTexture == nullptr || pTexture->handle == nullptr)
		return vr::VRCompositorError_InvalidTexture;

	const auto submit_lambda = [&](vr::EVREye eye, const vr::Texture_t *texture, const vr::VRTextureBounds_t *bounds, uint32_t, vr::EVRSubmitFlags flags) {
		return VR_Interface_Call(eye, texture, bounds, flags);
	};

	switch (pTexture->eType)
	{
	case vr::TextureType_DirectX:
		return on_vr_submit_d3d11(pThis, eEye, pTexture, pBounds, static_cast<uint32_t>(eEye), nSubmitFlags, submit_lambda);
	case vr::TextureType_OpenGL:
		return on_vr_submit_opengl(pThis, eEye, pTexture, pBounds, static_cast<uint32_t>(eEye), nSubmitFlags, submit_lambda);
	default:
		assert(false);
		return vr::VRCompositorError_InvalidTexture;
	}
}, vr::EVRCompositorError, vr::EVREye eEye, const vr::Texture_t *pTexture, const vr::VRTextureBounds_t *pBounds, vr::EVRSubmitFlags nSubmitFlags)

VR_Interface_Impl(IVRCompositor, Submit, 5, 012, {
	if (pTexture == nullptr || pTexture->handle == nullptr)
		return vr::VRCompositorError_InvalidTexture;

	const auto submit_lambda = [&](vr::EVREye eye, const vr::Texture_t *texture, const vr::VRTextureBounds_t *bounds, uint32_t, vr::EVRSubmitFlags flags) {
		return VR_Interface_Call(eye, texture, bounds, flags);
	};

	switch (pTexture->eType)
	{
	case vr::TextureType_DirectX:
		return on_vr_submit_d3d11(pThis, eEye, pTexture, pBounds, static_cast<uint32_t>(eEye), nSubmitFlags, submit_lambda);
	case vr::TextureType_DirectX12:
		return on_vr_submit_d3d12(pThis, eEye, pTexture, pBounds, static_cast<uint32_t>(eEye), nSubmitFlags, submit_lambda);
	case vr::TextureType_OpenGL:
		return on_vr_submit_opengl(pThis, eEye, pTexture, pBounds, static_cast<uint32_t>(eEye), nSubmitFlags, submit_lambda);
	case vr::TextureType_Vulkan:
		return on_vr_submit_vulkan(pThis, eEye, pTexture, pBounds, static_cast<uint32_t>(eEye), nSubmitFlags, submit_lambda);
	default:
		assert(false);
		return vr::VRCompositorError_InvalidTexture;
	}
}, vr::EVRCompositorError, vr::EVREye eEye, const vr::Texture_t *pTexture, const vr::VRTextureBounds_t *pBounds, vr::EVRSubmitFlags nSubmitFlags)

VR_Interface_Impl(IVRCompositor, SubmitWithArrayIndex, 6, 028, {
	if (pTexture == nullptr || pTexture->handle == nullptr)
		return vr::VRCompositorError_InvalidTexture;

	const auto submit_lambda = [&](vr::EVREye eye, const vr::Texture_t *texture, const vr::VRTextureBounds_t *bounds, uint32_t layer, vr::EVRSubmitFlags flags) {
		return VR_Interface_Call(eye, texture, layer, bounds, flags);
	};

	switch (pTexture->eType)
	{
	case vr::TextureType_DirectX:
		return on_vr_submit_d3d11(pThis, eEye, pTexture, pBounds, unTextureArrayIndex, nSubmitFlags, submit_lambda);
	case vr::TextureType_DirectX12:
		return on_vr_submit_d3d12(pThis, eEye, pTexture, pBounds, unTextureArrayIndex, nSubmitFlags, submit_lambda);
	case vr::TextureType_OpenGL:
		return on_vr_submit_opengl(pThis, eEye, pTexture, pBounds, unTextureArrayIndex, nSubmitFlags, submit_lambda);
	case vr::TextureType_Vulkan:
		return on_vr_submit_vulkan(pThis, eEye, pTexture, pBounds, unTextureArrayIndex, nSubmitFlags, submit_lambda);
	default:
		assert(false);
		return vr::VRCompositorError_InvalidTexture;
	}
}, vr::EVRCompositorError, vr::EVREye eEye, const vr::Texture_t *pTexture, uint32_t unTextureArrayIndex, const vr::VRTextureBounds_t *pBounds, vr::EVRSubmitFlags nSubmitFlags)

void check_and_init_openvr_hooks()
{
	vr::EVRInitError error_code = vr::VRInitError_None;
	vr::VR_GetGenericInterface(vr::IVRCompositor_Version, &error_code);
}

extern "C" uint32_t VR_CALLTYPE VR_GetInitToken()
{
	static const auto trampoline = reshade::hooks::is_hooked(VR_GetInitToken) ? reshade::hooks::call(VR_GetInitToken) : nullptr;
	return trampoline != nullptr ? trampoline() : vr::VRToken();
}

extern "C" void *   VR_CALLTYPE VR_Init(vr::EVRInitError *peError, vr::EVRApplicationType eApplicationType) // Export used before OpenVR 0.9.17
{
	reshade::log::message(reshade::log::level::info, "Redirecting VR_Init(eApplicationType = %d) ...", static_cast<int>(eApplicationType));

	// Force update of interface pointers
	vr::VRToken() = 0;
	vr::OpenVRInternal_ModuleContext().Clear();

	void *const interface_instance = reshade::hooks::call(VR_Init)(peError, eApplicationType);

	// Force hooking of compositor interface, since 'VR_GetGenericInterface' may not be called by the application
	if (interface_instance != nullptr)
		check_and_init_openvr_hooks();

	return interface_instance;
}

extern "C" uint32_t VR_CALLTYPE VR_InitInternal(vr::EVRInitError *peError, vr::EVRApplicationType eApplicationType) // Export used before OpenVR 1.0.10
{
	reshade::log::message(reshade::log::level::info, "Redirecting VR_InitInternal(eApplicationType = %d) ...", static_cast<int>(eApplicationType));

	vr::OpenVRInternal_ModuleContext().Clear();

	return vr::VRToken() = reshade::hooks::call(VR_InitInternal)(peError, eApplicationType);
}

extern "C" uint32_t VR_CALLTYPE VR_InitInternal2(vr::EVRInitError *peError, vr::EVRApplicationType eApplicationType, const char *pStartupInfo)
{
	reshade::log::message(reshade::log::level::info, "Redirecting VR_InitInternal2(eApplicationType = %d) ...", static_cast<int>(eApplicationType));

	vr::OpenVRInternal_ModuleContext().Clear();

	return vr::VRToken() = reshade::hooks::call(VR_InitInternal2)(peError, eApplicationType, pStartupInfo);
}

extern "C" void     VR_CALLTYPE VR_Shutdown() // Export used before OpenVR 0.9.17
{
	reshade::log::message(reshade::log::level::info, "Redirecting VR_Shutdown() ...");

	delete s_vr_swapchain;
	s_vr_swapchain = nullptr;

	reshade::hooks::call(VR_Shutdown)();
}

extern "C" void     VR_CALLTYPE VR_ShutdownInternal()
{
	reshade::log::message(reshade::log::level::info, "Redirecting VR_ShutdownInternal() ...");

	delete s_vr_swapchain;
	s_vr_swapchain = nullptr;

	reshade::hooks::call(VR_ShutdownInternal)();
}

extern "C" void *   VR_CALLTYPE VR_GetGenericInterface(const char *pchInterfaceVersion, vr::EVRInitError *peError)
{
	if (GetModuleHandleW(L"openvr_api.dll") == nullptr)
	{
		// Fall back to SteamVR client library directly, to support usage with OpenXR
#ifndef _WIN64
		if (const auto vrclient_module = GetModuleHandleW(L"vrclient.dll"))
#else
		if (const auto vrclient_module = GetModuleHandleW(L"vrclient_x64.dll"))
#endif
		{
			if (const auto vrclient_factory = reinterpret_cast<void *(VR_CALLTYPE *)(const char *pInterfaceName, int *pReturnCode)>(GetProcAddress(vrclient_module, "VRClientCoreFactory")))
			{
				if (const auto vrclient_core = static_cast<vr::IVRClientCore *>(vrclient_factory(vr::IVRClientCore_Version, reinterpret_cast<int *>(peError))))
				{
					return vrclient_core->GetGenericInterface(pchInterfaceVersion, peError);
				}
			}
		}

		if (peError != nullptr)
			*peError = vr::VRInitError_Init_NotInitialized;
		return nullptr;
	}

	reshade::log::message(reshade::log::level::info, "Redirecting VR_GetGenericInterface(pchInterfaceVersion = %s) ...", pchInterfaceVersion);

	void *const interface_instance = reshade::hooks::call(VR_GetGenericInterface)(pchInterfaceVersion, peError);

	// Only install hooks once, for the first compositor interface version encountered to avoid duplicated hooks
	// This is necessary because vrclient.dll may create an internal compositor instance with a different version than the application to translate older versions, which with hooks installed for both would cause an infinite loop
	if (static unsigned int compositor_version = 0;
		compositor_version == 0 && interface_instance != nullptr &&
		std::sscanf(pchInterfaceVersion, "IVRCompositor_%u", &compositor_version) != 0)
	{
		// There is a new internal 'IVRCompositor_29' with changed vtable layout, skip this for now
		if (compositor_version > 28)
			compositor_version = 0;

		// The 'IVRCompositor::Submit' function definition has been stable and has had the same virtual function table index since the OpenVR 1.0 release (which was at 'IVRCompositor_015')
		if (compositor_version >= 12)
			reshade::hooks::install("IVRCompositor::Submit", reshade::hooks::vtable_from_instance(static_cast<vr::IVRCompositor *>(interface_instance)), 5, reinterpret_cast<reshade::hook::address>(&IVRCompositor_Submit_012));
		else if (compositor_version >= 9)
			reshade::hooks::install("IVRCompositor::Submit", reshade::hooks::vtable_from_instance(static_cast<vr::IVRCompositor *>(interface_instance)), 4, reinterpret_cast<reshade::hook::address>(&IVRCompositor_Submit_009));
		else if (compositor_version == 8)
			reshade::hooks::install("IVRCompositor::Submit", reshade::hooks::vtable_from_instance(static_cast<vr::IVRCompositor *>(interface_instance)), 6, reinterpret_cast<reshade::hook::address>(&IVRCompositor_Submit_008));
		else if (compositor_version == 7)
			reshade::hooks::install("IVRCompositor::Submit", reshade::hooks::vtable_from_instance(static_cast<vr::IVRCompositor *>(interface_instance)), 6, reinterpret_cast<reshade::hook::address>(&IVRCompositor_Submit_007));

		if (compositor_version == 28)
			reshade::hooks::install("IVRCompositor::SubmitWithArrayIndex", reshade::hooks::vtable_from_instance(static_cast<vr::IVRCompositor *>(interface_instance)), 6, reinterpret_cast<reshade::hook::address>(&IVRCompositor_SubmitWithArrayIndex_028));
	}

	return interface_instance;
}
