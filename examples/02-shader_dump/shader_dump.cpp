/*
 * Copyright (C) 2021 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include <reshade.hpp>
#include "crc32_hash.hpp"
#include <fstream>
#include <filesystem>

using namespace reshade::api;

static void dump_shader_code(device_api device_type, shader_stage, const shader_desc &desc)
{
	if (desc.code_size == 0)
		return;

	uint32_t shader_hash = compute_crc32(static_cast<const uint8_t *>(desc.code), desc.code_size);

	const wchar_t *extension = L".cso";
	if (device_type == device_api::vulkan || (
		device_type == device_api::opengl && desc.code_size > sizeof(uint32_t) && *static_cast<const uint32_t *>(desc.code) == 0x07230203 /* SPIR-V magic */))
		extension = L".spv"; // Vulkan uses SPIR-V (and sometimes OpenGL does too)
	else if (device_type == device_api::opengl)
		extension = L".glsl"; // OpenGL otherwise uses plain text GLSL

	// Prepend executable file name to image files
	WCHAR file_prefix[MAX_PATH] = L"";
	GetModuleFileNameW(nullptr, file_prefix, ARRAYSIZE(file_prefix));

	char hash_string[11];
	sprintf_s(hash_string, "0x%08X", shader_hash);

	std::filesystem::path dump_path = file_prefix;
	dump_path += L'_';
	dump_path += L"shader_";
	dump_path += hash_string;
	dump_path += extension;

	std::ofstream file(dump_path, std::ios::binary);
	file.write(static_cast<const char *>(desc.code), desc.code_size);
}

static bool on_create_pipeline(device *device, pipeline_desc &desc, uint32_t, const dynamic_state *)
{
	const device_api device_type = device->get_api();

	// Go through all shader stages that are in this pipeline and dump the associated shader code
	if ((desc.type & pipeline_stage::vertex_shader) != 0)
		dump_shader_code(device_type, shader_stage::vertex, desc.graphics.vertex_shader);
	if ((desc.type & pipeline_stage::hull_shader) != 0)
		dump_shader_code(device_type, shader_stage::hull, desc.graphics.hull_shader);
	if ((desc.type & pipeline_stage::domain_shader) != 0)
		dump_shader_code(device_type, shader_stage::domain, desc.graphics.domain_shader);
	if ((desc.type & pipeline_stage::geometry_shader) != 0)
		dump_shader_code(device_type, shader_stage::geometry, desc.graphics.geometry_shader);
	if ((desc.type & pipeline_stage::pixel_shader) != 0)
		dump_shader_code(device_type, shader_stage::pixel, desc.graphics.pixel_shader);
	if ((desc.type & pipeline_stage::compute_shader) != 0)
		dump_shader_code(device_type, shader_stage::compute, desc.compute.shader);

	return false;
}

extern "C" __declspec(dllexport) const char *NAME = "Shader Dump";
extern "C" __declspec(dllexport) const char *DESCRIPTION = "Example add-on that dumps all shaders used by the application to disk.";

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
