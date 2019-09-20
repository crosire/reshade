/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "ini_file.hpp"
#include <fstream>

static std::unordered_map<std::wstring, reshade::ini_file> g_ini_cache;

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

reshade::ini_file::ini_file(const std::filesystem::path &path)
	: _path(path)
{
	load();
}
reshade::ini_file::~ini_file()
{
	flush();
}

void reshade::ini_file::load()
{
	enum class condition { none, open, not_found, blocked, unknown };
	condition condition = condition::none;

	std::error_code ec;
	std::ifstream file;

	const std::filesystem::file_time_type modified_at = std::filesystem::last_write_time(_path, ec);
	if (ec.value() == 0)
		condition = condition::open;
	else if (ec.value() == 0x2 || ec.value() == 0x3) // 0x2: ERROR_FILE_NOT_FOUND, 0x3: ERROR_PATH_NOT_FOUND
		condition = condition::not_found;
	else
		condition = condition::unknown;

	if (condition == condition::open)
		if (file.open(_path); file.fail())
			condition = condition::blocked;

	if (condition == condition::blocked || condition == condition::unknown)
		return;

	if (condition == condition::open && _modified_at >= modified_at)
		return;

	_sections.clear();
	_modified = false;

	if (condition == condition::not_found)
		return;

	_modified_at = modified_at;
	file.imbue(std::locale("en-us.UTF-8"));

	// Remove BOM (0xefbbbf means 0xfeff)
	if (file.get() != 0xef || file.get() != 0xbb || file.get() != 0xbf)
		file.seekg(0, std::ios::beg);

	std::string line, section;
	while (std::getline(file, line))
	{
		trim(line);

		if (line.empty() || line[0] == ';' || line[0] == '/')
			continue;

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
			_sections[section][line] = {};
		}
	}
}
bool reshade::ini_file::flush()
{
	if (!_modified)
		return true;

	enum class condition { none, open, create, blocked, unknown };
	condition condition = condition::none;

	std::error_code ec;
	std::ofstream file;

	std::filesystem::file_time_type modified_at = std::filesystem::last_write_time(_path, ec);
	if (ec.value() == 0)
		condition = condition::open;
	else if (ec.value() == 0x2 || ec.value() == 0x3) // 0x2: ERROR_FILE_NOT_FOUND, 0x3: ERROR_PATH_NOT_FOUND
		condition = condition::create;
	else
		condition = condition::unknown;

	if (condition == condition::open || condition == condition::create)
		if (file.open(_path); file.fail())
			if (condition == condition::open)
				condition = condition::blocked;
			else
				condition = condition::unknown;

	if (condition == condition::blocked || condition == condition::unknown)
		return false;

	if (_modified &= _modified_at > modified_at; !_modified)
		return true;

	file.imbue(std::locale("en-us.UTF-8"));
	std::vector<std::string> section_names, key_names;

	for (const auto &section : _sections)
		section_names.push_back(section.first);

	std::sort(section_names.begin(), section_names.end(), [](std::string a, std::string b) {
		std::transform(a.begin(), a.end(), a.begin(), [](std::string::value_type c) { return static_cast<std::string::value_type>(toupper(c)); });
		std::transform(b.begin(), b.end(), b.begin(), [](std::string::value_type c) { return static_cast<std::string::value_type>(toupper(c)); });
		return a < b;
		});

	for (const auto &section_name : section_names)
	{
		const auto &keys = _sections.at(section_name);

		for (const auto &key : keys)
			key_names.push_back(key.first);

		std::sort(key_names.begin(), key_names.end(), [](std::string a, std::string b) {
			std::transform(a.begin(), a.end(), a.begin(), [](std::string::value_type c) { return static_cast<std::string::value_type>(toupper(c)); });
			std::transform(b.begin(), b.end(), b.begin(), [](std::string::value_type c) { return static_cast<std::string::value_type>(toupper(c)); });
			return a < b;
			});

		if (!section_name.empty())
			file << '[' << section_name << ']' << '\n';

		for (const auto &key_name : key_names)
		{
			file << key_name << '=';

			size_t i = 0;

			for (const auto &item : keys.at(key_name))
			{
				if (i++ != 0)
					file << ',';

				file << item;
			}

			file << '\n';
		}

		file << '\n';
		key_names.clear();
	}

	if (file.close(); file.fail())
		return false;

	_modified = false;

	if (modified_at = std::filesystem::last_write_time(_path, ec); ec.value() == 0)
		_modified_at = modified_at;

	return true;
}

reshade::ini_file &reshade::ini_file::load_cache(const std::filesystem::path &path)
{
	if (const auto it = g_ini_cache.try_emplace(path, path); it.second)
		return it.first->second;
	else
		return it.first->second.load(), it.first->second;
}

void reshade::ini_file::cache_loop()
{
	const auto now = std::filesystem::file_time_type::clock::now();
	for (auto &file : g_ini_cache)
		if (file.second._modified && now - file.second._modified_at > std::chrono::seconds(1))
			file.second.flush();
}
