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
		mouse_wheel,
		freepie,
		overlay_open,
		overlay_active,
		overlay_hovered,
		unknown
	};

	template <typename T, size_t SAMPLES>
	class moving_average
	{
	public:
		moving_average() : _index(0), _average(0), _tick_sum(0), _tick_list() {}

		inline operator T() const { return _average; }

		void clear()
		{
			_index = 0;
			_average = 0;
			_tick_sum = 0;

			for (size_t i = 0; i < SAMPLES; i++)
				_tick_list[i] = 0;
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

#if RESHADE_FX
	struct texture final : reshadefx::texture_info
	{
		texture(const reshadefx::texture_info &init) : texture_info(init) {}

		auto annotation_as_int(const char *ann_name, size_t i = 0) const
		{
			const auto it = std::find_if(annotations.begin(), annotations.end(),
				[ann_name](const auto &annotation) { return annotation.name == ann_name; });
			if (it == annotations.end()) return 0;
			return it->type.is_integral() ? it->value.as_int[i] : static_cast<int>(it->value.as_float[i]);
		}
		auto annotation_as_uint(const char *ann_name, size_t i = 0) const
		{
			const auto it = std::find_if(annotations.begin(), annotations.end(),
				[ann_name](const auto &annotation) { return annotation.name == ann_name; });
			if (it == annotations.end()) return 0u;
			return it->type.is_integral() ? it->value.as_uint[i] : static_cast<unsigned int>(it->value.as_float[i]);
		}
		auto annotation_as_float(const char *ann_name, size_t i = 0) const
		{
			const auto it = std::find_if(annotations.begin(), annotations.end(),
				[ann_name](const auto &annotation) { return annotation.name == ann_name; });
			if (it == annotations.end()) return 0.0f;
			return it->type.is_floating_point() ? it->value.as_float[i] : static_cast<float>(it->value.as_int[i]);
		}
		auto annotation_as_string(const char *ann_name) const
		{
			const auto it = std::find_if(annotations.begin(), annotations.end(),
				[ann_name](const auto &annotation) { return annotation.name == ann_name; });
			if (it == annotations.end()) return std::string_view();
			return std::string_view(it->value.string_data);
		}

		bool matches_description(const reshadefx::texture_info &desc) const
		{
			return width == desc.width && height == desc.height && levels == desc.levels && format == desc.format;
		}

		size_t effect_index = std::numeric_limits<size_t>::max();
		std::vector<size_t> shared;
		bool loaded = false;

		api::resource resource = {};
		api::resource_view srv[2] = {};
		api::resource_view rtv[2] = {};
		api::resource_view uav = {};
	};

	struct uniform final : reshadefx::uniform_info
	{
		uniform(const reshadefx::uniform_info &init) : uniform_info(init) {}

		auto annotation_as_int(const char *ann_name, size_t i = 0, int default_value = 0) const
		{
			const auto it = std::find_if(annotations.begin(), annotations.end(),
				[ann_name](const auto &annotation) { return annotation.name == ann_name; });
			if (it == annotations.end()) return default_value;
			return it->type.is_integral() ? it->value.as_int[i] : static_cast<int>(it->value.as_float[i]);
		}
		auto annotation_as_uint(const char *ann_name, size_t i = 0, unsigned int default_value = 0) const
		{
			const auto it = std::find_if(annotations.begin(), annotations.end(),
				[ann_name](const auto &annotation) { return annotation.name == ann_name; });
			if (it == annotations.end()) return default_value;
			return it->type.is_integral() ? it->value.as_uint[i] : static_cast<unsigned int>(it->value.as_float[i]);
		}
		auto annotation_as_float(const char *ann_name, size_t i = 0, float default_value = 0.0f) const
		{
			const auto it = std::find_if(annotations.begin(), annotations.end(),
				[ann_name](const auto &annotation) { return annotation.name == ann_name; });
			if (it == annotations.end()) return default_value;
			return it->type.is_floating_point() ? it->value.as_float[i] : static_cast<float>(it->value.as_int[i]);
		}
		auto annotation_as_string(const char *ann_name, const std::string_view &default_value = std::string_view()) const
		{
			const auto it = std::find_if(annotations.begin(), annotations.end(),
				[ann_name](const auto &annotation) { return annotation.name == ann_name; });
			if (it == annotations.end()) return default_value;
			return std::string_view(it->value.string_data);
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
		unsigned int toggle_key_data[4] = {};
	};

	struct technique final : reshadefx::technique_info
	{
		technique(const reshadefx::technique_info &init) : technique_info(init) {}

		auto annotation_as_int(const char *ann_name, size_t i = 0) const
		{
			const auto it = std::find_if(annotations.begin(), annotations.end(),
				[ann_name](const auto &annotation) { return annotation.name == ann_name; });
			if (it == annotations.end()) return 0;
			return it->type.is_integral() ? it->value.as_int[i] : static_cast<int>(it->value.as_float[i]);
		}
		auto annotation_as_uint(const char *ann_name, size_t i = 0) const
		{
			const auto it = std::find_if(annotations.begin(), annotations.end(),
				[ann_name](const auto &annotation) { return annotation.name == ann_name; });
			if (it == annotations.end()) return 0u;
			return it->type.is_integral() ? it->value.as_uint[i] : static_cast<unsigned int>(it->value.as_float[i]);
		}
		auto annotation_as_float(const char *ann_name, size_t i = 0) const
		{
			const auto it = std::find_if(annotations.begin(), annotations.end(),
				[ann_name](const auto &annotation) { return annotation.name == ann_name; });
			if (it == annotations.end()) return 0.0f;
			return it->type.is_floating_point() ? it->value.as_float[i] : static_cast<float>(it->value.as_int[i]);
		}
		auto annotation_as_string(const char *ann_name) const
		{
			const auto it = std::find_if(annotations.begin(), annotations.end(),
				[ann_name](const auto &annotation) { return annotation.name == ann_name; });
			if (it == annotations.end()) return std::string_view();
			return std::string_view(it->value.string_data);
		}

		size_t effect_index = std::numeric_limits<size_t>::max();
		bool hidden = false;
		bool enabled = false;
		int64_t time_left = 0;
		unsigned int toggle_key_data[4] = {};
		moving_average<uint64_t, 60> average_cpu_duration;
		moving_average<uint64_t, 60> average_gpu_duration;

		struct pass_data
		{
			api::resource_view render_target_views[8] = {};
			api::pipeline pipeline = {};
			api::descriptor_set texture_set = {};
			api::descriptor_set storage_set = {};
			std::vector<api::resource> modified_resources;
			std::vector<api::resource_view> generate_mipmap_views;
		};

		std::vector<pass_data> passes_data;
		uint32_t query_base_index = 0;
	};

	struct effect final
	{
		unsigned int rendering = 0;
		bool skipped = false;
		bool compiled = false;
		bool preprocessed = false;
		std::string errors;
		reshadefx::module module;
		size_t source_hash = 0;
		std::filesystem::path source_file;
		std::vector<std::filesystem::path> included_files;
		std::vector<std::pair<std::string, std::string>> definitions;
		std::unordered_map<std::string, std::pair<std::string, std::string>> assembly;
		std::vector<uniform> uniforms;
		std::vector<unsigned char> uniform_data_storage;

		struct binding_data
		{
			std::string semantic;
			api::descriptor_set set;
			uint32_t index;
			api::sampler sampler;
			bool srgb;
		};

		api::resource cb = {};
		api::pipeline_layout layout = {};
		api::descriptor_set cb_set = {};
		api::descriptor_set sampler_set = {};
		api::query_pool query_pool = {};
		std::vector<binding_data> texture_semantic_to_binding;
	};
#endif

	struct screenshot final
	{
		bool enabled = false;

		int error_code = 0;
		std::string error_message;
		std::filesystem::path path;

		bool running = false;
		std::thread thread;

		std::vector<uint8_t> data;
		int color = 0;
		unsigned int width = 0;
		unsigned int height = 0;
	};
}
