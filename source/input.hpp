/*
 * Copyright (C) 2014 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <mutex>
#include <memory>
#include <string>

namespace reshade
{
	class input
	{
	public:
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
}
