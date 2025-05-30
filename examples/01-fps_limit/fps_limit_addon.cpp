/*
 * Copyright (C) 2022 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause OR MIT
 */

#include <imgui.h>
#include <reshade.hpp>
#include <chrono>
#include <thread>

static int s_fps_limit = 0;
static std::chrono::high_resolution_clock::time_point s_last_time_point;

static void on_present(reshade::api::command_queue *, reshade::api::swapchain *, const reshade::api::rect *, const reshade::api::rect *, uint32_t, const reshade::api::rect *)
{
	if (s_fps_limit <= 0)
		return;

	const auto time_per_frame = std::chrono::high_resolution_clock::duration(std::chrono::seconds(1)) / s_fps_limit;
	const auto next_time_point = s_last_time_point + time_per_frame;

	while (next_time_point > std::chrono::high_resolution_clock::now())
		std::this_thread::sleep_for(std::chrono::high_resolution_clock::duration(std::chrono::milliseconds(1)));

	s_last_time_point = next_time_point;
}

static void draw_settings(reshade::api::effect_runtime *)
{
	if (ImGui::DragInt("Target FPS", &s_fps_limit, 1, 0, 200))
		s_last_time_point = std::chrono::high_resolution_clock::now();

	ImGui::SetItemTooltip("Set to zero to disable the FPS limit.");
}

extern "C" __declspec(dllexport) const char *NAME = "FPS Limiter";
extern "C" __declspec(dllexport) const char *DESCRIPTION = "Example add-on that limits the frame rate of an application to a specified FPS value.";

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
