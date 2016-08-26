#include "log.hpp"
#include "filesystem.hpp"
#include "input.hpp"
#include "hook_manager.hpp"
#include "version.h"

#include <Windows.h>

namespace reshade
{
	filesystem::path s_injector_path, s_executable_path;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD fdwReason, LPVOID lpvReserved)
{
	UNREFERENCED_PARAMETER(lpvReserved);

	using namespace reshade;

	switch (fdwReason)
	{
		case DLL_PROCESS_ATTACH:
		{
			DisableThreadLibraryCalls(hModule);

			s_injector_path = filesystem::get_module_path(hModule);
			s_executable_path = filesystem::get_module_path(nullptr);

			filesystem::path log_path(s_injector_path);
			log_path.replace_extension(".log");

			log::open(log_path);

#ifdef WIN64
#define VERSION_PLATFORM "64-bit"
#else
#define VERSION_PLATFORM "32-bit"
#endif
			LOG(INFO) << "Initializing crosire's ReShade version '" VERSION_STRING_FILE "' (" << VERSION_PLATFORM << ") built on '" VERSION_DATE " " VERSION_TIME "' loaded from " << s_injector_path << " to " << s_executable_path << " ...";

			const auto system_path = filesystem::get_special_folder_path(filesystem::special_folder::system);
			hooks::register_module(system_path / "d3d8.dll");
			hooks::register_module(system_path / "d3d9.dll");
			hooks::register_module(system_path / "d3d10.dll");
			hooks::register_module(system_path / "d3d10_1.dll");
			hooks::register_module(system_path / "d3d11.dll");
			hooks::register_module(system_path / "d3d12.dll");
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
