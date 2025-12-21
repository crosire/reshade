/*
 * Copyright (C) 2014 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <memory>
#include <cstdint>

namespace reshade
{
	class input_gamepad
	{
	public:
		enum button
		{
			button_a = 0x1000,
			button_b = 0x2000,
			button_x = 0x4000,
			button_y = 0x8000,
			button_dpad_up = 0x0001,
			button_dpad_down = 0x0002,
			button_dpad_left = 0x0004,
			button_dpad_right = 0x0008,
			button_back = 0x0020,
			button_start = 0x0010,
			button_left_thumb = 0x0040,
			button_right_thumb = 0x0080,
			button_left_shoulder = 0x0100,
			button_right_shoulder = 0x0200,
		};

		explicit input_gamepad();

		/// <summary>
		/// Loads XInput and creates or gets the gamepad input manager singleton.
		/// </summary>
		/// <returns>Pointer to the input manager.</returns>
		static std::shared_ptr<input_gamepad> load();

		/// <summary>
		/// Gets the connection status of the gamepad.
		/// </summary>
		bool is_connected() const { return _last_packet_num != 0; }

		bool is_button_down(unsigned int button) const;
		bool is_button_pressed(unsigned int button) const;
		bool is_button_released(unsigned int button) const;

		auto left_thumb_axis_x() const { return _left_thumb_axis_x; }
		auto left_thumb_axis_y() const { return _left_thumb_axis_y; }
		auto right_thumb_axis_x() const { return _right_thumb_axis_x; }
		auto right_thumb_axis_y() const { return _right_thumb_axis_y; }

		auto left_trigger_position() const { return _left_trigger; }
		auto right_trigger_position() const { return _right_trigger; }

		/// <summary>
		/// Notifies the gamepad input manager to advance a frame.
		/// </summary>
		void next_frame();

	private:
		uint16_t _buttons = 0;
		uint16_t _last_buttons = 0;
		float _left_trigger = 0.0f;
		float _right_trigger = 0.0f;
		float _left_thumb_axis_x = 0.0f;
		float _left_thumb_axis_y = 0.0f;
		float _right_thumb_axis_x = 0.0f;
		float _right_thumb_axis_y = 0.0f;
		uint32_t _last_packet_num = 0;
	};
}
