#pragma once

#include <string>
#include <vector>
#include <memory>
#include <boost\filesystem\path.hpp>

namespace ReShade
{
	class EffectPreprocessor
	{
	public:
		EffectPreprocessor();
		~EffectPreprocessor();

		inline const std::vector<boost::filesystem::path> &GetIncludes() const
		{
			return this->mIncludes;
		}
		inline const std::vector<std::string> &GetPragmas() const
		{
			return this->mPragmas;
		}

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
		std::vector<boost::filesystem::path> mIncludes;
		std::vector<std::string> mPragmas;
	};
}