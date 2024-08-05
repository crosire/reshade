/*
 * Copyright (C) 2021 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause OR MIT
 */

#include <reshade.hpp>
#include "config.hpp"
#include "crc32_hash.hpp"
#include <cstring>
#include <fstream>
#include <filesystem>

using namespace reshade::api;

constexpr uint32_t SPIRV_MAGIC = 0x07230203;

static thread_local std::vector<std::vector<uint8_t>> s_data_to_delete;

static bool load_shader_code(device_api device_type, shader_desc &desc, std::vector<std::vector<uint8_t>> &data_to_delete)
{
	if (desc.code_size == 0)
		return false;

	uint32_t shader_hash = compute_crc32(static_cast<const uint8_t *>(desc.code), desc.code_size);

	const wchar_t *extension = L".cso";
	if (device_type == device_api::vulkan || (device_type == device_api::opengl && desc.code_size > sizeof(uint32_t) && *static_cast<const uint32_t *>(desc.code) == SPIRV_MAGIC))
		extension = L".spv"; // Vulkan uses SPIR-V (and sometimes OpenGL does too)
	else if (device_type == device_api::opengl)
		extension = desc.code_size > 5 && std::strncmp(static_cast<const char *>(desc.code), "!!ARB", 5) == 0 ? L".txt" : L".glsl"; // OpenGL otherwise uses plain text ARB assembly language or GLSL

	// Prepend executable file name to image files
	wchar_t file_prefix[MAX_PATH] = L"";
	GetModuleFileNameW(nullptr, file_prefix, ARRAYSIZE(file_prefix));

	std::filesystem::path replace_path = file_prefix;
	replace_path  = replace_path.parent_path();
	replace_path /= RESHADE_ADDON_SHADER_LOAD_DIR;

	wchar_t hash_string[11];
	swprintf_s(hash_string, L"0x%08X", shader_hash);

	replace_path /= hash_string;
	replace_path += extension;

	// Check if a replacement file for this shader hash exists and if so, overwrite the shader code with its contents
	if (!std::filesystem::exists(replace_path))
		return false;

	std::ifstream file(replace_path, std::ios::binary);
	file.seekg(0, std::ios::end);
	std::vector<uint8_t> shader_code(static_cast<size_t>(file.tellg()));
	file.seekg(0, std::ios::beg).read(reinterpret_cast<char *>(shader_code.data()), shader_code.size());

	// Keep the shader code memory alive after returning from this 'create_pipeline' event callback
	// It may only be freed after the 'init_pipeline' event was called for this pipeline
	data_to_delete.push_back(std::move(shader_code));

	desc.code = data_to_delete.back().data();
	desc.code_size = data_to_delete.back().size();
	return true;
}

static bool on_create_pipeline(device *device, pipeline_layout, uint32_t subobject_count, const pipeline_subobject *subobjects)
{
	bool replaced_stages = false;
	const device_api device_type = device->get_api();

	// Go through all shader stages that are in this pipeline and potentially replace the associated shader code
	for (uint32_t i = 0; i < subobject_count; ++i)
	{
		switch (subobjects[i].type)
		{
		case pipeline_subobject_type::vertex_shader:
		case pipeline_subobject_type::hull_shader:
		case pipeline_subobject_type::domain_shader:
		case pipeline_subobject_type::geometry_shader:
		case pipeline_subobject_type::pixel_shader:
		case pipeline_subobject_type::compute_shader:
		case pipeline_subobject_type::amplification_shader:
		case pipeline_subobject_type::mesh_shader:
		case pipeline_subobject_type::raygen_shader:
		case pipeline_subobject_type::any_hit_shader:
		case pipeline_subobject_type::closest_hit_shader:
		case pipeline_subobject_type::miss_shader:
		case pipeline_subobject_type::intersection_shader:
		case pipeline_subobject_type::callable_shader:
			replaced_stages |= load_shader_code(device_type, *static_cast<shader_desc *>(subobjects[i].data), s_data_to_delete);
			break;
		}
	}

	// Return whether any shader code was replaced
	return replaced_stages;
}
static void on_after_create_pipeline(device *, pipeline_layout, uint32_t, const pipeline_subobject *, pipeline)
{
	// Free the memory allocated in the 'load_shader_code' call above
	s_data_to_delete.clear();
}

extern "C" __declspec(dllexport) const char *NAME = "Shader Replace";
extern "C" __declspec(dllexport) const char *DESCRIPTION = "Example add-on that replaces shader binaries before they are used by the application with binaries from disk (\"" RESHADE_ADDON_SHADER_LOAD_DIR "\" directory).";

BOOL APIENTRY DllMain(HMODULE hModule, DWORD fdwReason, LPVOID)
{
	switch (fdwReason)
	{
	case DLL_PROCESS_ATTACH:
		if (!reshade::register_addon(hModule))
			return FALSE;
		reshade::register_event<reshade::addon_event::create_pipeline>(on_create_pipeline);
		reshade::register_event<reshade::addon_event::init_pipeline>(on_after_create_pipeline);
		break;
	case DLL_PROCESS_DETACH:
		reshade::unregister_addon(hModule);
		break;
	}

	return TRUE;
}
