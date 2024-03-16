/*
 * Copyright (C) 2024 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <imgui.h>
#include <reshade.hpp>
#include <vector>
#include <shared_mutex>

using namespace reshade::api;

static std::shared_mutex s_mutex;
static bool s_sync = false;
static std::vector<effect_runtime *> s_runtimes;

static void on_init(effect_runtime *runtime)
{
	const std::unique_lock<std::shared_mutex> lock(s_mutex);

	s_runtimes.push_back(runtime);

	reshade::get_config_value(nullptr, "ADDON", "SyncEffectRuntimes", s_sync);
}
static void on_destroy(effect_runtime *runtime)
{
	const std::unique_lock<std::shared_mutex> lock(s_mutex);

	s_runtimes.erase(std::remove(s_runtimes.begin(), s_runtimes.end(), runtime), s_runtimes.end());
}

static bool on_reshade_set_uniform_value(effect_runtime *runtime, effect_uniform_variable variable, const void *new_value, size_t new_value_size)
{
	if (!s_sync)
		return false;

	// Skip special uniform variables
	if (runtime->get_annotation_string_from_uniform_variable(variable, "source", nullptr, nullptr))
		return false;

	format base_type = format::unknown;
	runtime->get_uniform_variable_type(variable, &base_type);

	char name[128] = "";
	runtime->get_uniform_variable_name(variable, name);
	char effect_name[128] = "";
	runtime->get_uniform_variable_effect_name(variable, effect_name);

	const std::shared_lock<std::shared_mutex> lock(s_mutex);

	for (effect_runtime *const synced_runtime : s_runtimes)
	{
		if (synced_runtime == runtime)
			continue;

		const effect_uniform_variable synced_variable = synced_runtime->find_uniform_variable(effect_name, name);
		if (synced_variable == 0)
			continue;

		switch (base_type)
		{
		case format::r32_typeless:
			synced_runtime->set_uniform_value_bool(synced_variable, static_cast<const bool *>(new_value), new_value_size / sizeof(bool));
			break;
		case format::r32_float:
			synced_runtime->set_uniform_value_float(synced_variable, static_cast<const float *>(new_value), new_value_size / sizeof(float));
			break;
		case format::r32_sint:
			synced_runtime->set_uniform_value_int(synced_variable, static_cast<const int32_t *>(new_value), new_value_size / sizeof(int32_t));
			break;
		case format::r32_uint:
			synced_runtime->set_uniform_value_uint(synced_variable, static_cast<const uint32_t *>(new_value), new_value_size / sizeof(uint32_t));
			break;
		}
	}

	return false;
}
static bool on_reshade_set_technique_state(effect_runtime *runtime, effect_technique technique, bool enabled)
{
	if (!s_sync)
		return false;

	// Skip timeout updates
	if (!enabled && runtime->get_annotation_string_from_technique(technique, "timeout", nullptr, nullptr))
		return false;

	char name[128] = "";
	runtime->get_technique_name(technique, name);
	char effect_name[128] = "";
	runtime->get_technique_effect_name(technique, effect_name);

	const std::shared_lock<std::shared_mutex> lock(s_mutex);

	for (effect_runtime *const synced_runtime : s_runtimes)
	{
		if (synced_runtime == runtime)
			continue;

		const effect_technique synced_technique = synced_runtime->find_technique(effect_name, name);
		if (synced_technique == 0)
			continue;

		synced_runtime->set_technique_state(synced_technique, enabled);
	}

	return false;
}
static void on_reshade_set_current_preset_path(effect_runtime *runtime, const char *path)
{
	if (!s_sync)
		return;

	const std::shared_lock<std::shared_mutex> lock(s_mutex);

	for (effect_runtime *synced_runtime : s_runtimes)
	{
		if (synced_runtime == runtime)
			continue;

		synced_runtime->set_current_preset_path(path);
	}
}
static bool on_reshade_reorder_techniques(effect_runtime *runtime, size_t count, effect_technique *techniques)
{
	if (!s_sync)
		return false;

	const std::shared_lock<std::shared_mutex> lock(s_mutex);

	for (effect_runtime *const synced_runtime : s_runtimes)
	{
		if (synced_runtime == runtime)
			continue;

		std::vector<effect_technique> synced_techniques(count);
		for (size_t i = 0; i < count; ++i)
		{
			char name[128] = "";
			runtime->get_technique_name(techniques[i], name);
			char effect_name[128] = "";
			runtime->get_technique_effect_name(techniques[i], effect_name);

			synced_techniques[i] = synced_runtime->find_technique(effect_name, name);
		}

		synced_runtime->reorder_techniques(synced_techniques.size(), synced_techniques.data());
	}

	return false;
}

static void draw_settings_overlay(effect_runtime *runtime)
{
	const std::unique_lock<std::shared_mutex> lock(s_mutex);

	if (s_runtimes.size() == 1)
	{
		assert(s_runtimes[0] == runtime);

		ImGui::TextUnformatted("This is the only active effect runtime instance.");
		return;
	}

	if (ImGui::Button("Apply preset of this effect runtime to all other instances", ImVec2(-1, 0)))
	{
		char preset_path[256] = "";
		runtime->get_current_preset_path(preset_path);

		for (effect_runtime *const synced_runtime : s_runtimes)
		{
			if (synced_runtime == runtime)
				continue;

			synced_runtime->set_current_preset_path(preset_path);
		}
	}

	if (ImGui::Checkbox("Synchronize effect runtimes", &s_sync))
		reshade::set_config_value(nullptr, "ADDON", "SyncEffectRuntimes", s_sync);

	if (!s_sync)
		return;

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Spacing();

	ImGui::Text("%p is being synchronized with:", runtime);

	for (effect_runtime *const synced_runtime : s_runtimes)
	{
		if (synced_runtime == runtime)
			continue;

		ImGui::Text("> %p", synced_runtime);
	}
}

void register_addon_effect_runtime_sync()
{
	reshade::register_overlay(nullptr, draw_settings_overlay);

	reshade::register_event<reshade::addon_event::init_effect_runtime>(on_init);
	reshade::register_event<reshade::addon_event::destroy_effect_runtime>(on_destroy);

	reshade::register_event<reshade::addon_event::reshade_set_uniform_value>(on_reshade_set_uniform_value);
	reshade::register_event<reshade::addon_event::reshade_set_technique_state>(on_reshade_set_technique_state);
	reshade::register_event<reshade::addon_event::reshade_set_current_preset_path>(on_reshade_set_current_preset_path);
	reshade::register_event<reshade::addon_event::reshade_reorder_techniques>(on_reshade_reorder_techniques);
}
void unregister_addon_effect_runtime_sync()
{
	reshade::unregister_event<reshade::addon_event::init_effect_runtime>(on_init);
	reshade::unregister_event<reshade::addon_event::destroy_effect_runtime>(on_destroy);

	reshade::unregister_event<reshade::addon_event::reshade_set_uniform_value>(on_reshade_set_uniform_value);
	reshade::unregister_event<reshade::addon_event::reshade_set_technique_state>(on_reshade_set_technique_state);
	reshade::unregister_event<reshade::addon_event::reshade_set_current_preset_path>(on_reshade_set_current_preset_path);
	reshade::unregister_event<reshade::addon_event::reshade_reorder_techniques>(on_reshade_reorder_techniques);
}

#ifndef BUILTIN_ADDON

extern "C" __declspec(dllexport) const char *NAME = "Effect Runtime Sync";
extern "C" __declspec(dllexport) const char *DESCRIPTION = "Adds preset synchronization between different effect runtime instances, e.g. to have changes in a desktop window reflect in VR.";

BOOL APIENTRY DllMain(HMODULE hModule, DWORD fdwReason, LPVOID)
{
	switch (fdwReason)
	{
	case DLL_PROCESS_ATTACH:
		if (!reshade::register_addon(hModule))
			return FALSE;
		register_addon_effect_runtime_sync();
		break;
	case DLL_PROCESS_DETACH:
		unregister_addon_effect_runtime_sync();
		reshade::unregister_addon(hModule);
		break;
	}

	return TRUE;
}

#endif
