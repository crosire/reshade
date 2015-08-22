#pragma once

#include "Hook.hpp"

#include <boost\filesystem\path.hpp>

#define VTABLE(object) (*reinterpret_cast<ReShade::Hook::Function **>(object))

namespace ReShade
{
	namespace Hooks
	{
		/// <summary>
		/// Install hook for the specified target function.
		/// </summary>
		bool Install(Hook::Function target, Hook::Function replacement);
		/// <summary>
		/// Install hook for the specified virtual function table entry.
		/// </summary>
		bool Install(Hook::Function vtable[], unsigned int offset, Hook::Function replacement);
		/// <summary>
		/// Uninstall all previously installed hooks.
		/// </summary>
		void Uninstall();
		/// <summary>
		/// Register the matching exports in the specified module and install or delay their hooking.
		/// </summary>
		void RegisterModule(const boost::filesystem::path &path);

		/// <summary>
		/// Call the original/trampoline function for the specified hook.
		/// </summary>
		Hook::Function Call(Hook::Function replacement);
		template <typename F>
		inline F Call(F replacement)
		{
			return reinterpret_cast<F>(Call(reinterpret_cast<Hook::Function>(replacement)));
		}
	}
}
