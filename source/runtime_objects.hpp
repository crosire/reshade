/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include "effect_module.hpp"

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
		freepie,
		overlay_open,
		bufready_depth,
	};

	enum class texture_reference
	{
		none,
		back_buffer,
		depth_buffer
	};

	template <typename T, size_t SAMPLES>
	class moving_average
	{
	public:
		moving_average() : _index(0), _average(0), _tick_sum(0), _tick_list() { }

		inline operator T() const { return _average; }

		void clear()
		{
			_index = 0;
			_average = 0;
			_tick_sum = 0;

			for (size_t i = 0; i < SAMPLES; i++)
			{
				_tick_list[i] = 0;
			}
		}
		void append(T value)
		{
			_tick_sum -= _tick_list[_index];
			_tick_sum += _tick_list[_index] = value;
			_index = ++_index % SAMPLES;
			_average = _tick_sum / SAMPLES;
		}

	private:
		size_t _index;
		T _average, _tick_sum, _tick_list[SAMPLES];
	};

	struct texture final : reshadefx::texture_info
	{
		texture() {} // For standalone textures like the font atlas
		texture(const reshadefx::texture_info &init) : texture_info(init) {}

		int annotation_as_int(const char *ann_name, size_t i = 0) const
		{
			const auto it = std::find_if(annotations.begin(), annotations.end(),
				[ann_name](const auto &annotation) { return annotation.name == ann_name; });
			if (it == annotations.end()) return 0;
			return it->type.is_integral() ? it->value.as_int[i] : static_cast<int>(it->value.as_float[i]);
		}
		float annotation_as_float(const char *ann_name, size_t i = 0) const
		{
			const auto it = std::find_if(annotations.begin(), annotations.end(),
				[ann_name](const auto &annotation) { return annotation.name == ann_name; });
			if (it == annotations.end()) return 0.0f;
			return it->type.is_floating_point() ? it->value.as_float[i] : static_cast<float>(it->value.as_int[i]);
		}
		std::string_view annotation_as_string(const char *ann_name) const
		{
			const auto it = std::find_if(annotations.begin(), annotations.end(),
				[ann_name](const auto &annotation) { return annotation.name == ann_name; });
			if (it == annotations.end()) return std::string_view();
			return it->value.string_data;
		}

		bool matches_description(const reshadefx::texture_info &desc) const
		{
			return width == desc.width && height == desc.height && levels == desc.levels && format == desc.format;
		}

		void *impl = nullptr;
		size_t effect_index = std::numeric_limits<size_t>::max();
		texture_reference impl_reference = texture_reference::none;
		bool shared = false;
		bool loaded = false;
	};

	struct uniform final : reshadefx::uniform_info
	{
		uniform(const reshadefx::uniform_info &init) : uniform_info(init) {}

		int annotation_as_int(const char *ann_name, size_t i = 0) const
		{
			const auto it = std::find_if(annotations.begin(), annotations.end(),
				[ann_name](const auto &annotation) { return annotation.name == ann_name; });
			if (it == annotations.end()) return 0;
			return it->type.is_integral() ? it->value.as_int[i] : static_cast<int>(it->value.as_float[i]);
		}
		float annotation_as_float(const char *ann_name, size_t i = 0) const
		{
			const auto it = std::find_if(annotations.begin(), annotations.end(),
				[ann_name](const auto &annotation) { return annotation.name == ann_name; });
			if (it == annotations.end()) return 0.0f;
			return it->type.is_floating_point() ? it->value.as_float[i] : static_cast<float>(it->value.as_int[i]);
		}
		std::string_view annotation_as_string(const char *ann_name) const
		{
			const auto it = std::find_if(annotations.begin(), annotations.end(),
				[ann_name](const auto &annotation) { return annotation.name == ann_name; });
			if (it == annotations.end()) return std::string_view();
			return it->value.string_data;
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
		special_uniform special = special_uniform::none;
		uint32_t toggle_key_data[4] = {};
	};

	struct technique final : reshadefx::technique_info
	{
		technique(const reshadefx::technique_info &init) : technique_info(init) {}

		int annotation_as_int(const char *ann_name, size_t i = 0) const
		{
			const auto it = std::find_if(annotations.begin(), annotations.end(),
				[ann_name](const auto &annotation) { return annotation.name == ann_name; });
			if (it == annotations.end()) return 0;
			return it->type.is_integral() ? it->value.as_int[i] : static_cast<int>(it->value.as_float[i]);
		}
		float annotation_as_float(const char *ann_name, size_t i = 0) const
		{
			const auto it = std::find_if(annotations.begin(), annotations.end(),
				[ann_name](const auto &annotation) { return annotation.name == ann_name; });
			if (it == annotations.end()) return 0.0f;
			return it->type.is_floating_point() ? it->value.as_float[i] : static_cast<float>(it->value.as_int[i]);
		}
		std::string_view annotation_as_string(const char *ann_name) const
		{
			const auto it = std::find_if(annotations.begin(), annotations.end(),
				[ann_name](const auto &annotation) { return annotation.name == ann_name; });
			if (it == annotations.end()) return std::string_view();
			return it->value.string_data;
		}

		void *impl = nullptr;
		size_t effect_index = std::numeric_limits<size_t>::max();
		bool hidden = false;
		bool enabled = false;
		int64_t time_left = 0;
		uint32_t toggle_key_data[4] = {};
		moving_average<uint64_t, 60> average_cpu_duration;
		moving_average<uint64_t, 60> average_gpu_duration;
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
		std::vector<std::pair<std::string, std::string>> definitions;
		std::unordered_map<std::string, std::string> assembly;
		std::vector<uniform> uniforms;
		std::vector<unsigned char> uniform_data_storage;
	};
}
