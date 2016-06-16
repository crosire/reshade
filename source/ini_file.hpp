#pragma once

#include "filesystem.hpp"
#include "variant.hpp"
#include <unordered_map>

namespace reshade
{
	class ini_file
	{
	public:
		explicit ini_file(const filesystem::path &path);
		~ini_file();

		void load();
		void save() const;

		variant get(const std::string &section, const std::string &key, const variant &default = "") const;
		void set(const std::string &section, const std::string &key, const variant &value);

	private:
		filesystem::path _path;
		std::unordered_map<std::string, std::unordered_map<std::string, variant>> _data;
	};
}
