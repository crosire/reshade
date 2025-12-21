/*
 * Copyright (C) 2014 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "version.h"
#include "dll_log.hpp"
#include "ini_file.hpp"
#include "hook_manager.hpp"
#include "addon_manager.hpp"
#include <Windows.h>
#include <Psapi.h>
#include <delayimp.h> // Delay-load helpers

// Export special symbol to identify modules as ReShade instances
extern "C" __declspec(dllexport) const char *ReShadeVersion = VERSION_STRING_PRODUCT;

HANDLE g_exit_event = nullptr;
HMODULE g_module_handle = nullptr;
std::filesystem::path g_reshade_dll_path;
std::filesystem::path g_reshade_base_path;
std::filesystem::path g_target_executable_path;

/// <summary>
/// Checks whether the current application is an UWP app.
/// </summary>
bool is_uwp_app()
{
	const auto GetCurrentPackageFullName = reinterpret_cast<LONG(WINAPI *)(UINT32 *, PWSTR)>(GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "GetCurrentPackageFullName"));
	if (GetCurrentPackageFullName == nullptr)
		return false;
	// This will return APPMODEL_ERROR_NO_PACKAGE if not a packaged UWP app
	UINT32 length = 0;
	return GetCurrentPackageFullName(&length, nullptr) == ERROR_INSUFFICIENT_BUFFER;
}

/// <summary>
/// Checks whether the current operating system is Windows 7 or earlier.
/// </summary>
bool is_windows7()
{
	ULONGLONG condition = 0;
	VER_SET_CONDITION(condition, VER_MAJORVERSION, VER_LESS_EQUAL);
	VER_SET_CONDITION(condition, VER_MINORVERSION, VER_LESS_EQUAL);

	OSVERSIONINFOEX verinfo_windows7 = { sizeof(verinfo_windows7), 6, 1 };
	return VerifyVersionInfo(&verinfo_windows7, VER_MAJORVERSION | VER_MINORVERSION, condition) != FALSE;
}

/// <summary>
/// Expands any environment variables in the path (like "%USERPROFILE%") and checks whether it points towards an existing directory.
/// </summary>
static bool resolve_env_path(std::filesystem::path &path, const std::filesystem::path &base = g_reshade_dll_path.parent_path())
{
	if (path.empty())
		return false;

	WCHAR buf[4096];
	if (ExpandEnvironmentStringsW(path.c_str(), buf, ARRAYSIZE(buf)))
		path = buf;
	else
		return false;

	path = base / path;

	std::error_code ec;
	path = std::filesystem::canonical(path, ec);
	return !ec && std::filesystem::is_directory(path, ec);
}

/// <summary>
/// Returns the path that should be used as base for relative paths.
/// </summary>
std::filesystem::path get_base_path(bool default_to_target_executable_path = false)
{
	std::filesystem::path path_override;

	// Cannot use global config here yet, since it uses base path for look up, so look at config file next to target executable instead
	if (reshade::ini_file::load_cache(g_target_executable_path.parent_path() / L"ReShade.ini").get("INSTALL", "BasePath", path_override) &&
		resolve_env_path(path_override))
		return path_override;

	WCHAR buf[4096];
	path_override.assign(buf, buf + GetEnvironmentVariableW(L"RESHADE_BASE_PATH_OVERRIDE", buf, ARRAYSIZE(buf)));
	if (resolve_env_path(path_override))
		return path_override;

	return default_to_target_executable_path ? g_target_executable_path.parent_path() : g_reshade_dll_path.parent_path();
}

/// <summary>
/// Returns the path to the "System32" directory or the module path from global configuration if it exists.
/// </summary>
std::filesystem::path get_system_path()
{
	std::filesystem::path path_override;

	if (reshade::global_config().get("INSTALL", "ModulePath", path_override) &&
		resolve_env_path(path_override))
		return path_override;

	WCHAR buf[4096];
	path_override.assign(buf, buf + GetEnvironmentVariableW(L"RESHADE_MODULE_PATH_OVERRIDE", buf, ARRAYSIZE(buf)));
	if (resolve_env_path(path_override))
		return path_override;

	return std::filesystem::path(buf, buf + GetSystemDirectoryW(buf, ARRAYSIZE(buf)));
}

/// <summary>
/// Returns the path to the module file identified by the specified <paramref name="module"/> handle.
/// </summary>
std::filesystem::path get_module_path(HMODULE module)
{
	WCHAR buf[4096];
	return std::filesystem::path(buf, buf + GetModuleFileNameW(module, buf, ARRAYSIZE(buf)));
}

#ifndef RESHADE_TEST_APPLICATION

#ifndef NDEBUG
#include <DbgHelp.h>

static PVOID s_exception_handler_handle = nullptr;
#endif

BOOL APIENTRY DllMain(HMODULE hModule, DWORD fdwReason, LPVOID)
{
	switch (fdwReason)
	{
		case DLL_PROCESS_ATTACH:
		{
			// Do NOT call 'DisableThreadLibraryCalls', since we are linking against the static CRT, which requires the thread notifications to work properly
			// It does not do anything when static TLS is used anyway, which is the case (see https://docs.microsoft.com/windows/win32/api/libloaderapi/nf-libloaderapi-disablethreadlibrarycalls)
			g_module_handle = hModule;
			g_reshade_dll_path = get_module_path(hModule);
			g_target_executable_path = get_module_path(nullptr);

			const std::filesystem::path module_name = g_reshade_dll_path.stem();

			const bool is_d3d = _wcsnicmp(module_name.c_str(), L"d3d", 3) == 0;
			const bool is_dxgi = _wcsicmp(module_name.c_str(), L"dxgi") == 0;
			const bool is_opengl = _wcsicmp(module_name.c_str(), L"opengl32") == 0;
			const bool is_dinput = _wcsnicmp(module_name.c_str(), L"dinput", 6) == 0;
			const bool is_asi = g_reshade_dll_path.extension() == L".asi";

			// UWP apps do not have write access to the application directory, so never default the base path to it for them
			const bool default_base_to_target_executable_path = !is_d3d && !is_dxgi && !is_opengl && !is_dinput && !is_asi && !is_uwp_app();

			g_reshade_base_path = get_base_path(default_base_to_target_executable_path);

			const reshade::ini_file &config = reshade::global_config();

			// When ReShade is not loaded by proxy, only actually load when a configuration file exists for the target executable
			// This e.g. prevents loading the implicit Vulkan layer when not explicitly enabled for an application
			if (default_base_to_target_executable_path && !GetEnvironmentVariableW(L"RESHADE_DISABLE_LOADING_CHECK", nullptr, 0))
			{
#ifndef NDEBUG
				// Avoid loading in the ReShade test application
				if (g_target_executable_path.filename() ==
#ifndef _WIN64
						L"ReShade32.exe"
#else
						L"ReShade64.exe"
#endif
						)
				{
					return FALSE;
				}
#endif

				std::error_code ec;
				if (!std::filesystem::exists(config.path(), ec))
				{
#ifndef NDEBUG
					// Log was not yet opened at this point, so this only writes to debug output
					reshade::log::message(reshade::log::level::warning, "ReShade was not enabled for '%s'! Aborting initialization ...", g_target_executable_path.u8string().c_str());
#endif
					return FALSE; // Make the 'LoadLibrary' call that loaded this instance fail
				}
			}

			if (config.get("INSTALL", "Logging") || (!config.has("INSTALL", "Logging") && !GetEnvironmentVariableW(L"RESHADE_DISABLE_LOGGING", nullptr, 0)))
			{
				std::filesystem::path log_path = config.path();
				log_path.replace_extension(L".log");

				std::error_code ec;
				if (!reshade::log::open_log_file(log_path, ec))
				{
					// Try a different file if the default failed to open (e.g. because currently in use by another ReShade instance)
					for (int log_index = 0; log_index < 10 && std::filesystem::exists(log_path, ec); ++log_index)
					{
						log_path.replace_extension(L".log" + std::to_wstring(log_index + 1));

						if (reshade::log::open_log_file(log_path, ec))
							break;
					}

#ifndef NDEBUG
					if (ec)
						reshade::log::message(reshade::log::level::error, "Opening the ReShade log file failed with error code %d.", ec.value());
#endif
				}
			}

			reshade::log::message(reshade::log::level::info,
				"Initializing crosire's ReShade version '" VERSION_STRING_FILE "' "
#ifndef _WIN64
				"(32-bit) "
#else
				"(64-bit) "
#endif
				"loaded from '%s' into '%s' (0x%X) ...",
				g_reshade_dll_path.u8string().c_str(),
#ifndef NDEBUG
				static_cast<const char *>(GetCommandLineA()),
#else
				// Do not log full command-line in release builds, since it may contain sensitive information like authentication tokens
				g_target_executable_path.u8string().c_str(),
#endif
				static_cast<unsigned int>(std::hash<std::string>()(g_target_executable_path.stem().u8string()) & 0xFFFFFFFF));

			// Check if another ReShade instance was already loaded into the process
			if (HMODULE modules[1024]; K32EnumProcessModules(GetCurrentProcess(), modules, sizeof(modules), &fdwReason)) // Use kernel32 variant which is available in DllMain
			{
				// Skip first module (the main application module)
				for (DWORD i = 1; i < std::min<DWORD>(fdwReason / sizeof(HMODULE), ARRAYSIZE(modules)); ++i)
				{
					if (modules[i] != hModule && GetProcAddress(modules[i], "ReShadeVersion") != nullptr)
					{
						reshade::log::message(reshade::log::level::warning, "Another ReShade instance was already loaded from '%s'! Aborting initialization ...", get_module_path(modules[i]).u8string().c_str());
						return FALSE; // Make the 'LoadLibrary' call that loaded this instance fail
					}
				}
			}

#ifndef NDEBUG
			if (config.get("INSTALL", "DumpExceptions"))
			{
				// Load debug helper library as soon as possible, so that it is later available when an exception is handled
				CreateThread(nullptr, 0, reinterpret_cast<LPTHREAD_START_ROUTINE>(&LoadLibraryW), const_cast<LPVOID>(static_cast<LPCVOID>(L"dbghelp.dll")), 0, nullptr);

				s_exception_handler_handle = AddVectoredExceptionHandler(1, [](PEXCEPTION_POINTERS ex) -> LONG {
					// Ignore debugging and some common language exceptions
					if (const DWORD code = ex->ExceptionRecord->ExceptionCode;
						code == CONTROL_C_EXIT || code == 0x406D1388 /* SetThreadName */ ||
						code == DBG_PRINTEXCEPTION_C || code == DBG_PRINTEXCEPTION_WIDE_C || code == STATUS_BREAKPOINT ||
						code == 0xE0434352 /* CLR exception */ ||
						code == 0xE06D7363 /* Visual C++ exception */ ||
						((code ^ 0xE24C4A00) <= 0xFF) /* LuaJIT exception */)
						goto continue_search;

					// Create dump with exception information for the first 100 occurrences
					if (static unsigned int dump_index = 0; dump_index < 100)
					{
						const auto dbghelp_module = GetModuleHandleW(L"dbghelp.dll");
						if (dbghelp_module == nullptr)
							goto continue_search;

						const auto dbghelp_write_dump = reinterpret_cast<BOOL(WINAPI *)(HANDLE, DWORD, HANDLE, MINIDUMP_TYPE, PMINIDUMP_EXCEPTION_INFORMATION, PMINIDUMP_USER_STREAM_INFORMATION, PMINIDUMP_CALLBACK_INFORMATION)>(
							GetProcAddress(dbghelp_module, "MiniDumpWriteDump"));
						if (dbghelp_write_dump == nullptr)
							goto continue_search;

						char dump_name[] = "exception_00.dmp";
						dump_name[10] = '0' + static_cast<char>(dump_index / 10);
						dump_name[11] = '0' + static_cast<char>(dump_index % 10);

						const HANDLE file = CreateFileA(dump_name, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
						if (file == INVALID_HANDLE_VALUE)
							goto continue_search;

						MINIDUMP_EXCEPTION_INFORMATION info;
						info.ThreadId = GetCurrentThreadId();
						info.ExceptionPointers = ex;
						info.ClientPointers = TRUE;

						if (dbghelp_write_dump(GetCurrentProcess(), GetCurrentProcessId(), file, MiniDumpNormal, &info, nullptr, nullptr))
							dump_index++;
						else
							reshade::log::message(reshade::log::level::error, "Failed to write minidump!");

						CloseHandle(file);
					}

				continue_search:
					return EXCEPTION_CONTINUE_SEARCH;
				});
			}
#endif

			if (config.get("INSTALL", "PreventUnloading"))
			{
				GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_PIN | GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, reinterpret_cast<LPCWSTR>(hModule), &hModule);
			}

			// Register modules to hook
			{
				if (!GetEnvironmentVariableW(L"RESHADE_DISABLE_INPUT_HOOK", nullptr, 0))
				{
					g_exit_event = CreateEvent(nullptr, TRUE, FALSE, nullptr);

					reshade::hooks::register_module(L"user32.dll");

					reshade::hooks::register_module(get_system_path() / L"dinput.dll");
					reshade::hooks::register_module(get_system_path() / L"dinput8.dll");
				}

#if RESHADE_ADDON == 1
				if (!GetEnvironmentVariableW(L"RESHADE_DISABLE_NETWORK_HOOK", nullptr, 0))
				{
					reshade::hooks::register_module(L"ws2_32.dll");
				}
				else
				{
					// Disable network hooks when requested through an environment variable and always disable add-ons in that case
					extern volatile long g_network_traffic;
					g_network_traffic = std::numeric_limits<long>::max(); // Special value to indicate that add-ons should never be enabled
					reshade::addon_enabled = false;
				}
#endif

				if (!GetEnvironmentVariableW(L"RESHADE_DISABLE_GRAPHICS_HOOK", nullptr, 0))
				{
					// Can optionally hook the graphics entry points exported by NVIDIA Streamline, instead of the system ones, to have ReShade apply before Streamline
					// This does not work when module is called dxgi.dll though, as Streamline would then load the ReShade module itself again to continue its call chain 
					const bool streamline = !is_d3d && !is_dxgi && config.get("INSTALL", "HookStreamline");
					if (streamline)
					{
						reshade::hooks::register_module(L"sl.interposer.dll");
					}

					// Only register DirectX hooks when module is not called opengl32.dll
					if ((!is_opengl && !streamline) || config.get("INSTALL", "HookDirectX"))
					{
						// Register DirectDraw module in case it was used to load ReShade (but ignore otherwise)
						if (_wcsicmp(module_name.c_str(), L"ddraw") == 0)
							reshade::hooks::register_module(get_system_path() / L"ddraw.dll");

						reshade::hooks::register_module(get_system_path() / L"d2d1.dll");
						reshade::hooks::register_module(get_system_path() / L"d3d9.dll");
						reshade::hooks::register_module(get_system_path() / L"d3d10.dll");
						reshade::hooks::register_module(get_system_path() / L"d3d10_1.dll");
						reshade::hooks::register_module(get_system_path() / L"d3d11.dll");

						// On Windows 7 the d3d12on7 module is not in the system path, so register to hook any d3d12.dll loaded instead
						if (is_windows7() && _wcsicmp(module_name.c_str(), L"d3d12") != 0)
							reshade::hooks::register_module(L"d3d12.dll");
						else
							reshade::hooks::register_module(get_system_path() / L"d3d12.dll");

						reshade::hooks::register_module(get_system_path() / L"dxgi.dll");
					}

					// Only register OpenGL hooks when module is not called any D3D module name
					if ((!is_d3d && !is_dxgi && !streamline) || config.get("INSTALL", "HookOpenGL"))
					{
						reshade::hooks::register_module(get_system_path() / L"opengl32.dll");
					}

					// Do not register Vulkan hooks, since Vulkan layering mechanism is used instead

					reshade::hooks::register_module(L"openvr_api.dll");
				}
			}

			reshade::log::message(reshade::log::level::info, "Initialized.");

#if RESHADE_ADDON >= 2
			// It is not safe to call 'LoadLibrary' from 'DllMain', but there are cases where add-ons want to be loaded as early as possible, so at least give the option
			if (config.get("ADDON", "LoadFromDllMain"))
			{
				reshade::load_addons();
			}
#endif
			break;
		}
		case DLL_PROCESS_DETACH:
		{
			reshade::log::message(reshade::log::level::info, "Exiting ...");

#if RESHADE_ADDON >= 2
			if (reshade::has_loaded_addons())
			{
				if (reshade::global_config().get("ADDON", "LoadFromDllMain"))
					reshade::unload_addons();
				else
					reshade::log::message(reshade::log::level::warning, "Add-ons are still loaded! Application may crash on exit.");
			}
#endif

			reshade::hooks::uninstall();

			// Module is now invalid, so break out of any message loops that may still have it in the call stack (see 'HookGetMessage' implementation in input.cpp)
			// This is necessary since a different thread may have called into the 'GetMessage' hook from ReShade, but may not receive a message until after the ReShade module was unloaded
			// At that point it would return to code that was already unloaded and crash
			// Hooks were already uninstalled now, so after returning from any existing 'GetMessage' hook call, application will call the real one next and things continue to work
			if (g_exit_event != nullptr)
			{
				SetEvent(g_exit_event);
				Sleep(1000);
				CloseHandle(g_exit_event);
			}

#ifndef NDEBUG
			if (s_exception_handler_handle != nullptr)
				RemoveVectoredExceptionHandler(s_exception_handler_handle);
#endif

			reshade::log::message(reshade::log::level::info, "Finished exiting.");
			break;
		}
	}

	return TRUE;
}

static FARPROC WINAPI DliNotifyHook2(unsigned dliNotify, PDelayLoadInfo pdli)
{
	if (dliNotify == dliNotePreLoadLibrary && _stricmp(pdli->szDll, "D3DCompiler_47.dll") == 0)
	{
		// Prefer loading up-to-date system D3DCompiler DLL over local variants
		// Do not check system path when running in Wine though, since the D3DCompiler DLL there does not support various features
		if (GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "wine_get_version") == nullptr)
		{
			const HMODULE d3dcompiler_47_module = LoadLibraryW((get_system_path() / L"D3DCompiler_47.dll").c_str());
			return reinterpret_cast<FARPROC>(d3dcompiler_47_module);
		}

		if (const HMODULE d3dcompiler_47_module = LoadLibraryW(L"D3DCompiler_47.dll"))
		{
			return reinterpret_cast<FARPROC>(d3dcompiler_47_module);
		}

		// Fall back to older D3DCompiler version
		if (const HMODULE d3dcompiler_43_module = LoadLibraryW(L"D3DCompiler_43.dll"))
		{
			return reinterpret_cast<FARPROC>(d3dcompiler_43_module);
		}
	}

	return nullptr;
}

// See https://learn.microsoft.com/cpp/build/reference/understanding-the-helper-function
extern "C" const PfnDliHook __pfnDliNotifyHook2 = DliNotifyHook2;

#endif
