/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include "variant.hpp"
#include "moving_average.hpp"
#include "effect_expression.hpp"

namespace reshade
{
	enum class texture_filter
	{
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

	class base_object abstract
	{
	public:
		virtual ~base_object() { }

		template <typename T>
		T *as() { return dynamic_cast<T *>(this); }
		template <typename T>
		const T *as() const { return dynamic_cast<const T *>(this); }
	};

	struct texture final : reshadefx::texture_info
	{
		texture(const reshadefx::texture_info &init) : texture_info(init) { }

		std::string effect_filename;
		texture_reference impl_reference = texture_reference::none;
		std::unique_ptr<base_object> impl;
	};
	struct uniform final : reshadefx::uniform_info
	{
		uniform(const reshadefx::uniform_info &init) : uniform_info(init) { }

		std::string effect_filename;
		size_t storage_offset = 0;
		bool hidden = false;
	};
	struct technique final : reshadefx::technique_info
	{
		technique(const reshadefx::technique_info &init) : technique_info(init) { }

		std::string effect_filename;
		std::vector<std::unique_ptr<base_object>> passes_data;
		bool hidden = false;
		bool enabled = false;
		int32_t timeout = 0;
		int32_t timeleft = 0;
		uint32_t toggle_key_data[4];
		moving_average<uint64_t, 60> average_cpu_duration;
		moving_average<uint64_t, 60> average_gpu_duration;
		std::unique_ptr<base_object> impl;
	};
}
