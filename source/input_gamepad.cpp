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

reshade::input_gamepad::input_gamepad(void *module) : _xinput_module(module)
{
	_xinput_get_state = reinterpret_cast<void *>(GetProcAddress(static_cast<HMODULE>(_xinput_module), "XInputGetState"));
	assert(_xinput_get_state != nullptr);
}
reshade::input_gamepad::~input_gamepad()
{
	FreeLibrary(static_cast<HMODULE>(_xinput_module));
}

std::shared_ptr<reshade::input_gamepad> reshade::input_gamepad::load()
{
	const std::unique_lock<std::shared_mutex> lock(s_xinput_mutex);

	if (!s_xinput_instance.expired())
		return s_xinput_instance.lock();

	const HMODULE module = LoadLibrary(XINPUT_DLL);
	if (module == nullptr)
		return nullptr;

	const auto instance = std::make_shared<input_gamepad>(module);
	s_xinput_instance = instance;

	return instance;
}

bool reshade::input_gamepad::get_state(unsigned int index, state &state)
{
	XINPUT_STATE xstate;
	if (static_cast<decltype(&XInputGetState)>(_xinput_get_state)(index, &xstate) == ERROR_SUCCESS)
	{
		state.a = (xstate.Gamepad.wButtons & XINPUT_GAMEPAD_A) != 0;
		state.b = (xstate.Gamepad.wButtons & XINPUT_GAMEPAD_B) != 0;
		state.x = (xstate.Gamepad.wButtons & XINPUT_GAMEPAD_X) != 0;
		state.y = (xstate.Gamepad.wButtons & XINPUT_GAMEPAD_Y) != 0;
		state.dpad_up = (xstate.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_UP) != 0;
		state.dpad_down = (xstate.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN) != 0;
		state.dpad_left = (xstate.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT) != 0;
		state.dpad_right = (xstate.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT) != 0;
		state.start = (xstate.Gamepad.wButtons & XINPUT_GAMEPAD_START) != 0;
		state.back = (xstate.Gamepad.wButtons & XINPUT_GAMEPAD_BACK) != 0;
		state.left_thumb = (xstate.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_THUMB) != 0;
		state.right_thumb = (xstate.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_THUMB) != 0;
		state.left_shoulder = (xstate.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER) != 0;
		state.right_shoulder = (xstate.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER) != 0;

		state.left_thumb_axis_x =
			(xstate.Gamepad.sThumbLX < -XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE) ? xstate.Gamepad.sThumbLX / 32768.0f :
			(xstate.Gamepad.sThumbLX > +XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE) ? xstate.Gamepad.sThumbLX / 32767.0f : 0.0f;
		state.left_thumb_axis_y =
			(xstate.Gamepad.sThumbLY < -XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE) ? xstate.Gamepad.sThumbLY / 32768.0f :
			(xstate.Gamepad.sThumbLY > +XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE) ? xstate.Gamepad.sThumbLY / 32767.0f : 0.0f;
		state.right_thumb_axis_x =
			(xstate.Gamepad.sThumbLX < -XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE) ? xstate.Gamepad.sThumbLX / 32768.0f :
			(xstate.Gamepad.sThumbLX > +XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE) ? xstate.Gamepad.sThumbLX / 32767.0f : 0.0f;
		state.right_thumb_axis_y =
			(xstate.Gamepad.sThumbLY < -XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE) ? xstate.Gamepad.sThumbLY / 32768.0f :
			(xstate.Gamepad.sThumbLY > +XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE) ? xstate.Gamepad.sThumbLY / 32767.0f : 0.0f;

		state.left_trigger = (xstate.Gamepad.bLeftTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD) ? xstate.Gamepad.bLeftTrigger / 255.0f : 0.0f;
		state.right_trigger = (xstate.Gamepad.bRightTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD) ? xstate.Gamepad.bRightTrigger / 255.0f : 0.0f;

		return true;
	}
	else
	{
		return false;
	}
}
