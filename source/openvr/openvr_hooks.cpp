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

static vr::EVRCompositorError on_vr_submit_d3d10(vr::IVRCompositor *compositor, vr::EVREye eye, ID3D10Texture2D *texture, vr::EColorSpace color_space, const vr::VRTextureBounds_t *bounds, vr::EVRSubmitFlags flags,
	std::function<vr::EVRCompositorError(vr::EVREye eye, void *texture, const vr::VRTextureBounds_t *bounds, vr::EVRSubmitFlags flags)> submit, D3D10Device *device_proxy)
{
	if (device_proxy == nullptr)
		return submit(eye, texture, bounds, flags); // No proxy device found, so just submit normally
	else if (s_vr_swapchain == nullptr)
		s_vr_swapchain = new reshade::openvr::swapchain_impl(device_proxy, compositor);
	// It is not valid to switch the texture type once submitted for the first time
	else if (s_vr_swapchain->get_device() != device_proxy)
		return vr::VRCompositorError_InvalidTexture;

	if (!s_vr_swapchain->on_vr_submit(device_proxy, eye, { reinterpret_cast<uintptr_t>(texture) }, color_space, bounds, eye))
	{
		// Failed to initialize effect runtime or copy the eye texture, so submit normally without applying effects
#if RESHADE_VERBOSE_LOG
		reshade::log::message(reshade::log::level::error, "Failed to initialize effect runtime or copy the eye texture for eye %d!", static_cast<int>(eye));
#endif
		return submit(eye, texture, bounds, flags);
	}

	// Skip submission of the first eye and instead submit both left and right eye in one step after application submitted both
	if (eye != vr::Eye_Right)
		return vr::VRCompositorError_None;

	const auto target_texture = reinterpret_cast<ID3D10Texture2D *>(s_vr_swapchain->get_back_buffer().handle);

	// The left and right eye were copied side-by-side to a single texture in 'on_vr_submit', so set bounds accordingly
	const vr::VRTextureBounds_t left_bounds = calc_side_by_side_bounds(vr::Eye_Left, bounds);
	submit(vr::Eye_Left, target_texture, &left_bounds, flags);
	const vr::VRTextureBounds_t right_bounds = calc_side_by_side_bounds(vr::Eye_Right, bounds);
	return submit(vr::Eye_Right, target_texture, &right_bounds, flags);
}
static vr::EVRCompositorError on_vr_submit_d3d11(vr::IVRCompositor *compositor, vr::EVREye eye, ID3D11Texture2D *texture, vr::EColorSpace color_space, const vr::VRTextureBounds_t *bounds, vr::EVRSubmitFlags flags,
	std::function<vr::EVRCompositorError(vr::EVREye eye, void *texture, const vr::VRTextureBounds_t *bounds, vr::EVRSubmitFlags flags)> submit)
{
	com_ptr<ID3D11Device> device;
	texture->GetDevice(&device); // 'GetDevice' is at the same virtual function table index for 'ID3D10Texture2D' and 'ID3D11Texture2D', so this happens to work for either case

	// Check if the passed texture is actually a D3D10 texture
	// Cannot just 'QueryInterface' with 'ID3D10Texture2D', since that is supported on D3D11 textures too for some reason, so instead check the device
	if (com_ptr<ID3D10Device> device10;
		device->QueryInterface(&device10) == S_OK)
	{
		// Also check that the device has a proxy 'D3D10Device' interface, otherwise it is likely still a D3D11 device exposing the 'ID3D10Device' interface, after 'ID3D11Device::CreateDeviceContextState' was called
		if (const auto device10_proxy = get_private_pointer_d3dx<D3D10Device>(device10.get()))
		{
			// Whoops, this is actually a D3D10 texture, redirect ...
			return on_vr_submit_d3d10(compositor, eye, reinterpret_cast<ID3D10Texture2D *>(texture), color_space, bounds, flags, submit, device10_proxy);
		}
	}

	const auto device_proxy = get_private_pointer_d3dx<D3D11Device>(device.get());
	if (device_proxy == nullptr)
		return submit(eye, texture, bounds, flags); // No proxy device found, so just submit normally
	else if (s_vr_swapchain == nullptr)
		s_vr_swapchain = new reshade::openvr::swapchain_impl(device_proxy, compositor);
	// It is not valid to switch the texture type once submitted for the first time
	else if (s_vr_swapchain->get_device() != device_proxy)
		return vr::VRCompositorError_InvalidTexture;

	if (!s_vr_swapchain->on_vr_submit(device_proxy->_immediate_context, eye, { reinterpret_cast<uintptr_t>(texture) }, color_space, bounds, eye))
	{
		// Failed to initialize effect runtime or copy the eye texture, so submit normally without applying effects
#if RESHADE_VERBOSE_LOG
		reshade::log::message(reshade::log::level::error, "Failed to initialize effect runtime or copy the eye texture for eye %d!", static_cast<int>(eye));
#endif
		return submit(eye, texture, bounds, flags);
	}

	// Skip submission of the first eye and instead submit both left and right eye in one step after application submitted both
	if (eye != vr::Eye_Right)
		return vr::VRCompositorError_None;

	const auto target_texture = reinterpret_cast<ID3D11Texture2D *>(s_vr_swapchain->get_back_buffer().handle);

	// The left and right eye were copied side-by-side to a single texture in 'on_vr_submit', so set bounds accordingly
	const vr::VRTextureBounds_t left_bounds = calc_side_by_side_bounds(vr::Eye_Left, bounds);
	submit(vr::Eye_Left, target_texture, &left_bounds, flags);
	const vr::VRTextureBounds_t right_bounds = calc_side_by_side_bounds(vr::Eye_Right, bounds);
	return submit(vr::Eye_Right, target_texture, &right_bounds, flags);
}
static vr::EVRCompositorError on_vr_submit_d3d12(vr::IVRCompositor *compositor, vr::EVREye eye, const vr::D3D12TextureData_t *texture, vr::EColorSpace color_space, const vr::VRTextureBounds_t *bounds, vr::EVRSubmitFlags flags,
	std::function<vr::EVRCompositorError(vr::EVREye eye, void *texture, const vr::VRTextureBounds_t *bounds, vr::EVRSubmitFlags flags)> submit)
{
	com_ptr<D3D12CommandQueue> command_queue_proxy;
	if (FAILED(texture->m_pCommandQueue->QueryInterface(IID_PPV_ARGS(&command_queue_proxy))))
		return submit(eye, (void *)texture, bounds, flags); // No proxy command queue found, so just submit normally
	else if (s_vr_swapchain == nullptr)
		s_vr_swapchain = new reshade::openvr::swapchain_impl(command_queue_proxy.get(), compositor);
	else if (s_vr_swapchain->get_device() != command_queue_proxy->get_device())
		return vr::VRCompositorError_InvalidTexture;

	// Synchronize access to the command queue while events are invoked and the immediate command list may be accessed
	std::unique_lock<std::shared_mutex> lock(command_queue_proxy->_mutex);

	// Resource should be in D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE state at this point
	if (!s_vr_swapchain->on_vr_submit(command_queue_proxy.get(), eye, { reinterpret_cast<uintptr_t>(texture->m_pResource) }, color_space, bounds, eye))
	{
		// Failed to initialize effect runtime or copy the eye texture, so submit normally without applying effects
#if RESHADE_VERBOSE_LOG
		reshade::log::message(reshade::log::level::error, "Failed to initialize effect runtime or copy the eye texture for eye %d!", static_cast<int>(eye));
#endif
		return submit(eye, (void *)texture, bounds, flags);
	}

	// Skip submission of the first eye and instead submit both left and right eye in one step after application submitted both
	if (eye != vr::Eye_Right)
		return vr::VRCompositorError_None;

	command_queue_proxy->flush_immediate_command_list();

	lock.unlock();

	vr::D3D12TextureData_t target_texture = *texture;
	target_texture.m_pResource = reinterpret_cast<ID3D12Resource *>(s_vr_swapchain->get_back_buffer().handle);

	const vr::VRTextureBounds_t left_bounds = calc_side_by_side_bounds(vr::Eye_Left, bounds);
	submit(vr::Eye_Left, &target_texture, &left_bounds, flags);
	const vr::VRTextureBounds_t right_bounds = calc_side_by_side_bounds(vr::Eye_Right, bounds);
	return submit(vr::Eye_Right, &target_texture, &right_bounds, flags);
}
static vr::EVRCompositorError on_vr_submit_opengl(vr::IVRCompositor *compositor, vr::EVREye eye, GLuint object, vr::EColorSpace color_space, const vr::VRTextureBounds_t *bounds, vr::EVRSubmitFlags flags,
	std::function<vr::EVRCompositorError(vr::EVREye eye, void *texture, const vr::VRTextureBounds_t *bounds, vr::EVRSubmitFlags flags)> submit)
{
	extern thread_local reshade::opengl::device_context_impl *g_current_context;

	if (g_current_context == nullptr)
		return submit(eye, reinterpret_cast<void *>(static_cast<uintptr_t>(object)), bounds, flags);
	else if (s_vr_swapchain == nullptr)
		s_vr_swapchain = new reshade::openvr::swapchain_impl(g_current_context->get_device(), g_current_context, compositor);
	else if (s_vr_swapchain->get_device() != g_current_context->get_device())
		return vr::VRCompositorError_InvalidTexture;

	const reshade::api::resource eye_texture = reshade::opengl::make_resource_handle(
		(flags & vr::Submit_GlRenderBuffer) != 0 ? GL_RENDERBUFFER : ((flags & vr::Submit_GlArrayTexture) != 0 ? GL_TEXTURE_2D_ARRAY : GL_TEXTURE_2D), object);

	if (!s_vr_swapchain->on_vr_submit(g_current_context, eye, eye_texture, color_space, bounds, eye))
	{
		// Failed to initialize effect runtime or copy the eye texture, so submit normally without applying effects
#if RESHADE_VERBOSE_LOG
		reshade::log::message(reshade::log::level::error, "Failed to initialize effect runtime or copy the eye texture for eye %d!", static_cast<int>(eye));
#endif
		return submit(eye, reinterpret_cast<void *>(static_cast<uintptr_t>(object)), bounds, flags);
	}

	// Skip submission of the first eye and instead submit both left and right eye in one step after application submitted both
	if (eye != vr::Eye_Right)
		return vr::VRCompositorError_None;

	const GLuint target_texture = s_vr_swapchain->get_back_buffer().handle & 0xFFFFFFFF;

	// Target object created in 'on_vr_submit' is a 2D texture
	flags = static_cast<vr::EVRSubmitFlags>(flags & ~(vr::Submit_GlRenderBuffer | vr::Submit_GlArrayTexture));

	const vr::VRTextureBounds_t left_bounds = calc_side_by_side_bounds(vr::Eye_Left, bounds);
	submit(vr::Eye_Left, reinterpret_cast<void *>(static_cast<uintptr_t>(target_texture)), &left_bounds, flags);
	const vr::VRTextureBounds_t right_bounds = calc_side_by_side_bounds(vr::Eye_Right, bounds);
	return submit(vr::Eye_Right, reinterpret_cast<void *>(static_cast<uintptr_t>(target_texture)), &right_bounds, flags);
}
static vr::EVRCompositorError on_vr_submit_vulkan(vr::IVRCompositor *compositor, vr::EVREye eye, const vr::VRVulkanTextureData_t *texture, vr::EColorSpace color_space, const vr::VRTextureBounds_t *bounds, vr::EVRSubmitFlags flags,
	std::function<vr::EVRCompositorError(vr::EVREye eye, void *texture, const vr::VRTextureBounds_t *bounds, vr::EVRSubmitFlags flags)> submit)
{
	extern lockfree_linear_map<void *, reshade::vulkan::device_impl *, 8> g_vulkan_devices;

	reshade::vulkan::device_impl *device = g_vulkan_devices.at(dispatch_key_from_handle(texture->m_pDevice));
	if (device == nullptr)
		return submit(eye, (void *)texture, bounds, flags);

	reshade::vulkan::command_queue_impl *queue = nullptr;
	if (const auto queue_it = std::find_if(device->_queues.cbegin(), device->_queues.cend(),
			[texture](reshade::vulkan::command_queue_impl *queue) { return queue->_orig == texture->m_pQueue; });
		queue_it != device->_queues.cend())
		queue = *queue_it;
	else
		return submit(eye, (void *)texture, bounds, flags);

	if (s_vr_swapchain == nullptr)
		// OpenVR requires the passed in queue to be a graphics queue, so can safely use it
		s_vr_swapchain = new reshade::openvr::swapchain_impl(device, queue, compositor);
	else if (s_vr_swapchain->get_device() != device)
		return vr::VRCompositorError_InvalidTexture;

	// Image should be in VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL layout at this point
	if (!s_vr_swapchain->on_vr_submit(queue, eye, { (uint64_t)(VkImage)texture->m_nImage }, color_space, bounds, (flags & vr::Submit_VulkanTextureWithArrayData) != 0 ? static_cast<const vr::VRVulkanTextureArrayData_t *>(texture)->m_unArrayIndex : 0))
	{
		// Failed to initialize effect runtime or copy the eye texture, so submit normally without applying effects
#if RESHADE_VERBOSE_LOG
		reshade::log::message(reshade::log::level::error, "Failed to initialize effect runtime or copy the eye texture for eye %d!", static_cast<int>(eye));
#endif
		return submit(eye, (void *)texture, bounds, flags);
	}

	// Skip submission of the first eye and instead submit both left and right eye in one step after application submitted both
	if (eye != vr::Eye_Right)
		return vr::VRCompositorError_None;

	queue->flush_immediate_command_list();

	const reshade::api::resource target = s_vr_swapchain->get_current_back_buffer();
	const reshade::api::resource_desc target_desc = device->get_resource_desc(target);

	vr::VRVulkanTextureData_t target_texture = *texture;
	target_texture.m_nImage = (uint64_t)(VkImage)target.handle;
	target_texture.m_nWidth = target_desc.texture.width;
	target_texture.m_nHeight = target_desc.texture.height;
	// Multisampled source textures were already resolved, so sample count is always one at this point
	target_texture.m_nSampleCount = target_desc.texture.samples;
	// The side-by-side texture is not an array texture
	flags = static_cast<vr::EVRSubmitFlags>(flags & ~vr::Submit_VulkanTextureWithArrayData);

	const vr::VRTextureBounds_t left_bounds = calc_side_by_side_bounds(vr::Eye_Left, bounds);
	submit(vr::Eye_Left, &target_texture, &left_bounds, flags);
	const vr::VRTextureBounds_t right_bounds = calc_side_by_side_bounds(vr::Eye_Right, bounds);
	return submit(vr::Eye_Right, &target_texture, &right_bounds, flags);
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

	const auto submit_lambda = [&](vr::EVREye eye, void *handle, const vr::VRTextureBounds_t *bounds, vr::EVRSubmitFlags flags) {
		assert(flags == vr::Submit_Default);
		return VR_Interface_Call(eye, eTextureType, handle, bounds);
	};

	switch (eTextureType)
	{
	case vr::TextureType_DirectX: // API_DirectX
		return on_vr_submit_d3d11(pThis, eEye, static_cast<ID3D11Texture2D *>(pTexture), vr::ColorSpace_Auto, pBounds, vr::Submit_Default, submit_lambda);
	case vr::TextureType_OpenGL:  // API_OpenGL
		return on_vr_submit_opengl(pThis, eEye, static_cast<GLuint>(reinterpret_cast<uintptr_t>(pTexture)), vr::ColorSpace_Auto, pBounds, vr::Submit_Default, submit_lambda);
	default:
		return vr::VRCompositorError_InvalidTexture;
	}
}, vr::EVRCompositorError, /* vr::Hmd_Eye */ vr::EVREye eEye, /* vr::GraphicsAPIConvention */ unsigned int eTextureType, void *pTexture, const vr::VRTextureBounds_t *pBounds)

VR_Interface_Impl(IVRCompositor, Submit, 6, 008, {
	if (pTexture == nullptr)
		return vr::VRCompositorError_InvalidTexture;

	const auto submit_lambda = [&](vr::EVREye eye, void *handle, const vr::VRTextureBounds_t *bounds, vr::EVRSubmitFlags flags) {
		return VR_Interface_Call(eye, eTextureType, handle, bounds, flags);
	};

	switch (eTextureType)
	{
	case vr::TextureType_DirectX: // API_DirectX
		return on_vr_submit_d3d11(pThis, eEye, static_cast<ID3D11Texture2D *>(pTexture), vr::ColorSpace_Auto, pBounds, nSubmitFlags, submit_lambda);
	case vr::TextureType_OpenGL:  // API_OpenGL
		return on_vr_submit_opengl(pThis, eEye, static_cast<GLuint>(reinterpret_cast<uintptr_t>(pTexture)), vr::ColorSpace_Auto, pBounds, nSubmitFlags, submit_lambda);
	default:
		return vr::VRCompositorError_InvalidTexture;
	}
}, vr::EVRCompositorError, /* vr::Hmd_Eye */ vr::EVREye eEye, /* vr::GraphicsAPIConvention */ unsigned int eTextureType, void *pTexture, const vr::VRTextureBounds_t *pBounds, /* vr::VRSubmitFlags_t */ vr::EVRSubmitFlags nSubmitFlags)

VR_Interface_Impl(IVRCompositor, Submit, 4, 009, {
	if (pTexture == nullptr || pTexture->handle == nullptr)
		return vr::VRCompositorError_InvalidTexture;

	const auto submit_lambda = [&](vr::EVREye eye, void *handle, const vr::VRTextureBounds_t *bounds, vr::EVRSubmitFlags flags) {
		// The 'vr::Submit_TextureWithPose' and 'vr::Submit_TextureWithDepth' flags did not exist in this OpenVR version yet, so can keep it simple
		vr::Texture_t texture;
		texture.handle = handle;
		texture.eType = pTexture->eType;
		texture.eColorSpace = pTexture->eColorSpace;
		return VR_Interface_Call(eye, &texture, bounds, flags);
	};

	switch (pTexture->eType)
	{
	case vr::TextureType_DirectX:
		return on_vr_submit_d3d11(pThis, eEye, static_cast<ID3D11Texture2D *>(pTexture->handle), pTexture->eColorSpace, pBounds, nSubmitFlags, submit_lambda);
	case vr::TextureType_OpenGL:
		return on_vr_submit_opengl(pThis, eEye, static_cast<GLuint>(reinterpret_cast<uintptr_t>(pTexture->handle)), pTexture->eColorSpace, pBounds, nSubmitFlags, submit_lambda);
	default:
		return vr::VRCompositorError_InvalidTexture;
	}
}, vr::EVRCompositorError, vr::EVREye eEye, const vr::Texture_t *pTexture, const vr::VRTextureBounds_t *pBounds, vr::EVRSubmitFlags nSubmitFlags)

VR_Interface_Impl(IVRCompositor, Submit, 5, 012, {
	if (pTexture == nullptr || pTexture->handle == nullptr)
		return vr::VRCompositorError_InvalidTexture;

	// Keep track of pose and depth information of both eyes, so that it can all be send in one step after application submitted both
	static vr::VRTextureWithPoseAndDepth_t s_last_texture[2];
	switch (nSubmitFlags & (vr::Submit_TextureWithPose | vr::Submit_TextureWithDepth))
	{
	case 0:
		std::memcpy(&s_last_texture[eEye], pTexture, sizeof(vr::Texture_t));
		break;
	case vr::Submit_TextureWithPose:
		std::memcpy(&s_last_texture[eEye], pTexture, sizeof(vr::VRTextureWithPose_t));
		break;
	case vr::Submit_TextureWithDepth:
		// This is not technically compatible with 'vr::VRTextureWithPoseAndDepth_t', but that is fine, since it's only used for storage and none of the members are accessed directly
		// TODO: The depth texture bounds may be different then the side-by-side bounds which are used for submission
		std::memcpy(&s_last_texture[eEye], pTexture, sizeof(vr::VRTextureWithDepth_t));
		break;
	case vr::Submit_TextureWithPose | vr::Submit_TextureWithDepth:
		std::memcpy(&s_last_texture[eEye], pTexture, sizeof(vr::VRTextureWithPoseAndDepth_t));
		break;
	}

	const auto submit_lambda = [&](vr::EVREye eye, void *handle, const vr::VRTextureBounds_t *bounds, vr::EVRSubmitFlags flags) {
		// Use the pose and/or depth information that was previously stored during submission, but overwrite the texture handle
		vr::VRTextureWithPoseAndDepth_t texture = s_last_texture[eye];
		texture.handle = handle;
		return VR_Interface_Call(eye, &texture, bounds, flags);
	};

	switch (pTexture->eType)
	{
	case vr::TextureType_DirectX:
		return on_vr_submit_d3d11(pThis, eEye, static_cast<ID3D11Texture2D *>(pTexture->handle), pTexture->eColorSpace, pBounds, nSubmitFlags, submit_lambda);
	case vr::TextureType_DirectX12:
		return on_vr_submit_d3d12(pThis, eEye, static_cast<const vr::D3D12TextureData_t *>(pTexture->handle), pTexture->eColorSpace, pBounds, nSubmitFlags, submit_lambda);
	case vr::TextureType_OpenGL:
		return on_vr_submit_opengl(pThis, eEye, static_cast<GLuint>(reinterpret_cast<uintptr_t>(pTexture->handle)), pTexture->eColorSpace, pBounds, nSubmitFlags, submit_lambda);
	case vr::TextureType_Vulkan:
		return on_vr_submit_vulkan(pThis, eEye, static_cast<const vr::VRVulkanTextureData_t *>(pTexture->handle), pTexture->eColorSpace, pBounds, nSubmitFlags, submit_lambda);
	default:
		return vr::VRCompositorError_InvalidTexture;
	}
}, vr::EVRCompositorError, vr::EVREye eEye, const vr::Texture_t *pTexture, const vr::VRTextureBounds_t *pBounds, vr::EVRSubmitFlags nSubmitFlags)

VR_Interface_Impl(IVRClientCore, Cleanup, 1, 001, {
	reshade::log::message(reshade::log::level::info, "Redirecting IVRClientCore::Cleanup(this = %p) ...", pThis);

	delete s_vr_swapchain;
	s_vr_swapchain = nullptr;

	VR_Interface_Call();
}, void)

VR_Interface_Impl(IVRClientCore, GetGenericInterface, 3, 001, {
	assert(pchNameAndVersion != nullptr);

	reshade::log::message(reshade::log::level::info, "Redirecting IVRClientCore::GetGenericInterface(this = %p, pchNameAndVersion = %s) ...", pThis, pchNameAndVersion);

	void *const interface_instance = VR_Interface_Call(pchNameAndVersion, peError);

	// Only install hooks once, for the first compositor interface version encountered to avoid duplicated hooks
	// This is necessary because vrclient.dll may create an internal compositor instance with a different version than the application to translate older versions, which with hooks installed for both would cause an infinite loop
	if (static unsigned int compositor_version = 0;
		compositor_version == 0 && interface_instance != nullptr && std::sscanf(pchNameAndVersion, "IVRCompositor_%u", &compositor_version))
	{
		// The 'IVRCompositor::Submit' function definition has been stable and has had the same virtual function table index since the OpenVR 1.0 release (which was at 'IVRCompositor_015')
		if (compositor_version >= 12)
			reshade::hooks::install("IVRCompositor::Submit", reshade::hooks::vtable_from_instance(static_cast<vr::IVRCompositor *>(interface_instance)), 5, reinterpret_cast<reshade::hook::address>(&IVRCompositor_Submit_012));
		else if (compositor_version >= 9)
			reshade::hooks::install("IVRCompositor::Submit", reshade::hooks::vtable_from_instance(static_cast<vr::IVRCompositor *>(interface_instance)), 4, reinterpret_cast<reshade::hook::address>(&IVRCompositor_Submit_009));
		else if (compositor_version == 8)
			reshade::hooks::install("IVRCompositor::Submit", reshade::hooks::vtable_from_instance(static_cast<vr::IVRCompositor *>(interface_instance)), 6, reinterpret_cast<reshade::hook::address>(&IVRCompositor_Submit_008));
		else if (compositor_version == 7)
			reshade::hooks::install("IVRCompositor::Submit", reshade::hooks::vtable_from_instance(static_cast<vr::IVRCompositor *>(interface_instance)), 6, reinterpret_cast<reshade::hook::address>(&IVRCompositor_Submit_007));
	}

	return interface_instance;
}, void *, const char *pchNameAndVersion, vr::EVRInitError *peError)

vr::IVRClientCore *g_client_core = nullptr;

extern "C" void *VR_CALLTYPE VRClientCoreFactory(const char *pInterfaceName, int *pReturnCode)
{
	assert(pInterfaceName != nullptr);

	reshade::log::message(reshade::log::level::info, "Redirecting VRClientCoreFactory(pInterfaceName = %s) ...", pInterfaceName);

	void *const interface_instance = reshade::hooks::call(VRClientCoreFactory)(pInterfaceName, pReturnCode);

	if (static unsigned int client_core_version = 0;
		client_core_version == 0 && interface_instance != nullptr && std::sscanf(pInterfaceName, "IVRClientCore_%u", &client_core_version))
	{
		g_client_core = static_cast<vr::IVRClientCore *>(interface_instance);

		// The 'IVRClientCore::Cleanup' and 'IVRClientCore::GetGenericInterface' functions did not change between 'IVRClientCore_001' and 'IVRClientCore_003'
		reshade::hooks::install("IVRClientCore::Cleanup", reshade::hooks::vtable_from_instance(static_cast<vr::IVRClientCore *>(interface_instance)), 1, reinterpret_cast<reshade::hook::address>(&IVRClientCore_Cleanup_001));
		reshade::hooks::install("IVRClientCore::GetGenericInterface", reshade::hooks::vtable_from_instance(static_cast<vr::IVRClientCore *>(interface_instance)), 3, reinterpret_cast<reshade::hook::address>(&IVRClientCore_GetGenericInterface_001));
	}

	return interface_instance;
}

void check_and_init_openvr_hooks()
{
	if (g_client_core != nullptr ||
#ifndef _WIN64
		GetModuleHandleW(L"vrclient.dll") == nullptr)
#else
		GetModuleHandleW(L"vrclient_x64.dll") == nullptr)
#endif
		return;

	vr::EVRInitError error_code = vr::VRInitError_None;
	VRClientCoreFactory(vr::IVRClientCore_Version, reinterpret_cast<int *>(&error_code));
	if (g_client_core != nullptr)
		g_client_core->GetGenericInterface(vr::IVRCompositor_Version, &error_code);
}
