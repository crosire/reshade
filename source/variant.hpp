/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include <string>
#include <vector>
#include <filesystem>

namespace reshade
{
	class variant
	{
	public:
		variant() { }
		variant(const char *value) : _values(1, value) { }
		template <typename T>
		variant(const T &value) : variant(std::to_string(value)) { }
		template <>
		variant(const bool &value) : variant(value ? "1" : "0") { }
		template <>
		variant(const std::string &value) : _values(1, value) { }
		template <>
		variant(const std::vector<std::string> &values) : _values(values) { }
		variant(const std::vector<std::string> &&values) : _values(std::move(values)) { }
		template<class InputIt>
		variant(InputIt first, InputIt last) : _values(first, last) { }
		template <>
		variant(const std::filesystem::path &value) : variant(value.u8string()) { }
		template <>
		variant(const std::vector<std::filesystem::path> &values) : _values(values.size())
		{
			for (size_t i = 0; i < values.size(); i++)
				_values[i] = values[i].u8string();
		}
		template <typename T>
		variant(const T *values, size_t count) : _values(count)
		{
			for (size_t i = 0; i < count; i++)
				_values[i] = std::to_string(values[i]);
		}
		template <>
		variant(const bool *values, size_t count) : _values(count)
		{
			for (size_t i = 0; i < count; i++)
				_values[i] = values[i] ? "1" : "0";
		}
		template <typename T, size_t COUNT>
		variant(const T(&values)[COUNT]) : variant(values, COUNT) { }
		template <typename T>
		variant(std::initializer_list<T> values) : variant(values.begin(), values.size()) { }

		std::vector<std::string> &data() { return _values; }
		const std::vector<std::string> &data() const { return _values; }

		template <typename T>
		const T as(size_t index = 0) const = delete;
		template <>
		const bool as(size_t i) const
		{
			return as<int>(i) != 0 || i < _values.size() && (_values[i] == "true" || _values[i] == "True" || _values[i] == "TRUE");
		}
		template <>
		const int as(size_t i) const
		{
			return static_cast<int>(as<long>(i));
		}
		template <>
		const unsigned int as(size_t i) const
		{
			return static_cast<unsigned int>(as<unsigned long>(i));
		}
		template <>
		const long as(size_t i) const
		{
			return i < _values.size() ? std::strtol(_values[i].c_str(), nullptr, 10) : 0l;
		}
		template <>
		const unsigned long as(size_t i) const
		{
			return i < _values.size() ? std::strtoul(_values[i].c_str(), nullptr, 10) : 0ul;
		}
		template <>
		const long long as(size_t i) const
		{
			return i < _values.size() ? std::strtoll(_values[i].c_str(), nullptr, 10) : 0ll;
		}
		template <>
		const unsigned long long as(size_t i) const
		{
			return i < _values.size() ? std::strtoull(_values[i].c_str(), nullptr, 10) : 0ull;
		}
		template <>
		const float as(size_t i) const
		{
			return static_cast<float>(as<double>(i));
		}
		template <>
		const double as(size_t i) const
		{
			return i < _values.size() ? std::strtod(_values[i].c_str(), nullptr) : 0.0;
		}
		template <>
		const std::string as(size_t i) const
		{
			return i < _values.size() ? _values[i] : std::string();
		}
		template <>
		const std::filesystem::path as(size_t i) const
		{
			return as<std::string>(i);
		}

	private:
		std::vector<std::string> _values;
	};
}
