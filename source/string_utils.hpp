#pragma once

#include <vector>
#include <string>
#include <sstream>

namespace
{
	std::vector<std::string> split(const std::string &s, char delim)
	{
		std::vector<std::string> output;
		std::string item;
		std::stringstream ss(s);

		while (std::getline(ss, item, delim))
		{
			if (!item.empty())
			{
				output.push_back(item);
			}
		}

		return output;
	}

	std::string &trim(std::string &str, const char *chars = " \t")
	{
		str.erase(0, str.find_first_not_of(chars));
		str.erase(str.find_last_not_of(chars) + 1);
		return str;
	}
	std::string trim(const std::string &str, const char *chars = " \t")
	{
		std::string res(str);
		return trim(res, chars);
	}
}
