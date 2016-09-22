#include "log.hpp"
#include "hook_manager.hpp"
#include "critical_section.hpp"
#include <assert.h>
#include <algorithm>
#include <vector>
#include <unordered_map>
#include <Windows.h>

extern HMODULE g_module_handle;

namespace reshade::hooks
{
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
			hook::address address;
			const char *name;
			unsigned short ordinal;
		};

		std::vector<module_export> get_module_exports(HMODULE handle)
		{
			std::vector<module_export> exports;
			const auto imagebase = reinterpret_cast<const BYTE *>(handle);
			const auto imageheader = reinterpret_cast<const IMAGE_NT_HEADERS *>(imagebase + reinterpret_cast<const IMAGE_DOS_HEADER *>(imagebase)->e_lfanew);

			if (imageheader->Signature != IMAGE_NT_SIGNATURE || imageheader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].Size == 0)
			{
				return exports;
			}

			const auto exportdir = reinterpret_cast<const IMAGE_EXPORT_DIRECTORY *>(imagebase + imageheader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress);
			const auto exportbase = static_cast<WORD>(exportdir->Base);

			if (exportdir->NumberOfFunctions == 0)
			{
				return exports;
			}

			const auto count = static_cast<size_t>(exportdir->NumberOfNames);
			exports.reserve(count);

			for (size_t i = 0; i < count; i++)
			{
				module_export symbol;
				symbol.ordinal = reinterpret_cast<const WORD *>(imagebase + exportdir->AddressOfNameOrdinals)[i] + exportbase;
				symbol.name = reinterpret_cast<const char *>(imagebase + reinterpret_cast<const DWORD *>(imagebase + exportdir->AddressOfNames)[i]);
				symbol.address = const_cast<void *>(reinterpret_cast<const void *>(imagebase + reinterpret_cast<const DWORD *>(imagebase + exportdir->AddressOfFunctions)[symbol.ordinal - exportbase]));

				exports.push_back(std::move(symbol));
			}

			return exports;
		}

		critical_section s_cs;
		filesystem::path s_export_hook_path;
		std::vector<filesystem::path> s_delayed_hook_paths;
		std::vector<HMODULE> s_delayed_hook_modules;
		std::vector<std::pair<hook, hook_method>> s_hooks;
		std::unordered_map<hook::address, hook::address *> s_vtable_addresses;

		bool install(hook::address target, hook::address replacement, hook_method method)
		{
			LOG(INFO) << "Installing hook for '0x" << target << "' with '0x" << replacement << "' using method " << static_cast<int>(method) << " ...";

			hook hook(target, replacement);
			hook.trampoline = target;

			hook::status status = hook::status::unknown;

			switch (method)
			{
				case hook_method::export_hook:
				{
					status = hook::status::success;
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
					const auto target_address = s_vtable_addresses.at(target);

					if (VirtualProtect(target_address, sizeof(*target_address), protection, &protection))
					{
						*target_address = replacement;

						VirtualProtect(target_address, sizeof(*target_address), protection, &protection);

						status = hook::status::success;
					}
					else
					{
						status = hook::status::memory_protection_failure;
					}
					break;
				}
			}

			if (status != hook::status::success)
			{
				LOG(ERROR) << "Failed to install hook for '0x" << target << "' with status code " << static_cast<int>(status) << ".";

				return false;
			}

			LOG(INFO) << "> Succeeded.";

			const critical_section::lock lock(s_cs);

			s_hooks.emplace_back(std::move(hook), method);

			return true;
		}
		bool install(const HMODULE target_module, const HMODULE replacement_module, hook_method method)
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
			std::vector<std::pair<hook::address, hook::address>> matches;
			matches.reserve(replacement_exports.size());

			LOG(INFO) << "> Dumping matches in export table:";
			LOG(INFO) << "  +--------------------+---------+----------------------------------------------------+";
			LOG(INFO) << "  | Address            | Ordinal | Name                                               |";
			LOG(INFO) << "  +--------------------+---------+----------------------------------------------------+";

			// Analyze export table
			for (const auto &symbol : target_exports)
			{
				if (symbol.name == nullptr || symbol.address == nullptr)
				{
					continue;
				}

				// Find appropriate replacement
				const auto it = std::find_if(replacement_exports.cbegin(), replacement_exports.cend(),
					[&symbol](const module_export &moduleexport) {
						return std::strcmp(moduleexport.name, symbol.name) == 0;
					});

				// Filter uninteresting functions
				if (it != replacement_exports.cend() &&
					std::strcmp(symbol.name, "DXGIReportAdapterConfiguration") != 0 &&
					std::strcmp(symbol.name, "DXGIDumpJournal") != 0)
				{
					LOG(INFO) << "  | 0x" << std::setw(16) << symbol.address << " | " << std::setw(7) << symbol.ordinal << " | " << std::setw(50) << symbol.name << " |";

					matches.push_back({ symbol.address, it->address });
				}
			}

			LOG(INFO) << "  +--------------------+---------+----------------------------------------------------+";
			LOG(INFO) << "> Found " << matches.size() << " match(es). Installing ...";

			// Hook matching exports
			for (const auto &match : matches)
			{
				if (install(match.first, match.second, method))
				{
					install_count++;
				}
			}

			return install_count != 0;
		}
		bool uninstall(hook &hook, hook_method method)
		{
			LOG(INFO) << "Uninstalling hook for '0x" << hook.target << "' ...";

			if (hook.uninstalled())
			{
				LOG(WARNING) << "> Already uninstalled.";

				return true;
			}

			hook::status status = hook::status::unknown;

			switch (method)
			{
				case hook_method::export_hook:
				{
					LOG(INFO) << "> Skipped.";

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

						status = hook::status::success;
					}
					else
					{
						status = hook::status::memory_protection_failure;
					}
					break;
				}
			}

			if (status != hook::status::success)
			{
				LOG(WARNING) << "Failed to uninstall hook for '0x" << hook.target << "' with status code " << static_cast<int>(status) << ".";

				return false;
			}

			LOG(INFO) << "> Succeeded.";

			hook.trampoline = nullptr;

			return true;
		}

		hook find(hook::address replacement)
		{
			const critical_section::lock lock(s_cs);

			const auto it = std::find_if(s_hooks.cbegin(), s_hooks.cend(),
				[replacement](const std::pair<hook, hook_method> &hook)
			{
				return hook.first.replacement == replacement;
			});

			if (it == s_hooks.cend())
			{
				return hook();
			}

			return it->first;
		}
		template <typename T>
		inline T call_unchecked(T replacement)
		{
			return reinterpret_cast<T>(find(reinterpret_cast<hook::address>(replacement)).call());
		}

		HMODULE WINAPI HookLoadLibraryA(LPCSTR lpFileName)
		{
			static const auto trampoline = call_unchecked(&HookLoadLibraryA);

			const HMODULE handle = trampoline(lpFileName);

			if (handle == nullptr || handle == g_module_handle)
			{
				return handle;
			}

			const critical_section::lock lock(s_cs);

			const auto remove = std::remove_if(s_delayed_hook_paths.begin(), s_delayed_hook_paths.end(),
				[lpFileName](const filesystem::path &path) {
					HMODULE delayed_handle = nullptr;
					GetModuleHandleExW(0, path.wstring().c_str(), &delayed_handle);

					if (delayed_handle == nullptr)
					{
						return false;
					}

					LOG(INFO) << "Installing delayed hooks for " << path << " (Just loaded via 'LoadLibraryA(\"" << lpFileName << "\")') ...";

					s_delayed_hook_modules.push_back(delayed_handle);

					return install(delayed_handle, g_module_handle, hook_method::function_hook);
				});

			s_delayed_hook_paths.erase(remove, s_delayed_hook_paths.end());

			return handle;
		}
		HMODULE WINAPI HookLoadLibraryExA(LPCSTR lpFileName, HANDLE hFile, DWORD dwFlags)
		{
			if (dwFlags == 0)
			{
				return HookLoadLibraryA(lpFileName);
			}

			static const auto trampoline = call_unchecked(&HookLoadLibraryExA);

			return trampoline(lpFileName, hFile, dwFlags);
		}
		HMODULE WINAPI HookLoadLibraryW(LPCWSTR lpFileName)
		{
			static const auto trampoline = call_unchecked(&HookLoadLibraryW);

			const HMODULE handle = trampoline(lpFileName);

			if (handle == nullptr || handle == g_module_handle)
			{
				return handle;
			}

			const critical_section::lock lock(s_cs);

			const auto remove = std::remove_if(s_delayed_hook_paths.begin(), s_delayed_hook_paths.end(),
				[lpFileName](const filesystem::path &path) {
					HMODULE delayed_handle = nullptr;
					GetModuleHandleExW(0, path.wstring().c_str(), &delayed_handle);

					if (delayed_handle == nullptr)
					{
						return false;
					}

					LOG(INFO) << "Installing delayed hooks for " << path << " (Just loaded via 'LoadLibraryW(\"" << lpFileName << "\")') ...";

					s_delayed_hook_modules.push_back(delayed_handle);

					return install(delayed_handle, g_module_handle, hook_method::function_hook);
				});

			s_delayed_hook_paths.erase(remove, s_delayed_hook_paths.end());

			return handle;
		}
		HMODULE WINAPI HookLoadLibraryExW(LPCWSTR lpFileName, HANDLE hFile, DWORD dwFlags)
		{
			if (dwFlags == 0)
			{
				return HookLoadLibraryW(lpFileName);
			}

			static const auto trampoline = call_unchecked(&HookLoadLibraryExW);

			return trampoline(lpFileName, hFile, dwFlags);
		}
	}

	bool install(hook::address target, hook::address replacement)
	{
		assert(target != nullptr);
		assert(replacement != nullptr);

		if (target == replacement)
		{
			return false;
		}

		const hook hook = find(replacement);

		if (hook.installed())
		{
			return target == hook.target;
		}

		return install(target, replacement, hook_method::function_hook);
	}
	bool install(hook::address vtable[], unsigned int offset, hook::address replacement)
	{
		assert(vtable != nullptr);
		assert(replacement != nullptr);

		DWORD protection = PAGE_READONLY;
		hook::address &target = vtable[offset];

		if (VirtualProtect(&target, sizeof(hook::address), protection, &protection))
		{
			const critical_section::lock lock(s_cs);

			const auto insert = s_vtable_addresses.emplace(target, &target);

			VirtualProtect(&target, sizeof(hook::address), protection, &protection);

			if (insert.second)
			{
				if (target != replacement && install(target, replacement, hook_method::vtable_hook))
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
	void uninstall()
	{
		const critical_section::lock lock(s_cs);

		LOG(INFO) << "Uninstalling " << s_hooks.size() << " hook(s) ...";

		// Uninstall hooks
		for (auto &hook : s_hooks)
		{
			uninstall(hook.first, hook.second);
		}

		s_hooks.clear();

		// Free loaded modules
		for (HMODULE module : s_delayed_hook_modules)
		{
			FreeLibrary(module);
		}

		s_delayed_hook_modules.clear();
	}
	void register_module(const filesystem::path &target_path) // Unsafe
	{
		install(reinterpret_cast<hook::address>(&LoadLibraryA), reinterpret_cast<hook::address>(&HookLoadLibraryA));
		install(reinterpret_cast<hook::address>(&LoadLibraryExA), reinterpret_cast<hook::address>(&HookLoadLibraryExA));
		install(reinterpret_cast<hook::address>(&LoadLibraryW), reinterpret_cast<hook::address>(&HookLoadLibraryW));
		install(reinterpret_cast<hook::address>(&LoadLibraryExW), reinterpret_cast<hook::address>(&HookLoadLibraryExW));

		LOG(INFO) << "Registering hooks for " << target_path << " ...";

		const auto target_filename = target_path.filename_without_extension();
		const auto replacement_filename = filesystem::get_module_path(g_module_handle).filename_without_extension();

		if (target_filename == replacement_filename)
		{
			LOG(INFO) << "> Delayed.";

			s_export_hook_path = target_path;
		}
		else
		{
			HMODULE handle = nullptr;
			GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_PIN, target_path.wstring().c_str(), &handle);

			if (handle != nullptr)
			{
				LOG(INFO) << "> Libraries loaded.";

				s_delayed_hook_modules.push_back(handle);

				install(handle, g_module_handle, hook_method::function_hook);
			}
			else
			{
				LOG(INFO) << "> Delayed.";

				s_delayed_hook_paths.push_back(target_path);
			}
		}
	}

	hook::address call(hook::address replacement)
	{
		const critical_section::lock lock(s_cs);

		if (!s_export_hook_path.empty())
		{
			const HMODULE handle = HookLoadLibraryW(s_export_hook_path.wstring().c_str());

			LOG(INFO) << "Installing delayed hooks for " << s_export_hook_path << " ...";

			if (handle != nullptr)
			{
				s_export_hook_path = "";
				s_delayed_hook_modules.push_back(handle);

				install(handle, g_module_handle, hook_method::export_hook);
			}
			else
			{
				LOG(ERROR) << "Failed to load " << s_export_hook_path << "!";
			}
		}

		const hook hook = find(replacement);

		if (!hook.valid())
		{
			LOG(ERROR) << "Unable to resolve hook for '0x" << replacement << "'!";

			return nullptr;
		}

		return hook.call();
	}
}
