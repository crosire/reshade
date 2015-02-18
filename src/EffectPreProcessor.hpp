#pragma once

#include <string>
#include <memory>
#include <vector>
#include <boost\filesystem\path.hpp>

namespace ReShade
{
	namespace FX
	{
		class PreProcessor
		{
		public:
			PreProcessor();
			~PreProcessor();

			inline const std::vector<std::string> &GetPragmas() const
			{
				return this->mPragmas;
			}
			inline const std::vector<boost::filesystem::path> &GetIncludes() const
			{
				return this->mIncludes;
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
			std::vector<std::string> mPragmas;
			std::vector<boost::filesystem::path> mIncludes;
		};
	}
}