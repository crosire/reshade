/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "log.hpp"
#include "version.h"
#include "runtime.hpp"
#include "runtime_objects.hpp"
#include "effect_parser.hpp"
#include "effect_codegen.hpp"
#include "effect_preprocessor.hpp"
#include "input.hpp"
#include "ini_file.hpp"
#include <assert.h>
#include <thread>
#include <algorithm>
#include <stb_image.h>
#include <stb_image_dds.h>
#include <stb_image_write.h>
#include <stb_image_resize.h>

extern volatile long g_network_traffic;
extern std::filesystem::path g_reshade_dll_path;
extern std::filesystem::path g_target_executable_path;

static bool find_file(const std::vector<std::filesystem::path> &search_paths, std::filesystem::path &path)
{
	std::error_code ec;
	if (path.is_relative())
		for (const auto &search_path : search_paths)
		{
			std::filesystem::path canonical_search_path = search_path;
			if (search_path.is_relative())
				canonical_search_path = std::filesystem::canonical(g_reshade_dll_path.parent_path() / search_path, ec);
			if (ec || canonical_search_path.empty())
				continue;

			if (std::filesystem::path result = canonical_search_path / path; std::filesystem::exists(result, ec))
			{
				path = std::move(result);
				return true;
			}
		}
	return std::filesystem::exists(path, ec);
}

static std::vector<std::filesystem::path> find_files(const std::vector<std::filesystem::path> &search_paths, std::initializer_list<const char *> extensions)
{
	std::error_code ec;
	std::vector<std::filesystem::path> files;
	for (const auto &search_path : search_paths)
	{
		std::filesystem::path canonical_search_path = search_path;
		if (search_path.is_relative()) // Ignore the working directory and instead start relative paths at the DLL location
			canonical_search_path = std::filesystem::canonical(g_reshade_dll_path.parent_path() / search_path, ec);
		if (ec || canonical_search_path.empty())
			continue; // Failed to find a valid directory matching the search path

		for (const auto &entry : std::filesystem::directory_iterator(canonical_search_path, ec))
			for (auto ext : extensions)
				if (entry.path().extension() == ext)
					files.push_back(entry.path());
	}
	return files;
}

reshade::runtime::runtime() :
	_start_time(std::chrono::high_resolution_clock::now()),
	_last_present_time(std::chrono::high_resolution_clock::now()),
	_last_frame_duration(std::chrono::milliseconds(1)),
	_effect_search_paths({ ".\\" }),
	_texture_search_paths({ ".\\" }),
	_global_preprocessor_definitions({
		"RESHADE_DEPTH_LINEARIZATION_FAR_PLANE=1000.0",
		"RESHADE_DEPTH_INPUT_IS_UPSIDE_DOWN=0",
		"RESHADE_DEPTH_INPUT_IS_REVERSED=1",
		"RESHADE_DEPTH_INPUT_IS_LOGARITHMIC=0" }),
	_reload_key_data(),
	_effects_key_data(),
	_screenshot_key_data(),
	_screenshot_path(g_target_executable_path.parent_path())
{
	// Default shortcut PrtScrn
	_screenshot_key_data[0] = 0x2C;

	_configuration_path = g_reshade_dll_path;
	_configuration_path.replace_extension(".ini");
	if (std::error_code ec; !std::filesystem::exists(_configuration_path, ec))
		_configuration_path = g_reshade_dll_path.parent_path() / "ReShade.ini";

	_needs_update = check_for_update(_latest_version);

#if RESHADE_GUI
	init_ui();
#endif
	load_config();
}
reshade::runtime::~runtime()
{
#if RESHADE_GUI
	deinit_ui();
#endif

	assert(!_is_initialized && _techniques.empty());
}

bool reshade::runtime::on_init(input::window_handle window)
{
	LOG(INFO) << "Recreated runtime environment on runtime " << this << '.';

	_input = input::register_window(window);

	// Reset frame count to zero so effects are loaded in 'update_and_render_effects'
	_framecount = 0;

	_is_initialized = true;
	_last_reload_time = std::chrono::high_resolution_clock::now();

#if RESHADE_GUI
	build_font_atlas();
#endif

	return true;
}
void reshade::runtime::on_reset()
{
	unload_effects();

	if (!_is_initialized)
		return;

#if RESHADE_GUI
	destroy_font_atlas();
#endif

	LOG(INFO) << "Destroyed runtime environment on runtime " << this << '.';

	_width = _height = 0;
	_is_initialized = false;
}
void reshade::runtime::on_present()
{
	// Get current time and date
	time_t t = std::time(nullptr); tm tm;
	localtime_s(&tm, &t);
	_date[0] = tm.tm_year + 1900;
	_date[1] = tm.tm_mon + 1;
	_date[2] = tm.tm_mday;
	_date[3] = tm.tm_hour * 3600 + tm.tm_min * 60 + tm.tm_sec;

	// Advance various statistics
	_framecount++;
	_last_present_time += _last_frame_duration = std::chrono::high_resolution_clock::now() - _last_present_time;

	// Lock input so it cannot be modified by other threads while we are reading it here
	const auto input_lock = _input->lock();

	// Handle keyboard shortcuts
	if (!_ignore_shortcuts)
	{
		if (_input->is_key_pressed(_reload_key_data))
			load_effects();

		if (_input->is_key_pressed(_effects_key_data))
			_effects_enabled = !_effects_enabled;

		if (_input->is_key_pressed(_screenshot_key_data))
			save_screenshot();
	}

#if RESHADE_GUI
	// Draw overlay
	draw_ui();
#endif

	// Reset input status
	_input->next_frame();

	static int cooldown = 0, traffic = 0;

	if (cooldown-- > 0)
	{
		traffic += g_network_traffic > 0;
	}
	else
	{
		_has_high_network_activity = traffic > 10;
		traffic = 0;
		cooldown = 30;
	}

	g_network_traffic = 0;
	_drawcalls = _vertices = 0;
}

void reshade::runtime::load_effect(const std::filesystem::path &path, size_t &out_id)
{
	effect_data effect;
	effect.source_file = path;
	effect.compile_sucess = true;

	{ // Load, pre-process and compile the source file
		reshadefx::preprocessor pp;
		if (path.is_absolute())
			pp.add_include_path(path.parent_path());

		for (const auto &include_path : _effect_search_paths)
		{
			std::error_code ec;
			std::filesystem::path canonical_include_path = include_path;
			if (include_path.is_relative()) // Ignore the working directory and instead start relative paths at the DLL location
				canonical_include_path = std::filesystem::canonical(g_reshade_dll_path.parent_path() / include_path, ec);

			if (!ec && !canonical_include_path.empty())
				pp.add_include_path(canonical_include_path);
		}

		pp.add_macro_definition("__RESHADE__", std::to_string(VERSION_MAJOR * 10000 + VERSION_MINOR * 100 + VERSION_REVISION));
		pp.add_macro_definition("__RESHADE_PERFORMANCE_MODE__", _performance_mode ? "1" : "0");
		pp.add_macro_definition("__VENDOR__", std::to_string(_vendor_id));
		pp.add_macro_definition("__DEVICE__", std::to_string(_device_id));
		pp.add_macro_definition("__RENDERER__", std::to_string(_renderer_id));
		pp.add_macro_definition("__APPLICATION__", std::to_string(std::hash<std::string>()(g_target_executable_path.stem().u8string())));
		pp.add_macro_definition("BUFFER_WIDTH", std::to_string(_width));
		pp.add_macro_definition("BUFFER_HEIGHT", std::to_string(_height));
		pp.add_macro_definition("BUFFER_RCP_WIDTH", "(1.0 / BUFFER_WIDTH)");
		pp.add_macro_definition("BUFFER_RCP_HEIGHT", "(1.0 / BUFFER_HEIGHT)");

		std::vector<std::string> preprocessor_definitions = _global_preprocessor_definitions;
		preprocessor_definitions.insert(preprocessor_definitions.end(), _preset_preprocessor_definitions.begin(), _preset_preprocessor_definitions.end());

		for (const auto &definition : preprocessor_definitions)
		{
			if (definition.empty())
				continue; // Skip invalid definitions

			const size_t equals_index = definition.find('=');
			if (equals_index != std::string::npos)
				pp.add_macro_definition(
					definition.substr(0, equals_index),
					definition.substr(equals_index + 1));
			else
				pp.add_macro_definition(definition);
		}

		if (!pp.append_file(path))
		{
			LOG(ERROR) << "Failed to load " << path << ":\n" << pp.errors();
			effect.compile_sucess = false;
		}

		unsigned shader_model;
		if (_renderer_id == 0x9000)
			shader_model = 30;
		else if (_renderer_id < 0xa100)
			shader_model = 40;
		else if (_renderer_id < 0xb000)
			shader_model = 41;
		else if (_renderer_id < 0xc000)
			shader_model = 50;
		else
			shader_model = 60;

		std::unique_ptr<reshadefx::codegen> codegen;
		if ((_renderer_id & 0xF0000) == 0)
			codegen.reset(reshadefx::create_codegen_hlsl(shader_model, true, _performance_mode));
		else if (_renderer_id < 0x20000)
			codegen.reset(reshadefx::create_codegen_glsl(true, _performance_mode));
		else // Vulkan uses SPIR-V input
			codegen.reset(reshadefx::create_codegen_spirv(true, _performance_mode));

		reshadefx::parser parser;

		// Compile the pre-processed source code (try the compile even if the preprocessor step failed to get additional error information)
		if (!parser.parse(std::move(pp.output()), codegen.get()))
		{
			LOG(ERROR) << "Failed to compile " << path << ":\n" << parser.errors();
			effect.compile_sucess = false;
		}

		// Append preprocessor and parser errors to the error list
		effect.errors = std::move(pp.errors()) + std::move(parser.errors());

		// Write result to effect module
		codegen->write_result(effect.module);
	}

	// Fill all specialization constants with values from the current preset
	if (_performance_mode && !_current_preset_path.empty() && effect.compile_sucess)
	{
		const ini_file preset(_current_preset_path);
		const std::string section(path.filename().u8string());

		for (reshadefx::uniform_info &constant : effect.module.spec_constants)
		{
			effect.preamble += "#define SPEC_CONSTANT_" + constant.name + ' ';

			switch (constant.type.base)
			{
			case reshadefx::type::t_int:
				preset.get(section, constant.name, constant.initializer_value.as_int);
				break;
			case reshadefx::type::t_bool:
			case reshadefx::type::t_uint:
				preset.get(section, constant.name, constant.initializer_value.as_uint);
				break;
			case reshadefx::type::t_float:
				preset.get(section, constant.name, constant.initializer_value.as_float);
				break;
			}

			for (unsigned int i = 0; i < constant.type.components(); ++i)
			{
				switch (constant.type.base)
				{
				case reshadefx::type::t_bool:
					effect.preamble += constant.initializer_value.as_uint[i] ? "true" : "false";
					break;
				case reshadefx::type::t_int:
					effect.preamble += std::to_string(constant.initializer_value.as_int[i]);
					break;
				case reshadefx::type::t_uint:
					effect.preamble += std::to_string(constant.initializer_value.as_uint[i]);
					break;
				case reshadefx::type::t_float:
					effect.preamble += std::to_string(constant.initializer_value.as_float[i]);
					break;
				}

				if (i + 1 < constant.type.components())
					effect.preamble += ", ";
			}

			effect.preamble += '\n';
		}
	}

	// Guard access to shared variables
	const std::lock_guard<std::mutex> lock(_reload_mutex);

	effect.index = out_id = _loaded_effects.size();
	effect.storage_offset = _uniform_data_storage.size();

	for (const reshadefx::uniform_info &info : effect.module.uniforms)
	{
		uniform &variable = _uniforms.emplace_back(info);
		variable.effect_index = effect.index;

		variable.storage_offset = effect.storage_offset + variable.offset;
		// Create space for the new variable in the storage area and fill it with the initializer value
		_uniform_data_storage.resize(variable.storage_offset + variable.size);

		// Copy initial data into uniform storage area
		reset_uniform_value(variable);

		const std::string_view special = variable.annotation_as_string("source");
		if (special.empty()) /* Ignore if annotation is missing */;
		else if (special == "frametime")
			variable.special = special_uniform::frame_time;
		else if (special == "framecount")
			variable.special = special_uniform::frame_count;
		else if (special == "random")
			variable.special = special_uniform::random;
		else if (special == "pingpong")
			variable.special = special_uniform::ping_pong;
		else if (special == "date")
			variable.special = special_uniform::date;
		else if (special == "timer")
			variable.special = special_uniform::timer;
		else if (special == "key")
			variable.special = special_uniform::key;
		else if (special == "mousepoint")
			variable.special = special_uniform::mouse_point;
		else if (special == "mousedelta")
			variable.special = special_uniform::mouse_delta;
		else if (special == "mousebutton")
			variable.special = special_uniform::mouse_button;
	}

	effect.storage_size = (_uniform_data_storage.size() - effect.storage_offset + 15) & ~15;
	_uniform_data_storage.resize(effect.storage_offset + effect.storage_size);

	for (const reshadefx::texture_info &info : effect.module.textures)
	{
		// Try to share textures with the same name across effects
		if (const auto existing_texture = std::find_if(_textures.begin(), _textures.end(),
			[&info](const auto &item) { return item.unique_name == info.unique_name; });
			existing_texture != _textures.end())
		{
			if (info.semantic.empty() && (
				existing_texture->width != info.width ||
				existing_texture->height != info.height ||
				existing_texture->levels != info.levels ||
				existing_texture->format != info.format))
			{
				effect.errors += "warning: " + info.unique_name + ": another effect already created a texture with the same name but different dimensions; textures are shared across all effects, so either rename the variable or adjust the dimensions so they match\n";
			}

			existing_texture->shared = true;
			continue;
		}

		texture &texture = _textures.emplace_back(info);
		texture.effect_index = effect.index;

		if (info.semantic == "COLOR")
			texture.impl_reference = texture_reference::back_buffer;
		else if (info.semantic == "DEPTH")
			texture.impl_reference = texture_reference::depth_buffer;
		else if (!info.semantic.empty())
			effect.errors += "warning: " + info.unique_name + ": unknown semantic '" + info.semantic + "'\n";
	}

	_loaded_effects.push_back(effect); // The 'enable_technique' call below needs to access this, so append the effect now

	for (const reshadefx::technique_info &info : effect.module.techniques)
	{
		technique &technique = _techniques.emplace_back(info);
		technique.effect_index = effect.index;

		technique.hidden = technique.annotation_as_int("hidden") != 0;
		technique.timeout = technique.annotation_as_int("timeout");
		technique.timeleft = technique.timeout;
		technique.toggle_key_data[0] = technique.annotation_as_int("toggle");
		technique.toggle_key_data[1] = technique.annotation_as_int("togglectrl");
		technique.toggle_key_data[2] = technique.annotation_as_int("toggleshift");
		technique.toggle_key_data[3] = technique.annotation_as_int("togglealt");

		if (technique.annotation_as_int("enabled"))
			enable_technique(technique);
	}

	if (effect.compile_sucess)
		if (effect.errors.empty())
			LOG(INFO) << "Successfully loaded " << path << '.';
		else
			LOG(WARN) << "Successfully loaded " << path << " with warnings:\n" << effect.errors;

	_reload_remaining_effects--;
	_last_reload_successful &= effect.compile_sucess;
}
void reshade::runtime::load_effects()
{
	// Clear out any previous effects
	unload_effects();

	_last_reload_successful = true;

	// Reload preprocessor definitions from current preset
	if (!_current_preset_path.empty())
	{
		_preset_preprocessor_definitions.clear();
		const ini_file preset(_current_preset_path);
		preset.get("", "PreprocessorDefinitions", _preset_preprocessor_definitions);
	}

	// Build a list of effect files by walking through the effect search paths
	const std::vector<std::filesystem::path> effect_files =
		find_files(_effect_search_paths, { ".fx" });

	_reload_total_effects = effect_files.size();
	_reload_remaining_effects = _reload_total_effects;

	// Now that we have a list of files, load them in parallel
	for (const std::filesystem::path &file : effect_files)
		_worker_threads.emplace_back([this, file]() { size_t id; load_effect(file, id); }); // Keep track of the spawned threads, so the runtime cannot be destroyed while they are still running
}
void reshade::runtime::load_textures()
{
	LOG(INFO) << "Loading image files for textures ...";

	for (texture &texture : _textures)
	{
		if (texture.impl == nullptr || texture.impl_reference != texture_reference::none)
			continue; // Ignore textures that are not created yet and those that are handled in the runtime implementation

		std::filesystem::path source_path = std::filesystem::u8path(
			texture.annotation_as_string("source"));
		// Ignore textures that have no image file attached to them (e.g. plain render targets)
		if (source_path.empty())
			continue;

		struct _stat64 st {};
		// Search for image file using the provided search paths unless the path provided is already absolute
		if (!find_file(_texture_search_paths, source_path) || _wstati64(source_path.wstring().c_str(), &st) != 0)
		{
			LOG(ERROR) << "> Source " << source_path << " for texture '" << texture.unique_name << "' could not be found in any of the texture search paths.";
			continue;
		}

		unsigned char *filedata = nullptr;
		int width = 0, height = 0, channels = 0;

		if (FILE *file; _wfopen_s(&file, source_path.wstring().c_str(), L"rb") == 0)
		{
			std::vector<uint8_t> filebuffer(static_cast<size_t>(st.st_size));
			fread(filebuffer.data(), 1, filebuffer.size(), file);
			fclose(file);

			if (stbi_dds_test_memory(filebuffer.data(), filebuffer.size()))
				filedata = stbi_dds_load_from_memory(filebuffer.data(), filebuffer.size(), &width, &height, &channels, STBI_rgb_alpha);
			else
				filedata = stbi_load_from_memory(filebuffer.data(), filebuffer.size(), &width, &height, &channels, STBI_rgb_alpha);
		}

		if (filedata == nullptr)
		{
			LOG(ERROR) << "> Source " << source_path << " for texture '" << texture.unique_name << "' could not be loaded! Make sure it is of a compatible file format.";
			continue;
		}

		if (texture.width != uint32_t(width) || texture.height != uint32_t(height))
		{
			LOG(INFO) << "> Resizing image data for texture '" << texture.unique_name << "' from " << width << "x" << height << " to " << texture.width << "x" << texture.height << " ...";

			std::vector<uint8_t> resized(texture.width * texture.height * 4);
			stbir_resize_uint8(filedata, width, height, 0, resized.data(), texture.width, texture.height, 0, 4);
			upload_texture(texture, resized.data());
		}
		else
		{
			upload_texture(texture, filedata);
		}

		stbi_image_free(filedata);
	}

	_textures_loaded = true;
}

void reshade::runtime::unload_effect(size_t id)
{
	_uniforms.erase(std::remove_if(_uniforms.begin(), _uniforms.end(),
		[id](const auto &it) { return it.effect_index == id; }), _uniforms.end());
	_textures.erase(std::remove_if(_textures.begin(), _textures.end(),
		[id](const auto &it) { return it.effect_index == id; }), _textures.end());
	_techniques.erase(std::remove_if(_techniques.begin(), _techniques.end(),
		[id](const auto &it) { return it.effect_index == id; }), _techniques.end());

	_loaded_effects[id].source_file.clear();

#if RESHADE_GUI
	// Remove all texture preview windows since some may no longer be valid
	_texture_previews.clear();
#endif
}
void reshade::runtime::unload_effects()
{
	// Make sure no threads are still accessing effect data
	for (std::thread &thread : _worker_threads)
		thread.join();
	_worker_threads.clear();

	_uniforms.clear();
	_textures.clear();
	_techniques.clear();

	_loaded_effects.clear();
	_uniform_data_storage.clear();

	_textures_loaded = false;

#if RESHADE_GUI
	// Remove all texture preview windows since those textures were deleted above
	_texture_previews.clear();
#endif
}

void reshade::runtime::update_and_render_effects()
{
	// Delay first load to the first render call to avoid loading while the application is still initializing
	if (_framecount == 0 && !_no_reload_on_init)
		load_effects();

	if (_reload_remaining_effects == 0)
	{
		// Finished loading effects, so apply preset to figure out which ones need compiling
		load_current_preset();

		_last_reload_time = std::chrono::high_resolution_clock::now();
		_reload_total_effects = 0;
		_reload_remaining_effects = std::numeric_limits<size_t>::max();
	}
	else if (_reload_remaining_effects != std::numeric_limits<size_t>::max())
	{
		return; // Cannot render while effects are still being loaded
	}
	else
	{
		if (!_reload_compile_queue.empty())
		{
			// Pop an effect from the queue
			size_t effect_index = _reload_compile_queue.back();
			_reload_compile_queue.pop_back();

			effect_data &effect = _loaded_effects[effect_index];

			// Create textures now, since they are referenced when building samplers in the 'compile_effect' call below
			bool success = true;
			for (texture &texture : _textures)
				if (texture.impl == nullptr && (texture.effect_index == effect_index || texture.shared))
					success &= init_texture(texture);

			// Compile the effect with the back-end implementation
			if (success && !compile_effect(effect))
			{
				success = false;

				// De-duplicate error lines (D3DCompiler sometimes repeats the same error multiple times)
				for (size_t cur_line_offset = 0, next_line_offset, end_offset;
					(next_line_offset = effect.errors.find('\n', cur_line_offset)) != std::string::npos && (end_offset = effect.errors.find('\n', next_line_offset + 1)) != std::string::npos; cur_line_offset = next_line_offset + 1)
				{
					const std::string_view cur_line(effect.errors.c_str() + cur_line_offset, next_line_offset - cur_line_offset);
					const std::string_view next_line(effect.errors.c_str() + next_line_offset + 1, end_offset - next_line_offset - 1);

					if (cur_line == next_line)
					{
						effect.errors.erase(next_line_offset, end_offset - next_line_offset);
						next_line_offset = cur_line_offset - 1;
					}
				}

				LOG(ERROR) << "Failed to compile " << effect.source_file << ":\n" << effect.errors;
			}

			if (!success)
			{
				// Destroy all textures belonging to this effect
				for (texture &texture : _textures)
					if (texture.effect_index == effect_index && !texture.shared)
						texture.impl.reset();
				// Disable all techniques belonging to this effect
				for (technique &technique : _techniques)
					if (technique.effect_index == effect_index)
						disable_technique(technique);

				effect.compile_sucess = false;
				_last_reload_successful = false;
			}

			// An effect has changed, need to reload textures
			_textures_loaded = false;
		}
		else if (!_textures_loaded)
		{
			load_textures();
		}
	}

	// Lock input so it cannot be modified by other threads while we are reading it here
	// TODO: This does not catch input happening between now and 'on_present'
	const auto input_lock = _input->lock();

	// Nothing to do here if effects are disabled globally
	if (!_effects_enabled)
		return;

	// Update special uniform variables
	for (uniform &variable : _uniforms)
	{
		switch (variable.special)
		{
		case special_uniform::frame_time:
			set_uniform_value(variable, _last_frame_duration.count() * 1e-6f, 0.0f, 0.0f, 0.0f);
			break;
		case special_uniform::frame_count:
			if (variable.type.is_boolean())
				set_uniform_value(variable, (_framecount % 2) == 0);
			else
				set_uniform_value(variable, static_cast<unsigned int>(_framecount % UINT_MAX));
			break;
		case special_uniform::random: {
			const int min = variable.annotation_as_int("min");
			const int max = variable.annotation_as_int("max");
			set_uniform_value(variable, min + (std::rand() % (max - min + 1)));
			break; }
		case special_uniform::ping_pong: {
			const float min = variable.annotation_as_float("min");
			const float max = variable.annotation_as_float("max");
			const float step_min = variable.annotation_as_float("step", 0);
			const float step_max = variable.annotation_as_float("step", 1);
			float increment = step_max == 0 ? step_min : (step_min + std::fmodf(static_cast<float>(std::rand()), step_max - step_min + 1));
			const float smoothing = variable.annotation_as_float("smoothing");

			float value[2] = { 0, 0 };
			get_uniform_value(variable, value, 2);
			if (value[1] >= 0)
			{
				increment = std::max(increment - std::max(0.0f, smoothing - (max - value[0])), 0.05f);
				increment *= _last_frame_duration.count() * 1e-9f;

				if ((value[0] += increment) >= max)
					value[0] = max, value[1] = -1;
			}
			else
			{
				increment = std::max(increment - std::max(0.0f, smoothing - (value[0] - min)), 0.05f);
				increment *= _last_frame_duration.count() * 1e-9f;

				if ((value[0] -= increment) <= min)
					value[0] = min, value[1] = +1;
			}
			set_uniform_value(variable, value, 2);
			break; }
		case special_uniform::date:
			set_uniform_value(variable, _date, 4);
			break;
		case special_uniform::timer:
			set_uniform_value(variable, std::chrono::duration_cast<std::chrono::nanoseconds>(_last_present_time - _start_time).count() * 1e-6f);
			break;
		case special_uniform::key:
			if (const int keycode = variable.annotation_as_int("keycode");
				keycode > 7 && keycode < 256)
				if (const std::string_view mode = variable.annotation_as_string("mode");
					mode == "toggle" || variable.annotation_as_int("toggle")) {
					bool current_value = false;
					get_uniform_value(variable, &current_value, 1);
					if (_input->is_key_pressed(keycode))
						set_uniform_value(variable, !current_value);
				} else if (mode == "press")
					set_uniform_value(variable, _input->is_key_pressed(keycode));
				else
					set_uniform_value(variable, _input->is_key_down(keycode));
			break;
		case special_uniform::mouse_point:
			set_uniform_value(variable, _input->mouse_position_x(), _input->mouse_position_y());
			break;
		case special_uniform::mouse_delta:
			set_uniform_value(variable, _input->mouse_movement_delta_x(), _input->mouse_movement_delta_y());
			break;
		case special_uniform::mouse_button:
			if (const int keycode = variable.annotation_as_int("keycode");
				keycode >= 0 && keycode < 5)
				if (const std::string_view mode = variable.annotation_as_string("mode");
					mode == "toggle" || variable.annotation_as_int("toggle")) {
					bool current_value = false;
					get_uniform_value(variable, &current_value, 1);
					if (_input->is_mouse_button_pressed(keycode))
						set_uniform_value(variable, !current_value);
				} else if (mode == "press")
					set_uniform_value(variable, _input->is_mouse_button_pressed(keycode));
				else
					set_uniform_value(variable, _input->is_mouse_button_down(keycode));
			break;
		}
	}

	// Render all enabled techniques
	for (technique &technique : _techniques)
	{
		if (technique.timeleft > 0)
		{
			technique.timeleft -= std::chrono::duration_cast<std::chrono::milliseconds>(_last_frame_duration).count();
			if (technique.timeleft <= 0)
				disable_technique(technique);
		}
		else if (!_ignore_shortcuts && (_input->is_key_pressed(technique.toggle_key_data) ||
			(technique.toggle_key_data[0] >= 0x01 && technique.toggle_key_data[0] <= 0x06 && _input->is_mouse_button_pressed(technique.toggle_key_data[0] - 1))))
		{
			if (!technique.enabled)
				enable_technique(technique);
			else
				disable_technique(technique);
		}

		if (technique.impl == nullptr || !technique.enabled)
			continue; // Ignore techniques that are not fully loaded or currently disabled

		const auto time_technique_started = std::chrono::high_resolution_clock::now();
		render_technique(technique);
		const auto time_technique_finished = std::chrono::high_resolution_clock::now();

		technique.average_cpu_duration.append(std::chrono::duration_cast<std::chrono::nanoseconds>(time_technique_finished - time_technique_started).count());
	}
}

void reshade::runtime::enable_technique(technique &technique)
{
	if (!_loaded_effects[technique.effect_index].compile_sucess)
		return; // Cannot enable techniques that failed to compile

	const bool status_changed = !technique.enabled;
	technique.enabled = true;
	technique.timeleft = technique.timeout;

	// Queue effect file for compilation if it was not fully loaded yet
	if (technique.impl == nullptr && // Avoid adding the same effect multiple times to the queue if it contains multiple techniques that were enabled simultaneously
		std::find(_reload_compile_queue.begin(), _reload_compile_queue.end(), technique.effect_index) == _reload_compile_queue.end())
	{
		_reload_total_effects++;
		_reload_compile_queue.push_back(technique.effect_index);
	}

	if (status_changed) // Increase rendering reference count
		_loaded_effects[technique.effect_index].rendering++;
}
void reshade::runtime::disable_technique(technique &technique)
{
	const bool status_changed =  technique.enabled;
	technique.enabled = false;
	technique.timeleft = 0;
	technique.average_cpu_duration.clear();
	technique.average_gpu_duration.clear();

	if (status_changed) // Decrease rendering reference count
		_loaded_effects[technique.effect_index].rendering--;
}

void reshade::runtime::subscribe_to_load_config(std::function<void(const ini_file &)> function)
{
	_load_config_callables.push_back(function);

	const ini_file config(_configuration_path);
	function(config);
}
void reshade::runtime::subscribe_to_save_config(std::function<void(ini_file &)> function)
{
	_save_config_callables.push_back(function);

	ini_file config(_configuration_path);
	function(config);
}

void reshade::runtime::load_config()
{
	const ini_file config(_configuration_path);
	std::filesystem::path current_preset_path;

	config.get("INPUT", "KeyScreenshot", _screenshot_key_data);
	config.get("INPUT", "KeyReload", _reload_key_data);
	config.get("INPUT", "KeyEffects", _effects_key_data);

	config.get("GENERAL", "PerformanceMode", _performance_mode);
	config.get("GENERAL", "EffectSearchPaths", _effect_search_paths);
	config.get("GENERAL", "TextureSearchPaths", _texture_search_paths);
	config.get("GENERAL", "PreprocessorDefinitions", _global_preprocessor_definitions);
	config.get("GENERAL", "CurrentPresetPath", current_preset_path);
	config.get("GENERAL", "ScreenshotPath", _screenshot_path);
	config.get("GENERAL", "ScreenshotFormat", _screenshot_format);
	config.get("GENERAL", "ScreenshotIncludePreset", _screenshot_include_preset);
	config.get("GENERAL", "NoReloadOnInit", _no_reload_on_init);

	set_current_preset(current_preset_path);

	for (const auto &callback : _load_config_callables)
		callback(config);
}
void reshade::runtime::save_config() const
{
	save_config(_configuration_path);
}
void reshade::runtime::save_config(const std::filesystem::path &path) const
{
	ini_file config(_configuration_path, path);

	config.set("INPUT", "KeyScreenshot", _screenshot_key_data);
	config.set("INPUT", "KeyReload", _reload_key_data);
	config.set("INPUT", "KeyEffects", _effects_key_data);

	config.set("GENERAL", "PerformanceMode", _performance_mode);
	config.set("GENERAL", "EffectSearchPaths", _effect_search_paths);
	config.set("GENERAL", "TextureSearchPaths", _texture_search_paths);
	config.set("GENERAL", "PreprocessorDefinitions", _global_preprocessor_definitions);
	config.set("GENERAL", "CurrentPresetPath", _current_preset_path);
	config.set("GENERAL", "ScreenshotPath", _screenshot_path);
	config.set("GENERAL", "ScreenshotFormat", _screenshot_format);
	config.set("GENERAL", "ScreenshotIncludePreset", _screenshot_include_preset);
	config.set("GENERAL", "NoReloadOnInit", _no_reload_on_init);

	for (const auto &callback : _save_config_callables)
		callback(config);
}

void reshade::runtime::load_preset(const std::filesystem::path &path)
{
	const ini_file preset(path);

	std::vector<std::string> technique_list;
	preset.get("", "Techniques", technique_list);
	std::vector<std::string> technique_sorting_list;
	preset.get("", "TechniqueSorting", technique_sorting_list);
	std::vector<std::string> preset_preprocessor_definitions;
	preset.get("", "PreprocessorDefinitions", preset_preprocessor_definitions);

	// Recompile effects if preprocessor definitions have changed or running in performance mode (in which case all preset values are compile-time constants)
	if (_reload_remaining_effects != 0 && // ... unless this is the 'load_current_preset' call in 'update_and_render_effects'
		(_performance_mode || preset_preprocessor_definitions != _preset_preprocessor_definitions))
	{
		assert(path == _current_preset_path);
		_preset_preprocessor_definitions = preset_preprocessor_definitions;
		load_effects();
		return; // Preset values are loaded in 'update_and_render_effects' during effect loading
	}

	// Reorder techniques
	if (technique_sorting_list.empty())
		technique_sorting_list = technique_list;

	std::sort(_techniques.begin(), _techniques.end(),
		[&technique_sorting_list](const auto &lhs, const auto &rhs) {
			return (std::find(technique_sorting_list.begin(), technique_sorting_list.end(), lhs.name) - technique_sorting_list.begin()) <
			       (std::find(technique_sorting_list.begin(), technique_sorting_list.end(), rhs.name) - technique_sorting_list.begin());
		});

	for (uniform &variable : _uniforms)
	{
		reshadefx::constant values;

		switch (variable.type.base)
		{
		case reshadefx::type::t_int:
			get_uniform_value(variable, values.as_int, 16);
			preset.get(_loaded_effects[variable.effect_index].source_file.filename().u8string(), variable.name, values.as_int);
			set_uniform_value(variable, values.as_int, 16);
			break;
		case reshadefx::type::t_bool:
		case reshadefx::type::t_uint:
			get_uniform_value(variable, values.as_uint, 16);
			preset.get(_loaded_effects[variable.effect_index].source_file.filename().u8string(), variable.name, values.as_uint);
			set_uniform_value(variable, values.as_uint, 16);
			break;
		case reshadefx::type::t_float:
			get_uniform_value(variable, values.as_float, 16);
			preset.get(_loaded_effects[variable.effect_index].source_file.filename().u8string(), variable.name, values.as_float);
			set_uniform_value(variable, values.as_float, 16);
			break;
		}
	}

	for (technique &technique : _techniques)
	{
		// Ignore preset if "enabled" annotation is set
		if (technique.annotation_as_int("enabled")
			|| std::find(technique_list.begin(), technique_list.end(), technique.name) != technique_list.end())
			enable_technique(technique);
		else
			disable_technique(technique);

		// Reset toggle key first, since it may not exist in the preset
		std::fill_n(technique.toggle_key_data, _countof(technique.toggle_key_data), 0);
		preset.get("", "Key" + technique.name, technique.toggle_key_data);
	}
}
void reshade::runtime::load_current_preset()
{
	load_preset(_current_preset_path);
}
void reshade::runtime::save_preset(const std::filesystem::path &path) const
{
	ini_file preset(path);

	std::vector<size_t> effect_list;
	std::vector<std::string> technique_list;
	std::vector<std::string> technique_sorting_list;

	for (const technique &technique : _techniques)
	{
		if (technique.enabled)
			technique_list.push_back(technique.name);
		if (technique.enabled || technique.toggle_key_data[0] != 0)
			effect_list.push_back(technique.effect_index);

		// Keep track of the order of all techniques and not just the enabled ones
		technique_sorting_list.push_back(technique.name);

		if (technique.toggle_key_data[0] != 0)
			preset.set("", "Key" + technique.name, technique.toggle_key_data);
		else if (int value = 0; preset.get("", "Key" + technique.name, value), value != 0)
			preset.set("", "Key" + technique.name, 0); // Clear toggle key data
	}

	preset.set("", "Techniques", std::move(technique_list));
	preset.set("", "TechniqueSorting", std::move(technique_sorting_list));
	preset.set("", "PreprocessorDefinitions", _preset_preprocessor_definitions);

	// TODO: Do we want to save spec constants here too? The preset will be rather empty in performance mode otherwise.
	for (const uniform &variable : _uniforms)
	{
		if (variable.special != special_uniform::none
			|| std::find(effect_list.begin(), effect_list.end(), variable.effect_index) == effect_list.end())
			continue;

		assert(variable.type.components() < 16);

		reshadefx::constant values;

		switch (variable.type.base)
		{
		case reshadefx::type::t_int:
			get_uniform_value(variable, values.as_int, 16);
			preset.set(_loaded_effects[variable.effect_index].source_file.filename().u8string(), variable.name, variant(values.as_int, variable.type.components()));
			break;
		case reshadefx::type::t_bool:
		case reshadefx::type::t_uint:
			get_uniform_value(variable, values.as_uint, 16);
			preset.set(_loaded_effects[variable.effect_index].source_file.filename().u8string(), variable.name, variant(values.as_uint, variable.type.components()));
			break;
		case reshadefx::type::t_float:
			get_uniform_value(variable, values.as_float, 16);
			preset.set(_loaded_effects[variable.effect_index].source_file.filename().u8string(), variable.name, variant(values.as_float, variable.type.components()));
			break;
		}
	}
}
void reshade::runtime::save_current_preset() const
{
	save_preset(_current_preset_path);
}

void reshade::runtime::save_screenshot()
{
	std::vector<uint8_t> data(_width * _height * 4);
	capture_screenshot(data.data());

	const int hour = _date[3] / 3600;
	const int minute = (_date[3] - hour * 3600) / 60;
	const int seconds = _date[3] - hour * 3600 - minute * 60;

	char filename[21];
	sprintf_s(filename, " %.4d-%.2d-%.2d %.2d-%.2d-%.2d", _date[0], _date[1], _date[2], hour, minute, seconds);

	const std::wstring least = (_screenshot_path.is_relative() ? g_target_executable_path.parent_path() / _screenshot_path : _screenshot_path) / g_target_executable_path.stem().concat(filename);
	const std::wstring screenshot_path = least + (_screenshot_format == 0 ? L".bmp" : L".png");

	LOG(INFO) << "Saving screenshot to " << screenshot_path << " ...";

	_screenshot_save_success = false; // Default to a save failure unless it is reported to succeed below

	if (FILE *file; _wfopen_s(&file, screenshot_path.c_str(), L"wb") == 0)
	{
		const auto write_callback = [](void *context, void *data, int size) {
			fwrite(data, 1, size, static_cast<FILE *>(context));
		};

		switch (_screenshot_format)
		{
		case 0:
			_screenshot_save_success = stbi_write_bmp_to_func(write_callback, file, _width, _height, 4, data.data()) != 0;
			break;
		case 1:
			_screenshot_save_success = stbi_write_png_to_func(write_callback, file, _width, _height, 4, data.data(), 0) != 0;
			break;
		}

		fclose(file);
	}

	_last_screenshot_file = screenshot_path;
	_last_screenshot_time = std::chrono::high_resolution_clock::now();

	if (!_screenshot_save_success)
	{
		LOG(ERROR) << "Failed to write screenshot to " << screenshot_path << '!';
	}
	else if (_screenshot_include_preset)
	{
		save_preset(least + L" Preset.ini");
		save_config(least + L" Settings.ini");
	}
}

void reshade::runtime::get_uniform_value(const uniform &variable, uint8_t *data, size_t size) const
{
	assert(data != nullptr);

	size = std::min(size, size_t(variable.size));

	assert(variable.storage_offset + size <= _uniform_data_storage.size());

	std::memcpy(data, &_uniform_data_storage[variable.storage_offset], size);
}
void reshade::runtime::get_uniform_value(const uniform &variable, bool *values, size_t count) const
{
	count = std::min(count, size_t(variable.size / 4));

	assert(values != nullptr);

	const auto data = static_cast<uint8_t *>(alloca(variable.size));
	get_uniform_value(variable, data, variable.size);

	for (size_t i = 0; i < count; i++)
		values[i] = reinterpret_cast<const uint32_t *>(data)[i] != 0;
}
void reshade::runtime::get_uniform_value(const uniform &variable, int32_t *values, size_t count) const
{
	if (!variable.type.is_floating_point() && _renderer_id != 0x9000)
	{
		get_uniform_value(variable, reinterpret_cast<uint8_t *>(values), count * sizeof(int32_t));
		return;
	}

	count = std::min(count, variable.size / sizeof(float));

	assert(values != nullptr);

	const auto data = static_cast<uint8_t *>(alloca(variable.size));
	get_uniform_value(variable, data, variable.size);

	for (size_t i = 0; i < count; i++)
		values[i] = static_cast<int32_t>(reinterpret_cast<const float *>(data)[i]);
}
void reshade::runtime::get_uniform_value(const uniform &variable, uint32_t *values, size_t count) const
{
	get_uniform_value(variable, reinterpret_cast<int32_t *>(values), count);
}
void reshade::runtime::get_uniform_value(const uniform &variable, float *values, size_t count) const
{
	if (variable.type.is_floating_point() || _renderer_id == 0x9000)
	{
		get_uniform_value(variable, reinterpret_cast<uint8_t *>(values), count * sizeof(float));
		return;
	}

	count = std::min(count, variable.size / sizeof(int32_t));

	assert(values != nullptr);

	const auto data = static_cast<uint8_t *>(alloca(variable.size));
	get_uniform_value(variable, data, variable.size);

	for (size_t i = 0; i < count; ++i)
		if (variable.type.is_signed())
			values[i] = static_cast<float>(reinterpret_cast<const int32_t *>(data)[i]);
		else
			values[i] = static_cast<float>(reinterpret_cast<const uint32_t *>(data)[i]);
}
void reshade::runtime::set_uniform_value(uniform &variable, const uint8_t *data, size_t size)
{
	assert(data != nullptr);

	size = std::min(size, size_t(variable.size));

	assert(variable.storage_offset + size <= _uniform_data_storage.size());

	std::memcpy(&_uniform_data_storage[variable.storage_offset], data, size);
}
void reshade::runtime::set_uniform_value(uniform &variable, const bool *values, size_t count)
{
	const auto data = static_cast<uint8_t *>(alloca(count * 4));
	switch (_renderer_id != 0x9000 ? variable.type.base : reshadefx::type::t_float)
	{
	case reshadefx::type::t_bool:
		for (size_t i = 0; i < count; ++i)
			reinterpret_cast<int32_t *>(data)[i] = values[i] ? -1 : 0;
		break;
	case reshadefx::type::t_int:
	case reshadefx::type::t_uint:
		for (size_t i = 0; i < count; ++i)
			reinterpret_cast<int32_t *>(data)[i] = values[i] ? 1 : 0;
		break;
	case reshadefx::type::t_float:
		for (size_t i = 0; i < count; ++i)
			reinterpret_cast<float *>(data)[i] = values[i] ? 1.0f : 0.0f;
		break;
	}

	set_uniform_value(variable, data, count * 4);
}
void reshade::runtime::set_uniform_value(uniform &variable, const int32_t *values, size_t count)
{
	if (!variable.type.is_floating_point() && _renderer_id != 0x9000)
	{
		set_uniform_value(variable, reinterpret_cast<const uint8_t *>(values), count * sizeof(int));
		return;
	}

	const auto data = static_cast<float *>(alloca(count * sizeof(float)));
	for (size_t i = 0; i < count; ++i)
		data[i] = static_cast<float>(values[i]);

	set_uniform_value(variable, reinterpret_cast<const uint8_t *>(data), count * sizeof(float));
}
void reshade::runtime::set_uniform_value(uniform &variable, const uint32_t *values, size_t count)
{
	if (!variable.type.is_floating_point() && _renderer_id != 0x9000)
	{
		set_uniform_value(variable, reinterpret_cast<const uint8_t *>(values), count * sizeof(int));
		return;
	}

	const auto data = static_cast<float *>(alloca(count * sizeof(float)));
	for (size_t i = 0; i < count; ++i)
		data[i] = static_cast<float>(values[i]);

	set_uniform_value(variable, reinterpret_cast<const uint8_t *>(data), count * sizeof(float));
}
void reshade::runtime::set_uniform_value(uniform &variable, const float *values, size_t count)
{
	if (variable.type.is_floating_point() || _renderer_id == 0x9000)
	{
		set_uniform_value(variable, reinterpret_cast<const uint8_t *>(values), count * sizeof(float));
		return;
	}

	const auto data = static_cast<int32_t *>(alloca(count * sizeof(int32_t)));
	for (size_t i = 0; i < count; ++i)
		data[i] = static_cast<int32_t>(values[i]);

	set_uniform_value(variable, reinterpret_cast<const uint8_t *>(data), count * sizeof(int32_t));
}

void reshade::runtime::reset_uniform_value(uniform &variable)
{
	if (!variable.has_initializer_value)
	{
		memset(_uniform_data_storage.data() + variable.storage_offset, 0, variable.size);
		return;
	}

	if (_renderer_id != 0x9000)
	{
		memcpy(_uniform_data_storage.data() + variable.storage_offset, variable.initializer_value.as_uint, variable.size);
		return;
	}

	// Force all uniforms to floating-point in D3D9
	for (size_t i = 0; i < variable.size / sizeof(float); i++)
	{
		switch (variable.type.base)
		{
		case reshadefx::type::t_int:
			reinterpret_cast<float *>(_uniform_data_storage.data() + variable.storage_offset)[i] = static_cast<float>(variable.initializer_value.as_int[i]);
			break;
		case reshadefx::type::t_bool:
		case reshadefx::type::t_uint:
			reinterpret_cast<float *>(_uniform_data_storage.data() + variable.storage_offset)[i] = static_cast<float>(variable.initializer_value.as_uint[i]);
			break;
		case reshadefx::type::t_float:
			reinterpret_cast<float *>(_uniform_data_storage.data() + variable.storage_offset)[i] = variable.initializer_value.as_float[i];
			break;
		}
	}
}

void reshade::runtime::set_current_preset()
{
	set_current_preset(_current_browse_path);
}
void reshade::runtime::set_current_preset(std::filesystem::path path)
{
	std::error_code ec;
	std::filesystem::path reshade_container_path = g_reshade_dll_path.parent_path();

	enum class path_state { invalid, valid };
	path_state path_state = path_state::invalid;

	if (path.has_filename())
		if (const std::wstring extension(path.extension()); extension == L".ini" || extension == L".txt")
			if (!std::filesystem::exists(reshade_container_path / path, ec))
				path_state = path_state::valid;
			else if (const reshade::ini_file preset(reshade_container_path / path); preset.has("", "TechniqueSorting"))
				path_state = path_state::valid;

	// Select a default preset file if none exists yet or not own
	if (path_state == path_state::invalid)
		path = "DefaultPreset.ini";
	else if (const std::filesystem::path preset_canonical_path = std::filesystem::weakly_canonical(reshade_container_path / path, ec);
		std::equal(reshade_container_path.begin(), reshade_container_path.end(), preset_canonical_path.begin()))
		path = preset_canonical_path.lexically_proximate(reshade_container_path);
	else if (const std::filesystem::path preset_absolute_path = std::filesystem::absolute(reshade_container_path / path, ec);
		std::equal(reshade_container_path.begin(), reshade_container_path.end(), preset_absolute_path.begin()))
		path = preset_absolute_path.lexically_proximate(reshade_container_path);
	else
		path = preset_canonical_path;

	_current_browse_path = path;
	_current_preset_path = reshade_container_path / path;
}
