#pragma once

#include <string>
#include <memory>
#include <vector>
#include <unordered_map>

namespace ReShade
{
	namespace FX
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
				inline Annotation(bool value)
				{
					this->mValue[0] = value ? "1" : "0";
				}
				inline Annotation(const bool values[4])
				{
					this->mValue[0] = values[0] ? "1" : "0";
					this->mValue[1] = values[1] ? "1" : "0";
					this->mValue[2] = values[2] ? "1" : "0";
					this->mValue[3] = values[3] ? "1" : "0";
				}
				inline Annotation(int value)
				{
					this->mValue[0] = std::to_string(value);
				}
				inline Annotation(const int values[4])
				{
					this->mValue[0] = std::to_string(values[0]);
					this->mValue[1] = std::to_string(values[1]);
					this->mValue[2] = std::to_string(values[2]);
					this->mValue[3] = std::to_string(values[3]);
				}
				inline Annotation(unsigned int value)
				{
					this->mValue[0] = std::to_string(value);
				}
				inline Annotation(const unsigned int values[4])
				{
					this->mValue[0] = std::to_string(values[0]);
					this->mValue[1] = std::to_string(values[1]);
					this->mValue[2] = std::to_string(values[2]);
					this->mValue[3] = std::to_string(values[3]);
				}
				inline Annotation(float value)
				{
					this->mValue[0] = std::to_string(value);
				}
				inline Annotation(const float values[4])
				{
					this->mValue[0] = std::to_string(values[0]);
					this->mValue[1] = std::to_string(values[1]);
					this->mValue[2] = std::to_string(values[2]);
					this->mValue[3] = std::to_string(values[3]);
				}
				inline Annotation(const std::string &value)
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
			class Texture
			{
			public:
				enum class Format
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
				struct Description
				{
					std::string Name;
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
					Float
				};
				struct Description
				{
					Type Type;
					std::string Name;
					unsigned int Rows, Columns, Elements;
					std::size_t StorageOffset, StorageSize;
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
					std::string Name;
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

			virtual void Enter() const = 0;
			virtual void Leave() const = 0;

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
}