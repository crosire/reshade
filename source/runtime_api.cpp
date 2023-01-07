/*
 * Copyright (C) 2022 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "runtime.hpp"
#include "runtime_objects.hpp"
#include "input.hpp"

reshade::input::window_handle reshade::runtime::get_hwnd() const
{
	return _input != nullptr ? _input->get_window_handle() : nullptr;
}

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

void reshade::runtime::get_mouse_cursor_position(uint32_t *out_x, uint32_t *out_y, int16_t *out_wheel_delta) const
{
	if (out_x != nullptr)
		*out_x = (_input != nullptr) ? _input->mouse_position_x() : 0;
	if (out_y != nullptr)
		*out_y = (_input != nullptr) ? _input->mouse_position_y() : 0;
	if (out_wheel_delta != nullptr)
		*out_wheel_delta = (_input != nullptr) ? _input->mouse_wheel_delta() : 0;
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
	const auto variable = reinterpret_cast<const uniform *>(handle.handle);
	if (variable != nullptr)
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

void reshade::runtime::get_uniform_variable_name([[maybe_unused]] api::effect_uniform_variable handle, [[maybe_unused]] char *value, size_t *length) const
{
	if (length == nullptr)
		return;

#if RESHADE_FX
	const auto variable = reinterpret_cast<const uniform *>(handle.handle);
	if (variable != nullptr)
	{
		if (value != nullptr && *length != 0)
			value[variable->name.copy(value, *length - 1)] = '\0';

		*length = variable->name.size();
	}
	else
#endif
		*length = 0;
}

bool reshade::runtime::get_annotation_bool_from_uniform_variable([[maybe_unused]] api::effect_uniform_variable handle, [[maybe_unused]] const char *name, bool *values, size_t count, [[maybe_unused]] size_t array_index) const
{
#if RESHADE_FX
	const auto variable = reinterpret_cast<const uniform *>(handle.handle);
	if (variable != nullptr)
	{
		if (const auto it = std::find_if(variable->annotations.begin(), variable->annotations.end(),
				[name](const reshadefx::annotation &annotation) { return annotation.name == name; });
			it != variable->annotations.end())
		{
			for (size_t i = 0; i < count; ++i)
				values[i] = variable->annotation_as_int(name, i + array_index) != 0;
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
	const auto variable = reinterpret_cast<const uniform *>(handle.handle);
	if (variable != nullptr)
	{
		if (const auto it = std::find_if(variable->annotations.begin(), variable->annotations.end(),
				[name](const reshadefx::annotation &annotation) { return annotation.name == name; });
			it != variable->annotations.end())
		{
			for (size_t i = 0; i < count; ++i)
				values[i] = variable->annotation_as_float(name, i + array_index);
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
	const auto variable = reinterpret_cast<const uniform *>(handle.handle);
	if (variable != nullptr)
	{
		if (const auto it = std::find_if(variable->annotations.begin(), variable->annotations.end(),
				[name](const reshadefx::annotation &annotation) { return annotation.name == name; });
			it != variable->annotations.end())
		{
			for (size_t i = 0; i < count; ++i)
				values[i] = variable->annotation_as_int(name, array_index + i);
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
	const auto variable = reinterpret_cast<const uniform *>(handle.handle);
	if (variable != nullptr)
	{
		if (const auto it = std::find_if(variable->annotations.begin(), variable->annotations.end(),
				[name](const reshadefx::annotation &annotation) { return annotation.name == name; });
			it != variable->annotations.end())
		{
			for (size_t i = 0; i < count; ++i)
				values[i] = variable->annotation_as_uint(name, array_index + i);
			return true;
		}
	}
#endif

	for (size_t i = 0; i < count; ++i)
		values[i] = 0;
	return false;
}
bool reshade::runtime::get_annotation_string_from_uniform_variable([[maybe_unused]] api::effect_uniform_variable handle, [[maybe_unused]] const char *name, [[maybe_unused]] char *value, size_t *length) const
{
#if RESHADE_FX
	const auto variable = reinterpret_cast<const uniform *>(handle.handle);
	if (variable != nullptr && length != nullptr)
	{
		if (const auto it = std::find_if(variable->annotations.begin(), variable->annotations.end(),
				[name](const reshadefx::annotation &annotation) { return annotation.name == name; });
			it != variable->annotations.end())
		{
			const std::string_view annotation = variable->annotation_as_string(name);

			if (value != nullptr && *length != 0)
				value[annotation.copy(value, *length - 1)] = '\0';

			*length = annotation.size();
			return true;
		}
	}
#endif

	if (length != nullptr)
		*length = 0;
	return false;
}

void reshade::runtime::get_uniform_value_bool([[maybe_unused]] api::effect_uniform_variable handle, bool *values, size_t count, [[maybe_unused]] size_t array_index) const
{
#if RESHADE_FX
	const auto variable = reinterpret_cast<const uniform *>(handle.handle);
	if (variable != nullptr)
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
	const auto variable = reinterpret_cast<const uniform *>(handle.handle);
	if (variable != nullptr)
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
	const auto variable = reinterpret_cast<const uniform *>(handle.handle);
	if (variable != nullptr)
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
	const auto variable = reinterpret_cast<const uniform *>(handle.handle);
	if (variable != nullptr)
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

	if (variable->special == special_uniform::none)
		save_current_preset();

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

	if (variable->special == special_uniform::none)
		save_current_preset();

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

	if (variable->special == special_uniform::none)
		save_current_preset();

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

	if (variable->special == special_uniform::none)
		save_current_preset();

#if RESHADE_ADDON
	_is_in_api_call = was_is_in_api_call;
#endif
#endif
}

void reshade::runtime::enumerate_texture_variables([[maybe_unused]] const char *effect_name, [[maybe_unused]] void(*callback)(effect_runtime *runtime, api::effect_texture_variable variable, void *user_data), [[maybe_unused]] void *user_data)
{
#if RESHADE_FX
	if (is_loading() || !_reload_create_queue.empty())
		return;

	for (const texture &variable : _textures)
	{
		if (effect_name != nullptr &&
			std::find_if(variable.shared.begin(), variable.shared.end(),
				[this, effect_name](size_t effect_index) {
					return _effects[effect_index].source_file.filename() == effect_name;
				}) == variable.shared.end())
			continue;

		callback(this, { reinterpret_cast<uintptr_t>(&variable) }, user_data);
	}
#endif
}

reshade::api::effect_texture_variable reshade::runtime::find_texture_variable([[maybe_unused]] const char *effect_name, [[maybe_unused]] const char *variable_name) const
{
#if RESHADE_FX
	if (is_loading() || !_reload_create_queue.empty())
		return { 0 };

	for (const texture &variable : _textures)
	{
		if (effect_name != nullptr &&
			std::find_if(variable.shared.begin(), variable.shared.end(),
				[this, effect_name](size_t effect_index) {
					return _effects[effect_index].source_file.filename() == effect_name;
				}) == variable.shared.end())
			continue;

		if (variable.name == variable_name || variable.unique_name == variable_name)
			return { reinterpret_cast<uintptr_t>(&variable) };
	}
#endif

	return { 0 };
}

void reshade::runtime::get_texture_variable_name([[maybe_unused]] api::effect_texture_variable handle, [[maybe_unused]] char *value, size_t *length) const
{
	if (length == nullptr)
		return;

#if RESHADE_FX
	const auto variable = reinterpret_cast<const texture *>(handle.handle);
	if (variable != nullptr)
	{
		if (value != nullptr && *length != 0)
			value[variable->name.copy(value, *length - 1)] = '\0';

		*length = variable->name.size();
	}
	else
#endif
		*length = 0;
}

bool reshade::runtime::get_annotation_bool_from_texture_variable([[maybe_unused]] api::effect_texture_variable handle, [[maybe_unused]] const char *name, bool *values, size_t count, [[maybe_unused]] size_t array_index) const
{
#if RESHADE_FX
	const auto variable = reinterpret_cast<const texture *>(handle.handle);
	if (variable != nullptr)
	{
		if (const auto it = std::find_if(variable->annotations.begin(), variable->annotations.end(),
				[name](const reshadefx::annotation &annotation) { return annotation.name == name; });
			it != variable->annotations.end())
		{
			for (size_t i = 0; i < count; ++i)
				values[i] = variable->annotation_as_int(name, array_index + i) != 0;
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
	const auto variable = reinterpret_cast<const texture *>(handle.handle);
	if (variable != nullptr)
	{
		if (const auto it = std::find_if(variable->annotations.begin(), variable->annotations.end(),
				[name](const reshadefx::annotation &annotation) { return annotation.name == name; });
			it != variable->annotations.end())
		{
			for (size_t i = 0; i < count; ++i)
				values[i] = variable->annotation_as_float(name, array_index + i);
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
	const auto variable = reinterpret_cast<const texture *>(handle.handle);
	if (variable != nullptr)
	{
		if (const auto it = std::find_if(variable->annotations.begin(), variable->annotations.end(),
				[name](const reshadefx::annotation &annotation) { return annotation.name == name; });
			it != variable->annotations.end())
		{
			for (size_t i = 0; i < count; ++i)
				values[i] = variable->annotation_as_int(name, array_index + i);
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
	const auto variable = reinterpret_cast<const texture *>(handle.handle);
	if (variable != nullptr)
	{
		if (const auto it = std::find_if(variable->annotations.begin(), variable->annotations.end(),
				[name](const reshadefx::annotation &annotation) { return annotation.name == name; });
			it != variable->annotations.end())
		{
			for (size_t i = 0; i < count; ++i)
				values[i] = variable->annotation_as_uint(name, array_index + i);
			return true;
		}
	}
#endif

	for (size_t i = 0; i < count; ++i)
		values[i] = 0;
	return false;
}
bool reshade::runtime::get_annotation_string_from_texture_variable([[maybe_unused]] api::effect_texture_variable handle, [[maybe_unused]] const char *name, [[maybe_unused]] char *value, size_t *length) const
{
#if RESHADE_FX
	const auto variable = reinterpret_cast<const texture *>(handle.handle);
	if (variable != nullptr && length != nullptr)
	{
		if (const auto it = std::find_if(variable->annotations.begin(), variable->annotations.end(),
				[name](const reshadefx::annotation &annotation) { return annotation.name == name; });
			it != variable->annotations.end())
		{
			const std::string_view annotation = variable->annotation_as_string(name);

			if (value != nullptr && *length != 0)
				value[annotation.copy(value, *length - 1)] = '\0';

			*length = annotation.size();
			return true;
		}
	}
#endif

	if (length != nullptr)
		*length = 0;
	return false;
}

void reshade::runtime::update_texture([[maybe_unused]] api::effect_texture_variable handle, [[maybe_unused]] const uint32_t width, [[maybe_unused]] const uint32_t height, [[maybe_unused]] const uint8_t *pixels)
{
#if RESHADE_FX
	const auto variable = reinterpret_cast<texture *>(handle.handle);
	if (variable == nullptr || variable->resource == 0)
		return;

	update_texture(*variable, width, height, pixels);
#endif
}

void reshade::runtime::get_texture_binding([[maybe_unused]] api::effect_texture_variable handle, api::resource_view *out_srv, api::resource_view *out_srv_srgb) const
{
#if RESHADE_FX
	const auto variable = reinterpret_cast<const texture *>(handle.handle);
	if (variable != nullptr)
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

	// Make sure all previous frames have finished before freeing the image view and updating descriptors (since they may be in use otherwise)
	if (_is_initialized)
		_graphics_queue->wait_idle();

	// Update texture bindings
	size_t num_bindings = 0;
	for (effect &effect_data : _effects)
		num_bindings += effect_data.texture_semantic_to_binding.size();

	std::vector<api::descriptor_set_update> descriptor_writes;
	std::vector<api::sampler_with_resource_view> sampler_descriptors(num_bindings);

	for (effect &effect_data : _effects)
	{
		for (const effect::binding_data &binding : effect_data.texture_semantic_to_binding)
		{
			if (binding.semantic != semantic)
				continue;

			api::descriptor_set_update &write = descriptor_writes.emplace_back();
			write.set = binding.set;
			write.binding = binding.index;
			write.count = 1;

			if (binding.sampler != 0)
			{
				write.type = api::descriptor_type::sampler_with_resource_view;
				write.descriptors = &sampler_descriptors[--num_bindings];
			}
			else
			{
				write.type = api::descriptor_type::shader_resource_view;
				write.descriptors = &sampler_descriptors[--num_bindings].view;
			}

			sampler_descriptors[num_bindings].sampler = binding.sampler;
			sampler_descriptors[num_bindings].view = binding.srgb ? srv_srgb : srv;
		}
	}

	_device->update_descriptor_sets(static_cast<uint32_t>(descriptor_writes.size()), descriptor_writes.data());
#endif
}

void reshade::runtime::enumerate_techniques([[maybe_unused]] const char *effect_name, [[maybe_unused]] void(*callback)(effect_runtime *runtime, api::effect_technique technique, void *user_data), [[maybe_unused]] void *user_data)
{
#if RESHADE_FX
	if (is_loading())
		return;

	for (const technique &technique : _techniques)
	{
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

void reshade::runtime::get_technique_name([[maybe_unused]] api::effect_technique handle, [[maybe_unused]] char *value, size_t *length) const
{
	if (length == nullptr)
		return;

#if RESHADE_FX
	const auto tech = reinterpret_cast<const technique *>(handle.handle);
	if (tech != nullptr)
	{
		if (value != nullptr && *length != 0)
			value[tech->name.copy(value, *length - 1)] = '\0';

		*length = tech->name.size();
	}
	else
#endif
		*length = 0;
}

bool reshade::runtime::get_annotation_bool_from_technique([[maybe_unused]] api::effect_technique handle, [[maybe_unused]] const char *name, bool *values, size_t count, [[maybe_unused]] size_t array_index) const
{
#if RESHADE_FX
	const auto tech = reinterpret_cast<const technique *>(handle.handle);
	if (tech != nullptr)
	{
		if (const auto it = std::find_if(tech->annotations.begin(), tech->annotations.end(),
				[name](const reshadefx::annotation &annotation) { return annotation.name == name; });
			it != tech->annotations.end())
		{
			for (size_t i = 0; i < count; ++i)
				values[i] = tech->annotation_as_int(name, array_index + i) != 0;
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
	const auto tech = reinterpret_cast<const technique *>(handle.handle);
	if (tech != nullptr)
	{
		if (const auto it = std::find_if(tech->annotations.begin(), tech->annotations.end(),
				[name](const reshadefx::annotation &annotation) { return annotation.name == name; });
			it != tech->annotations.end())
		{
			for (size_t i = 0; i < count; ++i)
				values[i] = tech->annotation_as_float(name, array_index + i);
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
	const auto tech = reinterpret_cast<const technique *>(handle.handle);
	if (tech != nullptr)
	{
		if (const auto it = std::find_if(tech->annotations.begin(), tech->annotations.end(),
				[name](const reshadefx::annotation &annotation) { return annotation.name == name; });
			it != tech->annotations.end())
		{
			for (size_t i = 0; i < count; ++i)
				values[i] = tech->annotation_as_int(name, array_index + i);
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
	const auto tech = reinterpret_cast<const technique *>(handle.handle);
	if (tech != nullptr)
	{
		if (const auto it = std::find_if(tech->annotations.begin(), tech->annotations.end(),
				[name](const reshadefx::annotation &annotation) { return annotation.name == name; });
			it != tech->annotations.end())
		{
			for (size_t i = 0; i < count; ++i)
				values[i] = tech->annotation_as_uint(name, array_index + i);
			return true;
		}
	}
#endif

	for (size_t i = 0; i < count; ++i)
		values[i] = 0;
	return false;
}
bool reshade::runtime::get_annotation_string_from_technique([[maybe_unused]] api::effect_technique handle, [[maybe_unused]] const char *name, [[maybe_unused]] char *value, size_t *length) const
{
#if RESHADE_FX
	const auto tech = reinterpret_cast<const technique *>(handle.handle);
	if (tech != nullptr && length != nullptr)
	{
		if (const auto it = std::find_if(tech->annotations.begin(), tech->annotations.end(),
				[name](const reshadefx::annotation &annotation) { return annotation.name == name; });
			it != tech->annotations.end())
		{
			const std::string_view annotation = tech->annotation_as_string(name);

			if (value != nullptr && *length != 0)
				value[annotation.copy(value, *length - 1)] = '\0';

			*length = annotation.size();
			return true;
		}
	}
#endif

	if (length != nullptr)
		*length = 0;
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

void reshade::runtime::set_preprocessor_definition([[maybe_unused]] const char *name, [[maybe_unused]] const char *value)
{
#if RESHADE_FX
	if (name == nullptr)
		return;

	auto preset_it = _preset_preprocessor_definitions.begin();
	for (; preset_it != _preset_preprocessor_definitions.end(); ++preset_it)
	{
		const size_t equals_index = preset_it->find('=');
		if (equals_index != std::string::npos && name == preset_it->substr(0, equals_index))
			break;
	}

	if (value == nullptr || value[0] == '\0') // An empty value removes the definition
	{
		if (preset_it != _preset_preprocessor_definitions.end())
		{
			_preset_preprocessor_definitions.erase(preset_it);
		}
	}
	else
	{
		if (preset_it != _preset_preprocessor_definitions.end())
		{
			*preset_it = std::string(name) + '=' + value;
		}
		else
		{
			_preset_preprocessor_definitions.push_back(std::string(name) + '=' + value);
		}
	}

	// Save preset first, as preprocessor definitions are reset to those in the current preset during reloading
	save_current_preset();
	reload_effects();
#endif
}
bool reshade::runtime::get_preprocessor_definition(const char *name, [[maybe_unused]] char *value, size_t *length) const
{
	if (name == nullptr || length == nullptr)
		return false;

#if RESHADE_FX
	for (auto preset_it = _preset_preprocessor_definitions.begin(); preset_it != _preset_preprocessor_definitions.end(); ++preset_it)
	{
		const size_t equals_index = preset_it->find('=');
		if (equals_index != std::string::npos && name == preset_it->substr(0, equals_index))
		{
			if (value != nullptr && *length != 0)
				value[preset_it->copy(value, *length - 1, equals_index + 1)] = '\0';

			*length = preset_it->size() - (equals_index + 1);
			return true;
		}
	}
	for (auto global_it = _global_preprocessor_definitions.begin(); global_it != _global_preprocessor_definitions.end(); ++global_it)
	{
		const size_t equals_index = global_it->find('=');
		if (equals_index != std::string::npos && name == global_it->substr(0, equals_index))
		{
			if (value != nullptr && *length != 0)
				value[global_it->copy(value, *length - 1, equals_index + 1)] = '\0';

			*length = global_it->size() - (equals_index + 1);
			return true;
		}
	}
#endif

	*length = 0;
	return false;
}

#if RESHADE_FX
void reshade::runtime::render_technique(api::effect_technique handle, api::command_list *cmd_list, api::resource_view rtv, api::resource_view rtv_srgb)
{
	const auto tech = reinterpret_cast<technique *>(handle.handle);
	if (tech == nullptr)
		return;

	// Queue effect file for initialization if it was not fully loaded yet
	if (tech->passes_data.empty() &&
		std::find(_reload_create_queue.begin(), _reload_create_queue.end(), tech->effect_index) == _reload_create_queue.end())
		_reload_create_queue.push_back(tech->effect_index);

	if (is_loading() || tech->passes_data.empty() || rtv == 0)
		return;

	if (rtv_srgb == 0)
		rtv_srgb = rtv;

	const api::resource back_buffer_resource = _device->get_resource_from_view(rtv);

	// Ensure format of the effect color resource matches that of the input back buffer resource (so that the copy to the effect color resource succeeds)
	if (!update_effect_color_tex(_device->get_resource_desc(back_buffer_resource).texture.format))
		return;

#if RESHADE_ADDON
	const bool was_is_in_api_call = _is_in_api_call;
	_is_in_api_call = true;
#endif

	render_technique(*tech, cmd_list, back_buffer_resource, rtv, rtv_srgb);

#if RESHADE_ADDON
	_is_in_api_call = was_is_in_api_call;
#endif
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

void reshade::runtime::get_current_preset_path([[maybe_unused]] char *path, size_t *length) const
{
	if (length == nullptr)
		return;

#if RESHADE_FX
	const std::string path_string = _current_preset_path.u8string();

	if (path != nullptr && *length != 0)
		path[path_string.copy(path, *length - 1)] = '\0';

	*length = path_string.size();
#else
	*length = 0;
#endif
}
void reshade::runtime::set_current_preset_path([[maybe_unused]] const char *path)
{
#if RESHADE_FX
	// First save current preset, before switching to a new one
	save_current_preset();

	_current_preset_path = std::filesystem::u8path(path);

	save_config();
	load_current_preset();
#endif
}
