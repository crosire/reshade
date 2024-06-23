/*
 * Copyright (C) 2023 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause OR MIT
 */

#include "openxr_hooks.hpp"
#include "lockfree_linear_map.hpp"
#include <cstring> // std::strcmp

extern lockfree_linear_map<XrInstance, openxr_dispatch_table, 16> g_openxr_instances;

#define HOOK_PROC(name) \
	if (0 == std::strcmp(pName, "xr" #name)) { \
		*pFunction = reinterpret_cast<PFN_xrVoidFunction>(xr##name); \
		return XR_SUCCESS; \
	}

XrResult XRAPI_CALL xrGetInstanceProcAddr(XrInstance instance, const char *pName, PFN_xrVoidFunction *pFunction)
{
	// Core 1_0
	HOOK_PROC(DestroyInstance);
	HOOK_PROC(CreateSession);
	HOOK_PROC(DestroySession);
	HOOK_PROC(CreateSwapchain);
	HOOK_PROC(DestroySwapchain);
	HOOK_PROC(AcquireSwapchainImage);
	HOOK_PROC(ReleaseSwapchainImage);
	HOOK_PROC(EndFrame);

	if (instance == XR_NULL_HANDLE)
		return XR_ERROR_HANDLE_INVALID;

	const auto trampoline = g_openxr_instances.at(instance).GetInstanceProcAddr;
	return trampoline(instance, pName, pFunction);
}
