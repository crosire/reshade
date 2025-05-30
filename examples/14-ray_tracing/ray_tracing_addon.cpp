/*
 * Copyright (C) 2023 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause OR MIT
 */

#include <reshade.hpp>
#include <vector>
#include <fstream>
#include <algorithm>
#include <filesystem>

using namespace reshade::api;

static std::filesystem::path s_addon_path;

struct __declspec(uuid("CF2A5A7D-FF11-434F-AA7B-811A2935A8FE")) runtime_data
{
	pipeline_layout layout = {};
	pipeline pipeline = {};
	resource shader_binding_table = {};
	uint32_t shader_binding_table_alignment = 0;
	uint32_t shader_binding_table_handle_size = 0;
	uint32_t shader_binding_table_handle_alignment = 0;

	resource blas_resource = {};
	resource_view blas = {};
	resource tlas_resource = {};
	resource_view tlas = {};

	resource output_texture = {};
	resource_view output_texture_view = {};

	descriptor_table descriptor_table = {};

	void init(device *device, command_queue *queue)
	{
		if (!device->check_capability(device_caps::ray_tracing))
			return;

		if (init_pipeline(device))
			init_shader_binding_table(device);

		if (init_bottom_level_acceleration_structure(device, queue))
			init_top_level_acceleration_structure(device, queue);

		init_output_texture(device);
		init_descriptor_table(device);
	}
	void destroy(device *device)
	{
		device->free_descriptor_table(descriptor_table);

		device->destroy_resource_view(output_texture_view);
		device->destroy_resource(output_texture);

		device->destroy_resource_view(tlas);
		device->destroy_resource(tlas_resource);
		device->destroy_resource_view(blas);
		device->destroy_resource(blas_resource);

		device->destroy_resource(shader_binding_table);

		device->destroy_pipeline(pipeline);
		device->destroy_pipeline_layout(layout);
	}

	bool init_pipeline(device *device)
	{
		const descriptor_range ranges[] = {
			{ 0, 0, 0, 1, shader_stage::raygen, 1, descriptor_type::acceleration_structure },
			{ 1, 0, 0, 1, shader_stage::raygen, 1, descriptor_type::unordered_access_view },
		};

		const pipeline_layout_param params[] = {
			{ static_cast<uint32_t>(std::size(ranges)), ranges }
		};

		if (!device->create_pipeline_layout(static_cast<uint32_t>(std::size(params)), params, &layout))
			return false;

		std::ifstream shaders_file(s_addon_path / (device->get_api() == device_api::vulkan ? L"ray_tracing_shaders.spv" : L"ray_tracing_shaders.cso"), std::ios::binary);
		const std::vector<char> shaders_data { std::istreambuf_iterator<char>(shaders_file), std::istreambuf_iterator<char>() };

		shader_desc raygen;
		raygen.code = shaders_data.data();
		raygen.code_size = shaders_data.size();
		raygen.entry_point = "main_raygen";
		shader_desc miss;
		miss.code = shaders_data.data();
		miss.code_size = shaders_data.size();
		miss.entry_point = "main_miss";
		shader_desc closest_hit;
		closest_hit.code = shaders_data.data();
		closest_hit.code_size = shaders_data.size();
		closest_hit.entry_point = "main_closest_hit";

		const shader_group groups[] = {
			{ shader_group_type::raygen, 0 },
			{ shader_group_type::miss, 0 },
			{ shader_group_type::hit_group_triangles, 0 },
		};

		const uint32_t max_payload_size = 3 * sizeof(float);

		const pipeline_subobject subobjects[] = {
			{ pipeline_subobject_type::raygen_shader, 1, &raygen },
			{ pipeline_subobject_type::miss_shader, 1, &miss },
			{ pipeline_subobject_type::closest_hit_shader, 1, &closest_hit },
			{ pipeline_subobject_type::shader_groups, static_cast<uint32_t>(std::size(groups)), (void *)groups },
			{ pipeline_subobject_type::max_payload_size, 1, (void *)&max_payload_size}
		};

		return device->create_pipeline(layout, static_cast<uint32_t>(std::size(subobjects)), subobjects, &pipeline);
	}
	bool init_shader_binding_table(device *device)
	{
		device->get_property(device_properties::shader_group_handle_size, &shader_binding_table_handle_size);
		device->get_property(device_properties::shader_group_alignment, &shader_binding_table_alignment);
		device->get_property(device_properties::shader_group_handle_alignment, &shader_binding_table_handle_alignment);

		std::vector<uint8_t> sbt(3 * std::max(shader_binding_table_alignment, shader_binding_table_handle_size));
		device->get_pipeline_shader_group_handles(pipeline, 0, 1, sbt.data() + shader_binding_table_alignment * 0);
		device->get_pipeline_shader_group_handles(pipeline, 1, 1, sbt.data() + shader_binding_table_alignment * 1);
		device->get_pipeline_shader_group_handles(pipeline, 2, 1, sbt.data() + shader_binding_table_alignment * 2);

		const subresource_data sbt_data = { sbt.data(), static_cast<uint32_t>(sbt.size()) };

		return device->create_resource(resource_desc(sbt.size(), memory_heap::gpu_only, resource_usage::shader_resource_non_pixel), &sbt_data, resource_usage::shader_resource_non_pixel, &shader_binding_table);
	}
	bool init_output_texture(device *device)
	{
		if (!device->create_resource(resource_desc(800, 600, 1, 1, format::r8g8b8a8_unorm, 1, memory_heap::gpu_only, resource_usage::unordered_access | resource_usage::copy_source), nullptr, resource_usage::unordered_access, &output_texture) ||
			!device->create_resource_view(output_texture, resource_usage::unordered_access, resource_view_desc(format::r8g8b8a8_unorm), &output_texture_view))
			return false;
		return true;
	}
	bool init_descriptor_table(device *device)
	{
		if (!device->allocate_descriptor_table(layout, 0, &descriptor_table))
			return false;

		const descriptor_table_update updates[] = {
			{ descriptor_table, 0, 0, 1, descriptor_type::acceleration_structure, &tlas },
			{ descriptor_table, 1, 0, 1, descriptor_type::unordered_access_view, &output_texture_view },
		};

		device->update_descriptor_tables(static_cast<uint32_t>(std::size(updates)), updates);

		return true;
	}

	bool init_bottom_level_acceleration_structure(device *device, command_queue *queue)
	{
		const float vertices[][3] = {
			{  1.0f,  1.0f, 1.0f },
			{ -1.0f,  1.0f, 1.0f },
			{  0.0f, -1.0f, 1.0f },
		};
		const uint32_t indices[] = { 0, 1, 2 };

		const subresource_data vertices_data = { (void *)vertices, sizeof(vertices) };
		const subresource_data indices_data = { (void *)indices, sizeof(indices) };

		resource vertices_resource = {};
		if (!device->create_resource(resource_desc(sizeof(vertices), memory_heap::gpu_only, resource_usage::vertex_buffer | resource_usage::shader_resource_non_pixel), &vertices_data, resource_usage::shader_resource_non_pixel, &vertices_resource))
			return false;
		resource indices_resource = {};
		if (!device->create_resource(resource_desc(sizeof(indices), memory_heap::gpu_only, resource_usage::index_buffer | resource_usage::shader_resource_non_pixel), &indices_data, resource_usage::shader_resource_non_pixel, &indices_resource))
			return false;

		acceleration_structure_build_input geometry;
		geometry.type = acceleration_structure_build_input_type::triangles;
		geometry.flags = acceleration_structure_build_input_flags::opaque;
		geometry.triangles.vertex_buffer = vertices_resource;
		geometry.triangles.vertex_count = 3;
		geometry.triangles.vertex_stride = sizeof(float) * 3;
		geometry.triangles.vertex_format = format::r32g32b32_float;
		geometry.triangles.index_buffer = indices_resource;
		geometry.triangles.index_count = 3;
		geometry.triangles.index_format = format::r32_uint;

		uint64_t size = 0;
		uint64_t scratch_size = 0;
		device->get_acceleration_structure_size(acceleration_structure_type::bottom_level, acceleration_structure_build_flags::prefer_fast_trace, 1, &geometry, &size, &scratch_size, nullptr);

		resource scratch_resource = {};
		if (!device->create_resource(resource_desc(scratch_size, memory_heap::gpu_only, resource_usage::unordered_access), nullptr, resource_usage::unordered_access, &scratch_resource))
			return false;

		if (!device->create_resource(resource_desc(size, memory_heap::gpu_only, resource_usage::acceleration_structure), nullptr, resource_usage::acceleration_structure, &blas_resource) ||
			!device->create_resource_view(blas_resource, resource_usage::acceleration_structure, resource_view_desc(), &blas))
			return false;

		command_list *const cmd_list = queue->get_immediate_command_list();
		cmd_list->build_acceleration_structure(acceleration_structure_type::bottom_level, acceleration_structure_build_flags::prefer_fast_trace, 1, &geometry, scratch_resource, 0, {}, blas, acceleration_structure_build_mode::build);

		queue->wait_idle();

		device->destroy_resource(scratch_resource);
		device->destroy_resource(indices_resource);
		device->destroy_resource(vertices_resource);

		return true;
	}
	bool init_top_level_acceleration_structure(device *device, command_queue *queue)
	{
		const float transform[3 * 4] = {
			1.0f, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f
		};

		acceleration_structure_instance instance = {};
		std::memcpy(instance.transform, transform, sizeof(transform));
		instance.mask = 0xFF;
		instance.flags = 0x1;
		instance.acceleration_structure_gpu_address = device->get_resource_view_gpu_address(blas);

		const subresource_data instance_data = { (void *)&instance, sizeof(instance) };

		resource instance_resource;
		if (!device->create_resource(resource_desc(sizeof(instance), memory_heap::gpu_only, resource_usage::shader_resource_non_pixel), &instance_data, resource_usage::shader_resource_non_pixel, &instance_resource))
			return false;

		acceleration_structure_build_input geometry;
		geometry.type = acceleration_structure_build_input_type::instances;
		geometry.flags = acceleration_structure_build_input_flags::opaque;
		geometry.instances.buffer = instance_resource;
		geometry.instances.count = 1;
		geometry.instances.array_of_pointers = false;

		uint64_t size = 0, scratch_size = 0;
		device->get_acceleration_structure_size(acceleration_structure_type::top_level, acceleration_structure_build_flags::prefer_fast_trace, 1, &geometry, &size, &scratch_size, nullptr);

		resource scratch_resource = {};
		if (!device->create_resource(resource_desc(scratch_size, memory_heap::gpu_only, resource_usage::unordered_access), nullptr, resource_usage::unordered_access, &scratch_resource))
			return false;

		if (!device->create_resource(resource_desc(size, memory_heap::gpu_only, resource_usage::acceleration_structure), nullptr, resource_usage::acceleration_structure, &tlas_resource) ||
			!device->create_resource_view(tlas_resource, resource_usage::acceleration_structure, resource_view_desc(), &tlas))
			return false;

		command_list *const cmd_list = queue->get_immediate_command_list();
		cmd_list->build_acceleration_structure(acceleration_structure_type::top_level, acceleration_structure_build_flags::prefer_fast_trace, 1, &geometry, scratch_resource, 0, {}, tlas, acceleration_structure_build_mode::build);

		queue->wait_idle();

		device->destroy_resource(scratch_resource);
		device->destroy_resource(instance_resource);

		return true;
	}

	void frame(command_list *cmd_list, resource back_buffer)
	{
		if (pipeline == 0 || shader_binding_table == 0 || output_texture == 0 || descriptor_table == 0)
			return;

		cmd_list->bind_pipeline(pipeline_stage::all_ray_tracing, pipeline);
		cmd_list->bind_descriptor_table(shader_stage::all_ray_tracing, layout, 0, descriptor_table);

		cmd_list->dispatch_rays(
			shader_binding_table, shader_binding_table_alignment * 0, shader_binding_table_handle_size,
			shader_binding_table, shader_binding_table_alignment * 1, shader_binding_table_handle_size, shader_binding_table_handle_alignment,
			shader_binding_table, shader_binding_table_alignment * 2, shader_binding_table_handle_size, shader_binding_table_handle_alignment,
			{}, 0, 0, 0,
			800, 600, 1);

		cmd_list->barrier(output_texture, resource_usage::unordered_access, resource_usage::copy_source);
		cmd_list->barrier(back_buffer, resource_usage::present, resource_usage::copy_dest);
		cmd_list->copy_texture_region(output_texture, 0, nullptr, back_buffer, 0, nullptr);
		cmd_list->barrier(back_buffer, resource_usage::copy_dest, resource_usage::present);
		cmd_list->barrier(output_texture, resource_usage::copy_source, resource_usage::unordered_access);
	}
};

static void on_init(effect_runtime *runtime)
{
	const auto data = runtime->create_private_data<runtime_data>();
	data->init(runtime->get_device(), runtime->get_command_queue());
}
static void on_destroy(effect_runtime *runtime)
{
	const auto data = runtime->get_private_data<runtime_data>();
	data->destroy(runtime->get_device());

	runtime->destroy_private_data<runtime_data>();
}

static void on_present(effect_runtime *runtime)
{
	const auto data = runtime->get_private_data<runtime_data>();
	data->frame(runtime->get_command_queue()->get_immediate_command_list(), runtime->get_current_back_buffer());
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD fdwReason, LPVOID)
{
	switch (fdwReason)
	{
	case DLL_PROCESS_ATTACH:
		if (!reshade::register_addon(hModule))
			return FALSE;

		// Get add-on directory path
		{
			TCHAR module_path[MAX_PATH] = TEXT("");
			GetModuleFileName(hModule, module_path, ARRAYSIZE(module_path));
			s_addon_path = module_path;
			s_addon_path = s_addon_path.parent_path();
		}

		reshade::register_event<reshade::addon_event::init_effect_runtime>(on_init);
		reshade::register_event<reshade::addon_event::destroy_effect_runtime>(on_destroy);
		reshade::register_event<reshade::addon_event::reshade_present>(on_present);
		break;
	case DLL_PROCESS_DETACH:
		reshade::unregister_addon(hModule);
		break;
	}

	return TRUE;
}
