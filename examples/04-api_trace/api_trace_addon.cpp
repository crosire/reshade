/*
 * Copyright (C) 2021 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause OR MIT
 */

#include <reshade.hpp>
#include <cassert>
#include <sstream>
#include <shared_mutex>
#include <unordered_set>

using namespace reshade::api;

namespace
{
	bool s_do_capture = false;
	std::shared_mutex s_mutex;
	std::unordered_set<uint64_t> s_samplers;
	std::unordered_set<uint64_t> s_resources;
	std::unordered_set<uint64_t> s_resource_views;
	std::unordered_set<uint64_t> s_pipelines;

	inline auto to_string(shader_stage value)
	{
		switch (value)
		{
		case shader_stage::vertex:
			return "vertex";
		case shader_stage::hull:
			return "hull";
		case shader_stage::domain:
			return "domain";
		case shader_stage::geometry:
			return "geometry";
		case shader_stage::pixel:
			return "pixel";
		case shader_stage::compute:
			return "compute";
		case shader_stage::amplification:
			return "amplification";
		case shader_stage::mesh:
			return "mesh";
		case shader_stage::raygen:
			return "raygen";
		case shader_stage::any_hit:
			return "any_hit";
		case shader_stage::closest_hit:
			return "closest_hit";
		case shader_stage::miss:
			return "miss";
		case shader_stage::intersection:
			return "intersection";
		case shader_stage::callable:
			return "callable";
		case shader_stage::all:
			return "all";
		case shader_stage::all_graphics:
			return "all_graphics";
		case shader_stage::all_ray_tracing:
			return "all_raytracing";
		default:
			return "unknown";
		}
	}
	inline auto to_string(pipeline_stage value)
	{
		switch (value)
		{
		case pipeline_stage::vertex_shader:
			return "vertex_shader";
		case pipeline_stage::hull_shader:
			return "hull_shader";
		case pipeline_stage::domain_shader:
			return "domain_shader";
		case pipeline_stage::geometry_shader:
			return "geometry_shader";
		case pipeline_stage::pixel_shader:
			return "pixel_shader";
		case pipeline_stage::compute_shader:
			return "compute_shader";
		case pipeline_stage::amplification_shader:
			return "amplification_shader";
		case pipeline_stage::mesh_shader:
			return "mesh_shader";
		case pipeline_stage::input_assembler:
			return "input_assembler";
		case pipeline_stage::stream_output:
			return "stream_output";
		case pipeline_stage::rasterizer:
			return "rasterizer";
		case pipeline_stage::depth_stencil:
			return "depth_stencil";
		case pipeline_stage::output_merger:
			return "output_merger";
		case pipeline_stage::all:
			return "all";
		case pipeline_stage::all_graphics:
			return "all_graphics";
		case pipeline_stage::all_ray_tracing:
			return "all_ray_tracing";
		case pipeline_stage::all_shader_stages:
			return "all_shader_stages";
		default:
			return "unknown";
		}
	}
	inline auto to_string(descriptor_type value)
	{
		switch (value)
		{
		case descriptor_type::sampler:
			return "sampler";
		case descriptor_type::sampler_with_resource_view:
			return "sampler_with_resource_view";
		case descriptor_type::shader_resource_view:
			return "shader_resource_view";
		case descriptor_type::unordered_access_view:
			return "unordered_access_view";
		case descriptor_type::constant_buffer:
			return "constant_buffer";
		case descriptor_type::acceleration_structure:
			return "acceleration_structure";
		default:
			return "unknown";
		}
	}
	inline auto to_string(dynamic_state value)
	{
		switch (value)
		{
		default:
		case dynamic_state::unknown:
			return "unknown";
		case dynamic_state::alpha_test_enable:
			return "alpha_test_enable";
		case dynamic_state::alpha_reference_value:
			return "alpha_reference_value";
		case dynamic_state::alpha_func:
			return "alpha_func";
		case dynamic_state::srgb_write_enable:
			return "srgb_write_enable";
		case dynamic_state::primitive_topology:
			return "primitive_topology";
		case dynamic_state::sample_mask:
			return "sample_mask";
		case dynamic_state::alpha_to_coverage_enable:
			return "alpha_to_coverage_enable";
		case dynamic_state::blend_enable:
			return "blend_enable";
		case dynamic_state::logic_op_enable:
			return "logic_op_enable";
		case dynamic_state::color_blend_op:
			return "color_blend_op";
		case dynamic_state::source_color_blend_factor:
			return "src_color_blend_factor";
		case dynamic_state::dest_color_blend_factor:
			return "dst_color_blend_factor";
		case dynamic_state::alpha_blend_op:
			return "alpha_blend_op";
		case dynamic_state::source_alpha_blend_factor:
			return "src_alpha_blend_factor";
		case dynamic_state::dest_alpha_blend_factor:
			return "dst_alpha_blend_factor";
		case dynamic_state::logic_op:
			return "logic_op";
		case dynamic_state::blend_constant:
			return "blend_constant";
		case dynamic_state::render_target_write_mask:
			return "render_target_write_mask";
		case dynamic_state::fill_mode:
			return "fill_mode";
		case dynamic_state::cull_mode:
			return "cull_mode";
		case dynamic_state::front_counter_clockwise:
			return "front_counter_clockwise";
		case dynamic_state::depth_bias:
			return "depth_bias";
		case dynamic_state::depth_bias_clamp:
			return "depth_bias_clamp";
		case dynamic_state::depth_bias_slope_scaled:
			return "depth_bias_slope_scaled";
		case dynamic_state::depth_clip_enable:
			return "depth_clip_enable";
		case dynamic_state::scissor_enable:
			return "scissor_enable";
		case dynamic_state::multisample_enable:
			return "multisample_enable";
		case dynamic_state::antialiased_line_enable:
			return "antialiased_line_enable";
		case dynamic_state::depth_enable:
			return "depth_enable";
		case dynamic_state::depth_write_mask:
			return "depth_write_mask";
		case dynamic_state::depth_func:
			return "depth_func";
		case dynamic_state::stencil_enable:
			return "stencil_enable";
		case dynamic_state::front_stencil_read_mask:
			return "front_stencil_read_mask";
		case dynamic_state::front_stencil_write_mask:
			return "front_stencil_write_mask";
		case dynamic_state::front_stencil_reference_value:
			return "front_stencil_reference_value";
		case dynamic_state::front_stencil_func:
			return "front_stencil_func";
		case dynamic_state::front_stencil_pass_op:
			return "front_stencil_pass_op";
		case dynamic_state::front_stencil_fail_op:
			return "front_stencil_fail_op";
		case dynamic_state::front_stencil_depth_fail_op:
			return "front_stencil_depth_fail_op";
		case dynamic_state::back_stencil_read_mask:
			return "back_stencil_read_mask";
		case dynamic_state::back_stencil_write_mask:
			return "back_stencil_write_mask";
		case dynamic_state::back_stencil_reference_value:
			return "back_stencil_reference_value";
		case dynamic_state::back_stencil_func:
			return "back_stencil_func";
		case dynamic_state::back_stencil_pass_op:
			return "back_stencil_pass_op";
		case dynamic_state::back_stencil_fail_op:
			return "back_stencil_fail_op";
		case dynamic_state::back_stencil_depth_fail_op:
			return "back_stencil_depth_fail_op";
		}
	}
	inline auto to_string(resource_usage value)
	{
		switch (value)
		{
		default:
		case resource_usage::undefined:
			return "undefined";
		case resource_usage::index_buffer:
			return "index_buffer";
		case resource_usage::vertex_buffer:
			return "vertex_buffer";
		case resource_usage::constant_buffer:
			return "constant_buffer";
		case resource_usage::stream_output:
			return "stream_output";
		case resource_usage::indirect_argument:
			return "indirect_argument";
		case resource_usage::depth_stencil:
		case resource_usage::depth_stencil_read:
		case resource_usage::depth_stencil_write:
			return "depth_stencil";
		case resource_usage::render_target:
			return "render_target";
		case resource_usage::shader_resource:
		case resource_usage::shader_resource_pixel:
		case resource_usage::shader_resource_non_pixel:
			return "shader_resource";
		case resource_usage::unordered_access:
			return "unordered_access";
		case resource_usage::copy_dest:
			return "copy_dest";
		case resource_usage::copy_source:
			return "copy_source";
		case resource_usage::resolve_dest:
			return "resolve_dest";
		case resource_usage::resolve_source:
			return "resolve_source";
		case resource_usage::acceleration_structure:
			return "acceleration_structure";
		case resource_usage::general:
			return "general";
		case resource_usage::present:
			return "present";
		case resource_usage::cpu_access:
			return "cpu_access";
		}
	}
	inline auto to_string(query_type value)
	{
		switch (value)
		{
		case query_type::occlusion:
			return "occlusion";
		case query_type::binary_occlusion:
			return "binary_occlusion";
		case query_type::timestamp:
			return "timestamp";
		case query_type::pipeline_statistics:
			return "pipeline_statistics";
		case query_type::stream_output_statistics_0:
			return "stream_output_statistics_0";
		case query_type::stream_output_statistics_1:
			return "stream_output_statistics_1";
		case query_type::stream_output_statistics_2:
			return "stream_output_statistics_2";
		case query_type::stream_output_statistics_3:
			return "stream_output_statistics_3";
		default:
			return "unknown";
		}
	}
	inline auto to_string(acceleration_structure_type value)
	{
		switch (value)
		{
		case acceleration_structure_type::top_level:
			return "top_level";
		case acceleration_structure_type::bottom_level:
			return "bottom_level";
		default:
		case acceleration_structure_type::generic:
			return "generic";
		}
	}
	inline auto to_string(acceleration_structure_copy_mode value)
	{
		switch (value)
		{
		case acceleration_structure_copy_mode::clone:
			return "clone";
		case acceleration_structure_copy_mode::compact:
			return "compact";
		case acceleration_structure_copy_mode::serialize:
			return "serialize";
		case acceleration_structure_copy_mode::deserialize:
			return "deserialize";
		default:
			return "unknown";
		}
	}
	inline auto to_string(acceleration_structure_build_mode value)
	{
		switch (value)
		{
		case acceleration_structure_build_mode::build:
			return "build";
		case acceleration_structure_build_mode::update:
			return "update";
		default:
			return "unknown";
		}
	}
}

static void on_init_swapchain(swapchain *swapchain, bool)
{
	const std::unique_lock<std::shared_mutex> lock(s_mutex);

	for (uint32_t i = 0; i < swapchain->get_back_buffer_count(); ++i)
	{
		const resource buffer = swapchain->get_back_buffer(i);

		s_resources.emplace(buffer.handle);
	}
}
static void on_destroy_swapchain(swapchain *swapchain, bool)
{
	const std::unique_lock<std::shared_mutex> lock(s_mutex);

	for (uint32_t i = 0; i < swapchain->get_back_buffer_count(); ++i)
	{
		const resource buffer = swapchain->get_back_buffer(i);

		s_resources.erase(buffer.handle);
	}
}
static void on_init_sampler(device *device, const sampler_desc &desc, sampler handle)
{
	const std::unique_lock<std::shared_mutex> lock(s_mutex);

	s_samplers.emplace(handle.handle);
}
static void on_destroy_sampler(device *device, sampler handle)
{
	const std::unique_lock<std::shared_mutex> lock(s_mutex);

	assert(s_samplers.find(handle.handle) != s_samplers.end());
	s_samplers.erase(handle.handle);
}
static void on_init_resource(device *device, const resource_desc &desc, const subresource_data *, resource_usage, resource handle)
{
	const std::unique_lock<std::shared_mutex> lock(s_mutex);

	s_resources.emplace(handle.handle);
}
static void on_destroy_resource(device *device, resource handle)
{
	const std::unique_lock<std::shared_mutex> lock(s_mutex);

	assert(s_resources.find(handle.handle) != s_resources.end());
	s_resources.erase(handle.handle);
}
static void on_init_resource_view(device *device, resource resource, resource_usage usage_type, const resource_view_desc &desc, resource_view handle)
{
	const std::unique_lock<std::shared_mutex> lock(s_mutex);

	assert(resource == 0 || s_resources.find(resource.handle) != s_resources.end());
	s_resource_views.emplace(handle.handle);
}
static void on_destroy_resource_view(device *device, resource_view handle)
{
	const std::unique_lock<std::shared_mutex> lock(s_mutex);

	assert(s_resource_views.find(handle.handle) != s_resource_views.end());
	s_resource_views.erase(handle.handle);
}
static void on_init_pipeline(device *device, pipeline_layout, uint32_t, const pipeline_subobject *, pipeline handle)
{
	const std::unique_lock<std::shared_mutex> lock(s_mutex);

	s_pipelines.emplace(handle.handle);
}
static void on_destroy_pipeline(device *device, pipeline handle)
{
	const std::unique_lock<std::shared_mutex> lock(s_mutex);

	assert(s_pipelines.find(handle.handle) != s_pipelines.end());
	s_pipelines.erase(handle.handle);
}

static void on_barrier(command_list *, uint32_t num_resources, const resource *resources, const resource_usage *old_states, const resource_usage *new_states)
{
	if (!s_do_capture)
		return;

#ifndef NDEBUG
	{	const std::shared_lock<std::shared_mutex> lock(s_mutex);

		for (uint32_t i = 0; i < num_resources; ++i)
			assert(resources[i] == 0 || s_resources.find(resources[i].handle) != s_resources.end());
	}
#endif

	for (uint32_t i = 0; i < num_resources; ++i)
	{
		std::stringstream s;
		s << "barrier(" << (void *)resources[i].handle << ", " << to_string(old_states[i]) << ", " << to_string(new_states[i]) << ")";
		reshade::log::message(reshade::log::level::info, s.str().c_str());
	}
}

static void on_begin_render_pass(command_list *, uint32_t count, const render_pass_render_target_desc *rts, const render_pass_depth_stencil_desc *ds)
{
	if (!s_do_capture)
		return;

	std::stringstream s;
	s << "begin_render_pass(" << count << ", { ";
	for (uint32_t i = 0; i < count; ++i)
		s << (void *)rts[i].view.handle << ", ";
	s << " }, " << (ds != nullptr ? (void *)ds->view.handle : 0) << ")";

	reshade::log::message(reshade::log::level::info, s.str().c_str());
}
static void on_end_render_pass(command_list *)
{
	if (!s_do_capture)
		return;

	std::stringstream s;
	s << "end_render_pass()";

	reshade::log::message(reshade::log::level::info, s.str().c_str());
}
static void on_bind_render_targets_and_depth_stencil(command_list *, uint32_t count, const resource_view *rtvs, resource_view dsv)
{
	if (!s_do_capture)
		return;

#ifndef NDEBUG
	{	const std::shared_lock<std::shared_mutex> lock(s_mutex);

		for (uint32_t i = 0; i < count; ++i)
			assert(rtvs[i] == 0 || s_resource_views.find(rtvs[i].handle) != s_resource_views.end());
		assert(dsv == 0 || s_resource_views.find(dsv.handle) != s_resource_views.end());
	}
#endif

	std::stringstream s;
	s << "bind_render_targets_and_depth_stencil(" << count << ", { ";
	for (uint32_t i = 0; i < count; ++i)
		s << (void *)rtvs[i].handle << ", ";
	s << " }, " << (void *)dsv.handle << ")";

	reshade::log::message(reshade::log::level::info, s.str().c_str());
}

static void on_bind_pipeline(command_list *, pipeline_stage type, pipeline pipeline)
{
	if (!s_do_capture)
		return;

#ifndef NDEBUG
	{	const std::shared_lock<std::shared_mutex> lock(s_mutex);

		assert(pipeline.handle == 0 || s_pipelines.find(pipeline.handle) != s_pipelines.end());
	}
#endif

	std::stringstream s;
	s << "bind_pipeline(" << to_string(type) << ", " << (void *)pipeline.handle << ")";

	reshade::log::message(reshade::log::level::info, s.str().c_str());
}
static void on_bind_pipeline_states(command_list *, uint32_t count, const dynamic_state *states, const uint32_t *values)
{
	if (!s_do_capture)
		return;

	for (uint32_t i = 0; i < count; ++i)
	{
		std::stringstream s;
		s << "bind_pipeline_state(" << to_string(states[i]) << ", " << values[i] << ")";
		reshade::log::message(reshade::log::level::info, s.str().c_str());
	}
}
static void on_bind_viewports(command_list *, uint32_t first, uint32_t count, const viewport *viewports)
{
	if (!s_do_capture)
		return;

	std::stringstream s;
	s << "bind_viewports(" << first << ", " << count << ", { ... })";

	reshade::log::message(reshade::log::level::info, s.str().c_str());
}
static void on_bind_scissor_rects(command_list *, uint32_t first, uint32_t count, const rect *rects)
{
	if (!s_do_capture)
		return;

	std::stringstream s;
	s << "bind_scissor_rects(" << first << ", " << count << ", { ... })";

	reshade::log::message(reshade::log::level::info, s.str().c_str());
}
static void on_push_constants(command_list *, shader_stage stages, pipeline_layout layout, uint32_t param_index, uint32_t first, uint32_t count, const void *values)
{
	if (!s_do_capture)
		return;

	std::stringstream s;
	s << "push_constants(" << to_string(stages) << ", " << (void *)layout.handle << ", " << param_index << ", " << first << ", " << count << ", { ";
	for (uint32_t i = 0; i < count; ++i)
		s << std::hex << static_cast<const uint32_t *>(values)[i] << std::dec << ", ";
	s << " })";

	reshade::log::message(reshade::log::level::info, s.str().c_str());
}
static void on_push_descriptors(command_list *, shader_stage stages, pipeline_layout layout, uint32_t param_index, const descriptor_table_update &update)
{
	if (!s_do_capture)
		return;

#ifndef NDEBUG
	{	const std::shared_lock<std::shared_mutex> lock(s_mutex);

		switch (update.type)
		{
		case descriptor_type::sampler:
			for (uint32_t i = 0; i < update.count; ++i)
				assert(static_cast<const sampler *>(update.descriptors)[i].handle == 0 || s_samplers.find(static_cast<const sampler *>(update.descriptors)[i].handle) != s_samplers.end());
			break;
		case descriptor_type::sampler_with_resource_view:
			for (uint32_t i = 0; i < update.count; ++i)
				assert(static_cast<const sampler_with_resource_view *>(update.descriptors)[i].view.handle == 0 || s_resource_views.find(static_cast<const sampler_with_resource_view *>(update.descriptors)[i].view.handle) != s_resource_views.end());
			break;
		case descriptor_type::shader_resource_view:
		case descriptor_type::unordered_access_view:
		case descriptor_type::acceleration_structure:
			for (uint32_t i = 0; i < update.count; ++i)
				assert(static_cast<const resource_view *>(update.descriptors)[i].handle == 0 || s_resource_views.find(static_cast<const resource_view *>(update.descriptors)[i].handle) != s_resource_views.end());
			break;
		case descriptor_type::constant_buffer:
			for (uint32_t i = 0; i < update.count; ++i)
				assert(static_cast<const buffer_range *>(update.descriptors)[i].buffer.handle == 0 || s_resources.find(static_cast<const buffer_range *>(update.descriptors)[i].buffer.handle) != s_resources.end());
			break;
		default:
			break;
		}
	}
#endif

	std::stringstream s;
	s << "push_descriptors(" << to_string(stages) << ", " << (void *)layout.handle << ", " << param_index << ", { " << to_string(update.type) << ", " << update.binding << ", " << update.count << " })";

	reshade::log::message(reshade::log::level::info, s.str().c_str());
}
static void on_bind_descriptor_tables(command_list *, shader_stage stages, pipeline_layout layout, uint32_t first, uint32_t count, const descriptor_table *tables)
{
	if (!s_do_capture)
		return;

	for (uint32_t i = 0; i < count; ++i)
	{
		std::stringstream s;
		s << "bind_descriptor_table(" << to_string(stages) << ", " << (void *)layout.handle << ", " << (first + i) << ", " << (void *)tables[i].handle << ")";
		reshade::log::message(reshade::log::level::info, s.str().c_str());
	}
}
static void on_bind_index_buffer(command_list *, resource buffer, uint64_t offset, uint32_t index_size)
{
	if (!s_do_capture)
		return;

#ifndef NDEBUG
	{	const std::shared_lock<std::shared_mutex> lock(s_mutex);

		assert(buffer.handle == 0 || s_resources.find(buffer.handle) != s_resources.end());
	}
#endif

	std::stringstream s;
	s << "bind_index_buffer(" << (void *)buffer.handle << ", " << offset << ", " << index_size << ")";

	reshade::log::message(reshade::log::level::info, s.str().c_str());
}
static void on_bind_vertex_buffers(command_list *, uint32_t first, uint32_t count, const resource *buffers, const uint64_t *offsets, const uint32_t *strides)
{
	if (!s_do_capture)
		return;

#ifndef NDEBUG
	{	const std::shared_lock<std::shared_mutex> lock(s_mutex);

		for (uint32_t i = 0; i < count; ++i)
			assert(buffers[i].handle == 0 || s_resources.find(buffers[i].handle) != s_resources.end());
	}
#endif

	for (uint32_t i = 0; i < count; ++i)
	{
		std::stringstream s;
		s << "bind_vertex_buffer(" << (first + i) << ", " << (void *)buffers[i].handle << ", " << (offsets != nullptr ? offsets[i] : 0) << ", " << (strides != nullptr ? strides[i] : 0) << ")";
		reshade::log::message(reshade::log::level::info, s.str().c_str());
	}
}

static bool on_draw(command_list *, uint32_t vertices, uint32_t instances, uint32_t first_vertex, uint32_t first_instance)
{
	if (!s_do_capture)
		return false;

	std::stringstream s;
	s << "draw(" << vertices << ", " << instances << ", " << first_vertex << ", " << first_instance << ")";

	reshade::log::message(reshade::log::level::info, s.str().c_str());

	return false;
}
static bool on_draw_indexed(command_list *, uint32_t indices, uint32_t instances, uint32_t first_index, int32_t vertex_offset, uint32_t first_instance)
{
	if (!s_do_capture)
		return false;

	std::stringstream s;
	s << "draw_indexed(" << indices << ", " << instances << ", " << first_index << ", " << vertex_offset << ", " << first_instance << ")";

	reshade::log::message(reshade::log::level::info, s.str().c_str());

	return false;
}
static bool on_dispatch(command_list *, uint32_t group_count_x, uint32_t group_count_y, uint32_t group_count_z)
{
	if (!s_do_capture)
		return false;

	std::stringstream s;
	s << "dispatch(" << group_count_x << ", " << group_count_y << ", " << group_count_z << ")";

	reshade::log::message(reshade::log::level::info, s.str().c_str());

	return false;
}
static bool on_dispatch_mesh(command_list *, uint32_t group_count_x, uint32_t group_count_y, uint32_t group_count_z)
{
	if (!s_do_capture)
		return false;

	std::stringstream s;
	s << "dispatch_mesh(" << group_count_x << ", " << group_count_y << ", " << group_count_z << ")";

	reshade::log::message(reshade::log::level::info, s.str().c_str());

	return false;
}
static bool on_dispatch_rays(command_list *, resource raygen, uint64_t raygen_offset, uint64_t raygen_size, resource miss, uint64_t miss_offset, uint64_t miss_size, uint64_t miss_stride, resource hit_group, uint64_t hit_group_offset, uint64_t hit_group_size, uint64_t hit_group_stride, resource callable, uint64_t callable_offset, uint64_t callable_size, uint64_t callable_stride, uint32_t width, uint32_t height, uint32_t depth)
{
	if (!s_do_capture)
		return false;

	std::stringstream s;
	s << "dispatch_rays(" << (void *)raygen.handle << ", " << raygen_offset << ", " << raygen_size << ", " << (void *)miss.handle << ", " << miss_offset << ", " << miss_size << ", " << miss_stride << (void *)hit_group.handle << ", " << hit_group_offset << ", " << hit_group_size << ", " << hit_group_stride << ", " << (void *)callable.handle << ", " << callable_offset << ", " << callable_size << ", " << callable_stride << ", " << width << ", " << height << ", " << depth << ")";

	reshade::log::message(reshade::log::level::info, s.str().c_str());

	return false;
}
static bool on_draw_or_dispatch_indirect(command_list *, indirect_command type, resource buffer, uint64_t offset, uint32_t draw_count, uint32_t stride)
{
	if (!s_do_capture)
		return false;

	std::stringstream s;
	switch (type)
	{
	case indirect_command::unknown:
		s << "draw_or_dispatch_indirect(" << (void *)buffer.handle << ", " << offset << ", " << draw_count << ", " << stride << ")";
		break;
	case indirect_command::draw:
		s << "draw_indirect(" << (void *)buffer.handle << ", " << offset << ", " << draw_count << ", " << stride << ")";
		break;
	case indirect_command::draw_indexed:
		s << "draw_indexed_indirect(" << (void *)buffer.handle << ", " << offset << ", " << draw_count << ", " << stride << ")";
		break;
	case indirect_command::dispatch:
		s << "dispatch_indirect(" << (void *)buffer.handle << ", " << offset << ", " << draw_count << ", " << stride << ")";
		break;
	case indirect_command::dispatch_mesh:
		s << "dispatch_mesh_indirect(" << (void *)buffer.handle << ", " << offset << ", " << draw_count << ", " << stride << ")";
		break;
	case indirect_command::dispatch_rays:
		s << "dispatch_rays_indirect(" << (void *)buffer.handle << ", " << offset << ", " << draw_count << ", " << stride << ")";
		break;
	}

	reshade::log::message(reshade::log::level::info, s.str().c_str());

	return false;
}

static bool on_copy_resource(command_list *, resource src, resource dst)
{
	if (!s_do_capture)
		return false;

#ifndef NDEBUG
	{	const std::shared_lock<std::shared_mutex> lock(s_mutex);

		assert(s_resources.find(src.handle) != s_resources.end());
		assert(s_resources.find(dst.handle) != s_resources.end());
	}
#endif

	std::stringstream s;
	s << "copy_resource(" << (void *)src.handle << ", " << (void *)dst.handle << ")";

	reshade::log::message(reshade::log::level::info, s.str().c_str());

	return false;
}
static bool on_copy_buffer_region(command_list *, resource src, uint64_t src_offset, resource dst, uint64_t dst_offset, uint64_t size)
{
	if (!s_do_capture)
		return false;

#ifndef NDEBUG
	{	const std::shared_lock<std::shared_mutex> lock(s_mutex);

		assert(s_resources.find(src.handle) != s_resources.end());
		assert(s_resources.find(dst.handle) != s_resources.end());
	}
#endif

	std::stringstream s;
	s << "copy_buffer_region(" << (void *)src.handle << ", " << src_offset << ", " << (void *)dst.handle << ", " << dst_offset << ", " << size << ")";

	reshade::log::message(reshade::log::level::info, s.str().c_str());

	return false;
}
static bool on_copy_buffer_to_texture(command_list *, resource src, uint64_t src_offset, uint32_t row_length, uint32_t slice_height, resource dst, uint32_t dst_subresource, const subresource_box *)
{
	if (!s_do_capture)
		return false;

#ifndef NDEBUG
	{	const std::shared_lock<std::shared_mutex> lock(s_mutex);

		assert(s_resources.find(src.handle) != s_resources.end());
		assert(s_resources.find(dst.handle) != s_resources.end());
	}
#endif

	std::stringstream s;
	s << "copy_buffer_to_texture(" << (void *)src.handle << ", " << src_offset << ", " << row_length << ", " << slice_height << ", " << (void *)dst.handle << ", " << dst_subresource << ")";

	reshade::log::message(reshade::log::level::info, s.str().c_str());

	return false;
}
static bool on_copy_texture_region(command_list *, resource src, uint32_t src_subresource, const subresource_box *, resource dst, uint32_t dst_subresource, const subresource_box *, filter_mode filter)
{
	if (!s_do_capture)
		return false;

#ifndef NDEBUG
	{	const std::shared_lock<std::shared_mutex> lock(s_mutex);

		assert(s_resources.find(src.handle) != s_resources.end());
		assert(s_resources.find(dst.handle) != s_resources.end());
	}
#endif

	std::stringstream s;
	s << "copy_texture_region(" << (void *)src.handle << ", " << src_subresource << ", " << (void *)dst.handle << ", " << dst_subresource << ", " << (uint32_t)filter << ")";

	reshade::log::message(reshade::log::level::info, s.str().c_str());

	return false;
}
static bool on_copy_texture_to_buffer(command_list *, resource src, uint32_t src_subresource, const subresource_box *, resource dst, uint64_t dst_offset, uint32_t row_length, uint32_t slice_height)
{
	if (!s_do_capture)
		return false;

#ifndef NDEBUG
	{	const std::shared_lock<std::shared_mutex> lock(s_mutex);

		assert(s_resources.find(src.handle) != s_resources.end());
		assert(s_resources.find(dst.handle) != s_resources.end());
	}
#endif

	std::stringstream s;
	s << "copy_texture_to_buffer(" << (void *)src.handle << ", " << src_subresource << ", " << (void *)dst.handle << ", " << dst_offset << ", " << row_length << ", " << slice_height << ")";

	reshade::log::message(reshade::log::level::info, s.str().c_str());

	return false;
}
static bool on_resolve_texture_region(command_list *, resource src, uint32_t src_subresource, const subresource_box *, resource dst, uint32_t dst_subresource, uint32_t dst_x, uint32_t dst_y, uint32_t dst_z, format format)
{
	if (!s_do_capture)
		return false;

#ifndef NDEBUG
	{	const std::shared_lock<std::shared_mutex> lock(s_mutex);

		assert(s_resources.find(src.handle) != s_resources.end());
		assert(s_resources.find(dst.handle) != s_resources.end());
	}
#endif

	std::stringstream s;
	s << "resolve_texture_region(" << (void *)src.handle << ", " << src_subresource << ", { ... }, " << (void *)dst.handle << ", " << dst_subresource << ", " << dst_x << ", " << dst_y << ", " << dst_z << ", " << (uint32_t)format << ")";

	reshade::log::message(reshade::log::level::info, s.str().c_str());

	return false;
}

static bool on_clear_depth_stencil_view(command_list *, resource_view dsv, const float *depth, const uint8_t *stencil, uint32_t, const rect *)
{
	if (!s_do_capture)
		return false;

#ifndef NDEBUG
	{	const std::shared_lock<std::shared_mutex> lock(s_mutex);

		assert(s_resource_views.find(dsv.handle) != s_resource_views.end());
	}
#endif

	std::stringstream s;
	s << "clear_depth_stencil_view(" << (void *)dsv.handle << ", " << (depth != nullptr ? *depth : 0.0f) << ", " << (stencil != nullptr ? *stencil : 0) << ")";

	reshade::log::message(reshade::log::level::info, s.str().c_str());

	return false;
}
static bool on_clear_render_target_view(command_list *, resource_view rtv, const float color[4], uint32_t, const rect *)
{
	if (!s_do_capture)
		return false;

#ifndef NDEBUG
	{	const std::shared_lock<std::shared_mutex> lock(s_mutex);

		assert(s_resource_views.find(rtv.handle) != s_resource_views.end());
	}
#endif

	std::stringstream s;
	s << "clear_render_target_view(" << (void *)rtv.handle << ", { " << color[0] << ", " << color[1] << ", " << color[2] << ", " << color[3] << " })";

	reshade::log::message(reshade::log::level::info, s.str().c_str());

	return false;
}
static bool on_clear_unordered_access_view_uint(command_list *, resource_view uav, const uint32_t values[4], uint32_t, const rect *)
{
	if (!s_do_capture)
		return false;

#ifndef NDEBUG
	{	const std::shared_lock<std::shared_mutex> lock(s_mutex);

		assert(s_resource_views.find(uav.handle) != s_resource_views.end());
	}
#endif

	std::stringstream s;
	s << "clear_unordered_access_view_uint(" << (void *)uav.handle << ", { " << values[0] << ", " << values[1] << ", " << values[2] << ", " << values[3] << " })";

	reshade::log::message(reshade::log::level::info, s.str().c_str());

	return false;
}
static bool on_clear_unordered_access_view_float(command_list *, resource_view uav, const float values[4], uint32_t, const rect *)
{
	if (!s_do_capture)
		return false;

#ifndef NDEBUG
	{	const std::shared_lock<std::shared_mutex> lock(s_mutex);

		assert(s_resource_views.find(uav.handle) != s_resource_views.end());
	}
#endif

	std::stringstream s;
	s << "clear_unordered_access_view_float(" << (void *)uav.handle << ", { " << values[0] << ", " << values[1] << ", " << values[2] << ", " << values[3] << " })";

	reshade::log::message(reshade::log::level::info, s.str().c_str());

	return false;
}

static bool on_generate_mipmaps(command_list *, resource_view srv)
{
	if (!s_do_capture)
		return false;

#ifndef NDEBUG
	{	const std::shared_lock<std::shared_mutex> lock(s_mutex);

		assert(s_resource_views.find(srv.handle) != s_resource_views.end());
	}
#endif

	std::stringstream s;
	s << "generate_mipmaps(" << (void *)srv.handle << ")";

	reshade::log::message(reshade::log::level::info, s.str().c_str());

	return false;
}

static bool on_begin_query(command_list *cmd_list, query_heap heap, query_type type, uint32_t index)
{
	if (!s_do_capture)
		return false;

	std::stringstream s;
	s << "begin_query(" << (void *)heap.handle << ", " << to_string(type) << ", " << index << ")";

	reshade::log::message(reshade::log::level::info, s.str().c_str());

	return false;
}
static bool on_end_query(command_list *cmd_list, query_heap heap, query_type type, uint32_t index)
{
	if (!s_do_capture)
		return false;

	std::stringstream s;
	s << "end_query(" << (void *)heap.handle << ", " << to_string(type) << ", " << index << ")";

	reshade::log::message(reshade::log::level::info, s.str().c_str());

	return false;
}
static bool on_copy_query_heap_results(command_list *cmd_list, query_heap heap, query_type type, uint32_t first, uint32_t count, resource dest, uint64_t dest_offset, uint32_t stride)
{
	if (!s_do_capture)
		return false;

#ifndef NDEBUG
	{	const std::shared_lock<std::shared_mutex> lock(s_mutex);

		assert(s_resources.find(dest.handle) != s_resources.end());
	}
#endif

	std::stringstream s;
	s << "copy_query_heap_results(" << (void *)heap.handle << ", " << to_string(type) << ", " << first << ", " << count << (void *)dest.handle << ", " << dest_offset << ", " << stride << ")";

	reshade::log::message(reshade::log::level::info, s.str().c_str());

	return false;
}

static bool on_copy_acceleration_structure(command_list *, resource_view source, resource_view dest, acceleration_structure_copy_mode mode)
{
	if (!s_do_capture)
		return false;

#ifndef NDEBUG
	{	const std::shared_lock<std::shared_mutex> lock(s_mutex);

		assert(s_resource_views.find(source.handle) != s_resource_views.end() && s_resource_views.find(dest.handle) != s_resource_views.end());
	}
#endif

	std::stringstream s;
	s << "copy_acceleration_structure(" << (void *)source.handle << ", " << (void *)dest.handle << ", " << to_string(mode) << ")";

	reshade::log::message(reshade::log::level::info, s.str().c_str());

	return false;
}
static bool on_build_acceleration_structure(command_list *, acceleration_structure_type type, acceleration_structure_build_flags flags, uint32_t input_count, const acceleration_structure_build_input *inputs, resource scratch, uint64_t scratch_offset, resource_view source, resource_view dest, acceleration_structure_build_mode mode)
{
	if (!s_do_capture)
		return false;

#ifndef NDEBUG
	{	const std::shared_lock<std::shared_mutex> lock(s_mutex);

		assert((source.handle == 0 || s_resource_views.find(source.handle) != s_resource_views.end()) && s_resource_views.find(dest.handle) != s_resource_views.end());
	}
#endif

	std::stringstream s;
	s << "build_acceleration_structure(" << to_string(type) << ", " << std::hex << static_cast<uint32_t>(flags) << std::dec << ", " << input_count << ", { ... }, " << (void *)scratch.handle << ", " << scratch_offset << ", " << (void *)source.handle << ", " << (void *)dest.handle << ", " << to_string(mode) << ")";

	reshade::log::message(reshade::log::level::info, s.str().c_str());

	return false;
}
static bool on_query_acceleration_structures(command_list *, uint32_t count, const resource_view *acceleration_structures, query_heap heap, query_type type, uint32_t first)
{
	if (!s_do_capture)
		return false;

#ifndef NDEBUG
	{
		const std::shared_lock<std::shared_mutex> lock(s_mutex);

		for (uint32_t i = 0; i < count; ++i)
			assert(s_resource_views.find(acceleration_structures[i].handle) != s_resource_views.end());
	}
#endif

	std::stringstream s;
	s << "query_acceleration_structures(" << count << ", " << count << ", { ... }, " << (void *)heap.handle << ", " << to_string(type) << ", " << first << ")";

	reshade::log::message(reshade::log::level::info, s.str().c_str());

	return false;
}

static void on_present(effect_runtime *runtime)
{
	if (s_do_capture)
	{
		reshade::log::message(reshade::log::level::info, "present()");
		reshade::log::message(reshade::log::level::info, "--- End Frame ---");
		s_do_capture = false;
	}
	else
	{
		// The keyboard shortcut to trigger logging
		if (runtime->is_key_pressed(VK_F10))
		{
			s_do_capture = true;
			reshade::log::message(reshade::log::level::info, "--- Frame ---");
		}
	}
}

extern "C" __declspec(dllexport) const char *NAME = "API Trace";
extern "C" __declspec(dllexport) const char *DESCRIPTION = "Example add-on that logs the graphics API calls done by the application of the next frame after pressing a keyboard shortcut.";

BOOL APIENTRY DllMain(HMODULE hModule, DWORD fdwReason, LPVOID)
{
	switch (fdwReason)
	{
	case DLL_PROCESS_ATTACH:
		if (!reshade::register_addon(hModule))
			return FALSE;

		reshade::register_event<reshade::addon_event::init_swapchain>(on_init_swapchain);
		reshade::register_event<reshade::addon_event::destroy_swapchain>(on_destroy_swapchain);
		reshade::register_event<reshade::addon_event::init_sampler>(on_init_sampler);
		reshade::register_event<reshade::addon_event::destroy_sampler>(on_destroy_sampler);
		reshade::register_event<reshade::addon_event::init_resource>(on_init_resource);
		reshade::register_event<reshade::addon_event::destroy_resource>(on_destroy_resource);
		reshade::register_event<reshade::addon_event::init_resource_view>(on_init_resource_view);
		reshade::register_event<reshade::addon_event::destroy_resource_view>(on_destroy_resource_view);
		reshade::register_event<reshade::addon_event::init_pipeline>(on_init_pipeline);
		reshade::register_event<reshade::addon_event::destroy_pipeline>(on_destroy_pipeline);

		reshade::register_event<reshade::addon_event::barrier>(on_barrier);
		reshade::register_event<reshade::addon_event::begin_render_pass>(on_begin_render_pass);
		reshade::register_event<reshade::addon_event::end_render_pass>(on_end_render_pass);
		reshade::register_event<reshade::addon_event::bind_render_targets_and_depth_stencil>(on_bind_render_targets_and_depth_stencil);
		reshade::register_event<reshade::addon_event::bind_pipeline>(on_bind_pipeline);
		reshade::register_event<reshade::addon_event::bind_pipeline_states>(on_bind_pipeline_states);
		reshade::register_event<reshade::addon_event::bind_viewports>(on_bind_viewports);
		reshade::register_event<reshade::addon_event::bind_scissor_rects>(on_bind_scissor_rects);
		reshade::register_event<reshade::addon_event::push_constants>(on_push_constants);
		reshade::register_event<reshade::addon_event::push_descriptors>(on_push_descriptors);
		reshade::register_event<reshade::addon_event::bind_descriptor_tables>(on_bind_descriptor_tables);
		reshade::register_event<reshade::addon_event::bind_index_buffer>(on_bind_index_buffer);
		reshade::register_event<reshade::addon_event::bind_vertex_buffers>(on_bind_vertex_buffers);
		reshade::register_event<reshade::addon_event::draw>(on_draw);
		reshade::register_event<reshade::addon_event::draw_indexed>(on_draw_indexed);
		reshade::register_event<reshade::addon_event::dispatch>(on_dispatch);
		reshade::register_event<reshade::addon_event::dispatch_mesh>(on_dispatch_mesh);
		reshade::register_event<reshade::addon_event::dispatch_rays>(on_dispatch_rays);
		reshade::register_event<reshade::addon_event::draw_or_dispatch_indirect>(on_draw_or_dispatch_indirect);
		reshade::register_event<reshade::addon_event::copy_resource>(on_copy_resource);
		reshade::register_event<reshade::addon_event::copy_buffer_region>(on_copy_buffer_region);
		reshade::register_event<reshade::addon_event::copy_buffer_to_texture>(on_copy_buffer_to_texture);
		reshade::register_event<reshade::addon_event::copy_texture_region>(on_copy_texture_region);
		reshade::register_event<reshade::addon_event::copy_texture_to_buffer>(on_copy_texture_to_buffer);
		reshade::register_event<reshade::addon_event::resolve_texture_region>(on_resolve_texture_region);
		reshade::register_event<reshade::addon_event::clear_depth_stencil_view>(on_clear_depth_stencil_view);
		reshade::register_event<reshade::addon_event::clear_render_target_view>(on_clear_render_target_view);
		reshade::register_event<reshade::addon_event::clear_unordered_access_view_uint>(on_clear_unordered_access_view_uint);
		reshade::register_event<reshade::addon_event::clear_unordered_access_view_float>(on_clear_unordered_access_view_float);
		reshade::register_event<reshade::addon_event::generate_mipmaps>(on_generate_mipmaps);
		reshade::register_event<reshade::addon_event::begin_query>(on_begin_query);
		reshade::register_event<reshade::addon_event::end_query>(on_end_query);
		reshade::register_event<reshade::addon_event::copy_query_heap_results>(on_copy_query_heap_results);
		reshade::register_event<reshade::addon_event::copy_acceleration_structure>(on_copy_acceleration_structure);
		reshade::register_event<reshade::addon_event::build_acceleration_structure>(on_build_acceleration_structure);
		reshade::register_event<reshade::addon_event::query_acceleration_structures>(on_query_acceleration_structures);

		reshade::register_event<reshade::addon_event::reshade_present>(on_present);
		break;
	case DLL_PROCESS_DETACH:
		reshade::unregister_addon(hModule);
		break;
	}

	return TRUE;
}
