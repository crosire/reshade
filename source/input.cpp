/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "log.hpp"
#include "input.hpp"
#include "hook_manager.hpp"
#include <assert.h>
#include <Windows.h>
#include <mutex>
#include <algorithm>
#include <unordered_map>

namespace reshade
{
	static std::mutex s_mutex;
	static std::unordered_map<HWND, unsigned int> s_raw_input_windows;
	static std::unordered_map<HWND, std::weak_ptr<input>> s_windows;

	input::input(window_handle window) : _window(window)
	{
		assert(window != nullptr);
	}

	void input::register_window_with_raw_input(window_handle window, bool no_legacy_keyboard, bool no_legacy_mouse)
	{
		const std::lock_guard<std::mutex> lock(s_mutex);

		const auto flags = (no_legacy_keyboard ? 0x1 : 0u) | (no_legacy_mouse ? 0x2 : 0u);
		const auto insert = s_raw_input_windows.emplace(static_cast<HWND>(window), flags);

		if (!insert.second)
		{
			insert.first->second |= flags;
		}
	}
	std::shared_ptr<input> input::register_window(window_handle window)
	{
		const std::lock_guard<std::mutex> lock(s_mutex);

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

		// Ignore messages that are not related to mouse or keyboard input
		if (details.message != WM_INPUT && !is_mouse_message && !is_keyboard_message)
		{
			return false;
		}

		const std::lock_guard<std::mutex> lock(s_mutex);

		// Remove any expired entry from the list
		for (auto it = s_windows.begin(); it != s_windows.end();)
			it->second.expired() ? it = s_windows.erase(it) : ++it;

		// Look up the window in the list of known input windows
		auto input_window = s_windows.find(details.hwnd);
		const auto raw_input_window = s_raw_input_windows.find(details.hwnd);

		if (input_window == s_windows.end())
		{
			// Walk through the window chain and until an known window is found
			EnumChildWindows(details.hwnd, [](HWND hwnd, LPARAM lparam) -> BOOL {
				auto &input_window = *reinterpret_cast<decltype(s_windows)::iterator *>(lparam);
				// Return true to continue enumeration
				return (input_window = s_windows.find(hwnd)) == s_windows.end();
			}, reinterpret_cast<LPARAM>(&input_window));
		}

		if (input_window == s_windows.end() && raw_input_window != s_raw_input_windows.end())
		{
			// Reroute this raw input message to the window with the most rendering
			input_window = std::max_element(s_windows.begin(), s_windows.end(),
				[](auto lhs, auto rhs) { return lhs.second.lock()->_frame_count < rhs.second.lock()->_frame_count; });
		}

		if (input_window == s_windows.end())
		{
			return false;
		}

		RAWINPUT raw_data = {};
		UINT raw_data_size = sizeof(raw_data);

		const auto input_lock = input_window->second.lock();
		input &input = *input_lock;

		// Calculate window client mouse position
		ScreenToClient(static_cast<HWND>(input._window), &details.pt);

		input._mouse_position[0] = details.pt.x;
		input._mouse_position[1] = details.pt.y;

		switch (details.message)
		{
			case WM_INPUT:
				if (GET_RAWINPUT_CODE_WPARAM(details.wParam) != RIM_INPUT ||
					GetRawInputData(reinterpret_cast<HRAWINPUT>(details.lParam), RID_INPUT, &raw_data, &raw_data_size, sizeof(raw_data.header)) == UINT(-1))
				{
					break;
				}

				switch (raw_data.header.dwType)
				{
					case RIM_TYPEMOUSE:
						is_mouse_message = true;

						if (raw_input_window != s_raw_input_windows.end() && (raw_input_window->second & 0x2) == 0)
							break;

						if (raw_data.data.mouse.usButtonFlags & RI_MOUSE_LEFT_BUTTON_DOWN)
							input._mouse_buttons[0] = 0x88;
						else if (raw_data.data.mouse.usButtonFlags & RI_MOUSE_LEFT_BUTTON_UP)
							input._mouse_buttons[0] = 0x08;
						if (raw_data.data.mouse.usButtonFlags & RI_MOUSE_RIGHT_BUTTON_DOWN)
							input._mouse_buttons[1] = 0x88;
						else if (raw_data.data.mouse.usButtonFlags & RI_MOUSE_RIGHT_BUTTON_UP)
							input._mouse_buttons[1] = 0x08;
						if (raw_data.data.mouse.usButtonFlags & RI_MOUSE_MIDDLE_BUTTON_DOWN)
							input._mouse_buttons[2] = 0x88;
						else if (raw_data.data.mouse.usButtonFlags & RI_MOUSE_MIDDLE_BUTTON_UP)
							input._mouse_buttons[2] = 0x08;

						if (raw_data.data.mouse.usButtonFlags & RI_MOUSE_BUTTON_4_DOWN)
							input._mouse_buttons[3] = 0x88;
						else if (raw_data.data.mouse.usButtonFlags & RI_MOUSE_BUTTON_4_UP)
							input._mouse_buttons[3] = 0x08;

						if (raw_data.data.mouse.usButtonFlags & RI_MOUSE_BUTTON_5_DOWN)
							input._mouse_buttons[4] = 0x88;
						else if (raw_data.data.mouse.usButtonFlags & RI_MOUSE_BUTTON_5_UP)
							input._mouse_buttons[4] = 0x08;

						if (raw_data.data.mouse.usButtonFlags & RI_MOUSE_WHEEL)
							input._mouse_wheel_delta += static_cast<short>(raw_data.data.mouse.usButtonData) / WHEEL_DELTA;
						break;
					case RIM_TYPEKEYBOARD:
						is_keyboard_message = true;

						if (raw_input_window != s_raw_input_windows.end() && (raw_input_window->second & 0x1) == 0)
							break;

						if (raw_data.data.keyboard.VKey != 0xFF)
							input._keys[raw_data.data.keyboard.VKey] = (raw_data.data.keyboard.Flags & RI_KEY_BREAK) == 0 ? 0x88 : 0x08;
						break;
				}
				break;
			case WM_KEYDOWN:
			case WM_SYSKEYDOWN:
				input._keys[details.wParam] = 0x88;
				break;
			case WM_KEYUP:
			case WM_SYSKEYUP:
				input._keys[details.wParam] = 0x08;
				break;
			case WM_LBUTTONDOWN:
				input._mouse_buttons[0] = 0x88;
				break;
			case WM_LBUTTONUP:
				input._mouse_buttons[0] = 0x08;
				break;
			case WM_RBUTTONDOWN:
				input._mouse_buttons[1] = 0x88;
				break;
			case WM_RBUTTONUP:
				input._mouse_buttons[1] = 0x08;
				break;
			case WM_MBUTTONDOWN:
				input._mouse_buttons[2] = 0x88;
				break;
			case WM_MBUTTONUP:
				input._mouse_buttons[2] = 0x08;
				break;
			case WM_MOUSEWHEEL:
				input._mouse_wheel_delta += GET_WHEEL_DELTA_WPARAM(details.wParam) / WHEEL_DELTA;
				break;
			case WM_XBUTTONDOWN:
				assert(HIWORD(details.wParam) < 3);
				input._mouse_buttons[2 + HIWORD(details.wParam)] = 0x88;
				break;
			case WM_XBUTTONUP:
				assert(HIWORD(details.wParam) < 3);
				input._mouse_buttons[2 + HIWORD(details.wParam)] = 0x08;
				break;
		}

		return (is_mouse_message && input._block_mouse) || (is_keyboard_message && input._block_keyboard);
	}

	bool input::is_key_down(unsigned int keycode) const
	{
		assert(keycode < 256);

		return (_keys[keycode] & 0x80) == 0x80;
	}
	bool input::is_key_down(unsigned int keycode, bool ctrl, bool shift, bool alt) const
	{
		return is_key_down(keycode) && (!ctrl || is_key_down(VK_CONTROL)) && (!shift || is_key_down(VK_SHIFT)) && (!alt || is_key_down(VK_MENU));
	}
	bool input::is_key_pressed(unsigned int keycode) const
	{
		assert(keycode < 256);

		return (_keys[keycode] & 0x88) == 0x88;
	}
	bool input::is_key_pressed(unsigned int keycode, bool ctrl, bool shift, bool alt) const
	{
		return is_key_pressed(keycode) && (!ctrl || is_key_down(VK_CONTROL)) && (!shift || is_key_down(VK_SHIFT)) && (!alt || is_key_down(VK_MENU));
	}
	bool input::is_key_released(unsigned int keycode) const
	{
		assert(keycode < 256);

		return (_keys[keycode] & 0x88) == 0x08;
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
		return last_key_pressed() != 0;
	}
	bool input::is_any_key_released() const
	{
		return last_key_released() != 0;
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
	bool input::is_mouse_button_down(unsigned int button) const
	{
		assert(button < 5);

		return (_mouse_buttons[button] & 0x80) == 0x80;
	}
	bool input::is_mouse_button_pressed(unsigned int button) const
	{
		assert(button < 5);

		return (_mouse_buttons[button] & 0x88) == 0x88;
	}
	bool input::is_mouse_button_released(unsigned int button) const
	{
		assert(button < 5);

		return (_mouse_buttons[button] & 0x88) == 0x08;
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

	unsigned short input::key_to_text(unsigned int keycode) const
	{
		WORD ch = 0;
		return ToAscii(keycode, MapVirtualKey(keycode, MAPVK_VK_TO_VSC), _keys, &ch, 0) ? ch : 0;
	}

	void input::block_mouse_input(bool enable)
	{
		_block_mouse = enable;

		if (enable)
		{
			ClipCursor(nullptr);
		}
	}
	void input::block_keyboard_input(bool enable)
	{
		_block_keyboard = enable;
	}

	static inline bool is_blocking_mouse_input()
	{
		const auto predicate = [](auto input_window) {
			return !input_window.second.expired() && input_window.second.lock()->is_blocking_mouse_input();
		};

		return std::any_of(reshade::s_windows.cbegin(), reshade::s_windows.cend(), predicate);
	}
	static inline bool is_blocking_keyboard_input()
	{
		const auto predicate = [](auto input_window) {
			return !input_window.second.expired() && input_window.second.lock()->is_blocking_keyboard_input();
		};

		return std::any_of(reshade::s_windows.cbegin(), reshade::s_windows.cend(), predicate);
	}

	void input::next_frame()
	{
		_frame_count++;

		for (auto &state : _keys)
		{
			state &= ~0x8;
		}
		for (auto &state : _mouse_buttons)
		{
			state &= ~0x8;
		}

		_mouse_wheel_delta = 0;
		_last_mouse_position[0] = _mouse_position[0];
		_last_mouse_position[1] = _mouse_position[1];

		// Update caps lock state
		_keys[VK_CAPITAL] |= GetKeyState(VK_CAPITAL) & 0x1;

		// Update modifier key state
		if ((_keys[VK_MENU] & 0x88) != 0 &&
			(GetKeyState(VK_MENU) & 0x8000) == 0)
		{
			_keys[VK_MENU] = 0x08;
		}

		// Update print screen state
		if ((_keys[VK_SNAPSHOT] & 0x80) == 0 &&
			(GetAsyncKeyState(VK_SNAPSHOT) & 0x8000) != 0)
		{
			_keys[VK_SNAPSHOT] = 0x88;
		}
	}
}

HOOK_EXPORT BOOL WINAPI HookGetMessageA(LPMSG lpMsg, HWND hWnd, UINT wMsgFilterMin, UINT wMsgFilterMax)
{
	static const auto trampoline = reshade::hooks::call(&HookGetMessageA);

	if (!trampoline(lpMsg, hWnd, wMsgFilterMin, wMsgFilterMax))
	{
		return FALSE;
	}

	assert(lpMsg != nullptr);

	if (lpMsg->hwnd != nullptr && reshade::input::handle_window_message(lpMsg))
	{
		// Change message so it is ignored by the recipient window
		lpMsg->message = WM_NULL;
	}

	return TRUE;
}
HOOK_EXPORT BOOL WINAPI HookGetMessageW(LPMSG lpMsg, HWND hWnd, UINT wMsgFilterMin, UINT wMsgFilterMax)
{
	static const auto trampoline = reshade::hooks::call(&HookGetMessageW);

	if (!trampoline(lpMsg, hWnd, wMsgFilterMin, wMsgFilterMax))
	{
		return FALSE;
	}

	assert(lpMsg != nullptr);

	if (lpMsg->hwnd != nullptr && reshade::input::handle_window_message(lpMsg))
	{
		// Change message so it is ignored by the recipient window
		lpMsg->message = WM_NULL;
	}

	return TRUE;
}
HOOK_EXPORT BOOL WINAPI HookPeekMessageA(LPMSG lpMsg, HWND hWnd, UINT wMsgFilterMin, UINT wMsgFilterMax, UINT wRemoveMsg)
{
	static const auto trampoline = reshade::hooks::call(&HookPeekMessageA);

	if (!trampoline(lpMsg, hWnd, wMsgFilterMin, wMsgFilterMax, wRemoveMsg))
	{
		return FALSE;
	}

	assert(lpMsg != nullptr);

	if (lpMsg->hwnd != nullptr && (wRemoveMsg & PM_REMOVE) != 0 && reshade::input::handle_window_message(lpMsg))
	{
		// Change message so it is ignored by the recipient window
		lpMsg->message = WM_NULL;
	}

	return TRUE;
}
HOOK_EXPORT BOOL WINAPI HookPeekMessageW(LPMSG lpMsg, HWND hWnd, UINT wMsgFilterMin, UINT wMsgFilterMax, UINT wRemoveMsg)
{
	static const auto trampoline = reshade::hooks::call(&HookPeekMessageW);

	if (!trampoline(lpMsg, hWnd, wMsgFilterMin, wMsgFilterMax, wRemoveMsg))
	{
		return FALSE;
	}

	assert(lpMsg != nullptr);

	if (lpMsg->hwnd != nullptr && (wRemoveMsg & PM_REMOVE) != 0 && reshade::input::handle_window_message(lpMsg))
	{
		// Change message so it is ignored by the recipient window
		lpMsg->message = WM_NULL;
	}

	return TRUE;
}

HOOK_EXPORT BOOL WINAPI HookRegisterRawInputDevices(PCRAWINPUTDEVICE pRawInputDevices, UINT uiNumDevices, UINT cbSize)
{
	LOG(INFO) << "Redirecting '" << "RegisterRawInputDevices" << "(" << pRawInputDevices << ", " << uiNumDevices << ", " << cbSize << ")' ...";

	for (UINT i = 0; i < uiNumDevices; ++i)
	{
		const auto &device = pRawInputDevices[i];

		LOG(INFO) << "> Dumping device registration at index " << i << ":";
		LOG(INFO) << "  +-----------------------------------------+-----------------------------------------+";
		LOG(INFO) << "  | Parameter                               | Value                                   |";
		LOG(INFO) << "  +-----------------------------------------+-----------------------------------------+";
		LOG(INFO) << "  | UsagePage                               | " << std::setw(39) << std::hex << device.usUsagePage << std::dec << " |";
		LOG(INFO) << "  | Usage                                   | " << std::setw(39) << std::hex << device.usUsage << std::dec << " |";
		LOG(INFO) << "  | Flags                                   | " << std::setw(39) << std::hex << device.dwFlags << std::dec << " |";
		LOG(INFO) << "  | TargetWindow                            | " << std::setw(39) << device.hwndTarget << " |";
		LOG(INFO) << "  +-----------------------------------------+-----------------------------------------+";

		if (device.usUsagePage != 1 || device.hwndTarget == nullptr)
		{
			continue;
		}

		reshade::input::register_window_with_raw_input(device.hwndTarget, device.usUsage == 0x06 && (device.dwFlags & RIDEV_NOLEGACY) != 0, device.usUsage == 0x02 && (device.dwFlags & RIDEV_NOLEGACY) != 0);
	}

	if (!reshade::hooks::call(&HookRegisterRawInputDevices)(pRawInputDevices, uiNumDevices, cbSize))
	{
		LOG(WARNING) << "> 'RegisterRawInputDevices' failed with error code " << GetLastError() << "!";

		return FALSE;
	}

	return TRUE;
}

HOOK_EXPORT BOOL WINAPI HookPostMessageA(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	if (reshade::is_blocking_mouse_input() && Msg == WM_MOUSEMOVE)
	{
		return TRUE;
	}

	static const auto trampoline = reshade::hooks::call(&HookPostMessageA);

	return trampoline(hWnd, Msg, wParam, lParam);
}
HOOK_EXPORT BOOL WINAPI HookPostMessageW(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	if (reshade::is_blocking_mouse_input() && Msg == WM_MOUSEMOVE)
	{
		return TRUE;
	}

	static const auto trampoline = reshade::hooks::call(&HookPostMessageW);

	return trampoline(hWnd, Msg, wParam, lParam);
}

POINT last_cursor_position = { };

HOOK_EXPORT BOOL WINAPI HookSetCursorPosition(int X, int Y)
{
	if (reshade::is_blocking_mouse_input())
	{
		last_cursor_position.x = X;
		last_cursor_position.y = Y;

		return TRUE;
	}

	static const auto trampoline = reshade::hooks::call(&HookSetCursorPosition);

	return trampoline(X, Y);
}
HOOK_EXPORT BOOL WINAPI HookGetCursorPosition(LPPOINT lpPoint)
{
	if (reshade::is_blocking_mouse_input())
	{
		assert(lpPoint != nullptr);

		*lpPoint = last_cursor_position;

		return TRUE;
	}

	static const auto trampoline = reshade::hooks::call(&HookGetCursorPosition);

	return trampoline(lpPoint);
}
