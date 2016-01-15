#include "Log.hpp"
#include "Version.h"
#include "Runtime.hpp"
#include "HookManager.hpp"
#include "FX\Parser.hpp"
#include "FX\PreProcessor.hpp"
#include "GUI.hpp"
#include "Input.hpp"
#include "Utils\FileWatcher.hpp"

#include <iterator>
#include <assert.h>
#include <stb_dxt.h>
#include <stb_image.h>
#include <stb_image_write.h>
#include <stb_image_resize.h>
#include <boost\filesystem\operations.hpp>

namespace ReShade
{
	namespace
	{
		boost::filesystem::path ObfuscatePath(const boost::filesystem::path &path)
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
		inline boost::filesystem::path GetSystemDirectory()
		{
			TCHAR path[MAX_PATH];
			::GetSystemDirectory(path, MAX_PATH);
			return path;
		}

		std::unique_ptr<FileWatcher> sEffectWatcher;
		boost::filesystem::path sExecutablePath, sInjectorPath, sEffectPath;
	}

	volatile long NetworkUpload = 0;

	// ---------------------------------------------------------------------------------------------------

	void Runtime::Startup(const boost::filesystem::path &executablePath, const boost::filesystem::path &injectorPath)
	{
		sInjectorPath = injectorPath;
		sExecutablePath = executablePath;

		boost::filesystem::path logPath = injectorPath, logTracePath = injectorPath;
		logPath.replace_extension("log");
		logTracePath.replace_extension("tracelog");

		if (boost::filesystem::exists(logTracePath))
		{
			Log::Open(logTracePath, Log::Level::Trace);
		}
		else
		{
			Log::Open(logPath, Log::Level::Info);
		}

		LOG(INFO) << "Initializing crosire's ReShade version '" BOOST_STRINGIZE(VERSION_FULL) "' built on '" VERSION_DATE " " VERSION_TIME "' loaded from " << ObfuscatePath(injectorPath) << " to " << ObfuscatePath(executablePath) << " ...";

		const boost::filesystem::path systemPath = GetSystemDirectory();

		Hooks::RegisterModule(systemPath / "d3d8.dll");
		Hooks::RegisterModule(systemPath / "d3d9.dll");
		Hooks::RegisterModule(systemPath / "d3d10.dll");
		Hooks::RegisterModule(systemPath / "d3d10_1.dll");
		Hooks::RegisterModule(systemPath / "d3d11.dll");
		Hooks::RegisterModule(systemPath / "d3d12.dll");
		Hooks::RegisterModule(systemPath / "dxgi.dll");
		Hooks::RegisterModule(systemPath / "opengl32.dll");
		Hooks::RegisterModule(systemPath / "user32.dll");
		Hooks::RegisterModule(systemPath / "ws2_32.dll");

		sEffectWatcher.reset(new FileWatcher(sInjectorPath.parent_path(), true));

		LOG(INFO) << "Initialized.";
	}
	void Runtime::Shutdown()
	{
		LOG(INFO) << "Exiting ...";

		Input::UnRegisterRawInputDevices();

		sEffectWatcher.reset();

		Hooks::Uninstall();

		LOG(INFO) << "Exited.";
	}

	// ---------------------------------------------------------------------------------------------------

	Runtime::Runtime(unsigned int renderer) : _isInitialized(false), _isEffectCompiled(false), _width(0), _height(0), _vendorId(0), _deviceId(0), _rendererId(renderer), _stats(), _compileStep(0), _compileCount(0), _screenshotFormat("png"), _screenshotPath(sExecutablePath.parent_path()), _screenshotKey(VK_SNAPSHOT), _showStatistics(false), _showFPS(false), _showClock(false), _showToggleMessage(false), _showInfoMessages(true)
	{
		memset(&_stats, 0, sizeof(Statistics));

		_status = "Initializing ...";
		_startTime = boost::chrono::high_resolution_clock::now();
	}
	Runtime::~Runtime()
	{
		assert(!_isInitialized && !_isEffectCompiled);
	}

	bool Runtime::OnInit()
	{
		_compileStep = 1;

		LOG(INFO) << "Recreated runtime environment on runtime " << this << ".";

		_isInitialized = true;

		return true;
	}
	void Runtime::OnReset()
	{
		OnResetEffect();

		if (!_isInitialized)
		{
			return;
		}

		LOG(INFO) << "Destroyed runtime environment on runtime " << this << ".";

		_width = _height = 0;
		_isInitialized = false;
	}
	void Runtime::OnResetEffect()
	{
		if (!_isEffectCompiled)
		{
			return;
		}

		_textures.clear();
		_uniforms.clear();
		_techniques.clear();
		_uniformDataStorage.clear();

		_isEffectCompiled = false;
	}
	void Runtime::OnPresent()
	{
		const std::time_t time = std::time(nullptr);
		const auto timePresent = boost::chrono::high_resolution_clock::now();
		const auto frametime = boost::chrono::duration_cast<boost::chrono::nanoseconds>(timePresent - _lastPresent);
		const auto timeSinceCreate = boost::chrono::duration_cast<boost::chrono::seconds>(timePresent - _lastCreate);

		tm tm;
		localtime_s(&tm, &time);

		#pragma region Screenshot
		if (GetAsyncKeyState(_screenshotKey) & 0x8000)
		{
			char timeString[128];
			std::strftime(timeString, 128, "%Y-%m-%d %H-%M-%S", &tm);
			const boost::filesystem::path path = _screenshotPath / (sExecutablePath.stem().string() + ' ' + timeString + '.' + _screenshotFormat);
			std::unique_ptr<unsigned char[]> data(new unsigned char[_width * _height * 4]());

			Screenshot(data.get());

			LOG(INFO) << "Saving screenshot to " << ObfuscatePath(path) << " ...";

			bool success = false;

			if (path.extension() == ".bmp")
			{
				success = stbi_write_bmp(path.string().c_str(), _width, _height, 4, data.get()) != 0;
			}
			else if (path.extension() == ".png")
			{
				success = stbi_write_png(path.string().c_str(), _width, _height, 4, data.get(), 0) != 0;
			}

			if (!success)
			{
				LOG(ERROR) << "Failed to write screenshot to " << ObfuscatePath(path) << "!";
			}
		}
		#pragma endregion

		#pragma region Compile effect
		std::vector<boost::filesystem::path> modifications, matchedmodifications;

		if (sEffectWatcher->GetModifications(modifications))
		{
			std::sort(modifications.begin(), modifications.end());
			std::sort(_includedFiles.begin(), _includedFiles.end());
			std::set_intersection(modifications.begin(), modifications.end(), _includedFiles.begin(), _includedFiles.end(), std::back_inserter(matchedmodifications));

			if (!matchedmodifications.empty())
			{
				LOG(INFO) << "Detected modification to " << ObfuscatePath(matchedmodifications.front()) << ". Reloading ...";

				_compileStep = 1;
			}
		}

		if (_compileStep != 0)
		{
			_lastCreate = timePresent;

			switch (_compileStep)
			{
				case 1:
					_status = "Loading effect ...";
					_compileStep++;
					_compileCount++;
					break;
				case 2:
					_compileStep = LoadEffect() ? 3 : 0;
					break;
				case 3:
					_status = "Compiling effect ...";
					_compileStep++;
					break;
				case 4:
					_compileStep = CompileEffect() ? 5 : 0;
					break;
				case 5:
					ProcessEffect();
					_compileStep = 0;
					break;
			}
		}
		#pragma endregion

		#pragma region Draw overlay
		if (_gui != nullptr && _gui->BeginFrame())
		{
			if (_showInfoMessages || _compileCount <= 1)
			{
				if (!_status.empty())
				{
					_gui->AddLabel(22, 0xFFBCBCBC, "ReShade " BOOST_STRINGIZE(VERSION_FULL) " by crosire");
					_gui->AddLabel(16, 0xFFBCBCBC, "Visit http://reshade.me for news, updates, shaders and discussion.");
					_gui->AddLabel(16, 0xFFBCBCBC, _status);

					if (!_errors.empty() && _compileStep == 0)
					{
						if (!_isEffectCompiled)
						{
							_gui->AddLabel(16, 0xFFFF0000, _errors);
						}
						else
						{
							_gui->AddLabel(16, 0xFFFFFF00, _errors);
						}
					}
				}

				if (!_message.empty() && _isEffectCompiled)
				{
					_gui->AddLabel(16, 0xFFBCBCBC, _message);
				}
			}

			std::stringstream stats;

			if (_showClock)
			{
				stats << std::setfill('0') << std::setw(2) << tm.tm_hour << ':' << std::setw(2) << tm.tm_min << std::endl;
			}
			if (_showFPS)
			{
				stats << _stats.FrameRate << std::endl;
			}
			if (_showStatistics)
			{
				stats << "General" << std::endl << "-------" << std::endl;
				stats << "Application: " << std::hash<std::string>()(sExecutablePath.stem().string()) << std::endl;
				stats << "Date: " << static_cast<int>(_stats.Date[0]) << '-' << static_cast<int>(_stats.Date[1]) << '-' << static_cast<int>(_stats.Date[2]) << ' ' << static_cast<int>(_stats.Date[3]) << '\n';
				stats << "Device: " << std::hex << std::uppercase << _vendorId << ' ' << _deviceId << std::nouppercase << std::dec << std::endl;
				stats << "FPS: " << _stats.FrameRate << std::endl;
				stats << "Draw Calls: " << _stats.DrawCalls << " (" << _stats.Vertices << " vertices)" << std::endl;
				stats << "Frame " << (_stats.FrameCount + 1) << ": " << (frametime.count() * 1e-6f) << "ms" << std::endl;
				stats << "PostProcessing: " << (boost::chrono::duration_cast<boost::chrono::nanoseconds>(_lastPostProcessingDuration).count() * 1e-6f) << "ms" << std::endl;
				stats << "Timer: " << std::fmod(boost::chrono::duration_cast<boost::chrono::nanoseconds>(_lastPresent - _startTime).count() * 1e-6f, 16777216.0f) << "ms" << std::endl;
				stats << "Network: " << NetworkUpload << "B up" << std::endl;

				stats << std::endl;
				stats << "Textures" << std::endl << "--------" << std::endl;

				for (const auto &texture : _textures)
				{
					stats << texture->Name << ": " << texture->Width << "x" << texture->Height << "+" << (texture->Levels - 1) << " (" << texture->StorageSize << "B)" << std::endl;
				}

				stats << std::endl;
				stats << "Techniques" << std::endl << "----------" << std::endl;

				for (const auto &technique : _techniques)
				{
					stats << technique->Name << " (" << technique->PassCount << " passes): " << (boost::chrono::duration_cast<boost::chrono::nanoseconds>(technique->LastDuration).count() * 1e-6f) << "ms" << std::endl;
				}
			}

			_gui->DrawDebugText(0, 0, 1, static_cast<float>(_width), 16, 0xFFBCBCBC, stats.str());

			_gui->EndFrame();
		}

		if (_isEffectCompiled && timeSinceCreate.count() > (!_showInfoMessages && _compileCount <= 1 ? 12 : _errors.empty() ? 4 : 8))
		{
			_status.clear();
			_message.clear();
		}
		#pragma endregion

		#pragma region Update statistics
		NetworkUpload = 0;
		_lastPresent = timePresent;
		_lastFrameDuration = frametime;
		_stats.FrameCount++;
		_stats.DrawCalls = _stats.Vertices = 0;
		_stats.FrameRate.Calculate(frametime.count());
		_stats.Date[0] = static_cast<float>(tm.tm_year + 1900);
		_stats.Date[1] = static_cast<float>(tm.tm_mon + 1);
		_stats.Date[2] = static_cast<float>(tm.tm_mday);
		_stats.Date[3] = static_cast<float>(tm.tm_hour * 3600 + tm.tm_min * 60 + tm.tm_sec);
		#pragma endregion

		_input->NextFrame();
	}
	void Runtime::OnDrawCall(unsigned int vertices)
	{
		_stats.Vertices += vertices;
		_stats.DrawCalls += 1;
	}
	void Runtime::OnApplyEffect()
	{
		const auto timePostProcessingStarted = boost::chrono::high_resolution_clock::now();

		for (const auto &variable : _uniforms)
		{
			const std::string source = variable->Annotations["source"].As<std::string>();

			if (source.empty())
			{
				continue;
			}
			else if (source == "frametime")
			{
				const float value = _lastFrameDuration.count() * 1e-6f;
				SetEffectValue(*variable, &value, 1);
			}
			else if (source == "framecount" || source == "framecounter")
			{
				switch (variable->BaseType)
				{
					case Uniform::Type::Bool:
					{
						const bool even = (_stats.FrameCount % 2) == 0;
						SetEffectValue(*variable, &even, 1);
						break;
					}
					case Uniform::Type::Int:
					case Uniform::Type::Uint:
					{
						const unsigned int framecount = static_cast<unsigned int>(_stats.FrameCount % UINT_MAX);
						SetEffectValue(*variable, &framecount, 1);
						break;
					}
					case Uniform::Type::Float:
					{
						const float framecount = static_cast<float>(_stats.FrameCount % 16777216);
						SetEffectValue(*variable, &framecount, 1);
						break;
					}
				}
			}
			else if (source == "pingpong")
			{
				float value[2] = { 0, 0 };
				GetEffectValue(*variable, value, 2);

				const float min = variable->Annotations["min"].As<float>(), max = variable->Annotations["max"].As<float>();
				const float stepMin = variable->Annotations["step"].As<float>(0), stepMax = variable->Annotations["step"].As<float>(1);
				float increment = stepMax == 0 ? stepMin : (stepMin + std::fmodf(static_cast<float>(std::rand()), stepMax - stepMin + 1));
				const float smoothing = variable->Annotations["smoothing"].As<float>();

				if (value[1] >= 0)
				{
					increment = std::max(increment - std::max(0.0f, smoothing - (max - value[0])), 0.05f);
					increment *= _lastFrameDuration.count() * 1e-9f;

					if ((value[0] += increment) >= max)
					{
						value[0] = max;
						value[1] = -1;
					}
				}
				else
				{
					increment = std::max(increment - std::max(0.0f, smoothing - (value[0] - min)), 0.05f);
					increment *= _lastFrameDuration.count() * 1e-9f;

					if ((value[0] -= increment) <= min)
					{
						value[0] = min;
						value[1] = +1;
					}
				}

				SetEffectValue(*variable, value, 2);
			}
			else if (source == "date")
			{
				SetEffectValue(*variable, _stats.Date, 4);
			}
			else if (source == "timer")
			{
				const unsigned long long timer = boost::chrono::duration_cast<boost::chrono::nanoseconds>(_lastPresent - _startTime).count();

				switch (variable->BaseType)
				{
					case Uniform::Type::Bool:
					{
						const bool even = (timer % 2) == 0;
						SetEffectValue(*variable, &even, 1);
						break;
					}
					case Uniform::Type::Int:
					case Uniform::Type::Uint:
					{
						const unsigned int timerInt = static_cast<unsigned int>(timer % UINT_MAX);
						SetEffectValue(*variable, &timerInt, 1);
						break;
					}
					case Uniform::Type::Float:
					{
						const float timerFloat = std::fmod(static_cast<float>(timer * 1e-6f), 16777216.0f);
						SetEffectValue(*variable, &timerFloat, 1);
						break;
					}
				}
			}
			else if (source == "key")
			{
				const int key = variable->Annotations["keycode"].As<int>();

				if (key > 0 && key < 256)
				{
					if (variable->Annotations["toggle"].As<bool>())
					{
						bool current = false;
						GetEffectValue(*variable, &current, 1);

						if (_input->GetKeyJustPressed(key))
						{
							current = !current;

							SetEffectValue(*variable, &current, 1);
						}
					}
					else
					{
						const bool state = _input->GetKeyState(key);

						SetEffectValue(*variable, &state, 1);
					}
				}
			}
			else if (source == "gaze")
			{
				const float values[2] = { _window->GetGazePosition().x, _window->GetGazePosition().y };

				SetEffectValue(*variable, values, 2);
			}
			else if (source == "random")
			{
				const int min = variable->Annotations["min"].As<int>(), max = variable->Annotations["max"].As<int>();
				const int value = min + (std::rand() % (max - min + 1));

				SetEffectValue(*variable, &value, 1);
			}
		}

		for (const auto &technique : _techniques)
		{
			if (technique->ToggleTime != 0 && technique->ToggleTime == static_cast<int>(_stats.Date[3]))
			{
				technique->Enabled = !technique->Enabled;
				technique->Timeleft = technique->Timeout;
				technique->ToggleTime = 0;
			}
			else if (technique->Timeleft > 0)
			{
				technique->Timeleft -= static_cast<unsigned int>(boost::chrono::duration_cast<boost::chrono::milliseconds>(_lastFrameDuration).count());

				if (technique->Timeleft <= 0)
				{
					technique->Enabled = !technique->Enabled;
					technique->Timeleft = 0;
				}
			}
			else if (_input->GetKeyJustPressed(technique->Toggle) && (!technique->ToggleCtrl || _input->GetKeyState(VK_CONTROL)) && (!technique->ToggleShift || _input->GetKeyState(VK_SHIFT)) && (!technique->ToggleAlt || _input->GetKeyState(VK_MENU)))
			{
				technique->Enabled = !technique->Enabled;
				technique->Timeleft = technique->Timeout;

				if (_showToggleMessage)
				{
					_status = technique->Name + (technique->Enabled ? " enabled." : " disabled.");
					_lastCreate = timePostProcessingStarted;
				}
			}

			if (!technique->Enabled)
			{
				technique->LastDuration = boost::chrono::high_resolution_clock::duration(0);

				continue;
			}

			const auto timeTechniqueStarted = boost::chrono::high_resolution_clock::now();

			OnApplyEffectTechnique(technique.get());

			if (boost::chrono::duration_cast<boost::chrono::milliseconds>(timeTechniqueStarted - technique->LastDurationUpdate).count() > 250)
			{
				technique->LastDuration = boost::chrono::high_resolution_clock::now() - timeTechniqueStarted;
				technique->LastDurationUpdate = timeTechniqueStarted;
			}
		}

		_lastPostProcessingDuration = boost::chrono::high_resolution_clock::now() - timePostProcessingStarted;
	}
	void Runtime::OnApplyEffectTechnique(const Technique *technique)
	{
		for (const auto &variable : _uniforms)
		{
			if (variable->Annotations["source"].As<std::string>() == "timeleft")
			{
				SetEffectValue(*variable, &technique->Timeleft, 1);
			}
		}
	}

	void Runtime::GetEffectValue(const Uniform &variable, unsigned char *data, size_t size) const
	{
		assert(data != nullptr);
		assert(_isEffectCompiled);

		size = std::min(size, variable.StorageSize);

		assert(variable.StorageOffset + size < _uniformDataStorage.size());

		std::memcpy(data, &_uniformDataStorage[variable.StorageOffset], size);
	}
	void Runtime::GetEffectValue(const Uniform &variable, bool *values, size_t count) const
	{
		static_assert(sizeof(int) == 4 && sizeof(float) == 4, "expected int and float size to equal 4");

		count = std::min(count, variable.StorageSize / 4);

		assert(values != nullptr);

		unsigned char *const data = static_cast<unsigned char *>(alloca(variable.StorageSize));
		GetEffectValue(variable, data, variable.StorageSize);

		for (size_t i = 0; i < count; i++)
		{
			values[i] = reinterpret_cast<const unsigned int *>(data)[i] != 0;
		}
	}
	void Runtime::GetEffectValue(const Uniform &variable, int *values, size_t count) const
	{
		switch (variable.BaseType)
		{
			case Uniform::Type::Bool:
			case Uniform::Type::Int:
			case Uniform::Type::Uint:
			{
				GetEffectValue(variable, reinterpret_cast<unsigned char *>(values), count * sizeof(int));
				break;
			}
			case Uniform::Type::Float:
			{
				count = std::min(count, variable.StorageSize / sizeof(float));

				assert(values != nullptr);

				unsigned char *const data = static_cast<unsigned char *>(alloca(variable.StorageSize));
				GetEffectValue(variable, data, variable.StorageSize);

				for (size_t i = 0; i < count; i++)
				{
					values[i] = static_cast<int>(reinterpret_cast<const float *>(data)[i]);
				}
				break;
			}
		}
	}
	void Runtime::GetEffectValue(const Uniform &variable, unsigned int *values, size_t count) const
	{
		GetEffectValue(variable, reinterpret_cast<int *>(values), count);
	}
	void Runtime::GetEffectValue(const Uniform &variable, float *values, size_t count) const
	{
		switch (variable.BaseType)
		{
			case Uniform::Type::Bool:
			case Uniform::Type::Int:
			case Uniform::Type::Uint:
			{
				count = std::min(count, variable.StorageSize / sizeof(int));

				assert(values != nullptr);

				unsigned char *const data = static_cast<unsigned char *>(alloca(variable.StorageSize));
				GetEffectValue(variable, data, variable.StorageSize);

				for (size_t i = 0; i < count; ++i)
				{
					if (variable.BaseType != Uniform::Type::Uint)
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
			case Uniform::Type::Float:
			{
				GetEffectValue(variable, reinterpret_cast<unsigned char *>(values), count * sizeof(float));
				break;
			}
		}
	}
	void Runtime::SetEffectValue(Uniform &variable, const unsigned char *data, size_t size)
	{
		assert(data != nullptr);
		assert(_isEffectCompiled);

		size = std::min(size, variable.StorageSize);

		assert(variable.StorageOffset + size < _uniformDataStorage.size());

		std::memcpy(&_uniformDataStorage[variable.StorageOffset], data, size);
	}
	void Runtime::SetEffectValue(Uniform &variable, const bool *values, size_t count)
	{
		static_assert(sizeof(int) == 4 && sizeof(float) == 4, "expected int and float size to equal 4");

		unsigned char *const data = static_cast<unsigned char *>(alloca(count * 4));

		switch (variable.BaseType)
		{
			case Uniform::Type::Bool:
				for (size_t i = 0; i < count; ++i)
				{
					reinterpret_cast<int *>(data)[i] = values[i] ? -1 : 0;
				}
				break;
			case Uniform::Type::Int:
			case Uniform::Type::Uint:
				for (size_t i = 0; i < count; ++i)
				{
					reinterpret_cast<int *>(data)[i] = values[i] ? 1 : 0;
				}
				break;
			case Uniform::Type::Float:
				for (size_t i = 0; i < count; ++i)
				{
					reinterpret_cast<float *>(data)[i] = values[i] ? 1.0f : 0.0f;
				}
				break;
		}

		SetEffectValue(variable, data, count * 4);
	}
	void Runtime::SetEffectValue(Uniform &variable, const int *values, size_t count)
	{
		switch (variable.BaseType)
		{
			case Uniform::Type::Bool:
			case Uniform::Type::Int:
			case Uniform::Type::Uint:
			{
				SetEffectValue(variable, reinterpret_cast<const unsigned char *>(values), count * sizeof(int));
				break;
			}
			case Uniform::Type::Float:
			{
				float *const data = static_cast<float *>(alloca(count * sizeof(float)));

				for (size_t i = 0; i < count; ++i)
				{
					data[i] = static_cast<float>(values[i]);
				}

				SetEffectValue(variable, reinterpret_cast<const unsigned char *>(data), count * sizeof(float));
				break;
			}
		}
	}
	void Runtime::SetEffectValue(Uniform &variable, const unsigned int *values, size_t count)
	{
		switch (variable.BaseType)
		{
			case Uniform::Type::Bool:
			case Uniform::Type::Int:
			case Uniform::Type::Uint:
			{
				SetEffectValue(variable, reinterpret_cast<const unsigned char *>(values), count * sizeof(int));
				break;
			}
			case Uniform::Type::Float:
			{
				float *const data = static_cast<float *>(alloca(count * sizeof(float)));

				for (size_t i = 0; i < count; ++i)
				{
					data[i] = static_cast<float>(values[i]);
				}

				SetEffectValue(variable, reinterpret_cast<const unsigned char *>(data), count * sizeof(float));
				break;
			}
		}
	}
	void Runtime::SetEffectValue(Uniform &variable, const float *values, size_t count)
	{
		switch (variable.BaseType)
		{
			case Uniform::Type::Bool:
			case Uniform::Type::Int:
			case Uniform::Type::Uint:
			{
				int *const data = static_cast<int *>(alloca(count * sizeof(int)));

				for (size_t i = 0; i < count; ++i)
				{
					data[i] = static_cast<int>(values[i]);
				}

				SetEffectValue(variable, reinterpret_cast<const unsigned char *>(data), count * sizeof(int));
				break;
			}
			case Uniform::Type::Float:
			{
				SetEffectValue(variable, reinterpret_cast<const unsigned char *>(values), count * sizeof(float));
				break;
			}
		}
	}

	bool Runtime::LoadEffect()
	{
		_errors.clear();
		_message.clear();
		_effectSource.clear();
		_includedFiles.clear();
		_showStatistics = _showFPS = _showClock = _showToggleMessage = false;
		_screenshotKey = VK_SNAPSHOT;
		_screenshotPath = sExecutablePath.parent_path();
		_screenshotFormat = "png";

		sEffectPath = sInjectorPath;
		sEffectPath.replace_extension("fx");

		if (!boost::filesystem::exists(sEffectPath))
		{
			sEffectPath = sEffectPath.parent_path() / "ReShade.fx";

			if (!boost::filesystem::exists(sEffectPath))
			{
				LOG(ERROR) << "Effect file " << sEffectPath << " does not exist.";

				_status += " No effect found!";

				return false;
			}
		}

		if ((GetFileAttributes(sEffectPath.c_str()) & FILE_ATTRIBUTE_REPARSE_POINT) != 0)
		{
			TCHAR path2[MAX_PATH] = { 0 };
			const HANDLE handle = CreateFile(sEffectPath.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
			assert(handle != INVALID_HANDLE_VALUE);
			GetFinalPathNameByHandle(handle, path2, MAX_PATH, 0);
			CloseHandle(handle);

			// Uh oh, shouldn't do this. Get's rid of the //?/ prefix.
			sEffectPath = path2 + 4;

			sEffectWatcher.reset(new FileWatcher(sEffectPath.parent_path()));
		}

		tm tm;
		std::time_t time = std::time(nullptr);
		::localtime_s(&tm, &time);

		// Pre-process
		std::vector<std::pair<std::string, std::string>> defines;
		defines.emplace_back("__RESHADE__", std::to_string(VERSION_MAJOR * 10000 + VERSION_MINOR * 100 + VERSION_REVISION));
		defines.emplace_back("__VENDOR__", std::to_string(_vendorId));
		defines.emplace_back("__DEVICE__", std::to_string(_deviceId));
		defines.emplace_back("__RENDERER__", std::to_string(_rendererId));
		defines.emplace_back("__APPLICATION__", std::to_string(std::hash<std::string>()(sExecutablePath.stem().string())));
		defines.emplace_back("__DATE_YEAR__", std::to_string(tm.tm_year + 1900));
		defines.emplace_back("__DATE_MONTH__", std::to_string(tm.tm_mday));
		defines.emplace_back("__DATE_DAY__", std::to_string(tm.tm_mon + 1));
		defines.emplace_back("BUFFER_WIDTH", std::to_string(_width));
		defines.emplace_back("BUFFER_HEIGHT", std::to_string(_height));
		defines.emplace_back("BUFFER_RCP_WIDTH", std::to_string(1.0f / static_cast<float>(_width)));
		defines.emplace_back("BUFFER_RCP_HEIGHT", std::to_string(1.0f / static_cast<float>(_height)));

		_includedFiles.push_back(sEffectPath.parent_path());

		LOG(INFO) << "Loading effect from " << ObfuscatePath(sEffectPath) << " ...";
		LOG(TRACE) << "> Running preprocessor ...";

		if (!FX::preprocess(sEffectPath, defines, _includedFiles, _pragmas, _effectSource, _errors))
		{
			LOG(ERROR) << "Failed to preprocess effect on context " << this << ":\n\n" << _errors << "\n";

			_status += " Failed!";

			OnResetEffect();

			return false;
		}

		_includedFiles.push_back(sEffectPath);

		_showInfoMessages = true;

		for (const auto &pragma : _pragmas)
		{
			FX::lexer lexer(pragma);

			const auto prefix_token = lexer.lex();

			if (prefix_token.literal_as_string == "message")
			{
				const auto message_token = lexer.lex();

				if (message_token == FX::lexer::tokenid::string_literal)
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
				_showStatistics = true;
			}
			else if (command_token.literal_as_string == "showfps")
			{
				_showFPS = true;
			}
			else if (command_token.literal_as_string == "showclock")
			{
				_showClock = true;
			}
			else if (command_token.literal_as_string == "showtogglemessage")
			{
				_showToggleMessage = true;
			}
			else if (command_token.literal_as_string == "noinfomessages")
			{
				_showInfoMessages = false;
			}
			else if (command_token.literal_as_string == "screenshot_key")
			{
				const auto screenshot_key_token = lexer.lex();

				if (screenshot_key_token == FX::lexer::tokenid::int_literal || screenshot_key_token == FX::lexer::tokenid::uint_literal)
				{
					_screenshotKey = screenshot_key_token.literal_as_uint;
				}
			}
			else if (command_token.literal_as_string == "screenshot_format")
			{
				const auto screenshot_format_token = lexer.lex();

				if (screenshot_format_token == FX::lexer::tokenid::identifier || screenshot_format_token == FX::lexer::tokenid::string_literal)
				{
					_screenshotFormat = screenshot_format_token.literal_as_string;
				}
			}
			else if (command_token.literal_as_string == "screenshot_location")
			{
				const auto screenshot_location_token = lexer.lex();

				if (screenshot_location_token == FX::lexer::tokenid::string_literal)
				{
					if (boost::filesystem::exists(screenshot_location_token.literal_as_string))
					{
						_screenshotPath = screenshot_location_token.literal_as_string;
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
	bool Runtime::CompileEffect()
	{
		OnResetEffect();

		LOG(TRACE) << "> Running parser ...";

		FX::nodetree ast;

		const bool success = FX::parse(_effectSource, ast, _errors);

		if (!success)
		{
			LOG(ERROR) << "Failed to compile effect on context " << this << ":\n\n" << _errors << "\n";

			_status += " Failed!";

			return false;
		}

		// Compile
		LOG(TRACE) << "> Running compiler ...";

		_isEffectCompiled = UpdateEffect(ast, _pragmas, _errors);

		if (!_isEffectCompiled)
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
	void Runtime::ProcessEffect()
	{
		if (_techniques.empty())
		{
			LOG(WARNING) << "> Effect doesn't contain any techniques.";
			return;
		}

		for (const auto &texture : _textures)
		{
			const std::string source = texture->Annotations["source"].As<std::string>();

			if (!source.empty())
			{
				const boost::filesystem::path path = boost::filesystem::absolute(source, sEffectPath.parent_path());
				int widthFile = 0, heightFile = 0, channelsFile = 0, channels = STBI_default;

				switch (texture->Format)
				{
					case Texture::PixelFormat::R8:
						channels = STBI_r;
						break;
					case Texture::PixelFormat::RG8:
						channels = STBI_rg;
						break;
					case Texture::PixelFormat::DXT1:
						channels = STBI_rgb;
						break;
					case Texture::PixelFormat::RGBA8:
					case Texture::PixelFormat::DXT5:
						channels = STBI_rgba;
						break;
					default:
						LOG(ERROR) << "> Texture " << texture->Name << " uses unsupported format ('R32F'/'RGBA16'/'RGBA16F'/'RGBA32F'/'DXT3'/'LATC1'/'LATC2') for image loading.";
						continue;
				}

				size_t dataSize = texture->Width * texture->Height * channels;
				unsigned char *const dataFile = stbi_load(path.string().c_str(), &widthFile, &heightFile, &channelsFile, channels);
				std::unique_ptr<unsigned char[]> data(new unsigned char[dataSize]);

				if (dataFile != nullptr)
				{
					if (texture->Width != static_cast<unsigned int>(widthFile) || texture->Height != static_cast<unsigned int>(heightFile))
					{
						LOG(INFO) << "> Resizing image data for texture '" << texture->Name << "' from " << widthFile << "x" << heightFile << " to " << texture->Width << "x" << texture->Height << " ...";

						stbir_resize_uint8(dataFile, widthFile, heightFile, 0, data.get(), texture->Width, texture->Height, 0, channels);
					}
					else
					{
						std::memcpy(data.get(), dataFile, dataSize);
					}

					stbi_image_free(dataFile);

					switch (texture->Format)
					{
						case Texture::PixelFormat::DXT1:
							stb_compress_dxt_block(data.get(), data.get(), FALSE, STB_DXT_NORMAL);
							dataSize = ((texture->Width + 3) >> 2) * ((texture->Height + 3) >> 2) * 8;
							break;
						case Texture::PixelFormat::DXT5:
							stb_compress_dxt_block(data.get(), data.get(), TRUE, STB_DXT_NORMAL);
							dataSize = ((texture->Width + 3) >> 2) * ((texture->Height + 3) >> 2) * 16;
							break;
					}

					UpdateTexture(texture.get(), data.get(), dataSize);
				}
				else
				{
					_errors += "Unable to load source for texture '" + texture->Name + "'!";

					LOG(ERROR) << "> Source " << ObfuscatePath(path) << " for texture '" << texture->Name << "' could not be loaded! Make sure it exists and of a compatible format.";
				}

				texture->StorageSize = dataSize;
			}
		}
		for (const auto &technique : _techniques)
		{
			technique->Enabled = technique->Annotations["enabled"].As<bool>();
			technique->Timeleft = technique->Timeout = technique->Annotations["timeout"].As<int>();
			technique->Toggle = technique->Annotations["toggle"].As<int>();
			technique->ToggleCtrl = technique->Annotations["togglectrl"].As<bool>();
			technique->ToggleShift = technique->Annotations["toggleshift"].As<bool>();
			technique->ToggleAlt = technique->Annotations["togglealt"].As<bool>();
			technique->ToggleTime = technique->Annotations["toggletime"].As<int>();
		}
	}
}
