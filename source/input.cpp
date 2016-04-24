#include "log.hpp"
#include "input.hpp"

#include <Windows.h>
#include <unordered_map>

namespace reshade
{
	namespace
	{
		std::unordered_map<HWND, std::weak_ptr<input>> s_windows;
		std::unordered_map<HWND, std::shared_ptr<input>> s_raw_input_windows;
	}

	input::input(window_handle window) : _window(window)
	{
	}

	void input::register_window_with_raw_input(window_handle window)
	{
		const auto insert = s_raw_input_windows.emplace(static_cast<HWND>(window), nullptr);

		if (insert.second)
		{
			insert.first->second = register_window(window);
		}
	}
	std::shared_ptr<input> input::register_window(window_handle window)
	{
		const auto insert = s_windows.emplace(static_cast<HWND>(window), std::weak_ptr<input>());

		if (insert.second || insert.first->second.expired())
		{
			LOG(INFO) << "Starting input capture for window " << window << " ...";

			const auto instance = std::make_shared<input>(window);

			insert.first->second = instance;

			return instance;
		}
		else
		{
			return insert.first->second.lock();
		}
	}
	void input::uninstall()
	{
		s_windows.clear();
		s_raw_input_windows.clear();
	}

	bool input::handle_window_message(const void *message_data)
	{
		assert(message_data != nullptr);

		MSG details = *static_cast<const MSG *>(message_data);

		bool is_mouse_message = details.message >= WM_MOUSEFIRST && details.message <= WM_MOUSELAST;
		bool is_keyboard_message = details.message >= WM_KEYFIRST && details.message <= WM_KEYLAST;

		if (details.message != WM_INPUT && !is_mouse_message && !is_keyboard_message)
		{
			return false;
		}

		auto it = s_windows.find(details.hwnd);

		if (it == s_windows.end() && s_raw_input_windows.find(details.hwnd) != s_raw_input_windows.end())
		{
			// This is a raw input message. Just reroute it to the first window for now, since it is rare to have more than one active at a time.
			it = s_windows.begin();
		}
		if (it == s_windows.end() || it->second.expired())
		{
			return false;
		}

		RAWINPUT raw_data = { };
		UINT raw_data_size = sizeof(raw_data);
		const auto input_lock = it->second.lock();
		input &input = *input_lock;

		ScreenToClient(static_cast<HWND>(input._window), &details.pt);

		input._mouse_position[0] = details.pt.x;
		input._mouse_position[1] = details.pt.y;

		switch (details.message)
		{
			case WM_INPUT:
				if (GET_RAWINPUT_CODE_WPARAM(details.wParam) != RIM_INPUT)
				{
					break;
				}
				if (GetRawInputData(reinterpret_cast<HRAWINPUT>(details.lParam), RID_INPUT, &raw_data, &raw_data_size, sizeof(raw_data.header)) == UINT(-1))
				{
					break;
				}

				switch (raw_data.header.dwType)
				{
					case RIM_TYPEMOUSE:
						is_mouse_message = true;

						if (raw_data.data.mouse.usButtonFlags & RI_MOUSE_LEFT_BUTTON_DOWN)
							input._mouse_buttons[0] = 1;
						else if (raw_data.data.mouse.usButtonFlags & RI_MOUSE_LEFT_BUTTON_UP)
							input._mouse_buttons[0] = -1;
						if (raw_data.data.mouse.usButtonFlags & RI_MOUSE_RIGHT_BUTTON_DOWN)
							input._mouse_buttons[1] = 1;
						else if (raw_data.data.mouse.usButtonFlags & RI_MOUSE_RIGHT_BUTTON_UP)
							input._mouse_buttons[1] = -1;
						if (raw_data.data.mouse.usButtonFlags & RI_MOUSE_MIDDLE_BUTTON_DOWN)
							input._mouse_buttons[2] = 1;
						else if (raw_data.data.mouse.usButtonFlags & RI_MOUSE_MIDDLE_BUTTON_UP)
							input._mouse_buttons[2] = -1;

						if (raw_data.data.mouse.usButtonFlags & RI_MOUSE_BUTTON_4_DOWN)
							input._mouse_buttons[3] = 1;
						else if (raw_data.data.mouse.usButtonFlags & RI_MOUSE_BUTTON_4_UP)
							input._mouse_buttons[3] = -1;

						if (raw_data.data.mouse.usButtonFlags & RI_MOUSE_BUTTON_5_DOWN)
							input._mouse_buttons[4] = 1;
						else if (raw_data.data.mouse.usButtonFlags & RI_MOUSE_BUTTON_5_UP)
							input._mouse_buttons[4] = -1;

						if (raw_data.data.mouse.usButtonFlags & RI_MOUSE_WHEEL)
							input._mouse_wheel_delta += static_cast<short>(raw_data.data.mouse.usButtonData) / WHEEL_DELTA;
						break;
					case RIM_TYPEKEYBOARD:
						is_keyboard_message = true;

						if (raw_data.data.keyboard.VKey != 0xFF)
							input._keys[raw_data.data.keyboard.VKey] = (raw_data.data.keyboard.Flags & RI_KEY_BREAK) == 0 ? 1 : -1;
						break;
				}
				break;
			case WM_KEYDOWN:
			case WM_SYSKEYDOWN:
				input._keys[details.wParam] = 1;
				break;
			case WM_KEYUP:
			case WM_SYSKEYUP:
				input._keys[details.wParam] = -1;
				break;
			case WM_LBUTTONDOWN:
				input._mouse_buttons[0] = 1;
				break;
			case WM_LBUTTONUP:
				input._mouse_buttons[0] = -1;
				break;
			case WM_RBUTTONDOWN:
				input._mouse_buttons[1] = 1;
				break;
			case WM_RBUTTONUP:
				input._mouse_buttons[1] = -1;
				break;
			case WM_MBUTTONDOWN:
				input._mouse_buttons[2] = 1;
				break;
			case WM_MBUTTONUP:
				input._mouse_buttons[2] = -1;
				break;
			case WM_MOUSEWHEEL:
				input._mouse_wheel_delta += GET_WHEEL_DELTA_WPARAM(details.wParam) / WHEEL_DELTA;
				break;
			case WM_XBUTTONDOWN:
				input._mouse_buttons[2 + HIWORD(details.wParam)] = 1;
				break;
			case WM_XBUTTONUP:
				input._mouse_buttons[2 + HIWORD(details.wParam)] = -1;
				break;
		}

		return (is_mouse_message && input._block_mouse) || (is_keyboard_message && input._block_keyboard);
	}

	bool input::is_any_key_down() const
	{
		for (unsigned int i = 0; i < 256; i++)
		{
			if (is_key_down(i))
			{
				return true;
			}
		}

		return false;
	}
	bool input::is_any_key_pressed() const
	{
		for (unsigned int i = 0; i < 256; i++)
		{
			if (is_key_pressed(i))
			{
				return true;
			}
		}

		return false;
	}
	bool input::is_any_key_released() const
	{
		for (unsigned int i = 0; i < 256; i++)
		{
			if (is_key_released(i))
			{
				return true;
			}
		}

		return false;
	}
	unsigned int input::last_key_pressed() const
	{
		for (unsigned int i = 0; i < 256; i++)
		{
			if (is_key_pressed(i))
			{
				return i;
			}
		}

		return 0;
	}
	unsigned int input::last_key_released() const
	{
		for (unsigned int i = 0; i < 256; i++)
		{
			if (is_key_released(i))
			{
				return i;
			}
		}

		return 0;
	}
	bool input::is_any_mouse_button_down() const
	{
		for (unsigned int i = 0; i < 5; i++)
		{
			if (is_mouse_button_down(i))
			{
				return true;
			}
		}

		return false;
	}
	bool input::is_any_mouse_button_pressed() const
	{
		for (unsigned int i = 0; i < 5; i++)
		{
			if (is_mouse_button_pressed(i))
			{
				return true;
			}
		}

		return false;
	}
	bool input::is_any_mouse_button_released() const
	{
		for (unsigned int i = 0; i < 5; i++)
		{
			if (is_mouse_button_released(i))
			{
				return true;
			}
		}

		return false;
	}

	void input::next_frame()
	{
		for (auto &state : _keys)
		{
			if (state == -1 || state == 1)
			{
				state++;
			}
		}
		for (auto &state : _mouse_buttons)
		{
			if (state == -1 || state == 1)
			{
				state++;
			}
		}

		_mouse_wheel_delta = 0;

		if (_keys[VK_SNAPSHOT] < 1 && GetAsyncKeyState(VK_SNAPSHOT) & 0x8000)
		{
			_keys[VK_SNAPSHOT] = 1;
		}
	}
}
