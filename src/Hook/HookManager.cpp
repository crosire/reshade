#include "Log.hpp"
#include "HookManager.hpp"
#include "Utils\CriticalSection.hpp"

#include <vector>
#include <algorithm>
#include <unordered_map>
#include <boost\algorithm\string.hpp>
#include <assert.h>
#include <Windows.h>

namespace ReShade
{
	namespace Hooks
	{
		namespace
		{
			enum class HookType
			{
				Export,
				FunctionHook,
				VTableHook
			};
			struct ModuleExport
			{
				void *Address;
				const char *Name;
				unsigned short Ordinal;
			};

			inline HMODULE GetCurrentModuleHandle()
			{
				HMODULE handle = nullptr;
				GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, reinterpret_cast<LPCTSTR>(&GetCurrentModuleHandle), &handle);

				return handle;
			}
			std::vector<ModuleExport> GetModuleExports(HMODULE handle)
			{
				std::vector<ModuleExport> exports;
				BYTE *const imagebase = reinterpret_cast<BYTE *>(handle);
				IMAGE_NT_HEADERS *const imageheader = reinterpret_cast<IMAGE_NT_HEADERS *>(imagebase + reinterpret_cast<IMAGE_DOS_HEADER *>(imagebase)->e_lfanew);

				if (imageheader->Signature != IMAGE_NT_SIGNATURE || imageheader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].Size == 0)
				{
					return exports;
				}

				IMAGE_EXPORT_DIRECTORY *const exportdir = reinterpret_cast<IMAGE_EXPORT_DIRECTORY *>(imagebase + imageheader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress);
				const WORD exportbase = static_cast<WORD>(exportdir->Base);

				if (exportdir->NumberOfFunctions == 0)
				{
					return exports;
				}

				const std::size_t count = static_cast<std::size_t>(exportdir->NumberOfNames);
				exports.reserve(count);

				for (std::size_t i = 0; i < count; ++i)
				{
					ModuleExport symbol;
					symbol.Ordinal = reinterpret_cast<WORD *>(imagebase + exportdir->AddressOfNameOrdinals)[i] + exportbase;
					symbol.Name = reinterpret_cast<const char *>(imagebase + reinterpret_cast<DWORD *>(imagebase + exportdir->AddressOfNames)[i]);
					symbol.Address = reinterpret_cast<void *>(imagebase + reinterpret_cast<DWORD *>(imagebase + exportdir->AddressOfFunctions)[symbol.Ordinal - exportbase]);

					exports.push_back(std::move(symbol));
				}

				return exports;
			}
			inline boost::filesystem::path GetModuleFileName(HMODULE handle)
			{
				WCHAR path[MAX_PATH];
				GetModuleFileNameW(handle, path, MAX_PATH);

				return path;
			}

			Utils::CriticalSection sCS;
			boost::filesystem::path sExportHookPath;
			std::vector<HMODULE> sDelayedHookModules;
			std::vector<boost::filesystem::path> sDelayedHookPaths;
			std::vector<std::pair<Hook, HookType>> sHooks;
			std::unordered_map<Hook::Function, Hook::Function *> sVTableAddresses;
			const HMODULE sCurrentModuleHandle = GetCurrentModuleHandle();

			bool Install(Hook::Function target, Hook::Function replacement, HookType method)
			{
				LOG(TRACE) << "Installing hook for '0x" << target << "' with '0x" << replacement << "' using method " << static_cast<int>(method) << " ...";

				Hook hook(target, replacement);
				hook.Trampoline = target;

				Hook::Status status = Hook::Status::Unknown;

				switch (method)
				{
					case HookType::Export:
					{
						status = Hook::Status::Success;
						break;
					}
					case HookType::FunctionHook:
					{
						status = hook.Install();
						break;
					}
					case HookType::VTableHook:
					{
						DWORD protection = 0;
						Hook::Function *const targetAddress = sVTableAddresses.at(target);
		
						if (VirtualProtect(targetAddress, sizeof(Hook::Function), PAGE_READWRITE, &protection) != FALSE)
						{
							*targetAddress = replacement;

							VirtualProtect(targetAddress, sizeof(Hook::Function), protection, &protection);

							status = Hook::Status::Success;
						}
						else
						{
							status = Hook::Status::MemoryProtectionFailure;
						}
						break;
					}
				}

				if (status == Hook::Status::Success)
				{
					LOG(TRACE) << "> Succeeded.";

					Utils::CriticalSection::Lock lock(sCS);

					sHooks.emplace_back(std::move(hook), method);

					return true;
				}
				else
				{
					LOG(ERROR) << "Failed to install hook for '0x" << target << "' with status code " << static_cast<int>(status) << ".";

					return false;
				}
			}
			bool Install(const HMODULE targetModule, const HMODULE replacementModule, HookType method)
			{
				assert(targetModule != nullptr);
				assert(replacementModule != nullptr);

				// Load export tables
				const std::vector<ModuleExport> targetExports = GetModuleExports(targetModule);
				const std::vector<ModuleExport> replacementExports = GetModuleExports(replacementModule);

				if (targetExports.empty())
				{
					LOG(INFO) << "> Empty export table! Skipped.";

					return false;
				}

				std::size_t counter = 0;
				std::vector<std::pair<Hook::Function, Hook::Function>> matches;
				matches.reserve(replacementExports.size());

				LOG(TRACE) << "> Analyzing export table:";
				LOG(TRACE) << "  +--------------------+---------+----------------------------------------------------+";
				LOG(TRACE) << "  | Address            | Ordinal | Name                                               |";
				LOG(TRACE) << "  +--------------------+---------+----------------------------------------------------+";

				// Analyze export table
				for (const auto &symbol : targetExports)
				{
					if (symbol.Name == nullptr || symbol.Address == nullptr)
					{
						continue;
					}

					// Find appropriate replacement
					const auto it = std::find_if(replacementExports.cbegin(), replacementExports.cend(),
						[&symbol](const ModuleExport &it)
						{
							return boost::equals(it.Name, symbol.Name);
						});

					// Filter uninteresting functions
					if (it == replacementExports.cend() || (boost::equals(symbol.Name, "DXGIReportAdapterConfiguration") || boost::equals(symbol.Name, "DXGIDumpJournal")))
					{
						LOG(TRACE) << "  | 0x" << std::setw(16) << symbol.Address << " | " << std::setw(7) << symbol.Ordinal << " | " << std::setw(50) << symbol.Name << " |";
					}
					else
					{
						LOG(TRACE) << "  | 0x" << std::setw(16) << symbol.Address << " | " << std::setw(7) << symbol.Ordinal << " | " << std::setw(50) << symbol.Name << " | <";

						matches.push_back(std::make_pair(reinterpret_cast<Hook::Function>(symbol.Address), reinterpret_cast<Hook::Function>(it->Address)));
					}
				}

				LOG(TRACE) << "  +--------------------+---------+----------------------------------------------------+";
				LOG(INFO) << "> Found " << matches.size() << " match(es). Installing ...";

				// Hook matching exports
				for (const auto &match : matches)
				{
					if (Install(match.first, match.second, method))
					{
						counter++;
					}
				}

				if (counter != 0)
				{
					LOG(INFO) << "> Installed " << counter << " hook(s).";

					return true;
				}
				else
				{
					LOG(WARNING) << "> Installed 0 hook(s).";

					return false;
				}
			}
			bool Uninstall(Hook &hook, HookType method)
			{
				LOG(TRACE) << "Uninstalling hook for '0x" << hook.Target << "' ...";

				if (!hook.IsInstalled())
				{
					LOG(TRACE) << "> Already uninstalled.";

					return true;
				}
				else if (method == HookType::Export)
				{
					LOG(TRACE) << "> Skipped.";

					return true;
				}

				Hook::Status status = Hook::Status::Unknown;

				switch (method)
				{
					case HookType::FunctionHook:
					{
						status = hook.Uninstall();
						break;
					}
					case HookType::VTableHook:
					{
						DWORD protection = 0;
						Hook::Function *const targetAddress = sVTableAddresses.at(hook.Target);
		
						if (VirtualProtect(reinterpret_cast<LPVOID *>(targetAddress), sizeof(Hook::Function), PAGE_READWRITE, &protection) != FALSE)
						{
							*targetAddress = hook.Target;
							sVTableAddresses.erase(hook.Target);

							VirtualProtect(reinterpret_cast<LPVOID *>(targetAddress), sizeof(Hook::Function), protection, &protection);

							status = Hook::Status::Success;
						}
						else
						{
							status = Hook::Status::MemoryProtectionFailure;
						}
						break;
					}
				}

				if (status == Hook::Status::Success)
				{
					LOG(TRACE) << "> Succeeded.";

					hook.Trampoline = nullptr;

					return true;
				}
				else
				{
					LOG(WARNING) << "Failed to uninstall hook for '0x" << hook.Target << "' with status code " << static_cast<int>(status) << ".";

					return false;
				}
			}

			Hook Find(Hook::Function replacement)
			{
				Utils::CriticalSection::Lock lock(sCS);

				const auto it =	std::find_if(sHooks.cbegin(), sHooks.cend(),
					[replacement](const std::pair<Hook, HookType> &it)
					{
						return it.first.Replacement == replacement;
					});

				if (it == sHooks.cend())
				{
					return Hook();
				}

				return it->first;
			}
			template <typename F>
			inline F CallUnchecked(F replacement)
			{
				return reinterpret_cast<F>(Find(reinterpret_cast<Hook::Function>(replacement)).Call());
			}

			HMODULE WINAPI HookLoadLibraryA(LPCSTR lpFileName)
			{
				static const auto trampoline = CallUnchecked(&HookLoadLibraryA);

				const HMODULE handle = trampoline(lpFileName);

				if (handle == nullptr || handle == sCurrentModuleHandle)
				{
					return handle;
				}

				Utils::CriticalSection::Lock lock(sCS);

				const auto remove = std::remove_if(sDelayedHookPaths.begin(), sDelayedHookPaths.end(),
					[lpFileName](const boost::filesystem::path &it)
					{
						HMODULE handle = nullptr;
						GetModuleHandleExW(0, it.c_str(), &handle);

						if (handle == nullptr)
						{
							return false;
						}

						LOG(INFO) << "Installing delayed hooks for " << it << " (Just loaded via 'LoadLibraryA(\"" << lpFileName << "\")') ...";

						sDelayedHookModules.push_back(handle);

						return Install(handle, sCurrentModuleHandle, HookType::FunctionHook);
					});

				sDelayedHookPaths.erase(remove, sDelayedHookPaths.end());

				return handle;
			}
			HMODULE WINAPI HookLoadLibraryExA(LPCSTR lpFileName, HANDLE hFile, DWORD dwFlags)
			{
				if (dwFlags == 0)
				{
					return HookLoadLibraryA(lpFileName);
				}

				static const auto trampoline = CallUnchecked(&HookLoadLibraryExA);

				return trampoline(lpFileName, hFile, dwFlags);
			}
			HMODULE WINAPI HookLoadLibraryW(LPCWSTR lpFileName)
			{
				static const auto trampoline = CallUnchecked(&HookLoadLibraryW);

				const HMODULE handle = trampoline(lpFileName);

				if (handle == nullptr || handle == sCurrentModuleHandle)
				{
					return handle;
				}

				Utils::CriticalSection::Lock lock(sCS);

				const auto remove = std::remove_if(sDelayedHookPaths.begin(), sDelayedHookPaths.end(),
					[lpFileName](const boost::filesystem::path &it)
					{
						HMODULE handle = nullptr;
						GetModuleHandleExW(0, it.c_str(), &handle);

						if (handle == nullptr)
						{
							return false;
						}

						LOG(INFO) << "Installing delayed hooks for " << it << " (Just loaded via 'LoadLibraryW(\"" << lpFileName << "\")') ...";

						sDelayedHookModules.push_back(handle);

						return Install(handle, sCurrentModuleHandle, HookType::FunctionHook);
					});

				sDelayedHookPaths.erase(remove, sDelayedHookPaths.end());

				return handle;
			}
			HMODULE WINAPI HookLoadLibraryExW(LPCWSTR lpFileName, HANDLE hFile, DWORD dwFlags)
			{
				if (dwFlags == 0)
				{
					return HookLoadLibraryW(lpFileName);
				}

				static const auto trampoline = CallUnchecked(&HookLoadLibraryExW);

				return trampoline(lpFileName, hFile, dwFlags);
			}
		}

		bool Install(Hook::Function target, Hook::Function replacement)
		{
			assert(target != nullptr);
			assert(replacement != nullptr);

			if (target == replacement)
			{
				return false;
			}

			const Hook hook = Find(replacement);

			if (hook.IsInstalled())
			{
				return target == hook.Target;
			}

			return Install(target, replacement, HookType::FunctionHook);
		}
		bool Install(Hook::Function vtable[], unsigned int offset, Hook::Function replacement)
		{
			assert(vtable != nullptr);
			assert(replacement != nullptr);

			DWORD protection = 0;
			Hook::Function &target = vtable[offset];

			if (VirtualProtect(&target, sizeof(Hook::Function), PAGE_READONLY, &protection) != FALSE)
			{
				Utils::CriticalSection::Lock lock(sCS);

				const auto insert = sVTableAddresses.emplace(target, &target);

				VirtualProtect(&target, sizeof(Hook::Function), protection, &protection);

				if (insert.second)
				{
					if (target != replacement && Install(target, replacement, HookType::VTableHook))
					{
						return true;
					}
					else
					{
						sVTableAddresses.erase(insert.first);
					}
				}
				else
				{
					return insert.first->first == target;
				}
			}

			return false;
		}
		void Uninstall()
		{
			Utils::CriticalSection::Lock lock(sCS);

			LOG(INFO) << "Uninstalling " << sHooks.size() << " hook(s) ...";

			// Uninstall hooks
			for (auto &it : sHooks)
			{
				Uninstall(it.first, it.second);
			}

			sHooks.clear();

			// Free loaded modules
			for (HMODULE module : sDelayedHookModules)
			{
				FreeLibrary(module);
			}

			sDelayedHookModules.clear();
		}
		void RegisterModule(const boost::filesystem::path &targetPath) // Unsafe
		{
			const boost::filesystem::path replacementPath = GetModuleFileName(sCurrentModuleHandle);

			Install(reinterpret_cast<Hook::Function>(&LoadLibraryA), reinterpret_cast<Hook::Function>(&HookLoadLibraryA));
			Install(reinterpret_cast<Hook::Function>(&LoadLibraryExA), reinterpret_cast<Hook::Function>(&HookLoadLibraryExA));
			Install(reinterpret_cast<Hook::Function>(&LoadLibraryW), reinterpret_cast<Hook::Function>(&HookLoadLibraryW));
			Install(reinterpret_cast<Hook::Function>(&LoadLibraryExW), reinterpret_cast<Hook::Function>(&HookLoadLibraryExW));

			LOG(INFO) << "Registering hooks for " << targetPath << " ...";

			if (boost::iequals(targetPath.stem().native(), replacementPath.stem().native()))
			{
				LOG(INFO) << "> Delayed.";

				sExportHookPath = targetPath;
			}
			else
			{
				HMODULE targetModule = nullptr;
				GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_PIN, targetPath.c_str(), &targetModule);

				if (targetModule != nullptr)
				{
					LOG(INFO) << "> Libraries loaded.";

					sDelayedHookModules.push_back(targetModule);

					Install(targetModule, sCurrentModuleHandle, HookType::FunctionHook);
				}
				else
				{
					LOG(INFO) << "> Delayed.";

					sDelayedHookPaths.push_back(targetPath);
				}
			}
		}

		Hook::Function Call(Hook::Function replacement)
		{
			Utils::CriticalSection::Lock lock(sCS);

			if (!sExportHookPath.empty())
			{
				const HMODULE handle = HookLoadLibraryW(sExportHookPath.c_str());

				LOG(INFO) << "Installing delayed hooks for " << sExportHookPath << " ...";

				if (handle != nullptr)
				{
					sExportHookPath.clear();
					sDelayedHookModules.push_back(handle);

					Install(handle, sCurrentModuleHandle, HookType::Export);
				}
				else
				{
					LOG(ERROR) << "Failed to load " << sExportHookPath << "!";
				}
			}

			const Hook hook = Find(replacement);

			if (!hook.IsValid())
			{
				LOG(ERROR) << "Unable to resolve hook for '0x" << replacement << "'!";

				return nullptr;
			}

			return hook.Call();
		}
	}
}
