/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include "effect_expression.hpp"
#include "moving_average.hpp"
#include <filesystem>

namespace reshade
{
	enum class special_uniform
	{
		none,
		frame_time,
		frame_count,
		random,
		ping_pong,
		date,
		timer,
		key,
		mouse_point,
		mouse_delta,
		mouse_button,
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

	struct effect_data
	{
		size_t index = std::numeric_limits<size_t>::max();
		unsigned int rendering = 0;
		bool compile_sucess = false;
		std::string errors;
		reshadefx::module module;
		std::filesystem::path source_file;
		size_t storage_offset = 0, storage_size = 0;
	};

	struct texture final : reshadefx::texture_info
	{
		texture() {}
		texture(const reshadefx::texture_info &init) : texture_info(init) { }

		size_t effect_index = std::numeric_limits<size_t>::max();
		texture_reference impl_reference = texture_reference::none;
		std::unique_ptr<base_object> impl;
		bool shared = false;
	};

	struct uniform final : reshadefx::uniform_info
	{
		uniform(const reshadefx::uniform_info &init) : uniform_info(init) { }

		size_t effect_index = std::numeric_limits<size_t>::max();
		size_t storage_offset = 0;
		bool hidden = false;
		special_uniform special = special_uniform::none;
	};

	struct technique final : reshadefx::technique_info
	{
		technique(const reshadefx::technique_info &init) : technique_info(init) { }

		size_t effect_index = std::numeric_limits<size_t>::max();
		std::vector<std::unique_ptr<base_object>> passes_data;
		bool hidden = false;
		bool enabled = false;
		int32_t timeout = 0;
		int64_t timeleft = 0;
		uint32_t toggle_key_data[4];
		moving_average<uint64_t, 60> average_cpu_duration;
		moving_average<uint64_t, 60> average_gpu_duration;
		std::unique_ptr<base_object> impl;
	};
}
