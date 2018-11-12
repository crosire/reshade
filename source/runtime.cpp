/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "log.hpp"
#include "version.h"
#include "runtime.hpp"
#include "effect_parser.hpp"
#include "effect_preprocessor.hpp"
#include "input.hpp"
#include "ini_file.hpp"
#include <assert.h>
#include <thread>
#include <algorithm>
#include <unordered_set>
#include <imgui.h>
#include <imgui_internal.h>
#include <stb_image.h>
#include <stb_image_dds.h>
#include <stb_image_write.h>
#include <stb_image_resize.h>

namespace reshade
{
	runtime::runtime(uint32_t renderer) :
		_renderer_id(renderer),
		_start_time(std::chrono::high_resolution_clock::now()),
		_last_frame_duration(std::chrono::milliseconds(1)),
		_imgui_context(ImGui::CreateContext()),
		_effect_search_paths({ g_reshade_dll_path.parent_path() }),
		_texture_search_paths({ g_reshade_dll_path.parent_path() }),
		_preprocessor_definitions({
			"RESHADE_DEPTH_LINEARIZATION_FAR_PLANE=1000.0",
			"RESHADE_DEPTH_INPUT_IS_UPSIDE_DOWN=0",
			"RESHADE_DEPTH_INPUT_IS_REVERSED=1",
			"RESHADE_DEPTH_INPUT_IS_LOGARITHMIC=0" }),
		_menu_key_data(),
		_screenshot_key_data(),
		_reload_key_data(),
		_effects_key_data(),
		_screenshot_path(g_target_executable_path.parent_path()),
		_variable_editor_height(500)
	{
		_menu_key_data[0] = 0x71; // VK_F2
		_menu_key_data[2] = true; // VK_SHIFT
		_screenshot_key_data[0] = 0x2C; // VK_SNAPSHOT

		_configuration_path = g_reshade_dll_path;
		_configuration_path.replace_extension(".ini");
		if (std::error_code ec; !std::filesystem::exists(_configuration_path, ec))
			_configuration_path = g_reshade_dll_path.parent_path() / "ReShade.ini";

		_needs_update = check_for_update(_latest_version);

		auto &imgui_io = _imgui_context->IO;
		auto &imgui_style = _imgui_context->Style;
		imgui_io.IniFilename = nullptr;
		imgui_io.KeyMap[ImGuiKey_Tab] = 0x09; // VK_TAB
		imgui_io.KeyMap[ImGuiKey_LeftArrow] = 0x25; // VK_LEFT
		imgui_io.KeyMap[ImGuiKey_RightArrow] = 0x27; // VK_RIGHT
		imgui_io.KeyMap[ImGuiKey_UpArrow] = 0x26; // VK_UP
		imgui_io.KeyMap[ImGuiKey_DownArrow] = 0x28; // VK_DOWN
		imgui_io.KeyMap[ImGuiKey_PageUp] = 0x21; // VK_PRIOR
		imgui_io.KeyMap[ImGuiKey_PageDown] = 0x22; // VK_NEXT
		imgui_io.KeyMap[ImGuiKey_Home] = 0x24; // VK_HOME
		imgui_io.KeyMap[ImGuiKey_End] = 0x23; // VK_END
		imgui_io.KeyMap[ImGuiKey_Insert] = 0x2D; // VK_INSERT
		imgui_io.KeyMap[ImGuiKey_Delete] = 0x2E; // VK_DELETE
		imgui_io.KeyMap[ImGuiKey_Backspace] = 0x08; // VK_BACK
		imgui_io.KeyMap[ImGuiKey_Space] = 0x20; // VK_SPACE
		imgui_io.KeyMap[ImGuiKey_Enter] = 0x0D; // VK_RETURN
		imgui_io.KeyMap[ImGuiKey_Escape] = 0x1B; // VK_ESCAPE
		imgui_io.KeyMap[ImGuiKey_A] = 'A';
		imgui_io.KeyMap[ImGuiKey_C] = 'C';
		imgui_io.KeyMap[ImGuiKey_V] = 'V';
		imgui_io.KeyMap[ImGuiKey_X] = 'X';
		imgui_io.KeyMap[ImGuiKey_Y] = 'Y';
		imgui_io.KeyMap[ImGuiKey_Z] = 'Z';
		imgui_io.ConfigFlags = ImGuiConfigFlags_NavEnableKeyboard;
		imgui_style.WindowRounding = 0.0f;
		imgui_style.WindowBorderSize = 0.0f;
		imgui_style.ChildRounding = 0.0f;
		imgui_style.FrameRounding = 0.0f;
		imgui_style.ScrollbarRounding = 0.0f;
		imgui_style.GrabRounding = 0.0f;

		ImGui::SetCurrentContext(nullptr);

		imgui_io.Fonts->AddFontDefault();
		const auto font_path = g_windows_path / "Fonts" / "consolab.ttf";
		if (std::error_code ec; std::filesystem::exists(font_path, ec))
			imgui_io.Fonts->AddFontFromFileTTF(font_path.u8string().c_str(), 18.0f);
		else
			imgui_io.Fonts->AddFontDefault();

		load_config();

		subscribe_to_menu("Home", [this]() { draw_overlay_menu_home(); });
		subscribe_to_menu("Settings", [this]() { draw_overlay_menu_settings(); });
		subscribe_to_menu("Statistics", [this]() { draw_overlay_menu_statistics(); });
		subscribe_to_menu("Log", [this]() { draw_overlay_menu_log(); });
		subscribe_to_menu("About", [this]() { draw_overlay_menu_about(); });
	}
	runtime::~runtime()
	{
		ImGui::DestroyContext(_imgui_context);

		assert(!_is_initialized && _techniques.empty());
	}

	bool runtime::on_init()
	{
		// Finish initializing ImGui
		auto &imgui_io = _imgui_context->IO;
		imgui_io.DisplaySize.x = static_cast<float>(_width);
		imgui_io.DisplaySize.y = static_cast<float>(_height);
		imgui_io.Fonts->TexID = _imgui_font_atlas_texture.get();

		LOG(INFO) << "Recreated runtime environment on runtime " << this << ".";

		_is_initialized = true;
		_last_reload_time = std::chrono::high_resolution_clock::now();

		if (!_no_reload_on_init)
		{
			reload();
		}

		return true;
	}
	void runtime::on_reset()
	{
		on_reset_effect();

		if (!_is_initialized)
		{
			return;
		}

		// Reset ImGui settings
		auto &imgui_io = _imgui_context->IO;
		imgui_io.DisplaySize.x = 0;
		imgui_io.DisplaySize.y = 0;
		imgui_io.Fonts->TexID = nullptr;

		_imgui_font_atlas_texture.reset();

		LOG(INFO) << "Destroyed runtime environment on runtime " << this << ".";

		_width = _height = 0;
		_is_initialized = false;
	}
	void runtime::on_reset_effect()
	{
		_textures.clear();
		_uniforms.clear();
		_techniques.clear();
		_uniform_data_storage.clear();
	}
	void runtime::on_present()
	{
		// Get current time and date
		time_t t = std::time(nullptr); tm tm;
		localtime_s(&tm, &t);
		_date[0] = tm.tm_year + 1900;
		_date[1] = tm.tm_mon + 1;
		_date[2] = tm.tm_mday;
		_date[3] = tm.tm_hour * 3600 + tm.tm_min * 60 + tm.tm_sec;

		// Advance various statistics
		if (0 == _framecount++)
		{
			_last_present_time = std::chrono::high_resolution_clock::now();
		}

		_last_frame_duration = std::chrono::high_resolution_clock::now() - _last_present_time;
		_last_present_time += _last_frame_duration;

		if (_input->is_key_pressed(_reload_key_data[0], _reload_key_data[1] != 0, _reload_key_data[2] != 0, _reload_key_data[3] != 0))
		{
			reload();
		}

		// Create and save screenshot if associated shortcut is down
		if (!_screenshot_key_setting_active &&
			_input->is_key_pressed(_screenshot_key_data[0], _screenshot_key_data[1] != 0, _screenshot_key_data[2] != 0, _screenshot_key_data[3] != 0))
		{
			save_screenshot();
		}

		// Draw overlay
		draw_overlay();

		// Reset input status
		_input->next_frame();

		g_network_traffic = _drawcalls = _vertices = 0;

		if (_has_finished_reloading)
		{
			load_current_preset();
			load_textures();

			_has_finished_reloading = false;
		}
		else if (!_reload_queue.empty())
		{
			effect_data &effect = _loaded_effects[_reload_queue.back()];

			_reload_queue.pop_back();

			if (!load_effect(effect))
			{
				LOG(ERROR) << "Failed to compile " << effect.source_file << ":\n" << effect.errors;
				_last_reload_successful = false;
			}
		}
	}
	void runtime::on_present_effect()
	{
		if (!_toggle_key_setting_active &&
			_input->is_key_pressed(_effects_key_data[0], _effects_key_data[1] != 0, _effects_key_data[2] != 0, _effects_key_data[3] != 0))
			_effects_enabled = !_effects_enabled;

		if (!_effects_enabled)
			return;

		// Update all uniform variables
		for (auto &variable : _uniforms)
		{
			const auto it = variable.annotations.find("source");

			if (it == variable.annotations.end())
				continue;
			const auto &source = it->second.second.string_data;

			if (source == "frametime")
			{
				const float value = _last_frame_duration.count() * 1e-6f;
				set_uniform_value(variable, &value, 1);
			}
			else if (source == "framecount")
			{
				switch (variable.type.base)
				{
					case reshadefx::type::t_bool:
					{
						const bool even = (_framecount % 2) == 0;
						set_uniform_value(variable, &even, 1);
						break;
					}
					case reshadefx::type::t_int:
					case reshadefx::type::t_uint:
					{
						const unsigned int framecount = static_cast<unsigned int>(_framecount % UINT_MAX);
						set_uniform_value(variable, &framecount, 1);
						break;
					}
					case reshadefx::type::t_float:
					{
						const float framecount = static_cast<float>(_framecount % 16777216);
						set_uniform_value(variable, &framecount, 1);
						break;
					}
				}
			}
			else if (source == "pingpong")
			{
				float value[2] = { 0, 0 };
				get_uniform_value(variable, value, 2);

				const float min = variable.annotations["min"].second.as_float[0], max = variable.annotations["max"].second.as_float[0];
				const float step_min = variable.annotations["step"].second.as_float[0], step_max = variable.annotations["step"].second.as_float[1];
				float increment = step_max == 0 ? step_min : (step_min + std::fmodf(static_cast<float>(std::rand()), step_max - step_min + 1));
				const float smoothing = variable.annotations["smoothing"].second.as_float[0];

				if (value[1] >= 0)
				{
					increment = std::max(increment - std::max(0.0f, smoothing - (max - value[0])), 0.05f);
					increment *= _last_frame_duration.count() * 1e-9f;

					if ((value[0] += increment) >= max)
					{
						value[0] = max;
						value[1] = -1;
					}
				}
				else
				{
					increment = std::max(increment - std::max(0.0f, smoothing - (value[0] - min)), 0.05f);
					increment *= _last_frame_duration.count() * 1e-9f;

					if ((value[0] -= increment) <= min)
					{
						value[0] = min;
						value[1] = +1;
					}
				}

				set_uniform_value(variable, value, 2);
			}
			else if (source == "date")
			{
				set_uniform_value(variable, _date, 4);
			}
			else if (source == "timer")
			{
				const unsigned long long timer = std::chrono::duration_cast<std::chrono::nanoseconds>(_last_present_time - _start_time).count();

				switch (variable.type.base)
				{
					case reshadefx::type::t_bool:
					{
						const bool even = (timer % 2) == 0;
						set_uniform_value(variable, &even, 1);
						break;
					}
					case reshadefx::type::t_int:
					case reshadefx::type::t_uint:
					{
						const unsigned int timer_int = static_cast<unsigned int>(timer % UINT_MAX);
						set_uniform_value(variable, &timer_int, 1);
						break;
					}
					case reshadefx::type::t_float:
					{
						const float timer_float = std::fmod(static_cast<float>(timer * 1e-6f), 16777216.0f);
						set_uniform_value(variable, &timer_float, 1);
						break;
					}
				}
			}
			else if (source == "key")
			{
				const int key = variable.annotations["keycode"].second.as_int[0];

				if (key > 7 && key < 256)
				{
					const std::string mode = variable.annotations["mode"].second.string_data;

					if (mode == "toggle" || variable.annotations["toggle"].second.as_uint[0])
					{
						bool current = false;
						get_uniform_value(variable, &current, 1);

						if (_input->is_key_pressed(key))
						{
							current = !current;

							set_uniform_value(variable, &current, 1);
						}
					}
					else if (mode == "press")
					{
						const bool state = _input->is_key_pressed(key);

						set_uniform_value(variable, &state, 1);
					}
					else
					{
						const bool state = _input->is_key_down(key);

						set_uniform_value(variable, &state, 1);
					}
				}
			}
			else if (source == "mousepoint")
			{
				const float values[2] = { static_cast<float>(_input->mouse_position_x()), static_cast<float>(_input->mouse_position_y()) };

				set_uniform_value(variable, values, 2);
			}
			else if (source == "mousedelta")
			{
				const float values[2] = { static_cast<float>(_input->mouse_movement_delta_x()), static_cast<float>(_input->mouse_movement_delta_y()) };

				set_uniform_value(variable, values, 2);
			}
			else if (source == "mousebutton")
			{
				const int index = variable.annotations["keycode"].second.as_int[0];

				if (index >= 0 && index < 5)
				{
					const std::string mode = variable.annotations["mode"].second.string_data;

					if (mode == "toggle" || variable.annotations["toggle"].second.as_uint[0])
					{
						bool current = false;
						get_uniform_value(variable, &current, 1);

						if (_input->is_mouse_button_pressed(index))
						{
							current = !current;

							set_uniform_value(variable, &current, 1);
						}
					}
					else if (mode == "press")
					{
						const bool state = _input->is_mouse_button_pressed(index);

						set_uniform_value(variable, &state, 1);
					}
					else
					{
						const bool state = _input->is_mouse_button_down(index);

						set_uniform_value(variable, &state, 1);
					}
				}
			}
			else if (source == "random")
			{
				const int min = variable.annotations["min"].second.as_int[0], max = variable.annotations["max"].second.as_int[0];
				const int value = min + (std::rand() % (max - min + 1));

				set_uniform_value(variable, &value, 1);
			}
		}

		// Render all enabled techniques
		for (auto &technique : _techniques)
		{
			if (technique.impl == nullptr)
				continue;

			if (technique.timeleft > 0)
			{
				technique.timeleft -= static_cast<unsigned int>(std::chrono::duration_cast<std::chrono::milliseconds>(_last_frame_duration).count());

				if (technique.timeleft <= 0)
				{
					technique.enabled = false;
					technique.timeleft = 0;
					technique.average_cpu_duration.clear();
					technique.average_gpu_duration.clear();
				}
			}
			else if (!_toggle_key_setting_active &&
				_input->is_key_pressed(technique.toggle_key_data[0], technique.toggle_key_data[1] != 0, technique.toggle_key_data[2] != 0, technique.toggle_key_data[3] != 0) ||
				(technique.toggle_key_data[0] >= 0x01 && technique.toggle_key_data[0] <= 0x06 && _input->is_mouse_button_pressed(technique.toggle_key_data[0] - 1)))
			{
				technique.enabled = !technique.enabled;
				technique.timeleft = technique.enabled ? technique.timeout : 0;
			}

			if (!technique.enabled)
			{
				technique.average_cpu_duration.clear();
				technique.average_gpu_duration.clear();
				continue;
			}

			const auto time_technique_started = std::chrono::high_resolution_clock::now();

			render_technique(technique);

			const auto time_technique_finished = std::chrono::high_resolution_clock::now();

			technique.average_cpu_duration.append(std::chrono::duration_cast<std::chrono::nanoseconds>(time_technique_finished - time_technique_started).count());
		}
	}

	void runtime::reload()
	{
		on_reset_effect();

		_loaded_effects.clear();

		_last_reload_successful = true;
		_has_finished_reloading = false;

		std::vector<std::filesystem::path> files;

		for (const auto &search_path : _effect_search_paths)
		{
			std::error_code ec;
			for (const auto &entry : std::filesystem::directory_iterator(search_path, ec))
				if (entry.path().extension() == ".fx")
					files.push_back(entry.path());
		}

		_reload_remaining_effects = files.size();

		for (const auto &file : files)
			std::thread([this, file]() { load_effect(file); }).detach();
	}
	void runtime::load_effect(const std::filesystem::path &path)
	{
		std::string source_code, errors;
		reshadefx::module module;

		{ reshadefx::preprocessor pp;

			if (path.is_absolute())
				pp.add_include_path(path.parent_path());

			for (const auto &include_path : _effect_search_paths)
			{
				if (include_path.empty())
					continue; // Skip invalid paths

				pp.add_include_path(include_path);
			}

			pp.add_macro_definition("__RESHADE__", std::to_string(VERSION_MAJOR * 10000 + VERSION_MINOR * 100 + VERSION_REVISION));
			pp.add_macro_definition("__RESHADE_PERFORMANCE_MODE__", _performance_mode ? "1" : "0");
			pp.add_macro_definition("__VENDOR__", std::to_string(_vendor_id));
			pp.add_macro_definition("__DEVICE__", std::to_string(_device_id));
			pp.add_macro_definition("__RENDERER__", std::to_string(_renderer_id));
			pp.add_macro_definition("__APPLICATION__", std::to_string(std::hash<std::string>()(g_target_executable_path.stem().u8string())));
			pp.add_macro_definition("BUFFER_WIDTH", std::to_string(_width));
			pp.add_macro_definition("BUFFER_HEIGHT", std::to_string(_height));
			pp.add_macro_definition("BUFFER_RCP_WIDTH", std::to_string(1.0f / static_cast<float>(_width)));
			pp.add_macro_definition("BUFFER_RCP_HEIGHT", std::to_string(1.0f / static_cast<float>(_height)));

			for (const auto &definition : _preprocessor_definitions)
			{
				if (definition.empty())
					continue; // Skip invalid definitions

				const size_t equals_index = definition.find_first_of('=');
				if (equals_index != std::string::npos)
					pp.add_macro_definition(definition.substr(0, equals_index), definition.substr(equals_index + 1));
				else
					pp.add_macro_definition(definition);
			}

			// Pre-process the source file
			if (!pp.append_file(path))
			{
				LOG(ERROR) << "Failed to pre-process " << path << ":\n" << pp.errors();
				_last_reload_successful = false;
				return;
			}

			errors = std::move(pp.errors()); // Append any pre-processor warnings to the error list
			source_code = std::move(pp.output());
		}

		{ reshadefx::parser parser;

			unsigned shader_model;
			if (_renderer_id < 0xa000)
				shader_model = 30;
			else if (_renderer_id < 0xa100)
				shader_model = 40;
			else if (_renderer_id < 0xb000)
				shader_model = 41;
			else
				shader_model = 50;

			// Compile the pre-processed source code
			if (!parser.parse(source_code, _renderer_id & 0x10000 ? reshadefx::codegen::backend::glsl : reshadefx::codegen::backend::hlsl, shader_model, true, _performance_mode, module))
			{
				LOG(ERROR) << "Failed to compile " << path << ":\n" << parser.errors();
				_last_reload_successful = false;
				return;
			}

			errors += parser.errors();
		}

#if RESHADE_DUMP_NATIVE_SHADERS
		std::ofstream("ReShade-ShaderDump-" + path.stem().u8string() + ".hlsl", std::ios::trunc) << module.hlsl;
#endif

		// Fill all specialization constants with values from the current preset
		if (_performance_mode && _current_preset >= 0)
		{
			ini_file preset(_preset_files[_current_preset]);

			for (auto &constant : module.spec_constants)
			{
				switch (constant.type.base)
				{
				case reshadefx::type::t_int:
					preset.get(path.filename().u8string(), constant.name, constant.initializer_value.as_int);
					break;
				case reshadefx::type::t_uint:
					preset.get(path.filename().u8string(), constant.name, constant.initializer_value.as_uint);
					break;
				case reshadefx::type::t_float:
					preset.get(path.filename().u8string(), constant.name, constant.initializer_value.as_float);
					break;
				}
			}
		}

		std::lock_guard<std::mutex> lock(_reload_mutex);

		const size_t storage_base_offset = _uniform_data_storage.size();

		for (const reshadefx::uniform_info &info : module.uniforms)
		{
			uniform &uniform = _uniforms.emplace_back(info);
			uniform.effect_filename = path.filename().u8string();
			uniform.hidden = uniform.annotations["hidden"].second.as_uint[0];

			uniform.storage_offset = storage_base_offset + uniform.offset;

			// Create space for the new variable in the storage area and fill it with the initializer value
			_uniform_data_storage.resize(uniform.storage_offset + uniform.size);

			if (uniform.has_initializer_value)
				memcpy(_uniform_data_storage.data() + uniform.storage_offset, uniform.initializer_value.as_uint, uniform.size);
			else
				memset(_uniform_data_storage.data() + uniform.storage_offset, 0, uniform.size);
		}

		for (const reshadefx::texture_info &info : module.textures)
		{
			if (const texture *const existing_texture = find_texture(info.unique_name); existing_texture != nullptr)
			{
				if (info.semantic.empty() && (
					existing_texture->width != info.width ||
					existing_texture->height != info.height ||
					existing_texture->levels != info.levels ||
					existing_texture->format != info.format))
					errors += "warning: " + existing_texture->effect_filename + " already created a texture with the same name but different dimensions; textures are shared across all effects, so either rename the variable or adjust the dimensions so they match\n";
				continue;
			}

			if (!info.semantic.empty() && (info.semantic != "COLOR" && info.semantic != "DEPTH"))
				errors += "warning: " + info.unique_name + ": unknown semantic '" + info.semantic + "'\n";

			texture &texture = _textures.emplace_back(info);
			texture.effect_filename = path.filename().u8string();
		}

		for (const reshadefx::technique_info &info : module.techniques)
		{
			technique &technique = _techniques.emplace_back(info);
			technique.effect_filename = path.filename().u8string();
			technique.enabled = technique.annotations["enabled"].second.as_uint[0];
			technique.hidden = technique.annotations["hidden"].second.as_uint[0];
			technique.timeleft = technique.timeout = technique.annotations["timeout"].second.as_int[0];
			technique.toggle_key_data[0] = technique.annotations["toggle"].second.as_uint[0];
			technique.toggle_key_data[1] = technique.annotations["togglectrl"].second.as_uint[0] ? 1 : 0;
			technique.toggle_key_data[2] = technique.annotations["toggleshift"].second.as_uint[0] ? 1 : 0;
			technique.toggle_key_data[3] = technique.annotations["togglealt"].second.as_uint[0] ? 1 : 0;

			_technique_to_effect[info.name] = _loaded_effects.size();
		}

		if (errors.empty())
			LOG(INFO) << "Successfully compiled " << path << ".";
		else
			LOG(WARNING) << "Successfully compiled " << path << " with warnings:\n" << errors;

		effect_data effect;
		effect.errors = std::move(errors);
		effect.module = std::move(module);
		effect.source_file = path;
		effect.storage_offset = storage_base_offset;

		const auto roundto16 = [](size_t size) { return (size + 15) & ~15; };
		effect.storage_size = roundto16(_uniform_data_storage.size() - storage_base_offset);
		_uniform_data_storage.resize(effect.storage_offset + effect.storage_size);

		_loaded_effects.push_back(std::move(effect));

		if (--_reload_remaining_effects == 0)
		{
			_last_reload_time = std::chrono::high_resolution_clock::now();
			_has_finished_reloading = true;
		}
	}
	void runtime::load_textures()
	{
		LOG(INFO) << "Loading image files for textures ...";

		for (auto &texture : _textures)
		{
			if (texture.impl == nullptr)
				continue;
			if (texture.impl_reference != texture_reference::none)
				continue;

			const auto it = texture.annotations.find("source");

			if (it == texture.annotations.end())
				continue;

			std::error_code ec;
			std::filesystem::path path;
			for (const auto &search_path : _texture_search_paths)
				if (std::filesystem::exists(path = search_path / it->second.second.string_data, ec))
					break;

			if (!std::filesystem::exists(path, ec))
			{
				LOG(ERROR) << "> Source " << path << " for texture '" << texture.unique_name << "' could not be found.";
				continue;
			}

			FILE *file;
			unsigned char *filedata = nullptr;
			int width = 0, height = 0, channels = 0;
			bool success = false;

			if (_wfopen_s(&file, path.wstring().c_str(), L"rb") == 0)
			{
				if (stbi_dds_test_file(file))
				{
					filedata = stbi_dds_load_from_file(file, &width, &height, &channels, STBI_rgb_alpha);
				}
				else
				{
					filedata = stbi_load_from_file(file, &width, &height, &channels, STBI_rgb_alpha);
				}

				fclose(file);
			}

			if (filedata != nullptr)
			{
				if (texture.width != static_cast<unsigned int>(width) ||
					texture.height != static_cast<unsigned int>(height))
				{
					LOG(INFO) << "> Resizing image data for texture '" << texture.unique_name << "' from " << width << "x" << height << " to " << texture.width << "x" << texture.height << " ...";

					std::vector<uint8_t> resized(texture.width * texture.height * 4);
					stbir_resize_uint8(filedata, width, height, 0, resized.data(), texture.width, texture.height, 0, 4);
					success = update_texture(texture, resized.data());
				}
				else
				{
					success = update_texture(texture, filedata);
				}

				stbi_image_free(filedata);
			}

			if (!success)
			{
				LOG(ERROR) << "> Source " << path << " for texture '" << texture.unique_name << "' could not be loaded! Make sure it is of a compatible file format.";
				continue;
			}
		}
	}

	void runtime::activate_technique(const std::string &name)
	{
		if (std::find_if(_techniques.begin(), _techniques.end(),
			[&name](const auto &tech) { return tech.name == name && tech.impl != nullptr; }) != _techniques.end())
			return; // Effect file was already fully compiled, so there is nothing to do

		const auto it = _technique_to_effect.find(name);

		if (it == _technique_to_effect.end())
			return;

		_reload_queue.push_back(it->second);
	}
	void runtime::deactivate_technique(const std::string &name)
	{
	}

	void runtime::load_config()
	{
		const ini_file config(_configuration_path);

		config.get("INPUT", "KeyMenu", _menu_key_data);
		config.get("INPUT", "KeyScreenshot", _screenshot_key_data);
		config.get("INPUT", "KeyReload", _reload_key_data);
		config.get("INPUT", "KeyEffects", _effects_key_data);
		config.get("INPUT", "InputProcessing", _input_processing_mode);

		config.get("GENERAL", "PerformanceMode", _performance_mode);
		config.get("GENERAL", "EffectSearchPaths", _effect_search_paths);
		config.get("GENERAL", "TextureSearchPaths", _texture_search_paths);
		config.get("GENERAL", "PreprocessorDefinitions", _preprocessor_definitions);
		config.get("GENERAL", "PresetFiles", _preset_files);
		config.get("GENERAL", "CurrentPreset", _current_preset);
		config.get("GENERAL", "TutorialProgress", _tutorial_index);
		config.get("GENERAL", "ScreenshotPath", _screenshot_path);
		config.get("GENERAL", "ScreenshotFormat", _screenshot_format);
		config.get("GENERAL", "ScreenshotIncludePreset", _screenshot_include_preset);
		config.get("GENERAL", "ScreenshotIncludeConfiguration", _screenshot_include_configuration);
		config.get("GENERAL", "ShowClock", _show_clock);
		config.get("GENERAL", "ShowFPS", _show_framerate);
		config.get("GENERAL", "FontGlobalScale", _imgui_context->IO.FontGlobalScale);
		config.get("GENERAL", "NoFontScaling", _no_font_scaling);
		config.get("GENERAL", "NoReloadOnInit", _no_reload_on_init);
		config.get("GENERAL", "SaveWindowState", _save_imgui_window_state);

		_imgui_context->IO.IniFilename = _save_imgui_window_state ? "ReShadeGUI.ini" : nullptr;

		config.get("STYLE", "Alpha", _imgui_context->Style.Alpha);
		config.get("STYLE", "ColBackground", _imgui_col_background);
		config.get("STYLE", "ColItemBackground", _imgui_col_item_background);
		config.get("STYLE", "ColActive", _imgui_col_active);
		config.get("STYLE", "ColText", _imgui_col_text);
		config.get("STYLE", "ColFPSText", _imgui_col_text_fps);

		_imgui_context->Style.Colors[ImGuiCol_Text] = ImVec4(_imgui_col_text[0], _imgui_col_text[1], _imgui_col_text[2], 1.00f);
		_imgui_context->Style.Colors[ImGuiCol_TextDisabled] = ImVec4(_imgui_col_text[0], _imgui_col_text[1], _imgui_col_text[2], 0.58f);
		_imgui_context->Style.Colors[ImGuiCol_WindowBg] = ImVec4(_imgui_col_background[0], _imgui_col_background[1], _imgui_col_background[2], 1.00f);
		_imgui_context->Style.Colors[ImGuiCol_ChildBg] = ImVec4(_imgui_col_item_background[0], _imgui_col_item_background[1], _imgui_col_item_background[2], 0.00f);
		_imgui_context->Style.Colors[ImGuiCol_Border] = ImVec4(_imgui_col_text[0], _imgui_col_text[1], _imgui_col_text[2], 0.30f);
		_imgui_context->Style.Colors[ImGuiCol_FrameBg] = ImVec4(_imgui_col_item_background[0], _imgui_col_item_background[1], _imgui_col_item_background[2], 1.00f);
		_imgui_context->Style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(_imgui_col_active[0], _imgui_col_active[1], _imgui_col_active[2], 0.68f);
		_imgui_context->Style.Colors[ImGuiCol_FrameBgActive] = ImVec4(_imgui_col_active[0], _imgui_col_active[1], _imgui_col_active[2], 1.00f);
		_imgui_context->Style.Colors[ImGuiCol_TitleBg] = ImVec4(_imgui_col_active[0], _imgui_col_active[1], _imgui_col_active[2], 0.45f);
		_imgui_context->Style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(_imgui_col_active[0], _imgui_col_active[1], _imgui_col_active[2], 0.35f);
		_imgui_context->Style.Colors[ImGuiCol_TitleBgActive] = ImVec4(_imgui_col_active[0], _imgui_col_active[1], _imgui_col_active[2], 0.78f);
		_imgui_context->Style.Colors[ImGuiCol_MenuBarBg] = ImVec4(_imgui_col_item_background[0], _imgui_col_item_background[1], _imgui_col_item_background[2], 0.57f);
		_imgui_context->Style.Colors[ImGuiCol_ScrollbarBg] = ImVec4(_imgui_col_item_background[0], _imgui_col_item_background[1], _imgui_col_item_background[2], 1.00f);
		_imgui_context->Style.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(_imgui_col_active[0], _imgui_col_active[1], _imgui_col_active[2], 0.31f);
		_imgui_context->Style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(_imgui_col_active[0], _imgui_col_active[1], _imgui_col_active[2], 0.78f);
		_imgui_context->Style.Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(_imgui_col_active[0], _imgui_col_active[1], _imgui_col_active[2], 1.00f);
		_imgui_context->Style.Colors[ImGuiCol_PopupBg] = ImVec4(_imgui_col_item_background[0], _imgui_col_item_background[1], _imgui_col_item_background[2], 0.92f);
		_imgui_context->Style.Colors[ImGuiCol_CheckMark] = ImVec4(_imgui_col_active[0], _imgui_col_active[1], _imgui_col_active[2], 0.80f);
		_imgui_context->Style.Colors[ImGuiCol_SliderGrab] = ImVec4(_imgui_col_active[0], _imgui_col_active[1], _imgui_col_active[2], 0.24f);
		_imgui_context->Style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(_imgui_col_active[0], _imgui_col_active[1], _imgui_col_active[2], 1.00f);
		_imgui_context->Style.Colors[ImGuiCol_Button] = ImVec4(_imgui_col_active[0], _imgui_col_active[1], _imgui_col_active[2], 0.44f);
		_imgui_context->Style.Colors[ImGuiCol_ButtonHovered] = ImVec4(_imgui_col_active[0], _imgui_col_active[1], _imgui_col_active[2], 0.86f);
		_imgui_context->Style.Colors[ImGuiCol_ButtonActive] = ImVec4(_imgui_col_active[0], _imgui_col_active[1], _imgui_col_active[2], 1.00f);
		_imgui_context->Style.Colors[ImGuiCol_Header] = ImVec4(_imgui_col_active[0], _imgui_col_active[1], _imgui_col_active[2], 0.76f);
		_imgui_context->Style.Colors[ImGuiCol_HeaderHovered] = ImVec4(_imgui_col_active[0], _imgui_col_active[1], _imgui_col_active[2], 0.86f);
		_imgui_context->Style.Colors[ImGuiCol_HeaderActive] = ImVec4(_imgui_col_active[0], _imgui_col_active[1], _imgui_col_active[2], 1.00f);
		_imgui_context->Style.Colors[ImGuiCol_Separator] = ImVec4(_imgui_col_text[0], _imgui_col_text[1], _imgui_col_text[2], 0.32f);
		_imgui_context->Style.Colors[ImGuiCol_SeparatorHovered] = ImVec4(_imgui_col_text[0], _imgui_col_text[1], _imgui_col_text[2], 0.78f);
		_imgui_context->Style.Colors[ImGuiCol_SeparatorActive] = ImVec4(_imgui_col_text[0], _imgui_col_text[1], _imgui_col_text[2], 1.00f);
		_imgui_context->Style.Colors[ImGuiCol_ResizeGrip] = ImVec4(_imgui_col_active[0], _imgui_col_active[1], _imgui_col_active[2], 0.20f);
		_imgui_context->Style.Colors[ImGuiCol_ResizeGripHovered] = ImVec4(_imgui_col_active[0], _imgui_col_active[1], _imgui_col_active[2], 0.78f);
		_imgui_context->Style.Colors[ImGuiCol_ResizeGripActive] = ImVec4(_imgui_col_active[0], _imgui_col_active[1], _imgui_col_active[2], 1.00f);
		_imgui_context->Style.Colors[ImGuiCol_PlotLines] = ImVec4(_imgui_col_text[0], _imgui_col_text[1], _imgui_col_text[2], 0.63f);
		_imgui_context->Style.Colors[ImGuiCol_PlotLinesHovered] = ImVec4(_imgui_col_active[0], _imgui_col_active[1], _imgui_col_active[2], 1.00f);
		_imgui_context->Style.Colors[ImGuiCol_PlotHistogram] = ImVec4(_imgui_col_text[0], _imgui_col_text[1], _imgui_col_text[2], 0.63f);
		_imgui_context->Style.Colors[ImGuiCol_PlotHistogramHovered] = ImVec4(_imgui_col_active[0], _imgui_col_active[1], _imgui_col_active[2], 1.00f);
		_imgui_context->Style.Colors[ImGuiCol_TextSelectedBg] = ImVec4(_imgui_col_active[0], _imgui_col_active[1], _imgui_col_active[2], 0.43f);

		if (_current_preset >= static_cast<ptrdiff_t>(_preset_files.size()))
		{
			_current_preset = -1;
		}

		const std::filesystem::path parent_path = g_reshade_dll_path.parent_path();

		std::error_code ec;
		for (const auto &entry : std::filesystem::directory_iterator(parent_path, ec))
		{
			const std::filesystem::path preset_file = entry.path();
			if (preset_file.extension() != ".ini" && preset_file.extension() != ".txt")
				continue;

			const ini_file preset(preset_file);

			std::vector<std::string> techniques;
			preset.get("", "Techniques", techniques);

			if (!techniques.empty() && std::find_if(_preset_files.begin(), _preset_files.end(),
				[&preset_file, &parent_path](const auto &path) {
					return preset_file.filename() == path.filename() && (path.parent_path() == parent_path || !path.is_absolute());
				}) == _preset_files.end())
			{
				_preset_files.push_back(preset_file);
			}
		}

		for (auto &function : _load_config_callables)
		{
			function(config);
		}
	}
	void runtime::save_config() const
	{
		save_config(_configuration_path);
	}
	void runtime::save_config(const std::filesystem::path &save_path) const
	{
		ini_file config(_configuration_path, save_path);

		config.set("INPUT", "KeyMenu", _menu_key_data);
		config.set("INPUT", "KeyScreenshot", _screenshot_key_data);
		config.set("INPUT", "KeyReload", _reload_key_data);
		config.set("INPUT", "KeyEffects", _effects_key_data);
		config.set("INPUT", "InputProcessing", _input_processing_mode);

		config.set("GENERAL", "PerformanceMode", _performance_mode);
		config.set("GENERAL", "EffectSearchPaths", _effect_search_paths);
		config.set("GENERAL", "TextureSearchPaths", _texture_search_paths);
		config.set("GENERAL", "PreprocessorDefinitions", _preprocessor_definitions);
		config.set("GENERAL", "PresetFiles", _preset_files);
		config.set("GENERAL", "CurrentPreset", _current_preset);
		config.set("GENERAL", "TutorialProgress", _tutorial_index);
		config.set("GENERAL", "ScreenshotPath", _screenshot_path);
		config.set("GENERAL", "ScreenshotFormat", _screenshot_format);
		config.set("GENERAL", "ScreenshotIncludePreset", _screenshot_include_preset);
		config.set("GENERAL", "ScreenshotIncludeConfiguration", _screenshot_include_configuration);
		config.set("GENERAL", "ShowClock", _show_clock);
		config.set("GENERAL", "ShowFPS", _show_framerate);
		config.set("GENERAL", "FontGlobalScale", _imgui_context->IO.FontGlobalScale);
		config.set("GENERAL", "NoReloadOnInit", _no_reload_on_init);
		config.set("GENERAL", "SaveWindowState", _save_imgui_window_state);

		config.set("STYLE", "Alpha", _imgui_context->Style.Alpha);
		config.set("STYLE", "ColBackground", _imgui_col_background);
		config.set("STYLE", "ColItemBackground", _imgui_col_item_background);
		config.set("STYLE", "ColActive", _imgui_col_active);
		config.set("STYLE", "ColText", _imgui_col_text);
		config.set("STYLE", "ColFPSText", _imgui_col_text_fps);

		for (auto &function : _save_config_callables)
		{
			function(config);
		}
	}

	void runtime::load_preset(const std::filesystem::path &path)
	{
		ini_file preset(path);

		// Reorder techniques
		std::vector<std::string> technique_list;
		preset.get("", "Techniques", technique_list);
		std::vector<std::string> technique_sorting_list;
		preset.get("", "TechniqueSorting", technique_sorting_list);

		for (const auto &name : technique_list)
			activate_technique(name);

		if (technique_sorting_list.empty())
			technique_sorting_list = technique_list;

		std::sort(_techniques.begin(), _techniques.end(),
			[&technique_sorting_list](const auto &lhs, const auto &rhs) {
				return (std::find(technique_sorting_list.begin(), technique_sorting_list.end(), lhs.name) - technique_sorting_list.begin()) <
					   (std::find(technique_sorting_list.begin(), technique_sorting_list.end(), rhs.name) - technique_sorting_list.begin());
			});

		for (auto &variable : _uniforms)
		{
			int values_int[16] = {};
			unsigned int values_uint[16] = {};
			float values_float[16] = {};

			switch (variable.type.base)
			{
			case reshadefx::type::t_int:
				get_uniform_value(variable, values_int, 16);
				preset.get(variable.effect_filename, variable.name, values_int);
				set_uniform_value(variable, values_int, 16);
				break;
			case reshadefx::type::t_bool:
			case reshadefx::type::t_uint:
				get_uniform_value(variable, values_uint, 16);
				preset.get(variable.effect_filename, variable.name, values_uint);
				set_uniform_value(variable, values_uint, 16);
				break;
			case reshadefx::type::t_float:
				get_uniform_value(variable, values_float, 16);
				preset.get(variable.effect_filename, variable.name, values_float);
				set_uniform_value(variable, values_float, 16);
				break;
			}
		}

		for (auto &technique : _techniques)
		{
			// Ignore preset if "enabled" annotation is set
			technique.enabled = technique.annotations["enabled"].second.as_uint[0] || std::find(technique_list.begin(), technique_list.end(), technique.name) != technique_list.end();

			preset.get("", "Key" + technique.name, technique.toggle_key_data);
		}
	}
	void runtime::load_current_preset()
	{
		if (_current_preset >= 0)
		{
			load_preset(_preset_files[_current_preset]);
		}
	}
	void runtime::save_preset(const std::filesystem::path &path) const
	{
		save_preset(path, path);
	}
	void runtime::save_preset(const std::filesystem::path &path, const std::filesystem::path &save_path) const
	{
		ini_file preset(path, save_path);

		std::unordered_set<std::string> active_effect_filenames;
		for (const auto &technique : _techniques)
		{
			if (technique.enabled)
			{
				active_effect_filenames.insert(technique.effect_filename);
			}
		}

		for (const auto &variable : _uniforms)
		{
			if (variable.annotations.count("source") || !active_effect_filenames.count(variable.effect_filename))
				continue;

			int values_int[16] = {};
			unsigned int values_uint[16] = {};
			float values_float[16] = {};

			assert(variable.type.components() < 16);

			switch (variable.type.base)
			{
			case reshadefx::type::t_int:
				get_uniform_value(variable, values_int, 16);
				preset.set(variable.effect_filename, variable.name, variant(values_int, variable.type.components()));
				break;
			case reshadefx::type::t_bool:
			case reshadefx::type::t_uint:
				get_uniform_value(variable, values_uint, 16);
				preset.set(variable.effect_filename, variable.name, variant(values_uint, variable.type.components()));
				break;
			case reshadefx::type::t_float:
				get_uniform_value(variable, values_float, 16);
				preset.set(variable.effect_filename, variable.name, variant(values_float, variable.type.components()));
				break;
			}
		}

		std::vector<std::string> technique_list, technique_sorting_list;
		std::unordered_set<std::string> effects_files;

		for (const auto &technique : _techniques)
		{
			if (technique.enabled)
			{
				effects_files.emplace(technique.effect_filename);
				technique_list.push_back(technique.name);
			}

			technique_sorting_list.push_back(technique.name);

			if (technique.toggle_key_data[0] != 0)
			{
				// Make sure techniques that are disabled but can be enabled via hotkey are loaded during fast loading too
				effects_files.emplace(technique.effect_filename);

				preset.set("", "Key" + technique.name, technique.toggle_key_data);
			}
			else if (int value = 0; preset.get("", "Key" + technique.name, value), value != 0)
			{
				// Clear toggle key data
				preset.set("", "Key" + technique.name, 0);
			}
		}

		preset.set("", "Effects", variant(std::make_move_iterator(effects_files.cbegin()), std::make_move_iterator(effects_files.cend())));
		preset.set("", "Techniques", std::move(technique_list));
		preset.set("", "TechniqueSorting", std::move(technique_sorting_list));
	}
	void runtime::save_current_preset() const
	{
		if (_current_preset >= 0)
		{
			save_preset(_preset_files[_current_preset]);
		}
	}

	void runtime::save_screenshot() const
	{
		std::vector<uint8_t> data(_width * _height * 4);
		capture_frame(data.data());

		const int hour = _date[3] / 3600;
		const int minute = (_date[3] - hour * 3600) / 60;
		const int second = _date[3] - hour * 3600 - minute * 60;

		char filename[21];
		ImFormatString(filename, sizeof(filename), " %.4d-%.2d-%.2d %.2d-%.2d-%.2d", _date[0], _date[1], _date[2], hour, minute, second);
		const std::wstring least = _screenshot_path / g_target_executable_path.stem().concat(filename);
		const std::wstring screenshot_path = least + (_screenshot_format == 0 ? L".bmp" : L".png");

		LOG(INFO) << "Saving screenshot to " << screenshot_path << " ...";

		FILE *file;
		bool success = false;

		if (_wfopen_s(&file, screenshot_path.c_str(), L"wb") == 0)
		{
			stbi_write_func *const func = [](void *context, void *data, int size) {
				fwrite(data, 1, size, static_cast<FILE *>(context));
			};

			switch (_screenshot_format)
			{
			case 0:
				success = stbi_write_bmp_to_func(func, file, _width, _height, 4, data.data()) != 0;
				break;
			case 1:
				success = stbi_write_png_to_func(func, file, _width, _height, 4, data.data(), 0) != 0;
				break;
			}

			fclose(file);
		}

		if (!success)
		{
			LOG(ERROR) << "Failed to write screenshot to " << screenshot_path << "!";
			return;
		}

		if (_screenshot_include_preset && _current_preset >= 0)
		{
			const std::filesystem::path preset_file = _preset_files[_current_preset];

			if (_screenshot_include_configuration)
			{
				const std::wstring config_path = least + L".ini";
				save_config(config_path);
			}

			const std::wstring preset_path = least + L' ' + preset_file.stem().wstring() + L".ini";
			save_preset(preset_file, preset_path);
		}
	}
}
