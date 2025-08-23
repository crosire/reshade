/*
 * Copyright (C) 2025
 * SPDX-License-Identifier: BSD-3-Clause OR MIT
 */

#include <imgui.h>
#include <reshade.hpp>

using namespace reshade::api;

static bool s_hide_hdr = false;

static void on_init(effect_runtime *runtime)
{
    // Load config once (global) and apply to each runtime on init
    static bool s_config_loaded = false;
    if (!s_config_loaded)
    {
        if (!reshade::get_config_value(nullptr, "ADDON", "HideHDR", s_hide_hdr))
            reshade::set_config_value(nullptr, "ADDON", "HideHDR", 0);
        s_config_loaded = true;
    }

    runtime->set_hide_hdr(s_hide_hdr);
}

static void draw_overlay(effect_runtime *runtime)
{
    bool hide_hdr = s_hide_hdr;
    if (ImGui::Checkbox("Hide HDR (force SDR reporting)", &hide_hdr))
    {
        s_hide_hdr = hide_hdr;
        reshade::set_config_value(nullptr, "ADDON", "HideHDR", hide_hdr ? 1 : 0);
        runtime->set_hide_hdr(hide_hdr);
    }
}

#ifndef BUILTIN_ADDON

extern "C" __declspec(dllexport) const char *NAME = "Hide HDR";
extern "C" __declspec(dllexport) const char *DESCRIPTION =
    "Adds a setting to hide HDR from applications by forcing SDR reporting via DXGI.\n\n"
    "Configured via ReShade.ini:\n"
    "[ADDON]\n"
    "HideHDR=<0/1>\n";

BOOL APIENTRY DllMain(HMODULE hModule, DWORD fdwReason, LPVOID)
{
    switch (fdwReason)
    {
    case DLL_PROCESS_ATTACH:
        if (!reshade::register_addon(hModule))
            return FALSE;
        reshade::register_overlay(nullptr, draw_overlay);
        reshade::register_event<reshade::addon_event::init_effect_runtime>(on_init);
        break;
    case DLL_PROCESS_DETACH:
        reshade::unregister_event<reshade::addon_event::init_effect_runtime>(on_init);
        reshade::unregister_addon(hModule);
        break;
    }

    return TRUE;
}

#endif


