#pragma once

#include <string>
#include <vector>
#include "filesystem.hpp"

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
		template <>
		variant(const filesystem::path &value) : variant(value.string()) { }
		template <>
		variant(const std::vector<filesystem::path> &values) : _values(values.size())
		{
			for (size_t i = 0; i < values.size(); i++)
				_values[i] = values[i].string();
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

		std::vector<std::string> &data()
		{
			return _values;
		}
		const std::vector<std::string> &data() const
		{
			return _values;
		}

		template <typename T>
		const T as(size_t index = 0) const;
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
			if (i >= _values.size())
			{
				return 0l;
			}

			return std::strtol(_values[i].c_str(), nullptr, 10);
		}
		template <>
		const unsigned long as(size_t i) const
		{
			if (i >= _values.size())
			{
				return 0ul;
			}

			return std::strtoul(_values[i].c_str(), nullptr, 10);
		}
		template <>
		const float as(size_t i) const
		{
			return static_cast<float>(as<double>(i));
		}
		template <>
		const double as(size_t i) const
		{
			if (i >= _values.size())
			{
				return 0.0;
			}

			return std::strtod(_values[i].c_str(), nullptr);
		}
		template <>
		const std::string as(size_t i) const
		{
			if (i >= _values.size())
			{
				return std::string();
			}

			return _values[i];
		}
		template <>
		const filesystem::path as(size_t i) const
		{
			return as<std::string>(i);
		}

	private:
		std::vector<std::string> _values;
	};
}
