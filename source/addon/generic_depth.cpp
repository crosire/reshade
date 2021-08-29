/*
 * Copyright (C) 2021 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#if RESHADE_ADDON

#include "dll_log.hpp"
#include "ini_file.hpp"
#include "reshade.hpp"
#include "addon_manager.hpp"
#include <mutex>
#include <vector>
#include <unordered_map>
#include <imgui.h>
#include <imgui_internal.h>

static bool s_disable_intz = false;
static std::mutex s_mutex;

using namespace reshade::api;

struct draw_stats
{
	uint32_t vertices = 0;
	uint32_t drawcalls = 0;
	float last_viewport[6] = {};
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

struct state_tracking
{
	static constexpr uint8_t GUID[16] = { 0x43, 0x31, 0x9e, 0x83, 0x38, 0x7c, 0x44, 0x8e, 0x88, 0x1c, 0x7e, 0x68, 0xfc, 0x2e, 0x52, 0xc4 };

	draw_stats best_copy_stats;
	bool first_empty_stats = true;
	bool has_indirect_drawcalls = false;
	resource current_depth_stencil = { 0 };
	float current_viewport[6] = {};
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
		has_indirect_drawcalls = false;
		counters_per_used_depth_stencil.clear();
	}

	void merge(const state_tracking &source)
	{
		// Executing a command list in a different command list inherits state
		current_depth_stencil = source.current_depth_stencil;

		if (first_empty_stats)
			first_empty_stats = source.first_empty_stats;
		has_indirect_drawcalls |= source.has_indirect_drawcalls;

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
			target_snapshot.current_stats.vertices += snapshot.current_stats.vertices;
			target_snapshot.current_stats.drawcalls += snapshot.current_stats.drawcalls;

			target_snapshot.clears.insert(target_snapshot.clears.end(), snapshot.clears.begin(), snapshot.clears.end());

			target_snapshot.copied_during_frame |= snapshot.copied_during_frame;
		}
	}
};

struct state_tracking_context
{
	static constexpr uint8_t GUID[16] = { 0x7c, 0x63, 0x63, 0xc7, 0xf9, 0x4e, 0x43, 0x7a, 0x91, 0x60, 0x14, 0x17, 0x82, 0xc4, 0x4a, 0x98 };

	// Enable or disable the creation of backup copies at clear operations on the selected depth-stencil
	bool preserve_depth_buffers = false;
	// Enable or disable the aspect ratio check from 'check_aspect_ratio' in the detection heuristic
	bool use_aspect_ratio_heuristics = true;

	// Set to zero for automatic detection, otherwise will use the clear operation at the specific index within a frame
	size_t force_clear_index = 0;

	// Stats of the previous frame for the selected depth-stencil
	draw_stats previous_stats = {};

	// A resource used as target for a backup copy for the selected depth-stencil
	resource backup_texture = { 0 };

	// The depth-stencil that is currently selected as being the main depth target
	// Any clear operations on it are subject for special handling (backup copy or replacement)
	resource selected_depth_stencil = { 0 };

	// Resource used to override automatic depth-stencil selection
	resource override_depth_stencil = { 0 };

	// The current shader resource view bound to shaders
	// This can be created from either the original depth-stencil of the application (if it supports shader access), or from the backup resource, or from one of the replacement resources
	resource_view selected_shader_resource = { 0 };

	// List of resources that were deleted this frame
	std::vector<resource> destroyed_resources;

#if RESHADE_GUI
	// List of all encountered depth-stencils of the last frame
	std::vector<std::pair<resource, depth_stencil_info>> current_depth_stencil_list;
	std::unordered_map<resource, unsigned int, depth_stencil_hash> display_count_per_depth_stencil;
#endif

	// Checks whether the aspect ratio of the two sets of dimensions is similar or not
	bool check_aspect_ratio(float width_to_check, float height_to_check, uint32_t width, uint32_t height) const
	{
		if (width_to_check == 0.0f || height_to_check == 0.0f)
			return true;

		const float w = static_cast<float>(width);
		const float w_ratio = w / width_to_check;
		const float h = static_cast<float>(height);
		const float h_ratio = h / height_to_check;
		const float aspect_ratio = (w / h) - (static_cast<float>(width_to_check) / height_to_check);

		return std::fabs(aspect_ratio) <= 0.1f && w_ratio <= 1.85f && h_ratio <= 1.85f && w_ratio >= 0.5f && h_ratio >= 0.5f;
	}

	// Update the backup texture to match the requested dimensions
	void update_backup_texture(device *device, resource_desc desc)
	{
		if (backup_texture != 0)
		{
			const resource_desc existing_desc = device->get_resource_desc(backup_texture);

			if (desc.texture.width == existing_desc.texture.width && desc.texture.height == existing_desc.texture.height && desc.texture.format == existing_desc.texture.format)
				return; // Texture already matches dimensions, so can re-use

			device->wait_idle(); // Texture may still be in use on device, so wait for all operations to finish before destroying it
			device->destroy_resource(backup_texture);
			backup_texture = { 0 };
		}

		desc.type = resource_type::texture_2d;
		desc.heap = memory_heap::gpu_only;
		desc.usage = resource_usage::shader_resource | resource_usage::copy_dest;

		if (device->get_api() == device_api::d3d9)
			desc.texture.format = format::r32_float; // D3DFMT_R32F, size INTZ does not support D3DUSAGE_RENDERTARGET which is required for copying
		else if (device->get_api() != device_api::vulkan) // Use depth format as-is in Vulkan, since those are valid for shader resource views there
			desc.texture.format = format_to_typeless(desc.texture.format);

		if (!device->create_resource(desc, nullptr, resource_usage::copy_dest, &backup_texture))
			LOG(ERROR) << "Failed to create backup depth-stencil texture!";
	}
};

static void clear_depth_impl(command_list *cmd_list, state_tracking &state, const state_tracking_context &device_state, resource depth_stencil, bool fullscreen_draw_call)
{
	if (depth_stencil == 0 || device_state.backup_texture == 0 || depth_stencil != device_state.selected_depth_stencil)
		return;

	depth_stencil_info &counters = state.counters_per_used_depth_stencil[depth_stencil];

	// Update stats with data from previous frame
	if (!fullscreen_draw_call && counters.current_stats.drawcalls == 0 && state.first_empty_stats)
	{
		counters.current_stats = device_state.previous_stats;
		state.first_empty_stats = false;
	}

	// Ignore clears when there was no meaningful workload
	if (counters.current_stats.drawcalls == 0)
		return;

	// Skip clears when last viewport only affected a subset of the depth-stencil
	if (const resource_desc desc = cmd_list->get_device()->get_resource_desc(depth_stencil);
		!device_state.check_aspect_ratio(counters.current_stats.last_viewport[2], counters.current_stats.last_viewport[3], desc.texture.width, desc.texture.height))
	{
		counters.current_stats = { 0, 0 };
		return;
	}

	counters.clears.push_back({ counters.current_stats, fullscreen_draw_call });

	// Make a backup copy of the depth texture before it is cleared
	if (device_state.force_clear_index == 0 ?
		// If clear index override is set to zero, always copy any suitable buffers
		// Use greater equals operator here to handle case where the same scene is first rendered into a shadow map and then for real (e.g. Mirror's Edge)
		fullscreen_draw_call || counters.current_stats.vertices >= state.best_copy_stats.vertices :
		// This is not really correct, since clears may accumulate over multiple command lists, but it's unlikely that the same depth-stencil is used in more than one
		counters.clears.size() == device_state.force_clear_index)
	{
		// Since clears from fullscreen draw calls are selected based on their order (last one wins), their stats are ignored for the regular clear heuristic
		if (!fullscreen_draw_call)
			state.best_copy_stats = counters.current_stats;

		// A resource has to be in this state for a clear operation, so can assume it here
		cmd_list->barrier(depth_stencil, resource_usage::depth_stencil_write, resource_usage::copy_source);
		cmd_list->copy_resource(depth_stencil, device_state.backup_texture);
		cmd_list->barrier(depth_stencil, resource_usage::copy_source, resource_usage::depth_stencil_write);

		counters.copied_during_frame = true;
	}

	// Reset draw call stats for clears
	counters.current_stats = { 0, 0 };
}

static void on_init_device(device *device)
{
	state_tracking_context &device_state = device->get_user_data<state_tracking_context>(state_tracking_context::GUID);

	const ini_file &config = reshade::global_config();
	config.get("DEPTH", "DisableINTZ", s_disable_intz);
	config.get("DEPTH", "DepthCopyBeforeClears", device_state.preserve_depth_buffers);
	config.get("DEPTH", "DepthCopyAtClearIndex", device_state.force_clear_index);
	config.get("DEPTH", "UseAspectRatioHeuristics", device_state.use_aspect_ratio_heuristics);

	if (device_state.force_clear_index == std::numeric_limits<uint32_t>::max())
		device_state.force_clear_index  = 0;
}
static void on_destroy_device(device *device)
{
	state_tracking_context &device_state = device->get_user_data<state_tracking_context>(state_tracking_context::GUID);

	if (device_state.backup_texture != 0)
		device->destroy_resource(device_state.backup_texture);
	if (device_state.selected_shader_resource != 0)
		device->destroy_resource_view(device_state.selected_shader_resource);

	device->destroy_user_data<state_tracking_context>(state_tracking_context::GUID);
}
static void on_destroy_queue_or_command_list(api_object *queue_or_cmd_list)
{
	queue_or_cmd_list->destroy_user_data<state_tracking>(state_tracking::GUID);
}

static bool on_create_resource(device *device, resource_desc &desc, subresource_data *, resource_usage)
{
	if (device->get_api() == device_api::d3d12)
		return false; // No need to modify resources in D3D12, since backup texture is used always
	if (desc.type != resource_type::surface && desc.type != resource_type::texture_2d)
		return false; // Skip resources that are not 2D textures
	if (desc.texture.samples != 1)
		return false; // Skip MSAA textures

	// Allow shader access to images that are used as depth-stencil attachments
	if ((desc.usage & resource_usage::depth_stencil) == resource_usage::undefined ||
		(desc.usage & resource_usage::shader_resource) != resource_usage::undefined)
		return false;

	if (device->get_api() == device_api::d3d9)
	{
		if (s_disable_intz)
			return false;
		desc.texture.format = format::intz;
	}
	if (device->get_api() >= device_api::d3d10 && device->get_api() <= device_api::d3d12)
	{
		desc.texture.format = format_to_typeless(desc.texture.format);
	}

	desc.usage |= resource_usage::shader_resource;

	return true;
}
static bool on_create_resource_view(device *device, resource resource, resource_usage usage_type, resource_view_desc &desc)
{
	// A view cannot be created with a typeless format (which was set in 'on_create_resource' above), so fix it in case defaults are used
	if ((device->get_api() != device_api::d3d10 && device->get_api() != device_api::d3d11) || desc.format != format::unknown)
		return false;

	const resource_desc texture_desc = device->get_resource_desc(resource);
	// Only non-MSAA textures where modified, so skip all others
	if (texture_desc.texture.samples != 1 || (texture_desc.usage & resource_usage::depth_stencil) == resource_usage::undefined)
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
		desc.texture.levels = (usage_type == resource_usage::shader_resource) ? 0xFFFFFFFF : 1;
		desc.texture.first_layer = 0;
		desc.texture.layers = (usage_type == resource_usage::shader_resource) ? 0xFFFFFFFF : 1;
	}

	return true;
}
static void on_destroy_resource(device *device, resource resource)
{
	state_tracking_context &device_state = device->get_user_data<state_tracking_context>(state_tracking_context::GUID);

	std::lock_guard<std::mutex> lock(s_mutex);
	device_state.destroyed_resources.push_back(resource);
}

static bool on_draw(command_list *cmd_list, uint32_t vertices, uint32_t instances, uint32_t, uint32_t)
{
	auto &state = cmd_list->get_user_data<state_tracking>(state_tracking::GUID);
	if (state.current_depth_stencil == 0)
		return false; // This is a draw call with no depth-stencil bound

#if 0
	// Check if this draw call likely represets a fullscreen rectangle (one or two triangles), which would clear the depth-stencil
	const state_tracking_context &device_state = cmd_list->get_device()->get_data<state_tracking_context>(state_tracking_context::GUID);
	if (device_state.preserve_depth_buffers && (vertices == 3 || vertices == 6))
	{
		// TODO: Check pipeline state (cull mode none, depth test enabled, depth write enabled, depth compare function always)
		clear_depth_impl(cmd_list, state, device_state, state.current_depth_stencil, true);
		return false;
	}
#endif

	depth_stencil_info &counters = state.counters_per_used_depth_stencil[state.current_depth_stencil];
	counters.total_stats.vertices += vertices * instances;
	counters.total_stats.drawcalls += 1;
	counters.current_stats.vertices += vertices * instances;
	counters.current_stats.drawcalls += 1;
	std::memcpy(counters.current_stats.last_viewport, state.current_viewport, 6 * sizeof(float));

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

	for (uint32_t i = 0; i < draw_count; ++i)
		on_draw(cmd_list, 0, 0, 0, 0);

	auto &state = cmd_list->get_user_data<state_tracking>(state_tracking::GUID);
	state.has_indirect_drawcalls = true;

	return false;
}

static void on_bind_viewport(command_list *cmd_list, uint32_t first, uint32_t count, const float *viewport)
{
	if (first != 0 || count == 0)
		return; // Only interested in the main viewport

	auto &state = cmd_list->get_user_data<state_tracking>(state_tracking::GUID);
	std::memcpy(state.current_viewport, viewport, 6 * sizeof(float));
}
static void on_begin_render_pass(command_list *cmd_list, render_pass, framebuffer fbo)
{
	device *const device = cmd_list->get_device();
	auto &state = cmd_list->get_user_data<state_tracking>(state_tracking::GUID);

	resource depth_stencil = { 0 };
	resource_view depth_stencil_view = { 0 };
	if (device->get_framebuffer_attachment(fbo, attachment_type::depth, 0, &depth_stencil_view))
	{
		device->get_resource_from_view(depth_stencil_view, &depth_stencil);
	}

	// Make a backup of the depth texture before it is used differently, since in D3D12 or Vulkan the underlying memory may be aliased to a different resource, so cannot just access it at the end of the frame
	if (depth_stencil != state.current_depth_stencil && state.current_depth_stencil != 0 && (device->get_api() == device_api::d3d12 || device->get_api() == device_api::vulkan))
	{
		clear_depth_impl(cmd_list, state, device->get_user_data<state_tracking_context>(state_tracking_context::GUID), state.current_depth_stencil, true);
	}

	state.current_depth_stencil = depth_stencil;
}
static void on_bind_depth_stencil(command_list *cmd_list, uint32_t, const resource_view *, resource_view depth_stencil_view)
{
	device *const device = cmd_list->get_device();
	auto &state = cmd_list->get_user_data<state_tracking>(state_tracking::GUID);

	resource depth_stencil = { 0 };
	if (depth_stencil_view.handle != 0)
	{
		device->get_resource_from_view(depth_stencil_view, &depth_stencil);
	}

	// Make a backup of the depth texture before it is used differently, since in D3D12 or Vulkan the underlying memory may be aliased to a different resource, so cannot just access it at the end of the frame
	if (depth_stencil != state.current_depth_stencil && state.current_depth_stencil != 0 && (device->get_api() == device_api::d3d12 || device->get_api() == device_api::vulkan))
	{
		clear_depth_impl(cmd_list, state, device->get_user_data<state_tracking_context>(state_tracking_context::GUID), state.current_depth_stencil, true);
	}

	state.current_depth_stencil = depth_stencil;
}
static bool on_clear_depth_stencil_attachment(command_list *cmd_list, attachment_type flags, const float[4], float, uint8_t, uint32_t, const int32_t *)
{
	device *const device = cmd_list->get_device();
	const state_tracking_context &device_state = device->get_user_data<state_tracking_context>(state_tracking_context::GUID);

	// Ignore clears that do not affect the depth buffer (stencil clears)
	// Also cannot preserve depth buffers here in Vulkan, since it is not valid to issue copy commands inside a render pass (and since this event is being called from 'vkCmdClearAttachments' always is inside one)
	if ((flags & attachment_type::depth) == attachment_type::depth && device_state.preserve_depth_buffers && device->get_api() != device_api::vulkan)
	{
		auto &state = cmd_list->get_user_data<state_tracking>(state_tracking::GUID);

		clear_depth_impl(cmd_list, state, device_state, state.current_depth_stencil, false);
	}

	return false;
}
static bool on_clear_depth_stencil(command_list *cmd_list, resource_view dsv, attachment_type flags, float, uint8_t, uint32_t, const int32_t *)
{
	device *const device = cmd_list->get_device();
	const state_tracking_context &device_state = device->get_user_data<state_tracking_context>(state_tracking_context::GUID);

	// Ignore clears that do not affect the depth buffer (stencil clears)
	if ((flags & attachment_type::depth) == attachment_type::depth && device_state.preserve_depth_buffers)
	{
		auto &state = cmd_list->get_user_data<state_tracking>(state_tracking::GUID);

		resource depth_stencil = { 0 };
		device->get_resource_from_view(dsv, &depth_stencil);

		clear_depth_impl(cmd_list, state, device_state, depth_stencil, false);
	}

	return false;
}

static void on_reset(command_list *cmd_list)
{
	auto &target_state = cmd_list->get_user_data<state_tracking>(state_tracking::GUID);
	target_state.reset();
}
static void on_execute(api_object *queue_or_cmd_list, command_list *cmd_list)
{
	auto &source_state = cmd_list->get_user_data<state_tracking>(state_tracking::GUID);
	auto &target_state = queue_or_cmd_list->get_user_data<state_tracking>(state_tracking::GUID);
	target_state.merge(source_state);
}

static void on_present(command_queue *, swapchain *swapchain)
{
	effect_runtime *const runtime = swapchain->get_effect_runtime();
	device *const device = runtime->get_device();
	command_queue *const queue = runtime->get_command_queue();
	state_tracking &queue_state = queue->get_user_data<state_tracking>(state_tracking::GUID);
	state_tracking_context &device_state = device->get_user_data<state_tracking_context>(state_tracking_context::GUID);

#if RESHADE_GUI
	device_state.current_depth_stencil_list.clear();
	device_state.current_depth_stencil_list.reserve(queue_state.counters_per_used_depth_stencil.size());
#endif

	uint32_t frame_width, frame_height;
	runtime->get_frame_width_and_height(&frame_width, &frame_height);

	resource_desc best_desc = {};
	resource best_match = { 0 };
	depth_stencil_info best_snapshot;

	for (const auto &[resource, snapshot] : queue_state.counters_per_used_depth_stencil)
	{
		if (std::lock_guard<std::mutex> lock(s_mutex);
			std::find(device_state.destroyed_resources.begin(), device_state.destroyed_resources.end(), resource) != device_state.destroyed_resources.end())
			continue; // Skip resources that were destroyed by the application

#if RESHADE_GUI
		// Save to current list of depth-stencils on the device, so that it can be displayed in the GUI
		device_state.current_depth_stencil_list.emplace_back(resource, snapshot);
#endif

		if (snapshot.total_stats.drawcalls == 0)
			continue; // Skip unused

		const resource_desc desc = device->get_resource_desc(resource);
		if (desc.texture.samples > 1)
			continue; // Ignore MSAA textures, since they would need to be resolved first

		if (device_state.use_aspect_ratio_heuristics && !device_state.check_aspect_ratio(static_cast<float>(desc.texture.width), static_cast<float>(desc.texture.height), frame_width, frame_height))
			continue; // Not a good fit

		if (!queue_state.has_indirect_drawcalls ?
			// Choose snapshot with the most vertices, since that is likely to contain the main scene
			snapshot.total_stats.vertices > best_snapshot.total_stats.vertices :
			// Or check draw calls, since vertices may not be accurate if application is using indirect draw calls
			snapshot.total_stats.drawcalls > best_snapshot.total_stats.drawcalls)
		{
			best_desc = desc;
			best_match = resource;
			best_snapshot = snapshot;
		}
	}

	if (std::lock_guard<std::mutex> lock(s_mutex);
		device_state.override_depth_stencil != 0 &&
		std::find(device_state.destroyed_resources.begin(), device_state.destroyed_resources.end(), device_state.override_depth_stencil) == device_state.destroyed_resources.end())
	{
		best_desc = device->get_resource_desc(device_state.override_depth_stencil);
		best_match = device_state.override_depth_stencil;
		best_snapshot = queue_state.counters_per_used_depth_stencil[best_match];
	}

	if (best_match != 0)
	{
		if (best_match != device_state.selected_depth_stencil)
		{
			// Destroy previous resource view, since the underlying resource has changed
			if (device_state.selected_shader_resource != 0)
			{
				device->wait_idle(); // Ensure resource view is no longer in-use before destroying it
				device->destroy_resource_view(device_state.selected_shader_resource);
			}

			device_state.selected_depth_stencil = best_match;
			device_state.selected_shader_resource = { 0 };

			// Create two-dimensional resource view to the first level and layer of the depth-stencil resource
			resource_view_desc srv_desc(device->get_api() != device_api::vulkan ? format_to_default_typed(best_desc.texture.format) : best_desc.texture.format);

			// Need to create backup texture only if doing backup copies or original resource does not support shader access (which is necessary for binding it to effects)
			// Also always create a backup texture in D3D12 or Vulkan to circument problems in case application makes use of resource aliasing
			if (device_state.preserve_depth_buffers || (best_desc.usage & resource_usage::shader_resource) == resource_usage::undefined || (device->get_api() == device_api::d3d12 || device->get_api() == device_api::vulkan))
			{
				device_state.update_backup_texture(device, best_desc);

				if (device_state.backup_texture != 0)
				{
					if (device->get_api() == device_api::d3d9)
						srv_desc.format = format::r32_float; // Same format as backup texture, as set in 'update_backup_texture'

					device->create_resource_view(device_state.backup_texture, resource_usage::shader_resource, srv_desc, &device_state.selected_shader_resource);
				}
			}
			else
			{
				device->create_resource_view(best_match, resource_usage::shader_resource, srv_desc, &device_state.selected_shader_resource);

				if (device_state.backup_texture != 0)
				{
					device->destroy_resource(device_state.backup_texture);
					device_state.backup_texture = { 0 };
				}
			}

			runtime->update_texture_bindings("DEPTH", device_state.selected_shader_resource);

			const bool bufready_depth_value = true;
			runtime->update_uniform_variables("bufready_depth", &bufready_depth_value, 1);
		}

		if (device_state.preserve_depth_buffers)
		{
			device_state.previous_stats = best_snapshot.current_stats;
		}
		else
		{
			// Copy to backup texture unless already copied during the current frame
			if (device_state.backup_texture != 0 && !best_snapshot.copied_during_frame && (best_desc.usage & resource_usage::copy_source) != resource_usage::undefined)
			{
				command_list *const cmd_list = queue->get_immediate_command_list();

				cmd_list->barrier(best_match, resource_usage::depth_stencil | resource_usage::shader_resource, resource_usage::copy_source);
				cmd_list->copy_resource(best_match, device_state.backup_texture);
				cmd_list->barrier(best_match, resource_usage::copy_source, resource_usage::depth_stencil | resource_usage::shader_resource);
			}
		}

		best_snapshot.copied_during_frame = false;
	}
	else
	{
		// Unset any existing depth-stencil selected in previous frames
		if (device_state.selected_depth_stencil != 0)
		{
			if (device_state.selected_shader_resource != 0)
			{
				device->wait_idle(); // Ensure resource view is no longer in-use before destroying it
				device->destroy_resource_view(device_state.selected_shader_resource);
			}

			device_state.selected_depth_stencil = { 0 };
			device_state.selected_shader_resource = { 0 };

			runtime->update_texture_bindings("DEPTH", device_state.selected_shader_resource);

			const bool bufready_depth_value = false;
			runtime->update_uniform_variables("bufready_depth", &bufready_depth_value, 1);
		}
	}

	queue_state.reset_on_present();

	std::lock_guard<std::mutex> lock(s_mutex);
	device_state.destroyed_resources.clear();
}

static void on_init_effect_runtime(effect_runtime *runtime)
{
	device *const device = runtime->get_device();
	const state_tracking_context &device_state = device->get_user_data<state_tracking_context>(state_tracking_context::GUID);

	// Need to set texture binding again after a runtime was reset
	runtime->update_texture_bindings("DEPTH", device_state.selected_shader_resource);

	const bool bufready_depth_value = device_state.selected_shader_resource != 0;
	runtime->update_uniform_variables("bufready_depth", &bufready_depth_value, 1);
}
static void on_begin_render_effects(effect_runtime *runtime, command_list *cmd_list)
{
	device *const device = runtime->get_device();
	const state_tracking_context &device_state = device->get_user_data<state_tracking_context>(state_tracking_context::GUID);

	if (device_state.selected_shader_resource != 0)
	{
		resource resource = { 0 };
		device->get_resource_from_view(device_state.selected_shader_resource, &resource);

		if (resource == device_state.backup_texture)
		{
			cmd_list->barrier(resource, resource_usage::copy_dest, resource_usage::shader_resource);
		}
		else
		{
			cmd_list->barrier(resource, resource_usage::depth_stencil | resource_usage::shader_resource, resource_usage::shader_resource);
		}
	}
}
static void on_finish_render_effects(effect_runtime *runtime, command_list *cmd_list)
{
	device *const device = runtime->get_device();
	const state_tracking_context &device_state = device->get_user_data<state_tracking_context>(state_tracking_context::GUID);

	if (device_state.selected_shader_resource != 0)
	{
		resource resource = { 0 };
		device->get_resource_from_view(device_state.selected_shader_resource, &resource);

		if (resource == device_state.backup_texture)
		{
			cmd_list->barrier(resource, resource_usage::shader_resource, resource_usage::copy_dest);
		}
		else
		{
			cmd_list->barrier(resource, resource_usage::shader_resource, resource_usage::depth_stencil | resource_usage::shader_resource);
		}
	}
}

#if RESHADE_GUI
static void draw_debug_menu(effect_runtime *runtime, void *)
{
	device *const device = runtime->get_device();
	state_tracking_context &device_state = device->get_user_data<state_tracking_context>(state_tracking_context::GUID);

	bool modified = false;
	if (device->get_api() == device_api::d3d9)
		modified |= ImGui::Checkbox("Disable replacement with INTZ format (requires restart)", &s_disable_intz);

	modified |= ImGui::Checkbox("Use aspect ratio heuristics", &device_state.use_aspect_ratio_heuristics);
	modified |= ImGui::Checkbox("Copy depth buffer before clear operations", &device_state.preserve_depth_buffers);

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Spacing();

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
		if (auto it = device_state.display_count_per_depth_stencil.find(resource);
			it == device_state.display_count_per_depth_stencil.end())
		{
			sorted_item_list.push_back({ 1u, resource, snapshot, device->get_resource_desc(resource) });
		}
		else
		{
			sorted_item_list.push_back({ it->second + 1u, resource, snapshot, device->get_resource_desc(resource) });
		}
	}

	std::sort(sorted_item_list.begin(), sorted_item_list.end(), [](const depth_stencil_item &a, const depth_stencil_item &b) {
		return (a.display_count > b.display_count) ||
		       (a.display_count == b.display_count && ((a.desc.texture.width > b.desc.texture.width || (a.desc.texture.width == b.desc.texture.width && a.desc.texture.height > b.desc.texture.height)) ||
		                                               (a.desc.texture.width == b.desc.texture.width && a.desc.texture.height == b.desc.texture.height && a.resource < b.resource)));
	});

	device_state.display_count_per_depth_stencil.clear();
	for (const depth_stencil_item &item : sorted_item_list)
	{
		device_state.display_count_per_depth_stencil[item.resource] = item.display_count;

		char label[512] = "";
		sprintf_s(label, "%s0x%016llx", (item.resource == device_state.selected_depth_stencil ? "> " : "  "), item.resource.handle);

		if (item.desc.texture.samples > 1) // Disable widget for MSAA textures
		{
			ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
			ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
		}

		if (bool value = (item.resource == device_state.override_depth_stencil);
			ImGui::Checkbox(label, &value))
		{
			device_state.override_depth_stencil = value ? item.resource : resource { 0 };
			modified = true;
		}

		ImGui::SameLine();
		ImGui::Text("| %4ux%-4u | %5u draw calls ==> %8u vertices |%s",
			item.desc.texture.width, item.desc.texture.height, item.snapshot.total_stats.drawcalls, item.snapshot.total_stats.vertices, (item.desc.texture.samples > 1 ? " MSAA" : ""));

		if (device_state.preserve_depth_buffers && item.resource == device_state.selected_depth_stencil)
		{
			for (size_t clear_index = 1; clear_index <= item.snapshot.clears.size(); ++clear_index)
			{
				sprintf_s(label, "%s  CLEAR %2zu", (clear_index == device_state.force_clear_index ? "> " : "  "), clear_index);

				if (bool value = (device_state.force_clear_index == clear_index);
					ImGui::Checkbox(label, &value))
				{
					device_state.force_clear_index = value ? clear_index : 0;
					modified = true;
				}

				ImGui::SameLine();
				ImGui::Text("        |           | %5u draw calls ==> %8u vertices |%s",
					item.snapshot.clears[clear_index - 1].drawcalls, item.snapshot.clears[clear_index - 1].vertices,
					item.snapshot.clears[clear_index - 1].rect ? " RECT" : "");
			}
		}

		if (item.desc.texture.samples > 1)
		{
			ImGui::PopStyleColor();
			ImGui::PopItemFlag();
		}
	}

	if (modified)
	{
		// Reset selected depth-stencil to force re-creation of resources next frame (like the backup texture)
		if (device_state.selected_shader_resource != 0)
		{
			device->wait_idle(); // Ensure resource view is no longer in-use before destroying it
			device->destroy_resource_view(device_state.selected_shader_resource);
		}

		device_state.selected_depth_stencil = { 0 };
		device_state.selected_shader_resource = { 0 };

		on_init_effect_runtime(runtime);

		ini_file &config = reshade::global_config();
		config.set("DEPTH", "DisableINTZ", s_disable_intz);
		config.set("DEPTH", "DepthCopyBeforeClears", device_state.preserve_depth_buffers);
		config.set("DEPTH", "DepthCopyAtClearIndex", device_state.force_clear_index);
		config.set("DEPTH", "UseAspectRatioHeuristics", device_state.use_aspect_ratio_heuristics);

		config.save();
	}
}
#endif

void register_builtin_addon_depth(reshade::addon::info &info)
{
	info.name = "Generic Depth";

#if RESHADE_GUI
	reshade::register_overlay("Depth", draw_debug_menu);
#endif

	reshade::register_event<reshade::addon_event::init_device>(on_init_device);
	reshade::register_event<reshade::addon_event::destroy_device>(on_destroy_device);
	reshade::register_event<reshade::addon_event::destroy_command_list>(reinterpret_cast<void(*)(command_list *)>(on_destroy_queue_or_command_list));
	reshade::register_event<reshade::addon_event::destroy_command_queue>(reinterpret_cast<void(*)(command_queue *)>(on_destroy_queue_or_command_list));
	reshade::register_event<reshade::addon_event::init_effect_runtime>(on_init_effect_runtime);

	reshade::register_event<reshade::addon_event::create_resource>(on_create_resource);
	reshade::register_event<reshade::addon_event::create_resource_view>(on_create_resource_view);
	reshade::register_event<reshade::addon_event::destroy_resource>(on_destroy_resource);

	reshade::register_event<reshade::addon_event::draw>(on_draw);
	reshade::register_event<reshade::addon_event::draw_indexed>(on_draw_indexed);
	reshade::register_event<reshade::addon_event::draw_or_dispatch_indirect>(on_draw_indirect);
	reshade::register_event<reshade::addon_event::bind_viewports>(on_bind_viewport);
	reshade::register_event<reshade::addon_event::begin_render_pass>(on_begin_render_pass);
	reshade::register_event<reshade::addon_event::bind_render_targets_and_depth_stencil>(on_bind_depth_stencil);
	reshade::register_event<reshade::addon_event::clear_attachments>(on_clear_depth_stencil_attachment);
	reshade::register_event<reshade::addon_event::clear_depth_stencil_view>(on_clear_depth_stencil);

	reshade::register_event<reshade::addon_event::reset_command_list>(on_reset);
	reshade::register_event<reshade::addon_event::execute_command_list>(reinterpret_cast<void(*)(command_queue *, command_list *)>(on_execute));
	reshade::register_event<reshade::addon_event::execute_secondary_command_list>(reinterpret_cast<void(*)(command_list *, command_list *)>(on_execute));

	reshade::register_event<reshade::addon_event::present>(on_present);

	reshade::register_event<reshade::addon_event::reshade_begin_effects>(on_begin_render_effects);
	reshade::register_event<reshade::addon_event::reshade_finish_effects>(on_finish_render_effects);
}
void unregister_builtin_addon_depth()
{
#if RESHADE_GUI
	reshade::unregister_overlay("Depth");
#endif

	reshade::unregister_event<reshade::addon_event::init_device>(on_init_device);
	reshade::unregister_event<reshade::addon_event::destroy_device>(on_destroy_device);
	reshade::unregister_event<reshade::addon_event::destroy_command_list>(reinterpret_cast<void(*)(command_list *)>(on_destroy_queue_or_command_list));
	reshade::unregister_event<reshade::addon_event::destroy_command_queue>(reinterpret_cast<void(*)(command_queue *)>(on_destroy_queue_or_command_list));
	reshade::unregister_event<reshade::addon_event::init_effect_runtime>(on_init_effect_runtime);

	reshade::unregister_event<reshade::addon_event::create_resource>(on_create_resource);
	reshade::unregister_event<reshade::addon_event::create_resource_view>(on_create_resource_view);
	reshade::unregister_event<reshade::addon_event::destroy_resource>(on_destroy_resource);

	reshade::unregister_event<reshade::addon_event::draw>(on_draw);
	reshade::unregister_event<reshade::addon_event::draw_indexed>(on_draw_indexed);
	reshade::unregister_event<reshade::addon_event::draw_or_dispatch_indirect>(on_draw_indirect);
	reshade::unregister_event<reshade::addon_event::bind_viewports>(on_bind_viewport);
	reshade::unregister_event<reshade::addon_event::begin_render_pass>(on_begin_render_pass);
	reshade::unregister_event<reshade::addon_event::bind_render_targets_and_depth_stencil>(on_bind_depth_stencil);
	reshade::unregister_event<reshade::addon_event::clear_attachments>(on_clear_depth_stencil_attachment);
	reshade::unregister_event<reshade::addon_event::clear_depth_stencil_view>(on_clear_depth_stencil);

	reshade::unregister_event<reshade::addon_event::reset_command_list>(on_reset);
	reshade::unregister_event<reshade::addon_event::execute_command_list>(reinterpret_cast<void(*)(command_queue *, command_list *)>(on_execute));
	reshade::unregister_event<reshade::addon_event::execute_secondary_command_list>(reinterpret_cast<void(*)(command_list *, command_list *)>(on_execute));

	reshade::unregister_event<reshade::addon_event::present>(on_present);

	reshade::unregister_event<reshade::addon_event::reshade_begin_effects>(on_begin_render_effects);
	reshade::unregister_event<reshade::addon_event::reshade_finish_effects>(on_finish_render_effects);
}

#endif
