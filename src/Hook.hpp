#pragma once

namespace ReHook
{
	class														Hook
	{
	public:
		typedef void *											Function;

	public:
		static Hook												Install(Function target, const Function replacement);
		template <typename F> static inline Hook				Install(F target, const F replacement)
		{
			return Install(reinterpret_cast<Function>(target), reinterpret_cast<const Function>(replacement));
		}
		static bool												Uninstall(Hook &hook);

	public:
		inline Hook(void) : mTarget(nullptr), mTrampoline(nullptr)
		{
		}

		bool													IsEnabled(void) const;
		bool													IsInstalled(void) const;

		Function												Call(void) const;
		template <typename F> inline F							Call(void) const
		{
			return reinterpret_cast<F>(Call());
		}

		bool													Enable(bool enable = true);
		inline bool												Uninstall(void)
		{
			return Uninstall(*this);
		}

	private:
		Function												mTarget, mTrampoline;
	};
}