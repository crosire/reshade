#pragma once

#include "Effect.hpp"

#include <memory>
#include <vector>
#include <boost\chrono.hpp>
#include <boost\filesystem\path.hpp>

struct NVGcontext;

namespace ReShade
{
	struct EffectTree;

	class Runtime abstract
	{
	public:
		struct InfoTechnique
		{
			bool Enabled;
			int Timeout, Timeleft;
			int Toggle, ToggleTime;
		};

	public:
		static void Startup(const boost::filesystem::path &executablePath, const boost::filesystem::path &injectorPath);
		static void Shutdown();

	public:
		Runtime();
		virtual ~Runtime();

		virtual bool OnCreate(unsigned int width, unsigned int height);
		virtual void OnDelete();
		virtual void OnDraw(unsigned int vertices);
		virtual void OnPresent();

	protected:
		virtual std::unique_ptr<Effect> CreateEffect(const EffectTree &ast, std::string &errors) const = 0;
		void CreateResources();
		virtual void CreateScreenshot(unsigned char *buffer, std::size_t size) const = 0;
		void CreateScreenshot(const boost::filesystem::path &path);

	protected:
		unsigned int mWidth, mHeight;
		unsigned int mVendorId, mDeviceId, mRendererId;
		unsigned long mLastDrawCalls, mLastDrawCallVertices;
		NVGcontext *mNVG;

	private:
		std::unique_ptr<Effect> mEffect;
		std::vector<std::pair<const Effect::Technique *, InfoTechnique>> mTechniques;
		std::vector<Effect::Texture *> mColorTargets, mDepthTargets;
		boost::chrono::high_resolution_clock::time_point mStartTime, mLastCreate, mLastPresent;
		boost::chrono::high_resolution_clock::duration mLastFrametime;
		unsigned long long mLastFrameCount;
		std::string mErrors, mMessage;
		bool mShowStatistics;
	};
}