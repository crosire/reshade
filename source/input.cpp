/*
 * Copyright (C) 2014 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "input.hpp"
#include <cassert>

extern bool is_keyboard_layout_german();

reshade::input::input(window_handle window)
	: _window(window)
{
}

bool reshade::input::is_key_down(unsigned int keycode) const
{
	assert(keycode < std::size(_keys));
	return keycode < std::size(_keys) && (_keys[keycode] & 0x80) == 0x80;
}
bool reshade::input::is_key_pressed(unsigned int keycode) const
{
	assert(keycode < std::size(_keys));
	return keycode > 0 && keycode < std::size(_keys) && (_keys[keycode] & 0x88) == 0x88 && !is_key_repeated(keycode);
}
bool reshade::input::is_key_pressed(unsigned int keycode, bool ctrl, bool shift, bool alt, bool force_modifiers) const
{
	if (keycode == 0)
		return false;

	const bool key_down = is_key_pressed(keycode), ctrl_down = is_key_down(key_ctrl), shift_down = is_key_down(key_shift), alt_down = is_key_down(key_alt);
	if (force_modifiers) // Modifier state is required to match
		return key_down && (ctrl == ctrl_down && shift == shift_down && alt == alt_down);
	else // Modifier state is optional and only has to match when down
		return key_down && (!ctrl || ctrl_down) && (!shift || shift_down) && (!alt || alt_down);
}
bool reshade::input::is_key_released(unsigned int keycode) const
{
	assert(keycode < std::size(_keys));
	return keycode > 0 && keycode < std::size(_keys) && (_keys[keycode] & 0x88) == 0x08;
}
bool reshade::input::is_key_repeated(unsigned int keycode) const
{
	assert(keycode < std::size(_keys));
	return keycode < std::size(_keys) && (_last_keys[keycode] & 0x80) == 0x80 && (_keys[keycode] & 0x80) == 0x80;
}

bool reshade::input::is_any_key_down() const
{
	// Skip mouse buttons
	for (unsigned int i = key_button_xbutton2 + 1; i < std::size(_keys); i++)
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
	for (unsigned int i = key_button_middle; i < std::size(_keys); i++)
		if (is_key_pressed(i))
			return i;
	return 0;
}
unsigned int reshade::input::last_key_released() const
{
	for (unsigned int i = key_button_middle; i < std::size(_keys); i++)
		if (is_key_released(i))
			return i;
	return 0;
}

bool reshade::input::is_mouse_button_down(unsigned int button) const
{
	assert(button < 5);
	return is_key_down(key_button_left + button + (button < 2 ? 0 : 1)); // VK_CANCEL is being ignored by runtime
}
bool reshade::input::is_mouse_button_pressed(unsigned int button) const
{
	assert(button < 5);
	return is_key_pressed(key_button_left + button + (button < 2 ? 0 : 1)); // VK_CANCEL is being ignored by runtime
}
bool reshade::input::is_mouse_button_released(unsigned int button) const
{
	assert(button < 5);
	return is_key_released(key_button_left + button + (button < 2 ? 0 : 1)); // VK_CANCEL is being ignored by runtime
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

std::string reshade::input::key_name(unsigned int keycode)
{
	if (keycode >= 256)
		return std::string();

	if (keycode == key_home && is_keyboard_layout_german())
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
	assert(key[0] != key_ctrl && key[0] != key_shift && key[0] != key_alt);

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

reshade::input_gamepad::input_gamepad()
{
}

bool reshade::input_gamepad::is_button_down(unsigned int button) const
{
	assert(
		button == button_dpad_up ||
		button == button_dpad_down ||
		button == button_dpad_left ||
		button == button_dpad_right ||
		button == button_start ||
		button == button_back ||
		button == button_left_thumb ||
		button == button_right_thumb ||
		button == button_left_shoulder ||
		button == button_right_shoulder ||
		button == button_a ||
		button == button_b ||
		button == button_x ||
		button == button_y);

	return (_buttons & button) != 0;
}
bool reshade::input_gamepad::is_button_pressed(unsigned int button) const
{
	return (_buttons & button) != 0 && (_last_buttons & button) == 0;
}
bool reshade::input_gamepad::is_button_released(unsigned int button) const
{
	return (_buttons & button) == 0 && (_last_buttons & button) != 0;
}
