#pragma once

#include <string>
#include <memory>
#include <boost\filesystem\path.hpp>

namespace ReShade
{
	class EffectPreprocessor
	{
	public:
		EffectPreprocessor();
		~EffectPreprocessor();

		void AddDefine(const std::string &name, const std::string &value = "1");
		void AddIncludePath(const boost::filesystem::path &path);

		inline std::string Run(const boost::filesystem::path &path)
		{
			std::string errors;
			return Run(path, errors);
		}
		std::string Run(const boost::filesystem::path &path, std::string &errors);

	private:
		struct Impl;
		std::unique_ptr<Impl> mImpl;
	};
}