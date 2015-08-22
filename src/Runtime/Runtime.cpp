#include "Log.hpp"
#include "Version.h"
#include "Runtime.hpp"
#include "HookManager.hpp"
#include "FX\Parser.hpp"
#include "FX\PreProcessor.hpp"
#include "FileWatcher.hpp"
#include "WindowWatcher.hpp"

#include <sstream>
#include <stb_dxt.h>
#include <stb_image.h>
#include <stb_image_write.h>
#include <stb_image_resize.h>
#include <boost\algorithm\string\replace.hpp>
#include <boost\algorithm\string\predicate.hpp>
#include <boost\filesystem\path.hpp>
#include <boost\filesystem\operations.hpp>
#include <nanovg.h>

namespace ReShade
{
	namespace
	{
		void EscapeString(std::string &buffer)
		{
			for (auto it = buffer.begin(); it != buffer.end(); ++it)
			{
				if (it[0] == '\\')
				{
					switch (it[1])
					{
						case '"':
							it[0] = '"';
							break;
						case '\'':
							it[0] = '\'';
							break;
						case '\\':
							it[0] = '\\';
							break;
						case 'a':
							it[0] = '\a';
							break;
						case 'b':
							it[0] = '\b';
							break;
						case 'f':
							it[0] = '\f';
							break;
						case 'n':
							it[0] = '\n';
							break;
						case 'r':
							it[0] = '\r';
							break;
						case 't':
							it[0] = '\t';
							break;
						case 'v':
							it[0] = '\v';
							break;
						default:
							it[0] = it[1];
							break;
					}

					it = std::prev(buffer.erase(it + 1));
				}
			}
		}
		boost::filesystem::path ObfuscatePath(const boost::filesystem::path &path)
		{
			char username[257];
			DWORD usernameSize = ARRAYSIZE(username);
			::GetUserNameA(username, &usernameSize);

			std::string result = path.string();
			boost::algorithm::replace_all(result, username, std::string(usernameSize - 1, '*'));

			return result;
		}
		inline boost::filesystem::path GetSystemDirectory()
		{
			TCHAR path[MAX_PATH];
			::GetSystemDirectory(path, MAX_PATH);
			return path;
		}
		inline boost::filesystem::path GetWindowsDirectory()
		{
			TCHAR path[MAX_PATH];
			::GetWindowsDirectory(path, MAX_PATH);
			return path;
		}

		FileWatcher *sEffectWatcher = nullptr;
		unsigned int sCompileCounter = 0;
		boost::filesystem::path sExecutablePath, sInjectorPath, sEffectPath;
	}

	// -----------------------------------------------------------------------------------------------------

	void Runtime::Startup(const boost::filesystem::path &executablePath, const boost::filesystem::path &injectorPath)
	{
		sInjectorPath = injectorPath;
		sExecutablePath = executablePath;
		sEffectPath = injectorPath;
		sEffectPath.replace_extension("fx");
		boost::filesystem::path systemPath = GetSystemDirectory();

		boost::filesystem::path logPath = injectorPath, logTracePath = injectorPath;
		logPath.replace_extension("log");
		logTracePath.replace_extension("tracelog");

		DeleteFile(logPath.c_str());

		if (GetFileAttributes(logTracePath.c_str()) != INVALID_FILE_ATTRIBUTES)
		{
			DeleteFile(logTracePath.c_str());

			Log::Open(logTracePath, Log::Level::Trace);
		}
		else
		{
			Log::Open(logPath, Log::Level::Info);
		}

		LOG(INFO) << "Initializing Crosire's ReShade version '" BOOST_STRINGIZE(VERSION_FULL) "' built on '" VERSION_DATE " " VERSION_TIME "' loaded from " << ObfuscatePath(injectorPath) << " to " << ObfuscatePath(executablePath) << " ...";

		Hooks::RegisterModule(systemPath / "d3d8.dll");
		Hooks::RegisterModule(systemPath / "d3d9.dll");
		Hooks::RegisterModule(systemPath / "d3d10.dll");
		Hooks::RegisterModule(systemPath / "d3d10_1.dll");
		Hooks::RegisterModule(systemPath / "d3d11.dll");
		Hooks::RegisterModule(systemPath / "dxgi.dll");
		Hooks::RegisterModule(systemPath / "opengl32.dll");
		Hooks::RegisterModule(systemPath / "user32.dll");
		Hooks::RegisterModule(systemPath / "ws2_32.dll");

		sEffectWatcher = new FileWatcher(sEffectPath.parent_path(), true);

		LOG(INFO) << "Initialized.";
	}
	void Runtime::Shutdown()
	{
		LOG(INFO) << "Exiting ...";

		WindowWatcher::UnRegisterRawInputDevices();

		delete sEffectWatcher;

		Hooks::Uninstall();

		LOG(INFO) << "Exited.";
	}

	volatile long NetworkUpload = 0, NetworkDownload = 0;

	// -----------------------------------------------------------------------------------------------------

	Runtime::Runtime() : mVSync(0), mWidth(0), mHeight(0), mVendorId(0), mDeviceId(0), mRendererId(0), mLastFrameCount(0), mLastDrawCalls(0), mLastDrawCallVertices(0), mDate(), mCompileStep(0), mNVG(nullptr), mScreenshotFormat("png"), mScreenshotPath(sExecutablePath.parent_path()), mScreenshotKey(VK_SNAPSHOT), mShowStatistics(false), mShowFPS(false), mShowClock(false), mShowToggleMessage(false), mSkipShaderOptimization(false)
	{
		this->mStatus = "Initializing ...";
		this->mStartTime = boost::chrono::high_resolution_clock::now();
	}
	Runtime::~Runtime()
	{
		OnDelete();
	}

	void Runtime::OnCreate(unsigned int width, unsigned int height)
	{
		if (this->mEffect != nullptr)
		{
			return;
		}

		this->mWidth = width;
		this->mHeight = height;

		if (this->mNVG != nullptr)
		{
			nvgCreateFont(this->mNVG, "Courier", (GetWindowsDirectory() / "Fonts" / "courbd.ttf").string().c_str());
		}

		this->mCompileStep = 1;

		LOG(INFO) << "Recreated effect environment on runtime " << this << ".";
	}
	void Runtime::OnDelete()
	{
		if (this->mEffect == nullptr)
		{
			return;
		}

		this->mTextures.clear();
		this->mTechniques.clear();

		this->mEffect.reset();

		LOG(INFO) << "Destroyed effect environment on runtime " << this << ".";
	}
	void Runtime::OnDraw(unsigned int vertices)
	{
		this->mLastDrawCalls++;
		this->mLastDrawCallVertices += vertices;
	}
	void Runtime::OnPostProcess()
	{
		if (this->mEffect == nullptr)
		{
			this->mLastPostProcessingDuration = boost::chrono::high_resolution_clock::duration(0);
			return;
		}

		const auto timePostProcessingStarted = boost::chrono::high_resolution_clock::now();

		this->mEffect->Enter();

		for (TechniqueInfo &info : this->mTechniques)
		{
			if (info.ToggleTime != 0 && info.ToggleTime == static_cast<int>(this->mDate[3]))
			{
				info.Enabled = !info.Enabled;
				info.Timeleft = info.Timeout;
				info.ToggleTime = 0;
			}
			else if (info.Timeleft > 0)
			{
				info.Timeleft -= static_cast<unsigned int>(boost::chrono::duration_cast<boost::chrono::milliseconds>(this->mLastFrameDuration).count());

				if (info.Timeleft <= 0)
				{
					info.Enabled = !info.Enabled;
					info.Timeleft = 0;
				}
			}
			else if (this->mWindow->GetKeyJustPressed(info.Toggle) && (!info.ToggleCtrl || this->mWindow->GetKeyState(VK_CONTROL)) && (!info.ToggleShift || this->mWindow->GetKeyState(VK_SHIFT)) && (!info.ToggleAlt || this->mWindow->GetKeyState(VK_MENU)))
			{
				info.Enabled = !info.Enabled;
				info.Timeleft = info.Timeout;

				if (this->mShowToggleMessage)
				{
					this->mStatus = info.Technique->GetDescription().Name + (info.Enabled ? " enabled." : " disabled.");
					this->mLastCreate = timePostProcessingStarted;
				}
			}

			if (!info.Enabled)
			{
				info.LastDuration = boost::chrono::high_resolution_clock::duration(0);

				continue;
			}

			#pragma region Update Constants
			for (const std::string &name : this->mEffect->GetConstants())
			{
				FX::Effect::Constant *constant = this->mEffect->GetConstant(name);
				const std::string source = constant->GetAnnotation("source").As<std::string>();

				if (source.empty())
				{
					continue;
				}
				else if (source == "frametime")
				{
					const float value = this->mLastFrameDuration.count() * 1e-6f;

					constant->SetValue(&value, 1);
				}
				else if (source == "framecount" || source == "framecounter")
				{
					switch (constant->GetDescription().Type)
					{
						case FX::Effect::Constant::Type::Bool:
						{
							const bool even = (this->mLastFrameCount % 2) == 0;
							constant->SetValue(&even, 1);
							break;
						}
						case FX::Effect::Constant::Type::Int:
						case FX::Effect::Constant::Type::Uint:
						{
							const unsigned int framecount = static_cast<unsigned int>(this->mLastFrameCount % UINT_MAX);
							constant->SetValue(&framecount, 1);
							break;
						}
						case FX::Effect::Constant::Type::Float:
						{
							const float framecount = static_cast<float>(this->mLastFrameCount % 16777216);
							constant->SetValue(&framecount, 1);
							break;
						}
					}
				}
				else if (source == "pingpong")
				{
					float value[2] = { 0, 0 };
					constant->GetValue(value, 2);

					const float min = constant->GetAnnotation("min").As<float>(), max = constant->GetAnnotation("max").As<float>();
					const float stepMin = constant->GetAnnotation("step").As<float>(0), stepMax = constant->GetAnnotation("step").As<float>(1);
					float increment = stepMax == 0 ? stepMin : (stepMin + std::fmodf(static_cast<float>(std::rand()), stepMax - stepMin + 1));
					const float smoothing = constant->GetAnnotation("smoothing").As<float>();

					if (value[1] >= 0)
					{
						increment = std::max(increment - std::max(0.0f, smoothing - (max - value[0])), 0.05f);
						increment *= this->mLastFrameDuration.count() * 1e-9f;

						if ((value[0] += increment) >= max)
						{
							value[0] = max;
							value[1] = -1;
						}
					}
					else
					{
						increment = std::max(increment - std::max(0.0f, smoothing - (value[0] - min)), 0.05f);
						increment *= this->mLastFrameDuration.count() * 1e-9f;

						if ((value[0] -= increment) <= min)
						{
							value[0] = min;
							value[1] = +1;
						}
					}

					constant->SetValue(value, 2);
				}
				else if (source == "date")
				{
					constant->SetValue(this->mDate, 4);
				}
				else if (source == "timer")
				{
					const unsigned long long timer = boost::chrono::duration_cast<boost::chrono::nanoseconds>(this->mLastPresent - this->mStartTime).count();

					switch (constant->GetDescription().Type)
					{
						case FX::Effect::Constant::Type::Bool:
						{
							const bool even = (timer % 2) == 0;
							constant->SetValue(&even, 1);
							break;
						}
						case FX::Effect::Constant::Type::Int:
						case FX::Effect::Constant::Type::Uint:
						{
							const unsigned int timerInt = static_cast<unsigned int>(timer % UINT_MAX);
							constant->SetValue(&timerInt, 1);
							break;
						}
						case FX::Effect::Constant::Type::Float:
						{
							const float timerFloat = std::fmod(static_cast<float>(timer * 1e-6f), 16777216.0f);
							constant->SetValue(&timerFloat, 1);
							break;
						}
					}
				}
				else if (source == "timeleft")
				{
					constant->SetValue(&info.Timeleft, 1);
				}
				else if (source == "key")
				{
					const int key = constant->GetAnnotation("keycode").As<int>();

					if (key > 0 && key < 256)
					{
						if (constant->GetAnnotation("toggle").As<bool>())
						{
							bool current = false;
							constant->GetValue(&current, 1);

							if (this->mWindow->GetKeyJustPressed(key))
							{
								current = !current;

								constant->SetValue(&current, 1);
							}
						}
						else
						{
							const bool state = this->mWindow->GetKeyState(key);

							constant->SetValue(&state, 1);
						}
					}
				}
				else if (source == "random")
				{
					const int min = constant->GetAnnotation("min").As<int>(), max = constant->GetAnnotation("max").As<int>();
					const int value = min + (std::rand() % (max - min + 1));

					constant->SetValue(&value, 1);
				}
			}
			#pragma endregion

			const auto timeTechniqueStarted = boost::chrono::high_resolution_clock::now();

			info.Technique->Render();

			if (boost::chrono::duration_cast<boost::chrono::milliseconds>(timeTechniqueStarted - info.LastDurationUpdate).count() > 250)
			{
				info.LastDuration = boost::chrono::high_resolution_clock::now() - timeTechniqueStarted;
				info.LastDurationUpdate = timeTechniqueStarted;
			}
		}

		this->mWindow->NextFrame();

		this->mEffect->Leave();

		this->mLastPostProcessingDuration = boost::chrono::high_resolution_clock::now() - timePostProcessingStarted;
	}
	void Runtime::OnPresent()
	{
		const std::time_t time = std::time(nullptr);
		const auto timePresent = boost::chrono::high_resolution_clock::now();
		const boost::chrono::nanoseconds frametime = boost::chrono::duration_cast<boost::chrono::nanoseconds>(timePresent - this->mLastPresent);

		tm tm;
		localtime_s(&tm, &time);

		// Create screenshot
		if (GetAsyncKeyState(this->mScreenshotKey) & 0x8000)
		{
			char timeString[128];
			std::strftime(timeString, 128, "%Y-%m-%d %H-%M-%S", &tm);

			CreateScreenshot(this->mScreenshotPath / (sExecutablePath.stem().string() + ' ' + timeString + '.' + this->mScreenshotFormat));
		}

		// Check for file modifications
		std::vector<boost::filesystem::path> modifications, matchedmodifications;

		if (sEffectWatcher->GetModifications(modifications))
		{
			std::sort(modifications.begin(), modifications.end());
			std::set_intersection(modifications.begin(), modifications.end(), this->mIncludedFiles.begin(), this->mIncludedFiles.end(), std::back_inserter(matchedmodifications));

			if (!matchedmodifications.empty())
			{
				LOG(INFO) << "Detected modification to " << ObfuscatePath(matchedmodifications.front()) << ". Reloading ...";

				this->mCompileStep = 1;
			}
		}

		if (this->mCompileStep != 0)
		{
			this->mLastCreate = timePresent;

			switch (this->mCompileStep)
			{
				case 1:
					this->mStatus = "Loading effect ...";
					this->mCompileStep++;
					break;
				case 2:
					this->mCompileStep = LoadEffect() ? 3 : 0;
					break;
				case 3:
					this->mStatus = "Compiling effect ...";
					this->mCompileStep++;
					break;
				case 4:
					this->mCompileStep = CompileEffect() ? 5 : 0;
					break;
				case 5:
					ProcessEffect();
					this->mCompileStep = 0;
					break;
			}
		}

		// Draw overlay
		if (this->mNVG != nullptr)
		{
			nvgBeginFrame(this->mNVG, this->mWidth, this->mHeight, 1);

			const boost::chrono::seconds timeSinceCreate = boost::chrono::duration_cast<boost::chrono::seconds>(timePresent - this->mLastCreate);

			nvgFontFace(this->mNVG, "Courier");

			if (!this->mStatus.empty())
			{
				nvgFillColor(this->mNVG, nvgRGB(188, 188, 188));
				nvgTextAlign(this->mNVG, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);

				nvgFontSize(this->mNVG, 20);
				nvgText(this->mNVG, 0, 0, "ReShade " BOOST_STRINGIZE(VERSION_FULL) " by Crosire", nullptr);
				nvgFontSize(this->mNVG, 16);
				nvgText(this->mNVG, 0, 22, "Visit http://reshade.me for news, updates, shaders and discussion.", nullptr);
				nvgText(this->mNVG, 0, 42, this->mStatus.c_str(), nullptr);

				if (!this->mErrors.empty() && this->mCompileStep == 0)
				{
					if (this->mEffect == nullptr)
					{
						nvgFillColor(this->mNVG, nvgRGB(255, 0, 0));
					}
					else
					{
						nvgFillColor(this->mNVG, nvgRGB(255, 255, 0));
					}

					nvgTextBox(this->mNVG, 0, 60, static_cast<float>(this->mWidth), this->mErrors.c_str(), nullptr);
				}
			}

			nvgFontSize(this->mNVG, 16);
			nvgFillColor(this->mNVG, nvgRGB(188, 188, 188));

			if (!this->mMessage.empty())
			{
				nvgTextAlign(this->mNVG, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);

				float bounds[4];
				nvgTextBoxBounds(this->mNVG, 0, 0, static_cast<float>(this->mWidth), this->mMessage.c_str(), nullptr, bounds);

				nvgTextBox(this->mNVG, 0, static_cast<float>(this->mHeight) / 2 - bounds[3] / 2, static_cast<float>(this->mWidth), this->mMessage.c_str(), nullptr);
			}

			std::stringstream stats;

			if (this->mShowClock)
			{
				stats << std::setfill('0') << std::setw(2) << tm.tm_hour << ':' << std::setw(2) << tm.tm_min << std::endl;
			}
			if (this->mShowFPS)
			{
				stats << this->mFramerate << std::endl;
			}
			if (this->mShowStatistics)
			{
				stats << "General" << std::endl << "-------" << std::endl;
				stats << "Application: " << std::hash<std::string>()(sExecutablePath.stem().string()) << std::endl;
				stats << "Date: " << static_cast<int>(this->mDate[0]) << '-' << static_cast<int>(this->mDate[1]) << '-' << static_cast<int>(this->mDate[2]) << ' ' << static_cast<int>(this->mDate[3]) << '\n';
				stats << "Device: " << std::hex << std::uppercase << this->mVendorId << ' ' << this->mDeviceId << std::nouppercase << std::dec << std::endl;
				stats << "FPS: " << this->mFramerate << " (VSync " << this->mVSync << ")" << std::endl;
				stats << "Draw Calls: " << this->mLastDrawCalls << " (" << this->mLastDrawCallVertices << " vertices)" << std::endl;
				stats << "Frame " << (this->mLastFrameCount + 1) << ": " << (frametime.count() * 1e-6f) << "ms" << std::endl;
				stats << "PostProcessing: " << (boost::chrono::duration_cast<boost::chrono::nanoseconds>(this->mLastPostProcessingDuration).count() * 1e-6f) << "ms" << std::endl;
				stats << "Timer: " << std::fmod(boost::chrono::duration_cast<boost::chrono::nanoseconds>(this->mLastPresent - this->mStartTime).count() * 1e-6f, 16777216.0f) << "ms" << std::endl;
				stats << "Network: " << NetworkUpload << "B up / " << NetworkDownload << "B down" << std::endl;

				stats << std::endl;
				stats << "Textures" << std::endl << "--------" << std::endl;

				for (auto &it : this->mTextures)
				{
					const FX::Effect::Texture::Description desc = it.first->GetDescription();

					stats << desc.Name << ": " << desc.Width << "x" << desc.Height << "+" << (desc.Levels - 1) << " (" << it.second << "B)" << std::endl;
				}

				stats << std::endl;
				stats << "Techniques" << std::endl << "----------" << std::endl;

				for (TechniqueInfo &info : this->mTechniques)
				{
					const FX::Effect::Technique::Description desc = info.Technique->GetDescription();

					stats << desc.Name << " (" << desc.Passes << " passes): " << (boost::chrono::duration_cast<boost::chrono::nanoseconds>(info.LastDuration).count() * 1e-6f) << "ms" << std::endl;
				}
			}

			nvgTextAlign(this->mNVG, NVG_ALIGN_RIGHT | NVG_ALIGN_TOP);

			nvgTextBox(this->mNVG, 0, 0, static_cast<float>(this->mWidth), stats.str().c_str(), nullptr);

			nvgEndFrame(this->mNVG);

			if (timeSinceCreate.count() > (this->mErrors.empty() ? 4 : 8) && this->mEffect != nullptr)
			{
				this->mStatus.clear();
				this->mMessage.clear();
			}
		}

		// Update inputs
		this->mDate[0] = static_cast<float>(tm.tm_year + 1900);
		this->mDate[1] = static_cast<float>(tm.tm_mon + 1);
		this->mDate[2] = static_cast<float>(tm.tm_mday);
		this->mDate[3] = static_cast<float>(tm.tm_hour * 3600 + tm.tm_min * 60 + tm.tm_sec);

		NetworkUpload = NetworkDownload = 0;
		this->mLastPresent = timePresent;
		this->mLastFrameDuration = frametime;
		this->mLastFrameCount++;
		this->mLastDrawCalls = this->mLastDrawCallVertices = 0;
		this->mFramerate.Calculate(frametime.count());
	}

	bool Runtime::LoadEffect()
	{
		this->mMessage.clear();
		this->mShowStatistics = this->mShowFPS = this->mShowClock = this->mShowToggleMessage = this->mSkipShaderOptimization = false;
		this->mScreenshotKey = VK_SNAPSHOT;
		this->mScreenshotPath = sExecutablePath.parent_path();
		this->mScreenshotFormat = "png";

		boost::filesystem::path path = sEffectPath;

		if (!boost::filesystem::exists(path))
		{
			path = path.parent_path() / "ReShade.fx";
		}
		if (!boost::filesystem::exists(path))
		{
			LOG(ERROR) << "Effect file " << sEffectPath << " does not exist.";

			this->mStatus += " No effect found!";

			return false;
		}

		if ((GetFileAttributes(path.c_str()) & FILE_ATTRIBUTE_REPARSE_POINT) != 0)
		{
			TCHAR path2[MAX_PATH] = { 0 };
			const HANDLE handle = CreateFile(path.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
			assert(handle != INVALID_HANDLE_VALUE);
			GetFinalPathNameByHandle(handle, path2, MAX_PATH, 0);
			CloseHandle(handle);

			// Uh oh, shouldn't do this. Get's rid of the //?/ prefix.
			sEffectPath = path2 + 4;

			delete sEffectWatcher;
			sEffectWatcher = new FileWatcher(sEffectPath.parent_path());
		}

		tm tm;
		std::time_t time = std::time(nullptr);
		::localtime_s(&tm, &time);

		// Preprocess
		FX::PreProcessor preprocessor;
		preprocessor.AddDefine("__RESHADE__", std::to_string(VERSION_MAJOR * 10000 + VERSION_MINOR * 100 + VERSION_REVISION));
		preprocessor.AddDefine("__VENDOR__", std::to_string(this->mVendorId));
		preprocessor.AddDefine("__DEVICE__", std::to_string(this->mDeviceId));
		preprocessor.AddDefine("__RENDERER__", std::to_string(this->mRendererId));
		preprocessor.AddDefine("__APPLICATION__", std::to_string(std::hash<std::string>()(sExecutablePath.stem().string())));
		preprocessor.AddDefine("__DATE_YEAR__", std::to_string(tm.tm_year + 1900));
		preprocessor.AddDefine("__DATE_MONTH__", std::to_string(tm.tm_mday));
		preprocessor.AddDefine("__DATE_DAY__", std::to_string(tm.tm_mon + 1));
		preprocessor.AddDefine("BUFFER_WIDTH", std::to_string(this->mWidth));
		preprocessor.AddDefine("BUFFER_HEIGHT", std::to_string(this->mHeight));
		preprocessor.AddDefine("BUFFER_RCP_WIDTH", std::to_string(1.0f / static_cast<float>(this->mWidth)));
		preprocessor.AddDefine("BUFFER_RCP_HEIGHT", std::to_string(1.0f / static_cast<float>(this->mHeight)));
		preprocessor.AddIncludePath(sEffectPath.parent_path());

		LOG(INFO) << "Loading effect from " << ObfuscatePath(path) << " ...";
		LOG(TRACE) << "> Running preprocessor ...";

		this->mIncludedFiles.clear();

		std::string errors;
		std::vector<std::string> pragmas;
		const std::string source = preprocessor.Run(path, errors, pragmas, this->mIncludedFiles);

		if (source.empty())
		{
			LOG(ERROR) << "Failed to preprocess effect on context " << this << ":\n\n" << errors << "\n";

			this->mStatus += " Failed!";
			this->mErrors = errors;
			this->mEffectSource.clear();

			return false;
		}
		else if (source == this->mEffectSource && this->mEffect != nullptr)
		{
			LOG(INFO) << "> Already compiled.";

			this->mStatus += " Already compiled.";

			return false;
		}
		else
		{
			this->mErrors = errors;
			this->mEffectSource = source;
		}

		for (const std::string &pragma : pragmas)
		{
			if (boost::starts_with(pragma, "message "))
			{
				if (sCompileCounter == 0)
				{
					this->mMessage += pragma.substr(9, pragma.length() - 10);
				}
			}
			else if (!boost::istarts_with(pragma, "reshade "))
			{
				continue;
			}

			const std::string command = pragma.substr(8);

			if (boost::iequals(command, "showstatistics"))
			{
				this->mShowStatistics = true;
			}
			else if (boost::iequals(command, "showfps"))
			{
				this->mShowFPS = true;
			}
			else if (boost::iequals(command, "showclock"))
			{
				this->mShowClock = true;
			}
			else if (boost::iequals(command, "showtogglemessage"))
			{
				this->mShowToggleMessage = true;
			}
			else if (boost::iequals(command, "skipoptimization") || boost::iequals(command, "nooptimization"))
			{
				this->mSkipShaderOptimization = true;
			}
			else if (boost::istarts_with(command, "screenshot_key"))
			{
				this->mScreenshotKey = strtol(command.substr(15).c_str(), nullptr, 0);
				
				if (this->mScreenshotKey == 0)
				{
					this->mScreenshotKey = VK_SNAPSHOT;
				}
			}
			else if (boost::istarts_with(command, "screenshot_format "))
			{
				this->mScreenshotFormat = command.substr(18);
			}
			else if (boost::istarts_with(command, "screenshot_location "))
			{
				std::string path = command.substr(20);
				const std::size_t beg = path.find_first_of('"') + 1, end = path.find_last_of('"');
				path = path.substr(beg, end - beg);
				EscapeString(path);

				if (boost::filesystem::exists(path))
				{
					this->mScreenshotPath = path;
				}
				else
				{
					LOG(ERROR) << "Failed to find screenshot location \"" << path << "\".";
				}
			}
		}

		EscapeString(this->mMessage);

		return true;
	}
	bool Runtime::CompileEffect()
	{
		this->mTextures.clear();
		this->mTechniques.clear();
		this->mEffect.reset();

		FX::Tree ast;
		FX::Lexer lexer(this->mEffectSource);
		FX::Parser parser(lexer, ast);

		LOG(TRACE) << "> Running parser ...";

		const bool success = parser.Parse(this->mErrors);

		if (!success)
		{
			LOG(ERROR) << "Failed to compile effect on context " << this << ":\n\n" << this->mErrors << "\n";

			this->mStatus += " Failed!";

			return false;
		}

		// Compile
		LOG(TRACE) << "> Running compiler ...";

		this->mEffect = CompileEffect(ast, this->mErrors);

		if (this->mEffect == nullptr)
		{
			LOG(ERROR) << "Failed to compile effect on context " << this << ":\n\n" << this->mErrors << "\n";

			this->mStatus += " Failed!";

			return false;
		}
		else if (!this->mErrors.empty())
		{
			LOG(WARNING) << "> Successfully compiled effect with warnings:" << "\n\n" << this->mErrors << "\n";

			this->mStatus += " Succeeded!";
		}
		else
		{
			LOG(INFO) << "> Successfully compiled effect.";

			this->mStatus += " Succeeded!";
		}

		sCompileCounter++;

		return true;
	}
	void Runtime::ProcessEffect()
	{
		const auto techniques = this->mEffect->GetTechniques();

		if (techniques.empty())
		{
			LOG(WARNING) << "> Effect doesn't contain any techniques.";

			return;
		}

		this->mTechniques.reserve(techniques.size());

		for (const std::string &name : techniques)
		{
			const FX::Effect::Technique *technique = this->mEffect->GetTechnique(name);

			TechniqueInfo info;
			info.Technique = technique;

			info.Enabled = technique->GetAnnotation("enabled").As<bool>();
			info.Timeleft = info.Timeout = technique->GetAnnotation("timeout").As<int>();
			info.Toggle = technique->GetAnnotation("toggle").As<int>();
			info.ToggleCtrl = technique->GetAnnotation("togglectrl").As<bool>();
			info.ToggleShift = technique->GetAnnotation("toggleshift").As<bool>();
			info.ToggleAlt = technique->GetAnnotation("togglealt").As<bool>();
			info.ToggleTime = technique->GetAnnotation("toggletime").As<int>();

			this->mTechniques.push_back(std::move(info));
		}

		const auto textures = this->mEffect->GetTextures();

		this->mTextures.reserve(textures.size());

		for (const std::string &name : textures)
		{
			FX::Effect::Texture *texture = this->mEffect->GetTexture(name);
			FX::Effect::Texture::Description desc = texture->GetDescription();
			const std::string source = texture->GetAnnotation("source").As<std::string>();

			if (!source.empty())
			{
				const boost::filesystem::path path = boost::filesystem::absolute(source, sEffectPath.parent_path());
				int widthFile = 0, heightFile = 0, channelsFile = 0, channels = STBI_default;

				switch (desc.Format)
				{
					case FX::Effect::Texture::Format::R8:
						channels = STBI_r;
						break;
					case FX::Effect::Texture::Format::RG8:
						channels = STBI_rg;
						break;
					case FX::Effect::Texture::Format::DXT1:
						channels = STBI_rgb;
						break;
					case FX::Effect::Texture::Format::RGBA8:
					case FX::Effect::Texture::Format::DXT5:
						channels = STBI_rgba;
						break;
					case FX::Effect::Texture::Format::R32F:
					case FX::Effect::Texture::Format::RGBA16:
					case FX::Effect::Texture::Format::RGBA16F:
					case FX::Effect::Texture::Format::RGBA32F:
					case FX::Effect::Texture::Format::DXT3:
					case FX::Effect::Texture::Format::LATC1:
					case FX::Effect::Texture::Format::LATC2:
						LOG(ERROR) << "> Texture " << name << " uses unsupported format ('R32F'/'RGBA16'/'RGBA16F'/'RGBA32F'/'DXT3'/'LATC1'/'LATC2') for image loading.";
						continue;
				}

				std::size_t dataSize = desc.Width * desc.Height * channels;
				unsigned char *dataFile = stbi_load(path.string().c_str(), &widthFile, &heightFile, &channelsFile, channels), *data = new unsigned char[dataSize];

				if (dataFile != nullptr)
				{
					if (desc.Width != static_cast<unsigned int>(widthFile) || desc.Height != static_cast<unsigned int>(heightFile))
					{
						LOG(INFO) << "> Resizing image data for texture '" << name << "' from " << widthFile << "x" << heightFile << " to " << desc.Width << "x" << desc.Height << " ...";

						stbir_resize_uint8(dataFile, widthFile, heightFile, 0, data, desc.Width, desc.Height, 0, channels);
					}
					else
					{
						std::memcpy(data, dataFile, dataSize);
					}

					stbi_image_free(dataFile);

					switch (desc.Format)
					{
						case FX::Effect::Texture::Format::DXT1:
							stb_compress_dxt_block(data, data, FALSE, STB_DXT_NORMAL);
							dataSize = ((desc.Width + 3) >> 2) * ((desc.Height + 3) >> 2) * 8;
							break;
						case FX::Effect::Texture::Format::DXT5:
							stb_compress_dxt_block(data, data, TRUE, STB_DXT_NORMAL);
							dataSize = ((desc.Width + 3) >> 2) * ((desc.Height + 3) >> 2) * 16;
							break;
					}

					texture->Update(data, dataSize);

					this->mTextures.push_back(std::make_pair(texture, dataSize));
				}
				else
				{
					this->mErrors += "Unable to load source for texture '" + name + "'!";

					LOG(ERROR) << "> Source " << ObfuscatePath(path) << " for texture '" << name << "' could not be loaded! Make sure it exists and of a compatible format.";
				}

				delete[] data;
			}
		}
	}

	void Runtime::CreateScreenshot(const boost::filesystem::path &path)
	{
		const std::size_t dataSize = this->mWidth * this->mHeight * 4;

		if (dataSize == 0)
		{
			return;
		}

		unsigned char *data = new unsigned char[dataSize];
		::memset(data, 0, dataSize);
		CreateScreenshot(data, dataSize);

		LOG(INFO) << "Saving screenshot to " << ObfuscatePath(path) << " ...";

		bool success = false;

		if (path.extension() == ".bmp")
		{
			success = stbi_write_bmp(path.string().c_str(), this->mWidth, this->mHeight, 4, data) != 0;
		}
		else if (path.extension() == ".png")
		{
			success = stbi_write_png(path.string().c_str(), this->mWidth, this->mHeight, 4, data, 0) != 0;
		}

		if (!success)
		{
			LOG(ERROR) << "Failed to write screenshot to " << ObfuscatePath(path) << "!";
		}

		delete[] data;
	}
}
