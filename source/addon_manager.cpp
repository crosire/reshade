/*
 * Copyright (C) 2021 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#if RESHADE_ADDON

#include "reshade.hpp"
#include "addon_manager.hpp"
#include "dll_log.hpp"
#include "ini_file.hpp"
#include <algorithm> // std::find, std::find_if, std::remove, std::remove_if

extern void register_addon_depth();
extern void register_addon_effect_runtime_sync();
extern void unregister_addon_depth();
extern void unregister_addon_effect_runtime_sync();

extern HMODULE g_module_handle;

extern std::filesystem::path get_module_path(HMODULE module);

#if RESHADE_VERBOSE_LOG
static const char *addon_event_to_string(reshade::addon_event ev)
{
	using reshade::addon_event;
	switch (ev)
	{
	case addon_event::init_device: return "init_device";
	case addon_event::create_device: return "create_device";
	case addon_event::destroy_device: return "destroy_device";
	case addon_event::init_command_list: return "init_command_list";
	case addon_event::destroy_command_list: return "destroy_command_list";
	case addon_event::init_command_queue: return "init_command_queue";
	case addon_event::destroy_command_queue: return "destroy_command_queue";
	case addon_event::init_swapchain: return "init_swapchain";
	case addon_event::create_swapchain: return "create_swapchain";
	case addon_event::destroy_swapchain: return "destroy_swapchain";
	case addon_event::init_effect_runtime: return "init_effect_runtime";
	case addon_event::destroy_effect_runtime: return "destroy_effect_runtime";
	case addon_event::init_sampler: return "init_sampler";
	case addon_event::create_sampler: return "create_sampler";
	case addon_event::destroy_sampler: return "destroy_sampler";
	case addon_event::init_resource: return "init_resource";
	case addon_event::create_resource: return "create_resource";
	case addon_event::destroy_resource: return "destroy_resource";
	case addon_event::init_resource_view: return "init_resource_view";
	case addon_event::create_resource_view: return "create_resource_view";
	case addon_event::destroy_resource_view: return "destroy_resource_view";
	case addon_event::map_buffer_region: return "map_buffer_region";
	case addon_event::unmap_buffer_region: return "unmap_buffer_region";
	case addon_event::map_texture_region: return "map_texture_region";
	case addon_event::unmap_texture_region: return "unmap_texture_region";
	case addon_event::update_buffer_region: return "update_buffer_region";
	case addon_event::update_buffer_region_command: return "update_buffer_region_command";
	case addon_event::update_texture_region: return "update_texture_region";
	case addon_event::update_texture_region_command: return "update_texture_region_command";
	case addon_event::init_pipeline: return "init_pipeline";
	case addon_event::create_pipeline: return "create_pipeline";
	case addon_event::destroy_pipeline: return "destroy_pipeline";
	case addon_event::init_pipeline_layout: return "init_pipeline_layout";
	case addon_event::create_pipeline_layout: return "create_pipeline_layout";
	case addon_event::destroy_pipeline_layout: return "destroy_pipeline_layout";
	case addon_event::copy_descriptor_tables: return "copy_descriptor_tables";
	case addon_event::update_descriptor_tables: return "update_descriptor_tables";
	case addon_event::init_query_heap: return "init_query_heap";
	case addon_event::create_query_heap: return "create_query_heap";
	case addon_event::destroy_query_heap: return "destroy_query_heap";
	case addon_event::get_query_heap_results: return "get_query_heap_results";
	case addon_event::barrier: return "barrier";
	case addon_event::begin_render_pass: return "begin_render_pass";
	case addon_event::end_render_pass: return "end_render_pass";
	case addon_event::bind_render_targets_and_depth_stencil: return "bind_render_targets_and_depth_stencil";
	case addon_event::bind_pipeline: return "bind_pipeline";
	case addon_event::bind_pipeline_states: return "bind_pipeline_states";
	case addon_event::bind_viewports: return "bind_viewports";
	case addon_event::bind_scissor_rects: return "bind_scissor_rects";
	case addon_event::push_constants: return "push_constants";
	case addon_event::push_descriptors: return "push_descriptors";
	case addon_event::bind_descriptor_tables: return "bind_descriptor_tables";
	case addon_event::bind_index_buffer: return "bind_index_buffer";
	case addon_event::bind_vertex_buffers: return "bind_vertex_buffers";
	case addon_event::bind_stream_output_buffers: return "bind_stream_output_buffers";
	case addon_event::draw: return "draw";
	case addon_event::draw_indexed: return "draw_indexed";
	case addon_event::dispatch: return "dispatch";
	case addon_event::dispatch_mesh: return "dispatch_mesh";
	case addon_event::dispatch_rays: return "dispatch_rays";
	case addon_event::draw_or_dispatch_indirect: return "draw_or_dispatch_indirect";
	case addon_event::copy_resource: return "copy_resource";
	case addon_event::copy_buffer_region: return "copy_buffer_region";
	case addon_event::copy_buffer_to_texture: return "copy_buffer_to_texture";
	case addon_event::copy_texture_region: return "copy_texture_region";
	case addon_event::copy_texture_to_buffer: return "copy_texture_to_buffer";
	case addon_event::resolve_texture_region: return "resolve_texture_region";
	case addon_event::clear_depth_stencil_view: return "clear_depth_stencil_view";
	case addon_event::clear_render_target_view: return "clear_render_target_view";
	case addon_event::clear_unordered_access_view_uint: return "clear_unordered_access_view_uint";
	case addon_event::clear_unordered_access_view_float: return "clear_unordered_access_view_float";
	case addon_event::generate_mipmaps: return "generate_mipmaps";
	case addon_event::begin_query: return "begin_query";
	case addon_event::end_query: return "end_query";
	case addon_event::copy_query_heap_results: return "copy_query_heap_results";
	case addon_event::copy_acceleration_structure: return "copy_acceleration_structure";
	case addon_event::build_acceleration_structure: return "build_acceleration_structure";
	case addon_event::query_acceleration_structures: return "query_acceleration_structures";
	case addon_event::reset_command_list: return "reset_command_list";
	case addon_event::close_command_list: return "close_command_list";
	case addon_event::execute_command_list: return "execute_command_list";
	case addon_event::execute_secondary_command_list: return "execute_secondary_command_list";
	case addon_event::present: return "present";
	case addon_event::finish_present: return "finish_present";
	case addon_event::set_fullscreen_state: return "set_fullscreen_state";
	case addon_event::reshade_present: return "reshade_present";
	case addon_event::reshade_begin_effects: return "reshade_begin_effects";
	case addon_event::reshade_finish_effects: return "reshade_finish_effects";
	case addon_event::reshade_reloaded_effects: return "reshade_reloaded_effects";
	case addon_event::reshade_set_uniform_value: return "reshade_set_uniform_value";
	case addon_event::reshade_set_technique_state: return "reshade_set_technique_state";
	case addon_event::reshade_overlay: return "reshade_overlay";
	case addon_event::reshade_screenshot: return "reshade_screenshot";
	case addon_event::reshade_render_technique: return "reshade_render_technique";
	case addon_event::reshade_set_effects_state: return "reshade_set_effects_state";
	case addon_event::reshade_set_current_preset_path: return "reshade_set_current_preset_path";
	case addon_event::reshade_reorder_techniques: return "reshade_reorder_techniques";
	case addon_event::reshade_open_overlay: return "reshade_open_overlay";
	case addon_event::reshade_overlay_uniform_variable: return "reshade_overlay_uniform_variable";
	case addon_event::reshade_overlay_technique: return "reshade_overlay_technique";
	}
	return "unknown";
}
#endif

#if RESHADE_ADDON == 1
bool reshade::addon_enabled = true;
#endif
bool reshade::addon_all_loaded = true;
std::vector<void *> reshade::addon_event_list[static_cast<uint32_t>(reshade::addon_event::max)];
std::vector<reshade::addon_info> reshade::addon_loaded_info;
thread_local const reshade::addon_info *reshade::addon_current = nullptr;
static unsigned long s_reference_count = 0;

void reshade::load_addons()
{
	// Only load add-ons the first time a reference is added
	if (InterlockedIncrement(&s_reference_count) != 1)
		return;

	ini_file &config = global_config();

	addon_all_loaded = true;

	std::vector<std::string> disabled_addons;
	config.get("ADDON", "DisabledAddons", disabled_addons);

#if RESHADE_VERBOSE_LOG
	log::message(log::level::info, "Loading built-in add-ons ...");
#endif

#if 1
	{	addon_info &info = addon_loaded_info.emplace_back();
		info.name = "Generic Depth";
		info.description = "Automatic depth buffer detection that works in the majority of games.";
		info.file = g_reshade_dll_path.filename().u8string();
		info.author = "crosire";
		info.external = false;

		if (std::find(disabled_addons.cbegin(), disabled_addons.cend(), info.name) == disabled_addons.cend())
		{
			info.handle = g_module_handle;

			register_addon_depth();
		}
	}
	{	addon_info &info = addon_loaded_info.emplace_back();
		info.name = "Effect Runtime Sync";
		info.description = "Adds preset synchronization between different effect runtime instances, e.g. to have changes in a desktop window reflect in VR.";
		info.file = g_reshade_dll_path.filename().u8string();
		info.author = "crosire";
		info.external = false;

		if (std::find(disabled_addons.cbegin(), disabled_addons.cend(), info.name) == disabled_addons.cend())
		{
			info.handle = g_module_handle;

			register_addon_effect_runtime_sync();
		}
	}
#endif

	// Initialize any add-ons that were registered externally
	const std::vector<addon_info> loaded_info_copy = addon_loaded_info;
	for (const addon_info &info : loaded_info_copy)
	{
		if (info.handle == nullptr || info.handle == g_module_handle)
			continue; // Skip disabled and built-in add-ons

		assert(info.external);

		log::message(log::level::info, "Loading externally registered add-on \"%s\" ...", info.name.c_str());

		auto module = static_cast<HMODULE>(info.handle);

		const auto init_func = reinterpret_cast<bool(*)(HMODULE addon_module, HMODULE reshade_module)>(GetProcAddress(module, "AddonInit"));
		if (init_func != nullptr && !init_func(module, g_module_handle))
		{
			addon_all_loaded = false;
			log::message(log::level::error, "Failed to load add-on \"%s\" because initialization was not successful!", info.name.c_str());
		}
	}

	// Get directory from where to load add-ons from
	std::filesystem::path addon_search_path = g_reshade_base_path;
	if (config.get("ADDON", "AddonPath", addon_search_path))
		addon_search_path = g_reshade_base_path / addon_search_path;

	log::message(log::level::info, "Searching for add-ons (*.addon"
#ifndef _WIN64
		", *.addon32"
#else
		", *.addon64"
#endif
		") in '%s' ...", addon_search_path.u8string().c_str());

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

#if RESHADE_ADDON == 1
		// Indicate that add-ons exist that could not be loaded because this build of ReShade has only limited add-on functionality
		addon_all_loaded = false;

		log::message(log::level::warning, "Skipped loading add-on from '%s' because this build of ReShade has only limited add-on functionality.", path.u8string().c_str());
#else
		// Avoid loading library altogether when it is found in the disabled add-on list
		if (addon_info info;
			std::find_if(disabled_addons.cbegin(), disabled_addons.cend(),
				[file_name = path.filename().u8string(), &info](const std::string_view addon_name) {
					const size_t at_pos = addon_name.find('@');
					if (at_pos == std::string_view::npos)
						return false;
					info.name = addon_name.substr(0, at_pos);
					info.file = addon_name.substr(at_pos + 1);
					return file_name == info.file;
				}) != disabled_addons.cend())
		{
			info.handle = nullptr;
			info.external = false;
			addon_loaded_info.push_back(std::move(info));
			continue;
		}

		log::message(log::level::info, "Loading add-on from '%s' ...", path.u8string().c_str());

		// Use 'LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR' to temporarily add add-on search path to the list of directories 'LoadLibraryEx' will use to resolve DLL dependencies
		const HMODULE module = LoadLibraryExW(path.c_str(), nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
		if (module == nullptr)
		{
			const DWORD error_code = GetLastError();

			if (error_code == ERROR_DLL_INIT_FAILED && !addon_loaded_info.empty() && path.filename().u8string() == addon_loaded_info.back().file)
			{
				// Avoid logging an error if loading failed because the add-on is disabled
				assert(addon_loaded_info.back().handle == nullptr);

				addon_loaded_info.back().external = false;

				log::message(log::level::warning, "> Add-on failed to initialize or is disabled. Skipped.");
			}
			else
			{
				addon_all_loaded = false;
				log::message(log::level::error, "Failed to load add-on from '%s' with error code %lu!", path.u8string().c_str(), error_code);
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

				addon_loaded_info.back().external = false;

				log::message(log::level::warning, "> Add-on failed to initialize or is disabled. Skipped.");
			}
			else
			{
				addon_all_loaded = false;
				log::message(log::level::error, "Failed to load add-on from '%s' because initialization was not successful!", path.u8string().c_str());
			}

			FreeLibrary(module);
			continue;
		}

		if (addon_info *const registered_info = find_addon(module))
		{
			registered_info->external = false;
		}
		else
		{
			addon_all_loaded = false;
			log::message(log::level::warning, "No add-on was registered by '%s'. Unloading again ...", path.u8string().c_str());

			FreeLibrary(module);
		}
#endif
	}

	if (ec)
		log::message(log::level::warning, "Failed to iterate all files in '%s' with error code %d!", addon_search_path.u8string().c_str(), ec.value());
}
void reshade::unload_addons()
{
	// Only unload add-ons after the last reference to the manager was released
	if (InterlockedDecrement(&s_reference_count) != 0)
		return;

#if RESHADE_ADDON == 1
	// There are no add-ons to unload ...
#else
	// Create copy of add-on list before unloading, since add-ons call 'ReShadeUnregisterAddon' during 'FreeLibrary', which modifies the list
	const std::vector<addon_info> loaded_info_copy = addon_loaded_info;
	for (const addon_info &info : loaded_info_copy)
	{
		if (info.handle == nullptr || info.handle == g_module_handle)
			continue; // Skip disabled and built-in add-ons

		log::message(log::level::info, "Unloading add-on \"%s\" ...", info.name.c_str());

		const auto module = static_cast<HMODULE>(info.handle);

		// Call optional unloading entry point
		const auto uninit_func = reinterpret_cast<void(*)(HMODULE addon_module, HMODULE reshade_module)>(GetProcAddress(module, "AddonUninit"));
		if (uninit_func != nullptr)
			uninit_func(module, g_module_handle);

		if (info.external)
			continue;

		if (!FreeLibrary(module))
			log::message(log::level::warning, "Failed to unload '%s' with error code %lu!", info.file.c_str(), GetLastError());

		if (addon_info *const registered_info = find_addon(module))
		{
			log::message(log::level::warning, "Add-on \"%s\" was not unregistered!", registered_info->name.c_str());

			// Make add-on external at this point, so that it is not removed, since a consecutive load must be able to find it
			registered_info->external = true;
		}
	}
#endif

#if RESHADE_VERBOSE_LOG
	log::message(log::level::info, "Unloading built-in add-ons ...");
#endif

#if 1
	unregister_addon_depth();
	unregister_addon_effect_runtime_sync();
#endif

	// Remove all unloaded add-ons
	addon_loaded_info.erase(
		std::remove_if(addon_loaded_info.begin(), addon_loaded_info.end(),
			[](const addon_info &info) {
				// There should only be external, disabled and built-in add-ons in the list at this point (any other well behaving add-ons should already have unregistered themselves during unloading)
				assert(info.external || info.handle == nullptr || info.handle == g_module_handle);
				return !info.external;
			}),
		addon_loaded_info.end());
}

bool reshade::has_loaded_addons()
{
	// Ignore disabled and built-in add-ons
	return s_reference_count != 0 && std::find_if(addon_loaded_info.cbegin(), addon_loaded_info.cend(),
		[](const addon_info &info) {
			return info.handle != nullptr && info.handle != g_module_handle;
		}) != addon_loaded_info.cend();
}

reshade::addon_info *reshade::find_addon(void *address)
{
	if (address == nullptr)
		return nullptr;

	HMODULE module = nullptr;
	if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, reinterpret_cast<LPCWSTR>(address), &module))
		return nullptr;

	assert(module != nullptr);

	for (auto it = addon_loaded_info.rbegin(); it != addon_loaded_info.rend(); ++it)
		if (it->handle == module)
			return &(*it);
	return nullptr;
}

#if defined(RESHADE_API_LIBRARY_EXPORT)

bool ReShadeRegisterAddon(HMODULE module, uint32_t api_version)
{
	// Can only register an add-on module once
	if (module == nullptr || module == g_module_handle || reshade::find_addon(module))
	{
		reshade::log::message(reshade::log::level::error, "Failed to register add-on, because it provided an invalid module handle!");
		return false;
	}

	// Check that the requested API version is supported
	if (api_version == 0 || api_version > RESHADE_API_VERSION || (api_version / 10000) != (RESHADE_API_VERSION / 10000))
	{
		reshade::log::message(reshade::log::level::error, "Failed to register add-on, because the requested API version (%u) is not supported (%u)!", api_version, static_cast<uint32_t>(RESHADE_API_VERSION));
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
			char subblock[64];
			std::snprintf(subblock, std::size(subblock), "\\StringFileInfo\\%04x%04x\\%s", language, codepage, name);
			if (char *value = nullptr;
				VerQueryValueA(version_data.data(), subblock, reinterpret_cast<LPVOID *>(&value), nullptr))
				target = value;
		};

		query_file_version_info(info.name, "ProductName");
		query_file_version_info(info.author, "CompanyName");
		query_file_version_info(info.description, "FileDescription");

		if (VS_FIXEDFILEINFO *fixed_file_version_info = nullptr;
			VerQueryValueW(version_data.data(), L"\\", reinterpret_cast<LPVOID *>(&fixed_file_version_info), nullptr))
		{
			info.version = {
				HIWORD(fixed_file_version_info->dwFileVersionMS),
				LOWORD(fixed_file_version_info->dwFileVersionMS),
				HIWORD(fixed_file_version_info->dwFileVersionLS),
				LOWORD(fixed_file_version_info->dwFileVersionLS)
			};
		}
	}

	if (const char *const *name = reinterpret_cast<const char *const *>(GetProcAddress(module, "NAME")))
		info.name = *name;
	if (const char *const *description = reinterpret_cast<const char *const *>(GetProcAddress(module, "DESCRIPTION")))
		info.description = *description;
	if (const char *const *website_url = reinterpret_cast<const char *const *>(GetProcAddress(module, "WEBSITE")))
		info.website_url = *website_url;
	if (const char *const *issues_url = reinterpret_cast<const char *const *>(GetProcAddress(module, "ISSUES")))
		info.issues_url = *issues_url;

	if (std::find_if(reshade::addon_loaded_info.cbegin(), reshade::addon_loaded_info.cend(),
			[&info](const reshade::addon_info &existing_info) {
				return existing_info.name == info.name;
			}) != reshade::addon_loaded_info.cend())
	{
		// Prevent registration if another add-on with the same name already exists
		reshade::log::message(reshade::log::level::error, "Failed to register add-on, because another one with the same name (\"%s\") was already registered!", info.name.c_str());
		return false;
	}

	if (std::vector<std::string> disabled_addons;
		reshade::global_config().get("ADDON", "DisabledAddons", disabled_addons) &&
		std::find_if(disabled_addons.cbegin(), disabled_addons.cend(),
			[&info](const std::string_view addon_name) {
				const size_t at_pos = addon_name.find('@');
				if (at_pos == std::string_view::npos)
					return addon_name == info.name;
				return (at_pos == 0 || addon_name.substr(0, at_pos) == info.name) && addon_name.substr(at_pos + 1) == info.file;
			}) != disabled_addons.cend())
	{
		info.handle = nullptr;
		reshade::addon_loaded_info.push_back(std::move(info));
		return false; // Disable this add-on
	}

	reshade::log::message(reshade::log::level::info, "Registered add-on \"%s\" v%hu.%hu.%hu.%hu using ReShade API version %u.", info.name.c_str(), info.version.number.major, info.version.number.minor, info.version.number.build, info.version.number.revision, api_version);

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

	reshade::log::message(reshade::log::level::info, "Unregistered add-on \"%s\".", info->name.c_str());

	reshade::addon_loaded_info.erase(reshade::addon_loaded_info.begin() + (info - reshade::addon_loaded_info.data()));
}

void ReShadeRegisterEvent(reshade::addon_event ev, void *callback)
{
	ReShadeRegisterEventForAddon(nullptr, ev, callback);
}
void ReShadeRegisterEventForAddon(HMODULE module, reshade::addon_event ev, void *callback)
{
	if (ev >= reshade::addon_event::max)
		return;

	reshade::addon_info *const info = reshade::find_addon(module != nullptr ? module : callback);
	if (info == nullptr)
	{
		reshade::log::message(reshade::log::level::error, "Could not find associated add-on and therefore failed to register an event.");
		return;
	}

	assert(info->handle == module || module == nullptr);

#if RESHADE_ADDON == 1
	// Block all application events when building without add-on loading support
	if (info->handle != g_module_handle && (ev > reshade::addon_event::destroy_effect_runtime && ev < reshade::addon_event::present))
	{
		reshade::log::message(reshade::log::level::error, "Failed to register an event because only limited add-on functionality is available!");
		return;
	}
#endif

	std::vector<void *> &event_list = reshade::addon_event_list[static_cast<uint32_t>(ev)];
	event_list.push_back(callback);

	info->event_callbacks.emplace_back(static_cast<uint32_t>(ev), callback);

#if RESHADE_VERBOSE_LOG
	reshade::log::message(reshade::log::level::debug, "Registered event callback %p for event %s.", callback, addon_event_to_string(ev));
#endif
}
void ReShadeUnregisterEvent(reshade::addon_event ev, void *callback)
{
	ReShadeUnregisterEventForAddon(nullptr, ev, callback);
}
void ReShadeUnregisterEventForAddon(HMODULE module, reshade::addon_event ev, void *callback)
{
	if (ev >= reshade::addon_event::max)
		return;

	reshade::addon_info *const info = reshade::find_addon(module != nullptr ? module : callback);
	if (info == nullptr)
		return; // Do not log an error here, since this may be called if an add-on failed to load

	assert(info->handle == module || module == nullptr);

#if RESHADE_ADDON == 1
	if (info->handle != g_module_handle && (ev > reshade::addon_event::destroy_effect_runtime && ev < reshade::addon_event::present))
		return;
#endif

	std::vector<void *> &event_list = reshade::addon_event_list[static_cast<uint32_t>(ev)];
	event_list.erase(std::remove(event_list.begin(), event_list.end(), callback), event_list.end());

	info->event_callbacks.erase(std::remove(info->event_callbacks.begin(), info->event_callbacks.end(), std::make_pair(static_cast<uint32_t>(ev), callback)), info->event_callbacks.end());

#if RESHADE_VERBOSE_LOG
	reshade::log::message(reshade::log::level::debug, "Unregistered event callback %p for event %s.", callback, addon_event_to_string(ev));
#endif
}

#if RESHADE_GUI

void ReShadeRegisterOverlay(const char *title, void(*callback)(reshade::api::effect_runtime *runtime))
{
	ReShadeRegisterOverlayForAddon(nullptr, title, callback);
}
void ReShadeRegisterOverlayForAddon(HMODULE module, const char *title, void(*callback)(reshade::api::effect_runtime *runtime))
{
	reshade::addon_info *const info = reshade::find_addon(module != nullptr ? module : static_cast<void *>(callback));
	if (info == nullptr)
	{
		reshade::log::message(reshade::log::level::error, "Could not find associated add-on and therefore failed to register overlay with title \"%s\".", title);
		return;
	}

	assert(info->handle == module || module == nullptr);

	if (title == nullptr)
	{
		info->settings_overlay_callback = callback;
		return;
	}

	info->overlay_callbacks.push_back(reshade::addon_info::overlay_callback { title, callback });

#if RESHADE_VERBOSE_LOG
	reshade::log::message(reshade::log::level::debug, "Registered overlay with title \"%s\" and callback %p.", title, static_cast<void *>(callback));
#endif
}
void ReShadeUnregisterOverlay(const char *title, void(*callback)(reshade::api::effect_runtime *runtime))
{
	ReShadeUnregisterOverlayForAddon(nullptr, title, callback);
}
void ReShadeUnregisterOverlayForAddon(HMODULE module, const char *title, void(*callback)(reshade::api::effect_runtime *runtime))
{
	reshade::addon_info *const info = reshade::find_addon(module != nullptr ? module : static_cast<void *>(callback));
	if (info == nullptr)
		return; // Do not log an error here, since this may be called if an add-on failed to load

	assert(info->handle == module || module == nullptr);

	if (title == nullptr)
	{
		assert(callback == info->settings_overlay_callback);
		info->settings_overlay_callback = nullptr;
		return;
	}

#if RESHADE_VERBOSE_LOG
	// Log before removing from overlay list below, since pointer to title string may become invalid by the removal
	reshade::log::message(reshade::log::level::debug, "Unregistered overlay with title \"%s\" and callback %p.", title, static_cast<void *>(callback));
#endif

	info->overlay_callbacks.erase(std::remove_if(info->overlay_callbacks.begin(), info->overlay_callbacks.end(),
		[title, callback](const reshade::addon_info::overlay_callback &item) {
			return item.title == title && item.callback == callback;
		}), info->overlay_callbacks.end());
}

#endif

#endif

#endif
