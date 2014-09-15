#pragma once

#include "Hook.hpp"

#include <cstdint>
#include <boost\filesystem\path.hpp>

#define VTable(pointer, offset) reinterpret_cast<ReHook::Hook::Function>((*reinterpret_cast<uintptr_t **>(pointer))[offset])

namespace ReHook
{
	bool														Register(Hook::Function target, const Hook::Function replacement);
	void														Register(const boost::filesystem::path &dll);

	Hook														Find(const Hook::Function replacement);
	template <typename F> inline Hook							Find(const F replacement)
	{
		return Find(reinterpret_cast<const Hook::Function>(replacement));
	}
	Hook::Function												Call(const Hook::Function replacement);
	template <typename F> inline F								Call(const F replacement)
	{
		return reinterpret_cast<F>(Call(reinterpret_cast<const Hook::Function>(replacement)));
	}

	void														Cleanup(void);
}