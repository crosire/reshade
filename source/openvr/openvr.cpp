#include "dll_log.hpp"
#include "hook_manager.hpp"
#include "openvr_defs.hpp"

reshade::hook::address *as_vtable(void *instance)
{
	return *static_cast<reshade::hook::address **>(instance);
}

vr::EVRCompositorError IVRCompositor022_Submit(void *ivrCompositor, vr::EVREye eEye, const vr::Texture_t *pTexture, const vr::VRTextureBounds_t* pBounds, vr::EVRSubmitFlags nSubmitFlags)
{
	LOG(DEBUG) << "VR: Intercepted call to IVRCompositor022::Submit";
	const auto trampoline = reshade::hooks::call(IVRCompositor022_Submit, as_vtable(ivrCompositor) + 5);
	return trampoline(ivrCompositor, eEye, pTexture, pBounds, nSubmitFlags);
}

HOOK_EXPORT void *VR_GetGenericInterface(const char *pchInterfaceVersion, vr::EVRInitError *peError)
{
	static const auto trampoline = reshade::hooks::call(VR_GetGenericInterface);
	LOG(DEBUG) << "VR: Requested interface " << pchInterfaceVersion;
	auto vrInterface = trampoline(pchInterfaceVersion, peError);

	if (strcmp(pchInterfaceVersion, "IVRCompositor_022") == 0)
	{
		LOG(INFO) << "VR: Hooking into " << pchInterfaceVersion;
		reshade::hooks::install("IVRCompositor022::Submit", as_vtable(vrInterface), 5, reinterpret_cast<reshade::hook::address>(IVRCompositor022_Submit));
	}
	
	return vrInterface;
}
