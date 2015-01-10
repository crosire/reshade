#include "Log.hpp"
#include "Version.h"
#include "Runtime.hpp"
#include "HookManager.hpp"
#include "EffectPreprocessor.hpp"
#include "EffectParser.hpp"
#include "EffectLexer.h"
#include "FileWatcher.hpp"

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

		el::Configurations logConfig;
		logConfig.set(el::Level::Global, el::ConfigurationType::Enabled, "true");
		logConfig.set(el::Level::Trace, el::ConfigurationType::Enabled, "false");
		logConfig.set(el::Level::Global, el::ConfigurationType::Filename, logPath.string());
		logConfig.set(el::Level::Global, el::ConfigurationType::ToFile, "true");
		logConfig.set(el::Level::Global, el::ConfigurationType::ToStandardOutput, "false");
		logConfig.set(el::Level::Global, el::ConfigurationType::MaxLogFileSize, "0");
		logConfig.set(el::Level::Global, el::ConfigurationType::LogFlushThreshold, "0");
		logConfig.set(el::Level::Global, el::ConfigurationType::Format, "%datetime | %level | %msg");

		DeleteFile(logPath.c_str());

		if (GetFileAttributes(logTracePath.c_str()) != INVALID_FILE_ATTRIBUTES)
		{
			DeleteFile(logTracePath.c_str());

			logConfig.set(el::Level::Trace, el::ConfigurationType::Enabled, "true");
			logConfig.set(el::Level::Global, el::ConfigurationType::Filename, logTracePath.string());
		}

		el::Loggers::reconfigureLogger("default", logConfig);

		LOG(INFO) << "Initializing Crosire's ReShade version '" BOOST_STRINGIZE(VERSION_FULL) "' built on '" VERSION_DATE " " VERSION_TIME "' loaded from " << ObfuscatePath(injectorPath) << " to " << ObfuscatePath(executablePath) << " ...";

		Hooks::Register(systemPath / "d3d8.dll");
		Hooks::Register(systemPath / "d3d9.dll");
		Hooks::Register(systemPath / "d3d10.dll");
		Hooks::Register(systemPath / "d3d10_1.dll");
		Hooks::Register(systemPath / "d3d11.dll");
		Hooks::Register(systemPath / "dxgi.dll");
		Hooks::Register(systemPath / "opengl32.dll");
		Hooks::Register(systemPath / "ws2_32.dll");

		sEffectWatcher = new FileWatcher(sEffectPath.parent_path(), true);

		LOG(INFO) << "Initialized.";
	}
	void Runtime::Shutdown()
	{
		LOG(INFO) << "Exiting ...";

		delete sEffectWatcher;

		Hooks::Uninstall();

		LOG(INFO) << "Exited.";
	}

	unsigned int Runtime::sNetworkUpload = 0, Runtime::sNetworkDownload = 0;

	// -----------------------------------------------------------------------------------------------------

	Runtime::Runtime() : mWidth(0), mHeight(0), mVendorId(0), mDeviceId(0), mRendererId(0), mLastFrameCount(0), mLastDrawCalls(0), mLastDrawCallVertices(0), mDate(), mCompileStep(0), mNVG(nullptr), mScreenshotFormat("png"), mShowStatistics(false)
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
		const auto timePostProcessingStarted = boost::chrono::high_resolution_clock::now();

		for (TechniqueInfo &info : this->mTechniques)
		{
			if (info.ToggleTime != 0 && info.ToggleTime == static_cast<int>(this->mDate[3]))
			{
				info.Enabled = !info.Enabled;
				info.Timeleft = info.Timeout;
				info.ToggleTime = 0;
			}
			else if ((info.Toggle > 0 && info.Toggle < 256) && ::GetKeyState(info.Toggle) & 0x8000)
			{
				info.Enabled = !info.Enabled;
				info.Timeleft = info.Timeout;

				BYTE keys[256];
				::GetKeyboardState(keys);
				keys[info.Toggle] = FALSE;
				::SetKeyboardState(keys);
			}

			if (info.Timeleft > 0)
			{
				info.Timeleft -= static_cast<unsigned int>(boost::chrono::duration_cast<boost::chrono::milliseconds>(this->mLastFrameDuration).count());

				if (info.Timeleft <= 0)
				{
					info.Enabled = !info.Enabled;
					info.Timeleft = 0;
				}
			}

			if (!info.Enabled)
			{
				continue;
			}

			this->mEffect->Begin();

			for (unsigned int i = 0, passes = info.Technique->GetDescription().Passes; i < passes; ++i)
			{
				#pragma region Update Constants
				for (const std::string &name : this->mEffect->GetConstants())
				{
					Effect::Constant *constant = this->mEffect->GetConstant(name);
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
							case Effect::Constant::Type::Bool:
							{
								const bool even = (this->mLastFrameCount % 2) == 0;
								constant->SetValue(&even, 1);
								break;
							}
							case Effect::Constant::Type::Int:
							case Effect::Constant::Type::Uint:
							{
								const unsigned int framecount = static_cast<unsigned int>(this->mLastFrameCount % UINT_MAX);
								constant->SetValue(&framecount, 1);
								break;
							}
							case Effect::Constant::Type::Float:
							{
								const float framecount = static_cast<float>(this->mLastFrameCount % 16777216);
								constant->SetValue(&framecount, 1);
								break;
							}
						}
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
							case Effect::Constant::Type::Bool:
							{
								const bool even = (timer % 2) == 0;
								constant->SetValue(&even, 1);
								break;
							}
							case Effect::Constant::Type::Int:
							case Effect::Constant::Type::Uint:
							{
								const unsigned int timerInt = static_cast<unsigned int>(timer % UINT_MAX);
								constant->SetValue(&timerInt, 1);
								break;
							}
							case Effect::Constant::Type::Float:
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
							const bool state = (::GetAsyncKeyState(key) & 0x8000) != 0;

							constant->SetValue(&state, 1);
						}
					}
				}
				#pragma endregion

				info.Technique->RenderPass(i);
			}

			this->mEffect->End();
		}

		this->mLastPostProcessingDuration = boost::chrono::high_resolution_clock::now() - timePostProcessingStarted;
	}
	void Runtime::OnPresent()
	{
		const std::time_t time = std::time(nullptr);
		const auto timePresent = boost::chrono::high_resolution_clock::now();
		const boost::chrono::nanoseconds frametime = boost::chrono::duration_cast<boost::chrono::nanoseconds>(timePresent - this->mLastPresent);

		tm tm;
		::localtime_s(&tm, &time);

		// Create screenshot
		if (::GetAsyncKeyState(VK_SNAPSHOT) & 0x8000)
		{
			char timeString[128];
			std::strftime(timeString, 128, "%Y-%m-%d %H-%M-%S", &tm); 

			CreateScreenshot(sExecutablePath.parent_path() / (sExecutablePath.stem().string() + ' ' + timeString + '.' + this->mScreenshotFormat));
		}

		// Check for file modifications
		std::vector<boost::filesystem::path> modifications;

		if (sEffectWatcher->GetModifications(modifications))
		{
			for (const auto &path : modifications)
			{
				const boost::filesystem::path extension = path.extension();
					
				if (extension == ".fx" || extension == ".hlsl" || extension == ".h" || extension == ".txt")
				{
					LOG(INFO) << "Detected modification to " << ObfuscatePath(path) << ". Reloading ...";

					this->mCompileStep = 1;
					break;
				}
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
				nvgFillColor(this->mNVG, nvgRGB(255, 255, 255));
				nvgTextAlign(this->mNVG, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);

				nvgFontSize(this->mNVG, 20);
				nvgText(this->mNVG, 0, 0, "ReShade " BOOST_STRINGIZE(VERSION_FULL) " by Crosire", nullptr);
				nvgFontSize(this->mNVG, 16);
				nvgText(this->mNVG, 0, 22, this->mStatus.c_str(), nullptr);

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

					nvgTextBox(this->mNVG, 0, 46, static_cast<float>(this->mWidth), this->mErrors.c_str(), nullptr);
				}
			}

			if (!this->mMessage.empty())
			{
				nvgFillColor(this->mNVG, nvgRGB(255, 255, 255));
				nvgTextAlign(this->mNVG, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
				nvgFontSize(this->mNVG, 16);

				float bounds[4];
				nvgTextBoxBounds(this->mNVG, 0, 0, static_cast<float>(this->mWidth), this->mMessage.c_str(), nullptr, bounds);

				nvgTextBox(this->mNVG, 0, static_cast<float>(this->mHeight) / 2 - bounds[3] / 2, static_cast<float>(this->mWidth), this->mMessage.c_str(), nullptr);
			}
			if (this->mShowStatistics)
			{
				std::stringstream stats;
				stats << "Statistics" << std::endl;
				stats << "Date: " << static_cast<int>(this->mDate[0]) << '-' << static_cast<int>(this->mDate[1]) << '-' << static_cast<int>(this->mDate[2]) << ' ' << static_cast<int>(this->mDate[3]) << '\n';
				stats << "Device: " << std::hex << std::uppercase << this->mVendorId << ' ' << this->mDeviceId << std::nouppercase << std::dec << std::endl;
				stats << "FPS: " << (1000 / std::max(boost::chrono::duration_cast<boost::chrono::milliseconds>(frametime).count(), 1LL)) << std::endl;
				stats << "Draw Calls: " << this->mLastDrawCalls << " (" << this->mLastDrawCallVertices << " vertices)" << std::endl;
				stats << "Frame " << (this->mLastFrameCount + 1) << ": " << (frametime.count() * 1e-6f) << "ms" << std::endl;
				stats << "PostProcessing: " << (boost::chrono::duration_cast<boost::chrono::nanoseconds>(this->mLastPostProcessingDuration).count() * 1e-6f) << "ms" << std::endl;
				stats << "Timer: " << std::fmod(boost::chrono::duration_cast<boost::chrono::nanoseconds>(this->mLastPresent - this->mStartTime).count() * 1e-6f, 16777216.0f) << "ms" << std::endl;
				stats << "Network: " << sNetworkUpload << "B up / " << sNetworkDownload << "B down" << std::endl;

				nvgFillColor(this->mNVG, nvgRGB(255, 255, 255));
				nvgTextAlign(this->mNVG, NVG_ALIGN_RIGHT | NVG_ALIGN_TOP);
				nvgFontSize(this->mNVG, 16);
				nvgTextBox(this->mNVG, 0, 0, static_cast<float>(this->mWidth), stats.str().c_str(), nullptr);
			}

			nvgEndFrame(this->mNVG);

			if (timeSinceCreate.count() > 8 && this->mEffect != nullptr)
			{
				this->mStatus.clear();
				this->mMessage.clear();
			}
		}

		// Update inputs
		this->mDate[0] = static_cast<float>(tm.tm_year + 1900);
		this->mDate[1] = static_cast<float>(tm.tm_mon + 1);
		this->mDate[2] = static_cast<float>(tm.tm_mday);
		this->mDate[3] = static_cast<float>(tm.tm_hour * 3600 + tm.tm_min * 60 + tm.tm_sec + 1);

		sNetworkUpload = sNetworkDownload = 0;
		this->mLastPresent = timePresent;
		this->mLastFrameDuration = frametime;
		this->mLastFrameCount++;
		this->mLastDrawCalls = this->mLastDrawCallVertices = 0;
	}

	bool Runtime::LoadEffect()
	{
		this->mMessage.clear();
		this->mShowStatistics = false;

		boost::filesystem::path path = sEffectPath;

		if (!boost::filesystem::exists(path))
		{
			path = path.parent_path() / "ReShade.fx";
		}
		if (!boost::filesystem::exists(path))
		{
			path = path.parent_path() / "Sweet.fx";
		}
		if (!boost::filesystem::exists(path))
		{
			LOG(ERROR) << "Effect file " << sEffectPath << " does not exist.";

			this->mStatus += " No effect found!";

			return false;
		}

		tm tm;
		std::time_t time = std::time(nullptr);
		::localtime_s(&tm, &time);

		// Preprocess
		EffectPreprocessor preprocessor;
		preprocessor.AddDefine("__RESHADE__", std::to_string(VERSION_MAJOR * 10000 + VERSION_MINOR * 100 + VERSION_REVISION));
		preprocessor.AddDefine("__VENDOR__", std::to_string(this->mVendorId));
		preprocessor.AddDefine("__DEVICE__", std::to_string(this->mDeviceId));
		preprocessor.AddDefine("__RENDERER__", std::to_string(this->mRendererId));
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

		std::string errors;
		const std::string source = preprocessor.Run(path, errors);

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

		for (const std::string &pragma : preprocessor.GetPragmas())
		{
			if (boost::starts_with(pragma, "message "))
			{
				this->mMessage += pragma.substr(9, pragma.length() - 10);
			}
			else if (!boost::istarts_with(pragma, "reshade "))
			{
				continue;
			}

			const std::string command = pragma.substr(8);

			if (boost::iequals(command, "statistics") || boost::iequals(command, "showstatistics"))
			{
				this->mShowStatistics = true;
			}
			else if (boost::istarts_with(command, "screenshot_format "))
			{
				this->mScreenshotFormat = command.substr(18);
			}
		}

		if (!this->mMessage.empty())
		{
			std::size_t len = this->mMessage.length();
			EscapeString(&this->mMessage.front(), len);
			this->mMessage = this->mMessage.substr(0, len);
		}

		return true;
	}
	bool Runtime::CompileEffect()
	{
		this->mTechniques.clear();
		this->mEffect.reset();

		EffectTree ast;
		EffectParser parser(ast);

		LOG(TRACE) << "> Running parser ...";

		if (!parser.Parse(this->mEffectSource, this->mErrors))
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
			const Effect::Technique *technique = this->mEffect->GetTechnique(name);
				
			TechniqueInfo info;
			info.Technique = technique;

			info.Enabled = technique->GetAnnotation("enabled").As<bool>();
			info.Timeleft = info.Timeout = technique->GetAnnotation("timeout").As<int>();
			info.Toggle = technique->GetAnnotation("toggle").As<int>();
			info.ToggleTime = technique->GetAnnotation("toggletime").As<int>();

			this->mTechniques.push_back(std::move(info));
		}

		const auto textures = this->mEffect->GetTextures();

		for (const std::string &name : textures)
		{
			Effect::Texture *texture = this->mEffect->GetTexture(name);
			Effect::Texture::Description desc = texture->GetDescription();
			const std::string source = texture->GetAnnotation("source").As<std::string>();

			if (!source.empty())
			{
				const boost::filesystem::path path = boost::filesystem::absolute(source, sEffectPath.parent_path());
				int widthFile = 0, heightFile = 0, channelsFile = 0, channels = STBI_default;

				switch (desc.Format)
				{
					case Effect::Texture::Format::R8:
						channels = STBI_r;
						break;
					case Effect::Texture::Format::RG8:
						channels = STBI_rg;
						break;
					case Effect::Texture::Format::DXT1:
						channels = STBI_rgb;
						break;
					case Effect::Texture::Format::RGBA8:
					case Effect::Texture::Format::DXT5:
						channels = STBI_rgba;
						break;
					case Effect::Texture::Format::R32F:
					case Effect::Texture::Format::RGBA16:
					case Effect::Texture::Format::RGBA16F:
					case Effect::Texture::Format::RGBA32F:
					case Effect::Texture::Format::DXT3:
					case Effect::Texture::Format::LATC1:
					case Effect::Texture::Format::LATC2:
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
						case Effect::Texture::Format::DXT1:
							stb_compress_dxt_block(data, data, FALSE, STB_DXT_NORMAL);
							dataSize = ((desc.Width + 3) >> 2) * ((desc.Height + 3) >> 2) * 8;
							break;
						case Effect::Texture::Format::DXT5:
							stb_compress_dxt_block(data, data, TRUE, STB_DXT_NORMAL);
							dataSize = ((desc.Width + 3) >> 2) * ((desc.Height + 3) >> 2) * 16;
							break;
					}

					texture->Update(0, data, dataSize);
				}
				else
				{
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