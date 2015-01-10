#pragma once

#include "Effect.hpp"

#include <memory>
#include <vector>
#include <boost\chrono.hpp>
#include <boost\filesystem\path.hpp>

struct NVGcontext;

namespace ReShade
{
	class Runtime abstract
	{
	public:
		struct TechniqueInfo
		{
			bool Enabled;
			int Timeout, Timeleft;
			int Toggle, ToggleTime;
			const Effect::Technique *Technique;
		};

	public:
		static void Startup(const boost::filesystem::path &executablePath, const boost::filesystem::path &injectorPath);
		static void Shutdown();

	public:
		static unsigned int sNetworkUpload, sNetworkDownload;

	public:
		Runtime();
		virtual ~Runtime();

	protected:
		void OnCreate(unsigned int width, unsigned int height);
		void OnDelete();
		void OnDraw(unsigned int vertices);
		void OnPostProcess();
		void OnPresent();

		bool LoadEffect();
		bool CompileEffect();
		virtual std::unique_ptr<Effect> CompileEffect(const struct EffectTree &ast, std::string &errors) const = 0;
		void ProcessEffect();

		void CreateScreenshot(const boost::filesystem::path &path);
		virtual void CreateScreenshot(unsigned char *buffer, std::size_t size) const = 0;

	protected:
		unsigned int mWidth, mHeight;
		unsigned int mVendorId, mDeviceId, mRendererId;
		unsigned long mLastDrawCalls, mLastDrawCallVertices;
		NVGcontext *mNVG;
		std::unique_ptr<Effect> mEffect;
		std::vector<TechniqueInfo> mTechniques;
		boost::chrono::high_resolution_clock::time_point mStartTime, mLastCreate, mLastPresent;
		boost::chrono::high_resolution_clock::duration mLastFrameDuration, mLastPostProcessingDuration;
		unsigned long long mLastFrameCount;
		unsigned int mCompileStep;
		float mDate[4];
		std::string mStatus, mErrors, mMessage, mEffectSource;
		std::string mScreenshotFormat;
		bool mShowStatistics;
	};
}