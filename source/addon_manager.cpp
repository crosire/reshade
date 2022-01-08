/*
 * Copyright (C) 2021 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#if RESHADE_ADDON

#include "version.h"
#include "reshade.hpp"
#include "addon_manager.hpp"
#include "dll_log.hpp"
#include "ini_file.hpp"

extern void register_addon_depth();
extern void unregister_addon_depth();

extern HMODULE g_module_handle;

extern std::filesystem::path get_module_path(HMODULE module);

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
		CASE(map_buffer_region);
		CASE(unmap_buffer_region);
		CASE(map_texture_region);
		CASE(unmap_texture_region);
		CASE(update_buffer_region);
		CASE(update_texture_region);
		CASE(init_pipeline);
		CASE(create_pipeline);
		CASE(destroy_pipeline);
		CASE(init_pipeline_layout);
		CASE(create_pipeline_layout);
		CASE(destroy_pipeline_layout);
		CASE(copy_descriptor_sets);
		CASE(update_descriptor_sets);
		CASE(init_query_pool);
		CASE(create_query_pool);
		CASE(destroy_query_pool);
		CASE(get_query_pool_results);
		CASE(barrier);
		CASE(begin_render_pass);
		CASE(end_render_pass);
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
		CASE(clear_depth_stencil_view);
		CASE(clear_render_target_view);
		CASE(clear_unordered_access_view_uint);
		CASE(clear_unordered_access_view_float);
		CASE(generate_mipmaps);
		CASE(begin_query);
		CASE(end_query);
		CASE(copy_query_pool_results);
		CASE(reset_command_list);
		CASE(execute_command_list);
		CASE(execute_secondary_command_list);
		CASE(present);
		CASE(reshade_begin_effects);
		CASE(reshade_finish_effects);
		CASE(reshade_reloaded_effects);
	}
#undef  CASE
	return "unknown";
}
#endif

#if RESHADE_LITE
bool reshade::addon::enabled = true;
#endif
std::vector<void *> reshade::addon::event_list[static_cast<uint32_t>(reshade::addon_event::max)];
std::vector<reshade::addon::info> reshade::addon::loaded_info;
static unsigned long s_reference_count = 0;

void reshade::load_addons()
{
	// Only load add-ons the first time a reference is added
	if (s_reference_count++ != 0)
		return;

#if RESHADE_VERBOSE_LOG
	LOG(INFO) << "Loading built-in add-ons ...";
#endif

	internal::get_reshade_module_handle() = g_module_handle;
	internal::get_current_module_handle() = g_module_handle;

	std::vector<std::string> disabled_addons;
	global_config().get("ADDON", "DisabledAddons", disabled_addons);

	{	addon::info &info = addon::loaded_info.emplace_back();
		info.name = "Generic Depth";
		info.description = "Automatic depth buffer detection that works in the majority of games.";
		info.file = g_reshade_dll_path.u8string();
		info.author = "crosire";
		info.version = VERSION_STRING_FILE;

		if (std::find(disabled_addons.begin(), disabled_addons.end(), info.name) == disabled_addons.end())
		{
			info.handle = g_module_handle;

			register_addon_depth();
		}
	}

#if !RESHADE_LITE
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

#if !RESHADE_LITE
	// Create copy of add-on list before unloading, since add-ons call 'ReShadeUnregisterAddon' during 'FreeLibrary', which modifies the list
	const std::vector<addon::info> loaded_info_copy = addon::loaded_info;
	for (const addon::info &info : loaded_info_copy)
	{
		if (info.handle == nullptr || info.handle == g_module_handle)
			continue; // Skip disabled and built-in add-ons

		LOG(INFO) << "Unloading add-on \"" << info.name << "\" ...";

		FreeLibrary(static_cast<HMODULE>(info.handle));
	}
#endif

#if RESHADE_VERBOSE_LOG
	LOG(INFO) << "Unloading built-in add-ons ...";
#endif

	unregister_addon_depth();

	addon::loaded_info.clear();
}

#if RESHADE_ADDON

reshade::addon::info *find_addon(HMODULE module)
{
	for (auto it = reshade::addon::loaded_info.rbegin(); it != reshade::addon::loaded_info.rend(); ++it)
		if (it->handle == module)
			return &(*it);
	return nullptr;
}
reshade::addon::info *find_addon_from_address(void *address)
{
	if (address == nullptr)
		return nullptr;

	HMODULE module = nullptr;
	if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, reinterpret_cast<LPCWSTR>(address), &module))
		return nullptr;

	return find_addon(module);
}

extern "C" __declspec(dllexport) bool ReShadeRegisterAddon(HMODULE module, uint32_t api_version);
extern "C" __declspec(dllexport) void ReShadeUnregisterAddon(HMODULE module);

extern "C" __declspec(dllexport) void ReShadeRegisterEvent(reshade::addon_event ev, void *callback);
extern "C" __declspec(dllexport) void ReShadeUnregisterEvent(reshade::addon_event ev, void *callback);

extern "C" __declspec(dllexport) void ReShadeRegisterOverlay(const char *title, void(*callback)(reshade::api::effect_runtime *runtime));
extern "C" __declspec(dllexport) void ReShadeUnregisterOverlay(const char *title, void(*callback)(reshade::api::effect_runtime *runtime));

bool ReShadeRegisterAddon(HMODULE module, uint32_t api_version)
{
	// Can only register an add-on module once
	if (module == nullptr || module == g_module_handle || find_addon(module) != nullptr)
		return false;

	// Check that the requested API version is supported
	if (api_version > RESHADE_API_VERSION)
		return false;

	const std::filesystem::path path = get_module_path(module);

	reshade::addon::info info;
	info.name = path.stem().u8string();
	info.file = path.u8string();
	info.handle = module;

	DWORD version_dummy, version_size = GetFileVersionInfoSizeW(path.c_str(), &version_dummy);
	std::vector<uint8_t> version_data(version_size);
	if (GetFileVersionInfoW(path.c_str(), version_dummy, version_size, version_data.data()))
	{
		if (char *product_name = nullptr;
			VerQueryValueA(version_data.data(), "\\StringFileInfo\\040004b0\\ProductName", reinterpret_cast<LPVOID *>(&product_name), nullptr))
			info.name = product_name;

		if (char *company_name = nullptr;
			VerQueryValueA(version_data.data(), "\\StringFileInfo\\040004b0\\CompanyName", reinterpret_cast<LPVOID *>(&company_name), nullptr))
			info.author = company_name;

		if (char *file_version = nullptr;
			VerQueryValueA(version_data.data(), "\\StringFileInfo\\040004b0\\FileVersion", reinterpret_cast<LPVOID *>(&file_version), nullptr))
			info.version = file_version;

		if (char *file_description = nullptr;
			VerQueryValueA(version_data.data(), "\\StringFileInfo\\040004b0\\FileDescription", reinterpret_cast<LPVOID *>(&file_description), nullptr))
			info.description = file_description;
	}

	if (const char *const *name = reinterpret_cast<const char *const *>(GetProcAddress(module, "NAME"));
		name != nullptr)
		info.name = *name;
	if (const char *const *description = reinterpret_cast<const char *const *>(GetProcAddress(module, "DESCRIPTION"));
		description != nullptr)
		info.description = *description;

	if (info.version.empty())
		info.version = "1.0.0.0";

	if (std::find_if(reshade::addon::loaded_info.begin(), reshade::addon::loaded_info.end(),
		[&info](const auto &existing_info) { return existing_info.name == info.name; }) != reshade::addon::loaded_info.end())
	{
		// Prevent registration if another add-on with the same name already exists
		return false;
	}

	if (std::vector<std::string> disabled_addons;
		reshade::global_config().get("ADDON", "DisabledAddons", disabled_addons) &&
		std::find(disabled_addons.begin(), disabled_addons.end(), info.name) != disabled_addons.end())
	{
		info.handle = nullptr;
		reshade::addon::loaded_info.push_back(std::move(info));
		return false; // Disable this add-on
	}

	LOG(INFO) << "Registered add-on \"" << info.name << "\" v" << info.version << '.';

	reshade::addon::loaded_info.push_back(std::move(info));

	return true;
}
void ReShadeUnregisterAddon(HMODULE module)
{
	if (module == nullptr || module == g_module_handle)
		return;

	reshade::addon::info *const info = find_addon(module);
	if (info == nullptr)
		return;

	// Unregister all event callbacks registered by this add-on
	while (!info->event_callbacks.empty())
	{
		const auto &last_event_callback = info->event_callbacks.back();
		ReShadeUnregisterEvent(static_cast<reshade::addon_event>(last_event_callback.first), last_event_callback.second);
	}

#if RESHADE_GUI
	// Unregister all overlay callbacks associated with this add-on
	while (!info->overlay_callbacks.empty())
	{
		const auto &last_overlay_callback = info->overlay_callbacks.back();
		ReShadeUnregisterOverlay(last_overlay_callback.first.c_str(), last_overlay_callback.second);
	}
#endif

	LOG(INFO) << "Unregistered add-on \"" << info->name << "\".";

	reshade::addon::loaded_info.erase(reshade::addon::loaded_info.begin() + (info - reshade::addon::loaded_info.data()));
}

void ReShadeRegisterEvent(reshade::addon_event ev, void *callback)
{
	if (ev >= reshade::addon_event::max)
		return;

	reshade::addon::info *const info = find_addon_from_address(callback);
	if (info == nullptr)
		return;

#if RESHADE_LITE
	// Block all application events when building without add-on loading support
	if (info->handle != g_module_handle && (ev > reshade::addon_event::destroy_effect_runtime && ev < reshade::addon_event::present))
		return;
#endif

	auto &event_list = reshade::addon::event_list[static_cast<uint32_t>(ev)];
	event_list.push_back(callback);

	info->event_callbacks.emplace_back(static_cast<uint32_t>(ev), callback);

#if RESHADE_VERBOSE_LOG
	LOG(DEBUG) << "Registered event callback " << callback << " for event " << addon_event_to_string(ev) << '.';
#endif
}
void ReShadeUnregisterEvent(reshade::addon_event ev, void *callback)
{
	if (ev >= reshade::addon_event::max)
		return;

	reshade::addon::info *const info = find_addon_from_address(callback);
	if (info == nullptr)
		return;

#if RESHADE_LITE
	if (info->handle != g_module_handle && (ev > reshade::addon_event::destroy_effect_runtime && ev < reshade::addon_event::present))
		return;
#endif

	auto &event_list = reshade::addon::event_list[static_cast<uint32_t>(ev)];
	event_list.erase(std::remove(event_list.begin(), event_list.end(), callback), event_list.end());

	info->event_callbacks.erase(std::remove(info->event_callbacks.begin(), info->event_callbacks.end(), std::make_pair(static_cast<uint32_t>(ev), callback)), info->event_callbacks.end());

#if RESHADE_VERBOSE_LOG
	LOG(DEBUG) << "Unregistered event callback " << callback << " for event " << addon_event_to_string(ev) << '.';
#endif
}

#if RESHADE_GUI
void ReShadeRegisterOverlay(const char *title, void(*callback)(reshade::api::effect_runtime *runtime))
{
	reshade::addon::info *const info = find_addon_from_address(callback);
	if (info == nullptr)
		return;

	if (title == nullptr)
	{
		info->settings_overlay_callback = callback;
		return;
	}

	info->overlay_callbacks.emplace_back(title, callback);

#if RESHADE_VERBOSE_LOG
	LOG(DEBUG) << "Registered overlay with title \"" << title << "\" and callback " << callback << '.';
#endif
}
void ReShadeUnregisterOverlay(const char *title, void(*callback)(reshade::api::effect_runtime *runtime))
{
	reshade::addon::info *const info = find_addon_from_address(callback);
	if (info == nullptr)
		return;

	if (title == nullptr)
	{
		assert(callback == info->settings_overlay_callback);
		info->settings_overlay_callback = nullptr;
		return;
	}

	info->overlay_callbacks.erase(std::remove(info->overlay_callbacks.begin(), info->overlay_callbacks.end(), std::make_pair<std::string>(title, callback)), info->overlay_callbacks.end());

#if RESHADE_VERBOSE_LOG
	LOG(DEBUG) << "Unregistered overlay with title \"" << title << "\" and callback " << callback << '.';
#endif
}
#endif

#endif

#endif
