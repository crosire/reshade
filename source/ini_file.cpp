#include "ini_file.hpp"

#include <Windows.h>

// TODO: Custom parser

namespace reshade
{
	namespace utils
	{
		ini_file::ini_file()
		{
		}
		ini_file::ini_file(const boost::filesystem::path &path) : _path(path)
		{
		}

		annotation ini_file::get(const std::string &section, const std::string &key, const annotation &default) const
		{
			char data[1024] = "";
			GetPrivateProfileStringA(section.c_str(), key.c_str(), default.as<std::string>().c_str(), data, sizeof(data), _path.string().c_str());
			return std::string(data);
		}
		void ini_file::set(const std::string &section, const std::string &key, const annotation &value)
		{
			WritePrivateProfileStringA(section.c_str(), key.c_str(), value.as<std::string>().c_str(), _path.string().c_str());
		}
	}
}
