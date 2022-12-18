/*
 * Copyright (C) 2022 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause OR MIT
 */

#pragma once

#include <vector>
#include <unordered_map>

/// <summary>
/// A state block capturing current state of a command list.
/// An instance of this is automatically created for all command lists and can be queried with <c>cmd_list->get_private_data&lt;state_block&gt;()</c> (assuming state tracking was registered via <c>register_state_tracking</c>).
/// </summary>
struct __declspec(uuid("c9abddf0-f9c2-4a7b-af49-89d8d470e207")) state_block
{
	/// <summary>
	/// Binds all state captured by this state block on the specified command list.
	/// </summary>
	/// <param name="cmd_list">Target command list to bind the state on.</param>
	void apply(reshade::api::command_list *cmd_list) const;

	/// <summary>
	/// Removes all state in this state block.
	/// </summary>
	void clear();

public /* internal */:
	struct descriptor_set_data
	{
		reshade::api::pipeline_layout layout;
		std::vector<reshade::api::descriptor_set> sets;
	};

	std::vector<reshade::api::resource_view> render_targets;
	reshade::api::resource_view depth_stencil = { 0 };
	std::unordered_map<reshade::api::pipeline_stage, reshade::api::pipeline> pipelines;
	reshade::api::primitive_topology primitive_topology = reshade::api::primitive_topology::undefined;
	uint32_t blend_constant = 0;
	std::vector<reshade::api::viewport> viewports;
	std::vector<reshade::api::rect> scissor_rects;
	std::unordered_map<reshade::api::shader_stage, descriptor_set_data> descriptor_sets;
};

void register_state_tracking();
void unregister_state_tracking();
