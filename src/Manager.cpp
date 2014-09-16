#include "Log.hpp"
#include "Version.h"
#include "Manager.hpp"
#include "HookManager.hpp"

#include <stb_dxt.h>
#include <stb_image.h>
#include <boost\filesystem\operations.hpp>
#include <boost\assign\list_of.hpp>

namespace ReShade
{
	namespace
	{
		boost::filesystem::path									shaderPath;
	}

	bool														Manager::Initialize(const boost::filesystem::path &executablePath, const boost::filesystem::path &injectorPath, const boost::filesystem::path &systemPath)
	{
		shaderPath = injectorPath;
		shaderPath.replace_extension("fx");

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
		logConfig.set(el::Level::Global, el::ConfigurationType::Format, "%datetime [%thread] | %level | %msg");
		el::Loggers::reconfigureLogger("default", logConfig);

		// Initialize injector
		LOG(INFO) << "Initializing version '" BOOST_STRINGIZE(VERSION_FULL) "' built on '" << VERSION_DATE << ":" << VERSION_TIME << "' loaded from " << injectorPath << " for " << executablePath << " ...";

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
		LOG(INFO) << "Acquiring effect context " << this->mEffectContext << "...";

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

		LOG(INFO) << "Loading effect from " << shaderPath << " ...";

		std::string errors;
		unsigned int bufferWidth = 0, bufferHeight = 0;
		this->mEffectContext->GetDimension(bufferWidth, bufferHeight);
		const std::vector<std::pair<std::string, std::string>> defines = boost::assign::list_of<std::pair<std::string, std::string>>
			("RESHADE", "")
			("BUFFER_WIDTH", std::to_string(bufferWidth))
			("BUFFER_HEIGHT", std::to_string(bufferHeight))
			("BUFFER_RCP_WIDTH", "(1.0f / BUFFER_WIDTH)")
			("BUFFER_RCP_HEIGHT", "(1.0f / BUFFER_HEIGHT)");

		std::unique_ptr<Effect> effect = this->mEffectContext->Compile(shaderPath, defines, errors);

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
				if (desc.Width == bufferWidth && desc.Height == bufferHeight && desc.Depth == 1 && desc.Format == Effect::Texture::Format::RGBA8)
				{
					this->mColorTargets.push_back(texture);
				}
				else
				{
					LOG(ERROR) << "> Texture '" << name << "' doesn't match backbuffer requirements (Width = " << bufferWidth << ", Height = " << bufferHeight << ", Depth = 1, Format = R8G8B8A8).";
				}
			}
			else if (source == "depthbuffer")
			{
				if (desc.Width == bufferWidth && desc.Height == bufferHeight && desc.Depth == 1 && desc.Format == Effect::Texture::Format::R8)
				{
					this->mDepthTargets.push_back(texture);
				}
				else
				{
					LOG(ERROR) << "> Texture '" << name << "' doesn't match depthbuffer requirements (Width = " << bufferWidth << ", Height = " << bufferHeight << ", Depth = 1, Format = R8).";
				}
			}
			else if (boost::filesystem::exists(source))
			{
				int width = 0, height = 0, channels = STBI_default;

				switch (desc.Format)
				{
					case Effect::Texture::Format::R8:
						channels = STBI_grey;
						break;
					case Effect::Texture::Format::RG8:
						channels = STBI_grey_alpha;
						break;
					case Effect::Texture::Format::DXT1:
						channels = STBI_rgb;
						break;
					case Effect::Texture::Format::RGBA8:
					case Effect::Texture::Format::DXT5:
						channels = STBI_rgb_alpha;
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

				unsigned char *data = stbi_load(source.c_str(), &width, &height, &channels, channels);
				desc.Size = width * height * channels;

				switch (desc.Format)
				{
					case Effect::Texture::Format::DXT1:
						stb_compress_dxt_block(data, data, FALSE, STB_DXT_NORMAL);
						desc.Size = ((width + 3) >> 2) * ((height + 3) >> 2) * 8;
						break;
					case Effect::Texture::Format::DXT5:
						stb_compress_dxt_block(data, data, TRUE, STB_DXT_NORMAL);
						desc.Size = ((width + 3) >> 2) * ((height + 3) >> 2) * 16;
						break;
				}

				if (desc.Width == 1 && desc.Height == 1 && desc.Depth == 1)
				{
					desc.Width = width;
					desc.Height = height;

					texture->Resize(desc);
				}
				
				texture->Update(0, data, desc.Size);

				stbi_image_free(data);
			}
			else
			{
				LOG(ERROR) << "> Source " << boost::filesystem::absolute(source) << " for texture '" << name << "' could not be found.";
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
	}
}