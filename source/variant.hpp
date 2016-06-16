#pragma once

#include <string>
#include <vector>

namespace reshade
{
	class variant
	{
	public:
		variant()
		{
		}
		variant(const char *value) : _values(1, value)
		{
		}
		template <typename T>
		variant(const T &value) : variant(std::to_string(value))
		{
		}
		template <>
		variant(const bool &value) : variant(value ? "1" : "0")
		{
		}
		template <>
		variant(const std::string &value) : _values(1, value)
		{
		}
		template <>
		variant(const std::vector<std::string> &values) : _values(values)
		{
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
		variant(const T(&values)[COUNT]) : variant(values, COUNT)
		{
		}
		template <typename T>
		variant(std::initializer_list<T> values) : variant(values.begin(), values.size())
		{
		}

		inline std::vector<std::string> &data()
		{
			return _values;
		}
		inline const std::vector<std::string> &data() const
		{
			return _values;
		}

		template <typename T>
		const T as(size_t index = 0) const;
		template <>
		inline const bool as(size_t i) const
		{
			return as<int>(i) != 0 || i < _values.size() && (_values[i] == "true" || _values[i] == "True" || _values[i] == "TRUE");
		}
		template <>
		inline const int as(size_t i) const
		{
			return static_cast<int>(as<long>(i));
		}
		template <>
		inline const unsigned int as(size_t i) const
		{
			return static_cast<unsigned int>(as<unsigned long>(i));
		}
		template <>
		inline const long as(size_t i) const
		{
			if (i >= _values.size())
			{
				return 0l;
			}

			return std::strtol(_values[i].c_str(), nullptr, 10);
		}
		template <>
		inline const unsigned long as(size_t i) const
		{
			if (i >= _values.size())
			{
				return 0ul;
			}

			return std::strtoul(_values[i].c_str(), nullptr, 10);
		}
		template <>
		inline const float as(size_t i) const
		{
			return static_cast<float>(as<double>(i));
		}
		template <>
		inline const double as(size_t i) const
		{
			if (i >= _values.size())
			{
				return 0.0;
			}

			return std::strtod(_values[i].c_str(), nullptr);
		}
		template <>
		inline const std::string as(size_t i) const
		{
			if (i >= _values.size())
			{
				return "";
			}

			return _values[i];
		}

	private:
		std::vector<std::string> _values;
	};
}
