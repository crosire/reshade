#pragma once

#include "Effect.hpp"

#include <chrono>
#include <memory>
#include <vector>
#include <boost\filesystem\path.hpp>

namespace ReShade
{
	struct EffectTree;

	class Runtime abstract
	{
	public:
		static void Startup(const boost::filesystem::path &executablePath, const boost::filesystem::path &injectorPath);
		static void Shutdown(void);

	public:
		Runtime();
		~Runtime();

		bool ReCreate();
		bool ReCreate(unsigned int width, unsigned int height);
		bool OnCreate(unsigned int width, unsigned int height);
		void OnDelete();
		void OnPostProcess();
		void OnPresent();

	protected:
		virtual std::unique_ptr<Effect> CreateEffect(const EffectTree &ast, std::string &errors) const = 0;
		void CreateResources();
		virtual void CreateScreenshot(unsigned char *buffer, std::size_t size) const = 0;
		void CreateScreenshot(const boost::filesystem::path &path);

	protected:
		unsigned int mWidth, mHeight;

	private:
		std::unique_ptr<Effect> mEffect;
		std::vector<std::pair<bool, const Effect::Technique *>> mTechniques;
		std::vector<Effect::Texture *> mColorTargets;
		std::chrono::high_resolution_clock::time_point mLastPresent;
		std::chrono::high_resolution_clock::duration mLastFrametime, mLastPostProcessTime;
		std::chrono::system_clock::time_point mLastEffectModification;
	};
}