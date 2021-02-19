#include "dll_log.hpp"
#include "hook_manager.hpp"
#include "openvr_defs.hpp"

HOOK_EXPORT void *VR_GetGenericInterface(const char *pchInterfaceVersion, vr::EVRInitError *peError)
{
	static const auto trampoline = reshade::hooks::call(VR_GetGenericInterface);
	LOG(INFO) << "VR: Requested interface " << pchInterfaceVersion;
	auto vrInterface = trampoline(pchInterfaceVersion, peError);
	return vrInterface;
}
