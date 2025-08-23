/*
 * Copyright (C) 2024 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause OR MIT
 */

#include <imgui.h>
#include <reshade.hpp>
#include <chrono>
#include <thread>
#include <string>
#include <windows.h>

static bool s_block_mouse_input = false;
static bool s_block_mouse_input_manual = false; // Manual override state
static bool s_block_keyboard_input = false;
static bool s_block_keyboard_input_manual = false; // Manual override state
static bool s_block_mouse_cursor_warping = false;
static bool s_block_mouse_cursor_warping_manual = false; // Manual override state
static std::chrono::high_resolution_clock::time_point s_last_background_check;
static bool s_auto_disable_in_background = false; // Auto-disable when game is in background
static bool s_game_is_foreground = true; // Current foreground state


std::atomic<HWND> g_last_swapchain_hwnd;

void OnInitSwapchain(reshade::api::swapchain* swapchain, bool resize) {
	HWND hwnd = static_cast<HWND>(swapchain->get_hwnd());
	g_last_swapchain_hwnd.store(hwnd);

}

static void on_reshade_present(reshade::api::effect_runtime *runtime)
{
	const auto now = std::chrono::high_resolution_clock::now();
	
	// Check foreground state every configurable interval

	HWND foreground_window = GetForegroundWindow();
	
	// Check if the game window is the foreground window
	s_game_is_foreground = (foreground_window == g_last_swapchain_hwnd.load());
		
	// Apply auto-disable logic if enabled
	s_block_mouse_input = s_block_mouse_input_manual;
	s_block_keyboard_input = s_block_keyboard_input_manual;
	s_block_mouse_cursor_warping = s_block_mouse_cursor_warping_manual;

	if (s_auto_disable_in_background)
	{
		if (!s_game_is_foreground)
		{
			s_block_mouse_input = true;	
			s_block_keyboard_input = true;
			s_block_mouse_cursor_warping = true;
		}
	}

	runtime->block_mouse_input(s_block_mouse_input);
	runtime->block_keyboard_input(s_block_keyboard_input);
	runtime->block_mouse_cursor_warping(s_block_mouse_cursor_warping);
}

static void draw_settings(reshade::api::effect_runtime *runtime)
{
	ImGui::Text("Input Control Addon");
	ImGui::Text("This addon demonstrates the input blocking API.");
	
	ImGui::Separator();
	ImGui::Text("Current Status:");

	
	ImGui::Text("Mouse Input Blocked: %s", s_block_mouse_input ? "Yes" : "No");
	ImGui::Text("Keyboard Input Blocked: %s", s_block_keyboard_input ? "Yes" : "No");
	ImGui::Text("Mouse Cursor Warping Blocked: %s", s_block_mouse_cursor_warping ? "Yes" : "No");
	ImGui::Text("Game in Foreground: %s", s_game_is_foreground ? "Yes" : "No");
	
	ImGui::Separator();
	ImGui::Text("Background Monitoring:");
	bool auto_disable = s_auto_disable_in_background;
	if (ImGui::Checkbox("Auto-disable input when game is in background", &auto_disable))
	{
		s_auto_disable_in_background = auto_disable;
		
		// If disabling auto-disable, restore manual settings
		if (!s_auto_disable_in_background)
		{
			s_block_mouse_input = s_block_mouse_input_manual;
			s_block_keyboard_input = s_block_keyboard_input_manual;
			s_block_mouse_cursor_warping = s_block_mouse_cursor_warping_manual;
			
			runtime->block_mouse_input(s_block_mouse_input);
			runtime->block_keyboard_input(s_block_keyboard_input);
			runtime->block_mouse_cursor_warping(s_block_mouse_cursor_warping);
		}
	}
	ImGui::SetItemTooltip("Automatically blocks all input when the game window loses focus");
	
	
	if (s_auto_disable_in_background)
	{
		ImGui::TextWrapped("Note: When enabled, input will be automatically blocked when the game is in the background "
			"and restored to your manual settings when the game regains focus.");
	}
	
	ImGui::Separator();
	ImGui::Text("Manual Control:");
	
	bool block_mouse = s_block_mouse_input_manual;
	if (ImGui::Checkbox("Block Mouse Input", &block_mouse))
	{
		s_block_mouse_input_manual = block_mouse;
		if (!s_auto_disable_in_background || s_game_is_foreground)
		{
			s_block_mouse_input = s_block_mouse_input_manual;
			runtime->block_mouse_input(s_block_mouse_input);
		}
	}
	
	bool block_keyboard = s_block_keyboard_input_manual;
	if (ImGui::Checkbox("Block Keyboard Input", &block_keyboard))
	{
		s_block_keyboard_input_manual = block_keyboard;
		if (!s_auto_disable_in_background || s_game_is_foreground)
		{
			s_block_keyboard_input = s_block_keyboard_input_manual;
			runtime->block_keyboard_input(s_block_keyboard_input);
		}
	}
	
	bool block_cursor_warping = s_block_mouse_cursor_warping_manual;
	if (ImGui::Checkbox("Block Mouse Cursor Warping", &block_cursor_warping))
	{
		s_block_mouse_cursor_warping_manual = block_cursor_warping;
		if (!s_auto_disable_in_background || s_game_is_foreground)
		{
			s_block_mouse_cursor_warping = s_block_mouse_cursor_warping_manual;
			runtime->block_mouse_cursor_warping(s_block_mouse_cursor_warping);
		}
	}
	
	ImGui::Separator();
	ImGui::TextWrapped("Note: This API allows you to control input blocking. "
		"Use with caution as it can completely block user input to the application.");
}

extern "C" __declspec(dllexport) const char *NAME = "Input Control";
extern "C" __declspec(dllexport) const char *DESCRIPTION = "Example addon that demonstrates the input blocking API functions with configurable background monitoring capabilities.";

BOOL APIENTRY DllMain(HMODULE hModule, DWORD fdwReason, LPVOID)
{
	switch (fdwReason)
	{
	case DLL_PROCESS_ATTACH:
		if (!reshade::register_addon(hModule))
			return FALSE;
		
		// Initialize manual state variables
		s_block_mouse_input_manual = s_block_mouse_input;
		s_block_keyboard_input_manual = s_block_keyboard_input;
		s_block_mouse_cursor_warping_manual = s_block_mouse_cursor_warping;
		
		// Initialize background check timer
		s_last_background_check = std::chrono::high_resolution_clock::now();
		
		reshade::register_event<reshade::addon_event::reshade_present>(on_reshade_present);
		reshade::register_overlay(nullptr, draw_settings);

		reshade::register_event<reshade::addon_event::init_swapchain>(OnInitSwapchain);
		break;
	case DLL_PROCESS_DETACH:
		reshade::unregister_addon(hModule);
		reshade::unregister_event<reshade::addon_event::init_swapchain>(OnInitSwapchain);
		reshade::unregister_event<reshade::addon_event::reshade_present>(on_reshade_present);
		reshade::unregister_overlay(nullptr, draw_settings);
		break;
	}

	return TRUE;
}
