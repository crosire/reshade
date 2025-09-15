/*
 * Copyright (C) 2014 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "input.hpp"
#include "dll_log.hpp"
#include "hook_manager.hpp"
#include <atomic>
#include <shared_mutex>
#include <unordered_map>
#include <cstring> // std::memset
#include <algorithm> // std::any_of, std::copy_n, std::max_element
#include <Windows.h>

extern bool is_uwp_app();

extern HANDLE g_exit_event;
static std::shared_mutex s_windows_mutex;
static std::unordered_map<HWND, unsigned int> s_raw_input_windows;
static std::unordered_map<HWND, std::weak_ptr<reshade::input>> s_windows;
static RECT s_last_clip_cursor = {};
static POINT s_last_cursor_position = {};
static std::atomic<bool> s_block_mouse = false;
static std::atomic<bool> s_block_keyboard = false;
static std::atomic<bool> s_block_cursor_warping = false;

extern "C" BOOL WINAPI HookClipCursor(const RECT *lpRect);
extern "C" auto WINAPI HookGetKeyState(int vKey) -> SHORT;
extern "C" auto WINAPI HookGetAsyncKeyState(int vKey) -> SHORT;

reshade::input::input(window_handle window)
	: _window(window)
{
}

void reshade::input::register_window_with_raw_input(window_handle window, bool no_legacy_keyboard, bool no_legacy_mouse)
{
	if (is_uwp_app()) // UWP apps never use legacy input messages
		no_legacy_keyboard = no_legacy_mouse = true;

	assert(window != nullptr);

	const std::unique_lock<std::shared_mutex> lock(s_windows_mutex);

	const auto flags = (no_legacy_keyboard ? 0x1u : 0u) | (no_legacy_mouse ? 0x2u : 0u);
	const auto insert = s_raw_input_windows.emplace(static_cast<HWND>(window), flags);

	if (!insert.second) insert.first->second |= flags;
}

std::shared_ptr<reshade::input> reshade::input::register_window(window_handle window)
{
	assert(window != nullptr);

	DWORD process_id = 0;
	GetWindowThreadProcessId(static_cast<HWND>(window), &process_id);
	if (process_id != GetCurrentProcessId())
	{
		reshade::log::message(reshade::log::level::warning, "Cannot capture input for window %p created by a different process (%lu).", window, process_id);
		return nullptr;
	}

	const std::unique_lock<std::shared_mutex> lock(s_windows_mutex);

	const auto insert = s_windows.emplace(static_cast<HWND>(window), std::weak_ptr<input>());

	if (insert.second || insert.first->second.expired())
	{
#if RESHADE_VERBOSE_LOG
		reshade::log::message(reshade::log::level::debug, "Starting input capture for window %p.", window);
#endif

		const auto instance = std::make_shared<input>(window);
		insert.first->second = instance;

		return instance;
	}
	else
	{
		return insert.first->second.lock();
	}
}

bool reshade::input::handle_window_message(const void *message_data)
{
	assert(message_data != nullptr);

	MSG details = *static_cast<const MSG *>(message_data);

	bool is_mouse_message = details.message >= WM_MOUSEFIRST && details.message <= WM_MOUSELAST;
	bool is_keyboard_message = details.message >= WM_KEYFIRST && details.message <= WM_KEYLAST;

	// Ignore messages that are not related to mouse or keyboard input
	if (details.message != WM_INPUT && !is_mouse_message && !is_keyboard_message)
		return false;

	// Guard access to windows list against race conditions
	std::unique_lock<std::shared_mutex> lock(s_windows_mutex);

	// Remove any expired entry from the list
	for (auto it = s_windows.begin(); it != s_windows.end();)
		if (it->second.expired())
			it = s_windows.erase(it);
		else
			++it;

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
	if (input_window == s_windows.end())
	{
		// Some applications handle input in a child window to the main render window
		if (const HWND parent = GetParent(details.hwnd); parent != NULL)
			input_window = s_windows.find(parent);
	}

	if (input_window == s_windows.end() && raw_input_window != s_raw_input_windows.end())
	{
		// Reroute this raw input message to the window with the most rendering
		input_window = std::max_element(s_windows.begin(), s_windows.end(),
			[](auto lhs, auto rhs) { return lhs.second.lock()->_frame_count < rhs.second.lock()->_frame_count; });
	}

	if (input_window == s_windows.end())
		return false;

	const std::shared_ptr<input> input = input_window->second.lock();
	// It may happen that the input was destroyed between the removal of expired entries above and here, so need to abort in this case
	if (input == nullptr)
		return false;

	// At this point we have a shared pointer to the input object and no longer reference any memory from the windows list, so can release the lock
	lock.unlock();

	// Calculate window client mouse position
	ScreenToClient(static_cast<HWND>(input->_window), &details.pt);

	// Prevent input threads from modifying input while it is accessed elsewhere
	const std::unique_lock<std::recursive_mutex> input_lock(input->_mutex);

	input->_mouse_position[0] = details.pt.x;
	input->_mouse_position[1] = details.pt.y;

	switch (details.message)
	{
	case WM_INPUT:
		RAWINPUT raw_data;
		if (UINT raw_data_size = sizeof(raw_data);
			GET_RAWINPUT_CODE_WPARAM(details.wParam) != RIM_INPUT || // Ignore all input sink messages (when window is not focused)
			GetRawInputData(reinterpret_cast<HRAWINPUT>(details.lParam), RID_INPUT, &raw_data, &raw_data_size, sizeof(raw_data.header)) == UINT(-1))
			break;
		switch (raw_data.header.dwType)
		{
		case RIM_TYPEMOUSE:
			is_mouse_message = true;

			if (raw_input_window == s_raw_input_windows.end() || (raw_input_window->second & 0x2) == 0)
				break; // Input is already handled (since legacy mouse messages are enabled), so nothing to do here

			if (raw_data.data.mouse.usButtonFlags & RI_MOUSE_LEFT_BUTTON_DOWN)
				input->_keys[VK_LBUTTON] = 0x88;
			else if (raw_data.data.mouse.usButtonFlags & RI_MOUSE_LEFT_BUTTON_UP)
				input->_keys[VK_LBUTTON] = 0x08;
			if (raw_data.data.mouse.usButtonFlags & RI_MOUSE_RIGHT_BUTTON_DOWN)
				input->_keys[VK_RBUTTON] = 0x88;
			else if (raw_data.data.mouse.usButtonFlags & RI_MOUSE_RIGHT_BUTTON_UP)
				input->_keys[VK_RBUTTON] = 0x08;
			if (raw_data.data.mouse.usButtonFlags & RI_MOUSE_MIDDLE_BUTTON_DOWN)
				input->_keys[VK_MBUTTON] = 0x88;
			else if (raw_data.data.mouse.usButtonFlags & RI_MOUSE_MIDDLE_BUTTON_UP)
				input->_keys[VK_MBUTTON] = 0x08;

			if (raw_data.data.mouse.usButtonFlags & RI_MOUSE_BUTTON_4_DOWN)
				input->_keys[VK_XBUTTON1] = 0x88;
			else if (raw_data.data.mouse.usButtonFlags & RI_MOUSE_BUTTON_4_UP)
				input->_keys[VK_XBUTTON1] = 0x08;

			if (raw_data.data.mouse.usButtonFlags & RI_MOUSE_BUTTON_5_DOWN)
				input->_keys[VK_XBUTTON2] = 0x88;
			else if (raw_data.data.mouse.usButtonFlags & RI_MOUSE_BUTTON_5_UP)
				input->_keys[VK_XBUTTON2] = 0x08;

			if (raw_data.data.mouse.usButtonFlags & RI_MOUSE_WHEEL)
				input->_mouse_wheel_delta += static_cast<short>(raw_data.data.mouse.usButtonData) / WHEEL_DELTA;
			break;
		case RIM_TYPEKEYBOARD:
			if (raw_data.data.keyboard.VKey == 0)
				break; // Ignore messages without a valid key code

			is_keyboard_message = true;
			// Do not block key up messages if the key down one was not blocked previously
			if (input->is_blocking_keyboard_input() && (raw_data.data.keyboard.Flags & RI_KEY_BREAK) != 0 && raw_data.data.keyboard.VKey < 0xFF && (input->_keys[raw_data.data.keyboard.VKey] & 0x04) == 0)
				is_keyboard_message = false;

			if (raw_input_window == s_raw_input_windows.end() || (raw_input_window->second & 0x1) == 0)
				break; // Input is already handled by 'WM_KEYDOWN' and friends (since legacy keyboard messages are enabled), so nothing to do here

			// Filter out prefix messages without a key code
			if (raw_data.data.keyboard.VKey < 0xFF)
				input->_keys[raw_data.data.keyboard.VKey] = (raw_data.data.keyboard.Flags & RI_KEY_BREAK) == 0 ? 0x88 : 0x08,
				input->_keys_time[raw_data.data.keyboard.VKey] = details.time;

			// No 'WM_CHAR' messages are sent if legacy keyboard messages are disabled, so need to generate text input manually here
			// Cannot use the ToUnicode function always as it seems to reset dead key state and thus calling it can break subsequent application input, should be fine here though since the application is already explicitly using raw input
			// Since Windows 10 version 1607 this supports the 0x2 flag, which prevents the keyboard state from being changed, so it is not a problem there anymore either way
			if (WCHAR ch[3] = {}; (raw_data.data.keyboard.Flags & RI_KEY_BREAK) == 0 && ToUnicode(raw_data.data.keyboard.VKey, raw_data.data.keyboard.MakeCode, input->_keys, ch, 2, 0x2))
				input->_text_input += ch;
			break;
		}
		break;
	case WM_CHAR:
		input->_text_input += static_cast<wchar_t>(details.wParam);
		break;
	case WM_KEYDOWN:
	case WM_SYSKEYDOWN:
		assert(details.wParam > 0 && details.wParam < ARRAYSIZE(input->_keys));
		input->_keys[details.wParam] = 0x88;
		input->_keys_time[details.wParam] = details.time;
		if (input->is_blocking_keyboard_input())
			input->_keys[details.wParam] |= 0x04;
		break;
	case WM_KEYUP:
	case WM_SYSKEYUP:
		assert(details.wParam > 0 && details.wParam < ARRAYSIZE(input->_keys));
		// Do not block key up messages if the key down one was not blocked previously (so key does not get stuck for the application)
		if (input->is_blocking_keyboard_input() && (input->_keys[details.wParam] & 0x04) == 0)
			is_keyboard_message = false;
		input->_keys[details.wParam] = 0x08;
		input->_keys_time[details.wParam] = details.time;
		break;
	case WM_LBUTTONDOWN:
	case WM_LBUTTONDBLCLK: // Double clicking generates this sequence: WM_LBUTTONDOWN -> WM_LBUTTONUP -> WM_LBUTTONDBLCLK -> WM_LBUTTONUP, so handle it like a normal down
		input->_keys[VK_LBUTTON] = 0x88;
		break;
	case WM_LBUTTONUP:
		input->_keys[VK_LBUTTON] = 0x08;
		break;
	case WM_RBUTTONDOWN:
	case WM_RBUTTONDBLCLK:
		input->_keys[VK_RBUTTON] = 0x88;
		break;
	case WM_RBUTTONUP:
		input->_keys[VK_RBUTTON] = 0x08;
		break;
	case WM_MBUTTONDOWN:
	case WM_MBUTTONDBLCLK:
		input->_keys[VK_MBUTTON] = 0x88;
		break;
	case WM_MBUTTONUP:
		input->_keys[VK_MBUTTON] = 0x08;
		break;
	case WM_MOUSEWHEEL:
		input->_mouse_wheel_delta += GET_WHEEL_DELTA_WPARAM(details.wParam) / WHEEL_DELTA;
		break;
	case WM_XBUTTONDOWN:
		assert(HIWORD(details.wParam) == XBUTTON1 || HIWORD(details.wParam) == XBUTTON2);
		input->_keys[VK_XBUTTON1 + (HIWORD(details.wParam) - XBUTTON1)] = 0x88;
		break;
	case WM_XBUTTONUP:
		assert(HIWORD(details.wParam) == XBUTTON1 || HIWORD(details.wParam) == XBUTTON2);
		input->_keys[VK_XBUTTON1 + (HIWORD(details.wParam) - XBUTTON1)] = 0x08;
		break;
	}

	return (is_mouse_message && input->is_blocking_mouse_input()) || (is_keyboard_message && input->is_blocking_keyboard_input());
}

bool reshade::input::is_key_down(unsigned int keycode) const
{
	assert(keycode < ARRAYSIZE(_keys));
	return keycode < ARRAYSIZE(_keys) && (_keys[keycode] & 0x80) == 0x80;
}
bool reshade::input::is_key_pressed(unsigned int keycode) const
{
	assert(keycode < ARRAYSIZE(_keys));
	return keycode > 0 && keycode < ARRAYSIZE(_keys) && (_keys[keycode] & 0x88) == 0x88 && !is_key_repeated(keycode);
}
bool reshade::input::is_key_pressed(unsigned int keycode, bool ctrl, bool shift, bool alt, bool force_modifiers) const
{
	if (keycode == 0)
		return false;

	const bool key_down = is_key_pressed(keycode), ctrl_down = is_key_down(VK_CONTROL), shift_down = is_key_down(VK_SHIFT), alt_down = is_key_down(VK_MENU);
	if (force_modifiers) // Modifier state is required to match
		return key_down && (ctrl == ctrl_down && shift == shift_down && alt == alt_down);
	else // Modifier state is optional and only has to match when down
		return key_down && (!ctrl || ctrl_down) && (!shift || shift_down) && (!alt || alt_down);
}
bool reshade::input::is_key_released(unsigned int keycode) const
{
	assert(keycode < ARRAYSIZE(_keys));
	return keycode > 0 && keycode < ARRAYSIZE(_keys) && (_keys[keycode] & 0x88) == 0x08;
}
bool reshade::input::is_key_repeated(unsigned int keycode) const
{
	assert(keycode < ARRAYSIZE(_keys));
	return keycode < ARRAYSIZE(_keys) && (_last_keys[keycode] & 0x80) == 0x80 && (_keys[keycode] & 0x80) == 0x80;
}

bool reshade::input::is_any_key_down() const
{
	// Skip mouse buttons
	for (unsigned int i = VK_XBUTTON2 + 1; i < ARRAYSIZE(_keys); i++)
		if (is_key_down(i))
			return true;
	return false;
}
bool reshade::input::is_any_key_pressed() const
{
	return last_key_pressed() != 0;
}
bool reshade::input::is_any_key_released() const
{
	return last_key_released() != 0;
}

unsigned int reshade::input::last_key_pressed() const
{
	for (unsigned int i = VK_MBUTTON; i < ARRAYSIZE(_keys); i++)
		if (is_key_pressed(i))
			return i;
	return 0;
}
unsigned int reshade::input::last_key_released() const
{
	for (unsigned int i = VK_MBUTTON; i < ARRAYSIZE(_keys); i++)
		if (is_key_released(i))
			return i;
	return 0;
}

bool reshade::input::is_mouse_button_down(unsigned int button) const
{
	assert(button < 5);
	return is_key_down(VK_LBUTTON + button + (button < 2 ? 0 : 1)); // VK_CANCEL is being ignored by runtime
}
bool reshade::input::is_mouse_button_pressed(unsigned int button) const
{
	assert(button < 5);
	return is_key_pressed(VK_LBUTTON + button + (button < 2 ? 0 : 1)); // VK_CANCEL is being ignored by runtime
}
bool reshade::input::is_mouse_button_released(unsigned int button) const
{
	assert(button < 5);
	return is_key_released(VK_LBUTTON + button + (button < 2 ? 0 : 1)); // VK_CANCEL is being ignored by runtime
}

bool reshade::input::is_any_mouse_button_down() const
{
	for (unsigned int i = 0; i < 5; i++)
		if (is_mouse_button_down(i))
			return true;
	return false;
}
bool reshade::input::is_any_mouse_button_pressed() const
{
	for (unsigned int i = 0; i < 5; i++)
		if (is_mouse_button_pressed(i))
			return true;
	return false;
}
bool reshade::input::is_any_mouse_button_released() const
{
	for (unsigned int i = 0; i < 5; i++)
		if (is_mouse_button_released(i))
			return true;
	return false;
}

void reshade::input::max_mouse_position(unsigned int position[2]) const
{
	RECT rect = {};
	GetClientRect(static_cast<HWND>(_window), &rect);
	position[0] = rect.right;
	position[1] = rect.bottom;
}

void reshade::input::next_frame()
{
	static const auto GetKeyState_trampoline = reshade::hooks::is_hooked(GetKeyState) ? reshade::hooks::call(HookGetKeyState, GetKeyState) : GetKeyState;
	static const auto GetAsyncKeyState_trampoline = reshade::hooks::is_hooked(GetAsyncKeyState) ? reshade::hooks::call(HookGetAsyncKeyState, GetAsyncKeyState) : GetAsyncKeyState;

	_frame_count++;

	// Backup key states from the last processed frame so that state transitions can be identified
	std::copy_n(_keys, 256, _last_keys);

	for (uint8_t &state : _keys)
		state &= ~0x08;

	// Reset any pressed down key states (apart from mouse buttons) that have not been updated for more than 5 seconds
	// Do not check mouse buttons here, since 'GetAsyncKeyState' always returns the state of the physical mouse buttons, not the logical ones in case they were remapped
	// See https://docs.microsoft.com/windows/win32/api/winuser/nf-winuser-getasynckeystate
	// And time is not tracked for mouse buttons anyway
	const DWORD time = GetTickCount();
	for (unsigned int i = 8; i < 256; ++i)
		if ((_keys[i] & 0x80) != 0 &&
			(time - _keys_time[i]) > 5000 &&
			(GetAsyncKeyState_trampoline(i) & 0x8000) == 0)
			(_keys[i] = 0x08);

	_text_input.clear();
	_mouse_wheel_delta = 0;
	_last_mouse_position[0] = _mouse_position[0];
	_last_mouse_position[1] = _mouse_position[1];

	// Update caps lock state
	_keys[VK_CAPITAL] |= GetKeyState_trampoline(VK_CAPITAL) & 0x1;

	// Update modifier key state
	if ((_keys[VK_MENU] & 0x88) != 0 &&
		(GetKeyState_trampoline(VK_MENU) & 0x8000) == 0)
		(_keys[VK_MENU] = 0x08);

	// Update print screen state (there is no key down message, but the key up one is received via the message queue)
	if ((_keys[VK_SNAPSHOT] & 0x80) == 0 &&
		(GetAsyncKeyState_trampoline(VK_SNAPSHOT) & 0x8000) != 0)
		(_keys[VK_SNAPSHOT] = 0x88),
		(_keys_time[VK_SNAPSHOT] = time);

	// Run through all forms of input blocking for all windows and establish whether any of them are blocking input
	const std::shared_lock<std::shared_mutex> lock(s_windows_mutex);

	s_block_mouse.store(std::any_of(s_windows.cbegin(), s_windows.cend(),
		[](const std::pair<HWND, std::weak_ptr<reshade::input>> &input_window) {
			return !input_window.second.expired() && input_window.second.lock()->is_blocking_mouse_input();
		}));
	s_block_keyboard.store(std::any_of(s_windows.cbegin(), s_windows.cend(),
		[](const std::pair<HWND, std::weak_ptr<reshade::input>> &input_window) {
			return !input_window.second.expired() && input_window.second.lock()->is_blocking_keyboard_input();
		}));
	s_block_cursor_warping.store(std::any_of(s_windows.cbegin(), s_windows.cend(),
		[](const std::pair<HWND, std::weak_ptr<reshade::input>> &input_window) {
			return !input_window.second.expired() && input_window.second.lock()->is_blocking_mouse_cursor_warping();
		}));
}

std::string reshade::input::key_name(unsigned int keycode)
{
	if (keycode >= 256)
		return std::string();

	if (keycode == VK_HOME &&
		LOBYTE(GetKeyboardLayout(0)) == LANG_GERMAN)
		return "Pos1";

	static const char *keyboard_keys[256] = {
		"", "Left Mouse", "Right Mouse", "Cancel", "Middle Mouse", "X1 Mouse", "X2 Mouse", "", "Backspace", "Tab", "", "", "Clear", "Enter", "", "",
		"Shift", "Control", "Alt", "Pause", "Caps Lock", "", "", "", "", "", "", "Escape", "", "", "", "",
		"Space", "Page Up", "Page Down", "End", "Home", "Left Arrow", "Up Arrow", "Right Arrow", "Down Arrow", "Select", "", "", "Print Screen", "Insert", "Delete", "Help",
		"0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "", "", "", "", "", "",
		"", "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M", "N", "O",
		"P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z", "Left Windows", "Right Windows", "Apps", "", "Sleep",
		"Numpad 0", "Numpad 1", "Numpad 2", "Numpad 3", "Numpad 4", "Numpad 5", "Numpad 6", "Numpad 7", "Numpad 8", "Numpad 9", "Numpad *", "Numpad +", "", "Numpad -", "Numpad Decimal", "Numpad /",
		"F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9", "F10", "F11", "F12", "F13", "F14", "F15", "F16",
		"F17", "F18", "F19", "F20", "F21", "F22", "F23", "F24", "", "", "", "", "", "", "", "",
		"Num Lock", "Scroll Lock", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
		"Left Shift", "Right Shift", "Left Control", "Right Control", "Left Menu", "Right Menu", "Browser Back", "Browser Forward", "Browser Refresh", "Browser Stop", "Browser Search", "Browser Favorites", "Browser Home", "Volume Mute", "Volume Down", "Volume Up",
		"Next Track", "Previous Track", "Media Stop", "Media Play/Pause", "Mail", "Media Select", "Launch App 1", "Launch App 2", "", "", "OEM ;", "OEM +", "OEM ,", "OEM -", "OEM .", "OEM /",
		"OEM ~", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
		"", "", "", "", "", "", "", "", "", "", "", "OEM [", "OEM \\", "OEM ]", "OEM '", "OEM 8",
		"", "", "OEM <", "", "", "", "", "", "", "", "", "", "", "", "", "",
		"", "", "", "", "", "", "Attn", "CrSel", "ExSel", "Erase EOF", "Play", "Zoom", "", "PA1", "OEM Clear", ""
	};

	return keyboard_keys[keycode];
}
std::string reshade::input::key_name(const unsigned int key[4])
{
	assert(key[0] != VK_CONTROL && key[0] != VK_SHIFT && key[0] != VK_MENU);

	return (key[1] ? "Ctrl + " : std::string()) + (key[2] ? "Shift + " : std::string()) + (key[3] ? "Alt + " : std::string()) + key_name(key[0]);
}

void reshade::input::block_mouse_input(bool enable)
{
	_block_mouse = enable;
}
void reshade::input::block_keyboard_input(bool enable)
{
	_block_keyboard = enable;
}
void reshade::input::block_mouse_cursor_warping(bool enable)
{
	static const auto ClipCursor_trampoline = reshade::hooks::is_hooked(ClipCursor) ? reshade::hooks::call(HookClipCursor, ClipCursor) : ClipCursor;

	// Some games setup ClipCursor with a tiny area which could make the cursor stay in that area instead of the whole window
	if (enable)
	{
		ClipCursor_trampoline(nullptr);
	}
	else if (_block_cursor_warping && (s_last_clip_cursor.right - s_last_clip_cursor.left) != 0 && (s_last_clip_cursor.bottom - s_last_clip_cursor.top) != 0)
	{
		// Restore previous clipping rectangle when not blocking mouse input
		ClipCursor_trampoline(&s_last_clip_cursor);
	}

	_block_cursor_warping = enable;
}

bool reshade::input::is_blocking_any_mouse_input(window_handle target)
{
	if (target != nullptr)
	{
		const std::shared_lock<std::shared_mutex> lock(s_windows_mutex);

		return std::any_of(s_windows.cbegin(), s_windows.cend(),
			[target](const std::pair<HWND, std::weak_ptr<input>> &input_window) {
				return !input_window.second.expired() && input_window.first == target && input_window.second.lock()->is_blocking_mouse_input();
			});
	}

	return s_block_mouse.load();
}
bool reshade::input::is_blocking_any_keyboard_input(window_handle target)
{
	if (target != nullptr)
	{
		const std::shared_lock<std::shared_mutex> lock(s_windows_mutex);

		return std::any_of(s_windows.cbegin(), s_windows.cend(),
			[target](const std::pair<HWND, std::weak_ptr<input>> &input_window) {
				return !input_window.second.expired() && input_window.first == target && input_window.second.lock()->is_blocking_keyboard_input();
			});
	}

	return s_block_keyboard.load();
}
bool reshade::input::is_blocking_any_mouse_cursor_warping()
{
	return s_block_cursor_warping.load();
}

extern "C" BOOL WINAPI HookGetMessageA(LPMSG lpMsg, HWND hWnd, UINT wMsgFilterMin, UINT wMsgFilterMax)
{
#ifndef RESHADE_TEST_APPLICATION
	DWORD mask = QS_ALLINPUT;
	if (wMsgFilterMin != 0 || wMsgFilterMax != 0)
	{
		mask = QS_POSTMESSAGE | QS_SENDMESSAGE;
		if (wMsgFilterMin <= WM_KEYLAST && wMsgFilterMax >= WM_KEYFIRST)
			mask |= QS_KEY;
		if ((wMsgFilterMin <= WM_MOUSELAST &&
			 wMsgFilterMax >= WM_MOUSEFIRST) ||
			(wMsgFilterMin <= WM_MOUSELAST + WM_NCMOUSEMOVE - WM_MOUSEMOVE &&
			 wMsgFilterMax >= WM_MOUSEFIRST + WM_NCMOUSEMOVE - WM_MOUSEMOVE))
			mask |= QS_MOUSE;
		if (wMsgFilterMin <= WM_TIMER && wMsgFilterMax >= WM_TIMER)
			mask |= QS_TIMER;
		if (wMsgFilterMin <= WM_PAINT && wMsgFilterMax >= WM_PAINT)
			mask |= QS_PAINT;
		if (wMsgFilterMin <= WM_HOTKEY && wMsgFilterMax >= WM_HOTKEY)
			mask |= QS_HOTKEY;
		if (wMsgFilterMin <= WM_INPUT && wMsgFilterMax >= WM_INPUT)
			mask |= QS_RAWINPUT;
	}

	// Implement 'GetMessage' with an additional trigger event (see also DLL_PROCESS_DETACH in dllmain.cpp for more explanation)
	while (!PeekMessageA(lpMsg, hWnd, wMsgFilterMin, wMsgFilterMax, PM_REMOVE))
	{
		assert(g_exit_event != nullptr);

		if (MsgWaitForMultipleObjects(1, &g_exit_event, FALSE, INFINITE, mask) != (WAIT_OBJECT_0 + 1))
		{
			// Clear message structure, so application does not process it
			std::memset(lpMsg, 0, sizeof(MSG));
			// Do not return an error (-1), since this causes WPF to throw an exception
			return 1;
		}
	}
#else
	static const auto trampoline = reshade::hooks::call(HookGetMessageA);
	const BOOL result = trampoline(lpMsg, hWnd, wMsgFilterMin, wMsgFilterMax);
	if (result < 0) // If there is an error, the return value is negative (https://docs.microsoft.com/windows/win32/api/winuser/nf-winuser-getmessage)
		return result;

	assert(lpMsg != nullptr);

	if (lpMsg->hwnd != nullptr && reshade::input::handle_window_message(lpMsg))
	{
		// We still want 'WM_CHAR' messages, so translate message
		TranslateMessage(lpMsg);

		// Change message so it is ignored by the recipient window
		lpMsg->message = WM_NULL;
	}
#endif

	return lpMsg->message != WM_QUIT;
}
extern "C" BOOL WINAPI HookGetMessageW(LPMSG lpMsg, HWND hWnd, UINT wMsgFilterMin, UINT wMsgFilterMax)
{
#ifndef RESHADE_TEST_APPLICATION
	DWORD mask = QS_ALLINPUT;
	if (wMsgFilterMin != 0 || wMsgFilterMax != 0)
	{
		mask = QS_POSTMESSAGE | QS_SENDMESSAGE;
		if (wMsgFilterMin <= WM_KEYLAST && wMsgFilterMax >= WM_KEYFIRST)
			mask |= QS_KEY;
		if ((wMsgFilterMin <= WM_MOUSELAST &&
			 wMsgFilterMax >= WM_MOUSEFIRST) ||
			(wMsgFilterMin <= WM_MOUSELAST + WM_NCMOUSEMOVE - WM_MOUSEMOVE &&
			 wMsgFilterMax >= WM_MOUSEFIRST + WM_NCMOUSEMOVE - WM_MOUSEMOVE))
			mask |= QS_MOUSE;
		if (wMsgFilterMin <= WM_TIMER && wMsgFilterMax >= WM_TIMER)
			mask |= QS_TIMER;
		if (wMsgFilterMin <= WM_PAINT && wMsgFilterMax >= WM_PAINT)
			mask |= QS_PAINT;
		if (wMsgFilterMin <= WM_HOTKEY && wMsgFilterMax >= WM_HOTKEY)
			mask |= QS_HOTKEY;
		if (wMsgFilterMin <= WM_INPUT && wMsgFilterMax >= WM_INPUT)
			mask |= QS_RAWINPUT;
	}

	while (!PeekMessageW(lpMsg, hWnd, wMsgFilterMin, wMsgFilterMax, PM_REMOVE))
	{
		assert(g_exit_event != nullptr);

		if (MsgWaitForMultipleObjects(1, &g_exit_event, FALSE, INFINITE, mask) != (WAIT_OBJECT_0 + 1))
		{
			std::memset(lpMsg, 0, sizeof(MSG));
			return 1;
		}
	}
#else
	static const auto trampoline = reshade::hooks::call(HookGetMessageW);
	const BOOL result = trampoline(lpMsg, hWnd, wMsgFilterMin, wMsgFilterMax);
	if (result < 0)
		return result;

	assert(lpMsg != nullptr);

	if (lpMsg->hwnd != nullptr && reshade::input::handle_window_message(lpMsg))
	{
		// We still want 'WM_CHAR' messages, so translate message
		TranslateMessage(lpMsg);

		// Change message so it is ignored by the recipient window
		lpMsg->message = WM_NULL;
	}
#endif

	return lpMsg->message != WM_QUIT;
}
extern "C" BOOL WINAPI HookPeekMessageA(LPMSG lpMsg, HWND hWnd, UINT wMsgFilterMin, UINT wMsgFilterMax, UINT wRemoveMsg)
{
	static const auto trampoline = reshade::hooks::call(HookPeekMessageA);
	if (!trampoline(lpMsg, hWnd, wMsgFilterMin, wMsgFilterMax, wRemoveMsg) || lpMsg == nullptr)
		return FALSE;

	if (lpMsg->hwnd != nullptr && (wRemoveMsg & PM_REMOVE) != 0 && reshade::input::handle_window_message(lpMsg))
	{
		// We still want 'WM_CHAR' messages, so translate message
		TranslateMessage(lpMsg);

		// Change message so it is ignored by the recipient window
		lpMsg->message = WM_NULL;
	}

	return TRUE;
}
extern "C" BOOL WINAPI HookPeekMessageW(LPMSG lpMsg, HWND hWnd, UINT wMsgFilterMin, UINT wMsgFilterMax, UINT wRemoveMsg)
{
	static const auto trampoline = reshade::hooks::call(HookPeekMessageW);
	if (!trampoline(lpMsg, hWnd, wMsgFilterMin, wMsgFilterMax, wRemoveMsg) || lpMsg == nullptr)
		return FALSE;

	if (lpMsg->hwnd != nullptr && (wRemoveMsg & PM_REMOVE) != 0 && reshade::input::handle_window_message(lpMsg))
	{
		// We still want 'WM_CHAR' messages, so translate message
		TranslateMessage(lpMsg);

		// Change message so it is ignored by the recipient window
		lpMsg->message = WM_NULL;
	}

	return TRUE;
}

extern "C" BOOL WINAPI HookPostMessageA(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	// Do not allow mouse movement simulation while we block input
	if (reshade::input::is_blocking_any_mouse_input(hWnd) && Msg == WM_MOUSEMOVE)
		return TRUE;

	static const auto trampoline = reshade::hooks::call(HookPostMessageA);
	return trampoline(hWnd, Msg, wParam, lParam);
}
extern "C" BOOL WINAPI HookPostMessageW(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	if (reshade::input::is_blocking_any_mouse_input(hWnd) && Msg == WM_MOUSEMOVE)
		return TRUE;

	static const auto trampoline = reshade::hooks::call(HookPostMessageW);
	return trampoline(hWnd, Msg, wParam, lParam);
}

extern "C" BOOL WINAPI HookRegisterRawInputDevices(PCRAWINPUTDEVICE pRawInputDevices, UINT uiNumDevices, UINT cbSize)
{
#if RESHADE_VERBOSE_LOG
	reshade::log::message(
		reshade::log::level::debug,
		"Redirecting RegisterRawInputDevices(pRawInputDevices = %p, uiNumDevices = %u, cbSize = %u) ...",
		pRawInputDevices, uiNumDevices, cbSize);
#endif

	for (UINT i = 0; i < uiNumDevices; ++i)
	{
		const RAWINPUTDEVICE &device = pRawInputDevices[i];

#if RESHADE_VERBOSE_LOG
		reshade::log::message(reshade::log::level::debug, "> Dumping device registration at index %u:", i);
		reshade::log::message(reshade::log::level::debug, "  +-----------------------------------------+-----------------------------------------+");
		reshade::log::message(reshade::log::level::debug, "  | Parameter                               | Value                                   |");
		reshade::log::message(reshade::log::level::debug, "  +-----------------------------------------+-----------------------------------------+");
		reshade::log::message(reshade::log::level::debug, "  | UsagePage                               | %-39hu |", device.usUsagePage);
		reshade::log::message(reshade::log::level::debug, "  | Usage                                   | %-39hu |", device.usUsage);
		reshade::log::message(reshade::log::level::debug, "  | Flags                                   | %-39lu |", device.dwFlags);
		reshade::log::message(reshade::log::level::debug, "  | TargetWindow                            | %-39p |", device.hwndTarget);
		reshade::log::message(reshade::log::level::debug, "  +-----------------------------------------+-----------------------------------------+");
#endif

		if (device.usUsagePage != 1 || device.hwndTarget == nullptr)
			continue;

		reshade::input::register_window_with_raw_input(device.hwndTarget, device.usUsage == 0x06 && (device.dwFlags & RIDEV_NOLEGACY) != 0, device.usUsage == 0x02 && (device.dwFlags & RIDEV_NOLEGACY) != 0);
	}

	static const auto trampoline = reshade::hooks::call(HookRegisterRawInputDevices);
	if (!trampoline(pRawInputDevices, uiNumDevices, cbSize))
	{
		reshade::log::message(reshade::log::level::warning, "RegisterRawInputDevices failed with error code %lu.", GetLastError());
		return FALSE;
	}

	return TRUE;
}

extern "C" BOOL WINAPI HookClipCursor(const RECT *lpRect)
{
	s_last_clip_cursor = (lpRect != nullptr) ? *lpRect : RECT {};

	if (reshade::input::is_blocking_any_mouse_input() || reshade::input::is_blocking_any_mouse_cursor_warping())
		// Some applications clip the mouse cursor, so disable that while we want full control over mouse input
		lpRect = nullptr;

	static const auto trampoline = reshade::hooks::call(HookClipCursor);
	return trampoline(lpRect);
}

extern "C" BOOL WINAPI HookSetCursorPosition(int X, int Y)
{
	s_last_cursor_position.x = X;
	s_last_cursor_position.y = Y;

	if (reshade::input::is_blocking_any_mouse_cursor_warping())
		return TRUE;

	static const auto trampoline = reshade::hooks::call(HookSetCursorPosition);
	return trampoline(X, Y);
}
extern "C" BOOL WINAPI HookGetCursorPosition(LPPOINT lpPoint)
{
	if (reshade::input::is_blocking_any_mouse_input())
	{
		assert(lpPoint != nullptr);

		*lpPoint = s_last_cursor_position;

		return TRUE;
	}

	static const auto trampoline = reshade::hooks::call(HookGetCursorPosition);
	const BOOL result = trampoline(lpPoint);
	if (result)
	{
		assert(lpPoint != nullptr);

		// Update position in case it is not overriden via 'SetCursorPos'
		s_last_cursor_position = *lpPoint;
	}

	return result;
}

extern "C" auto WINAPI HookGetKeyState(int vKey) -> SHORT
{
	// Valid keyboard keys are between 8 and 255
	if ((vKey & 0xF8) != 0)
	{
		if (reshade::input::is_blocking_any_keyboard_input())
			return 0;
	}
	else if (vKey >= VK_LBUTTON && vKey <= VK_XBUTTON2)
	{
		// Some games use this API for mouse buttons
		if (reshade::input::is_blocking_any_mouse_input())
			return 0;
	}

	static const auto trampoline = reshade::hooks::call(HookGetKeyState);
	return trampoline(vKey);
}
extern "C" auto WINAPI HookGetAsyncKeyState(int vKey) -> SHORT
{
	// Valid keyboard keys are between 8 and 255
	if ((vKey & 0xF8) != 0)
	{
		if (reshade::input::is_blocking_any_keyboard_input())
			return 0;
	}
	else if (vKey >= VK_LBUTTON && vKey <= VK_XBUTTON2)
	{
		// Some games use this API for mouse buttons
		if (reshade::input::is_blocking_any_mouse_input())
			return 0;
	}

	static const auto trampoline = reshade::hooks::call(HookGetAsyncKeyState);
	return trampoline(vKey);
}
extern "C" BOOL WINAPI HookGetKeyboardState(PBYTE lpKeyState)
{
	static const auto trampoline = reshade::hooks::call(HookGetKeyboardState);
	const BOOL result = trampoline(lpKeyState);
	if (result)
	{
		if (reshade::input::is_blocking_any_mouse_input())
			std::memset(lpKeyState, 0, 7);
		if (reshade::input::is_blocking_any_keyboard_input())
			std::memset(lpKeyState + 7, 0, 256 - 7);
	}

	return result;
}

// This variant of raw input does not use 'WM_INPUT' messages, so a hook is necessary to block these inputs
extern "C" UINT WINAPI HookGetRawInputBuffer(PRAWINPUT pData, PUINT pcbSize, UINT cbSizeHeader)
{
	static const auto trampoline = reshade::hooks::call(HookGetRawInputBuffer);
	const UINT result = trampoline(pData, pcbSize, cbSizeHeader);
	// This is a high throughput API (i.e. 8 kHz mouse polling), so need a fast path to exit
	if (result < 0 || pData == nullptr || *pcbSize == 0 || !(reshade::input::is_blocking_any_mouse_input() || reshade::input::is_blocking_any_keyboard_input()))
		return result;

	using QWORD = UINT64;

	for (UINT i = 0; i < result; ++i, pData = NEXTRAWINPUTBLOCK(pData))
	{
		switch (pData->header.dwType)
		{
		case RIM_TYPEMOUSE:
			if (reshade::input::is_blocking_any_mouse_input())
			{
				pData->header.hDevice = nullptr;
				pData->header.wParam = RIM_INPUTSINK;

				// Zero mouse data, to ensure ignore input is ignored even if 'RIM_INPUTSINK' is not respected
				std::memset(&pData->data.mouse, 0, pData->header.dwSize - sizeof(RAWINPUTHEADER));
			}
			break;
		case RIM_TYPEKEYBOARD:
			if (reshade::input::is_blocking_any_keyboard_input())
			{
				// Supplying an invalid device will early-out SDL before it calls HID APIs to try and get an input report that we don't want it to see
				pData->header.hDevice = nullptr;
				// Most engines will honor the input sink state
				pData->header.wParam = RIM_INPUTSINK;

				if (0 == (pData->data.keyboard.Flags & RI_KEY_BREAK))
				{
					// Fake a key up message
					pData->data.keyboard.Flags |= RI_KEY_BREAK;
					pData->data.keyboard.VKey = 0;
				}
			}
			break;
		}
	}

	return result;
}

#include <concurrent_unordered_map.h>

static concurrency::concurrent_unordered_map<UINT64, std::pair<HHOOK, HOOKPROC>> s_windows_hooks;

template <DWORD hook_id, bool(*is_blocking_input)(reshade::input::window_handle)>
static LRESULT CALLBACK handle_windows_hook(int nCode, WPARAM wParam, LPARAM lParam)
{
	if ((nCode == HC_ACTION || nCode == HC_NOREMOVE) && !is_blocking_input(nullptr))
	{
		const UINT64 global_lookup_key = (static_cast<UINT64>(hook_id) << 32);
		const UINT64 thread_lookup_key = GetCurrentThreadId() | global_lookup_key;

		// Look up original hook function for the current thread and hook type
		if (const auto it = s_windows_hooks.find(thread_lookup_key);
			it != s_windows_hooks.end())
		{
			const HOOKPROC orig_hook_proc = it->second.second;
			// A non-zero result value prevents the system from passing the message to the target window procedure
			// In that case our 'GetMessage' and 'PeekMessage' hooks will not receive them, so ignore that result and continue with the hook chain instead below
			if (orig_hook_proc(nCode, wParam, lParam) == 0)
				return 0;
		}

		// Alternatively look up global hook function for this hook type (registered for thread ID zero)
		if (const auto it = s_windows_hooks.find(global_lookup_key);
			it != s_windows_hooks.end())
		{
			const HOOKPROC orig_hook_proc = it->second.second;
			if (orig_hook_proc(nCode, wParam, lParam) == 0)
				return 0;
		}
	}

	// Bypass the original hook function and continue with the hook chain
	return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

extern "C" HHOOK WINAPI HookSetWindowsHookExA(int idHook, HOOKPROC lpfn, HINSTANCE hmod, DWORD dwThreadId)
{
#if RESHADE_VERBOSE_LOG
	reshade::log::message(reshade::log::level::info, "Redirecting SetWindowsHookExA(idHook = %d, lpfn = %p, hmod = %p, dwThreadId = %lu) ...", idHook, lpfn, hmod, dwThreadId);
#endif

	HOOKPROC orig_hook_proc = lpfn;
	const UINT64 thread_lookup_key = dwThreadId | (static_cast<UINT64>(idHook) << 32);

	if (const auto it = s_windows_hooks.find(thread_lookup_key);
		it == s_windows_hooks.end() || it->second.first == nullptr)
	{
		switch (idHook)
		{
		case WH_MOUSE:
			lpfn = &handle_windows_hook<WH_MOUSE, reshade::input::is_blocking_any_mouse_input>;
			break;
		case WH_MOUSE_LL:
			lpfn = &handle_windows_hook<WH_MOUSE_LL, reshade::input::is_blocking_any_mouse_input>;
			break;
		case WH_KEYBOARD:
			lpfn = &handle_windows_hook<WH_KEYBOARD, reshade::input::is_blocking_any_keyboard_input>;
			break;
		case WH_KEYBOARD_LL:
			lpfn = &handle_windows_hook<WH_KEYBOARD_LL, reshade::input::is_blocking_any_keyboard_input>;
			break;
		}
	}
	else
	{
#if RESHADE_VERBOSE_LOG
		reshade::log::message(reshade::log::level::warning, "Windows hook function of type %d was already registered for thread %lu.", idHook, dwThreadId);
#endif
	}

	static const auto trampoline = reshade::hooks::call(HookSetWindowsHookExA);
	const HHOOK result = trampoline(idHook, lpfn, hmod, dwThreadId);

	if (result != nullptr && lpfn != orig_hook_proc)
	{
#if RESHADE_VERBOSE_LOG
		reshade::log::message(reshade::log::level::info, "Windows hook function of type %d registered for thread %lu.", idHook, dwThreadId);
#endif

		s_windows_hooks[thread_lookup_key] = std::make_pair(result, orig_hook_proc);
	}

	return result;
}
extern "C" HHOOK WINAPI HookSetWindowsHookExW(int idHook, HOOKPROC lpfn, HINSTANCE hmod, DWORD dwThreadId)
{
#if RESHADE_VERBOSE_LOG
	reshade::log::message(reshade::log::level::info, "Redirecting SetWindowsHookExW(idHook = %d, lpfn = %p, hmod = %p, dwThreadId = %lu) ...", idHook, lpfn, hmod, dwThreadId);
#endif

	HOOKPROC orig_hook_proc = lpfn;
	const UINT64 thread_lookup_key = dwThreadId | (static_cast<UINT64>(idHook) << 32);

	if (const auto it = s_windows_hooks.find(thread_lookup_key);
		it == s_windows_hooks.end() || it->second.first == nullptr)
	{
		switch (idHook)
		{
		case WH_MOUSE:
			lpfn = &handle_windows_hook<WH_MOUSE, reshade::input::is_blocking_any_mouse_input>;
			break;
		case WH_MOUSE_LL:
			lpfn = &handle_windows_hook<WH_MOUSE_LL, reshade::input::is_blocking_any_mouse_input>;
			break;
		case WH_KEYBOARD:
			lpfn = &handle_windows_hook<WH_KEYBOARD, reshade::input::is_blocking_any_keyboard_input>;
			break;
		case WH_KEYBOARD_LL:
			lpfn = &handle_windows_hook<WH_KEYBOARD_LL, reshade::input::is_blocking_any_keyboard_input>;
			break;
		}
	}
	else
	{
#if RESHADE_VERBOSE_LOG
		reshade::log::message(reshade::log::level::warning, "Windows hook function of type %d was already registered for thread %lu.", idHook, dwThreadId);
#endif
	}

	static const auto trampoline = reshade::hooks::call(HookSetWindowsHookExW);
	const HHOOK result = trampoline(idHook, lpfn, hmod, dwThreadId);

	if (result != nullptr && lpfn != orig_hook_proc)
	{
#if RESHADE_VERBOSE_LOG
		reshade::log::message(reshade::log::level::info, "Windows hook function of type %d registered for thread %lu.", idHook, dwThreadId);
#endif

		s_windows_hooks[thread_lookup_key] = std::make_pair(result, orig_hook_proc);
	}

	return result;
}
extern "C" BOOL  WINAPI HookUnhookWindowsHookEx(HHOOK hhk)
{
#if RESHADE_VERBOSE_LOG
	reshade::log::message(reshade::log::level::info, "Redirecting UnhookWindowsHookEx(hhk = %p) ...", hhk);
#endif

	for (const std::pair<UINT64, std::pair<HHOOK, HOOKPROC>> &hook : s_windows_hooks)
	{
		if (hook.second.first == hhk)
		{
#if RESHADE_VERBOSE_LOG
			reshade::log::message(reshade::log::level::info, "Windows hook function of type %d unregistered for thread %lu.", static_cast<int>(hook.first >> 32), static_cast<DWORD>(hook.first & 0xFFFFFFFF));
#endif

			s_windows_hooks[hook.first] = std::make_pair(nullptr, nullptr);
			break;
		}
	}

	static const auto trampoline = reshade::hooks::call(HookUnhookWindowsHookEx);
	return trampoline(hhk);
}
