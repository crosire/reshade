/*
 * Copyright (C) 2023 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause OR MIT
 */

#include "openxr_hooks.hpp"
#include "lockfree_linear_map.hpp"
#include <cstring> // std::strcmp

extern lockfree_linear_map<XrInstance, openxr_instance, 16> g_openxr_instances;

#define RESHADE_OPENXR_HOOK_PROC(name) \
	if (0 == std::strcmp(pName, "xr" #name)) { \
		*pFunction = reinterpret_cast<PFN_xrVoidFunction>(xr##name); \
		return XR_SUCCESS; \
	}

XrResult XRAPI_CALL xrGetInstanceProcAddr(XrInstance instance, const char *pName, PFN_xrVoidFunction *pFunction)
{
	// Core 1_0
	RESHADE_OPENXR_HOOK_PROC(DestroyInstance);
	RESHADE_OPENXR_HOOK_PROC(CreateSession);
	RESHADE_OPENXR_HOOK_PROC(DestroySession);
	RESHADE_OPENXR_HOOK_PROC(CreateSwapchain);
	RESHADE_OPENXR_HOOK_PROC(DestroySwapchain);
	RESHADE_OPENXR_HOOK_PROC(AcquireSwapchainImage);
	RESHADE_OPENXR_HOOK_PROC(ReleaseSwapchainImage);
	RESHADE_OPENXR_HOOK_PROC(EndFrame);

	if (instance == XR_NULL_HANDLE)
		return XR_ERROR_HANDLE_INVALID;

	const auto trampoline = g_openxr_instances.at(instance).dispatch_table.GetInstanceProcAddr;
	return trampoline(instance, pName, pFunction);
}
