#pragma once

#include "Effect.hpp"
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
		bool													mCreated;
		std::vector<std::pair<const Effect::Technique *, bool>> mTechniques;
		std::vector<Effect::Texture *>							mColorTargets, mDepthTargets;
	};
}