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

		bool valid() const;
		bool enabled() const;
		bool installed() const;
		bool uninstalled() const
		{
			return !installed();
		}

		bool enable(bool enable = true) const;
		status install();
		status uninstall();

		address call() const;
		template <typename T>
		inline T call() const
		{
			return reinterpret_cast<T>(call());
		}

		address target, replacement, trampoline;
	};
}
