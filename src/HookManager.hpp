#pragma once

#include "Hook.hpp"

#include <boost\filesystem\path.hpp>

#define EXPORT extern "C"
#define VTABLE(object) (*reinterpret_cast<ReShade::Hook::Function **>(object))

namespace ReShade
{
	enum class HookType
	{
		Export,
		FunctionHook,
		VTableHook
	};

	// -----------------------------------------------------------------------------------------------------

	namespace Hooks
	{
		void Register(const boost::filesystem::path &module);
		bool Register(Hook::Function target, const Hook::Function replacement);
		bool Register(Hook::Function vtable[], unsigned int offset, const Hook::Function replacement);
		void Uninstall();

		Hook Find(const Hook::Function replacement);
		template <typename F>
		inline Hook Find(const F replacement)
		{
			return Find(reinterpret_cast<const Hook::Function>(replacement));
		}
		Hook::Function Call(const Hook::Function replacement);
		template <typename F>
		inline F Call(const F replacement)
		{
			return reinterpret_cast<F>(Call(reinterpret_cast<const Hook::Function>(replacement)));
		}
	}
}