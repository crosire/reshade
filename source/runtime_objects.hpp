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

	class base_object
	{
	public:
		virtual ~base_object() {}

		template <typename T>
		T *as() { return dynamic_cast<T *>(this); }
		template <typename T>
		const T *as() const { return dynamic_cast<const T *>(this); }
	};

	struct effect final
	{
		unsigned int rendering = 0;
		bool compile_sucess = false;
		std::string errors;
		std::string preamble;
		reshadefx::module module;
		std::filesystem::path source_file;
		std::vector<std::filesystem::path> included_files;
		std::vector<std::string> macro_ifdefs;
		std::unordered_map<std::string, std::string> assembly;
		size_t storage_offset = 0, storage_size = 0;
	};

	struct texture final : reshadefx::texture_info
	{
		texture() {}
		texture(const reshadefx::texture_info &init) : texture_info(init) {}

		int annotation_as_int(const char *ann_name, size_t i = 0) const
		{
			const auto it = annotations.find(ann_name);
			if (it == annotations.end()) return 0;
			return it->second.first.is_integral() ? it->second.second.as_int[i] : static_cast<int>(it->second.second.as_float[i]);
		}
		float annotation_as_float(const char *ann_name, size_t i = 0) const
		{
			const auto it = annotations.find(ann_name);
			if (it == annotations.end()) return 0.0f;
			return it->second.first.is_floating_point() ? it->second.second.as_float[i] : static_cast<float>(it->second.second.as_int[i]);
		}
		std::string_view annotation_as_string(const char *ann_name) const
		{
			const auto it = annotations.find(ann_name);
			if (it == annotations.end()) return std::string_view();
			return it->second.second.string_data;
		}

		bool matches_description(const reshadefx::texture_info &desc) const
		{
			return width == desc.width && height == desc.height && levels == desc.levels && format == desc.format;
		}

		size_t effect_index = std::numeric_limits<size_t>::max();
		texture_reference impl_reference = texture_reference::none;
		std::unique_ptr<base_object> impl;
		bool shared = false;
	};

	struct uniform final : reshadefx::uniform_info
	{
		uniform(const reshadefx::uniform_info &init) : uniform_info(init) {}

		int annotation_as_int(const char *ann_name, size_t i = 0) const
		{
			const auto it = annotations.find(ann_name);
			if (it == annotations.end()) return 0;
			return it->second.first.is_integral() ? it->second.second.as_int[i] : static_cast<int>(it->second.second.as_float[i]);
		}
		float annotation_as_float(const char *ann_name, size_t i = 0) const
		{
			const auto it = annotations.find(ann_name);
			if (it == annotations.end()) return 0.0f;
			return it->second.first.is_floating_point() ? it->second.second.as_float[i] : static_cast<float>(it->second.second.as_int[i]);
		}
		std::string_view annotation_as_string(const char *ann_name) const
		{
			const auto it = annotations.find(ann_name);
			if (it == annotations.end()) return std::string_view();
			return it->second.second.string_data;
		}

		bool supports_toggle_key() const
		{
			if (type.base == reshadefx::type::t_bool)
				return true;
			if (type.base != reshadefx::type::t_int && type.base != reshadefx::type::t_uint)
				return false;
			const std::string_view ui_type = annotation_as_string("ui_type");
			return ui_type == "list" || ui_type == "combo" || ui_type == "radio";
		}

		size_t effect_index = std::numeric_limits<size_t>::max();
		size_t storage_offset = 0;
		special_uniform special = special_uniform::none;
		uint32_t toggle_key_data[4] = {};
	};

	struct technique final : reshadefx::technique_info
	{
		technique(const reshadefx::technique_info &init) : technique_info(init) {}

		int annotation_as_int(const char *ann_name, size_t i = 0) const
		{
			const auto it = annotations.find(ann_name);
			if (it == annotations.end()) return 0;
			return it->second.first.is_integral() ? it->second.second.as_int[i] : static_cast<int>(it->second.second.as_float[i]);
		}
		float annotation_as_float(const char *ann_name, size_t i = 0) const
		{
			const auto it = annotations.find(ann_name);
			if (it == annotations.end()) return 0.0f;
			return it->second.first.is_floating_point() ? it->second.second.as_float[i] : static_cast<float>(it->second.second.as_int[i]);
		}
		std::string_view annotation_as_string(const char *ann_name) const
		{
			const auto it = annotations.find(ann_name);
			if (it == annotations.end()) return std::string_view();
			return it->second.second.string_data;
		}

		size_t effect_index = std::numeric_limits<size_t>::max();
		std::vector<std::unique_ptr<base_object>> passes_data;
		bool hidden = false;
		bool enabled = false;
		int32_t timeout = 0;
		int64_t timeleft = 0;
		uint32_t toggle_key_data[4] = {};
		moving_average<uint64_t, 60> average_cpu_duration;
		moving_average<uint64_t, 60> average_gpu_duration;
		std::unique_ptr<base_object> impl;
	};
}
