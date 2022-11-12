/*
 * Copyright (C) 2014 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <memory>

namespace reshade
{
	class input_gamepad
	{
	public:
		explicit input_gamepad(void *xinput_module);
		~input_gamepad();

		static std::shared_ptr<input_gamepad> load();

		struct state
		{
			uint16_t a : 1;
			uint16_t b : 1;
			uint16_t x : 1;
			uint16_t y : 1;
			uint16_t dpad_up : 1;
			uint16_t dpad_down : 1;
			uint16_t dpad_left : 1;
			uint16_t dpad_right : 1;
			uint16_t start : 1;
			uint16_t back : 1;
			uint16_t left_thumb : 1;
			uint16_t right_thumb : 1;
			uint16_t left_shoulder : 1;
			uint16_t right_shoulder : 1;
			float left_trigger;
			float right_trigger;
			float left_thumb_axis_x;
			float left_thumb_axis_y;
			float right_thumb_axis_x;
			float right_thumb_axis_y;
		};

		bool get_state(unsigned int index, state &state);

	private:
		void *_xinput_module = nullptr;
		void *_xinput_get_state = nullptr;
	};
}
