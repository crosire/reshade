#include "log.hpp"
#include "version.h"
#include "runtime.hpp"
#include "hook_manager.hpp"
#include "fx_ast.hpp"
#include "fx_parser.hpp"
#include "fx_preprocessor.hpp"
#include "gui.hpp"
#include "input.hpp"
#include "file_watcher.hpp"

#include <iterator>
#include <stb_dxt.h>
#include <stb_image.h>
#include <stb_image_write.h>
#include <stb_image_resize.h>
#include <boost/filesystem/operations.hpp>

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

	std::atomic<unsigned int> runtime::s_network_traffic(0);

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

		LOG(INFO) << "Initialized.";
	}
	void runtime::shutdown()
	{
		LOG(INFO) << "Exiting ...";

		s_effect_filewatcher.reset();

		input::uninstall();
		hooks::uninstall();

		LOG(INFO) << "Exited.";
	}

	runtime::runtime(unsigned int renderer) : _is_initialized(false), _is_effect_compiled(false), _width(0), _height(0), _vendor_id(0), _device_id(0), _framecount(0), _drawcalls(0), _vertices(0), _input(nullptr), _renderer_id(renderer), _date(), _compile_step(0), _compile_count(0), _screenshot_format("png"), _screenshot_path(s_executable_path.parent_path()), _screenshot_key(VK_SNAPSHOT), _show_statistics(false), _show_fps(false), _show_clock(false), _show_toggle_message(false), _show_info_messages(true)
	{
		_status = "Initializing ...";
		_start_time = boost::chrono::high_resolution_clock::now();
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
			std::vector<unsigned char> data(_width * _height * 4);

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

		#pragma region Draw overlay
		if (_gui != nullptr && _gui->begin_frame())
		{
			if (_show_info_messages || _compile_count <= 1)
			{
				if (!_status.empty())
				{
					_gui->add_label(22, 0xFFBCBCBC, "ReShade " BOOST_STRINGIZE(VERSION_FULL) " by crosire");
					_gui->add_label(16, 0xFFBCBCBC, "Visit http://reshade.me for news, updates, shaders and discussion.");
					_gui->add_label(16, 0xFFBCBCBC, _status);

					if (!_errors.empty() && _compile_step == 0)
					{
						if (!_is_effect_compiled)
						{
							_gui->add_label(16, 0xFFFF0000, _errors);
						}
						else
						{
							_gui->add_label(16, 0xFFFFFF00, _errors);
						}
					}
				}

				if (!_message.empty() && _is_effect_compiled)
				{
					_gui->add_label(16, 0xFFBCBCBC, _message);
				}
			}

			std::stringstream stats;

			if (_show_clock)
			{
				stats << std::setfill('0') << std::setw(2) << tm.tm_hour << ':' << std::setw(2) << tm.tm_min << std::endl;
			}
			if (_show_fps)
			{
				stats << (1e9f / _average_frametime) << std::endl;
			}
			if (_show_statistics)
			{
				stats << "General" << std::endl << "-------" << std::endl;
				stats << "Application: " << std::hash<std::string>()(s_executable_name) << std::endl;
				stats << "Date: " << static_cast<int>(_date[0]) << '-' << static_cast<int>(_date[1]) << '-' << static_cast<int>(_date[2]) << ' ' << static_cast<int>(_date[3]) << '\n';
				stats << "Device: " << std::hex << std::uppercase << _vendor_id << ' ' << _device_id << std::nouppercase << std::dec << std::endl;
				stats << "FPS: " << (1e9f / _average_frametime) << std::endl;
				stats << "Draw Calls: " << _drawcalls << " (" << _vertices << " vertices)" << std::endl;
				stats << "Frame " << (_framecount + 1) << ": " << (frametime.count() * 1e-6f) << "ms" << std::endl;
				stats << "PostProcessing: " << (boost::chrono::duration_cast<boost::chrono::nanoseconds>(_last_postprocessing_duration).count() * 1e-6f) << "ms" << std::endl;
				stats << "Timer: " << std::fmod(boost::chrono::duration_cast<boost::chrono::nanoseconds>(_last_present - _start_time).count() * 1e-6f, 16777216.0f) << "ms" << std::endl;
				stats << "Network: " << s_network_traffic << "B" << std::endl;

				stats << std::endl;
				stats << "Textures" << std::endl << "--------" << std::endl;

				for (const auto &texture : _textures)
				{
					stats << texture->name << ": " << texture->width << "x" << texture->height << "+" << (texture->levels - 1) << " (" << texture->storage_size << "B)" << std::endl;
				}

				stats << std::endl;
				stats << "Techniques" << std::endl << "----------" << std::endl;

				for (const auto &technique : _techniques)
				{
					stats << technique->name << " (" << technique->passes.size() << " passes): " << (technique->average_duration * 1e-6f) << "ms" << std::endl;
				}
			}

			_gui->draw_debug_text(0, 0, 1, static_cast<float>(_width), 16, 0xFFBCBCBC, stats.str());

			_gui->end_frame();
		}

		if (_is_effect_compiled && time_since_create.count() > (!_show_info_messages && _compile_count <= 1 ? 12 : _errors.empty() ? 4 : 8))
		{
			_status.clear();
			_message.clear();
		}
		#pragma endregion

		#pragma region Update statistics
		s_network_traffic = 0;
		_last_present = time_present;
		_last_frame_duration = frametime;
		_framecount++;
		_drawcalls = _vertices = 0;
		_average_frametime.append(frametime.count());
		_date[0] = static_cast<float>(tm.tm_year + 1900);
		_date[1] = static_cast<float>(tm.tm_mon + 1);
		_date[2] = static_cast<float>(tm.tm_mday);
		_date[3] = static_cast<float>(tm.tm_hour * 3600 + tm.tm_min * 60 + tm.tm_sec);
		#pragma endregion

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
					case uniform::datatype::bool_:
					{
						const bool even = (_framecount % 2) == 0;
						set_uniform_value(*variable, &even, 1);
						break;
					}
					case uniform::datatype::int_:
					case uniform::datatype::uint_:
					{
						const unsigned int framecount = static_cast<unsigned int>(_framecount % UINT_MAX);
						set_uniform_value(*variable, &framecount, 1);
						break;
					}
					case uniform::datatype::float_:
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
					case uniform::datatype::bool_:
					{
						const bool even = (timer % 2) == 0;
						set_uniform_value(*variable, &even, 1);
						break;
					}
					case uniform::datatype::int_:
					case uniform::datatype::uint_:
					{
						const unsigned int timerInt = static_cast<unsigned int>(timer % UINT_MAX);
						set_uniform_value(*variable, &timerInt, 1);
						break;
					}
					case uniform::datatype::float_:
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
				const float values[2] = { static_cast<float>(_input->mouse_position().x), static_cast<float>(_input->mouse_position().y) };

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

	texture *runtime::find_texture(const std::string &name)
	{
		const auto it = std::find_if(_textures.begin(), _textures.end(),
			[name](const std::unique_ptr<texture> &item)
			{
				return item->name == name;
			});

		return it != _textures.end() ? it->get() : nullptr;
	}

	void runtime::get_uniform_value(const uniform &variable, unsigned char *data, size_t size) const
	{
		assert(data != nullptr);
		assert(_is_effect_compiled);

		size = std::min(size, variable.storage_size);

		assert(variable.storage_offset + size < _uniform_data_storage.size());

		std::memcpy(data, &_uniform_data_storage[variable.storage_offset], size);
	}
	void runtime::get_uniform_value(const uniform &variable, bool *values, size_t count) const
	{
		static_assert(sizeof(int) == 4 && sizeof(float) == 4, "expected int and float size to equal 4");

		count = std::min(count, variable.storage_size / 4);

		assert(values != nullptr);

		const auto data = static_cast<unsigned char *>(alloca(variable.storage_size));
		get_uniform_value(variable, data, variable.storage_size);

		for (size_t i = 0; i < count; i++)
		{
			values[i] = reinterpret_cast<const unsigned int *>(data)[i] != 0;
		}
	}
	void runtime::get_uniform_value(const uniform &variable, int *values, size_t count) const
	{
		switch (variable.basetype)
		{
			case uniform::datatype::bool_:
			case uniform::datatype::int_:
			case uniform::datatype::uint_:
			{
				get_uniform_value(variable, reinterpret_cast<unsigned char *>(values), count * sizeof(int));
				break;
			}
			case uniform::datatype::float_:
			{
				count = std::min(count, variable.storage_size / sizeof(float));

				assert(values != nullptr);

				const auto data = static_cast<unsigned char *>(alloca(variable.storage_size));
				get_uniform_value(variable, data, variable.storage_size);

				for (size_t i = 0; i < count; i++)
				{
					values[i] = static_cast<int>(reinterpret_cast<const float *>(data)[i]);
				}
				break;
			}
		}
	}
	void runtime::get_uniform_value(const uniform &variable, unsigned int *values, size_t count) const
	{
		get_uniform_value(variable, reinterpret_cast<int *>(values), count);
	}
	void runtime::get_uniform_value(const uniform &variable, float *values, size_t count) const
	{
		switch (variable.basetype)
		{
			case uniform::datatype::bool_:
			case uniform::datatype::int_:
			case uniform::datatype::uint_:
			{
				count = std::min(count, variable.storage_size / sizeof(int));

				assert(values != nullptr);

				const auto data = static_cast<unsigned char *>(alloca(variable.storage_size));
				get_uniform_value(variable, data, variable.storage_size);

				for (size_t i = 0; i < count; ++i)
				{
					if (variable.basetype != uniform::datatype::uint_)
					{
						values[i] = static_cast<float>(reinterpret_cast<const int *>(data)[i]);
					}
					else
					{
						values[i] = static_cast<float>(reinterpret_cast<const unsigned int *>(data)[i]);
					}
				}
				break;
			}
			case uniform::datatype::float_:
			{
				get_uniform_value(variable, reinterpret_cast<unsigned char *>(values), count * sizeof(float));
				break;
			}
		}
	}
	void runtime::set_uniform_value(uniform &variable, const unsigned char *data, size_t size)
	{
		assert(data != nullptr);
		assert(_is_effect_compiled);

		size = std::min(size, variable.storage_size);

		assert(variable.storage_offset + size < _uniform_data_storage.size());

		std::memcpy(&_uniform_data_storage[variable.storage_offset], data, size);
	}
	void runtime::set_uniform_value(uniform &variable, const bool *values, size_t count)
	{
		static_assert(sizeof(int) == 4 && sizeof(float) == 4, "expected int and float size to equal 4");

		const auto data = static_cast<unsigned char *>(alloca(count * 4));

		switch (variable.basetype)
		{
			case uniform::datatype::bool_:
				for (size_t i = 0; i < count; ++i)
				{
					reinterpret_cast<int *>(data)[i] = values[i] ? -1 : 0;
				}
				break;
			case uniform::datatype::int_:
			case uniform::datatype::uint_:
				for (size_t i = 0; i < count; ++i)
				{
					reinterpret_cast<int *>(data)[i] = values[i] ? 1 : 0;
				}
				break;
			case uniform::datatype::float_:
				for (size_t i = 0; i < count; ++i)
				{
					reinterpret_cast<float *>(data)[i] = values[i] ? 1.0f : 0.0f;
				}
				break;
		}

		set_uniform_value(variable, data, count * 4);
	}
	void runtime::set_uniform_value(uniform &variable, const int *values, size_t count)
	{
		switch (variable.basetype)
		{
			case uniform::datatype::bool_:
			case uniform::datatype::int_:
			case uniform::datatype::uint_:
			{
				set_uniform_value(variable, reinterpret_cast<const unsigned char *>(values), count * sizeof(int));
				break;
			}
			case uniform::datatype::float_:
			{
				const auto data = static_cast<float *>(alloca(count * sizeof(float)));

				for (size_t i = 0; i < count; ++i)
				{
					data[i] = static_cast<float>(values[i]);
				}

				set_uniform_value(variable, reinterpret_cast<const unsigned char *>(data), count * sizeof(float));
				break;
			}
		}
	}
	void runtime::set_uniform_value(uniform &variable, const unsigned int *values, size_t count)
	{
		switch (variable.basetype)
		{
			case uniform::datatype::bool_:
			case uniform::datatype::int_:
			case uniform::datatype::uint_:
			{
				set_uniform_value(variable, reinterpret_cast<const unsigned char *>(values), count * sizeof(int));
				break;
			}
			case uniform::datatype::float_:
			{
				const auto data = static_cast<float *>(alloca(count * sizeof(float)));

				for (size_t i = 0; i < count; ++i)
				{
					data[i] = static_cast<float>(values[i]);
				}

				set_uniform_value(variable, reinterpret_cast<const unsigned char *>(data), count * sizeof(float));
				break;
			}
		}
	}
	void runtime::set_uniform_value(uniform &variable, const float *values, size_t count)
	{
		switch (variable.basetype)
		{
			case uniform::datatype::bool_:
			case uniform::datatype::int_:
			case uniform::datatype::uint_:
			{
				const auto data = static_cast<int *>(alloca(count * sizeof(int)));

				for (size_t i = 0; i < count; ++i)
				{
					data[i] = static_cast<int>(values[i]);
				}

				set_uniform_value(variable, reinterpret_cast<const unsigned char *>(data), count * sizeof(int));
				break;
			}
			case uniform::datatype::float_:
			{
				set_uniform_value(variable, reinterpret_cast<const unsigned char *>(values), count * sizeof(float));
				break;
			}
		}
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
					case texture::pixelformat::r8:
						channels = STBI_r;
						break;
					case texture::pixelformat::rg8:
						channels = STBI_rg;
						break;
					case texture::pixelformat::dxt1:
						channels = STBI_rgb;
						break;
					case texture::pixelformat::rgba8:
					case texture::pixelformat::dxt5:
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
						case texture::pixelformat::dxt1:
							stb_compress_dxt_block(data.get(), data.get(), FALSE, STB_DXT_NORMAL);
							data_size = ((texture->width + 3) >> 2) * ((texture->height + 3) >> 2) * 8;
							break;
						case texture::pixelformat::dxt5:
							stb_compress_dxt_block(data.get(), data.get(), TRUE, STB_DXT_NORMAL);
							data_size = ((texture->width + 3) >> 2) * ((texture->height + 3) >> 2) * 16;
							break;
					}

					update_texture(texture.get(), data.get(), data_size);
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
