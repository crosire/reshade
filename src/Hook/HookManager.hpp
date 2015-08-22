#pragma once

#include "Hook.hpp"

#include <boost\filesystem\path.hpp>

#define VTABLE(object) (*reinterpret_cast<ReShade::Hook::Function **>(object))

namespace ReShade
{
	namespace Hooks
	{
		bool Install(Hook::Function target, Hook::Function replacement);
		bool Install(Hook::Function vtable[], unsigned int offset, Hook::Function replacement);
		void Uninstall();
		void RegisterModule(const boost::filesystem::path &path);

		Hook::Function Call(Hook::Function replacement);
		template <typename F>
		inline F Call(F replacement)
		{
			return reinterpret_cast<F>(Call(reinterpret_cast<Hook::Function>(replacement)));
		}
	}
}
