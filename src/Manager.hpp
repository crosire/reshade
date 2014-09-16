#pragma once

#include "EffectContext.hpp"

#include <boost\filesystem\path.hpp>

namespace ReShade
{
	class														Manager
	{
	public:
		static bool												Initialize(const boost::filesystem::path &executablePath, const boost::filesystem::path &injectorPath, const boost::filesystem::path &systemPath);
		static void												Exit(void);

	public:
		Manager(std::shared_ptr<EffectContext> context);
		~Manager(void);

		bool													OnCreate(void);
		void													OnDelete(void);
		void													OnPostProcess(void);
		void													OnPresent(void);

	private:
		std::unique_ptr<Effect>									mEffect;
		std::shared_ptr<EffectContext>							mEffectContext;
		const Effect::Technique *								mSelectedTechnique;
		bool													mCreated, mEnabled;
	};
}