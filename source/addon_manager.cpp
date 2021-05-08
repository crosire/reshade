/*
 * Copyright (C) 2021 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#if RESHADE_ADDON

#include "dll_log.hpp"
#include "dll_config.hpp"
#include "addon_manager.hpp"
#include <Windows.h>

bool g_addons_enabled = true;
std::vector<void *> reshade::addon::event_list[static_cast<uint32_t>(reshade::addon_event::max)];
std::vector<reshade::addon::info> reshade::addon::loaded_info;
#if RESHADE_GUI
std::vector<std::pair<std::string, void(*)(reshade::api::effect_runtime *, void *)>> reshade::addon::overlay_list;
#endif
static unsigned long s_reference_count = 0;

extern void register_builtin_addon_depth(reshade::addon::info &info);
extern void unregister_builtin_addon_depth();

#if RESHADE_VERBOSE_LOG
static const char *addon_event_to_string(reshade::addon_event ev)
{
#define CASE(name) case reshade::addon_event::name: return #name
	switch (ev)
	{
		CASE(init_device);
		CASE(destroy_device);
		CASE(init_command_list);
		CASE(destroy_command_list);
		CASE(init_command_queue);
		CASE(destroy_command_queue);
		CASE(init_effect_runtime);
		CASE(destroy_effect_runtime);
		CASE(create_resource);
		CASE(create_resource_view);
		CASE(create_pipeline);
		CASE(create_shader_module);
		CASE(upload_buffer_region);
		CASE(upload_texture_region);
		CASE(bind_index_buffer);
		CASE(bind_vertex_buffers);
		CASE(bind_pipeline);
		CASE(bind_pipeline_states);
		CASE(push_constants);
		CASE(push_descriptors);
		CASE(bind_descriptor_heaps);
		CASE(bind_descriptor_tables);
		CASE(bind_viewports);
		CASE(bind_scissor_rects);
		CASE(bind_render_targets_and_depth_stencil);
		CASE(draw);
		CASE(draw_indexed);
		CASE(dispatch);
		CASE(draw_or_dispatch_indirect);
		CASE(blit);
		CASE(resolve);
		CASE(copy_resource);
		CASE(copy_buffer_region);
		CASE(copy_buffer_to_texture);
		CASE(copy_texture_region);
		CASE(copy_texture_to_buffer);
		CASE(clear_depth_stencil_view);
		CASE(clear_render_target_views);
		CASE(clear_unordered_access_view_uint);
		CASE(clear_unordered_access_view_float);
		CASE(reset_command_list);
		CASE(execute_command_list);
		CASE(execute_secondary_command_list);
		CASE(resize);
		CASE(present);
		CASE(reshade_after_effects);
		CASE(reshade_before_effects);
	}
#undef  CASE
	return "unknown";
}
#endif

void reshade::addon::load_addons()
{
	// Only load add-ons the first time a reference is added
	if (s_reference_count++ != 0)
		return;

#if RESHADE_VERBOSE_LOG
	LOG(INFO) << "Loading built-in add-ons ...";
#endif
	register_builtin_addon_depth(loaded_info.emplace_back());

#if RESHADE_ADDON_LOAD
	// Get directory from where to load add-ons from
	std::filesystem::path addon_search_path = g_reshade_base_path;
	if (global_config().get("INSTALL", "AddonPath", addon_search_path))
		addon_search_path = g_reshade_base_path / addon_search_path;

	LOG(INFO) << "Searching for add-ons (*.addon) in " << addon_search_path << " ...";

	std::error_code ec;
	for (std::filesystem::path path : std::filesystem::directory_iterator(addon_search_path, std::filesystem::directory_options::skip_permission_denied, ec))
	{
		if (path.extension() != L".addon")
			continue;

		LOG(INFO) << "Loading add-on from " << path << " ...";

		const HMODULE handle = LoadLibraryW(path.c_str());
		if (handle == nullptr)
			continue;

		reshade::addon::info info;
		info.name = path.stem().u8string();
		info.handle = handle;

		if (const char *name = reinterpret_cast<const char *>(GetProcAddress(handle, "NAME"));
			name != nullptr)
			info.name = name;
		if (const char *description = reinterpret_cast<const char *>(GetProcAddress(handle, "DESCRIPTION"));
			description != nullptr)
			info.description = description;

		LOG(INFO) << "> Loaded as \"" << info.name << "\".";

		loaded_info.push_back(std::move(info));
	}
#endif
}
void reshade::addon::unload_addons()
{
	// Only unload add-ons after the last reference to the manager was released
	if (--s_reference_count != 0)
		return;

#if RESHADE_ADDON_LOAD
	for (const reshade::addon::info &info : loaded_info)
	{
		if (info.handle == nullptr)
			continue; // Skip built-in add-ons

		LOG(INFO) << "Unloading add-on \"" << info.name << "\" ...";

		FreeLibrary(static_cast<HMODULE>(info.handle));
	}
#endif

#if RESHADE_VERBOSE_LOG
	LOG(INFO) << "Unloading built-in add-ons ...";
#endif
	unregister_builtin_addon_depth();

	loaded_info.clear();
}

void reshade::addon::enable_or_disable_addons(bool enabled)
{
	if (enabled == g_addons_enabled)
		return;

	static std::vector<void *> disabled_event_list[std::size(event_list)];
	if (enabled)
	{
		for (size_t event_index = 0; event_index < std::size(event_list); ++event_index)
			event_list[event_index] = std::move(disabled_event_list[event_index]);
	}
	else
	{
		for (size_t event_index = 0; event_index < std::size(event_list); ++event_index)
			disabled_event_list[event_index] = std::move(event_list[event_index]);
	}

	g_addons_enabled = enabled;
}

extern "C" __declspec(dllexport) void ReShadeRegisterEvent(reshade::addon_event ev, void *callback)
{
	auto &event_list = reshade::addon::event_list[static_cast<size_t>(ev)];
	event_list.push_back(callback);

#if RESHADE_VERBOSE_LOG
	LOG(DEBUG) << "Registered event callback " << callback << " for event " << addon_event_to_string(ev) << '.';
#endif
}
extern "C" __declspec(dllexport) void ReShadeUnregisterEvent(reshade::addon_event ev, void *callback)
{
	auto &event_list = reshade::addon::event_list[static_cast<size_t>(ev)];
	event_list.erase(std::remove(event_list.begin(), event_list.end(), callback), event_list.end());

#if RESHADE_VERBOSE_LOG
	LOG(DEBUG) << "Unregistered event callback " << callback << " for event " << addon_event_to_string(ev) << '.';
#endif
}

#if RESHADE_GUI
extern "C" __declspec(dllexport) void ReShadeRegisterOverlay(const char *title, void(*callback)(reshade::api::effect_runtime *runtime, void *imgui_context))
{
	auto &overlay_list = reshade::addon::overlay_list;
	overlay_list.emplace_back(title, callback);

#if RESHADE_VERBOSE_LOG
	LOG(DEBUG) << "Registered overlay callback " << callback << " with title \"" << title << "\".";
#endif
}
extern "C" __declspec(dllexport) void ReShadeUnregisterOverlay(const char *title)
{
	auto &overlay_list = reshade::addon::overlay_list;

	// Need to use a functor instead of a lambda here, since lambdas don't compile in functions declared 'extern "C"' on MSVC ...
	struct predicate {
		const char *title;
		bool operator()(const std::pair<std::string, void(*)(reshade::api::effect_runtime *, void *)> &it) const { return it.first == title; }
	};

	const auto callback_it = std::find_if(overlay_list.begin(), overlay_list.end(), predicate { title });
	if (callback_it == overlay_list.end())
		return;
#if RESHADE_VERBOSE_LOG
	LOG(DEBUG) << "Unregistered overlay callback " << callback_it->second << " with title \"" << title << "\".";
#endif

	overlay_list.erase(callback_it);
}

extern struct imgui_function_table g_imgui_function_table;

extern "C" __declspec(dllexport) const imgui_function_table *ReShadeGetImGuiFunctionTable()
{
	return &g_imgui_function_table;
}
#endif

#endif
