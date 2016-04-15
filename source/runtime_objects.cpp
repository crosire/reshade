#include "runtime.hpp"
#include "runtime_objects.hpp"

namespace reshade
{
	void runtime::add_texture(texture *texture)
	{
		_textures.emplace_back(texture);
	}
	void runtime::add_uniform(uniform *uniform)
	{
		_uniforms.emplace_back(uniform);
	}
	void runtime::add_technique(technique *technique)
	{
		_techniques.emplace_back(technique);
	}
	texture *runtime::find_texture(const std::string &name)
	{
		const auto it = std::find_if(_textures.begin(), _textures.end(),
			[name](const std::unique_ptr<texture> &item)
		{
			return item->name == name;
		});

		return it != _textures.end() ? it->get() : nullptr;
	}

	void runtime::get_uniform_value(const uniform &variable, unsigned char *data, size_t size) const
	{
		assert(data != nullptr);
		assert(_is_effect_compiled);

		size = std::min(size, variable.storage_size);

		assert(variable.storage_offset + size < _uniform_data_storage.size());

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
			case uniform_datatype::bool_:
			case uniform_datatype::int_:
			case uniform_datatype::uint_:
			{
				get_uniform_value(variable, reinterpret_cast<unsigned char *>(values), count * sizeof(int));
				break;
			}
			case uniform_datatype::float_:
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
			case uniform_datatype::bool_:
			case uniform_datatype::int_:
			case uniform_datatype::uint_:
			{
				count = std::min(count, variable.storage_size / sizeof(int));

				assert(values != nullptr);

				const auto data = static_cast<unsigned char *>(alloca(variable.storage_size));
				get_uniform_value(variable, data, variable.storage_size);

				for (size_t i = 0; i < count; ++i)
				{
					if (variable.basetype != uniform_datatype::uint_)
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
			case uniform_datatype::float_:
			{
				get_uniform_value(variable, reinterpret_cast<unsigned char *>(values), count * sizeof(float));
				break;
			}
		}
	}
	void runtime::set_uniform_value(uniform &variable, const unsigned char *data, size_t size)
	{
		assert(data != nullptr);
		assert(_is_effect_compiled);

		size = std::min(size, variable.storage_size);

		assert(variable.storage_offset + size < _uniform_data_storage.size());

		std::memcpy(&_uniform_data_storage[variable.storage_offset], data, size);
	}
	void runtime::set_uniform_value(uniform &variable, const bool *values, size_t count)
	{
		static_assert(sizeof(int) == 4 && sizeof(float) == 4, "expected int and float size to equal 4");

		const auto data = static_cast<unsigned char *>(alloca(count * 4));

		switch (variable.basetype)
		{
			case uniform_datatype::bool_:
				for (size_t i = 0; i < count; ++i)
				{
					reinterpret_cast<int *>(data)[i] = values[i] ? -1 : 0;
				}
				break;
			case uniform_datatype::int_:
			case uniform_datatype::uint_:
				for (size_t i = 0; i < count; ++i)
				{
					reinterpret_cast<int *>(data)[i] = values[i] ? 1 : 0;
				}
				break;
			case uniform_datatype::float_:
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
			case uniform_datatype::bool_:
			case uniform_datatype::int_:
			case uniform_datatype::uint_:
			{
				set_uniform_value(variable, reinterpret_cast<const unsigned char *>(values), count * sizeof(int));
				break;
			}
			case uniform_datatype::float_:
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
			case uniform_datatype::bool_:
			case uniform_datatype::int_:
			case uniform_datatype::uint_:
			{
				set_uniform_value(variable, reinterpret_cast<const unsigned char *>(values), count * sizeof(int));
				break;
			}
			case uniform_datatype::float_:
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
			case uniform_datatype::bool_:
			case uniform_datatype::int_:
			case uniform_datatype::uint_:
			{
				const auto data = static_cast<int *>(alloca(count * sizeof(int)));

				for (size_t i = 0; i < count; ++i)
				{
					data[i] = static_cast<int>(values[i]);
				}

				set_uniform_value(variable, reinterpret_cast<const unsigned char *>(data), count * sizeof(int));
				break;
			}
			case uniform_datatype::float_:
			{
				set_uniform_value(variable, reinterpret_cast<const unsigned char *>(values), count * sizeof(float));
				break;
			}
		}
	}
}
