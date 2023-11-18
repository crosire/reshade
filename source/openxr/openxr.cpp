/*
 * OpenXR Vulkan wiring by The Iron Wolf.
 *
 * I tried creating a general solution using OXR API Layer (xr-vk-layer branch), but no matter what I do game would
 * crash on call to ::xrCreateSession. So instead use good old hooks.
 *
 * Special thanks to creators of ReShade, OpenVR wiring and Matthieu Bucchianeri for guidance and samples:
 * https://github.com/mbucchia/_ARCHIVE_XR_APILAYER_NOVENDOR_fov_modifier
 * https://github.com/mbucchia/Quad-Views-Foveated
 *
 * Copyright (C) 2021 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "openxr_impl_swapchain.hpp"
#include "vulkan/vulkan_hooks.hpp"
#include "vulkan/vulkan_impl_device.hpp"
#include "vulkan/vulkan_impl_command_queue.hpp"
#include "dll_log.hpp"
#include "com_utils.hpp"
#include "hook_manager.hpp"
#include "lockfree_linear_map.hpp"
#include <functional>
#include <deque>

#define XR_USE_PLATFORM_WIN32
#define XR_USE_GRAPHICS_API_VULKAN
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

static auto constexpr OXR_EYE_COUNT = 2u;

struct CapturedSwapchain
{
  // TODO_OXR: why deque?
  std::deque<uint32_t> acquiredIndex;
  uint32_t lastReleasedIndex{ 0 };

  XrSwapchainCreateInfo createInfo{};
  std::vector<XrSwapchainImageVulkanKHR> surfaceImages;
};

std::unordered_map<XrSwapchain, CapturedSwapchain> gCapturedSwapchains;
VkDevice gVKDevice = VK_NULL_HANDLE;
uint32_t gVKQueueIndex = 0u;

reshade::openxr::swapchain_impl* s_vr_swapchain = nullptr;

XRAPI_ATTR XrResult XRAPI_CALL
Hook_xrCreateSession(XrInstance instance, const XrSessionCreateInfo* createInfo, XrSession* session)
{
  static auto const Orig_xrCreateSession = reshade::hooks::call(Hook_xrCreateSession);

  auto const ret = Orig_xrCreateSession(instance, createInfo, session);
  if (XR_SUCCEEDED(ret)) {
    auto const gbv = reinterpret_cast<XrGraphicsBindingVulkanKHR const*>(createInfo->next);
    gVKDevice = gbv->device;
    gVKQueueIndex = gbv->queueIndex;
  }
  return ret;
}

// DESTROY
XRAPI_ATTR XrResult XRAPI_CALL
Hook_xrDestroySession(XrSession session)
{
  static auto const Orig_xrDestroySession = reshade::hooks::call(Hook_xrDestroySession);

  delete s_vr_swapchain;
  return Orig_xrDestroySession(session);
}

XRAPI_ATTR XrResult XRAPI_CALL
Hook_xrCreateSwapchain(XrSession session, const XrSwapchainCreateInfo* createInfo, XrSwapchain* swapchain)
{
  static auto const Orig_xrCreateSwapchain = reshade::hooks::call(Hook_xrCreateSwapchain);

  const_cast<XrSwapchainCreateInfo*>(createInfo)->usageFlags |= XR_SWAPCHAIN_USAGE_TRANSFER_SRC_BIT;
  auto const ret = Orig_xrCreateSwapchain(session, createInfo, swapchain);
  if (XR_SUCCEEDED(ret)) {
    CapturedSwapchain newEntry{};
    newEntry.createInfo = *createInfo;

    uint32_t numSurfaces = 0u;
    static auto const pfnxrEnumerateSwapchainImages = reinterpret_cast<PFN_xrEnumerateSwapchainImages>(
      ::GetProcAddress(::GetModuleHandle(L"openxr_loader.dll"), "xrEnumerateSwapchainImages"));
    assert(pfnxrEnumerateSwapchainImages != nullptr);
    auto rv = pfnxrEnumerateSwapchainImages(*swapchain, 0u, &numSurfaces, nullptr);
    assert(XR_SUCCEEDED(rv));
    if (!XR_SUCCEEDED(rv))
      return ret;

    // Track our own information about the swapchain, so we can draw stuff onto it.
    newEntry.surfaceImages.resize(numSurfaces, { XR_TYPE_SWAPCHAIN_IMAGE_VULKAN_KHR });
    rv = pfnxrEnumerateSwapchainImages(*swapchain,
                                       numSurfaces,
                                       &numSurfaces,
                                       reinterpret_cast<XrSwapchainImageBaseHeader*>(newEntry.surfaceImages.data()));
    assert(XR_SUCCEEDED(rv));
    if (!XR_SUCCEEDED(rv))
      return ret;

    gCapturedSwapchains.insert_or_assign(*swapchain, std::move(newEntry));
  }
  return ret;
}

XRAPI_ATTR XrResult XRAPI_CALL
Hook_xrDestroySwapchain(XrSwapchain swapchain)
{
  static auto const Orig_xrDestroySwapchain = reshade::hooks::call(Hook_xrDestroySwapchain);

  auto const ret = Orig_xrDestroySwapchain(swapchain);
  if (XR_SUCCEEDED(ret)) {
    auto it = gCapturedSwapchains.find(swapchain);
    if (it != gCapturedSwapchains.end())
      gCapturedSwapchains.erase(it);
  }
  return ret;
}

XRAPI_ATTR XrResult XRAPI_CALL
Hook_xrAcquireSwapchainImage(XrSwapchain swapchain, const XrSwapchainImageAcquireInfo* acquireInfo, uint32_t* index)
{
  static auto const Orig_xrAcquireSwapchainImage = reshade::hooks::call(Hook_xrAcquireSwapchainImage);

  auto const ret = Orig_xrAcquireSwapchainImage(swapchain, acquireInfo, index);
  if (XR_SUCCEEDED(ret)) {
    auto it = gCapturedSwapchains.find(swapchain);
    if (it != gCapturedSwapchains.end()) {
      auto& csd = it->second;
      csd.acquiredIndex.push_back(*index);
    }
  }
  return ret;
}

XRAPI_ATTR XrResult XRAPI_CALL
Hook_xrReleaseSwapchainImage(XrSwapchain swapchain, const XrSwapchainImageReleaseInfo* releaseInfo)
{
  static auto const Orig_xrReleaseSwapchainImage = reshade::hooks::call(Hook_xrReleaseSwapchainImage);

  auto const ret = Orig_xrReleaseSwapchainImage(swapchain, releaseInfo);
  if (XR_SUCCEEDED(ret)) {
    auto it = gCapturedSwapchains.find(swapchain);
    if (it != gCapturedSwapchains.end()) {
      auto& csd = it->second;
      csd.lastReleasedIndex = csd.acquiredIndex.front();
      csd.acquiredIndex.pop_front();
    }
  }
  return ret;
}

XRAPI_ATTR XrResult XRAPI_CALL
Hook_xrEndFrame(XrSession session, const XrFrameEndInfo* frameEndInfo)
{
  static auto const Orig_xrEndFrame = reshade::hooks::call(Hook_xrEndFrame);

  XrSwapchainImageVulkanKHR leftImage{ XR_TYPE_SWAPCHAIN_IMAGE_VULKAN_KHR };
  XrSwapchainCreateInfo leftCI{};
  XrSwapchainImageVulkanKHR rightImage{ XR_TYPE_SWAPCHAIN_IMAGE_VULKAN_KHR };
  XrSwapchainCreateInfo rightCI{};
  auto applyEffects = false;
  for (auto i = 0u; i < frameEndInfo->layerCount; ++i) {
    // We don't care about overlays.
    if (frameEndInfo->layers[i]->type != XR_TYPE_COMPOSITION_LAYER_PROJECTION)
      continue;

    applyEffects = true;

    auto const* proj = reinterpret_cast<const XrCompositionLayerProjection*>(frameEndInfo->layers[i]);
    auto const itLeft
      = gCapturedSwapchains.find(proj->views[static_cast<int>(reshade::openxr::eye::left)].subImage.swapchain);
    assert(itLeft != gCapturedSwapchains.end());
    auto const llri = itLeft->second.lastReleasedIndex;
    leftImage = itLeft->second.surfaceImages[llri];
    leftCI = itLeft->second.createInfo;

    auto const itRight
      = gCapturedSwapchains.find(proj->views[static_cast<int>(reshade::openxr::eye::right)].subImage.swapchain);
    assert(itRight != gCapturedSwapchains.end());
    auto const rlri = itRight->second.lastReleasedIndex;
    rightImage = itRight->second.surfaceImages[rlri];
    rightCI = itRight->second.createInfo;

    break;
  }

  if (applyEffects) {
    extern lockfree_linear_map<void*, reshade::vulkan::device_impl*, 8> g_vulkan_devices;
    auto* device = g_vulkan_devices.at(dispatch_key_from_handle(gVKDevice));

    VkQueue graphicsQueue = VK_NULL_HANDLE;
    device->_dispatch_table.GetDeviceQueue(gVKDevice, device->_graphics_queue_family_index, 0, &graphicsQueue);

    reshade::vulkan::command_queue_impl* queue = nullptr;
    if (const auto queue_it = std::find_if(
          device->_queues.cbegin(),
          device->_queues.cend(),
          [graphicsQueue](reshade::vulkan::command_queue_impl* queue) { return queue->_orig == graphicsQueue; });
        queue_it != device->_queues.cend())
      queue = *queue_it;

    if (s_vr_swapchain == nullptr)
      s_vr_swapchain = new reshade::openxr::swapchain_impl(device, queue, session);

    s_vr_swapchain->on_present({ static_cast<uint64_t>(leftImage.image) }, { static_cast<uint64_t>(rightImage.image) });
    assert(queue == s_vr_swapchain->get_command_queue());
    queue->flush_immediate_command_list();
  }

  return Orig_xrEndFrame(session, frameEndInfo);
}

bool g_oxr_hooks_init_attempted = false;
void
init_oxr_hooks()
{
#ifndef _WIN64
  auto const oxrlh = GetModuleHandleW(L"openxr_loader.dll");
#else
  auto const oxrlh = GetModuleHandleW(L"openxr_loader64.dll");
#endif
  assert(oxrlh != NULL);

  reshade::hooks::install("xrCreateSession", GetProcAddress(oxrlh, "xrCreateSession"), Hook_xrCreateSession);
  reshade::hooks::install("xrDestroySession", GetProcAddress(oxrlh, "xrDestroySession"), Hook_xrDestroySession);
  reshade::hooks::install(
    "xrAcquireSwapchainImage", GetProcAddress(oxrlh, "xrAcquireSwapchainImage"), Hook_xrAcquireSwapchainImage);
  reshade::hooks::install(
    "xrReleaseSwapchainImage", GetProcAddress(oxrlh, "xrReleaseSwapchainImage"), Hook_xrReleaseSwapchainImage);
  reshade::hooks::install("xrCreateSwapchain", GetProcAddress(oxrlh, "xrCreateSwapchain"), Hook_xrCreateSwapchain);
  reshade::hooks::install("xrDestroySwapchain", GetProcAddress(oxrlh, "xrDestroySwapchain"), Hook_xrDestroySwapchain);
  reshade::hooks::install("xrEndFrame", GetProcAddress(oxrlh, "xrEndFrame"), Hook_xrEndFrame);
}

void
check_and_init_openxr_hooks()
{
  if (!g_oxr_hooks_init_attempted) {
    g_oxr_hooks_init_attempted = true;
#ifndef _WIN64
    if (GetModuleHandleW(L"openxr_loader.dll") == nullptr)
      return;
#else
    // TODO_OXR: not tested
    if (GetModuleHandleW(L"openxr_loader64.dll") == nullptr)
      return;
#endif
    init_oxr_hooks();
  }
}
