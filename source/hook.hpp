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
		/// Address of a function.
		/// </summary>
		using address = void *;

		/// <summary>
		/// Enumeration of status codes.
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
		/// Actually enable or disable any queues hooks.
		/// </summary>
		static bool apply_queued_actions();

		/// <summary>
		/// Return whether the hook is valid.
		/// </summary>
		bool valid() const { return target != nullptr && replacement != nullptr && target != replacement; }
		/// <summary>
		/// Return whether the hook is currently installed.
		/// </summary>
		bool installed() const { return trampoline != nullptr; }
		/// <summary>
		/// Return whether the hook is not currently installed.
		/// </summary>
		bool uninstalled() const { return trampoline == nullptr; }

		/// <summary>
		/// Enable or disable the hook. This queues the action for later execution in <see cref="apply_queued_actions"/>.
		/// </summary>
		/// <param name="enable">Boolean indicating if hook should be enabled or disabled.</param>
		void enable(bool enable) const;
		/// <summary>
		/// Install the hook.
		/// </summary>
		hook::status install();
		/// <summary>
		/// Uninstall the hook.
		/// </summary>
		hook::status uninstall();

		/// <summary>
		/// Return the trampoline function address of the hook.
		/// </summary>
		address call() const;
		template <typename T>
		T call() const { return reinterpret_cast<T>(call()); }

		address target = nullptr;
		address trampoline = nullptr;
		address replacement = nullptr;
	};
}
