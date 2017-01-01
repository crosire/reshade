/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

namespace reshade
{
	struct hook
	{
		typedef void *address;
		enum class status
		{
			unknown = -1,
			success,
			not_executable = 7,
			unsupported_function,
			allocation_failure,
			memory_protection_failure,
		};

		hook();
		hook(address target, address replacement);

		/// <summary>
		/// Return whether the hook is valid.
		/// </summary>
		bool valid() const;
		/// <summary>
		/// Return whether the hook is currently enabled.
		/// </summary>
		bool enabled() const;
		/// <summary>
		/// Return whether the hook is currently installed.
		/// </summary>
		bool installed() const;
		/// <summary>
		/// Return whether the hook is not currently installed.
		/// </summary>
		bool uninstalled() const
		{
			return !installed();
		}

		/// <summary>
		/// Enable or disable the hook.
		/// </summary>
		/// <param name="enable">Boolean indicating if hook should be enabled or disabled.</param>
		bool enable(bool enable = true) const;
		/// <summary>
		/// Install the hook.
		/// </summary>
		status install();
		/// <summary>
		/// Uninstall the hook.
		/// </summary>
		status uninstall();

		/// <summary>
		/// Return the trampoline function address of the hook.
		/// </summary>
		address call() const;
		template <typename T>
		inline T call() const
		{
			return reinterpret_cast<T>(call());
		}

		address target, replacement, trampoline;
	};
}
