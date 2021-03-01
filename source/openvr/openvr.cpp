/*
 * Copyright (C) 2021 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "dll_log.hpp"
#include "hook_manager.hpp"
#include "d3d11/d3d11_device.hpp"
#include "d3d11/d3d11_device_context.hpp"
#include "d3d11/runtime_d3d11.hpp"
#include "d3d12/d3d12_device.hpp"
#include "d3d12/d3d12_command_queue.hpp"
#include "d3d12/runtime_d3d12.hpp"
#include "opengl/runtime_gl.hpp"
#include "vulkan/vulkan_hooks.hpp"
#include "vulkan/runtime_vk.hpp"
#include <openvr.h>

static std::pair<reshade::runtime *, vr::ETextureType> s_vr_runtime = { nullptr, vr::TextureType_Invalid };

extern lockfree_table<void *, reshade::vulkan::device_impl *, 16> g_vulkan_devices;

static bool on_submit_d3d11(vr::EVREye, ID3D11Texture2D *texture, const vr::VRTextureBounds_t *bounds)
{
	if (s_vr_runtime.first == nullptr)
	{
		com_ptr<ID3D11Device> device;
		com_ptr< D3D11Device> device_proxy;
		texture->GetDevice(&device);
		if (UINT data_size = sizeof(device_proxy);
			FAILED(device->GetPrivateData(__uuidof(D3D11Device), &data_size, reinterpret_cast<void *>(&device_proxy))))
			return false;

		s_vr_runtime = { new reshade::d3d11::runtime_impl(device_proxy.get(), device_proxy->_immediate_context, nullptr), vr::TextureType_DirectX };
	}

	if (s_vr_runtime.second != vr::TextureType_DirectX)
		return false;

	D3D11_TEXTURE2D_DESC tex_desc;
	texture->GetDesc(&tex_desc);

	D3D11_BOX region = { 0, 0, 0, tex_desc.Width, tex_desc.Height, 1 };
	if (bounds != nullptr)
	{
		region.left = static_cast<UINT>(region.right * std::min(bounds->uMin, bounds->uMax));
		region.top =   static_cast<UINT>(region.bottom * std::min(bounds->vMin, bounds->vMax));
		region.right = static_cast<UINT>(region.right * std::max(bounds->uMin, bounds->uMax));
		region.bottom = static_cast<UINT>(region.bottom * std::max(bounds->vMin, bounds->vMax));
	}

	return static_cast<reshade::d3d11::runtime_impl *>(s_vr_runtime.first)->on_present(texture, region, nullptr);
}
static bool on_submit_d3d12(vr::EVREye, const vr::D3D12TextureData_t *texture, const vr::VRTextureBounds_t *bounds)
{
	if (s_vr_runtime.first == nullptr)
	{
		com_ptr<D3D12CommandQueue> command_queue_proxy;
		if (FAILED(texture->m_pCommandQueue->QueryInterface(IID_PPV_ARGS(&command_queue_proxy))))
			return false;

		s_vr_runtime = { new reshade::d3d12::runtime_impl(command_queue_proxy->_device, command_queue_proxy.get(), nullptr), vr::TextureType_DirectX12 };
	}

	if (s_vr_runtime.second != vr::TextureType_DirectX12)
		return false;

	const D3D12_RESOURCE_DESC tex_desc = texture->m_pResource->GetDesc();

	D3D12_BOX region = { 0, 0, 0, static_cast<UINT>(tex_desc.Width), tex_desc.Height, 1 };
	if (bounds != nullptr)
	{
		region.left = static_cast<UINT>(region.right * std::min(bounds->uMin, bounds->uMax));
		region.top =   static_cast<UINT>(region.bottom * std::min(bounds->vMin, bounds->vMax));
		region.right = static_cast<UINT>(region.right * std::max(bounds->uMin, bounds->uMax));
		region.bottom = static_cast<UINT>(region.bottom * std::max(bounds->vMin, bounds->vMax));
	}

	// Resource should be in D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE state at this point
	return static_cast<reshade::d3d12::runtime_impl *>(s_vr_runtime.first)->on_present(texture->m_pResource, region, nullptr);
}
static bool on_submit_opengl(vr::EVREye, GLuint object, bool is_rbo, bool is_array, const vr::VRTextureBounds_t *bounds)
{
	if (s_vr_runtime.first == nullptr)
	{
		const HDC device_context = wglGetCurrentDC();
		const HGLRC render_context = wglGetCurrentContext();

		s_vr_runtime = { new reshade::opengl::runtime_impl(device_context, render_context), vr::TextureType_OpenGL };
	}

	if (s_vr_runtime.second != vr::TextureType_OpenGL)
		return false;

	reshade::api::resource_desc object_desc = static_cast<reshade::opengl::runtime_impl *>(s_vr_runtime.first)->get_resource_desc(
		reshade::opengl::make_resource_handle(is_rbo ? GL_RENDERBUFFER : GL_TEXTURE, object));

	GLint region[4] = { 0, 0, static_cast<GLint>(object_desc.width), static_cast<GLint>(object_desc.height) };
	if (bounds != nullptr)
	{
		region[0] = static_cast<GLint>(object_desc.width * std::min(bounds->uMin, bounds->uMax));
		region[1] = static_cast<GLint>(object_desc.height * std::min(bounds->vMin, bounds->vMax));
		region[2] = static_cast<GLint>(object_desc.width * std::max(bounds->uMin, bounds->uMax));
		region[3] = static_cast<GLint>(object_desc.height * std::max(bounds->vMin, bounds->vMax));
	}

	return static_cast<reshade::opengl::runtime_impl *>(s_vr_runtime.first)->on_present(object, is_rbo, is_array, object_desc.width, object_desc.height, region);
}
static bool on_submit_vulkan(vr::EVREye, const vr::VRVulkanTextureData_t *texture, bool with_array_data, const vr::VRTextureBounds_t *bounds)
{
	if (s_vr_runtime.first == nullptr)
	{
		reshade::vulkan::device_impl *const device = g_vulkan_devices.at(dispatch_key_from_handle(texture->m_pDevice));
		if (device == nullptr)
			return false;

		const auto queue_it = std::find_if(device->_queues.begin(), device->_queues.end(),
			[texture](reshade::vulkan::command_queue_impl *queue) { return queue->_orig == texture->m_pQueue; });
		if (queue_it == device->_queues.end())
			return false;

		// OpenVR requires the passed in queue to be a graphics queue, so can safely use it
		s_vr_runtime = { new reshade::vulkan::runtime_impl(device, *queue_it), vr::TextureType_Vulkan };
	}

	if (s_vr_runtime.second != vr::TextureType_Vulkan)
		return false;

	VkRect2D region = {
		VkOffset2D { 0, 0 },
		VkExtent2D { texture->m_nWidth, texture->m_nHeight }
	};
	if (bounds != nullptr)
	{
		region.offset.x = static_cast<int32_t>(texture->m_nWidth * std::min(bounds->uMin, bounds->uMax));
		region.extent.width = static_cast<uint32_t>(texture->m_nWidth * std::max(bounds->uMin, bounds->uMax) - region.offset.x);
		region.offset.y = static_cast<int32_t>(texture->m_nHeight * std::min(bounds->vMin, bounds->vMax));
		region.extent.height = static_cast<uint32_t>(texture->m_nHeight * std::max(bounds->vMin, bounds->vMax) - region.offset.y);
	}

	const uint32_t layer_index = with_array_data ? static_cast<const vr::VRVulkanTextureArrayData_t *>(texture)->m_unArrayIndex : 0;

	// Image should be in VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL layout at this point
	std::vector<VkSemaphore> wait_semaphores;
	return static_cast<reshade::vulkan::runtime_impl *>(s_vr_runtime.first)->on_present(
		texture->m_pQueue,
		(VkImage)texture->m_nImage,
		static_cast<VkFormat>(texture->m_nFormat),
		static_cast<VkSampleCountFlags>(texture->m_nSampleCount),
		region,
		layer_index,
		nullptr,
		wait_semaphores);
}

#ifdef WIN64
	#define IVRCompositor_Submit_Impl(vtable_offset, interface_version, impl) \
		static vr::EVRCompositorError IVRCompositor_Submit_##interface_version(vr::IVRCompositor *pCompositor, IVRCompositor_Submit_##interface_version##_ArgTypes) \
		{ \
			if (pTexture == nullptr) \
				return vr::VRCompositorError_InvalidTexture; \
			bool status = false; \
			impl \
			if (!status) \
				return vr::VRCompositorError_InvalidTexture; \
			static const auto trampoline = reshade::hooks::call(IVRCompositor_Submit_##interface_version, vtable_from_instance(pCompositor) + vtable_offset); \
			return trampoline(pCompositor, IVRCompositor_Submit_##interface_version##_ArgNames); \
		}
#else
	// The 'IVRCompositor' functions use the __thiscall calling convention on x86, so need to emulate that with __fastcall and a dummy second argument which passes along the EDX register value
	#define IVRCompositor_Submit_Impl(vtable_offset, interface_version, impl) \
		static vr::EVRCompositorError __fastcall IVRCompositor_Submit_##interface_version(vr::IVRCompositor *pCompositor, void *EDX, IVRCompositor_Submit_##interface_version##_ArgTypes) \
		{ \
			if (pTexture == nullptr) \
				return vr::VRCompositorError_InvalidTexture; \
			bool status = false; \
			impl \
			if (!status) \
				return vr::VRCompositorError_InvalidTexture; \
			static const auto trampoline = reshade::hooks::call(IVRCompositor_Submit_##interface_version, vtable_from_instance(pCompositor) + vtable_offset); \
			return trampoline(pCompositor, EDX, IVRCompositor_Submit_##interface_version##_ArgNames); \
		}
#endif

#define IVRCompositor_Submit_007_ArgTypes vr::EVREye eEye, /* vr::GraphicsAPIConvention */ unsigned int eTextureType, void *pTexture, const vr::VRTextureBounds_t *pBounds
#define IVRCompositor_Submit_007_ArgNames eEye, eTextureType, pTexture, pBounds

#define IVRCompositor_Submit_008_ArgTypes vr::EVREye eEye, /* vr::GraphicsAPIConvention */ unsigned int eTextureType, void *pTexture, const vr::VRTextureBounds_t *pBounds, vr::EVRSubmitFlags nSubmitFlags
#define IVRCompositor_Submit_008_ArgNames eEye, eTextureType, pTexture, pBounds, nSubmitFlags

#define IVRCompositor_Submit_009_ArgTypes vr::EVREye eEye, const vr::Texture_t *pTexture, const vr::VRTextureBounds_t *pBounds, vr::EVRSubmitFlags nSubmitFlags
#define IVRCompositor_Submit_009_ArgNames eEye, pTexture, pBounds, nSubmitFlags

#define IVRCompositor_Submit_012_ArgTypes vr::EVREye eEye, const vr::Texture_t *pTexture, const vr::VRTextureBounds_t *pBounds, vr::EVRSubmitFlags nSubmitFlags
#define IVRCompositor_Submit_012_ArgNames eEye, pTexture, pBounds, nSubmitFlags

IVRCompositor_Submit_Impl(6, 007, {
	switch (eTextureType)
	{
	case 0: // API_DirectX
		status = on_submit_d3d11(eEye, static_cast<ID3D11Texture2D *>(pTexture), pBounds);
		break;
	case 1: // API_OpenGL
		status = on_submit_opengl(eEye, static_cast<GLuint>(reinterpret_cast<uintptr_t>(pTexture)), false, false, pBounds);
		break;
	} })
IVRCompositor_Submit_Impl(6, 008, {
	switch (eTextureType)
	{
	case 0: // API_DirectX
		status = on_submit_d3d11(eEye, static_cast<ID3D11Texture2D *>(pTexture), pBounds);
		break;
	case 1: // API_OpenGL
		status = on_submit_opengl(eEye, static_cast<GLuint>(reinterpret_cast<uintptr_t>(pTexture)), (nSubmitFlags & vr::Submit_GlRenderBuffer) != 0, false, pBounds);
		break;
	} })
IVRCompositor_Submit_Impl(4, 009, {
	switch (pTexture->eType)
	{
	case vr::TextureType_DirectX:
		status = on_submit_d3d11(eEye, static_cast<ID3D11Texture2D *>(pTexture->handle), pBounds);
		break;
	case vr::TextureType_OpenGL:
		status = on_submit_opengl(eEye, static_cast<GLuint>(reinterpret_cast<uintptr_t>(pTexture->handle)), (nSubmitFlags & vr::Submit_GlRenderBuffer) != 0, false, pBounds);
		break;
	} })
IVRCompositor_Submit_Impl(5, 012, {
	switch (pTexture->eType)
	{
	case vr::TextureType_DirectX:
		status = on_submit_d3d11(eEye, static_cast<ID3D11Texture2D *>(pTexture->handle), pBounds);
		break;
	case vr::TextureType_DirectX12:
		status = on_submit_d3d12(eEye, static_cast<const vr::D3D12TextureData_t *>(pTexture->handle), pBounds);
		break;
	case vr::TextureType_OpenGL:
		status = on_submit_opengl(eEye, static_cast<GLuint>(reinterpret_cast<uintptr_t>(pTexture->handle)), (nSubmitFlags & vr::Submit_GlRenderBuffer) != 0, (nSubmitFlags & vr::Submit_GlArrayTexture) != 0, pBounds);
		break;
	case vr::TextureType_Vulkan:
		status = on_submit_vulkan(eEye, static_cast<const vr::VRVulkanTextureData_t *>(pTexture->handle), (nSubmitFlags & vr::Submit_VulkanTextureWithArrayData) != 0, pBounds);
		break;
	} })

HOOK_EXPORT uint32_t VR_CALLTYPE VR_InitInternal2(vr::EVRInitError *peError, vr::EVRApplicationType eApplicationType, const char *pStartupInfo)
{
	LOG(INFO) << "Redirecting " << "VR_InitInternal2" << '(' << "peError = " << peError << ", eApplicationType = " << eApplicationType << ", pStartupInfo = " << (pStartupInfo != nullptr ? pStartupInfo : "0") << ')' << " ...";

	return  reshade::hooks::call(VR_InitInternal2)(peError, eApplicationType, pStartupInfo);
}
HOOK_EXPORT void     VR_CALLTYPE VR_ShutdownInternal()
{
	LOG(INFO) << "Redirecting " << "VR_ShutdownInternal" << '(' << ')' << " ...";

	switch (s_vr_runtime.second)
	{
	case vr::TextureType_DirectX:
		delete static_cast<reshade::d3d11::runtime_impl *>(s_vr_runtime.first);
		break;
	case vr::TextureType_DirectX12:
		delete static_cast<reshade::d3d12::runtime_impl *>(s_vr_runtime.first);
		break;
	case vr::TextureType_OpenGL:
		delete static_cast<reshade::opengl::runtime_impl *>(s_vr_runtime.first);
		break;
	case vr::TextureType_Vulkan:
		delete static_cast<reshade::vulkan::runtime_impl *>(s_vr_runtime.first);
		break;
	}

	s_vr_runtime = { nullptr, vr::TextureType_Invalid };

	reshade::hooks::call(VR_ShutdownInternal)();
}

HOOK_EXPORT void *   VR_CALLTYPE VR_GetGenericInterface(const char *pchInterfaceVersion, vr::EVRInitError *peError)
{
	assert(pchInterfaceVersion != nullptr);

	LOG(INFO) << "Redirecting " << "VR_GetGenericInterface" << '(' << "pchInterfaceVersion = " << pchInterfaceVersion << ", peError = " << peError << ')' << " ...";

	void *const interface_instance = reshade::hooks::call(VR_GetGenericInterface)(pchInterfaceVersion, peError);

	if (unsigned int compositor_version = 0;
		std::sscanf(pchInterfaceVersion, "IVRCompositor_%u", &compositor_version))
	{
		// The 'IVRCompositor::Submit' function definition has been stable and has had the same vtable index since the OpenVR 1.0 release (which was at 'IVRCompositor_015')
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
}
