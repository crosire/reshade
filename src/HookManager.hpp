#pragma once

#include "Hook.hpp"

#include <boost\filesystem\path.hpp>

#define EXPORT extern "C"
#define VTABLE(object) (*reinterpret_cast<ReShade::Hook::Function **>(object))

namespace ReShade
{
	namespace Hooks
	{
		bool Install(Hook::Function target, const Hook::Function replacement);
		bool Install(Hook::Function vtable[], unsigned int offset, const Hook::Function replacement);
		void Uninstall();
		void RegisterModule(const boost::filesystem::path &path);

		Hook::Function Call(const Hook::Function replacement);
		template <typename F>
		inline F Call(const F replacement)
		{
			return reinterpret_cast<F>(Call(reinterpret_cast<const Hook::Function>(replacement)));
		}
	}
}