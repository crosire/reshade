#include "Effect.hpp"

#include <assert.h>
#include <algorithm>

namespace ReShade
{
	namespace FX
	{
		Effect::Effect()
		{
		}
		Effect::~Effect()
		{
		}

		Effect::Texture *Effect::GetTexture(const std::string &name)
		{
			return const_cast<Texture *>(static_cast<const Effect *>(this)->GetTexture(name));
		}
		const Effect::Texture *Effect::GetTexture(const std::string &name) const
		{
			const auto it = this->mTextures.find(name);

			if (it != this->mTextures.end())
			{
				return it->second.get();
			}
			else
			{
				return nullptr;
			}
		}
		Effect::Constant *Effect::GetConstant(const std::string &name)
		{
			return const_cast<Constant *>(static_cast<const Effect *>(this)->GetConstant(name));
		}
		const Effect::Constant *Effect::GetConstant(const std::string &name) const
		{
			const auto it = this->mConstants.find(name);

			if (it != this->mConstants.end())
			{
				return it->second.get();
			}
			else
			{
				return nullptr;
			}
		}
		const Effect::Technique *Effect::GetTechnique(const std::string &name) const
		{
			const auto begin = this->mTechniques.begin(), end = this->mTechniques.end(), it = std::find_if(begin, end, [&name](const std::pair<std::string, std::unique_ptr<Technique>> &it) { return it.first == name; });

			if (it != end)
			{
				return it->second.get();
			}
			else
			{
				return nullptr;
			}
		}
		std::vector<std::string> Effect::GetTextures() const
		{
			std::vector<std::string> names;
			names.reserve(this->mTextures.size());
		
			for (const auto &it : this->mTextures)
			{
				names.push_back(it.first);
			}

			return names;
		}
		std::vector<std::string> Effect::GetConstants() const
		{
			std::vector<std::string> names;
			names.reserve(this->mConstants.size());

			for (const auto &it : this->mConstants)
			{
				names.push_back(it.first);
			}

			return names;
		}
		std::vector<std::string> Effect::GetTechniques() const
		{
			std::vector<std::string> names;
			names.reserve(this->mTechniques.size());

			for (const auto &it : this->mTechniques)
			{
				names.push_back(it.first);
			}

			return names;
		}

		Effect::Texture::Texture(const Description &desc) : mDesc(desc)
		{
		}
		Effect::Texture::~Texture()
		{
		}

		const Effect::Annotation Effect::Texture::GetAnnotation(const std::string &name) const
		{
			const auto it = this->mAnnotations.find(name);

			if (it != this->mAnnotations.end())
			{
				return it->second;
			}
			else
			{
				return Effect::Annotation();
			}
		}
		const Effect::Texture::Description Effect::Texture::GetDescription() const
		{
			return this->mDesc;
		}

		Effect::Constant::Constant(const Description &desc) : mDesc(desc)
		{
		}
		Effect::Constant::~Constant()
		{
		}

		const Effect::Annotation Effect::Constant::GetAnnotation(const std::string &name) const
		{
			const auto it = this->mAnnotations.find(name);

			if (it != this->mAnnotations.end())
			{
				return it->second;
			}
			else
			{
				return Effect::Annotation();
			}
		}
		const Effect::Constant::Description Effect::Constant::GetDescription() const
		{
			return this->mDesc;
		}

		template <>
		void Effect::Constant::GetValue<bool>(bool *values, std::size_t count) const
		{
			const Description desc = GetDescription();

			assert(count == 0 || values != nullptr);

			unsigned char *data = static_cast<unsigned char *>(::alloca(desc.StorageSize));
			GetValue(data, desc.StorageSize);

			switch (desc.Type)
			{
				case Type::Bool:
				case Type::Int:
				case Type::Uint:
				{
					for (std::size_t i = 0; i < count; ++i)
					{
						values[i] = reinterpret_cast<const int *>(data)[i] != 0;
					}
					break;
				}
				case Type::Float:
				{
					for (std::size_t i = 0; i < count; ++i)
					{
						values[i] = reinterpret_cast<const float *>(data)[i] != 0.0f;
					}
					break;
				}
			}
		}
		template <>
		void Effect::Constant::GetValue<int>(int *values, std::size_t count) const
		{
			const Description desc = GetDescription();

			assert(count == 0 || values != nullptr);

			if (desc.Type == Type::Bool || desc.Type == Type::Int || desc.Type == Type::Uint)
			{
				GetValue(reinterpret_cast<unsigned char *>(values), count * sizeof(int));
			}
			else
			{
				unsigned char *data = static_cast<unsigned char *>(::alloca(desc.StorageSize));
				GetValue(data, desc.StorageSize);

				for (std::size_t i = 0; i < count; ++i)
				{
					values[i] = static_cast<int>(reinterpret_cast<const float *>(data)[i]);
				}
			}
		}
		template <>
		void Effect::Constant::GetValue<unsigned int>(unsigned int *values, std::size_t count) const
		{
			const Description desc = GetDescription();

			assert(count == 0 || values != nullptr);

			if (desc.Type == Type::Bool || desc.Type == Type::Int || desc.Type == Type::Uint)
			{
				GetValue(reinterpret_cast<unsigned char *>(values), count * sizeof(int));
			}
			else
			{
				unsigned char *data = static_cast<unsigned char *>(::alloca(desc.StorageSize));
				GetValue(data, desc.StorageSize);

				for (std::size_t i = 0; i < count; ++i)
				{
					values[i] = static_cast<unsigned int>(reinterpret_cast<const float *>(data)[i]);
				}
			}
		}
		template <>
		void Effect::Constant::GetValue<long>(long *values, std::size_t count) const
		{
			const Description desc = GetDescription();

			assert(count == 0 || values != nullptr);

			unsigned char *data = static_cast<unsigned char *>(::alloca(desc.StorageSize));
			GetValue(data, desc.StorageSize);

			switch (desc.Type)
			{
				case Type::Bool:
				case Type::Int:
				{
					for (std::size_t i = 0; i < count; ++i)
					{
						values[i] = static_cast<long>(reinterpret_cast<const int *>(data)[i]);
					}
					break;
				}
				case Type::Uint:
				{
					for (std::size_t i = 0; i < count; ++i)
					{
						values[i] = static_cast<long>(reinterpret_cast<const unsigned int *>(data)[i]);
					}
					break;
				}
				case Type::Float:
				{
					for (std::size_t i = 0; i < count; ++i)
					{
						values[i] = static_cast<long>(reinterpret_cast<const float *>(data)[i]);
					}
					break;
				}
			}
		}
		template <>
		void Effect::Constant::GetValue<unsigned long>(unsigned long *values, std::size_t count) const
		{
			const Description desc = GetDescription();

			assert(count == 0 || values != nullptr);

			unsigned char *data = static_cast<unsigned char *>(::alloca(desc.StorageSize));
			GetValue(data, desc.StorageSize);

			switch (desc.Type)
			{
				case Type::Bool:
				case Type::Int:
				{
					for (std::size_t i = 0; i < count; ++i)
					{
						values[i] = static_cast<unsigned long>(reinterpret_cast<const int *>(data)[i]);
					}
					break;
				}
				case Type::Uint:
				{
					for (std::size_t i = 0; i < count; ++i)
					{
						values[i] = static_cast<unsigned long>(reinterpret_cast<const unsigned int *>(data)[i]);
					}
					break;
				}
				case Type::Float:
				{
					for (std::size_t i = 0; i < count; ++i)
					{
						values[i] = static_cast<unsigned long>(reinterpret_cast<const float *>(data)[i]);
					}
					break;
				}
			}
		}
		template <>
		void Effect::Constant::GetValue<long long>(long long *values, std::size_t count) const
		{
			const Description desc = GetDescription();

			assert(count == 0 || values != nullptr);

			unsigned char *data = static_cast<unsigned char *>(::alloca(desc.StorageSize));
			GetValue(data, desc.StorageSize);

			switch (desc.Type)
			{
				case Type::Bool:
				case Type::Int:
				{
					for (std::size_t i = 0; i < count; ++i)
					{
						values[i] = static_cast<long long>(reinterpret_cast<const int *>(data)[i]);
					}
					break;
				}
				case Type::Uint:
				{
					for (std::size_t i = 0; i < count; ++i)
					{
						values[i] = static_cast<long long>(reinterpret_cast<const unsigned int *>(data)[i]);
					}
					break;
				}
				case Type::Float:
				{
					for (std::size_t i = 0; i < count; ++i)
					{
						values[i] = static_cast<long long>(reinterpret_cast<const float *>(data)[i]);
					}
					break;
				}
			}
		}
		template <>
		void Effect::Constant::GetValue<unsigned long long>(unsigned long long *values, std::size_t count) const
		{
			const Description desc = GetDescription();

			assert(count == 0 || values != nullptr);

			unsigned char *data = static_cast<unsigned char *>(::alloca(desc.StorageSize));
			GetValue(data, desc.StorageSize);

			switch (desc.Type)
			{
				case Type::Bool:
				case Type::Int:
				{
					for (std::size_t i = 0; i < count; ++i)
					{
						values[i] = static_cast<unsigned long long>(reinterpret_cast<const int *>(data)[i]);
					}
					break;
				}
				case Type::Uint:
				{
					for (std::size_t i = 0; i < count; ++i)
					{
						values[i] = static_cast<unsigned long long>(reinterpret_cast<const unsigned int *>(data)[i]);
					}
					break;
				}
				case Type::Float:
				{
					for (std::size_t i = 0; i < count; ++i)
					{
						values[i] = static_cast<unsigned long long>(reinterpret_cast<const float *>(data)[i]);
					}
					break;
				}
			}
		}
		template <>
		void Effect::Constant::GetValue<float>(float *values, std::size_t count) const
		{
			const Description desc = GetDescription();

			assert(count == 0 || values != nullptr);

			if (desc.Type == Type::Float)
			{
				GetValue(reinterpret_cast<unsigned char *>(values), count * sizeof(float));
				return;
			}

			unsigned char *data = static_cast<unsigned char *>(::alloca(desc.StorageSize));
			GetValue(data, desc.StorageSize);

			switch (desc.Type)
			{
				case Type::Bool:
				case Type::Int:
				{
					for (std::size_t i = 0; i < count; ++i)
					{
						values[i] = static_cast<float>(reinterpret_cast<const int *>(data)[i]);
					}
					break;
				}
				case Type::Uint:
				{
					for (std::size_t i = 0; i < count; ++i)
					{
						values[i] = static_cast<float>(reinterpret_cast<const unsigned int *>(data)[i]);
					}
					break;
				}
			}
		}
		template <>
		void Effect::Constant::GetValue<double>(double *values, std::size_t count) const
		{
			const Description desc = GetDescription();

			assert(count == 0 || values != nullptr);

			unsigned char *data = static_cast<unsigned char *>(::alloca(desc.StorageSize));
			GetValue(data, desc.StorageSize);

			switch (desc.Type)
			{
				case Type::Bool:
				case Type::Int:
				{
					for (std::size_t i = 0; i < count; ++i)
					{
						values[i] = static_cast<double>(reinterpret_cast<const int *>(data)[i]);
					}
					break;
				}
				case Type::Uint:
				{
					for (std::size_t i = 0; i < count; ++i)
					{
						values[i] = static_cast<double>(reinterpret_cast<const unsigned int *>(data)[i]);
					}
					break;
				}
				case Type::Float:
				{
					for (std::size_t i = 0; i < count; ++i)
					{
						values[i] = static_cast<double>(reinterpret_cast<const float *>(data)[i]);
					}
					break;
				}
			}
		}
		template <>
		void Effect::Constant::SetValue<bool>(const bool *values, std::size_t count)
		{
			const Description desc = GetDescription();

			assert(count == 0 || values != nullptr);

			const unsigned char *data = nullptr;
			std::size_t dataSize = 0;

			switch (desc.Type)
			{
				case Type::Bool:
				{
					dataSize = count * sizeof(int);
					int *dataTyped = static_cast<int *>(::alloca(dataSize));
					data = reinterpret_cast<const unsigned char *>(dataTyped);

					for (std::size_t i = 0; i < count; ++i)
					{
						dataTyped[i] = values[i] ? -1 : 0;
					}
					break;
				}
				case Type::Int:
				case Type::Uint:
				{
					dataSize = count * sizeof(int);
					int *dataTyped = static_cast<int *>(::alloca(dataSize));
					data = reinterpret_cast<const unsigned char *>(dataTyped);

					for (std::size_t i = 0; i < count; ++i)
					{
						dataTyped[i] = values[i] ? 1 : 0;
					}
					break;
				}
				case Type::Float:
				{
					dataSize = count * sizeof(float);
					float *dataTyped = static_cast<float *>(::alloca(dataSize));
					data = reinterpret_cast<const unsigned char *>(dataTyped);

					for (std::size_t i = 0; i < count; ++i)
					{
						dataTyped[i] = values[i] ? 1.0f : 0.0f;
					}
					break;
				}
			}

			SetValue(data, dataSize);
		}
		template <>
		void Effect::Constant::SetValue<int>(const int *values, std::size_t count)
		{
			const Description desc = GetDescription();

			assert(count == 0 || values != nullptr);

			const unsigned char *data = nullptr;
			std::size_t dataSize = 0;

			switch (desc.Type)
			{
				case Type::Bool:
				case Type::Int:
				case Type::Uint:
				{
					data = reinterpret_cast<const unsigned char *>(values);
					dataSize = count * sizeof(int);
					break;
				}
				case Type::Float:
				{
					dataSize = count * sizeof(float);
					float *dataTyped = static_cast<float *>(::alloca(dataSize));
					data = reinterpret_cast<const unsigned char *>(dataTyped);

					for (std::size_t i = 0; i < count; ++i)
					{
						dataTyped[i] = static_cast<float>(values[i]);
					}
					break;
				}
			}

			SetValue(data, dataSize);
		}
		template <>
		void Effect::Constant::SetValue<unsigned int>(const unsigned int *values, std::size_t count)
		{
			const Description desc = GetDescription();
		
			assert(count == 0 || values != nullptr);

			const unsigned char *data = nullptr;
			std::size_t dataSize = 0;

			switch (desc.Type)
			{
				case Type::Bool:
				case Type::Int:
				case Type::Uint:
				{
					data = reinterpret_cast<const unsigned char *>(values);
					dataSize = count * sizeof(int);
					break;
				}
				case Type::Float:
				{
					dataSize = count * sizeof(float);
					float *dataTyped = static_cast<float *>(::alloca(dataSize));
					data = reinterpret_cast<const unsigned char *>(dataTyped);
				
					for (std::size_t i = 0; i < count; ++i)
					{
						dataTyped[i] = static_cast<float>(values[i]);
					}
					break;
				}
			}

			SetValue(data, dataSize);
		}
		template <>
		void Effect::Constant::SetValue<long>(const long *values, std::size_t count)
		{
			const Description desc = GetDescription();

			assert(count == 0 || values != nullptr);

			const unsigned char *data = nullptr;
			std::size_t dataSize = 0;

			switch (desc.Type)
			{
				case Type::Bool:
				case Type::Int:
				case Type::Uint:
				{
					dataSize = count * sizeof(int);
					int *dataTyped = static_cast<int *>(::alloca(dataSize));
					data = reinterpret_cast<const unsigned char *>(dataTyped);

					for (std::size_t i = 0; i < count; ++i)
					{
						dataTyped[i] = static_cast<int>(values[i]);
					}
					break;
				}
				case Type::Float:
				{
					dataSize = count * sizeof(float);
					float *dataTyped = static_cast<float *>(::alloca(dataSize));
					data = reinterpret_cast<const unsigned char *>(dataTyped);

					for (std::size_t i = 0; i < count; ++i)
					{
						dataTyped[i] = static_cast<float>(values[i]);
					}
					break;
				}
			}

			SetValue(data, dataSize);
		}
		template <>
		void Effect::Constant::SetValue<unsigned long>(const unsigned long *values, std::size_t count)
		{
			const Description desc = GetDescription();

			assert(count == 0 || values != nullptr);

			const unsigned char *data = nullptr;
			std::size_t dataSize = 0;

			switch (desc.Type)
			{
				case Type::Bool:
				case Type::Int:
				case Type::Uint:
				{
					dataSize = count * sizeof(int);
					int *dataTyped = static_cast<int *>(::alloca(dataSize));
					data = reinterpret_cast<const unsigned char *>(dataTyped);

					for (std::size_t i = 0; i < count; ++i)
					{
						dataTyped[i] = static_cast<int>(values[i]);
					}
					break;
				}
				case Type::Float:
				{
					dataSize = count * sizeof(float);
					float *dataTyped = static_cast<float *>(::alloca(dataSize));
					data = reinterpret_cast<const unsigned char *>(dataTyped);

					for (std::size_t i = 0; i < count; ++i)
					{
						dataTyped[i] = static_cast<float>(values[i]);
					}
					break;
				}
			}

			SetValue(data, dataSize);
		}
		template <>
		void Effect::Constant::SetValue<long long>(const long long *values, std::size_t count)
		{
			const Description desc = GetDescription();

			assert(count == 0 || values != nullptr);

			const unsigned char *data = nullptr;
			std::size_t dataSize = 0;

			switch (desc.Type)
			{
				case Type::Bool:
				case Type::Int:
				case Type::Uint:
				{
					dataSize = count * sizeof(int);
					int *dataTyped = static_cast<int *>(::alloca(dataSize));
					data = reinterpret_cast<const unsigned char *>(dataTyped);

					for (std::size_t i = 0; i < count; ++i)
					{
						dataTyped[i] = static_cast<int>(values[i]);
					}
					break;
				}
				case Type::Float:
				{
					dataSize = count * sizeof(float);
					float *dataTyped = static_cast<float *>(::alloca(dataSize));
					data = reinterpret_cast<const unsigned char *>(dataTyped);

					for (std::size_t i = 0; i < count; ++i)
					{
						dataTyped[i] = static_cast<float>(values[i]);
					}
					break;
				}
			}

			SetValue(data, dataSize);
		}
		template <>
		void Effect::Constant::SetValue<unsigned long long>(const unsigned long long *values, std::size_t count)
		{
			const Description desc = GetDescription();

			assert(count == 0 || values != nullptr);

			const unsigned char *data = nullptr;
			std::size_t dataSize = 0;

			switch (desc.Type)
			{
				case Type::Bool:
				case Type::Int:
				case Type::Uint:
				{
					dataSize = count * sizeof(int);
					int *dataTyped = static_cast<int *>(::alloca(dataSize));
					data = reinterpret_cast<const unsigned char *>(dataTyped);

					for (std::size_t i = 0; i < count; ++i)
					{
						dataTyped[i] = static_cast<int>(values[i]);
					}
					break;
				}
				case Type::Float:
				{
					dataSize = count * sizeof(float);
					float *dataTyped = static_cast<float *>(::alloca(dataSize));
					data = reinterpret_cast<const unsigned char *>(dataTyped);

					for (std::size_t i = 0; i < count; ++i)
					{
						dataTyped[i] = static_cast<float>(values[i]);
					}
					break;
				}
			}

			SetValue(data, dataSize);
		}
		template <>
		void Effect::Constant::SetValue<float>(const float *values, std::size_t count)
		{
			const Description desc = GetDescription();

			assert(count == 0 || values != nullptr);

			const unsigned char *data = nullptr;
			std::size_t dataSize = 0;

			switch (desc.Type)
			{
				case Type::Bool:
				case Type::Int:
				case Type::Uint:
				{
					dataSize = count * sizeof(int);
					int *dataTyped = static_cast<int *>(::alloca(dataSize));
					data = reinterpret_cast<const unsigned char *>(dataTyped);

					for (std::size_t i = 0; i < count; ++i)
					{
						dataTyped[i] = static_cast<int>(values[i]);
					}
					break;
				}
				case Type::Float:
				{
					data = reinterpret_cast<const unsigned char *>(values);
					dataSize = count * sizeof(float);
					break;
				}
			}

			SetValue(data, dataSize);
		}
		template <>
		void Effect::Constant::SetValue<double>(const double *values, std::size_t count)
		{
			const Description desc = GetDescription();

			assert(count == 0 || values != nullptr);

			const unsigned char *data = nullptr;
			std::size_t dataSize = 0;

			switch (desc.Type)
			{
				case Type::Bool:
				case Type::Int:
				case Type::Uint:
				{
					dataSize = count * sizeof(int);
					int *dataTyped = static_cast<int *>(::alloca(dataSize));
					data = reinterpret_cast<const unsigned char *>(dataTyped);

					for (std::size_t i = 0; i < count; ++i)
					{
						dataTyped[i] = static_cast<int>(values[i]);
					}
					break;
				}
				case Type::Float:
				{
					dataSize = count * sizeof(float);
					float *dataTyped = static_cast<float *>(::alloca(dataSize));
					data = reinterpret_cast<const unsigned char *>(dataTyped);

					for (std::size_t i = 0; i < count; ++i)
					{
						dataTyped[i] = static_cast<float>(values[i]);
					}
					break;
				}
			}

			SetValue(data, dataSize);
		}

		Effect::Technique::Technique(const Description &desc) : mDesc(desc)
		{
		}
		Effect::Technique::~Technique()
		{
		}

		const Effect::Annotation Effect::Technique::GetAnnotation(const std::string &name) const
		{
			const auto it = this->mAnnotations.find(name);

			if (it != this->mAnnotations.end())
			{
				return it->second;
			}
			else
			{
				return Effect::Annotation();
			}
		}
		const Effect::Technique::Description Effect::Technique::GetDescription() const
		{
			return this->mDesc;
		}

		void Effect::Technique::Render() const
		{
			for (unsigned int i = 0; i < this->mDesc.Passes; ++i)
			{
				RenderPass(i);
			}
		}
	}
}