#pragma once

namespace ReShade
{
	struct Hook
	{
	public:
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

	public:
		static Status Install(Hook &hook);
		static Status Uninstall(Hook &hook);

	public:
		Hook();
		Hook(Function target, const Function replacement);

		bool IsValid() const;
		bool IsEnabled() const;
		bool IsInstalled() const;

		Function Call() const;
		template <typename F>
		inline F Call() const
		{
			return reinterpret_cast<F>(Call());
		}

		bool Enable(bool enable = true);
		inline Status Uninstall()
		{
			return Uninstall(*this);
		}

	public:
		Function Target, Replacement, Trampoline;
	};
}