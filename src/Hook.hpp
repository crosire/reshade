#pragma once

namespace ReHook
{
	class Hook
	{
	public:
		typedef void *Function;

	public:
		static Hook Install(Function target, const Function replacement);
		template <typename F>
		static inline Hook Install(F target, const F replacement)
		{
			return Install(reinterpret_cast<Function>(target), reinterpret_cast<const Function>(replacement));
		}
		static bool Uninstall(Hook &hook);

	public:
		inline Hook() : mTarget(nullptr), mTrampoline(nullptr)
		{
		}

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

	private:
		Function mTarget, mTrampoline;
	};
}