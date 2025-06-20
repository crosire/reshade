/*
 * Copyright (C) 2014 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause OR MIT
 */

#include "vulkan_hooks.hpp"
#include "vulkan_impl_device.hpp"
#include "hook_manager.hpp"
#include "lockfree_linear_map.hpp"
#include <cstring> // std::strcmp

extern lockfree_linear_map<void *, instance_dispatch_table, 16> g_vulkan_instances;
extern lockfree_linear_map<void *, reshade::vulkan::device_impl *, 8> g_vulkan_devices;

#define HOOK_PROC(name) \
	if (0 == std::strcmp(pName, "vk" #name)) \
		return reinterpret_cast<PFN_vkVoidFunction>(vk##name)
#define HOOK_PROC_OPTIONAL(name, suffix) \
	if (0 == std::strcmp(pName, "vk" #name #suffix) && g_vulkan_devices.at(dispatch_key_from_handle(device))->_dispatch_table.name != nullptr) \
		return reinterpret_cast<PFN_vkVoidFunction>(vk##name);

PFN_vkVoidFunction VKAPI_CALL vkGetDeviceProcAddr(VkDevice device, const char *pName)
{
	// The Vulkan loader gets the 'vkDestroyDevice' function from the device dispatch table
	HOOK_PROC(DestroyDevice);

	// Core 1_0
#if RESHADE_ADDON
	HOOK_PROC(QueueSubmit);
	HOOK_PROC(BindBufferMemory);
	HOOK_PROC(BindImageMemory);
	HOOK_PROC(CreateQueryPool);
	HOOK_PROC(DestroyQueryPool);
#endif
#if RESHADE_ADDON >= 2
	HOOK_PROC(GetQueryPoolResults);
#endif
#if RESHADE_ADDON
	HOOK_PROC(CreateBuffer);
	HOOK_PROC(DestroyBuffer);
	HOOK_PROC(CreateBufferView);
	HOOK_PROC(DestroyBufferView);
	HOOK_PROC(CreateImage);
	HOOK_PROC(DestroyImage);
	HOOK_PROC(CreateImageView);
	HOOK_PROC(DestroyImageView);
#endif
#if RESHADE_ADDON >= 2
	HOOK_PROC(CreateShaderModule);
	HOOK_PROC(DestroyShaderModule);
	HOOK_PROC(CreateGraphicsPipelines);
	HOOK_PROC(CreateComputePipelines);
	HOOK_PROC(DestroyPipeline);
	HOOK_PROC(CreatePipelineLayout);
	HOOK_PROC(DestroyPipelineLayout);
#endif
#if RESHADE_ADDON
	HOOK_PROC(CreateSampler);
	HOOK_PROC(DestroySampler);
#endif
#if RESHADE_ADDON >= 2
	HOOK_PROC(CreateDescriptorSetLayout);
	HOOK_PROC(DestroyDescriptorSetLayout);
	HOOK_PROC(CreateDescriptorPool);
	HOOK_PROC(DestroyDescriptorPool);
	HOOK_PROC(ResetDescriptorPool);
	HOOK_PROC(AllocateDescriptorSets);
	HOOK_PROC(FreeDescriptorSets);
	HOOK_PROC(UpdateDescriptorSets);
#endif
#if RESHADE_ADDON
	HOOK_PROC(CreateFramebuffer);
	HOOK_PROC(DestroyFramebuffer);
	HOOK_PROC(CreateRenderPass);
	HOOK_PROC(DestroyRenderPass);

	HOOK_PROC(AllocateCommandBuffers);
	HOOK_PROC(FreeCommandBuffers);
	HOOK_PROC(BeginCommandBuffer);
	HOOK_PROC(EndCommandBuffer);
#endif
#if RESHADE_ADDON >= 2
	HOOK_PROC(CmdBindPipeline);
#endif
#if RESHADE_ADDON
	HOOK_PROC(CmdSetViewport);
	HOOK_PROC(CmdSetScissor);
#endif
#if RESHADE_ADDON >= 2
	HOOK_PROC(CmdSetDepthBias);
	HOOK_PROC(CmdSetBlendConstants);
	HOOK_PROC(CmdSetStencilCompareMask);
	HOOK_PROC(CmdSetStencilWriteMask);
	HOOK_PROC(CmdSetStencilReference);
	HOOK_PROC(CmdBindDescriptorSets);
	HOOK_PROC(CmdBindIndexBuffer);
	HOOK_PROC(CmdBindVertexBuffers);
#endif
#if RESHADE_ADDON
	HOOK_PROC(CmdDraw);
	HOOK_PROC(CmdDrawIndexed);
	HOOK_PROC(CmdDrawIndirect);
	HOOK_PROC(CmdDrawIndexedIndirect);
	HOOK_PROC(CmdDispatch);
	HOOK_PROC(CmdDispatchIndirect);
#endif
#if RESHADE_ADDON >= 2
	HOOK_PROC(CmdCopyBuffer);
	HOOK_PROC(CmdCopyImage);
	HOOK_PROC(CmdBlitImage);
	HOOK_PROC(CmdCopyBufferToImage);
	HOOK_PROC(CmdCopyImageToBuffer);
#endif
#if RESHADE_ADDON
	HOOK_PROC(CmdClearColorImage);
	HOOK_PROC(CmdClearDepthStencilImage);
	HOOK_PROC(CmdClearAttachments);
#endif
#if RESHADE_ADDON >= 2
	HOOK_PROC(CmdResolveImage);
	HOOK_PROC(CmdPipelineBarrier);
	HOOK_PROC(CmdBeginQuery);
	HOOK_PROC(CmdEndQuery);
	HOOK_PROC(CmdWriteTimestamp);
	HOOK_PROC(CmdCopyQueryPoolResults);
	HOOK_PROC(CmdPushConstants);
#endif
#if RESHADE_ADDON
	HOOK_PROC(CmdBeginRenderPass);
	HOOK_PROC(CmdNextSubpass);
	HOOK_PROC(CmdEndRenderPass);
	HOOK_PROC(CmdExecuteCommands);

	// Core 1_1
	HOOK_PROC(BindBufferMemory2);
	HOOK_PROC(BindImageMemory2);
	HOOK_PROC(CreateDescriptorUpdateTemplate);
	HOOK_PROC(DestroyDescriptorUpdateTemplate);
	HOOK_PROC(UpdateDescriptorSetWithTemplate);

	// Core 1_2
	HOOK_PROC(CmdDrawIndirectCount);
	HOOK_PROC(CmdDrawIndexedIndirectCount);
	HOOK_PROC(CreateRenderPass2);
	HOOK_PROC(CmdBeginRenderPass2);
	HOOK_PROC(CmdNextSubpass2);
	HOOK_PROC(CmdEndRenderPass2);
#endif

	// Core 1_3
#if RESHADE_ADDON >= 2
	HOOK_PROC_OPTIONAL(CmdPipelineBarrier2,);
	HOOK_PROC_OPTIONAL(CmdWriteTimestamp2,);
#endif
#if RESHADE_ADDON
	HOOK_PROC_OPTIONAL(QueueSubmit2,);
#endif
#if RESHADE_ADDON >= 2
	HOOK_PROC_OPTIONAL(CmdCopyBuffer2,);
	HOOK_PROC_OPTIONAL(CmdCopyImage2,);
	HOOK_PROC_OPTIONAL(CmdCopyBufferToImage2,);
	HOOK_PROC_OPTIONAL(CmdCopyImageToBuffer2,);
	HOOK_PROC_OPTIONAL(CmdBlitImage2,);
	HOOK_PROC_OPTIONAL(CmdResolveImage2,);
#endif
#if RESHADE_ADDON
	HOOK_PROC_OPTIONAL(CmdBeginRendering,);
	HOOK_PROC_OPTIONAL(CmdEndRendering,);
#endif
#if RESHADE_ADDON >= 2
	HOOK_PROC_OPTIONAL(CmdBindVertexBuffers2,);
#endif

	// VK_KHR_swapchain
	HOOK_PROC(CreateSwapchainKHR);
	HOOK_PROC(DestroySwapchainKHR);
	HOOK_PROC(AcquireNextImageKHR);
	HOOK_PROC(QueuePresentKHR);
	HOOK_PROC(AcquireNextImage2KHR);

#if RESHADE_ADDON
	// VK_KHR_dynamic_rendering
	HOOK_PROC_OPTIONAL(CmdBeginRendering, KHR);
	HOOK_PROC_OPTIONAL(CmdEndRendering, KHR);
#endif

#if RESHADE_ADDON >= 2
	// VK_KHR_push_descriptor
	HOOK_PROC_OPTIONAL(CmdPushDescriptorSetKHR,);
	HOOK_PROC_OPTIONAL(CmdPushDescriptorSetWithTemplateKHR,);

	// VK_KHR_descriptor_update_template
	HOOK_PROC_OPTIONAL(CreateDescriptorUpdateTemplate, KHR);
	HOOK_PROC_OPTIONAL(DestroyDescriptorUpdateTemplate, KHR);
	HOOK_PROC_OPTIONAL(UpdateDescriptorSetWithTemplate, KHR);
#endif

#if RESHADE_ADDON
	// VK_KHR_create_renderpass2
	HOOK_PROC_OPTIONAL(CreateRenderPass2, KHR);
	HOOK_PROC_OPTIONAL(CmdBeginRenderPass2, KHR);
	HOOK_PROC_OPTIONAL(CmdNextSubpass2, KHR);
	HOOK_PROC_OPTIONAL(CmdEndRenderPass2, KHR);

	// VK_KHR_bind_memory2
	HOOK_PROC_OPTIONAL(BindBufferMemory2, KHR);
	HOOK_PROC_OPTIONAL(BindImageMemory2, KHR);

	// VK_KHR_draw_indirect_count
	HOOK_PROC_OPTIONAL(CmdDrawIndirectCount, KHR);
	HOOK_PROC_OPTIONAL(CmdDrawIndexedIndirectCount, KHR);
#endif

#if RESHADE_ADDON >= 2
	// VK_KHR_synchronization2
	HOOK_PROC_OPTIONAL(CmdPipelineBarrier2, KHR);
	HOOK_PROC_OPTIONAL(CmdWriteTimestamp2, KHR);
#endif
#if RESHADE_ADDON
	HOOK_PROC_OPTIONAL(QueueSubmit2, KHR);
#endif

#if RESHADE_ADDON >= 2
	// VK_KHR_copy_commands2
	HOOK_PROC_OPTIONAL(CmdCopyBuffer2, KHR);
	HOOK_PROC_OPTIONAL(CmdCopyImage2, KHR);
	HOOK_PROC_OPTIONAL(CmdCopyBufferToImage2, KHR);
	HOOK_PROC_OPTIONAL(CmdCopyImageToBuffer2, KHR);
	HOOK_PROC_OPTIONAL(CmdBlitImage2, KHR);
	HOOK_PROC_OPTIONAL(CmdResolveImage2, KHR);

	// VK_EXT_transform_feedback
	HOOK_PROC_OPTIONAL(CmdBindTransformFeedbackBuffersEXT,);
	HOOK_PROC_OPTIONAL(CmdBeginQueryIndexedEXT,);
	HOOK_PROC_OPTIONAL(CmdEndQueryIndexedEXT,);

	// VK_EXT_extended_dynamic_state
	HOOK_PROC_OPTIONAL(CmdBindVertexBuffers2, EXT);
#endif

#if RESHADE_ADDON
	// VK_EXT_full_screen_exclusive
	HOOK_PROC_OPTIONAL(AcquireFullScreenExclusiveModeEXT,);
	HOOK_PROC_OPTIONAL(ReleaseFullScreenExclusiveModeEXT,);

	// VK_KHR_acceleration_structure
	HOOK_PROC_OPTIONAL(CreateAccelerationStructureKHR,);
	HOOK_PROC_OPTIONAL(DestroyAccelerationStructureKHR,);
#endif
#if RESHADE_ADDON >= 2
	HOOK_PROC_OPTIONAL(CmdBuildAccelerationStructuresKHR,);
	HOOK_PROC_OPTIONAL(CmdBuildAccelerationStructuresIndirectKHR,);
	HOOK_PROC_OPTIONAL(CmdCopyAccelerationStructureKHR,);
	HOOK_PROC_OPTIONAL(CmdWriteAccelerationStructuresPropertiesKHR,);

	// VK_KHR_ray_tracing_pipeline
	HOOK_PROC_OPTIONAL(CmdTraceRaysKHR,);
	HOOK_PROC_OPTIONAL(CreateRayTracingPipelinesKHR,);
	HOOK_PROC_OPTIONAL(CmdTraceRaysIndirectKHR,);

	// VK_KHR_ray_tracing_maintenance1
	HOOK_PROC_OPTIONAL(CmdTraceRaysIndirect2KHR,);

	// VK_EXT_mesh_shader
	HOOK_PROC_OPTIONAL(CmdDrawMeshTasksEXT,);
	HOOK_PROC_OPTIONAL(CmdDrawMeshTasksIndirectEXT,);
	HOOK_PROC_OPTIONAL(CmdDrawMeshTasksIndirectCountEXT,);
#endif

	// Need to self-intercept as well, since some layers rely on this (e.g. Steam overlay)
	// See https://github.com/KhronosGroup/Vulkan-Loader/blob/master/loader/LoaderAndLayerInterface.md#layer-conventions-and-rules
	HOOK_PROC(GetDeviceProcAddr);

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
	// Core 1_0
	HOOK_PROC(CreateInstance);
	HOOK_PROC(DestroyInstance);
	HOOK_PROC(CreateDevice);
	HOOK_PROC(DestroyDevice);

	// Core 1_3
	HOOK_PROC(GetPhysicalDeviceToolProperties);

	// VK_KHR_surface
	HOOK_PROC(CreateWin32SurfaceKHR);
	HOOK_PROC(DestroySurfaceKHR);

	// VK_EXT_tooling_info
	HOOK_PROC(GetPhysicalDeviceToolPropertiesEXT);

	// Self-intercept here as well to stay consistent with 'vkGetDeviceProcAddr' implementation
	HOOK_PROC(GetInstanceProcAddr);

#ifdef RESHADE_TEST_APPLICATION
	static const auto trampoline = reshade::hooks::call(vkGetInstanceProcAddr);
#else
	if (instance == VK_NULL_HANDLE)
		return nullptr;

	const auto trampoline = g_vulkan_instances.at(dispatch_key_from_handle(instance)).GetInstanceProcAddr;
#endif
	return trampoline(instance, pName);
}

VkResult VKAPI_CALL vkNegotiateLoaderLayerInterfaceVersion(VkNegotiateLayerInterface *pVersionStruct)
{
	if (pVersionStruct == nullptr ||
		pVersionStruct->sType != LAYER_NEGOTIATE_INTERFACE_STRUCT)
		return VK_ERROR_INITIALIZATION_FAILED;

	pVersionStruct->loaderLayerInterfaceVersion = CURRENT_LOADER_LAYER_INTERFACE_VERSION;
	pVersionStruct->pfnGetDeviceProcAddr = vkGetDeviceProcAddr;
	pVersionStruct->pfnGetInstanceProcAddr = vkGetInstanceProcAddr;
	pVersionStruct->pfnGetPhysicalDeviceProcAddr = nullptr;

	return VK_SUCCESS;
}
