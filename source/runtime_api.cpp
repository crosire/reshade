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

void reshade::runtime::enumerate_uniform_variables([[maybe_unused]] const char *effect_name, [[maybe_unused]] void(*callback)(effect_runtime *runtime, api::effect_uniform_variable variable, void *user_data), [[maybe_unused]] void *user_data)
{
#if RESHADE_FX
	if (is_loading())
		return;

	for (const effect &effect : _effects)
	{
		if (effect_name != nullptr && effect.source_file.filename() != effect_name)
			continue;

		for (const uniform &variable : effect.uniforms)
			callback(this, { reinterpret_cast<uintptr_t>(&variable) }, user_data);

		if (effect_name != nullptr)
			break;
	}
#endif
}

reshade::api::effect_uniform_variable reshade::runtime::find_uniform_variable([[maybe_unused]] const char *effect_name, [[maybe_unused]] const char *variable_name) const
{
#if RESHADE_FX
	if (is_loading())
		return { 0 };

	for (const effect &effect : _effects)
	{
		if (effect_name != nullptr && effect.source_file.filename() != effect_name)
			continue;

		for (const uniform &variable : effect.uniforms)
		{
			if (variable.name == variable_name)
				return { reinterpret_cast<uintptr_t>(&variable) };
		}

		if (effect_name != nullptr)
			break;
	}
#endif

	return { 0 };
}

void reshade::runtime::get_uniform_variable_type([[maybe_unused]] api::effect_uniform_variable handle, api::format *out_base_type, uint32_t *out_rows, uint32_t *out_columns, uint32_t *out_array_length) const
{
#if RESHADE_FX
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
#endif

	if (out_base_type != nullptr)
		*out_base_type = api::format::unknown;
	if (out_rows != nullptr)
		*out_rows = 0;
	if (out_columns != nullptr)
		*out_columns = 0;
	if (out_array_length != nullptr)
		*out_array_length = 0;
}

void reshade::runtime::get_uniform_variable_name([[maybe_unused]] api::effect_uniform_variable handle, [[maybe_unused]] char *value, size_t *size) const
{
	if (size == nullptr)
		return;

#if RESHADE_FX
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
#endif
		*size = 0;
}
void reshade::runtime::get_uniform_variable_effect_name([[maybe_unused]] api::effect_uniform_variable handle, [[maybe_unused]] char *value, size_t *size) const
{
	if (size == nullptr)
		return;

#if RESHADE_FX
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
#endif
		*size = 0;
}

bool reshade::runtime::get_annotation_bool_from_uniform_variable([[maybe_unused]] api::effect_uniform_variable handle, [[maybe_unused]] const char *name, bool *values, size_t count, [[maybe_unused]] size_t array_index) const
{
#if RESHADE_FX
	if (const auto variable = reinterpret_cast<const uniform *>(handle.handle))
	{
		const std::string_view ann_name(name);

		if (const auto it = std::find_if(variable->annotations.cbegin(), variable->annotations.cend(),
				[ann_name](const reshadefx::annotation &annotation) { return annotation.name == ann_name; });
			it != variable->annotations.cend())
		{
			for (size_t i = 0; i < count; ++i)
				values[i] = variable->annotation_as_int(ann_name, i + array_index) != 0;
			return true;
		}
	}
#endif

	for (size_t i = 0; i < count; ++i)
		values[i] = false;
	return false;
}
bool reshade::runtime::get_annotation_float_from_uniform_variable([[maybe_unused]] api::effect_uniform_variable handle, [[maybe_unused]] const char *name, float *values, size_t count, [[maybe_unused]] size_t array_index) const
{
#if RESHADE_FX
	if (const auto variable = reinterpret_cast<const uniform *>(handle.handle))
	{
		const std::string_view ann_name(name);

		if (const auto it = std::find_if(variable->annotations.cbegin(), variable->annotations.cend(),
				[ann_name](const reshadefx::annotation &annotation) { return annotation.name == ann_name; });
			it != variable->annotations.cend())
		{
			for (size_t i = 0; i < count; ++i)
				values[i] = variable->annotation_as_float(ann_name, i + array_index);
			return true;
		}
	}
#endif

	for (size_t i = 0; i < count; ++i)
		values[i] = 0.0f;
	return false;
}
bool reshade::runtime::get_annotation_int_from_uniform_variable([[maybe_unused]] api::effect_uniform_variable handle, [[maybe_unused]] const char *name, int32_t *values, size_t count, [[maybe_unused]] size_t array_index) const
{
#if RESHADE_FX
	if (const auto variable = reinterpret_cast<const uniform *>(handle.handle))
	{
		const std::string_view ann_name(name);

		if (const auto it = std::find_if(variable->annotations.cbegin(), variable->annotations.cend(),
				[ann_name](const reshadefx::annotation &annotation) { return annotation.name == ann_name; });
			it != variable->annotations.cend())
		{
			for (size_t i = 0; i < count; ++i)
				values[i] = variable->annotation_as_int(ann_name, array_index + i);
			return true;
		}
	}
#endif

	for (size_t i = 0; i < count; ++i)
		values[i] = 0;
	return false;
}
bool reshade::runtime::get_annotation_uint_from_uniform_variable([[maybe_unused]] api::effect_uniform_variable handle, [[maybe_unused]] const char *name, uint32_t *values, size_t count, [[maybe_unused]] size_t array_index) const
{
#if RESHADE_FX
	if (const auto variable = reinterpret_cast<const uniform *>(handle.handle))
	{
		const std::string_view ann_name(name);

		if (const auto it = std::find_if(variable->annotations.cbegin(), variable->annotations.cend(),
				[ann_name](const reshadefx::annotation &annotation) { return annotation.name == ann_name; });
			it != variable->annotations.cend())
		{
			for (size_t i = 0; i < count; ++i)
				values[i] = variable->annotation_as_uint(ann_name, array_index + i);
			return true;
		}
	}
#endif

	for (size_t i = 0; i < count; ++i)
		values[i] = 0;
	return false;
}
bool reshade::runtime::get_annotation_string_from_uniform_variable([[maybe_unused]] api::effect_uniform_variable handle, [[maybe_unused]] const char *name, [[maybe_unused]] char *value, size_t *size) const
{
#if RESHADE_FX
	const auto variable = reinterpret_cast<const uniform *>(handle.handle);
	if (variable != nullptr)
	{
		const std::string_view ann_name(name);

		if (const auto it = std::find_if(variable->annotations.cbegin(), variable->annotations.cend(),
				[ann_name](const reshadefx::annotation &annotation) { return annotation.name == ann_name; });
			it != variable->annotations.cend())
		{
			const std::string_view annotation = variable->annotation_as_string(ann_name);

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
#endif

	if (size != nullptr)
		*size = 0;
	return false;
}

void reshade::runtime::reset_uniform_value([[maybe_unused]] api::effect_uniform_variable handle)
{
#if RESHADE_FX
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
#endif
}

void reshade::runtime::get_uniform_value_bool([[maybe_unused]] api::effect_uniform_variable handle, bool *values, size_t count, [[maybe_unused]] size_t array_index) const
{
#if RESHADE_FX
	if (const auto variable = reinterpret_cast<const uniform *>(handle.handle))
	{
		get_uniform_value(*variable, values, count, array_index);
		return;
	}
#endif

	for (size_t i = 0; i < count; ++i)
		values[i] = false;
}
void reshade::runtime::get_uniform_value_float([[maybe_unused]] api::effect_uniform_variable handle, float *values, size_t count, [[maybe_unused]] size_t array_index) const
{
#if RESHADE_FX
	if (const auto variable = reinterpret_cast<const uniform *>(handle.handle))
	{
		get_uniform_value(*variable, values, count, array_index);
		return;
	}
#endif

	for (size_t i = 0; i < count; ++i)
		values[i] = 0.0f;
}
void reshade::runtime::get_uniform_value_int([[maybe_unused]] api::effect_uniform_variable handle, int32_t *values, size_t count, [[maybe_unused]] size_t array_index) const
{
#if RESHADE_FX
	if (const auto variable = reinterpret_cast<const uniform *>(handle.handle))
	{
		get_uniform_value(*variable, values, count, array_index);
		return;
	}
#endif

	for (size_t i = 0; i < count; ++i)
		values[i] = 0;
}
void reshade::runtime::get_uniform_value_uint([[maybe_unused]] api::effect_uniform_variable handle, uint32_t *values, size_t count, [[maybe_unused]] size_t array_index) const
{
#if RESHADE_FX
	if (const auto variable = reinterpret_cast<const uniform *>(handle.handle))
	{
		get_uniform_value(*variable, values, count, array_index);
		return;
	}
#endif

	for (size_t i = 0; i < count; ++i)
		values[i] = 0;
}

void reshade::runtime::set_uniform_value_bool([[maybe_unused]] api::effect_uniform_variable handle, [[maybe_unused]] const bool *values, [[maybe_unused]] size_t count, [[maybe_unused]] size_t array_index)
{
#if RESHADE_FX
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
#endif
}
void reshade::runtime::set_uniform_value_float([[maybe_unused]] api::effect_uniform_variable handle, [[maybe_unused]] const float *values, [[maybe_unused]] size_t count, [[maybe_unused]] size_t array_index)
{
#if RESHADE_FX
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
#endif
}
void reshade::runtime::set_uniform_value_int([[maybe_unused]] api::effect_uniform_variable handle, [[maybe_unused]] const int32_t *values, [[maybe_unused]] size_t count, [[maybe_unused]] size_t array_index)
{
#if RESHADE_FX
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
#endif
}
void reshade::runtime::set_uniform_value_uint([[maybe_unused]] api::effect_uniform_variable handle, [[maybe_unused]] const uint32_t *values, [[maybe_unused]] size_t count, [[maybe_unused]] size_t array_index)
{
#if RESHADE_FX
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
#endif
}

void reshade::runtime::enumerate_texture_variables([[maybe_unused]] const char *effect_name, [[maybe_unused]] void(*callback)(effect_runtime *runtime, api::effect_texture_variable variable, void *user_data), [[maybe_unused]] void *user_data)
{
#if RESHADE_FX
	if (is_loading())
		return;

	for (const texture &variable : _textures)
	{
		if (effect_name != nullptr &&
			std::find_if(variable.shared.cbegin(), variable.shared.cend(),
				[this, effect_name = std::string_view(effect_name)](size_t effect_index) {
					return _effects[effect_index].source_file.filename() == effect_name;
				}) == variable.shared.cend())
			continue;

		callback(this, { reinterpret_cast<uintptr_t>(&variable) }, user_data);
	}
#endif
}

reshade::api::effect_texture_variable reshade::runtime::find_texture_variable([[maybe_unused]] const char *effect_name, [[maybe_unused]] const char *variable_name) const
{
#if RESHADE_FX
	if (is_loading())
		return { 0 };

	const std::string_view name(variable_name);

	for (const texture &variable : _textures)
	{
		if (effect_name != nullptr &&
			std::find_if(variable.shared.cbegin(), variable.shared.cend(),
				[this, effect_name = std::string_view(effect_name)](size_t effect_index) {
					return _effects[effect_index].source_file.filename() == effect_name;
				}) == variable.shared.cend())
			continue;

		if (variable.name == name || variable.unique_name == name)
			return { reinterpret_cast<uintptr_t>(&variable) };
	}
#endif

	return { 0 };
}

void reshade::runtime::get_texture_variable_name([[maybe_unused]] api::effect_texture_variable handle, [[maybe_unused]] char *value, size_t *size) const
{
	if (size == nullptr)
		return;

#if RESHADE_FX
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
#endif
		*size = 0;
}
void reshade::runtime::get_texture_variable_effect_name([[maybe_unused]] api::effect_texture_variable handle, [[maybe_unused]] char *value, size_t *size) const
{
	if (size == nullptr)
		return;

#if RESHADE_FX
	if (const auto variable = reinterpret_cast<const texture *>(handle.handle))
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
#endif
		*size = 0;
}

bool reshade::runtime::get_annotation_bool_from_texture_variable([[maybe_unused]] api::effect_texture_variable handle, [[maybe_unused]] const char *name, bool *values, size_t count, [[maybe_unused]] size_t array_index) const
{
#if RESHADE_FX
	if (const auto variable = reinterpret_cast<const texture *>(handle.handle))
	{
		const std::string_view ann_name(name);

		if (const auto it = std::find_if(variable->annotations.cbegin(), variable->annotations.cend(),
				[ann_name](const reshadefx::annotation &annotation) { return annotation.name == ann_name; });
			it != variable->annotations.cend())
		{
			for (size_t i = 0; i < count; ++i)
				values[i] = variable->annotation_as_int(ann_name, array_index + i) != 0;
			return true;
		}
	}
#endif

	for (size_t i = 0; i < count; ++i)
		values[i] = false;
	return false;
}
bool reshade::runtime::get_annotation_float_from_texture_variable([[maybe_unused]] api::effect_texture_variable handle, [[maybe_unused]] const char *name, float *values, size_t count, [[maybe_unused]] size_t array_index) const
{
#if RESHADE_FX
	if (const auto variable = reinterpret_cast<const texture *>(handle.handle))
	{
		const std::string_view ann_name(name);

		if (const auto it = std::find_if(variable->annotations.cbegin(), variable->annotations.cend(),
				[ann_name](const reshadefx::annotation &annotation) { return annotation.name == ann_name; });
			it != variable->annotations.cend())
		{
			for (size_t i = 0; i < count; ++i)
				values[i] = variable->annotation_as_float(ann_name, array_index + i);
			return true;
		}
	}
#endif

	for (size_t i = 0; i < count; ++i)
		values[i] = 0.0f;
	return false;
}
bool reshade::runtime::get_annotation_int_from_texture_variable([[maybe_unused]] api::effect_texture_variable handle, [[maybe_unused]] const char *name, int32_t *values, size_t count, [[maybe_unused]] size_t array_index) const
{
#if RESHADE_FX
	if (const auto variable = reinterpret_cast<const texture *>(handle.handle))
	{
		const std::string_view ann_name(name);

		if (const auto it = std::find_if(variable->annotations.cbegin(), variable->annotations.cend(),
				[ann_name](const reshadefx::annotation &annotation) { return annotation.name == ann_name; });
			it != variable->annotations.cend())
		{
			for (size_t i = 0; i < count; ++i)
				values[i] = variable->annotation_as_int(ann_name, array_index + i);
			return true;
		}
	}
#endif

	for (size_t i = 0; i < count; ++i)
		values[i] = 0;
	return false;
}
bool reshade::runtime::get_annotation_uint_from_texture_variable([[maybe_unused]] api::effect_texture_variable handle, [[maybe_unused]] const char *name, uint32_t *values, size_t count, [[maybe_unused]] size_t array_index) const
{
#if RESHADE_FX
	if (const auto variable = reinterpret_cast<const texture *>(handle.handle))
	{
		const std::string_view ann_name(name);

		if (const auto it = std::find_if(variable->annotations.cbegin(), variable->annotations.cend(),
				[ann_name](const reshadefx::annotation &annotation) { return annotation.name == ann_name; });
			it != variable->annotations.cend())
		{
			for (size_t i = 0; i < count; ++i)
				values[i] = variable->annotation_as_uint(ann_name, array_index + i);
			return true;
		}
	}
#endif

	for (size_t i = 0; i < count; ++i)
		values[i] = 0;
	return false;
}
bool reshade::runtime::get_annotation_string_from_texture_variable([[maybe_unused]] api::effect_texture_variable handle, [[maybe_unused]] const char *name, [[maybe_unused]] char *value, size_t *size) const
{
#if RESHADE_FX
	const auto variable = reinterpret_cast<const texture *>(handle.handle);
	if (variable != nullptr)
	{
		const std::string_view ann_name(name);

		if (const auto it = std::find_if(variable->annotations.cbegin(), variable->annotations.cend(),
				[ann_name](const reshadefx::annotation &annotation) { return annotation.name == ann_name; });
			it != variable->annotations.cend())
		{
			const std::string_view annotation = variable->annotation_as_string(ann_name);

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
#endif

	if (size != nullptr)
		*size = 0;
	return false;
}

void reshade::runtime::update_texture([[maybe_unused]] api::effect_texture_variable handle, [[maybe_unused]] const uint32_t width, [[maybe_unused]] const uint32_t height, [[maybe_unused]] const void *pixels)
{
#if RESHADE_FX
	const auto variable = reinterpret_cast<texture *>(handle.handle);
	if (variable == nullptr || variable->resource == 0)
		return;

	update_texture(*variable, width, height, 1, pixels);
#endif
}

void reshade::runtime::get_texture_binding([[maybe_unused]] api::effect_texture_variable handle, api::resource_view *out_srv, api::resource_view *out_srv_srgb) const
{
#if RESHADE_FX
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
#endif

	if (out_srv != nullptr)
		*out_srv = { 0 };
	if (out_srv_srgb != nullptr)
		*out_srv_srgb = { 0 };
}

void reshade::runtime::update_texture_bindings([[maybe_unused]] const char *semantic, [[maybe_unused]] api::resource_view srv, [[maybe_unused]] api::resource_view srv_srgb)
{
#if RESHADE_FX
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
		num_bindings += effect_data.texture_semantic_to_binding.size();

	std::vector<api::descriptor_table_update> descriptor_writes;
	descriptor_writes.reserve(num_bindings);
	std::vector<api::sampler_with_resource_view> sampler_descriptors(num_bindings);

	for (const effect &effect_data : _effects)
	{
		for (const effect::binding_data &binding : effect_data.texture_semantic_to_binding)
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

	if (descriptor_writes.empty())
		return; // Avoid waiting on graphics queue when nothing changes

	// Make sure all previous frames have finished before updating descriptors (since they may be in use otherwise)
	if (_is_initialized && (_device->get_api() == api::device_api::d3d12 || _device->get_api() == api::device_api::vulkan))
		_graphics_queue->wait_idle();

	_device->update_descriptor_tables(static_cast<uint32_t>(descriptor_writes.size()), descriptor_writes.data());
#endif
}

void reshade::runtime::enumerate_techniques([[maybe_unused]] const char *effect_name, [[maybe_unused]] void(*callback)(effect_runtime *runtime, api::effect_technique technique, void *user_data), [[maybe_unused]] void *user_data)
{
#if RESHADE_FX
	if (is_loading())
		return;

	for (size_t technique_index : _technique_sorting)
	{
		const technique &technique = _techniques[technique_index];

		if (effect_name != nullptr && _effects[technique.effect_index].source_file.filename() != effect_name)
			continue;

		callback(this, { reinterpret_cast<uintptr_t>(&technique) }, user_data);
	}
#endif
}

reshade::api::effect_technique reshade::runtime::find_technique([[maybe_unused]] const char *effect_name, [[maybe_unused]] const char *technique_name)
{
#if RESHADE_FX
	if (is_loading())
		return { 0 };

	for (const technique &technique : _techniques)
	{
		if (effect_name != nullptr && _effects[technique.effect_index].source_file.filename() != effect_name)
			continue;

		if (technique.name == technique_name)
			return { reinterpret_cast<uintptr_t>(&technique) };
	}
#endif

	return { 0 };
}

void reshade::runtime::get_technique_name([[maybe_unused]] api::effect_technique handle, [[maybe_unused]] char *value, size_t *size) const
{
	if (size == nullptr)
		return;

#if RESHADE_FX
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
#endif
		*size = 0;
}
void reshade::runtime::get_technique_effect_name([[maybe_unused]] api::effect_technique handle, [[maybe_unused]] char *value, size_t *size) const
{
	if (size == nullptr)
		return;

#if RESHADE_FX
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
#endif
		*size = 0;
}

bool reshade::runtime::get_annotation_bool_from_technique([[maybe_unused]] api::effect_technique handle, [[maybe_unused]] const char *name, bool *values, size_t count, [[maybe_unused]] size_t array_index) const
{
#if RESHADE_FX
	if (const auto tech = reinterpret_cast<const technique *>(handle.handle))
	{
		const std::string_view ann_name(name);

		if (const auto it = std::find_if(tech->annotations.cbegin(), tech->annotations.cend(),
				[ann_name](const reshadefx::annotation &annotation) { return annotation.name == ann_name; });
			it != tech->annotations.cend())
		{
			for (size_t i = 0; i < count; ++i)
				values[i] = tech->annotation_as_int(ann_name, array_index + i) != 0;
			return true;
		}
	}
#endif

	for (size_t i = 0; i < count; ++i)
		values[i] = false;
	return false;
}
bool reshade::runtime::get_annotation_float_from_technique([[maybe_unused]] api::effect_technique handle, [[maybe_unused]] const char *name, float *values, size_t count, [[maybe_unused]] size_t array_index) const
{
#if RESHADE_FX
	if (const auto tech = reinterpret_cast<const technique *>(handle.handle))
	{
		const std::string_view ann_name(name);

		if (const auto it = std::find_if(tech->annotations.cbegin(), tech->annotations.cend(),
				[ann_name](const reshadefx::annotation &annotation) { return annotation.name == ann_name; });
			it != tech->annotations.cend())
		{
			for (size_t i = 0; i < count; ++i)
				values[i] = tech->annotation_as_float(ann_name, array_index + i);
			return true;
		}
	}
#endif

	for (size_t i = 0; i < count; ++i)
		values[i] = 0.0f;
	return false;
}
bool reshade::runtime::get_annotation_int_from_technique([[maybe_unused]] api::effect_technique handle, [[maybe_unused]] const char *name, int32_t *values, size_t count, [[maybe_unused]] size_t array_index) const
{
#if RESHADE_FX
	if (const auto tech = reinterpret_cast<const technique *>(handle.handle))
	{
		const std::string_view ann_name(name);

		if (const auto it = std::find_if(tech->annotations.cbegin(), tech->annotations.cend(),
				[ann_name](const reshadefx::annotation &annotation) { return annotation.name == ann_name; });
			it != tech->annotations.cend())
		{
			for (size_t i = 0; i < count; ++i)
				values[i] = tech->annotation_as_int(ann_name, array_index + i);
			return true;
		}
	}
#endif

	for (size_t i = 0; i < count; ++i)
		values[i] = 0;
	return false;
}
bool reshade::runtime::get_annotation_uint_from_technique([[maybe_unused]] api::effect_technique handle, [[maybe_unused]] const char *name, uint32_t *values, size_t count, [[maybe_unused]] size_t array_index) const
{
#if RESHADE_FX
	if (const auto tech = reinterpret_cast<const technique *>(handle.handle))
	{
		const std::string_view ann_name(name);

		if (const auto it = std::find_if(tech->annotations.cbegin(), tech->annotations.cend(),
				[ann_name](const reshadefx::annotation &annotation) { return annotation.name == ann_name; });
			it != tech->annotations.cend())
		{
			for (size_t i = 0; i < count; ++i)
				values[i] = tech->annotation_as_uint(ann_name, array_index + i);
			return true;
		}
	}
#endif

	for (size_t i = 0; i < count; ++i)
		values[i] = 0;
	return false;
}
bool reshade::runtime::get_annotation_string_from_technique([[maybe_unused]] api::effect_technique handle, [[maybe_unused]] const char *name, [[maybe_unused]] char *value, size_t *size) const
{
#if RESHADE_FX
	const auto tech = reinterpret_cast<const technique *>(handle.handle);
	if (tech != nullptr)
	{
		const std::string_view ann_name(name);

		if (const auto it = std::find_if(tech->annotations.cbegin(), tech->annotations.cend(),
				[ann_name](const reshadefx::annotation &annotation) { return annotation.name == ann_name; });
			it != tech->annotations.cend())
		{
			const std::string_view annotation = tech->annotation_as_string(ann_name);

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
#endif

	if (size != nullptr)
		*size = 0;
	return false;
}

bool reshade::runtime::get_technique_state([[maybe_unused]] api::effect_technique handle) const
{
#if RESHADE_FX
	const auto tech = reinterpret_cast<const technique *>(handle.handle);
	if (tech == nullptr)
		return false;

	return tech->enabled;
#else
	return false;
#endif
}
void reshade::runtime::set_technique_state([[maybe_unused]] api::effect_technique handle, [[maybe_unused]] bool enabled)
{
#if RESHADE_FX
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
#endif
}

constexpr int EFFECT_SCOPE_FLAG = 0b001;
constexpr int PRESET_SCOPE_FLAG = 0b010;
constexpr int GLOBAL_SCOPE_FLAG = 0b100;

void reshade::runtime::set_preprocessor_definition(const char *name, const char *value)
{
	set_preprocessor_definition_for_effect(nullptr, name, value);
}
void reshade::runtime::set_preprocessor_definition_for_effect([[maybe_unused]] const char *effect_name, [[maybe_unused]] const char *name, [[maybe_unused]] const char *value)
{
#if RESHADE_FX
	if (name == nullptr)
		return;

	std::string effect_name_string;
	if (effect_name != nullptr)
		effect_name_string = effect_name;

	const int scope_mask =
		effect_name_string.find('.') != std::string::npos ? EFFECT_SCOPE_FLAG :
		effect_name_string.compare(0, 6, "PRESET") == 0 ? PRESET_SCOPE_FLAG :
		effect_name_string.compare(0, 6, "GLOBAL") == 0 ? GLOBAL_SCOPE_FLAG :
		EFFECT_SCOPE_FLAG | PRESET_SCOPE_FLAG | GLOBAL_SCOPE_FLAG;
	int scope_mask_updated = 0;

	if (value == nullptr || *value == '\0')
	{
		if ((scope_mask & EFFECT_SCOPE_FLAG) != 0)
		{
			if (const auto preset_it = _preset_preprocessor_definitions.find(effect_name_string);
				preset_it != _preset_preprocessor_definitions.end() && !preset_it->second.empty())
			{
				if (const auto it = std::remove_if(preset_it->second.begin(), preset_it->second.end(),
						[name = std::string_view(name)](const std::pair<std::string, std::string> &definition) { return definition.first == name; });
					it != preset_it->second.end())
				{
					preset_it->second.erase(it, preset_it->second.end());
					scope_mask_updated |= EFFECT_SCOPE_FLAG;
				}
			}
		}
		if ((scope_mask & PRESET_SCOPE_FLAG) != 0)
		{
			if (const auto preset_it = _preset_preprocessor_definitions.find({});
				preset_it != _preset_preprocessor_definitions.end() && !preset_it->second.empty())
			{
				if (const auto it = std::remove_if(preset_it->second.begin(), preset_it->second.end(),
						[name = std::string_view(name)](const std::pair<std::string, std::string> &definition) { return definition.first == name; });
					it != preset_it->second.end())
				{
					preset_it->second.erase(it, preset_it->second.end());
					scope_mask_updated |= PRESET_SCOPE_FLAG;
				}
			}
		}
		if ((scope_mask & GLOBAL_SCOPE_FLAG) != 0)
		{
			if (const auto it = std::remove_if(_global_preprocessor_definitions.begin(), _global_preprocessor_definitions.end(),
					[name = std::string_view(name)](const std::pair<std::string, std::string> &definition) { return definition.first == name; });
				it != _global_preprocessor_definitions.end())
			{
				_global_preprocessor_definitions.erase(it, _global_preprocessor_definitions.end());
				scope_mask_updated |= GLOBAL_SCOPE_FLAG;
			}
		}
	}
	else
	{
		if (scope_mask == EFFECT_SCOPE_FLAG)
		{
			if (const auto preset_it = _preset_preprocessor_definitions.find(effect_name_string);
				preset_it != _preset_preprocessor_definitions.end())
			{
				if (auto it = std::find_if(preset_it->second.begin(), preset_it->second.end(),
						[name = std::string_view(name)](const std::pair<std::string, std::string> &definition) { return definition.first == name; });
					it != preset_it->second.end())
				{
					if (value != it->second)
					{
						it->second = value;
						scope_mask_updated = EFFECT_SCOPE_FLAG;
					}
				}
				else
				{
					preset_it->second.emplace_back(name, value);
					scope_mask_updated = EFFECT_SCOPE_FLAG;
				}
			}
		}
		else
		if (scope_mask == PRESET_SCOPE_FLAG)
		{
			if (const auto preset_it = _preset_preprocessor_definitions.find({});
				preset_it != _preset_preprocessor_definitions.end())
			{
				if (auto it = std::find_if(preset_it->second.begin(), preset_it->second.end(),
						[name = std::string_view(name)](const std::pair<std::string, std::string> &definition) { return definition.first == name; });
					it != preset_it->second.end())
				{
					if (value != it->second)
					{
						it->second = value;
						scope_mask_updated = PRESET_SCOPE_FLAG;
					}
				}
				else
				{
					preset_it->second.emplace_back(name, value);
					scope_mask_updated = PRESET_SCOPE_FLAG;
				}
			}
		}
		else
		if (scope_mask == GLOBAL_SCOPE_FLAG)
		{
			if (const auto it = std::find_if(_global_preprocessor_definitions.begin(), _global_preprocessor_definitions.end(),
					[name = std::string_view(name)](const std::pair<std::string, std::string> &definition) { return definition.first == name; });
				it != _global_preprocessor_definitions.end())
			{
				if (value != it->second)
				{
					it->second = value;
					scope_mask_updated = GLOBAL_SCOPE_FLAG;
				}
			}
			else
			{
				_global_preprocessor_definitions.emplace_back(name, value);
				scope_mask_updated = GLOBAL_SCOPE_FLAG;
			}
		}
		else
		{
			std::vector<std::pair<std::string, std::string>> *definition_scope = nullptr;
			std::vector<std::pair<std::string, std::string>>::iterator definition_it;

			if (get_preprocessor_definition(effect_name_string, name, scope_mask, definition_scope, definition_it) &&
				definition_scope != &_global_preprocessor_definitions && (effect_name_string.empty() || definition_scope != &_preset_preprocessor_definitions[{}]))
				definition_it->second = value;
			else
				_preset_preprocessor_definitions[effect_name_string].emplace_back(name, value);

			scope_mask_updated = PRESET_SCOPE_FLAG;
		}
	}

	if (scope_mask_updated != 0)
	{
		if ((scope_mask_updated & (GLOBAL_SCOPE_FLAG)) != 0)
		{
			ini_file::load_cache(_config_path).set("GENERAL", "PreprocessorDefinitions", _global_preprocessor_definitions);
		}

		if ((scope_mask_updated & (GLOBAL_SCOPE_FLAG | PRESET_SCOPE_FLAG)) != 0)
		{
			_should_reload_effect = _effects.size();
		}
		else
		{
			const size_t effect_index = std::distance(_effects.cbegin(), std::find_if(_effects.cbegin(), _effects.cend(),
				[effect_name = std::filesystem::u8path(effect_name_string)](const effect &effect) { return effect_name == effect.source_file.filename(); }));

			if (effect_index != _should_reload_effect || _should_reload_effect == std::numeric_limits<size_t>::max())
				_should_reload_effect = effect_index;
		}
	}
#endif
}
bool reshade::runtime::get_preprocessor_definition(const char *name, char *value, size_t *size) const
{
	return get_preprocessor_definition_for_effect(nullptr, name, value, size);
}
bool reshade::runtime::get_preprocessor_definition_for_effect([[maybe_unused]] const char *effect_name, [[maybe_unused]] const char *name, [[maybe_unused]] char *value, size_t *size) const
{
#if RESHADE_FX
	std::string effect_name_string;
	if (effect_name != nullptr)
		effect_name_string = effect_name;

	const int scope_mask =
		effect_name_string.find('.') != std::string::npos ? EFFECT_SCOPE_FLAG :
		effect_name_string.compare(0, 6, "PRESET") == 0 ? PRESET_SCOPE_FLAG :
		effect_name_string.compare(0, 6, "GLOBAL") == 0 ? GLOBAL_SCOPE_FLAG :
		EFFECT_SCOPE_FLAG | PRESET_SCOPE_FLAG | GLOBAL_SCOPE_FLAG;

	if (name == nullptr) // Enumerate existing definitions when there is no name to query
	{
		size_t estimate_size = 0;
		std::vector<std::string> definitions;

		const auto emplace_to_list = [&estimate_size, &definitions](const std::pair<std::string, std::string> &adding)
		{
			if (std::all_of(definitions.cbegin(), definitions.cend(),
					[&adding](const std::string &added) { return added != adding.first; }))
			{
				estimate_size += adding.first.size() + 1; // '\0'
				definitions.emplace_back(adding.first);
			}
		};

		if ((scope_mask & EFFECT_SCOPE_FLAG) != 0)
		{
			if (auto it = _preset_preprocessor_definitions.find(effect_name_string);
				it != _preset_preprocessor_definitions.end())
				std::for_each(it->second.begin(), it->second.end(), emplace_to_list);
		}
		if ((scope_mask & PRESET_SCOPE_FLAG) != 0)
		{
			if (auto it = _preset_preprocessor_definitions.find({});
				it != _preset_preprocessor_definitions.end())
				std::for_each(it->second.begin(), it->second.end(), emplace_to_list);
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

		if (get_preprocessor_definition(effect_name_string, name, scope_mask, definition_scope, definition_it))
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
#endif

	if (size != nullptr)
		*size = 0;
	return false;
}

#if RESHADE_FX
bool reshade::runtime::get_preprocessor_definition(const std::string &effect_name, const std::string &name, int scope_mask, std::vector<std::pair<std::string, std::string>> *&scope, std::vector<std::pair<std::string, std::string>>::iterator &value) const
{
	const auto find_preprocessor_definition = [&name, &scope, &value](std::vector<std::pair<std::string, std::string>> &definitions)
	{
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

	if ((scope_mask & EFFECT_SCOPE_FLAG) != 0)
	{
		if (const auto it = _preset_preprocessor_definitions.find(effect_name);
			it != _preset_preprocessor_definitions.end() &&
			find_preprocessor_definition(const_cast<std::vector<std::pair<std::string, std::string>> &>(it->second)))
			return true;
	}
	if ((scope_mask & PRESET_SCOPE_FLAG) != 0)
	{
		if (const auto it = _preset_preprocessor_definitions.find({});
			it != _preset_preprocessor_definitions.end() &&
			find_preprocessor_definition(const_cast<std::vector<std::pair<std::string, std::string>> &>(it->second)))
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

	// Queue effect file for initialization if it was not fully loaded yet
	if (tech->passes_data.empty() &&
		std::find(_reload_create_queue.cbegin(), _reload_create_queue.cend(), tech->effect_index) == _reload_create_queue.cend())
		_reload_create_queue.push_back(tech->effect_index);

	if (rtv == 0 || is_loading())
		return;
	if (rtv_srgb == 0)
		rtv_srgb = rtv;

	const api::resource back_buffer_resource = _device->get_resource_from_view(rtv);

#if RESHADE_ADDON
	{
		const api::resource_desc back_buffer_desc = _device->get_resource_desc(back_buffer_resource);
		if (back_buffer_desc.texture.samples > 1)
			return; // Multisampled render targets are not supported

		api::format color_format = back_buffer_desc.texture.format;
		if (api::format_to_typeless(color_format) == color_format)
			color_format = _device->get_resource_view_desc(rtv).format;

		// Ensure dimensions and format of the effect color resource matches that of the input back buffer resource (so that the copy to the effect color resource succeeds)
		// Never perform an immediate reload here, as the list of techniques must not be modified in case this was called from within 'enumerate_techniques'!
		if (!update_effect_color_and_stencil_tex(back_buffer_desc.texture.width, back_buffer_desc.texture.height, color_format, _effect_stencil_format))
			return;
	}

	if (!_is_in_present_call)
		capture_state(cmd_list, _app_state);

	invoke_addon_event<addon_event::reshade_begin_effects>(this, cmd_list, rtv, rtv_srgb);

	const bool was_is_in_api_call = _is_in_api_call;
	_is_in_api_call = true;
#endif

	render_technique(*tech, cmd_list, back_buffer_resource, rtv, rtv_srgb);

#if RESHADE_ADDON
	_is_in_api_call = was_is_in_api_call;

	invoke_addon_event<addon_event::reshade_finish_effects>(this, cmd_list, rtv, rtv_srgb);

	if (!_is_in_present_call)
		apply_state(cmd_list, _app_state);
#endif
}
#else
void reshade::runtime::render_effects(api::command_list * /*cmd_list*/, api::resource_view /*rtv*/, api::resource_view /*rtv_srgb*/)
{
}
void reshade::runtime::render_technique(api::effect_technique /*handle*/, api::command_list * /*cmd_list*/, api::resource_view /*rtv*/, api::resource_view /*rtv_srgb*/)
{
}
#endif

bool reshade::runtime::get_effects_state() const
{
#if RESHADE_FX
	return _effects_enabled;
#else
	return false;
#endif
}
void reshade::runtime::set_effects_state([[maybe_unused]] bool enabled)
{
#if RESHADE_FX
	_effects_enabled = enabled;
#endif
}

void reshade::runtime::get_current_preset_path([[maybe_unused]] char *path, size_t *size) const
{
	if (size == nullptr)
		return;

#if RESHADE_FX
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
#else
	*size = 0;
#endif
}
void reshade::runtime::set_current_preset_path([[maybe_unused]] const char *path)
{
#if RESHADE_FX
	std::error_code ec;
	std::filesystem::path preset_path = std::filesystem::u8path(path);

	// Only change preset when this is a valid preset path
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
#endif
}

void reshade::runtime::reorder_techniques([[maybe_unused]] size_t count, [[maybe_unused]] const api::effect_technique *techniques)
{
#if RESHADE_FX
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
#endif
}

#if RESHADE_GUI == 0
bool reshade::runtime::open_overlay(bool /*open*/, api::input_source /*source*/)
{
	return false;
}
#endif
