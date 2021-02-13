/*
 * Copyright (C) 2021 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "dll_config.hpp"
#include "addon_manager.hpp"
#include "runtime.hpp"
#include <algorithm>

reshade::api::api_data::entry::entry(uint32_t size, const void *data) : guid(), size(size)
{
	if (size > sizeof(storage))
	{
		const auto storage_mem = new uint8_t[size];
		std::memcpy(storage_mem, data, size);
		storage = reinterpret_cast<uint64_t>(storage_mem);
	}
	else
	{
		storage = 0;
		std::memcpy(&storage, data, size);
	}
}
reshade::api::api_data::entry::~entry()
{
	if (size > sizeof(storage))
	{
		delete[] reinterpret_cast<uint8_t *>(storage);
	}
}

bool reshade::api::api_data::get_data(const uint8_t guid[16], uint32_t size, void *data)
{
	const auto it = std::find_if(_entries.begin(), _entries.end(), [guid](const entry &e) { return std::memcmp(e.guid, guid, 16) == 0; });
	if (it != _entries.end())
	{
		size = std::min(size, it->size);
		if (it->size > sizeof(it->storage))
		{
			std::memcpy(data, reinterpret_cast<uint8_t *>(it->storage), size);
		}
		else
		{
			std::memcpy(data, &it->storage, size);
		}

		return true;
	}
	else
	{
		return false;
	}
}
void reshade::api::api_data::set_data(const uint8_t guid[16], uint32_t size, const void *data)
{
	const auto it = std::find_if(_entries.begin(), _entries.end(), [guid](const entry &e) { return std::memcmp(e.guid, guid, 16) == 0; });
	if (it != _entries.end())
	{
		if (data == nullptr)
		{
			_entries.erase(it);
		}
		else
		{
			*it = entry(size, data);
			std::memcpy(it->guid, guid, 16);
		}
	}
	else
	{
		_entries.emplace_back(size, data);
		std::memcpy(_entries.back().guid, guid, 16);
	}
}

bool reshade::addon::event_list_enabled = true;
std::vector<void *> reshade::addon::event_list[static_cast<size_t>(reshade::addon_event::num_addon_events)];
std::vector<reshade::addon::info> reshade::addon::loaded_info;
#if RESHADE_GUI
std::vector<std::pair<std::string, void(*)(reshade::api::effect_runtime *, void *)>> reshade::addon::overlay_list;
#endif

#if RESHADE_ADDON
#include <Windows.h>

extern void register_builtin_addon_depth();
extern void unregister_builtin_addon_depth();

static unsigned long s_reference_count = 0;

void reshade::addon::load_addons()
{
	if (s_reference_count++ != 0)
		return;

	// Load built-in add-ons
	register_builtin_addon_depth();
	loaded_info.push_back({ nullptr, "Generic Depth" });

	std::filesystem::path addon_search_path = g_reshade_base_path;
	if (global_config().get("INSTALL", "AddonPath", addon_search_path))
		addon_search_path = g_reshade_base_path / addon_search_path;

	std::error_code ec;
	for (std::filesystem::path path : std::filesystem::directory_iterator(addon_search_path, std::filesystem::directory_options::skip_permission_denied,ec))
	{
		if (path.extension() != L".addon")
			continue;

		reshade::addon::info info;
		info.name = path.filename().u8string();
		info.handle = LoadLibraryW(path.c_str());
		if (info.handle == nullptr)
			continue;

		loaded_info.push_back(std::move(info));
	}
}
void reshade::addon::unload_addons()
{
	if (--s_reference_count != 0)
		return;

	for (const reshade::addon::info &info : loaded_info)
	{
		if (info.handle != nullptr)
			FreeLibrary(static_cast<HMODULE>(info.handle));
	}

	// Unload built-in add-ons
	unregister_builtin_addon_depth();

	loaded_info.clear();
}

extern "C" __declspec(dllexport) void ReShadeRegisterEvent(reshade::addon_event ev, void *callback)
{
	auto &event_list = reshade::addon::event_list[static_cast<size_t>(ev)];
	event_list.push_back(callback);
}
extern "C" __declspec(dllexport) void ReShadeUnregisterEvent(reshade::addon_event ev, void *callback)
{
	auto &event_list = reshade::addon::event_list[static_cast<size_t>(ev)];
	event_list.erase(std::remove(event_list.begin(), event_list.end(), callback), event_list.end());
}

#if RESHADE_GUI
extern "C" __declspec(dllexport) void ReShadeRegisterOverlay(const char *title, void(*callback)(reshade::api::effect_runtime *runtime, void *imgui_context))
{
	auto &overlay_list = reshade::addon::overlay_list;
	overlay_list.emplace_back(title, callback);
}
extern "C" __declspec(dllexport) void ReShadeUnregisterOverlay(const char *title)
{
	struct func {
		const char *title;
		bool operator()(const std::pair<std::string, void(*)(reshade::api::effect_runtime *, void *)> &it) { return it.first == title; }
	};

	auto &overlay_list = reshade::addon::overlay_list;
	overlay_list.erase(std::remove_if(overlay_list.begin(), overlay_list.end(), func { title }), overlay_list.end());
}

extern struct imgui_function_table g_imgui_function_table;

extern "C" __declspec(dllexport) imgui_function_table *ReShadeGetImGuiFunctionTable()
{
	return &g_imgui_function_table;
}
#endif

#endif
