/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
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
		/// <summary>
		/// A window handle (HWND).
		/// </summary>
		using window_handle = void *;

		explicit input(window_handle window);

		/// <summary>
		/// Registers a window using raw input with the input manager.
		/// </summary>
		/// <param name="window">The window handle.</param>
		/// <param name="no_legacy_keyboard"><c>true</c> if 'RIDEV_NOLEGACY' is set for the keyboard device, <c>false</c> otherwise.</param>
		/// <param name="no_legacy_mouse"><c>true</c> if 'RIDEV_NOLEGACY' is set for the mouse device, <c>false</c> otherwise.</param>
		static void register_window_with_raw_input(window_handle window, bool no_legacy_keyboard, bool no_legacy_mouse);
		/// <summary>
		/// Registers a window using normal input window messages with the input manager.
		/// </summary>
		/// <param name="window">The window handle.</param>
		/// <returns>A pointer to the input manager for the <paramref name="window"/>.</returns>
		static std::shared_ptr<input> register_window(window_handle window);

		bool is_key_down(unsigned int keycode) const;
		bool is_key_pressed(unsigned int keycode) const;
		bool is_key_pressed(unsigned int keycode, bool ctrl, bool shift, bool alt, bool force_modifiers = false) const;
		bool is_key_pressed(const unsigned int key[4], bool force_modifiers = false) const { return is_key_pressed(key[0], key[1] != 0, key[2] != 0, key[3] != 0, force_modifiers); }
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
		int mouse_movement_delta_x() const { return _mouse_position[0] - _last_mouse_position[0]; }
		int mouse_movement_delta_y() const { return _mouse_position[1] - _last_mouse_position[1]; }
		unsigned int mouse_position_x() const { return _mouse_position[0]; }
		unsigned int mouse_position_y() const { return _mouse_position[1]; }

		/// <summary>
		/// Returns the character input as captured by 'WM_CHAR' for the current frame.
		/// </summary>
		const std::wstring &text_input() const { return _text_input; }

		/// <summary>
		/// Set to <c>true</c> to prevent mouse input window messages from reaching the application.
		/// </summary>
		void block_mouse_input(bool enable) { _block_mouse = enable; }
		bool is_blocking_mouse_input() const { return _block_mouse; }
		/// <summary>
		/// Set to <c>true</c> to prevent keyboard input window messages from reaching the application.
		/// </summary>
		void block_keyboard_input(bool enable) { _block_keyboard = enable; }
		bool is_blocking_keyboard_input() const { return _block_keyboard; }

		/// <summary>
		/// Locks access to the input data to the current thread.
		/// </summary>
		/// <returns>A RAII object holding the lock, which releases it after going out of scope.</returns>
		auto lock() { return std::lock_guard<std::mutex>(_mutex); }

		/// <summary>
		/// Notifies the input manager to advance a frame.
		/// This updates input state to e.g. track whether a key was pressed this frame or before.
		/// </summary>
		void next_frame();

		/// <summary>
		/// Generates a human-friendly text representation of the specified <paramref name="keycode"/>.
		/// </summary>
		/// <param name="keycode">The virtual key code to use.</param>
		static std::string key_name(unsigned int keycode);
		/// <summary>
		/// Generates a human-friendly text representation of the specified <paramref name="key"/> shortcut.
		/// </summary>
		/// <param name="key">The shortcut, consisting of the [virtual key code, Ctrl, Shift, Alt].</param>
		static std::string key_name(const unsigned int key[4]);

		/// <summary>
		/// Internal window message procedure. This looks for input messages and updates state for the corresponding windows accordingly.
		/// </summary>
		/// <param name="message_data">A pointer to a <see cref="MSG"/> with the message data.</param>
		/// <returns><c>true</c> if the called should ignore this message, or <c>false</c> if it should pass it on to the application.</returns>
		static bool handle_window_message(const void *message_data);

	private:
		std::mutex _mutex;
		window_handle _window;
		bool _block_mouse = false;
		bool _block_keyboard = false;
		uint8_t _keys[256] = {};
		unsigned int _keys_time[256] = {};
		short _mouse_wheel_delta = 0;
		unsigned int _mouse_position[2] = {};
		unsigned int _last_mouse_position[2] = {};
		uint64_t _frame_count = 0; // Keep track of frame count to identify windows with a lot of rendering
		std::wstring _text_input;
	};
}
