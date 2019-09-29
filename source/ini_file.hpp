/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include <filesystem>
#include <vector>
#include <string>
#include <unordered_map>

namespace reshade
{
	class ini_file
	{
	public:
		explicit ini_file(const std::filesystem::path &path);
		~ini_file();

		bool flush();

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

		template <typename T>
		void get(const std::string &section, const std::string &key, T &value) const
		{
			const auto it1 = _sections.find(section);
			if (it1 == _sections.end())
				return;
			const auto it2 = it1->second.find(key);
			if (it2 == it1->second.end())
				return;
			value = convert<T>(it2->second, 0);
		}
		template <typename T, size_t SIZE>
		void get(const std::string &section, const std::string &key, T(&values)[SIZE]) const
		{
			const auto it1 = _sections.find(section);
			if (it1 == _sections.end())
				return;
			const auto it2 = it1->second.find(key);
			if (it2 == it1->second.end())
				return;
			for (size_t i = 0; i < SIZE; ++i)
				values[i] = convert<T>(it2->second, i);
		}
		template <typename T>
		void get(const std::string &section, const std::string &key, std::vector<T> &values) const
		{
			const auto it1 = _sections.find(section);
			if (it1 == _sections.end())
				return;
			const auto it2 = it1->second.find(key);
			if (it2 == it1->second.end())
				return;
			values.resize(it2->second.size());
			for (size_t i = 0; i < it2->second.size(); ++i)
				values[i] = convert<T>(it2->second, i);
		}

		template <typename T>
		void set(const std::string &section, const std::string &key, const T &value)
		{
			set<std::string>(section, key, std::to_string(value));
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
		template <>
		void set(const std::string &section, const std::string &key, const std::filesystem::path &value)
		{
			set<std::string>(section, key, value.u8string());
		}
		template <typename T, size_t SIZE>
		void set(const std::string &section, const std::string &key, const T(&values)[SIZE], size_t size = SIZE)
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

		static reshade::ini_file &load_cache(const std::filesystem::path &path);
		static void cache_loop();

	private:
		void load();

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

		bool _modified = false;
		std::filesystem::path _path;
		using value = std::vector<std::string>;
		using section = std::unordered_map<std::string, value>;
		std::unordered_map<std::string, section> _sections;
		std::filesystem::file_time_type _modified_at;
	};
}
