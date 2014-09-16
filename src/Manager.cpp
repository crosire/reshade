#include "Log.hpp"
#include "Version.h"
#include "Manager.hpp"
#include "HookManager.hpp"

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

		this->mEffect = this->mEffectContext->Compile(shaderPath, defines, errors);

		if (this->mEffect == nullptr)
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

		const auto techniques = this->mEffect->GetTechniqueNames();

		if (techniques.empty())
		{
			LOG(WARNING) << "> Effect doesn't contain any techniques. Skipped.";

			this->mEffect.reset();

			return false;
		}
		else
		{
			this->mSelectedTechnique = this->mEffect->GetTechnique(techniques.front());
		}

		LOG(INFO) << "Recreated effect environment on context " << this->mEffectContext << ".";

		return this->mCreated = true;
	}
	void														Manager::OnDelete(void)
	{
		if (!this->mCreated)
		{
			return;
		}

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