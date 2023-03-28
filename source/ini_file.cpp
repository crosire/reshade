/*
 * Copyright (C) 2014 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause OR MIT
 */

#include "ini_file.hpp"
#include <cctype>
#include <cassert>
#include <fstream>
#include <sstream>
#include <shared_mutex>

static std::shared_mutex s_ini_cache_mutex;
static std::unordered_map<std::wstring, std::unique_ptr<ini_file>> s_ini_cache;

ini_file &reshade::global_config()
{
	return ini_file::load_cache(g_target_executable_path.parent_path() / L"ReShade.ini");
}

ini_file::ini_file(const std::filesystem::path &path) : _path(path)
{
	load();
}

bool ini_file::load()
{
	std::error_code ec;
	const std::filesystem::file_time_type modified_at = std::filesystem::last_write_time(_path, ec);
	if (!ec && _modified_at >= modified_at)
		return true; // Skip loading if there was no modification to the file since it was last loaded

	// Clear when file does not exist too
	_sections.clear();

	std::ifstream file(_path);
	if (!file)
		return false;

	_modified = false;
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
		const size_t assign_index = line.find('=');
		if (assign_index != std::string::npos)
		{
			const std::string key = trim(line.substr(0, assign_index));
			const std::string value = trim(line.substr(assign_index + 1));

			if (value.empty())
			{
				_sections[section].insert({ key, {} });
				continue;
			}

			// Append to key if it already exists
			ini_file::value_type &elements = _sections[section][key];
			for (size_t offset = 0, base = 0, len = value.size(); offset <= len;)
			{
				// Treat ",," as an escaped comma and only split on single ","
				const size_t found = std::min(value.find(',', offset), len);
				if (found + 1 < len && value[found + 1] == ',')
				{
					offset = found + 2;
				}
				else
				{
					std::string &element = elements.emplace_back();
					element.reserve(found - base);

					while (base < found)
					{
						const char c = value[base++];
						element += c;

						if (c == ',' && base < found && value[base] == ',')
							base++; // Skip second comma in a ",," escape sequence
					}

					offset = base = found + 1;
				}
			}
		}
		else
		{
			_sections[section].insert({ line, {} });
		}
	}

	return true;
}
bool ini_file::save()
{
	if (!_modified)
		return true;

	// Reset state even on failure to avoid 'flush_cache' repeatedly trying and failing to save
	_modified = false;

	std::error_code ec;
	const std::filesystem::file_time_type modified_at = std::filesystem::last_write_time(_path, ec);
	if (!ec && modified_at > _modified_at)
		return false; // File exists and was modified on disk and therefore may have different data, so cannot save

	std::stringstream data;
	std::vector<std::string> section_names, key_names;

	section_names.reserve(_sections.size());
	for (const std::pair<const std::string, section_type> &section : _sections)
		section_names.push_back(section.first);

	// Sort sections to generate consistent files
	std::sort(section_names.begin(), section_names.end(),
		[](std::string a, std::string b) {
			std::transform(a.begin(), a.end(), a.begin(), [](std::string::value_type c) { return static_cast<std::string::value_type>(std::toupper(c)); });
			std::transform(b.begin(), b.end(), b.begin(), [](std::string::value_type c) { return static_cast<std::string::value_type>(std::toupper(c)); });
			return a < b;
		});

	for (const std::string &section_name : section_names)
	{
		if (const section_type &keys = _sections.at(section_name); !keys.empty())
		{
			key_names.clear();
			key_names.reserve(keys.size());
			for (const std::pair<const std::string, value_type> &key : keys)
				key_names.push_back(key.first);

			std::sort(key_names.begin(), key_names.end(),
				[](std::string a, std::string b) {
					std::transform(a.begin(), a.end(), a.begin(), [](std::string::value_type c) { return static_cast<std::string::value_type>(std::toupper(c)); });
					std::transform(b.begin(), b.end(), b.begin(), [](std::string::value_type c) { return static_cast<std::string::value_type>(std::toupper(c)); });
					return a < b;
				});

			// Empty section should have been sorted to the top, so do not need to append it before keys
			if (!section_name.empty())
				data << '[' << section_name << ']' << '\n';

			for (const std::string &key_name : key_names)
			{
				data << key_name << '=';

				if (const ini_file::value_type &elements = keys.at(key_name); !elements.empty())
				{
					std::string value;
					for (const std::string &element : elements)
					{
						// Empty elements mess with escaped commas, so simply skip them
						if (element.empty())
							continue;

						value.reserve(value.size() + element.size() + 1);
						for (const char c : element)
							value.append(c == ',' ? 2 : 1, c);
						value += ','; // Separate multiple values with a comma
					}

					// Remove the last comma
					if (!value.empty())
					{
						assert(value.back() == ',');
						value.pop_back();
					}

					data << value;
				}

				data << '\n';
			}

			data << '\n';
		}
	}

	std::ofstream file(_path);
	if (!file)
		return false;

	file.imbue(std::locale("en-us.UTF-8"));

	const std::string str = data.str();
	if (!file.write(str.data(), str.size()))
		return false;

	// Flush stream to disk before updating last write time
	file.close();
	_modified_at = std::filesystem::last_write_time(_path, ec);

	assert(!ec && std::filesystem::file_size(_path, ec) > 0);

	return true;
}

bool ini_file::flush_cache()
{
	bool success = true;

	const std::shared_lock<std::shared_mutex> lock(s_ini_cache_mutex);

	// Save all files that were modified in one second intervals
	for (auto &file : s_ini_cache)
		// Check modified status before requesting file time, since the latter is costly and therefore should be avoided when not necessary
		if (file.second->_modified && (std::filesystem::file_time_type::clock::now() - file.second->_modified_at) > std::chrono::seconds(1))
			success &= file.second->save();

	return success;
}
bool ini_file::flush_cache(const std::filesystem::path &path)
{
	const std::shared_lock<std::shared_mutex> lock(s_ini_cache_mutex);

	const auto it = s_ini_cache.find(path);
	return it != s_ini_cache.end() && it->second->save();
}

void ini_file::clear_cache()
{
	const std::unique_lock<std::shared_mutex> lock(s_ini_cache_mutex);

	s_ini_cache.clear();
}
void ini_file::clear_cache(const std::filesystem::path &path)
{
	const std::unique_lock<std::shared_mutex> lock(s_ini_cache_mutex);

	s_ini_cache.erase(path);
}

ini_file &ini_file::load_cache(const std::filesystem::path &path)
{
	const std::unique_lock<std::shared_mutex> lock(s_ini_cache_mutex);

	const auto insert = s_ini_cache.try_emplace(path);
	const auto it = insert.first;

	// Only construct when actually adding a new entry to the cache, since the 'ini_file' constructor performs a costly load of the file
	if (insert.second)
		it->second = std::make_unique<ini_file>(path);
	// Don't reload file when it was just loaded or there are still modifications pending
	else if (!it->second->_modified)
		it->second->load();

	return *it->second;
}
