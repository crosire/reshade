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
	filesystem::path runtime::s_reshade_dll_path, runtime::s_target_executable_path;

	runtime::runtime(uint32_t renderer) :
		_renderer_id(renderer),
		_start_time(std::chrono::high_resolution_clock::now()),
		_last_frame_duration(std::chrono::milliseconds(1)),
		_imgui_context(ImGui::CreateContext()),
		_effect_search_paths({ s_reshade_dll_path.parent_path() }),
		_texture_search_paths({ s_reshade_dll_path.parent_path() }),
		_preprocessor_definitions({
			"RESHADE_DEPTH_LINEARIZATION_FAR_PLANE=1000.0",
			"RESHADE_DEPTH_INPUT_IS_UPSIDE_DOWN=0",
			"RESHADE_DEPTH_INPUT_IS_REVERSED=1",
			"RESHADE_DEPTH_INPUT_IS_LOGARITHMIC=0" }),
		_menu_key_data(),
		_screenshot_key_data(),
		_reload_key_data(),
		_effects_key_data(),
		_screenshot_path(s_target_executable_path.parent_path()),
		_variable_editor_height(500)
	{
		_menu_key_data[0] = 0x71; // VK_F2
		_menu_key_data[2] = true; // VK_SHIFT
		_screenshot_key_data[0] = 0x2C; // VK_SNAPSHOT

		_configuration_path = s_reshade_dll_path;
		_configuration_path.replace_extension(".ini");
		if (!filesystem::exists(_configuration_path))
			_configuration_path = s_reshade_dll_path.parent_path() / "ReShade.ini";

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
		const auto font_path = filesystem::get_special_folder_path(filesystem::special_folder::windows) / "Fonts" / "consolab.ttf";
		if (filesystem::exists(font_path))
			imgui_io.Fonts->AddFontFromFileTTF(font_path.string().c_str(), 18.0f);
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

		_texture_count = 0;
		_uniform_count = 0;
		_technique_count = 0;
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

		// Update and compile next effect queued for reloading
		if (_reload_remaining_effects != 0 && _framecount > 1)
		{
			load_effect(_effect_files[_effect_files.size() - _reload_remaining_effects]);

			_last_reload_time = std::chrono::high_resolution_clock::now();
			_reload_remaining_effects--;

			if (_reload_remaining_effects == 0)
			{
				load_textures();

				load_current_preset();

				if (_effect_filter_buffer[0] != '\0' && strcmp(_effect_filter_buffer, "Search") != 0)
				{
					filter_techniques(_effect_filter_buffer);
				}
			}
		}

		g_network_traffic = _drawcalls = _vertices = 0;
	}
	void runtime::on_present_effect()
	{
		if (!_toggle_key_setting_active && _input->is_key_pressed(_effects_key_data[0], _effects_key_data[1] != 0, _effects_key_data[2] != 0, _effects_key_data[3] != 0))
		{
			_effects_enabled = !_effects_enabled;
		}

		if (!_effects_enabled)
		{
			return;
		}

		// Update all uniform variables
		for (auto &variable : _uniforms)
		{
			const auto it = variable.annotations.find("source");

			if (it == variable.annotations.end())
			{
				continue;
			}

			const auto source = it->second.as<std::string>();

			if (source == "frametime")
			{
				const float value = _last_frame_duration.count() * 1e-6f;
				set_uniform_value(variable, &value, 1);
			}
			else if (source == "framecount")
			{
				switch (variable.basetype)
				{
					case uniform_datatype::boolean:
					{
						const bool even = (_framecount % 2) == 0;
						set_uniform_value(variable, &even, 1);
						break;
					}
					case uniform_datatype::signed_integer:
					case uniform_datatype::unsigned_integer:
					{
						const unsigned int framecount = static_cast<unsigned int>(_framecount % UINT_MAX);
						set_uniform_value(variable, &framecount, 1);
						break;
					}
					case uniform_datatype::floating_point:
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

				const float min = variable.annotations["min"].as<float>(), max = variable.annotations["max"].as<float>();
				const float step_min = variable.annotations["step"].as<float>(0), step_max = variable.annotations["step"].as<float>(1);
				float increment = step_max == 0 ? step_min : (step_min + std::fmodf(static_cast<float>(std::rand()), step_max - step_min + 1));
				const float smoothing = variable.annotations["smoothing"].as<float>();

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

				switch (variable.basetype)
				{
					case uniform_datatype::boolean:
					{
						const bool even = (timer % 2) == 0;
						set_uniform_value(variable, &even, 1);
						break;
					}
					case uniform_datatype::signed_integer:
					case uniform_datatype::unsigned_integer:
					{
						const unsigned int timer_int = static_cast<unsigned int>(timer % UINT_MAX);
						set_uniform_value(variable, &timer_int, 1);
						break;
					}
					case uniform_datatype::floating_point:
					{
						const float timer_float = std::fmod(static_cast<float>(timer * 1e-6f), 16777216.0f);
						set_uniform_value(variable, &timer_float, 1);
						break;
					}
				}
			}
			else if (source == "key")
			{
				const int key = variable.annotations["keycode"].as<int>();

				if (key > 7 && key < 256)
				{
					const std::string mode = variable.annotations["mode"].as<std::string>();

					if (mode == "toggle" || variable.annotations["toggle"].as<bool>())
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
				const int index = variable.annotations["keycode"].as<int>();

				if (index >= 0 && index < 5)
				{
					const std::string mode = variable.annotations["mode"].as<std::string>();

					if (mode == "toggle" || variable.annotations["toggle"].as<bool>())
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
				const int min = variable.annotations["min"].as<int>(), max = variable.annotations["max"].as<int>();
				const int value = min + (std::rand() % (max - min + 1));

				set_uniform_value(variable, &value, 1);
			}
		}

		// Render all enabled techniques
		for (auto &technique : _techniques)
		{
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

		_effect_files.clear();

		// Clear log on reload so that errors disappear from the splash screen
		reshade::log::lines.clear();

		std::vector<std::string> fastloading_filenames;

		if (_current_preset >= 0 && _performance_mode && !_show_menu)
		{
			const ini_file preset(_preset_files[_current_preset]);

			// Fast loading: Only load effect files that are actually used in the active preset
			preset.get("", "Effects", fastloading_filenames);
		}

		_is_fast_loading = !fastloading_filenames.empty();

		if (_is_fast_loading)
		{
			LOG(INFO) << "Loading " << fastloading_filenames.size() << " active effect files";

			for (const auto &effect : fastloading_filenames)
			{
				LOG(INFO) << "Searching for effect file: " << effect;

				for (const auto &search_path : _effect_search_paths)
				{
					auto effect_file = search_path / effect;

					if (exists(effect_file))
					{
						LOG(INFO) << ">> Found";
						_effect_files.push_back(std::move(effect_file));
						break;
					}

					LOG(INFO) << ">> Not Found";
				}
			}
		}
		else
		{
			for (const auto &search_path : _effect_search_paths)
			{
				const std::vector<filesystem::path> matching_files = filesystem::list_files(search_path, "*.fx");

				_effect_files.insert(_effect_files.end(), matching_files.begin(), matching_files.end());
			}
		}

		_reload_remaining_effects = _effect_files.size();
	}
	void runtime::load_effect(const filesystem::path &path)
	{
		LOG(INFO) << "Compiling " << path << " ...";

		reshadefx::preprocessor pp;

		if (path.is_absolute())
		{
			pp.add_include_path(path.parent_path());
		}

		for (const auto &include_path : _effect_search_paths)
		{
			if (include_path.empty())
			{
				continue;
			}

			pp.add_include_path(include_path);
		}

		pp.add_macro_definition("__RESHADE__", std::to_string(VERSION_MAJOR * 10000 + VERSION_MINOR * 100 + VERSION_REVISION));
		pp.add_macro_definition("__RESHADE_PERFORMANCE_MODE__", _performance_mode ? "1" : "0");
		pp.add_macro_definition("__VENDOR__", std::to_string(_vendor_id));
		pp.add_macro_definition("__DEVICE__", std::to_string(_device_id));
		pp.add_macro_definition("__RENDERER__", std::to_string(_renderer_id));
		pp.add_macro_definition("__APPLICATION__", std::to_string(std::hash<std::string>()(s_target_executable_path.filename_without_extension().string())));
		pp.add_macro_definition("BUFFER_WIDTH", std::to_string(_width));
		pp.add_macro_definition("BUFFER_HEIGHT", std::to_string(_height));
		pp.add_macro_definition("BUFFER_RCP_WIDTH", std::to_string(1.0f / static_cast<float>(_width)));
		pp.add_macro_definition("BUFFER_RCP_HEIGHT", std::to_string(1.0f / static_cast<float>(_height)));

		for (const auto &definition : _preprocessor_definitions)
		{
			if (definition.empty())
			{
				continue;
			}

			const size_t equals_index = definition.find_first_of('=');

			if (equals_index != std::string::npos)
			{
				pp.add_macro_definition(definition.substr(0, equals_index), definition.substr(equals_index + 1));
			}
			else
			{
				pp.add_macro_definition(definition);
			}
		}

		if (!pp.run(path))
		{
			LOG(ERROR) << "Failed to preprocess " << path << ":\n" << pp.errors();
			return;
		}

		reshadefx::syntax_tree ast;
		reshadefx::parser parser(ast);

		if (!parser.run(pp.current_output()))
		{
			LOG(ERROR) << "Failed to compile " << path << ":\n" << parser.errors();
			return;
		}

		if (_performance_mode && _current_preset >= 0)
		{
			ini_file preset(_preset_files[_current_preset]);

			for (auto variable : ast.variables)
			{
				if (!variable->type.has_qualifier(reshadefx::nodes::type_node::qualifier_uniform) ||
					variable->initializer_expression == nullptr ||
					variable->initializer_expression->id != reshadefx::nodeid::literal_expression ||
					variable->annotation_list.count("source"))
				{
					continue;
				}

				const auto initializer = static_cast<reshadefx::nodes::literal_expression_node *>(variable->initializer_expression);

				switch (initializer->type.basetype)
				{
				case reshadefx::nodes::type_node::datatype_int:
					preset.get(path.filename().string(), variable->name, initializer->value_int);
					break;
				case reshadefx::nodes::type_node::datatype_bool:
				case reshadefx::nodes::type_node::datatype_uint:
					preset.get(path.filename().string(), variable->name, initializer->value_uint);
					break;
				case reshadefx::nodes::type_node::datatype_float:
					preset.get(path.filename().string(), variable->name, initializer->value_float);
					break;
				}

				variable->type.qualifiers ^= reshadefx::nodes::type_node::qualifier_uniform;
				variable->type.qualifiers |= reshadefx::nodes::type_node::qualifier_static | reshadefx::nodes::type_node::qualifier_const;
			}
		}

		std::string errors = parser.errors();

		if (!load_effect(ast, errors))
		{
			LOG(ERROR) << "Failed to compile " << path << ":\n" << errors;
			_textures.erase(_textures.begin() + _texture_count, _textures.end());
			_uniforms.erase(_uniforms.begin() + _uniform_count, _uniforms.end());
			_techniques.erase(_techniques.begin() + _technique_count, _techniques.end());
			return;
		}
		else if (errors.empty())
		{
			LOG(INFO) << "> Successfully compiled.";
		}
		else
		{
			LOG(WARNING) << "> Successfully compiled with warnings:\n" << errors;
		}

		for (size_t i = _uniform_count, max = _uniform_count = _uniforms.size(); i < max; i++)
		{
			auto &variable = _uniforms[i];
			variable.effect_filename = path.filename().string();
			variable.hidden = variable.annotations["hidden"].as<bool>();
		}
		for (size_t i = _texture_count, max = _texture_count = _textures.size(); i < max; i++)
		{
			auto &texture = _textures[i];
			texture.effect_filename = path.filename().string();
		}
		for (size_t i = _technique_count, max = _technique_count = _techniques.size(); i < max; i++)
		{
			auto &technique = _techniques[i];
			technique.effect_filename = path.filename().string();
			technique.enabled = technique.annotations["enabled"].as<bool>();
			technique.hidden = technique.annotations["hidden"].as<bool>();
			technique.timeleft = technique.timeout = technique.annotations["timeout"].as<int>();
			technique.toggle_key_data[0] = technique.annotations["toggle"].as<unsigned int>();
			technique.toggle_key_data[1] = technique.annotations["togglectrl"].as<bool>() ? 1 : 0;
			technique.toggle_key_data[2] = technique.annotations["toggleshift"].as<bool>() ? 1 : 0;
			technique.toggle_key_data[3] = technique.annotations["togglealt"].as<bool>() ? 1 : 0;
		}
	}
	void runtime::load_textures()
	{
		LOG(INFO) << "Loading image files for textures ...";

		for (auto &texture : _textures)
		{
			if (texture.impl_reference != texture_reference::none)
			{
				continue;
			}

			const auto it = texture.annotations.find("source");

			if (it == texture.annotations.end())
			{
				continue;
			}

			const filesystem::path path = filesystem::resolve(it->second.as<std::string>(), _texture_search_paths);

			if (!filesystem::exists(path))
			{
				LOG(ERROR) << "> Source " << path << " for texture '" << texture.name << "' could not be found.";
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
					LOG(INFO) << "> Resizing image data for texture '" << texture.name << "' from " << width << "x" << height << " to " << texture.width << "x" << texture.height << " ...";

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
				LOG(ERROR) << "> Source " << path << " for texture '" << texture.name << "' could not be loaded! Make sure it is of a compatible file format.";
				continue;
			}
		}
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

		const filesystem::path parent_path = s_reshade_dll_path.parent_path();
		auto preset_files2 = filesystem::list_files(parent_path, "*.ini");
		auto preset_files3 = filesystem::list_files(parent_path, "*.txt");
		preset_files2.insert(preset_files2.end(), std::make_move_iterator(preset_files3.begin()), std::make_move_iterator(preset_files3.end()));

		for (const auto &preset_file : preset_files2)
		{
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

#if 0
		auto to_absolute = [&parent_path](auto &paths) {
			for (auto &path : paths)
				path = filesystem::absolute(path, parent_path);
		};

		to_absolute(_preset_files);
		to_absolute(_effect_search_paths);
		to_absolute(_texture_search_paths);
#endif

		for (auto &function : _load_config_callables)
		{
			function(config);
		}
	}
	void runtime::save_config() const
	{
		save_config(_configuration_path);
	}
	void runtime::save_config(const filesystem::path &save_path) const
	{
		ini_file config(_configuration_path,save_path);

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

	void runtime::load_preset(const filesystem::path &path)
	{
		ini_file preset(path);

		for (auto &variable : _uniforms)
		{
			int values_int[16] = {};
			unsigned int values_uint[16] = {};
			float values_float[16] = {};

			switch (variable.basetype)
			{
			case uniform_datatype::signed_integer:
				get_uniform_value(variable, values_int, 16);
				preset.get(variable.effect_filename, variable.name, values_int);
				set_uniform_value(variable, values_int, 16);
				break;
			case uniform_datatype::boolean:
			case uniform_datatype::unsigned_integer:
				get_uniform_value(variable, values_uint, 16);
				preset.get(variable.effect_filename, variable.name, values_uint);
				set_uniform_value(variable, values_uint, 16);
				break;
			case uniform_datatype::floating_point:
				get_uniform_value(variable, values_float, 16);
				preset.get(variable.effect_filename, variable.name, values_float);
				set_uniform_value(variable, values_float, 16);
				break;
			}
		}

		// Reorder techniques
		std::vector<std::string> technique_list;
		preset.get("", "Techniques", technique_list);
		std::vector<std::string> technique_sorting_list;
		preset.get("", "TechniqueSorting", technique_sorting_list);

		if (technique_sorting_list.empty())
			technique_sorting_list = technique_list;

		std::sort(_techniques.begin(), _techniques.end(),
			[&technique_sorting_list](const auto &lhs, const auto &rhs) {
				return (std::find(technique_sorting_list.begin(), technique_sorting_list.end(), lhs.name) - technique_sorting_list.begin()) <
					   (std::find(technique_sorting_list.begin(), technique_sorting_list.end(), rhs.name) - technique_sorting_list.begin());
			});

		for (auto &technique : _techniques)
		{
			// Ignore preset if "enabled" annotation is set
			technique.enabled = technique.annotations["enabled"].as<bool>() || std::find(technique_list.begin(), technique_list.end(), technique.name) != technique_list.end();

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
	void runtime::save_preset(const filesystem::path &path) const
	{
		save_preset(path, path);
	}
	void runtime::save_preset(const filesystem::path &path, const filesystem::path &save_path) const
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
			{
				continue;
			}

			int values_int[16] = {};
			unsigned int values_uint[16] = {};
			float values_float[16] = {};

			assert(variable.rows * variable.columns < 16);

			switch (variable.basetype)
			{
			case uniform_datatype::signed_integer:
				get_uniform_value(variable, values_int, 16);
				preset.set(variable.effect_filename, variable.name, variant(values_int, variable.rows * variable.columns));
				break;
			case uniform_datatype::boolean:
			case uniform_datatype::unsigned_integer:
				get_uniform_value(variable, values_uint, 16);
				preset.set(variable.effect_filename, variable.name, variant(values_uint, variable.rows * variable.columns));
				break;
			case uniform_datatype::floating_point:
				get_uniform_value(variable, values_float, 16);
				preset.set(variable.effect_filename, variable.name, variant(values_float, variable.rows * variable.columns));
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

		char filename[25];
		ImFormatString(filename, sizeof(filename), " %.4d-%.2d-%.2d %.2d-%.2d-%.2d", _date[0], _date[1], _date[2], hour, minute, second);
		const auto path = _screenshot_path / (s_target_executable_path.filename_without_extension() + filename + (_screenshot_format == 0 ? ".bmp" : ".png"));

		LOG(INFO) << "Saving screenshot to " << path << " ...";

		FILE *file;
		bool success = false;

		if (_wfopen_s(&file, path.wstring().c_str(), L"wb") == 0)
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
			LOG(ERROR) << "Failed to write screenshot to " << path << "!";
			return;
		}

		if (_screenshot_include_preset && _current_preset >= 0)
		{
			const auto preset_file = _preset_files[_current_preset];
			const auto save_preset_path = _screenshot_path / (s_target_executable_path.filename_without_extension() + filename + " " + preset_file.filename_without_extension() + ".ini");
			save_preset(preset_file, save_preset_path);

			if (_screenshot_include_configuration)
			{
				const auto save_configuration_path = _screenshot_path / (s_target_executable_path.filename_without_extension() + filename + ".ini");
				save_config(save_configuration_path);
			}
		}
	}
}
