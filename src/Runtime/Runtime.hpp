#pragma once

#include "Utils\Time.hpp"

#include <string>
#include <vector>
#include <unordered_map>
#include <boost\chrono.hpp>
#include <boost\filesystem\path.hpp>

#pragma region Forward Declarations
namespace ReShade
{
	class GUI;
	class WindowWatcher;

	namespace FX
	{
		class NodeTree;
	}
}
#pragma endregion

namespace ReShade
{
	extern volatile long NetworkUpload;

	// -----------------------------------------------------------------------------------------------------

	struct Annotation
	{
		Annotation()
		{
		}
		Annotation(bool value)
		{
			this->mValue[0] = value ? "1" : "0";
		}
		Annotation(const bool values[4])
		{
			this->mValue[0] = values[0] ? "1" : "0";
			this->mValue[1] = values[1] ? "1" : "0";
			this->mValue[2] = values[2] ? "1" : "0";
			this->mValue[3] = values[3] ? "1" : "0";
		}
		Annotation(int value)
		{
			this->mValue[0] = std::to_string(value);
		}
		Annotation(const int values[4])
		{
			this->mValue[0] = std::to_string(values[0]);
			this->mValue[1] = std::to_string(values[1]);
			this->mValue[2] = std::to_string(values[2]);
			this->mValue[3] = std::to_string(values[3]);
		}
		Annotation(unsigned int value)
		{
			this->mValue[0] = std::to_string(value);
		}
		Annotation(const unsigned int values[4])
		{
			this->mValue[0] = std::to_string(values[0]);
			this->mValue[1] = std::to_string(values[1]);
			this->mValue[2] = std::to_string(values[2]);
			this->mValue[3] = std::to_string(values[3]);
		}
		Annotation(float value)
		{
			this->mValue[0] = std::to_string(value);
		}
		Annotation(const float values[4])
		{
			this->mValue[0] = std::to_string(values[0]);
			this->mValue[1] = std::to_string(values[1]);
			this->mValue[2] = std::to_string(values[2]);
			this->mValue[3] = std::to_string(values[3]);
		}
		Annotation(const std::string &value)
		{
			this->mValue[0] = value;
		}

		template <typename T>
		const T As(std::size_t index = 0) const;
		template <>
		inline const bool As(std::size_t i) const
		{
			return As<int>(i) != 0 || (this->mValue[i] == "true" || this->mValue[i] == "True" || this->mValue[i] == "TRUE");
		}
		template <>
		inline const int As(std::size_t i) const
		{
			return static_cast<int>(std::strtol(this->mValue[i].c_str(), nullptr, 10));
		}
		template <>
		inline const unsigned int As(std::size_t i) const
		{
			return static_cast<unsigned int>(std::strtoul(this->mValue[i].c_str(), nullptr, 10));
		}
		template <>
		inline const float As(std::size_t i) const
		{
			return static_cast<float>(std::strtod(this->mValue[i].c_str(), nullptr));
		}
		template <>
		inline const double As(std::size_t i) const
		{
			return std::strtod(this->mValue[i].c_str(), nullptr);
		}
		template <>
		inline const std::string As(std::size_t i) const
		{
			return this->mValue[i];
		}
		template <>
		inline const char *const As(std::size_t i) const
		{
			return this->mValue[i].c_str();
		}

	private:
		std::string mValue[4];
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
		std::size_t StorageSize;
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
		std::size_t StorageOffset, StorageSize;
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

	// -----------------------------------------------------------------------------------------------------

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

		Runtime();
		virtual ~Runtime();

		inline unsigned int GetBufferWidth() const
		{
			return this->mWidth;
		}
		inline unsigned int GetBufferHeight() const
		{
			return this->mHeight;
		}
		inline const WindowWatcher *GetWindow() const
		{
			return this->mWindow.get();
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
		virtual bool UpdateEffect(const FX::NodeTree &ast, const std::vector<std::string> &pragmas, std::string &errors) = 0;
		/// <summary>
		/// Update the image data of the specified <paramref name="texture"/>.
		/// </summary>
		/// <param name="texture">The texture to update.</param>
		/// <param name="data">The image data to update the texture to.</param>
		/// <param name="size">The size of the image in <paramref name="data"/>.</param>
		virtual bool UpdateTexture(Texture *texture, const unsigned char *data, std::size_t size) = 0;
		/// <summary>
		/// Get the value of the specified <paramref name="variable"/>.
		/// </summary>
		/// <param name="variable">The variable to retrieve the value from.</param>
		/// <param name="data">The buffer to store the value in.</param>
		/// <param name="size">The size of the buffer in <paramref name="data"/>.</param>
		void GetEffectValue(const Uniform &variable, unsigned char *data, std::size_t size) const;
		void GetEffectValue(const Uniform &variable, bool *values, std::size_t count) const;
		void GetEffectValue(const Uniform &variable, int *values, std::size_t count) const;
		void GetEffectValue(const Uniform &variable, unsigned int *values, std::size_t count) const;
		void GetEffectValue(const Uniform &variable, float *values, std::size_t count) const;
		/// <summary>
		/// Update the value of the specified <paramref name="variable"/>.
		/// </summary>
		/// <param name="variable">The variable to update.</param>
		/// <param name="data">The value data to update the variable to.</param>
		/// <param name="size">The size of the value in <paramref name="data"/>.</param>
		virtual void SetEffectValue(Uniform &variable, const unsigned char *data, std::size_t size);
		void SetEffectValue(Uniform &variable, const bool *values, std::size_t count);
		void SetEffectValue(Uniform &variable, const int *values, std::size_t count);
		void SetEffectValue(Uniform &variable, const unsigned int *values, std::size_t count);
		void SetEffectValue(Uniform &variable, const float *values, std::size_t count);

		bool mIsInitialized, mIsEffectCompiled;
		unsigned int mWidth, mHeight;
		unsigned int mVendorId, mDeviceId, mRendererId;
		Statistics mStats;
		std::unique_ptr<GUI> mGUI;
		std::unique_ptr<WindowWatcher> mWindow;
		std::vector<std::unique_ptr<Texture>> mTextures;
		std::vector<std::unique_ptr<Uniform>> mUniforms;
		std::vector<std::unique_ptr<Technique>> mTechniques;
		std::vector<unsigned char> mUniformDataStorage;

	private:
		bool LoadEffect();
		bool CompileEffect();
		void ProcessEffect();

		std::vector<std::string> mPragmas;
		std::vector<boost::filesystem::path> mIncludedFiles;
		boost::chrono::high_resolution_clock::time_point mStartTime, mLastCreate, mLastPresent;
		boost::chrono::high_resolution_clock::duration mLastFrameDuration, mLastPostProcessingDuration;
		unsigned int mCompileStep;
		std::string mStatus, mErrors, mMessage, mEffectSource;
		std::string mScreenshotFormat;
		boost::filesystem::path mScreenshotPath;
		unsigned int mScreenshotKey;
		bool mShowStatistics, mShowFPS, mShowClock, mShowToggleMessage;
	};
}
