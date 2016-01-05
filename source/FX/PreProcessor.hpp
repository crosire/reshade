#pragma once

#include <string>
#include <vector>
#include <boost\filesystem\path.hpp>

namespace ReShade
{
	namespace FX
	{
		bool preprocess(const boost::filesystem::path &path, const std::vector<std::pair<std::string, std::string>> &defines, std::vector<boost::filesystem::path> &include_paths, std::vector<std::string> &pragmas, std::string &output, std::string &errors);
	}
}
