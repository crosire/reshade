#include "dll_log.hpp"
#include "hook_manager.hpp"
#include "openvr_runtime_d3d11.hpp"

reshade::openvr::openvr_runtime_d3d11 *vr_runtime_d3d11 = nullptr;

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
			vr_runtime_d3d11 = new reshade::openvr::openvr_runtime_d3d11(pTexture, pBounds, trampoline, compositor);
		}
		vr_runtime_d3d11->on_submit(eEye, pTexture, pBounds);
		// FIXME: should we do better error reporting here?
		return vr::VRCompositorError_None;
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
