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
static std::chrono::high_resolution_clock::time_point s_last_point;

static void on_present(reshade::api::command_queue *, reshade::api::swapchain *, const reshade::api::rect *, const reshade::api::rect *, uint32_t, const reshade::api::rect *)
{
	if (!s_enabled || s_fps_limit == 0)
		return;

	if (!s_started)
	{
		s_started = true;
		s_last_point = std::chrono::high_resolution_clock::now();
	}

	const auto time_per_frame = std::chrono::high_resolution_clock::duration(std::chrono::seconds(1)) / s_fps_limit;
	const auto next_point = s_last_point + time_per_frame;

	while (next_point > std::chrono::high_resolution_clock::now())
		std::this_thread::sleep_for(std::chrono::high_resolution_clock::duration(std::chrono::milliseconds(1)));

	s_last_point = next_point;
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
		reshade::register_event<reshade::addon_event::present>(on_present);
		reshade::register_overlay(nullptr, draw_settings);
		break;
	case DLL_PROCESS_DETACH:
		reshade::unregister_addon(hModule);
		break;
	}

	return TRUE;
}
