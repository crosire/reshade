/*
 * Copyright (C) 2021 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "dll_log.hpp"
#include "hook_manager.hpp"
#include "d3d11/d3d11_device.hpp"
#include "d3d11/d3d11_device_context.hpp"
#include "d3d11/runtime_d3d11.hpp"
#include <openvr.h>

std::unique_ptr<reshade::d3d11::runtime_impl> vr_runtime_d3d11;

static void on_submit(vr::EVREye, ID3D11Texture2D *texture, const vr::VRTextureBounds_t *pBounds)
{
	static const vr::VRTextureBounds_t full_texture { 0, 0, 1, 1 };
	if (pBounds == nullptr)
		pBounds = &full_texture;

	if (vr_runtime_d3d11 == nullptr)
	{
		com_ptr<ID3D11Device> device;
		com_ptr< D3D11Device> device_proxy;
		texture->GetDevice(&device);
		if (UINT data_size = sizeof(device_proxy);
			FAILED(device->GetPrivateData(__uuidof(D3D11Device), &data_size, reinterpret_cast<void *>(&device_proxy))))
			return;

		vr_runtime_d3d11.reset(new reshade::d3d11::runtime_impl(device_proxy.get(), device_proxy->_immediate_context, nullptr));
	}

	D3D11_TEXTURE2D_DESC tex_desc;
	texture->GetDesc(&tex_desc);

	D3D11_BOX region;
	region.left = static_cast<UINT>(tex_desc.Width * std::min(pBounds->uMin, pBounds->uMax));
	region.right = static_cast<UINT>(tex_desc.Width * std::max(pBounds->uMin, pBounds->uMax));
	region.top = static_cast<UINT>(tex_desc.Height * std::min(pBounds->vMin, pBounds->vMax));
	region.bottom = static_cast<UINT>(tex_desc.Height * std::max(pBounds->vMin, pBounds->vMax));
	region.front = 0;
	region.back = 1;

	vr_runtime_d3d11->on_present(texture, region);
}

#ifdef WIN64
	#define IVRCompositor_Submit_Impl(vtable_offset, interface_version, impl) \
		static vr::EVRCompositorError IVRCompositor_Submit_##interface_version(vr::IVRCompositor *pCompositor, IVRCompositor_Submit_##interface_version##_ArgTypes) \
		{ \
			if (pTexture == nullptr) \
				return vr::VRCompositorError_InvalidTexture; \
			impl \
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
			impl \
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
	if (eTextureType == 1) // API_DirectX
		on_submit(eEye, static_cast<ID3D11Texture2D *>(pTexture), pBounds);
	})
IVRCompositor_Submit_Impl(6, 008, {
	if (eTextureType == 1) // API_DirectX
		on_submit(eEye, static_cast<ID3D11Texture2D *>(pTexture), pBounds);
	})
IVRCompositor_Submit_Impl(4, 009, {
	if (pTexture->eType == vr::TextureType_DirectX)
		on_submit(eEye, static_cast<ID3D11Texture2D *>(pTexture->handle), pBounds);
	})
IVRCompositor_Submit_Impl(5, 012, {
	if (pTexture->eType == vr::TextureType_DirectX)
		on_submit(eEye, static_cast<ID3D11Texture2D *>(pTexture->handle), pBounds);
	})

HOOK_EXPORT uint32_t VR_CALLTYPE VR_InitInternal2(vr::EVRInitError *peError, vr::EVRApplicationType eApplicationType, const char *pStartupInfo)
{
	LOG(INFO) << "Redirecting " << "VR_InitInternal2" << '(' << "peError = " << peError << ", eApplicationType = " << eApplicationType << ", pStartupInfo = " << (pStartupInfo != nullptr ? pStartupInfo : "0") << ')' << " ...";

	return  reshade::hooks::call(VR_InitInternal2)(peError, eApplicationType, pStartupInfo);
}
HOOK_EXPORT void     VR_CALLTYPE VR_ShutdownInternal()
{
	LOG(INFO) << "Redirecting " << "VR_ShutdownInternal" << '(' << ')' << " ...";

	vr_runtime_d3d11.reset();

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
