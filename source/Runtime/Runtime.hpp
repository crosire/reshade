#pragma once

#include "Utils\Time.hpp"

#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <boost\chrono.hpp>
#include <boost\filesystem\path.hpp>

#pragma region Forward Declarations
namespace ReShade
{
	class GUI;
	class Input;

	namespace FX
	{
		class nodetree;
	}
}
#pragma endregion

namespace ReShade
{
	extern volatile long NetworkUpload;

	// ---------------------------------------------------------------------------------------------------

	struct Annotation
	{
		Annotation()
		{
		}
		Annotation(bool value)
		{
			_value[0] = value ? "1" : "0";
		}
		Annotation(const bool values[4])
		{
			_value[0] = values[0] ? "1" : "0";
			_value[1] = values[1] ? "1" : "0";
			_value[2] = values[2] ? "1" : "0";
			_value[3] = values[3] ? "1" : "0";
		}
		Annotation(int value)
		{
			_value[0] = std::to_string(value);
		}
		Annotation(const int values[4])
		{
			_value[0] = std::to_string(values[0]);
			_value[1] = std::to_string(values[1]);
			_value[2] = std::to_string(values[2]);
			_value[3] = std::to_string(values[3]);
		}
		Annotation(unsigned int value)
		{
			_value[0] = std::to_string(value);
		}
		Annotation(const unsigned int values[4])
		{
			_value[0] = std::to_string(values[0]);
			_value[1] = std::to_string(values[1]);
			_value[2] = std::to_string(values[2]);
			_value[3] = std::to_string(values[3]);
		}
		Annotation(float value)
		{
			_value[0] = std::to_string(value);
		}
		Annotation(const float values[4])
		{
			_value[0] = std::to_string(values[0]);
			_value[1] = std::to_string(values[1]);
			_value[2] = std::to_string(values[2]);
			_value[3] = std::to_string(values[3]);
		}
		Annotation(const std::string &value)
		{
			_value[0] = value;
		}

		template <typename T>
		const T As(size_t index = 0) const;
		template <>
		inline const bool As(size_t i) const
		{
			return As<int>(i) != 0 || (_value[i] == "true" || _value[i] == "True" || _value[i] == "TRUE");
		}
		template <>
		inline const int As(size_t i) const
		{
			return static_cast<int>(std::strtol(_value[i].c_str(), nullptr, 10));
		}
		template <>
		inline const unsigned int As(size_t i) const
		{
			return static_cast<unsigned int>(std::strtoul(_value[i].c_str(), nullptr, 10));
		}
		template <>
		inline const float As(size_t i) const
		{
			return static_cast<float>(std::strtod(_value[i].c_str(), nullptr));
		}
		template <>
		inline const double As(size_t i) const
		{
			return std::strtod(_value[i].c_str(), nullptr);
		}
		template <>
		inline const std::string As(size_t i) const
		{
			return _value[i];
		}
		template <>
		inline const char *const As(size_t i) const
		{
			return _value[i].c_str();
		}

	private:
		std::string _value[4];
	};
	struct Texture abstract
	{
		enum class PixelFormat
		{
			Unknown,

			R8,
			R16F,
			R32F,
			RG8,
			RG16,
			RG16F,
			RG32F,
			RGBA8,
			RGBA16,
			RGBA16F,
			RGBA32F,

			DXT1,
			DXT3,
			DXT5,
			LATC1,
			LATC2
		};

		Texture() : Width(0), Height(0), Levels(0), Format(PixelFormat::Unknown), StorageSize(0) { }
		virtual ~Texture() { }

		std::string Name;
		unsigned int Width, Height, Levels;
		PixelFormat Format;
		size_t StorageSize;
		std::unordered_map<std::string, Annotation> Annotations;
	};
	struct Uniform
	{
		enum class Type
		{
			Bool,
			Int,
			Uint,
			Float
		};

		Uniform() : BaseType(Type::Float), Rows(0), Columns(0), Elements(0), StorageOffset(0), StorageSize(0) { }
		virtual ~Uniform() { }

		std::string Name;
		Type BaseType;
		unsigned int Rows, Columns, Elements;
		size_t StorageOffset, StorageSize;
		std::unordered_map<std::string, Annotation> Annotations;
	};
	struct Technique abstract
	{
		Technique() : PassCount(0), Enabled(false), Timeout(0), Timeleft(0), Toggle(0), ToggleTime(0), ToggleCtrl(false), ToggleShift(false), ToggleAlt(false) { }
		virtual ~Technique() { }

		std::string Name;
		unsigned int PassCount;
		std::unordered_map<std::string, Annotation> Annotations;
		bool Enabled;
		int Timeout, Timeleft;
		int Toggle, ToggleTime;
		bool ToggleCtrl, ToggleShift, ToggleAlt;
		boost::chrono::high_resolution_clock::duration LastDuration;
		boost::chrono::high_resolution_clock::time_point LastDurationUpdate;
	};

	// ---------------------------------------------------------------------------------------------------

	class Runtime abstract
	{
	public:
		struct Statistics
		{
			Utils::Framerate FrameRate;
			unsigned long long FrameCount;
			unsigned int DrawCalls, Vertices;
			float Date[4];
		};

		/// <summary>
		/// Initialize ReShade. Registers hooks and starts logging.
		/// </summary>
		/// <param name="executablePath">Path to the executable ReShade was injected into.</param>
		/// <param name="injectorPath">Path to the ReShade module itself.</param>
		static void Startup(const boost::filesystem::path &executablePath, const boost::filesystem::path &injectorPath);
		/// <summary>
		/// Shut down ReShade. Removes all installed hooks and cleans up.
		/// </summary>
		static void Shutdown();

		Runtime(unsigned int renderer);
		virtual ~Runtime();

		unsigned int GetBufferWidth() const
		{
			return _width;
		}
		unsigned int GetBufferHeight() const
		{
			return _height;
		}
		const Input *GetInput() const
		{
			return _input;
		}

	protected:
		/// <summary>
		/// Callback function called when the runtime is initialized.
		/// </summary>
		/// <returns>Returns if the initialization succeeded.</returns>
		virtual bool OnInit();
		/// <summary>
		/// Callback function called when the runtime is uninitialized.
		/// </summary>
		virtual void OnReset();
		/// <summary>
		/// Callback function called when the post-processing effects are uninitialized.
		/// </summary>
		virtual void OnResetEffect();
		/// <summary>
		/// Callback function called at the end of each frame.
		/// </summary>
		virtual void OnPresent();
		/// <summary>
		/// Callback function called at every draw call.
		/// </summary>
		/// <param name="vertices">The number of vertices this draw call generates.</param>
		virtual void OnDrawCall(unsigned int vertices);
		/// <summary>
		/// Callback function called to apply the post-processing effects to the screen.
		/// </summary>
		virtual void OnApplyEffect();
		/// <summary>
		/// Callback function called to render the specified effect technique.
		/// </summary>
		/// <param name="technique">The technique to render.</param>
		virtual void OnApplyEffectTechnique(const Technique *technique);

		/// <summary>
		/// Create a copy of the current image on the screen.
		/// </summary>
		/// <param name="buffer">The buffer to save the copy to. It has to be the size of at least "WIDTH * HEIGHT * 4".</param>
		virtual void Screenshot(unsigned char *buffer) const = 0;
		/// <summary>
		/// Compile effect from the specified abstract syntax tree and initialize textures, constants and techniques.
		/// </summary>
		/// <param name="ast">The abstract syntax tree representation of the effect to compile.</param>
		/// <param name="errors">A string buffer to store any errors that occur during compilation.</param>
		virtual bool UpdateEffect(const FX::nodetree &ast, const std::vector<std::string> &pragmas, std::string &errors) = 0;
		/// <summary>
		/// Update the image data of the specified <paramref name="texture"/>.
		/// </summary>
		/// <param name="texture">The texture to update.</param>
		/// <param name="data">The image data to update the texture to.</param>
		/// <param name="size">The size of the image in <paramref name="data"/>.</param>
		virtual bool UpdateTexture(Texture *texture, const unsigned char *data, size_t size) = 0;
		/// <summary>
		/// Get the value of the specified <paramref name="variable"/>.
		/// </summary>
		/// <param name="variable">The variable to retrieve the value from.</param>
		/// <param name="data">The buffer to store the value in.</param>
		/// <param name="size">The size of the buffer in <paramref name="data"/>.</param>
		void GetEffectValue(const Uniform &variable, unsigned char *data, size_t size) const;
		void GetEffectValue(const Uniform &variable, bool *values, size_t count) const;
		void GetEffectValue(const Uniform &variable, int *values, size_t count) const;
		void GetEffectValue(const Uniform &variable, unsigned int *values, size_t count) const;
		void GetEffectValue(const Uniform &variable, float *values, size_t count) const;
		/// <summary>
		/// Update the value of the specified <paramref name="variable"/>.
		/// </summary>
		/// <param name="variable">The variable to update.</param>
		/// <param name="data">The value data to update the variable to.</param>
		/// <param name="size">The size of the value in <paramref name="data"/>.</param>
		virtual void SetEffectValue(Uniform &variable, const unsigned char *data, size_t size);
		void SetEffectValue(Uniform &variable, const bool *values, size_t count);
		void SetEffectValue(Uniform &variable, const int *values, size_t count);
		void SetEffectValue(Uniform &variable, const unsigned int *values, size_t count);
		void SetEffectValue(Uniform &variable, const float *values, size_t count);

		bool _isInitialized, _isEffectCompiled;
		unsigned int _width, _height;
		unsigned int _vendorId, _deviceId;
		Statistics _stats;
		std::unique_ptr<GUI> _gui;
		Input *_input;
		std::vector<std::unique_ptr<Texture>> _textures;
		std::vector<std::unique_ptr<Uniform>> _uniforms;
		std::vector<std::unique_ptr<Technique>> _techniques;
		std::vector<unsigned char> _uniformDataStorage;

	private:
		bool LoadEffect();
		bool CompileEffect();
		void ProcessEffect();

		unsigned int _rendererId;
		std::vector<std::string> _pragmas;
		std::vector<boost::filesystem::path> _includedFiles;
		boost::chrono::high_resolution_clock::time_point _startTime, _lastCreate, _lastPresent;
		boost::chrono::high_resolution_clock::duration _lastFrameDuration, _lastPostProcessingDuration;
		unsigned int _compileStep, _compileCount;
		std::string _status, _errors, _message, _effectSource;
		std::string _screenshotFormat;
		boost::filesystem::path _screenshotPath;
		unsigned int _screenshotKey;
		bool _showStatistics, _showFPS, _showClock, _showToggleMessage, _showInfoMessages;
	};
}
