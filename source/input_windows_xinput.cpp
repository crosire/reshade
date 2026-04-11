/*
 * Copyright (C) 2014 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "input.hpp"
#include <Windows.h>
#include <Xinput.h>

static_assert(
	reshade::input_gamepad::button_dpad_up == XINPUT_GAMEPAD_DPAD_UP ||
	reshade::input_gamepad::button_dpad_down == XINPUT_GAMEPAD_DPAD_DOWN ||
	reshade::input_gamepad::button_dpad_left == XINPUT_GAMEPAD_DPAD_LEFT ||
	reshade::input_gamepad::button_dpad_right == XINPUT_GAMEPAD_DPAD_RIGHT ||
	reshade::input_gamepad::button_start == XINPUT_GAMEPAD_START ||
	reshade::input_gamepad::button_back == XINPUT_GAMEPAD_BACK ||
	reshade::input_gamepad::button_left_thumb == XINPUT_GAMEPAD_LEFT_THUMB ||
	reshade::input_gamepad::button_right_thumb == XINPUT_GAMEPAD_RIGHT_THUMB ||
	reshade::input_gamepad::button_left_shoulder == XINPUT_GAMEPAD_LEFT_SHOULDER ||
	reshade::input_gamepad::button_right_shoulder == XINPUT_GAMEPAD_RIGHT_SHOULDER ||
	reshade::input_gamepad::button_a == XINPUT_GAMEPAD_A ||
	reshade::input_gamepad::button_b == XINPUT_GAMEPAD_B ||
	reshade::input_gamepad::button_x == XINPUT_GAMEPAD_X ||
	reshade::input_gamepad::button_y == XINPUT_GAMEPAD_Y);

std::shared_ptr<reshade::input_gamepad> reshade::input_gamepad::load()
{
	static std::shared_ptr<reshade::input_gamepad> s_instance = std::make_shared<input_gamepad>();
	return s_instance;
}

void reshade::input_gamepad::next_frame()
{
	XINPUT_STATE xstate;
	if (XInputGetState(0, &xstate))
	{
		_buttons = 0;
		_last_buttons = 0;
		_left_trigger = 0.0f;
		_right_trigger = 0.0f;
		_left_thumb_axis_x = 0.0f;
		_left_thumb_axis_y = 0.0f;
		_right_thumb_axis_x = 0.0f;
		_right_thumb_axis_y = 0.0f;
		_last_packet_num = 0;
		return;
	}

	_last_buttons = _buttons;

	if (xstate.dwPacketNumber == _last_packet_num)
		return; // Nothing changed, so can skip the update

	_buttons = xstate.Gamepad.wButtons;

	_left_trigger = (xstate.Gamepad.bLeftTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD) ? xstate.Gamepad.bLeftTrigger / 255.0f : 0.0f;
	_right_trigger = (xstate.Gamepad.bRightTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD) ? xstate.Gamepad.bRightTrigger / 255.0f : 0.0f;

	_left_thumb_axis_x =
		(xstate.Gamepad.sThumbLX < -XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE) ? xstate.Gamepad.sThumbLX / 32768.0f :
		(xstate.Gamepad.sThumbLX > +XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE) ? xstate.Gamepad.sThumbLX / 32767.0f : 0.0f;
	_left_thumb_axis_y =
		(xstate.Gamepad.sThumbLY < -XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE) ? xstate.Gamepad.sThumbLY / 32768.0f :
		(xstate.Gamepad.sThumbLY > +XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE) ? xstate.Gamepad.sThumbLY / 32767.0f : 0.0f;
	_right_thumb_axis_x =
		(xstate.Gamepad.sThumbRX < -XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE) ? xstate.Gamepad.sThumbRX / 32768.0f :
		(xstate.Gamepad.sThumbRX > +XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE) ? xstate.Gamepad.sThumbRX / 32767.0f : 0.0f;
	_right_thumb_axis_y =
		(xstate.Gamepad.sThumbRY < -XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE) ? xstate.Gamepad.sThumbRY / 32768.0f :
		(xstate.Gamepad.sThumbRY > +XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE) ? xstate.Gamepad.sThumbRY / 32767.0f : 0.0f;

	_last_packet_num = xstate.dwPacketNumber;
}
