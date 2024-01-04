// *** THIS FILE IS GENERATED - DO NOT EDIT ***
// See layer_dispatch_table_generator.py for modifications

/***************************************************************************
 *
 * Copyright (c) 2015-2023 The Khronos Group Inc.
 * Copyright (c) 2015-2023 Valve Corporation
 * Copyright (c) 2015-2023 LunarG, Inc.
 * Copyright (c) 2015-2023 Google Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 ****************************************************************************/
// NOLINTBEGIN// clang-format off
#pragma once

typedef PFN_vkVoidFunction(VKAPI_PTR* PFN_GetPhysicalDeviceProcAddr)(VkInstance instance, const char* pName);

// Instance function pointer dispatch table
typedef struct VkLayerInstanceDispatchTable_ {
    PFN_GetPhysicalDeviceProcAddr GetPhysicalDeviceProcAddr;

    PFN_vkCreateInstance CreateInstance;
    PFN_vkDestroyInstance DestroyInstance;
    PFN_vkEnumeratePhysicalDevices EnumeratePhysicalDevices;
    PFN_vkGetPhysicalDeviceFeatures GetPhysicalDeviceFeatures;
    PFN_vkGetPhysicalDeviceFormatProperties GetPhysicalDeviceFormatProperties;
    PFN_vkGetPhysicalDeviceImageFormatProperties GetPhysicalDeviceImageFormatProperties;
    PFN_vkGetPhysicalDeviceProperties GetPhysicalDeviceProperties;
    PFN_vkGetPhysicalDeviceQueueFamilyProperties GetPhysicalDeviceQueueFamilyProperties;
    PFN_vkGetPhysicalDeviceMemoryProperties GetPhysicalDeviceMemoryProperties;
    PFN_vkGetInstanceProcAddr GetInstanceProcAddr;
    PFN_vkCreateDevice CreateDevice;
    PFN_vkEnumerateInstanceExtensionProperties EnumerateInstanceExtensionProperties;
    PFN_vkEnumerateDeviceExtensionProperties EnumerateDeviceExtensionProperties;
    PFN_vkEnumerateInstanceLayerProperties EnumerateInstanceLayerProperties;
    PFN_vkEnumerateDeviceLayerProperties EnumerateDeviceLayerProperties;
    PFN_vkGetPhysicalDeviceSparseImageFormatProperties GetPhysicalDeviceSparseImageFormatProperties;
    PFN_vkEnumerateInstanceVersion EnumerateInstanceVersion;
    PFN_vkEnumeratePhysicalDeviceGroups EnumeratePhysicalDeviceGroups;
    PFN_vkGetPhysicalDeviceFeatures2 GetPhysicalDeviceFeatures2;
    PFN_vkGetPhysicalDeviceProperties2 GetPhysicalDeviceProperties2;
    PFN_vkGetPhysicalDeviceFormatProperties2 GetPhysicalDeviceFormatProperties2;
    PFN_vkGetPhysicalDeviceImageFormatProperties2 GetPhysicalDeviceImageFormatProperties2;
    PFN_vkGetPhysicalDeviceQueueFamilyProperties2 GetPhysicalDeviceQueueFamilyProperties2;
    PFN_vkGetPhysicalDeviceMemoryProperties2 GetPhysicalDeviceMemoryProperties2;
    PFN_vkGetPhysicalDeviceSparseImageFormatProperties2 GetPhysicalDeviceSparseImageFormatProperties2;
    PFN_vkGetPhysicalDeviceExternalBufferProperties GetPhysicalDeviceExternalBufferProperties;
    PFN_vkGetPhysicalDeviceExternalFenceProperties GetPhysicalDeviceExternalFenceProperties;
    PFN_vkGetPhysicalDeviceExternalSemaphoreProperties GetPhysicalDeviceExternalSemaphoreProperties;
    PFN_vkGetPhysicalDeviceToolProperties GetPhysicalDeviceToolProperties;
    PFN_vkDestroySurfaceKHR DestroySurfaceKHR;
    PFN_vkGetPhysicalDeviceSurfaceSupportKHR GetPhysicalDeviceSurfaceSupportKHR;
    PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR GetPhysicalDeviceSurfaceCapabilitiesKHR;
    PFN_vkGetPhysicalDeviceSurfaceFormatsKHR GetPhysicalDeviceSurfaceFormatsKHR;
    PFN_vkGetPhysicalDeviceSurfacePresentModesKHR GetPhysicalDeviceSurfacePresentModesKHR;
    PFN_vkGetPhysicalDevicePresentRectanglesKHR GetPhysicalDevicePresentRectanglesKHR;
    PFN_vkGetPhysicalDeviceDisplayPropertiesKHR GetPhysicalDeviceDisplayPropertiesKHR;
    PFN_vkGetPhysicalDeviceDisplayPlanePropertiesKHR GetPhysicalDeviceDisplayPlanePropertiesKHR;
    PFN_vkGetDisplayPlaneSupportedDisplaysKHR GetDisplayPlaneSupportedDisplaysKHR;
    PFN_vkGetDisplayModePropertiesKHR GetDisplayModePropertiesKHR;
    PFN_vkCreateDisplayModeKHR CreateDisplayModeKHR;
    PFN_vkGetDisplayPlaneCapabilitiesKHR GetDisplayPlaneCapabilitiesKHR;
    PFN_vkCreateDisplayPlaneSurfaceKHR CreateDisplayPlaneSurfaceKHR;
#ifdef VK_USE_PLATFORM_XLIB_KHR
    PFN_vkCreateXlibSurfaceKHR CreateXlibSurfaceKHR;
#endif  // VK_USE_PLATFORM_XLIB_KHR
#ifdef VK_USE_PLATFORM_XLIB_KHR
    PFN_vkGetPhysicalDeviceXlibPresentationSupportKHR GetPhysicalDeviceXlibPresentationSupportKHR;
#endif  // VK_USE_PLATFORM_XLIB_KHR
#ifdef VK_USE_PLATFORM_XCB_KHR
    PFN_vkCreateXcbSurfaceKHR CreateXcbSurfaceKHR;
#endif  // VK_USE_PLATFORM_XCB_KHR
#ifdef VK_USE_PLATFORM_XCB_KHR
    PFN_vkGetPhysicalDeviceXcbPresentationSupportKHR GetPhysicalDeviceXcbPresentationSupportKHR;
#endif  // VK_USE_PLATFORM_XCB_KHR
#ifdef VK_USE_PLATFORM_WAYLAND_KHR
    PFN_vkCreateWaylandSurfaceKHR CreateWaylandSurfaceKHR;
#endif  // VK_USE_PLATFORM_WAYLAND_KHR
#ifdef VK_USE_PLATFORM_WAYLAND_KHR
    PFN_vkGetPhysicalDeviceWaylandPresentationSupportKHR GetPhysicalDeviceWaylandPresentationSupportKHR;
#endif  // VK_USE_PLATFORM_WAYLAND_KHR
#ifdef VK_USE_PLATFORM_ANDROID_KHR
    PFN_vkCreateAndroidSurfaceKHR CreateAndroidSurfaceKHR;
#endif  // VK_USE_PLATFORM_ANDROID_KHR
#ifdef VK_USE_PLATFORM_WIN32_KHR
    PFN_vkCreateWin32SurfaceKHR CreateWin32SurfaceKHR;
#endif  // VK_USE_PLATFORM_WIN32_KHR
#ifdef VK_USE_PLATFORM_WIN32_KHR
    PFN_vkGetPhysicalDeviceWin32PresentationSupportKHR GetPhysicalDeviceWin32PresentationSupportKHR;
#endif  // VK_USE_PLATFORM_WIN32_KHR
    PFN_vkGetPhysicalDeviceVideoCapabilitiesKHR GetPhysicalDeviceVideoCapabilitiesKHR;
    PFN_vkGetPhysicalDeviceVideoFormatPropertiesKHR GetPhysicalDeviceVideoFormatPropertiesKHR;
    PFN_vkGetPhysicalDeviceFeatures2KHR GetPhysicalDeviceFeatures2KHR;
    PFN_vkGetPhysicalDeviceProperties2KHR GetPhysicalDeviceProperties2KHR;
    PFN_vkGetPhysicalDeviceFormatProperties2KHR GetPhysicalDeviceFormatProperties2KHR;
    PFN_vkGetPhysicalDeviceImageFormatProperties2KHR GetPhysicalDeviceImageFormatProperties2KHR;
    PFN_vkGetPhysicalDeviceQueueFamilyProperties2KHR GetPhysicalDeviceQueueFamilyProperties2KHR;
    PFN_vkGetPhysicalDeviceMemoryProperties2KHR GetPhysicalDeviceMemoryProperties2KHR;
    PFN_vkGetPhysicalDeviceSparseImageFormatProperties2KHR GetPhysicalDeviceSparseImageFormatProperties2KHR;
    PFN_vkEnumeratePhysicalDeviceGroupsKHR EnumeratePhysicalDeviceGroupsKHR;
    PFN_vkGetPhysicalDeviceExternalBufferPropertiesKHR GetPhysicalDeviceExternalBufferPropertiesKHR;
    PFN_vkGetPhysicalDeviceExternalSemaphorePropertiesKHR GetPhysicalDeviceExternalSemaphorePropertiesKHR;
    PFN_vkGetPhysicalDeviceExternalFencePropertiesKHR GetPhysicalDeviceExternalFencePropertiesKHR;
    PFN_vkEnumeratePhysicalDeviceQueueFamilyPerformanceQueryCountersKHR
        EnumeratePhysicalDeviceQueueFamilyPerformanceQueryCountersKHR;
    PFN_vkGetPhysicalDeviceQueueFamilyPerformanceQueryPassesKHR GetPhysicalDeviceQueueFamilyPerformanceQueryPassesKHR;
    PFN_vkGetPhysicalDeviceSurfaceCapabilities2KHR GetPhysicalDeviceSurfaceCapabilities2KHR;
    PFN_vkGetPhysicalDeviceSurfaceFormats2KHR GetPhysicalDeviceSurfaceFormats2KHR;
    PFN_vkGetPhysicalDeviceDisplayProperties2KHR GetPhysicalDeviceDisplayProperties2KHR;
    PFN_vkGetPhysicalDeviceDisplayPlaneProperties2KHR GetPhysicalDeviceDisplayPlaneProperties2KHR;
    PFN_vkGetDisplayModeProperties2KHR GetDisplayModeProperties2KHR;
    PFN_vkGetDisplayPlaneCapabilities2KHR GetDisplayPlaneCapabilities2KHR;
    PFN_vkGetPhysicalDeviceFragmentShadingRatesKHR GetPhysicalDeviceFragmentShadingRatesKHR;
#ifdef VK_ENABLE_BETA_EXTENSIONS
    PFN_vkGetPhysicalDeviceVideoEncodeQualityLevelPropertiesKHR GetPhysicalDeviceVideoEncodeQualityLevelPropertiesKHR;
#endif  // VK_ENABLE_BETA_EXTENSIONS
    PFN_vkGetPhysicalDeviceCooperativeMatrixPropertiesKHR GetPhysicalDeviceCooperativeMatrixPropertiesKHR;
    PFN_vkCreateDebugReportCallbackEXT CreateDebugReportCallbackEXT;
    PFN_vkDestroyDebugReportCallbackEXT DestroyDebugReportCallbackEXT;
    PFN_vkDebugReportMessageEXT DebugReportMessageEXT;
#ifdef VK_USE_PLATFORM_GGP
    PFN_vkCreateStreamDescriptorSurfaceGGP CreateStreamDescriptorSurfaceGGP;
#endif  // VK_USE_PLATFORM_GGP
    PFN_vkGetPhysicalDeviceExternalImageFormatPropertiesNV GetPhysicalDeviceExternalImageFormatPropertiesNV;
#ifdef VK_USE_PLATFORM_VI_NN
    PFN_vkCreateViSurfaceNN CreateViSurfaceNN;
#endif  // VK_USE_PLATFORM_VI_NN
    PFN_vkReleaseDisplayEXT ReleaseDisplayEXT;
#ifdef VK_USE_PLATFORM_XLIB_XRANDR_EXT
    PFN_vkAcquireXlibDisplayEXT AcquireXlibDisplayEXT;
#endif  // VK_USE_PLATFORM_XLIB_XRANDR_EXT
#ifdef VK_USE_PLATFORM_XLIB_XRANDR_EXT
    PFN_vkGetRandROutputDisplayEXT GetRandROutputDisplayEXT;
#endif  // VK_USE_PLATFORM_XLIB_XRANDR_EXT
    PFN_vkGetPhysicalDeviceSurfaceCapabilities2EXT GetPhysicalDeviceSurfaceCapabilities2EXT;
#ifdef VK_USE_PLATFORM_IOS_MVK
    PFN_vkCreateIOSSurfaceMVK CreateIOSSurfaceMVK;
#endif  // VK_USE_PLATFORM_IOS_MVK
#ifdef VK_USE_PLATFORM_MACOS_MVK
    PFN_vkCreateMacOSSurfaceMVK CreateMacOSSurfaceMVK;
#endif  // VK_USE_PLATFORM_MACOS_MVK
    PFN_vkCreateDebugUtilsMessengerEXT CreateDebugUtilsMessengerEXT;
    PFN_vkDestroyDebugUtilsMessengerEXT DestroyDebugUtilsMessengerEXT;
    PFN_vkSubmitDebugUtilsMessageEXT SubmitDebugUtilsMessageEXT;
    PFN_vkGetPhysicalDeviceMultisamplePropertiesEXT GetPhysicalDeviceMultisamplePropertiesEXT;
    PFN_vkGetPhysicalDeviceCalibrateableTimeDomainsEXT GetPhysicalDeviceCalibrateableTimeDomainsEXT;
#ifdef VK_USE_PLATFORM_FUCHSIA
    PFN_vkCreateImagePipeSurfaceFUCHSIA CreateImagePipeSurfaceFUCHSIA;
#endif  // VK_USE_PLATFORM_FUCHSIA
#ifdef VK_USE_PLATFORM_METAL_EXT
    PFN_vkCreateMetalSurfaceEXT CreateMetalSurfaceEXT;
#endif  // VK_USE_PLATFORM_METAL_EXT
    PFN_vkGetPhysicalDeviceToolPropertiesEXT GetPhysicalDeviceToolPropertiesEXT;
    PFN_vkGetPhysicalDeviceCooperativeMatrixPropertiesNV GetPhysicalDeviceCooperativeMatrixPropertiesNV;
    PFN_vkGetPhysicalDeviceSupportedFramebufferMixedSamplesCombinationsNV
        GetPhysicalDeviceSupportedFramebufferMixedSamplesCombinationsNV;
#ifdef VK_USE_PLATFORM_WIN32_KHR
    PFN_vkGetPhysicalDeviceSurfacePresentModes2EXT GetPhysicalDeviceSurfacePresentModes2EXT;
#endif  // VK_USE_PLATFORM_WIN32_KHR
    PFN_vkCreateHeadlessSurfaceEXT CreateHeadlessSurfaceEXT;
    PFN_vkAcquireDrmDisplayEXT AcquireDrmDisplayEXT;
    PFN_vkGetDrmDisplayEXT GetDrmDisplayEXT;
#ifdef VK_USE_PLATFORM_WIN32_KHR
    PFN_vkAcquireWinrtDisplayNV AcquireWinrtDisplayNV;
#endif  // VK_USE_PLATFORM_WIN32_KHR
#ifdef VK_USE_PLATFORM_WIN32_KHR
    PFN_vkGetWinrtDisplayNV GetWinrtDisplayNV;
#endif  // VK_USE_PLATFORM_WIN32_KHR
#ifdef VK_USE_PLATFORM_DIRECTFB_EXT
    PFN_vkCreateDirectFBSurfaceEXT CreateDirectFBSurfaceEXT;
#endif  // VK_USE_PLATFORM_DIRECTFB_EXT
#ifdef VK_USE_PLATFORM_DIRECTFB_EXT
    PFN_vkGetPhysicalDeviceDirectFBPresentationSupportEXT GetPhysicalDeviceDirectFBPresentationSupportEXT;
#endif  // VK_USE_PLATFORM_DIRECTFB_EXT
#ifdef VK_USE_PLATFORM_SCREEN_QNX
    PFN_vkCreateScreenSurfaceQNX CreateScreenSurfaceQNX;
#endif  // VK_USE_PLATFORM_SCREEN_QNX
#ifdef VK_USE_PLATFORM_SCREEN_QNX
    PFN_vkGetPhysicalDeviceScreenPresentationSupportQNX GetPhysicalDeviceScreenPresentationSupportQNX;
#endif  // VK_USE_PLATFORM_SCREEN_QNX
    PFN_vkGetPhysicalDeviceOpticalFlowImageFormatsNV GetPhysicalDeviceOpticalFlowImageFormatsNV;
} VkLayerInstanceDispatchTable;

// Device function pointer dispatch table
typedef struct VkLayerDispatchTable_ {
    PFN_vkGetDeviceProcAddr GetDeviceProcAddr;
    PFN_vkDestroyDevice DestroyDevice;
    PFN_vkGetDeviceQueue GetDeviceQueue;
    PFN_vkQueueSubmit QueueSubmit;
    PFN_vkQueueWaitIdle QueueWaitIdle;
    PFN_vkDeviceWaitIdle DeviceWaitIdle;
    PFN_vkAllocateMemory AllocateMemory;
    PFN_vkFreeMemory FreeMemory;
    PFN_vkMapMemory MapMemory;
    PFN_vkUnmapMemory UnmapMemory;
    PFN_vkFlushMappedMemoryRanges FlushMappedMemoryRanges;
    PFN_vkInvalidateMappedMemoryRanges InvalidateMappedMemoryRanges;
    PFN_vkGetDeviceMemoryCommitment GetDeviceMemoryCommitment;
    PFN_vkBindBufferMemory BindBufferMemory;
    PFN_vkBindImageMemory BindImageMemory;
    PFN_vkGetBufferMemoryRequirements GetBufferMemoryRequirements;
    PFN_vkGetImageMemoryRequirements GetImageMemoryRequirements;
    PFN_vkGetImageSparseMemoryRequirements GetImageSparseMemoryRequirements;
    PFN_vkQueueBindSparse QueueBindSparse;
    PFN_vkCreateFence CreateFence;
    PFN_vkDestroyFence DestroyFence;
    PFN_vkResetFences ResetFences;
    PFN_vkGetFenceStatus GetFenceStatus;
    PFN_vkWaitForFences WaitForFences;
    PFN_vkCreateSemaphore CreateSemaphore;
    PFN_vkDestroySemaphore DestroySemaphore;
    PFN_vkCreateEvent CreateEvent;
    PFN_vkDestroyEvent DestroyEvent;
    PFN_vkGetEventStatus GetEventStatus;
    PFN_vkSetEvent SetEvent;
    PFN_vkResetEvent ResetEvent;
    PFN_vkCreateQueryPool CreateQueryPool;
    PFN_vkDestroyQueryPool DestroyQueryPool;
    PFN_vkGetQueryPoolResults GetQueryPoolResults;
    PFN_vkCreateBuffer CreateBuffer;
    PFN_vkDestroyBuffer DestroyBuffer;
    PFN_vkCreateBufferView CreateBufferView;
    PFN_vkDestroyBufferView DestroyBufferView;
    PFN_vkCreateImage CreateImage;
    PFN_vkDestroyImage DestroyImage;
    PFN_vkGetImageSubresourceLayout GetImageSubresourceLayout;
    PFN_vkCreateImageView CreateImageView;
    PFN_vkDestroyImageView DestroyImageView;
    PFN_vkCreateShaderModule CreateShaderModule;
    PFN_vkDestroyShaderModule DestroyShaderModule;
    PFN_vkCreatePipelineCache CreatePipelineCache;
    PFN_vkDestroyPipelineCache DestroyPipelineCache;
    PFN_vkGetPipelineCacheData GetPipelineCacheData;
    PFN_vkMergePipelineCaches MergePipelineCaches;
    PFN_vkCreateGraphicsPipelines CreateGraphicsPipelines;
    PFN_vkCreateComputePipelines CreateComputePipelines;
    PFN_vkDestroyPipeline DestroyPipeline;
    PFN_vkCreatePipelineLayout CreatePipelineLayout;
    PFN_vkDestroyPipelineLayout DestroyPipelineLayout;
    PFN_vkCreateSampler CreateSampler;
    PFN_vkDestroySampler DestroySampler;
    PFN_vkCreateDescriptorSetLayout CreateDescriptorSetLayout;
    PFN_vkDestroyDescriptorSetLayout DestroyDescriptorSetLayout;
    PFN_vkCreateDescriptorPool CreateDescriptorPool;
    PFN_vkDestroyDescriptorPool DestroyDescriptorPool;
    PFN_vkResetDescriptorPool ResetDescriptorPool;
    PFN_vkAllocateDescriptorSets AllocateDescriptorSets;
    PFN_vkFreeDescriptorSets FreeDescriptorSets;
    PFN_vkUpdateDescriptorSets UpdateDescriptorSets;
    PFN_vkCreateFramebuffer CreateFramebuffer;
    PFN_vkDestroyFramebuffer DestroyFramebuffer;
    PFN_vkCreateRenderPass CreateRenderPass;
    PFN_vkDestroyRenderPass DestroyRenderPass;
    PFN_vkGetRenderAreaGranularity GetRenderAreaGranularity;
    PFN_vkCreateCommandPool CreateCommandPool;
    PFN_vkDestroyCommandPool DestroyCommandPool;
    PFN_vkResetCommandPool ResetCommandPool;
    PFN_vkAllocateCommandBuffers AllocateCommandBuffers;
    PFN_vkFreeCommandBuffers FreeCommandBuffers;
    PFN_vkBeginCommandBuffer BeginCommandBuffer;
    PFN_vkEndCommandBuffer EndCommandBuffer;
    PFN_vkResetCommandBuffer ResetCommandBuffer;
    PFN_vkCmdBindPipeline CmdBindPipeline;
    PFN_vkCmdSetViewport CmdSetViewport;
    PFN_vkCmdSetScissor CmdSetScissor;
    PFN_vkCmdSetLineWidth CmdSetLineWidth;
    PFN_vkCmdSetDepthBias CmdSetDepthBias;
    PFN_vkCmdSetBlendConstants CmdSetBlendConstants;
    PFN_vkCmdSetDepthBounds CmdSetDepthBounds;
    PFN_vkCmdSetStencilCompareMask CmdSetStencilCompareMask;
    PFN_vkCmdSetStencilWriteMask CmdSetStencilWriteMask;
    PFN_vkCmdSetStencilReference CmdSetStencilReference;
    PFN_vkCmdBindDescriptorSets CmdBindDescriptorSets;
    PFN_vkCmdBindIndexBuffer CmdBindIndexBuffer;
    PFN_vkCmdBindVertexBuffers CmdBindVertexBuffers;
    PFN_vkCmdDraw CmdDraw;
    PFN_vkCmdDrawIndexed CmdDrawIndexed;
    PFN_vkCmdDrawIndirect CmdDrawIndirect;
    PFN_vkCmdDrawIndexedIndirect CmdDrawIndexedIndirect;
    PFN_vkCmdDispatch CmdDispatch;
    PFN_vkCmdDispatchIndirect CmdDispatchIndirect;
    PFN_vkCmdCopyBuffer CmdCopyBuffer;
    PFN_vkCmdCopyImage CmdCopyImage;
    PFN_vkCmdBlitImage CmdBlitImage;
    PFN_vkCmdCopyBufferToImage CmdCopyBufferToImage;
    PFN_vkCmdCopyImageToBuffer CmdCopyImageToBuffer;
    PFN_vkCmdUpdateBuffer CmdUpdateBuffer;
    PFN_vkCmdFillBuffer CmdFillBuffer;
    PFN_vkCmdClearColorImage CmdClearColorImage;
    PFN_vkCmdClearDepthStencilImage CmdClearDepthStencilImage;
    PFN_vkCmdClearAttachments CmdClearAttachments;
    PFN_vkCmdResolveImage CmdResolveImage;
    PFN_vkCmdSetEvent CmdSetEvent;
    PFN_vkCmdResetEvent CmdResetEvent;
    PFN_vkCmdWaitEvents CmdWaitEvents;
    PFN_vkCmdPipelineBarrier CmdPipelineBarrier;
    PFN_vkCmdBeginQuery CmdBeginQuery;
    PFN_vkCmdEndQuery CmdEndQuery;
    PFN_vkCmdResetQueryPool CmdResetQueryPool;
    PFN_vkCmdWriteTimestamp CmdWriteTimestamp;
    PFN_vkCmdCopyQueryPoolResults CmdCopyQueryPoolResults;
    PFN_vkCmdPushConstants CmdPushConstants;
    PFN_vkCmdBeginRenderPass CmdBeginRenderPass;
    PFN_vkCmdNextSubpass CmdNextSubpass;
    PFN_vkCmdEndRenderPass CmdEndRenderPass;
    PFN_vkCmdExecuteCommands CmdExecuteCommands;
    PFN_vkBindBufferMemory2 BindBufferMemory2;
    PFN_vkBindImageMemory2 BindImageMemory2;
    PFN_vkGetDeviceGroupPeerMemoryFeatures GetDeviceGroupPeerMemoryFeatures;
    PFN_vkCmdSetDeviceMask CmdSetDeviceMask;
    PFN_vkCmdDispatchBase CmdDispatchBase;
    PFN_vkGetImageMemoryRequirements2 GetImageMemoryRequirements2;
    PFN_vkGetBufferMemoryRequirements2 GetBufferMemoryRequirements2;
    PFN_vkGetImageSparseMemoryRequirements2 GetImageSparseMemoryRequirements2;
    PFN_vkTrimCommandPool TrimCommandPool;
    PFN_vkGetDeviceQueue2 GetDeviceQueue2;
    PFN_vkCreateSamplerYcbcrConversion CreateSamplerYcbcrConversion;
    PFN_vkDestroySamplerYcbcrConversion DestroySamplerYcbcrConversion;
    PFN_vkCreateDescriptorUpdateTemplate CreateDescriptorUpdateTemplate;
    PFN_vkDestroyDescriptorUpdateTemplate DestroyDescriptorUpdateTemplate;
    PFN_vkUpdateDescriptorSetWithTemplate UpdateDescriptorSetWithTemplate;
    PFN_vkGetDescriptorSetLayoutSupport GetDescriptorSetLayoutSupport;
    PFN_vkCmdDrawIndirectCount CmdDrawIndirectCount;
    PFN_vkCmdDrawIndexedIndirectCount CmdDrawIndexedIndirectCount;
    PFN_vkCreateRenderPass2 CreateRenderPass2;
    PFN_vkCmdBeginRenderPass2 CmdBeginRenderPass2;
    PFN_vkCmdNextSubpass2 CmdNextSubpass2;
    PFN_vkCmdEndRenderPass2 CmdEndRenderPass2;
    PFN_vkResetQueryPool ResetQueryPool;
    PFN_vkGetSemaphoreCounterValue GetSemaphoreCounterValue;
    PFN_vkWaitSemaphores WaitSemaphores;
    PFN_vkSignalSemaphore SignalSemaphore;
    PFN_vkGetBufferDeviceAddress GetBufferDeviceAddress;
    PFN_vkGetBufferOpaqueCaptureAddress GetBufferOpaqueCaptureAddress;
    PFN_vkGetDeviceMemoryOpaqueCaptureAddress GetDeviceMemoryOpaqueCaptureAddress;
    PFN_vkCreatePrivateDataSlot CreatePrivateDataSlot;
    PFN_vkDestroyPrivateDataSlot DestroyPrivateDataSlot;
    PFN_vkSetPrivateData SetPrivateData;
    PFN_vkGetPrivateData GetPrivateData;
    PFN_vkCmdSetEvent2 CmdSetEvent2;
    PFN_vkCmdResetEvent2 CmdResetEvent2;
    PFN_vkCmdWaitEvents2 CmdWaitEvents2;
    PFN_vkCmdPipelineBarrier2 CmdPipelineBarrier2;
    PFN_vkCmdWriteTimestamp2 CmdWriteTimestamp2;
    PFN_vkQueueSubmit2 QueueSubmit2;
    PFN_vkCmdCopyBuffer2 CmdCopyBuffer2;
    PFN_vkCmdCopyImage2 CmdCopyImage2;
    PFN_vkCmdCopyBufferToImage2 CmdCopyBufferToImage2;
    PFN_vkCmdCopyImageToBuffer2 CmdCopyImageToBuffer2;
    PFN_vkCmdBlitImage2 CmdBlitImage2;
    PFN_vkCmdResolveImage2 CmdResolveImage2;
    PFN_vkCmdBeginRendering CmdBeginRendering;
    PFN_vkCmdEndRendering CmdEndRendering;
    PFN_vkCmdSetCullMode CmdSetCullMode;
    PFN_vkCmdSetFrontFace CmdSetFrontFace;
    PFN_vkCmdSetPrimitiveTopology CmdSetPrimitiveTopology;
    PFN_vkCmdSetViewportWithCount CmdSetViewportWithCount;
    PFN_vkCmdSetScissorWithCount CmdSetScissorWithCount;
    PFN_vkCmdBindVertexBuffers2 CmdBindVertexBuffers2;
    PFN_vkCmdSetDepthTestEnable CmdSetDepthTestEnable;
    PFN_vkCmdSetDepthWriteEnable CmdSetDepthWriteEnable;
    PFN_vkCmdSetDepthCompareOp CmdSetDepthCompareOp;
    PFN_vkCmdSetDepthBoundsTestEnable CmdSetDepthBoundsTestEnable;
    PFN_vkCmdSetStencilTestEnable CmdSetStencilTestEnable;
    PFN_vkCmdSetStencilOp CmdSetStencilOp;
    PFN_vkCmdSetRasterizerDiscardEnable CmdSetRasterizerDiscardEnable;
    PFN_vkCmdSetDepthBiasEnable CmdSetDepthBiasEnable;
    PFN_vkCmdSetPrimitiveRestartEnable CmdSetPrimitiveRestartEnable;
    PFN_vkGetDeviceBufferMemoryRequirements GetDeviceBufferMemoryRequirements;
    PFN_vkGetDeviceImageMemoryRequirements GetDeviceImageMemoryRequirements;
    PFN_vkGetDeviceImageSparseMemoryRequirements GetDeviceImageSparseMemoryRequirements;
    PFN_vkCreateSwapchainKHR CreateSwapchainKHR;
    PFN_vkDestroySwapchainKHR DestroySwapchainKHR;
    PFN_vkGetSwapchainImagesKHR GetSwapchainImagesKHR;
    PFN_vkAcquireNextImageKHR AcquireNextImageKHR;
    PFN_vkQueuePresentKHR QueuePresentKHR;
    PFN_vkGetDeviceGroupPresentCapabilitiesKHR GetDeviceGroupPresentCapabilitiesKHR;
    PFN_vkGetDeviceGroupSurfacePresentModesKHR GetDeviceGroupSurfacePresentModesKHR;
    PFN_vkAcquireNextImage2KHR AcquireNextImage2KHR;
    PFN_vkCreateSharedSwapchainsKHR CreateSharedSwapchainsKHR;
    PFN_vkCreateVideoSessionKHR CreateVideoSessionKHR;
    PFN_vkDestroyVideoSessionKHR DestroyVideoSessionKHR;
    PFN_vkGetVideoSessionMemoryRequirementsKHR GetVideoSessionMemoryRequirementsKHR;
    PFN_vkBindVideoSessionMemoryKHR BindVideoSessionMemoryKHR;
    PFN_vkCreateVideoSessionParametersKHR CreateVideoSessionParametersKHR;
    PFN_vkUpdateVideoSessionParametersKHR UpdateVideoSessionParametersKHR;
    PFN_vkDestroyVideoSessionParametersKHR DestroyVideoSessionParametersKHR;
    PFN_vkCmdBeginVideoCodingKHR CmdBeginVideoCodingKHR;
    PFN_vkCmdEndVideoCodingKHR CmdEndVideoCodingKHR;
    PFN_vkCmdControlVideoCodingKHR CmdControlVideoCodingKHR;
    PFN_vkCmdDecodeVideoKHR CmdDecodeVideoKHR;
    PFN_vkCmdBeginRenderingKHR CmdBeginRenderingKHR;
    PFN_vkCmdEndRenderingKHR CmdEndRenderingKHR;
    PFN_vkGetDeviceGroupPeerMemoryFeaturesKHR GetDeviceGroupPeerMemoryFeaturesKHR;
    PFN_vkCmdSetDeviceMaskKHR CmdSetDeviceMaskKHR;
    PFN_vkCmdDispatchBaseKHR CmdDispatchBaseKHR;
    PFN_vkTrimCommandPoolKHR TrimCommandPoolKHR;
#ifdef VK_USE_PLATFORM_WIN32_KHR
    PFN_vkGetMemoryWin32HandleKHR GetMemoryWin32HandleKHR;
#endif  // VK_USE_PLATFORM_WIN32_KHR
#ifdef VK_USE_PLATFORM_WIN32_KHR
    PFN_vkGetMemoryWin32HandlePropertiesKHR GetMemoryWin32HandlePropertiesKHR;
#endif  // VK_USE_PLATFORM_WIN32_KHR
    PFN_vkGetMemoryFdKHR GetMemoryFdKHR;
    PFN_vkGetMemoryFdPropertiesKHR GetMemoryFdPropertiesKHR;
#ifdef VK_USE_PLATFORM_WIN32_KHR
    PFN_vkImportSemaphoreWin32HandleKHR ImportSemaphoreWin32HandleKHR;
#endif  // VK_USE_PLATFORM_WIN32_KHR
#ifdef VK_USE_PLATFORM_WIN32_KHR
    PFN_vkGetSemaphoreWin32HandleKHR GetSemaphoreWin32HandleKHR;
#endif  // VK_USE_PLATFORM_WIN32_KHR
    PFN_vkImportSemaphoreFdKHR ImportSemaphoreFdKHR;
    PFN_vkGetSemaphoreFdKHR GetSemaphoreFdKHR;
    PFN_vkCmdPushDescriptorSetKHR CmdPushDescriptorSetKHR;
    PFN_vkCmdPushDescriptorSetWithTemplateKHR CmdPushDescriptorSetWithTemplateKHR;
    PFN_vkCreateDescriptorUpdateTemplateKHR CreateDescriptorUpdateTemplateKHR;
    PFN_vkDestroyDescriptorUpdateTemplateKHR DestroyDescriptorUpdateTemplateKHR;
    PFN_vkUpdateDescriptorSetWithTemplateKHR UpdateDescriptorSetWithTemplateKHR;
    PFN_vkCreateRenderPass2KHR CreateRenderPass2KHR;
    PFN_vkCmdBeginRenderPass2KHR CmdBeginRenderPass2KHR;
    PFN_vkCmdNextSubpass2KHR CmdNextSubpass2KHR;
    PFN_vkCmdEndRenderPass2KHR CmdEndRenderPass2KHR;
    PFN_vkGetSwapchainStatusKHR GetSwapchainStatusKHR;
#ifdef VK_USE_PLATFORM_WIN32_KHR
    PFN_vkImportFenceWin32HandleKHR ImportFenceWin32HandleKHR;
#endif  // VK_USE_PLATFORM_WIN32_KHR
#ifdef VK_USE_PLATFORM_WIN32_KHR
    PFN_vkGetFenceWin32HandleKHR GetFenceWin32HandleKHR;
#endif  // VK_USE_PLATFORM_WIN32_KHR
    PFN_vkImportFenceFdKHR ImportFenceFdKHR;
    PFN_vkGetFenceFdKHR GetFenceFdKHR;
    PFN_vkAcquireProfilingLockKHR AcquireProfilingLockKHR;
    PFN_vkReleaseProfilingLockKHR ReleaseProfilingLockKHR;
    PFN_vkGetImageMemoryRequirements2KHR GetImageMemoryRequirements2KHR;
    PFN_vkGetBufferMemoryRequirements2KHR GetBufferMemoryRequirements2KHR;
    PFN_vkGetImageSparseMemoryRequirements2KHR GetImageSparseMemoryRequirements2KHR;
    PFN_vkCreateSamplerYcbcrConversionKHR CreateSamplerYcbcrConversionKHR;
    PFN_vkDestroySamplerYcbcrConversionKHR DestroySamplerYcbcrConversionKHR;
    PFN_vkBindBufferMemory2KHR BindBufferMemory2KHR;
    PFN_vkBindImageMemory2KHR BindImageMemory2KHR;
    PFN_vkGetDescriptorSetLayoutSupportKHR GetDescriptorSetLayoutSupportKHR;
    PFN_vkCmdDrawIndirectCountKHR CmdDrawIndirectCountKHR;
    PFN_vkCmdDrawIndexedIndirectCountKHR CmdDrawIndexedIndirectCountKHR;
    PFN_vkGetSemaphoreCounterValueKHR GetSemaphoreCounterValueKHR;
    PFN_vkWaitSemaphoresKHR WaitSemaphoresKHR;
    PFN_vkSignalSemaphoreKHR SignalSemaphoreKHR;
    PFN_vkCmdSetFragmentShadingRateKHR CmdSetFragmentShadingRateKHR;
    PFN_vkWaitForPresentKHR WaitForPresentKHR;
    PFN_vkGetBufferDeviceAddressKHR GetBufferDeviceAddressKHR;
    PFN_vkGetBufferOpaqueCaptureAddressKHR GetBufferOpaqueCaptureAddressKHR;
    PFN_vkGetDeviceMemoryOpaqueCaptureAddressKHR GetDeviceMemoryOpaqueCaptureAddressKHR;
    PFN_vkCreateDeferredOperationKHR CreateDeferredOperationKHR;
    PFN_vkDestroyDeferredOperationKHR DestroyDeferredOperationKHR;
    PFN_vkGetDeferredOperationMaxConcurrencyKHR GetDeferredOperationMaxConcurrencyKHR;
    PFN_vkGetDeferredOperationResultKHR GetDeferredOperationResultKHR;
    PFN_vkDeferredOperationJoinKHR DeferredOperationJoinKHR;
    PFN_vkGetPipelineExecutablePropertiesKHR GetPipelineExecutablePropertiesKHR;
    PFN_vkGetPipelineExecutableStatisticsKHR GetPipelineExecutableStatisticsKHR;
    PFN_vkGetPipelineExecutableInternalRepresentationsKHR GetPipelineExecutableInternalRepresentationsKHR;
    PFN_vkMapMemory2KHR MapMemory2KHR;
    PFN_vkUnmapMemory2KHR UnmapMemory2KHR;
#ifdef VK_ENABLE_BETA_EXTENSIONS
    PFN_vkGetEncodedVideoSessionParametersKHR GetEncodedVideoSessionParametersKHR;
#endif  // VK_ENABLE_BETA_EXTENSIONS
#ifdef VK_ENABLE_BETA_EXTENSIONS
    PFN_vkCmdEncodeVideoKHR CmdEncodeVideoKHR;
#endif  // VK_ENABLE_BETA_EXTENSIONS
    PFN_vkCmdSetEvent2KHR CmdSetEvent2KHR;
    PFN_vkCmdResetEvent2KHR CmdResetEvent2KHR;
    PFN_vkCmdWaitEvents2KHR CmdWaitEvents2KHR;
    PFN_vkCmdPipelineBarrier2KHR CmdPipelineBarrier2KHR;
    PFN_vkCmdWriteTimestamp2KHR CmdWriteTimestamp2KHR;
    PFN_vkQueueSubmit2KHR QueueSubmit2KHR;
    PFN_vkCmdWriteBufferMarker2AMD CmdWriteBufferMarker2AMD;
    PFN_vkGetQueueCheckpointData2NV GetQueueCheckpointData2NV;
    PFN_vkCmdCopyBuffer2KHR CmdCopyBuffer2KHR;
    PFN_vkCmdCopyImage2KHR CmdCopyImage2KHR;
    PFN_vkCmdCopyBufferToImage2KHR CmdCopyBufferToImage2KHR;
    PFN_vkCmdCopyImageToBuffer2KHR CmdCopyImageToBuffer2KHR;
    PFN_vkCmdBlitImage2KHR CmdBlitImage2KHR;
    PFN_vkCmdResolveImage2KHR CmdResolveImage2KHR;
    PFN_vkCmdTraceRaysIndirect2KHR CmdTraceRaysIndirect2KHR;
    PFN_vkGetDeviceBufferMemoryRequirementsKHR GetDeviceBufferMemoryRequirementsKHR;
    PFN_vkGetDeviceImageMemoryRequirementsKHR GetDeviceImageMemoryRequirementsKHR;
    PFN_vkGetDeviceImageSparseMemoryRequirementsKHR GetDeviceImageSparseMemoryRequirementsKHR;
    PFN_vkCmdBindIndexBuffer2KHR CmdBindIndexBuffer2KHR;
    PFN_vkGetRenderingAreaGranularityKHR GetRenderingAreaGranularityKHR;
    PFN_vkGetDeviceImageSubresourceLayoutKHR GetDeviceImageSubresourceLayoutKHR;
    PFN_vkGetImageSubresourceLayout2KHR GetImageSubresourceLayout2KHR;
    PFN_vkDebugMarkerSetObjectTagEXT DebugMarkerSetObjectTagEXT;
    PFN_vkDebugMarkerSetObjectNameEXT DebugMarkerSetObjectNameEXT;
    PFN_vkCmdDebugMarkerBeginEXT CmdDebugMarkerBeginEXT;
    PFN_vkCmdDebugMarkerEndEXT CmdDebugMarkerEndEXT;
    PFN_vkCmdDebugMarkerInsertEXT CmdDebugMarkerInsertEXT;
    PFN_vkCmdBindTransformFeedbackBuffersEXT CmdBindTransformFeedbackBuffersEXT;
    PFN_vkCmdBeginTransformFeedbackEXT CmdBeginTransformFeedbackEXT;
    PFN_vkCmdEndTransformFeedbackEXT CmdEndTransformFeedbackEXT;
    PFN_vkCmdBeginQueryIndexedEXT CmdBeginQueryIndexedEXT;
    PFN_vkCmdEndQueryIndexedEXT CmdEndQueryIndexedEXT;
    PFN_vkCmdDrawIndirectByteCountEXT CmdDrawIndirectByteCountEXT;
    PFN_vkCreateCuModuleNVX CreateCuModuleNVX;
    PFN_vkCreateCuFunctionNVX CreateCuFunctionNVX;
    PFN_vkDestroyCuModuleNVX DestroyCuModuleNVX;
    PFN_vkDestroyCuFunctionNVX DestroyCuFunctionNVX;
    PFN_vkCmdCuLaunchKernelNVX CmdCuLaunchKernelNVX;
    PFN_vkGetImageViewHandleNVX GetImageViewHandleNVX;
    PFN_vkGetImageViewAddressNVX GetImageViewAddressNVX;
    PFN_vkCmdDrawIndirectCountAMD CmdDrawIndirectCountAMD;
    PFN_vkCmdDrawIndexedIndirectCountAMD CmdDrawIndexedIndirectCountAMD;
    PFN_vkGetShaderInfoAMD GetShaderInfoAMD;
#ifdef VK_USE_PLATFORM_WIN32_KHR
    PFN_vkGetMemoryWin32HandleNV GetMemoryWin32HandleNV;
#endif  // VK_USE_PLATFORM_WIN32_KHR
    PFN_vkCmdBeginConditionalRenderingEXT CmdBeginConditionalRenderingEXT;
    PFN_vkCmdEndConditionalRenderingEXT CmdEndConditionalRenderingEXT;
    PFN_vkCmdSetViewportWScalingNV CmdSetViewportWScalingNV;
    PFN_vkDisplayPowerControlEXT DisplayPowerControlEXT;
    PFN_vkRegisterDeviceEventEXT RegisterDeviceEventEXT;
    PFN_vkRegisterDisplayEventEXT RegisterDisplayEventEXT;
    PFN_vkGetSwapchainCounterEXT GetSwapchainCounterEXT;
    PFN_vkGetRefreshCycleDurationGOOGLE GetRefreshCycleDurationGOOGLE;
    PFN_vkGetPastPresentationTimingGOOGLE GetPastPresentationTimingGOOGLE;
    PFN_vkCmdSetDiscardRectangleEXT CmdSetDiscardRectangleEXT;
    PFN_vkCmdSetDiscardRectangleEnableEXT CmdSetDiscardRectangleEnableEXT;
    PFN_vkCmdSetDiscardRectangleModeEXT CmdSetDiscardRectangleModeEXT;
    PFN_vkSetHdrMetadataEXT SetHdrMetadataEXT;
    PFN_vkSetDebugUtilsObjectNameEXT SetDebugUtilsObjectNameEXT;
    PFN_vkSetDebugUtilsObjectTagEXT SetDebugUtilsObjectTagEXT;
    PFN_vkQueueBeginDebugUtilsLabelEXT QueueBeginDebugUtilsLabelEXT;
    PFN_vkQueueEndDebugUtilsLabelEXT QueueEndDebugUtilsLabelEXT;
    PFN_vkQueueInsertDebugUtilsLabelEXT QueueInsertDebugUtilsLabelEXT;
    PFN_vkCmdBeginDebugUtilsLabelEXT CmdBeginDebugUtilsLabelEXT;
    PFN_vkCmdEndDebugUtilsLabelEXT CmdEndDebugUtilsLabelEXT;
    PFN_vkCmdInsertDebugUtilsLabelEXT CmdInsertDebugUtilsLabelEXT;
#ifdef VK_USE_PLATFORM_ANDROID_KHR
    PFN_vkGetAndroidHardwareBufferPropertiesANDROID GetAndroidHardwareBufferPropertiesANDROID;
#endif  // VK_USE_PLATFORM_ANDROID_KHR
#ifdef VK_USE_PLATFORM_ANDROID_KHR
    PFN_vkGetMemoryAndroidHardwareBufferANDROID GetMemoryAndroidHardwareBufferANDROID;
#endif  // VK_USE_PLATFORM_ANDROID_KHR
#ifdef VK_ENABLE_BETA_EXTENSIONS
    PFN_vkCreateExecutionGraphPipelinesAMDX CreateExecutionGraphPipelinesAMDX;
#endif  // VK_ENABLE_BETA_EXTENSIONS
#ifdef VK_ENABLE_BETA_EXTENSIONS
    PFN_vkGetExecutionGraphPipelineScratchSizeAMDX GetExecutionGraphPipelineScratchSizeAMDX;
#endif  // VK_ENABLE_BETA_EXTENSIONS
#ifdef VK_ENABLE_BETA_EXTENSIONS
    PFN_vkGetExecutionGraphPipelineNodeIndexAMDX GetExecutionGraphPipelineNodeIndexAMDX;
#endif  // VK_ENABLE_BETA_EXTENSIONS
#ifdef VK_ENABLE_BETA_EXTENSIONS
    PFN_vkCmdInitializeGraphScratchMemoryAMDX CmdInitializeGraphScratchMemoryAMDX;
#endif  // VK_ENABLE_BETA_EXTENSIONS
#ifdef VK_ENABLE_BETA_EXTENSIONS
    PFN_vkCmdDispatchGraphAMDX CmdDispatchGraphAMDX;
#endif  // VK_ENABLE_BETA_EXTENSIONS
#ifdef VK_ENABLE_BETA_EXTENSIONS
    PFN_vkCmdDispatchGraphIndirectAMDX CmdDispatchGraphIndirectAMDX;
#endif  // VK_ENABLE_BETA_EXTENSIONS
#ifdef VK_ENABLE_BETA_EXTENSIONS
    PFN_vkCmdDispatchGraphIndirectCountAMDX CmdDispatchGraphIndirectCountAMDX;
#endif  // VK_ENABLE_BETA_EXTENSIONS
    PFN_vkCmdSetSampleLocationsEXT CmdSetSampleLocationsEXT;
    PFN_vkGetImageDrmFormatModifierPropertiesEXT GetImageDrmFormatModifierPropertiesEXT;
    PFN_vkCreateValidationCacheEXT CreateValidationCacheEXT;
    PFN_vkDestroyValidationCacheEXT DestroyValidationCacheEXT;
    PFN_vkMergeValidationCachesEXT MergeValidationCachesEXT;
    PFN_vkGetValidationCacheDataEXT GetValidationCacheDataEXT;
    PFN_vkCmdBindShadingRateImageNV CmdBindShadingRateImageNV;
    PFN_vkCmdSetViewportShadingRatePaletteNV CmdSetViewportShadingRatePaletteNV;
    PFN_vkCmdSetCoarseSampleOrderNV CmdSetCoarseSampleOrderNV;
    PFN_vkCreateAccelerationStructureNV CreateAccelerationStructureNV;
    PFN_vkDestroyAccelerationStructureNV DestroyAccelerationStructureNV;
    PFN_vkGetAccelerationStructureMemoryRequirementsNV GetAccelerationStructureMemoryRequirementsNV;
    PFN_vkBindAccelerationStructureMemoryNV BindAccelerationStructureMemoryNV;
    PFN_vkCmdBuildAccelerationStructureNV CmdBuildAccelerationStructureNV;
    PFN_vkCmdCopyAccelerationStructureNV CmdCopyAccelerationStructureNV;
    PFN_vkCmdTraceRaysNV CmdTraceRaysNV;
    PFN_vkCreateRayTracingPipelinesNV CreateRayTracingPipelinesNV;
    PFN_vkGetRayTracingShaderGroupHandlesKHR GetRayTracingShaderGroupHandlesKHR;
    PFN_vkGetRayTracingShaderGroupHandlesNV GetRayTracingShaderGroupHandlesNV;
    PFN_vkGetAccelerationStructureHandleNV GetAccelerationStructureHandleNV;
    PFN_vkCmdWriteAccelerationStructuresPropertiesNV CmdWriteAccelerationStructuresPropertiesNV;
    PFN_vkCompileDeferredNV CompileDeferredNV;
    PFN_vkGetMemoryHostPointerPropertiesEXT GetMemoryHostPointerPropertiesEXT;
    PFN_vkCmdWriteBufferMarkerAMD CmdWriteBufferMarkerAMD;
    PFN_vkGetCalibratedTimestampsEXT GetCalibratedTimestampsEXT;
    PFN_vkCmdDrawMeshTasksNV CmdDrawMeshTasksNV;
    PFN_vkCmdDrawMeshTasksIndirectNV CmdDrawMeshTasksIndirectNV;
    PFN_vkCmdDrawMeshTasksIndirectCountNV CmdDrawMeshTasksIndirectCountNV;
    PFN_vkCmdSetExclusiveScissorEnableNV CmdSetExclusiveScissorEnableNV;
    PFN_vkCmdSetExclusiveScissorNV CmdSetExclusiveScissorNV;
    PFN_vkCmdSetCheckpointNV CmdSetCheckpointNV;
    PFN_vkGetQueueCheckpointDataNV GetQueueCheckpointDataNV;
    PFN_vkInitializePerformanceApiINTEL InitializePerformanceApiINTEL;
    PFN_vkUninitializePerformanceApiINTEL UninitializePerformanceApiINTEL;
    PFN_vkCmdSetPerformanceMarkerINTEL CmdSetPerformanceMarkerINTEL;
    PFN_vkCmdSetPerformanceStreamMarkerINTEL CmdSetPerformanceStreamMarkerINTEL;
    PFN_vkCmdSetPerformanceOverrideINTEL CmdSetPerformanceOverrideINTEL;
    PFN_vkAcquirePerformanceConfigurationINTEL AcquirePerformanceConfigurationINTEL;
    PFN_vkReleasePerformanceConfigurationINTEL ReleasePerformanceConfigurationINTEL;
    PFN_vkQueueSetPerformanceConfigurationINTEL QueueSetPerformanceConfigurationINTEL;
    PFN_vkGetPerformanceParameterINTEL GetPerformanceParameterINTEL;
    PFN_vkSetLocalDimmingAMD SetLocalDimmingAMD;
    PFN_vkGetBufferDeviceAddressEXT GetBufferDeviceAddressEXT;
#ifdef VK_USE_PLATFORM_WIN32_KHR
    PFN_vkAcquireFullScreenExclusiveModeEXT AcquireFullScreenExclusiveModeEXT;
#endif  // VK_USE_PLATFORM_WIN32_KHR
#ifdef VK_USE_PLATFORM_WIN32_KHR
    PFN_vkReleaseFullScreenExclusiveModeEXT ReleaseFullScreenExclusiveModeEXT;
#endif  // VK_USE_PLATFORM_WIN32_KHR
#ifdef VK_USE_PLATFORM_WIN32_KHR
    PFN_vkGetDeviceGroupSurfacePresentModes2EXT GetDeviceGroupSurfacePresentModes2EXT;
#endif  // VK_USE_PLATFORM_WIN32_KHR
    PFN_vkCmdSetLineStippleEXT CmdSetLineStippleEXT;
    PFN_vkResetQueryPoolEXT ResetQueryPoolEXT;
    PFN_vkCmdSetCullModeEXT CmdSetCullModeEXT;
    PFN_vkCmdSetFrontFaceEXT CmdSetFrontFaceEXT;
    PFN_vkCmdSetPrimitiveTopologyEXT CmdSetPrimitiveTopologyEXT;
    PFN_vkCmdSetViewportWithCountEXT CmdSetViewportWithCountEXT;
    PFN_vkCmdSetScissorWithCountEXT CmdSetScissorWithCountEXT;
    PFN_vkCmdBindVertexBuffers2EXT CmdBindVertexBuffers2EXT;
    PFN_vkCmdSetDepthTestEnableEXT CmdSetDepthTestEnableEXT;
    PFN_vkCmdSetDepthWriteEnableEXT CmdSetDepthWriteEnableEXT;
    PFN_vkCmdSetDepthCompareOpEXT CmdSetDepthCompareOpEXT;
    PFN_vkCmdSetDepthBoundsTestEnableEXT CmdSetDepthBoundsTestEnableEXT;
    PFN_vkCmdSetStencilTestEnableEXT CmdSetStencilTestEnableEXT;
    PFN_vkCmdSetStencilOpEXT CmdSetStencilOpEXT;
    PFN_vkCopyMemoryToImageEXT CopyMemoryToImageEXT;
    PFN_vkCopyImageToMemoryEXT CopyImageToMemoryEXT;
    PFN_vkCopyImageToImageEXT CopyImageToImageEXT;
    PFN_vkTransitionImageLayoutEXT TransitionImageLayoutEXT;
    PFN_vkGetImageSubresourceLayout2EXT GetImageSubresourceLayout2EXT;
    PFN_vkReleaseSwapchainImagesEXT ReleaseSwapchainImagesEXT;
    PFN_vkGetGeneratedCommandsMemoryRequirementsNV GetGeneratedCommandsMemoryRequirementsNV;
    PFN_vkCmdPreprocessGeneratedCommandsNV CmdPreprocessGeneratedCommandsNV;
    PFN_vkCmdExecuteGeneratedCommandsNV CmdExecuteGeneratedCommandsNV;
    PFN_vkCmdBindPipelineShaderGroupNV CmdBindPipelineShaderGroupNV;
    PFN_vkCreateIndirectCommandsLayoutNV CreateIndirectCommandsLayoutNV;
    PFN_vkDestroyIndirectCommandsLayoutNV DestroyIndirectCommandsLayoutNV;
    PFN_vkCmdSetDepthBias2EXT CmdSetDepthBias2EXT;
    PFN_vkCreatePrivateDataSlotEXT CreatePrivateDataSlotEXT;
    PFN_vkDestroyPrivateDataSlotEXT DestroyPrivateDataSlotEXT;
    PFN_vkSetPrivateDataEXT SetPrivateDataEXT;
    PFN_vkGetPrivateDataEXT GetPrivateDataEXT;
#ifdef VK_USE_PLATFORM_METAL_EXT
    PFN_vkExportMetalObjectsEXT ExportMetalObjectsEXT;
#endif  // VK_USE_PLATFORM_METAL_EXT
    PFN_vkGetDescriptorSetLayoutSizeEXT GetDescriptorSetLayoutSizeEXT;
    PFN_vkGetDescriptorSetLayoutBindingOffsetEXT GetDescriptorSetLayoutBindingOffsetEXT;
    PFN_vkGetDescriptorEXT GetDescriptorEXT;
    PFN_vkCmdBindDescriptorBuffersEXT CmdBindDescriptorBuffersEXT;
    PFN_vkCmdSetDescriptorBufferOffsetsEXT CmdSetDescriptorBufferOffsetsEXT;
    PFN_vkCmdBindDescriptorBufferEmbeddedSamplersEXT CmdBindDescriptorBufferEmbeddedSamplersEXT;
    PFN_vkGetBufferOpaqueCaptureDescriptorDataEXT GetBufferOpaqueCaptureDescriptorDataEXT;
    PFN_vkGetImageOpaqueCaptureDescriptorDataEXT GetImageOpaqueCaptureDescriptorDataEXT;
    PFN_vkGetImageViewOpaqueCaptureDescriptorDataEXT GetImageViewOpaqueCaptureDescriptorDataEXT;
    PFN_vkGetSamplerOpaqueCaptureDescriptorDataEXT GetSamplerOpaqueCaptureDescriptorDataEXT;
    PFN_vkGetAccelerationStructureOpaqueCaptureDescriptorDataEXT GetAccelerationStructureOpaqueCaptureDescriptorDataEXT;
    PFN_vkCmdSetFragmentShadingRateEnumNV CmdSetFragmentShadingRateEnumNV;
    PFN_vkGetDeviceFaultInfoEXT GetDeviceFaultInfoEXT;
    PFN_vkCmdSetVertexInputEXT CmdSetVertexInputEXT;
#ifdef VK_USE_PLATFORM_FUCHSIA
    PFN_vkGetMemoryZirconHandleFUCHSIA GetMemoryZirconHandleFUCHSIA;
#endif  // VK_USE_PLATFORM_FUCHSIA
#ifdef VK_USE_PLATFORM_FUCHSIA
    PFN_vkGetMemoryZirconHandlePropertiesFUCHSIA GetMemoryZirconHandlePropertiesFUCHSIA;
#endif  // VK_USE_PLATFORM_FUCHSIA
#ifdef VK_USE_PLATFORM_FUCHSIA
    PFN_vkImportSemaphoreZirconHandleFUCHSIA ImportSemaphoreZirconHandleFUCHSIA;
#endif  // VK_USE_PLATFORM_FUCHSIA
#ifdef VK_USE_PLATFORM_FUCHSIA
    PFN_vkGetSemaphoreZirconHandleFUCHSIA GetSemaphoreZirconHandleFUCHSIA;
#endif  // VK_USE_PLATFORM_FUCHSIA
#ifdef VK_USE_PLATFORM_FUCHSIA
    PFN_vkCreateBufferCollectionFUCHSIA CreateBufferCollectionFUCHSIA;
#endif  // VK_USE_PLATFORM_FUCHSIA
#ifdef VK_USE_PLATFORM_FUCHSIA
    PFN_vkSetBufferCollectionImageConstraintsFUCHSIA SetBufferCollectionImageConstraintsFUCHSIA;
#endif  // VK_USE_PLATFORM_FUCHSIA
#ifdef VK_USE_PLATFORM_FUCHSIA
    PFN_vkSetBufferCollectionBufferConstraintsFUCHSIA SetBufferCollectionBufferConstraintsFUCHSIA;
#endif  // VK_USE_PLATFORM_FUCHSIA
#ifdef VK_USE_PLATFORM_FUCHSIA
    PFN_vkDestroyBufferCollectionFUCHSIA DestroyBufferCollectionFUCHSIA;
#endif  // VK_USE_PLATFORM_FUCHSIA
#ifdef VK_USE_PLATFORM_FUCHSIA
    PFN_vkGetBufferCollectionPropertiesFUCHSIA GetBufferCollectionPropertiesFUCHSIA;
#endif  // VK_USE_PLATFORM_FUCHSIA
    PFN_vkGetDeviceSubpassShadingMaxWorkgroupSizeHUAWEI GetDeviceSubpassShadingMaxWorkgroupSizeHUAWEI;
    PFN_vkCmdSubpassShadingHUAWEI CmdSubpassShadingHUAWEI;
    PFN_vkCmdBindInvocationMaskHUAWEI CmdBindInvocationMaskHUAWEI;
    PFN_vkGetMemoryRemoteAddressNV GetMemoryRemoteAddressNV;
    PFN_vkGetPipelinePropertiesEXT GetPipelinePropertiesEXT;
    PFN_vkCmdSetPatchControlPointsEXT CmdSetPatchControlPointsEXT;
    PFN_vkCmdSetRasterizerDiscardEnableEXT CmdSetRasterizerDiscardEnableEXT;
    PFN_vkCmdSetDepthBiasEnableEXT CmdSetDepthBiasEnableEXT;
    PFN_vkCmdSetLogicOpEXT CmdSetLogicOpEXT;
    PFN_vkCmdSetPrimitiveRestartEnableEXT CmdSetPrimitiveRestartEnableEXT;
    PFN_vkCmdSetColorWriteEnableEXT CmdSetColorWriteEnableEXT;
    PFN_vkCmdDrawMultiEXT CmdDrawMultiEXT;
    PFN_vkCmdDrawMultiIndexedEXT CmdDrawMultiIndexedEXT;
    PFN_vkCreateMicromapEXT CreateMicromapEXT;
    PFN_vkDestroyMicromapEXT DestroyMicromapEXT;
    PFN_vkCmdBuildMicromapsEXT CmdBuildMicromapsEXT;
    PFN_vkBuildMicromapsEXT BuildMicromapsEXT;
    PFN_vkCopyMicromapEXT CopyMicromapEXT;
    PFN_vkCopyMicromapToMemoryEXT CopyMicromapToMemoryEXT;
    PFN_vkCopyMemoryToMicromapEXT CopyMemoryToMicromapEXT;
    PFN_vkWriteMicromapsPropertiesEXT WriteMicromapsPropertiesEXT;
    PFN_vkCmdCopyMicromapEXT CmdCopyMicromapEXT;
    PFN_vkCmdCopyMicromapToMemoryEXT CmdCopyMicromapToMemoryEXT;
    PFN_vkCmdCopyMemoryToMicromapEXT CmdCopyMemoryToMicromapEXT;
    PFN_vkCmdWriteMicromapsPropertiesEXT CmdWriteMicromapsPropertiesEXT;
    PFN_vkGetDeviceMicromapCompatibilityEXT GetDeviceMicromapCompatibilityEXT;
    PFN_vkGetMicromapBuildSizesEXT GetMicromapBuildSizesEXT;
    PFN_vkCmdDrawClusterHUAWEI CmdDrawClusterHUAWEI;
    PFN_vkCmdDrawClusterIndirectHUAWEI CmdDrawClusterIndirectHUAWEI;
    PFN_vkSetDeviceMemoryPriorityEXT SetDeviceMemoryPriorityEXT;
    PFN_vkGetDescriptorSetLayoutHostMappingInfoVALVE GetDescriptorSetLayoutHostMappingInfoVALVE;
    PFN_vkGetDescriptorSetHostMappingVALVE GetDescriptorSetHostMappingVALVE;
    PFN_vkCmdCopyMemoryIndirectNV CmdCopyMemoryIndirectNV;
    PFN_vkCmdCopyMemoryToImageIndirectNV CmdCopyMemoryToImageIndirectNV;
    PFN_vkCmdDecompressMemoryNV CmdDecompressMemoryNV;
    PFN_vkCmdDecompressMemoryIndirectCountNV CmdDecompressMemoryIndirectCountNV;
    PFN_vkGetPipelineIndirectMemoryRequirementsNV GetPipelineIndirectMemoryRequirementsNV;
    PFN_vkCmdUpdatePipelineIndirectBufferNV CmdUpdatePipelineIndirectBufferNV;
    PFN_vkGetPipelineIndirectDeviceAddressNV GetPipelineIndirectDeviceAddressNV;
    PFN_vkCmdSetTessellationDomainOriginEXT CmdSetTessellationDomainOriginEXT;
    PFN_vkCmdSetDepthClampEnableEXT CmdSetDepthClampEnableEXT;
    PFN_vkCmdSetPolygonModeEXT CmdSetPolygonModeEXT;
    PFN_vkCmdSetRasterizationSamplesEXT CmdSetRasterizationSamplesEXT;
    PFN_vkCmdSetSampleMaskEXT CmdSetSampleMaskEXT;
    PFN_vkCmdSetAlphaToCoverageEnableEXT CmdSetAlphaToCoverageEnableEXT;
    PFN_vkCmdSetAlphaToOneEnableEXT CmdSetAlphaToOneEnableEXT;
    PFN_vkCmdSetLogicOpEnableEXT CmdSetLogicOpEnableEXT;
    PFN_vkCmdSetColorBlendEnableEXT CmdSetColorBlendEnableEXT;
    PFN_vkCmdSetColorBlendEquationEXT CmdSetColorBlendEquationEXT;
    PFN_vkCmdSetColorWriteMaskEXT CmdSetColorWriteMaskEXT;
    PFN_vkCmdSetRasterizationStreamEXT CmdSetRasterizationStreamEXT;
    PFN_vkCmdSetConservativeRasterizationModeEXT CmdSetConservativeRasterizationModeEXT;
    PFN_vkCmdSetExtraPrimitiveOverestimationSizeEXT CmdSetExtraPrimitiveOverestimationSizeEXT;
    PFN_vkCmdSetDepthClipEnableEXT CmdSetDepthClipEnableEXT;
    PFN_vkCmdSetSampleLocationsEnableEXT CmdSetSampleLocationsEnableEXT;
    PFN_vkCmdSetColorBlendAdvancedEXT CmdSetColorBlendAdvancedEXT;
    PFN_vkCmdSetProvokingVertexModeEXT CmdSetProvokingVertexModeEXT;
    PFN_vkCmdSetLineRasterizationModeEXT CmdSetLineRasterizationModeEXT;
    PFN_vkCmdSetLineStippleEnableEXT CmdSetLineStippleEnableEXT;
    PFN_vkCmdSetDepthClipNegativeOneToOneEXT CmdSetDepthClipNegativeOneToOneEXT;
    PFN_vkCmdSetViewportWScalingEnableNV CmdSetViewportWScalingEnableNV;
    PFN_vkCmdSetViewportSwizzleNV CmdSetViewportSwizzleNV;
    PFN_vkCmdSetCoverageToColorEnableNV CmdSetCoverageToColorEnableNV;
    PFN_vkCmdSetCoverageToColorLocationNV CmdSetCoverageToColorLocationNV;
    PFN_vkCmdSetCoverageModulationModeNV CmdSetCoverageModulationModeNV;
    PFN_vkCmdSetCoverageModulationTableEnableNV CmdSetCoverageModulationTableEnableNV;
    PFN_vkCmdSetCoverageModulationTableNV CmdSetCoverageModulationTableNV;
    PFN_vkCmdSetShadingRateImageEnableNV CmdSetShadingRateImageEnableNV;
    PFN_vkCmdSetRepresentativeFragmentTestEnableNV CmdSetRepresentativeFragmentTestEnableNV;
    PFN_vkCmdSetCoverageReductionModeNV CmdSetCoverageReductionModeNV;
    PFN_vkGetShaderModuleIdentifierEXT GetShaderModuleIdentifierEXT;
    PFN_vkGetShaderModuleCreateInfoIdentifierEXT GetShaderModuleCreateInfoIdentifierEXT;
    PFN_vkCreateOpticalFlowSessionNV CreateOpticalFlowSessionNV;
    PFN_vkDestroyOpticalFlowSessionNV DestroyOpticalFlowSessionNV;
    PFN_vkBindOpticalFlowSessionImageNV BindOpticalFlowSessionImageNV;
    PFN_vkCmdOpticalFlowExecuteNV CmdOpticalFlowExecuteNV;
    PFN_vkCreateShadersEXT CreateShadersEXT;
    PFN_vkDestroyShaderEXT DestroyShaderEXT;
    PFN_vkGetShaderBinaryDataEXT GetShaderBinaryDataEXT;
    PFN_vkCmdBindShadersEXT CmdBindShadersEXT;
    PFN_vkGetFramebufferTilePropertiesQCOM GetFramebufferTilePropertiesQCOM;
    PFN_vkGetDynamicRenderingTilePropertiesQCOM GetDynamicRenderingTilePropertiesQCOM;
    PFN_vkSetLatencySleepModeNV SetLatencySleepModeNV;
    PFN_vkLatencySleepNV LatencySleepNV;
    PFN_vkSetLatencyMarkerNV SetLatencyMarkerNV;
    PFN_vkGetLatencyTimingsNV GetLatencyTimingsNV;
    PFN_vkQueueNotifyOutOfBandNV QueueNotifyOutOfBandNV;
    PFN_vkCmdSetAttachmentFeedbackLoopEnableEXT CmdSetAttachmentFeedbackLoopEnableEXT;
#ifdef VK_USE_PLATFORM_SCREEN_QNX
    PFN_vkGetScreenBufferPropertiesQNX GetScreenBufferPropertiesQNX;
#endif  // VK_USE_PLATFORM_SCREEN_QNX
    PFN_vkCreateAccelerationStructureKHR CreateAccelerationStructureKHR;
    PFN_vkDestroyAccelerationStructureKHR DestroyAccelerationStructureKHR;
    PFN_vkCmdBuildAccelerationStructuresKHR CmdBuildAccelerationStructuresKHR;
    PFN_vkCmdBuildAccelerationStructuresIndirectKHR CmdBuildAccelerationStructuresIndirectKHR;
    PFN_vkBuildAccelerationStructuresKHR BuildAccelerationStructuresKHR;
    PFN_vkCopyAccelerationStructureKHR CopyAccelerationStructureKHR;
    PFN_vkCopyAccelerationStructureToMemoryKHR CopyAccelerationStructureToMemoryKHR;
    PFN_vkCopyMemoryToAccelerationStructureKHR CopyMemoryToAccelerationStructureKHR;
    PFN_vkWriteAccelerationStructuresPropertiesKHR WriteAccelerationStructuresPropertiesKHR;
    PFN_vkCmdCopyAccelerationStructureKHR CmdCopyAccelerationStructureKHR;
    PFN_vkCmdCopyAccelerationStructureToMemoryKHR CmdCopyAccelerationStructureToMemoryKHR;
    PFN_vkCmdCopyMemoryToAccelerationStructureKHR CmdCopyMemoryToAccelerationStructureKHR;
    PFN_vkGetAccelerationStructureDeviceAddressKHR GetAccelerationStructureDeviceAddressKHR;
    PFN_vkCmdWriteAccelerationStructuresPropertiesKHR CmdWriteAccelerationStructuresPropertiesKHR;
    PFN_vkGetDeviceAccelerationStructureCompatibilityKHR GetDeviceAccelerationStructureCompatibilityKHR;
    PFN_vkGetAccelerationStructureBuildSizesKHR GetAccelerationStructureBuildSizesKHR;
    PFN_vkCmdTraceRaysKHR CmdTraceRaysKHR;
    PFN_vkCreateRayTracingPipelinesKHR CreateRayTracingPipelinesKHR;
    PFN_vkGetRayTracingCaptureReplayShaderGroupHandlesKHR GetRayTracingCaptureReplayShaderGroupHandlesKHR;
    PFN_vkCmdTraceRaysIndirectKHR CmdTraceRaysIndirectKHR;
    PFN_vkGetRayTracingShaderGroupStackSizeKHR GetRayTracingShaderGroupStackSizeKHR;
    PFN_vkCmdSetRayTracingPipelineStackSizeKHR CmdSetRayTracingPipelineStackSizeKHR;
    PFN_vkCmdDrawMeshTasksEXT CmdDrawMeshTasksEXT;
    PFN_vkCmdDrawMeshTasksIndirectEXT CmdDrawMeshTasksIndirectEXT;
    PFN_vkCmdDrawMeshTasksIndirectCountEXT CmdDrawMeshTasksIndirectCountEXT;
} VkLayerDispatchTable;
// clang-format on// NOLINTEND
