/*
 * Copyright (C) 2022 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "openxr_impl_swapchain.hpp"
#include "dll_log.hpp"

reshade::openxr::swapchain_impl::swapchain_impl(api::device* device,
                                                api::command_queue* graphics_queue,
                                                XrSession session)
  : api_object_impl(session, device, graphics_queue)
{
  //_is_vr = true;
  _is_vr = false; // TODO_OXR: this calls init_vr_gui.  UI is not implemented yet.
  _renderer_id = static_cast<unsigned int>(device->get_api());
}

reshade::openxr::swapchain_impl::~swapchain_impl()
{
  on_reset();
}

reshade::api::resource
reshade::openxr::swapchain_impl::get_back_buffer(uint32_t index)
{
  assert(index == 0);

  return _side_by_side_texture;
}

reshade::api::rect
reshade::openxr::swapchain_impl::get_eye_rect(reshade::openxr::eye eye) const
{
  return api::rect{ static_cast<int32_t>(static_cast<int>(eye) * (_width / 2)),
                    0,
                    static_cast<int32_t>((static_cast<int>(eye) + 1) * (_width / 2)),
                    static_cast<int32_t>(_height) };
}

reshade::api::subresource_box
reshade::openxr::swapchain_impl::get_eye_subresource_box(reshade::openxr::eye eye) const
{
  return api::subresource_box{ static_cast<int32_t>(static_cast<int>(eye) * (_width / 2)),
                               0,
                               0,
                               static_cast<int32_t>((static_cast<int>(eye) + 1) * (_width / 2)),
                               static_cast<int32_t>(_height),
                               1 };
}

bool
reshade::openxr::swapchain_impl::on_init()
{
  // Created in 'on_vr_submit' below
  assert(_side_by_side_texture != 0);

#if RESHADE_ADDON
  invoke_addon_event<addon_event::init_swapchain>(this);
#endif

  return runtime::on_init(nullptr);
}
void
reshade::openxr::swapchain_impl::on_reset()
{
  if (_side_by_side_texture == 0)
    return;

  runtime::on_reset();

#if RESHADE_ADDON
  invoke_addon_event<addon_event::destroy_swapchain>(this);
#endif

  // Make sure none of the resources below are currently in use
  _graphics_queue->wait_idle();

  _device->destroy_resource(_side_by_side_texture);
  _side_by_side_texture = {};
}

void
reshade::openxr::swapchain_impl::on_present(api::resource left_xr_swapchain_image,
                                            api::resource right_xr_swapchain_image)
{
  const api::resource_desc source_desc = _device->get_resource_desc(left_xr_swapchain_image);

  if (source_desc.texture.samples > 1 && !_device->check_capability(api::device_caps::resolve_region))
    return; // Can only copy whole subresources when the resource is multisampled

  reshade::api::subresource_box source_box;
  source_box.left = 0;
  source_box.top = 0;
  source_box.front = 0;
  source_box.right = source_desc.texture.width;
  source_box.bottom = source_desc.texture.height;
  source_box.back = 1;

  auto const region_width = source_box.width();
  auto const target_width = region_width * 2;
  auto const region_height = source_box.height();

  if (region_width == 0 || region_height == 0)
    return;

  // Due to rounding errors with the bounds we have to use a tolerance of 1 pixel per eye (2 pixels in total)
  auto const width_difference = std::abs(static_cast<int32_t>(target_width) - static_cast<int32_t>(_width));
  auto const height_difference = std::abs(static_cast<int32_t>(region_height) - static_cast<int32_t>(_height));

  if (width_difference > 2 || height_difference > 2
      || api::format_to_typeless(source_desc.texture.format) != api::format_to_typeless(_back_buffer_format)) {
    LOG(INFO) << "Resizing runtime " << this << " in VR to " << target_width << "x" << region_height << " ...";

    on_reset();

    if (!_device->create_resource(api::resource_desc(target_width,
                                                     region_height,
                                                     1,
                                                     1,
                                                     api::format_to_typeless(source_desc.texture.format),
                                                     1,
                                                     api::memory_heap::gpu_only,
                                                     api::resource_usage::render_target
                                                       | api::resource_usage::copy_source
                                                       | api::resource_usage::copy_dest),
                                  nullptr,
                                  api::resource_usage::general,
                                  &_side_by_side_texture)) {
      LOG(ERROR) << "Failed to create region texture!";
      return;
    }

    if (!on_init())
      return;
  }

  if (!is_initialized())
    return;

  auto* const cmd_list = _graphics_queue->get_immediate_command_list();

  // Copy region of the source texture (in case of an array texture, copy from the layer corresponding to the current
  // eye)
  auto const left_dest_box = get_eye_subresource_box(eye::left);
  auto const right_dest_box = get_eye_subresource_box(eye::right);

  cmd_list->barrier(_side_by_side_texture, api::resource_usage::undefined, api::resource_usage::copy_dest);
  // OXR end frame images are in VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL.
  cmd_list->barrier(left_xr_swapchain_image, api::resource_usage::render_target, api::resource_usage::copy_source);
  cmd_list->barrier(right_xr_swapchain_image, api::resource_usage::render_target, api::resource_usage::copy_source);
  if (source_desc.texture.samples <= 1) {
    cmd_list->copy_texture_region(left_xr_swapchain_image,
                                  0,
                                  &source_box,
                                  _side_by_side_texture,
                                  0,
                                  &left_dest_box,
                                  api::filter_mode::min_mag_mip_point);
    cmd_list->copy_texture_region(right_xr_swapchain_image,
                                  0,
                                  &source_box,
                                  _side_by_side_texture,
                                  0,
                                  &right_dest_box,
                                  api::filter_mode::min_mag_mip_point);
  } else {
    cmd_list->resolve_texture_region(left_xr_swapchain_image,
                                     0,
                                     &source_box,
                                     _side_by_side_texture,
                                     0,
                                     left_dest_box.left,
                                     left_dest_box.top,
                                     left_dest_box.front,
                                     source_desc.texture.format);

    cmd_list->resolve_texture_region(right_xr_swapchain_image,
                                     0,
                                     &source_box,
                                     _side_by_side_texture,
                                     0,
                                     right_dest_box.left,
                                     right_dest_box.top,
                                     right_dest_box.front,
                                     source_desc.texture.format);
  }
  cmd_list->barrier(_side_by_side_texture, api::resource_usage::copy_dest, api::resource_usage::present);

  runtime::on_present();

  reshade::api::subresource_box left_source_box;
  left_source_box.left = 0;
  left_source_box.top = 0;
  left_source_box.front = 0;
  left_source_box.right = source_desc.texture.width;
  left_source_box.bottom = source_desc.texture.height;
  left_source_box.back = 1;

  reshade::api::subresource_box right_source_box;
  right_source_box.left = source_desc.texture.width;
  right_source_box.top = 0;
  right_source_box.front = 0;
  right_source_box.right = source_desc.texture.width * 2;
  right_source_box.bottom = source_desc.texture.height;
  right_source_box.back = 1;

  // Copy region of the source texture (in case of an array texture, copy from the layer corresponding to the current
  // eye)
  auto const dest_box = get_eye_subresource_box(reshade::openxr::eye::left);

  cmd_list->barrier(left_xr_swapchain_image, api::resource_usage::copy_source, api::resource_usage::copy_dest);
  cmd_list->barrier(right_xr_swapchain_image, api::resource_usage::copy_source, api::resource_usage::copy_dest);
  auto bb = get_back_buffer();
  cmd_list->barrier(bb, api::resource_usage::present, api::resource_usage::copy_source);

  if (source_desc.texture.samples <= 1) {
    cmd_list->copy_texture_region(
      bb, 0, &left_source_box, left_xr_swapchain_image, 0, &dest_box, api::filter_mode::min_mag_mip_point);
    cmd_list->copy_texture_region(
      bb, 0, &right_source_box, right_xr_swapchain_image, 0, &dest_box, api::filter_mode::min_mag_mip_point);
  }

  cmd_list->barrier(left_xr_swapchain_image, api::resource_usage::copy_dest, api::resource_usage::render_target);
  cmd_list->barrier(right_xr_swapchain_image, api::resource_usage::copy_dest, api::resource_usage::render_target);
}

#if RESHADE_ADDON && RESHADE_FX
void
reshade::openxr::swapchain_impl::render_effects(api::command_list* cmd_list,
                                                api::resource_view rtv,
                                                api::resource_view rtv_srgb)
{
  runtime::render_effects(cmd_list, rtv, rtv_srgb);
}
void
reshade::openxr::swapchain_impl::render_technique(api::effect_technique handle,
                                                  api::command_list* cmd_list,
                                                  api::resource_view rtv,
                                                  api::resource_view rtv_srgb)
{
  runtime::render_technique(handle, cmd_list, rtv, rtv_srgb);
}
#endif
