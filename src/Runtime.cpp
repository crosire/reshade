#include "Log.hpp"
#include "Version.h"
#include "Runtime.hpp"
#include "HookManager.hpp"
#include "EffectTree.hpp"
#include "EffectPreprocessor.hpp"
#include "EffectParser.hpp"
#include "FileWatcher.hpp"

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
		inline std::chrono::system_clock::time_point GetLastWriteTime(const boost::filesystem::path &path)
		{
			WIN32_FILE_ATTRIBUTE_DATA attributes;
			
			if (!::GetFileAttributesEx(path.c_str(), GetFileExInfoStandard, reinterpret_cast<LPVOID>(&attributes)))
			{
				return std::chrono::system_clock::time_point();
			}

			ULARGE_INTEGER ull;
			ull.LowPart = attributes.ftLastWriteTime.dwLowDateTime;
			ull.HighPart = attributes.ftLastWriteTime.dwHighDateTime;

			return std::chrono::system_clock::time_point(std::chrono::system_clock::duration(ull.QuadPart));
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

		ReHook::Register(systemPath / "d3d8.dll");
		ReHook::Register(systemPath / "d3d9.dll");
		ReHook::Register(systemPath / "d3d10.dll");
		ReHook::Register(systemPath / "d3d10_1.dll");
		ReHook::Register(systemPath / "d3d11.dll");
		ReHook::Register(systemPath / "dxgi.dll");
		ReHook::Register(systemPath / "opengl32.dll");

		sEffectWatcher = new FileWatcher(sEffectPath.parent_path(), true);

		LOG(INFO) << "Initialized.";
	}
	void Runtime::Shutdown()
	{
		LOG(INFO) << "Exiting ...";

		delete sEffectWatcher;

		ReHook::Cleanup();

		LOG(INFO) << "Exited.";
	}

	// -----------------------------------------------------------------------------------------------------

	Runtime::Runtime() : mWidth(0), mHeight(0), mVendorId(0), mDeviceId(0), mRendererId(0), mLastFrameCount(0), mNVG(nullptr)
	{
		this->mStartTime = std::chrono::high_resolution_clock::now();
	}
	Runtime::~Runtime()
	{
		OnDelete();
	}

	bool Runtime::OnCreate(unsigned int width, unsigned int height)
	{
		if (this->mEffect != nullptr)
		{
			return false;
		}
		if (width == 0 || height == 0)
		{
			LOG(WARNING) << "Failed to reload effects due to invalid size of " << width << "x" << height << ".";

			return false;
		}

		this->mWidth = width;
		this->mHeight = height;

		if (this->mNVG != nullptr)
		{
			nvgCreateFont(this->mNVG, "Courier", (GetWindowsDirectory() / "Fonts" / "cour.ttf").string().c_str());
		}

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

			return false;
		}

		// Preprocess
		EffectPreprocessor preprocessor;
		preprocessor.AddDefine("__RESHADE__", std::to_string(VERSION_MAJOR * 10000 + VERSION_MINOR * 100 + VERSION_REVISION));
		preprocessor.AddDefine("__VENDOR__", std::to_string(this->mVendorId));
		preprocessor.AddDefine("__DEVICE__", std::to_string(this->mDeviceId));
		preprocessor.AddDefine("__RENDERER__", std::to_string(this->mRendererId));
		preprocessor.AddDefine("BUFFER_WIDTH", std::to_string(width));
		preprocessor.AddDefine("BUFFER_HEIGHT", std::to_string(height));
		preprocessor.AddDefine("BUFFER_RCP_WIDTH", std::to_string(1.0f / static_cast<float>(width)));
		preprocessor.AddDefine("BUFFER_RCP_HEIGHT", std::to_string(1.0f / static_cast<float>(height)));
		preprocessor.AddIncludePath(sEffectPath.parent_path());

		LOG(INFO) << "Loading effect from " << ObfuscatePath(path) << " ...";
		LOG(TRACE) << "> Running preprocessor ...";

		this->mErrors.clear();
		this->mMessage.clear();

		const std::string source = preprocessor.Run(path, errors);
		
		if (source.empty())
		{
			LOG(ERROR) << "Failed to preprocess effect on context " << this << ":\n\n" << this->mErrors << "\n";

			return false;
		}

		// Parse
		EffectTree ast;
		EffectParser parser(ast);

		LOG(TRACE) << "> Running parser ...";

		if (!parser.Parse(source, this->mErrors))
		{
			LOG(ERROR) << "Failed to compile effect on context " << this << ":\n\n" << this->mErrors << "\n";

			return false;
		}

		for (const std::string &pragma : parser.GetPragmas())
		{
			if (boost::starts_with(pragma, "message "))
			{
				this->mMessage += pragma.substr(9, pragma.length() - 10);
			}
			else if (!boost::istarts_with(pragma, "reshade "))
			{
				continue;
			}
		}

		// Compile
		LOG(TRACE) << "> Running compiler ...";

		std::unique_ptr<Effect> effect = CreateEffect(ast, this->mErrors);

		if (effect == nullptr)
		{
			LOG(ERROR) << "Failed to compile effect on context " << this << ":\n\n" << this->mErrors << "\n";

			return false;
		}
		else if (!this->mErrors.empty())
		{
			LOG(WARNING) << "> Successfully compiled effect with warnings:" << "\n\n" << this->mErrors << "\n";
		}
		else
		{
			LOG(INFO) << "> Successfully compiled effect.";
		}

		// Process
		const auto techniques = effect->GetTechniqueNames();

		if (techniques.empty())
		{
			LOG(WARNING) << "> Effect doesn't contain any techniques. Skipped.";

			return false;
		}
		else
		{
			this->mTechniques.reserve(techniques.size());

			for (const std::string &name : techniques)
			{
				const Effect::Technique *technique = effect->GetTechnique(name);
				
				InfoTechnique info;
				info.Enabled = technique->GetAnnotation("enabled").As<bool>();
				info.Timeleft = info.Timeout = technique->GetAnnotation("timeout").As<int>();
				info.Toggle = technique->GetAnnotation("toggle").As<int>();
				info.ToggleTime = technique->GetAnnotation("toggletime").As<int>();

				this->mTechniques.push_back(std::make_pair(technique, info));
			}
		}

		this->mEffect.swap(effect);

		CreateResources();

		this->mStartTime = std::chrono::high_resolution_clock::now();

		LOG(INFO) << "Recreated effect environment on context " << this << ".";

		return true;
	}
	void Runtime::OnDelete()
	{
		if (this->mEffect == nullptr)
		{
			return;
		}

		this->mTechniques.clear();
		this->mColorTargets.clear();

		this->mEffect.reset();

		LOG(INFO) << "Destroyed effect environment on context " << this << ".";
	}
	void Runtime::OnPresent()
	{
		for (auto &it : this->mTechniques)
		{
			const Effect::Technique *technique = it.first;
			InfoTechnique &info = it.second;

			tm tm;
			std::time_t t = std::chrono::system_clock::to_time_t(this->mLastPresent);
			::localtime_s(&tm, &t);
			const float date[4] = { static_cast<float>(tm.tm_year + 1900), static_cast<float>(tm.tm_mon + 1), static_cast<float>(tm.tm_mday), static_cast<float>(tm.tm_hour * 3600 + tm.tm_min * 60 + tm.tm_sec + 1) };

			if (info.ToggleTime != 0 && info.ToggleTime == static_cast<int>(date[3]))
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
				info.Timeleft -= static_cast<unsigned int>(std::chrono::duration_cast<std::chrono::milliseconds>(this->mLastFrametime).count());

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

			unsigned int passes = 0;

			if (technique->Begin(passes))
			{
				for (unsigned int i = 0; i < passes; ++i)
				{
					for (auto &target : this->mColorTargets)
					{
						target->UpdateFromColorBuffer();
					}
					for (const std::string &name : this->mEffect->GetConstantNames())
					{
						Effect::Constant *constant = this->mEffect->GetConstant(name);
						const std::string source = constant->GetAnnotation("source").As<std::string>();

						if (source.empty())
						{
							continue;
						}
						else if (source == "frametime")
						{
							const unsigned int frametime = static_cast<unsigned int>(std::chrono::duration_cast<std::chrono::milliseconds>(this->mLastFrametime).count());

							constant->SetValue(&frametime, 1);
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
							constant->SetValue(date, 4);
						}
						else if (source == "timer")
						{
							const unsigned long long timer = std::chrono::duration_cast<std::chrono::milliseconds>(this->mLastPresent.time_since_epoch()).count();

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
									const float timerFloat = static_cast<float>(timer % 16777216);
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

					technique->RenderPass(i);
				}

				technique->End();
			}
			else
			{
				LOG(ERROR) << "Failed to start rendering technique!";
			}
		}

		const auto time = std::chrono::high_resolution_clock::now();
		const std::chrono::milliseconds frametime = std::chrono::duration_cast<std::chrono::milliseconds>(time - this->mLastPresent);

		if (::GetAsyncKeyState(VK_SNAPSHOT) & 0x8000)
		{
			tm tm;
			const time_t t = std::chrono::system_clock::to_time_t(time);
			::localtime_s(&tm, &t);
			char timeString[128];
			std::strftime(timeString, 128, "%Y-%m-%d %H-%M-%S", &tm); 

			CreateScreenshot(sExecutablePath.parent_path() / (sExecutablePath.stem().string() + ' ' + timeString + ".png"));
		}

		std::vector<FileWatcher::Change> changes;

		if (sEffectWatcher->GetChanges(changes, 0))
		{
			for (const auto &change : changes)
			{
				const boost::filesystem::path &path = change.Filename;
				const boost::filesystem::path extension = path.extension();
					
				if (extension == ".fx" || extension == ".txt" || extension == ".h")
				{
					LOG(INFO) << "Detected modification to " << ObfuscatePath(path) << ". Reloading ...";
			
					OnDelete();
					OnCreate(this->mWidth, this->mHeight);
					return;
				}
			}
		}

		if (this->mNVG != nullptr)
		{
			nvgBeginFrame(this->mNVG, this->mWidth, this->mHeight, 1);

			const std::chrono::seconds timeSinceStart = std::chrono::duration_cast<std::chrono::seconds>(time - this->mStartTime);

			nvgFontFace(this->mNVG, "Courier");
			nvgTextAlign(this->mNVG, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);

			float bounds[4] = { 0, 0, 0, 0 };

			if (timeSinceStart.count() < 8)
			{
				std::string message = "ReShade by Crosire\nVersion " BOOST_STRINGIZE(VERSION_FULL);

				if (!this->mMessage.empty())
				{
					message += "\n\n" + this->mMessage;
				}

				nvgFillColor(this->mNVG, nvgRGB(255, 255, 255));
				nvgTextBoxBounds(this->mNVG, 0, 0, static_cast<float>(this->mWidth), message.c_str(), nullptr, bounds);
				nvgTextBox(this->mNVG, 0, 0, static_cast<float>(this->mWidth), message.c_str(), nullptr);
			}
			if (!this->mErrors.empty())
			{
				if (this->mEffect == nullptr)
				{
					nvgFillColor(this->mNVG, nvgRGB(255, 0, 0));
					nvgTextBox(this->mNVG, 0, bounds[3], static_cast<float>(this->mWidth), ("ReShade failed to compile effect:\n\n" + this->mErrors).c_str(), nullptr);
				}
				else if (timeSinceStart.count() < 6)
				{
					nvgFillColor(this->mNVG, nvgRGB(255, 255, 0));
					nvgTextBox(this->mNVG, 0, bounds[3], static_cast<float>(this->mWidth), ("ReShade successfully compiled effect with warnings:\n\n" + this->mErrors).c_str(), nullptr);
				}
			}

			nvgEndFrame(this->mNVG);
		}

		this->mLastPresent = time;
		this->mLastFrametime = frametime;
		this->mLastFrameCount++;
	}

	void Runtime::CreateResources()
	{
		const auto textures = this->mEffect->GetTextureNames();

		for (const std::string &name : textures)
		{
			Effect::Texture *texture = this->mEffect->GetTexture(name);
			Effect::Texture::Description desc = texture->GetDescription();
			const std::string source = texture->GetAnnotation("source").As<std::string>();

			if (source.empty())
			{
				continue;
			}
			else if (source == "backbuffer")
			{
				if (desc.Width == this->mWidth && desc.Height == this->mHeight && desc.Format == Effect::Texture::Format::RGBA8)
				{
					this->mColorTargets.push_back(texture);
				}
				else
				{
					LOG(ERROR) << "> Texture '" << name << "' (Width = " << desc.Width << ", Height = " << desc.Height << ") doesn't match backbuffer requirements (Width = " << this->mWidth << ", Height = " << this->mHeight << ", Format = R8G8B8A8).";
				}
			}
			else
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

		if (!stbi_write_png(path.string().c_str(), this->mWidth, this->mHeight, 4, data, 0))
		{
			LOG(ERROR) << "Failed to write screenshot to " << ObfuscatePath(path) << "!";
		}

		delete[] data;
	}
}