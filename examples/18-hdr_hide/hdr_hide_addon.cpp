/*
 * Copyright (C) 2024 ReShade Contributors
 * SPDX-License-Identifier: BSD-3-Clause OR MIT
 */

#include <imgui.h>
#include <reshade.hpp>

static bool s_hide_hdr_when_auto_enabled = true;

static bool on_check_color_space_support(reshade::api::effect_runtime *runtime, reshade::api::color_space color_space, bool *supported)
{
	// Only hide HDR color spaces when auto-enabled
	if (s_hide_hdr_when_auto_enabled)
	{
		// Check if this is an HDR color space
		if (color_space == reshade::api::color_space::hdr10_st2084 ||
		    color_space == reshade::api::color_space::hdr10_hlg)
		{
			// Hide HDR support by setting supported to false
			*supported = false;
			return true; // Return true to indicate we've overridden the result
		}
	}

	// For all other color spaces or when hiding is disabled, don't override
	return false;
}

static void draw_settings(reshade::api::effect_runtime *)
{
	ImGui::Checkbox("Hide HDR when auto-enabled", &s_hide_hdr_when_auto_enabled);
	ImGui::SetItemTooltip("When enabled, HDR color spaces (HDR10 ST2084 and HDR10 HLG) will be hidden from overlay color space support checks.\nThis prevents HDR from being automatically enabled for overlays.");

	if (s_hide_hdr_when_auto_enabled)
	{
		ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.0f, 1.0f), "HDR color spaces are currently hidden");
	}
	else
	{
		ImGui::TextColored(ImVec4(0.0f, 0.7f, 0.0f, 1.0f), "HDR color spaces are visible");
	}
}

extern "C" __declspec(dllexport) const char *NAME = "HDR Hide";
extern "C" __declspec(dllexport) const char *DESCRIPTION = "Example add-on that hides HDR color space support when auto-enabled, preventing HDR from being automatically enabled for overlays.";

BOOL APIENTRY DllMain(HMODULE hModule, DWORD fdwReason, LPVOID)
{
	switch (fdwReason)
	{
	case DLL_PROCESS_ATTACH:
		if (!reshade::register_addon(hModule))
			return FALSE;
		reshade::register_event<reshade::addon_event::reshade_overlay_check_color_space_support>(on_check_color_space_support);
		reshade::register_overlay(nullptr, draw_settings);
		break;
	case DLL_PROCESS_DETACH:
		reshade::unregister_addon(hModule);
		break;
	}

	return TRUE;
}
