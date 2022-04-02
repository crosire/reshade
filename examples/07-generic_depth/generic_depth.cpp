/*
 * Copyright (C) 2021 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#define ImTextureID unsigned long long

#include <imgui.h>
#include <reshade.hpp>
#include <cmath>
#include <cstring>
#include <algorithm>
#include <vector>
#include <shared_mutex>
#include <unordered_map>

static std::shared_mutex s_mutex;

static bool s_disable_intz = false;
// Enable or disable the creation of backup copies at clear operations on the selected depth-stencil
static bool s_preserve_depth_buffers = false;
// Enable or disable the aspect ratio check from 'check_aspect_ratio' in the detection heuristic
static unsigned int s_use_aspect_ratio_heuristics = 1;

using namespace reshade::api;

struct draw_stats
{
	uint32_t vertices = 0;
	uint32_t drawcalls = 0;
	uint32_t drawcalls_indirect = 0;
	viewport last_viewport = {};
};
struct clear_stats : public draw_stats
{
	bool rect = false;
};

struct depth_stencil_info
{
	draw_stats total_stats;
	draw_stats current_stats; // Stats since last clear operation
	std::vector<clear_stats> clears;
	bool copied_during_frame = false;
};

struct depth_stencil_hash
{
	inline size_t operator()(resource value) const
	{
		// Simply use the handle (which is usually a pointer) as hash value (with some bits shaved off due to pointer alignment)
		return static_cast<size_t>(value.handle >> 4);
	}
};

struct __declspec(uuid("43319e83-387c-448e-881c-7e68fc2e52c4")) state_tracking
{
	draw_stats best_copy_stats;
	bool first_empty_stats = true;
	viewport current_viewport = {};
	resource current_depth_stencil = { 0 };
	std::unordered_map<resource, depth_stencil_info, depth_stencil_hash> counters_per_used_depth_stencil;

	state_tracking()
	{
		// Reserve some space upfront to avoid rehashing during command recording
		counters_per_used_depth_stencil.reserve(32);
	}

	void reset()
	{
		reset_on_present();
		current_depth_stencil = { 0 };
	}
	void reset_on_present()
	{
		best_copy_stats = { 0, 0 };
		first_empty_stats = true;
		counters_per_used_depth_stencil.clear();
	}

	void merge(const state_tracking &source)
	{
		// Executing a command list in a different command list inherits state
		current_depth_stencil = source.current_depth_stencil;

		if (first_empty_stats)
			first_empty_stats = source.first_empty_stats;

		if (source.best_copy_stats.vertices > best_copy_stats.vertices)
			best_copy_stats = source.best_copy_stats;

		if (source.counters_per_used_depth_stencil.empty())
			return;

		counters_per_used_depth_stencil.reserve(source.counters_per_used_depth_stencil.size());
		for (const auto &[depth_stencil_handle, snapshot] : source.counters_per_used_depth_stencil)
		{
			depth_stencil_info &target_snapshot = counters_per_used_depth_stencil[depth_stencil_handle];
			target_snapshot.total_stats.vertices += snapshot.total_stats.vertices;
			target_snapshot.total_stats.drawcalls += snapshot.total_stats.drawcalls;
			target_snapshot.total_stats.drawcalls_indirect += snapshot.total_stats.drawcalls_indirect;
			target_snapshot.current_stats.vertices += snapshot.current_stats.vertices;
			target_snapshot.current_stats.drawcalls += snapshot.current_stats.drawcalls;
			target_snapshot.current_stats.drawcalls_indirect += snapshot.current_stats.drawcalls_indirect;

			target_snapshot.clears.insert(target_snapshot.clears.end(), snapshot.clears.begin(), snapshot.clears.end());

			target_snapshot.copied_during_frame |= snapshot.copied_during_frame;
		}
	}
};

struct __declspec(uuid("7c6363c7-f94e-437a-9160-141782c44a98")) state_tracking_inst
{
	// The depth-stencil that is currently selected as being the main depth target
	resource selected_depth_stencil = { 0 };

	// Resource used to override automatic depth-stencil selection
	resource override_depth_stencil = { 0 };

	// The current shader resource view bound to shaders
	// This can be created from either the original depth-stencil of the application (if it supports shader access) or from a backup resource
	resource_view selected_shader_resource = { 0 };

	// True when the shader resource view was created from the backup resource, false when it was created from the original depth-stencil
	bool using_backup_texture = false;

	std::unordered_map<resource, unsigned int, depth_stencil_hash> display_count_per_depth_stencil;
};

struct depth_stencil_backup
{
	size_t references = 1;

	// Set to zero for automatic detection, otherwise will use the clear operation at the specific index within a frame
	size_t force_clear_index = 0;

	// A resource used as target for a backup copy of this depth-stencil
	resource backup_texture = { 0 };

	// The depth-stencil that should be copied from
	resource depth_stencil_resource = { 0 };

	// Stats of the previous frame for this depth-stencil
	draw_stats previous_stats = {};
};

struct __declspec(uuid("e006e162-33ac-4b9f-b10f-0e15335c7bdb")) state_tracking_context
{
	// List of resources that were deleted this frame
	std::vector<resource> destroyed_resources;

	// List of resources that are enqueued for delayed destruction in the future
	std::vector<std::pair<resource, int>> delayed_destroy_resources;

	// List of all encountered depth-stencils of the last frame
	std::vector<std::pair<resource, depth_stencil_info>> current_depth_stencil_list;

	// List of depth-stencils that should be tracked throughout each frame and potentially be backed up during clear operations
	std::vector<depth_stencil_backup> depth_stencil_backups;

	depth_stencil_backup *find_depth_stencil_backup(resource resource)
	{
		for (depth_stencil_backup &backup : depth_stencil_backups)
			if (backup.depth_stencil_resource == resource)
				return &backup;
		return nullptr;
	}

	depth_stencil_backup *track_depth_stencil_for_backup(device *device, resource resource, resource_desc desc)
	{
		const auto it = std::find_if(depth_stencil_backups.begin(), depth_stencil_backups.end(),
			[resource](const depth_stencil_backup &existing) { return existing.depth_stencil_resource == resource; });
		if (it != depth_stencil_backups.end())
		{
			it->references++;
			return &(*it);
		}

		depth_stencil_backup &backup = depth_stencil_backups.emplace_back();
		backup.depth_stencil_resource = resource;

		desc.type = resource_type::texture_2d;
		desc.heap = memory_heap::gpu_only;
		desc.usage = resource_usage::shader_resource | resource_usage::copy_dest;

		if (device->get_api() == device_api::d3d9)
			desc.texture.format = format::r32_float; // D3DFMT_R32F, since INTZ does not support D3DUSAGE_RENDERTARGET which is required for copying
		else if (device->get_api() != device_api::vulkan) // Use depth format as-is in Vulkan, since those are valid for shader resource views there
			desc.texture.format = format_to_typeless(desc.texture.format);

		if (device->create_resource(desc, nullptr, resource_usage::copy_dest, &backup.backup_texture))
			device->set_resource_name(backup.backup_texture, "ReShade depth backup texture");
		else
			reshade::log_message(1, "Failed to create backup depth-stencil texture!");

		return &backup;
	}

	void untrack_depth_stencil(resource resource)
	{
		const auto it = std::find_if(depth_stencil_backups.begin(), depth_stencil_backups.end(),
			[resource](const depth_stencil_backup &existing) { return existing.depth_stencil_resource == resource; });
		if (it == depth_stencil_backups.end() || --it->references != 0)
			return;

		depth_stencil_backup &backup = *it;

		if (backup.backup_texture != 0)
		{
			// Do not destroy backup texture immediately since it may still be referenced by a command list that is in flight or was prerecorded
			// Instead enqueue it for delayed destruction in the future
			delayed_destroy_resources.emplace_back(backup.backup_texture, 50); // Destroy after 50 frames
		}

		depth_stencil_backups.erase(it);
	}
};

// Checks whether the aspect ratio of the two sets of dimensions is similar or not
static bool check_aspect_ratio(float width_to_check, float height_to_check, uint32_t width, uint32_t height)
{
	if (width_to_check == 0.0f || height_to_check == 0.0f)
		return true;

	const float w = static_cast<float>(width);
	float w_ratio = w / width_to_check;
	const float h = static_cast<float>(height);
	float h_ratio = h / height_to_check;
	const float aspect_ratio = (w / h) - (static_cast<float>(width_to_check) / height_to_check);

	// Accept if dimensions are similar in value or almost exact multiples
	return std::fabs(aspect_ratio) <= 0.1f && ((w_ratio <= 1.85f && w_ratio >= 0.5f && h_ratio <= 1.85f && h_ratio >= 0.5f) || (s_use_aspect_ratio_heuristics == 2 && std::modf(w_ratio, &w_ratio) <= 0.02f && std::modf(h_ratio, &h_ratio) <= 0.02f));
}

static void on_clear_depth_impl(command_list *cmd_list, state_tracking &state, resource depth_stencil, bool fullscreen_draw_call)
{
	if (depth_stencil == 0)
		return;

	device *const device = cmd_list->get_device();

	depth_stencil_backup *const depth_stencil_backup = device->get_private_data<state_tracking_context>().find_depth_stencil_backup(depth_stencil);
	if (depth_stencil_backup == nullptr || depth_stencil_backup->backup_texture == 0)
		return;

	depth_stencil_info &counters = state.counters_per_used_depth_stencil[depth_stencil];

	// Update stats with data from previous frame
	if (!fullscreen_draw_call && counters.current_stats.drawcalls == 0 && state.first_empty_stats)
	{
		counters.current_stats = depth_stencil_backup->previous_stats;
		state.first_empty_stats = false;
	}

	// Ignore clears when there was no meaningful workload
	if (counters.current_stats.drawcalls == 0)
		return;

	// Skip clears when last viewport only affected a subset of the depth-stencil
	if (const resource_desc desc = device->get_resource_desc(depth_stencil);
		!check_aspect_ratio(counters.current_stats.last_viewport.width, counters.current_stats.last_viewport.height, desc.texture.width, desc.texture.height))
	{
		counters.current_stats = { 0, 0 };
		return;
	}

	counters.clears.push_back({ counters.current_stats, fullscreen_draw_call });

	// Make a backup copy of the depth texture before it is cleared
	if (depth_stencil_backup->force_clear_index == 0 ?
		// If clear index override is set to zero, always copy any suitable buffers
		// Use greater equals operator here to handle case where the same scene is first rendered into a shadow map and then for real (e.g. Mirror's Edge)
		fullscreen_draw_call || counters.current_stats.vertices >= state.best_copy_stats.vertices :
		// This is not really correct, since clears may accumulate over multiple command lists, but it's unlikely that the same depth-stencil is used in more than one
		counters.clears.size() == depth_stencil_backup->force_clear_index)
	{
		// Since clears from fullscreen draw calls are selected based on their order (last one wins), their stats are ignored for the regular clear heuristic
		if (!fullscreen_draw_call)
			state.best_copy_stats = counters.current_stats;

		// A resource has to be in this state for a clear operation, so can assume it here
		cmd_list->barrier(depth_stencil, resource_usage::depth_stencil_write, resource_usage::copy_source);
		cmd_list->copy_resource(depth_stencil, depth_stencil_backup->backup_texture);
		cmd_list->barrier(depth_stencil, resource_usage::copy_source, resource_usage::depth_stencil_write);

		counters.copied_during_frame = true;
	}

	// Reset draw call stats for clears
	counters.current_stats = { 0, 0 };
}

static void update_effect_runtime(effect_runtime *runtime)
{
	const state_tracking_inst &instance = runtime->get_private_data<state_tracking_inst>();

	runtime->update_texture_bindings("DEPTH", instance.selected_shader_resource);

	runtime->enumerate_uniform_variables(nullptr, [&instance](effect_runtime *runtime, auto variable) {
		char source[32] = "";
		if (runtime->get_annotation_string_from_uniform_variable(variable, "source", source) && std::strcmp(source, "bufready_depth") == 0)
			runtime->set_uniform_value_bool(variable, instance.selected_shader_resource != 0);
	});
}

static void on_init_device(device *device)
{
	device->create_private_data<state_tracking_context>();

	reshade::config_get_value(nullptr, "DEPTH", "DisableINTZ", s_disable_intz);
	reshade::config_get_value(nullptr, "DEPTH", "DepthCopyBeforeClears", s_preserve_depth_buffers);
	reshade::config_get_value(nullptr, "DEPTH", "UseAspectRatioHeuristics", s_use_aspect_ratio_heuristics);
}
static void on_init_queue_or_command_list(api_object *queue_or_cmd_list)
{
	queue_or_cmd_list->create_private_data<state_tracking>();
}
static void on_init_effect_runtime(effect_runtime *runtime)
{
	runtime->create_private_data<state_tracking_inst>();
}
static void on_destroy_device(device *device)
{
	state_tracking_context &device_state = device->get_private_data<state_tracking_context>();

	// Destroy any remaining resources
	for (const auto &[resource, _] : device_state.delayed_destroy_resources)
	{
		device->destroy_resource(resource);
	}

	for (depth_stencil_backup &depth_stencil_backup : device_state.depth_stencil_backups)
	{
		if (depth_stencil_backup.backup_texture != 0)
			device->destroy_resource(depth_stencil_backup.backup_texture);
	}

	device->destroy_private_data<state_tracking_context>();
}
static void on_destroy_queue_or_command_list(api_object *queue_or_cmd_list)
{
	queue_or_cmd_list->destroy_private_data<state_tracking>();
}
static void on_destroy_effect_runtime(effect_runtime *runtime)
{
	device *const device = runtime->get_device();

	state_tracking_inst &instance = runtime->get_private_data<state_tracking_inst>();

	if (instance.selected_shader_resource != 0)
		device->destroy_resource_view(instance.selected_shader_resource);

	runtime->destroy_private_data<state_tracking_inst>();
}

static bool on_create_resource(device *device, resource_desc &desc, subresource_data *, resource_usage)
{
	if (desc.type != resource_type::surface && desc.type != resource_type::texture_2d)
		return false; // Skip resources that are not 2D textures
	if (desc.texture.samples != 1 || (desc.usage & resource_usage::depth_stencil) == 0 || desc.texture.format == format::s8_uint)
		return false; // Skip MSAA textures and resources that are not used as depth buffers

	switch (device->get_api())
	{
	case device_api::d3d9:
		if (s_disable_intz)
			return false;
		// Skip textures that are sampled as PCF shadow maps using hardware support (see https://aras-p.info/texts/D3D9GPUHacks.html#shadowmap), since changing format would break that
		if (desc.type == resource_type::texture_2d && (desc.texture.format == format::d16_unorm || desc.texture.format == format::d24_unorm_x8_uint || desc.texture.format == format::d24_unorm_s8_uint))
			return false;
		// Replace texture format with special format that supports normal sampling (see https://aras-p.info/texts/D3D9GPUHacks.html#depth)
		desc.texture.format = format::intz;
		desc.usage |= resource_usage::shader_resource;
		break;
	case device_api::d3d10:
	case device_api::d3d11:
		// Allow shader access to images that are used as depth-stencil attachments
		desc.texture.format = format_to_typeless(desc.texture.format);
		desc.usage |= resource_usage::shader_resource;
		break;
	case device_api::d3d12:
	case device_api::vulkan:
		// D3D12 and Vulkan always use backup texture, but need to be able to copy to it
		desc.usage |= resource_usage::copy_source;
		break;
	}

	return true;
}
static bool on_create_resource_view(device *device, resource resource, resource_usage usage_type, resource_view_desc &desc)
{
	// A view cannot be created with a typeless format (which was set in 'on_create_resource' above), so fix it in case defaults are used
	if ((device->get_api() != device_api::d3d10 && device->get_api() != device_api::d3d11) || desc.format != format::unknown)
		return false;

	const resource_desc texture_desc = device->get_resource_desc(resource);
	// Only non-MSAA textures where modified, so skip all others
	if (texture_desc.texture.samples != 1 || (texture_desc.usage & resource_usage::depth_stencil) == 0)
		return false;

	switch (usage_type)
	{
	case resource_usage::depth_stencil:
		desc.format = format_to_depth_stencil_typed(texture_desc.texture.format);
		break;
	case resource_usage::shader_resource:
		desc.format = format_to_default_typed(texture_desc.texture.format);
		break;
	}

	// Only need to set the rest of the fields if the application did not pass in a valid description already
	if (desc.type == resource_view_type::unknown)
	{
		desc.type = texture_desc.texture.depth_or_layers > 1 ? resource_view_type::texture_2d_array : resource_view_type::texture_2d;
		desc.texture.first_level = 0;
		desc.texture.level_count = (usage_type == resource_usage::shader_resource) ? UINT32_MAX : 1;
		desc.texture.first_layer = 0;
		desc.texture.layer_count = (usage_type == resource_usage::shader_resource) ? UINT32_MAX : 1;
	}

	return true;
}
static void on_destroy_resource(device *device, resource resource)
{
	state_tracking_context &device_state = device->get_private_data<state_tracking_context>();

	// In some cases the 'destroy_device' event may be called before all resources have been destroyed
	// The state tracking context would have been destroyed already in that case, so return early if it does not exist
	if (&device_state == nullptr)
		return;

	const std::unique_lock<std::shared_mutex> lock(s_mutex);

	device_state.destroyed_resources.push_back(resource);
}

static bool on_draw(command_list *cmd_list, uint32_t vertices, uint32_t instances, uint32_t, uint32_t)
{
	auto &state = cmd_list->get_private_data<state_tracking>();
	if (state.current_depth_stencil == 0)
		return false; // This is a draw call with no depth-stencil bound

#if 0
	// Check if this draw call likely represets a fullscreen rectangle (one or two triangles), which would clear the depth-stencil
	if (s_preserve_depth_buffers && (vertices == 3 || vertices == 6))
	{
		// TODO: Check pipeline state (cull mode none, depth test enabled, depth write enabled, depth compare function always)
		on_clear_depth_impl(cmd_list, state, state.current_depth_stencil, true);
		return false;
	}
#endif

	depth_stencil_info &counters = state.counters_per_used_depth_stencil[state.current_depth_stencil];
	counters.total_stats.vertices += vertices * instances;
	counters.total_stats.drawcalls += 1;
	counters.current_stats.vertices += vertices * instances;
	counters.current_stats.drawcalls += 1;
	counters.current_stats.last_viewport = state.current_viewport;

	return false;
}
static bool on_draw_indexed(command_list *cmd_list, uint32_t indices, uint32_t instances, uint32_t, int32_t, uint32_t)
{
	on_draw(cmd_list, indices, instances, 0, 0);

	return false;
}
static bool on_draw_indirect(command_list *cmd_list, indirect_command type, resource, uint64_t, uint32_t draw_count, uint32_t)
{
	if (type == indirect_command::dispatch)
		return false;

	auto &state = cmd_list->get_private_data<state_tracking>();
	if (state.current_depth_stencil == 0)
		return false; // This is a draw call with no depth-stencil bound

	depth_stencil_info &counters = state.counters_per_used_depth_stencil[state.current_depth_stencil];
	counters.total_stats.drawcalls += draw_count;
	counters.total_stats.drawcalls_indirect += draw_count;
	counters.current_stats.drawcalls += draw_count;
	counters.current_stats.last_viewport = state.current_viewport;
	counters.current_stats.drawcalls_indirect += draw_count;

	return false;
}

static void on_bind_viewport(command_list *cmd_list, uint32_t first, uint32_t count, const viewport *viewport)
{
	if (first != 0 || count == 0)
		return; // Only interested in the main viewport

	auto &state = cmd_list->get_private_data<state_tracking>();
	state.current_viewport = viewport[0];
}
static void on_bind_depth_stencil(command_list *cmd_list, uint32_t, const resource_view *, resource_view depth_stencil_view)
{
	device *const device = cmd_list->get_device();
	auto &state = cmd_list->get_private_data<state_tracking>();

	resource depth_stencil = { 0 };
	if (depth_stencil_view != 0)
		depth_stencil = device->get_resource_from_view(depth_stencil_view);

	// Make a backup of the depth texture before it is used differently, since in D3D12 or Vulkan the underlying memory may be aliased to a different resource, so cannot just access it at the end of the frame
	if (depth_stencil != state.current_depth_stencil && state.current_depth_stencil != 0 && (device->get_api() == device_api::d3d12 || device->get_api() == device_api::vulkan))
		on_clear_depth_impl(cmd_list, state, state.current_depth_stencil, true);

	state.current_depth_stencil = depth_stencil;
}
static bool on_clear_depth_stencil(command_list *cmd_list, resource_view dsv, const float *depth, const uint8_t *, uint32_t, const rect *)
{
	// Ignore clears that do not affect the depth buffer (stencil clears)
	if (depth != nullptr && s_preserve_depth_buffers)
	{
		auto &state = cmd_list->get_private_data<state_tracking>();

		const resource depth_stencil = cmd_list->get_device()->get_resource_from_view(dsv);
		on_clear_depth_impl(cmd_list, state, depth_stencil, false);
	}

	return false;
}
static void on_begin_render_pass_with_depth_stencil(command_list *cmd_list, uint32_t, const render_pass_render_target_desc *, const render_pass_depth_stencil_desc *depth_stencil_desc)
{
#if 0
	if (depth_stencil_desc != nullptr && depth_stencil_desc->depth_load_op == render_pass_load_op::clear)
		on_clear_depth_stencil(cmd_list, depth_stencil_desc->view, &depth_stencil_desc->clear_depth, &depth_stencil_desc->clear_stencil, 0, nullptr);
#endif

	on_bind_depth_stencil(cmd_list, 0, nullptr, depth_stencil_desc != nullptr ? depth_stencil_desc->view : resource_view {});
}

static void on_reset(command_list *cmd_list)
{
	auto &target_state = cmd_list->get_private_data<state_tracking>();
	target_state.reset();
}
static void on_execute(api_object *queue_or_cmd_list, command_list *cmd_list)
{
	auto &target_state = queue_or_cmd_list->get_private_data<state_tracking>();
	const auto &source_state = cmd_list->get_private_data<state_tracking>();
	target_state.merge(source_state);
}

static void on_present(command_queue *queue, swapchain *swapchain, const rect *, const rect *, uint32_t, const rect *)
{
	auto &queue_state = queue->get_private_data<state_tracking>();

	// Only update device list if there are any depth-stencils, otherwise this may be a second present call (at which point 'reset_on_present' already cleared out the queue list in the first present call)
	if (queue_state.counters_per_used_depth_stencil.empty())
		return;

	// Also skip update when there has been very little activity (special case for emulators which may present more often than they render a frame)
	if (queue_state.counters_per_used_depth_stencil.size() == 1 && queue_state.counters_per_used_depth_stencil.begin()->second.total_stats.drawcalls <= 4)
		return;

	device *const device = swapchain->get_device();
	state_tracking_context &device_state = device->get_private_data<state_tracking_context>();

	const std::unique_lock<std::shared_mutex> lock(s_mutex);

	device_state.current_depth_stencil_list.clear();
	device_state.current_depth_stencil_list.reserve(queue_state.counters_per_used_depth_stencil.size());

	for (const auto &[resource, snapshot] : queue_state.counters_per_used_depth_stencil)
	{
		if (snapshot.total_stats.drawcalls == 0)
			continue; // Skip unused

		if (std::find(device_state.destroyed_resources.begin(), device_state.destroyed_resources.end(), resource) != device_state.destroyed_resources.end())
			continue; // Skip resources that were destroyed by the application

		// Save to current list of depth-stencils on the device, so that it can be displayed in the GUI
		device_state.current_depth_stencil_list.emplace_back(resource, snapshot);
	}

	queue_state.reset_on_present();

	device_state.destroyed_resources.clear();

	// Destroy resources that were enqueued for delayed destruction and have reached the targeted number of passed frames
	for (auto it = device_state.delayed_destroy_resources.begin(); it != device_state.delayed_destroy_resources.end();)
	{
		if (--it->second == 0)
		{
			device->destroy_resource(it->first);

			it = device_state.delayed_destroy_resources.erase(it);
		}
		else
		{
			++it;
		}
	}
}

static void on_begin_render_effects(effect_runtime *runtime, command_list *cmd_list, resource_view, resource_view)
{
	device *const device = runtime->get_device();
	command_queue *const queue = runtime->get_command_queue();

	state_tracking_inst &instance = runtime->get_private_data<state_tracking_inst>();
	state_tracking_context &device_state = device->get_private_data<state_tracking_context>();

	resource best_match = { 0 };
	resource_desc best_match_desc;
	depth_stencil_info best_snapshot;

	uint32_t frame_width, frame_height;
	runtime->get_screenshot_width_and_height(&frame_width, &frame_height);

	std::shared_lock<std::shared_mutex> lock(s_mutex);

	for (const auto &[resource, snapshot] : device_state.current_depth_stencil_list)
	{
		if (std::find(device_state.destroyed_resources.begin(), device_state.destroyed_resources.end(), resource) != device_state.destroyed_resources.end())
			continue; // Skip resources that were destroyed by the application (check here again in case effects are rendered during the frame)

		const resource_desc desc = device->get_resource_desc(resource);
		if (desc.texture.samples > 1)
			continue; // Ignore MSAA textures, since they would need to be resolved first

		if (s_use_aspect_ratio_heuristics && !check_aspect_ratio(static_cast<float>(desc.texture.width), static_cast<float>(desc.texture.height), frame_width, frame_height))
			continue; // Not a good fit

		if (snapshot.total_stats.drawcalls_indirect < (snapshot.total_stats.drawcalls / 3) ?
				// Choose snapshot with the most vertices, since that is likely to contain the main scene
				snapshot.total_stats.vertices > best_snapshot.total_stats.vertices :
				// Or check draw calls, since vertices may not be accurate if application is using indirect draw calls
				snapshot.total_stats.drawcalls > best_snapshot.total_stats.drawcalls)
		{
			best_match = resource;
			best_match_desc = desc;
			best_snapshot = snapshot;
		}
	}

	if (instance.override_depth_stencil != 0)
	{
		if (const auto it = std::find_if(device_state.current_depth_stencil_list.begin(), device_state.current_depth_stencil_list.end(), [&instance](const auto &current) { return current.first == instance.override_depth_stencil; });
			it != device_state.current_depth_stencil_list.end())
		{
			best_match = it->first;
			best_match_desc = device->get_resource_desc(it->first);
			best_snapshot = it->second;
		}
	}

	lock.unlock();

	if (best_match != 0)
	{
		depth_stencil_backup *depth_stencil_backup = device_state.find_depth_stencil_backup(best_match);

		if (best_match != instance.selected_depth_stencil || instance.selected_shader_resource == 0 || (s_preserve_depth_buffers && depth_stencil_backup == nullptr))
		{
			// Destroy previous resource view, since the underlying resource has changed
			if (instance.selected_shader_resource != 0)
			{
				queue->wait_idle(); // Ensure resource view is no longer in-use before destroying it
				device->destroy_resource_view(instance.selected_shader_resource);

				device_state.untrack_depth_stencil(instance.selected_depth_stencil);
			}

			instance.using_backup_texture = false;
			instance.selected_depth_stencil = best_match;
			instance.selected_shader_resource = { 0 };

			const device_api api = device->get_api();

			// Create two-dimensional resource view to the first level and layer of the depth-stencil resource
			resource_view_desc srv_desc(api != device_api::vulkan ? format_to_default_typed(best_match_desc.texture.format) : best_match_desc.texture.format);

			// Need to create backup texture only if doing backup copies or original resource does not support shader access (which is necessary for binding it to effects)
			// Also always create a backup texture in D3D12 or Vulkan to circument problems in case application makes use of resource aliasing
			if (s_preserve_depth_buffers || (best_match_desc.usage & resource_usage::shader_resource) == 0 || (api == device_api::d3d12 || api == device_api::vulkan))
			{
				depth_stencil_backup = device_state.track_depth_stencil_for_backup(device, best_match, best_match_desc);

				// Abort in case backup texture creation failed
				if (depth_stencil_backup->backup_texture == 0)
					return;

				reshade::config_get_value(nullptr, "DEPTH", "DepthCopyAtClearIndex", depth_stencil_backup->force_clear_index);

				if (api == device_api::d3d9)
					srv_desc.format = format::r32_float; // Same format as backup texture, as set in 'track_depth_stencil_for_backup'

				if (!device->create_resource_view(depth_stencil_backup->backup_texture, resource_usage::shader_resource, srv_desc, &instance.selected_shader_resource))
					return;

				instance.using_backup_texture = true;
			}
			else
			{
				if (!device->create_resource_view(best_match, resource_usage::shader_resource, srv_desc, &instance.selected_shader_resource))
					return;
			}

			update_effect_runtime(runtime);
		}

		if (instance.using_backup_texture)
		{
			assert(depth_stencil_backup != nullptr && depth_stencil_backup->backup_texture != 0);
			const resource backup_texture = depth_stencil_backup->backup_texture;

			if (s_preserve_depth_buffers)
			{
				depth_stencil_backup->previous_stats = best_snapshot.current_stats;
			}
			else
			{
				// Copy to backup texture unless already copied during the current frame
				if (!best_snapshot.copied_during_frame && (best_match_desc.usage & resource_usage::copy_source) != 0)
				{
					cmd_list->barrier(best_match, resource_usage::depth_stencil | resource_usage::shader_resource, resource_usage::copy_source);
					cmd_list->copy_resource(best_match, backup_texture);
					cmd_list->barrier(best_match, resource_usage::copy_source, resource_usage::depth_stencil | resource_usage::shader_resource);
				}
			}

			cmd_list->barrier(backup_texture, resource_usage::copy_dest, resource_usage::shader_resource);
		}
		else
		{
			cmd_list->barrier(best_match, resource_usage::depth_stencil | resource_usage::shader_resource, resource_usage::shader_resource);
		}
	}
	else
	{
		// Unset any existing depth-stencil selected in previous frames
		if (instance.selected_depth_stencil != 0)
		{
			if (instance.selected_shader_resource != 0)
			{
				queue->wait_idle(); // Ensure resource view is no longer in-use before destroying it
				device->destroy_resource_view(instance.selected_shader_resource);

				device_state.untrack_depth_stencil(instance.selected_depth_stencil);
			}

			instance.using_backup_texture = false;
			instance.selected_depth_stencil = { 0 };
			instance.selected_shader_resource = { 0 };

			update_effect_runtime(runtime);
		}
	}
}
static void on_finish_render_effects(effect_runtime *runtime, command_list *cmd_list, resource_view, resource_view)
{
	const state_tracking_inst &instance = runtime->get_private_data<state_tracking_inst>();

	if (instance.selected_shader_resource != 0)
	{
		if (instance.using_backup_texture)
		{
			const resource backup_texture = runtime->get_device()->get_resource_from_view(instance.selected_shader_resource);
			cmd_list->barrier(backup_texture, resource_usage::shader_resource, resource_usage::copy_dest);
		}
		else
		{
			cmd_list->barrier(instance.selected_depth_stencil, resource_usage::shader_resource, resource_usage::depth_stencil | resource_usage::shader_resource);
		}
	}
}

static void draw_settings_overlay(effect_runtime *runtime)
{
	device *const device = runtime->get_device();

	state_tracking_inst &instance = runtime->get_private_data<state_tracking_inst>();
	state_tracking_context &device_state = device->get_private_data<state_tracking_context>();

	bool modified = false;
	if (device->get_api() == device_api::d3d9)
		modified |= ImGui::Checkbox("Disable replacement with INTZ format (requires application restart)", &s_disable_intz);

	if (bool use_aspect_ratio_heuristics = s_use_aspect_ratio_heuristics != 0;
		(modified |= ImGui::Checkbox("Use aspect ratio heuristics", &use_aspect_ratio_heuristics)) != false)
		s_use_aspect_ratio_heuristics = use_aspect_ratio_heuristics ? 1 : 0;

	if (s_use_aspect_ratio_heuristics)
	{
		if (bool use_aspect_ratio_heuristics_ex = s_use_aspect_ratio_heuristics == 2;
			(modified |= ImGui::Checkbox("Use extended aspect ratio heuristics (enable when DLSS is active)", &use_aspect_ratio_heuristics_ex)) != false)
			s_use_aspect_ratio_heuristics = use_aspect_ratio_heuristics_ex ? 2 : 1;
	}

	modified |= ImGui::Checkbox("Copy depth buffer before clear operations", &s_preserve_depth_buffers);

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Spacing();

	std::shared_lock<std::shared_mutex> lock(s_mutex);

	if (device_state.current_depth_stencil_list.empty() && !modified)
	{
		ImGui::TextUnformatted("No depth buffers found.");
		return;
	}

	// Sort pointer list so that added/removed items do not change the GUI much
	struct depth_stencil_item
	{
		unsigned int display_count;
		resource resource;
		depth_stencil_info snapshot;
		resource_desc desc;
	};

	std::vector<depth_stencil_item> sorted_item_list;
	sorted_item_list.reserve(device_state.current_depth_stencil_list.size());

	for (const auto &[resource, snapshot] : device_state.current_depth_stencil_list)
	{
		if (auto it = instance.display_count_per_depth_stencil.find(resource);
			it == instance.display_count_per_depth_stencil.end())
		{
			sorted_item_list.push_back({ 1u, resource, snapshot, device->get_resource_desc(resource) });
		}
		else
		{
			sorted_item_list.push_back({ it->second + 1u, resource, snapshot, device->get_resource_desc(resource) });
		}
	}

	lock.unlock();

	std::sort(sorted_item_list.begin(), sorted_item_list.end(), [](const depth_stencil_item &a, const depth_stencil_item &b) {
		return (a.display_count > b.display_count) ||
		       (a.display_count == b.display_count && ((a.desc.texture.width > b.desc.texture.width || (a.desc.texture.width == b.desc.texture.width && a.desc.texture.height > b.desc.texture.height)) ||
		                                               (a.desc.texture.width == b.desc.texture.width && a.desc.texture.height == b.desc.texture.height && a.resource < b.resource)));
	});

	bool has_msaa_depth_stencil = false;

	const auto format_to_string = [](format format) {
		switch (format)
		{
		case format::d16_unorm:
		case format::r16_typeless:
			return "D16  ";
		case format::d16_unorm_s8_uint:
			return "D16S8";
		case format::d24_unorm_x8_uint:
			return "D24X8";
		case format::d24_unorm_s8_uint:
		case format::r24_g8_typeless:
			return "D24S8";
		case format::d32_float:
		case format::r32_float:
		case format::r32_typeless:
			return "D32  ";
		case format::d32_float_s8_uint:
		case format::r32_g8_typeless:
			return "D32S8";
		case format::intz:
			return "INTZ ";
		default:
			return "     ";
		}
	};

	instance.display_count_per_depth_stencil.clear();
	for (const depth_stencil_item &item : sorted_item_list)
	{
		instance.display_count_per_depth_stencil[item.resource] = item.display_count;

		char label[512] = "";
		sprintf_s(label, "%s0x%016llx", (item.resource == instance.selected_depth_stencil ? "> " : "  "), item.resource.handle);

		if (item.desc.texture.samples > 1) // Disable widget for MSAA textures
		{
			has_msaa_depth_stencil = true;

			ImGui::BeginDisabled();
			ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
		}

		if (bool value = (item.resource == instance.override_depth_stencil);
			ImGui::Checkbox(label, &value))
		{
			instance.override_depth_stencil = value ? item.resource : resource { 0 };
			modified = true;
		}

		ImGui::SameLine();
		ImGui::Text("| %4ux%-4u | %s | %5u draw calls (%5u indirect) ==> %8u vertices |%s",
			item.desc.texture.width,
			item.desc.texture.height,
			format_to_string(item.desc.texture.format),
			item.snapshot.total_stats.drawcalls,
			item.snapshot.total_stats.drawcalls_indirect,
			item.snapshot.total_stats.vertices,
			(item.desc.texture.samples > 1 ? " MSAA" : ""));

		if (item.desc.texture.samples > 1)
		{
			ImGui::PopStyleColor();
			ImGui::EndDisabled();
		}

		if (s_preserve_depth_buffers && item.resource == instance.selected_depth_stencil)
		{
			depth_stencil_backup *const depth_stencil_backup = device_state.find_depth_stencil_backup(item.resource);
			if (depth_stencil_backup == nullptr || depth_stencil_backup->backup_texture == 0)
				continue;

			for (size_t clear_index = 1; clear_index <= item.snapshot.clears.size(); ++clear_index)
			{
				sprintf_s(label, "%s  CLEAR %2zu", (clear_index == depth_stencil_backup->force_clear_index ? "> " : "  "), clear_index);

				const auto &clear_stats = item.snapshot.clears[clear_index - 1];

				if (clear_stats.rect)
				{
					ImGui::BeginDisabled();
					ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
				}

				if (bool value = (depth_stencil_backup->force_clear_index == clear_index);
					ImGui::Checkbox(label, &value))
				{
					depth_stencil_backup->force_clear_index = value ? clear_index : 0;
					modified = true;

					reshade::config_set_value(nullptr, "DEPTH", "DepthCopyAtClearIndex", depth_stencil_backup->force_clear_index);
				}

				ImGui::SameLine();
				ImGui::Text("        |           |       | %5u draw calls (%5u indirect) ==> %8u vertices |%s",
					clear_stats.drawcalls,
					clear_stats.drawcalls_indirect,
					clear_stats.vertices,
					clear_stats.rect ? " RECT" : "");

				if (clear_stats.rect)
				{
					ImGui::PopStyleColor();
					ImGui::EndDisabled();
				}
			}

		}
	}

	if (has_msaa_depth_stencil)
	{
		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		ImGui::TextUnformatted("Not all depth buffers are available.\nYou may have to disable MSAA in the game settings for depth buffer detection to work!");
	}

	if (modified)
	{
		// Reset selected depth-stencil to force re-creation of resources next frame (like the backup texture)
		if (instance.selected_shader_resource != 0)
		{
			command_queue *const queue = runtime->get_command_queue();

			queue->wait_idle(); // Ensure resource view is no longer in-use before destroying it
			device->destroy_resource_view(instance.selected_shader_resource);

			device_state.untrack_depth_stencil(instance.selected_depth_stencil);
		}

		instance.using_backup_texture = false;
		instance.selected_depth_stencil = { 0 };
		instance.selected_shader_resource = { 0 };

		update_effect_runtime(runtime);

		reshade::config_set_value(nullptr, "DEPTH", "DisableINTZ", s_disable_intz);
		reshade::config_set_value(nullptr, "DEPTH", "DepthCopyBeforeClears", s_preserve_depth_buffers);
		reshade::config_set_value(nullptr, "DEPTH", "UseAspectRatioHeuristics", s_use_aspect_ratio_heuristics);
	}
}

void register_addon_depth()
{
	reshade::register_overlay(nullptr, draw_settings_overlay);

	reshade::register_event<reshade::addon_event::init_device>(on_init_device);
	reshade::register_event<reshade::addon_event::init_command_list>(reinterpret_cast<void(*)(command_list *)>(on_init_queue_or_command_list));
	reshade::register_event<reshade::addon_event::init_command_queue>(reinterpret_cast<void(*)(command_queue *)>(on_init_queue_or_command_list));
	reshade::register_event<reshade::addon_event::init_effect_runtime>(on_init_effect_runtime);
	reshade::register_event<reshade::addon_event::destroy_device>(on_destroy_device);
	reshade::register_event<reshade::addon_event::destroy_command_list>(reinterpret_cast<void(*)(command_list *)>(on_destroy_queue_or_command_list));
	reshade::register_event<reshade::addon_event::destroy_command_queue>(reinterpret_cast<void(*)(command_queue *)>(on_destroy_queue_or_command_list));
	reshade::register_event<reshade::addon_event::destroy_effect_runtime>(on_destroy_effect_runtime);

	reshade::register_event<reshade::addon_event::create_resource>(on_create_resource);
	reshade::register_event<reshade::addon_event::create_resource_view>(on_create_resource_view);
	reshade::register_event<reshade::addon_event::destroy_resource>(on_destroy_resource);

	reshade::register_event<reshade::addon_event::draw>(on_draw);
	reshade::register_event<reshade::addon_event::draw_indexed>(on_draw_indexed);
	reshade::register_event<reshade::addon_event::draw_or_dispatch_indirect>(on_draw_indirect);
	reshade::register_event<reshade::addon_event::bind_viewports>(on_bind_viewport);
	reshade::register_event<reshade::addon_event::begin_render_pass>(on_begin_render_pass_with_depth_stencil);
	reshade::register_event<reshade::addon_event::bind_render_targets_and_depth_stencil>(on_bind_depth_stencil);
	reshade::register_event<reshade::addon_event::clear_depth_stencil_view>(on_clear_depth_stencil);

	reshade::register_event<reshade::addon_event::reset_command_list>(on_reset);
	reshade::register_event<reshade::addon_event::execute_command_list>(reinterpret_cast<void(*)(command_queue *, command_list *)>(on_execute));
	reshade::register_event<reshade::addon_event::execute_secondary_command_list>(reinterpret_cast<void(*)(command_list *, command_list *)>(on_execute));

	reshade::register_event<reshade::addon_event::present>(on_present);

	reshade::register_event<reshade::addon_event::reshade_begin_effects>(on_begin_render_effects);
	reshade::register_event<reshade::addon_event::reshade_finish_effects>(on_finish_render_effects);
	// Need to set texture binding again after reloading
	reshade::register_event<reshade::addon_event::reshade_reloaded_effects>(update_effect_runtime);
}
void unregister_addon_depth()
{
	reshade::unregister_event<reshade::addon_event::init_device>(on_init_device);
	reshade::unregister_event<reshade::addon_event::init_command_list>(reinterpret_cast<void(*)(command_list *)>(on_init_queue_or_command_list));
	reshade::unregister_event<reshade::addon_event::init_command_queue>(reinterpret_cast<void(*)(command_queue *)>(on_init_queue_or_command_list));
	reshade::unregister_event<reshade::addon_event::init_effect_runtime>(on_init_effect_runtime);
	reshade::unregister_event<reshade::addon_event::destroy_device>(on_destroy_device);
	reshade::unregister_event<reshade::addon_event::destroy_command_list>(reinterpret_cast<void(*)(command_list *)>(on_destroy_queue_or_command_list));
	reshade::unregister_event<reshade::addon_event::destroy_command_queue>(reinterpret_cast<void(*)(command_queue *)>(on_destroy_queue_or_command_list));
	reshade::unregister_event<reshade::addon_event::destroy_effect_runtime>(on_destroy_effect_runtime);

	reshade::unregister_event<reshade::addon_event::create_resource>(on_create_resource);
	reshade::unregister_event<reshade::addon_event::create_resource_view>(on_create_resource_view);
	reshade::unregister_event<reshade::addon_event::destroy_resource>(on_destroy_resource);

	reshade::unregister_event<reshade::addon_event::draw>(on_draw);
	reshade::unregister_event<reshade::addon_event::draw_indexed>(on_draw_indexed);
	reshade::unregister_event<reshade::addon_event::draw_or_dispatch_indirect>(on_draw_indirect);
	reshade::unregister_event<reshade::addon_event::bind_viewports>(on_bind_viewport);
	reshade::unregister_event<reshade::addon_event::begin_render_pass>(on_begin_render_pass_with_depth_stencil);
	reshade::unregister_event<reshade::addon_event::bind_render_targets_and_depth_stencil>(on_bind_depth_stencil);
	reshade::unregister_event<reshade::addon_event::clear_depth_stencil_view>(on_clear_depth_stencil);

	reshade::unregister_event<reshade::addon_event::reset_command_list>(on_reset);
	reshade::unregister_event<reshade::addon_event::execute_command_list>(reinterpret_cast<void(*)(command_queue *, command_list *)>(on_execute));
	reshade::unregister_event<reshade::addon_event::execute_secondary_command_list>(reinterpret_cast<void(*)(command_list *, command_list *)>(on_execute));

	reshade::unregister_event<reshade::addon_event::present>(on_present);

	reshade::unregister_event<reshade::addon_event::reshade_begin_effects>(on_begin_render_effects);
	reshade::unregister_event<reshade::addon_event::reshade_finish_effects>(on_finish_render_effects);
	reshade::unregister_event<reshade::addon_event::reshade_reloaded_effects>(update_effect_runtime);
}

#ifndef BUILTIN_ADDON

extern "C" __declspec(dllexport) const char *NAME = "Generic Depth";
extern "C" __declspec(dllexport) const char *DESCRIPTION = "Automatic depth buffer detection that works in the majority of games.";

BOOL APIENTRY DllMain(HMODULE hModule, DWORD fdwReason, LPVOID)
{
	switch (fdwReason)
	{
	case DLL_PROCESS_ATTACH:
		if (!reshade::register_addon(hModule))
			return FALSE;
		register_addon_depth();
		break;
	case DLL_PROCESS_DETACH:
		unregister_addon_depth();
		reshade::unregister_addon(hModule);
		break;
	}

	return TRUE;
}

#endif
