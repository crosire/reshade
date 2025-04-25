/*
 * Copyright (C) 2022 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "reshade_api_object_impl.hpp"
#include <Unknwn.h>
#include <openxr/openxr.h>

namespace reshade::openxr
{
	class swapchain_impl : public api::api_object_impl<XrSession, api::swapchain>
	{
	public:
		swapchain_impl(api::device *device, api::command_queue *graphics_queue, XrSession session);
		~swapchain_impl();

		api::device *get_device() final;

		void *get_hwnd() const final { return nullptr; }

		api::resource get_back_buffer(uint32_t index = 0) final;

		uint32_t get_back_buffer_count() const final;
		uint32_t get_current_back_buffer_index() const final;

		bool check_color_space_support(api::color_space color_space) const final { return color_space == api::color_space::srgb_nonlinear || color_space == api::color_space::extended_srgb_linear; }

		api::color_space get_color_space() const final { return api::color_space::unknown; }

		api::rect get_view_rect(uint32_t index, uint32_t view_count) const;
		api::subresource_box get_view_subresource_box(uint32_t index, uint32_t view_count) const;

		void on_init();
		void on_reset();

		void on_present(uint32_t view_count, const api::resource *view_textures, const api::subresource_box *view_boxes, const uint32_t *view_layers, const std::vector<api::resource> *swapchain_images, uint32_t swap_index);

	private:
		api::device *const _device;
		api::command_queue *const _graphics_queue;
		api::resource _side_by_side_texture = {};
		const std::vector<api::resource> *_swapchain_images = nullptr;
		uint32_t _swap_index = 0;
	};
}
