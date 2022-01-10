/*
 * Copyright (C) 2022 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "runtime.hpp"
#include "runtime_objects.hpp"
#include "input.hpp"
#include <cassert>

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

void reshade::runtime::enumerate_uniform_variables(const char *effect_name, void(*callback)(effect_runtime *runtime, api::effect_uniform_variable variable, void *user_data), void *user_data)
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

reshade::api::effect_uniform_variable reshade::runtime::find_uniform_variable(const char *effect_name, const char *variable_name) const
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

void reshade::runtime::get_uniform_variable_type(api::effect_uniform_variable handle, api::format *out_base_type, uint32_t *out_rows, uint32_t *out_columns, uint32_t *out_array_length) const
{
#if RESHADE_FX
	const auto variable = reinterpret_cast<const uniform *>(handle.handle);
	if (variable == nullptr)
		return;

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
	if (out_columns)
		*out_columns = variable->type.cols;
	if (out_array_length)
		*out_array_length = variable->type.array_length;
#endif
}

void reshade::runtime::get_uniform_variable_name(api::effect_uniform_variable handle, char *value, size_t *length) const
{
#if RESHADE_FX
	if (length == nullptr)
		return;

	const auto variable = reinterpret_cast<const uniform *>(handle.handle);
	if (variable != nullptr)
	{
		if (value != nullptr && *length != 0)
			value[variable->name.copy(value, *length - 1)] = '\0';

		*length = variable->name.size();
}
	else
	{
		*length = 0;
	}
#endif
}

void reshade::runtime::get_uniform_annotation_value(api::effect_uniform_variable handle, const char *name, bool *values, size_t count, size_t array_index) const
{
#if RESHADE_FX
	const auto variable = reinterpret_cast<const uniform *>(handle.handle);
	if (variable == nullptr)
		return;

	for (size_t i = 0; i < count; ++i)
		values[i] = variable->annotation_as_uint(name, array_index + i) != 0;
#endif
}
void reshade::runtime::get_uniform_annotation_value(api::effect_uniform_variable handle, const char *name, float *values, size_t count, size_t array_index) const
{
#if RESHADE_FX
	const auto variable = reinterpret_cast<const uniform *>(handle.handle);
	if (variable == nullptr)
		return;

	for (size_t i = 0; i < count; ++i)
		values[i] = variable->annotation_as_float(name, array_index + i);
#endif
}
void reshade::runtime::get_uniform_annotation_value(api::effect_uniform_variable handle, const char *name, int32_t *values, size_t count, size_t array_index) const
{
#if RESHADE_FX
	const auto variable = reinterpret_cast<const uniform *>(handle.handle);
	if (variable == nullptr)
		return;

	for (size_t i = 0; i < count; ++i)
		values[i] = variable->annotation_as_int(name, array_index + i);
#endif
}
void reshade::runtime::get_uniform_annotation_value(api::effect_uniform_variable handle, const char *name, uint32_t *values, size_t count, size_t array_index) const
{
#if RESHADE_FX
	const auto variable = reinterpret_cast<const uniform *>(handle.handle);
	if (variable == nullptr)
		return;

	for (size_t i = 0; i < count; ++i)
		values[i] = variable->annotation_as_uint(name, array_index + i);
#endif
}
void reshade::runtime::get_uniform_annotation_value(api::effect_uniform_variable handle, const char *name, char *value, size_t *length) const
{
#if RESHADE_FX
	if (length == nullptr)
		return;

	const auto variable = reinterpret_cast<const uniform *>(handle.handle);
	std::string_view annotation;

	if (variable != nullptr && !(annotation = variable->annotation_as_string(name)).empty())
	{
		if (value != nullptr && *length != 0)
			value[annotation.copy(value, *length - 1)] = '\0';

		*length = annotation.size();
	}
	else
	{
		*length = 0;
	}
#endif
}

void reshade::runtime::get_uniform_value(api::effect_uniform_variable handle, bool *values, size_t count, size_t array_index) const
{
#if RESHADE_FX
	const auto variable = reinterpret_cast<const uniform *>(handle.handle);
	if (variable == nullptr)
		return;

	get_uniform_value(*variable, values, count, array_index);
#endif
}
void reshade::runtime::get_uniform_value(api::effect_uniform_variable handle, float *values, size_t count, size_t array_index) const
{
#if RESHADE_FX
	const auto variable = reinterpret_cast<const uniform *>(handle.handle);
	if (variable == nullptr)
		return;

	get_uniform_value(*variable, values, count, array_index);
#endif
}
void reshade::runtime::get_uniform_value(api::effect_uniform_variable handle, int32_t *values, size_t count, size_t array_index) const
{
#if RESHADE_FX
	const auto variable = reinterpret_cast<const uniform *>(handle.handle);
	if (variable == nullptr)
		return;

	get_uniform_value(*variable, values, count, array_index);
#endif
}
void reshade::runtime::get_uniform_value(api::effect_uniform_variable handle, uint32_t *values, size_t count, size_t array_index) const
{
#if RESHADE_FX
	const auto variable = reinterpret_cast<const uniform *>(handle.handle);
	if (variable == nullptr)
		return;

	get_uniform_value(*variable, values, count, array_index);
#endif
}

void reshade::runtime::set_uniform_value(api::effect_uniform_variable handle, const bool *values, size_t count, size_t array_index)
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
void reshade::runtime::set_uniform_value(api::effect_uniform_variable handle, const float *values, size_t count, size_t array_index)
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
void reshade::runtime::set_uniform_value(api::effect_uniform_variable handle, const int32_t *values, size_t count, size_t array_index)
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
void reshade::runtime::set_uniform_value(api::effect_uniform_variable handle, const uint32_t *values, size_t count, size_t array_index)
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

void reshade::runtime::enumerate_texture_variables(const char *effect_name, void(*callback)(effect_runtime *runtime, api::effect_texture_variable variable, void *user_data), void *user_data)
{
#if RESHADE_FX
	if (is_loading() || !_reload_create_queue.empty())
		return;

	for (const texture &variable : _textures)
	{
		if (effect_name != nullptr &&
			std::find_if(variable.shared.begin(), variable.shared.end(),
				[this, effect_name](size_t effect_index) { return _effects[effect_index].source_file.filename() == effect_name; }) == variable.shared.end())
			continue;

		callback(this, { reinterpret_cast<uintptr_t>(&variable) }, user_data);
	}
#endif
}

reshade::api::effect_texture_variable reshade::runtime::find_texture_variable(const char *effect_name, const char *variable_name) const
{
#if RESHADE_FX
	if (is_loading() || !_reload_create_queue.empty())
		return { 0 };

	for (const texture &variable : _textures)
	{
		if (effect_name != nullptr &&
			std::find_if(variable.shared.begin(), variable.shared.end(),
				[this, effect_name](size_t effect_index) { return _effects[effect_index].source_file.filename() == effect_name; }) == variable.shared.end())
			continue;

		if (variable.name == variable_name || variable.unique_name == variable_name)
			return { reinterpret_cast<uintptr_t>(&variable) };
	}
#endif
	return { 0 };
}

void reshade::runtime::get_texture_variable_name(api::effect_texture_variable handle, char *value, size_t *length) const
{
#if RESHADE_FX
	if (length == nullptr)
		return;

	const auto variable = reinterpret_cast<const texture *>(handle.handle);
	if (variable != nullptr)
	{
		if (value != nullptr && *length != 0)
			value[variable->name.copy(value, *length - 1)] = '\0';

		*length = variable->name.size();
}
	else
	{
		*length = 0;
	}
#endif
}

void reshade::runtime::get_texture_annotation_value(api::effect_texture_variable handle, const char *name, bool *values, size_t count, size_t array_index) const
{
#if RESHADE_FX
	const auto variable = reinterpret_cast<const texture *>(handle.handle);
	if (variable == nullptr)
		return;

	for (size_t i = 0; i < count; ++i)
		values[i] = variable->annotation_as_uint(name, array_index + i) != 0;
#endif
}
void reshade::runtime::get_texture_annotation_value(api::effect_texture_variable handle, const char *name, float *values, size_t count, size_t array_index) const
{
#if RESHADE_FX
	const auto variable = reinterpret_cast<const texture *>(handle.handle);
	if (variable == nullptr)
		return;

	for (size_t i = 0; i < count; ++i)
		values[i] = variable->annotation_as_float(name, array_index + i);
#endif
}
void reshade::runtime::get_texture_annotation_value(api::effect_texture_variable handle, const char *name, int32_t *values, size_t count, size_t array_index) const
{
#if RESHADE_FX
	const auto variable = reinterpret_cast<const texture *>(handle.handle);
	if (variable == nullptr)
		return;

	for (size_t i = 0; i < count; ++i)
		values[i] = variable->annotation_as_int(name, array_index + i);
#endif
}
void reshade::runtime::get_texture_annotation_value(api::effect_texture_variable handle, const char *name, uint32_t *values, size_t count, size_t array_index) const
{
#if RESHADE_FX
	const auto variable = reinterpret_cast<const texture *>(handle.handle);
	if (variable == nullptr)
		return;

	for (size_t i = 0; i < count; ++i)
		values[i] = variable->annotation_as_uint(name, array_index + i);
#endif
}
void reshade::runtime::get_texture_annotation_value(api::effect_texture_variable handle, const char *name, char *value, size_t *length) const
{
#if RESHADE_FX
	if (length == nullptr)
		return;

	const auto variable = reinterpret_cast<const texture *>(handle.handle);
	std::string_view annotation;

	if (variable != nullptr && !(annotation = variable->annotation_as_string(name)).empty())
	{
		if (value != nullptr && *length != 0)
			value[annotation.copy(value, *length - 1)] = '\0';

		*length = annotation.size();
	}
	else
	{
		*length = 0;
	}
#endif
}

void reshade::runtime::update_texture(api::effect_texture_variable handle, const uint32_t width, const uint32_t height, const uint8_t *pixels)
{
#if RESHADE_FX
	if (handle == 0)
		return;
	auto &variable = *reinterpret_cast<texture *>(handle.handle);
	if (variable.resource == 0)
		return;

	update_texture(variable, width, height, pixels);
#endif
}

void reshade::runtime::get_texture_binding(api::effect_texture_variable variable, api::resource_view *out_srv, api::resource_view *out_srv_srgb) const
{
#if RESHADE_FX
	if (variable == 0)
		return;

	if (out_srv != nullptr)
		*out_srv = reinterpret_cast<const texture *>(variable.handle)->srv[0];
	if (out_srv_srgb != nullptr)
		*out_srv_srgb = reinterpret_cast<const texture *>(variable.handle)->srv[1];
#endif
}

void reshade::runtime::update_texture_bindings(const char *semantic, api::resource_view srv, api::resource_view srv_srgb)
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
	_graphics_queue->wait_idle();

	// Update texture bindings
	size_t num_bindings = 0;
	for (effect &effect_data : _effects)
		num_bindings += effect_data.texture_semantic_to_binding.size();

	std::vector<api::descriptor_set_update> descriptor_writes;
	std::vector<api::sampler_with_resource_view> sampler_descriptors(num_bindings);

	for (effect &effect_data : _effects)
	{
		for (const auto &binding : effect_data.texture_semantic_to_binding)
		{
			if (binding.semantic != semantic)
				continue;

			assert(num_bindings != 0);

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

void reshade::runtime::enumerate_techniques(const char *effect_name, void(*callback)(effect_runtime *runtime, api::effect_technique technique, void *user_data), void *user_data)
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

reshade::api::effect_technique reshade::runtime::find_technique(const char *effect_name, const char *technique_name)
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

void reshade::runtime::get_technique_name(api::effect_technique handle, char *value, size_t *length) const
{
#if RESHADE_FX
	if (length == nullptr)
		return;

	const auto tech = reinterpret_cast<const technique *>(handle.handle);
	if (tech != nullptr)
	{
		if (value != nullptr && *length != 0)
			value[tech->name.copy(value, *length - 1)] = '\0';

		*length = tech->name.size();
	}
	else
	{
		*length = 0;
	}
#endif
}

void reshade::runtime::get_technique_annotation_value(api::effect_technique handle, const char *name, bool *values, size_t count, size_t array_index) const
{
#if RESHADE_FX
	const auto tech = reinterpret_cast<const technique *>(handle.handle);
	if (tech == nullptr)
		return;

	for (size_t i = 0; i < count; ++i)
		values[i] = tech->annotation_as_uint(name, array_index + i) != 0;
#endif
}
void reshade::runtime::get_technique_annotation_value(api::effect_technique handle, const char *name, float *values, size_t count, size_t array_index) const
{
#if RESHADE_FX
	const auto tech = reinterpret_cast<const technique *>(handle.handle);
	if (tech == nullptr)
		return;

	for (size_t i = 0; i < count; ++i)
		values[i] = tech->annotation_as_float(name, array_index + i);
#endif
}
void reshade::runtime::get_technique_annotation_value(api::effect_technique handle, const char *name, int32_t *values, size_t count, size_t array_index) const
{
#if RESHADE_FX
	const auto tech = reinterpret_cast<const technique *>(handle.handle);
	if (tech == nullptr)
		return;

	for (size_t i = 0; i < count; ++i)
		values[i] = tech->annotation_as_int(name, array_index + i);
#endif
}
void reshade::runtime::get_technique_annotation_value(api::effect_technique handle, const char *name, uint32_t *values, size_t count, size_t array_index) const
{
#if RESHADE_FX
	const auto tech = reinterpret_cast<const technique *>(handle.handle);
	if (tech == nullptr)
		return;

	for (size_t i = 0; i < count; ++i)
		values[i] = tech->annotation_as_uint(name, array_index + i);
#endif
}
void reshade::runtime::get_technique_annotation_value(api::effect_technique handle, const char *name, char *value, size_t *length) const
{
#if RESHADE_FX
	if (length == nullptr)
		return;

	const auto tech = reinterpret_cast<const technique *>(handle.handle);
	std::string_view annotation;

	if (tech != nullptr && !(annotation = tech->annotation_as_string(name)).empty())
	{
		if (value != nullptr && *length != 0)
			value[annotation.copy(value, *length - 1)] = '\0';

		*length = annotation.size();
	}
	else
	{
		*length = 0;
	}
#endif
}

bool reshade::runtime::get_technique_enabled(api::effect_technique handle) const
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
void reshade::runtime::set_technique_enabled(api::effect_technique handle, bool enabled)
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

void reshade::runtime::set_preprocessor_definition(const char *name, const char *value)
{
#if RESHADE_FX
	if (name == nullptr)
		return;

	auto preset_it = _preset_preprocessor_definitions.begin();
	for (; preset_it != _preset_preprocessor_definitions.end(); ++preset_it)
	{
		char current_name[128] = "";
		const size_t equals_index = preset_it->find('=');
		preset_it->copy(current_name, std::min(equals_index, sizeof(current_name) - 1));

		if (strcmp(name, current_name) == 0 && equals_index != std::string::npos)
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

	reload_effects();
#endif
}
bool reshade::runtime::get_preprocessor_definition(const char *name, char *value, size_t *length) const
{
#if RESHADE_FX
	if (name == nullptr || length == nullptr)
		return false;

	for (auto it = _preset_preprocessor_definitions.begin(); it != _preset_preprocessor_definitions.end(); ++it)
	{
		char current_name[128] = "";
		const size_t equals_index = it->find('=');
		it->copy(current_name, std::min(equals_index, sizeof(current_name) - 1));

		if (strcmp(name, current_name) == 0 && equals_index != std::string::npos)
		{
			if (value != nullptr && *length != 0)
				value[it->copy(value, *length - 1, equals_index + 1)] = '\0';

			*length = it->size() - (equals_index + 1);
			return true;
		}
	}
	for (auto it = _global_preprocessor_definitions.begin(); it != _global_preprocessor_definitions.end(); ++it)
	{
		char current_name[128] = "";
		const size_t equals_index = it->find('=');
		it->copy(current_name, std::min(equals_index, sizeof(current_name) - 1));

		if (strcmp(name, current_name) == 0 && equals_index != std::string::npos)
		{
			if (value != nullptr && *length != 0)
				value[it->copy(value, *length - 1, equals_index + 1)] = '\0';

			*length = it->size() - (equals_index + 1);
			return true;
		}
	}
#endif
	return false;
}
