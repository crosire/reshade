#pragma once

#include <string>
#include <memory>
#include <vector>
#include <unordered_map>

namespace ReShade
{
	class Effect abstract
	{
	public:
		struct Annotation
		{
		public:
			inline Annotation()
			{
			}
			inline Annotation(bool value) : mValue(value ? "1" : "0")
			{
			}
			inline Annotation(int value) : mValue(std::to_string(value))
			{
			}
			inline Annotation(unsigned int value) : mValue(std::to_string(value))
			{
			}
			inline Annotation(float value) : mValue(std::to_string(value))
			{
			}
			inline Annotation(const std::string &value) : mValue(value)
			{
			}
			inline Annotation(const char *value) : mValue(value)
			{
			}

			template <typename T>
			const T As() const;
			template <>
			inline const bool As() const
			{
				return As<int>() != 0 || (this->mValue == "true" || this->mValue == "True" || this->mValue == "TRUE");
			}
			template <>
			inline const int As() const
			{
				return static_cast<int>(std::strtol(this->mValue.c_str(), nullptr, 10));
			}
			template <>
			inline const unsigned int As() const
			{
				return static_cast<unsigned int>(std::strtoul(this->mValue.c_str(), nullptr, 10));
			}
			template <>
			inline const float As() const
			{
				return static_cast<float>(std::strtod(this->mValue.c_str(), nullptr));
			}
			template <>
			inline const double As() const
			{
				return std::strtod(this->mValue.c_str(), nullptr);
			}
			template <>
			inline const std::string As() const
			{
				return this->mValue;
			}
			template <>
			inline const char *const As() const
			{
				return this->mValue.c_str();
			}

		private:
			std::string mValue;
		};
		class Texture
		{
		public:
			enum class Format
			{
				Unknown = 0,
				R8 = 1,
				RG8 = 2,
				RGBA8 = 4,
				R32F,
				RGBA16,
				RGBA16F,
				RGBA32F,
				DXT1,
				DXT3,
				DXT5,
				LATC1,
				LATC2
			};
			struct Description
			{
				unsigned int Width, Height, Levels;
				Format Format;
			};

		public:
			Texture(const Description &desc);
			virtual ~Texture();

			const Annotation GetAnnotation(const std::string &name) const;
			const Description GetDescription() const;

			virtual bool Update(unsigned int level, const unsigned char *data, std::size_t size) = 0;

		protected:
			Description mDesc;
			std::unordered_map<std::string, Annotation> mAnnotations;
		};
		class Constant
		{
		public:
			enum class Type
			{
				Bool,
				Int,
				Uint,
				Float,
				Struct
			};
			struct Description
			{
				Type Type;
				unsigned int Rows, Columns, Fields, Elements;
				std::size_t Size;
			};

		public:
			Constant(const Description &desc);
			virtual ~Constant();

			const Annotation GetAnnotation(const std::string &name) const;
			const Description GetDescription() const;

			virtual void GetValue(unsigned char *data, std::size_t size) const = 0;
			virtual void SetValue(const unsigned char *data, std::size_t size) = 0;

			template <typename T>
			inline void GetValue(T *values, std::size_t count) const
			{
				GetValue(reinterpret_cast<unsigned char *>(values), count * sizeof(T));
			}
			template <>
			inline void GetValue(unsigned char *values, std::size_t count) const
			{
				GetValue(values, count);
			}
			template <typename T>
			inline void SetValue(const T *values, std::size_t count)
			{
				SetValue(reinterpret_cast<const unsigned char *>(values), count * sizeof(T));
			}
			template <>
			inline void SetValue(const unsigned char *values, std::size_t count)
			{
				SetValue(values, count);
			}

		protected:
			Description mDesc;
			std::unordered_map<std::string, Annotation> mAnnotations;
		};
		class Technique
		{
		public:
			struct Description
			{
				unsigned int Passes;
			};

		public:
			Technique(const Description &desc);
			virtual ~Technique();

			const Annotation GetAnnotation(const std::string &name) const;
			const Description GetDescription() const;

			void Render() const;
			virtual void RenderPass(unsigned int index) const = 0;

		protected:
			Description mDesc;
			std::unordered_map<std::string, Effect::Annotation>	mAnnotations;
		};

	public:
		Effect();
		virtual ~Effect();

		Texture *GetTexture(const std::string &name);
		const Texture *GetTexture(const std::string &name) const;
		Constant *GetConstant(const std::string &name);
		const Constant *GetConstant(const std::string &name) const;
		const Technique *GetTechnique(const std::string &name) const;
		std::vector<std::string> GetTextures() const;
		std::vector<std::string> GetConstants() const;
		std::vector<std::string> GetTechniques() const;

		virtual void Begin() const = 0;
		virtual void End() const = 0;

	protected:
		std::unordered_map<std::string, std::unique_ptr<Texture>> mTextures;
		std::unordered_map<std::string, std::unique_ptr<Constant>> mConstants;
		std::unordered_map<std::string, std::unique_ptr<Technique>> mTechniques;
	};

	// -----------------------------------------------------------------------------------------------------

	template <>
	void Effect::Constant::GetValue(bool *values, std::size_t count) const;
	template <>
	void Effect::Constant::GetValue(int *values, std::size_t count) const;
	template <>
	void Effect::Constant::GetValue(unsigned int *values, std::size_t count) const;
	template <>
	void Effect::Constant::GetValue(long *values, std::size_t count) const;
	template <>
	void Effect::Constant::GetValue(unsigned long *values, std::size_t count) const;
	template <>
	void Effect::Constant::GetValue(long long *values, std::size_t count) const;
	template <>
	void Effect::Constant::GetValue(unsigned long long *values, std::size_t count) const;
	template <>
	void Effect::Constant::GetValue(float *values, std::size_t count) const;
	template <>
	void Effect::Constant::GetValue(double *values, std::size_t count) const;
	template <>
	void Effect::Constant::SetValue(const bool *values, std::size_t count);
	template <>
	void Effect::Constant::SetValue(const int *values, std::size_t count);
	template <>
	void Effect::Constant::SetValue(const unsigned int *values, std::size_t count);
	template <>
	void Effect::Constant::SetValue(const long *values, std::size_t count);
	template <>
	void Effect::Constant::SetValue(const unsigned long *values, std::size_t count);
	template <>
	void Effect::Constant::SetValue(const long long *values, std::size_t count);
	template <>
	void Effect::Constant::SetValue(const unsigned long long *values, std::size_t count);
	template <>
	void Effect::Constant::SetValue(const float *values, std::size_t count);
	template <>
	void Effect::Constant::SetValue(const double *values, std::size_t count);
}