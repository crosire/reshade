#pragma once

#include "Effect.hpp"

#include <chrono>
#include <memory>
#include <vector>
#include <boost\filesystem\path.hpp>

struct NVGcontext;

namespace ReShade
{
	struct EffectTree;

	class Runtime abstract
	{
	public:
		struct Info
		{
			unsigned int VendorId, DeviceId, RendererId;
		};
		struct InfoTechnique
		{
			bool Enabled;
			int Timeout, Timeleft;
			int Toggle, ToggleTime;
		};

	public:
		static void Startup(const boost::filesystem::path &executablePath, const boost::filesystem::path &injectorPath);
		static void Shutdown(void);

	public:
		Runtime();
		~Runtime();

		virtual Info GetInfo() const = 0;

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
		NVGcontext *mNVG;

	private:
		std::unique_ptr<Effect> mEffect;
		std::vector<std::pair<const Effect::Technique *, InfoTechnique>> mTechniques;
		std::vector<Effect::Texture *> mColorTargets;
		std::chrono::high_resolution_clock::time_point mLastPresent;
		std::chrono::high_resolution_clock::duration mLastFrametime;
		unsigned long long mLastFrameCount;
	};
}