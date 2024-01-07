/*
 * Copyright (C) 2014 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "effect_module.hpp"
#include "moving_average.hpp"

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
		overlay_open,
		overlay_active,
		overlay_hovered,
		screenshot,
		unknown
	};

#if RESHADE_FX
	struct texture final : reshadefx::texture_info
	{
		texture(const reshadefx::texture_info &init) : texture_info(init) {}

		auto annotation_as_int(const std::string_view ann_name, size_t i = 0, int default_value = 0) const
		{
			const auto it = std::find_if(annotations.cbegin(), annotations.cend(),
				[ann_name](const reshadefx::annotation &annotation) { return annotation.name == ann_name; });
			return it != annotations.cend() && i < 16 ?
				(it->type.is_integral() ? it->value.as_int[i] : static_cast<int>(it->value.as_float[i])) : default_value;
		}
		auto annotation_as_uint(const std::string_view ann_name, size_t i = 0, unsigned int default_value = 0) const
		{
			const auto it = std::find_if(annotations.cbegin(), annotations.cend(),
				[ann_name](const reshadefx::annotation &annotation) { return annotation.name == ann_name; });
			return it != annotations.cend() && i < 16 ?
				(it->type.is_integral() ? it->value.as_uint[i] : static_cast<unsigned int>(it->value.as_float[i])) : default_value;
		}
		auto annotation_as_float(const std::string_view ann_name, size_t i = 0, float default_value = 0.0f) const
		{
			const auto it = std::find_if(annotations.cbegin(), annotations.cend(),
				[ann_name](const reshadefx::annotation &annotation) { return annotation.name == ann_name; });
			return it != annotations.cend() && i < 16 ?
				(it->type.is_floating_point() ? it->value.as_float[i] : static_cast<float>(it->value.as_int[i])) : default_value;
		}
		auto annotation_as_string(const std::string_view ann_name, const std::string_view default_value = std::string_view()) const
		{
			const auto it = std::find_if(annotations.cbegin(), annotations.cend(),
				[ann_name](const reshadefx::annotation &annotation) { return annotation.name == ann_name; });
			return it != annotations.cend() ? std::string_view(it->value.string_data) : default_value;
		}

		bool matches_description(const reshadefx::texture_info &desc) const
		{
			return type == desc.type && width == desc.width && height == desc.height && levels == desc.levels && format == desc.format;
		}

		size_t effect_index = std::numeric_limits<size_t>::max();

		std::vector<size_t> shared;
		bool loaded = false;

		api::resource resource = {};
		api::resource_view srv[2] = {};
		api::resource_view rtv[2] = {};
		std::vector<api::resource_view> uav;
	};

	struct uniform final : reshadefx::uniform_info
	{
		uniform(const reshadefx::uniform_info &init) : uniform_info(init) {}

		auto annotation_as_int(const std::string_view ann_name, size_t i = 0, int default_value = 0) const
		{
			const auto it = std::find_if(annotations.cbegin(), annotations.cend(),
				[ann_name](const reshadefx::annotation &annotation) { return annotation.name == ann_name; });
			return it != annotations.cend() && i < 16 ?
				(it->type.is_integral() ? it->value.as_int[i] : static_cast<int>(it->value.as_float[i])) : default_value;
		}
		auto annotation_as_uint(const std::string_view ann_name, size_t i = 0, unsigned int default_value = 0) const
		{
			const auto it = std::find_if(annotations.cbegin(), annotations.cend(),
				[ann_name](const reshadefx::annotation &annotation) { return annotation.name == ann_name; });
			return it != annotations.cend() && i < 16 ?
				(it->type.is_integral() ? it->value.as_uint[i] : static_cast<unsigned int>(it->value.as_float[i])) : default_value;
		}
		auto annotation_as_float(const std::string_view ann_name, size_t i = 0, float default_value = 0.0f) const
		{
			const auto it = std::find_if(annotations.cbegin(), annotations.cend(),
				[ann_name](const reshadefx::annotation &annotation) { return annotation.name == ann_name; });
			return it != annotations.cend() && i < 16 ?
				(it->type.is_floating_point() ? it->value.as_float[i] : static_cast<float>(it->value.as_int[i])) : default_value;
		}
		auto annotation_as_string(const std::string_view ann_name, const std::string_view default_value = std::string_view()) const
		{
			const auto it = std::find_if(annotations.cbegin(), annotations.cend(),
				[ann_name](const reshadefx::annotation &annotation) { return annotation.name == ann_name; });
			return it != annotations.cend() ?
				std::string_view(it->value.string_data) : default_value;
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
		unsigned int toggle_key_data[4] = {};

		special_uniform special = special_uniform::none;
	};

	struct technique final : reshadefx::technique_info
	{
		technique(const reshadefx::technique_info &init) : technique_info(init) {}

		auto annotation_as_int(const std::string_view ann_name, size_t i = 0, int default_value = 0) const
		{
			const auto it = std::find_if(annotations.cbegin(), annotations.cend(),
				[ann_name](const reshadefx::annotation &annotation) { return annotation.name == ann_name; });
			return it != annotations.cend() && i < 16 ?
				(it->type.is_integral() ? it->value.as_int[i] : static_cast<int>(it->value.as_float[i])) : default_value;
		}
		auto annotation_as_uint(const std::string_view ann_name, size_t i = 0, unsigned int default_value = 0) const
		{
			const auto it = std::find_if(annotations.cbegin(), annotations.cend(),
				[ann_name](const reshadefx::annotation &annotation) { return annotation.name == ann_name; });
			return it != annotations.cend() && i < 16 ?
				(it->type.is_integral() ? it->value.as_uint[i] : static_cast<unsigned int>(it->value.as_float[i])) : default_value;
		}
		auto annotation_as_float(const std::string_view ann_name, size_t i = 0, float default_value = 0.0f) const
		{
			const auto it = std::find_if(annotations.cbegin(), annotations.cend(),
				[ann_name](const reshadefx::annotation &annotation) { return annotation.name == ann_name; });
			return it != annotations.cend() && i < 16 ?
				(it->type.is_floating_point() ? it->value.as_float[i] : static_cast<float>(it->value.as_int[i])) : default_value;
		}
		auto annotation_as_string(const std::string_view ann_name, const std::string_view default_value = std::string_view()) const
		{
			const auto it = std::find_if(annotations.cbegin(), annotations.cend(),
				[ann_name](const reshadefx::annotation &annotation) { return annotation.name == ann_name; });
			return it != annotations.cend() ?
				std::string_view(it->value.string_data) : default_value;
		}

		size_t effect_index = std::numeric_limits<size_t>::max();
		unsigned int toggle_key_data[4] = {};

		bool hidden = false;
		bool enabled = false;
		bool enabled_in_screenshot = true;
		int64_t time_left = 0;

		struct pass_data
		{
			api::resource_view render_target_views[8] = {};
			api::pipeline pipeline = {};
			api::descriptor_table texture_table = {};
			api::descriptor_table storage_table = {};
			std::vector<api::resource> modified_resources;
			std::vector<api::resource_view> generate_mipmap_views;
		};

		std::vector<pass_data> passes_data;
		uint32_t query_base_index = 0;
		moving_average<uint64_t, 60> average_cpu_duration;
		moving_average<uint64_t, 60> average_gpu_duration;
	};

	struct effect
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
		std::unordered_map<std::string, std::string> assembly;
		std::unordered_map<std::string, std::string> assembly_text;

		std::vector<uniform> uniforms;
		std::vector<uint8_t> uniform_data_storage;

		api::query_heap query_heap = {};
		api::resource cb = {};
		api::pipeline_layout layout = {};

		api::descriptor_table cb_table = {};
		api::descriptor_table sampler_table = {};

		struct binding_data
		{
			std::string semantic;
			api::descriptor_table table;
			uint32_t index;
			api::sampler sampler;
			bool srgb;
		};
		std::vector<binding_data> texture_semantic_to_binding;
	};
#endif
}
