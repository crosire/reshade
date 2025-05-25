/*
 * Copyright (C) 2024 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause OR MIT
 */

#include <reshade.hpp>
#include <cstdlib>

static bool on_create_swapchain(reshade::api::device_api, reshade::api::swapchain_desc &desc, void *)
{
	bool modified = false;

	if (bool force_vsync;
		reshade::get_config_value(nullptr, "APP", "ForceVsync", force_vsync))
	{
		if (force_vsync)
		{
			desc.sync_interval = 1;
			modified = true;
		}
	}
	else
	{
		reshade::set_config_value(nullptr, "APP", "ForceVsync", "0");
	}

	if (bool force_windowed;
		reshade::get_config_value(nullptr, "APP", "ForceWindowed", force_windowed))
	{
		if (force_windowed)
		{
			desc.fullscreen_state = false;
			modified = true;
		}
	}
	else
	{
		reshade::set_config_value(nullptr, "APP", "ForceWindowed", "0");
	}

	if (bool force_fullscreen;
		reshade::get_config_value(nullptr, "APP", "ForceFullscreen", force_fullscreen))
	{
		if (force_fullscreen)
		{
			desc.fullscreen_state = true;
			modified = true;
		}
	}
	else
	{
		reshade::set_config_value(nullptr, "APP", "ForceFullscreen", "0");
	}

	if (bool force_10bit_format;
		reshade::get_config_value(nullptr, "APP", "Force10BitFormat", force_10bit_format))
	{
		if (force_10bit_format)
		{
			desc.back_buffer.texture.format = reshade::api::format::r10g10b10a2_unorm;
			modified = true;
		}
	}
	else
	{
		reshade::set_config_value(nullptr, "APP", "Force10BitFormat", "0");
	}

	if (bool force_default_refresh_rate;
		reshade::get_config_value(nullptr, "APP", "ForceDefaultRefreshRate", force_default_refresh_rate))
	{
		if (force_default_refresh_rate)
		{
			desc.fullscreen_refresh_rate = 0.0f;
			modified = true;
		}
	}
	else
	{
		reshade::set_config_value(nullptr, "APP", "ForceDefaultRefreshRate", "0");
	}

	char force_resolution_string[32], *force_resolution_p = force_resolution_string;
	size_t force_resolution_string_size = sizeof(force_resolution_string);
	if (reshade::get_config_value(nullptr, "APP", "ForceResolution", force_resolution_string, &force_resolution_string_size))
	{
		const unsigned long width = std::strtoul(force_resolution_p, &force_resolution_p, 10);
		const char width_terminator = *force_resolution_p++;
		const unsigned long height = std::strtoul(force_resolution_p, &force_resolution_p, 10);
		const char height_terminator = *force_resolution_p++;

		if (width != 0 && width_terminator == '\0' &&
			height != 0 && height_terminator == '\0' &&
			static_cast<size_t>(force_resolution_p - force_resolution_string) == force_resolution_string_size)
		{
			desc.back_buffer.texture.width = static_cast<uint32_t>(width);
			desc.back_buffer.texture.height = static_cast<uint32_t>(height);
			modified = true;
		}
	}
	else
	{
		reshade::set_config_value(nullptr, "APP", "ForceResolution", "0,0");
	}

	return modified;
}

static bool on_set_fullscreen_state(reshade::api::swapchain *, bool fullscreen, void *)
{
	if (bool force_windowed;
		reshade::get_config_value(nullptr, "APP", "ForceWindowed", force_windowed))
	{
		if (force_windowed && fullscreen)
			return true; // Prevent entering fullscreen mode
	}
	if (bool force_fullscreen;
		reshade::get_config_value(nullptr, "APP", "ForceFullscreen", force_fullscreen))
	{
		if (force_fullscreen && !fullscreen)
			return true; // Prevent leaving fullscreen mode
	}

	return false;
}

extern "C" __declspec(dllexport) const char *NAME = "Swap chain override";
extern "C" __declspec(dllexport) const char *DESCRIPTION = "Adds options to force the application into windowed or fullscreen mode, or force a specific resolution or the default refresh rate.\n\n"
	"These are controlled via ReShade.ini:\n"
	"[APP]\n"
	"ForceVsync=<0/1>\n"
	"ForceWindowed=<0/1>\n"
	"ForceFullscreen=<0/1>\n"
	"Force10BitFormat=<0/1>\n"
	"ForceDefaultRefreshRate=<0/1>\n"
	"ForceResolution=<width>,<height>";

BOOL APIENTRY DllMain(HMODULE hModule, DWORD fdwReason, LPVOID)
{
	switch (fdwReason)
	{
	case DLL_PROCESS_ATTACH:
		if (!reshade::register_addon(hModule))
			return FALSE;
		reshade::register_event<reshade::addon_event::create_swapchain>(on_create_swapchain);
		reshade::register_event<reshade::addon_event::set_fullscreen_state>(on_set_fullscreen_state);
		break;
	case DLL_PROCESS_DETACH:
		reshade::unregister_addon(hModule);
		break;
	}

	return TRUE;
}
