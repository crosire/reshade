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

			void AddDefine(const std::string &name, const std::string &value = "1");
			void AddIncludePath(const boost::filesystem::path &path);

			std::string Run(const boost::filesystem::path &path, std::string &errors, std::vector<std::string> &pragmas, std::vector<boost::filesystem::path> &includes);

		private:
			struct Impl;
			std::unique_ptr<Impl> _impl;
		};
	}
}
