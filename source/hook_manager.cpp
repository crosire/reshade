/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "dll_log.hpp"
#include "hook_manager.hpp"
#include <mutex>
#include <cstring>
#include <algorithm>
#include <vector>
#include <Windows.h>

enum class hook_method
{
	export_hook,
	function_hook,
	vtable_hook
};

struct named_hook : public reshade::hook
{
	const char *name;
	hook_method method;
};

struct module_export
{
	reshade::hook::address address;
	const char *name;
	unsigned short ordinal;
};

extern HMODULE g_module_handle;
HMODULE g_export_module_handle = nullptr;
extern std::filesystem::path g_reshade_dll_path;
static std::filesystem::path s_export_hook_path;
static std::mutex s_hooks_mutex;
static std::vector<named_hook> s_hooks;
static std::mutex s_delayed_hook_paths_mutex;
static std::vector<std::filesystem::path> s_delayed_hook_paths;

std::vector<module_export> enumerate_module_exports(HMODULE handle)
{
	const auto image_base = reinterpret_cast<const BYTE *>(handle);
	const auto image_header = reinterpret_cast<const IMAGE_NT_HEADERS *>(image_base +
		reinterpret_cast<const IMAGE_DOS_HEADER *>(image_base)->e_lfanew);

	if (image_header->Signature != IMAGE_NT_SIGNATURE ||
		image_header->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].Size == 0)
		return {}; // The handle does not point to a valid module

	const auto export_dir = reinterpret_cast<const IMAGE_EXPORT_DIRECTORY *>(image_base +
		image_header->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress);
	const auto export_base = static_cast<WORD>(export_dir->Base);

	if (export_dir->NumberOfFunctions == 0)
		return {}; // This module does not contain any exported functions

	std::vector<module_export> exports;
	exports.reserve(export_dir->NumberOfNames);

	for (size_t i = 0; i < exports.capacity(); i++)
	{
		module_export &symbol = exports.emplace_back();
		symbol.ordinal = export_base +
			reinterpret_cast<const  WORD *>(image_base + export_dir->AddressOfNameOrdinals)[i];
		symbol.name = reinterpret_cast<const char *>(image_base +
			reinterpret_cast<const DWORD *>(image_base + export_dir->AddressOfNames)[i]);
		symbol.address = const_cast<void *>(reinterpret_cast<const void *>(image_base +
			reinterpret_cast<const DWORD *>(image_base + export_dir->AddressOfFunctions)[symbol.ordinal - export_base]));
	}

	return exports;
}

static bool install_internal(const char *name, reshade::hook &hook, hook_method method)
{
	// It does not make sense to install a hook which points to itself, so avoid that
	if (hook.target == hook.replacement)
		return false;

#if RESHADE_VERBOSE_LOG
	LOG(DEBUG) << "Installing hook for " << name << " at 0x" << hook.target << " with 0x" << hook.replacement << " using method " << static_cast<int>(method) << " ...";
#endif
	auto status = reshade::hook::status::unknown;

	switch (method)
	{
	case hook_method::export_hook:
		// Export functions are always called directly
		status = reshade::hook::status::success;
		break;
	case hook_method::function_hook:
		status = hook.install();
		break;
	case hook_method::vtable_hook:
		// Make virtual function table memory writable before modifying it
		if (DWORD protection = PAGE_READWRITE;
			VirtualProtect(hook.target, sizeof(reshade::hook::address), protection, &protection))
		{
			// Replace entry in virtual function table with the replacement function
			*reinterpret_cast<reshade::hook::address *>(hook.target) = hook.replacement;

			VirtualProtect(hook.target, sizeof(reshade::hook::address), protection, &protection);

			status = reshade::hook::status::success;
		}
		else
		{
			status = reshade::hook::status::memory_protection_failure;
		}
		break;
	}

	if (status != reshade::hook::status::success)
	{
		LOG(ERROR) << "Failed to install hook for " << name << " with status code " << static_cast<int>(status) << '!';
		return false;
	}

	// Protect access to hook list with a mutex
	{ const std::lock_guard<std::mutex> lock(s_hooks_mutex);
		s_hooks.push_back({ hook, name, method });
	}

#if RESHADE_VERBOSE_LOG
	LOG(DEBUG) << "> Succeeded.";
#endif

	return true;
}
static bool install_internal(HMODULE target_module, HMODULE replacement_module, hook_method method)
{
	assert(target_module != nullptr && replacement_module != nullptr && target_module != replacement_module);

	// Load export tables from both modules
	const auto target_exports = enumerate_module_exports(target_module);
	const auto replacement_exports = enumerate_module_exports(replacement_module);

	if (target_exports.empty())
	{
		LOG(WARN) << "> Empty export table! Skipped.";
		return false;
	}

	size_t num_installed_hooks = 0;
	std::vector<std::tuple<const char *, reshade::hook::address, reshade::hook::address>> matches;
	matches.reserve(replacement_exports.size());

#if RESHADE_VERBOSE_LOG
	LOG(DEBUG) << "> Dumping matches in export table:";
	LOG(DEBUG) << "  +--------------------+---------+----------------------------------------------------+";
	LOG(DEBUG) << "  | Address            | Ordinal | Name                                               |";
	LOG(DEBUG) << "  +--------------------+---------+----------------------------------------------------+";
#endif

	// Analyze export tables and find entries that exist in both modules
	for (const auto &symbol : target_exports)
	{
		if (symbol.name == nullptr || symbol.address == nullptr)
			continue;

		// Find appropriate replacement
		const auto it = std::find_if(replacement_exports.cbegin(), replacement_exports.cend(),
			[&symbol](const auto &module_export) {
				return std::strcmp(module_export.name, symbol.name) == 0;
			});

		// Filter out uninteresting functions
		if (it != replacement_exports.cend() &&
			std::strcmp(symbol.name, "CompatValue") != 0 &&
			std::strcmp(symbol.name, "CompatString") != 0 &&
			std::strcmp(symbol.name, "DXGIDumpJournal") != 0 &&
			std::strcmp(symbol.name, "DXGIReportAdapterConfiguration") != 0 &&
			std::strcmp(symbol.name, "DXGID3D10CreateDevice") != 0 &&
			std::strcmp(symbol.name, "DXGID3D10CreateLayeredDevice") != 0 &&
			std::strcmp(symbol.name, "DXGID3D10ETWRundown") != 0 &&
			std::strcmp(symbol.name, "DXGID3D10GetLayeredDeviceSize") != 0 &&
			std::strcmp(symbol.name, "DXGID3D10RegisterLayers") != 0 &&
			std::strcmp(symbol.name, "Direct3D9EnableMaximizedWindowedModeShim") != 0)
		{
#if RESHADE_VERBOSE_LOG
			LOG(DEBUG) << "  | 0x" << std::setw(16) << symbol.address << " | " << std::setw(7) << symbol.ordinal << " | " << std::setw(50) << symbol.name << " |";
#endif
			matches.push_back(std::make_tuple(symbol.name, symbol.address, it->address));
		}
	}

#if RESHADE_VERBOSE_LOG
	LOG(DEBUG) << "  +--------------------+---------+----------------------------------------------------+";
#endif
	LOG(INFO) << "> Found " << matches.size() << " match(es). Installing ...";

	// Hook all matching exports
	for (const auto &match : matches)
	{
		reshade::hook hook;
#ifdef RESHADE_TEST_APPLICATION
		// RenderDoc hooks the IAT, so get an updated function pointer via its 'GetProcAddress' hook
		hook.target = GetProcAddress(target_module, std::get<0>(match));
#else
		hook.target = std::get<1>(match);
#endif
		hook.trampoline = hook.target;
		hook.replacement = std::get<2>(match);

		if (install_internal(std::get<0>(match), hook, method))
			num_installed_hooks++;
	}

	// Status is successfull if at least one match was found and hooked
	return num_installed_hooks != 0;
}
static bool uninstall_internal(const char *name, reshade::hook &hook, hook_method method)
{
#if RESHADE_VERBOSE_LOG
	LOG(DEBUG) << "Uninstalling hook for " << name << " ...";
#endif
	if (hook.uninstalled())
	{
		LOG(WARN) << "Hook for " << name << " was already uninstalled.";
		return true;
	}

	auto status = reshade::hook::status::unknown;

	switch (method)
	{
	case hook_method::export_hook:
#if RESHADE_VERBOSE_LOG
		LOG(DEBUG) << "> Skipped.";
#endif
		return true;
	case hook_method::function_hook:
		status = hook.uninstall();
		break;
	case hook_method::vtable_hook:
		// Make virtual function table memory writable before modifying it
		if (DWORD protection = PAGE_READWRITE;
			VirtualProtect(hook.target, sizeof(reshade::hook::address), protection, &protection))
		{
			// Replace entry in virtual function table with the original function
			*reinterpret_cast<reshade::hook::address *>(hook.target) = hook.trampoline;

			VirtualProtect(hook.target, sizeof(reshade::hook::address), protection, &protection);

			status = reshade::hook::status::success;
		}
		else
		{
			status = reshade::hook::status::memory_protection_failure;
		}
		break;
	}

	if (status != reshade::hook::status::success)
	{
		LOG(WARN) << "Failed to uninstall hook for " << name << " with status code " << static_cast<int>(status) << '.';
		return false;
	}

#if RESHADE_VERBOSE_LOG
	LOG(DEBUG) << "> Succeeded.";
#endif
	hook.trampoline = nullptr;

	return true;
}

static void install_delayed_hooks(const std::filesystem::path &loaded_path)
{
	// Ignore this call if unable to acquire the mutex to avoid possible deadlock
	if (std::unique_lock<std::mutex> lock(s_delayed_hook_paths_mutex, std::try_to_lock); lock.owns_lock())
	{
		const auto remove = std::remove_if(s_delayed_hook_paths.begin(), s_delayed_hook_paths.end(),
			[&loaded_path](const std::filesystem::path &path) {
				// Pin the module so it cannot be unloaded by the application and cause problems when ReShade tries to call into it afterwards
				HMODULE delayed_handle = nullptr;
				if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_PIN, path.c_str(), &delayed_handle))
					return false;

				LOG(INFO) << "Installing delayed hooks for " << path << " (Just loaded via LoadLibrary(" << loaded_path << ")) ...";

				return install_internal(delayed_handle, g_module_handle, hook_method::function_hook) && reshade::hook::apply_queued_actions();
			});

		s_delayed_hook_paths.erase(remove, s_delayed_hook_paths.end());
	}
	else
	{
		LOG(WARN) << "Ignoring LoadLibrary(" << loaded_path << ") call to avoid possible deadlock.";
	}
}

static reshade::hook find_internal(reshade::hook::address target, reshade::hook::address replacement)
{
	// Protect access to hook list with a mutex
	const std::lock_guard<std::mutex> lock(s_hooks_mutex);

	// Enumerate list of installed hooks and find matching one
	const auto it = std::find_if(s_hooks.cbegin(), s_hooks.cend(),
		[target, replacement](const named_hook &hook) {
			return hook.replacement == replacement &&
				// Optionally compare the target address too (do not do this if it is unknown)
				(target == nullptr || hook.target == target);
		});

	return it != s_hooks.cend() ? *it : reshade::hook {};
}

template <typename T>
static inline T call_unchecked(T replacement)
{
	return reinterpret_cast<T>(find_internal(nullptr, reinterpret_cast<reshade::hook::address>(replacement)).call());
}

HMODULE WINAPI HookLoadLibraryA(LPCSTR lpFileName)
{
	static const auto trampoline = call_unchecked(&HookLoadLibraryA);

	const HMODULE handle = trampoline(lpFileName);
	if (handle != nullptr && handle != g_module_handle)
		install_delayed_hooks(lpFileName);

	return handle;
}
HMODULE WINAPI HookLoadLibraryExA(LPCSTR lpFileName, HANDLE hFile, DWORD dwFlags)
{
	static const auto trampoline = call_unchecked(&HookLoadLibraryExA);

	const HMODULE handle = trampoline(lpFileName, hFile, dwFlags);
	if (handle != nullptr && handle != g_module_handle && (dwFlags & (LOAD_LIBRARY_AS_DATAFILE | LOAD_LIBRARY_AS_DATAFILE_EXCLUSIVE | LOAD_LIBRARY_AS_IMAGE_RESOURCE)) == 0)
		install_delayed_hooks(lpFileName);

	return handle;
}
HMODULE WINAPI HookLoadLibraryW(LPCWSTR lpFileName)
{
	static const auto trampoline = call_unchecked(&HookLoadLibraryW);

	const HMODULE handle = trampoline(lpFileName);
	if (handle != nullptr && handle != g_module_handle)
		install_delayed_hooks(lpFileName);

	return handle;
}
HMODULE WINAPI HookLoadLibraryExW(LPCWSTR lpFileName, HANDLE hFile, DWORD dwFlags)
{
	static const auto trampoline = call_unchecked(&HookLoadLibraryExW);

	const HMODULE handle = trampoline(lpFileName, hFile, dwFlags);
	if (handle != nullptr && handle != g_module_handle && (dwFlags & (LOAD_LIBRARY_AS_DATAFILE | LOAD_LIBRARY_AS_DATAFILE_EXCLUSIVE | LOAD_LIBRARY_AS_IMAGE_RESOURCE)) == 0)
		install_delayed_hooks(lpFileName);

	return handle;
}

bool reshade::hooks::install(const char *name, hook::address target, hook::address replacement, bool queue_enable)
{
	assert(target != nullptr && replacement != nullptr);

	hook hook = find_internal(nullptr, replacement);
	// If the hook was already installed, make sure it was installed for the same target function
	if (hook.installed())
		return target == hook.target;

	// Otherwise, set up the new hook and install it
	hook.target = target;
	hook.replacement = replacement;

	return install_internal(name, hook, hook_method::function_hook) &&
		(queue_enable || hook::apply_queued_actions()); // Can optionally only queue up the hooks instead of installing them right away
}
bool reshade::hooks::install(const char *name, hook::address vtable[], unsigned int offset, hook::address replacement)
{
	assert(vtable != nullptr && replacement != nullptr);

	hook hook = find_internal(nullptr, replacement);
	// Check if the hook was already installed to this virtual function table
	if (hook.installed() && vtable[offset] == hook.replacement)
		return true;

	hook.target = &vtable[offset]; // Target is the address of the virtual function table entry
	hook.trampoline = vtable[offset]; // The current function in that entry is the original function to call
	hook.replacement = replacement;

	return install_internal(name, hook, hook_method::vtable_hook);
}
void reshade::hooks::uninstall()
{
	LOG(INFO) << "Uninstalling " << s_hooks.size() << " hook(s) ...";

	// Disable all hooks in a single batch job
	for (auto &hook_info : s_hooks)
		hook_info.disable();

	hook::apply_queued_actions();

	// Afterwards uninstall and remove all hooks from the list
	for (auto &hook_info : s_hooks)
		uninstall_internal(hook_info.name, hook_info, hook_info.method);

	s_hooks.clear();

	// Free reference to the module loaded for export hooks
	// Otherwise a subsequent call to 'LoadLibrary' could return the handle to the still loaded export module, instead of loading the ReShade module again
	if (g_export_module_handle)
		FreeLibrary(g_export_module_handle);
	g_export_module_handle = nullptr;
}

void reshade::hooks::register_module(const std::filesystem::path &target_path)
{
#ifndef RESHADE_TEST_APPLICATION
	install("LoadLibraryA", reinterpret_cast<hook::address>(&LoadLibraryA), reinterpret_cast<hook::address>(&HookLoadLibraryA), true);
	install("LoadLibraryExA", reinterpret_cast<hook::address>(&LoadLibraryExA), reinterpret_cast<hook::address>(&HookLoadLibraryExA), true);
	install("LoadLibraryW", reinterpret_cast<hook::address>(&LoadLibraryW), reinterpret_cast<hook::address>(&HookLoadLibraryW), true);
	install("LoadLibraryExW", reinterpret_cast<hook::address>(&LoadLibraryExW), reinterpret_cast<hook::address>(&HookLoadLibraryExW), true);

	// Install all "LoadLibrary" hooks in one go immediately
	// Skip this in the test application to make RenderDoc work (which hooks these too)
	hook::apply_queued_actions();
#endif

	LOG(INFO) << "Registering hooks for " << target_path << " ...";

	// Compare module names and delay export hooks for later installation since we cannot call 'LoadLibrary' from this function (it is called from 'DLLMain', which does not allow this)
	// Do a case insensitive comparison here to catch cases like "OPENGL32" refering to the same module as "opengl32.dll"
	const std::filesystem::path target_name = target_path.stem();
	const std::filesystem::path replacement_name = g_reshade_dll_path.stem();
	if (_wcsicmp(target_name.c_str(), replacement_name.c_str()) == 0)
	{
		assert(target_path != g_reshade_dll_path);

		LOG(INFO) << "> Delayed until first call to an exported function.";

		s_export_hook_path = target_path;
	}
	// Similarly, if the target module was not loaded yet, wait for it to get loaded in one of the 'LoadLibrary' hooks and install it then
	// Pin the module so it cannot be unloaded by the application and cause problems when ReShade tries to call into it afterwards
	else if (HMODULE handle; !GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_PIN, target_path.wstring().c_str(), &handle))
	{
		LOG(INFO) << "> Delayed.";

		s_delayed_hook_paths.push_back(target_path);
	}
	else // The target module is already loaded, so we can safely install hooks right away
	{
		LOG(INFO) << "> Libraries loaded.";

		install_internal(handle, g_module_handle, hook_method::function_hook);

		hook::apply_queued_actions();
	}
}

reshade::hook::address reshade::hooks::call(hook::address replacement, hook::address target)
{
	const hook hook = find_internal(target, replacement);
	if (hook.valid())
		return hook.call();

	// If the hook does not exist yet, delay-load export hooks and try again
	if (!g_export_module_handle && !s_export_hook_path.empty())
	{
		assert(s_export_hook_path.is_absolute());

		const HMODULE handle = LoadLibraryW(s_export_hook_path.c_str());

		LOG(INFO) << "Installing export hooks for " << s_export_hook_path << " ...";

		if (handle != nullptr)
		{
			assert(handle != g_module_handle);

			install_internal(handle, g_module_handle, hook_method::export_hook);

			s_export_hook_path.clear();
			g_export_module_handle = handle;

			return call(replacement, target);
		}
		else
		{
			LOG(ERROR) << "Failed to load " << s_export_hook_path << '!';
		}
	}

	LOG(ERROR) << "Unable to resolve hook for 0x" << replacement << '!';

	return nullptr;
}
