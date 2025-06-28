/*
 * Copyright (C) 2022 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "runtime.hpp"
#include "runtime_internal.hpp"
#include "ini_file.hpp"
#include "addon_manager.hpp"
#include "input.hpp"
#include <algorithm> // std::all_of, std::find, std::find_if, std::for_each, std::remove_if

extern bool resolve_path(std::filesystem::path &path, std::error_code &ec);
extern bool resolve_preset_path(std::filesystem::path &path, std::error_code &ec);

bool reshade::runtime::is_key_down(uint32_t keycode) const
{
	return _input != nullptr && _input->is_key_down(keycode);
}
bool reshade::runtime::is_key_pressed(uint32_t keycode) const
{
	return _input != nullptr && _input->is_key_pressed(keycode);
}
bool reshade::runtime::is_key_released(uint32_t keycode) const
{
	return _input != nullptr && _input->is_key_released(keycode);
}
bool reshade::runtime::is_mouse_button_down(uint32_t button) const
{
	return _input != nullptr && _input->is_mouse_button_down(button);
}
bool reshade::runtime::is_mouse_button_pressed(uint32_t button) const
{
	return _input != nullptr && _input->is_mouse_button_pressed(button);
}
bool reshade::runtime::is_mouse_button_released(uint32_t button) const
{
	return _input != nullptr && _input->is_mouse_button_released(button);
}

uint32_t reshade::runtime::last_key_pressed() const
{
	return _input != nullptr ? _input->last_key_pressed() : 0u;
}
uint32_t reshade::runtime::last_key_released() const
{
	return _input != nullptr ? _input->last_key_released() : 0u;
}

void reshade::runtime::get_mouse_cursor_position(uint32_t *out_x, uint32_t *out_y, int16_t *out_wheel_delta) const
{
	if (out_x != nullptr)
		*out_x = (_input != nullptr) ? _input->mouse_position_x() : 0;
	if (out_y != nullptr)
		*out_y = (_input != nullptr) ? _input->mouse_position_y() : 0;
	if (out_wheel_delta != nullptr)
		*out_wheel_delta = (_input != nullptr) ? _input->mouse_wheel_delta() : 0;
}

void reshade::runtime::block_input_next_frame()
{
#if RESHADE_GUI
	_block_input_next_frame = true;
#endif
}

void reshade::runtime::enumerate_uniform_variables(const char *effect_name_in, void(*callback)(effect_runtime *runtime, api::effect_uniform_variable variable, void *user_data), void *user_data)
{
	if (is_loading())
		return;

	const std::filesystem::path effect_name = effect_name_in != nullptr ? std::filesystem::u8path(effect_name_in) : std::filesystem::path();

	for (const effect &effect : _effects)
	{
		if (effect_name_in != nullptr && effect.source_file.filename() != effect_name)
			continue;

		for (const uniform &variable : effect.uniforms)
			callback(this, { reinterpret_cast<uintptr_t>(&variable) }, user_data);

		if (effect_name_in != nullptr)
			break;
	}
}

reshade::api::effect_uniform_variable reshade::runtime::find_uniform_variable(const char *effect_name_in, const char *variable_name_in) const
{
	if (is_loading() || variable_name_in == nullptr)
		return { 0 };

	const std::filesystem::path effect_name = effect_name_in != nullptr ? std::filesystem::u8path(effect_name_in) : std::filesystem::path();
	const std::string_view variable_name(variable_name_in);

	for (const effect &effect : _effects)
	{
		if (effect_name_in != nullptr && effect.source_file.filename() != effect_name)
			continue;

		for (const uniform &variable : effect.uniforms)
		{
			if (variable.name == variable_name)
				return { reinterpret_cast<uintptr_t>(&variable) };
		}

		if (effect_name_in != nullptr)
			break;
	}

	return { 0 };
}

void reshade::runtime::get_uniform_variable_type(api::effect_uniform_variable handle, api::format *out_base_type, uint32_t *out_rows, uint32_t *out_columns, uint32_t *out_array_length) const
{
	if (const auto variable = reinterpret_cast<const uniform *>(handle.handle))
	{
		if (out_base_type != nullptr)
		{
			switch (variable->type.base)
			{
			case reshadefx::type::t_bool:
				*out_base_type = api::format::r32_typeless;
				break;
			case reshadefx::type::t_min16int:
				*out_base_type = api::format::r16_sint;
				break;
			case reshadefx::type::t_int:
				*out_base_type = api::format::r32_sint;
				break;
			case reshadefx::type::t_min16uint:
				*out_base_type = api::format::r16_uint;
				break;
			case reshadefx::type::t_uint:
				*out_base_type = api::format::r32_uint;
				break;
			case reshadefx::type::t_min16float:
				*out_base_type = api::format::r16_float;
				break;
			case reshadefx::type::t_float:
				*out_base_type = api::format::r32_float;
				break;
			default:
				*out_base_type = api::format::unknown;
				break;
			}
		}

		if (out_rows != nullptr)
			*out_rows = variable->type.rows;
		if (out_columns != nullptr)
			*out_columns = variable->type.cols;
		if (out_array_length != nullptr)
			*out_array_length = variable->type.array_length;
		return;
	}

	if (out_base_type != nullptr)
		*out_base_type = api::format::unknown;
	if (out_rows != nullptr)
		*out_rows = 0;
	if (out_columns != nullptr)
		*out_columns = 0;
	if (out_array_length != nullptr)
		*out_array_length = 0;
}

void reshade::runtime::get_uniform_variable_name(api::effect_uniform_variable handle, char *value, size_t *size) const
{
	if (size == nullptr)
		return;

	if (const auto variable = reinterpret_cast<const uniform *>(handle.handle))
	{
		if (value == nullptr)
		{
			*size = variable->name.size() + 1;
		}
		else if (*size != 0)
		{
			*size = variable->name.copy(value, *size - 1);
			value[*size] = '\0';
		}
	}
	else
	{
		*size = 0;
	}
}
void reshade::runtime::get_uniform_variable_effect_name(api::effect_uniform_variable handle, char *value, size_t *size) const
{
	if (size == nullptr)
		return;

	if (const auto variable = reinterpret_cast<const uniform *>(handle.handle))
	{
		const std::string effect_name = _effects[variable->effect_index].source_file.filename().u8string();

		if (value == nullptr)
		{
			*size = effect_name.size() + 1;
		}
		else if (*size != 0)
		{
			*size = effect_name.copy(value, *size - 1);
			value[*size] = '\0';
		}
	}
	else
	{
		*size = 0;
	}
}

bool reshade::runtime::get_annotation_bool_from_uniform_variable(api::effect_uniform_variable handle, const char *name_in, bool *values, size_t count, size_t array_index) const
{
	if (handle.handle != 0 && name_in != nullptr)
	{
		const uniform &variable = *reinterpret_cast<const uniform *>(handle.handle);
		const std::string_view name(name_in);

		if (const auto it = std::find_if(variable.annotations.cbegin(), variable.annotations.cend(),
				[&](const reshadefx::annotation &annotation) { return annotation.name == name; });
			it != variable.annotations.cend())
		{
			for (size_t i = 0; i < count; ++i)
				values[i] = variable.annotation_as_int(name, i + array_index) != 0;
			return true;
		}
	}

	for (size_t i = 0; i < count; ++i)
		values[i] = false;
	return false;
}
bool reshade::runtime::get_annotation_float_from_uniform_variable(api::effect_uniform_variable handle, const char *name_in, float *values, size_t count, size_t array_index) const
{
	if (handle.handle != 0 && name_in != nullptr)
	{
		const uniform &variable = *reinterpret_cast<const uniform *>(handle.handle);
		const std::string_view name(name_in);

		if (const auto it = std::find_if(variable.annotations.cbegin(), variable.annotations.cend(),
				[&](const reshadefx::annotation &annotation) { return annotation.name == name; });
			it != variable.annotations.cend())
		{
			for (size_t i = 0; i < count; ++i)
				values[i] = variable.annotation_as_float(name, i + array_index);
			return true;
		}
	}

	for (size_t i = 0; i < count; ++i)
		values[i] = 0.0f;
	return false;
}
bool reshade::runtime::get_annotation_int_from_uniform_variable(api::effect_uniform_variable handle, const char *name_in, int32_t *values, size_t count, size_t array_index) const
{
	if (handle.handle != 0 && name_in != nullptr)
	{
		const uniform &variable = *reinterpret_cast<const uniform *>(handle.handle);
		const std::string_view name(name_in);

		if (const auto it = std::find_if(variable.annotations.cbegin(), variable.annotations.cend(),
				[&](const reshadefx::annotation &annotation) { return annotation.name == name; });
			it != variable.annotations.cend())
		{
			for (size_t i = 0; i < count; ++i)
				values[i] = variable.annotation_as_int(name, array_index + i);
			return true;
		}
	}

	for (size_t i = 0; i < count; ++i)
		values[i] = 0;
	return false;
}
bool reshade::runtime::get_annotation_uint_from_uniform_variable(api::effect_uniform_variable handle, const char *name_in, uint32_t *values, size_t count, size_t array_index) const
{
	if (handle.handle != 0 && name_in != nullptr)
	{
		const uniform &variable = *reinterpret_cast<const uniform *>(handle.handle);
		const std::string_view name(name_in);

		if (const auto it = std::find_if(variable.annotations.cbegin(), variable.annotations.cend(),
				[&](const reshadefx::annotation &annotation) { return annotation.name == name; });
			it != variable.annotations.cend())
		{
			for (size_t i = 0; i < count; ++i)
				values[i] = variable.annotation_as_uint(name, array_index + i);
			return true;
		}
	}

	for (size_t i = 0; i < count; ++i)
		values[i] = 0;
	return false;
}
bool reshade::runtime::get_annotation_string_from_uniform_variable(api::effect_uniform_variable handle, const char *name_in, char *value, size_t *size) const
{
	if (handle.handle != 0 && name_in != nullptr)
	{
		const uniform &variable = *reinterpret_cast<const uniform *>(handle.handle);
		const std::string_view name(name_in);

		if (const auto it = std::find_if(variable.annotations.cbegin(), variable.annotations.cend(),
				[&](const reshadefx::annotation &annotation) { return annotation.name == name; });
			it != variable.annotations.cend())
		{
			const std::string_view annotation = variable.annotation_as_string(name);

			if (size != nullptr)
			{
				if (value == nullptr)
				{
					*size = annotation.size() + 1;
				}
				else if (*size != 0)
				{
					*size = annotation.copy(value, *size - 1);
					value[*size] = '\0';
				}
			}

			return true;
		}
	}

	if (size != nullptr)
		*size = 0;
	return false;
}

void reshade::runtime::reset_uniform_value(api::effect_uniform_variable handle)
{
	const auto variable = reinterpret_cast<uniform *>(handle.handle);
	if (variable == nullptr)
		return;

#if RESHADE_ADDON
	const bool was_is_in_api_call = _is_in_api_call;
	_is_in_api_call = true;
#endif

	reset_uniform_value(*variable);

#if RESHADE_ADDON
	_is_in_api_call = was_is_in_api_call;
#endif
}

void reshade::runtime::get_uniform_value_bool(api::effect_uniform_variable handle, bool *values, size_t count, size_t array_index) const
{
	if (const auto variable = reinterpret_cast<const uniform *>(handle.handle))
	{
		get_uniform_value(*variable, values, count, array_index);
		return;
	}

	for (size_t i = 0; i < count; ++i)
		values[i] = false;
}
void reshade::runtime::get_uniform_value_float(api::effect_uniform_variable handle, float *values, size_t count, size_t array_index) const
{
	if (const auto variable = reinterpret_cast<const uniform *>(handle.handle))
	{
		get_uniform_value(*variable, values, count, array_index);
		return;
	}

	for (size_t i = 0; i < count; ++i)
		values[i] = 0.0f;
}
void reshade::runtime::get_uniform_value_int(api::effect_uniform_variable handle, int32_t *values, size_t count, size_t array_index) const
{
	if (const auto variable = reinterpret_cast<const uniform *>(handle.handle))
	{
		get_uniform_value(*variable, values, count, array_index);
		return;
	}

	for (size_t i = 0; i < count; ++i)
		values[i] = 0;
}
void reshade::runtime::get_uniform_value_uint(api::effect_uniform_variable handle, uint32_t *values, size_t count, size_t array_index) const
{
	if (const auto variable = reinterpret_cast<const uniform *>(handle.handle))
	{
		get_uniform_value(*variable, values, count, array_index);
		return;
	}

	for (size_t i = 0; i < count; ++i)
		values[i] = 0;
}

void reshade::runtime::set_uniform_value_bool(api::effect_uniform_variable handle, const bool *values, size_t count, size_t array_index)
{
	const auto variable = reinterpret_cast<uniform *>(handle.handle);
	if (variable == nullptr)
		return;

#if RESHADE_ADDON
	const bool was_is_in_api_call = _is_in_api_call;
	_is_in_api_call = true;
#endif

	set_uniform_value(*variable, values, count, array_index);

#if RESHADE_ADDON
	_is_in_api_call = was_is_in_api_call;
#endif
}
void reshade::runtime::set_uniform_value_float(api::effect_uniform_variable handle, const float *values, size_t count, size_t array_index)
{
	const auto variable = reinterpret_cast<uniform *>(handle.handle);
	if (variable == nullptr)
		return;

#if RESHADE_ADDON
	const bool was_is_in_api_call = _is_in_api_call;
	_is_in_api_call = true;
#endif

	set_uniform_value(*variable, values, count, array_index);

#if RESHADE_ADDON
	_is_in_api_call = was_is_in_api_call;
#endif
}
void reshade::runtime::set_uniform_value_int(api::effect_uniform_variable handle, const int32_t *values, size_t count, size_t array_index)
{
	const auto variable = reinterpret_cast<uniform *>(handle.handle);
	if (variable == nullptr)
		return;

#if RESHADE_ADDON
	const bool was_is_in_api_call = _is_in_api_call;
	_is_in_api_call = true;
#endif

	set_uniform_value(*variable, values, count, array_index);

#if RESHADE_ADDON
	_is_in_api_call = was_is_in_api_call;
#endif
}
void reshade::runtime::set_uniform_value_uint(api::effect_uniform_variable handle, const uint32_t *values, size_t count, size_t array_index)
{
	const auto variable = reinterpret_cast<uniform *>(handle.handle);
	if (variable == nullptr)
		return;

#if RESHADE_ADDON
	const bool was_is_in_api_call = _is_in_api_call;
	_is_in_api_call = true;
#endif

	set_uniform_value(*variable, values, count, array_index);

#if RESHADE_ADDON
	_is_in_api_call = was_is_in_api_call;
#endif
}

void reshade::runtime::enumerate_texture_variables(const char *effect_name_in, void(*callback)(effect_runtime *runtime, api::effect_texture_variable variable, void *user_data), void *user_data)
{
	if (is_loading())
		return;

	const std::filesystem::path effect_name = effect_name_in != nullptr ? std::filesystem::u8path(effect_name_in) : std::filesystem::path();

	for (const texture &variable : _textures)
	{
		if (effect_name_in != nullptr &&
			std::find_if(variable.shared.cbegin(), variable.shared.cend(),
				[&](size_t effect_index) {
					return _effects[effect_index].source_file.filename() == effect_name;
				}) == variable.shared.cend())
			continue;

		callback(this, { reinterpret_cast<uintptr_t>(&variable) }, user_data);
	}
}

reshade::api::effect_texture_variable reshade::runtime::find_texture_variable(const char *effect_name_in, const char *variable_name_in) const
{
	if (is_loading() || variable_name_in == nullptr)
		return { 0 };

	const std::filesystem::path effect_name = effect_name_in != nullptr ? std::filesystem::u8path(effect_name_in) : std::filesystem::path();
	const std::string_view variable_name(variable_name_in);

	for (const texture &variable : _textures)
	{
		if (effect_name_in != nullptr &&
			std::find_if(variable.shared.cbegin(), variable.shared.cend(),
				[&](size_t effect_index) {
					return _effects[effect_index].source_file.filename() == effect_name;
				}) == variable.shared.cend())
			continue;

		if (variable.name != variable_name && variable.unique_name != variable_name)
			continue;

		return { reinterpret_cast<uintptr_t>(&variable) };
	}

	return { 0 };
}

void reshade::runtime::get_texture_variable_name(api::effect_texture_variable handle, char *value, size_t *size) const
{
	if (size == nullptr)
		return;

	if (const auto variable = reinterpret_cast<const texture *>(handle.handle))
	{
		if (value == nullptr)
		{
			*size = variable->name.size() + 1;
		}
		else if (*size != 0)
		{
			*size = variable->name.copy(value, *size - 1);
			value[*size] = '\0';
		}
	}
	else
	{
		*size = 0;
	}
}
void reshade::runtime::get_texture_variable_effect_name(api::effect_texture_variable handle, char *value, size_t *size) const
{
	if (size == nullptr)
		return;

	if (handle.handle != 0)
	{
		const texture &variable = *reinterpret_cast<const texture *>(handle.handle);
		const std::string effect_name = _effects[variable.effect_index].source_file.filename().u8string();

		if (value == nullptr)
		{
			*size = effect_name.size() + 1;
		}
		else if (*size != 0)
		{
			*size = effect_name.copy(value, *size - 1);
			value[*size] = '\0';
		}
	}
	else
	{
		*size = 0;
	}
}

bool reshade::runtime::get_annotation_bool_from_texture_variable(api::effect_texture_variable handle, const char *name_in, bool *values, size_t count, size_t array_index) const
{
	if (handle.handle != 0 && name_in != nullptr)
	{
		const texture &variable = *reinterpret_cast<const texture *>(handle.handle);
		const std::string_view name(name_in);

		if (const auto it = std::find_if(variable.annotations.cbegin(), variable.annotations.cend(),
				[&](const reshadefx::annotation &annotation) { return annotation.name == name; });
			it != variable.annotations.cend())
		{
			for (size_t i = 0; i < count; ++i)
				values[i] = variable.annotation_as_int(name, array_index + i) != 0;
			return true;
		}
	}

	for (size_t i = 0; i < count; ++i)
		values[i] = false;
	return false;
}
bool reshade::runtime::get_annotation_float_from_texture_variable(api::effect_texture_variable handle, const char *name_in, float *values, size_t count, size_t array_index) const
{
	if (handle.handle != 0 && name_in != nullptr)
	{
		const texture &variable = *reinterpret_cast<const texture *>(handle.handle);
		const std::string_view name(name_in);

		if (const auto it = std::find_if(variable.annotations.cbegin(), variable.annotations.cend(),
				[&](const reshadefx::annotation &annotation) { return annotation.name == name; });
			it != variable.annotations.cend())
		{
			for (size_t i = 0; i < count; ++i)
				values[i] = variable.annotation_as_float(name, array_index + i);
			return true;
		}
	}

	for (size_t i = 0; i < count; ++i)
		values[i] = 0.0f;
	return false;
}
bool reshade::runtime::get_annotation_int_from_texture_variable(api::effect_texture_variable handle, const char *name_in, int32_t *values, size_t count, size_t array_index) const
{
	if (handle.handle != 0 && name_in != nullptr)
	{
		const texture &variable = *reinterpret_cast<const texture *>(handle.handle);
		const std::string_view name(name_in);

		if (const auto it = std::find_if(variable.annotations.cbegin(), variable.annotations.cend(),
				[&](const reshadefx::annotation &annotation) { return annotation.name == name; });
			it != variable.annotations.cend())
		{
			for (size_t i = 0; i < count; ++i)
				values[i] = variable.annotation_as_int(name, array_index + i);
			return true;
		}
	}

	for (size_t i = 0; i < count; ++i)
		values[i] = 0;
	return false;
}
bool reshade::runtime::get_annotation_uint_from_texture_variable(api::effect_texture_variable handle, const char *name_in, uint32_t *values, size_t count, size_t array_index) const
{
	if (handle.handle != 0 && name_in != nullptr)
	{
		const texture &variable = *reinterpret_cast<const texture *>(handle.handle);
		const std::string_view name(name_in);

		if (const auto it = std::find_if(variable.annotations.cbegin(), variable.annotations.cend(),
				[&](const reshadefx::annotation &annotation) { return annotation.name == name; });
			it != variable.annotations.cend())
		{
			for (size_t i = 0; i < count; ++i)
				values[i] = variable.annotation_as_uint(name, array_index + i);
			return true;
		}
	}

	for (size_t i = 0; i < count; ++i)
		values[i] = 0;
	return false;
}
bool reshade::runtime::get_annotation_string_from_texture_variable(api::effect_texture_variable handle, const char *name_in, char *value, size_t *size) const
{
	if (handle.handle != 0 && name_in != nullptr)
	{
		const texture &variable = *reinterpret_cast<const texture *>(handle.handle);
		const std::string_view name(name_in);

		if (const auto it = std::find_if(variable.annotations.cbegin(), variable.annotations.cend(),
				[&](const reshadefx::annotation &annotation) { return annotation.name == name; });
			it != variable.annotations.cend())
		{
			const std::string_view annotation = variable.annotation_as_string(name);

			if (size != nullptr)
			{
				if (value == nullptr)
				{
					*size = annotation.size() + 1;
				}
				else if (*size != 0)
				{
					*size = annotation.copy(value, *size - 1);
					value[*size] = '\0';
				}
			}

			return true;
		}
	}

	if (size != nullptr)
		*size = 0;
	return false;
}

void reshade::runtime::update_texture(api::effect_texture_variable handle, const uint32_t width, const uint32_t height, const void *pixels)
{
	const auto variable = reinterpret_cast<texture *>(handle.handle);
	if (variable == nullptr || variable->resource == 0)
		return;

	update_texture(*variable, width, height, 1, pixels);
}

void reshade::runtime::get_texture_binding(api::effect_texture_variable handle, api::resource_view *out_srv, api::resource_view *out_srv_srgb) const
{
	if (const auto variable = reinterpret_cast<const texture *>(handle.handle))
	{
		if (variable->semantic.empty())
		{
			if (out_srv != nullptr)
				*out_srv = variable->srv[0];
			if (out_srv_srgb != nullptr)
				*out_srv_srgb = variable->srv[1];
		}
		else if (const auto it = _texture_semantic_bindings.find(variable->semantic);
			it != _texture_semantic_bindings.end())
		{
			if (out_srv != nullptr)
				*out_srv = it->second.first;
			if (out_srv_srgb != nullptr)
				*out_srv_srgb = it->second.second;
		}
		return;
	}

	if (out_srv != nullptr)
		*out_srv = { 0 };
	if (out_srv_srgb != nullptr)
		*out_srv_srgb = { 0 };
}

void reshade::runtime::update_texture_bindings(const char *semantic, api::resource_view srv, api::resource_view srv_srgb)
{
	if (srv_srgb == 0)
		srv_srgb = srv;

	if (srv != 0)
	{
		_texture_semantic_bindings[semantic] = { srv, srv_srgb };
	}
	else
	{
		_texture_semantic_bindings.erase(semantic);

		// Overwrite with empty texture, since it is not valid to bind a zero handle
		srv = srv_srgb = _empty_srv;
	}

	// Update texture bindings
	size_t num_bindings = 0;
	for (const effect &effect_data : _effects)
	{
		if (!effect_data.compiled)
			continue;

		num_bindings += effect_data.permutations[0].texture_semantic_to_binding.size();
	}

	num_bindings *= _effect_permutations.size();

	std::vector<api::descriptor_table_update> descriptor_writes;
	descriptor_writes.reserve(num_bindings);
	std::vector<api::sampler_with_resource_view> sampler_descriptors(num_bindings);

	for (const effect &effect_data : _effects)
	{
		if (!effect_data.compiled)
			continue;

		for (size_t permutation_index = 0; permutation_index < _effect_permutations.size(); ++permutation_index)
		{
			if (permutation_index >= effect_data.permutations.size())
				break;

			for (const effect::binding &binding : effect_data.permutations[permutation_index].texture_semantic_to_binding)
			{
				if (binding.semantic != semantic)
					continue;

				api::descriptor_table_update &write = descriptor_writes.emplace_back();
				write.table = binding.table;
				write.binding = binding.index;
				write.count = 1;

				if (binding.sampler != 0)
				{
					write.type = api::descriptor_type::sampler_with_resource_view;
					write.descriptors = &sampler_descriptors[--num_bindings];

					sampler_descriptors[num_bindings].sampler = binding.sampler;
				}
				else
				{
					write.type = api::descriptor_type::shader_resource_view;
					write.descriptors = &sampler_descriptors[--num_bindings].view;
				}

				sampler_descriptors[num_bindings].view = binding.srgb ? srv_srgb : srv;
			}
		}
	}

	if (descriptor_writes.empty())
		return; // Avoid waiting on graphics queue when nothing changes

	// Make sure all previous frames have finished before updating descriptors (since they may be in use otherwise)
	if (_is_initialized && (_device->get_api() == api::device_api::d3d12 || _device->get_api() == api::device_api::vulkan))
		_graphics_queue->wait_idle();

	_device->update_descriptor_tables(static_cast<uint32_t>(descriptor_writes.size()), descriptor_writes.data());
}

void reshade::runtime::enumerate_techniques(const char *effect_name_in, void(*callback)(effect_runtime *runtime, api::effect_technique technique, void *user_data), void *user_data)
{
	if (is_loading())
		return;

	const std::filesystem::path effect_name = effect_name_in != nullptr ? std::filesystem::u8path(effect_name_in) : std::filesystem::path();

	for (size_t technique_index : _technique_sorting)
	{
		const technique &technique = _techniques[technique_index];

		if (effect_name_in != nullptr && _effects[technique.effect_index].source_file.filename() != effect_name)
			continue;

		callback(this, { reinterpret_cast<uintptr_t>(&technique) }, user_data);
	}
}

reshade::api::effect_technique reshade::runtime::find_technique(const char *effect_name_in, const char *technique_name_in)
{
	if (is_loading() || technique_name_in == nullptr)
		return { 0 };

	const std::filesystem::path effect_name = effect_name_in != nullptr ? std::filesystem::u8path(effect_name_in) : std::filesystem::path();
	const std::string_view technique_name(technique_name_in);

	for (const technique &technique : _techniques)
	{
		if (effect_name_in != nullptr && _effects[technique.effect_index].source_file.filename() != effect_name)
			continue;

		if (technique.name != technique_name)
			continue;

		return { reinterpret_cast<uintptr_t>(&technique) };
	}

	return { 0 };
}

void reshade::runtime::get_technique_name(api::effect_technique handle, char *value, size_t *size) const
{
	if (size == nullptr)
		return;

	if (const auto tech = reinterpret_cast<const technique *>(handle.handle))
	{
		if (value == nullptr)
		{
			*size = tech->name.size() + 1;
		}
		else if (*size != 0)
		{
			*size = tech->name.copy(value, *size - 1);
			value[*size] = '\0';
		}
	}
	else
	{
		*size = 0;
	}
}
void reshade::runtime::get_technique_effect_name(api::effect_technique handle, char *value, size_t *size) const
{
	if (size == nullptr)
		return;

	if (const auto tech = reinterpret_cast<const technique *>(handle.handle))
	{
		const std::string effect_name = _effects[tech->effect_index].source_file.filename().u8string();

		if (value == nullptr)
		{
			*size = effect_name.size() + 1;
		}
		else if (*size != 0)
		{
			*size = effect_name.copy(value, *size - 1);
			value[*size] = '\0';
		}
	}
	else
	{
		*size = 0;
	}
}

bool reshade::runtime::get_annotation_bool_from_technique(api::effect_technique handle, const char *name_in, bool *values, size_t count, size_t array_index) const
{
	if (handle.handle != 0 && name_in != nullptr)
	{
		const auto& tech = *reinterpret_cast<const technique *>(handle.handle);
		const std::string_view name(name_in);

		if (const auto it = std::find_if(tech.annotations.cbegin(), tech.annotations.cend(),
				[&](const reshadefx::annotation &annotation) { return annotation.name == name; });
			it != tech.annotations.cend())
		{
			for (size_t i = 0; i < count; ++i)
				values[i] = tech.annotation_as_int(name, array_index + i) != 0;
			return true;
		}
	}

	for (size_t i = 0; i < count; ++i)
		values[i] = false;
	return false;
}
bool reshade::runtime::get_annotation_float_from_technique(api::effect_technique handle, const char *name_in, float *values, size_t count, size_t array_index) const
{
	if (handle.handle != 0 && name_in != nullptr)
	{
		const auto &tech = *reinterpret_cast<const technique *>(handle.handle);
		const std::string_view name(name_in);

		if (const auto it = std::find_if(tech.annotations.cbegin(), tech.annotations.cend(),
				[&](const reshadefx::annotation &annotation) { return annotation.name == name; });
			it != tech.annotations.cend())
		{
			for (size_t i = 0; i < count; ++i)
				values[i] = tech.annotation_as_float(name, array_index + i);
			return true;
		}
	}

	for (size_t i = 0; i < count; ++i)
		values[i] = 0.0f;
	return false;
}
bool reshade::runtime::get_annotation_int_from_technique(api::effect_technique handle, const char *name_in, int32_t *values, size_t count, size_t array_index) const
{
	if (handle.handle != 0 && name_in != nullptr)
	{
		const auto &tech = *reinterpret_cast<const technique *>(handle.handle);
		const std::string_view name(name_in);

		if (const auto it = std::find_if(tech.annotations.cbegin(), tech.annotations.cend(),
				[&](const reshadefx::annotation &annotation) { return annotation.name == name; });
			it != tech.annotations.cend())
		{
			for (size_t i = 0; i < count; ++i)
				values[i] = tech.annotation_as_int(name, array_index + i);
			return true;
		}
	}

	for (size_t i = 0; i < count; ++i)
		values[i] = 0;
	return false;
}
bool reshade::runtime::get_annotation_uint_from_technique(api::effect_technique handle, const char *name_in, uint32_t *values, size_t count, size_t array_index) const
{
	if (handle.handle != 0 && name_in != nullptr)
	{
		const auto &tech = *reinterpret_cast<const technique *>(handle.handle);
		const std::string_view name(name_in);

		if (const auto it = std::find_if(tech.annotations.cbegin(), tech.annotations.cend(),
				[&](const reshadefx::annotation &annotation) { return annotation.name == name; });
			it != tech.annotations.cend())
		{
			for (size_t i = 0; i < count; ++i)
				values[i] = tech.annotation_as_uint(name, array_index + i);
			return true;
		}
	}

	for (size_t i = 0; i < count; ++i)
		values[i] = 0;
	return false;
}
bool reshade::runtime::get_annotation_string_from_technique(api::effect_technique handle, const char *name_in, char *value, size_t *size) const
{
	if (handle.handle != 0 && name_in != nullptr)
	{
		const auto &tech = *reinterpret_cast<const technique *>(handle.handle);
		const std::string_view name(name_in);

		if (const auto it = std::find_if(tech.annotations.cbegin(), tech.annotations.cend(),
				[&](const reshadefx::annotation &annotation) { return annotation.name == name; });
			it != tech.annotations.cend())
		{
			const std::string_view annotation = tech.annotation_as_string(name);

			if (size != nullptr)
			{
				if (value == nullptr)
				{
					*size = annotation.size() + 1;
				}
				else if (*size != 0)
				{
					*size = annotation.copy(value, *size - 1);
					value[*size] = '\0';
				}
			}

			return true;
		}
	}

	if (size != nullptr)
		*size = 0;
	return false;
}

bool reshade::runtime::get_technique_state(api::effect_technique handle) const
{
	const auto tech = reinterpret_cast<const technique *>(handle.handle);
	if (tech == nullptr)
		return false;

	return tech->enabled;
}
void reshade::runtime::set_technique_state(api::effect_technique handle, bool enabled)
{
	const auto tech = reinterpret_cast<technique *>(handle.handle);
	if (tech == nullptr)
		return;

#if RESHADE_ADDON
	const bool was_is_in_api_call = _is_in_api_call;
	_is_in_api_call = true;
#endif

	if (enabled)
		enable_technique(*tech);
	else
		disable_technique(*tech);

#if RESHADE_ADDON
	_is_in_api_call = was_is_in_api_call;
#endif
}

constexpr int EFFECT_SCOPE_FLAG = 0b001;
constexpr int PRESET_SCOPE_FLAG = 0b010;
constexpr int GLOBAL_SCOPE_FLAG = 0b100;

void reshade::runtime::set_preprocessor_definition(const char *name, const char *value)
{
	set_preprocessor_definition_for_effect(nullptr, name, value);
}
void reshade::runtime::set_preprocessor_definition_for_effect(const char *effect_name_in, const char *name, const char *value)
{
	if (name == nullptr)
		return;

	std::string effect_name;
	if (effect_name_in != nullptr)
		effect_name = effect_name_in;

	const int scope_mask =
		effect_name.find('.') != std::string::npos ? EFFECT_SCOPE_FLAG :
		effect_name.compare(0, 6, "PRESET") == 0 ? PRESET_SCOPE_FLAG :
		effect_name.compare(0, 6, "GLOBAL") == 0 ? GLOBAL_SCOPE_FLAG :
		EFFECT_SCOPE_FLAG | PRESET_SCOPE_FLAG | GLOBAL_SCOPE_FLAG;
	int scope_mask_updated = 0;

	if (value == nullptr || *value == '\0')
	{
		const auto remove_definition =
			[name = std::string_view(name), &scope_mask_updated](std::vector<std::pair<std::string, std::string>> &definitions, const int scope_flag) {
				if (const auto it = std::remove_if(definitions.begin(), definitions.end(),
						[name](const std::pair<std::string, std::string> &definition) { return definition.first == name; });
					it != definitions.end())
				{
					definitions.erase(it, definitions.end());
					scope_mask_updated |= scope_flag;
				}
			};

		if (const auto effect_definitions_it = _preset_preprocessor_definitions.find(effect_name);
			(scope_mask & EFFECT_SCOPE_FLAG) != 0 && effect_definitions_it != _preset_preprocessor_definitions.end() && !effect_definitions_it->second.empty())
		{
			remove_definition(effect_definitions_it->second, EFFECT_SCOPE_FLAG);
		}
		if (const auto preset_definitions_it = _preset_preprocessor_definitions.find({});
			(scope_mask & PRESET_SCOPE_FLAG) != 0 && preset_definitions_it != _preset_preprocessor_definitions.end() && !preset_definitions_it->second.empty())
		{
			remove_definition(preset_definitions_it->second, PRESET_SCOPE_FLAG);
		}
		if ((scope_mask & GLOBAL_SCOPE_FLAG) != 0)
		{
			remove_definition(_global_preprocessor_definitions, GLOBAL_SCOPE_FLAG);
		}
	}
	else
	{
		const auto update_definition =
			[name = std::string_view(name), value, &scope_mask_updated](std::vector<std::pair<std::string, std::string>> &definitions, const int scope_flag) {
				if (const auto it = std::find_if(definitions.begin(), definitions.end(),
						[name](const std::pair<std::string, std::string> &definition) { return definition.first == name; });
					it != definitions.end())
				{
					if (value != it->second)
					{
						it->second = value;
						scope_mask_updated = scope_flag;
					}
				}
				else
				{
					definitions.emplace_back(name, value);
					scope_mask_updated = scope_flag;
				}
			};

		if (const auto effect_definitions_it = _preset_preprocessor_definitions.find(effect_name);
			scope_mask == EFFECT_SCOPE_FLAG && effect_definitions_it != _preset_preprocessor_definitions.end())
		{
			update_definition(effect_definitions_it->second, EFFECT_SCOPE_FLAG);
		}
		else
		if (const auto preset_definitions_it = _preset_preprocessor_definitions.find({});
			scope_mask == PRESET_SCOPE_FLAG && preset_definitions_it != _preset_preprocessor_definitions.end())
		{
			update_definition(preset_definitions_it->second, PRESET_SCOPE_FLAG);
		}
		else
		if (scope_mask == GLOBAL_SCOPE_FLAG)
		{
			update_definition(_global_preprocessor_definitions, GLOBAL_SCOPE_FLAG);
		}
		else
		{
			std::vector<std::pair<std::string, std::string>> *definition_scope = nullptr;
			std::vector<std::pair<std::string, std::string>>::iterator definition_it;

			if (get_preprocessor_definition(effect_name, name, scope_mask, definition_scope, definition_it) &&
				definition_scope != &_global_preprocessor_definitions && (effect_name.empty() || definition_scope != &_preset_preprocessor_definitions[{}]))
				definition_it->second = value;
			else
				_preset_preprocessor_definitions[effect_name].emplace_back(name, value);

			scope_mask_updated = PRESET_SCOPE_FLAG;
		}
	}

	if (scope_mask_updated != 0)
	{
		if ((scope_mask_updated & EFFECT_SCOPE_FLAG) != 0)
		{
			ini_file &preset = ini_file::load_cache(_current_preset_path);

			if (const auto effect_definitions_it = _preset_preprocessor_definitions.find(effect_name);
				effect_definitions_it != _preset_preprocessor_definitions.end() && !effect_definitions_it->second.empty())
				preset.set(effect_name, "PreprocessorDefinitions", effect_definitions_it->second);
			else
				preset.remove_key(effect_name, "PreprocessorDefinitions");
		}
		if ((scope_mask_updated & PRESET_SCOPE_FLAG) != 0)
		{
			ini_file &preset = ini_file::load_cache(_current_preset_path);

			if (const auto preset_definitions_it = _preset_preprocessor_definitions.find({});
				preset_definitions_it != _preset_preprocessor_definitions.end() && !preset_definitions_it->second.empty())
				preset.set({}, "PreprocessorDefinitions", preset_definitions_it->second);
			else
				preset.remove_key({}, "PreprocessorDefinitions");
		}
		if ((scope_mask_updated & GLOBAL_SCOPE_FLAG) != 0)
		{
			ini_file::load_cache(_config_path).set("GENERAL", "PreprocessorDefinitions", _global_preprocessor_definitions);
		}

		reload_effect_next_frame((scope_mask_updated & (GLOBAL_SCOPE_FLAG | PRESET_SCOPE_FLAG)) != 0 ? nullptr : effect_name.c_str());
	}
}
bool reshade::runtime::get_preprocessor_definition(const char *name, char *value, size_t *size) const
{
	return get_preprocessor_definition_for_effect(nullptr, name, value, size);
}
bool reshade::runtime::get_preprocessor_definition_for_effect(const char *effect_name_in, const char *name, char *value, size_t *size) const
{
	std::string effect_name;
	if (effect_name_in != nullptr)
		effect_name = effect_name_in;

	const int scope_mask =
		effect_name.compare(0, 6, "GLOBAL") == 0 ? GLOBAL_SCOPE_FLAG :
		effect_name.compare(0, 6, "PRESET") == 0 ? PRESET_SCOPE_FLAG :
		effect_name.find('.') != std::string::npos ? EFFECT_SCOPE_FLAG :
		EFFECT_SCOPE_FLAG | PRESET_SCOPE_FLAG | GLOBAL_SCOPE_FLAG;

	if (name == nullptr) // Enumerate existing definitions when there is no name to query
	{
		size_t estimate_size = 0;
		std::vector<std::string> definitions;

		const auto emplace_to_list =
			[&estimate_size, &definitions](const std::pair<std::string, std::string> &adding) {
				if (std::all_of(definitions.cbegin(), definitions.cend(),
						[&adding](const std::string &added) { return added != adding.first; }))
				{
					estimate_size += adding.first.size() + 1; // '\0'
					definitions.emplace_back(adding.first);
				}
			};

		if (const auto effect_definitions_it = _preset_preprocessor_definitions.find(effect_name);
			(scope_mask & EFFECT_SCOPE_FLAG) != 0 && effect_definitions_it != _preset_preprocessor_definitions.end())
		{
			std::for_each(effect_definitions_it->second.begin(), effect_definitions_it->second.end(), emplace_to_list);
		}
		if (const auto preset_definitions_it = _preset_preprocessor_definitions.find({});
			(scope_mask & PRESET_SCOPE_FLAG) != 0 && preset_definitions_it != _preset_preprocessor_definitions.end())
		{
			std::for_each(preset_definitions_it->second.begin(), preset_definitions_it->second.end(), emplace_to_list);
		}
		if ((scope_mask & GLOBAL_SCOPE_FLAG) != 0)
		{
			std::for_each(_global_preprocessor_definitions.begin(), _global_preprocessor_definitions.end(), emplace_to_list);
		}

		std::string definitions_string;
		definitions_string.reserve(estimate_size);
		for (const std::string &definition : definitions)
			definitions_string += definition + '\0';

		if (size != nullptr)
		{
			if (value == nullptr)
			{
				*size = definitions_string.size() + 1;
			}
			else if (*size != 0)
			{
				*size = definitions_string.copy(value, *size - 1);
				value[*size] = '\0';
			}
		}

		return true; // Just enumerate the definitions so always returns true while the arguments are correct
	}
	else
	{
		std::vector<std::pair<std::string, std::string>> *definition_scope = nullptr;
		std::vector<std::pair<std::string, std::string>>::iterator definition_it;

		if (get_preprocessor_definition(effect_name, name, scope_mask, definition_scope, definition_it))
		{
			if (size != nullptr)
			{
				if (value == nullptr)
				{
					*size = definition_it->second.size() + 1;
				}
				else if (*size != 0)
				{
					*size = definition_it->second.copy(value, *size - 1);
					value[*size] = '\0';
				}
			}

			return true;
		}
	}

	if (size != nullptr)
		*size = 0;
	return false;
}

bool reshade::runtime::get_preprocessor_definition(const std::string &effect_name, const std::string &name, int scope_mask, std::vector<std::pair<std::string, std::string>> *&scope, std::vector<std::pair<std::string, std::string>>::iterator &value) const
{
	const auto find_preprocessor_definition =
		[&name, &scope, &value](std::vector<std::pair<std::string, std::string>> &definitions) {
			if (value = std::find_if(definitions.begin(), definitions.end(),
					[&name](const std::pair<std::string, std::string> &definition) { return definition.first == name; });
				value != definitions.end())
			{
				scope = &definitions;
				return true;
			}
			else
			{
				scope = nullptr;
				return false;
			}
		};

	if (const auto effect_definitions_it = _preset_preprocessor_definitions.find(effect_name);
		(scope_mask & EFFECT_SCOPE_FLAG) != 0 && effect_definitions_it != _preset_preprocessor_definitions.end())
	{
		if (find_preprocessor_definition(const_cast<std::vector<std::pair<std::string, std::string>> &>(effect_definitions_it->second)))
			return true;
	}
	if (const auto preset_definitions_it = _preset_preprocessor_definitions.find({});
		(scope_mask & PRESET_SCOPE_FLAG) != 0 && preset_definitions_it != _preset_preprocessor_definitions.end())
	{
		if (find_preprocessor_definition(const_cast<std::vector<std::pair<std::string, std::string>> &>(preset_definitions_it->second)))
			return true;
	}
	if ((scope_mask & GLOBAL_SCOPE_FLAG) != 0)
	{
		if (find_preprocessor_definition(const_cast<std::vector<std::pair<std::string, std::string>> &>(_global_preprocessor_definitions)))
			return true;
	}

	return false;
}

void reshade::runtime::render_technique(api::effect_technique handle, api::command_list *cmd_list, api::resource_view rtv, api::resource_view rtv_srgb)
{
	const auto tech = reinterpret_cast<technique *>(handle.handle);
	if (tech == nullptr)
		return;

	if (is_loading())
		return; // Skip reload enqueue below when effects are already loading, to avoid enqueing an effect for creation that is already in the process of being created

	if (rtv == 0)
		return;
	if (rtv_srgb == 0)
		rtv_srgb = rtv;

	const api::resource back_buffer_resource = _device->get_resource_from_view(rtv);

	size_t permutation_index = 0;
#if RESHADE_ADDON
	if (!_is_in_present_call &&
		// Special case for when add-on passed in the back buffer, which behaves as if this was called from within present, using the default permutation
		back_buffer_resource != get_current_back_buffer())
	{
		const api::resource_desc back_buffer_desc = _device->get_resource_desc(back_buffer_resource);
		if (back_buffer_desc.texture.samples > 1)
			return; // Multisampled render targets are not supported

		api::format color_format = back_buffer_desc.texture.format;
		if (api::format_to_typeless(color_format) == color_format)
			color_format = _device->get_resource_view_desc(rtv).format;

		// Ensure dimensions and format of the effect color resource matches that of the input back buffer resource (so that the copy to the effect color resource succeeds)
		// Never perform an immediate reload here, as the list of techniques must not be modified in case this was called from within 'enumerate_techniques'!
		permutation_index = add_effect_permutation(back_buffer_desc.texture.width, back_buffer_desc.texture.height, color_format, _effect_permutations[0].stencil_format, api::color_space::unknown);
		if (permutation_index == std::numeric_limits<size_t>::max())
			return;
	}

	const size_t effect_index = tech->effect_index;

	if (permutation_index >= tech->permutations.size() ||
		(!tech->permutations[permutation_index].created && _effects[effect_index].permutations[permutation_index].assembly.empty()))
	{
		if (std::find(_reload_required_effects.begin(), _reload_required_effects.end(), std::make_pair(effect_index, permutation_index)) == _reload_required_effects.end())
			_reload_required_effects.emplace_back(effect_index, permutation_index);
		return;
	}

	// Queue effect file for initialization if it was not fully loaded yet
	if (!tech->permutations[permutation_index].created)
	{
		if (std::find(_reload_create_queue.cbegin(), _reload_create_queue.cend(), std::make_pair(effect_index, permutation_index)) == _reload_create_queue.cend())
			_reload_create_queue.emplace_back(effect_index, permutation_index);
		return;
	}

	if (!_is_in_present_call)
		capture_state(cmd_list, _app_state);

	invoke_addon_event<addon_event::reshade_begin_effects>(this, cmd_list, rtv, rtv_srgb);

	const bool was_is_in_api_call = _is_in_api_call;
	_is_in_api_call = true;
#endif

	render_technique(*tech, cmd_list, back_buffer_resource, rtv, rtv_srgb, permutation_index);

#if RESHADE_ADDON
	_is_in_api_call = was_is_in_api_call;

	invoke_addon_event<addon_event::reshade_finish_effects>(this, cmd_list, rtv, rtv_srgb);

	if (!_is_in_present_call)
		apply_state(cmd_list, _app_state);
#endif
}

bool reshade::runtime::get_effects_state() const
{
	return _effects_enabled;
}
void reshade::runtime::set_effects_state(bool enabled)
{
	_effects_enabled = enabled;
}

void reshade::runtime::save_current_preset() const
{
	save_current_preset(ini_file::load_cache(_current_preset_path));
}
void reshade::runtime::export_current_preset(const char *path_in) const
{
	if (path_in == nullptr)
		return;

	std::filesystem::path preset_path = std::filesystem::u8path(path_in);

	std::error_code ec;
	resolve_path(preset_path, ec);

	if (ini_file *const cached_preset = ini_file::find_cache(preset_path))
	{
		save_current_preset(*cached_preset);
		return;
	}

	ini_file preset(preset_path);
	save_current_preset(preset);
	preset.save();
}

void reshade::runtime::get_current_preset_path(char *path, size_t *size) const
{
	if (size == nullptr)
		return;

	const std::string path_string = _current_preset_path.u8string();

	if (path == nullptr)
	{
		*size = path_string.size() + 1;
	}
	else if (*size != 0)
	{
		*size = path_string.copy(path, *size - 1);
		path[*size] = '\0';
	}
}
void reshade::runtime::set_current_preset_path(const char *path_in)
{
	if (path_in == nullptr)
		return;

	std::filesystem::path preset_path = std::filesystem::u8path(path_in);

	// Only change preset when this is a valid preset path
	std::error_code ec;
	if (resolve_preset_path(preset_path, ec))
	{
		if (is_loading())
		{
			_current_preset_path = std::move(preset_path);
		}
		else
		{
			// Stop any preset transition that may still be happening
			_is_in_preset_transition = false;

			// Reload preset even if it is the same as before
			if (preset_path != _current_preset_path)
			{
				// First save current preset, before switching to a new one
				save_current_preset();

				_current_preset_path = std::move(preset_path);

				save_config();
			}

			load_current_preset();
		}
	}
}

void reshade::runtime::reorder_techniques(size_t count, const api::effect_technique *techniques)
{
	if (count > _techniques.size())
		return;

	std::vector<size_t> technique_indices(_techniques.size());
	for (size_t i = 0; i < count; ++i)
	{
		const auto tech = reinterpret_cast<technique *>(techniques[i].handle);
		if (tech == nullptr)
			return;

		technique_indices[i] = tech - _techniques.data();
	}
	for (size_t i = count, k = 0; i < technique_indices.size(); ++i, ++k)
	{
		for (auto beg = technique_indices.cbegin(), end = technique_indices.cbegin() + i; std::find(beg, end, _technique_sorting[k]) != end; ++k)
			continue;

		technique_indices[i] = _technique_sorting[k];
	}

#if RESHADE_ADDON
	const bool was_is_in_api_call = _is_in_api_call;
	_is_in_api_call = true;
#endif

	reorder_techniques(std::move(technique_indices));

#if RESHADE_ADDON
	_is_in_api_call = was_is_in_api_call;
#endif
}

#if RESHADE_GUI == 0
bool reshade::runtime::open_overlay(bool /*open*/, api::input_source /*source*/)
{
	return false;
}
#endif

void reshade::runtime::set_color_space(api::color_space color_space)
{
	if (color_space == _back_buffer_color_space || color_space == api::color_space::unknown || !_is_initialized)
		return;

	_back_buffer_color_space = color_space;
	_effect_permutations[0].color_space = color_space;

	if (_frame_count != 0) // Do not need to reload effects here if they are already getting reloaded on next present anyway
		reload_effects();
}

void reshade::runtime::reload_effect_next_frame(const char *effect_name)
{
	if (effect_name == nullptr)
	{
		_reload_required_effects = { std::make_pair(std::numeric_limits<size_t>::max(), static_cast<size_t>(0u)) };
		return;
	}

	if (auto it = std::find_if(_effects.cbegin(), _effects.cend(),
			[effect_name = std::filesystem::u8path(effect_name)](const effect &effect) {
				return effect.source_file.filename() == effect_name;
			});
		it != _effects.cend())
	{
		const size_t effect_index = std::distance(_effects.cbegin(), it);

		if (std::find(_reload_required_effects.cbegin(), _reload_required_effects.cend(), std::make_pair(std::numeric_limits<size_t>::max(), static_cast<size_t>(0u))) == _reload_required_effects.cend() &&
			std::find(_reload_required_effects.cbegin(), _reload_required_effects.cend(), std::make_pair(effect_index, static_cast<size_t>(0u))) == _reload_required_effects.cend())
			_reload_required_effects.emplace_back(effect_index, static_cast<size_t>(0u));
	}
}
