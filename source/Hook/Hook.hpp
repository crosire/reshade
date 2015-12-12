#pragma once

namespace ReShade
{
	struct Hook
	{
		typedef void *Function;
		enum class Status
		{
			Unknown = -1,
			Success,
			NotExecutable = 7,
			UnsupportedFunction,
			AllocationFailure,
			MemoryProtectionFailure,
		};

		Hook();
		Hook(Function target, Function replacement);

		bool IsValid() const;
		bool IsEnabled() const;
		bool IsInstalled() const;

		bool Enable(bool enable = true) const;
		Status Install();
		Status Uninstall();

		Function Call() const;
		template <typename F>
		inline F Call() const
		{
			return reinterpret_cast<F>(Call());
		}

		Function Target, Replacement, Trampoline;
	};
}
