/*
 * Copyright (C) 2021 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "reshade.hpp"
#include "crc32_hash.hpp"
#include <fstream>
#include <filesystem>

#ifdef BUILTIN_ADDON
#include "ini_file.hpp"
#endif

using namespace reshade::api;

static void dump_shader_code(device_api device_type, pipeline_stage, const shader_desc &desc)
{
	bool dump = false;
	std::filesystem::path dump_path;

#ifdef BUILTIN_ADDON
	ini_file &config = reshade::global_config();
	config.get("SHADER", "DumpAll", dump);
	config.get("SHADER", "DumpPath", dump_path);
#else
	dump = true;
#endif

	if (dump)
	{
		uint32_t shader_hash = compute_crc32(static_cast<const uint8_t *>(desc.code), desc.code_size);

		const wchar_t *extension = L".cso";
		if (device_type == device_api::vulkan || (
			device_type == device_api::opengl && desc.code_size > sizeof(uint32_t) && *static_cast<const uint32_t *>(desc.code) == 0x07230203 /* SPIR-V magic */))
			extension = L".spv"; // Vulkan uses SPIR-V (and sometimes OpenGL does too)
		else if (device_type == device_api::opengl)
			extension = L".glsl"; // OpenGL otherwise uses plain text GLSL

		char hash_string[11];
		sprintf_s(hash_string, "0x%08x", shader_hash);

		dump_path /= L"shader_";
		dump_path += hash_string;
		dump_path += extension;

		std::ofstream file(dump_path, std::ios::binary);
		file.write(static_cast<const char *>(desc.code), desc.code_size);
	}
}

static bool on_create_pipeline(device *device, pipeline_desc &desc)
{
	const device_api device_type = device->get_api();

	if ((desc.type & pipeline_stage::vertex_shader) == pipeline_stage::vertex_shader)
		dump_shader_code(device_type, pipeline_stage::vertex_shader, desc.graphics.vertex_shader);
	if ((desc.type & pipeline_stage::hull_shader) == pipeline_stage::hull_shader)
		dump_shader_code(device_type, pipeline_stage::hull_shader, desc.graphics.hull_shader);
	if ((desc.type & pipeline_stage::domain_shader) == pipeline_stage::domain_shader)
		dump_shader_code(device_type, pipeline_stage::domain_shader, desc.graphics.domain_shader);
	if ((desc.type & pipeline_stage::geometry_shader) == pipeline_stage::geometry_shader)
		dump_shader_code(device_type, pipeline_stage::geometry_shader, desc.graphics.geometry_shader);
	if ((desc.type & pipeline_stage::pixel_shader) == pipeline_stage::pixel_shader)
		dump_shader_code(device_type, pipeline_stage::pixel_shader, desc.graphics.pixel_shader);
	if ((desc.type & pipeline_stage::compute_shader) == pipeline_stage::compute_shader)
		dump_shader_code(device_type, pipeline_stage::compute_shader, desc.compute.shader);

	return false;
}

void register_addon_shadermod_dump()
{
	reshade::register_event<reshade::addon_event::create_pipeline>(on_create_pipeline);
}
void unregister_addon_shadermod_dump()
{
	reshade::unregister_event<reshade::addon_event::create_pipeline>(on_create_pipeline);
}

#ifdef _WINDLL

extern "C" __declspec(dllexport) const char *NAME = "ShaderMod Dump";
extern "C" __declspec(dllexport) const char *DESCRIPTION = "Example add-on that dumps all shaders used by the application to disk.";

BOOL APIENTRY DllMain(HMODULE hModule, DWORD fdwReason, LPVOID)
{
	switch (fdwReason)
	{
	case DLL_PROCESS_ATTACH:
		if (!reshade::register_addon(hModule))
			return FALSE;
		register_addon_shadermod_dump();
		break;
	case DLL_PROCESS_DETACH:
		unregister_addon_shadermod_dump();
		reshade::unregister_addon(hModule);
		break;
	}

	return TRUE;
}

#endif
