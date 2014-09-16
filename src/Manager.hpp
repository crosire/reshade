#pragma once

#include "EffectContext.hpp"

#include <boost\filesystem\path.hpp>

namespace ReShade
{
	class														Manager
	{
	public:
		static bool												Initialize(const boost::filesystem::path &executable, const boost::filesystem::path &injector, const boost::filesystem::path &system);
		static void												Exit(void);

	public:
		Manager(std::shared_ptr<EffectContext> context);
		~Manager(void);

		bool													OnCreate(void);
		void													OnDelete(void);
		void													OnPostProcess(void);
		void													OnPresent(void);

	private:
		std::shared_ptr<EffectContext>							mEffectContext;
	};
}