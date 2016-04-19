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
#include <imgui.h>

namespace reshade
{
	namespace
	{
		boost::filesystem::path obfuscate_path(const boost::filesystem::path &path)
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

		std::unique_ptr<utils::file_watcher> s_effect_filewatcher;
		boost::filesystem::path s_executable_path, s_injector_path, s_effect_path;
		std::string s_executable_name;
	}

	void runtime::startup(const boost::filesystem::path &executable_path, const boost::filesystem::path &injector_path)
	{
		s_injector_path = injector_path;
		s_executable_path = executable_path;
		s_executable_name = executable_path.stem().string();

		boost::filesystem::path log_path = injector_path, tracelog_path = injector_path;
		log_path.replace_extension("log");
		tracelog_path.replace_extension("tracelog");

		if (boost::filesystem::exists(tracelog_path))
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
		const boost::filesystem::path system_path = system_path_buffer;

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

		s_effect_filewatcher.reset(new utils::file_watcher(s_injector_path.parent_path()));

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

		s_effect_filewatcher.reset();

		input::uninstall();
		hooks::uninstall();

		LOG(INFO) << "Exited.";
	}

	runtime::runtime(uint32_t renderer) :
		_renderer_id(renderer),
		_start_time(boost::chrono::high_resolution_clock::now()),
		_screenshot_format("png"),
		_screenshot_path(s_executable_path.parent_path()),
		_screenshot_key(VK_SNAPSHOT)
	{
	}
	runtime::~runtime()
	{
		assert(!_is_initialized && !_is_effect_compiled);
	}

	bool runtime::on_init()
	{
		_compile_step = 1;

		LOG(INFO) << "Recreated runtime environment on runtime " << this << ".";

		_is_initialized = true;

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

		_is_effect_compiled = false;
	}
	void runtime::on_present()
	{
		const std::time_t time = std::time(nullptr);
		const auto time_present = boost::chrono::high_resolution_clock::now();
		const auto frametime = boost::chrono::duration_cast<boost::chrono::nanoseconds>(time_present - _last_present);
		const auto time_since_create = boost::chrono::duration_cast<boost::chrono::seconds>(time_present - _last_create);

		tm tm;
		localtime_s(&tm, &time);

		#pragma region Screenshot
		if (GetAsyncKeyState(_screenshot_key) & 0x8000)
		{
			char time_string[128];
			std::strftime(time_string, 128, "%Y-%m-%d %H-%M-%S", &tm);
			const boost::filesystem::path path = _screenshot_path / (s_executable_path.stem().string() + ' ' + time_string + '.' + _screenshot_format);
			std::vector<uint8_t> data(_width * _height * 4);

			screenshot(data.data());

			LOG(INFO) << "Saving screenshot to " << obfuscate_path(path) << " ...";

			bool success = false;

			if (path.extension() == ".bmp")
			{
				success = stbi_write_bmp(path.string().c_str(), _width, _height, 4, data.data()) != 0;
			}
			else if (path.extension() == ".png")
			{
				success = stbi_write_png(path.string().c_str(), _width, _height, 4, data.data(), 0) != 0;
			}

			if (!success)
			{
				LOG(ERROR) << "Failed to write screenshot to " << obfuscate_path(path) << "!";
			}
		}
		#pragma endregion

		#pragma region Compile effect
		std::vector<boost::filesystem::path> modifications, matchedmodifications;

		if (s_effect_filewatcher->check(modifications))
		{
			std::sort(modifications.begin(), modifications.end());
			std::sort(_included_files.begin(), _included_files.end());
			std::set_intersection(modifications.begin(), modifications.end(), _included_files.begin(), _included_files.end(), std::back_inserter(matchedmodifications));

			if (!matchedmodifications.empty())
			{
				LOG(INFO) << "Detected modification to " << obfuscate_path(matchedmodifications.front()) << ". Reloading ...";

				_compile_step = 1;
			}
		}

		if (_compile_step != 0)
		{
			_last_create = time_present;

			switch (_compile_step)
			{
				case 1:
					_status = "Loading effect ...";
					_compile_step++;
					_compile_count++;
					break;
				case 2:
					_compile_step = load_effect() ? 3 : 0;
					break;
				case 3:
					_status = "Compiling effect ...";
					_compile_step++;
					break;
				case 4:
					_compile_step = compile_effect() ? 5 : 0;
					break;
				case 5:
					process_effect();
					_compile_step = 0;
					break;
			}
		}
		#pragma endregion

		draw_overlay(frametime.count() * 1e-9f);

		if (_is_effect_compiled && time_since_create.count() > (!_show_info_messages && _compile_count <= 1 ? 12 : _errors.empty() ? 4 : 8))
		{
			_status.clear();
			_message.clear();
		}

		#pragma region Update statistics
		g_network_traffic = 0;
		_last_present = time_present;
		_last_frame_duration = frametime;
		_framecount++;
		_drawcalls = _vertices = 0;
		_average_frametime.append(frametime.count());
		#pragma endregion
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
		const auto time_postprocessing_started = boost::chrono::high_resolution_clock::now();

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

				if (_show_toggle_message)
				{
					_status = technique->name + (technique->enabled ? " enabled." : " disabled.");
					_last_create = time_postprocessing_started;
				}
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

		_last_postprocessing_duration = boost::chrono::high_resolution_clock::now() - time_postprocessing_started;
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

	void runtime::draw_overlay(float dt)
	{
		const utils::critical_section::lock lock(_imgui_cs);

		auto &imgui_io = ImGui::GetIO();
		imgui_io.DeltaTime = dt;
		imgui_io.DisplaySize.x = static_cast<float>(_width);
		imgui_io.DisplaySize.y = static_cast<float>(_height);
		imgui_io.Fonts->TexID = _imgui_font_atlas.get();

		imgui_io.MousePos.x = static_cast<float>(_input->mouse_position_x());
		imgui_io.MousePos.y = static_cast<float>(_input->mouse_position_y());
		imgui_io.MouseWheel += _input->mouse_wheel_delta();
		imgui_io.KeyCtrl = _input->is_key_down(VK_CONTROL);
		imgui_io.KeyShift = _input->is_key_down(VK_SHIFT);
		imgui_io.KeyAlt = _input->is_key_down(VK_MENU);

		BYTE kbs[256];
		GetKeyboardState(kbs);

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

			WORD ch = 0;

			if (ToAscii(i, MapVirtualKey(i, MAPVK_VK_TO_VSC), kbs, &ch, 0))
			{
				imgui_io.AddInputCharacter(ch);
			}
		}

		ImGui::NewFrame();

		tm tm;
		const std::time_t time = std::time(nullptr);
		localtime_s(&tm, &time);

		if (_show_info_messages || _compile_count <= 1)
		{
			if (!_status.empty())
			{
				ImGui::SetNextWindowPos(ImVec2());
				ImGui::Begin("About", nullptr, ImVec2(), 0.0f, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoInputs);
				ImGui::TextColored(ImVec4(0.73f, 0.73f, 0.73f, 1.0f), "ReShade " BOOST_STRINGIZE(VERSION_FULL) " by crosire");
				ImGui::TextColored(ImVec4(0.73f, 0.73f, 0.73f, 1.0f), "Visit http://reshade.me for news, updates, shaders and discussion.");
				ImGui::TextColored(ImVec4(0.73f, 0.73f, 0.73f, 1.0f), _status.c_str());

				if (!_errors.empty() && _compile_step == 0)
				{
					ImGui::TextColored(_is_effect_compiled ? ImVec4(1.0f, 1.0f, 0.0f, 1.0f) : ImVec4(1.0f, 0.0f, 0.0f, 1.0f), _errors.c_str());
				}

				ImGui::End();
			}

			if (!_message.empty() && _is_effect_compiled)
			{
				ImGui::Begin("About");
				ImGui::TextColored(ImVec4(0.73f, 0.73f, 0.73f, 1.0f), _message.c_str());
				ImGui::End();
			}
		}

		if (_show_clock)
		{
			ImGui::Begin("Clock", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize);
			ImGui::Text("%.2d:%.2d", tm.tm_hour, tm.tm_min);
			ImGui::End();
		}
		if (_show_fps)
		{
			ImGui::Begin("FPS", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize);
			ImGui::Text("FPS: %.2f", (1e9f / _average_frametime));
			ImGui::End();
		}
		if (_show_statistics)
		{
			ImGui::Begin("Statistics");

			if (ImGui::CollapsingHeader("General", nullptr, true, true))
			{
				ImGui::Text("Application: %X", std::hash<std::string>()(s_executable_name));
				ImGui::Text("Date: %d-%d-%d %d", static_cast<int>(_date[0]), static_cast<int>(_date[1]), static_cast<int>(_date[2]), static_cast<int>(_date[3]));
				ImGui::Text("Device: %X %d", _vendor_id, _device_id);
				ImGui::Text("FPS: %.2f", (1e9f / _average_frametime));
				ImGui::Text("Draw Calls: %u (%u vertices)", _drawcalls, _vertices);
				ImGui::Text("Frame %llu: %fs", (_framecount + 1), dt);
				ImGui::Text("PostProcessing: %fms", (boost::chrono::duration_cast<boost::chrono::nanoseconds>(_last_postprocessing_duration).count() * 1e-6f));
				ImGui::Text("Timer: %fms", std::fmod(boost::chrono::duration_cast<boost::chrono::nanoseconds>(_last_present - _start_time).count() * 1e-6f, 16777216.0f));
				ImGui::Text("Network: %uB", g_network_traffic);
			}
			if (ImGui::CollapsingHeader("Textures", nullptr, true, true))
			{
				for (const auto &texture : _textures)
				{
					ImGui::Text("%s: %ux%u+%u (%uB)", texture->name.c_str(), texture->width, texture->height, (texture->levels - 1), static_cast<unsigned int>(texture->storage_size));
				}
			}
			if (ImGui::CollapsingHeader("Techniques", nullptr, true, true))
			{
				for (const auto &technique : _techniques)
				{
					ImGui::Text("%s (%u passes): %fms", technique->name.c_str(), static_cast<unsigned int>(technique->passes.size()), (technique->average_duration * 1e-6f));
				}
			}

			ImGui::End();
		}

		ImGui::Render();

		_input->block_mouse_input(imgui_io.WantCaptureMouse);
		_input->block_keyboard_input(imgui_io.WantCaptureKeyboard);

		render_draw_lists(ImGui::GetDrawData());
	}

	bool runtime::load_effect()
	{
		_errors.clear();
		_message.clear();
		_pragmas.clear();
		_effect_source.clear();
		_included_files.clear();
		_show_statistics = _show_fps = _show_clock = _show_toggle_message = false;
		_screenshot_key = VK_SNAPSHOT;
		_screenshot_path = s_executable_path.parent_path();
		_screenshot_format = "png";

		s_effect_path = s_injector_path;
		s_effect_path.replace_extension("fx");

		if (!boost::filesystem::exists(s_effect_path))
		{
			s_effect_path = s_effect_path.parent_path() / "ReShade.fx";

			if (!boost::filesystem::exists(s_effect_path))
			{
				LOG(ERROR) << "Effect file " << s_effect_path << " does not exist.";

				_status += " No effect found!";

				return false;
			}
		}

		if ((GetFileAttributes(s_effect_path.c_str()) & FILE_ATTRIBUTE_REPARSE_POINT) != 0)
		{
			TCHAR path2[MAX_PATH] = { 0 };
			const HANDLE handle = CreateFile(s_effect_path.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
			assert(handle != INVALID_HANDLE_VALUE);
			GetFinalPathNameByHandle(handle, path2, MAX_PATH, 0);
			CloseHandle(handle);

			// Uh oh, shouldn't do this. Get's rid of the //?/ prefix.
			s_effect_path = path2 + 4;

			s_effect_filewatcher.reset(new utils::file_watcher(s_effect_path.parent_path()));
		}

		tm tm;
		std::time_t time = std::time(nullptr);
		::localtime_s(&tm, &time);

		auto executable_name = s_executable_name;
		std::replace(executable_name.begin(), executable_name.end(), ' ', '_');

		// Pre-process
		fx::preprocessor preprocessor(_pragmas, _effect_source, _errors);
		preprocessor.add_include_path(s_effect_path.parent_path());
		preprocessor.add_macro_definition("__RESHADE__", std::to_string(VERSION_MAJOR * 10000 + VERSION_MINOR * 100 + VERSION_REVISION));
		preprocessor.add_macro_definition("__VENDOR__", std::to_string(_vendor_id));
		preprocessor.add_macro_definition("__DEVICE__", std::to_string(_device_id));
		preprocessor.add_macro_definition("__RENDERER__", std::to_string(_renderer_id));
		preprocessor.add_macro_definition("__APPLICATION__", std::to_string(std::hash<std::string>()(s_executable_name)));
		preprocessor.add_macro_definition("__APPLICATION_NAME__", executable_name);
		preprocessor.add_macro_definition("__DATE_YEAR__", std::to_string(tm.tm_year + 1900));
		preprocessor.add_macro_definition("__DATE_MONTH__", std::to_string(tm.tm_mday));
		preprocessor.add_macro_definition("__DATE_DAY__", std::to_string(tm.tm_mon + 1));
		preprocessor.add_macro_definition("BUFFER_WIDTH", std::to_string(_width));
		preprocessor.add_macro_definition("BUFFER_HEIGHT", std::to_string(_height));
		preprocessor.add_macro_definition("BUFFER_RCP_WIDTH", std::to_string(1.0f / static_cast<float>(_width)));
		preprocessor.add_macro_definition("BUFFER_RCP_HEIGHT", std::to_string(1.0f / static_cast<float>(_height)));

		LOG(INFO) << "Loading effect from " << obfuscate_path(s_effect_path) << " ...";
		LOG(TRACE) << "> Running preprocessor ...";

		if (!preprocessor.run(s_effect_path, _included_files))
		{
			LOG(ERROR) << "Failed to preprocess effect on context " << this << ":\n\n" << _errors << "\n";

			_status += " Failed!";

			on_reset_effect();

			return false;
		}

		_included_files.push_back(s_effect_path);

		_show_info_messages = true;

		for (const auto &pragma : _pragmas)
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
			else if (prefix_token.literal_as_string != "reshade")
			{
				continue;
			}

			const auto command_token = lexer.lex();

			if (command_token.literal_as_string == "showstatistics")
			{
				_show_statistics = true;
			}
			else if (command_token.literal_as_string == "showfps")
			{
				_show_fps = true;
			}
			else if (command_token.literal_as_string == "showclock")
			{
				_show_clock = true;
			}
			else if (command_token.literal_as_string == "showtogglemessage")
			{
				_show_toggle_message = true;
			}
			else if (command_token.literal_as_string == "noinfomessages")
			{
				_show_info_messages = false;
			}
			else if (command_token.literal_as_string == "screenshot_key")
			{
				const auto screenshot_key_token = lexer.lex();

				if (screenshot_key_token == fx::lexer::tokenid::int_literal || screenshot_key_token == fx::lexer::tokenid::uint_literal)
				{
					_screenshot_key = screenshot_key_token.literal_as_uint;
				}
			}
			else if (command_token.literal_as_string == "screenshot_format")
			{
				const auto screenshot_format_token = lexer.lex();

				if (screenshot_format_token == fx::lexer::tokenid::identifier || screenshot_format_token == fx::lexer::tokenid::string_literal)
				{
					_screenshot_format = screenshot_format_token.literal_as_string;
				}
			}
			else if (command_token.literal_as_string == "screenshot_location")
			{
				const auto screenshot_location_token = lexer.lex();

				if (screenshot_location_token == fx::lexer::tokenid::string_literal)
				{
					if (boost::filesystem::exists(screenshot_location_token.literal_as_string))
					{
						_screenshot_path = screenshot_location_token.literal_as_string;
					}
					else
					{
						LOG(ERROR) << "Failed to find screenshot location \"" << screenshot_location_token.literal_as_string << "\".";
					}
				}
			}
		}

		return true;
	}
	bool runtime::compile_effect()
	{
		on_reset_effect();

		LOG(TRACE) << "> Running parser ...";

		fx::nodetree ast;

		const bool success = fx::parser(_effect_source, ast, _errors).run();

		if (!success)
		{
			LOG(ERROR) << "Failed to compile effect on context " << this << ":\n\n" << _errors << "\n";

			_status += " Failed!";

			return false;
		}

		// Compile
		LOG(TRACE) << "> Running compiler ...";

		_is_effect_compiled = update_effect(ast, _pragmas, _errors);

		if (!_is_effect_compiled)
		{
			LOG(ERROR) << "Failed to compile effect on context " << this << ":\n\n" << _errors << "\n";

			_status += " Failed!";

			return false;
		}
		else if (!_errors.empty())
		{
			LOG(WARNING) << "> Successfully compiled effect with warnings:" << "\n\n" << _errors << "\n";

			_status += " Succeeded!";
		}
		else
		{
			LOG(INFO) << "> Successfully compiled effect.";

			_status += " Succeeded!";
		}

		return true;
	}
	void runtime::process_effect()
	{
		if (_techniques.empty())
		{
			LOG(WARNING) << "> Effect doesn't contain any techniques.";
			return;
		}

		for (const auto &texture : _textures)
		{
			const std::string source = texture->annotations["source"].as<std::string>();

			if (!source.empty())
			{
				const boost::filesystem::path path = boost::filesystem::absolute(source, s_effect_path.parent_path());
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
		for (const auto &technique : _techniques)
		{
			technique->enabled = technique->annotations["enabled"].as<bool>();
			technique->timeleft = technique->timeout = technique->annotations["timeout"].as<int>();
			technique->toggle_key = technique->annotations["toggle"].as<int>();
			technique->toggle_key_ctrl = technique->annotations["togglectrl"].as<bool>();
			technique->toggle_key_shift = technique->annotations["toggleshift"].as<bool>();
			technique->toggle_key_alt = technique->annotations["togglealt"].as<bool>();
			technique->toggle_time = technique->annotations["toggletime"].as<int>();
		}
	}
}
