#pragma once

#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include "variant.hpp"
#include "moving_average.hpp"

namespace reshade
{
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
	enum class texture_reference
	{
		none,
		back_buffer,
		depth_buffer
	};
	enum class uniform_datatype
	{
		boolean,
		signed_integer,
		unsigned_integer,
		floating_point
	};

	class base_object abstract
	{
	public:
		virtual ~base_object() { }

		template <typename T>
		T *as() { return dynamic_cast<T *>(this); }
		template <typename T>
		const T *as() const { return dynamic_cast<const T *>(this); }
	};

	struct texture final
	{
		std::string name, unique_name, effect_filename;
		unsigned int width = 0, height = 0, levels = 0;
		texture_format format = texture_format::unknown;
		std::unordered_map<std::string, variant> annotations;
		texture_reference impl_reference = texture_reference::none;
		std::unique_ptr<base_object> impl;
	};
	struct uniform final
	{
		std::string name, unique_name, effect_filename;
		uniform_datatype basetype = uniform_datatype::floating_point;
		uniform_datatype displaytype = uniform_datatype::floating_point;
		unsigned int rows = 0, columns = 0, elements = 0;
		size_t storage_offset = 0, storage_size = 0;
		std::unordered_map<std::string, variant> annotations;
		bool hidden = false;
	};
	struct technique final
	{
		std::string name, effect_filename;
		std::vector<std::unique_ptr<base_object>> passes;
		std::unordered_map<std::string, variant> annotations;
		bool enabled = false, hidden = false;
		int toggle_key = 0;
		bool toggle_key_ctrl = false, toggle_key_shift = false, toggle_key_alt = false;
		moving_average<uint64_t, 512> average_duration;
		int uniform_storage_offset = 0, uniform_storage_index = -1;
	};
}
