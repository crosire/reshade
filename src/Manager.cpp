#include "Log.hpp"
#include "Version.h"
#include "Manager.hpp"
#include "HookManager.hpp"

#include <stb_dxt.h>
#include <stb_image.h>
#include <stb_image_write.h>
#include <stb_image_resize.h>
#include <boost\filesystem\operations.hpp>
#include <boost\assign\list_of.hpp>
#include <boost\algorithm\string.hpp>

namespace ReShade
{
	namespace
	{
		boost::filesystem::path									ObfuscatePath(const boost::filesystem::path &path)
		{
			char username[257];
			DWORD usernameSize = ARRAYSIZE(username);
			::GetUserNameA(username, &usernameSize);

			std::string result = path.string();
			boost::algorithm::replace_all(result, username, std::string(usernameSize - 1, '*'));

			return result;
		}

		boost::filesystem::path									sExecutablePath, sShaderPath;
	}

	bool														Manager::Initialize(const boost::filesystem::path &executablePath, const boost::filesystem::path &injectorPath, const boost::filesystem::path &systemPath)
	{
		sExecutablePath = executablePath;
		sShaderPath = injectorPath;
		sShaderPath.replace_extension("fx");

		// Initialize logger
		boost::filesystem::path logPath = injectorPath;
		logPath.replace_extension("log");

		el::Configurations logConfig;
		logConfig.set(el::Level::Global, el::ConfigurationType::Enabled, "true");
#ifndef _DEBUG
		logConfig.set(el::Level::Trace, el::ConfigurationType::Enabled, "false");
#endif
		logConfig.set(el::Level::Global, el::ConfigurationType::Filename, logPath.string());
		logConfig.set(el::Level::Global, el::ConfigurationType::ToFile, "true");
		logConfig.set(el::Level::Global, el::ConfigurationType::ToStandardOutput, "false");
		logConfig.set(el::Level::Global, el::ConfigurationType::MaxLogFileSize, "0");
		logConfig.set(el::Level::Global, el::ConfigurationType::LogFlushThreshold, "0");
		logConfig.set(el::Level::Global, el::ConfigurationType::Format, "%datetime | %level | %msg");
		el::Loggers::reconfigureLogger("default", logConfig);

		// Initialize injector
		LOG(INFO) << "Initializing version '" BOOST_STRINGIZE(VERSION_FULL) "' built on '" << VERSION_DATE << " " << VERSION_TIME << "' loaded from " << ObfuscatePath(injectorPath) << " to " << ObfuscatePath(executablePath) << " ...";

		ReHook::Register(systemPath / "d3d8.dll");
		ReHook::Register(systemPath / "d3d9.dll");
		ReHook::Register(systemPath / "d3d10.dll");
		ReHook::Register(systemPath / "d3d10_1.dll");
		ReHook::Register(systemPath / "d3d11.dll");
		ReHook::Register(systemPath / "dxgi.dll");
		ReHook::Register(systemPath / "dinput8.dll");
		ReHook::Register(systemPath / "opengl32.dll");

		LOG(INFO) << "Initialized.";

		return true;
	}
	void														Manager::Exit(void)
	{
		LOG(INFO) << "Exiting ...";

		ReHook::Cleanup();

		LOG(INFO) << "Exited.";
	}

	// -----------------------------------------------------------------------------------------------------

	Manager::Manager(std::shared_ptr<EffectContext> context) : mEffectContext(context), mCreated(false), mEnabled(true), mSelectedTechnique(nullptr)
	{
		LOG(INFO) << "Acquiring effect context " << this->mEffectContext << " ...";

		OnCreate();
	}
	Manager::~Manager(void)
	{
		OnDelete();

		LOG(INFO) << "Releasing effect context " << this->mEffectContext << ".";
	}

	bool														Manager::OnCreate(void)
	{
		if (this->mCreated)
		{
			LOG(WARNING) << "Effect environment on context " << this->mEffectContext << " already created. Skipped recreation.";

			return true;
		}

		LOG(INFO) << "Loading effect from " << ObfuscatePath(sShaderPath) << " ...";

		std::string errors;
		unsigned int bufferWidth = 0, bufferHeight = 0;
		this->mEffectContext->GetDimension(bufferWidth, bufferHeight);
		const std::vector<std::pair<std::string, std::string>> defines = boost::assign::list_of<std::pair<std::string, std::string>>
			("RESHADE", "")
			("BUFFER_WIDTH", std::to_string(bufferWidth))
			("BUFFER_HEIGHT", std::to_string(bufferHeight))
			("BUFFER_RCP_WIDTH", "(1.0f / BUFFER_WIDTH)")
			("BUFFER_RCP_HEIGHT", "(1.0f / BUFFER_HEIGHT)");

		std::unique_ptr<Effect> effect = this->mEffectContext->Compile(sShaderPath, defines, errors);

		if (effect == nullptr)
		{
			LOG(ERROR) << "Failed to compile effect on context " << this->mEffectContext << ":\n\n" << errors << "\n";

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

		const auto techniques = effect->GetTechniqueNames();

		if (techniques.empty())
		{
			LOG(WARNING) << "> Effect doesn't contain any techniques. Skipped.";

			return false;
		}
		else
		{
			this->mSelectedTechnique = effect->GetTechnique(techniques.front());
		}

		#pragma region Parse Texture Annotations
		for (const std::string &name : effect->GetTextureNames())
		{
			Effect::Texture *texture = effect->GetTexture(name);
			Effect::Texture::Description desc = texture->GetDescription();
			const std::string source = texture->GetAnnotation("source").As<std::string>();

			if (source.empty())
			{
				continue;
			}
			else if (source == "backbuffer")
			{
				if (desc.Width == bufferWidth && desc.Height == bufferHeight && desc.Format == Effect::Texture::Format::RGBA8)
				{
					this->mColorTargets.push_back(texture);
				}
				else
				{
					LOG(ERROR) << "> Texture '" << name << "' doesn't match backbuffer requirements (Width = " << bufferWidth << ", Height = " << bufferHeight << ", Format = R8G8B8A8).";
				}
			}
			else if (source == "depthbuffer")
			{
				if (desc.Width == bufferWidth && desc.Height == bufferHeight && desc.Format == Effect::Texture::Format::R8)
				{
					this->mDepthTargets.push_back(texture);
				}
				else
				{
					LOG(ERROR) << "> Texture '" << name << "' doesn't match depthbuffer requirements (Width = " << bufferWidth << ", Height = " << bufferHeight << ", Format = R8).";
				}
			}
			else
			{
				const boost::filesystem::path path = boost::filesystem::absolute(source, sShaderPath.parent_path());
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
					LOG(ERROR) << "> Source " << ObfuscatePath(path) << " for texture '" << name << "' could not be loaded! Make sure it exists and is saved in a compatible format.";
				}

				delete[] data;
			}
		}
		#pragma endregion

		LOG(INFO) << "Recreated effect environment on context " << this->mEffectContext << ".";

		this->mEffect.swap(effect);

		return this->mCreated = true;
	}
	void														Manager::OnDelete(void)
	{
		if (!this->mCreated)
		{
			return;
		}

		this->mColorTargets.clear();
		this->mDepthTargets.clear();

		this->mEffect.reset();

		LOG(INFO) << "Destroyed effect environment on context " << this->mEffectContext << ".";

		this->mCreated = false;
	}
	void														Manager::OnPostProcess(void)
	{
		if (!(this->mCreated && this->mEnabled) || this->mSelectedTechnique == nullptr)
		{
			return;
		}

		unsigned int passes = 0;

		if (this->mSelectedTechnique->Begin(passes))
		{
			for (unsigned int i = 0; i < passes; ++i)
			{
				for (auto &target : this->mColorTargets)
				{
					target->UpdateFromColorBuffer();
				}
				for (auto &target : this->mDepthTargets)
				{
					target->UpdateFromDepthBuffer();
				}

				this->mSelectedTechnique->RenderPass(i);
			}

			this->mSelectedTechnique->End();
		}
		else
		{
			LOG(ERROR) << "Failed to start rendering selected technique!";
		}
	}
	void														Manager::OnPresent(void)
	{
		if (GetAsyncKeyState(VK_SNAPSHOT) & 0x8000)
		{
			unsigned int width = 0, height = 0;
			this->mEffectContext->GetDimension(width, height);

			const std::size_t dataSize = width * height * 4;
			unsigned char *data = new unsigned char[dataSize];
			this->mEffectContext->GetScreenshot(data, dataSize);

			tm tm;
			const time_t t = ::time(nullptr);
			::localtime_s(&tm, &t);
			char timeString[128];
			::strftime(timeString, 128, "(%Y-%m-%d %H.%M.%S)", &tm); 

			const boost::filesystem::path path = sExecutablePath.parent_path() / (sExecutablePath.stem().string() + ' ' + timeString + ".png");

			LOG(INFO) << "Saving screenshot to " << ObfuscatePath(path) << " ...";

			if (!stbi_write_png(path.string().c_str(), width, height, 4, data, 0))
			{
				LOG(ERROR) << "Failed to write screenshot to " << ObfuscatePath(path) << "!";
			}

			delete[] data;
		}
	}
}