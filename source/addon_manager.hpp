/*
 * Copyright (C) 2021 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include "reshade_events.hpp"
#include <string>
#include <vector>

#if RESHADE_ADDON
	#define RESHADE_ADDON_EVENT(name, ...) \
		for (void *const callback : reshade::addon::event_list[static_cast<size_t>(reshade::addon_event::name)]) \
			reinterpret_cast<typename reshade::addon_event_traits<reshade::addon_event::name>::decl>(callback)(__VA_ARGS__)
#else
	#define RESHADE_ADDON_EVENT(name, ...) ((void)0)
#endif

namespace reshade::api
{
	template <typename... T>
	class api_object_impl : public T...
	{
	public:
		bool get_data(const uint8_t guid[16], void **ptr) const override
		{
			for (auto it = _entries.begin(); it != _entries.end(); ++it)
			{
				if (it->guid[0] == reinterpret_cast<const uint64_t *>(guid)[0] &&
					it->guid[1] == reinterpret_cast<const uint64_t *>(guid)[1])
				{
					*ptr = it->data;
					return true;
				}
			}
			return false;
		}
		void set_data(const uint8_t guid[16], void * const ptr) override
		{
			for (auto it = _entries.begin(); it != _entries.end(); ++it)
			{
				if (it->guid[0] == reinterpret_cast<const uint64_t *>(guid)[0] &&
					it->guid[1] == reinterpret_cast<const uint64_t *>(guid)[1])
				{
					if (ptr == nullptr)
						_entries.erase(it);
					else
						it->data = ptr;
					return;
				}
			}

			if (ptr != nullptr)
			{
				_entries.push_back({ ptr, {
					reinterpret_cast<const uint64_t *>(guid)[0],
					reinterpret_cast<const uint64_t *>(guid)[1] } });
			}
		}

	private:
		struct entry
		{
			void *data;
			uint64_t guid[2];
		};

		std::vector<entry> _entries;
	};
}

namespace reshade::addon
{
	struct info
	{
		void *handle;
		std::string name;
		std::string description;
	};

	/// <summary>
	/// Can be used to check if add-on event callbacks are enabled or not.
	/// </summary>
	extern bool event_list_enabled;

	/// <summary>
	/// List of currently loaded add-ons.
	/// </summary>
	extern std::vector<info> loaded_info;

	/// <summary>
	/// List of installed add-on event callbacks.
	/// </summary>
	extern std::vector<void *> event_list[];

#if RESHADE_GUI
	/// <summary>
	/// List of overlays registered by loaded add-ons.
	/// </summary>
	extern std::vector<std::pair<std::string, void(*)(api::effect_runtime *, void *)>> overlay_list;
#endif

#if RESHADE_ADDON
	/// <summary>
	/// Load any add-ons found in the configured search paths.
	/// </summary>
	void load_addons();

	/// <summary>
	/// Unload any add-ons previously loaded via <see cref="load_addons"/>.
	/// </summary>
	void unload_addons();

	/// <summary>
	/// Enable or disable all loaded add-ons.
	/// </summary>
	void enable_or_disable_addons(bool enabled);
#endif
}
