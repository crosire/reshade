/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "runtime.hpp"
#include "runtime_objects.hpp"
#include <assert.h>
#include <algorithm>

namespace reshade
{
	texture *runtime::find_texture(const std::string &unique_name)
	{
		const auto it = std::find_if(_textures.begin(), _textures.end(),
			[unique_name](const auto &item)
			{
				return item.unique_name == unique_name;
			});

		return it != _textures.end() ? &(*it) : nullptr;
	}

	void runtime::get_uniform_value(const uniform &variable, unsigned char *data, size_t size) const
	{
		assert(data != nullptr);

		size = std::min(size, variable.storage_size);

		assert(variable.storage_offset + size <= _uniform_data_storage.size());

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
			case reshadefx::type::t_bool:
			case reshadefx::type::t_int:
			case reshadefx::type::t_uint:
			{
				get_uniform_value(variable, reinterpret_cast<unsigned char *>(values), count * sizeof(int));
				break;
			}
			case reshadefx::type::t_float:
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
			case reshadefx::type::t_bool:
			case reshadefx::type::t_int:
			case reshadefx::type::t_uint:
			{
				count = std::min(count, variable.storage_size / sizeof(int));

				assert(values != nullptr);

				const auto data = static_cast<unsigned char *>(alloca(variable.storage_size));
				get_uniform_value(variable, data, variable.storage_size);

				for (size_t i = 0; i < count; ++i)
				{
					if (variable.basetype != reshadefx::type::t_uint)
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
			case reshadefx::type::t_float:
			{
				get_uniform_value(variable, reinterpret_cast<unsigned char *>(values), count * sizeof(float));
				break;
			}
		}
	}
	void runtime::set_uniform_value(uniform &variable, const unsigned char *data, size_t size)
	{
		assert(data != nullptr);

		size = std::min(size, variable.storage_size);

		assert(variable.storage_offset + size <= _uniform_data_storage.size());

		std::memcpy(&_uniform_data_storage[variable.storage_offset], data, size);
	}
	void runtime::set_uniform_value(uniform &variable, const bool *values, size_t count)
	{
		static_assert(sizeof(int) == 4 && sizeof(float) == 4, "expected int and float size to equal 4");

		const auto data = static_cast<unsigned char *>(alloca(count * 4));

		switch (variable.basetype)
		{
			case reshadefx::type::t_bool:
			for (size_t i = 0; i < count; ++i)
				{
					reinterpret_cast<int *>(data)[i] = values[i] ? -1 : 0;
				}
				break;
			case reshadefx::type::t_int:
			case reshadefx::type::t_uint:
				for (size_t i = 0; i < count; ++i)
				{
					reinterpret_cast<int *>(data)[i] = values[i] ? 1 : 0;
				}
				break;
			case reshadefx::type::t_float:
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
			case reshadefx::type::t_bool:
			case reshadefx::type::t_int:
			case reshadefx::type::t_uint:
			{
				set_uniform_value(variable, reinterpret_cast<const unsigned char *>(values), count * sizeof(int));
				break;
			}
			case reshadefx::type::t_float:
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
			case reshadefx::type::t_bool:
			case reshadefx::type::t_int:
			case reshadefx::type::t_uint:
			{
				set_uniform_value(variable, reinterpret_cast<const unsigned char *>(values), count * sizeof(int));
				break;
			}
			case reshadefx::type::t_float:
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
			case reshadefx::type::t_bool:
			case reshadefx::type::t_int:
			case reshadefx::type::t_uint:
			{
				const auto data = static_cast<int *>(alloca(count * sizeof(int)));

				for (size_t i = 0; i < count; ++i)
				{
					data[i] = static_cast<int>(values[i]);
				}

				set_uniform_value(variable, reinterpret_cast<const unsigned char *>(data), count * sizeof(int));
				break;
			}
			case reshadefx::type::t_float:
			{
				set_uniform_value(variable, reinterpret_cast<const unsigned char *>(values), count * sizeof(float));
				break;
			}
		}
	}
}
