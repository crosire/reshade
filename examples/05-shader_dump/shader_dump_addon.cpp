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

static void save_shader_code(device_api device_type, const shader_desc &desc)
{
	if (desc.code_size == 0)
		return;

	uint32_t shader_hash = compute_crc32(static_cast<const uint8_t *>(desc.code), desc.code_size);

	const wchar_t *extension = L".cso";
	if (device_type == device_api::vulkan || (device_type == device_api::opengl && desc.code_size > sizeof(uint32_t) && *static_cast<const uint32_t *>(desc.code) == SPIRV_MAGIC))
		extension = L".spv"; // Vulkan uses SPIR-V (and sometimes OpenGL does too)
	else if (device_type == device_api::opengl)
		extension = desc.code_size > 5 && std::strncmp(static_cast<const char *>(desc.code), "!!ARB", 5) == 0 ? L".txt" : L".glsl"; // OpenGL otherwise uses plain text ARB assembly language or GLSL

	// Prepend executable directory to image files
	wchar_t file_prefix[MAX_PATH] = L"";
	GetModuleFileNameW(nullptr, file_prefix, ARRAYSIZE(file_prefix));

	std::filesystem::path dump_path = file_prefix;
	dump_path  = dump_path.parent_path();
	dump_path /= RESHADE_ADDON_SHADER_SAVE_DIR;

	if (std::filesystem::exists(dump_path) == false)
		std::filesystem::create_directory(dump_path);

	wchar_t hash_string[11];
	swprintf_s(hash_string, L"0x%08X", shader_hash);

	dump_path /= hash_string;
	dump_path += extension;

	std::ofstream file(dump_path, std::ios::binary);
	file.write(static_cast<const char *>(desc.code), desc.code_size);
}

static bool on_create_pipeline(device *device, pipeline_layout, uint32_t subobject_count, const pipeline_subobject *subobjects)
{
	const device_api device_type = device->get_api();

	// Go through all shader stages that are in this pipeline and dump the associated shader code
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
			save_shader_code(device_type, *static_cast<const shader_desc *>(subobjects[i].data));
			break;
		}
	}

	return false;
}

extern "C" __declspec(dllexport) const char *NAME = "Shader Dump";
extern "C" __declspec(dllexport) const char *DESCRIPTION = "Example add-on that dumps all shader binaries used by the application to disk (\"" RESHADE_ADDON_SHADER_SAVE_DIR "\" directory).";

BOOL APIENTRY DllMain(HMODULE hModule, DWORD fdwReason, LPVOID)
{
	switch (fdwReason)
	{
	case DLL_PROCESS_ATTACH:
		if (!reshade::register_addon(hModule))
			return FALSE;
		reshade::register_event<reshade::addon_event::create_pipeline>(on_create_pipeline);
		break;
	case DLL_PROCESS_DETACH:
		reshade::unregister_addon(hModule);
		break;
	}

	return TRUE;
}
