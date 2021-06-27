/*
 * Copyright (C) 2021 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include "addon_impl.hpp"
#include "reshade_events.hpp"

#if RESHADE_ADDON

namespace reshade
{
	template <addon_event ev, typename... Args>
	inline std::enable_if_t<addon_event_traits<ev>::type == 1, void> invoke_addon_event(const Args &... args)
	{
		std::vector<void *> &event_list = addon::event_list[static_cast<size_t>(ev)];
		for (size_t cb = 0, count = event_list.size(); cb < count; ++cb) // Generates better code than ranged-based for loop
			reinterpret_cast<typename reshade::addon_event_traits<ev>::decl>(event_list[cb])(args...);
	}
	template <addon_event ev, typename... Args>
	inline std::enable_if_t<addon_event_traits<ev>::type == 2, bool> invoke_addon_event(const Args &... args)
	{
		std::vector<void *> &event_list = addon::event_list[static_cast<size_t>(ev)];
		for (size_t cb = 0, count = event_list.size(); cb < count; ++cb)
			if (reinterpret_cast<typename reshade::addon_event_traits<ev>::decl>(event_list[cb])(args...))
				return true;
		return false;
	}
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
}

#endif
