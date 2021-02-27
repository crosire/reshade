#include "dll_log.hpp"
#include "hook_manager.hpp"
#include "openvr_runtime_d3d11.hpp"

std::unique_ptr<reshade::openvr::openvr_runtime_d3d11> vr_runtime_d3d11;

vr::EVRCompositorError IVRCompositor_Submit(vr::IVRCompositor *compositor, vr::EVREye eEye, const vr::Texture_t *pTexture, const vr::VRTextureBounds_t* pBounds, vr::EVRSubmitFlags nSubmitFlags)
{
	if (pTexture == nullptr)
		return vr::VRCompositorError_InvalidTexture;

	const auto trampoline = reshade::hooks::call(IVRCompositor_Submit, vtable_from_instance(compositor) + 5);

	static const vr::VRTextureBounds_t full_texture { 0, 0, 1, 1 };
	if (pBounds == nullptr)
		pBounds = &full_texture;

	if (pTexture->eType == vr::TextureType_DirectX)
	{
		if (vr_runtime_d3d11 == nullptr)
		{
			com_ptr<ID3D11Device> device;
			static_cast<ID3D11Texture2D*>(pTexture->handle)->GetDevice(&device);
			com_ptr< D3D11Device> device_proxy;
			if (UINT data_size = sizeof(device_proxy);
				FAILED(device->GetPrivateData(__uuidof(D3D11Device), &data_size, reinterpret_cast<void *>(&device_proxy))))
				return vr::VRCompositorError_TextureIsOnWrongDevice;
			vr_runtime_d3d11.reset(new reshade::openvr::openvr_runtime_d3d11(device_proxy.get(), trampoline, compositor));
		}
		return vr_runtime_d3d11->on_submit(eEye, pTexture, pBounds, nSubmitFlags);
	}
	
	return trampoline(compositor, eEye, pTexture, pBounds, nSubmitFlags);
}

HOOK_EXPORT void *VR_GetGenericInterface(const char *pchInterfaceVersion, vr::EVRInitError *peError)
{
	static const auto trampoline = reshade::hooks::call(VR_GetGenericInterface);
	LOG(DEBUG) << "VR: Requested interface " << pchInterfaceVersion;
	auto vrInterface = trampoline(pchInterfaceVersion, peError);

	// NOTE: there are different versions of the IVRCompositor interface that can be returned here.
	// However, the Submit function definition has been stable and has had the same vtable index since the
	// OpenVR 1.0 release. Hence we can get away with a unified hook function.
	if (strcmp(pchInterfaceVersion, "IVRCompositor_015") >= 0 && strcmp(pchInterfaceVersion, "IVRCompositor_026") <= 0)
	{
		LOG(INFO) << "VR: Hooking into " << pchInterfaceVersion;
		vr::IVRCompositor *compositor = static_cast< vr::IVRCompositor* >( vrInterface );
		reshade::hooks::install("IVRCompositor::Submit", vtable_from_instance(compositor), 5, reinterpret_cast<reshade::hook::address>(IVRCompositor_Submit));
	}
	
	return vrInterface;
}

HOOK_EXPORT void VR_ShutdownInternal()
{
	LOG(INFO) << "VR: Shutting down";
	vr_runtime_d3d11.reset();
	reshade::hooks::call(VR_ShutdownInternal)();
}
