/*
 * Copyright (C) 2014 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause OR MIT
 */

#include "vulkan_hooks.hpp"
#include "vulkan_impl_device.hpp"
#ifdef RESHADE_TEST_APPLICATION
#include "hook_manager.hpp"
#endif
#include "lockfree_linear_map.hpp"
#include <cstring> // std::strcmp

extern lockfree_linear_map<void *, vulkan_instance, 16> g_vulkan_instances;
extern lockfree_linear_map<void *, reshade::vulkan::device_impl *, 8> g_vulkan_devices;

#define RESHADE_VULKAN_HOOK_PROC(name) \
	if (0 == std::strcmp(name_without_prefix, #name)) \
		return reinterpret_cast<PFN_vkVoidFunction>(vk##name)
#define RESHADE_VULKAN_HOOK_PROC_OPTIONAL(name, suffix) \
	if (0 == std::strcmp(name_without_prefix, #name #suffix) && g_vulkan_devices.at(dispatch_key_from_handle(device))->_dispatch_table.name != nullptr) \
		return reinterpret_cast<PFN_vkVoidFunction>(vk##name);

PFN_vkVoidFunction VKAPI_CALL vkGetDeviceProcAddr(VkDevice device, const char *pName)
{
	if (pName == nullptr || pName[0] != 'v' || pName[1] != 'k')
		return nullptr;
	const char *name_without_prefix = pName + 2;

	// The Vulkan loader gets the 'vkDestroyDevice' function from the device dispatch table
	RESHADE_VULKAN_HOOK_PROC(DestroyDevice);

	// Core 1_0
#if RESHADE_ADDON
	RESHADE_VULKAN_HOOK_PROC(QueueSubmit);
	RESHADE_VULKAN_HOOK_PROC(BindBufferMemory);
	RESHADE_VULKAN_HOOK_PROC(BindImageMemory);
	RESHADE_VULKAN_HOOK_PROC(CreateQueryPool);
	RESHADE_VULKAN_HOOK_PROC(DestroyQueryPool);
#endif
#if RESHADE_ADDON >= 2
	RESHADE_VULKAN_HOOK_PROC(GetQueryPoolResults);
#endif
#if RESHADE_ADDON
	RESHADE_VULKAN_HOOK_PROC(CreateBuffer);
	RESHADE_VULKAN_HOOK_PROC(DestroyBuffer);
	RESHADE_VULKAN_HOOK_PROC(CreateBufferView);
	RESHADE_VULKAN_HOOK_PROC(DestroyBufferView);
	RESHADE_VULKAN_HOOK_PROC(CreateImage);
	RESHADE_VULKAN_HOOK_PROC(DestroyImage);
	RESHADE_VULKAN_HOOK_PROC(CreateImageView);
	RESHADE_VULKAN_HOOK_PROC(DestroyImageView);
#endif
#if RESHADE_ADDON >= 2
	RESHADE_VULKAN_HOOK_PROC(CreateShaderModule);
	RESHADE_VULKAN_HOOK_PROC(DestroyShaderModule);
	RESHADE_VULKAN_HOOK_PROC(CreateGraphicsPipelines);
	RESHADE_VULKAN_HOOK_PROC(CreateComputePipelines);
	RESHADE_VULKAN_HOOK_PROC(DestroyPipeline);
	RESHADE_VULKAN_HOOK_PROC(CreatePipelineLayout);
	RESHADE_VULKAN_HOOK_PROC(DestroyPipelineLayout);
#endif
#if RESHADE_ADDON
	RESHADE_VULKAN_HOOK_PROC(CreateSampler);
	RESHADE_VULKAN_HOOK_PROC(DestroySampler);
#endif
#if RESHADE_ADDON >= 2
	RESHADE_VULKAN_HOOK_PROC(CreateDescriptorSetLayout);
	RESHADE_VULKAN_HOOK_PROC(DestroyDescriptorSetLayout);
	RESHADE_VULKAN_HOOK_PROC(CreateDescriptorPool);
	RESHADE_VULKAN_HOOK_PROC(DestroyDescriptorPool);
	RESHADE_VULKAN_HOOK_PROC(ResetDescriptorPool);
	RESHADE_VULKAN_HOOK_PROC(AllocateDescriptorSets);
	RESHADE_VULKAN_HOOK_PROC(FreeDescriptorSets);
	RESHADE_VULKAN_HOOK_PROC(UpdateDescriptorSets);
#endif
#if RESHADE_ADDON
	RESHADE_VULKAN_HOOK_PROC(CreateFramebuffer);
	RESHADE_VULKAN_HOOK_PROC(DestroyFramebuffer);
	RESHADE_VULKAN_HOOK_PROC(CreateRenderPass);
	RESHADE_VULKAN_HOOK_PROC(DestroyRenderPass);

	RESHADE_VULKAN_HOOK_PROC(AllocateCommandBuffers);
	RESHADE_VULKAN_HOOK_PROC(FreeCommandBuffers);
	RESHADE_VULKAN_HOOK_PROC(BeginCommandBuffer);
	RESHADE_VULKAN_HOOK_PROC(EndCommandBuffer);
#endif
#if RESHADE_ADDON >= 2
	RESHADE_VULKAN_HOOK_PROC(CmdBindPipeline);
#endif
#if RESHADE_ADDON
	RESHADE_VULKAN_HOOK_PROC(CmdSetViewport);
	RESHADE_VULKAN_HOOK_PROC(CmdSetScissor);
#endif
#if RESHADE_ADDON >= 2
	RESHADE_VULKAN_HOOK_PROC(CmdSetDepthBias);
	RESHADE_VULKAN_HOOK_PROC(CmdSetBlendConstants);
	RESHADE_VULKAN_HOOK_PROC(CmdSetStencilCompareMask);
	RESHADE_VULKAN_HOOK_PROC(CmdSetStencilWriteMask);
	RESHADE_VULKAN_HOOK_PROC(CmdSetStencilReference);
	RESHADE_VULKAN_HOOK_PROC(CmdBindDescriptorSets);
	RESHADE_VULKAN_HOOK_PROC(CmdBindIndexBuffer);
	RESHADE_VULKAN_HOOK_PROC(CmdBindVertexBuffers);
#endif
#if RESHADE_ADDON
	RESHADE_VULKAN_HOOK_PROC(CmdDraw);
	RESHADE_VULKAN_HOOK_PROC(CmdDrawIndexed);
	RESHADE_VULKAN_HOOK_PROC(CmdDrawIndirect);
	RESHADE_VULKAN_HOOK_PROC(CmdDrawIndexedIndirect);
	RESHADE_VULKAN_HOOK_PROC(CmdDispatch);
	RESHADE_VULKAN_HOOK_PROC(CmdDispatchIndirect);
#endif
#if RESHADE_ADDON >= 2
	RESHADE_VULKAN_HOOK_PROC(CmdCopyBuffer);
	RESHADE_VULKAN_HOOK_PROC(CmdCopyImage);
	RESHADE_VULKAN_HOOK_PROC(CmdBlitImage);
	RESHADE_VULKAN_HOOK_PROC(CmdCopyBufferToImage);
	RESHADE_VULKAN_HOOK_PROC(CmdCopyImageToBuffer);
	RESHADE_VULKAN_HOOK_PROC(CmdUpdateBuffer);
#endif
#if RESHADE_ADDON
	RESHADE_VULKAN_HOOK_PROC(CmdClearColorImage);
	RESHADE_VULKAN_HOOK_PROC(CmdClearDepthStencilImage);
	RESHADE_VULKAN_HOOK_PROC(CmdClearAttachments);
#endif
#if RESHADE_ADDON >= 2
	RESHADE_VULKAN_HOOK_PROC(CmdResolveImage);
	RESHADE_VULKAN_HOOK_PROC(CmdPipelineBarrier);
	RESHADE_VULKAN_HOOK_PROC(CmdBeginQuery);
	RESHADE_VULKAN_HOOK_PROC(CmdEndQuery);
	RESHADE_VULKAN_HOOK_PROC(CmdWriteTimestamp);
	RESHADE_VULKAN_HOOK_PROC(CmdCopyQueryPoolResults);
	RESHADE_VULKAN_HOOK_PROC(CmdPushConstants);
#endif
#if RESHADE_ADDON
	RESHADE_VULKAN_HOOK_PROC(CmdBeginRenderPass);
	RESHADE_VULKAN_HOOK_PROC(CmdNextSubpass);
	RESHADE_VULKAN_HOOK_PROC(CmdEndRenderPass);
	RESHADE_VULKAN_HOOK_PROC(CmdExecuteCommands);

	// Core 1_1
	RESHADE_VULKAN_HOOK_PROC(BindBufferMemory2);
	RESHADE_VULKAN_HOOK_PROC(BindImageMemory2);
	RESHADE_VULKAN_HOOK_PROC(CreateDescriptorUpdateTemplate);
	RESHADE_VULKAN_HOOK_PROC(DestroyDescriptorUpdateTemplate);
	RESHADE_VULKAN_HOOK_PROC(UpdateDescriptorSetWithTemplate);

	// Core 1_2
	RESHADE_VULKAN_HOOK_PROC(CmdDrawIndirectCount);
	RESHADE_VULKAN_HOOK_PROC(CmdDrawIndexedIndirectCount);
	RESHADE_VULKAN_HOOK_PROC(CreateRenderPass2);
	RESHADE_VULKAN_HOOK_PROC(CmdBeginRenderPass2);
	RESHADE_VULKAN_HOOK_PROC(CmdNextSubpass2);
	RESHADE_VULKAN_HOOK_PROC(CmdEndRenderPass2);
#endif

	// Core 1_3
#if RESHADE_ADDON >= 2
	RESHADE_VULKAN_HOOK_PROC_OPTIONAL(CmdPipelineBarrier2, );
	RESHADE_VULKAN_HOOK_PROC_OPTIONAL(CmdWriteTimestamp2, );
#endif
#if RESHADE_ADDON
	RESHADE_VULKAN_HOOK_PROC_OPTIONAL(QueueSubmit2, );
#endif
#if RESHADE_ADDON >= 2
	RESHADE_VULKAN_HOOK_PROC_OPTIONAL(CmdCopyBuffer2, );
	RESHADE_VULKAN_HOOK_PROC_OPTIONAL(CmdCopyImage2, );
	RESHADE_VULKAN_HOOK_PROC_OPTIONAL(CmdCopyBufferToImage2, );
	RESHADE_VULKAN_HOOK_PROC_OPTIONAL(CmdCopyImageToBuffer2, );
	RESHADE_VULKAN_HOOK_PROC_OPTIONAL(CmdBlitImage2, );
	RESHADE_VULKAN_HOOK_PROC_OPTIONAL(CmdResolveImage2, );
#endif
#if RESHADE_ADDON
	RESHADE_VULKAN_HOOK_PROC_OPTIONAL(CmdBeginRendering, );
	RESHADE_VULKAN_HOOK_PROC_OPTIONAL(CmdEndRendering, );
#endif
#if RESHADE_ADDON >= 2
	RESHADE_VULKAN_HOOK_PROC_OPTIONAL(CmdBindVertexBuffers2, );
#endif

	// Core 1_4
#if RESHADE_ADDON >= 2
	RESHADE_VULKAN_HOOK_PROC_OPTIONAL(CmdPushDescriptorSet, );
	RESHADE_VULKAN_HOOK_PROC_OPTIONAL(CmdPushDescriptorSetWithTemplate, );
#endif

	// VK_KHR_swapchain
	RESHADE_VULKAN_HOOK_PROC(CreateSwapchainKHR);
	RESHADE_VULKAN_HOOK_PROC(DestroySwapchainKHR);
	RESHADE_VULKAN_HOOK_PROC(AcquireNextImageKHR);
	RESHADE_VULKAN_HOOK_PROC(QueuePresentKHR);
	RESHADE_VULKAN_HOOK_PROC(AcquireNextImage2KHR);

#if RESHADE_ADDON
	// VK_KHR_dynamic_rendering
	RESHADE_VULKAN_HOOK_PROC_OPTIONAL(CmdBeginRendering, KHR);
	RESHADE_VULKAN_HOOK_PROC_OPTIONAL(CmdEndRendering, KHR);
#endif

#if RESHADE_ADDON >= 2
	// VK_KHR_push_descriptor
	RESHADE_VULKAN_HOOK_PROC_OPTIONAL(CmdPushDescriptorSet, KHR);
	RESHADE_VULKAN_HOOK_PROC_OPTIONAL(CmdPushDescriptorSetWithTemplate, KHR);

	// VK_KHR_descriptor_update_template
	RESHADE_VULKAN_HOOK_PROC_OPTIONAL(CreateDescriptorUpdateTemplate, KHR);
	RESHADE_VULKAN_HOOK_PROC_OPTIONAL(DestroyDescriptorUpdateTemplate, KHR);
	RESHADE_VULKAN_HOOK_PROC_OPTIONAL(UpdateDescriptorSetWithTemplate, KHR);
#endif

#if RESHADE_ADDON
	// VK_KHR_create_renderpass2
	RESHADE_VULKAN_HOOK_PROC_OPTIONAL(CreateRenderPass2, KHR);
	RESHADE_VULKAN_HOOK_PROC_OPTIONAL(CmdBeginRenderPass2, KHR);
	RESHADE_VULKAN_HOOK_PROC_OPTIONAL(CmdNextSubpass2, KHR);
	RESHADE_VULKAN_HOOK_PROC_OPTIONAL(CmdEndRenderPass2, KHR);

	// VK_KHR_bind_memory2
	RESHADE_VULKAN_HOOK_PROC_OPTIONAL(BindBufferMemory2, KHR);
	RESHADE_VULKAN_HOOK_PROC_OPTIONAL(BindImageMemory2, KHR);

	// VK_KHR_draw_indirect_count
	RESHADE_VULKAN_HOOK_PROC_OPTIONAL(CmdDrawIndirectCount, KHR);
	RESHADE_VULKAN_HOOK_PROC_OPTIONAL(CmdDrawIndexedIndirectCount, KHR);
#endif

#if RESHADE_ADDON >= 2
	// VK_KHR_synchronization2
	RESHADE_VULKAN_HOOK_PROC_OPTIONAL(CmdPipelineBarrier2, KHR);
	RESHADE_VULKAN_HOOK_PROC_OPTIONAL(CmdWriteTimestamp2, KHR);
#endif
#if RESHADE_ADDON
	RESHADE_VULKAN_HOOK_PROC_OPTIONAL(QueueSubmit2, KHR);
#endif

#if RESHADE_ADDON >= 2
	// VK_KHR_copy_commands2
	RESHADE_VULKAN_HOOK_PROC_OPTIONAL(CmdCopyBuffer2, KHR);
	RESHADE_VULKAN_HOOK_PROC_OPTIONAL(CmdCopyImage2, KHR);
	RESHADE_VULKAN_HOOK_PROC_OPTIONAL(CmdCopyBufferToImage2, KHR);
	RESHADE_VULKAN_HOOK_PROC_OPTIONAL(CmdCopyImageToBuffer2, KHR);
	RESHADE_VULKAN_HOOK_PROC_OPTIONAL(CmdBlitImage2, KHR);
	RESHADE_VULKAN_HOOK_PROC_OPTIONAL(CmdResolveImage2, KHR);

	// VK_EXT_transform_feedback
	RESHADE_VULKAN_HOOK_PROC_OPTIONAL(CmdBindTransformFeedbackBuffersEXT, );
	RESHADE_VULKAN_HOOK_PROC_OPTIONAL(CmdBeginQueryIndexedEXT, );
	RESHADE_VULKAN_HOOK_PROC_OPTIONAL(CmdEndQueryIndexedEXT, );

	// VK_EXT_extended_dynamic_state
	RESHADE_VULKAN_HOOK_PROC_OPTIONAL(CmdBindVertexBuffers2, EXT);
#endif

#if RESHADE_ADDON
	// VK_EXT_full_screen_exclusive
	RESHADE_VULKAN_HOOK_PROC_OPTIONAL(AcquireFullScreenExclusiveModeEXT, );
	RESHADE_VULKAN_HOOK_PROC_OPTIONAL(ReleaseFullScreenExclusiveModeEXT, );

	// VK_EXT_multi_draw
	RESHADE_VULKAN_HOOK_PROC_OPTIONAL(CmdDrawMultiEXT, );
	RESHADE_VULKAN_HOOK_PROC_OPTIONAL(CmdDrawMultiIndexedEXT, );

	// VK_KHR_acceleration_structure
	RESHADE_VULKAN_HOOK_PROC_OPTIONAL(CreateAccelerationStructureKHR, );
	RESHADE_VULKAN_HOOK_PROC_OPTIONAL(DestroyAccelerationStructureKHR, );
#endif
#if RESHADE_ADDON >= 2
	RESHADE_VULKAN_HOOK_PROC_OPTIONAL(CmdBuildAccelerationStructuresKHR, );
	RESHADE_VULKAN_HOOK_PROC_OPTIONAL(CmdBuildAccelerationStructuresIndirectKHR, );
	RESHADE_VULKAN_HOOK_PROC_OPTIONAL(CmdCopyAccelerationStructureKHR, );
	RESHADE_VULKAN_HOOK_PROC_OPTIONAL(CmdWriteAccelerationStructuresPropertiesKHR, );

	// VK_KHR_ray_tracing_pipeline
	RESHADE_VULKAN_HOOK_PROC_OPTIONAL(CmdTraceRaysKHR, );
	RESHADE_VULKAN_HOOK_PROC_OPTIONAL(CreateRayTracingPipelinesKHR, );
	RESHADE_VULKAN_HOOK_PROC_OPTIONAL(CmdTraceRaysIndirectKHR, );

	// VK_KHR_ray_tracing_maintenance1
	RESHADE_VULKAN_HOOK_PROC_OPTIONAL(CmdTraceRaysIndirect2KHR, );

	// VK_EXT_mesh_shader
	RESHADE_VULKAN_HOOK_PROC_OPTIONAL(CmdDrawMeshTasksEXT, );
	RESHADE_VULKAN_HOOK_PROC_OPTIONAL(CmdDrawMeshTasksIndirectEXT, );
	RESHADE_VULKAN_HOOK_PROC_OPTIONAL(CmdDrawMeshTasksIndirectCountEXT, );
#endif

	// Need to self-intercept as well, since some layers rely on this (e.g. Steam overlay)
	// See https://github.com/KhronosGroup/Vulkan-Loader/blob/master/loader/LoaderAndLayerInterface.md#layer-conventions-and-rules
	RESHADE_VULKAN_HOOK_PROC(GetDeviceProcAddr);

#ifdef RESHADE_TEST_APPLICATION
	static const auto trampoline = reshade::hooks::call(vkGetDeviceProcAddr);
#else
	if (device == VK_NULL_HANDLE)
		return nullptr;

	const auto trampoline = g_vulkan_devices.at(dispatch_key_from_handle(device))->_dispatch_table.GetDeviceProcAddr;
#endif
	return trampoline(device, pName);
}

PFN_vkVoidFunction VKAPI_CALL vkGetInstanceProcAddr(VkInstance instance, const char *pName)
{
	if (pName == nullptr || pName[0] != 'v' || pName[1] != 'k')
		return nullptr;
	const char *name_without_prefix = pName + 2;

	// Core 1_0
	RESHADE_VULKAN_HOOK_PROC(CreateInstance);
	RESHADE_VULKAN_HOOK_PROC(DestroyInstance);
	RESHADE_VULKAN_HOOK_PROC(CreateDevice);
	RESHADE_VULKAN_HOOK_PROC(DestroyDevice);

	// Core 1_3
	RESHADE_VULKAN_HOOK_PROC(GetPhysicalDeviceToolProperties);

	// VK_KHR_surface
	RESHADE_VULKAN_HOOK_PROC(CreateWin32SurfaceKHR);
	RESHADE_VULKAN_HOOK_PROC(DestroySurfaceKHR);

	// VK_EXT_tooling_info
	RESHADE_VULKAN_HOOK_PROC(GetPhysicalDeviceToolPropertiesEXT);

	// Self-intercept here as well to stay consistent with 'vkGetDeviceProcAddr' implementation
	RESHADE_VULKAN_HOOK_PROC(GetInstanceProcAddr);

#ifdef RESHADE_TEST_APPLICATION
	static const auto trampoline = reshade::hooks::call(vkGetInstanceProcAddr);
#else
	if (instance == VK_NULL_HANDLE)
		return nullptr;

	const auto trampoline = g_vulkan_instances.at(dispatch_key_from_handle(instance)).dispatch_table.GetInstanceProcAddr;
#endif
	return trampoline(instance, pName);
}

enum VkNegotiateLayerStructType
{
	LAYER_NEGOTIATE_UNINTIALIZED = 0,
	LAYER_NEGOTIATE_INTERFACE_STRUCT = 1,
};

struct VkNegotiateLayerInterface
{
	VkNegotiateLayerStructType sType;
	void *pNext;
	uint32_t loaderLayerInterfaceVersion;
	PFN_vkGetInstanceProcAddr pfnGetInstanceProcAddr;
	PFN_vkGetDeviceProcAddr pfnGetDeviceProcAddr;
	PFN_vkGetInstanceProcAddr pfnGetPhysicalDeviceProcAddr;
};

extern "C" VkResult VKAPI_CALL vkNegotiateLoaderLayerInterfaceVersion(VkNegotiateLayerInterface *pVersionStruct)
{
	if (pVersionStruct == nullptr ||
		pVersionStruct->sType != LAYER_NEGOTIATE_INTERFACE_STRUCT)
		return VK_ERROR_INITIALIZATION_FAILED;

	pVersionStruct->loaderLayerInterfaceVersion = 2; // Version 2 added 'vkNegotiateLoaderLayerInterfaceVersion'
	pVersionStruct->pfnGetInstanceProcAddr = vkGetInstanceProcAddr;
	pVersionStruct->pfnGetDeviceProcAddr = vkGetDeviceProcAddr;
	pVersionStruct->pfnGetPhysicalDeviceProcAddr = nullptr;

	return VK_SUCCESS;
}
