/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "vulkan_hooks.hpp"
#include "reshade_api_device.hpp"

extern lockfree_table<void *, VkLayerInstanceDispatchTable, 16> g_instance_dispatch;
extern lockfree_table<void *, reshade::vulkan::device_impl *, 16>  g_vulkan_devices;

#define HOOK_PROC(name) \
	if (0 == strcmp(pName, "vk" #name)) \
		return reinterpret_cast<PFN_vkVoidFunction>(vk##name)

VK_LAYER_EXPORT PFN_vkVoidFunction VKAPI_CALL vkGetDeviceProcAddr(VkDevice device, const char *pName)
{
	// The Vulkan loader gets the 'vkDestroyDevice' function from the device dispatch table
	HOOK_PROC(DestroyDevice);
	HOOK_PROC(CreateSwapchainKHR);
	HOOK_PROC(DestroySwapchainKHR);
	HOOK_PROC(QueueSubmit);
	HOOK_PROC(QueuePresentKHR);

#if RESHADE_ADDON
	HOOK_PROC(CreateBuffer);
	HOOK_PROC(DestroyBuffer);
	HOOK_PROC(CreateBufferView);
	HOOK_PROC(DestroyBufferView);
	HOOK_PROC(CreateImage);
	HOOK_PROC(DestroyImage);
	HOOK_PROC(CreateImageView);
	HOOK_PROC(DestroyImageView);
	HOOK_PROC(CreateShaderModule);
	HOOK_PROC(CreateGraphicsPipelines);
	HOOK_PROC(CreateComputePipelines);
	HOOK_PROC(CreateRenderPass);
	HOOK_PROC(CreateRenderPass2);
	if (0 == strcmp(pName, "vkCreateRenderPass2KHR"))
		return reinterpret_cast<PFN_vkVoidFunction>(vkCreateRenderPass2);
	HOOK_PROC(DestroyRenderPass);
	HOOK_PROC(CreateFramebuffer);
	HOOK_PROC(DestroyFramebuffer);

	HOOK_PROC(AllocateCommandBuffers);
	HOOK_PROC(FreeCommandBuffers);
	HOOK_PROC(BeginCommandBuffer);

	HOOK_PROC(CmdBindPipeline);
	HOOK_PROC(CmdSetViewport);
	HOOK_PROC(CmdSetScissor);
	HOOK_PROC(CmdSetDepthBias);
	HOOK_PROC(CmdSetBlendConstants);
	HOOK_PROC(CmdSetStencilCompareMask);
	HOOK_PROC(CmdSetStencilWriteMask);
	HOOK_PROC(CmdSetStencilReference);
	HOOK_PROC(CmdBindDescriptorSets);
	HOOK_PROC(CmdBindIndexBuffer);
	HOOK_PROC(CmdBindVertexBuffers);
	HOOK_PROC(CmdDraw);
	HOOK_PROC(CmdDrawIndexed);
	HOOK_PROC(CmdDrawIndirect);
	HOOK_PROC(CmdDrawIndexedIndirect);
	HOOK_PROC(CmdDispatch);
	HOOK_PROC(CmdDispatchIndirect);
	HOOK_PROC(CmdCopyBuffer);
	HOOK_PROC(CmdCopyImage);
	HOOK_PROC(CmdBlitImage);
	HOOK_PROC(CmdCopyBufferToImage);
	HOOK_PROC(CmdCopyImageToBuffer);
	HOOK_PROC(CmdClearColorImage);
	HOOK_PROC(CmdClearDepthStencilImage);
	HOOK_PROC(CmdClearAttachments);
	HOOK_PROC(CmdResolveImage);
	HOOK_PROC(CmdPushConstants);
	HOOK_PROC(CmdBeginRenderPass);
	HOOK_PROC(CmdNextSubpass);
	HOOK_PROC(CmdEndRenderPass);
	HOOK_PROC(CmdExecuteCommands);
#endif

	// Need to self-intercept as well, since some layers rely on this (e.g. Steam overlay)
	// See https://github.com/KhronosGroup/Vulkan-Loader/blob/master/loader/LoaderAndLayerInterface.md#layer-conventions-and-rules
	HOOK_PROC(GetDeviceProcAddr);

	if (device == VK_NULL_HANDLE)
		return nullptr;

	const auto trampoline = g_vulkan_devices.at(dispatch_key_from_handle(device))->_dispatch_table.GetDeviceProcAddr;
	return trampoline(device, pName);
}

VK_LAYER_EXPORT PFN_vkVoidFunction VKAPI_CALL vkGetInstanceProcAddr(VkInstance instance, const char *pName)
{
	HOOK_PROC(CreateInstance);
	HOOK_PROC(DestroyInstance);
	HOOK_PROC(CreateDevice);
	HOOK_PROC(DestroyDevice);
	HOOK_PROC(CreateWin32SurfaceKHR);
	HOOK_PROC(DestroySurfaceKHR);

	// Self-intercept here as well to stay consistent with 'vkGetDeviceProcAddr' implementation
	HOOK_PROC(GetInstanceProcAddr);

	if (instance == VK_NULL_HANDLE)
		return nullptr;

	const auto trampoline = g_instance_dispatch.at(dispatch_key_from_handle(instance)).GetInstanceProcAddr;
	return trampoline(instance, pName);
}
