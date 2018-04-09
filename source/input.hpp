/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include <memory>

namespace reshade
{
	class input
	{
	public:
		using window_handle = void *;

		explicit input(window_handle window);

		static void register_window_with_raw_input(window_handle window, bool no_legacy_keyboard, bool no_legacy_mouse);
		static std::shared_ptr<input> register_window(window_handle window);
		static void uninstall();

		bool is_key_down(unsigned int keycode) const;
		bool is_key_down(unsigned int keycode, bool ctrl, bool shift, bool alt) const;
		bool is_key_pressed(unsigned int keycode) const;
		bool is_key_pressed(unsigned int keycode, bool ctrl, bool shift, bool alt) const;
		bool is_key_released(unsigned int keycode) const;
		bool is_any_key_down() const;
		bool is_any_key_pressed() const;
		bool is_any_key_released() const;
		unsigned int last_key_pressed() const;
		unsigned int last_key_released() const;
		bool is_mouse_button_down(unsigned int button) const;
		bool is_mouse_button_pressed(unsigned int button) const;
		bool is_mouse_button_released(unsigned int button) const;
		bool is_any_mouse_button_down() const;
		bool is_any_mouse_button_pressed() const;
		bool is_any_mouse_button_released() const;
		short mouse_wheel_delta() const { return _mouse_wheel_delta; }
		short mouse_movement_delta_x() const { return _mouse_delta[0]; }
		short mouse_movement_delta_y() const { return _mouse_delta[1]; }
		unsigned int mouse_position_x() const { return _mouse_position[0]; }
		unsigned int mouse_position_y() const { return _mouse_position[1]; }

		unsigned short key_to_text(unsigned int keycode) const;

		void block_mouse_input(bool enable);
		void block_keyboard_input(bool enable);

		bool is_blocking_mouse_input() const { return _block_mouse; }
		bool is_blocking_keyboard_input() const { return _block_keyboard; }

		void next_frame();

		static bool handle_window_message(const void *message_data);

	private:
		window_handle _window;
		bool _block_mouse = false, _block_keyboard = false;
		uint8_t _keys[256] = { }, _mouse_buttons[5] = { };
		short _mouse_wheel_delta = 0;
		unsigned int _mouse_position[2] = { };
		short _mouse_delta[2] = { };
		uint64_t _frame_count = 0;
	};
}
