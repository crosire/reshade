/*
 * Copyright (C) 2021 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include <string>
#include <vector>
#include <cassert>

template <typename T, size_t STACK_ELEMENTS = 16>
struct temp_mem
{
	temp_mem(size_t elements) : stack()
	{
		if (elements > STACK_ELEMENTS)
			p = new T[elements];
		else
			p = stack;
	}
	~temp_mem()
	{
		if (p != stack)
			delete[] p;
	}

	T &operator[](size_t element)
	{
		return p[element];
	}

	T *p, stack[STACK_ELEMENTS];
};

namespace reshade::api
{
	class api_object;
	class effect_runtime;

	template <typename T, typename... api_object_base>
	class api_object_impl : public api_object_base...
	{
		static_assert(sizeof(T) <= sizeof(uint64_t));

	public:
		api_object_impl(const api_object_impl &) = delete;
		api_object_impl &operator=(const api_object_impl &) = delete;

		void get_private_data(const uint8_t guid[16], uint64_t *data) const override
		{
			for (auto it = _private_data.begin(); it != _private_data.end(); ++it)
			{
				if (std::memcmp(it->guid, guid, 16) == 0)
				{
					*data = it->data;
					return;
				}
			}

			*data = 0;
		}
		void set_private_data(const uint8_t guid[16], const uint64_t data)  override
		{
			for (auto it = _private_data.begin(); it != _private_data.end(); ++it)
			{
				if (std::memcmp(it->guid, guid, 16) == 0)
				{
					if (data != 0)
						it->data = data;
					else
						_private_data.erase(it);
					return;
				}
			}

			if (data != 0)
			{
				_private_data.push_back({ data, {
					reinterpret_cast<const uint64_t *>(guid)[0],
					reinterpret_cast<const uint64_t *>(guid)[1] } });
			}
		}

		uint64_t get_native_object() const override { return (uint64_t)_orig; }

		T _orig;

	protected:
		template <typename... Args>
		explicit api_object_impl(T orig, Args... args) : api_object_base(std::forward<Args>(args)...)..., _orig(orig) {}
		~api_object_impl()
		{
			// All user data should ideally have been removed before destruction, to avoid leaks
			assert(_private_data.empty());
		}

	private:
		struct private_data
		{
			uint64_t data;
			uint64_t guid[2];
		};

		std::vector<private_data> _private_data;
	};
}

#if RESHADE_ADDON

namespace reshade::addon
{
	struct info
	{
		void *handle = nullptr;

		std::string name;
		std::string description;
		std::string file;
		std::string author;
		std::string version;

		std::vector<std::pair<uint32_t, void *>> event_callbacks;
#if RESHADE_GUI
		float settings_height = 0.0f;
		void(*settings_overlay_callback)(api::effect_runtime *) = nullptr;
		std::vector<std::pair<std::string, void(*)(api::effect_runtime *)>> overlay_callbacks;
#endif
	};

	/// <summary>
	/// Global switch to enable or disable all loaded add-ons.
	/// </summary>
	extern bool enabled;

	/// <summary>
	/// List of add-on event callbacks.
	/// </summary>
	extern std::vector<void *> event_list[];

	/// <summary>
	/// List of currently loaded add-ons.
	/// </summary>
	extern std::vector<addon::info> loaded_info;
}

#endif
