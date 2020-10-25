/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include "hook.hpp"
#include <filesystem>
#include <type_traits>

#define HOOK_EXPORT extern "C"

template <typename T>
inline reshade::hook::address *vtable_from_instance(T *instance)
{
	static_assert(std::is_polymorphic<T>::value, "can only get virtual function table from polymorphic types");

	return *reinterpret_cast<reshade::hook::address **>(instance);
}

namespace reshade::hooks
{
	/// <summary>
	/// Installs a hook for the specified target function.
	/// </summary>
	/// <param name="name">The function name. This is used for logging and debugging only.</param>
	/// <param name="target">The address of the target function.</param>
	/// <param name="replacement">The address of the hook function.</param>
	/// <param name="queue_enable">Set this to true to queue the enable action instead of immediately executing it.</param>
	/// <returns>The status of the hook installation.</returns>
	bool install(const char *name, hook::address target, hook::address replacement, bool queue_enable = false);
	/// <summary>
	/// Installs a hook for the specified virtual function table entry.
	/// </summary>
	/// <param name="name">The function name. This is used for logging and debugging only.</param>
	/// <param name="vtable">The virtual function table pointer of the target object.</param>
	/// <param name="offset">The index of the target function in the virtual function table.</param>
	/// <param name="replacement">The address of the hook function.</param>
	/// <returns>The status of the hook installation.</returns>
	bool install(const char *name, hook::address vtable[], unsigned int offset, hook::address replacement);
	/// <summary>
	/// Uninstalls all previously installed hooks.
	/// Only call this function as long as the loader-lock is active, since it is not thread-safe.
	/// </summary>
	void uninstall();

	/// <summary>
	/// Registers the matching exports in the specified module and install or delay their hooking.
	/// Only call this function as long as the loader-lock is active, since it is not thread-safe.
	/// </summary>
	/// <param name="path">The file path to the target module.</param>
	void register_module(const std::filesystem::path &path);

	/// <summary>
	/// Calls the original/trampoline function for the specified hook.
	/// </summary>
	/// <param name="target">The original target address of the hooked function (optional).</param>
	/// <param name="replacement">The address of the hook function.</param>
	/// <returns>The address of original/trampoline function.</returns>
	hook::address call(hook::address replacement, hook::address target);
	template <typename T>
	inline T call(T replacement, hook::address target = nullptr)
	{
		return reinterpret_cast<T>(call(reinterpret_cast<hook::address>(replacement), target));
	}
}
