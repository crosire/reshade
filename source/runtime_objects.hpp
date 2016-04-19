#pragma once

#include <string>
#include <memory>
#include <unordered_map>
#include "moving_average.hpp"

namespace reshade
{
	enum class texture_type
	{
		image,
		backbuffer,
		depthbuffer
	};
	enum class texture_filter
	{
		anisotropic = 0x55,
		min_mag_mip_point = 0,
		min_mag_point_mip_linear = 0x1,
		min_point_mag_linear_mip_point = 0x4,
		min_point_mag_mip_linear = 0x5,
		min_linear_mag_mip_point = 0x10,
		min_linear_mag_point_mip_linear = 0x11,
		min_mag_linear_mip_point = 0x14,
		min_mag_mip_linear = 0x15
	};
	enum class texture_format
	{
		unknown,

		r8,
		r16f,
		r32f,
		rg8,
		rg16,
		rg16f,
		rg32f,
		rgba8,
		rgba16,
		rgba16f,
		rgba32f,

		dxt1,
		dxt3,
		dxt5,
		latc1,
		latc2
	};
	enum class texture_address_mode
	{
		wrap = 1,
		mirror = 2,
		clamp = 3,
		border = 4
	};
	enum class uniform_datatype
	{
		bool_,
		int_,
		uint_,
		float_
	};

	struct annotation
	{
		annotation()
		{
		}
		annotation(bool value)
		{
			_value[0] = value ? "1" : "0";
		}
		annotation(const bool values[4])
		{
			_value[0] = values[0] ? "1" : "0";
			_value[1] = values[1] ? "1" : "0";
			_value[2] = values[2] ? "1" : "0";
			_value[3] = values[3] ? "1" : "0";
		}
		annotation(int value)
		{
			_value[0] = std::to_string(value);
		}
		annotation(const int values[4])
		{
			_value[0] = std::to_string(values[0]);
			_value[1] = std::to_string(values[1]);
			_value[2] = std::to_string(values[2]);
			_value[3] = std::to_string(values[3]);
		}
		annotation(unsigned int value)
		{
			_value[0] = std::to_string(value);
		}
		annotation(const unsigned int values[4])
		{
			_value[0] = std::to_string(values[0]);
			_value[1] = std::to_string(values[1]);
			_value[2] = std::to_string(values[2]);
			_value[3] = std::to_string(values[3]);
		}
		annotation(float value)
		{
			_value[0] = std::to_string(value);
		}
		annotation(const float values[4])
		{
			_value[0] = std::to_string(values[0]);
			_value[1] = std::to_string(values[1]);
			_value[2] = std::to_string(values[2]);
			_value[3] = std::to_string(values[3]);
		}
		annotation(const std::string &value)
		{
			_value[0] = value;
		}

		template <typename T>
		const T as(size_t index = 0) const;
		template <>
		inline const bool as(size_t i) const
		{
			return as<int>(i) != 0 || (_value[i] == "true" || _value[i] == "True" || _value[i] == "TRUE");
		}
		template <>
		inline const int as(size_t i) const
		{
			return static_cast<int>(std::strtol(_value[i].c_str(), nullptr, 10));
		}
		template <>
		inline const unsigned int as(size_t i) const
		{
			return static_cast<unsigned int>(std::strtoul(_value[i].c_str(), nullptr, 10));
		}
		template <>
		inline const float as(size_t i) const
		{
			return static_cast<float>(std::strtod(_value[i].c_str(), nullptr));
		}
		template <>
		inline const double as(size_t i) const
		{
			return std::strtod(_value[i].c_str(), nullptr);
		}
		template <>
		inline const std::string as(size_t i) const
		{
			return _value[i];
		}

	private:
		std::string _value[4];
	};

	struct texture abstract
	{
		virtual ~texture() { }

		std::string name;
		texture_type type = texture_type::image;
		unsigned int width = 0, height = 0, levels = 0;
		texture_format format = texture_format::unknown;
		size_t storage_size = 0;
		std::unordered_map<std::string, annotation> annotations;
	};
	struct uniform final
	{
		std::string name;
		uniform_datatype basetype = uniform_datatype::float_;
		unsigned int rows = 0, columns = 0, elements = 0;
		size_t storage_offset = 0, storage_size = 0;
		std::unordered_map<std::string, annotation> annotations;
	};
	struct technique final
	{
		struct pass abstract
		{
			virtual ~pass() { }
		};

		std::string name;
		std::vector<std::unique_ptr<pass>> passes;
		std::unordered_map<std::string, annotation> annotations;
		bool enabled = false;
		int timeout = 0, timeleft = 0;
		int toggle_key = 0, toggle_time = 0;
		bool toggle_key_ctrl = false, toggle_key_shift = false, toggle_key_alt = false;
		utils::moving_average<uint64_t, 512> average_duration;
	};
}
