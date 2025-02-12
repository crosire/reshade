/*
 * Copyright (C) 2025 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause OR MIT
 */

#include <reshade.hpp>
#include <shellapi.h>
#include <ShlObj.h>
#include <filesystem>

static void on_screenshot(reshade::api::effect_runtime *, const char *path_string)
{
	if (!OpenClipboard(nullptr))
		return;

	const std::wstring path = std::filesystem::u8path(path_string).wstring();

	const HDROP drop_handle = static_cast<HDROP>(GlobalAlloc(GHND, sizeof(DROPFILES) + (path.size() + 1 + 1) * sizeof(WCHAR))); // Terminated with two zero characters
	if (drop_handle != nullptr)
	{
		const LPDROPFILES drop_data = static_cast<LPDROPFILES>(GlobalLock(drop_handle));
		if (drop_data != nullptr)
		{
			drop_data->pFiles = sizeof(DROPFILES);
			drop_data->fWide = TRUE;

			std::memcpy(reinterpret_cast<LPBYTE>(drop_data) + drop_data->pFiles, path.c_str(), path.size() * sizeof(WCHAR));

			GlobalUnlock(drop_handle);

			EmptyClipboard();
			if (SetClipboardData(CF_HDROP, drop_handle) == nullptr)
				GlobalFree(drop_handle);
		}
		else
		{
			GlobalFree(drop_handle);
		}
	}

	CloseClipboard();
}

extern "C" __declspec(dllexport) const char *NAME = "Copy screenshot to clipboard";

BOOL APIENTRY DllMain(HMODULE hModule, DWORD fdwReason, LPVOID)
{
	switch (fdwReason)
	{
	case DLL_PROCESS_ATTACH:
		if (!reshade::register_addon(hModule))
			return FALSE;
		reshade::register_event<reshade::addon_event::reshade_screenshot>(on_screenshot);
		break;
	case DLL_PROCESS_DETACH:
		reshade::unregister_addon(hModule);
		break;
	}

	return TRUE;
}
