#pragma once

#include <string>
#include <fstream>
#include <boost\filesystem\path.hpp>
#include <fpp.h>

namespace ReShade
{
	class														EffectPreprocessor
	{
	public:
		EffectPreprocessor();

		void													AddDefine(const std::string &name, const std::string &value = "");
		void													AddIncludePath(const boost::filesystem::path &path);

		std::string												Run(const boost::filesystem::path &path, std::string &errors);

	private:
		static char *											OnInput(char *buffer, int size, void *userdata);
		static void												OnOutput(int ch, void *userdata);
		static void												OnError(void *userdata, char *fmt, va_list args);

	private:
		std::vector<fppTag>										mTags;
		std::fstream											mInput;
		std::string												mOutput;
		std::string												mErrors;
		std::vector<char>										mScratch;
		std::size_t												mScratchCursor;
	};
}