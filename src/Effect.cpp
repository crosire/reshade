#include "Effect.hpp"

#include <assert.h>

namespace ReShade
{
	template <>
	void Effect::Constant::GetValue<bool>(bool *values, std::size_t count) const
	{
		const Description desc = GetDescription();

		assert(count == 0 || values != nullptr);

		unsigned char *data = static_cast<unsigned char *>(::alloca(desc.Size));
		GetValue(data, desc.Size);

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
			unsigned char *data = static_cast<unsigned char *>(::alloca(desc.Size));
			GetValue(data, desc.Size);

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
			unsigned char *data = static_cast<unsigned char *>(::alloca(desc.Size));
			GetValue(data, desc.Size);

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

		unsigned char *data = static_cast<unsigned char *>(::alloca(desc.Size));
		GetValue(data, desc.Size);

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

		unsigned char *data = static_cast<unsigned char *>(::alloca(desc.Size));
		GetValue(data, desc.Size);

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

		unsigned char *data = static_cast<unsigned char *>(::alloca(desc.Size));
		GetValue(data, desc.Size);

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

		unsigned char *data = static_cast<unsigned char *>(::alloca(desc.Size));
		GetValue(data, desc.Size);

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

		unsigned char *data = static_cast<unsigned char *>(::alloca(desc.Size));
		GetValue(data, desc.Size);

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

		unsigned char *data = static_cast<unsigned char *>(::alloca(desc.Size));
		GetValue(data, desc.Size);

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
}