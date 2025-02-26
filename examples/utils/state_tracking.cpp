/*
 * Copyright (C) 2022 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause OR MIT
 */

#include "reshade.hpp"
#include "state_tracking.hpp"

using namespace reshade::api;

void state_block::apply(command_list *cmd_list) const
{
	if (!render_targets.empty() || depth_stencil != 0)
		cmd_list->bind_render_targets_and_depth_stencil(static_cast<uint32_t>(render_targets.size()), render_targets.data(), depth_stencil);

	for (const auto &[stages, pipeline] : pipelines)
		cmd_list->bind_pipeline(stages, pipeline);

	if (primitive_topology != primitive_topology::undefined)
		cmd_list->bind_pipeline_state(dynamic_state::primitive_topology, static_cast<uint32_t>(primitive_topology));
	if (blend_constant != 0)
		cmd_list->bind_pipeline_state(dynamic_state::blend_constant, blend_constant);
	if (sample_mask != 0xFFFFFFFF)
		cmd_list->bind_pipeline_state(dynamic_state::sample_mask, sample_mask);
	if (front_stencil_reference_value != 0)
		cmd_list->bind_pipeline_state(dynamic_state::front_stencil_reference_value, front_stencil_reference_value);
	if (back_stencil_reference_value != 0)
		cmd_list->bind_pipeline_state(dynamic_state::back_stencil_reference_value, back_stencil_reference_value);

	if (!viewports.empty())
		cmd_list->bind_viewports(0, static_cast<uint32_t>(viewports.size()), viewports.data());
	if (!scissor_rects.empty())
		cmd_list->bind_scissor_rects(0, static_cast<uint32_t>(scissor_rects.size()), scissor_rects.data());

	for (const auto &[stages, descriptor_state] : descriptor_tables)
		cmd_list->bind_descriptor_tables(stages, descriptor_state.first, 0, static_cast<uint32_t>(descriptor_state.second.size()), descriptor_state.second.data());
}

void state_block::clear()
{
	render_targets.clear();
	depth_stencil = { 0 };
	pipelines.clear();
	primitive_topology = primitive_topology::undefined;
	blend_constant = 0;
	sample_mask = 0xFFFFFFFF;
	front_stencil_reference_value = 0;
	back_stencil_reference_value = 0;
	viewports.clear();
	scissor_rects.clear();
	descriptor_tables.clear();
}

static void on_init_command_list(command_list *cmd_list)
{
	cmd_list->create_private_data<state_tracking>();
}
static void on_destroy_command_list(command_list *cmd_list)
{
	cmd_list->destroy_private_data<state_tracking>();
}

static void on_bind_render_targets_and_depth_stencil(command_list *cmd_list, uint32_t count, const resource_view *rtvs, resource_view dsv)
{
	auto &state = *cmd_list->get_private_data<state_tracking>();
	state.render_targets.assign(rtvs, rtvs + count);
	state.depth_stencil = dsv;
}

static void on_bind_pipeline(command_list *cmd_list, pipeline_stage stages, pipeline pipeline)
{
	auto &state = *cmd_list->get_private_data<state_tracking>();
	state.pipelines[stages] = pipeline;
}

static void on_bind_pipeline_states(command_list *cmd_list, uint32_t count, const dynamic_state *states, const uint32_t *values)
{
	auto &state = *cmd_list->get_private_data<state_tracking>();

	for (uint32_t i = 0; i < count; ++i)
	{
		switch (states[i])
		{
		case dynamic_state::primitive_topology:
			state.primitive_topology = static_cast<primitive_topology>(values[i]);
			break;
		case dynamic_state::blend_constant:
			state.blend_constant = values[i];
			break;
		case dynamic_state::sample_mask:
			state.sample_mask = values[i];
			break;
		case dynamic_state::front_stencil_reference_value:
			state.front_stencil_reference_value = values[i];
			break;
		case dynamic_state::back_stencil_reference_value:
			state.back_stencil_reference_value = values[i];
			break;
		}
	}
}

static void on_bind_viewports(command_list *cmd_list, uint32_t first, uint32_t count, const viewport *viewports)
{
	auto &state = *cmd_list->get_private_data<state_tracking>();

	const uint32_t total_count = first + count;
	if (state.viewports.size() < total_count)
		state.viewports.resize(total_count);

	for (uint32_t i = 0; i < count; ++i)
		state.viewports[i + first] = viewports[i];
}

static void on_bind_scissor_rects(command_list *cmd_list, uint32_t first, uint32_t count, const rect *rects)
{
	auto &state = *cmd_list->get_private_data<state_tracking>();

	const uint32_t total_count = first + count;
	if (state.scissor_rects.size() < total_count)
		state.scissor_rects.resize(total_count);

	for (uint32_t i = 0; i < count; ++i)
		state.scissor_rects[i + first] = rects[i];
}

static void on_bind_descriptor_tables(command_list *cmd_list, shader_stage stages, pipeline_layout layout, uint32_t first, uint32_t count, const descriptor_table *tables)
{
	auto &state = cmd_list->get_private_data<state_tracking>()->descriptor_tables[stages];

	if (layout != state.first)
		state.second.clear(); // Layout changed, which resets all descriptor table bindings
	state.first = layout;

	const uint32_t total_count = first + count;
	if (state.second.size() < total_count)
		state.second.resize(total_count);

	for (uint32_t i = 0; i < count; ++i)
		state.second[i + first] = tables[i];
}

static void on_reset_command_list(command_list *cmd_list)
{
	auto &state = *cmd_list->get_private_data<state_tracking>();
	state.clear();
}

void state_tracking::register_events()
{
	reshade::register_event<reshade::addon_event::init_command_list>(on_init_command_list);
	reshade::register_event<reshade::addon_event::destroy_command_list>(on_destroy_command_list);

	reshade::register_event<reshade::addon_event::bind_render_targets_and_depth_stencil>(on_bind_render_targets_and_depth_stencil);
	reshade::register_event<reshade::addon_event::bind_pipeline>(on_bind_pipeline);
	reshade::register_event<reshade::addon_event::bind_pipeline_states>(on_bind_pipeline_states);
	reshade::register_event<reshade::addon_event::bind_viewports>(on_bind_viewports);
	reshade::register_event<reshade::addon_event::bind_scissor_rects>(on_bind_scissor_rects);
	reshade::register_event<reshade::addon_event::bind_descriptor_tables>(on_bind_descriptor_tables);

	reshade::register_event<reshade::addon_event::reset_command_list>(on_reset_command_list);
}
void state_tracking::unregister_events()
{
	reshade::unregister_event<reshade::addon_event::init_command_list>(on_init_command_list);
	reshade::unregister_event<reshade::addon_event::destroy_command_list>(on_destroy_command_list);

	reshade::unregister_event<reshade::addon_event::bind_render_targets_and_depth_stencil>(on_bind_render_targets_and_depth_stencil);
	reshade::unregister_event<reshade::addon_event::bind_pipeline>(on_bind_pipeline);
	reshade::unregister_event<reshade::addon_event::bind_pipeline_states>(on_bind_pipeline_states);
	reshade::unregister_event<reshade::addon_event::bind_viewports>(on_bind_viewports);
	reshade::unregister_event<reshade::addon_event::bind_scissor_rects>(on_bind_scissor_rects);
	reshade::unregister_event<reshade::addon_event::bind_descriptor_tables>(on_bind_descriptor_tables);

	reshade::unregister_event<reshade::addon_event::reset_command_list>(on_reset_command_list);
}
