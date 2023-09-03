/*
 * OpenXR Vulkan wiring by The Iron Wolf.
 *
 * Note this is not a general wiring, it requires Reshade32 loaded by the game right
 * before OXR swapchain creation.  This used in GTR2 with CCGEP plugin.
 *
 * I tried creating a general solution using OXR API Layer (xr-vk-layerbranch), but no matter what I do game would
 * crash on call to ::xrCreateSession. So instead, GTR2 would optionally load Reshade before creating OXR
 * swapchains. And, use good old hooks.
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
#include <openxr.h>
#include "openxr_platform.h"

#ifdef false

static inline vr::VRTextureBounds_t
calc_side_by_side_bounds(vr::EVREye eye, const vr::VRTextureBounds_t* orig_bounds)
{
  auto bounds = (eye != vr::Eye_Right) ? vr::VRTextureBounds_t{ 0.0f, 0.0f, 0.5f, 1.0f } : // Left half of the texture
                  vr::VRTextureBounds_t{ 0.5f, 0.0f, 1.0f, 1.0f };                         // Right half of the texture

  if (orig_bounds != nullptr && orig_bounds->uMin > orig_bounds->uMax)
    std::swap(bounds.uMin, bounds.uMax);
  if (orig_bounds != nullptr && orig_bounds->vMin > orig_bounds->vMax)
    std::swap(bounds.vMin, bounds.vMax);

  return bounds;
}

static vr::EVRCompositorError
on_vr_submit_vulkan(vr::IVRCompositor* compositor,
                    vr::EVREye eye,
                    const vr::VRVulkanTextureData_t* texture,
                    const vr::VRTextureBounds_t* bounds,
                    vr::EVRSubmitFlags flags,
                    std::function<vr::EVRCompositorError(vr::EVREye eye,
                                                         void* texture,
                                                         const vr::VRTextureBounds_t* bounds,
                                                         vr::EVRSubmitFlags flags)> submit)
{
  extern lockfree_linear_map<void*, reshade::vulkan::device_impl*, 8> g_vulkan_devices;

  reshade::vulkan::device_impl* device = g_vulkan_devices.at(dispatch_key_from_handle(texture->m_pDevice));
  if (device == nullptr)
    goto normal_submit;

  reshade::vulkan::command_queue_impl* queue = nullptr;
  if (const auto queue_it = std::find_if(
        device->_queues.cbegin(),
        device->_queues.cend(),
        [texture](reshade::vulkan::command_queue_impl* queue) { return queue->_orig == texture->m_pQueue; });
      queue_it != device->_queues.cend())
    queue = *queue_it;
  else
    goto normal_submit;

  if (s_vr_swapchain == nullptr)
    // OpenVR requires the passed in queue to be a graphics queue, so can safely
    // use it
    s_vr_swapchain = new reshade::openvr::swapchain_impl(device, queue, compositor);
  else if (s_vr_swapchain->get_device() != device)
    return vr::VRCompositorError_InvalidTexture;

  // Image should be in VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL layout at this
  // point
  if (!s_vr_swapchain->on_vr_submit(eye,
                                    { (uint64_t)(VkImage)texture->m_nImage },
                                    bounds,
                                    (flags & vr::Submit_VulkanTextureWithArrayData) != 0
                                      ? static_cast<const vr::VRVulkanTextureArrayData_t*>(texture)->m_unArrayIndex
                                      : 0)) {
    // Failed to initialize effect runtime or copy the eye texture, so submit
    // normally without applying effects
#if RESHADE_VERBOSE_LOG
    LOG(ERROR) << "Failed to initialize effect runtime or copy the eye texture for eye " << eye << '!';
#endif
  normal_submit:
    return submit(eye, (void*)texture, bounds, flags);
  }

  // Skip submission of the first eye and instead submit both left and right eye
  // in one step after application submitted both
  if (eye != vr::Eye_Right) {
#if RESHADE_ADDON
    const reshade::api::rect left_rect = s_vr_swapchain->get_eye_rect(eye);
    reshade::invoke_addon_event<reshade::addon_event::present>(
      queue, s_vr_swapchain, &left_rect, &left_rect, 0, nullptr);
#endif
    return vr::VRCompositorError_None;
  } else {
    const reshade::api::rect right_rect = s_vr_swapchain->get_eye_rect(eye);
#if RESHADE_ADDON
    reshade::invoke_addon_event<reshade::addon_event::present>(
      queue, s_vr_swapchain, &right_rect, &right_rect, 0, nullptr);
#endif
    s_vr_swapchain->on_present();

    assert(queue == s_vr_swapchain->get_command_queue());
    queue->flush_immediate_command_list();

    vr::VRVulkanTextureData_t target_texture = *texture;
    target_texture.m_nImage = (uint64_t)(VkImage)s_vr_swapchain->get_back_buffer().handle;
    target_texture.m_nWidth = right_rect.width() * 2;
    target_texture.m_nHeight = right_rect.height();
    // Multisampled source textures were already resolved, so sample count is
    // always one at this point
    target_texture.m_nSampleCount = 1;
    // The side-by-side texture is not an array texture
    flags = static_cast<vr::EVRSubmitFlags>(flags & ~vr::Submit_VulkanTextureWithArrayData);

    const vr::VRTextureBounds_t left_bounds = calc_side_by_side_bounds(vr::Eye_Left, bounds);
    submit(vr::Eye_Left, &target_texture, &left_bounds, flags);
    const vr::VRTextureBounds_t right_bounds = calc_side_by_side_bounds(vr::Eye_Right, bounds);
    return submit(vr::Eye_Right, &target_texture, &right_bounds, flags);
  }
}

VR_Interface_Impl(
  IVRCompositor,
  Submit,
  6,
  007,
  {
    if (pTexture == nullptr)
      return vr::VRCompositorError_InvalidTexture;

    const auto submit_lambda
      = [&](vr::EVREye eye, void* handle, const vr::VRTextureBounds_t* bounds, vr::EVRSubmitFlags flags) {
          assert(flags == vr::Submit_Default);
          return VR_Interface_Call(eye, eTextureType, handle, bounds);
        };

    switch (eTextureType) {
      case vr::TextureType_DirectX: // API_DirectX
        return on_vr_submit_d3d11(
          pThis, eEye, static_cast<ID3D11Texture2D*>(pTexture), pBounds, vr::Submit_Default, submit_lambda);
      case vr::TextureType_OpenGL: // API_OpenGL
        return on_vr_submit_opengl(pThis,
                                   eEye,
                                   static_cast<GLuint>(reinterpret_cast<uintptr_t>(pTexture)),
                                   pBounds,
                                   vr::Submit_Default,
                                   submit_lambda);
      default:
        return vr::VRCompositorError_InvalidTexture;
    }
  },
  vr::EVRCompositorError,
  /* vr::Hmd_Eye */ vr::EVREye eEye,
  /* vr::GraphicsAPIConvention */ unsigned int eTextureType,
  void* pTexture,
  const vr::VRTextureBounds_t* pBounds)

  VR_Interface_Impl(
    IVRCompositor,
    Submit,
    5,
    012,
    {
      if (pTexture == nullptr || pTexture->handle == nullptr)
        return vr::VRCompositorError_InvalidTexture;

      // Keep track of pose and depth information of both eyes, so that it
      // can all be send in one step after application submitted both
      static vr::VRTextureWithPoseAndDepth_t s_last_texture[2];
      switch (nSubmitFlags & (vr::Submit_TextureWithPose | vr::Submit_TextureWithDepth)) {
        case 0:
          std::memcpy(&s_last_texture[eEye], pTexture, sizeof(vr::Texture_t));
          break;
        case vr::Submit_TextureWithPose:
          std::memcpy(&s_last_texture[eEye], pTexture, sizeof(vr::VRTextureWithPose_t));
          break;
        case vr::Submit_TextureWithDepth:
          // This is not technically compatible with
          // 'vr::VRTextureWithPoseAndDepth_t', but that is fine, since it's
          // only used for storage and none of the fields are accessed
          // directly
          // TODO: The depth texture bounds may be different then the
          // side-by-side bounds which are used for submission
          std::memcpy(&s_last_texture[eEye], pTexture, sizeof(vr::VRTextureWithDepth_t));
          break;
        case vr::Submit_TextureWithPose | vr::Submit_TextureWithDepth:
          std::memcpy(&s_last_texture[eEye], pTexture, sizeof(vr::VRTextureWithPoseAndDepth_t));
          break;
      }

      const auto submit_lambda
        = [&](vr::EVREye eye, void* handle, const vr::VRTextureBounds_t* bounds, vr::EVRSubmitFlags flags) {
            // Use the pose and/or depth information that was previously stored
            // during submission, but overwrite the texture handle
            vr::VRTextureWithPoseAndDepth_t texture = s_last_texture[eye];
            texture.handle = handle;
            return VR_Interface_Call(eye, &texture, bounds, flags);
          };

      switch (pTexture->eType) {
        case vr::TextureType_DirectX:
          return on_vr_submit_d3d11(
            pThis, eEye, static_cast<ID3D11Texture2D*>(pTexture->handle), pBounds, nSubmitFlags, submit_lambda);
        case vr::TextureType_DirectX12:
          return on_vr_submit_d3d12(pThis,
                                    eEye,
                                    static_cast<const vr::D3D12TextureData_t*>(pTexture->handle),
                                    pBounds,
                                    nSubmitFlags,
                                    submit_lambda);
        case vr::TextureType_OpenGL:
          return on_vr_submit_opengl(pThis,
                                     eEye,
                                     static_cast<GLuint>(reinterpret_cast<uintptr_t>(pTexture->handle)),
                                     pBounds,
                                     nSubmitFlags,
                                     submit_lambda);
        case vr::TextureType_Vulkan:
          return on_vr_submit_vulkan(pThis,
                                     eEye,
                                     static_cast<const vr::VRVulkanTextureData_t*>(pTexture->handle),
                                     pBounds,
                                     nSubmitFlags,
                                     submit_lambda);
        default:
          return vr::VRCompositorError_InvalidTexture;
      }
    },
    vr::EVRCompositorError,
    vr::EVREye eEye,
    const vr::Texture_t* pTexture,
    const vr::VRTextureBounds_t* pBounds,
    vr::EVRSubmitFlags nSubmitFlags)

#endif

  // TODO_OXR:
  static auto constexpr OXR_EYE_COUNT = 2u;
struct CapturedSwapchain
{
  // TODO_OXR: why deque?
  std::deque<uint32_t> acquiredIndex;
  uint32_t lastReleasedIndex{ 0 };

  XrSwapchainCreateInfo createInfo{};
  std::vector<XrSwapchainImageVulkanKHR> surfaceImages;

  // TODO_OXR:
  XrSwapchain fullFovSwapchain[OXR_EYE_COUNT]{ XR_NULL_HANDLE, XR_NULL_HANDLE };
  /* ComPtr<ID3D11Texture2D> flatImage[xr::QuadView::Count];
  ComPtr<ID3D11Texture2D> sharpenedImage[xr::StereoView::Count];

  std::vector<ID3D11Texture2D*> images;
  std::vector<ID3D11Texture2D*> fullFovSwapchainImages[xr::StereoView::Count];*/
};

std::unordered_map<XrSwapchain, CapturedSwapchain> gCapturedSwapchains;

XRAPI_ATTR XrResult XRAPI_CALL
Hook_xrCreateSwapchain(XrSession session, const XrSwapchainCreateInfo* createInfo, XrSwapchain* swapchain)
{
  static auto const Orig_xrCreateSwapchain = reshade::hooks::call(Hook_xrCreateSwapchain);

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
    if (it != gCapturedSwapchains.end()) {
      // Swapchain& entry = it->second;
      // TODO_OXR:
      /* for (uint32_t i = 0; i < OXR_EYE_COUNT; i++) {
        if (entry.fullFovSwapchain[i] != XR_NULL_HANDLE) {
          OpenXrApi::xrDestroySwapchain(entry.fullFovSwapchain[i]);
        }
      }*/
      gCapturedSwapchains.erase(it);
    }
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

// TODO_OXR: this may need cleanup on "Shutdown"
reshade::openxr::swapchain_impl* s_vr_swapchain = nullptr;

XRAPI_ATTR XrResult XRAPI_CALL
Hook_xrEndFrame(XrSession session, const XrFrameEndInfo* frameEndInfo)
{
  static auto const Orig_xrEndFrame = reshade::hooks::call(Hook_xrEndFrame);

  for (auto i = 0u; i < frameEndInfo->layerCount; ++i) {
    // We don't care about overlays.
    if (frameEndInfo->layers[i]->type != XR_TYPE_COMPOSITION_LAYER_PROJECTION)
      continue;

    auto const* proj = reinterpret_cast<const XrCompositionLayerProjection*>(frameEndInfo->layers[i]);
    auto const itLeft
      = gCapturedSwapchains.find(proj->views[static_cast<int>(reshade::openxr::eye::left)].subImage.swapchain);
    assert(itLeft != gCapturedSwapchains.end());

    auto const itRight
      = gCapturedSwapchains.find(proj->views[static_cast<int>(reshade::openxr::eye::right)].subImage.swapchain);
    assert(itRight != gCapturedSwapchains.end());

    break;
  }

  /* extern lockfree_linear_map<void*, reshade::vulkan::device_impl*, 8> g_vulkan_devices;
    reshade::vulkan::device_impl* device = g_vulkan_devices.at(dispatch_key_from_handle(texture->m_pDevice));
    if (device == nullptr)
      goto normal_submit;

    reshade::vulkan::command_queue_impl* queue = nullptr;
    if (const auto queue_it = std::find_if(
          device->_queues.cbegin(),
          device->_queues.cend(),
          [texture](reshade::vulkan::command_queue_impl* queue) { return queue->_orig == texture->m_pQueue; });
        queue_it != device->_queues.cend())
      queue = *queue_it;
    else
      goto normal_submit;

    if (s_vr_swapchain == nullptr)
      // OpenVR requires the passed in queue to be a graphics queue, so can safely
      // use it
      s_vr_swapchain = new reshade::openvr::swapchain_impl(device, queue, compositor);
    else if (s_vr_swapchain->get_device() != device)
      return vr::VRCompositorError_InvalidTexture;

    // Image should be in VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL layout at this
    // point
    if (!s_vr_swapchain->on_vr_submit(eye,
                                      { (uint64_t)(VkImage)texture->m_nImage },
                                      bounds,
                                      (flags & vr::Submit_VulkanTextureWithArrayData) != 0
                                        ? static_cast<const vr::VRVulkanTextureArrayData_t*>(texture)->m_unArrayIndex
                                        : 0)) {
      // Failed to initialize effect runtime or copy the eye texture, so submit
      // normally without applying effects
  #if RESHADE_VERBOSE_LOG
      LOG(ERROR) << "Failed to initialize effect runtime or copy the eye texture for eye " << eye << '!';
  #endif
    normal_submit:
      return submit(eye, (void*)texture, bounds, flags);
    }

    // Skip submission of the first eye and instead submit both left and right eye
    // in one step after application submitted both
    if (eye != vr::Eye_Right) {
  #if RESHADE_ADDON
      const reshade::api::rect left_rect = s_vr_swapchain->get_eye_rect(eye);
      reshade::invoke_addon_event<reshade::addon_event::present>(
        queue, s_vr_swapchain, &left_rect, &left_rect, 0, nullptr);
  #endif
      return vr::VRCompositorError_None;
    } else {
      const reshade::api::rect right_rect = s_vr_swapchain->get_eye_rect(eye);
  #if RESHADE_ADDON
      reshade::invoke_addon_event<reshade::addon_event::present>(
        queue, s_vr_swapchain, &right_rect, &right_rect, 0, nullptr);
  #endif
      s_vr_swapchain->on_present();

      assert(queue == s_vr_swapchain->get_command_queue());
      queue->flush_immediate_command_list();

      vr::VRVulkanTextureData_t target_texture = *texture;
      target_texture.m_nImage = (uint64_t)(VkImage)s_vr_swapchain->get_back_buffer().handle;
      target_texture.m_nWidth = right_rect.width() * 2;
      target_texture.m_nHeight = right_rect.height();
      // Multisampled source textures were already resolved, so sample count is
      // always one at this point
      target_texture.m_nSampleCount = 1;
      // The side-by-side texture is not an array texture
      flags = static_cast<vr::EVRSubmitFlags>(flags & ~vr::Submit_VulkanTextureWithArrayData);

      const vr::VRTextureBounds_t left_bounds = calc_side_by_side_bounds(vr::Eye_Left, bounds);
      submit(vr::Eye_Left, &target_texture, &left_bounds, flags);
      const vr::VRTextureBounds_t right_bounds = calc_side_by_side_bounds(vr::Eye_Right, bounds);
      return submit(vr::Eye_Right, &target_texture, &right_bounds, flags);
    }
    */
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
    // TODO_OXR: not sure
    if (GetModuleHandleW(L"openxr_loader64.dll") == nullptr)
      return;
#endif
    init_oxr_hooks();
  }
}
