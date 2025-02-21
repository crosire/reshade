/*
 * Copyright (C) 2014 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause OR MIT
 */

#include "ini_file.hpp"
#include <shared_mutex>
#include <cctype> // std::toupper
#include <cassert>
#include <algorithm> // std::min, std::sort, std::transform
#include <utf8/core.h>

static std::shared_mutex s_ini_cache_mutex;
static std::unordered_map<std::wstring, std::unique_ptr<ini_file>> s_ini_cache;

ini_file &reshade::global_config()
{
	return ini_file::load_cache(g_reshade_base_path / L"ReShade.ini");
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

	FILE *const file = _wfsopen(_path.c_str(), L"r", SH_DENYWR);
	if (file == nullptr)
		return false;

	_modified = false;
	_modified_at = modified_at;

	// Remove BOM (0xefbbbf means 0xfeff)
	if (fgetc(file) != utf8::bom[0] || fgetc(file) != utf8::bom[1] || fgetc(file) != utf8::bom[2])
		fseek(file, 0, SEEK_SET);

	std::string line_data, section;
	line_data.resize(BUFSIZ);
	while (fgets(line_data.data(), static_cast<int>(line_data.size() + 1), file))
	{
		size_t line_length;
		while ((line_length = std::strlen(line_data.data())) == line_data.size() && line_data.back() != '\n')
		{
			line_data.resize(line_data.size() + BUFSIZ);
			if (!fgets(line_data.data() + line_length, static_cast<int>(line_data.size() - line_length + 1), file))
				break;
		}

		const std::string_view line = trim(std::string_view(line_data.data(), line_length), " \t\r\n");

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
			const std::string_view key = trim(line.substr(0, assign_index));
			const std::string_view value = trim(line.substr(assign_index + 1));

			if (value.empty())
			{
				_sections[section].insert({ std::string(key), {} });
				continue;
			}

			// Append to key if it already exists
			ini_file::value_type &elements = _sections[section][std::string(key)];
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
			_sections[section].insert({ std::string(line), {} });
		}
	}

	fclose(file);

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
	if (!ec && (modified_at - _modified_at) > std::chrono::seconds(2))
		return false; // File exists and was modified on disk and therefore may have different data, so cannot save

	std::string data;
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
				data += '[' + section_name + ']' + '\n';

			for (const std::string &key_name : key_names)
			{
				data += key_name + '=';

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

					data += value;
				}

				data += '\n';
			}

			data += '\n';
		}
	}

	FILE *const file = _wfsopen(_path.c_str(), L"w", SH_DENYWR);
	if (file == nullptr)
		return false;
	const size_t file_size_written = fwrite(data.data(), 1, data.size(), file);
	fclose(file);
	if (file_size_written != data.size())
		return false;

	// Flush stream to disk before updating last write time
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
	assert(!path.empty() && path.is_absolute());

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
	assert(!path.empty() && path.is_absolute());

	const std::unique_lock<std::shared_mutex> lock(s_ini_cache_mutex);

	s_ini_cache.erase(path);
}

ini_file *ini_file::find_cache(const std::filesystem::path &path)
{
	assert(!path.empty() && path.is_absolute());

	const std::shared_lock<std::shared_mutex> lock(s_ini_cache_mutex);

	const auto it = s_ini_cache.find(path);
	return it != s_ini_cache.end() ? it->second.get() : nullptr;
}
ini_file &ini_file::load_cache(const std::filesystem::path &path)
{
	assert(!path.empty() && path.is_absolute());

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
