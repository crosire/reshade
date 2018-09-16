/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "log.hpp"
#include "hook_manager.hpp"
#include <assert.h>
#include <mutex>
#include <algorithm>
#include <tuple>
#include <vector>
#include <unordered_map>
#include <Windows.h>

extern HMODULE g_module_handle;

namespace
{
	enum class hook_method
	{
		export_hook,
		function_hook,
		vtable_hook
	};
	struct module_export
	{
		reshade::hook::address address;
		const char *name;
		unsigned short ordinal;
	};

	inline auto load_module(const reshade::filesystem::path &path)
	{
		return LoadLibraryW(path.wstring().c_str());
	}
	inline bool is_module_loaded(const reshade::filesystem::path &path)
	{
		HMODULE handle = nullptr;
		return GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, path.wstring().c_str(), &handle) && handle != nullptr;
	}
	inline bool is_module_loaded(const reshade::filesystem::path &path, HMODULE &out_handle)
	{
		return GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_PIN, path.wstring().c_str(), &out_handle) && out_handle != nullptr;
	}
	std::vector<module_export> get_module_exports(HMODULE handle)
	{
		const auto imagebase = reinterpret_cast<const BYTE *>(handle);
		const auto imageheader = reinterpret_cast<const IMAGE_NT_HEADERS *>(imagebase + reinterpret_cast<const IMAGE_DOS_HEADER *>(imagebase)->e_lfanew);

		if (imageheader->Signature != IMAGE_NT_SIGNATURE || imageheader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].Size == 0)
		{
			return { };
		}

		const auto exportdir = reinterpret_cast<const IMAGE_EXPORT_DIRECTORY *>(imagebase + imageheader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress);
		const auto exportbase = static_cast<WORD>(exportdir->Base);

		if (exportdir->NumberOfFunctions == 0)
		{
			return { };
		}

		std::vector<module_export> exports;
		exports.reserve(exportdir->NumberOfNames);

		for (size_t i = 0; i < exports.capacity(); i++)
		{
			module_export symbol;
			symbol.ordinal = reinterpret_cast<const WORD *>(imagebase + exportdir->AddressOfNameOrdinals)[i] + exportbase;
			symbol.name = reinterpret_cast<const char *>(imagebase + reinterpret_cast<const DWORD *>(imagebase + exportdir->AddressOfNames)[i]);
			symbol.address = const_cast<void *>(reinterpret_cast<const void *>(imagebase + reinterpret_cast<const DWORD *>(imagebase + exportdir->AddressOfFunctions)[symbol.ordinal - exportbase]));

			exports.push_back(std::move(symbol));
		}

		return exports;
	}

	reshade::filesystem::path s_export_hook_path;
	std::vector<std::tuple<const char *, reshade::hook, hook_method>> s_hooks; std::mutex s_mutex_hooks;
	std::vector<reshade::filesystem::path> s_delayed_hook_paths; std::mutex s_mutex_delayed_hook_paths;
	std::unordered_map<reshade::hook::address, reshade::hook::address *> s_vtable_addresses; std::mutex s_mutex_vtable_addresses;

	bool install_internal(const char *name, reshade::hook &hook, hook_method method)
	{
#if RESHADE_VERBOSE_LOG
		LOG(DEBUG) << "Installing hook for '" << name << "' at 0x" << hook.target << " with 0x" << hook.replacement << " using method " << static_cast<int>(method) << " ...";
#endif

		auto status = reshade::hook::status::unknown;

		switch (method)
		{
			case hook_method::export_hook:
			{
				status = reshade::hook::status::success;
				break;
			}
			case hook_method::function_hook:
			{
				status = hook.install();
				break;
			}
			case hook_method::vtable_hook:
			{
				DWORD protection = PAGE_READWRITE;
				const auto target_address = s_vtable_addresses.at(hook.target);

				if (VirtualProtect(target_address, sizeof(*target_address), protection, &protection))
				{
					*target_address = hook.replacement;

					VirtualProtect(target_address, sizeof(*target_address), protection, &protection);

					status = reshade::hook::status::success;
				}
				else
				{
					status = reshade::hook::status::memory_protection_failure;
				}
				break;
			}
		}

		if (status != reshade::hook::status::success)
		{
			LOG(ERROR) << "Failed to install hook for '" << name << "' with status code " << static_cast<int>(status) << ".";

			return false;
		}

#if RESHADE_VERBOSE_LOG
		LOG(DEBUG) << "> Succeeded.";
#endif

		{ const std::lock_guard<std::mutex> lock(s_mutex_hooks);
			s_hooks.push_back(std::make_tuple(name, hook, method));
		}

		return true;
	}
	bool install_internal(HMODULE target_module, HMODULE replacement_module, hook_method method)
	{
		assert(target_module != nullptr);
		assert(replacement_module != nullptr);

		// Load export tables
		const auto target_exports = get_module_exports(target_module);
		const auto replacement_exports = get_module_exports(replacement_module);

		if (target_exports.empty())
		{
			LOG(INFO) << "> Empty export table! Skipped.";

			return false;
		}

		size_t install_count = 0;
		std::vector<std::tuple<const char *, reshade::hook::address, reshade::hook::address>> matches;
		matches.reserve(replacement_exports.size());

#if RESHADE_VERBOSE_LOG
		LOG(DEBUG) << "> Dumping matches in export table:";
		LOG(DEBUG) << "  +--------------------+---------+----------------------------------------------------+";
		LOG(DEBUG) << "  | Address            | Ordinal | Name                                               |";
		LOG(DEBUG) << "  +--------------------+---------+----------------------------------------------------+";
#endif

		// Analyze export table
		for (const auto &symbol : target_exports)
		{
			if (symbol.name == nullptr || symbol.address == nullptr)
			{
				continue;
			}

			// Find appropriate replacement
			const auto it = std::find_if(replacement_exports.cbegin(), replacement_exports.cend(),
				[&symbol](const auto &moduleexport) {
					return std::strcmp(moduleexport.name, symbol.name) == 0;
				});

			// Filter uninteresting functions
			if (it != replacement_exports.cend() &&
				std::strcmp(symbol.name, "DXGIReportAdapterConfiguration") != 0 &&
				std::strcmp(symbol.name, "DXGIDumpJournal") != 0)
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

		// Hook matching exports
		for (const auto &match : matches)
		{
			reshade::hook hook;
			hook.target = std::get<1>(match);
			hook.trampoline = hook.target;
			hook.replacement = std::get<2>(match);

			if (install_internal(std::get<0>(match), hook, method))
			{
				install_count++;
			}
		}

		return install_count != 0;
	}
	bool uninstall_internal(const char *name, reshade::hook &hook, hook_method method)
	{
#if RESHADE_VERBOSE_LOG
		LOG(DEBUG) << "Uninstalling hook for '" << name << "' ...";
#endif

		if (hook.uninstalled())
		{
			LOG(WARNING) << "Hook for '" << name << "' was already uninstalled.";

			return true;
		}

		auto status = reshade::hook::status::unknown;

		switch (method)
		{
			case hook_method::export_hook:
			{
#if RESHADE_VERBOSE_LOG
				LOG(DEBUG) << "> Skipped.";
#endif

				return true;
			}
			case hook_method::function_hook:
			{
				status = hook.uninstall();
				break;
			}
			case hook_method::vtable_hook:
			{
				DWORD protection = PAGE_READWRITE;
				const auto target_address = s_vtable_addresses.at(hook.target);

				if (VirtualProtect(target_address, sizeof(*target_address), protection, &protection))
				{
					*target_address = hook.target;
					s_vtable_addresses.erase(hook.target);

					VirtualProtect(target_address, sizeof(*target_address), protection, &protection);

					status = reshade::hook::status::success;
				}
				else
				{
					status = reshade::hook::status::memory_protection_failure;
				}
				break;
			}
		}

		if (status != reshade::hook::status::success)
		{
			LOG(WARNING) << "Failed to uninstall hook for '" << name << "' with status code " << static_cast<int>(status) << ".";

			return false;
		}

#if RESHADE_VERBOSE_LOG
		LOG(DEBUG) << "> Succeeded.";
#endif

		hook.trampoline = nullptr;

		return true;
	}

	void install_delayed_hooks(const reshade::filesystem::path &loaded_path)
	{
		// Ignore this call if unable to acquire the mutex to avoid possible deadlock
		if (std::unique_lock<std::mutex> lock(s_mutex_delayed_hook_paths, std::try_to_lock); lock.owns_lock())
		{
			const auto remove = std::remove_if(s_delayed_hook_paths.begin(), s_delayed_hook_paths.end(),
				[&loaded_path](const auto &path) {
				HMODULE delayed_handle = nullptr;

				if (!is_module_loaded(path, delayed_handle))
					return false;

				LOG(INFO) << "Installing delayed hooks for " << path << " (Just loaded via 'LoadLibrary(\"" << loaded_path << "\")') ...";

				return install_internal(delayed_handle, g_module_handle, hook_method::function_hook) && reshade::hook::apply_queued_actions();
			});

			s_delayed_hook_paths.erase(remove, s_delayed_hook_paths.end());
		}
		else
		{
			LOG(WARNING) << "Ignoring 'LoadLibrary(\"" << loaded_path << "\")' call to avoid possible deadlock.";
		}
	}

	reshade::hook find_internal(reshade::hook::address replacement)
	{ const std::lock_guard<std::mutex> lock(s_mutex_hooks);
		const auto it = std::find_if(s_hooks.cbegin(), s_hooks.cend(),
			[replacement](const auto &hook) {
				return std::get<1>(hook).replacement == replacement;
			});

		return it != s_hooks.cend() ? std::get<1>(*it) : reshade::hook { };
	}
	template <typename T>
	inline T call_unchecked(T replacement)
	{
		return reinterpret_cast<T>(find_internal(reinterpret_cast<reshade::hook::address>(replacement)).call());
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

		if (dwFlags == 0 && handle != nullptr && handle != g_module_handle)
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
		if (dwFlags == 0 && handle != nullptr && handle != g_module_handle)
			install_delayed_hooks(lpFileName);

		return handle;
	}
}

bool reshade::hooks::install(const char *name, hook::address target, hook::address replacement, bool queue_enable)
{
	assert(target != nullptr);
	assert(replacement != nullptr);

	if (target == replacement)
	{
		return false;
	}

	hook hook = find_internal(replacement);

	if (hook.installed())
	{
		return target == hook.target;
	}
	else
	{
		hook.target = target;
		hook.replacement = replacement;
	}

	return install_internal(name, hook, hook_method::function_hook) && (!queue_enable || hook::apply_queued_actions());
}
bool reshade::hooks::install(const char *name, hook::address vtable[], unsigned int offset, hook::address replacement)
{
	assert(vtable != nullptr);
	assert(replacement != nullptr);

	DWORD protection = PAGE_READONLY;
	hook::address &target = vtable[offset];

	if (VirtualProtect(&target, sizeof(hook::address), protection, &protection))
	{ const std::lock_guard<std::mutex> lock(s_mutex_vtable_addresses);
		const auto insert = s_vtable_addresses.emplace(target, &target);

		VirtualProtect(&target, sizeof(hook::address), protection, &protection);

		if (insert.second)
		{
			hook hook { target, target, replacement };

			if (target != replacement && install_internal(name, hook, hook_method::vtable_hook))
			{
				return true;
			}

			s_vtable_addresses.erase(insert.first);
		}
		else
		{
			return insert.first->first == target;
		}
	}

	return false;
}
void reshade::hooks::uninstall()
{
	LOG(INFO) << "Uninstalling " << s_hooks.size() << " hook(s) ...";

	// Disable all hooks in a single batch job
	for (auto &hook_info : s_hooks)
	{
		const hook &hook = std::get<1>(hook_info);

		hook.enable(false);
	}

	hook::apply_queued_actions();

	// Afterwards remove all hooks from the list
	for (auto &hook_info : s_hooks)
	{
		uninstall_internal(std::get<0>(hook_info), std::get<1>(hook_info), std::get<2>(hook_info));
	}

	s_hooks.clear();
}
void reshade::hooks::register_module(const filesystem::path &target_path)
{
	install("LoadLibraryA", reinterpret_cast<hook::address>(&LoadLibraryA), reinterpret_cast<hook::address>(&HookLoadLibraryA), true);
	install("LoadLibraryExA", reinterpret_cast<hook::address>(&LoadLibraryExA), reinterpret_cast<hook::address>(&HookLoadLibraryExA), true);
	install("LoadLibraryW", reinterpret_cast<hook::address>(&LoadLibraryW), reinterpret_cast<hook::address>(&HookLoadLibraryW), true);
	install("LoadLibraryExW", reinterpret_cast<hook::address>(&LoadLibraryExW), reinterpret_cast<hook::address>(&HookLoadLibraryExW), true);

	hook::apply_queued_actions();

	LOG(INFO) << "Registering hooks for " << target_path << " ...";

	const auto target_filename = target_path.filename_without_extension();
	const auto replacement_filename = filesystem::get_module_path(g_module_handle).filename_without_extension();

	if (target_filename == replacement_filename)
	{
		LOG(INFO) << "> Delayed until first call to an exported function.";

		s_export_hook_path = target_path;
	}
	else
	{
		HMODULE handle = nullptr;

		if (is_module_loaded(target_path, handle))
		{
			LOG(INFO) << "> Libraries loaded.";

			install_internal(handle, g_module_handle, hook_method::function_hook);

			hook::apply_queued_actions();
		}
		else
		{
			LOG(INFO) << "> Delayed.";

			s_delayed_hook_paths.push_back(target_path);
		}
	}
}

reshade::hook::address reshade::hooks::call(hook::address replacement)
{
	const hook hook = find_internal(replacement);

	if (hook.valid())
	{
		return hook.call();
	}
	else if (!s_export_hook_path.empty())
	{
		const HMODULE handle = load_module(s_export_hook_path);

		LOG(INFO) << "Installing export hooks for " << s_export_hook_path << " ...";

		if (handle != nullptr)
		{
			install_internal(handle, g_module_handle, hook_method::export_hook);

			s_export_hook_path = "";

			return call(replacement);
		}
		else
		{
			LOG(ERROR) << "Failed to load " << s_export_hook_path << "!";
		}
	}

	LOG(ERROR) << "Unable to resolve hook for '0x" << replacement << "'!";

	return nullptr;
}
