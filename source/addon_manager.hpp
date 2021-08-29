/*
 * Copyright (C) 2021 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include "addon.hpp"
#include "reshade_events.hpp"

#if RESHADE_ADDON

namespace reshade
{
	/// <summary>
	/// Loads any add-ons found in the configured search paths.
	/// </summary>
	void load_addons();

	/// <summary>
	/// Unloads any add-ons previously loaded via <see cref="load_addons"/>.
	/// </summary>
	void unload_addons();

	/// <summary>
	/// Checks whether any callbacks were registered for the specified <paramref name="ev"/>ent.
	/// </summary>
	template <addon_event ev>
	__forceinline bool has_addon_event()
	{
		return !addon::event_list[static_cast<size_t>(ev)].empty();
	}

	/// <summary>
	/// Invokes all registered callbacks for the specified <typeparamref name="ev"/>ent.
	/// </summary>
	template <addon_event ev, typename... Args>
	__forceinline std::enable_if_t<std::is_same_v<typename addon_event_traits<ev>::type, void>, void> invoke_addon_event(Args &&... args)
	{
		// Allow a subset of events even when add-ons are disabled, to ensure they continue working correctly
		if constexpr (
			ev != addon_event::init_device &&
			ev != addon_event::destroy_device &&
			ev != addon_event::init_command_list &&
			ev != addon_event::destroy_command_list &&
			ev != addon_event::init_command_queue &&
			ev != addon_event::destroy_command_queue &&
			ev != addon_event::init_swapchain &&
			ev != addon_event::destroy_swapchain &&
			ev != addon_event::init_effect_runtime &&
			ev != addon_event::destroy_effect_runtime &&
			ev != addon_event::init_sampler &&
			ev != addon_event::destroy_sampler &&
			ev != addon_event::init_resource &&
			ev != addon_event::destroy_resource &&
			ev != addon_event::init_resource_view &&
			ev != addon_event::destroy_resource_view &&
			ev != addon_event::init_pipeline &&
			ev != addon_event::destroy_pipeline &&
			ev != addon_event::init_render_pass &&
			ev != addon_event::destroy_render_pass &&
			ev != addon_event::init_framebuffer &&
			ev != addon_event::destroy_framebuffer)
		if (!addon::enabled)
			return;
		std::vector<void *> &event_list = addon::event_list[static_cast<size_t>(ev)];
		for (size_t cb = 0, count = event_list.size(); cb < count; ++cb) // Generates better code than ranged-based for loop
			reinterpret_cast<typename reshade::addon_event_traits<ev>::decl>(event_list[cb])(std::forward<Args>(args)...);
	}
	/// <summary>
	/// Invokes registered callbacks for the specified <typeparamref name="ev"/>ent until a callback reports back as having handled this event by returning <see langword="true"/>.
	/// </summary>
	template <addon_event ev, typename... Args>
	__forceinline std::enable_if_t<std::is_same_v<typename addon_event_traits<ev>::type, bool>, bool> invoke_addon_event(Args &&... args)
	{
		if (!addon::enabled)
			return false;
		std::vector<void *> &event_list = addon::event_list[static_cast<size_t>(ev)];
		for (size_t cb = 0, count = event_list.size(); cb < count; ++cb)
			if (reinterpret_cast<typename reshade::addon_event_traits<ev>::decl>(event_list[cb])(std::forward<Args>(args)...))
				return true;
		return false;
	}
}

#endif
