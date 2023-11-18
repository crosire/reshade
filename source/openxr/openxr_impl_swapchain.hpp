/*
 * Vulkan impl by The Iron Wolf, based on OpenVR code.
 *
 * Copyright (C) 2022 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "runtime.hpp"
#include "addon_manager.hpp"
#include <openxr/openxr.h>

namespace reshade::openxr {

enum class eye
{
  left = 0,
  right
};

class swapchain_impl : public api::api_object_impl<XrSession, runtime> // TODO_OXR: Or XrInstance, or?
{
public:
  swapchain_impl(api::device* device, api::command_queue* graphics_queue, XrSession session);
  ~swapchain_impl();

  api::resource get_back_buffer(uint32_t index = 0) final;

  uint32_t get_back_buffer_count() const final { return 1; }
  uint32_t get_current_back_buffer_index() const final { return 0; }

  api::rect get_eye_rect(reshade::openxr::eye eye) const;
  api::subresource_box get_eye_subresource_box(reshade::openxr::eye eye) const;

  bool on_init();
  void on_reset();

  void on_present(api::resource left_texture, api::resource right_texture);

#if RESHADE_ADDON && RESHADE_FX
  void render_effects(api::command_list* cmd_list, api::resource_view rtv, api::resource_view rtv_srgb) final;
  void render_technique(api::effect_technique handle,
                        api::command_list* cmd_list,
                        api::resource_view rtv,
                        api::resource_view rtv_srgb) final;
#endif

private:
  api::resource _side_by_side_texture = {};
  void* _app_state = nullptr;
  void* _direct3d_device = nullptr;
};
}
