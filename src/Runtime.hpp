#pragma once

#include "Effect.hpp"
#include "ParseTree.hpp"

#include <boost\chrono.hpp>
#include <boost\filesystem\path.hpp>

struct NVGcontext;
class WindowWatcher;

namespace ReShade
{
	class Runtime abstract
	{
	public:
		class FPS
		{
		public:
			FPS() : mIndex(0), mTickSum(0), mTickList()
			{
			}

			inline operator float() const
			{
				return this->mFPS;
			}

			void Calculate(unsigned long long ns)
			{
				this->mTickSum -= this->mTickList[this->mIndex];
				this->mTickSum += this->mTickList[this->mIndex++] = ns;

				if (this->mIndex == Samples)
				{
					this->mIndex = 0;
				}

				this->mFPS = 1000000000.0f * Samples / this->mTickSum;
			}

		private:
			static const unsigned int Samples = 100;
			float mFPS;
			unsigned long long mIndex, mTickSum, mTickList[Samples];
		};
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

	public:
		static void Startup(const boost::filesystem::path &executablePath, const boost::filesystem::path &injectorPath);
		static void Shutdown();

	public:
		static volatile long sNetworkUpload, sNetworkDownload;

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
		virtual std::unique_ptr<FX::Effect> CompileEffect(const FX::Tree &ast, std::string &errors) const = 0;
		void ProcessEffect();

		void CreateScreenshot(const boost::filesystem::path &path);
		virtual void CreateScreenshot(unsigned char *buffer, std::size_t size) const = 0;

	protected:
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
		FPS mFramerate;
		float mDate[4];
		std::string mStatus, mErrors, mMessage, mEffectSource;
		std::string mScreenshotFormat;
		bool mShowStatistics, mShowFPS, mShowClock, mShowToggleMessage, mSkipShaderOptimization;
		std::unique_ptr<WindowWatcher> mWindow;
	};
}