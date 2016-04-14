#pragma once

#include <memory>
#include <unordered_map>
#include <Windows.h>

namespace reshade
{
	class input
	{
	public:
		explicit input(HWND hwnd);
		~input();

		static std::shared_ptr<input> register_window(HWND hwnd);
		static void register_raw_input_device(const RAWINPUTDEVICE &device);
		static void uninstall();

		bool is_key_down(unsigned int keycode) const
		{
			return _keys[keycode] > 0;
		}
		bool is_key_pressed(unsigned int keycode) const
		{
			return _keys[keycode] == 1;
		}
		bool is_key_released(unsigned int keycode) const
		{
			return _keys[keycode] == -1;
		}
		bool is_mouse_button_down(unsigned int button) const
		{
			return _mouse_buttons[button] > 0;
		}
		bool is_mouse_button_pressed(unsigned int button) const
		{
			return _mouse_buttons[button] == 1;
		}
		bool is_mouse_button_released(unsigned int button) const
		{
			return _mouse_buttons[button] == -1;
		}
		short mouse_wheel_delta() const
		{
			return _mouse_wheel_delta;
		}
		const POINT &mouse_position() const
		{
			return _mouse_position;
		}

		void next_frame();

	private:
		static LRESULT CALLBACK handle_window_message(int nCode, WPARAM wParam, LPARAM lParam);

		HWND _hwnd;
		HHOOK _hook_wndproc;
		signed char _keys[256], _mouse_buttons[5];
		POINT _mouse_position;
		short _mouse_wheel_delta;
		static std::unordered_map<HWND, HHOOK> s_raw_input_hooks;
		static std::unordered_map<HWND, std::weak_ptr<input>> s_windows;
	};
}
