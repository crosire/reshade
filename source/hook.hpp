/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

namespace reshade
{
	struct hook
	{
		/// <summary>
		/// Type which holds the address of a function.
		/// </summary>
		using address = void *;

		/// <summary>
		/// Enumeration of status codes returned by the hook installation functions.
		/// </summary>
		enum class status
		{
			unknown = -1,
			success,
			not_executable = 7,
			unsupported_function,
			allocation_failure,
			memory_protection_failure,
		};

		/// <summary>
		/// Actually enable or disable any queued hooks.
		/// </summary>
		static bool apply_queued_actions();

		/// <summary>
		/// Returns whether this hook is valid.
		/// </summary>
		bool valid() const { return target != nullptr && replacement != nullptr && target != replacement; }
		/// <summary>
		/// Returns whether this hook is currently installed.
		/// </summary>
		bool installed() const { return trampoline != nullptr; }
		/// <summary>
		/// Returns whether this hook is not currently installed.
		/// </summary>
		bool uninstalled() const { return trampoline == nullptr; }

		/// <summary>
		/// Enable or disable this hook. This queues the action for later execution in <see cref="apply_queued_actions"/>.
		/// </summary>
		/// <param name="enable">Boolean indicating if hook should be enabled or disabled.</param>
		void enable(bool enable) const;
		/// <summary>
		/// Install this hook.
		/// </summary>
		hook::status install();
		/// <summary>
		/// Uninstall this hook.
		/// </summary>
		hook::status uninstall();

		/// <summary>
		/// Returns the trampoline function address of the hook.
		/// </summary>
		address call() const;
		template <typename T>
		T call() const { return reinterpret_cast<T>(call()); }

		address target = nullptr;
		address trampoline = nullptr;
		address replacement = nullptr;
	};
}
