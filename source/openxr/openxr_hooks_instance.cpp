/*
 * Copyright (C) 2023 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause OR MIT
 */

#include "openxr_hooks.hpp"
#include "dll_log.hpp"
#include "lockfree_linear_map.hpp"
#include <openxr/openxr_loader_negotiation.h>

lockfree_linear_map<XrInstance, openxr_instance, 16> g_openxr_instances;

XrResult XRAPI_CALL xrCreateApiLayerInstance(const XrInstanceCreateInfo *pCreateInfo, const XrApiLayerCreateInfo *pApiLayerInfo, XrInstance *pInstance)
{
	reshade::log::message(reshade::log::level::info, "Redirecting xrCreateApiLayerInstance(pCreateInfo = %p, pApiLayerInfo = %p, pInstance = %p) ...", pCreateInfo, pApiLayerInfo, pInstance);

	assert(pCreateInfo != nullptr && pInstance != nullptr);

	if (pApiLayerInfo == nullptr ||
		pApiLayerInfo->structType != XR_LOADER_INTERFACE_STRUCT_API_LAYER_CREATE_INFO ||
		pApiLayerInfo->structVersion != XR_API_LAYER_CREATE_INFO_STRUCT_VERSION ||
		pApiLayerInfo->structSize != sizeof(XrApiLayerCreateInfo) ||
		pApiLayerInfo->nextInfo == nullptr ||
		pApiLayerInfo->nextInfo->structType != XR_LOADER_INTERFACE_STRUCT_API_LAYER_NEXT_INFO ||
		pApiLayerInfo->nextInfo->structVersion != XR_API_LAYER_NEXT_INFO_STRUCT_VERSION ||
		pApiLayerInfo->nextInfo->structSize != sizeof(XrApiLayerNextInfo))
		return XR_ERROR_INITIALIZATION_FAILED;

	// Get trampoline function pointers
	PFN_xrCreateApiLayerInstance trampoline = pApiLayerInfo->nextInfo->nextCreateApiLayerInstance;
	PFN_xrGetInstanceProcAddr get_instance_proc = pApiLayerInfo->nextInfo->nextGetInstanceProcAddr;

	if (trampoline == nullptr || get_instance_proc == nullptr) // Unable to resolve next 'xrCreateApiLayerInstance' function in the call chain
		return XR_ERROR_INITIALIZATION_FAILED;

	reshade::log::message(reshade::log::level::info, "> Dumping enabled instance extensions:");
	for (uint32_t i = 0; i < pCreateInfo->enabledExtensionCount; ++i)
		reshade::log::message(reshade::log::level::info, "  %s", pCreateInfo->enabledExtensionNames[i]);

	XrApiLayerCreateInfo api_layer_info = *pApiLayerInfo;
	api_layer_info.nextInfo = pApiLayerInfo->nextInfo->next;

	reshade::log::message(reshade::log::level::info, "Requesting new OpenXR instance for API version %hu.%hu.", XR_VERSION_MAJOR(pCreateInfo->applicationInfo.apiVersion), XR_VERSION_MINOR(pCreateInfo->applicationInfo.apiVersion));

	// Continue calling down the chain
	const XrResult result = trampoline(pCreateInfo, &api_layer_info, pInstance);
	if (XR_FAILED(result))
	{
		reshade::log::message(reshade::log::level::warning, "xrCreateApiLayerInstance failed with error code %d.", static_cast<int>(result));
		return result;
	}

	// Initialize the instance dispatch table
	openxr_instance instance = { *pInstance };
	instance.dispatch_table.GetInstanceProcAddr = get_instance_proc;

	// Core 1_0
	get_instance_proc(instance.handle, "xrDestroyInstance", reinterpret_cast<PFN_xrVoidFunction *>(&instance.dispatch_table.DestroyInstance));
	get_instance_proc(instance.handle, "xrCreateSession", reinterpret_cast<PFN_xrVoidFunction *>(&instance.dispatch_table.CreateSession));
	get_instance_proc(instance.handle, "xrDestroySession", reinterpret_cast<PFN_xrVoidFunction *>(&instance.dispatch_table.DestroySession));
	get_instance_proc(instance.handle, "xrEnumerateViewConfigurations", reinterpret_cast<PFN_xrVoidFunction *>(&instance.dispatch_table.EnumerateViewConfigurations));
	get_instance_proc(instance.handle, "xrCreateSwapchain", reinterpret_cast<PFN_xrVoidFunction *>(&instance.dispatch_table.CreateSwapchain));
	get_instance_proc(instance.handle, "xrDestroySwapchain", reinterpret_cast<PFN_xrVoidFunction *>(&instance.dispatch_table.DestroySwapchain));
	get_instance_proc(instance.handle, "xrEnumerateSwapchainImages", reinterpret_cast<PFN_xrVoidFunction *>(&instance.dispatch_table.EnumerateSwapchainImages));
	get_instance_proc(instance.handle, "xrAcquireSwapchainImage", reinterpret_cast<PFN_xrVoidFunction *>(&instance.dispatch_table.AcquireSwapchainImage));
	get_instance_proc(instance.handle, "xrReleaseSwapchainImage", reinterpret_cast<PFN_xrVoidFunction *>(&instance.dispatch_table.ReleaseSwapchainImage));
	get_instance_proc(instance.handle, "xrEndFrame", reinterpret_cast<PFN_xrVoidFunction *>(&instance.dispatch_table.EndFrame));

	g_openxr_instances.emplace(instance.handle, instance);

#if RESHADE_VERBOSE_LOG
	reshade::log::message(reshade::log::level::debug, "Returning OpenXR instance %" PRIx64 ".", instance.handle);
#endif
	return result;
}
XrResult XRAPI_CALL xrDestroyInstance(XrInstance instance)
{
	reshade::log::message(reshade::log::level::info, "Redirecting xrDestroyInstance(instance = %" PRIx64 ") ...", instance);

	// Get function pointer before removing it next
	RESHADE_OPENXR_GET_DISPATCH_PTR(DestroyInstance);

	// Remove instance dispatch table since this instance is being destroyed
	g_openxr_instances.erase(instance);

	return trampoline(instance);
}

extern "C" XrResult XRAPI_CALL xrNegotiateLoaderApiLayerInterface(const XrNegotiateLoaderInfo *pLoaderInfo, const char *, XrNegotiateApiLayerRequest *pApiLayerRequest)
{
	if (pLoaderInfo == nullptr ||
		pLoaderInfo->structType != XR_LOADER_INTERFACE_STRUCT_LOADER_INFO ||
		pLoaderInfo->structVersion != XR_LOADER_INFO_STRUCT_VERSION ||
		pLoaderInfo->structSize != sizeof(XrNegotiateLoaderInfo) ||
		pApiLayerRequest == nullptr ||
		pApiLayerRequest->structType != XR_LOADER_INTERFACE_STRUCT_API_LAYER_REQUEST ||
		pApiLayerRequest->structVersion != XR_API_LAYER_INFO_STRUCT_VERSION ||
		pApiLayerRequest->structSize != sizeof(XrNegotiateApiLayerRequest))
		return XR_ERROR_INITIALIZATION_FAILED;

	if (pLoaderInfo->minInterfaceVersion > XR_CURRENT_LOADER_API_LAYER_VERSION || pLoaderInfo->maxInterfaceVersion < XR_CURRENT_LOADER_API_LAYER_VERSION ||
		pLoaderInfo->minApiVersion > XR_CURRENT_API_VERSION || pLoaderInfo->maxApiVersion < XR_CURRENT_API_VERSION)
		return XR_ERROR_INITIALIZATION_FAILED;

	pApiLayerRequest->layerInterfaceVersion = XR_CURRENT_LOADER_API_LAYER_VERSION;
	pApiLayerRequest->layerApiVersion = XR_CURRENT_API_VERSION;
	pApiLayerRequest->getInstanceProcAddr = xrGetInstanceProcAddr;
	pApiLayerRequest->createApiLayerInstance = xrCreateApiLayerInstance;

	return XR_SUCCESS;
}
