/*
 * Copyright (C) 2021 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#if RESHADE_ADDON

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
		CASE(bind_stream_output_buffers);
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
		CASE(close_command_list);
		CASE(execute_command_list);
		CASE(execute_secondary_command_list);
		CASE(present);
		CASE(reshade_present);
		CASE(reshade_begin_effects);
		CASE(reshade_finish_effects);
		CASE(reshade_reloaded_effects);
		CASE(reshade_set_uniform_value);
		CASE(reshade_set_technique_state);
		CASE(reshade_overlay);
		CASE(reshade_screenshot);
		CASE(reshade_render_technique);
	}
#undef  CASE
	return "unknown";
}
#endif

#if RESHADE_ADDON_LITE
bool reshade::addon_enabled = true;
#endif
bool reshade::addon_all_loaded = true;
std::vector<void *> reshade::addon_event_list[static_cast<uint32_t>(reshade::addon_event::max)];
std::vector<reshade::addon_info> reshade::addon_loaded_info;
static unsigned long s_reference_count = 0;

void reshade::load_addons()
{
	// Only load add-ons the first time a reference is added
	if (InterlockedIncrement(&s_reference_count) != 1)
		return;

	ini_file &config = global_config();

#if RESHADE_VERBOSE_LOG
	LOG(INFO) << "Loading built-in add-ons ...";
#endif

	addon_all_loaded = true;
	internal::get_reshade_module_handle(g_module_handle);
	internal::get_current_module_handle(g_module_handle);

	std::vector<std::string> disabled_addons;
	config.get("ADDON", "DisabledAddons", disabled_addons);

#if 1
	{	addon_info &info = addon_loaded_info.emplace_back();
		info.name = "Generic Depth";
		info.description = "Automatic depth buffer detection that works in the majority of games.";
		info.file = g_reshade_dll_path.filename().u8string();
		info.author = "crosire";

		if (std::find(disabled_addons.begin(), disabled_addons.end(), info.name) == disabled_addons.end())
		{
			info.handle = g_module_handle;

			register_addon_depth();
		}
	}
#endif

#if !RESHADE_ADDON_LITE
	// Get directory from where to load add-ons from
	std::filesystem::path addon_search_path = g_reshade_base_path;
	if (config.get("ADDON", "AddonPath", addon_search_path))
		addon_search_path = g_reshade_base_path / addon_search_path;

#ifndef _WIN64
	LOG(INFO) << "Searching for add-ons (*.addon, *.addon32) in " << addon_search_path << " ...";
#else
	LOG(INFO) << "Searching for add-ons (*.addon, *.addon64) in " << addon_search_path << " ...";
#endif

	std::error_code ec;
	for (std::filesystem::path path : std::filesystem::directory_iterator(addon_search_path, std::filesystem::directory_options::skip_permission_denied, ec))
	{
		if (path.extension() != L".addon" &&
#ifndef _WIN64
			path.extension() != L".addon32")
#else
			path.extension() != L".addon64")
#endif
			continue;

		// Avoid loading library altogether when it is found in the disabled add-on list
		if (addon_info info;
			std::find_if(disabled_addons.begin(), disabled_addons.end(),
				[file_name = path.filename().u8string(), &info](const std::string_view &addon_name) {
					const size_t at_pos = addon_name.find('@');
					if (at_pos == std::string::npos)
						return false;
					info.name = addon_name.substr(0, at_pos);
					info.file = addon_name.substr(at_pos + 1);
					return file_name == info.file;
				}) != disabled_addons.end())
		{
			info.handle = nullptr;
			addon_loaded_info.push_back(std::move(info));
			continue;
		}

		LOG(INFO) << "Loading add-on from " << path << " ...";

		// Use 'LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR' to temporarily add add-on search path to the list of directories 'LoadLibraryEx' will use to resolve DLL dependencies
		const HMODULE module = LoadLibraryExW(path.c_str(), nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
		if (module == nullptr)
		{
			const DWORD error_code = GetLastError();

			if (!addon_loaded_info.empty() && path.filename().u8string() == addon_loaded_info.back().file)
			{
				// Avoid logging an error if loading failed because the add-on is disabled
				assert(addon_loaded_info.back().handle == nullptr);

				LOG(INFO) << "> Add-on is disabled. Skipped.";
			}
			else
			{
				addon_all_loaded = false;
				LOG(ERROR) << "Failed to load add-on from " << path << " with error code " << error_code << '!';
			}
			continue;
		}

		// Call optional loading entry point (for add-ons wanting to do more complicated one-time initialization than possible in 'DllMain')
		const auto init_func = reinterpret_cast<bool(*)(HMODULE addon_module, HMODULE reshade_module)>(GetProcAddress(module, "AddonInit"));
		if (init_func != nullptr && !init_func(module, g_module_handle))
		{
			if (!addon_loaded_info.empty() && path.filename().u8string() == addon_loaded_info.back().file)
			{
				assert(addon_loaded_info.back().handle == nullptr);

				LOG(INFO) << "> Add-on is disabled. Skipped.";
			}
			else
			{
				addon_all_loaded = false;
				LOG(ERROR) << "Failed to load add-on from " << path << " because initialization was not successfull!";
			}

			FreeLibrary(module);
			continue;
		}

		if (find_addon(module) == nullptr)
		{
			addon_all_loaded = false;
			LOG(WARN) << "No add-on was registered by " << path << ". Unloading again ...";

			FreeLibrary(module);
		}
	}

	if (ec)
		LOG(WARN) << "Failed to iterate all files in " << addon_search_path << " with error code " << ec.value() << '!';
#endif
}
void reshade::unload_addons()
{
	// Only unload add-ons after the last reference to the manager was released
	if (InterlockedDecrement(&s_reference_count) != 0)
		return;

#if !RESHADE_ADDON_LITE
	// Create copy of add-on list before unloading, since add-ons call 'ReShadeUnregisterAddon' during 'FreeLibrary', which modifies the list
	const std::vector<addon_info> loaded_info_copy = addon_loaded_info;
	for (const addon_info &info : loaded_info_copy)
	{
		if (info.handle == nullptr || info.handle == g_module_handle)
			continue; // Skip disabled and built-in add-ons

		LOG(INFO) << "Unloading add-on \"" << info.name << "\" ...";

		const auto module = static_cast<HMODULE>(info.handle);

		// Call optional unloading entry point
		const auto uninit_func = reinterpret_cast<void(*)(HMODULE addon_module, HMODULE reshade_module)>(GetProcAddress(module, "AddonUninit"));
		if (uninit_func != nullptr)
			uninit_func(module, g_module_handle);

		if (!FreeLibrary(module))
			LOG(WARN) << "Failed to unload " << std::filesystem::u8path(info.file) << " with error code " << GetLastError() << '!';
	}
#endif

#if RESHADE_VERBOSE_LOG
	LOG(INFO) << "Unloading built-in add-ons ...";
#endif

#if 1
	unregister_addon_depth();
#endif

#ifndef NDEBUG
	// All events should have been unregistered at this point
	for (const std::vector<void *> &event_info : addon_event_list)
		assert(event_info.empty());
#endif

	addon_loaded_info.clear();
}

bool reshade::has_loaded_addons()
{
	// Ignore disabled and built-in add-ons
	return s_reference_count != 0 && std::find_if(addon_loaded_info.begin(), addon_loaded_info.end(),
		[](const addon_info &info) {
			return info.handle != nullptr && info.handle != g_module_handle;
		}) != addon_loaded_info.end();
}

reshade::addon_info *reshade::find_addon(void *address)
{
	if (address == nullptr)
		return nullptr;

	HMODULE module = nullptr;
	if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, reinterpret_cast<LPCWSTR>(address), &module))
		return nullptr;

	for (auto it = addon_loaded_info.rbegin(); it != addon_loaded_info.rend(); ++it)
		if (it->handle == module)
			return &(*it);
	return nullptr;
}

extern "C" __declspec(dllexport) bool ReShadeRegisterAddon(HMODULE module, uint32_t api_version);
extern "C" __declspec(dllexport) void ReShadeUnregisterAddon(HMODULE module);

extern "C" __declspec(dllexport) void ReShadeRegisterEvent(reshade::addon_event ev, void *callback);
extern "C" __declspec(dllexport) void ReShadeUnregisterEvent(reshade::addon_event ev, void *callback);

#if RESHADE_GUI
extern "C" __declspec(dllexport) void ReShadeRegisterOverlay(const char *title, void(*callback)(reshade::api::effect_runtime *runtime));
extern "C" __declspec(dllexport) void ReShadeUnregisterOverlay(const char *title, void(*callback)(reshade::api::effect_runtime *runtime));
#endif

bool ReShadeRegisterAddon(HMODULE module, uint32_t api_version)
{
	// Can only register an add-on module once
	if (module == nullptr || module == g_module_handle || reshade::find_addon(module))
	{
		LOG(ERROR) << "Failed to register add-on, because it provided an invalid module handle!";
		return false;
	}

	// Check that the requested API version is supported
	if (api_version == 0 || api_version > RESHADE_API_VERSION || (api_version / 10000) != (RESHADE_API_VERSION / 10000))
	{
		LOG(ERROR) << "Failed to register add-on, because the requested API version (" << api_version << ") is not supported (" << RESHADE_API_VERSION << ")!";
		return false;
	}

	const std::filesystem::path path = get_module_path(module);

	reshade::addon_info info;
	info.name = path.stem().u8string();
	info.file = path.filename().u8string();
	info.handle = module;

	DWORD version_dummy, version_size = GetFileVersionInfoSizeW(path.c_str(), &version_dummy);
	std::vector<uint8_t> version_data(version_size);
	if (GetFileVersionInfoW(path.c_str(), version_dummy, version_size, version_data.data()))
	{
		WORD language = 0x0400;
		WORD codepage = 0x04b0;

		if (WORD *translation = nullptr;
			VerQueryValueA(version_data.data(), "\\VarFileInfo\\Translation", reinterpret_cast<LPVOID *>(&translation), nullptr))
			// Simply use the first translation available
			language = translation[0], codepage = translation[1];

		const auto query_file_version_info = [&version_data, language, codepage](std::string &target, const char *name) {
			char subblock[64] = "";
			sprintf_s(subblock, "\\StringFileInfo\\%04x%04x\\%s", language, codepage, name);
			if (char *value = nullptr;
				VerQueryValueA(version_data.data(), subblock, reinterpret_cast<LPVOID *>(&value), nullptr))
				target = value;
		};

		query_file_version_info(info.name, "ProductName");
		query_file_version_info(info.author, "CompanyName");
		query_file_version_info(info.version, "FileVersion");
		query_file_version_info(info.description, "FileDescription");
	}

	if (const char *const *name = reinterpret_cast<const char *const *>(GetProcAddress(module, "NAME"));
		name != nullptr)
		info.name = *name;
	if (const char *const *description = reinterpret_cast<const char *const *>(GetProcAddress(module, "DESCRIPTION"));
		description != nullptr)
		info.description = *description;

	if (std::find_if(reshade::addon_loaded_info.begin(), reshade::addon_loaded_info.end(),
			[&info](const reshade::addon_info &existing_info) {
				return existing_info.name == info.name;
			}) != reshade::addon_loaded_info.end())
	{
		// Prevent registration if another add-on with the same name already exists
		LOG(ERROR) << "Failed to register add-on, because another one with the same name (\"" << info.name << "\") was already registered!";
		return false;
	}

	if (std::vector<std::string> disabled_addons;
		reshade::global_config().get("ADDON", "DisabledAddons", disabled_addons) &&
		std::find_if(disabled_addons.begin(), disabled_addons.end(),
			[&info](const std::string_view &addon_name) {
				const size_t at_pos = addon_name.find('@');
				if (at_pos == std::string::npos)
					return addon_name == info.name;
				return addon_name.substr(0, at_pos) == info.name && addon_name.substr(at_pos + 1) == info.file;
			}) != disabled_addons.end())
	{
		info.handle = nullptr;
		reshade::addon_loaded_info.push_back(std::move(info));
		return false; // Disable this add-on
	}

	LOG(INFO) << "Registered add-on \"" << info.name << "\" v" << (info.version.empty() ? "1.0.0.0" : info.version) << '.';

	reshade::addon_loaded_info.push_back(std::move(info));

	return true;
}
void ReShadeUnregisterAddon(HMODULE module)
{
	if (module == nullptr || module == g_module_handle)
		return;

	reshade::addon_info *const info = reshade::find_addon(module);
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
		ReShadeUnregisterOverlay(last_overlay_callback.title.c_str(), last_overlay_callback.callback);
	}
#endif

	LOG(INFO) << "Unregistered add-on \"" << info->name << "\".";

	reshade::addon_loaded_info.erase(reshade::addon_loaded_info.begin() + (info - reshade::addon_loaded_info.data()));
}

void ReShadeRegisterEvent(reshade::addon_event ev, void *callback)
{
	if (ev >= reshade::addon_event::max)
		return;

	reshade::addon_info *const info = reshade::find_addon(callback);
	if (info == nullptr)
	{
		LOG(ERROR) << "Could not find associated add-on and therefore failed to register an event.";
		return;
	}

#if RESHADE_ADDON_LITE
	// Block all application events when building without add-on loading support
	if (info->handle != g_module_handle && (ev > reshade::addon_event::destroy_effect_runtime && ev < reshade::addon_event::present))
	{
		LOG(ERROR) << "Failed to register an event because only limited add-on functionality is available!";
		return;
	}
#endif

	std::vector<void *> &event_list = reshade::addon_event_list[static_cast<uint32_t>(ev)];
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

	reshade::addon_info *const info = reshade::find_addon(callback);
	if (info == nullptr)
		return; // Do not log an error here, since this may be called if an add-on failed to load

#if RESHADE_ADDON_LITE
	if (info->handle != g_module_handle && (ev > reshade::addon_event::destroy_effect_runtime && ev < reshade::addon_event::present))
		return;
#endif

	std::vector<void *> &event_list = reshade::addon_event_list[static_cast<uint32_t>(ev)];
	event_list.erase(std::remove(event_list.begin(), event_list.end(), callback), event_list.end());

	info->event_callbacks.erase(std::remove(info->event_callbacks.begin(), info->event_callbacks.end(), std::make_pair(static_cast<uint32_t>(ev), callback)), info->event_callbacks.end());

#if RESHADE_VERBOSE_LOG
	LOG(DEBUG) << "Unregistered event callback " << callback << " for event " << addon_event_to_string(ev) << '.';
#endif
}

#if RESHADE_GUI

void ReShadeRegisterOverlay(const char *title, void(*callback)(reshade::api::effect_runtime *runtime))
{
	reshade::addon_info *const info = reshade::find_addon(callback);
	if (info == nullptr)
	{
		LOG(ERROR) << "Could not find associated add-on and therefore failed to register overlay with title \"" << title << "\".";
		return;
	}

	if (title == nullptr)
	{
		info->settings_overlay_callback = callback;
		return;
	}

	info->overlay_callbacks.push_back(reshade::addon_info::overlay_callback { title, callback });

#if RESHADE_VERBOSE_LOG
	LOG(DEBUG) << "Registered overlay with title \"" << title << "\" and callback " << callback << '.';
#endif
}
void ReShadeUnregisterOverlay(const char *title, void(*callback)(reshade::api::effect_runtime *runtime))
{
	reshade::addon_info *const info = reshade::find_addon(callback);
	if (info == nullptr)
		return; // Do not log an error here, since this may be called if an add-on failed to load

	if (title == nullptr)
	{
		assert(callback == info->settings_overlay_callback);
		info->settings_overlay_callback = nullptr;
		return;
	}

#if RESHADE_VERBOSE_LOG
	// Log before removing from overlay list below, since pointer to title string may become invalid by the removal
	LOG(DEBUG) << "Unregistered overlay with title \"" << title << "\" and callback " << callback << '.';
#endif

	info->overlay_callbacks.erase(std::remove_if(info->overlay_callbacks.begin(), info->overlay_callbacks.end(),
		[title, callback](const reshade::addon_info::overlay_callback &item) { return item.title == title && item.callback == callback; }), info->overlay_callbacks.end());
}

#endif

#endif
