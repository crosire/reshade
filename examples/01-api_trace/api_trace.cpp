/*
 * Copyright (C) 2021 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause OR MIT
 */

#define ImTextureID unsigned long long

#include <imgui.h>
#include <reshade.hpp>
#include <mutex>
#include <sstream>
#include <unordered_set>
#include <cassert>

namespace
{
	bool s_do_capture = false;
	std::vector<std::string> s_capture_log;
	std::mutex s_mutex;

	std::unordered_set<uint64_t> s_samplers;
	std::unordered_set<uint64_t> s_resources;
	std::unordered_set<uint64_t> s_resource_views;
	std::unordered_set<uint64_t> s_pipelines;
}

static inline auto to_string(reshade::api::shader_stage value)
{
	switch (value)
	{
	case reshade::api::shader_stage::vertex:
		return "vertex";
	case reshade::api::shader_stage::hull:
		return "hull";
	case reshade::api::shader_stage::domain:
		return "domain";
	case reshade::api::shader_stage::geometry:
		return "geometry";
	case reshade::api::shader_stage::pixel:
		return "pixel";
	case reshade::api::shader_stage::compute:
		return "compute";
	case reshade::api::shader_stage::all:
		return "all";
	case reshade::api::shader_stage::all_graphics:
		return "all_graphics";
	default:
		return "unknown";
	}
}
static inline auto to_string(reshade::api::pipeline_stage value)
{
	switch (value)
	{
	case reshade::api::pipeline_stage::vertex_shader:
		return "vertex_shader";
	case reshade::api::pipeline_stage::hull_shader:
		return "hull_shader";
	case reshade::api::pipeline_stage::domain_shader:
		return "domain_shader";
	case reshade::api::pipeline_stage::geometry_shader:
		return "geometry_shader";
	case reshade::api::pipeline_stage::pixel_shader:
		return "pixel_shader";
	case reshade::api::pipeline_stage::compute_shader:
		return "compute_shader";
	case reshade::api::pipeline_stage::input_assembler:
		return "input_assembler";
	case reshade::api::pipeline_stage::stream_output:
		return "stream_output";
	case reshade::api::pipeline_stage::rasterizer:
		return "rasterizer";
	case reshade::api::pipeline_stage::depth_stencil:
		return "depth_stencil";
	case reshade::api::pipeline_stage::output_merger:
		return "output_merger";
	case reshade::api::pipeline_stage::all:
		return "all";
	case reshade::api::pipeline_stage::all_graphics:
		return "all_graphics";
	case reshade::api::pipeline_stage::all_shader_stages:
		return "all_shader_stages";
	default:
		return "unknown";
	}
}
static inline auto to_string(reshade::api::descriptor_type value)
{
	switch (value)
	{
	case reshade::api::descriptor_type::sampler:
		return "sampler";
	case reshade::api::descriptor_type::sampler_with_resource_view:
		return "sampler_with_resource_view";
	case reshade::api::descriptor_type::shader_resource_view:
		return "shader_resource_view";
	case reshade::api::descriptor_type::unordered_access_view:
		return "unordered_access_view";
	case reshade::api::descriptor_type::constant_buffer:
		return "constant_buffer";
	default:
		return "unknown";
	}
}
static inline auto to_string(reshade::api::dynamic_state value)
{
	switch (value)
	{
	default:
	case reshade::api::dynamic_state::unknown:
		return "unknown";
	case reshade::api::dynamic_state::alpha_test_enable:
		return "alpha_test_enable";
	case reshade::api::dynamic_state::alpha_reference_value:
		return "alpha_reference_value";
	case reshade::api::dynamic_state::alpha_func:
		return "alpha_func";
	case reshade::api::dynamic_state::srgb_write_enable:
		return "srgb_write_enable";
	case reshade::api::dynamic_state::primitive_topology:
		return "primitive_topology";
	case reshade::api::dynamic_state::sample_mask:
		return "sample_mask";
	case reshade::api::dynamic_state::alpha_to_coverage_enable:
		return "alpha_to_coverage_enable";
	case reshade::api::dynamic_state::blend_enable:
		return "blend_enable";
	case reshade::api::dynamic_state::logic_op_enable:
		return "logic_op_enable";
	case reshade::api::dynamic_state::color_blend_op:
		return "color_blend_op";
	case reshade::api::dynamic_state::source_color_blend_factor:
		return "src_color_blend_factor";
	case reshade::api::dynamic_state::dest_color_blend_factor:
		return "dst_color_blend_factor";
	case reshade::api::dynamic_state::alpha_blend_op:
		return "alpha_blend_op";
	case reshade::api::dynamic_state::source_alpha_blend_factor:
		return "src_alpha_blend_factor";
	case reshade::api::dynamic_state::dest_alpha_blend_factor:
		return "dst_alpha_blend_factor";
	case reshade::api::dynamic_state::logic_op:
		return "logic_op";
	case reshade::api::dynamic_state::blend_constant:
		return "blend_constant";
	case reshade::api::dynamic_state::render_target_write_mask:
		return "render_target_write_mask";
	case reshade::api::dynamic_state::fill_mode:
		return "fill_mode";
	case reshade::api::dynamic_state::cull_mode:
		return "cull_mode";
	case reshade::api::dynamic_state::front_counter_clockwise:
		return "front_counter_clockwise";
	case reshade::api::dynamic_state::depth_bias:
		return "depth_bias";
	case reshade::api::dynamic_state::depth_bias_clamp:
		return "depth_bias_clamp";
	case reshade::api::dynamic_state::depth_bias_slope_scaled:
		return "depth_bias_slope_scaled";
	case reshade::api::dynamic_state::depth_clip_enable:
		return "depth_clip_enable";
	case reshade::api::dynamic_state::scissor_enable:
		return "scissor_enable";
	case reshade::api::dynamic_state::multisample_enable:
		return "multisample_enable";
	case reshade::api::dynamic_state::antialiased_line_enable:
		return "antialiased_line_enable";
	case reshade::api::dynamic_state::depth_enable:
		return "depth_enable";
	case reshade::api::dynamic_state::depth_write_mask:
		return "depth_write_mask";
	case reshade::api::dynamic_state::depth_func:
		return "depth_func";
	case reshade::api::dynamic_state::stencil_enable:
		return "stencil_enable";
	case reshade::api::dynamic_state::stencil_read_mask:
		return "stencil_read_mask";
	case reshade::api::dynamic_state::stencil_write_mask:
		return "stencil_write_mask";
	case reshade::api::dynamic_state::stencil_reference_value:
		return "stencil_reference_value";
	case reshade::api::dynamic_state::front_stencil_func:
		return "front_stencil_func";
	case reshade::api::dynamic_state::front_stencil_pass_op:
		return "front_stencil_pass_op";
	case reshade::api::dynamic_state::front_stencil_fail_op:
		return "front_stencil_fail_op";
	case reshade::api::dynamic_state::front_stencil_depth_fail_op:
		return "front_stencil_depth_fail_op";
	case reshade::api::dynamic_state::back_stencil_func:
		return "back_stencil_func";
	case reshade::api::dynamic_state::back_stencil_pass_op:
		return "back_stencil_pass_op";
	case reshade::api::dynamic_state::back_stencil_fail_op:
		return "back_stencil_fail_op";
	case reshade::api::dynamic_state::back_stencil_depth_fail_op:
		return "back_stencil_depth_fail_op";
	}
}
static inline auto to_string(reshade::api::resource_usage value)
{
	switch (value)
	{
	default:
	case reshade::api::resource_usage::undefined:
		return "undefined";
	case reshade::api::resource_usage::index_buffer:
		return "index_buffer";
	case reshade::api::resource_usage::vertex_buffer:
		return "vertex_buffer";
	case reshade::api::resource_usage::constant_buffer:
		return "constant_buffer";
	case reshade::api::resource_usage::stream_output:
		return "stream_output";
	case reshade::api::resource_usage::indirect_argument:
		return "indirect_argument";
	case reshade::api::resource_usage::depth_stencil:
	case reshade::api::resource_usage::depth_stencil_read:
	case reshade::api::resource_usage::depth_stencil_write:
		return "depth_stencil";
	case reshade::api::resource_usage::render_target:
		return "render_target";
	case reshade::api::resource_usage::shader_resource:
	case reshade::api::resource_usage::shader_resource_pixel:
	case reshade::api::resource_usage::shader_resource_non_pixel:
		return "shader_resource";
	case reshade::api::resource_usage::unordered_access:
		return "unordered_access";
	case reshade::api::resource_usage::copy_dest:
		return "copy_dest";
	case reshade::api::resource_usage::copy_source:
		return "copy_source";
	case reshade::api::resource_usage::resolve_dest:
		return "resolve_dest";
	case reshade::api::resource_usage::resolve_source:
		return "resolve_source";
	case reshade::api::resource_usage::general:
		return "general";
	case reshade::api::resource_usage::present:
		return "present";
	case reshade::api::resource_usage::cpu_access:
		return "cpu_access";
	}
}

static void on_init_swapchain(reshade::api::swapchain *swapchain)
{
	const std::lock_guard<std::mutex> lock(s_mutex);

	reshade::api::device *const device = swapchain->get_device();

	for (uint32_t i = 0; i < swapchain->get_back_buffer_count(); ++i)
	{
		const reshade::api::resource buffer = swapchain->get_back_buffer(i);

		s_resources.emplace(buffer.handle);
		if (device->get_api() == reshade::api::device_api::d3d9 || device->get_api() == reshade::api::device_api::opengl)
			s_resource_views.emplace(buffer.handle);
	}
}
static void on_destroy_swapchain(reshade::api::swapchain *swapchain)
{
	const std::lock_guard<std::mutex> lock(s_mutex);

	reshade::api::device *const device = swapchain->get_device();

	for (uint32_t i = 0; i < swapchain->get_back_buffer_count(); ++i)
	{
		const reshade::api::resource buffer = swapchain->get_back_buffer(i);

		s_resources.erase(buffer.handle);
		if (device->get_api() == reshade::api::device_api::d3d9 || device->get_api() == reshade::api::device_api::opengl)
			s_resource_views.erase(buffer.handle);
	}
}
static void on_init_sampler(reshade::api::device *device, const reshade::api::sampler_desc &desc, reshade::api::sampler handle)
{
	const std::lock_guard<std::mutex> lock(s_mutex);

	s_samplers.emplace(handle.handle);
}
static void on_destroy_sampler(reshade::api::device *device, reshade::api::sampler handle)
{
	const std::lock_guard<std::mutex> lock(s_mutex);

	assert(s_samplers.find(handle.handle) != s_samplers.end());
	s_samplers.erase(handle.handle);
}
static void on_init_resource(reshade::api::device *device, const reshade::api::resource_desc &desc, const reshade::api::subresource_data *, reshade::api::resource_usage, reshade::api::resource handle)
{
	const std::lock_guard<std::mutex> lock(s_mutex);

	s_resources.emplace(handle.handle);
}
static void on_destroy_resource(reshade::api::device *device, reshade::api::resource handle)
{
	const std::lock_guard<std::mutex> lock(s_mutex);

	assert(s_resources.find(handle.handle) != s_resources.end());
	s_resources.erase(handle.handle);
}
static void on_init_resource_view(reshade::api::device *device, reshade::api::resource resource, reshade::api::resource_usage usage_type, const reshade::api::resource_view_desc &desc, reshade::api::resource_view handle)
{
	const std::lock_guard<std::mutex> lock(s_mutex);

	assert(resource == 0 || s_resources.find(resource.handle) != s_resources.end());
	s_resource_views.emplace(handle.handle);
}
static void on_destroy_resource_view(reshade::api::device *device, reshade::api::resource_view handle)
{
	const std::lock_guard<std::mutex> lock(s_mutex);

	assert(s_resource_views.find(handle.handle) != s_resource_views.end());
	s_resource_views.erase(handle.handle);
}
static void on_init_pipeline(reshade::api::device *device, reshade::api::pipeline_layout, uint32_t, const reshade::api::pipeline_subobject *, reshade::api::pipeline handle)
{
	const std::lock_guard<std::mutex> lock(s_mutex);

	s_pipelines.emplace(handle.handle);
}
static void on_destroy_pipeline(reshade::api::device *device, reshade::api::pipeline handle)
{
	const std::lock_guard<std::mutex> lock(s_mutex);

	assert(s_pipelines.find(handle.handle) != s_pipelines.end());
	s_pipelines.erase(handle.handle);
}

static void on_barrier(reshade::api::command_list *, uint32_t num_resources, const reshade::api::resource *resources, const reshade::api::resource_usage *old_states, const reshade::api::resource_usage *new_states)
{
	if (!s_do_capture)
		return;

	for (uint32_t i = 0; i < num_resources; ++i)
		assert(resources[i] == 0 || s_resources.find(resources[i].handle) != s_resources.end());

	std::stringstream s;
	for (uint32_t i = 0; i < num_resources; ++i)
		s << "barrier(" << (void *)resources[i].handle << ", " << to_string(old_states[i]) << ", " << to_string(new_states[i]) << ")" << std::endl;
	const std::lock_guard<std::mutex> lock(s_mutex); s_capture_log.push_back(s.str());
}

static void on_begin_render_pass(reshade::api::command_list *, uint32_t count, const reshade::api::render_pass_render_target_desc *rts, const reshade::api::render_pass_depth_stencil_desc *ds)
{
	if (!s_do_capture)
		return;

	std::stringstream s; s << "begin_render_pass(" << count << ", { ";
	for (uint32_t i = 0; i < count; ++i)
		s << (void *)rts[i].view.handle << ", ";
	s << " }, " << (ds != nullptr ? (void *)ds->view.handle : 0) << ")";
	const std::lock_guard<std::mutex> lock(s_mutex); s_capture_log.push_back(s.str());
}
static void on_end_render_pass(reshade::api::command_list *)
{
	if (!s_do_capture)
		return;

	std::stringstream s; s << "end_render_pass()";
	const std::lock_guard<std::mutex> lock(s_mutex); s_capture_log.push_back(s.str());
}
static void on_bind_render_targets_and_depth_stencil(reshade::api::command_list *, uint32_t count, const reshade::api::resource_view *rtvs, reshade::api::resource_view dsv)
{
	if (!s_do_capture)
		return;

	{	const std::lock_guard<std::mutex> lock(s_mutex);

		for (uint32_t i = 0; i < count; ++i)
			assert(rtvs[i] == 0 || s_resource_views.find(rtvs[i].handle) != s_resource_views.end());
		assert(dsv == 0 || s_resource_views.find(dsv.handle) != s_resource_views.end());
	}

	std::stringstream s; s << "bind_render_targets_and_depth_stencil(" << count << ", { ";
	for (uint32_t i = 0; i < count; ++i)
		s << (void *)rtvs[i].handle << ", ";
	s << " }, " << (void *)dsv.handle << ")";
	const std::lock_guard<std::mutex> lock(s_mutex); s_capture_log.push_back(s.str());
}

static void on_bind_pipeline(reshade::api::command_list *, reshade::api::pipeline_stage type, reshade::api::pipeline pipeline)
{
	if (!s_do_capture)
		return;

	{	const std::lock_guard<std::mutex> lock(s_mutex);

		assert(pipeline.handle == 0 || s_pipelines.find(pipeline.handle) != s_pipelines.end());
	}

	std::stringstream s; s << "bind_pipeline(" << to_string(type) << ", " << (void *)pipeline.handle << ")";
	const std::lock_guard<std::mutex> lock(s_mutex); s_capture_log.push_back(s.str());
}
static void on_bind_pipeline_states(reshade::api::command_list *, uint32_t count, const reshade::api::dynamic_state *states, const uint32_t *values)
{
	if (!s_do_capture)
		return;

	std::stringstream s;
	for (uint32_t i = 0; i < count; ++i)
		s << "bind_pipeline_state(" << to_string(states[i]) << ", " << values[i] << ")" << std::endl;
	const std::lock_guard<std::mutex> lock(s_mutex); s_capture_log.push_back(s.str());
}
static void on_bind_viewports(reshade::api::command_list *, uint32_t first, uint32_t count, const reshade::api::viewport *viewports)
{
	if (!s_do_capture)
		return;

	std::stringstream s; s << "bind_viewports(" << first << ", " << count << ", { ... })";
	const std::lock_guard<std::mutex> lock(s_mutex); s_capture_log.push_back(s.str());
}
static void on_bind_scissor_rects(reshade::api::command_list *, uint32_t first, uint32_t count, const reshade::api::rect *rects)
{
	if (!s_do_capture)
		return;

	std::stringstream s; s << "bind_scissor_rects(" << first << ", " << count << ", { ... })";
	const std::lock_guard<std::mutex> lock(s_mutex); s_capture_log.push_back(s.str());
}
static void on_push_constants(reshade::api::command_list *, reshade::api::shader_stage stages, reshade::api::pipeline_layout layout, uint32_t param_index, uint32_t first, uint32_t count, const uint32_t *values)
{
	if (!s_do_capture)
		return;

	std::stringstream s; s << "push_constants(" << to_string(stages) << ", " << (void *)layout.handle << ", " << param_index << ", " << first << ", " << count << ", { ";
	for (uint32_t i = 0; i < count; ++i)
		s << std::hex << values[i] << std::dec << ", ";
	s << " })";
	const std::lock_guard<std::mutex> lock(s_mutex); s_capture_log.push_back(s.str());
}
static void on_push_descriptors(reshade::api::command_list *, reshade::api::shader_stage stages, reshade::api::pipeline_layout layout, uint32_t param_index, const reshade::api::descriptor_set_update &update)
{
	if (!s_do_capture)
		return;

	{	const std::lock_guard<std::mutex> lock(s_mutex);

		switch (update.type)
		{
		case reshade::api::descriptor_type::sampler:
			for (uint32_t i = 0; i < update.count; ++i)
				assert(static_cast<const reshade::api::sampler *>(update.descriptors)[i].handle == 0 || s_samplers.find(static_cast<const reshade::api::sampler *>(update.descriptors)[i].handle) != s_samplers.end());
			break;
		case reshade::api::descriptor_type::sampler_with_resource_view:
			for (uint32_t i = 0; i < update.count; ++i)
				assert(static_cast<const reshade::api::sampler_with_resource_view *>(update.descriptors)[i].view.handle == 0 || s_resource_views.find(static_cast<const reshade::api::sampler_with_resource_view *>(update.descriptors)[i].view.handle) != s_resource_views.end());
			break;
		case reshade::api::descriptor_type::shader_resource_view:
		case reshade::api::descriptor_type::unordered_access_view:
			for (uint32_t i = 0; i < update.count; ++i)
				assert(static_cast<const reshade::api::resource_view *>(update.descriptors)[i].handle == 0 || s_resource_views.find(static_cast<const reshade::api::resource_view *>(update.descriptors)[i].handle) != s_resource_views.end());
			break;
		case reshade::api::descriptor_type::constant_buffer:
			for (uint32_t i = 0; i < update.count; ++i)
				assert(static_cast<const reshade::api::buffer_range *>(update.descriptors)[i].buffer.handle == 0 || s_resources.find(static_cast<const reshade::api::buffer_range *>(update.descriptors)[i].buffer.handle) != s_resources.end());
			break;
		default:
			break;
		}
	}

	std::stringstream s; s << "push_descriptors(" << to_string(stages) << ", " << (void *)layout.handle << ", " << param_index << ", { " << to_string(update.type) << ", " << update.binding << ", " << update.count << " })";
	const std::lock_guard<std::mutex> lock(s_mutex); s_capture_log.push_back(s.str());
}
static void on_bind_descriptor_sets(reshade::api::command_list *, reshade::api::shader_stage stages, reshade::api::pipeline_layout layout, uint32_t first, uint32_t count, const reshade::api::descriptor_set *sets)
{
	if (!s_do_capture)
		return;

	std::stringstream s;
	for (uint32_t i = 0; i < count; ++i)
		s << "bind_descriptor_set(" << to_string(stages) << ", " << (void *)layout.handle << ", " << (first + i) << ", " << (void *)sets[i].handle << ")" << std::endl;
	const std::lock_guard<std::mutex> lock(s_mutex); s_capture_log.push_back(s.str());
}
static void on_bind_index_buffer(reshade::api::command_list *, reshade::api::resource buffer, uint64_t offset, uint32_t index_size)
{
	if (!s_do_capture)
		return;

	{	const std::lock_guard<std::mutex> lock(s_mutex);

		assert(buffer.handle == 0 || s_resources.find(buffer.handle) != s_resources.end());
	}

	std::stringstream s; s << "bind_index_buffer(" << (void *)buffer.handle << ", " << offset << ", " << index_size << ")";
	const std::lock_guard<std::mutex> lock(s_mutex); s_capture_log.push_back(s.str());
}
static void on_bind_vertex_buffers(reshade::api::command_list *, uint32_t first, uint32_t count, const reshade::api::resource *buffers, const uint64_t *offsets, const uint32_t *strides)
{
	if (!s_do_capture)
		return;

	{	const std::lock_guard<std::mutex> lock(s_mutex);

		for (uint32_t i = 0; i < count; ++i)
			assert(buffers[i].handle == 0 || s_resources.find(buffers[i].handle) != s_resources.end());
	}

	std::stringstream s;
	for (uint32_t i = 0; i < count; ++i)
		s << "bind_vertex_buffer(" << (first + i) << ", " << (void *)buffers[i].handle << ", " << (offsets != nullptr ? offsets[i] : 0) << ", " << (strides != nullptr ? strides[i] : 0) << ")" << std::endl;
	const std::lock_guard<std::mutex> lock(s_mutex); s_capture_log.push_back(s.str());
}

static bool on_draw(reshade::api::command_list *, uint32_t vertices, uint32_t instances, uint32_t first_vertex, uint32_t first_instance)
{
	if (!s_do_capture)
		return false;

	std::stringstream s; s << "draw(" << vertices << ", " << instances << ", " << first_vertex << ", " << first_instance << ")";
	const std::lock_guard<std::mutex> lock(s_mutex); s_capture_log.push_back(s.str());

	return false;
}
static bool on_draw_indexed(reshade::api::command_list *, uint32_t indices, uint32_t instances, uint32_t first_index, int32_t vertex_offset, uint32_t first_instance)
{
	if (!s_do_capture)
		return false;

	std::stringstream s; s << "draw_indexed(" << indices << ", " << instances << ", " << first_index << ", " << vertex_offset << ", " << first_instance << ")";
	const std::lock_guard<std::mutex> lock(s_mutex); s_capture_log.push_back(s.str());

	return false;
}
static bool on_dispatch(reshade::api::command_list *, uint32_t group_count_x, uint32_t group_count_y, uint32_t group_count_z)
{
	if (!s_do_capture)
		return false;

	std::stringstream s; s << "dispatch(" << group_count_x << ", " << group_count_y << ", " << group_count_z << ")";
	const std::lock_guard<std::mutex> lock(s_mutex); s_capture_log.push_back(s.str());

	return false;
}
static bool on_draw_or_dispatch_indirect(reshade::api::command_list *, reshade::api::indirect_command type, reshade::api::resource buffer, uint64_t offset, uint32_t draw_count, uint32_t stride)
{
	if (!s_do_capture)
		return false;

	std::stringstream s;
	switch (type)
	{
	case reshade::api::indirect_command::unknown:
		s << "draw_or_dispatch_indirect(" << (void *)buffer.handle << ", " << offset << ", " << draw_count << ", " << stride << ")";
		break;
	case reshade::api::indirect_command::draw:
		s << "draw_indirect(" << (void *)buffer.handle << ", " << offset << ", " << draw_count << ", " << stride << ")";
		break;
	case reshade::api::indirect_command::draw_indexed:
		s << "draw_indexed_indirect(" << (void *)buffer.handle << ", " << offset << ", " << draw_count << ", " << stride << ")";
		break;
	case reshade::api::indirect_command::dispatch:
		s << "dispatch_indirect(" << (void *)buffer.handle << ", " << offset << ", " << draw_count << ", " << stride << ")";
		break;
	}

	const std::lock_guard<std::mutex> lock(s_mutex); s_capture_log.push_back(s.str());

	return false;
}

static bool on_copy_resource(reshade::api::command_list *, reshade::api::resource src, reshade::api::resource dst)
{
	if (!s_do_capture)
		return false;

	{	const std::lock_guard<std::mutex> lock(s_mutex);

		assert(s_resources.find(src.handle) != s_resources.end());
		assert(s_resources.find(dst.handle) != s_resources.end());
	}

	std::stringstream s; s << "copy_resource(" << (void *)src.handle << ", " << (void *)dst.handle << ")";
	const std::lock_guard<std::mutex> lock(s_mutex); s_capture_log.push_back(s.str());

	return false;
}
static bool on_copy_buffer_region(reshade::api::command_list *, reshade::api::resource src, uint64_t src_offset, reshade::api::resource dst, uint64_t dst_offset, uint64_t size)
{
	if (!s_do_capture)
		return false;

	{	const std::lock_guard<std::mutex> lock(s_mutex);

		assert(s_resources.find(src.handle) != s_resources.end());
		assert(s_resources.find(dst.handle) != s_resources.end());
	}

	std::stringstream s; s << "copy_buffer_region(" << (void *)src.handle << ", " << src_offset << ", " << (void *)dst.handle << ", " << dst_offset << ", " << size << ")";
	const std::lock_guard<std::mutex> lock(s_mutex); s_capture_log.push_back(s.str());

	return false;
}
static bool on_copy_buffer_to_texture(reshade::api::command_list *, reshade::api::resource src, uint64_t src_offset, uint32_t row_length, uint32_t slice_height, reshade::api::resource dst, uint32_t dst_subresource, const reshade::api::subresource_box *)
{
	if (!s_do_capture)
		return false;

	{	const std::lock_guard<std::mutex> lock(s_mutex);

		assert(s_resources.find(src.handle) != s_resources.end());
		assert(s_resources.find(dst.handle) != s_resources.end());
	}

	std::stringstream s; s << "copy_buffer_to_texture(" << (void *)src.handle << ", " << src_offset << ", " << row_length << ", " << slice_height << ", " << (void *)dst.handle << ", " << dst_subresource << ")";
	const std::lock_guard<std::mutex> lock(s_mutex); s_capture_log.push_back(s.str());

	return false;
}
static bool on_copy_texture_region(reshade::api::command_list *, reshade::api::resource src, uint32_t src_subresource, const reshade::api::subresource_box *, reshade::api::resource dst, uint32_t dst_subresource, const reshade::api::subresource_box *, reshade::api::filter_mode filter)
{
	if (!s_do_capture)
		return false;

	{	const std::lock_guard<std::mutex> lock(s_mutex);

		assert(s_resources.find(src.handle) != s_resources.end());
		assert(s_resources.find(dst.handle) != s_resources.end());
	}

	std::stringstream s; s << "copy_texture_region(" << (void *)src.handle << ", " << src_subresource << ", " << (void *)dst.handle << ", " << dst_subresource << ", " << (uint32_t)filter << ")";
	const std::lock_guard<std::mutex> lock(s_mutex); s_capture_log.push_back(s.str());

	return false;
}
static bool on_copy_texture_to_buffer(reshade::api::command_list *, reshade::api::resource src, uint32_t src_subresource, const reshade::api::subresource_box *, reshade::api::resource dst, uint64_t dst_offset, uint32_t row_length, uint32_t slice_height)
{
	if (!s_do_capture)
		return false;

	{	const std::lock_guard<std::mutex> lock(s_mutex);

		assert(s_resources.find(src.handle) != s_resources.end());
		assert(s_resources.find(dst.handle) != s_resources.end());
	}

	std::stringstream s; s << "copy_texture_to_buffer(" << (void *)src.handle << ", " << src_subresource << ", " << (void *)dst.handle << ", " << dst_offset << ", " << row_length << ", " << slice_height << ")";
	const std::lock_guard<std::mutex> lock(s_mutex); s_capture_log.push_back(s.str());

	return false;
}
static bool on_resolve_texture_region(reshade::api::command_list *, reshade::api::resource src, uint32_t src_subresource, const reshade::api::subresource_box *, reshade::api::resource dst, uint32_t dst_subresource, int32_t dst_x, int32_t dst_y, int32_t dst_z, reshade::api::format format)
{
	if (!s_do_capture)
		return false;

	{	const std::lock_guard<std::mutex> lock(s_mutex);

		assert(s_resources.find(src.handle) != s_resources.end());
		assert(s_resources.find(dst.handle) != s_resources.end());
	}

	std::stringstream s; s << "resolve_texture_region(" << (void *)src.handle << ", " << src_subresource << ", { ... }, " << (void *)dst.handle << ", " << dst_subresource << ", " << dst_x << ", " << dst_y << ", " << dst_z << ", " << (uint32_t)format << ")";
	const std::lock_guard<std::mutex> lock(s_mutex); s_capture_log.push_back(s.str());

	return false;
}

static bool on_clear_depth_stencil_view(reshade::api::command_list *, reshade::api::resource_view dsv, const float *depth, const uint8_t *stencil, uint32_t, const reshade::api::rect *)
{
	if (!s_do_capture)
		return false;

	{	const std::lock_guard<std::mutex> lock(s_mutex);

		assert(s_resource_views.find(dsv.handle) != s_resource_views.end());
	}

	std::stringstream s; s << "clear_depth_stencil_view(" << (void *)dsv.handle << ", " << (depth != nullptr ? *depth : 0.0f) << ", " << (stencil != nullptr ? *stencil : 0) << ")";
	const std::lock_guard<std::mutex> lock(s_mutex); s_capture_log.push_back(s.str());

	return false;
}
static bool on_clear_render_target_view(reshade::api::command_list *, reshade::api::resource_view rtv, const float color[4], uint32_t, const reshade::api::rect *)
{
	if (!s_do_capture)
		return false;

	{	const std::lock_guard<std::mutex> lock(s_mutex);

		assert(s_resource_views.find(rtv.handle) != s_resource_views.end());
	}

	std::stringstream s; s << "clear_render_target_view(" << (void *)rtv.handle << ", { " << color[0] << ", " << color[1] << ", " << color[2] << ", " << color[3] << " })";
	const std::lock_guard<std::mutex> lock(s_mutex); s_capture_log.push_back(s.str());

	return false;
}
static bool on_clear_unordered_access_view_uint(reshade::api::command_list *, reshade::api::resource_view uav, const uint32_t values[4], uint32_t, const reshade::api::rect *)
{
	if (!s_do_capture)
		return false;

	{	const std::lock_guard<std::mutex> lock(s_mutex);

		assert(s_resource_views.find(uav.handle) != s_resource_views.end());
	}

	std::stringstream s; s << "clear_unordered_access_view_uint(" << (void *)uav.handle << ", { " << values[0] << ", " << values[1] << ", " << values[2] << ", " << values[3] << " })";
	const std::lock_guard<std::mutex> lock(s_mutex); s_capture_log.push_back(s.str());

	return false;
}
static bool on_clear_unordered_access_view_float(reshade::api::command_list *, reshade::api::resource_view uav, const float values[4], uint32_t, const reshade::api::rect *)
{
	if (!s_do_capture)
		return false;

	{	const std::lock_guard<std::mutex> lock(s_mutex);

		assert(s_resource_views.find(uav.handle) != s_resource_views.end());
	}

	std::stringstream s; s << "clear_unordered_access_view_float(" << (void *)uav.handle << ", { " << values[0] << ", " << values[1] << ", " << values[2] << ", " << values[3] << " })";
	const std::lock_guard<std::mutex> lock(s_mutex); s_capture_log.push_back(s.str());

	return false;
}

static bool on_generate_mipmaps(reshade::api::command_list *, reshade::api::resource_view srv)
{
	if (!s_do_capture)
		return false;

	{	const std::lock_guard<std::mutex> lock(s_mutex);

		assert(s_resource_views.find(srv.handle) != s_resource_views.end());
	}

	std::stringstream s; s << "generate_mipmaps(" << (void *)srv.handle << ")";
	const std::lock_guard<std::mutex> lock(s_mutex); s_capture_log.push_back(s.str());

	return false;
}

static void on_present(reshade::api::command_queue *, reshade::api::swapchain *, const reshade::api::rect *, const reshade::api::rect *, uint32_t, const reshade::api::rect *)
{
	if (!s_do_capture)
		return;

	std::stringstream s; s << "present()";
	const std::lock_guard<std::mutex> lock(s_mutex); s_capture_log.push_back(s.str());

	s_do_capture = false;
}

static void draw_overlay(reshade::api::effect_runtime *)
{
	if (!s_do_capture)
	{
		if (ImGui::Button("Capture Frame"))
		{
			s_capture_log.clear();
			s_do_capture = true;
		}
		else
		{
			if (ImGui::BeginChild("log", ImVec2(0, 0), true, ImGuiWindowFlags_AlwaysHorizontalScrollbar))
			{
				for (size_t i = 0; i < s_capture_log.size(); ++i)
				{
					ImGui::TextUnformatted(s_capture_log[i].c_str(), s_capture_log[i].c_str() + s_capture_log[i].size());
				}
			} ImGui::EndChild();
		}
	}
}

extern "C" __declspec(dllexport) const char *NAME = "API Trace";
extern "C" __declspec(dllexport) const char *DESCRIPTION = "Example add-on that logs graphics API calls done by the application to an overlay.";

BOOL APIENTRY DllMain(HMODULE hModule, DWORD fdwReason, LPVOID)
{
	switch (fdwReason)
	{
	case DLL_PROCESS_ATTACH:
		if (!reshade::register_addon(hModule))
			return FALSE;

		reshade::register_overlay("API Trace", draw_overlay);

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
		reshade::register_event<reshade::addon_event::bind_descriptor_sets>(on_bind_descriptor_sets);
		reshade::register_event<reshade::addon_event::bind_index_buffer>(on_bind_index_buffer);
		reshade::register_event<reshade::addon_event::bind_vertex_buffers>(on_bind_vertex_buffers);
		reshade::register_event<reshade::addon_event::draw>(on_draw);
		reshade::register_event<reshade::addon_event::draw_indexed>(on_draw_indexed);
		reshade::register_event<reshade::addon_event::dispatch>(on_dispatch);
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

		reshade::register_event<reshade::addon_event::present>(on_present);
		break;
	case DLL_PROCESS_DETACH:
		reshade::unregister_addon(hModule);
		break;
	}

	return TRUE;
}
