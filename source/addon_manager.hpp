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
		if (reshade::addon::event_list_enabled) \
			for (const auto &hook_info : reshade::addon::event_list[static_cast<size_t>(reshade::addon_event::name)]) \
				reinterpret_cast<reshade::pfn_##name>(hook_info)(__VA_ARGS__)
#else
	#define RESHADE_ADDON_EVENT(name, ...) ((void)0)
#endif

namespace reshade::api
{
	class api_data
	{
	public:
		bool get_data(const uint8_t guid[16], uint32_t size, void *data);
		void set_data(const uint8_t guid[16], uint32_t size, const void *data);

	private:
		struct entry
		{
			entry(uint32_t size, const void *data);
			~entry();

			uint8_t guid[16];
			uint32_t size;
			uint64_t storage;
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
	/// Used to enable or disable callbacks.
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
	void load_builtin_addons();

	/// <summary>
	/// Unload any add-ons previously loaded via <see cref="load_addons"/>.
	/// </summary>
	void unload_addons();
	void unload_builtin_addons();
#endif
}
