#pragma once

#include <string>
#include <vector>

namespace ReShade
{
	class														Effect
	{
		friend class EffectContext;

	public:
		struct													Annotation
		{
			inline Annotation(void)
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

			template <typename T> const T						As(void) const;
			template <> inline const bool						As(void) const
			{
				return As<int>() != 0 || (this->mValue == "true" || this->mValue == "True" || this->mValue == "TRUE");
			}
			template <> inline const int						As(void) const
			{
				return static_cast<int>(std::strtol(this->mValue.c_str(), nullptr, 10));
			}
			template <> inline const unsigned int				As(void) const
			{
				return static_cast<unsigned int>(std::strtoul(this->mValue.c_str(), nullptr, 10));
			}
			template <> inline const float						As(void) const
			{
				return static_cast<float>(std::strtod(this->mValue.c_str(), nullptr));
			}
			template <> inline const double						As(void) const
			{
				return std::strtod(this->mValue.c_str(), nullptr);
			}
			template <> inline const std::string				As(void) const
			{
				return this->mValue;
			}
			template <> inline const char *const				As(void) const
			{
				return this->mValue.c_str();
			}

		private:
			std::string											mValue;
		};
		class													Texture
		{
		public:
			enum class											Format
			{
				Unknown											= 0,
				R8												= 1,
				RG8												= 2,
				RGBA8											= 4,
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
			struct												Description
			{
				unsigned int									Width, Height, Levels;
				Format											Format;
			};

		public:
			virtual const Description							GetDescription(void) const = 0;
			virtual const Annotation							GetAnnotation(const std::string &name) const = 0;

			virtual void										Update(unsigned int level, const unsigned char *data, std::size_t size) = 0;
			virtual void										UpdateFromColorBuffer(void) = 0;
			virtual void										UpdateFromDepthBuffer(void) = 0;
		};
		class													Constant
		{
		public:
			enum class											Type
			{
				Bool,
				Int,
				Uint,
				Float,
				Struct
			};
			struct												Description
			{
				Type											Type;
				unsigned int									Rows, Columns, Fields, Elements;
				std::size_t										Size;
			};

		public:
			virtual const Description							GetDescription(void) const = 0;
			virtual const Annotation							GetAnnotation(const std::string &name) const = 0;
			virtual void										GetValue(unsigned char *data, std::size_t size) const = 0;
			template <typename T> inline void					GetValue(T *values, std::size_t count) const
			{
				GetValue(reinterpret_cast<unsigned char *>(values), count * sizeof(T));
			}
			template <> inline void								GetValue(unsigned char *values, std::size_t count) const
			{
				GetValue(values, count);
			}
			virtual void										SetValue(const unsigned char *data, std::size_t size) = 0;
			template <typename T> inline void					SetValue(const T *values, std::size_t count)
			{
				SetValue(reinterpret_cast<const unsigned char *>(values), count * sizeof(T));
			}
			template <> inline void								SetValue(const unsigned char *values, std::size_t count)
			{
				SetValue(values, count);
			}
		};
		class													Technique
		{
		public:
			struct												Description
			{
				std::vector<std::string>						Passes;
			};

		public:
			virtual const Description							GetDescription(void) const = 0;
			virtual const Annotation							GetAnnotation(const std::string &name) const = 0;

			inline bool											Begin(void) const
			{
				unsigned int passes;
				return Begin(passes);
			}
			virtual bool										Begin(unsigned int &passes) const = 0;
			virtual void										End(void) const = 0;
			virtual void										RenderPass(unsigned int index) const = 0;
			void												RenderPass(const std::string &name) const;
		};

	public:
		virtual ~Effect(void)
		{
		}

		virtual const Texture *									GetTexture(const std::string &name) const = 0;
		inline Texture *										GetTexture(const std::string &name)
		{
			return const_cast<Texture *>(static_cast<const Effect *>(this)->GetTexture(name));
		}
		virtual std::vector<std::string>						GetTextureNames(void) const = 0;
		virtual const Constant *								GetConstant(const std::string &name) const = 0;
		inline Constant *										GetConstant(const std::string &name)
		{
			return const_cast<Constant *>(static_cast<const Effect *>(this)->GetConstant(name));
		}
		virtual std::vector<std::string>						GetConstantNames(void) const = 0;
		virtual const Technique *								GetTechnique(const std::string &name) const = 0;
		virtual std::vector<std::string>						GetTechniqueNames(void) const = 0;
	};

	// -----------------------------------------------------------------------------------------------------

	template <> void											Effect::Constant::GetValue(bool *values, std::size_t count) const;
	template <> void											Effect::Constant::GetValue(int *values, std::size_t count) const;
	template <> void											Effect::Constant::GetValue(unsigned int *values, std::size_t count) const;
	template <> void											Effect::Constant::GetValue(long *values, std::size_t count) const;
	template <> void											Effect::Constant::GetValue(unsigned long *values, std::size_t count) const;
	template <> void											Effect::Constant::GetValue(long long *values, std::size_t count) const;
	template <> void											Effect::Constant::GetValue(unsigned long long *values, std::size_t count) const;
	template <> void											Effect::Constant::GetValue(float *values, std::size_t count) const;
	template <> void											Effect::Constant::GetValue(double *values, std::size_t count) const;
	template <> void											Effect::Constant::SetValue(const bool *values, std::size_t count);
	template <> void											Effect::Constant::SetValue(const int *values, std::size_t count);
	template <> void											Effect::Constant::SetValue(const unsigned int *values, std::size_t count);
	template <> void											Effect::Constant::SetValue(const long *values, std::size_t count);
	template <> void											Effect::Constant::SetValue(const unsigned long *values, std::size_t count);
	template <> void											Effect::Constant::SetValue(const long long *values, std::size_t count);
	template <> void											Effect::Constant::SetValue(const unsigned long long *values, std::size_t count);
	template <> void											Effect::Constant::SetValue(const float *values, std::size_t count);
	template <> void											Effect::Constant::SetValue(const double *values, std::size_t count);
}