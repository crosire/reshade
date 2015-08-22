#pragma once

#include "Effect.hpp"
#include "FX\ParseTree.hpp"
#include "Utils\Time.hpp"

#include <boost\chrono.hpp>
#include <boost\filesystem\path.hpp>

struct NVGcontext;
class WindowWatcher;

namespace ReShade
{
	extern volatile long NetworkUpload, NetworkDownload;

	class Runtime abstract
	{
	public:
		struct TechniqueInfo
		{
			bool Enabled;
			int Timeout, Timeleft;
			int Toggle, ToggleTime;
			bool ToggleCtrl, ToggleShift, ToggleAlt;
			const FX::Effect::Technique *Technique;
			boost::chrono::high_resolution_clock::duration LastDuration;
			boost::chrono::high_resolution_clock::time_point LastDurationUpdate;
		};

		Runtime();
		virtual ~Runtime();

		static void Startup(const boost::filesystem::path &executablePath, const boost::filesystem::path &injectorPath);
		static void Shutdown();

	protected:
		void OnCreate(unsigned int width, unsigned int height);
		void OnDelete();
		void OnDraw(unsigned int vertices);
		void OnPostProcess();
		void OnPresent();

		bool LoadEffect();
		bool CompileEffect();
		virtual std::unique_ptr<FX::Effect> CompileEffect(const FX::Tree &ast, std::string &errors) const = 0;
		void ProcessEffect();

		void CreateScreenshot(const boost::filesystem::path &path);
		virtual void CreateScreenshot(unsigned char *buffer, std::size_t size) const = 0;

		unsigned int mVSync;
		unsigned int mWidth, mHeight;
		unsigned int mVendorId, mDeviceId, mRendererId;
		unsigned long mLastDrawCalls, mLastDrawCallVertices;
		NVGcontext *mNVG;
		std::unique_ptr<FX::Effect> mEffect;
		std::vector<TechniqueInfo> mTechniques;
		std::vector<std::pair<FX::Effect::Texture *, std::size_t>> mTextures;
		std::vector<boost::filesystem::path> mIncludedFiles;
		boost::chrono::high_resolution_clock::time_point mStartTime, mLastCreate, mLastPresent;
		boost::chrono::high_resolution_clock::duration mLastFrameDuration, mLastPostProcessingDuration;
		unsigned long long mLastFrameCount;
		unsigned int mCompileStep;
		Utils::Framerate mFramerate;
		float mDate[4];
		std::string mStatus, mErrors, mMessage, mEffectSource;
		std::string mScreenshotFormat;
		boost::filesystem::path mScreenshotPath;
		unsigned int mScreenshotKey;
		bool mShowStatistics, mShowFPS, mShowClock, mShowToggleMessage, mSkipShaderOptimization;
		std::unique_ptr<WindowWatcher> mWindow;
	};
}
