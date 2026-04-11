/*
 * Copyright (C) 2014 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <mutex>
#include <memory>
#include <string>
#include <cstdint>

namespace reshade
{
	class input
	{
	public:
		enum key
		{
			key_button_left = 0x01, // VK_LBUTTON
			key_button_right = 0x02, // VK_RBUTTON
			key_button_middle = 0x04, // VK_MBUTTON
			key_button_xbutton1 = 0x05, // VK_XBUTTON1
			key_button_xbutton2 = 0x06, // VK_XBUTTON2

			key_ctrl = 0x11, // VK_CONTROL
			key_left_ctrl = 0xA2, // VK_LCONTROL
			key_right_ctrl = 0xA3, // VK_RCONTROL
			key_shift = 0x10, // VK_SHIFT
			key_left_shift = 0xA0, // VK_LSHIFT
			key_right_shift = 0xA1, // VK_RSHIFT
			key_alt = 0x12, // VK_MENU
			key_left_alt = 0xA4, // VK_LMENU
			key_right_alt = 0xA5, // VK_RMENU
			key_left_windows = 0x5B, // VK_LWIN
			key_right_windows = 0x5C, // VK_RWIN

			key_space = 0x20, // VK_SPACE
			key_backspace = 0x08, // VK_BACK
			key_tab = 0x09, // VK_TAB
			key_return = 0x0D, // VK_RETURN
			key_pause = 0x13, // VK_PAUSE
			key_caps_lock = 0x14, // VK_CAPITAL
			key_escape = 0x1B, // VK_ESCAPE
			key_left = 0x25, // VK_LEFT
			key_right = 0x27, // VK_RIGHT
			key_up = 0x26, // VK_UP
			key_down = 0x28, // VK_DOWN
			key_page_up = 0x21, // VK_PRIOR
			key_page_down = 0x22, // VK_NEXT
			key_end = 0x23, // VK_END
			key_home = 0x24, // VK_HOME
			key_print_screen = 0x2C, // VK_SNAPSHOT
			key_insert = 0x2D, // VK_INSERT
			key_delete = 0x2E, // VK_DELETE
			key_application = 0x5D, // VK_APPS
			key_num_lock = 0x90, // VK_NUMLOCK
			key_scroll_lock = 0x91, // VK_SCROLL

			key_comma = 0xBC, // VK_OEM_COMMA
			key_minus = 0xBD, // VK_OEM_MINUS
			key_period = 0xBE, // VK_OEM_PERIOD
			key_slash = 0xBF, // VK_OEM_2
			key_semicolon = 0xBA, // VK_OEM_1
			key_plus = 0xBB, // VK_OEM_PLUS
			key_left_bracket = 0xDB, // VK_OEM_4
			key_backslash = 0xDC, // VK_OEM_5
			key_right_bracket = 0xDD, // VK_OEM_6
			key_apostrophe = 0xDE, // VK_OEM_7
			key_grave_accent = 0xC0, // VK_OEM_3

			key_f1 = 0x70, // VK_F1
			key_f2 = 0x71, // VK_F2
			key_f3 = 0x72, // VK_F3
			key_f4 = 0x73, // VK_F4
			key_f5 = 0x74, // VK_F5
			key_f6 = 0x75, // VK_F6
			key_f7 = 0x76, // VK_F7
			key_f8 = 0x77, // VK_F8
			key_f9 = 0x78, // VK_F9
			key_f10 = 0x79, // VK_F10
			key_f11 = 0x80, // VK_F11
			key_f12 = 0x81, // VK_F12

			key_numpad_0 = 0x60, // VK_NUMPAD0
			key_numpad_1 = 0x61, // VK_NUMPAD1
			key_numpad_2 = 0x62, // VK_NUMPAD2
			key_numpad_3 = 0x63, // VK_NUMPAD3
			key_numpad_4 = 0x64, // VK_NUMPAD4
			key_numpad_5 = 0x65, // VK_NUMPAD5
			key_numpad_6 = 0x66, // VK_NUMPAD6
			key_numpad_7 = 0x67, // VK_NUMPAD7
			key_numpad_8 = 0x68, // VK_NUMPAD8
			key_numpad_9 = 0x69, // VK_NUMPAD9
			key_numpad_multiply = 0x6A, // VK_MULTIPLY
			key_numpad_add = 0x6B, // VK_ADD
			key_numpad_subtract = 0x6D, // VK_SUBTRACT
			key_numpad_decimal = 0x6E, // VK_DECIMAL
			key_numpad_divide = 0x6F, // VK_DIVIDE
		};
		enum button
		{
			button_left,
			button_right,
			button_middle,
			button_xbutton1,
			button_xbutton2,
		};

		/// <summary>
		/// A window handle (HWND).
		/// </summary>
		using window_handle = void *;

		explicit input(window_handle window);

		/// <summary>
		/// Registers a window using raw input with the input manager.
		/// </summary>
		/// <param name="window">Window handle of the target window.</param>
		/// <param name="no_legacy_keyboard"><c>true</c> if 'RIDEV_NOLEGACY' is set for the keyboard device, <c>false</c> otherwise.</param>
		/// <param name="no_legacy_mouse"><c>true</c> if 'RIDEV_NOLEGACY' is set for the mouse device, <c>false</c> otherwise.</param>
		static void register_window_with_raw_input(window_handle window, bool no_legacy_keyboard, bool no_legacy_mouse);
		/// <summary>
		/// Registers a window using normal input window messages with the input manager.
		/// </summary>
		/// <param name="window">Window handle of the target window.</param>
		/// <returns>Pointer to the input manager registered for this <paramref name="window"/>.</returns>
		static std::shared_ptr<input> register_window(window_handle window);

		// Before accessing input data with any of the member functions below, first call "lock()" and keep the returned object alive while accessing it.

		bool is_key_down(unsigned int keycode) const;
		bool is_key_pressed(unsigned int keycode) const;
		bool is_key_pressed(unsigned int keycode, bool ctrl, bool shift, bool alt, bool force_modifiers = false) const;
		bool is_key_pressed(const unsigned int key[4], bool force_modifiers = false) const { return is_key_pressed(key[0], key[1] != 0, key[2] != 0, key[3] != 0, force_modifiers); }
		bool is_key_released(unsigned int keycode) const;
		bool is_key_repeated(unsigned int keycode) const;
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
		auto mouse_wheel_delta() const { return _mouse_wheel_delta; }
		auto mouse_movement_delta_x() const { return static_cast<int>(_mouse_position[0] - _last_mouse_position[0]); }
		auto mouse_movement_delta_y() const { return static_cast<int>(_mouse_position[1] - _last_mouse_position[1]); }
		unsigned int mouse_position_x() const { return _mouse_position[0]; }
		unsigned int mouse_position_y() const { return _mouse_position[1]; }
		void max_mouse_position(unsigned int position[2]) const;

		/// <summary>
		/// Gets the character input as captured by 'WM_CHAR' for the current frame.
		/// </summary>
		const std::wstring &text_input() const { return _text_input; }

		/// <summary>
		/// Set to <see langword="true"/> to prevent mouse input window messages from reaching the application.
		/// </summary>
		void block_mouse_input(bool enable);
		bool is_blocking_mouse_input() const { return _block_mouse; }
		static bool is_blocking_any_mouse_input(window_handle window = nullptr);
		/// <summary>
		/// Set to <see langword="true"/> to prevent keyboard input window messages from reaching the application.
		/// </summary>
		void block_keyboard_input(bool enable);
		bool is_blocking_keyboard_input() const { return _block_keyboard; }
		static bool is_blocking_any_keyboard_input(window_handle window = nullptr);
		/// <summary>
		/// Set to <see langword="true"/> to prevent 'SetCursorPos' calls from warping the cursor to a new position.
		/// </summary>
		void block_mouse_cursor_warping(bool enable);
		bool is_blocking_mouse_cursor_warping() const { return _block_cursor_warping; }
		static bool is_blocking_any_mouse_cursor_warping();

		/// <summary>
		/// Locks access to the input data so it cannot be modified in another thread.
		/// </summary>
		/// <returns>RAII object holding the lock, which releases it after going out of scope.</returns>
		auto lock() { return std::unique_lock<std::recursive_mutex>(_mutex); }

		/// <summary>
		/// Notifies the input manager to advance a frame.
		/// This updates input state to e.g. track whether a key was pressed this frame or before.
		/// </summary>
		void next_frame();

		/// <summary>
		/// Generates a human-friendly text representation of the specified <paramref name="keycode"/>.
		/// </summary>
		/// <param name="keycode">Virtual key code to use.</param>
		static std::string key_name(unsigned int keycode);
		/// <summary>
		/// Generates a human-friendly text representation of the specified <paramref name="key"/> shortcut.
		/// </summary>
		/// <param name="key">Key shortcut, consisting of [virtual key code, Ctrl, Shift, Alt].</param>
		static std::string key_name(const unsigned int key[4]);

		/// <summary>
		/// Internal window message procedure. This looks for input messages and updates state for the corresponding windows accordingly.
		/// </summary>
		/// <param name="message_data">Pointer to a <see cref="MSG"/> with the message data.</param>
		/// <returns><see langword="true"/> if the called should ignore this message, or <see langword="false"/> if it should pass it on to the application.</returns>
		static bool handle_window_message(const void *message_data);

	private:
		std::recursive_mutex _mutex;
		window_handle _window;
		bool _block_mouse = false;
		bool _block_keyboard = false;
		bool _block_cursor_warping = false;
		uint8_t _keys[256] = {};
		uint8_t _last_keys[256] = {};
		unsigned int _keys_time[256] = {};
		short _mouse_wheel_delta = 0;
		unsigned int _mouse_position[2] = {};
		unsigned int _last_mouse_position[2] = {};
		uint64_t _frame_count = 0; // Keep track of frame count to identify windows with a lot of rendering
		std::wstring _text_input;
	};

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
