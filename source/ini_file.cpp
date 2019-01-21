/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "ini_file.hpp"
#include <fstream>

namespace reshade
{
	static inline void trim(std::string &str, const char *chars = " \t")
	{
		str.erase(0, str.find_first_not_of(chars));
		str.erase(str.find_last_not_of(chars) + 1);
	}
	static inline std::string trim(const std::string &str, const char *chars = " \t")
	{
		std::string res(str);
		trim(res, chars);
		return res;
	}

	ini_file::ini_file(const std::filesystem::path &path)
		: _path(path), _save_path(path)
	{
		load();
	}
	ini_file::ini_file(const std::filesystem::path &path, const std::filesystem::path &save_path)
		: _path(path), _save_path(save_path)
	{
		load();
	}
	ini_file::~ini_file()
	{
		save();
	}

	void ini_file::load()
	{
		std::string line, section;
		std::ifstream file(_path.wstring());

		while (std::getline(file, line))
		{
			trim(line);

			if (line.empty() || line[0] == ';' || line[0] == '/')
			{
				continue;
			}

			// Read section name
			if (line[0] == '[')
			{
				section = trim(line.substr(0, line.find(']')), " \t[]");
				continue;
			}

			// Read section content
			const auto assign_index = line.find('=');

			if (assign_index != std::string::npos)
			{
				const auto key = trim(line.substr(0, assign_index));
				const auto value = trim(line.substr(assign_index + 1));
				std::vector<std::string> value_splitted;

				for (size_t i = 0, len = value.size(), found; i < len; i = found + 1)
				{
					found = value.find_first_of(',', i);

					if (found == std::string::npos)
						found = len;

					value_splitted.push_back(value.substr(i, found - i));
				}

				_sections[section][key] = value_splitted;
			}
			else
			{
				_sections[section][line] = 0;
			}
		}
	}
	void ini_file::save() const
	{
		if (!_modified)
		{
			return;
		}

		std::ofstream file(_save_path.wstring());

		const auto it = _sections.find("");

		if (it != _sections.end())
		{
			for (const auto &section_line : it->second)
			{
				file << section_line.first << '=';

				size_t i = 0;

				for (const auto &item : section_line.second.data())
				{
					if (i++ != 0)
					{
						file << ',';
					}

					file << item;
				}

				file << std::endl;
			}

			file << std::endl;
		}

		for (const auto &section : _sections)
		{
			if (section.first.empty())
			{
				continue;
			}

			file << '[' << section.first << ']' << std::endl;

			for (const auto &section_line : section.second)
			{
				file << section_line.first << '=';

				size_t i = 0;

				for (const auto &item : section_line.second.data())
				{
					if (i++ != 0)
					{
						file << ',';
					}

					file << item;
				}

				file << std::endl;
			}

			file << std::endl;
		}
	}
}
