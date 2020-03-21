/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "runtime_config.hpp"
#include <fstream>
#include <sstream>

static std::unordered_map<std::wstring, reshade::ini_file> g_ini_cache;

reshade::ini_file::ini_file(const std::filesystem::path &path)
	: _path(path)
{
	load();
}
reshade::ini_file::~ini_file()
{
	save();
}

void reshade::ini_file::load()
{
	enum class condition { none, open, not_found, blocked, unknown };
	condition condition = condition::none;

	std::error_code ec;

	const std::filesystem::file_time_type modified_at = std::filesystem::last_write_time(_path, ec);
	if (ec.value() == 0)
		condition = condition::open;
	else if (ec.value() == 0x2 || ec.value() == 0x3) // 0x2: ERROR_FILE_NOT_FOUND, 0x3: ERROR_PATH_NOT_FOUND
		condition = condition::not_found;
	else
		condition = condition::unknown;

	if (condition == condition::open && _modified_at >= modified_at)
		return;

	std::ifstream file;

	if (condition == condition::open)
		if (file.open(_path); file.fail())
			condition = condition::blocked;

	if (condition == condition::blocked || condition == condition::unknown)
		return;

	_sections.clear();
	_modified = false;

	if (condition == condition::not_found)
		return;

	assert(std::filesystem::file_size(_path, ec) > 0);

	_modified_at = modified_at;
	file.imbue(std::locale("en-us.UTF-8"));

	// Remove BOM (0xefbbbf means 0xfeff)
	if (file.get() != 0xef || file.get() != 0xbb || file.get() != 0xbf)
		file.seekg(0, std::ios::beg);

	std::string line, section;
	while (std::getline(file, line))
	{
		trim(line);

		if (line.empty() || line[0] == ';' || line[0] == '/' || line[0] == '#')
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
bool reshade::ini_file::save()
{
	if (!_modified)
		return true;

	std::error_code ec;
	std::filesystem::file_time_type modified_at = std::filesystem::last_write_time(_path, ec);
	if (ec.value() == 0 && modified_at >= _modified_at)
	{
		// File exists and was modified on disk and may have different data, so cannot save
		_modified = false;
		return true;
	}

	std::stringstream data;
	std::vector<std::string> section_names, key_names;

	section_names.reserve(_sections.size());
	for (const auto &section : _sections)
		section_names.push_back(section.first);

	// Sort sections to generate consistent files
	std::sort(section_names.begin(), section_names.end(), [](std::string a, std::string b) {
		std::transform(a.begin(), a.end(), a.begin(), [](std::string::value_type c) { return static_cast<std::string::value_type>(toupper(c)); });
		std::transform(b.begin(), b.end(), b.begin(), [](std::string::value_type c) { return static_cast<std::string::value_type>(toupper(c)); });
		return a < b;
		});

	for (const std::string &section_name : section_names)
	{
		const auto &keys = _sections.at(section_name);

		key_names.clear();
		key_names.reserve(keys.size());
		for (const auto &key : keys)
			key_names.push_back(key.first);

		std::sort(key_names.begin(), key_names.end(), [](std::string a, std::string b) {
			std::transform(a.begin(), a.end(), a.begin(), [](std::string::value_type c) { return static_cast<std::string::value_type>(toupper(c)); });
			std::transform(b.begin(), b.end(), b.begin(), [](std::string::value_type c) { return static_cast<std::string::value_type>(toupper(c)); });
			return a < b;
			});

		// Empty section should have been sorted to the top, so do not need to append it before keys
		if (!section_name.empty())
			data << '[' << section_name << ']' << '\n';

		for (const std::string &key_name : key_names)
		{
			data << key_name << '=';

			size_t i = 0;

			for (const std::string &item : keys.at(key_name))
			{
				if (i++ != 0) // Separate multiple values with a comma
					data << ',';

				data << item;
			}

			data << '\n';
		}

		data << '\n';
	}

	std::ofstream file(_path);
	if (!file.is_open() || file.fail())
	{
		// Reset state to avoid cache flushing to repeatedly save the file
		_modified = false;
		return false;
	}

	file.rdbuf()->pubsetbuf(nullptr, 0);

	const std::string str = data.str();
	file.imbue(std::locale("en-us.UTF-8"));
	file.write(str.data(), str.size());

	// Keep the modified flag if saving was not successful, so to try again later
	if (_modified = file.fail(); _modified)
		std::filesystem::last_write_time(_path, modified_at, ec);
	else if (modified_at = std::filesystem::last_write_time(_path, ec); ec.value() == 0)
		_modified_at = modified_at;

	assert(std::filesystem::file_size(_path, ec) > 0);

	return true;
}

reshade::ini_file &reshade::ini_file::load_cache(const std::filesystem::path &path)
{
	const auto it = g_ini_cache.try_emplace(path, path);
	if (it.second || (std::filesystem::file_time_type::clock::now() - it.first->second._modified_at) < std::chrono::seconds(1))
		return it.first->second; // Don't need to reload file when it was just loaded or there are still modifications pending
	else
		return it.first->second.load(), it.first->second;
}

bool reshade::ini_file::flush_cache()
{
	bool success = true;
	const auto now = std::filesystem::file_time_type::clock::now();

	// Save all files that were modified in one second intervals
	for (auto &file : g_ini_cache)
	{
		if (file.second._modified && (now - file.second._modified_at) > std::chrono::seconds(1))
			success &= file.second.save();
	}

	return success;
}
bool reshade::ini_file::flush_cache(const std::filesystem::path &path)
{
	const auto it = g_ini_cache.find(path);
	return it != g_ini_cache.end() && it->second.save();
}
