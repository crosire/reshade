/*
 * Copyright (C) 2022 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause OR MIT
 */

#include <imgui.h>
#include <reshade.hpp>
#include <chrono>
#include <thread>

static bool s_enabled = false;
static uint32_t s_fps_limit = 0;
static bool s_started = false;
static uint64_t s_num_frames = 0;
static std::chrono::high_resolution_clock::time_point s_start_point;

static void on_present(reshade::api::command_queue *, reshade::api::swapchain *, const reshade::api::rect *, const reshade::api::rect *, uint32_t, const reshade::api::rect *)
{
	if (!s_enabled || s_fps_limit == 0)
		return;

	if (!s_started)
	{
		s_started = true;
		s_num_frames = 0;
		s_start_point = std::chrono::high_resolution_clock::now();
	}

	const auto time_per_frame = std::chrono::high_resolution_clock::duration(std::chrono::seconds(1)) / s_fps_limit;

	const auto cur_frame_point = s_start_point + s_num_frames++ * time_per_frame;
	const auto next_frame_point = cur_frame_point + time_per_frame;

	while (true)
	{
		const auto now = std::chrono::high_resolution_clock::now();
		if (now < cur_frame_point || now > next_frame_point)
		{
			s_started = false;
			break;
		}

		const auto time_remaining = (next_frame_point - now);
		if (time_remaining <= std::chrono::milliseconds(1))
		{
			break;
		}

		std::this_thread::sleep_for(time_remaining - std::chrono::milliseconds(1));
	}
}

static void draw_settings(reshade::api::effect_runtime *)
{
	if (ImGui::Checkbox("Enabled", &s_enabled))
		s_started = false;

	if (ImGui::DragInt("Target FPS", reinterpret_cast<int *>(&s_fps_limit), 1, 0, 200))
		s_enabled = s_started = false;
}

extern "C" __declspec(dllexport) const char *NAME = "Framerate Limiter";
extern "C" __declspec(dllexport) const char *DESCRIPTION = "Example add-on that limits the framerate of an application to a specified FPS value.";

BOOL APIENTRY DllMain(HMODULE hModule, DWORD fdwReason, LPVOID)
{
	switch (fdwReason)
	{
	case DLL_PROCESS_ATTACH:
		if (!reshade::register_addon(hModule))
			return FALSE;
		reshade::register_overlay(nullptr, draw_settings);
		reshade::register_event<reshade::addon_event::present>(on_present);
		break;
	case DLL_PROCESS_DETACH:
		reshade::unregister_addon(hModule);
		break;
	}

	return TRUE;
}
