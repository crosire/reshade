/*
 * Copyright (C) 2021 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "addon.hpp"
#include "reshade_events.hpp"

#if RESHADE_ADDON

namespace reshade
{
#if RESHADE_ADDON == 1
	/// <summary>
	/// Global switch to enable or disable all loaded add-ons.
	/// </summary>
	extern bool addon_enabled;
#endif
	extern bool addon_all_loaded;

	/// <summary>
	/// List of add-on event callbacks.
	/// </summary>
	extern std::vector<void *> addon_event_list[];

	/// <summary>
	/// List of currently loaded add-ons.
	/// </summary>
	extern std::vector<addon_info> addon_loaded_info;

	/// <summary>
	/// Loads any add-ons found in the configured search paths.
	/// </summary>
	void load_addons();

	/// <summary>
	/// Unloads any add-ons previously loaded via <see cref="load_addons"/>.
	/// </summary>
	void unload_addons();

	/// <summary>
	/// Checks whether any add-ons were loaded.
	/// </summary>
	bool has_loaded_addons();

	/// <summary>
	/// Gets the add-on that was loaded at the specified address.
	/// </summary>
	addon_info *find_addon(void *address);

	/// <summary>
	/// Checks whether any callbacks were registered for the specified <paramref name="ev"/>ent.
	/// </summary>
	template <addon_event ev>
	__forceinline bool has_addon_event()
	{
		return !addon_event_list[static_cast<uint32_t>(ev)].empty();
	}

	/// <summary>
	/// Invokes all registered callbacks for the specified <typeparamref name="ev"/>ent.
	/// </summary>
	template <addon_event ev, typename... Args>
	__forceinline std::enable_if_t<std::is_same_v<typename addon_event_traits<ev>::type, void>, void> invoke_addon_event(Args &&... args)
	{
#if RESHADE_ADDON == 1
		// Ensure certain events are not compiled when only limited add-on support is enabled
		static_assert(
			ev != addon_event::map_buffer_region &&
			ev != addon_event::unmap_buffer_region &&
			ev != addon_event::map_texture_region &&
			ev != addon_event::unmap_texture_region &&
			ev != addon_event::barrier &&
			ev != addon_event::bind_pipeline &&
			ev != addon_event::bind_pipeline_states &&
			ev != addon_event::push_constants &&
			ev != addon_event::push_descriptors &&
			ev != addon_event::bind_descriptor_tables &&
			ev != addon_event::bind_index_buffer &&
			ev != addon_event::bind_vertex_buffers &&
			ev != addon_event::bind_stream_output_buffers,
			"Event that is disabled with limited add-on support was used!");

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
			ev != addon_event::init_pipeline_layout &&
			ev != addon_event::destroy_pipeline_layout &&
			ev != addon_event::init_query_heap &&
			ev != addon_event::destroy_query_heap)
		if (!addon_enabled)
			return;
#endif
		std::vector<void *> &event_list = addon_event_list[static_cast<uint32_t>(ev)];
		for (size_t cb = 0, count = event_list.size(); cb < count; ++cb) // Generates better code than ranged-based for loop
			reinterpret_cast<typename addon_event_traits<ev>::decl>(event_list[cb])(std::forward<Args>(args)...);
	}
	/// <summary>
	/// Invokes registered callbacks for the specified <typeparamref name="ev"/>ent until a callback reports back as having handled this event by returning <see langword="true"/>.
	/// </summary>
	template <addon_event ev, typename... Args>
	__forceinline std::enable_if_t<std::is_same_v<typename addon_event_traits<ev>::type, bool>, bool> invoke_addon_event(Args &&... args)
	{
#if RESHADE_ADDON == 1
		static_assert(
			ev != addon_event::update_buffer_region &&
			ev != addon_event::update_texture_region &&
			ev != addon_event::copy_descriptor_tables &&
			ev != addon_event::update_descriptor_tables &&
			ev != addon_event::get_query_heap_results &&
			ev != addon_event::copy_resource &&
			ev != addon_event::copy_buffer_region &&
			ev != addon_event::copy_buffer_to_texture &&
			ev != addon_event::copy_texture_region &&
			ev != addon_event::copy_texture_to_buffer &&
			ev != addon_event::resolve_texture_region &&
			ev != addon_event::generate_mipmaps &&
			ev != addon_event::begin_query &&
			ev != addon_event::end_query &&
			ev != addon_event::copy_query_heap_results &&
			ev != addon_event::copy_acceleration_structure &&
			ev != addon_event::build_acceleration_structure,
			"Event that is disabled with limited add-on support was used!");

		if constexpr (
			ev != addon_event::create_resource_view) // This is needed by the Generic Depth add-on so that view creation succeeds for resources where the format was overriden
		if (!addon_enabled)
			return false;
#endif
		bool skip = false;
		std::vector<void *> &event_list = addon_event_list[static_cast<uint32_t>(ev)];
		for (size_t cb = 0, count = event_list.size(); cb < count; ++cb)
			if (reinterpret_cast<typename addon_event_traits<ev>::decl>(event_list[cb])(std::forward<Args>(args)...))
				skip = true;
		return skip;
	}
}

#endif
