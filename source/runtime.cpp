#include "log.hpp"
#include "version.h"
#include "runtime.hpp"
#include "parser.hpp"
#include "preprocessor.hpp"
#include "input.hpp"
#include "ini_file.hpp"
#include <algorithm>
#include <stb_image.h>
#include <stb_image_dds.h>
#include <stb_image_write.h>
#include <stb_image_resize.h>
#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui.h>
#include <imgui_internal.h>

namespace reshade
{
	filesystem::path runtime::s_reshade_dll_path, runtime::s_target_executable_path;

	runtime::runtime(uint32_t renderer) :
		_renderer_id(renderer),
		_start_time(std::chrono::high_resolution_clock::now()),
		_last_frame_duration(std::chrono::milliseconds(1)),
		_imgui_context(ImGui::CreateContext()),
		_imgui_font_atlas(std::make_unique<ImFontAtlas>()),
		_effect_search_paths({ s_reshade_dll_path.parent_path() }),
		_texture_search_paths({ s_reshade_dll_path.parent_path() }),
		_preprocessor_definitions({
			"RESHADE_DEPTH_LINEARIZATION_FAR_PLANE=1000.0",
			"RESHADE_DEPTH_INPUT_IS_UPSIDE_DOWN=0",
			"RESHADE_DEPTH_INPUT_IS_REVERSED=0",
			"RESHADE_DEPTH_INPUT_IS_LOGARITHMIC=0" }),
		_menu_key({ 0x71, false, true }), // VK_F2 + VK_SHIFT
		_screenshot_key({ 0x2C, false, false }), // VK_SNAPSHOT
		_screenshot_path(s_target_executable_path.parent_path()),
		_variable_editor_height(300)
	{
		ImGui::SetCurrentContext(_imgui_context);

		auto &imgui_io = _imgui_context->IO;
		auto &imgui_style = _imgui_context->Style;
		imgui_io.Fonts = _imgui_font_atlas.get();
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
		imgui_io.KeyMap[ImGuiKey_Delete] = 0x2E; // VK_DELETE
		imgui_io.KeyMap[ImGuiKey_Backspace] = 0x08; // VK_BACK
		imgui_io.KeyMap[ImGuiKey_Enter] = 0x0D; // VK_RETURN
		imgui_io.KeyMap[ImGuiKey_Escape] = 0x1B; // VK_ESCAPE
		imgui_io.KeyMap[ImGuiKey_A] = 'A';
		imgui_io.KeyMap[ImGuiKey_C] = 'C';
		imgui_io.KeyMap[ImGuiKey_V] = 'V';
		imgui_io.KeyMap[ImGuiKey_X] = 'X';
		imgui_io.KeyMap[ImGuiKey_Y] = 'Y';
		imgui_io.KeyMap[ImGuiKey_Z] = 'Z';
		imgui_style.WindowRounding = 0.0f;
		imgui_style.ChildWindowRounding = 0.0f;
		imgui_style.FrameRounding = 0.0f;
		imgui_style.ScrollbarRounding = 0.0f;
		imgui_style.GrabRounding = 0.0f;

		_imgui_font_atlas->AddFontDefault();
		const auto font_path = filesystem::get_special_folder_path(filesystem::special_folder::windows) / "Fonts" / "consolab.ttf";
		_imgui_font_atlas->AddFontFromFileTTF(font_path.string().c_str(), 18.0f);

		load_configuration();
	}
	runtime::~runtime()
	{
		ImGui::SetCurrentContext(_imgui_context);

		ImGui::Shutdown();
		ImGui::DestroyContext(_imgui_context);

		assert(!_is_initialized && _techniques.empty());
	}

	bool runtime::on_init()
	{
		LOG(INFO) << "Recreated runtime environment on runtime " << this << ".";

		_is_initialized = true;
		_last_reload_time = std::chrono::high_resolution_clock::now();

		reload();

		return true;
	}
	void runtime::on_reset()
	{
		on_reset_effect();

		if (!_is_initialized)
		{
			return;
		}

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
		_errors.clear();

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
		g_network_traffic = 0;
		_framecount++;
		_drawcalls = _vertices = 0;
		_last_frame_duration = std::chrono::high_resolution_clock::now() - _last_present_time;
		_last_present_time += _last_frame_duration;

		// Create and save screenshot if associated shortcut is down
		if (!_screenshot_key_setting_active &&
			_input->is_key_pressed(_screenshot_key.keycode, _screenshot_key.ctrl, _screenshot_key.shift, false))
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

				if (_current_preset >= 0)
				{
					load_preset(_preset_files[_current_preset]);
				}
			}
		}
	}
	void runtime::on_present_effect()
	{
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
					if (variable.annotations["toggle"].as<bool>())
					{
						bool current = false;
						get_uniform_value(variable, &current, 1);

						if (_input->is_key_pressed(key))
						{
							current = !current;

							set_uniform_value(variable, &current, 1);
						}
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
			else if (source == "mousebutton")
			{
				const int index = variable.annotations["keycode"].as<int>();

				if (index > 0 && index < 5)
				{
					if (variable.annotations["toggle"].as<bool>())
					{
						bool current = false;
						get_uniform_value(variable, &current, 1);

						if (_input->is_mouse_button_pressed(index))
						{
							current = !current;

							set_uniform_value(variable, &current, 1);
						}
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
					technique.average_duration.clear();
				}
			}
			else if (_input->is_key_pressed(technique.toggle_key, technique.toggle_key_ctrl, technique.toggle_key_shift, technique.toggle_key_alt) ||
				(technique.toggle_key >= 0x01 && technique.toggle_key <= 0x06 && _input->is_mouse_button_pressed(technique.toggle_key - 1)))
			{
				technique.enabled = !technique.enabled;

				if (technique.enabled)
				{
					technique.timeleft = technique.timeout;
				}
				else
				{
					technique.timeleft = 0;
					technique.average_duration.clear();
				}
			}

			if (!technique.enabled)
			{
				continue;
			}

			const auto time_technique_started = std::chrono::high_resolution_clock::now();

			render_technique(technique);

			const auto time_technique_finished = std::chrono::high_resolution_clock::now();

			technique.average_duration.append(std::chrono::duration_cast<std::chrono::nanoseconds>(time_technique_finished - time_technique_started).count());
		}
	}

	void runtime::reload()
	{
		on_reset_effect();

		_effect_files.clear();

		for (const auto &search_path : _effect_search_paths)
		{
			const auto matching_files = filesystem::list_files(search_path, "*.fx");
			_effect_files.insert(_effect_files.end(), matching_files.begin(), matching_files.end());
		}

		_reload_remaining_effects = _effect_files.size();
	}
	void runtime::load_effect(const filesystem::path &path)
	{
		LOG(INFO) << "Compiling " << path << " ...";

		reshadefx::preprocessor pp;
		pp.add_include_path(path.parent_path());

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
			LOG(ERROR) << "Failed to preprocess " << path << ":\n" << pp.current_errors();
			_errors += path.string() + ":\n" + pp.current_errors();
			return;
		}

		std::string errors;
		reshadefx::syntax_tree ast;
		reshadefx::parser parser(ast, errors);

		if (!parser.run(pp.current_output()))
		{
			LOG(ERROR) << "Failed to compile " << path << ":\n" << errors;
			_errors += path.string() + ":\n" + errors;
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
				const auto data = preset.get(path.filename().string(), variable->name);

				for (unsigned int i = 0; i < std::min(variable->type.rows, static_cast<unsigned int>(data.data().size())); i++)
				{
					switch (initializer->type.basetype)
					{
						case reshadefx::nodes::type_node::datatype_int:
							initializer->value_int[i] = data.as<int>(i);
							break;
						case reshadefx::nodes::type_node::datatype_bool:
						case reshadefx::nodes::type_node::datatype_uint:
							initializer->value_uint[i] = data.as<unsigned int>(i);
							break;
						case reshadefx::nodes::type_node::datatype_float:
							initializer->value_float[i] = data.as<float>(i);
							break;
					}
				}

				variable->type.qualifiers ^= reshadefx::nodes::type_node::qualifier_uniform;
				variable->type.qualifiers |= reshadefx::nodes::type_node::qualifier_static | reshadefx::nodes::type_node::qualifier_const;
			}
		}

		if (!load_effect(ast, errors))
		{
			LOG(ERROR) << "Failed to compile " << path << ":\n" << errors;
			_errors += path.string() + ":\n" + errors;
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
			_errors += path.string() + ":\n" + errors;
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
			technique.toggle_key = technique.annotations["toggle"].as<int>();
			technique.toggle_key_ctrl = technique.annotations["togglectrl"].as<bool>();
			technique.toggle_key_shift = technique.annotations["toggleshift"].as<bool>();
			technique.toggle_key_alt = technique.annotations["togglealt"].as<bool>();
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
				_errors += "Source '" + path.string() + "' for texture '" + texture.name + "' could not be found.";

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
				_errors += "Unable to load source for texture '" + texture.name + "'!";

				LOG(ERROR) << "> Source " << path << " for texture '" << texture.name << "' could not be loaded! Make sure it is of a compatible file format.";
				continue;
			}
		}
	}

	void runtime::load_configuration()
	{
		filesystem::path path(s_reshade_dll_path);
		path.replace_extension(".ini");
		const ini_file config(path);

		const int menu_key[3] = { _menu_key.keycode, _menu_key.ctrl ? 1 : 0, _menu_key.shift ? 1 : 0 };
		_menu_key.keycode = config.get("INPUT", "KeyMenu", menu_key).as<int>();
		_menu_key.ctrl = config.get("INPUT", "KeyMenu", menu_key).as<bool>(1);
		_menu_key.shift = config.get("INPUT", "KeyMenu", menu_key).as<bool>(2);
		const int screenshot_key[3] = { _screenshot_key.keycode, _screenshot_key.ctrl ? 1 : 0, _screenshot_key.shift ? 1 : 0 };
		_screenshot_key.keycode = config.get("INPUT", "KeyScreenshot", screenshot_key).as<int>();
		_screenshot_key.ctrl = config.get("INPUT", "KeyScreenshot", screenshot_key).as<bool>(1);
		_screenshot_key.shift = config.get("INPUT", "KeyScreenshot", screenshot_key).as<bool>(2);
		_input_processing_mode = config.get("INPUT", "InputProcessing", _input_processing_mode).as<int>();

		_performance_mode = config.get("GENERAL", "PerformanceMode", _performance_mode).as<bool>();
		const auto effect_search_paths = config.get("GENERAL", "EffectSearchPaths", _effect_search_paths).data();
		_effect_search_paths.assign(effect_search_paths.begin(), effect_search_paths.end());
		const auto texture_search_paths = config.get("GENERAL", "TextureSearchPaths", _texture_search_paths).data();
		_texture_search_paths.assign(texture_search_paths.begin(), texture_search_paths.end());
		_preprocessor_definitions = config.get("GENERAL", "PreprocessorDefinitions", _preprocessor_definitions).data();
		const auto preset_files = config.get("GENERAL", "PresetFiles", _preset_files).data();
		_preset_files.assign(preset_files.begin(), preset_files.end());
		_current_preset = config.get("GENERAL", "CurrentPreset", _current_preset).as<int>();
		_tutorial_index = config.get("GENERAL", "TutorialProgress", _tutorial_index).as<unsigned int>();

		_screenshot_path = config.get("GENERAL", "ScreenshotPath", _screenshot_path).as<filesystem::path>();
		_screenshot_format = config.get("GENERAL", "ScreenshotFormat", 0).as<int>();

		_show_fps = config.get("GENERAL", "ShowFPS", _show_fps).as<bool>();

		auto &style = _imgui_context->Style;
		style.Alpha = config.get("STYLE", "Alpha", 0.95f).as<float>();

		for (size_t i = 0; i < 3; i++)
			_imgui_col_background[i] = config.get("STYLE", "ColBackground", _imgui_col_background).as<float>(i);
		for (size_t i = 0; i < 3; i++)
			_imgui_col_item_background[i] = config.get("STYLE", "ColItemBackground", _imgui_col_item_background).as<float>(i);
		for (size_t i = 0; i < 3; i++)
			_imgui_col_active[i] = config.get("STYLE", "ColActive", _imgui_col_active).as<float>(i);
		for (size_t i = 0; i < 3; i++)
			_imgui_col_text[i] = config.get("STYLE", "ColText", _imgui_col_text).as<float>(i);
		for (size_t i = 0; i < 3; i++)
			_imgui_col_text_fps[i] = config.get("STYLE", "ColFPSText", _imgui_col_text_fps).as<float>(i);

		style.Colors[ImGuiCol_Text] = ImVec4(_imgui_col_text[0], _imgui_col_text[1], _imgui_col_text[2], 1.00f);
		style.Colors[ImGuiCol_TextDisabled] = ImVec4(_imgui_col_text[0], _imgui_col_text[1], _imgui_col_text[2], 0.58f);
		style.Colors[ImGuiCol_WindowBg] = ImVec4(_imgui_col_background[0], _imgui_col_background[1], _imgui_col_background[2], 1.00f);
		style.Colors[ImGuiCol_ChildWindowBg] = ImVec4(_imgui_col_item_background[0], _imgui_col_item_background[1], _imgui_col_item_background[2], 0.00f);
		style.Colors[ImGuiCol_Border] = ImVec4(_imgui_col_text[0], _imgui_col_text[1], _imgui_col_text[2], 0.30f);
		style.Colors[ImGuiCol_FrameBg] = ImVec4(_imgui_col_item_background[0], _imgui_col_item_background[1], _imgui_col_item_background[2], 1.00f);
		style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(_imgui_col_active[0], _imgui_col_active[1], _imgui_col_active[2], 0.68f);
		style.Colors[ImGuiCol_FrameBgActive] = ImVec4(_imgui_col_active[0], _imgui_col_active[1], _imgui_col_active[2], 1.00f);
		style.Colors[ImGuiCol_TitleBg] = ImVec4(_imgui_col_active[0], _imgui_col_active[1], _imgui_col_active[2], 0.45f);
		style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(_imgui_col_active[0], _imgui_col_active[1], _imgui_col_active[2], 0.35f);
		style.Colors[ImGuiCol_TitleBgActive] = ImVec4(_imgui_col_active[0], _imgui_col_active[1], _imgui_col_active[2], 0.78f);
		style.Colors[ImGuiCol_MenuBarBg] = ImVec4(_imgui_col_item_background[0], _imgui_col_item_background[1], _imgui_col_item_background[2], 0.57f);
		style.Colors[ImGuiCol_ScrollbarBg] = ImVec4(_imgui_col_item_background[0], _imgui_col_item_background[1], _imgui_col_item_background[2], 1.00f);
		style.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(_imgui_col_active[0], _imgui_col_active[1], _imgui_col_active[2], 0.31f);
		style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(_imgui_col_active[0], _imgui_col_active[1], _imgui_col_active[2], 0.78f);
		style.Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(_imgui_col_active[0], _imgui_col_active[1], _imgui_col_active[2], 1.00f);
		style.Colors[ImGuiCol_ComboBg] = ImVec4(_imgui_col_item_background[0], _imgui_col_item_background[1], _imgui_col_item_background[2], 1.00f);
		style.Colors[ImGuiCol_CheckMark] = ImVec4(_imgui_col_active[0], _imgui_col_active[1], _imgui_col_active[2], 0.80f);
		style.Colors[ImGuiCol_SliderGrab] = ImVec4(_imgui_col_active[0], _imgui_col_active[1], _imgui_col_active[2], 0.24f);
		style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(_imgui_col_active[0], _imgui_col_active[1], _imgui_col_active[2], 1.00f);
		style.Colors[ImGuiCol_Button] = ImVec4(_imgui_col_active[0], _imgui_col_active[1], _imgui_col_active[2], 0.44f);
		style.Colors[ImGuiCol_ButtonHovered] = ImVec4(_imgui_col_active[0], _imgui_col_active[1], _imgui_col_active[2], 0.86f);
		style.Colors[ImGuiCol_ButtonActive] = ImVec4(_imgui_col_active[0], _imgui_col_active[1], _imgui_col_active[2], 1.00f);
		style.Colors[ImGuiCol_Header] = ImVec4(_imgui_col_active[0], _imgui_col_active[1], _imgui_col_active[2], 0.76f);
		style.Colors[ImGuiCol_HeaderHovered] = ImVec4(_imgui_col_active[0], _imgui_col_active[1], _imgui_col_active[2], 0.86f);
		style.Colors[ImGuiCol_HeaderActive] = ImVec4(_imgui_col_active[0], _imgui_col_active[1], _imgui_col_active[2], 1.00f);
		style.Colors[ImGuiCol_Column] = ImVec4(_imgui_col_text[0], _imgui_col_text[1], _imgui_col_text[2], 0.32f);
		style.Colors[ImGuiCol_ColumnHovered] = ImVec4(_imgui_col_text[0], _imgui_col_text[1], _imgui_col_text[2], 0.78f);
		style.Colors[ImGuiCol_ColumnActive] = ImVec4(_imgui_col_text[0], _imgui_col_text[1], _imgui_col_text[2], 1.00f);
		style.Colors[ImGuiCol_ResizeGrip] = ImVec4(_imgui_col_active[0], _imgui_col_active[1], _imgui_col_active[2], 0.20f);
		style.Colors[ImGuiCol_ResizeGripHovered] = ImVec4(_imgui_col_active[0], _imgui_col_active[1], _imgui_col_active[2], 0.78f);
		style.Colors[ImGuiCol_ResizeGripActive] = ImVec4(_imgui_col_active[0], _imgui_col_active[1], _imgui_col_active[2], 1.00f);
		style.Colors[ImGuiCol_CloseButton] = ImVec4(_imgui_col_text[0], _imgui_col_text[1], _imgui_col_text[2], 0.16f);
		style.Colors[ImGuiCol_CloseButtonHovered] = ImVec4(_imgui_col_text[0], _imgui_col_text[1], _imgui_col_text[2], 0.39f);
		style.Colors[ImGuiCol_CloseButtonActive] = ImVec4(_imgui_col_text[0], _imgui_col_text[1], _imgui_col_text[2], 1.00f);
		style.Colors[ImGuiCol_PlotLines] = ImVec4(_imgui_col_text[0], _imgui_col_text[1], _imgui_col_text[2], 0.63f);
		style.Colors[ImGuiCol_PlotLinesHovered] = ImVec4(_imgui_col_active[0], _imgui_col_active[1], _imgui_col_active[2], 1.00f);
		style.Colors[ImGuiCol_PlotHistogram] = ImVec4(_imgui_col_text[0], _imgui_col_text[1], _imgui_col_text[2], 0.63f);
		style.Colors[ImGuiCol_PlotHistogramHovered] = ImVec4(_imgui_col_active[0], _imgui_col_active[1], _imgui_col_active[2], 1.00f);
		style.Colors[ImGuiCol_TextSelectedBg] = ImVec4(_imgui_col_active[0], _imgui_col_active[1], _imgui_col_active[2], 0.43f);
		style.Colors[ImGuiCol_PopupBg] = ImVec4(_imgui_col_item_background[0], _imgui_col_item_background[1], _imgui_col_item_background[2], 0.92f);

		if (_current_preset >= _preset_files.size())
		{
			_current_preset = -1;
		}
	}
	void runtime::save_configuration() const
	{
		filesystem::path path(s_reshade_dll_path);
		path.replace_extension(".ini");
		ini_file config(path);

		config.set("INPUT", "KeyMenu", { _menu_key.keycode, _menu_key.ctrl ? 1 : 0, _menu_key.shift ? 1 : 0 });
		config.set("INPUT", "KeyScreenshot", { _screenshot_key.keycode, _screenshot_key.ctrl ? 1 : 0, _screenshot_key.shift ? 1 : 0 });
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
		config.set("GENERAL", "ShowFPS", _show_fps);

		const auto &style = _imgui_context->Style;
		config.set("STYLE", "Alpha", style.Alpha);
		config.set("STYLE", "ColBackground", _imgui_col_background);
		config.set("STYLE", "ColItemBackground", _imgui_col_item_background);
		config.set("STYLE", "ColActive", _imgui_col_active);
		config.set("STYLE", "ColText", _imgui_col_text);
		config.set("STYLE", "ColFPSText", _imgui_col_text_fps);
	}
	void runtime::load_preset(const filesystem::path &path)
	{
		ini_file preset(path);

		for (auto &variable : _uniforms)
		{
			float values[16] = { };
			get_uniform_value(variable, values, 16);

			const auto preset_values = preset.get(variable.effect_filename, variable.name, variant(values, 16));

			for (unsigned int i = 0; i < 16; i++)
			{
				values[i] = preset_values.as<float>(i);
			}

			set_uniform_value(variable, values, 16);
		}

		// Reorder techniques
		std::vector<std::string> technique_list = preset.get("", "Techniques").data();
		std::sort(_techniques.begin(), _techniques.end(),
			[&technique_list](const auto &lhs, const auto &rhs) {
				return
					(std::find(technique_list.begin(), technique_list.end(), lhs.name) - technique_list.begin()) <
					(std::find(technique_list.begin(), technique_list.end(), rhs.name) - technique_list.begin());
			});
		for (auto &technique : _techniques)
		{
			technique.enabled = std::find(technique_list.begin(), technique_list.end(), technique.name) != technique_list.end();

			const int toggle_key[4] = { technique.toggle_key, technique.toggle_key_ctrl ? 1 : 0, technique.toggle_key_shift ? 1 : 0, technique.toggle_key_alt ? 1 : 0 };
			technique.toggle_key = preset.get("", "Key" + technique.name, toggle_key).as<int>(0);
			technique.toggle_key_ctrl = preset.get("", "Key" + technique.name, toggle_key).as<bool>(1);
			technique.toggle_key_shift = preset.get("", "Key" + technique.name, toggle_key).as<bool>(2);
			technique.toggle_key_alt = preset.get("", "Key" + technique.name, toggle_key).as<bool>(3);
		}
	}
	void runtime::save_preset(const filesystem::path &path) const
	{
		ini_file preset(path);

		for (const auto &variable : _uniforms)
		{
			if (variable.annotations.count("source"))
			{
				continue;
			}

			float values[16] = { };
			get_uniform_value(variable, values, 16);

			assert(variable.rows * variable.columns < 16);

			preset.set(variable.effect_filename, variable.name, variant(values, variable.rows * variable.columns));
		}

		std::vector<std::string> technique_list;

		for (const auto &technique : _techniques)
		{
			if (technique.enabled)
			{
				technique_list.push_back(technique.name);
			}

			const int toggle_key[4] = { technique.toggle_key, technique.toggle_key_ctrl ? 1 : 0, technique.toggle_key_shift ? 1 : 0, technique.toggle_key_alt ? 1 : 0 };
			preset.set("", "Key" + technique.name, toggle_key);
		}

		preset.set("", "Techniques", technique_list);
	}
	void runtime::save_screenshot() const
	{
		std::vector<uint8_t> data(_width * _height * 4);
		capture_frame(data.data());

		const int hour = _date[3] / 3600;
		const int minute = (_date[3] - hour * 3600) / 60;
		const int second = _date[3] - hour * 3600 - minute * 60;

		char filename[25];
		ImFormatString(filename, sizeof(filename), " %.4d-%.2d-%.2d %.2d-%.2d-%.2d%s", _date[0], _date[1], _date[2], hour, minute, second, _screenshot_format == 0 ? ".bmp" : ".png");
		const auto path = _screenshot_path / (s_target_executable_path.filename_without_extension() + filename);

		LOG(INFO) << "Saving screenshot to " << path << " ...";

		FILE *file;
		bool success = false;

		if (_wfopen_s(&file, path.wstring().c_str(), L"wb") == 0)
		{
			stbi_write_func *const func =
				[](void *context, void *data, int size) {
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
		}
	}

	static const char keyboard_keys[256][16] = {
		"", "", "", "Cancel", "", "", "", "", "Backspace", "Tab", "", "", "Clear", "Enter", "", "",
		"Shift", "Control", "Alt", "Pause", "Caps Lock", "", "", "", "", "", "", "Escape", "", "", "", "",
		"Space", "Page Up", "Page Down", "End", "Home", "Left Arrow", "Up Arrow", "Right Arrow", "Down Arrow", "Select", "", "", "Print Screen", "Insert", "Delete", "Help",
		"0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "", "", "", "", "", "",
		"", "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M", "N", "O",
		"P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z", "Left Windows", "Right Windows", "", "", "Sleep",
		"Numpad 0", "Numpad 1", "Numpad 2", "Numpad 3", "Numpad 4", "Numpad 5", "Numpad 6", "Numpad 7", "Numpad 8", "Numpad 9", "Numpad *", "Numpad +", "", "Numpad -", "Numpad Decimal", "Numpad /",
		"F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9", "F10", "F11", "F12", "F13", "F14", "F15", "F16",
		"F17", "F18", "F19", "F20", "F21", "F22", "F23", "F24", "", "", "", "", "", "", "", "",
		"Num Lock", "Scroll Lock",
	};

	static inline std::vector<std::string> split(const std::string &str, char delim)
	{
		std::vector<std::string> result;

		for (size_t i = 0, len = str.size(), found; i < len; i = found + 1)
		{
			found = str.find_first_of(delim, i);

			if (found == std::string::npos)
				found = len;

			result.push_back(str.substr(i, found - i));
		}

		return result;
	}

	void runtime::draw_overlay()
	{
		const bool show_splash = std::chrono::duration_cast<std::chrono::seconds>(_last_present_time - _last_reload_time).count() < 5;

		if (!_overlay_key_setting_active &&
			_input->is_key_pressed(_menu_key.keycode, _menu_key.ctrl, _menu_key.shift, false))
		{
			_show_menu = !_show_menu;
		}

		if (!(_show_menu || _show_fps || _show_error_log || show_splash))
		{
			_input->block_mouse_input(false);
			_input->block_keyboard_input(false);
			return;
		}

		// Update ImGui configuration
		ImGui::SetCurrentContext(_imgui_context);

		auto &imgui_io = ImGui::GetIO();
		imgui_io.DeltaTime = _last_frame_duration.count() * 1e-9f;
		imgui_io.DisplaySize.x = static_cast<float>(_width);
		imgui_io.DisplaySize.y = static_cast<float>(_height);
		imgui_io.Fonts->TexID = _imgui_font_atlas_texture.get();
		imgui_io.MouseDrawCursor = _show_menu;

		imgui_io.KeyCtrl = _input->is_key_down(0x11); // VK_CONTROL
		imgui_io.KeyShift = _input->is_key_down(0x10); // VK_SHIFT
		imgui_io.KeyAlt = _input->is_key_down(0x12); // VK_MENU
		imgui_io.MousePos.x = static_cast<float>(_input->mouse_position_x());
		imgui_io.MousePos.y = static_cast<float>(_input->mouse_position_y());
		imgui_io.MouseWheel += _input->mouse_wheel_delta();

		for (unsigned int i = 0; i < 256; i++)
		{
			imgui_io.KeysDown[i] = _input->is_key_down(i);

			if (_input->is_key_pressed(i))
			{
				imgui_io.AddInputCharacter(_input->key_to_text(i));
			}
		}
		for (unsigned int i = 0; i < 5; i++)
		{
			imgui_io.MouseDown[i] = _input->is_mouse_button_down(i);
		}

		if (imgui_io.KeyCtrl)
		{
			// Change global font scale if user presses the control key and moves the mouse wheel
			imgui_io.FontGlobalScale = ImClamp(imgui_io.FontGlobalScale + imgui_io.MouseWheel * 0.10f, 1.0f, 2.50f);
		}

		// Create ImGui widgets and windows
		ImGui::NewFrame();

		if (show_splash)
		{
			ImGui::SetNextWindowPos(ImVec2(10, 10));
			ImGui::SetNextWindowSize(ImVec2(_width - 20.0f, ImGui::GetItemsLineHeightWithSpacing() * 3), ImGuiSetCond_Appearing);
			ImGui::Begin("Splash Screen", nullptr, ImVec2(), -1,
				ImGuiWindowFlags_NoTitleBar |
				ImGuiWindowFlags_NoScrollbar |
				ImGuiWindowFlags_NoMove |
				ImGuiWindowFlags_NoResize |
				ImGuiWindowFlags_NoSavedSettings |
				ImGuiWindowFlags_NoInputs |
				ImGuiWindowFlags_NoFocusOnAppearing);

			ImGui::TextUnformatted("ReShade " VERSION_STRING_FILE " by crosire");
			ImGui::TextUnformatted("Visit http://reshade.me for news, updates, shaders and discussion.");

			if (_reload_remaining_effects != 0)
			{
				ImGui::Spacing();
				ImGui::Text(
					"Loading (%u effects remaining) ... "
					"This might take a while. The application could become unresponsive for some time.",
					static_cast<unsigned int>(_reload_remaining_effects));
			}
			else
			{
				ImGui::Spacing();
				ImGui::Text(
					"Press '%s%s%s' to open the configuration menu.",
					_menu_key.ctrl ? "Ctrl + " : "",
					_menu_key.shift ? "Shift + " : "",
					keyboard_keys[_menu_key.keycode]);

				if (_errors.find("error") != std::string::npos)
				{
					ImGui::SetWindowSize(ImVec2(_width - 20.0f, ImGui::GetItemsLineHeightWithSpacing() * 4));

					ImGui::Spacing();
					ImGui::TextColored(ImVec4(1, 0, 0, 1),
						"There were errors compiling some shaders. "
						"Open the configuration menu and click on 'Show Log' for more details.");
				}
			}

			ImGui::End();
		}

		if (_reload_remaining_effects == 0)
		{
			if (_show_menu)
			{
				ImGui::SetNextWindowPosCenter(ImGuiSetCond_Once);
				ImGui::SetNextWindowSize(ImVec2(710, 650), ImGuiSetCond_Once);
				ImGui::Begin("ReShade " VERSION_STRING_FILE " by crosire###Main", &_show_menu,
					ImGuiWindowFlags_MenuBar |
					ImGuiWindowFlags_NoCollapse);

				draw_overlay_menu();

				ImGui::End();
			}

			if (_show_fps && !show_splash)
			{
				ImGui::SetNextWindowPos(ImVec2(_width - 80.f, 0));
				ImGui::SetNextWindowSize(ImVec2(80, 100));
				ImGui::PushFont(imgui_io.Fonts->Fonts[1]);
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(_imgui_col_text_fps[0], _imgui_col_text_fps[1], _imgui_col_text_fps[2], 1.0f));
				ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4());
				ImGui::Begin("FPS", nullptr,
					ImGuiWindowFlags_NoTitleBar |
					ImGuiWindowFlags_NoScrollbar |
					ImGuiWindowFlags_NoMove |
					ImGuiWindowFlags_NoResize |
					ImGuiWindowFlags_NoSavedSettings |
					ImGuiWindowFlags_NoInputs |
					ImGuiWindowFlags_NoFocusOnAppearing);

				ImGui::Text("%.0f fps", imgui_io.Framerate);
				ImGui::Text("%03lld ms", std::chrono::duration_cast<std::chrono::milliseconds>(_last_frame_duration).count());

				ImGui::End();
				ImGui::PopStyleColor(2);
				ImGui::PopFont();
			}

			if (_show_error_log)
			{
				ImGui::SetNextWindowSize(ImVec2(500, 100), ImGuiSetCond_Once);
				ImGui::Begin("Error Log", &_show_error_log);
				ImGui::PushTextWrapPos();

				for (const auto &line : split(_errors, '\n'))
				{
					ImVec4 textcol(1, 1, 1, 1);

					if (line.find("error") != std::string::npos)
					{
						textcol = ImVec4(1, 0, 0, 1);
					}
					else if (line.find("warning") != std::string::npos)
					{
						textcol = ImVec4(1, 1, 0, 1);
					}

					ImGui::TextColored(textcol, line.c_str());
				}

				ImGui::PopTextWrapPos();
				ImGui::End();
			}
		}

		// Render ImGui widgets and windows
		ImGui::Render();

		if (_input_processing_mode != 0)
		{
			_input->block_mouse_input(imgui_io.WantCaptureMouse || (_input_processing_mode == 2 && _show_menu));
			_input->block_keyboard_input(imgui_io.WantCaptureKeyboard || (_input_processing_mode == 2 && _show_menu));
		}

		render_imgui_draw_data(ImGui::GetDrawData());
	}
	void runtime::draw_overlay_menu()
	{
		if (ImGui::BeginMenuBar())
		{
			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImGui::GetStyle().ItemSpacing * 2);

			const char *const menu_items[] = { "Home", "Settings", "Statistics", "About" };

			for (int i = 0; i < 4; i++)
			{
				if (ImGui::Selectable(menu_items[i], _menu_index == i, 0, ImVec2(ImGui::CalcTextSize(menu_items[i]).x, 0)))
				{
					_menu_index = i;
				}

				ImGui::SameLine();
			}

			ImGui::PopStyleVar();
			ImGui::EndMenuBar();
		}

		switch (_menu_index)
		{
			case 0:
				draw_overlay_menu_home();
				break;
			case 1:
				draw_overlay_menu_settings();
				break;
			case 2:
				draw_overlay_menu_statistics();
				break;
			case 3:
				draw_overlay_menu_about();
				break;
		}
	}
	void runtime::draw_overlay_menu_home()
	{
		bool continue_tutorial = false;
		const char *tutorial_text =
			"Welcome! Since this is the first time you start ReShade, we'll go through a quick tutorial covering the most important features.\n\n"
			"Before we continue: If you have difficulties reading this text, press the 'Ctrl' key and adjust the text size with your mouse wheel. "
			"The window size is variable as well, just grab the bottom right corner and move it around.\n\n"
			"Click on the 'Continue' button to continue the tutorial.";

		if (_tutorial_index > 0)
		{
			if (_tutorial_index == 1)
			{
				tutorial_text =
					"This is the preset selection. All changes will be saved to the selected file.\n\n"
					"You can add a new one by clicking on the '+' button. Simply enter a new preset name or the full path to an existing file (*.ini) in the text box that opens.\n"
					"To delete the currently selected preset file, click on the '-' button.\n"
					"Make sure a valid file is selected here before starting to tweak any values later, or else your changes won't be saved!\n\n"
					"Add a new preset by clicking on the '+' button to continue the tutorial.";

				ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(1, 0, 0, 1));
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(1, 0, 0, 1));
			}

			const auto get_preset_file = [](void *data, int i, const char **out) {
				*out = static_cast<runtime *>(data)->_preset_files[i].string().c_str();
				return true;
			};

			ImGui::PushItemWidth(-(30 + ImGui::GetStyle().ItemSpacing.x) * 2 - 1);

			if (ImGui::Combo("##presets", &_current_preset, get_preset_file, this, _preset_files.size()))
			{
				save_configuration();

				if (_performance_mode)
				{
					reload();
				}
				else
				{
					load_preset(_preset_files[_current_preset]);
				}
			}

			ImGui::PopItemWidth();

			ImGui::SameLine();

			if (ImGui::Button("+", ImVec2(30, 0)))
			{
				ImGui::OpenPopup("Add Preset");
			}

			if (ImGui::BeginPopup("Add Preset"))
			{
				char buf[260] = { };

				if (ImGui::InputText("Name", buf, sizeof(buf), ImGuiInputTextFlags_EnterReturnsTrue))
				{
					auto path = filesystem::absolute(buf, s_reshade_dll_path.parent_path());
					path.replace_extension(".ini");

					if (filesystem::exists(path) || filesystem::exists(path.parent_path()))
					{
						_preset_files.push_back(path);

						_current_preset = _preset_files.size() - 1;

						load_preset(path);
						save_configuration();

						ImGui::CloseCurrentPopup();

						if (_tutorial_index == 1)
						{
							continue_tutorial = true;
						}
					}
				}

				ImGui::EndPopup();
			}

			if (_current_preset >= 0)
			{
				ImGui::SameLine();

				if (ImGui::Button("-", ImVec2(30, 0)))
				{
					ImGui::OpenPopup("Remove Preset");
				}

				if (ImGui::BeginPopup("Remove Preset"))
				{
					ImGui::Text("Do you really want to remove this preset?");

					if (ImGui::Button("Yes", ImVec2(-1, 0)))
					{
						_preset_files.erase(_preset_files.begin() + _current_preset);

						if (_current_preset == _preset_files.size())
						{
							_current_preset -= 1;
						}
						if (_current_preset >= 0)
						{
							load_preset(_preset_files[_current_preset]);
						}

						save_configuration();

						ImGui::CloseCurrentPopup();
					}

					ImGui::EndPopup();
				}
			}

			if (_tutorial_index == 1)
			{
				ImGui::PopStyleColor();
				ImGui::PopStyleColor();
			}
		}

		if (_tutorial_index > 1)
		{
			if (_tutorial_index == 2)
			{
				tutorial_text =
					"This is the list of techniques. It contains all effects (*.fx) that were found in the effect search paths as specified on the 'Settings' tab.\n\n"
					"Enter text in the box at the top of the list to filter it and search for specific techniques.\n\n"
					"Click on a technique to enable or disable it or drag it to a new location in the list to change the order in which the effects are applied.";

				ImGui::PushStyleColor(ImGuiCol_ChildWindowBg, ImVec4(1, 0, 0, 1));
			}

			ImGui::Spacing();

			const float bottom_height = _performance_mode ? ImGui::GetItemsLineHeightWithSpacing() + ImGui::GetStyle().ItemSpacing.y : _variable_editor_height;

			if (ImGui::BeginChild("##techniques", ImVec2(-1, -bottom_height), true))
			{
				draw_overlay_technique_editor();
			}

			ImGui::EndChild();

			if (_tutorial_index == 2)
			{
				ImGui::PopStyleColor();
			}
		}

		if (_tutorial_index > 2 && !_performance_mode)
		{
			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
			ImGui::InvisibleButton("splitter", ImVec2(-1, 5));
			ImGui::PopStyleVar();

			if (ImGui::IsItemActive())
			{
				_variable_editor_height -= ImGui::GetIO().MouseDelta.y;
			}

			if (_tutorial_index == 3)
			{
				tutorial_text =
					"This is the list of variables. It contains all tweakable options the effects expose. All values here apply in real-time. Press 'Ctrl' and click on a widget to manually edit the value.\n\n"
					"Enter text in the box at the top of the list to filter it and search for specific variables.\n\n"
					"Once you have finished tweaking your preset, be sure to go to the 'Settings' tab and change the 'Usage Mode' to 'Performance Mode'. "
					"This will recompile all shaders into a more optimal representation that gives a significant performance boost, but will disable variable tweaking and this list.";

				ImGui::PushStyleColor(ImGuiCol_ChildWindowBg, ImVec4(1, 0, 0, 1));
			}

			const float bottom_height = ImGui::GetItemsLineHeightWithSpacing() + ImGui::GetStyle().ItemSpacing.y + (_tutorial_index == 3 ? 125 : 0);

			if (ImGui::BeginChild("##variables", ImVec2(-1, -bottom_height), true))
			{
				draw_overlay_variable_editor();
			}

			ImGui::EndChild();

			if (_tutorial_index == 3)
			{
				ImGui::PopStyleColor();
			}
		}

		if (_tutorial_index > 3)
		{
			ImGui::Spacing();

			if (ImGui::Button("Reload", ImVec2(ImGui::GetWindowContentRegionWidth() * 0.5f - 5, 0)))
			{
				reload();
			}

			ImGui::SameLine();

			if (ImGui::Button("Show Log", ImVec2(ImGui::GetWindowContentRegionWidth() * 0.5f - 5, 0)))
			{
				_show_error_log = true;
			}
		}
		else
		{
			ImGui::BeginChildFrame(0, ImVec2(-1, 125));
			ImGui::TextWrapped(tutorial_text);
			ImGui::EndChildFrame();

			if ((_tutorial_index != 1 || _current_preset != -1) &&
				ImGui::Button(_tutorial_index == 3 ? "Finish" : "Continue", ImVec2(-1, 0)) || continue_tutorial)
			{
				_tutorial_index++;

				save_configuration();
			}
		}
	}
	void runtime::draw_overlay_menu_settings()
	{
		char edit_buffer[2048];

		const auto copy_key_shortcut_to_edit_buffer = [&edit_buffer](const key_shortcut &shortcut) {
			size_t offset = 0;
			if (shortcut.ctrl) memcpy(edit_buffer, "Ctrl + ", 8), offset += 7;
			if (shortcut.shift) memcpy(edit_buffer + offset, "Shift + ", 9), offset += 8;
			memcpy(edit_buffer + offset, keyboard_keys[shortcut.keycode], sizeof(*keyboard_keys));
		};
		const auto copy_vector_to_edit_buffer = [&edit_buffer](const std::vector<std::string> &data) {
			size_t offset = 0;
			edit_buffer[0] = '\0';
			for (const auto &line : data)
			{
				memcpy(edit_buffer + offset, line.c_str(), line.size());
				offset += line.size();
				edit_buffer[offset++] = '\n';
				edit_buffer[offset] = '\0';
			}
		};
		const auto copy_search_paths_to_edit_buffer = [&edit_buffer](const std::vector<filesystem::path> &search_paths) {
			size_t offset = 0;
			edit_buffer[0] = '\0';
			for (const auto &search_path : search_paths)
			{
				memcpy(edit_buffer + offset, search_path.string().c_str(), search_path.length());
				offset += search_path.length();
				edit_buffer[offset++] = '\n';
				edit_buffer[offset] = '\0';
			}
		};

		if (ImGui::CollapsingHeader("General", ImGuiTreeNodeFlags_DefaultOpen))
		{
			assert(_menu_key.keycode < 256);

			copy_key_shortcut_to_edit_buffer(_menu_key);

			ImGui::InputText("Overlay Key", edit_buffer, sizeof(edit_buffer), ImGuiInputTextFlags_ReadOnly);

			_overlay_key_setting_active = false;

			if (ImGui::IsItemActive())
			{
				_overlay_key_setting_active = true;

				const unsigned int last_key_pressed = _input->last_key_pressed();

				if (last_key_pressed != 0 && (last_key_pressed < 0x10 || last_key_pressed > 0x11))
				{
					_menu_key.keycode = last_key_pressed;
					_menu_key.ctrl = _input->is_key_down(0x11);
					_menu_key.shift = _input->is_key_down(0x10);

					save_configuration();
				}
			}
			else if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("Click in the field and press any key to change the shortcut to that key.");
			}

			int usage_mode_index = _performance_mode ? 0 : 1;

			if (ImGui::Combo("Usage Mode", &usage_mode_index, "Performance Mode\0Configuration Mode\0"))
			{
				_performance_mode = usage_mode_index == 0;

				save_configuration();
				reload();
			}

			if (ImGui::Combo("Input Processing", &_input_processing_mode, "Pass on all input\0Block input when cursor is on overlay\0Block all input when overlay is visible\0"))
			{
				save_configuration();
			}

			copy_search_paths_to_edit_buffer(_effect_search_paths);

			if (ImGui::InputTextMultiline("Effect Search Paths", edit_buffer, sizeof(edit_buffer), ImVec2(0, 60)))
			{
				const auto effect_search_paths = split(edit_buffer, '\n');
				_effect_search_paths.assign(effect_search_paths.begin(), effect_search_paths.end());

				save_configuration();
			}

			copy_search_paths_to_edit_buffer(_texture_search_paths);

			if (ImGui::InputTextMultiline("Texture Search Paths", edit_buffer, sizeof(edit_buffer), ImVec2(0, 60)))
			{
				const auto texture_search_paths = split(edit_buffer, '\n');
				_texture_search_paths.assign(texture_search_paths.begin(), texture_search_paths.end());

				save_configuration();
			}

			copy_vector_to_edit_buffer(_preprocessor_definitions);

			if (ImGui::InputTextMultiline("Preprocessor Definitions", edit_buffer, sizeof(edit_buffer), ImVec2(0, 100)))
			{
				_preprocessor_definitions = split(edit_buffer, '\n');

				save_configuration();
			}

			if (ImGui::Button("Restart Tutorial", ImVec2(ImGui::CalcItemWidth(), 0)))
			{
				_tutorial_index = 0;
			}
		}

		if (ImGui::CollapsingHeader("Screenshots", ImGuiTreeNodeFlags_DefaultOpen))
		{
			assert(_screenshot_key.keycode < 256);

			copy_key_shortcut_to_edit_buffer(_screenshot_key);

			ImGui::InputText("Screenshot Key", edit_buffer, sizeof(edit_buffer), ImGuiInputTextFlags_ReadOnly);

			_screenshot_key_setting_active = false;

			if (ImGui::IsItemActive())
			{
				_screenshot_key_setting_active = true;

				const unsigned int last_key_pressed = _input->last_key_pressed();

				if (last_key_pressed != 0 && (last_key_pressed < 0x10 || last_key_pressed > 0x11))
				{
					_screenshot_key.keycode = last_key_pressed;
					_screenshot_key.ctrl = _input->is_key_down(0x11);
					_screenshot_key.shift = _input->is_key_down(0x10);

					save_configuration();
				}
			}
			else if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("Click in the field and press any key to change the shortcut to that key.");
			}

			memcpy(edit_buffer, _screenshot_path.string().c_str(), _screenshot_path.length() + 1);

			if (ImGui::InputText("Screenshot Path", edit_buffer, sizeof(edit_buffer)))
			{
				_screenshot_path = edit_buffer;

				save_configuration();
			}

			if (ImGui::Combo("Screenshot Format", &_screenshot_format, "Bitmap (*.bmp)\0Portable Network Graphics (*.png)\0"))
			{
				save_configuration();
			}
		}

		if (ImGui::CollapsingHeader("User Interface", ImGuiTreeNodeFlags_DefaultOpen))
		{
			const bool modified1 = ImGui::DragFloat("Alpha", &ImGui::GetStyle().Alpha, 0.005f, 0.20f, 1.0f, "%.2f");
			const bool modified2 = ImGui::ColorEdit3("Background Color", _imgui_col_background);
			const bool modified3 = ImGui::ColorEdit3("Item Background Color", _imgui_col_item_background);
			const bool modified4 = ImGui::ColorEdit3("Active Item Color", _imgui_col_active);
			const bool modified5 = ImGui::ColorEdit3("Text Color", _imgui_col_text);
			const bool modified6 = ImGui::ColorEdit3("FPS Text Color", _imgui_col_text_fps);

			ImGui::SameLine(ImGui::GetWindowWidth() - 100);

			const bool modified7 = ImGui::Checkbox("Show FPS", &_show_fps);

			if (modified1 || modified2 || modified3 || modified4 || modified5 || modified6 || modified7)
			{
				save_configuration();
				load_configuration();
			}
		}
	}
	void runtime::draw_overlay_menu_statistics()
	{
		if (ImGui::CollapsingHeader("General", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::Text("Application: %X", std::hash<std::string>()(s_target_executable_path.filename_without_extension().string()));
			ImGui::Text("Date: %d-%d-%d %d", _date[0], _date[1], _date[2], _date[3]);
			ImGui::Text("Device: %X %d", _vendor_id, _device_id);
			ImGui::Text("FPS: %.2f", ImGui::GetIO().Framerate);
			ImGui::PushItemWidth(-1);
			ImGui::PlotLines("##framerate", _imgui_context->FramerateSecPerFrame, 120, _imgui_context->FramerateSecPerFrameIdx, nullptr, _imgui_context->FramerateSecPerFrameAccum / 120 * 0.5f, _imgui_context->FramerateSecPerFrameAccum / 120 * 1.5f, ImVec2(0, 50));
			ImGui::PopItemWidth();
			uint64_t post_processing_time = 0;
			for (const auto &technique : _techniques)
				post_processing_time += technique.average_duration;
			ImGui::Text("Post-Processing: %f ms", (post_processing_time * 1e-6f));
			ImGui::Text("Draw Calls: %u (%u vertices)", _drawcalls, _vertices);
			ImGui::Text("Frame %llu: %f ms", _framecount + 1, _last_frame_duration.count() * 1e-6f);
			ImGui::Text("Timer: %f ms", std::fmod(std::chrono::duration_cast<std::chrono::nanoseconds>(_last_present_time - _start_time).count() * 1e-6f, 16777216.0f));
			ImGui::Text("Network (traffic per frame): %u B", g_network_traffic);
		}

		if (ImGui::CollapsingHeader("Textures", ImGuiTreeNodeFlags_DefaultOpen))
		{
			for (const auto &texture : _textures)
			{
				if (texture.impl_reference != texture_reference::none)
				{
					continue;
				}

				ImGui::Text("%s: %ux%u+%u (~%u kB)", texture.name.c_str(), texture.width, texture.height, (texture.levels - 1), (texture.width * texture.height * 4) / 1000);
			}
		}

		if (ImGui::CollapsingHeader("Techniques", ImGuiTreeNodeFlags_DefaultOpen))
		{
			for (const auto &technique : _techniques)
			{
				ImGui::Text("%s (%u passes): %f ms", technique.name.c_str(), static_cast<unsigned int>(technique.passes.size()), (technique.average_duration * 1e-6f));
			}
		}
	}
	void runtime::draw_overlay_menu_about()
	{
		ImGui::PushTextWrapPos();
		ImGui::TextUnformatted(
			"Copyright (C) 2014 Patrick \"crosire\" Mours. All rights reserved.\n"
			"\n"
			"This software is provided 'as-is', without any express or implied warranty.\n"
			"In no event will the authors be held liable for any damages arising from the use of this software.\n"
			"\n"
			"Additional Credits:");

		if (ImGui::CollapsingHeader("MinHook"))
		{
			ImGui::TextUnformatted(R"(Copyright (C) 2009-2016 Tsuda Kageyu. All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

 1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
 2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.)");
		}
		if (ImGui::CollapsingHeader("Hacker Disassembler Engine 32/64 C"))
		{
			ImGui::TextUnformatted(R"(Copyright (c) 2008-2009, Vyacheslav Patkov. All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

 1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
 2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.)");
		}
		if (ImGui::CollapsingHeader("dear imgui"))
		{
			ImGui::TextUnformatted(R"(Copyright (c) 2014-2015 Omar Cornut and ImGui contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.)");
		}
		if (ImGui::CollapsingHeader("gl3w"))
		{
			ImGui::TextUnformatted("Slavomir Kaslev");
		}
		if (ImGui::CollapsingHeader("stb_image, stb_image_write"))
		{
			ImGui::TextUnformatted("Sean Barrett and contributors");
		}
		if (ImGui::CollapsingHeader("DDS loading from SOIL"))
		{
			ImGui::TextUnformatted("Jonathan \"lonesock\" Dummer");
		}

		ImGui::PopTextWrapPos();
	}
	void runtime::draw_overlay_variable_editor()
	{
		ImGui::PushItemWidth(-1);

		if (ImGui::InputText("##filter", _variable_filter_buffer, sizeof(_variable_filter_buffer)))
		{
			if (_variable_filter_buffer[0] == '\0')
			{
				for (auto &uniform : _uniforms)
				{
					if (uniform.annotations["hidden"].as<bool>())
						continue;

					uniform.hidden = false;
				}
			}
			else
			{
				const std::string filter(_variable_filter_buffer);

				for (auto &uniform : _uniforms)
				{
					if (uniform.annotations["hidden"].as<bool>())
						continue;

					uniform.hidden =
						std::search(uniform.name.begin(), uniform.name.end(), filter.begin(), filter.end(),
						[](auto c1, auto c2) {
							return tolower(c1) == tolower(c2);
						}) == uniform.name.end() && uniform.effect_filename.find(filter) == std::string::npos;
				}
			}
		}

		ImGui::PopItemWidth();

		ImGui::Spacing();

		ImGui::PushItemWidth(ImGui::GetWindowWidth() * 0.5f);

		bool current_tree_is_closed = true;
		std::string current_filename;

		for (int id = 0; id < static_cast<int>(_uniform_count); id++)
		{
			auto &variable = _uniforms[id];

			if (variable.hidden || variable.annotations.count("source"))
			{
				continue;
			}

			if (current_filename != variable.effect_filename)
			{
				if (!current_tree_is_closed)
					ImGui::TreePop();

				current_filename = variable.effect_filename;
				current_tree_is_closed = !ImGui::TreeNode(variable.effect_filename.c_str());
			}
			if (current_tree_is_closed)
			{
				continue;
			}

			bool modified = false;
			const auto ui_type = variable.annotations["ui_type"].as<std::string>();
			const auto ui_label = variable.annotations.count("ui_label") ? variable.annotations.at("ui_label").as<std::string>() : variable.name;
			const auto ui_tooltip = variable.annotations["ui_tooltip"].as<std::string>();

			ImGui::PushID(id);

			switch (variable.displaytype)
			{
				case uniform_datatype::boolean:
				{
					bool data[1] = { };
					get_uniform_value(variable, data, 1);

					int index = data[0] ? 0 : 1;

					if (ImGui::Combo(ui_label.c_str(), &index, "On\0Off\0"))
					{
						data[0] = index == 0;
						modified = true;

						set_uniform_value(variable, data, 1);
					}
					break;
				}
				case uniform_datatype::signed_integer:
				case uniform_datatype::unsigned_integer:
				{
					int data[4] = { };
					get_uniform_value(variable, data, 4);

					if (ui_type == "drag")
					{
						modified = ImGui::DragIntN(ui_label.c_str(), data, variable.rows, variable.annotations["ui_step"].as<float>(), variable.annotations["ui_min"].as<int>(), variable.annotations["ui_max"].as<int>(), nullptr);
					}
					else if (ui_type == "combo")
					{
						modified = ImGui::Combo(ui_label.c_str(), data, variable.annotations["ui_items"].as<std::string>().c_str());
					}
					else
					{
						modified = ImGui::InputIntN(ui_label.c_str(), data, variable.rows, 0);
					}

					if (modified)
					{
						set_uniform_value(variable, data, 4);
					}
					break;
				}
				case uniform_datatype::floating_point:
				{
					float data[4] = { };
					get_uniform_value(variable, data, 4);

					if (ui_type == "drag")
					{
						modified = ImGui::DragFloatN(ui_label.c_str(), data, variable.rows, variable.annotations["ui_step"].as<float>(), variable.annotations["ui_min"].as<float>(), variable.annotations["ui_max"].as<float>(), "%.3f", 1.0f);
					}
					else if (ui_type == "input" || (ui_type.empty() && variable.rows < 3))
					{
						modified = ImGui::InputFloatN(ui_label.c_str(), data, variable.rows, 8, 0);
					}
					else if (variable.rows == 3)
					{
						modified = ImGui::ColorEdit3(ui_label.c_str(), data);
					}
					else if (variable.rows == 4)
					{
						modified = ImGui::ColorEdit4(ui_label.c_str(), data);
					}

					if (modified)
					{
						set_uniform_value(variable, data, 4);
					}
					break;
				}
			}

			if (ImGui::IsItemHovered() && !ui_tooltip.empty())
			{
				ImGui::SetTooltip("%s", ui_tooltip.c_str());
			}

			ImGui::PopID();

			if (_current_preset >= 0 && modified)
			{
				save_preset(_preset_files[_current_preset]);
			}
		}

		if (!current_tree_is_closed)
		{
			ImGui::TreePop();
		}

		ImGui::PopItemWidth();
	}
	void runtime::draw_overlay_technique_editor()
	{
		ImGui::PushItemWidth(-1);

		if (ImGui::InputText("##filter", _technique_filter_buffer, sizeof(_technique_filter_buffer)))
		{
			if (_technique_filter_buffer[0] == '\0')
			{
				for (auto &technique : _techniques)
				{
					if (technique.annotations["hidden"].as<bool>())
						continue;

					technique.hidden = false;
				}
			}
			else
			{
				const std::string filter(_technique_filter_buffer);

				for (auto &technique : _techniques)
				{
					if (technique.annotations["hidden"].as<bool>())
						continue;

					technique.hidden =
						std::search(technique.name.begin(), technique.name.end(), filter.begin(), filter.end(),
							[](auto c1, auto c2) {
						return tolower(c1) == tolower(c2);
					}) == technique.name.end() && technique.effect_filename.find(filter) == std::string::npos;
				}
			}
		}

		ImGui::PopItemWidth();

		ImGui::Spacing();

		int hovered_technique_index = -1;
		bool current_tree_is_closed = true;
		std::string current_filename;
		char edit_buffer[2048];
		const float toggle_key_text_offset = ImGui::GetWindowContentRegionWidth() - ImGui::CalcTextSize("Toggle Key").x - 201;

		for (int id = 0; id < static_cast<int>(_technique_count); id++)
		{
			auto &technique = _techniques[id];

			if (technique.hidden)
			{
				continue;
			}

			if (current_filename != technique.effect_filename)
			{
				if (!current_tree_is_closed)
					ImGui::TreePop();

				current_filename = technique.effect_filename;
				current_tree_is_closed = !ImGui::TreeNodeEx(technique.effect_filename.c_str(), ImGuiTreeNodeFlags_DefaultOpen);
			}
			if (current_tree_is_closed)
			{
				continue;
			}

			ImGui::PushID(id);

			if (ImGui::Checkbox(technique.name.c_str(), &technique.enabled) && _current_preset >= 0)
			{
				save_preset(_preset_files[_current_preset]);
			}

			if (ImGui::IsItemActive())
			{
				_selected_technique = id;
			}
			if (ImGui::IsItemHoveredRect())
			{
				hovered_technique_index = id;
			}

			assert(technique.toggle_key < 256);

			size_t offset = 0;
			if (technique.toggle_key_ctrl) memcpy(edit_buffer, "Ctrl + ", 8), offset += 7;
			if (technique.toggle_key_shift) memcpy(edit_buffer + offset, "Shift + ", 9), offset += 8;
			if (technique.toggle_key_alt) memcpy(edit_buffer + offset, "Alt + ", 7), offset += 6;
			memcpy(edit_buffer + offset, keyboard_keys[technique.toggle_key], sizeof(*keyboard_keys));

			ImGui::SameLine(toggle_key_text_offset);
			ImGui::TextUnformatted("Toggle Key");
			ImGui::SameLine();
			ImGui::InputTextEx("##ToggleKey", edit_buffer, sizeof(edit_buffer), ImVec2(200, 0), ImGuiInputTextFlags_ReadOnly);

			if (ImGui::IsItemActive())
			{
				const unsigned int last_key_pressed = _input->last_key_pressed();

				if (last_key_pressed != 0)
				{
					if (last_key_pressed == 0x08) // Backspace
					{
						technique.toggle_key = 0;
						technique.toggle_key_ctrl = false;
						technique.toggle_key_shift = false;
						technique.toggle_key_alt = false;
					}
					else if (last_key_pressed < 0x10 || last_key_pressed > 0x12)
					{
						technique.toggle_key = last_key_pressed;
						technique.toggle_key_ctrl = _input->is_key_down(0x11);
						technique.toggle_key_shift = _input->is_key_down(0x10);
						technique.toggle_key_alt = _input->is_key_down(0x12);
					}

					if (_current_preset >= 0)
					{
						save_preset(_preset_files[_current_preset]);
					}
				}
			}
			else if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("Click in the field and press any key to change the toggle shortcut to that key.\nPress backspace to disable the shortcut.");
			}

			ImGui::PopID();
		}

		if (!current_tree_is_closed)
		{
			ImGui::TreePop();
		}

		if (ImGui::IsMouseDragging() && _selected_technique >= 0)
		{
			ImGui::SetTooltip(_techniques[_selected_technique].name.c_str());

			if (hovered_technique_index >= 0 && hovered_technique_index != _selected_technique)
			{
				std::swap(_techniques[hovered_technique_index], _techniques[_selected_technique]);
				_selected_technique = hovered_technique_index;

				if (_current_preset >= 0)
				{
					save_preset(_preset_files[_current_preset]);
				}
			}
		}
		else
		{
			_selected_technique = -1;
		}
	}
}
