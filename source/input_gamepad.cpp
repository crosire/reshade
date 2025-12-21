/*
 * Copyright (C) 2014 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "input_gamepad.hpp"
#include <cassert>
#include <shared_mutex>
#include <Windows.h>
#include <Xinput.h>

static std::shared_mutex s_xinput_mutex;
static std::weak_ptr<reshade::input_gamepad> s_xinput_instance;

reshade::input_gamepad::input_gamepad()
{
}

std::shared_ptr<reshade::input_gamepad> reshade::input_gamepad::load()
{
	const std::unique_lock<std::shared_mutex> lock(s_xinput_mutex);

	if (!s_xinput_instance.expired())
		return s_xinput_instance.lock();

	const auto instance = std::make_shared<input_gamepad>();
	s_xinput_instance = instance;

	return instance;
}

bool reshade::input_gamepad::is_button_down(unsigned int button) const
{
	static_assert(
		input_gamepad::button_dpad_up == XINPUT_GAMEPAD_DPAD_UP ||
		input_gamepad::button_dpad_down == XINPUT_GAMEPAD_DPAD_DOWN ||
		input_gamepad::button_dpad_left == XINPUT_GAMEPAD_DPAD_LEFT ||
		input_gamepad::button_dpad_right == XINPUT_GAMEPAD_DPAD_RIGHT ||
		input_gamepad::button_start == XINPUT_GAMEPAD_START ||
		input_gamepad::button_back == XINPUT_GAMEPAD_BACK ||
		input_gamepad::button_left_thumb == XINPUT_GAMEPAD_LEFT_THUMB ||
		input_gamepad::button_right_thumb == XINPUT_GAMEPAD_RIGHT_THUMB ||
		input_gamepad::button_left_shoulder == XINPUT_GAMEPAD_LEFT_SHOULDER ||
		input_gamepad::button_right_shoulder == XINPUT_GAMEPAD_RIGHT_SHOULDER ||
		input_gamepad::button_a == XINPUT_GAMEPAD_A ||
		input_gamepad::button_b == XINPUT_GAMEPAD_B ||
		input_gamepad::button_x == XINPUT_GAMEPAD_X ||
		input_gamepad::button_y == XINPUT_GAMEPAD_Y);

	assert(
		button == XINPUT_GAMEPAD_DPAD_UP ||
		button == XINPUT_GAMEPAD_DPAD_DOWN ||
		button == XINPUT_GAMEPAD_DPAD_LEFT ||
		button == XINPUT_GAMEPAD_DPAD_RIGHT ||
		button == XINPUT_GAMEPAD_START ||
		button == XINPUT_GAMEPAD_BACK ||
		button == XINPUT_GAMEPAD_LEFT_THUMB ||
		button == XINPUT_GAMEPAD_RIGHT_THUMB ||
		button == XINPUT_GAMEPAD_LEFT_SHOULDER ||
		button == XINPUT_GAMEPAD_RIGHT_SHOULDER ||
		button == XINPUT_GAMEPAD_A ||
		button == XINPUT_GAMEPAD_B ||
		button == XINPUT_GAMEPAD_X ||
		button == XINPUT_GAMEPAD_Y);

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
