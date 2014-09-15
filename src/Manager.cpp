#include "Log.hpp"
#include "Version.h"
#include "Manager.hpp"
#include "HookManager.hpp"

namespace ReShade
{
	bool														Manager::Initialize(const boost::filesystem::path &executablePath, const boost::filesystem::path &injectorPath, const boost::filesystem::path &systemPath)
	{
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
		LOG(INFO) << "Initializing version '" BOOST_STRINGIZE(VERSION_FULL) " [" << VERSION_DATE << ":" << VERSION_TIME << "]' loaded from " << injectorPath << " for " << executablePath << " ...";

		LOG(INFO) << "Initialized.";

		return true;
	}
	void														Manager::Exit(void)
	{
		LOG(INFO) << "Exiting ...";

		ReHook::Cleanup();

		LOG(INFO) << "Exited.";
	}
}