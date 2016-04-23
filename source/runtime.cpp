#include "log.hpp"
#include "version.h"
#include "runtime.hpp"
#include "runtime_objects.hpp"
#include "hook_manager.hpp"
#include "parser.hpp"
#include "preprocessor.hpp"
#include "input.hpp"
#include "file_watcher.hpp"

#include <iterator>
#include <stb_dxt.h>
#include <stb_image.h>
#include <stb_image_write.h>
#include <stb_image_resize.h>
#include <boost/filesystem/operations.hpp>
#include <ShlObj.h>

#define IMGUI_DISABLE_OBSOLETE_FUNCTIONS
#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui.h>
#include <imgui_internal.h>
#include "ini_file.hpp"

namespace reshade
{
	namespace fs = boost::filesystem;

	namespace
	{
		fs::path obfuscate_path(const fs::path &path)
		{
			WCHAR username_data[257];
			DWORD username_size = 257;
			GetUserNameW(username_data, &username_size);

			std::wstring result = path.wstring();
			const std::wstring username(username_data, username_size - 1);
			const std::wstring username_replacement(username_size - 1, L'*');

			size_t start_pos = 0;

			while ((start_pos = result.find(username, start_pos)) != std::string::npos)
			{
				result.replace(start_pos, username.length(), username_replacement);
				start_pos += username.length();
			}

			return result;
		}
		std::vector<std::string> split(const std::string& s, char delim)
		{
			std::vector<std::string> output;
			std::string item;
			std::stringstream ss(s);

			while (std::getline(ss, item, delim))
			{
				if (!item.empty())
				{
					output.push_back(item);
				}
			}

			return output;
		}

		utils::ini_file config;
		fs::path s_executable_path, s_injector_path;
		std::string s_executable_name;

		const char keyboard_keys[256][16] = {
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
	}

	void runtime::startup(const fs::path &executable_path, const fs::path &injector_path)
	{
		s_injector_path = injector_path;
		s_executable_path = executable_path;
		s_executable_name = executable_path.stem().string();

		fs::path log_path = injector_path, tracelog_path = injector_path;
		log_path.replace_extension("log");
		tracelog_path.replace_extension("tracelog");

		if (fs::exists(tracelog_path))
		{
			log::debug = true;

			log::open(tracelog_path);
		}
		else
		{
			log::open(log_path);
		}

#ifdef WIN64
#define VERSION_PLATFORM "64-bit"
#else
#define VERSION_PLATFORM "32-bit"
#endif
		LOG(INFO) << "Initializing crosire's ReShade version '" BOOST_STRINGIZE(VERSION_FULL) "' (" << VERSION_PLATFORM << ") built on '" VERSION_DATE " " VERSION_TIME "' loaded from " << obfuscate_path(injector_path) << " to " << obfuscate_path(executable_path) << " ...";

		TCHAR system_path_buffer[MAX_PATH];
		GetSystemDirectory(system_path_buffer, MAX_PATH);
		const fs::path system_path(system_path_buffer);

		TCHAR appdata_path_buffer[MAX_PATH];
		SHGetFolderPath(nullptr, CSIDL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, appdata_path_buffer);
		const fs::path appdata_path(appdata_path_buffer);

		if (!fs::exists(appdata_path / "ReShade"))
		{
			fs::create_directory(appdata_path / "ReShade");
		}

		config = utils::ini_file(appdata_path / "ReShade" / "ReShade.ini");

		hooks::register_module(system_path / "d3d8.dll");
		hooks::register_module(system_path / "d3d9.dll");
		hooks::register_module(system_path / "d3d10.dll");
		hooks::register_module(system_path / "d3d10_1.dll");
		hooks::register_module(system_path / "d3d11.dll");
		hooks::register_module(system_path / "d3d12.dll");
		hooks::register_module(system_path / "dxgi.dll");
		hooks::register_module(system_path / "opengl32.dll");
		hooks::register_module(system_path / "user32.dll");
		hooks::register_module(system_path / "ws2_32.dll");

		auto &imgui_io = ImGui::GetIO();
		imgui_io.IniFilename = nullptr;
		imgui_io.KeyMap[ImGuiKey_Tab] = VK_TAB;
		imgui_io.KeyMap[ImGuiKey_LeftArrow] = VK_LEFT;
		imgui_io.KeyMap[ImGuiKey_RightArrow] = VK_RIGHT;
		imgui_io.KeyMap[ImGuiKey_UpArrow] = VK_UP;
		imgui_io.KeyMap[ImGuiKey_DownArrow] = VK_DOWN;
		imgui_io.KeyMap[ImGuiKey_PageUp] = VK_PRIOR;
		imgui_io.KeyMap[ImGuiKey_PageDown] = VK_NEXT;
		imgui_io.KeyMap[ImGuiKey_Home] = VK_HOME;
		imgui_io.KeyMap[ImGuiKey_End] = VK_END;
		imgui_io.KeyMap[ImGuiKey_Delete] = VK_DELETE;
		imgui_io.KeyMap[ImGuiKey_Backspace] = VK_BACK;
		imgui_io.KeyMap[ImGuiKey_Enter] = VK_RETURN;
		imgui_io.KeyMap[ImGuiKey_Escape] = VK_ESCAPE;
		imgui_io.KeyMap[ImGuiKey_A] = 'A';
		imgui_io.KeyMap[ImGuiKey_C] = 'C';
		imgui_io.KeyMap[ImGuiKey_V] = 'V';
		imgui_io.KeyMap[ImGuiKey_X] = 'X';
		imgui_io.KeyMap[ImGuiKey_Y] = 'Y';
		imgui_io.KeyMap[ImGuiKey_Z] = 'Z';
		auto &imgui_style = ImGui::GetStyle();
		imgui_style.WindowRounding = 0.0f;
		imgui_style.ScrollbarRounding = 0.0f;
		imgui_style.Colors[ImGuiCol_WindowBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.78f);
		imgui_style.Colors[ImGuiCol_TitleBg] = ImVec4(0.91f, 0.27f, 0.05f, 1.00f);
		imgui_style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.91f, 0.27f, 0.05f, 1.00f);
		imgui_style.Colors[ImGuiCol_TitleBgActive] = ImVec4(1.00f, 0.43f, 0.05f, 1.00f);
		imgui_style.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.91f, 0.27f, 0.05f, 0.80f);
		imgui_style.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.91f, 0.27f, 0.05f, 1.00f);
		imgui_style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(1.00f, 0.43f, 0.05f, 1.00f);
		imgui_style.Colors[ImGuiCol_Header] = ImVec4(0.91f, 0.27f, 0.05f, 0.80f);
		imgui_style.Colors[ImGuiCol_HeaderHovered] = ImVec4(1.00f, 0.43f, 0.05f, 1.00f);
		imgui_style.Colors[ImGuiCol_HeaderActive] = ImVec4(1.00f, 0.43f, 0.05f, 1.00f);
		imgui_style.Colors[ImGuiCol_CloseButton] = ImVec4(0.90f, 0.90f, 0.90f, 1.00f);
		imgui_style.Colors[ImGuiCol_CloseButtonHovered] = ImVec4(0.70f, 0.70f, 0.70f, 1.00f);
		imgui_style.Colors[ImGuiCol_CloseButtonActive] = ImVec4(0.64f, 0.64f, 0.64f, 1.00f);

		LOG(INFO) << "Initialized.";
	}
	void runtime::shutdown()
	{
		LOG(INFO) << "Exiting ...";

		ImGui::Shutdown();

		input::uninstall();
		hooks::uninstall();

		LOG(INFO) << "Exited.";
	}

	runtime::runtime(uint32_t renderer) :
		_renderer_id(renderer),
		_start_time(boost::chrono::high_resolution_clock::now()),
		_shader_edit_buffer(2048)
	{
		_menu_key = config.get(s_executable_path.string(), "MenuKey", VK_F9).as<int>();
		_screenshot_key = config.get(s_executable_path.string(), "ScreenshotKey", VK_SNAPSHOT).as<int>();
		_screenshot_path = config.get(s_executable_path.string(), "ScreenshotPath", s_executable_path.parent_path().string()).as<std::string>();
		_screenshot_format = config.get(s_executable_path.string(), "ScreenshotFormat", 0).as<int>();
		_effect_search_paths = split(config.get(s_executable_path.string(), "EffectSearchPaths", s_injector_path.parent_path().string()).as<std::string>(), ',');
		_texture_search_paths = split(config.get(s_executable_path.string(), "TextureSearchPaths", s_injector_path.parent_path().string()).as<std::string>(), ',');
	}
	runtime::~runtime()
	{
		assert(!_is_initialized && !_is_effect_compiled);
	}

	bool runtime::on_init()
	{
		LOG(INFO) << "Recreated runtime environment on runtime " << this << ".";

		_is_initialized = true;

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

		_imgui_font_atlas.reset();

		LOG(INFO) << "Destroyed runtime environment on runtime " << this << ".";

		_width = _height = 0;
		_is_initialized = false;
	}
	void runtime::on_reset_effect()
	{
		if (!_is_effect_compiled)
		{
			return;
		}

		_textures.clear();
		_uniforms.clear();
		_techniques.clear();
		_uniform_data_storage.clear();

		_errors.clear();
		_included_files.clear();

		_is_effect_compiled = false;
	}
	void runtime::on_present()
	{
		const auto time_present = boost::chrono::high_resolution_clock::now();
		_last_frame_duration = boost::chrono::duration_cast<boost::chrono::nanoseconds>(time_present - _last_present);
		const auto time_since_create = boost::chrono::duration_cast<boost::chrono::seconds>(time_present - _last_create);

		if ((_menu_index != 1 || !_show_menu) && (GetAsyncKeyState(_screenshot_key) & 0x8000))
		{
			screenshot();
		}

		draw_overlay();

		struct tm tm;
		const std::time_t time = std::time(nullptr);
		localtime_s(&tm, &time);

		g_network_traffic = 0;
		_last_present = time_present;
		_framecount++;
		_drawcalls = _vertices = 0;
		_date[0] = tm.tm_year + 1900;
		_date[1] = tm.tm_mon + 1;
		_date[2] = tm.tm_mday;
		_date[3] = tm.tm_hour * 3600 + tm.tm_min * 60 + tm.tm_sec;

		_input->next_frame();
	}
	void runtime::on_draw_call(unsigned int vertices)
	{
		_vertices += vertices;
		_drawcalls += 1;
	}
	void runtime::on_apply_effect()
	{
		for (const auto &variable : _uniforms)
		{
			const auto source = variable->annotations["source"].as<std::string>();

			if (source.empty())
			{
				continue;
			}
			else if (source == "frametime")
			{
				const float value = _last_frame_duration.count() * 1e-6f;
				set_uniform_value(*variable, &value, 1);
			}
			else if (source == "framecount" || source == "framecounter")
			{
				switch (variable->basetype)
				{
					case uniform_datatype::bool_:
					{
						const bool even = (_framecount % 2) == 0;
						set_uniform_value(*variable, &even, 1);
						break;
					}
					case uniform_datatype::int_:
					case uniform_datatype::uint_:
					{
						const unsigned int framecount = static_cast<unsigned int>(_framecount % UINT_MAX);
						set_uniform_value(*variable, &framecount, 1);
						break;
					}
					case uniform_datatype::float_:
					{
						const float framecount = static_cast<float>(_framecount % 16777216);
						set_uniform_value(*variable, &framecount, 1);
						break;
					}
				}
			}
			else if (source == "pingpong")
			{
				float value[2] = { 0, 0 };
				get_uniform_value(*variable, value, 2);

				const float min = variable->annotations["min"].as<float>(), max = variable->annotations["max"].as<float>();
				const float stepMin = variable->annotations["step"].as<float>(0), stepMax = variable->annotations["step"].as<float>(1);
				float increment = stepMax == 0 ? stepMin : (stepMin + std::fmodf(static_cast<float>(std::rand()), stepMax - stepMin + 1));
				const float smoothing = variable->annotations["smoothing"].as<float>();

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

				set_uniform_value(*variable, value, 2);
			}
			else if (source == "date")
			{
				set_uniform_value(*variable, _date, 4);
			}
			else if (source == "timer")
			{
				const unsigned long long timer = boost::chrono::duration_cast<boost::chrono::nanoseconds>(_last_present - _start_time).count();

				switch (variable->basetype)
				{
					case uniform_datatype::bool_:
					{
						const bool even = (timer % 2) == 0;
						set_uniform_value(*variable, &even, 1);
						break;
					}
					case uniform_datatype::int_:
					case uniform_datatype::uint_:
					{
						const unsigned int timerInt = static_cast<unsigned int>(timer % UINT_MAX);
						set_uniform_value(*variable, &timerInt, 1);
						break;
					}
					case uniform_datatype::float_:
					{
						const float timerFloat = std::fmod(static_cast<float>(timer * 1e-6f), 16777216.0f);
						set_uniform_value(*variable, &timerFloat, 1);
						break;
					}
				}
			}
			else if (source == "key")
			{
				const int key = variable->annotations["keycode"].as<int>();

				if (key > 0 && key < 256)
				{
					if (variable->annotations["toggle"].as<bool>())
					{
						bool current = false;
						get_uniform_value(*variable, &current, 1);

						if (_input->is_key_pressed(key))
						{
							current = !current;

							set_uniform_value(*variable, &current, 1);
						}
					}
					else
					{
						const bool state = _input->is_key_down(key);

						set_uniform_value(*variable, &state, 1);
					}
				}
			}
			else if (source == "mousepoint")
			{
				const float values[2] = { static_cast<float>(_input->mouse_position_x()), static_cast<float>(_input->mouse_position_y()) };

				set_uniform_value(*variable, values, 2);
			}
			else if (source == "mousebutton")
			{
				const int index = variable->annotations["keycode"].as<int>();

				if (index > 0 && index < 3)
				{
					if (variable->annotations["toggle"].as<bool>())
					{
						bool current = false;
						get_uniform_value(*variable, &current, 1);

						if (_input->is_mouse_button_pressed(index))
						{
							current = !current;

							set_uniform_value(*variable, &current, 1);
						}
					}
					else
					{
						const bool state = _input->is_mouse_button_down(index);

						set_uniform_value(*variable, &state, 1);
					}
				}
			}
			else if (source == "random")
			{
				const int min = variable->annotations["min"].as<int>(), max = variable->annotations["max"].as<int>();
				const int value = min + (std::rand() % (max - min + 1));

				set_uniform_value(*variable, &value, 1);
			}
		}

		for (const auto &technique : _techniques)
		{
			if (technique->toggle_time != 0 && technique->toggle_time == static_cast<int>(_date[3]))
			{
				technique->enabled = !technique->enabled;
				technique->timeleft = technique->timeout;
				technique->toggle_time = 0;
			}
			else if (technique->timeleft > 0)
			{
				technique->timeleft -= static_cast<unsigned int>(boost::chrono::duration_cast<boost::chrono::milliseconds>(_last_frame_duration).count());

				if (technique->timeleft <= 0)
				{
					technique->enabled = !technique->enabled;
					technique->timeleft = 0;
				}
			}
			else if (_input->is_key_pressed(technique->toggle_key) && (!technique->toggle_key_ctrl || _input->is_key_down(VK_CONTROL)) && (!technique->toggle_key_shift || _input->is_key_down(VK_SHIFT)) && (!technique->toggle_key_alt || _input->is_key_down(VK_MENU)))
			{
				technique->enabled = !technique->enabled;
				technique->timeleft = technique->timeout;
			}

			if (!technique->enabled)
			{
				technique->average_duration.clear();

				continue;
			}

			const auto time_technique_started = boost::chrono::high_resolution_clock::now();

			on_apply_effect_technique(*technique);

			technique->average_duration.append(boost::chrono::duration_cast<boost::chrono::nanoseconds>(boost::chrono::high_resolution_clock::now() - time_technique_started).count());
		}
	}
	void runtime::on_apply_effect_technique(const technique &technique)
	{
		for (const auto &variable : _uniforms)
		{
			if (variable->annotations["source"].as<std::string>() == "timeleft")
			{
				set_uniform_value(*variable, &technique.timeleft, 1);
			}
		}
	}

	void runtime::reload()
	{
		on_reset_effect();

		for (const auto &search_path : _effect_search_paths)
		{
			if (!fs::is_directory(search_path))
			{
				continue;
			}

			for (fs::directory_iterator dirit(search_path); dirit != fs::directory_iterator(); dirit++)
			{
				if (dirit->path().extension() != ".fx")
				{
					continue;
				}

				load_effect(dirit->path());
			}
		}
	}
	void runtime::screenshot()
	{
		const int hour = _date[3] / 3600;
		const int minute = (_date[3] - hour * 3600) / 60;
		const int second = _date[3] - hour * 3600 - minute * 60;

		const std::string extensions[] { ".bmp", ".png" };
		const fs::path path = fs::path(_screenshot_path) / (s_executable_path.stem().string() + ' ' +
			std::to_string(_date[0]) + '-' + std::to_string(_date[1]) + '-' + std::to_string(_date[2]) + ' ' +
			std::to_string(hour) + '-' + std::to_string(minute) + '-' + std::to_string(second) +
			extensions[_screenshot_format]);
		std::vector<uint8_t> data(_width * _height * 4);

		screenshot(data.data());

		LOG(INFO) << "Saving screenshot to " << obfuscate_path(path) << " ...";

		bool success = false;

		switch (_screenshot_format)
		{
			case 0:
				success = stbi_write_bmp(path.string().c_str(), _width, _height, 4, data.data()) != 0;
				break;
			case 1:
				success = stbi_write_png(path.string().c_str(), _width, _height, 4, data.data(), 0) != 0;
				break;
		}

		if (!success)
		{
			LOG(ERROR) << "Failed to write screenshot to " << obfuscate_path(path) << "!";
		}
	}

	bool runtime::load_effect(const fs::path &path)
	{
		if (!fs::exists(path))
		{
			LOG(ERROR) << "Effect file " << path << " does not exist.";

			return false;
		}

		fx::syntax_tree ast;
		std::string source_code, errors;
		std::vector<std::string> pragmas;

		// Pre-process
		fx::preprocessor pp(pragmas, source_code, errors);
		pp.add_include_path(path.parent_path());
		pp.add_macro_definition("__RESHADE__", std::to_string(VERSION_MAJOR * 10000 + VERSION_MINOR * 100 + VERSION_REVISION));
		pp.add_macro_definition("__VENDOR__", std::to_string(_vendor_id));
		pp.add_macro_definition("__DEVICE__", std::to_string(_device_id));
		pp.add_macro_definition("__RENDERER__", std::to_string(_renderer_id));
		pp.add_macro_definition("__APPLICATION__", std::to_string(std::hash<std::string>()(s_executable_name)));
		pp.add_macro_definition("BUFFER_WIDTH", std::to_string(_width));
		pp.add_macro_definition("BUFFER_HEIGHT", std::to_string(_height));
		pp.add_macro_definition("BUFFER_RCP_WIDTH", std::to_string(1.0f / static_cast<float>(_width)));
		pp.add_macro_definition("BUFFER_RCP_HEIGHT", std::to_string(1.0f / static_cast<float>(_height)));

		LOG(INFO) << "Loading effect from " << obfuscate_path(path) << " ...";
		LOG(TRACE) << "> Running preprocessor ...";

		std::vector<fs::path> included_files;

		if (!pp.run(path, _included_files))
		{
			_errors += errors;
			LOG(ERROR) << "Failed to pre-process effect on context " << this << ":\n\n" << errors << "\n";

			return false;
		}

		_effect_files.push_back(path.string());
		_included_files.push_back(path);

		for (const auto &pragma : pragmas)
		{
			fx::lexer lexer(pragma);

			const auto prefix_token = lexer.lex();

			if (prefix_token.literal_as_string == "message")
			{
				const auto message_token = lexer.lex();

				if (message_token == fx::lexer::tokenid::string_literal)
				{
					_message += message_token.literal_as_string;
				}
				continue;
			}
		}

		// Parse
		LOG(TRACE) << "> Running parser ...";

		if (!fx::parser(source_code, ast, errors).run())
		{
			_errors += errors;
			LOG(ERROR) << "Failed to compile effect on context " << this << ":\n\n" << errors << "\n";

			return false;
		}

		// Compile
		LOG(TRACE) << "> Running compiler ...";

		if (!update_effect(ast, pragmas, errors))
		{
			_errors += errors;
			LOG(ERROR) << "Failed to compile effect on context " << this << ":\n\n" << errors << "\n";

			return false;
		}
		else if (!errors.empty())
		{
			_errors += errors;
			LOG(WARNING) << "> Successfully compiled effect with warnings:" << "\n\n" << errors << "\n";
		}
		else
		{
			LOG(INFO) << "> Successfully compiled effect.";
		}

		_is_effect_compiled = true;

		for (const auto &uniform : _uniforms)
		{
			if (uniform->annotations.count("__FILE__"))
			{
				continue;
			}

			uniform->annotations["__FILE__"] = path.string();
		}
		for (const auto &texture : _textures)
		{
			if (texture->annotations.count("__FILE__"))
			{
				continue;
			}

			texture->annotations["__FILE__"] = path.string();
		}
		for (const auto &technique : _techniques)
		{
			if (technique->annotations.count("__FILE__"))
			{
				continue;
			}

			technique->annotations["__FILE__"] = path.string();

			technique->enabled = technique->annotations["enabled"].as<bool>();
			technique->timeleft = technique->timeout = technique->annotations["timeout"].as<int>();
			technique->toggle_key = technique->annotations["toggle"].as<int>();
			technique->toggle_key_ctrl = technique->annotations["togglectrl"].as<bool>();
			technique->toggle_key_shift = technique->annotations["toggleshift"].as<bool>();
			technique->toggle_key_alt = technique->annotations["togglealt"].as<bool>();
			technique->toggle_time = technique->annotations["toggletime"].as<int>();
		}

		// Reorder techniques
		auto order = split(config.get(s_executable_path.string(), "Techniques").as<std::string>(), ',');
		std::sort(_techniques.begin(), _techniques.end(),
			[&order](const std::unique_ptr<technique> &lhs, const std::unique_ptr<technique> &rhs)
			{
				return (std::find(order.begin(), order.end(), lhs->name) - order.begin()) < (std::find(order.begin(), order.end(), rhs->name) - order.begin());
			});
		for (const auto &technique : _techniques)
		{
			technique->enabled = std::find(order.begin(), order.end(), technique->name) != order.end();
		}

		return true;
	}
	void runtime::load_textures()
	{
		for (const auto &texture : _textures)
		{
			const std::string source = texture->annotations["source"].as<std::string>();

			if (!source.empty())
			{
				const fs::path path = fs::absolute(source, s_injector_path.parent_path());
				int widthFile = 0, heightFile = 0, channelsFile = 0, channels = STBI_default;

				switch (texture->format)
				{
					case texture_format::r8:
						channels = STBI_r;
						break;
					case texture_format::rg8:
						channels = STBI_rg;
						break;
					case texture_format::dxt1:
						channels = STBI_rgb;
						break;
					case texture_format::rgba8:
					case texture_format::dxt5:
						channels = STBI_rgba;
						break;
					default:
						LOG(ERROR) << "> Texture " << texture->name << " uses unsupported format ('R32F'/'RGBA16'/'RGBA16F'/'RGBA32F'/'DXT3'/'LATC1'/'LATC2') for image loading.";
						continue;
				}

				size_t data_size = texture->width * texture->height * channels;
				unsigned char *const filedata = stbi_load(path.string().c_str(), &widthFile, &heightFile, &channelsFile, channels);
				std::unique_ptr<unsigned char[]> data(new unsigned char[data_size]);

				if (filedata != nullptr)
				{
					if (texture->width != static_cast<unsigned int>(widthFile) || texture->height != static_cast<unsigned int>(heightFile))
					{
						LOG(INFO) << "> Resizing image data for texture '" << texture->name << "' from " << widthFile << "x" << heightFile << " to " << texture->width << "x" << texture->height << " ...";

						stbir_resize_uint8(filedata, widthFile, heightFile, 0, data.get(), texture->width, texture->height, 0, channels);
					}
					else
					{
						std::memcpy(data.get(), filedata, data_size);
					}

					stbi_image_free(filedata);

					switch (texture->format)
					{
						case texture_format::dxt1:
							stb_compress_dxt_block(data.get(), data.get(), FALSE, STB_DXT_NORMAL);
							data_size = ((texture->width + 3) >> 2) * ((texture->height + 3) >> 2) * 8;
							break;
						case texture_format::dxt5:
							stb_compress_dxt_block(data.get(), data.get(), TRUE, STB_DXT_NORMAL);
							data_size = ((texture->width + 3) >> 2) * ((texture->height + 3) >> 2) * 16;
							break;
					}

					update_texture(*texture, data.get(), data_size);
				}
				else
				{
					_errors += "Unable to load source for texture '" + texture->name + "'!";

					LOG(ERROR) << "> Source " << obfuscate_path(path) << " for texture '" << texture->name << "' could not be loaded! Make sure it exists and of a compatible format.";
				}

				texture->storage_size = data_size;
			}
		}
	}
	void runtime::save_configuration()
	{
		std::string technique_list, effect_search_paths_list, texture_search_paths_list;

		for (const auto &technique : _techniques)
		{
			if (technique->enabled)
			{
				technique_list += technique->name + ',';
			}
		}

		for (const auto &path : _effect_search_paths)
		{
			effect_search_paths_list += path + ',';
		}
		for (const auto &path : _texture_search_paths)
		{
			texture_search_paths_list += path + ',';
		}

		config.set(s_executable_path.string(), "MenuKey", _menu_key);
		config.set(s_executable_path.string(), "ScreenshotKey", _screenshot_key);
		config.set(s_executable_path.string(), "ScreenshotPath", _screenshot_path);
		config.set(s_executable_path.string(), "ScreenshotFormat", _screenshot_format);
		config.set(s_executable_path.string(), "Techniques", technique_list);
		config.set(s_executable_path.string(), "EffectSearchPaths", effect_search_paths_list);
		config.set(s_executable_path.string(), "TextureSearchPaths", texture_search_paths_list);
	}

	void runtime::draw_overlay()
	{
		if (_input->is_key_pressed(_menu_key))
		{
			_show_menu = !_show_menu;
		}

		const utils::critical_section::lock lock(_imgui_cs);

		auto &imgui_io = ImGui::GetIO();
		imgui_io.DeltaTime = _last_frame_duration.count() * 1e-9f;
		imgui_io.DisplaySize.x = static_cast<float>(_width);
		imgui_io.DisplaySize.y = static_cast<float>(_height);
		imgui_io.Fonts->TexID = _imgui_font_atlas.get();

		imgui_io.MousePos.x = static_cast<float>(_input->mouse_position_x());
		imgui_io.MousePos.y = static_cast<float>(_input->mouse_position_y());
		imgui_io.MouseWheel += _input->mouse_wheel_delta();
		imgui_io.KeyCtrl = _input->is_key_down(VK_CONTROL);
		imgui_io.KeyShift = _input->is_key_down(VK_SHIFT);
		imgui_io.KeyAlt = _input->is_key_down(VK_MENU);

		for (unsigned int i = 0; i < 5; i++)
		{
			imgui_io.MouseDown[i] = _input->is_mouse_button_down(i);
		}
		for (unsigned int i = 0; i < 256; i++)
		{
			imgui_io.KeysDown[i] = _input->is_key_down(i);

			if (!_input->is_key_pressed(i))
			{
				continue;
			}

			imgui_io.AddInputCharacter(_input->key_to_text(i));
		}

		ImGui::NewFrame();

		if (boost::chrono::duration_cast<boost::chrono::seconds>(_last_present - _start_time).count() < 8)
		{
			ImGui::SetNextWindowPos(ImVec2(0, 0));
			ImGui::Begin("##logo", nullptr, ImVec2(), 0.0f, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoInputs);
			ImGui::TextColored(ImVec4(0.73f, 0.73f, 0.73f, 1.0f), "ReShade " VERSION_STRING_FILE " by crosire");
			ImGui::TextColored(ImVec4(0.73f, 0.73f, 0.73f, 1.0f), "Visit http://reshade.me for news, updates, shaders and discussion.");
			ImGui::End();
		}

		if (_show_menu)
		{
			ImGui::SetNextWindowSize(ImVec2(600, 500), ImGuiSetCond_Once);
			ImGui::SetNextWindowPosCenter(ImGuiSetCond_Once);
			ImGui::Begin("ReShade " VERSION_STRING_PRODUCT " by crosire", &_show_menu, ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoCollapse);

			if (ImGui::BeginMenuBar())
			{
				ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImGui::GetStyle().ItemSpacing * 2);

				const char *menu_items[] = { "Home", "Settings", "Statistics" };

				for (int i = 0; i < _countof(menu_items); i++)
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
					draw_home();
					break;
				case 1:
					draw_settings();
					break;
				case 2:
					draw_statistics();
					break;
			}

			ImGui::End();
		}

		if (_show_shader_editor)
		{
			ImGui::SetNextWindowSize(ImVec2(500, 500), ImGuiSetCond_Once);
			ImGui::Begin("Shader Editor", &_show_shader_editor);
			draw_shader_editor();
			ImGui::End();
		}
		if (_show_macro_editor)
		{
			ImGui::SetNextWindowSize(ImVec2(500, 500), ImGuiSetCond_Once);
			ImGui::Begin("Macro Editor", &_show_macro_editor);
			draw_macro_editor();
			ImGui::End();
		}
		if (_show_uniform_editor)
		{
			ImGui::SetNextWindowSize(ImVec2(500, 300), ImGuiSetCond_Once);
			ImGui::Begin("Uniform Editor", &_show_uniform_editor);
			draw_uniform_editor();
			ImGui::End();
		}

		ImGui::Render();

		_input->block_mouse_input(imgui_io.WantCaptureMouse);
		_input->block_keyboard_input(imgui_io.WantCaptureKeyboard);

		render_draw_lists(ImGui::GetDrawData());
	}
	void runtime::draw_home()
	{
		if (ImGui::Button("Reload", ImVec2(-1, 0)))
		{
			reload();
		}

		if (ImGui::Button("Open Shader Editor", ImVec2(-1, 0)))
		{
			_show_shader_editor = true;
		}
		if (ImGui::Button("Open Macro Editor", ImVec2(-1, 0)))
		{
			_show_macro_editor = true;
		}
		if (ImGui::Button("Open Uniform Editor", ImVec2(-1, 0)))
		{
			_show_uniform_editor = true;
		}

		ImGui::Text("Techniques");

		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("Click on a technique to enable/disable it.\nClick and then drag an item to a new location in the list to change the execution order.");
		}

		if (ImGui::BeginChild("##techniques", ImVec2(-1, -130), true, 0))
		{
			for (size_t n = 0; n < _techniques.size(); n++)
			{
				ImGui::PushID(n);

				const auto name = _techniques[n]->name + " [" + _techniques[n]->annotations["__FILE__"].as<std::string>() + "]";

				if (ImGui::Checkbox(name.c_str(), &_techniques[n]->enabled))
				{
					save_configuration();
				}

				if (ImGui::IsItemActive())
				{
					_selected_technique = n;
				}
				if (ImGui::IsItemHoveredRect())
				{
					_hovered_technique = n;
				}

				ImGui::PopID();
			}
		}

		ImGui::EndChild();

		if (ImGui::IsMouseDragging() && _selected_technique >= 0)
		{
			ImGui::SetTooltip(_techniques[_selected_technique]->name.c_str());

			if (_hovered_technique >= 0 && _hovered_technique != _selected_technique)
			{
				std::swap(_techniques[_hovered_technique], _techniques[_selected_technique]);
				_selected_technique = _hovered_technique;

				save_configuration();
			}
		}
		else
		{
			_selected_technique = _hovered_technique = -1;
		}

		ImGui::Text("Effect Compiler Log");

		if (ImGui::BeginChild("##errors", ImVec2(-1, -1), true, ImGuiWindowFlags_HorizontalScrollbar))
		{
			ImGui::TextColored(ImVec4(1, 0, 0, 1), _errors.c_str());
		}

		ImGui::EndChild();
	}
	void runtime::draw_settings()
	{
		char edit_buffer[2048];

		if (ImGui::CollapsingHeader("General", "settings_general", true, true))
		{
			ImGui::InputText("Overlay Key", const_cast<char *>(keyboard_keys[_menu_key]), sizeof(*keyboard_keys), ImGuiInputTextFlags_ReadOnly);

			if (ImGui::IsItemActive())
			{
				for (unsigned int i = 0; i < 256; i++)
				{
					if (_input->is_key_pressed(i))
					{
						_menu_key = i;
						save_configuration();
						break;
					}
				}
			}
			else if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("Click in the field and press any key to change the shortcut to that key.");
			}

			for (size_t i = 0, offset = 0; i < _effect_search_paths.size(); i++)
			{
				memcpy(edit_buffer + offset, _effect_search_paths[i].c_str(), _effect_search_paths[i].size());
				offset += _effect_search_paths[i].size();
				edit_buffer[offset++] = '\n';
				edit_buffer[offset] = '\0';
			}

			if (ImGui::InputTextMultiline("Effect Search Paths", edit_buffer, sizeof(edit_buffer), ImVec2(0, 100)))
			{
				_effect_search_paths = split(edit_buffer, '\n');

				save_configuration();
			}

			for (size_t i = 0, offset = 0; i < _texture_search_paths.size(); i++)
			{
				memcpy(edit_buffer + offset, _texture_search_paths[i].c_str(), _texture_search_paths[i].size());
				offset += _texture_search_paths[i].size();
				edit_buffer[offset++] = '\n';
				edit_buffer[offset] = '\0';
			}

			if (ImGui::InputTextMultiline("Texture Search Paths", edit_buffer, sizeof(edit_buffer), ImVec2(0, 100)))
			{
				_texture_search_paths = split(edit_buffer, '\n');

				save_configuration();
			}
		}

		if (ImGui::CollapsingHeader("Screenshots", "settings_screenshots", true, true))
		{
			ImGui::InputText("Screenshot Key", const_cast<char *>(keyboard_keys[_screenshot_key]), sizeof(*keyboard_keys), ImGuiInputTextFlags_ReadOnly);

			if (ImGui::IsItemActive())
			{
				for (unsigned int i = 0; i < 256; i++)
				{
					if (_input->is_key_pressed(i) || (i == VK_SNAPSHOT && GetAsyncKeyState(VK_SNAPSHOT) & 0x8000))
					{
						_screenshot_key = i;
						save_configuration();
						break;
					}
				}
			}
			else if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("Click in the field and press any key to change the shortcut to that key.");
			}

			memcpy(edit_buffer, _screenshot_path.c_str(), _screenshot_path.size() + 1);

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
	}
	void runtime::draw_shader_editor()
	{
		std::string effect_files;
		for (const auto &path : _included_files)
		{
			effect_files += path.string() + '\0';
		}

		ImGui::PushItemWidth(-1);
		if (ImGui::Combo("##effect_files", &_current_effect_file, effect_files.c_str()))
		{
			std::ifstream file(_included_files[_current_effect_file].c_str());
			std::copy(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>(), _shader_edit_buffer.begin());
		}

		ImGui::Separator();

		if (!_errors.empty() && ImGui::BeginChildFrame(0, ImVec2(-1, 80)))
		{
			ImGui::TextColored(ImVec4(1, 0, 0, 1), _errors.c_str());
			ImGui::EndChildFrame();
		}

		ImGui::InputTextMultiline("##editor", _shader_edit_buffer.data(), _shader_edit_buffer.size(), ImVec2(-1, -20), ImGuiInputTextFlags_AllowTabInput);

		if (ImGui::Button("Save", ImVec2(-1, 0)) || (_input->is_key_down(VK_CONTROL) && _input->is_key_pressed('S')))
		{
			std::ofstream file(_included_files[_current_effect_file].c_str(), std::ios::out | std::ios::trunc);
			file << _shader_edit_buffer.data();
			file.close();

			reload();
		}
	}
	void runtime::draw_macro_editor()
	{
		std::string effect_files;
		for (const auto &path : _included_files)
		{
			effect_files += path.string() + '\0';
		}

		ImGui::PushItemWidth(-1);
		if (ImGui::Combo("##effect_files", &_current_effect_file, effect_files.c_str()))
		{
		}

		ImGui::Separator();
	}
	void runtime::draw_uniform_editor()
	{
		bool opened = true;
		std::string current_header, new_header;

		for (size_t i = 0; i < _uniforms.size(); i++)
		{
			const auto &uniform = _uniforms[i];

			new_header = uniform->annotations["__FILE__"].as<std::string>();

			if (new_header != current_header)
			{
				current_header = new_header;
				opened = ImGui::CollapsingHeader(new_header.c_str());
			}
			if (!opened)
			{
				continue;
			}

			float data[4] = { };
			get_uniform_value(*uniform, data, 4);

			const auto ui_type = uniform->annotations["ui_type"].as<std::string>();

			ImGui::PushID(i);

			if (ui_type == "input")
			{
				ImGui::InputFloat4(uniform->name.c_str(), data);
			}
			else if (ui_type == "drag")
			{
				ImGui::DragFloat4(uniform->name.c_str(), data, uniform->annotations["ui_step"].as<float>(), uniform->annotations["ui_min"].as<float>(), uniform->annotations["ui_max"].as<float>());
			}
			else if (ui_type == "slider")
			{
				ImGui::SliderFloat4(uniform->name.c_str(), data, uniform->annotations["ui_min"].as<float>(), uniform->annotations["ui_max"].as<float>());
			}
			else if (ui_type == "color")
			{
				ImGui::ColorEdit4(uniform->name.c_str(), data);
			}
			else
			{
				ImGui::InputFloat4(uniform->name.c_str(), data, -1, ImGuiInputTextFlags_ReadOnly);
			}

			ImGui::PopID();

			set_uniform_value(*uniform, data, 4);
		}
	}
	void runtime::draw_statistics()
	{
		const auto state = static_cast<ImGuiState *>(ImGui::GetInternalState());

		if (ImGui::CollapsingHeader("General", "statistics_general", true, true))
		{
			ImGui::Text("Application: %X", std::hash<std::string>()(s_executable_name));
			ImGui::Text("Date: %d-%d-%d %d", _date[0], _date[1], _date[2], _date[3]);
			ImGui::Text("Device: %X %d", _vendor_id, _device_id);
			ImGui::Text("FPS: %.2f", ImGui::GetIO().Framerate);
			ImGui::PushItemWidth(-1);
			ImGui::PlotLines("##framerate", state->FramerateSecPerFrame, 120, state->FramerateSecPerFrameIdx, nullptr, state->FramerateSecPerFrameAccum / 120 * 0.5f, state->FramerateSecPerFrameAccum / 120 * 1.5f, ImVec2(0, 50));
			ImGui::Text("Draw Calls: %u (%u vertices)", _drawcalls, _vertices);
			ImGui::Text("Frame %llu: %fms", _framecount + 1, _last_frame_duration.count() * 1e-6f);
			ImGui::Text("Timer: %fms", std::fmod(boost::chrono::duration_cast<boost::chrono::nanoseconds>(_last_present - _start_time).count() * 1e-6f, 16777216.0f));
			ImGui::Text("Network: %uB", g_network_traffic);
		}
		if (ImGui::CollapsingHeader("Textures", "statistics_textures", true, true))
		{
			for (const auto &texture : _textures)
			{
				ImGui::Text("%s: %ux%u+%u (%uB)", texture->name.c_str(), texture->width, texture->height, (texture->levels - 1), static_cast<unsigned int>(texture->storage_size));
			}
		}
		if (ImGui::CollapsingHeader("Techniques", "statistics_techniques", true, true))
		{
			for (const auto &technique : _techniques)
			{
				ImGui::Text("%s (%u passes): %fms", technique->name.c_str(), static_cast<unsigned int>(technique->passes.size()), (technique->average_duration * 1e-6f));
			}
		}
	}
}
