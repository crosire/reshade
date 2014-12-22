#include "Log.hpp"
#include "HookManager.hpp"

#include <vector>
#include <unordered_map>
#include <windows.h>
#include <boost\algorithm\string.hpp>

namespace ReShade { namespace Hooks
{
	bool Install(const boost::filesystem::path &targetPath);
	bool Install(Hook::Function target, const Hook::Function replacement, HookType type);

	namespace
	{
		class Module
		{
		public:
			typedef HMODULE Handle;
					
		public:
			struct Export
			{
				void *Address;
				const char *Name;
				unsigned short Ordinal;
			};
			struct Import
			{
				const char *Library;
				const char *Name;
				unsigned short Ordinal;
			};

		public:
			static Handle Load(const boost::filesystem::path &path)
			{
				return ::LoadLibrary(path.c_str());
			}
			static Handle Loaded(const boost::filesystem::path &path)
			{
				return ::GetModuleHandle(path.c_str());
			}
			static Module Current()
			{
				HMODULE handle = nullptr;
				::GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, reinterpret_cast<LPCTSTR>(&Current), &handle);
					
				return handle;
			}
			static void Free(Handle handle)
			{
				assert(handle != nullptr);

				::FreeLibrary(handle);
			}

		public:
			inline Module() : mHandle(nullptr)
			{
			}
			inline Module(Handle handle) : mHandle(handle)
			{
			}

			inline operator Handle() const
			{
				return this->mHandle;
			}

			boost::filesystem::path GetPath() const
			{
				assert(this->mHandle != nullptr);

				TCHAR path[MAX_PATH];
				::GetModuleFileName(this->mHandle, path, MAX_PATH);

				return path;
			}
			std::vector<Export> GetExports() const
			{
				assert(this->mHandle != nullptr);

				std::vector<Export> exports;
				BYTE *imageBase = reinterpret_cast<BYTE *>(this->mHandle);
				IMAGE_DOS_HEADER *imageHeader = reinterpret_cast<IMAGE_DOS_HEADER *>(imageBase);
				IMAGE_NT_HEADERS *imageHeaderNT = reinterpret_cast<IMAGE_NT_HEADERS *>(imageBase + imageHeader->e_lfanew);

				if (imageHeader->e_magic != IMAGE_DOS_SIGNATURE || imageHeaderNT->Signature != IMAGE_NT_SIGNATURE || imageHeaderNT->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].Size == 0)
				{
					return exports;
				}

				IMAGE_EXPORT_DIRECTORY *imageExports = reinterpret_cast<IMAGE_EXPORT_DIRECTORY *>(imageBase + imageHeaderNT->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress);

				const std::size_t count = static_cast<std::size_t>(imageExports->NumberOfNames);
				exports.reserve(count);

				for (std::size_t i = 0; i < count; ++i)
				{
					Export symbol;
					symbol.Ordinal = reinterpret_cast<WORD *>(imageBase + imageExports->AddressOfNameOrdinals)[i] + static_cast<WORD>(imageExports->Base);
					symbol.Name = reinterpret_cast<const char *>(imageBase + (reinterpret_cast<DWORD *>(imageBase + imageExports->AddressOfNames)[i]));

					if (imageExports->NumberOfFunctions > 0)
					{
						symbol.Address = reinterpret_cast<void *>(imageBase + (reinterpret_cast<DWORD *>(imageBase + imageExports->AddressOfFunctions)[symbol.Ordinal - static_cast<WORD>(imageExports->Base)]));
					}
					else
					{
						symbol.Address = nullptr;
					}

					exports.push_back(symbol);
				}

				return exports;
			}
			std::vector<Import> GetImports() const
			{
				assert(this->mHandle != nullptr);

				std::vector<Import> imports;
				BYTE *imageBase = reinterpret_cast<BYTE *>(this->mHandle);
				IMAGE_DOS_HEADER *imageHeader = reinterpret_cast<IMAGE_DOS_HEADER *>(imageBase);
				IMAGE_NT_HEADERS *imageHeaderNT = reinterpret_cast<IMAGE_NT_HEADERS *>(imageBase + imageHeader->e_lfanew);

				if (imageHeader->e_magic != IMAGE_DOS_SIGNATURE || imageHeaderNT->Signature != IMAGE_NT_SIGNATURE || imageHeaderNT->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].Size == 0)
				{
					return imports;
				}

				std::size_t i = 0;
				IMAGE_IMPORT_DESCRIPTOR *imageImports = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR *>(imageBase + imageHeaderNT->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);

				while (imageImports->Name != 0)
				{
					IMAGE_THUNK_DATA *INT = reinterpret_cast<IMAGE_THUNK_DATA *>(imageBase + imageImports->OriginalFirstThunk);

					while (INT->u1.Function != 0)
					{
						Import symbol;
						symbol.Library = reinterpret_cast<const char *>(imageBase + imageImports->Name);

						if ((INT->u1.Ordinal & IMAGE_ORDINAL_FLAG) != IMAGE_ORDINAL_FLAG)
						{
							IMAGE_IMPORT_BY_NAME *import = reinterpret_cast<IMAGE_IMPORT_BY_NAME *>(imageBase + INT->u1.AddressOfData);
					
							symbol.Ordinal = import->Hint;
							symbol.Name = import->Name;
						}
						else
						{
							symbol.Ordinal = LOWORD(INT->u1.Ordinal);
							symbol.Name = nullptr;
						}

						imports.push_back(symbol);

						++INT;
						++i;
					}

					++imageImports;
				}

				return imports;
			}

			inline void Free()
			{
				Free(*this);

				this->mHandle = nullptr;
			}

		private:
			Handle mHandle;
		};
		class CriticalSection
		{
		public:
			struct Lock
			{
				inline Lock(CriticalSection &cs) : CS(cs)
				{
					this->CS.Enter();
				}
				inline ~Lock()
				{
					this->CS.Leave();
				}

				CriticalSection &CS;

			private:
				void operator =(const Lock &);
			};

		public:
			inline CriticalSection()
			{
				::InitializeCriticalSection(&this->mCS);
			}
			inline ~CriticalSection()
			{
				::DeleteCriticalSection(&this->mCS);
			}

			inline void Enter()
			{
				::EnterCriticalSection(&this->mCS);
			}
			inline void Leave()
			{
				::LeaveCriticalSection(&this->mCS);
			}

		private:
			CRITICAL_SECTION mCS;
		};

		CriticalSection sCS;
		std::vector<std::pair<Hook, HookType>> sHooks;
		std::vector<boost::filesystem::path> sHooksDelayed;
		std::unordered_map<Hook::Function, Hook::Function *> sVTableAddresses;
		std::vector<Module::Handle> sModules;

		HMODULE WINAPI HookLoadLibraryA(LPCSTR lpFileName)
		{
			const HMODULE handle = Call(&HookLoadLibraryA)(lpFileName);

			if (handle == nullptr)
			{
				return nullptr;
			}

			CriticalSection::Lock lock(sCS);
			const boost::filesystem::path path = lpFileName;

			for (auto it = sHooksDelayed.begin(), end = sHooksDelayed.end(); it != end; ++it)
			{
				if (!boost::iequals(it->stem().native(), path.stem().native()))
				{
					continue;
				}

				LOG(INFO) << "Installing delayed hooks for " << *it << " (Just loaded via 'LoadLibraryA(\"" << lpFileName << "\")') ...";

				if (Install(*it))
				{
					sHooksDelayed.erase(it);
					break;
				}
			}

			return handle;
		}
		HMODULE WINAPI HookLoadLibraryW(LPCWSTR lpFileName)
		{
			const HMODULE handle = Call(&HookLoadLibraryW)(lpFileName);

			if (handle == nullptr)
			{
				return nullptr;
			}

			CriticalSection::Lock lock(sCS);
			const boost::filesystem::path path = lpFileName;

			for (auto it = sHooksDelayed.begin(), end = sHooksDelayed.end(); it != end; ++it)
			{
				if (!boost::iequals(it->stem().native(), path.stem().native()))
				{
					continue;
				}

				LOG(INFO) << "Installing delayed hooks for " << *it << " (Just loaded via 'LoadLibraryW(\"" << lpFileName << "\")') ...";

				if (Install(*it))
				{
					sHooksDelayed.erase(it);
					break;
				}
			}

			return handle;
		}
	}

	// -----------------------------------------------------------------------------------------------------

	bool Install(const boost::filesystem::path &targetPath)
	{
		struct ScopeGuard
		{
			ScopeGuard()
			{
				Hook hook;

				hook = Find(reinterpret_cast<Hook::Function>(&HookLoadLibraryA));

				if (hook.IsInstalled())
				{
					hook.Enable(false);
				}

				hook = Find(reinterpret_cast<Hook::Function>(&HookLoadLibraryW));

				if (hook.IsInstalled())
				{
					hook.Enable(false);
				}
			}
			~ScopeGuard()
			{
				Hook hook;

				hook = Find(reinterpret_cast<Hook::Function>(&HookLoadLibraryA));

				if (hook.IsInstalled())
				{
					hook.Enable(true);
				}

				hook = Find(reinterpret_cast<Hook::Function>(&HookLoadLibraryW));

				if (hook.IsInstalled())
				{
					hook.Enable(true);
				}
			}
		} guard;

		// Load modules
		const Module targetModule = Module::Load(targetPath);
		const Module replacementModule = Module::Current();
		const boost::filesystem::path replacementPath = replacementModule.GetPath();

		if (!targetModule)
		{
			LOG(ERROR) << "> Failed to load " << targetPath << "! Skipped.";

			return false;
		}
		else
		{
			CriticalSection::Lock lock(sCS);

			sModules.push_back(targetModule);
		}
		
		LOG(INFO) << "> Libraries loaded.";

		std::size_t counter = 0;
		const bool detour = !boost::iequals(targetPath.stem().native(), replacementPath.stem().native());

		// Load export tables
		const std::vector<Module::Export> targetExports = targetModule.GetExports();
		const std::vector<Module::Export> replacementExports = replacementModule.GetExports();

		if (targetExports.empty())
		{
			LOG(INFO) << "> Empty export table! Skipped.";

			return false;
		}

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

			char line[88];
			sprintf_s(line, "  | 0x%016IX | %7hu | %-50.50s |", reinterpret_cast<uintptr_t>(symbol.Address), symbol.Ordinal, symbol.Name);

			// Find appropriate replacement
			const auto begin = replacementExports.cbegin(), end = replacementExports.cend(), it = std::find_if(begin, end, [&symbol](const Module::Export &it) { return boost::equals(it.Name, symbol.Name); });

			// Filter uninteresting functions
			if (it == end || (boost::starts_with(symbol.Name, "D3DKMT") || boost::starts_with(symbol.Name, "DXGID3D10") || boost::equals(symbol.Name, "DXGIReportAdapterConfiguration") || boost::equals(symbol.Name, "DXGIDumpJournal") || boost::starts_with(symbol.Name, "OpenAdapter10")))
			{
				LOG(TRACE) << line;
			}
			else
			{
				LOG(TRACE) << line << " <";

				matches.push_back(std::make_pair(reinterpret_cast<Hook::Function>(symbol.Address), reinterpret_cast<Hook::Function>(it->Address)));
			}
		}

		LOG(TRACE) << "  +--------------------+---------+----------------------------------------------------+";
		LOG(INFO) << "> Found " << matches.size() << " match(es). Installing ...";

		// Hook matching exports
		for (const auto &match : matches)
		{
			if (Install(match.first, match.second, detour ? HookType::FunctionHook : HookType::Export))
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
	bool Install(Hook::Function target, const Hook::Function replacement, HookType method)
	{
		LOG(TRACE) << "Installing hook for '0x" << target << "' with '0x" << replacement << "' using method " << static_cast<int>(method) << " ...";

		Hook hook(target, replacement);
		hook.Trampoline = target;

		bool success = false;

		switch (method)
		{
			case HookType::Export:
				success = true;
				break;
			case HookType::FunctionHook:
				success = Hook::Install(hook);
				break;
			case HookType::VTableHook:
			{
				DWORD protection = 0;
				Hook::Function *const targetAddress = sVTableAddresses.at(target);
		
				if (::VirtualProtect(reinterpret_cast<LPVOID *>(targetAddress), sizeof(Hook::Function), PAGE_READWRITE, &protection) != FALSE)
				{
					*targetAddress = replacement;

					::VirtualProtect(reinterpret_cast<LPVOID *>(targetAddress), sizeof(Hook::Function), protection, &protection);

					success = true;
				}
				break;
			}
		}

		if (success)
		{
			LOG(TRACE) << "> Succeeded.";

			CriticalSection::Lock lock(sCS);

			sHooks.emplace_back(std::move(hook), method);

			return true;
		}
		else
		{
			LOG(ERROR) << "Failed to install hook for '0x" << target << "'.";

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

		bool success = false;

		switch (method)
		{
			case HookType::Export:
				success = true;
				break;
			case HookType::FunctionHook:
				success = hook.Uninstall();
				break;
			case HookType::VTableHook:
			{
				DWORD protection = 0;
				Hook::Function *const targetAddress = sVTableAddresses.at(hook.Target);
		
				if (::VirtualProtect(reinterpret_cast<LPVOID *>(targetAddress), sizeof(Hook::Function), PAGE_READWRITE, &protection) != FALSE)
				{
					*targetAddress = hook.Target;
					sVTableAddresses.erase(hook.Target);

					::VirtualProtect(reinterpret_cast<LPVOID *>(targetAddress), sizeof(Hook::Function), protection, &protection);

					success = true;
				}
				break;
			}
		}

		if (success)
		{
			LOG(TRACE) << "> Succeeded.";

			hook.Trampoline = nullptr;

			return true;
		}
		else
		{
			LOG(WARNING) << "Failed to uninstall hook for '0x" << hook.Target << "'.";

			return false;
		}
	}

	void Register(const boost::filesystem::path &targetPath)
	{
		Register(reinterpret_cast<Hook::Function>(&::LoadLibraryA), reinterpret_cast<Hook::Function>(&HookLoadLibraryA));
		Register(reinterpret_cast<Hook::Function>(&::LoadLibraryW), reinterpret_cast<Hook::Function>(&HookLoadLibraryW));

		const boost::filesystem::path replacementPath = Module::Current().GetPath();

		LOG(INFO) << "Registering hooks for " << targetPath << " ...";
		
		if (!Module::Loaded(targetPath) && !boost::iequals(targetPath.stem().native(), replacementPath.stem().native()))
		{
			LOG(INFO) << "> Delayed.";

			CriticalSection::Lock lock(sCS);

			sHooksDelayed.push_back(targetPath);
		}
		else
		{
			Install(targetPath);
		}
	}
	bool Register(Hook::Function target, const Hook::Function replacement)
	{
		assert(target != nullptr);
		assert(replacement != nullptr);

		const Hook hook = Find(replacement);

		if (hook.IsInstalled())
		{
			return target == hook.Target;
		}

		return Install(target, replacement, HookType::FunctionHook);
	}
	bool Register(Hook::Function vtable[], unsigned int offset, const Hook::Function replacement)
	{
		assert(vtable != nullptr);
		assert(replacement != nullptr);

		DWORD protection = 0;
		
		if (::VirtualProtect(reinterpret_cast<LPVOID>(vtable + offset), sizeof(Hook::Function), PAGE_READONLY, &protection) != FALSE)
		{
			Hook::Function target = vtable[offset];
			const auto insert = sVTableAddresses.emplace(target, vtable + offset);

			::VirtualProtect(reinterpret_cast<LPVOID>(vtable + offset), sizeof(Hook::Function), protection, &protection);

			if (insert.second)
			{
				if (Install(target, replacement, HookType::VTableHook))
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
		CriticalSection::Lock lock(sCS);

		LOG(INFO) << "Uninstalling " << sHooks.size() << " hook(s) ...";

		// Uninstall hooks
		for (auto &it : sHooks)
		{
			Uninstall(it.first, it.second);
		}

		sHooks.clear();

		// Free loaded modules
		for (auto module : sModules)
		{
			Module::Free(module);
		}

		sModules.clear();
	}

	Hook Find(const Hook::Function replacement)
	{
		CriticalSection::Lock lock(sCS);

		// Lookup hook
		const auto begin = sHooks.begin(), end = sHooks.end(), it = std::find_if(begin, end, [&replacement](const std::pair<Hook, HookType> &it) { return it.first.Replacement == replacement; });

		if (it != end)
		{
			return it->first;
		}

		return Hook();
	}
	Hook::Function Call(const Hook::Function replacement)
	{
		const Hook hook = Find(replacement);

		if (!hook.IsValid())
		{
			LOG(ERROR) << "Unable to resolve hook for '0x" << replacement << "'!";

			return nullptr;
		}

		return hook.Call();
	}
} }