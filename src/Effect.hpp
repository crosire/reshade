#pragma once

#include <string>
#include <vector>
#include <unordered_map>

namespace ReShade
{
	class Effect
	{
	public:
		struct Annotation
		{
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
			const Description GetDescription() const;
			const Annotation GetAnnotation(const std::string &name) const;

			virtual bool Update(unsigned int level, const unsigned char *data, std::size_t size) = 0;
			virtual void UpdateFromColorBuffer() = 0;
			virtual void UpdateFromDepthBuffer() = 0;

		protected:
			Texture(const Description &desc);
			virtual ~Texture();

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
			const Description GetDescription() const;
			const Annotation GetAnnotation(const std::string &name) const;

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
			Constant(const Description &desc);
			virtual ~Constant();

		protected:
			Description mDesc;
			std::unordered_map<std::string, Annotation> mAnnotations;
		};
		class Technique
		{
		public:
			const Annotation GetAnnotation(const std::string &name) const;

			bool Begin() const;
			virtual bool Begin(unsigned int &passes) const = 0;
			virtual void End() const = 0;
			virtual void RenderPass(unsigned int index) const = 0;

		protected:
			Technique();
			virtual ~Technique();

		protected:
			std::unordered_map<std::string, Effect::Annotation>	mAnnotations;
		};

	public:
		virtual ~Effect();

		virtual const Texture *GetTexture(const std::string &name) const = 0;
		inline Texture *GetTexture(const std::string &name)
		{
			return const_cast<Texture *>(static_cast<const Effect *>(this)->GetTexture(name));
		}
		virtual std::vector<std::string> GetTextureNames() const = 0;
		virtual const Constant *GetConstant(const std::string &name) const = 0;
		inline Constant *GetConstant(const std::string &name)
		{
			return const_cast<Constant *>(static_cast<const Effect *>(this)->GetConstant(name));
		}
		virtual std::vector<std::string> GetConstantNames() const = 0;
		virtual const Technique *GetTechnique(const std::string &name) const = 0;
		virtual std::vector<std::string> GetTechniqueNames() const = 0;
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