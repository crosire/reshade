/*
 * Copyright (C) 2021 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "dll_log.hpp"
#include "hook_manager.hpp"
#include "lockfree_table.hpp"
#include "d3d11/d3d11_device.hpp"
#include "d3d11/d3d11_device_context.hpp"
#include "d3d11/reshade_api_swapchain.hpp"
#include "d3d12/d3d12_device.hpp"
#include "d3d12/d3d12_command_queue.hpp"
#include "d3d12/reshade_api_swapchain.hpp"
#include "opengl/opengl_hooks.hpp"
#include "opengl/reshade_api_swapchain.hpp"
#include "vulkan/vulkan_hooks.hpp"
#include "vulkan/reshade_api_device.hpp"
#include "vulkan/reshade_api_command_queue.hpp"
#include "vulkan/reshade_api_swapchain.hpp"
#include <openvr.h>
#include <ivrclientcore.h>

// There can only be a single global effect runtime in OpenVR (since its API is based on singletons)
static std::pair<reshade::api::swapchain *, vr::ETextureType> s_vr_swapchain = { nullptr, vr::TextureType_Invalid };

static inline vr::VRTextureBounds_t calc_side_by_side_bounds(vr::EVREye eye, const vr::VRTextureBounds_t *orig_bounds)
{
	vr::VRTextureBounds_t bounds = (eye != vr::Eye_Right) ?
		vr::VRTextureBounds_t { 0.0f, 0.0f, 0.5f, 1.0f } : // Left half of the texture
		vr::VRTextureBounds_t { 0.5f, 0.0f, 1.0f, 1.0f };  // Right half of the texture
	if (orig_bounds != nullptr && orig_bounds->uMin > orig_bounds->uMax)
		std::swap(bounds.uMin, bounds.uMax);
	if (orig_bounds != nullptr && orig_bounds->vMin > orig_bounds->vMax)
		std::swap(bounds.vMin, bounds.vMax);
	return bounds;
}

static vr::EVRCompositorError on_submit_d3d11(vr::EVREye eye, ID3D11Texture2D *texture, const vr::VRTextureBounds_t *bounds, vr::EVRSubmitFlags flags,
	std::function<vr::EVRCompositorError(vr::EVREye eye, void *texture, const vr::VRTextureBounds_t *bounds, vr::EVRSubmitFlags flags)> submit)
{
	com_ptr<ID3D11Device> device;
	D3D11Device *device_proxy = nullptr; // Was set via 'SetPrivateData', so do not use a 'com_ptr' here, since 'GetPrivateData' will not add a reference
	texture->GetDevice(&device);
	if (UINT data_size = sizeof(device_proxy);
		FAILED(device->GetPrivateData(__uuidof(D3D11Device), &data_size, reinterpret_cast<void *>(&device_proxy))))
		goto normal_submit; // No proxy device found, so just submit normally

	if (s_vr_swapchain.first == nullptr)
	{
		s_vr_swapchain = { new reshade::d3d11::swapchain_impl(device_proxy, device_proxy->_immediate_context, nullptr), vr::TextureType_DirectX };
	}

	// It is not valid to switch the texture type once submitted for the first time
	if (s_vr_swapchain.second != vr::TextureType_DirectX)
		return vr::VRCompositorError_InvalidTexture;

	ID3D11Texture2D *target_texture = nullptr;

	const auto runtime = static_cast<reshade::d3d11::swapchain_impl *>(s_vr_swapchain.first);
	// Copy current eye texture to single side-by-side texture for use by the effect runtime
	if (!runtime->on_layer_submit(
		static_cast<UINT>(eye),
		texture,
		reinterpret_cast<const float *>(bounds),
		&target_texture))
	{
	normal_submit:
		// Failed to initialize effect runtime or copy the eye texture, so submit normally without applying effects
		return submit(eye, texture, bounds, flags);
	}

	// Skip submission of the first eye and instead submit both left and right eye in one step after application submitted both
	if (eye != vr::Eye_Right)
	{
		return vr::VRCompositorError_None;
	}
	else
	{
#if RESHADE_ADDON
		reshade::invoke_addon_event<reshade::addon_event::present>(device_proxy->_immediate_context, runtime);
#endif

		runtime->on_present();

		// The left and right eye were copied side-by-side to a single texture in 'on_layer_submit', so set bounds accordingly
		const vr::VRTextureBounds_t left_bounds = calc_side_by_side_bounds(vr::Eye_Left, bounds);
		submit(vr::Eye_Left, target_texture, &left_bounds, flags);
		const vr::VRTextureBounds_t right_bounds = calc_side_by_side_bounds(vr::Eye_Right, bounds);
		return submit(vr::Eye_Right, target_texture, &right_bounds, flags);
	}
}
static vr::EVRCompositorError on_submit_d3d12(vr::EVREye eye, const vr::D3D12TextureData_t *texture, const vr::VRTextureBounds_t *bounds, vr::EVRSubmitFlags flags,
	std::function<vr::EVRCompositorError(vr::EVREye eye, void *texture, const vr::VRTextureBounds_t *bounds, vr::EVRSubmitFlags flags)> submit)
{
	com_ptr<D3D12CommandQueue> command_queue_proxy;
	if (FAILED(texture->m_pCommandQueue->QueryInterface(IID_PPV_ARGS(&command_queue_proxy))))
		goto normal_submit; // No proxy command queue found, so just submit normally

	if (s_vr_swapchain.first == nullptr)
	{
		s_vr_swapchain = { new reshade::d3d12::swapchain_impl(command_queue_proxy->_device, command_queue_proxy.get(), nullptr), vr::TextureType_DirectX12 };
	}

	if (s_vr_swapchain.second != vr::TextureType_DirectX12)
		return vr::VRCompositorError_InvalidTexture;

	vr::D3D12TextureData_t target_texture = *texture;

	const auto runtime = static_cast<reshade::d3d12::swapchain_impl *>(s_vr_swapchain.first);
	// Copy current eye texture to single side-by-side texture for use by the effect runtime
	if (!runtime->on_layer_submit(
		static_cast<UINT>(eye),
		texture->m_pResource, // Resource should be in D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE state at this point
		reinterpret_cast<const float *>(bounds),
		&target_texture.m_pResource))
	{
	normal_submit:
		// Failed to initialize effect runtime or copy the eye texture, so submit normally without applying effects
		return submit(eye, (void *)texture, bounds, flags);
	}

	// Skip submission of the first eye and instead submit both left and right eye in one step after application submitted both
	if (eye != vr::Eye_Right)
	{
		return vr::VRCompositorError_None;
	}
	else
	{
#if RESHADE_ADDON
		reshade::invoke_addon_event<reshade::addon_event::present>(command_queue_proxy.get(), runtime);
#endif

		runtime->on_present();

		command_queue_proxy->flush_immediate_command_list();

		const vr::VRTextureBounds_t left_bounds = calc_side_by_side_bounds(vr::Eye_Left, bounds);
		submit(vr::Eye_Left, &target_texture, &left_bounds, flags);
		const vr::VRTextureBounds_t right_bounds = calc_side_by_side_bounds(vr::Eye_Right, bounds);
		return submit(vr::Eye_Right, &target_texture, &right_bounds, flags);
	}
}
static vr::EVRCompositorError on_submit_opengl(vr::EVREye eye, GLuint object, const vr::VRTextureBounds_t *bounds, vr::EVRSubmitFlags flags,
	std::function<vr::EVRCompositorError(vr::EVREye eye, void *texture, const vr::VRTextureBounds_t *bounds, vr::EVRSubmitFlags flags)> submit)
{
	if (s_vr_swapchain.first == nullptr)
	{
		const HDC device_context = wglGetCurrentDC();
		const HGLRC render_context = wglGetCurrentContext();

		s_vr_swapchain = { new reshade::opengl::swapchain_impl(device_context, render_context), vr::TextureType_OpenGL };
	}

	if (s_vr_swapchain.second != vr::TextureType_OpenGL)
		return vr::VRCompositorError_InvalidTexture;

	GLuint target_rbo = 0;

	const auto runtime = static_cast<reshade::opengl::swapchain_impl *>(s_vr_swapchain.first);
	// Copy current eye texture to single side-by-side texture for use by the effect runtime
	if (!runtime->on_layer_submit(
		static_cast<uint32_t>(eye),
		object,
		(flags & vr::Submit_GlRenderBuffer) != 0,
		(flags & vr::Submit_GlArrayTexture) != 0,
		reinterpret_cast<const float *>(bounds),
		&target_rbo))
	{
		// Failed to initialize effect runtime or copy the eye texture, so submit normally without applying effects
		return submit(eye, reinterpret_cast<void *>(static_cast<uintptr_t>(object)), bounds, flags);
	}

	// Skip submission of the first eye and instead submit both left and right eye in one step after application submitted both
	if (eye != vr::Eye_Right)
	{
		return vr::VRCompositorError_None;
	}
	else
	{
#if RESHADE_ADDON
		reshade::invoke_addon_event<reshade::addon_event::present>(runtime, runtime);
#endif

		// Skip copy, data was already copied in 'on_layer_submit' above
		runtime->on_present(false);

		// Target object created in 'on_layer_submit' is a RBO, not a texture or array texture
		flags = static_cast<vr::EVRSubmitFlags>((flags & ~vr::Submit_GlArrayTexture) | vr::Submit_GlRenderBuffer);

		const vr::VRTextureBounds_t left_bounds = calc_side_by_side_bounds(vr::Eye_Left, bounds);
		submit(vr::Eye_Left, reinterpret_cast<void *>(static_cast<uintptr_t>(target_rbo)), &left_bounds, flags);
		const vr::VRTextureBounds_t right_bounds = calc_side_by_side_bounds(vr::Eye_Right, bounds);
		return submit(vr::Eye_Right, reinterpret_cast<void *>(static_cast<uintptr_t>(target_rbo)), &right_bounds, flags);
	}
}
static vr::EVRCompositorError on_submit_vulkan(vr::EVREye eye, const vr::VRVulkanTextureData_t *texture, const vr::VRTextureBounds_t *bounds, vr::EVRSubmitFlags flags,
	std::function<vr::EVRCompositorError(vr::EVREye eye, void *texture, const vr::VRTextureBounds_t *bounds, vr::EVRSubmitFlags flags)> submit)
{
	extern lockfree_table<void *, reshade::vulkan::device_impl *, 16> g_vulkan_devices;
	reshade::vulkan::device_impl *const device = g_vulkan_devices.at(dispatch_key_from_handle(texture->m_pDevice));
	reshade::vulkan::command_queue_impl *queue = nullptr;
	if (device == nullptr)
		goto normal_submit;

	if (const auto queue_it = std::find_if(device->_queues.begin(), device->_queues.end(),
		[texture](reshade::vulkan::command_queue_impl *queue) { return queue->_orig == texture->m_pQueue; });
		queue_it != device->_queues.end())
		queue = *queue_it;
	else
		goto normal_submit;

	if (s_vr_swapchain.first == nullptr)
	{
		// OpenVR requires the passed in queue to be a graphics queue, so can safely use it
		s_vr_swapchain = { new reshade::vulkan::swapchain_impl(device, queue), vr::TextureType_Vulkan };
	}

	if (s_vr_swapchain.second != vr::TextureType_Vulkan)
		return vr::VRCompositorError_InvalidTexture;

	VkImage target_image = VK_NULL_HANDLE;

	const auto runtime = static_cast<reshade::vulkan::swapchain_impl *>(s_vr_swapchain.first);
	// Copy current eye texture to single side-by-side texture for use by the effect runtime
	if (!runtime->on_layer_submit(
		static_cast<uint32_t>(eye),
		(VkImage)texture->m_nImage, // Image should be in VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL layout at this point
		VkExtent2D { texture->m_nWidth, texture->m_nHeight },
		static_cast<VkFormat>(texture->m_nFormat),
		static_cast<VkSampleCountFlags>(texture->m_nSampleCount),
		(flags & vr::Submit_VulkanTextureWithArrayData) != 0 ? static_cast<const vr::VRVulkanTextureArrayData_t *>(texture)->m_unArrayIndex : 0,
		reinterpret_cast<const float *>(bounds),
		&target_image))
	{
	normal_submit:
		// Failed to initialize effect runtime or copy the eye texture, so submit normally without applying effects
		return submit(eye, (void *)texture, bounds, flags);
	}

	// Skip submission of the first eye and instead submit both left and right eye in one step after application submitted both
	if (eye != vr::Eye_Right)
	{
		return vr::VRCompositorError_None;
	}
	else
	{
#if RESHADE_ADDON
		reshade::invoke_addon_event<reshade::addon_event::present>(queue, runtime);
#endif

		std::vector<VkSemaphore> wait_semaphores;
		runtime->on_present(texture->m_pQueue, 0, wait_semaphores);

		queue->flush_immediate_command_list();

		auto target_texture = *texture;
		target_texture.m_nImage = (uint64_t)target_image;
		// Multisampled source textures were already resolved in 'on_layer_submit', so sample count is always one at this point
		target_texture.m_nSampleCount = 1;
		runtime->get_frame_width_and_height(&target_texture.m_nWidth, &target_texture.m_nHeight);
		// Target texture created in 'on_layer_submit' is not an array texture
		flags = static_cast<vr::EVRSubmitFlags>(flags & ~vr::Submit_VulkanTextureWithArrayData);

		const vr::VRTextureBounds_t left_bounds = calc_side_by_side_bounds(vr::Eye_Left, bounds);
		submit(vr::Eye_Left, &target_texture, &left_bounds, flags);
		const vr::VRTextureBounds_t right_bounds = calc_side_by_side_bounds(vr::Eye_Right, bounds);
		return submit(vr::Eye_Right, &target_texture, &right_bounds, flags);
	}
}

#ifdef WIN64
	#define VR_Interface_Impl(type, method_name, vtable_offset, interface_version, impl, return_type, ...) \
		static return_type type##_##method_name##_##interface_version(vr::type *pThis, ##__VA_ARGS__) \
		{ \
			static const auto trampoline = reshade::hooks::call(type##_##method_name##_##interface_version, vtable_from_instance(pThis) + vtable_offset); \
			impl \
		}
	#define VR_Interface_Call(...) trampoline(pThis, ##__VA_ARGS__)
#else
	// The OpenVR interface functions use the __thiscall calling convention on x86, so need to emulate that with __fastcall and a dummy second argument which passes along the EDX register value
	#define VR_Interface_Impl(type, method_name, vtable_offset, interface_version, impl, return_type, ...) \
		static return_type __fastcall type##_##method_name##_##interface_version(vr::type *pThis, void *EDX, ##__VA_ARGS__) \
		{ \
			static const auto trampoline = reshade::hooks::call(type##_##method_name##_##interface_version, vtable_from_instance(pThis) + vtable_offset); \
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
	case 0: // API_DirectX
		return on_submit_d3d11(eEye, static_cast<ID3D11Texture2D *>(pTexture), pBounds, vr::Submit_Default, submit_lambda);
	case 1: // API_OpenGL
		return submit_lambda(eEye, pTexture, pBounds, vr::Submit_Default); // Unsupported because overwritting would require the 'vr::Submit_GlRenderBuffer' flag, which did not yet exist in this OpenVR version
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
	case 0: // API_DirectX
		return on_submit_d3d11(eEye, static_cast<ID3D11Texture2D *>(pTexture), pBounds, nSubmitFlags, submit_lambda);
	case 1: // API_OpenGL
		return on_submit_opengl(eEye, static_cast<GLuint>(reinterpret_cast<uintptr_t>(pTexture)), pBounds, nSubmitFlags, submit_lambda);
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
		return on_submit_d3d11(eEye, static_cast<ID3D11Texture2D *>(pTexture->handle), pBounds, nSubmitFlags, submit_lambda);
	case vr::TextureType_OpenGL:
		return on_submit_opengl(eEye, static_cast<GLuint>(reinterpret_cast<uintptr_t>(pTexture->handle)), pBounds, nSubmitFlags, submit_lambda);
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
		// This is not technically compatible with 'vr::VRTextureWithPoseAndDepth_t', but that is fine, since it's only used for storage and none of the fields are accessed directly
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
		return on_submit_d3d11(eEye, static_cast<ID3D11Texture2D *>(pTexture->handle), pBounds, nSubmitFlags, submit_lambda);
	case vr::TextureType_DirectX12:
		return on_submit_d3d12(eEye, static_cast<const vr::D3D12TextureData_t *>(pTexture->handle), pBounds, nSubmitFlags, submit_lambda);
	case vr::TextureType_OpenGL:
		return on_submit_opengl(eEye, static_cast<GLuint>(reinterpret_cast<uintptr_t>(pTexture->handle)), pBounds, nSubmitFlags, submit_lambda);
	case vr::TextureType_Vulkan:
		return on_submit_vulkan(eEye, static_cast<const vr::VRVulkanTextureData_t *>(pTexture->handle), pBounds, nSubmitFlags, submit_lambda);
	default:
		return vr::VRCompositorError_InvalidTexture;
	}
}, vr::EVRCompositorError, vr::EVREye eEye, const vr::Texture_t *pTexture, const vr::VRTextureBounds_t *pBounds, vr::EVRSubmitFlags nSubmitFlags)

VR_Interface_Impl(IVRClientCore, Cleanup, 1, 001, {
	LOG(INFO) << "Redirecting " << "IVRClientCore::Cleanup" << '(' << "this = " << pThis << ')' << " ...";

	switch (s_vr_swapchain.second)
	{
	case vr::TextureType_DirectX:
		delete static_cast<reshade::d3d11::swapchain_impl *>(s_vr_swapchain.first);
		break;
	case vr::TextureType_DirectX12:
		delete static_cast<reshade::d3d12::swapchain_impl *>(s_vr_swapchain.first);
		break;
	case vr::TextureType_OpenGL:
		delete static_cast<reshade::opengl::swapchain_impl *>(s_vr_swapchain.first);
		break;
	case vr::TextureType_Vulkan:
		delete static_cast<reshade::vulkan::swapchain_impl *>(s_vr_swapchain.first);
		break;
	}

	s_vr_swapchain.first = nullptr;
	s_vr_swapchain.second = vr::TextureType_Invalid;

	VR_Interface_Call();
}, void)

VR_Interface_Impl(IVRClientCore, GetGenericInterface, 3, 001, {
	assert(pchNameAndVersion != nullptr);

	LOG(INFO) << "Redirecting " << "IVRClientCore::GetGenericInterface" << '(' << "this = " << pThis << ", pchNameAndVersion = " << pchNameAndVersion << ')' << " ...";

	void *const interface_instance = VR_Interface_Call(pchNameAndVersion, peError);

	// Only install hooks once, for the first compositor interface version encountered to avoid duplicated hooks
	// This is necessary because vrclient.dll may create an internal compositor instance with a different version than the application to translate older versions, which with hooks installed for both would cause an infinite loop
	if (static unsigned int compositor_version = 0;
		compositor_version == 0 && std::sscanf(pchNameAndVersion, "IVRCompositor_%u", &compositor_version))
	{
		// The 'IVRCompositor::Submit' function definition has been stable and has had the same virtual function table index since the OpenVR 1.0 release (which was at 'IVRCompositor_015')
		if (compositor_version >= 12)
			reshade::hooks::install("IVRCompositor::Submit", vtable_from_instance(static_cast<vr::IVRCompositor *>(interface_instance)), 5, reinterpret_cast<reshade::hook::address>(IVRCompositor_Submit_012));
		else if (compositor_version >= 9)
			reshade::hooks::install("IVRCompositor::Submit", vtable_from_instance(static_cast<vr::IVRCompositor *>(interface_instance)), 4, reinterpret_cast<reshade::hook::address>(IVRCompositor_Submit_009));
		else if (compositor_version == 8)
			reshade::hooks::install("IVRCompositor::Submit", vtable_from_instance(static_cast<vr::IVRCompositor *>(interface_instance)), 6, reinterpret_cast<reshade::hook::address>(IVRCompositor_Submit_008));
		else if (compositor_version == 7)
			reshade::hooks::install("IVRCompositor::Submit", vtable_from_instance(static_cast<vr::IVRCompositor *>(interface_instance)), 6, reinterpret_cast<reshade::hook::address>(IVRCompositor_Submit_007));
	}

	return interface_instance;
}, void *, const char *pchNameAndVersion, vr::EVRInitError *peError)

vr::IVRClientCore *g_client_core = nullptr;

HOOK_EXPORT void *VR_CALLTYPE VRClientCoreFactory(const char *pInterfaceName, int *pReturnCode)
{
	LOG(INFO) << "Redirecting " << "VRClientCoreFactory" << '(' << "pInterfaceName = " << pInterfaceName << ')' << " ...";

	void *const interface_instance = reshade::hooks::call(VRClientCoreFactory)(pInterfaceName, pReturnCode);

	if (static unsigned int client_core_version = 0;
		client_core_version == 0 && std::sscanf(pInterfaceName, "IVRClientCore_%u", &client_core_version))
	{
		g_client_core = static_cast<vr::IVRClientCore *>(interface_instance);

		// The 'IVRClientCore::Cleanup' and 'IVRClientCore::GetGenericInterface' functions did not change between 'IVRClientCore_001' and 'IVRClientCore_003'
		reshade::hooks::install("IVRClientCore::Cleanup", vtable_from_instance(static_cast<vr::IVRClientCore *>(interface_instance)), 1, reinterpret_cast<reshade::hook::address>(IVRClientCore_Cleanup_001));
		reshade::hooks::install("IVRClientCore::GetGenericInterface", vtable_from_instance(static_cast<vr::IVRClientCore *>(interface_instance)), 3, reinterpret_cast<reshade::hook::address>(IVRClientCore_GetGenericInterface_001));
	}

	return interface_instance;
}
