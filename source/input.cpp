#include "log.hpp"
#include "input.hpp"

namespace reshade
{
	std::unordered_map<HWND, HHOOK> input::s_raw_input_hooks;
	std::unordered_map<HWND, std::weak_ptr<input>> input::s_windows;

	input::input(HWND hwnd) : _hwnd(hwnd), _keys(), _mouse_buttons(), _mouse_position()
	{
		_hook_wndproc = SetWindowsHookEx(WH_GETMESSAGE, &handle_window_message, nullptr, GetWindowThreadProcessId(hwnd, nullptr));
	}
	input::~input()
	{
		UnhookWindowsHookEx(_hook_wndproc);
	}

	std::shared_ptr<input> input::register_window(HWND hwnd)
	{
		const auto insert = s_windows.emplace(hwnd, std::weak_ptr<input>());

		if (insert.second || insert.first->second.expired())
		{
			LOG(INFO) << "Starting input capture for window " << hwnd << " ...";

			const auto instance = std::make_shared<input>(hwnd);

			insert.first->second = instance;

			return instance;
		}
		else
		{
			return insert.first->second.lock();
		}
	}
	void input::register_raw_input_device(const RAWINPUTDEVICE &device)
	{
		const auto insert = s_raw_input_hooks.emplace(device.hwndTarget, nullptr);

		if (insert.second)
		{
			LOG(INFO) << "Starting raw input capture for window " << device.hwndTarget << " ...";

			insert.first->second = SetWindowsHookEx(WH_GETMESSAGE, &handle_window_message, nullptr, GetWindowThreadProcessId(device.hwndTarget, nullptr));
		}
	}
	void input::uninstall()
	{
		for (auto &it : s_raw_input_hooks)
		{
			UnhookWindowsHookEx(it.second);
		}

		s_windows.clear();
		s_raw_input_hooks.clear();
	}

	LRESULT CALLBACK input::handle_window_message(int nCode, WPARAM wParam, LPARAM lParam)
	{
		if (nCode < HC_ACTION || (wParam & PM_REMOVE) == 0)
		{
			return CallNextHookEx(nullptr, nCode, wParam, lParam);
		}

		auto details = *reinterpret_cast<MSG *>(lParam);

		if (details.hwnd == nullptr)
		{
			details.hwnd = GetActiveWindow();
		}

		auto it = s_windows.find(details.hwnd);

		if (it == s_windows.end() && s_raw_input_hooks.find(details.hwnd) != s_raw_input_hooks.end())
		{
			// This is a raw input message. Just reroute it to the first window for now, since it is rare to have more than one active at a time.
			it = s_windows.begin();
		}
		if (it == s_windows.end() || it->second.expired())
		{
			return CallNextHookEx(nullptr, nCode, wParam, lParam);
		}

		RAWINPUT raw_data = { };
		UINT raw_data_size = sizeof(raw_data);
		const auto input_lock = it->second.lock();
		input &input = *input_lock;

		ScreenToClient(input._hwnd, &details.pt);

		input._mouse_position.x = details.pt.x;
		input._mouse_position.y = details.pt.y;

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
						if (raw_data.data.mouse.usButtonFlags & RI_MOUSE_LEFT_BUTTON_DOWN)
							input._mouse_buttons[0] = 1;
						else if (raw_data.data.mouse.usButtonFlags & RI_MOUSE_LEFT_BUTTON_UP)
							input._mouse_buttons[0] = -1;
						if (raw_data.data.mouse.usButtonFlags & RI_MOUSE_RIGHT_BUTTON_DOWN)
							input._mouse_buttons[2] = 1;
						else if (raw_data.data.mouse.usButtonFlags & RI_MOUSE_RIGHT_BUTTON_UP)
							input._mouse_buttons[2] = -1;
						if (raw_data.data.mouse.usButtonFlags & RI_MOUSE_MIDDLE_BUTTON_DOWN)
							input._mouse_buttons[1] = 1;
						else if (raw_data.data.mouse.usButtonFlags & RI_MOUSE_MIDDLE_BUTTON_UP)
							input._mouse_buttons[1] = -1;

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
				input._mouse_buttons[2] = 1;
				break;
			case WM_RBUTTONUP:
				input._mouse_buttons[2] = -1;
				break;
			case WM_MBUTTONDOWN:
				input._mouse_buttons[1] = 1;
				break;
			case WM_MBUTTONUP:
				input._mouse_buttons[1] = -1;
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

		return CallNextHookEx(nullptr, nCode, wParam, lParam);
	}

	void input::next_frame()
	{
		for (unsigned int i = 0; i < 256; i++)
		{
			if (_keys[i] == 1)
			{
				_keys[i] = 2;
			}
			else if (_keys[i] == -1)
			{
				_keys[i] = 0;
			}
		}

		for (unsigned int i = 0; i < 3; i++)
		{
			if (_mouse_buttons[i] == 1)
			{
				_mouse_buttons[i] = 2;
			}
			else if (_mouse_buttons[i] == -1)
			{
				_mouse_buttons[i] = 0;
			}
		}

		_mouse_wheel_delta = 0;
	}
}
