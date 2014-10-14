#include "Log.hpp"
#include "Version.h"
#include "Runtime.hpp"
#include "HookManager.hpp"
#include "EffectTree.hpp"
#include "EffectPreprocessor.hpp"
#include "EffectParser.hpp"

#include <stb_dxt.h>
#include <stb_image.h>
#include <stb_image_write.h>
#include <stb_image_resize.h>
#include <boost\algorithm\string\replace.hpp>
#include <boost\filesystem\path.hpp>
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

		boost::filesystem::path sExecutablePath, sInjectorPath, sEffectPath;
	}

	// -----------------------------------------------------------------------------------------------------

	void Runtime::Startup(const boost::filesystem::path &executablePath, const boost::filesystem::path &injectorPath)
	{
		sExecutablePath = executablePath;
		sEffectPath = injectorPath;
		sEffectPath.replace_extension("fx");
		boost::filesystem::path systemPath = GetSystemDirectory();

		boost::filesystem::path logPath = injectorPath;
		logPath.replace_extension("log");

		el::Configurations logConfig;
		logConfig.set(el::Level::Global, el::ConfigurationType::Enabled, "true");
#ifndef _DEBUG
		DeleteFile(logPath.c_str());
		//logConfig.set(el::Level::Trace, el::ConfigurationType::Enabled, "false");
#endif
		logConfig.set(el::Level::Global, el::ConfigurationType::Filename, logPath.string());
		logConfig.set(el::Level::Global, el::ConfigurationType::ToFile, "true");
		logConfig.set(el::Level::Global, el::ConfigurationType::ToStandardOutput, "false");
		logConfig.set(el::Level::Global, el::ConfigurationType::MaxLogFileSize, "0");
		logConfig.set(el::Level::Global, el::ConfigurationType::LogFlushThreshold, "0");
		logConfig.set(el::Level::Global, el::ConfigurationType::Format, "%datetime | %level | %msg");
		el::Loggers::reconfigureLogger("default", logConfig);

		LOG(INFO) << "Initializing version '" BOOST_STRINGIZE(VERSION_FULL) "' built on '" << VERSION_DATE << " " << VERSION_TIME << "' loaded from " << ObfuscatePath(injectorPath) << " to " << ObfuscatePath(executablePath) << " ...";

		ReHook::Register(systemPath / "d3d8.dll");
		ReHook::Register(systemPath / "d3d9.dll");
		ReHook::Register(systemPath / "d3d10.dll");
		ReHook::Register(systemPath / "d3d10_1.dll");
		ReHook::Register(systemPath / "d3d11.dll");
		ReHook::Register(systemPath / "dxgi.dll");
		ReHook::Register(systemPath / "opengl32.dll");

		LOG(INFO) << "Initialized.";
	}
	void Runtime::Shutdown(void)
	{
		LOG(INFO) << "Exiting ...";

		ReHook::Cleanup();

		LOG(INFO) << "Exited.";
	}

	// -----------------------------------------------------------------------------------------------------

	Runtime::Runtime() : mWidth(0), mHeight(0)
	{
	}
	Runtime::~Runtime()
	{
		OnDelete();
	}

	bool Runtime::ReCreate()
	{
		return ReCreate(this->mWidth, this->mHeight);
	}
	bool Runtime::ReCreate(unsigned int width, unsigned int height)
	{
		OnDelete();
		return OnCreate(width, height);
	}
	bool Runtime::OnCreate(unsigned int width, unsigned int height)
	{
		if (this->mEffect != nullptr)
		{
			return false;
		}
		if (width == 0 || height == 0)
		{
			return false;
		}

		std::string errors;

		// Preprocess
		EffectPreprocessor preprocessor;
		preprocessor.AddDefine("RESHADE");
		preprocessor.AddDefine("BUFFER_WIDTH", std::to_string(width));
		preprocessor.AddDefine("BUFFER_HEIGHT", std::to_string(height));
		preprocessor.AddDefine("BUFFER_RCP_WIDTH", std::to_string(1.0f / static_cast<float>(width)));
		preprocessor.AddDefine("BUFFER_RCP_HEIGHT", std::to_string(1.0f / static_cast<float>(height)));
		preprocessor.AddIncludePath(sEffectPath.parent_path());

		LOG(INFO) << "Loading effect from " << ObfuscatePath(sEffectPath) << " ...";
		LOG(TRACE) << "> Running preprocessor ...";

		const std::string source = preprocessor.Run(sEffectPath, errors);
		
		if (source.empty())
		{
			LOG(ERROR) << "Failed to preprocess effect on context " << this << ":\n\n" << errors << "\n";

			return false;
		}

		// Parse
		EffectTree ast;
		EffectParser parser(ast);

		LOG(TRACE) << "> Running parser ...";

		if (!parser.Parse(source, errors))
		{
			LOG(ERROR) << "Failed to compile effect on context " << this << ":\n\n" << errors << "\n";

			return false;
		}

		// Compile
		LOG(TRACE) << "> Running compiler ...";

		std::unique_ptr<Effect> effect = CreateEffect(ast, errors);

		if (effect == nullptr)
		{
			LOG(ERROR) << "Failed to compile effect on context " << this << ":\n\n" << errors << "\n";

			return false;
		}
		else if (!errors.empty())
		{
			LOG(WARNING) << "> Successfully compiled effect with warnings:" << "\n\n" << errors << "\n";
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

			for (const auto &name : techniques)
			{
				this->mTechniques.push_back(std::make_pair(false, effect->GetTechnique(name)));
			}

			this->mTechniques.front().first = true;
		}

		this->mWidth = width;
		this->mHeight = height;
		this->mEffect.swap(effect);

		CreateResources();

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
	void Runtime::OnPostProcess()
	{
		const auto time = std::chrono::high_resolution_clock::now();

		for (auto &it : this->mTechniques)
		{
			bool &enabled = it.first;
			const Effect::Technique *technique = it.second;

			const int toggleKey = technique->GetAnnotation("toggle").As<int>();

			if (toggleKey != 0 && ::GetAsyncKeyState(toggleKey) & 0x8000)
			{
				::Sleep(200);
				enabled = !enabled;
			}

			if (!enabled)
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

					technique->RenderPass(i);
				}

				technique->End();
			}
			else
			{
				LOG(ERROR) << "Failed to start rendering technique!";
			}
		}

		this->mLastPostProcessTime = std::chrono::high_resolution_clock::now() - time;
	}
	void Runtime::OnPresent()
	{
		const auto time = std::chrono::high_resolution_clock::now();
		const std::chrono::seconds uptime = std::chrono::duration_cast<std::chrono::seconds>(time.time_since_epoch());
		const std::chrono::milliseconds frametime = std::chrono::duration_cast<std::chrono::milliseconds>(time - this->mLastPresent);

		if (::GetAsyncKeyState(VK_SNAPSHOT) & 0x8000)
		{
			tm tm;
			const time_t t = std::chrono::system_clock::to_time_t(time);
			::localtime_s(&tm, &t);
			char timeString[128];
			std::strftime(timeString, 128, "(%Y-%m-%d %H.%M.%S)", &tm); 

			CreateScreenshot(sExecutablePath.parent_path() / (sExecutablePath.stem().string() + ' ' + timeString + ".png"));
		}

		if (uptime.count() % 5 == 0)
		{
			const std::chrono::system_clock::time_point writetime = GetLastWriteTime(sEffectPath);

			if (writetime > this->mLastEffectModification)
			{
				ReCreate();

				this->mLastEffectModification = writetime;
			}
		}

		this->mLastPresent = time;
		this->mLastFrametime = frametime;
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
					if (widthFile != desc.Width || heightFile != desc.Height)
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