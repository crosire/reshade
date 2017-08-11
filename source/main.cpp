/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "log.hpp"
#include "filesystem.hpp"
#include "input.hpp"
#include "runtime.hpp"
#include "hook_manager.hpp"
#include "version.h"
#include <Windows.h>

HMODULE g_module_handle = nullptr;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD fdwReason, LPVOID lpvReserved)
{
	UNREFERENCED_PARAMETER(lpvReserved);

	using namespace reshade;

	switch (fdwReason)
	{
		case DLL_PROCESS_ATTACH:
		{
			g_module_handle = hModule;
			runtime::s_reshade_dll_path = filesystem::get_module_path(hModule);
			runtime::s_target_executable_path = filesystem::get_module_path(nullptr);
			const filesystem::path system_path = filesystem::get_special_folder_path(filesystem::special_folder::system);

			log::open(filesystem::path(runtime::s_reshade_dll_path).replace_extension(".log"));

#ifdef WIN64
#define VERSION_PLATFORM "64-bit"
#else
#define VERSION_PLATFORM "32-bit"
#endif
			LOG(INFO) << "Initializing crosire's ReShade version '" VERSION_STRING_FILE "' (" << VERSION_PLATFORM << ") built on '" VERSION_DATE " " VERSION_TIME "' loaded from " << runtime::s_reshade_dll_path << " to " << runtime::s_target_executable_path << " ...";

			hooks::register_module(system_path / "d3d9.dll");
			hooks::register_module(system_path / "d3d10.dll");
			hooks::register_module(system_path / "d3d10_1.dll");
			hooks::register_module(system_path / "d3d11.dll");
			hooks::register_module(system_path / "dxgi.dll");
			hooks::register_module(system_path / "opengl32.dll");
			hooks::register_module(system_path / "user32.dll");
			hooks::register_module(system_path / "ws2_32.dll");

			LOG(INFO) << "Initialized.";
			break;
		}
		case DLL_PROCESS_DETACH:
		{
			LOG(INFO) << "Exiting ...";

			input::uninstall();
			hooks::uninstall();

			LOG(INFO) << "Exited.";
			break;
		}
	}

	return TRUE;
}
