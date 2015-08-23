#include "Log.hpp"
#include "Version.h"
#include "Runtime.hpp"
#include "HookManager.hpp"
#include "FX\Parser.hpp"
#include "FX\PreProcessor.hpp"
#include "GUI.hpp"
#include "FileWatcher.hpp"
#include "WindowWatcher.hpp"
#include "Utils\Algorithm.hpp"

#include <stb_dxt.h>
#include <stb_image.h>
#include <stb_image_write.h>
#include <stb_image_resize.h>
#include <boost\filesystem\operations.hpp>

namespace ReShade
{
	namespace
	{
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

		FileWatcher *sEffectWatcher = nullptr;
		unsigned int sCompileCounter = 0;
		boost::filesystem::path sExecutablePath, sInjectorPath, sEffectPath;
	}

	volatile long NetworkUpload = 0;

	// -----------------------------------------------------------------------------------------------------

	void Runtime::Startup(const boost::filesystem::path &executablePath, const boost::filesystem::path &injectorPath)
	{
		sInjectorPath = injectorPath;
		sExecutablePath = executablePath;

		boost::filesystem::path logPath = injectorPath, logTracePath = injectorPath;
		logPath.replace_extension("log");
		logTracePath.replace_extension("tracelog");

		if (GetFileAttributes(logTracePath.c_str()) == INVALID_FILE_ATTRIBUTES)
		{
			Log::Open(logPath, Log::Level::Info);
		}
		else
		{
			Log::Open(logTracePath, Log::Level::Trace);
		}

		LOG(INFO) << "Initializing Crosire's ReShade version '" BOOST_STRINGIZE(VERSION_FULL) "' built on '" VERSION_DATE " " VERSION_TIME "' loaded from " << ObfuscatePath(injectorPath) << " to " << ObfuscatePath(executablePath) << " ...";

		const boost::filesystem::path systemPath = GetSystemDirectory();

		Hooks::RegisterModule(systemPath / "d3d8.dll");
		Hooks::RegisterModule(systemPath / "d3d9.dll");
		Hooks::RegisterModule(systemPath / "d3d10.dll");
		Hooks::RegisterModule(systemPath / "d3d10_1.dll");
		Hooks::RegisterModule(systemPath / "d3d11.dll");
		Hooks::RegisterModule(systemPath / "dxgi.dll");
		Hooks::RegisterModule(systemPath / "opengl32.dll");
		Hooks::RegisterModule(systemPath / "user32.dll");
		Hooks::RegisterModule(systemPath / "ws2_32.dll");

		sEffectWatcher = new FileWatcher(sInjectorPath.parent_path(), true);

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

	// -----------------------------------------------------------------------------------------------------

	Runtime::Runtime() : mIsInitialized(false), mIsEffectCompiled(false), mWidth(0), mHeight(0), mVendorId(0), mDeviceId(0), mRendererId(0), mStats(), mScreenshotFormat("png"), mScreenshotPath(sExecutablePath.parent_path()), mScreenshotKey(VK_SNAPSHOT), mCompileStep(0), mShowStatistics(false), mShowFPS(false), mShowClock(false), mShowToggleMessage(false)
	{
		memset(&this->mStats, 0, sizeof(Statistics));

		this->mStatus = "Initializing ...";
		this->mStartTime = boost::chrono::high_resolution_clock::now();
	}
	Runtime::~Runtime()
	{
		assert(!this->mIsInitialized && !this->mIsEffectCompiled);
	}

	bool Runtime::OnInit()
	{
		this->mCompileStep = 1;

		LOG(INFO) << "Recreated runtime environment on runtime " << this << ".";

		this->mIsInitialized = true;

		return true;
	}
	void Runtime::OnReset()
	{
		OnResetEffect();

		if (!this->mIsInitialized)
		{
			return;
		}

		LOG(INFO) << "Destroyed runtime environment on runtime " << this << ".";

		this->mWidth = this->mHeight = 0;
		this->mIsInitialized = false;
	}
	void Runtime::OnResetEffect()
	{
		if (!this->mIsEffectCompiled)
		{
			return;
		}

		this->mTextures.clear();
		this->mUniforms.clear();
		this->mTechniques.clear();
		this->mUniformDataStorage.clear();

		this->mIsEffectCompiled = false;
	}
	void Runtime::OnPresent()
	{
		const std::time_t time = std::time(nullptr);
		const auto timePresent = boost::chrono::high_resolution_clock::now();
		const boost::chrono::nanoseconds frametime = boost::chrono::duration_cast<boost::chrono::nanoseconds>(timePresent - this->mLastPresent);

		tm tm;
		localtime_s(&tm, &time);

		#pragma region Screenshot
		if (GetAsyncKeyState(this->mScreenshotKey) & 0x8000)
		{
			char timeString[128];
			std::strftime(timeString, 128, "%Y-%m-%d %H-%M-%S", &tm);
			const boost::filesystem::path path = this->mScreenshotPath / (sExecutablePath.stem().string() + ' ' + timeString + '.' + this->mScreenshotFormat);
			std::unique_ptr<unsigned char[]> data(new unsigned char[this->mWidth * this->mHeight * 4]());

			Screenshot(data.get());

			LOG(INFO) << "Saving screenshot to " << ObfuscatePath(path) << " ...";

			bool success = false;

			if (path.extension() == ".bmp")
			{
				success = stbi_write_bmp(path.string().c_str(), this->mWidth, this->mHeight, 4, data.get()) != 0;
			}
			else if (path.extension() == ".png")
			{
				success = stbi_write_png(path.string().c_str(), this->mWidth, this->mHeight, 4, data.get(), 0) != 0;
			}

			if (!success)
			{
				LOG(ERROR) << "Failed to write screenshot to " << ObfuscatePath(path) << "!";
			}
		}
		#pragma endregion

		#pragma region Compile effect
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
		#pragma endregion

		#pragma region Draw overlay
		if (this->mGUI->BeginFrame())
		{
			const boost::chrono::seconds timeSinceCreate = boost::chrono::duration_cast<boost::chrono::seconds>(timePresent - this->mLastCreate);

			if (!this->mStatus.empty())
			{
				this->mGUI->DrawSimpleText(0,  0, 20, 0xBCBCBC, "ReShade " BOOST_STRINGIZE(VERSION_FULL) " by Crosire");
				this->mGUI->DrawSimpleText(0, 22, 16, 0xBCBCBC, "Visit http://reshade.me for news, updates, shaders and discussion.");
				this->mGUI->DrawSimpleText(0, 42, 16, 0xBCBCBC, this->mStatus);

				if (!this->mErrors.empty() && this->mCompileStep == 0)
				{
					if (!this->mIsEffectCompiled)
					{
						this->mGUI->DrawSimpleTextMultiLine(0, 60, static_cast<float>(this->mWidth), 16, 0xFF0000, this->mErrors);
					}
					else
					{
						this->mGUI->DrawSimpleTextMultiLine(0, 60, static_cast<float>(this->mWidth), 16, 0xFFFF00, this->mErrors);
					}
				}
			}

			if (!this->mMessage.empty())
			{
				this->mGUI->SetAlignment(0, 0);
				this->mGUI->DrawSimpleTextMultiLine(0, 0, static_cast<float>(this->mWidth), 16, 0xBCBCBC, this->mMessage);
			}

			std::stringstream stats;

			if (this->mShowClock)
			{
				stats << std::setfill('0') << std::setw(2) << tm.tm_hour << ':' << std::setw(2) << tm.tm_min << std::endl;
			}
			if (this->mShowFPS)
			{
				stats << this->mStats.FrameRate << std::endl;
			}
			if (this->mShowStatistics)
			{
				stats << "General" << std::endl << "-------" << std::endl;
				stats << "Application: " << std::hash<std::string>()(sExecutablePath.stem().string()) << std::endl;
				stats << "Date: " << static_cast<int>(this->mStats.Date[0]) << '-' << static_cast<int>(this->mStats.Date[1]) << '-' << static_cast<int>(this->mStats.Date[2]) << ' ' << static_cast<int>(this->mStats.Date[3]) << '\n';
				stats << "Device: " << std::hex << std::uppercase << this->mVendorId << ' ' << this->mDeviceId << std::nouppercase << std::dec << std::endl;
				stats << "FPS: " << this->mStats.FrameRate << std::endl;
				stats << "Draw Calls: " << this->mStats.DrawCalls << " (" << this->mStats.Vertices << " vertices)" << std::endl;
				stats << "Frame " << (this->mStats.FrameCount + 1) << ": " << (frametime.count() * 1e-6f) << "ms" << std::endl;
				stats << "PostProcessing: " << (boost::chrono::duration_cast<boost::chrono::nanoseconds>(this->mLastPostProcessingDuration).count() * 1e-6f) << "ms" << std::endl;
				stats << "Timer: " << std::fmod(boost::chrono::duration_cast<boost::chrono::nanoseconds>(this->mLastPresent - this->mStartTime).count() * 1e-6f, 16777216.0f) << "ms" << std::endl;
				stats << "Network: " << NetworkUpload << "B up" << std::endl;

				stats << std::endl;
				stats << "Textures" << std::endl << "--------" << std::endl;

				for (const auto &texture : this->mTextures)
				{
					stats << texture->Name << ": " << texture->Width << "x" << texture->Height << "+" << (texture->Levels - 1) << " (" << texture->StorageSize << "B)" << std::endl;
				}

				stats << std::endl;
				stats << "Techniques" << std::endl << "----------" << std::endl;

				for (const auto &technique : this->mTechniques)
				{
					stats << technique->Name << " (" << technique->PassCount << " passes): " << (boost::chrono::duration_cast<boost::chrono::nanoseconds>(technique->LastDuration).count() * 1e-6f) << "ms" << std::endl;
				}
			}

			this->mGUI->SetAlignment(1, -1);
			this->mGUI->DrawSimpleTextMultiLine(0, 0, static_cast<float>(this->mWidth), 16, 0xBCBCBC, stats.str());

			if (timeSinceCreate.count() > (this->mErrors.empty() ? 4 : 8) && this->mIsEffectCompiled)
			{
				this->mStatus.clear();
				this->mMessage.clear();
			}

			this->mGUI->FlushFrame();
		}
		#pragma endregion

		#pragma region Update statistics
		NetworkUpload = 0;
		this->mLastPresent = timePresent;
		this->mLastFrameDuration = frametime;
		this->mStats.FrameCount++;
		this->mStats.DrawCalls = this->mStats.Vertices = 0;
		this->mStats.FrameRate.Calculate(frametime.count());
		this->mStats.Date[0] = static_cast<float>(tm.tm_year + 1900);
		this->mStats.Date[1] = static_cast<float>(tm.tm_mon + 1);
		this->mStats.Date[2] = static_cast<float>(tm.tm_mday);
		this->mStats.Date[3] = static_cast<float>(tm.tm_hour * 3600 + tm.tm_min * 60 + tm.tm_sec);
		#pragma endregion

		this->mWindow->NextFrame();
	}
	void Runtime::OnDrawCall(unsigned int vertices)
	{
		this->mStats.Vertices += vertices;
		this->mStats.DrawCalls += 1;
	}
	void Runtime::OnApplyEffect()
	{
		const auto timePostProcessingStarted = boost::chrono::high_resolution_clock::now();

		for (const auto &technique : this->mTechniques)
		{
			if (technique->ToggleTime != 0 && technique->ToggleTime == static_cast<int>(this->mStats.Date[3]))
			{
				technique->Enabled = !technique->Enabled;
				technique->Timeleft = technique->Timeout;
				technique->ToggleTime = 0;
			}
			else if (technique->Timeleft > 0)
			{
				technique->Timeleft -= static_cast<unsigned int>(boost::chrono::duration_cast<boost::chrono::milliseconds>(this->mLastFrameDuration).count());

				if (technique->Timeleft <= 0)
				{
					technique->Enabled = !technique->Enabled;
					technique->Timeleft = 0;
				}
			}
			else if (this->mWindow->GetKeyJustPressed(technique->Toggle) && (!technique->ToggleCtrl || this->mWindow->GetKeyState(VK_CONTROL)) && (!technique->ToggleShift || this->mWindow->GetKeyState(VK_SHIFT)) && (!technique->ToggleAlt || this->mWindow->GetKeyState(VK_MENU)))
			{
				technique->Enabled = !technique->Enabled;
				technique->Timeleft = technique->Timeout;

				if (this->mShowToggleMessage)
				{
					this->mStatus = technique->Name + (technique->Enabled ? " enabled." : " disabled.");
					this->mLastCreate = timePostProcessingStarted;
				}
			}

			if (!technique->Enabled)
			{
				technique->LastDuration = boost::chrono::high_resolution_clock::duration(0);

				continue;
			}

			const auto timeTechniqueStarted = boost::chrono::high_resolution_clock::now();

			OnApplyEffectTechnique(technique.get());

			if (boost::chrono::duration_cast<boost::chrono::milliseconds>(timeTechniqueStarted - technique->LastDurationUpdate).count() > 250)
			{
				technique->LastDuration = boost::chrono::high_resolution_clock::now() - timeTechniqueStarted;
				technique->LastDurationUpdate = timeTechniqueStarted;
			}
		}

		this->mLastPostProcessingDuration = boost::chrono::high_resolution_clock::now() - timePostProcessingStarted;
	}
	void Runtime::OnApplyEffectTechnique(const Technique *technique)
	{
		for (const auto &variable : this->mUniforms)
		{
			const std::string source = variable->Annotations["source"].As<std::string>();

			if (source.empty())
			{
				continue;
			}
			else if (source == "frametime")
			{
				const float value = this->mLastFrameDuration.count() * 1e-6f;
				SetEffectValue(*variable, &value, 1);
			}
			else if (source == "framecount" || source == "framecounter")
			{
				switch (variable->BaseType)
				{
					case Uniform::Type::Bool:
					{
						const bool even = (this->mStats.FrameCount % 2) == 0;
						SetEffectValue(*variable, &even, 1);
						break;
					}
					case Uniform::Type::Int:
					case Uniform::Type::Uint:
					{
						const unsigned int framecount = static_cast<unsigned int>(this->mStats.FrameCount % UINT_MAX);
						SetEffectValue(*variable, &framecount, 1);
						break;
					}
					case Uniform::Type::Float:
					{
						const float framecount = static_cast<float>(this->mStats.FrameCount % 16777216);
						SetEffectValue(*variable, &framecount, 1);
						break;
					}
				}
			}
			else if (source == "pingpong")
			{
				float value[2] = { 0, 0 };
				GetEffectValue(*variable, value, 2);

				const float min = variable->Annotations["min"].As<float>(), max = variable->Annotations["max"].As<float>();
				const float stepMin = variable->Annotations["step"].As<float>(0), stepMax = variable->Annotations["step"].As<float>(1);
				float increment = stepMax == 0 ? stepMin : (stepMin + std::fmodf(static_cast<float>(std::rand()), stepMax - stepMin + 1));
				const float smoothing = variable->Annotations["smoothing"].As<float>();

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

				SetEffectValue(*variable, value, 2);
			}
			else if (source == "date")
			{
				SetEffectValue(*variable, this->mStats.Date, 4);
			}
			else if (source == "timer")
			{
				const unsigned long long timer = boost::chrono::duration_cast<boost::chrono::nanoseconds>(this->mLastPresent - this->mStartTime).count();

				switch (variable->BaseType)
				{
					case Uniform::Type::Bool:
					{
						const bool even = (timer % 2) == 0;
						SetEffectValue(*variable, &even, 1);
						break;
					}
					case Uniform::Type::Int:
					case Uniform::Type::Uint:
					{
						const unsigned int timerInt = static_cast<unsigned int>(timer % UINT_MAX);
						SetEffectValue(*variable, &timerInt, 1);
						break;
					}
					case Uniform::Type::Float:
					{
						const float timerFloat = std::fmod(static_cast<float>(timer * 1e-6f), 16777216.0f);
						SetEffectValue(*variable, &timerFloat, 1);
						break;
					}
				}
			}
			else if (source == "timeleft")
			{
				SetEffectValue(*variable, &technique->Timeleft, 1);
			}
			else if (source == "key")
			{
				const int key = variable->Annotations["keycode"].As<int>();

				if (key > 0 && key < 256)
				{
					if (variable->Annotations["toggle"].As<bool>())
					{
						bool current = false;
						GetEffectValue(*variable, &current, 1);

						if (this->mWindow->GetKeyJustPressed(key))
						{
							current = !current;

							SetEffectValue(*variable, &current, 1);
						}
					}
					else
					{
						const bool state = this->mWindow->GetKeyState(key);

						SetEffectValue(*variable, &state, 1);
					}
				}
			}
			else if (source == "random")
			{
				const int min = variable->Annotations["min"].As<int>(), max = variable->Annotations["max"].As<int>();
				const int value = min + (std::rand() % (max - min + 1));

				SetEffectValue(*variable, &value, 1);
			}
		}
	}

	void Runtime::GetEffectValue(const Uniform &variable, unsigned char *data, std::size_t size) const
	{
		assert(data != nullptr);
		assert(this->mIsEffectCompiled);

		size = std::min(size, variable.StorageSize);

		assert(variable.StorageOffset + size < this->mUniformDataStorage.size());

		std::copy_n(this->mUniformDataStorage.begin() + variable.StorageOffset, size, data);
	}
	void Runtime::GetEffectValue(const Uniform &variable, bool *values, std::size_t count) const
	{
		static_assert(sizeof(int) == 4 && sizeof(float) == 4, "expected int and float size to equal 4");

		count = std::min(count, variable.StorageSize / 4);

		assert(values != nullptr);

		unsigned char *const data = static_cast<unsigned char *>(alloca(variable.StorageSize));
		GetEffectValue(variable, data, variable.StorageSize);

		for (std::size_t i = 0; i < count; i++)
		{
			values[i] = reinterpret_cast<const unsigned int *>(data)[i] != 0;
		}
	}
	void Runtime::GetEffectValue(const Uniform &variable, int *values, std::size_t count) const
	{
		switch (variable.BaseType)
		{
			case Uniform::Type::Bool:
			case Uniform::Type::Int:
			case Uniform::Type::Uint:
			{
				GetEffectValue(variable, reinterpret_cast<unsigned char *>(values), count * sizeof(int));
				break;
			}
			case Uniform::Type::Float:
			{
				count = std::min(count, variable.StorageSize / sizeof(float));

				assert(values != nullptr);

				unsigned char *const data = static_cast<unsigned char *>(alloca(variable.StorageSize));
				GetEffectValue(variable, data, variable.StorageSize);

				for (std::size_t i = 0; i < count; i++)
				{
					values[i] = static_cast<int>(reinterpret_cast<const float *>(data)[i]);
				}
				break;
			}
		}
	}
	void Runtime::GetEffectValue(const Uniform &variable, unsigned int *values, std::size_t count) const
	{
		GetEffectValue(variable, reinterpret_cast<int *>(values), count);
	}
	void Runtime::GetEffectValue(const Uniform &variable, float *values, std::size_t count) const
	{
		switch (variable.BaseType)
		{
			case Uniform::Type::Bool:
			case Uniform::Type::Int:
			case Uniform::Type::Uint:
			{
				count = std::min(count, variable.StorageSize / sizeof(int));

				assert(values != nullptr);

				unsigned char *const data = static_cast<unsigned char *>(alloca(variable.StorageSize));
				GetEffectValue(variable, data, variable.StorageSize);

				for (std::size_t i = 0; i < count; ++i)
				{
					if (variable.BaseType != Uniform::Type::Uint)
					{
						values[i] = static_cast<float>(reinterpret_cast<const int *>(data)[i]);
					}
					else
					{
						values[i] = static_cast<float>(reinterpret_cast<const unsigned int *>(data)[i]);
					}
				}
				break;
			}
			case Uniform::Type::Float:
			{
				GetEffectValue(variable, reinterpret_cast<unsigned char *>(values), count * sizeof(float));
				break;
			}
		}
	}
	void Runtime::SetEffectValue(Uniform &variable, const unsigned char *data, std::size_t size)
	{
		assert(data != nullptr);
		assert(this->mIsEffectCompiled);

		size = std::min(size, variable.StorageSize);

		assert(variable.StorageOffset + size < this->mUniformDataStorage.size());

		std::copy_n(data, size, this->mUniformDataStorage.begin() + variable.StorageOffset);
	}
	void Runtime::SetEffectValue(Uniform &variable, const bool *values, std::size_t count)
	{
		static_assert(sizeof(int) == 4 && sizeof(float) == 4, "expected int and float size to equal 4");

		unsigned char *const data = static_cast<unsigned char *>(alloca(count * 4));

		switch (variable.BaseType)
		{
			case Uniform::Type::Bool:
				for (std::size_t i = 0; i < count; ++i)
				{
					reinterpret_cast<int *>(data)[i] = values[i] ? -1 : 0;
				}
				break;
			case Uniform::Type::Int:
			case Uniform::Type::Uint:
				for (std::size_t i = 0; i < count; ++i)
				{
					reinterpret_cast<int *>(data)[i] = values[i] ? 1 : 0;
				}
				break;
			case Uniform::Type::Float:
				for (std::size_t i = 0; i < count; ++i)
				{
					reinterpret_cast<float *>(data)[i] = values[i] ? 1.0f : 0.0f;
				}
				break;
		}

		SetEffectValue(variable, data, count * 4);
	}
	void Runtime::SetEffectValue(Uniform &variable, const int *values, std::size_t count)
	{
		switch (variable.BaseType)
		{
			case Uniform::Type::Bool:
			case Uniform::Type::Int:
			case Uniform::Type::Uint:
			{
				SetEffectValue(variable, reinterpret_cast<const unsigned char *>(values), count * sizeof(int));
				break;
			}
			case Uniform::Type::Float:
			{
				float *const data = static_cast<float *>(alloca(count * sizeof(float)));

				for (std::size_t i = 0; i < count; ++i)
				{
					data[i] = static_cast<float>(values[i]);
				}

				SetEffectValue(variable, reinterpret_cast<const unsigned char *>(data), count * sizeof(float));
				break;
			}
		}
	}
	void Runtime::SetEffectValue(Uniform &variable, const unsigned int *values, std::size_t count)
	{
		switch (variable.BaseType)
		{
			case Uniform::Type::Bool:
			case Uniform::Type::Int:
			case Uniform::Type::Uint:
			{
				SetEffectValue(variable, reinterpret_cast<const unsigned char *>(values), count * sizeof(int));
				break;
			}
			case Uniform::Type::Float:
			{
				float *const data = static_cast<float *>(alloca(count * sizeof(float)));

				for (std::size_t i = 0; i < count; ++i)
				{
					data[i] = static_cast<float>(values[i]);
				}

				SetEffectValue(variable, reinterpret_cast<const unsigned char *>(data), count * sizeof(float));
				break;
			}
		}
	}
	void Runtime::SetEffectValue(Uniform &variable, const float *values, std::size_t count)
	{
		switch (variable.BaseType)
		{
			case Uniform::Type::Bool:
			case Uniform::Type::Int:
			case Uniform::Type::Uint:
			{
				int *const data = static_cast<int *>(alloca(count * sizeof(int)));

				for (std::size_t i = 0; i < count; ++i)
				{
					data[i] = static_cast<int>(values[i]);
				}

				SetEffectValue(variable, reinterpret_cast<const unsigned char *>(data), count * sizeof(int));
				break;
			}
			case Uniform::Type::Float:
			{
				SetEffectValue(variable, reinterpret_cast<const unsigned char *>(values), count * sizeof(float));
				break;
			}
		}
	}

	bool Runtime::LoadEffect()
	{
		this->mMessage.clear();
		this->mShowStatistics = this->mShowFPS = this->mShowClock = this->mShowToggleMessage = false;
		this->mScreenshotKey = VK_SNAPSHOT;
		this->mScreenshotPath = sExecutablePath.parent_path();
		this->mScreenshotFormat = "png";

		sEffectPath = sInjectorPath;
		sEffectPath.replace_extension("fx");

		if (!boost::filesystem::exists(sEffectPath))
		{
			sEffectPath = sEffectPath.parent_path() / "ReShade.fx";

			if (!boost::filesystem::exists(sEffectPath))
			{
				LOG(ERROR) << "Effect file " << sEffectPath << " does not exist.";

				this->mStatus += " No effect found!";

				return false;
			}
		}

		if ((GetFileAttributes(sEffectPath.c_str()) & FILE_ATTRIBUTE_REPARSE_POINT) != 0)
		{
			TCHAR path2[MAX_PATH] = { 0 };
			const HANDLE handle = CreateFile(sEffectPath.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
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

		LOG(INFO) << "Loading effect from " << ObfuscatePath(sEffectPath) << " ...";
		LOG(TRACE) << "> Running preprocessor ...";

		this->mPragmas.clear();
		this->mIncludedFiles.clear();

		std::string errors;

		const std::string source = preprocessor.Run(sEffectPath, errors, this->mPragmas, this->mIncludedFiles);

		if (source.empty())
		{
			LOG(ERROR) << "Failed to preprocess effect on context " << this << ":\n\n" << errors << "\n";

			this->mStatus += " Failed!";
			this->mErrors = errors;
			this->mEffectSource.clear();

			OnResetEffect();

			return false;
		}
		else if (source == this->mEffectSource && this->mIsEffectCompiled)
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

		for (const std::string &pragma : this->mPragmas)
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
				Utils::EscapeString(path);

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

		Utils::EscapeString(this->mMessage);

		return true;
	}
	bool Runtime::CompileEffect()
	{
		OnResetEffect();

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

		this->mIsEffectCompiled = UpdateEffect(ast, this->mPragmas, this->mErrors);

		if (!this->mIsEffectCompiled)
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
		if (this->mTechniques.empty())
		{
			LOG(WARNING) << "> Effect doesn't contain any techniques.";
			return;
		}

		for (const auto &texture : this->mTextures)
		{
			const std::string source = texture->Annotations["source"].As<std::string>();

			if (!source.empty())
			{
				const boost::filesystem::path path = boost::filesystem::absolute(source, sEffectPath.parent_path());
				int widthFile = 0, heightFile = 0, channelsFile = 0, channels = STBI_default;

				switch (texture->Format)
				{
					case Texture::PixelFormat::R8:
						channels = STBI_r;
						break;
					case Texture::PixelFormat::RG8:
						channels = STBI_rg;
						break;
					case Texture::PixelFormat::DXT1:
						channels = STBI_rgb;
						break;
					case Texture::PixelFormat::RGBA8:
					case Texture::PixelFormat::DXT5:
						channels = STBI_rgba;
						break;
					default:
						LOG(ERROR) << "> Texture " << texture->Name << " uses unsupported format ('R32F'/'RGBA16'/'RGBA16F'/'RGBA32F'/'DXT3'/'LATC1'/'LATC2') for image loading.";
						continue;
				}

				std::size_t dataSize = texture->Width * texture->Height * channels;
				unsigned char *const dataFile = stbi_load(path.string().c_str(), &widthFile, &heightFile, &channelsFile, channels);
				std::unique_ptr<unsigned char[]> data(new unsigned char[dataSize]);

				if (dataFile != nullptr)
				{
					if (texture->Width != static_cast<unsigned int>(widthFile) || texture->Height != static_cast<unsigned int>(heightFile))
					{
						LOG(INFO) << "> Resizing image data for texture '" << texture->Name << "' from " << widthFile << "x" << heightFile << " to " << texture->Width << "x" << texture->Height << " ...";

						stbir_resize_uint8(dataFile, widthFile, heightFile, 0, data.get(), texture->Width, texture->Height, 0, channels);
					}
					else
					{
						std::memcpy(data.get(), dataFile, dataSize);
					}

					stbi_image_free(dataFile);

					switch (texture->Format)
					{
						case Texture::PixelFormat::DXT1:
							stb_compress_dxt_block(data.get(), data.get(), FALSE, STB_DXT_NORMAL);
							dataSize = ((texture->Width + 3) >> 2) * ((texture->Height + 3) >> 2) * 8;
							break;
						case Texture::PixelFormat::DXT5:
							stb_compress_dxt_block(data.get(), data.get(), TRUE, STB_DXT_NORMAL);
							dataSize = ((texture->Width + 3) >> 2) * ((texture->Height + 3) >> 2) * 16;
							break;
					}

					UpdateTexture(texture.get(), data.get(), dataSize);
				}
				else
				{
					this->mErrors += "Unable to load source for texture '" + texture->Name + "'!";

					LOG(ERROR) << "> Source " << ObfuscatePath(path) << " for texture '" << texture->Name << "' could not be loaded! Make sure it exists and of a compatible format.";
				}

				texture->StorageSize = dataSize;
			}
		}
		for (const auto &technique : this->mTechniques)
		{
			technique->Enabled = technique->Annotations["enabled"].As<bool>();
			technique->Timeleft = technique->Timeout = technique->Annotations["timeout"].As<int>();
			technique->Toggle = technique->Annotations["toggle"].As<int>();
			technique->ToggleCtrl = technique->Annotations["togglectrl"].As<bool>();
			technique->ToggleShift = technique->Annotations["toggleshift"].As<bool>();
			technique->ToggleAlt = technique->Annotations["togglealt"].As<bool>();
			technique->ToggleTime = technique->Annotations["toggletime"].As<int>();
		}
	}
}
