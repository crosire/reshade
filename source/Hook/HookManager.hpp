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
		/// <param name="target">The address of the target function.</param>
		/// <param name="replacement">The address of the hook function.</param>
		bool Install(Hook::Function target, Hook::Function replacement);
		/// <summary>
		/// Install hook for the specified virtual function table entry.
		/// </summary>
		/// <param name="vtable">The virtual function table pointer of the object to targeted object.</param>
		/// <param name="offset">The index of the target function in the virtual function table.</param>
		/// <param name="replacement">The address of the hook function.</param>
		bool Install(Hook::Function vtable[], unsigned int offset, Hook::Function replacement);
		/// <summary>
		/// Uninstall all previously installed hooks.
		/// </summary>
		void Uninstall();
		/// <summary>
		/// Register the matching exports in the specified module and install or delay their hooking.
		/// </summary>
		/// <param name="path">The file path to the target module.</param>
		void RegisterModule(const boost::filesystem::path &path);

		/// <summary>
		/// Call the original/trampoline function for the specified hook.
		/// </summary>
		/// <param name="replacement">The address of the hook function which was previously used to install a hook.</param>
		Hook::Function Call(Hook::Function replacement);
		template <typename F>
		inline F Call(F replacement)
		{
			return reinterpret_cast<F>(Call(reinterpret_cast<Hook::Function>(replacement)));
		}
	}
}
