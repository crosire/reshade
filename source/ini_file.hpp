/*
 * Copyright (C) 2014 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause OR MIT
 */

#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <unordered_map>

extern std::filesystem::path g_reshade_dll_path;
extern std::filesystem::path g_reshade_base_path;
extern std::filesystem::path g_target_executable_path;

inline std::string trim(std::string str, const char chars[] = " \t")
{
	str.erase(0, str.find_first_not_of(chars));
	str.erase(str.find_last_not_of(chars) + 1);
	return str;
}

class ini_file
{
public:
	/// <summary>
	/// Opens the INI file at the specified <paramref name="path"/>.
	/// </summary>
	/// <param name="path">Path to the INI file to access.</param>
	explicit ini_file(const std::filesystem::path &path);

	/// <summary>
	/// Gets the path to this INI file.
	/// </summary>
	const std::filesystem::path &path() const { return _path; }

	/// <summary>
	/// Checks whether the specified <paramref name="section"/> and <paramref name="key"/> currently exist in the INI.
	/// </summary>
	bool has(const std::string &section, const std::string &key) const
	{
		const auto it1 = _sections.find(section);
		if (it1 == _sections.end())
			return false;
		const auto it2 = it1->second.find(key);
		if (it2 == it1->second.end())
			return false;
		return true;
	}

	/// <summary>
	/// Gets the value of the specified <paramref name="section"/> and <paramref name="key"/> from the INI.
	/// </summary>
	/// <param name="value">Reference filled with the data of this INI entry.</param>
	/// <returns><see langword="true"/> if the key exists, <see langword="false"/> otherwise.</returns>
	template <typename T>
	bool get(const std::string &section, const std::string &key, T &value) const
	{
		const auto it1 = _sections.find(section);
		if (it1 == _sections.end())
			return false;
		const auto it2 = it1->second.find(key);
		if (it2 == it1->second.end())
			return false;
		value = convert<T>(it2->second, 0);
		return true;
	}
	template <typename T, size_t SIZE>
	bool get(const std::string &section, const std::string &key, T(&values)[SIZE]) const
	{
		const auto it1 = _sections.find(section);
		if (it1 == _sections.end())
			return false;
		const auto it2 = it1->second.find(key);
		if (it2 == it1->second.end())
			return false;
		for (size_t i = 0; i < SIZE; ++i)
			values[i] = convert<T>(it2->second, i);
		return true;
	}
	template <typename T>
	bool get(const std::string &section, const std::string &key, std::vector<T> &values) const
	{
		const auto it1 = _sections.find(section);
		if (it1 == _sections.end())
			return false;
		const auto it2 = it1->second.find(key);
		if (it2 == it1->second.end())
			return false;
		values.resize(it2->second.size());
		for (size_t i = 0; i < it2->second.size(); ++i)
			values[i] = convert<T>(it2->second, i);
		return true;
	}
	template <>
	bool get(const std::string &section, const std::string &key, std::vector<std::pair<std::string, std::string>> &values) const
	{
		const auto it1 = _sections.find(section);
		if (it1 == _sections.end())
			return false;
		const auto it2 = it1->second.find(key);
		if (it2 == it1->second.end())
			return false;
		values.resize(it2->second.size());
		for (size_t i = 0; i < it2->second.size(); ++i)
		{
			std::string value = convert<std::string>(it2->second, i);
			if (const size_t equals_sign = value.find('=');
				equals_sign != std::string::npos)
				values[i] = { value.substr(0, equals_sign), value.substr(equals_sign + 1) };
			else
				values[i].first = std::move(value);
		}
		return true;
	}

	/// <summary>
	/// Returns <see langword="true"/> only if the specified <paramref name="section"/> and <paramref name="key"/> exists and is not zero.
	/// </summary>
	/// <returns><see langword="true"/> if the key exists and is not zero, <see langword="false"/> otherwise.</returns>
	bool get(const std::string &section, const std::string &key) const
	{
		bool value = false;
		return get<bool>(section, key, value) && value;
	}

	/// <summary>
	/// Sets the value of the specified <paramref name="section"/> and <paramref name="key"/> to a new <paramref name="value"/>.
	/// </summary>
	/// <param name="value">Data to set this INI entry to.</param>
	template <typename T>
	void set(const std::string &section, const std::string &key, const T &value)
	{
		set(section, key, std::to_string(value));
	}
	template <>
	void set(const std::string &section, const std::string &key, const bool &value)
	{
		set<std::string>(section, key, value ? "1" : "0");
	}
	template <>
	void set(const std::string &section, const std::string &key, const std::string &value)
	{
		auto &v = _sections[section][key];
		v.assign(1, value);
		_modified = true;
		_modified_at = std::filesystem::file_time_type::clock::now();
	}
	void set(const std::string &section, const std::string &key, std::string &&value)
	{
		auto &v = _sections[section][key];
		v.resize(1);
		v[0] = std::forward<std::string>(value);
		_modified = true;
		_modified_at = std::filesystem::file_time_type::clock::now();
	}
	template <>
	void set(const std::string &section, const std::string &key, const std::filesystem::path &value)
	{
		set(section, key, value.u8string());
	}
	template <typename T, size_t SIZE>
	void set(const std::string &section, const std::string &key, const T(&values)[SIZE], const size_t size = SIZE)
	{
		auto &v = _sections[section][key];
		v.resize(size);
		for (size_t i = 0; i < size; ++i)
			v[i] = std::to_string(values[i]);
		_modified = true;
		_modified_at = std::filesystem::file_time_type::clock::now();
	}
	template <typename T>
	void set(const std::string &section, const std::string &key, const std::vector<T> &values)
	{
		auto &v = _sections[section][key];
		v.resize(values.size());
		for (size_t i = 0; i < values.size(); ++i)
			v[i] = std::to_string(values[i]);
		_modified = true;
		_modified_at = std::filesystem::file_time_type::clock::now();
	}
	template <>
	void set(const std::string &section, const std::string &key, const std::vector<std::string> &values)
	{
		auto &v = _sections[section][key];
		v = values;
		_modified = true;
		_modified_at = std::filesystem::file_time_type::clock::now();
	}
	void set(const std::string &section, const std::string &key, std::vector<std::string> &&values)
	{
		auto &v = _sections[section][key];
		v = std::forward<std::vector<std::string>>(values);
		_modified = true;
		_modified_at = std::filesystem::file_time_type::clock::now();
	}
	template <>
	void set(const std::string &section, const std::string &key, const std::vector<std::pair<std::string, std::string>> &values)
	{
		auto &v = _sections[section][key];
		v.resize(values.size());
		for (size_t i = 0; i < values.size(); ++i)
		{
			const std::pair<std::string, std::string> &value = values[i];
			v[i] = value.first;
			if (!value.second.empty())
				v[i] += '=' + value.second;
		}
		_modified = true;
		_modified_at = std::filesystem::file_time_type::clock::now();
	}
	template <>
	void set(const std::string &section, const std::string &key, const std::vector<std::filesystem::path> &values)
	{
		auto &v = _sections[section][key];
		v.resize(values.size());
		for (size_t i = 0; i < values.size(); ++i)
			v[i] = values[i].u8string();
		_modified = true;
		_modified_at = std::filesystem::file_time_type::clock::now();
	}

	/// <summary>
	/// Removes all values.
	/// </summary>
	void clear()
	{
		_sections.clear();
		_modified = true;
		_modified_at = std::filesystem::file_time_type::clock::now();
	}

	/// <summary>
	/// Removes the specified <paramref name="key"/> from the <paramref name="section"/>.
	/// </summary>
	void remove_key(const std::string &section, const std::string &key)
	{
		const auto it1 = _sections.find(section);
		if (it1 == _sections.end())
			return;
		const auto it2 = it1->second.find(key);
		if (it2 == it1->second.end())
			return;
		it1->second.erase(it2);
		_modified = true;
		_modified_at = std::filesystem::file_time_type::clock::now();
	}

	/// <summary>
	/// Loads all values from disk.
	/// </summary>
	bool load();
	/// <summary>
	/// Saves all changes to this INI file to disk.
	/// </summary>
	bool save();

	/// <summary>
	/// Saves all changes to INI files that were loaded through <see cref="load_cache"/> to disk.
	/// </summary>
	static bool flush_cache();
	static bool flush_cache(std::filesystem::path path);

	/// <summary>
	/// Removes all INI files from cache, without saving changes.
	/// </summary>
	static void clear_cache();
	static void clear_cache(std::filesystem::path path);

	/// <summary>
	/// Gets the specified INI file from cache or opens it when it was not cached yet.
	/// </summary>
	/// <param name="path">Path to the INI file to access.</param>
	/// <returns>Reference to the cached data.</returns>
	static ini_file &load_cache(std::filesystem::path path);

private:
	template <typename T>
	static const T convert(const std::vector<std::string> &values, size_t i) = delete;
	template <>
	static const bool convert(const std::vector<std::string> &values, size_t i)
	{
		return convert<int>(values, i) != 0 || i < values.size() && (values[i] == "true" || values[i] == "True" || values[i] == "TRUE");
	}
	template <>
	static const int convert(const std::vector<std::string> &values, size_t i)
	{
		return static_cast<int>(convert<long>(values, i));
	}
	template <>
	static const unsigned int convert(const std::vector<std::string> &values, size_t i)
	{
		return static_cast<unsigned int>(convert<unsigned long>(values, i));
	}
	template <>
	static const long convert(const std::vector<std::string> &values, size_t i)
	{
		return i < values.size() ? std::strtol(values[i].c_str(), nullptr, 10) : 0l;
	}
	template <>
	static const unsigned long convert(const std::vector<std::string> &values, size_t i)
	{
		return i < values.size() ? std::strtoul(values[i].c_str(), nullptr, 10) : 0ul;
	}
	template <>
	static const long long convert(const std::vector<std::string> &values, size_t i)
	{
		return i < values.size() ? std::strtoll(values[i].c_str(), nullptr, 10) : 0ll;
	}
	template <>
	static const unsigned long long convert(const std::vector<std::string> &values, size_t i)
	{
		return i < values.size() ? std::strtoull(values[i].c_str(), nullptr, 10) : 0ull;
	}
	template <>
	static const float convert(const std::vector<std::string> &values, size_t i)
	{
		return static_cast<float>(convert<double>(values, i));
	}
	template <>
	static const double convert(const std::vector<std::string> &values, size_t i)
	{
		return i < values.size() ? std::strtod(values[i].c_str(), nullptr) : 0.0;
	}
	template <>
	static const std::string convert(const std::vector<std::string> &values, size_t i)
	{
		return i < values.size() ? values[i] : std::string();
	}
	template <>
	static const std::filesystem::path convert(const std::vector<std::string> &values, size_t i)
	{
		return i < values.size() ? std::filesystem::u8path(values[i]) : std::filesystem::path();
	}

	/// <summary>
	/// Describes a single value in an INI file.
	/// </summary>
	using value_type = std::vector<std::string>;
	/// <summary>
	/// Describes a section of multiple key/value pairs in an INI file.
	/// </summary>
	using section_type = std::unordered_map<std::string, value_type>;

	const std::filesystem::path _path;
	std::unordered_map<std::string, section_type> _sections;
	bool _modified = false;
	std::filesystem::file_time_type _modified_at;
};

namespace reshade
{
	/// <summary>
	/// Global configuration that can be used for general settings that are not specific to an effect runtime instance.
	/// </summary>
	ini_file &global_config();
}
