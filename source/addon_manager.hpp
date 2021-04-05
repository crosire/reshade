/*
 * Copyright (C) 2021 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include "addon_impl.hpp"
#include "reshade_events.hpp"
#include <string>

namespace reshade
{
#if RESHADE_ADDON
	template <addon_event ev, typename F, typename RArgs = addon_event_traits<ev>::decl>
	struct addon_event_call_chain;
	template <addon_event ev, typename F, typename R, typename... Args>
	struct addon_event_call_chain<ev, F, R(*)(addon_event_trampoline<ev> &, Args...)> : public addon_event_trampoline_data<R(Args...)>
	{
		F terminator_data;

		explicit addon_event_call_chain(F &&lambda) : terminator_data(lambda)
		{
			std::vector<void *> &event_list = addon::event_list[static_cast<size_t>(ev)];
#if _ITERATOR_DEBUG_LEVEL != 0
			next_callback = reinterpret_cast<callback_type *>(event_list.data());
			last_callback = next_callback + event_list.size();
#else // std::vector already stores pointers to the begin and end, so loading those directly generates better code (but only works without iterator debugging in case it is empty)
			next_callback = reinterpret_cast<callback_type *>(&(*event_list.begin()));
			last_callback = reinterpret_cast<callback_type *>(&(*event_list.end()));
#endif
			term_callback = static_cast<callback_type>(
				[](addon_event_trampoline_data &call_chain, Args... args) -> R {
				return static_cast<addon_event_call_chain &>(call_chain).terminator_data(std::forward<Args>(args)...);
			});
	}
};
#endif

	template <addon_event ev, typename F, typename... Args>
	inline void invoke_addon_event(F &&terminator, Args... args)
	{
		static_assert(reshade::addon_event_traits<ev>::has_trampoline == true);
#if RESHADE_ADDON
		addon_event_call_chain<ev, F>(std::move(terminator))(std::forward<Args>(args)...);
#else
		terminator(std::forward<Args>(args)...);
#endif
	}
	template <addon_event ev, typename... Args>
	inline void invoke_addon_event_without_trampoline(Args... args)
	{
		static_assert(reshade::addon_event_traits<ev>::has_trampoline == false);
#if RESHADE_ADDON
		std::vector<void *> &event_list = addon::event_list[static_cast<size_t>(ev)];
		for (size_t cb = 0; cb < event_list.size(); ++cb) // Generates better code than ranged-based for loop
			reinterpret_cast<typename reshade::addon_event_traits<ev>::decl>(event_list[cb])(std::forward<Args>(args)...);
#endif
	}
}

#if RESHADE_ADDON
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
