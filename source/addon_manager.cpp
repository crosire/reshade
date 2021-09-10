/*
 * Copyright (C) 2021 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#if RESHADE_ADDON

#include "dll_log.hpp"
#include "ini_file.hpp"
#include "addon_manager.hpp"
#include <Windows.h>

extern std::filesystem::path get_module_path(HMODULE module);

bool reshade::addon::enabled = true;
std::vector<void *> reshade::addon::event_list[static_cast<size_t>(reshade::addon_event::max)];
std::vector<reshade::addon::info> reshade::addon::loaded_info;
#if RESHADE_GUI
std::vector<std::pair<std::string, void(*)(reshade::api::effect_runtime *, void *)>> reshade::addon::overlay_list;
#endif
static unsigned long s_reference_count = 0;

extern void register_builtin_addon_depth(reshade::addon::info &info);
extern void unregister_builtin_addon_depth();

void reshade::load_addons()
{
	// Only load add-ons the first time a reference is added
	if (s_reference_count++ != 0)
		return;

#if RESHADE_VERBOSE_LOG
	LOG(INFO) << "Loading built-in add-ons ...";
#endif
	register_builtin_addon_depth(addon::loaded_info.emplace_back());

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

		// Use 'LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR' to temporarily add add-on search path to the list of directories "LoadLibraryEx" will use to resolve DLL dependencies
		const HMODULE handle = LoadLibraryExW(path.c_str(), nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
		if (handle == nullptr)
		{
			LOG(WARN) << "Failed to load add-on from " << path << " with error code " << GetLastError() << '.';
			continue;
		}
	}
#endif
}
void reshade::unload_addons()
{
	// Only unload add-ons after the last reference to the manager was released
	if (--s_reference_count != 0)
		return;

#if RESHADE_ADDON_LOAD
	for (const reshade::addon::info &info : addon::loaded_info)
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

	addon::loaded_info.clear();
}

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
		CASE(init_swapchain);
		CASE(create_swapchain);
		CASE(destroy_swapchain);
		CASE(init_effect_runtime);
		CASE(destroy_effect_runtime);
		CASE(init_sampler);
		CASE(create_sampler);
		CASE(destroy_sampler);
		CASE(init_resource);
		CASE(create_resource);
		CASE(destroy_resource);
		CASE(init_resource_view);
		CASE(create_resource_view);
		CASE(destroy_resource_view);
		CASE(init_pipeline);
		CASE(create_pipeline);
		CASE(destroy_pipeline);
		CASE(init_render_pass);
		CASE(create_render_pass);
		CASE(destroy_render_pass);
		CASE(init_framebuffer);
		CASE(create_framebuffer);
		CASE(destroy_framebuffer);
		CASE(update_buffer_region);
		CASE(update_texture_region);
		CASE(update_descriptor_sets);
		CASE(barrier);
		CASE(begin_render_pass);
		CASE(finish_render_pass);
		CASE(bind_render_targets_and_depth_stencil);
		CASE(bind_pipeline);
		CASE(bind_pipeline_states);
		CASE(bind_viewports);
		CASE(bind_scissor_rects);
		CASE(push_constants);
		CASE(push_descriptors);
		CASE(bind_descriptor_sets);
		CASE(bind_index_buffer);
		CASE(bind_vertex_buffers);
		CASE(draw);
		CASE(draw_indexed);
		CASE(dispatch);
		CASE(draw_or_dispatch_indirect);
		CASE(copy_resource);
		CASE(copy_buffer_region);
		CASE(copy_buffer_to_texture);
		CASE(copy_texture_region);
		CASE(copy_texture_to_buffer);
		CASE(resolve_texture_region);
		CASE(clear_attachments);
		CASE(clear_depth_stencil_view);
		CASE(clear_render_target_view);
		CASE(clear_unordered_access_view_uint);
		CASE(clear_unordered_access_view_float);
		CASE(generate_mipmaps);
		CASE(reset_command_list);
		CASE(execute_command_list);
		CASE(execute_secondary_command_list);
		CASE(present);
		CASE(reshade_begin_effects);
		CASE(reshade_finish_effects);
	}
#undef  CASE
	return "unknown";
}
#endif

extern "C" __declspec(dllexport) bool ReShadeRegister(HMODULE module, uint32_t api_version)
{
	if (module == nullptr)
		return false;

	// Check that the requested API version is supported
	if (api_version > RESHADE_API_VERSION)
		return false;

	reshade::addon::info info;
	info.name = get_module_path(module).stem().u8string();
	info.handle = module;

	if (const char *const *name = reinterpret_cast<const char *const *>(GetProcAddress(module, "NAME"));
		name != nullptr)
		info.name = *name;
	if (const char *const *description = reinterpret_cast<const char *const *>(GetProcAddress(module, "DESCRIPTION"));
		description != nullptr)
		info.description = *description;

	LOG(INFO) << "Registered add-on \"" << info.name << "\".";

	reshade::addon::loaded_info.push_back(std::move(info));

	return true;
}
extern "C" __declspec(dllexport) void ReShadeUnregister(HMODULE module)
{
	for (auto it = reshade::addon::loaded_info.begin(); it != reshade::addon::loaded_info.end();)
	{
		const reshade::addon::info &info = *it;

		if (info.handle == module)
		{
			LOG(INFO) << "Unregistered add-on \"" << info.name << "\".";

			it = reshade::addon::loaded_info.erase(it);
		}
		else
		{
			++it;
		}
	}
}

extern "C" __declspec(dllexport) void ReShadeRegisterEvent(reshade::addon_event ev, void *callback)
{
	if (ev >= reshade::addon_event::max)
		return;

	assert(callback != nullptr);

	auto &event_list = reshade::addon::event_list[static_cast<size_t>(ev)];
	event_list.push_back(callback);

#if RESHADE_VERBOSE_LOG
	LOG(DEBUG) << "Registered event callback " << callback << " for event " << addon_event_to_string(ev) << '.';
#endif
}
extern "C" __declspec(dllexport) void ReShadeUnregisterEvent(reshade::addon_event ev, void *callback)
{
	if (ev >= reshade::addon_event::max)
		return;

	assert(callback != nullptr);

	auto &event_list = reshade::addon::event_list[static_cast<size_t>(ev)];
	event_list.erase(std::remove(event_list.begin(), event_list.end(), callback), event_list.end());

#if RESHADE_VERBOSE_LOG
	LOG(DEBUG) << "Unregistered event callback " << callback << " for event " << addon_event_to_string(ev) << '.';
#endif
}

#if RESHADE_GUI
extern "C" __declspec(dllexport) void ReShadeRegisterOverlay(const char *title, void(*callback)(reshade::api::effect_runtime *runtime, void *imgui_context))
{
	assert(callback != nullptr);

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
#endif

#endif
