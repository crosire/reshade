#pragma once

namespace ReShade
{
	struct Hook
	{
	public:
		typedef void *Function;

	public:
		static bool Install(Hook &hook);
		static bool Uninstall(Hook &hook);

	public:
		Hook();
		Hook(Function target, const Function replacement);

		bool IsEnabled() const;
		bool IsInstalled() const;

		Function Call() const;
		template <typename F>
		inline F Call() const
		{
			return reinterpret_cast<F>(Call());
		}

		bool Enable(bool enable = true);
		inline bool Uninstall()
		{
			return Uninstall(*this);
		}

	public:
		Function Target, Replacement, Trampoline;
	};
}