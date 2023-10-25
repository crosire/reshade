/*
 * Copyright (C) 2014 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause OR MIT
 */

#pragma once

#include "hook.hpp"
#include <filesystem>
#include <type_traits>

namespace reshade::hooks
{
	/// <summary>
	/// Installs a hook for the specified target function.
	/// </summary>
	/// <param name="name">Function name. This is used for logging and debugging only.</param>
	/// <param name="target">Address of the target function.</param>
	/// <param name="replacement">Address of the hook function.</param>
	/// <param name="queue_enable">Set this to <see langword="true"/> to queue the enable action instead of immediately executing it.</param>
	/// <returns>Status of the hook installation.</returns>
	bool install(const char *name, hook::address target, hook::address replacement, bool queue_enable = false);
	/// <summary>
	/// Installs a hook for the specified virtual function table entry.
	/// </summary>
	/// <param name="name">Function name. This is used for logging and debugging only.</param>
	/// <param name="vtable">Virtual function table pointer of the target object.</param>
	/// <param name="vtable_index">Index of the target function in the virtual function table.</param>
	/// <param name="replacement">Address of the hook function.</param>
	/// <returns>Status of the hook installation.</returns>
	bool install(const char *name, hook::address vtable[], size_t vtable_index, hook::address replacement);
	/// <summary>
	/// Uninstalls all previously installed hooks.
	/// Only call this function while the loader lock is held, since it is not thread-safe.
	/// </summary>
	void uninstall();

	/// <summary>
	/// Registers any matching exports in the specified module and installs or delays their hooking.
	/// Only call this function while the loader lock is held, since it is not thread-safe.
	/// </summary>
	/// <param name="path">File path to the target module.</param>
	void register_module(const std::filesystem::path &path);

	/// <summary>
	/// Loads the module for export hooks if it has not been loaded yet.
	/// </summary>
	void ensure_export_module_loaded();

	/// <summary>
	/// Checks whether the specified function is currently hooked.
	/// </summary>
	/// <param name="target">Original target address of a function.</param>
	bool is_hooked(hook::address target);

	/// <summary>
	/// Gets the original/trampoline function for the specified hook.
	/// </summary>
	/// <param name="target">Original target address of the hooked function (optional).</param>
	/// <param name="replacement">Address of the hook function.</param>
	/// <returns>Address of original/trampoline function.</returns>
	hook::address call(hook::address replacement, hook::address target);
	template <typename T>
	inline T call(T replacement, hook::address target = nullptr)
	{
		return reinterpret_cast<T>(call(reinterpret_cast<hook::address>(replacement), target));
	}

	/// <summary>
	/// Gets the virtual function table of the specified object.
	/// </summary>
	/// <typeparam name="T">Type of the object.</typeparam>
	/// <param name="instance">Pointer to the object.</param>
	/// <returns>Address of the virtual function table.</returns>
	template <typename T>
	inline hook::address *vtable_from_instance(T *instance)
	{
		static_assert(std::is_polymorphic<T>::value, "can only get virtual function table from polymorphic types");

		return *reinterpret_cast<hook::address **>(instance);
	}

	/// <summary>
	/// Calls the original function for the specified virtual function table entry.
	/// </summary>
	/// <typeparam name="R">Type of the return value.</typeparam>
	/// <typeparam name="T">Type of the object.</typeparam>
	/// <typeparam name="...Args">Type of the function parameters.</typeparam>
	/// <typeparam name="vtable_index">Index of the target function in the virtual function table.</typeparam>
	/// <param name="instance">Pointer to the object.</param>
	/// <param name="...args">Arguments to call the function with.</param>
	/// <returns>Return value from the called function.</returns>
	template <size_t vtable_index, typename R, typename T, typename... Args>
	inline R call_vtable(T *instance, Args... args)
	{
		const auto vtable_entry = vtable_from_instance(instance) + vtable_index;
		const auto func = is_hooked(vtable_entry) ?
			call<R(STDMETHODCALLTYPE *)(T *, Args...)>(nullptr, vtable_entry) :
			reinterpret_cast<R(STDMETHODCALLTYPE *)(T *, Args...)>(*vtable_entry);
		return func(instance, std::forward<Args>(args)...);
	}
}
