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
	enum class eye
	{
		left = 0,
		right
	};

	class swapchain_impl : public api::api_object_impl<XrSession, api::swapchain>
	{
	public:
		swapchain_impl(api::device *device, api::command_queue *graphics_queue, XrSession session);
		~swapchain_impl();

		api::device *get_device() final;

		void *get_hwnd() const final { return nullptr; }

		api::resource get_back_buffer(uint32_t index = 0) final;

		uint32_t get_back_buffer_count() const final { return 1; }
		uint32_t get_current_back_buffer_index() const final { return 0; }

		bool check_color_space_support(api::color_space color_space) const final { return color_space == api::color_space::srgb_nonlinear || color_space == api::color_space::extended_srgb_linear; }

		api::color_space get_color_space() const final { return api::color_space::unknown; }

		api::rect get_eye_rect(eye eye) const;
		api::subresource_box get_eye_subresource_box(eye eye) const;

		bool on_init();
		void on_reset();

		void on_present(api::resource left_texture, const api::rect &left_rect, uint32_t left_layer, api::resource right_texture, const api::rect &right_rect, uint32_t right_layer);

	private:
		api::device *const _device;
		api::command_queue *const _graphics_queue;
		api::resource _side_by_side_texture = {};
	};
}
