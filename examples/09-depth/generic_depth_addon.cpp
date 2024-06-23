/*
 * Copyright (C) 2021 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <imgui.h>
#include <reshade.hpp>
#include <vector>
#include <shared_mutex>
#include <unordered_map>
#include <cmath> // std::abs, std::modf
#include <cstring> // std::strcmp
#include <algorithm> // std::find_if, std::remove, std::sort
#include <Unknwn.h>

using namespace reshade::api;

static std::shared_mutex s_mutex;

static bool s_disable_intz = false;
// Enable or disable the creation of backup copies at clear operations on the selected depth-stencil
static unsigned int s_preserve_depth_buffers = 0;
// Enable or disable the aspect ratio check from 'check_aspect_ratio' in the detection heuristic
static unsigned int s_use_aspect_ratio_heuristics = 1;
// Enable or disable the format check from 'check_depth_format' in the detection heuristic
static unsigned int s_use_format_filtering = 0;
static unsigned int s_custom_resolution_filtering[2] = {};

enum class clear_op
{
	clear_depth_stencil_view,
	fullscreen_draw,
	unbind_depth_stencil_view,
};

struct draw_stats
{
	uint32_t vertices = 0;
	uint32_t drawcalls = 0;
	uint32_t drawcalls_indirect = 0;
	viewport last_viewport = {};
};
struct clear_stats : public draw_stats
{
	clear_op clear_op = clear_op::clear_depth_stencil_view;
	bool copied_during_frame = false;
};

struct depth_stencil_frame_stats
{
	draw_stats total_stats;
	draw_stats current_stats; // Stats since last clear operation
	std::vector<clear_stats> clears;
	bool copied_during_frame = false;
};

struct resource_hash
{
	size_t operator()(resource value) const
	{
		// Simply use the handle (which is usually a pointer) as hash value (with some bits shaved off due to pointer alignment)
		return static_cast<size_t>(value.handle >> 4);
	}
};

struct __declspec(uuid("43319e83-387c-448e-881c-7e68fc2e52c4")) state_tracking
{
	const bool is_queue;
	viewport current_viewport = {};
	resource current_depth_stencil = { 0 };
	std::unordered_map<resource, depth_stencil_frame_stats, resource_hash> counters_per_used_depth_stencil;
	bool first_draw_since_bind = true;
	draw_stats best_copy_stats;

	state_tracking(bool is_queue) : is_queue(is_queue)
	{
		// Reserve some space upfront to avoid rehashing during command recording
		counters_per_used_depth_stencil.reserve(32);
	}

	void reset()
	{
		best_copy_stats = { 0, 0 };
		counters_per_used_depth_stencil.clear();
		current_depth_stencil = { 0 };
	}
	void reset_on_present()
	{
		assert(is_queue);
		best_copy_stats = { 0, 0 };
		counters_per_used_depth_stencil.clear();
	}

	void merge(const state_tracking &source)
	{
		// Executing a command list in a different command list inherits state
		current_depth_stencil = source.current_depth_stencil;

		if (source.best_copy_stats.vertices >= best_copy_stats.vertices)
			best_copy_stats = source.best_copy_stats;

		if (source.counters_per_used_depth_stencil.empty())
			return;

		counters_per_used_depth_stencil.reserve(source.counters_per_used_depth_stencil.size());
		for (const auto &[depth_stencil_handle, source_counters] : source.counters_per_used_depth_stencil)
		{
			depth_stencil_frame_stats &counters = counters_per_used_depth_stencil[depth_stencil_handle];
			counters.total_stats.vertices += source_counters.total_stats.vertices;
			counters.total_stats.drawcalls += source_counters.total_stats.drawcalls;
			counters.total_stats.drawcalls_indirect += source_counters.total_stats.drawcalls_indirect;
			counters.current_stats.vertices += source_counters.current_stats.vertices;
			counters.current_stats.drawcalls += source_counters.current_stats.drawcalls;
			counters.current_stats.drawcalls_indirect += source_counters.current_stats.drawcalls_indirect;

			counters.clears.insert(counters.clears.end(), source_counters.clears.begin(), source_counters.clears.end());

			counters.copied_during_frame |= source_counters.copied_during_frame;
		}
	}
};

struct __declspec(uuid("7c6363c7-f94e-437a-9160-141782c44a98")) generic_depth_data
{
	// The depth-stencil resource that is currently selected as being the main depth target
	resource selected_depth_stencil = { 0 };

	// Resource used to override automatic depth-stencil selection
	resource override_depth_stencil = { 0 };

	// The current depth shader resource view bound to shaders
	// This can be created from either the selected depth-stencil resource (if it supports shader access) or from a backup resource
	resource_view selected_shader_resource = { 0 };

	// True when the shader resource view was created from the backup resource, false when it was created from the original depth-stencil
	bool using_backup_texture = false;
};

struct depth_stencil_backup
{
	// The number of effect runtimes referencing this backup
	uint32_t references = 1;

	// Index of the frame after which the backup resource may be destroyed (used to delay destruction until resource is no longer in use)
	uint64_t destroy_after_frame = std::numeric_limits<uint64_t>::max();

	// A resource used as target for a backup copy of this depth-stencil
	resource backup_texture = { 0 };

	// The depth-stencil that should be copied from
	resource depth_stencil_resource = { 0 };

	// Frame dimensions of the last effect runtime this backup was used with
	uint32_t frame_width = 0;
	uint32_t frame_height = 0;

	// Set to zero for automatic detection, otherwise will use the clear operation at the specific index within a frame
	uint32_t force_clear_index = 0;
	uint32_t current_clear_index = 0;
};

struct depth_stencil_resource
{
	depth_stencil_frame_stats last_counters;

	// Index of the frame in which the depth-stencil was last/first seen used in
	uint64_t last_used_in_frame = std::numeric_limits<uint64_t>::max();
	uint64_t first_used_in_frame = std::numeric_limits<uint64_t>::max();
};

struct __declspec(uuid("e006e162-33ac-4b9f-b10f-0e15335c7bdb")) generic_depth_device_data
{
	uint64_t frame_index = 0;

	// List of queues created for this device
	std::vector<command_queue *> queues;

	// List of all encountered depth-stencils of the last frame
	std::unordered_map<resource, depth_stencil_resource, resource_hash> depth_stencil_resources;

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
		assert(resource != 0);

		const auto it = std::find_if(depth_stencil_backups.begin(), depth_stencil_backups.end(),
			[resource](const depth_stencil_backup &existing) { return existing.depth_stencil_resource == resource; });
		if (it != depth_stencil_backups.end())
		{
			depth_stencil_backup &backup = *it;
			backup.references++;

			return &backup;
		}

		const device_api api = device->get_api();
		if (api <= device_api::d3d12)
		{
			std::shared_lock<std::shared_mutex> lock(s_mutex);
			if (depth_stencil_resources.find(resource) == depth_stencil_resources.end())
				return nullptr;

			// Add reference to the resource so that it is not destroyed while it is being copied to the backup texture
			reinterpret_cast<IUnknown *>(resource.handle)->AddRef();
		}

		desc.type = resource_type::texture_2d;
		desc.heap = memory_heap::gpu_only;
		desc.usage = resource_usage::shader_resource | resource_usage::copy_dest;

		if (desc.texture.samples > 1)
		{
			desc.texture.samples = 1;
			desc.usage |= resource_usage::resolve_dest;
		}

		if (api == device_api::d3d9)
			desc.texture.format = format::r32_float; // D3DFMT_R32F, since INTZ does not support D3DUSAGE_RENDERTARGET which is required for copying
		// Use depth format as-is in OpenGL and Vulkan, since those are valid for shader resource views there
		else if (api != device_api::opengl && api != device_api::vulkan)
			desc.texture.format = format_to_typeless(desc.texture.format);

		// First try to revive a backup resource that was previously enqueued for delayed destruction
		for (depth_stencil_backup &backup : depth_stencil_backups)
		{
			if (backup.depth_stencil_resource != 0)
				continue;

			assert(backup.references == 0 && backup.destroy_after_frame != std::numeric_limits<uint64_t>::max());

			const resource_desc existing_desc = device->get_resource_desc(backup.backup_texture);
			if (desc.texture.width == existing_desc.texture.width && desc.texture.height == existing_desc.texture.height && desc.texture.format == existing_desc.texture.format)
			{
				backup.references++;
				backup.depth_stencil_resource = resource;
				backup.destroy_after_frame = std::numeric_limits<uint64_t>::max();

				return &backup;
			}
		}

		depth_stencil_backup &backup = depth_stencil_backups.emplace_back();
		backup.depth_stencil_resource = resource;

		if (device->create_resource(desc, nullptr, resource_usage::copy_dest, &backup.backup_texture))
		{
			device->set_resource_name(backup.backup_texture, "ReShade depth backup texture");

			return &backup;
		}
		else
		{
			depth_stencil_backups.pop_back();
			reshade::log_message(reshade::log_level::error, "Failed to create backup depth-stencil texture!");

			return nullptr;
		}
	}

	void untrack_depth_stencil(device *device, resource resource)
	{
		assert(resource != 0);

		const auto it = std::find_if(depth_stencil_backups.begin(), depth_stencil_backups.end(),
			[resource](const depth_stencil_backup &existing) { return existing.depth_stencil_resource == resource; });
		if (it == depth_stencil_backups.end() || --it->references != 0)
			return;

		depth_stencil_backup &backup = *it;
		backup.depth_stencil_resource = { 0 };

		// Do not destroy backup texture immediately since it may still be referenced by a command list that is in flight or was prerecorded
		// Instead mark it for delayed destruction in the future
		backup.destroy_after_frame = frame_index + 50; // Destroy after 50 frames

		const device_api api = device->get_api();
		if (api <= device_api::d3d12)
		{
			// Release the reference that was added above
			reinterpret_cast<IUnknown *>(resource.handle)->Release();
		}
	}
};

static bool check_depth_format(format format)
{
	switch (s_use_format_filtering)
	{
	default:
		return false;
	case 1:
		return format == format::d16_unorm || format == format::r16_typeless;
	case 2:
		return format == format::d16_unorm_s8_uint;
	case 3:
		return format == format::d24_unorm_x8_uint;
	case 4:
		return format == format::d24_unorm_s8_uint || format == format::r24_g8_typeless;
	case 5:
		return format == format::d32_float || format == format::r32_float || format == format::r32_typeless;
	case 6:
		return format == format::d32_float_s8_uint || format == format::r32_g8_typeless;
	case 7:
		return format == format::intz;
	}
}
// Checks whether the aspect ratio of the two sets of dimensions is similar or not
static bool check_aspect_ratio(float width_to_check, float height_to_check, float width, float height)
{
	if (width_to_check == 0.0f || height_to_check == 0.0f)
		return true;

	if (s_use_aspect_ratio_heuristics == 3 || (s_use_aspect_ratio_heuristics == 4 && s_custom_resolution_filtering[0] == 0 && s_custom_resolution_filtering[1] == 0))
		return width_to_check == width && height_to_check == height;
	if (s_use_aspect_ratio_heuristics == 4)
		return width_to_check == s_custom_resolution_filtering[0] && width_to_check == s_custom_resolution_filtering[1];

	float w_ratio = width / width_to_check;
	float h_ratio = height / height_to_check;
	const float aspect_ratio_delta = (width / height) - (width_to_check / height_to_check);

	// Accept if dimensions are similar in value or almost exact multiples
	return std::abs(aspect_ratio_delta) <= 0.1f && ((w_ratio <= 1.85f && w_ratio >= 0.5f && h_ratio <= 1.85f && h_ratio >= 0.5f) ||
		(s_use_aspect_ratio_heuristics == 2 && std::modf(w_ratio, &w_ratio) <= 0.02f && std::modf(h_ratio, &h_ratio) <= 0.02f));
}

static void on_clear_depth_impl(command_list *cmd_list, state_tracking &state, resource depth_stencil, clear_op op)
{
	if (depth_stencil == 0)
		return;

	device *const device = cmd_list->get_device();

	depth_stencil_backup *const depth_stencil_backup = device->get_private_data<generic_depth_device_data>().find_depth_stencil_backup(depth_stencil);
	if (depth_stencil_backup == nullptr || depth_stencil_backup->backup_texture == 0)
		return;

	// If this is queue state (happens if this is a immediate command list), need to protect access to it, since another thread may be in a present call, which can reset it
	std::shared_lock<std::shared_mutex> lock(s_mutex, std::defer_lock);
	if (state.is_queue)
		lock.lock();

	bool do_copy = true;
	depth_stencil_frame_stats &counters = state.counters_per_used_depth_stencil[depth_stencil];

	// Ignore clears when there was no meaningful workload (e.g. at the start of a frame)
	// Don't do this in Vulkan, to handle common case of DXVK flushing its immediate command buffer and thus resetting its stats during the frame
	if (counters.current_stats.drawcalls == 0 && (device->get_api() != device_api::vulkan))
		return;

	const draw_stats current_stats = counters.current_stats;

	// Reset draw call stats for clears
	counters.current_stats = { 0, 0 };

	// Ignore clears when the last viewport rendered to only affected a small subset of the depth-stencil (fixes flickering in some games)
	switch (op)
	{
	case clear_op::clear_depth_stencil_view:
		// Mirror's Edge and Portal occasionally render something into a small viewport (16x16 in Mirror's Edge, 512x512 in Portal to render underwater geometry)
		do_copy = current_stats.last_viewport.width > 1024 || (current_stats.last_viewport.width == 0 || depth_stencil_backup->frame_width <= 1024);
		break;
	case clear_op::fullscreen_draw:
		// Mass Effect 3 in Mass Effect Legendary Edition sometimes uses a larger common depth buffer for shadow map and scene rendering, where the former uses a 1024x1024 viewport and the latter uses a viewport matching the render resolution
		do_copy = check_aspect_ratio(current_stats.last_viewport.width, current_stats.last_viewport.height, static_cast<float>(depth_stencil_backup->frame_width), static_cast<float>(depth_stencil_backup->frame_height));
		break;
	case clear_op::unbind_depth_stencil_view:
		break;
	}

	if (do_copy)
	{
		if (op != clear_op::unbind_depth_stencil_view)
		{
			// If clear index override is set to zero, always copy any suitable buffers
			if (depth_stencil_backup->force_clear_index == 0)
			{
				// Use greater equals operator here to handle case where the same scene is first rendered into a shadow map and then for real (e.g. Mirror's Edge main menu)
				do_copy = current_stats.vertices >= state.best_copy_stats.vertices || (op == clear_op::fullscreen_draw && current_stats.drawcalls >= state.best_copy_stats.drawcalls);
			}
			else
			if (depth_stencil_backup->force_clear_index == std::numeric_limits<uint32_t>::max())
			{
				// Special case for Garry's Mod which chooses the last clear operation that has a high workload
				do_copy = current_stats.vertices >= 5000;
			}
			else
			{
				do_copy = (depth_stencil_backup->current_clear_index++) == (depth_stencil_backup->force_clear_index - 1);
			}

			counters.clears.push_back({ current_stats, op, do_copy });
		}

		// Make a backup copy of the depth texture before it is cleared
		if (do_copy)
		{
			state.best_copy_stats = current_stats;

			counters.copied_during_frame = true;

			// Unlock before calling into the device, since e.g. in D3D11 this can cause delayed destruction of resources (calls 'CDevice::FlushDeletionPool'), which calls 'on_destroy_resource' below, which tries to lock the same mutex
			if (state.is_queue)
				lock.unlock();

			const resource_desc desc = device->get_resource_desc(depth_stencil);
			if (desc.texture.samples > 1)
			{
				assert(device->check_capability(device_caps::resolve_depth_stencil) && (desc.usage & resource_usage::resolve_source) != 0);

				cmd_list->barrier(depth_stencil, resource_usage::depth_stencil_write, resource_usage::resolve_source);
				cmd_list->resolve_texture_region(depth_stencil, 0, nullptr, depth_stencil_backup->backup_texture, 0, 0, 0, 0, format_to_default_typed(desc.texture.format));
				cmd_list->barrier(depth_stencil, resource_usage::resolve_source, resource_usage::depth_stencil_write);
			}
			else
			{
				assert((desc.usage & resource_usage::copy_source) != 0);

				// A resource has to be in this state for a clear operation, so can assume it here
				cmd_list->barrier(depth_stencil, resource_usage::depth_stencil_write, resource_usage::copy_source);
				cmd_list->copy_resource(depth_stencil, depth_stencil_backup->backup_texture);
				cmd_list->barrier(depth_stencil, resource_usage::copy_source, resource_usage::depth_stencil_write);
			}
		}
	}
}

static void update_effect_runtime(effect_runtime *runtime)
{
	const generic_depth_data &instance = runtime->get_private_data<generic_depth_data>();

	runtime->update_texture_bindings("DEPTH", instance.selected_shader_resource, instance.selected_shader_resource);

	runtime->enumerate_uniform_variables(nullptr, [&instance](effect_runtime *runtime, effect_uniform_variable variable) {
		char source[32];
		if (runtime->get_annotation_string_from_uniform_variable(variable, "source", source) &&
			std::strcmp(source, "bufready_depth") == 0)
			runtime->set_uniform_value_bool(variable, instance.selected_shader_resource != 0);
	});
}

static void on_init_device(device *device)
{
	device->create_private_data<generic_depth_device_data>();

	reshade::get_config_value(nullptr, "DEPTH", "DisableINTZ", s_disable_intz);
	reshade::get_config_value(nullptr, "DEPTH", "DepthCopyBeforeClears", s_preserve_depth_buffers);
	reshade::get_config_value(nullptr, "DEPTH", "UseAspectRatioHeuristics", s_use_aspect_ratio_heuristics);

	reshade::get_config_value(nullptr, "DEPTH", "FilterFormat", s_use_format_filtering);
	reshade::get_config_value(nullptr, "DEPTH", "FilterResolutionWidth", s_custom_resolution_filtering[0]);
	reshade::get_config_value(nullptr, "DEPTH", "FilterResolutionHeight", s_custom_resolution_filtering[1]);

	if (s_use_aspect_ratio_heuristics > 4)
		s_use_aspect_ratio_heuristics = 1;
}
static void on_init_command_list(command_list *cmd_list)
{
	cmd_list->create_private_data<state_tracking>(false);
}
static void on_init_command_queue(command_queue *cmd_queue)
{
	cmd_queue->create_private_data<state_tracking>(true);

	if ((cmd_queue->get_type() & command_queue_type::graphics) == 0)
		return;

	auto &device_data = cmd_queue->get_device()->get_private_data<generic_depth_device_data>();
	device_data.queues.push_back(cmd_queue);
}
static void on_init_effect_runtime(effect_runtime *runtime)
{
	runtime->create_private_data<generic_depth_data>();
}
static void on_destroy_device(device *device)
{
	auto &device_data = device->get_private_data<generic_depth_device_data>();

	// Destroy any remaining resources
	for (depth_stencil_backup &backup : device_data.depth_stencil_backups)
		device->destroy_resource(backup.backup_texture);

	device->destroy_private_data<generic_depth_device_data>();
}
static void on_destroy_command_list(command_list *cmd_list)
{
	cmd_list->destroy_private_data<state_tracking>();
}
static void on_destroy_command_queue(command_queue *cmd_queue)
{
	cmd_queue->destroy_private_data<state_tracking>();

	auto &device_data = cmd_queue->get_device()->get_private_data<generic_depth_device_data>();
	device_data.queues.erase(std::remove(device_data.queues.begin(), device_data.queues.end(), cmd_queue), device_data.queues.end());
}
static void on_destroy_effect_runtime(effect_runtime *runtime)
{
	auto &data = runtime->get_private_data<generic_depth_data>();

	if (data.selected_shader_resource != 0)
	{
		device *const device = runtime->get_device();

		device->destroy_resource_view(data.selected_shader_resource);

		auto &device_data = device->get_private_data<generic_depth_device_data>();
		device_data.untrack_depth_stencil(device, data.selected_depth_stencil);
	}

	runtime->destroy_private_data<generic_depth_data>();
}

static bool on_create_resource(device *device, resource_desc &desc, subresource_data *, resource_usage)
{
	if (desc.type != resource_type::surface && desc.type != resource_type::texture_2d)
		return false; // Skip resources that are not 2D textures

	if ((desc.texture.samples > 1 && !device->check_capability(device_caps::resolve_depth_stencil)) || (desc.usage & resource_usage::depth_stencil) == 0 || desc.texture.format == format::s8_uint)
		return false; // Skip MSAA textures and resources that are not used as depth buffers

	switch (device->get_api())
	{
	case device_api::d3d9:
		if (s_disable_intz || desc.texture.samples > 1)
			return false;
		// Skip textures that are sampled as PCF shadow maps (see https://aras-p.info/texts/D3D9GPUHacks.html#shadowmap) using hardware support, since changing format would break that
		if (desc.type == resource_type::texture_2d && (desc.texture.format == format::d16_unorm || desc.texture.format == format::d24_unorm_x8_uint || desc.texture.format == format::d24_unorm_s8_uint))
			return false;
		// Skip small textures that are likely just shadow maps too (fixes a hang in Dragon's Dogma: Dark Arisen when changing areas)
		if (desc.texture.width <= 512)
			return false;
		if (desc.texture.format == format::d32_float || desc.texture.format == format::d32_float_s8_uint)
			reshade::log_message(reshade::log_level::warning, "Replacing high bit depth depth-stencil format with a lower bit depth format");
		// Replace texture format with special format that supports normal sampling (see https://aras-p.info/texts/D3D9GPUHacks.html#depth)
		desc.texture.format = format::intz;
		desc.usage |= resource_usage::shader_resource;
		break;
	case device_api::d3d10:
	case device_api::d3d11:
		// Allow shader access to textures that are used as depth-stencil attachments
		desc.texture.format = format_to_typeless(desc.texture.format);
		desc.usage |= resource_usage::shader_resource;
		break;
	case device_api::d3d12:
	case device_api::vulkan:
		// D3D12 and Vulkan always use backup texture, but need to be able to copy to it
		if (desc.texture.samples > 1)
			desc.usage |= resource_usage::resolve_source;
		else
			desc.usage |= resource_usage::copy_source;
		break;
	case device_api::opengl:
		// No need to change anything in OpenGL
		return false;
	}

	return true;
}
static bool on_create_resource_view(device *device, resource resource, resource_usage usage_type, resource_view_desc &desc)
{
	// A view cannot be created with a typeless format (which was set in 'on_create_resource' above), so fix it in case defaults are used
	if ((device->get_api() != device_api::d3d10 && device->get_api() != device_api::d3d11) || (desc.format != format::unknown && desc.format != format_to_typeless(desc.format)))
		return false;

	const resource_desc texture_desc = device->get_resource_desc(resource);
	// Only non-MSAA textures where modified, so skip all others
	if ((texture_desc.texture.samples > 1 && !device->check_capability(device_caps::resolve_depth_stencil)) || (texture_desc.usage & resource_usage::depth_stencil) == 0)
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
	auto &device_data = device->get_private_data<generic_depth_device_data>();

	// In some cases the 'destroy_device' event may be called before all resources have been destroyed
	// The state tracking context would have been destroyed already in that case, so return early if it does not exist
	if (std::addressof(device_data) == nullptr)
		return;

	std::unique_lock<std::shared_mutex> lock(s_mutex);

	// Remove this destroyed resource from the list of tracked depth-stencil resources
	if (const auto it = device_data.depth_stencil_resources.find(resource);
		it != device_data.depth_stencil_resources.end())
	{
		device_data.depth_stencil_resources.erase(it);

		// A backup resource is always created in D3D12 and Vulkan, so to find out if an effect runtime references this depth-stencil resource, can simply check if a backup resource was created for it
		if (device_data.find_depth_stencil_backup(resource) != nullptr)
		{
			lock.unlock();

			reshade::log_message(reshade::log_level::warning, "A depth-stencil resource was destroyed while still in use.");

			// This is bad ... the resource may still be in use by an effect on the GPU and destroying it would crash it
			// Try to mitigate that somehow by delaying this thread a little to hopefully give the GPU enough time to catch up before the resource memory is deallocated
			Sleep(500);
		}
	}
}

static bool on_draw(command_list *cmd_list, uint32_t vertices, uint32_t instances, uint32_t, uint32_t)
{
	auto &state = cmd_list->get_private_data<state_tracking>();
	if (state.current_depth_stencil == 0)
		return false; // This is a draw call with no depth-stencil bound

	// Check if this draw call likely represets a fullscreen rectangle (two triangles), which would clear the depth-stencil
	const bool fullscreen_draw = vertices == 6 && instances == 1;
	if (fullscreen_draw &&
		s_preserve_depth_buffers == 2 &&
		state.first_draw_since_bind &&
		// But ignore that in Vulkan (since it is invalid to copy a resource inside an active render pass)
		cmd_list->get_device()->get_api() != device_api::vulkan)
		on_clear_depth_impl(cmd_list, state, state.current_depth_stencil, clear_op::fullscreen_draw);

	// If this is queue state (happens if this is a immediate command list), need to protect access to it, since another thread may be in a present call, which can reset it
	std::shared_lock<std::shared_mutex> lock(s_mutex, std::defer_lock);
	if (state.is_queue)
		lock.lock();

	state.first_draw_since_bind = false;

	depth_stencil_frame_stats &counters = state.counters_per_used_depth_stencil[state.current_depth_stencil];
	counters.total_stats.vertices += vertices * instances;
	counters.total_stats.drawcalls += 1;
	counters.current_stats.vertices += vertices * instances;
	counters.current_stats.drawcalls += 1;

	// Skip updating last viewport for fullscreen draw calls, to prevent a clear operation in Prince of Persia: The Sands of Time from getting filtered out
	if (!fullscreen_draw)
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

	// If this is queue state (happens if this is a immediate command list), need to protect access to it, since another thread may be in a present call, which can reset it
	std::shared_lock<std::shared_mutex> lock(s_mutex, std::defer_lock);
	if (state.is_queue)
		lock.lock();

	depth_stencil_frame_stats &counters = state.counters_per_used_depth_stencil[state.current_depth_stencil];
	counters.total_stats.drawcalls += draw_count;
	counters.total_stats.drawcalls_indirect += draw_count;
	counters.current_stats.drawcalls += draw_count;
	counters.current_stats.drawcalls_indirect += draw_count;
	counters.current_stats.last_viewport = state.current_viewport;

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
	auto &state = cmd_list->get_private_data<state_tracking>();

	const resource depth_stencil = (depth_stencil_view != 0) ? cmd_list->get_device()->get_resource_from_view(depth_stencil_view) : resource { 0 };

	if (depth_stencil != state.current_depth_stencil)
	{
		if (depth_stencil != 0)
			state.first_draw_since_bind = true;

		// Make a backup of the depth texture before it is used differently, since in D3D12 or Vulkan the underlying memory may be aliased to a different resource, so cannot just access it at the end of the frame
		if (s_preserve_depth_buffers == 2 &&
			state.current_depth_stencil != 0 && depth_stencil == 0 && (
			cmd_list->get_device()->get_api() == device_api::d3d12 || cmd_list->get_device()->get_api() == device_api::vulkan))
			on_clear_depth_impl(cmd_list, state, state.current_depth_stencil, clear_op::unbind_depth_stencil_view);
	}

	state.current_depth_stencil = depth_stencil;
}
static bool on_clear_depth_stencil(command_list *cmd_list, resource_view dsv, const float *depth, const uint8_t *, uint32_t, const rect *)
{
	// Ignore clears that do not affect the depth buffer (stencil clears)
	if (depth != nullptr && s_preserve_depth_buffers)
	{
		auto &state = cmd_list->get_private_data<state_tracking>();

		const resource depth_stencil = cmd_list->get_device()->get_resource_from_view(dsv);

		// Note: This does not work when called from 'vkCmdClearAttachments', since it is invalid to copy a resource inside an active render pass
		on_clear_depth_impl(cmd_list, state, depth_stencil, clear_op::clear_depth_stencil_view);
	}

	return false;
}
static void on_begin_render_pass_with_depth_stencil(command_list *cmd_list, uint32_t, const render_pass_render_target_desc *, const render_pass_depth_stencil_desc *depth_stencil_desc)
{
	if (depth_stencil_desc != nullptr && depth_stencil_desc->depth_load_op == render_pass_load_op::clear)
	{
		on_clear_depth_stencil(cmd_list, depth_stencil_desc->view, &depth_stencil_desc->clear_depth, nullptr, 0, nullptr);

		// Prevent 'on_bind_depth_stencil' from copying depth buffer again
		auto &state = cmd_list->get_private_data<state_tracking>();
		state.current_depth_stencil = { 0 };
	}

	// If render pass has depth store operation set to 'discard', any copy performed after the render pass will likely contain broken data, so can only hope that the depth buffer can be copied before that ...

	on_bind_depth_stencil(cmd_list, 0, nullptr, depth_stencil_desc != nullptr ? depth_stencil_desc->view : resource_view {});
}

static void on_reset(command_list *cmd_list)
{
	auto &target_state = cmd_list->get_private_data<state_tracking>();
	target_state.reset();
}
static void on_execute_primary(command_queue *queue, command_list *cmd_list)
{
	// Skip merging state when this execution event is just the immediate command list getting flushed
	if (cmd_list == queue->get_immediate_command_list())
		return;

	auto &target_state = queue->get_private_data<state_tracking>();
	const auto &source_state = cmd_list->get_private_data<state_tracking>();
	assert(target_state.is_queue && !source_state.is_queue);

	// Need to protect access to the queue state, since another thread may be in a present call, which can reset this state
	const std::unique_lock<std::shared_mutex> lock(s_mutex);

	target_state.merge(source_state);
}
static void on_execute_secondary(command_list *cmd_list, command_list *secondary_cmd_list)
{
	auto &target_state = cmd_list->get_private_data<state_tracking>();
	const auto &source_state = secondary_cmd_list->get_private_data<state_tracking>();

	// If this is a secondary command list that was recorded without a depth-stencil binding, but is now executed using a depth-stencil binding, handle it as if an indirect draw call was performed to ensure the depth-stencil is tracked
	if (target_state.current_depth_stencil != 0 && source_state.current_depth_stencil == 0 && source_state.counters_per_used_depth_stencil.empty())
	{
		target_state.current_viewport = source_state.current_viewport;

		on_draw_indirect(cmd_list, indirect_command::draw, { 0 }, 0, 1, 0);
	}
	else
	{
		// If this is queue state (happens if this is a immediate command list), need to protect access to it, since another thread may be in a present call, which can reset it
		std::shared_lock<std::shared_mutex> lock(s_mutex, std::defer_lock);
		if (target_state.is_queue)
			lock.lock();

		target_state.merge(source_state);
	}
}

static void on_present(command_queue *, swapchain *swapchain, const rect *, const rect *, uint32_t, const rect *)
{
	device *const device = swapchain->get_device();
	auto &device_data = device->get_private_data<generic_depth_device_data>();

	const std::unique_lock<std::shared_mutex> lock(s_mutex);

	state_tracking queue_state(true);
	// Merge state from all graphics queues
	for (command_queue *const queue : device_data.queues)
	{
		auto &state = queue->get_private_data<state_tracking>();
		queue_state.merge(state);

		state.reset_on_present();
	}

	// Only update device list if there are any depth-stencils, otherwise this may be a second present call (at which point 'reset_on_present' already cleared out the queue list in the first present call)
	if (queue_state.counters_per_used_depth_stencil.empty())
		return;

	// Also skip update when there has been very little activity (special case for emulators like PCSX2 which may present more often than they render a frame)
	if (queue_state.counters_per_used_depth_stencil.size() == 1 && queue_state.counters_per_used_depth_stencil.begin()->second.total_stats.drawcalls <= 8)
		return;

	device_data.frame_index++;

	for (auto it = device_data.depth_stencil_resources.begin(); it != device_data.depth_stencil_resources.end();)
	{
		depth_stencil_resource &info = it->second;

		if (queue_state.counters_per_used_depth_stencil.find(it->first) == queue_state.counters_per_used_depth_stencil.end() && device_data.frame_index > (info.last_used_in_frame + 30))
		{
			// Remove from list when not used for a couple of frames (e.g. because the resource was actually destroyed since)
			it = device_data.depth_stencil_resources.erase(it);
			continue;
		}

		++it;
	}

	for (const auto &[resource, counters] : queue_state.counters_per_used_depth_stencil)
	{
		// Save to current list of depth-stencils on the device, so that it can be displayed in the GUI
		// This might add a resource again that was destroyed during the frame, so need to add a grace period of a couple of frames before using it to be sure
		depth_stencil_resource &info = device_data.depth_stencil_resources[resource];

		info.last_counters = counters;
		info.last_used_in_frame = device_data.frame_index;

		if (std::numeric_limits<uint64_t>::max() == info.first_used_in_frame)
			info.first_used_in_frame = device_data.frame_index;
	}

	// Destroy resources that were enqueued for delayed destruction and have reached the targeted number of passed frames
	for (auto it = device_data.depth_stencil_backups.begin(); it != device_data.depth_stencil_backups.end();)
	{
		if (device_data.frame_index >= it->destroy_after_frame)
		{
			assert(it->references == 0);

			device->destroy_resource(it->backup_texture);
			it = device_data.depth_stencil_backups.erase(it);
			continue;
		}

		// Reset current clear index
		it->current_clear_index = 0;

		++it;
	}
}

static void on_begin_render_effects(effect_runtime *runtime, command_list *cmd_list, resource_view, resource_view)
{
	device *const device = runtime->get_device();
	auto &data = runtime->get_private_data<generic_depth_data>();
	auto &device_data = device->get_private_data<generic_depth_device_data>();

	if (std::addressof(device_data) == nullptr)
		return;

	resource best_match = { 0 };
	resource_desc best_match_desc;
	const depth_stencil_frame_stats *best_snapshot = nullptr;

	uint32_t frame_width, frame_height;
	runtime->get_screenshot_width_and_height(&frame_width, &frame_height);

	std::shared_lock<std::shared_mutex> lock(s_mutex);
	const std::unordered_map<resource, depth_stencil_resource, resource_hash> current_depth_stencil_resources = device_data.depth_stencil_resources;
	// Unlock while calling into device below, since device may hold a lock itself and that then can deadlock another thread that calls into 'on_destroy_resource' from the device holding that lock
	lock.unlock();

	for (auto &[resource, info] : current_depth_stencil_resources)
	{
		if (info.last_counters.total_stats.drawcalls == 0 || info.last_counters.total_stats.vertices <= 3)
			continue; // Skip unused

		if (info.last_used_in_frame < device_data.frame_index || device_data.frame_index <= (info.first_used_in_frame + 1))
			continue; // Skip resources not used this frame or those that only just appeared for the first time

		const resource_desc desc = device->get_resource_desc(resource);
		if (desc.texture.samples > 1 && !device->check_capability(device_caps::resolve_depth_stencil))
			continue; // Ignore MSAA textures, since they would need to be resolved first

		if (s_use_format_filtering && !check_depth_format(desc.texture.format))
			continue;
		if (s_use_aspect_ratio_heuristics && !check_aspect_ratio(static_cast<float>(desc.texture.width), static_cast<float>(desc.texture.height), static_cast<float>(frame_width), static_cast<float>(frame_height)))
			continue; // Not a good fit

		const depth_stencil_frame_stats &snapshot = info.last_counters;
		if (best_snapshot == nullptr || (snapshot.total_stats.drawcalls_indirect < (snapshot.total_stats.drawcalls / 3) ?
				// Choose snapshot with the most vertices, since that is likely to contain the main scene
				snapshot.total_stats.vertices > best_snapshot->total_stats.vertices :
				// Or check draw calls, since vertices may not be accurate if application is using indirect draw calls
				snapshot.total_stats.drawcalls > best_snapshot->total_stats.drawcalls))
		{
			best_match = resource;
			best_match_desc = desc;
			best_snapshot = &snapshot;
		}
	}

	if (data.override_depth_stencil != 0)
	{
		const auto it = current_depth_stencil_resources.find(data.override_depth_stencil);
		if (it != current_depth_stencil_resources.end())
		{
			best_match = it->first;
			best_match_desc = device->get_resource_desc(it->first);
			best_snapshot = &it->second.last_counters;
		}
	}

	const resource_view prev_shader_resource = data.selected_shader_resource;

	if (best_match != 0) do
	{
		const device_api api = device->get_api();

		depth_stencil_backup *depth_stencil_backup = device_data.find_depth_stencil_backup(best_match);

		if (best_match != data.selected_depth_stencil || prev_shader_resource == 0 || (s_preserve_depth_buffers && depth_stencil_backup == nullptr))
		{
			// Untrack previous depth-stencil first, so that backup texture can potentially be reused
			if (data.selected_depth_stencil != 0)
			{
				device_data.untrack_depth_stencil(device, data.selected_depth_stencil);

				data.using_backup_texture = false;
				data.selected_depth_stencil = { 0 };
			}

			// Create two-dimensional resource view to the first level and layer of the depth-stencil resource
			resource_view_desc srv_desc(api != device_api::opengl && api != device_api::vulkan ? format_to_default_typed(best_match_desc.texture.format) : best_match_desc.texture.format);

			// Need to create backup texture only if doing backup copies or original resource does not support shader access (which is necessary for binding it to effects)
			// Also always create a backup texture in D3D12 or Vulkan to circument problems in case application makes use of resource aliasing
			if (s_preserve_depth_buffers || (best_match_desc.usage & resource_usage::shader_resource) == 0 || best_match_desc.texture.samples > 1 || (api == device_api::d3d12 || api == device_api::vulkan))
			{
				depth_stencil_backup = device_data.track_depth_stencil_for_backup(device, best_match, best_match_desc);

				// Abort in case backup texture creation failed
				if (depth_stencil_backup == nullptr)
					break;
				assert(depth_stencil_backup->backup_texture != 0);

				depth_stencil_backup->frame_width = frame_width;
				depth_stencil_backup->frame_height = frame_height;

				if (s_preserve_depth_buffers)
					reshade::get_config_value(nullptr, "DEPTH", "DepthCopyAtClearIndex", depth_stencil_backup->force_clear_index);
				else
					depth_stencil_backup->force_clear_index = 0;

				// Avoid recreating shader resource view when the backup texture did not change
				if (prev_shader_resource == 0 || device->get_resource_from_view(prev_shader_resource) != depth_stencil_backup->backup_texture)
				{
					if (api == device_api::d3d9)
						srv_desc.format = format::r32_float; // Same format as backup texture, as set in 'track_depth_stencil_for_backup'

					if (!device->create_resource_view(depth_stencil_backup->backup_texture, resource_usage::shader_resource, srv_desc, &data.selected_shader_resource))
						break;
				}

				data.using_backup_texture = true;
			}
			else
			{
				if (!device->create_resource_view(best_match, resource_usage::shader_resource, srv_desc, &data.selected_shader_resource))
					break;

				assert(!data.using_backup_texture);
			}

			data.selected_depth_stencil = best_match;
		}

		if (data.using_backup_texture)
		{
			assert(depth_stencil_backup != nullptr && depth_stencil_backup->backup_texture != 0 && best_snapshot != nullptr);
			const resource backup_texture = depth_stencil_backup->backup_texture;

			// Copy to backup texture unless already copied during the current frame
			if (!best_snapshot->copied_during_frame && (best_match_desc.usage & (resource_usage::copy_source | resource_usage::resolve_source)) != 0 && (s_preserve_depth_buffers != 2 || !(api == device_api::d3d12 || api == device_api::vulkan)))
			{
				bool do_copy = true;
				// Ensure barriers are not created with 'D3D12_RESOURCE_STATE_[...]_SHADER_RESOURCE' when resource has 'D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE' flag set
				const resource_usage old_state = best_match_desc.usage & (resource_usage::depth_stencil | resource_usage::shader_resource);

				lock.lock();
				// Indicate that the copy is now being done, so it is not repeated in case effects are rendered by another runtime (e.g. when there are multiple present calls in a frame)
				if (const auto it = device_data.depth_stencil_resources.find(best_match);
					it != device_data.depth_stencil_resources.end())
					it->second.last_counters.copied_during_frame = true;
				else
					// Resource disappeared from the current depth-stencil list between earlier in this function and now, which indicates that it was destroyed in the meantime
					do_copy = false;
				lock.unlock();

				if (do_copy)
				{
					if (best_match_desc.texture.samples > 1)
					{
						assert(device->check_capability(device_caps::resolve_depth_stencil));

						cmd_list->barrier(best_match, old_state, resource_usage::resolve_source);
						cmd_list->resolve_texture_region(best_match, 0, nullptr, backup_texture, 0, 0, 0, 0, format_to_default_typed(best_match_desc.texture.format));
						cmd_list->barrier(best_match, resource_usage::resolve_source, old_state);
					}
					else
					{
						cmd_list->barrier(best_match, old_state, resource_usage::copy_source);
						cmd_list->copy_resource(best_match, backup_texture);
						cmd_list->barrier(best_match, resource_usage::copy_source, old_state);
					}
				}
			}

			cmd_list->barrier(backup_texture, resource_usage::copy_dest, resource_usage::shader_resource);
		}
		else
		{
			// Unset current depth-stencil view, in case it is bound to an effect as a shader resource (which will fail if it is still bound on output)
			if (api <= device_api::d3d11)
				cmd_list->bind_render_targets_and_depth_stencil(0, nullptr);

			cmd_list->barrier(best_match, resource_usage::depth_stencil | resource_usage::shader_resource, resource_usage::shader_resource);
		}
	} while (0);
	else do
	{
		// Untrack any existing depth-stencil selected in previous frames
		if (data.selected_depth_stencil != 0)
		{
			device_data.untrack_depth_stencil(device, data.selected_depth_stencil);

			data.using_backup_texture = false;
			data.selected_depth_stencil = { 0 };
			data.selected_shader_resource = { 0 };
		}
	} while (0);

	if (prev_shader_resource != data.selected_shader_resource)
	{
		update_effect_runtime(runtime);

		device->destroy_resource_view(prev_shader_resource);
	}
}
static void on_finish_render_effects(effect_runtime *runtime, command_list *cmd_list, resource_view, resource_view)
{
	const auto &data = runtime->get_private_data<generic_depth_data>();

	if (data.selected_shader_resource != 0)
	{
		if (data.using_backup_texture)
		{
			const resource backup_texture = runtime->get_device()->get_resource_from_view(data.selected_shader_resource);
			cmd_list->barrier(backup_texture, resource_usage::shader_resource, resource_usage::copy_dest);
		}
		else
		{
			cmd_list->barrier(data.selected_depth_stencil, resource_usage::shader_resource, resource_usage::depth_stencil | resource_usage::shader_resource);
		}
	}
}

static const char *const format_to_string(format format)
{
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
}

static void draw_settings_overlay(effect_runtime *runtime)
{
	bool force_reset = false;

	const char *const heuristic_items[] = {
		"None",
		"Similar aspect ratio",
		"Multiples of resolution (for DLSS or resolution scaling)",
		"Match resolution exactly",
		"Match custom width and height exactly"
	};
	if (ImGui::Combo("Aspect ratio heuristics", reinterpret_cast<int *>(&s_use_aspect_ratio_heuristics), heuristic_items, static_cast<int>(std::size(heuristic_items))))
	{
		reshade::set_config_value(nullptr, "DEPTH", "UseAspectRatioHeuristics", s_use_aspect_ratio_heuristics);
		force_reset = true;
	}

	if (s_use_aspect_ratio_heuristics == 4)
	{
		if (ImGui::InputInt2("Filter by width and height", reinterpret_cast<int *>(s_custom_resolution_filtering)))
		{
			reshade::set_config_value(nullptr, "DEPTH", "FilterResolutionWidth", s_custom_resolution_filtering[0]);
			reshade::set_config_value(nullptr, "DEPTH", "FilterResolutionHeight", s_custom_resolution_filtering[1]);
			force_reset = true;
		}
	}

	const char *const depth_format_items[] = { // Needs to match switch in 'check_depth_format' above
		"All",
		"D16  ",
		"D16S8",
		"D24X8",
		"D24S8",
		"D32  ",
		"D32S8",
		"INTZ "
	};
	if (ImGui::Combo("Filter by depth buffer format", reinterpret_cast<int *>(&s_use_format_filtering), depth_format_items, static_cast<int>(std::size(depth_format_items))))
	{
		reshade::set_config_value(nullptr, "DEPTH", "FilterFormat", s_use_format_filtering);
		force_reset = true;
	}

	if (bool copy_before_clear_operations = s_preserve_depth_buffers != 0;
		ImGui::Checkbox("Copy depth buffer before clear operations", &copy_before_clear_operations))
	{
		s_preserve_depth_buffers = copy_before_clear_operations ? 1 : 0;
		reshade::set_config_value(nullptr, "DEPTH", "DepthCopyBeforeClears", s_preserve_depth_buffers);
		force_reset = true;
	}

	if (s_preserve_depth_buffers == 0)
		ImGui::SetItemTooltip("Enable this when the depth buffer is empty");

	device *const device = runtime->get_device();
	const bool is_d3d12_or_vulkan = device->get_api() == device_api::d3d12 || device->get_api() == device_api::vulkan;

	if (s_preserve_depth_buffers || is_d3d12_or_vulkan)
	{
		if (bool copy_before_fullscreen_draws = s_preserve_depth_buffers == 2;
			ImGui::Checkbox(is_d3d12_or_vulkan ? "Copy depth buffer during frame to prevent artifacts" : "Copy depth buffer before fullscreen draw calls", &copy_before_fullscreen_draws))
		{
			s_preserve_depth_buffers = copy_before_fullscreen_draws ? 2 : 1;
			reshade::set_config_value(nullptr, "DEPTH", "DepthCopyBeforeClears", s_preserve_depth_buffers);
		}
	}

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Spacing();

	auto &data = runtime->get_private_data<generic_depth_data>();
	auto &device_data = device->get_private_data<generic_depth_device_data>();

	std::shared_lock<std::shared_mutex> lock(s_mutex);

	if (std::addressof(device_data) == nullptr || device_data.depth_stencil_resources.empty())
	{
		ImGui::TextUnformatted("No depth buffers found.");
		return;
	}

	// Sort pointer list so that added/removed items do not change the GUI much
	struct depth_stencil_item
	{
		resource resource;
		depth_stencil_frame_stats snapshot;
		bool unusable;
		resource_desc desc;
	};

	std::vector<depth_stencil_item> sorted_item_list;
	sorted_item_list.reserve(device_data.depth_stencil_resources.size());
	for (const auto &[resource, info] : device_data.depth_stencil_resources)
		sorted_item_list.push_back({ resource, info.last_counters, device_data.frame_index > (info.last_used_in_frame + 5) });

	// Unlock while calling into device below
	lock.unlock();

	for (depth_stencil_item &item : sorted_item_list)
		item.desc = device->get_resource_desc(item.resource);

	std::sort(sorted_item_list.begin(), sorted_item_list.end(),
		[](const depth_stencil_item &a, const depth_stencil_item &b) {
			return ((a.desc.texture.width > b.desc.texture.width || (a.desc.texture.width == b.desc.texture.width && a.desc.texture.height > b.desc.texture.height)) ||
					(a.desc.texture.width == b.desc.texture.width && a.desc.texture.height == b.desc.texture.height && a.resource < b.resource));
		});

	uint32_t frame_width, frame_height;
	runtime->get_screenshot_width_and_height(&frame_width, &frame_height);

	bool has_msaa_depth_stencil = false;
	bool has_no_clear_operations = false;

	for (const depth_stencil_item &item : sorted_item_list)
	{
		bool disabled = item.unusable;
		if (item.desc.texture.samples > 1 && !device->check_capability(device_caps::resolve_depth_stencil)) // Disable widget for MSAA textures
			has_msaa_depth_stencil = disabled = true;

		const bool selected = item.resource == data.selected_depth_stencil;
		const bool candidate =
			(!s_use_format_filtering || check_depth_format(item.desc.texture.format)) &&
			(!s_use_aspect_ratio_heuristics || check_aspect_ratio(static_cast<float>(item.desc.texture.width), static_cast<float>(item.desc.texture.height), static_cast<float>(frame_width), static_cast<float>(frame_height)));

		char label[512] = "";
		sprintf_s(label, "%c 0x%016llx", (selected ? '>' : ' '), item.resource.handle);

		ImGui::BeginDisabled(disabled);
		ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[disabled ? ImGuiCol_TextDisabled : selected || candidate ? ImGuiCol_ButtonActive : ImGuiCol_Text]);

		if (bool value = (item.resource == data.override_depth_stencil);
			ImGui::Checkbox(label, &value))
		{
			data.override_depth_stencil = value ? item.resource : resource { 0 };
			force_reset = true;
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

		ImGui::PopStyleColor();
		ImGui::EndDisabled();

		if (s_preserve_depth_buffers && item.resource == data.selected_depth_stencil)
		{
			if (item.snapshot.clears.empty())
			{
				has_no_clear_operations = !is_d3d12_or_vulkan;
				continue;
			}

			depth_stencil_backup *const depth_stencil_backup = device_data.find_depth_stencil_backup(item.resource);
			if (depth_stencil_backup == nullptr || depth_stencil_backup->backup_texture == 0)
				continue;

			for (uint32_t clear_index = 1; clear_index <= static_cast<uint32_t>(item.snapshot.clears.size()); ++clear_index)
			{
				const clear_stats &clear_stats = item.snapshot.clears[clear_index - 1];

				sprintf_s(label, "%c   CLEAR %2u", clear_stats.copied_during_frame ? '>' : ' ', clear_index);

				if (bool value = (depth_stencil_backup->force_clear_index == clear_index);
					ImGui::Checkbox(label, &value))
				{
					depth_stencil_backup->force_clear_index = value ? clear_index : 0;
					reshade::set_config_value(nullptr, "DEPTH", "DepthCopyAtClearIndex", depth_stencil_backup->force_clear_index);
				}

				ImGui::SameLine();
				ImGui::Text("        |           |       | %5u draw calls (%5u indirect) ==> %8u vertices |%s",
					clear_stats.drawcalls,
					clear_stats.drawcalls_indirect,
					clear_stats.vertices,
					clear_stats.clear_op == clear_op::fullscreen_draw ? " Fullscreen draw call" : "");
			}

			if (sorted_item_list.size() == 1 && !is_d3d12_or_vulkan)
			{
				if (bool value = (depth_stencil_backup->force_clear_index == std::numeric_limits<uint32_t>::max());
					ImGui::Checkbox("    Choose last clear operation with high number of draw calls", &value))
				{
					depth_stencil_backup->force_clear_index = value ? std::numeric_limits<uint32_t>::max() : 0;
					reshade::set_config_value(nullptr, "DEPTH", "DepthCopyAtClearIndex", depth_stencil_backup->force_clear_index);
				}
			}
		}
	}

	if (has_msaa_depth_stencil || has_no_clear_operations)
	{
		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		ImGui::PushTextWrapPos();
		if (has_msaa_depth_stencil)
			ImGui::TextUnformatted("Not all depth buffers are available.\nYou may have to disable MSAA in the game settings for depth buffer detection to work!");
		if (has_no_clear_operations)
			ImGui::Text("No clear operations were found for the selected depth buffer.\n%s",
				s_preserve_depth_buffers != 2 ? "Try enabling \"Copy depth buffer before fullscreen draw calls\" or disable \"Copy depth buffer before clear operations\"!" : "Disable \"Copy depth buffer before clear operations\" or select a different depth buffer!");
		ImGui::PopTextWrapPos();
	}

	if (force_reset)
	{
		// Reset selected depth-stencil to force re-creation of resources next frame (like the backup texture)
		if (data.selected_shader_resource != 0)
		{
			command_queue *const queue = runtime->get_command_queue();

			queue->wait_idle(); // Ensure resource view is no longer in-use before destroying it
			device->destroy_resource_view(data.selected_shader_resource);

			device_data.untrack_depth_stencil(device, data.selected_depth_stencil);
		}

		data.using_backup_texture = false;
		data.selected_depth_stencil = { 0 };
		data.selected_shader_resource = { 0 };

		update_effect_runtime(runtime);
	}
}

void register_addon_depth()
{
	reshade::register_overlay(nullptr, draw_settings_overlay);

	reshade::register_event<reshade::addon_event::init_device>(on_init_device);
	reshade::register_event<reshade::addon_event::init_command_list>(on_init_command_list);
	reshade::register_event<reshade::addon_event::init_command_queue>(on_init_command_queue);
	reshade::register_event<reshade::addon_event::init_effect_runtime>(on_init_effect_runtime);
	reshade::register_event<reshade::addon_event::destroy_device>(on_destroy_device);
	reshade::register_event<reshade::addon_event::destroy_command_list>(on_destroy_command_list);
	reshade::register_event<reshade::addon_event::destroy_command_queue>(on_destroy_command_queue);
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
	reshade::register_event<reshade::addon_event::execute_command_list>(on_execute_primary);
	reshade::register_event<reshade::addon_event::execute_secondary_command_list>(on_execute_secondary);

	reshade::register_event<reshade::addon_event::present>(on_present);

	reshade::register_event<reshade::addon_event::reshade_begin_effects>(on_begin_render_effects);
	reshade::register_event<reshade::addon_event::reshade_finish_effects>(on_finish_render_effects);
	// Need to set texture binding again after reloading
	reshade::register_event<reshade::addon_event::reshade_reloaded_effects>(update_effect_runtime);
}
void unregister_addon_depth()
{
	reshade::unregister_event<reshade::addon_event::init_device>(on_init_device);
	reshade::unregister_event<reshade::addon_event::init_command_list>(on_init_command_list);
	reshade::unregister_event<reshade::addon_event::init_command_queue>(on_init_command_queue);
	reshade::unregister_event<reshade::addon_event::init_effect_runtime>(on_init_effect_runtime);
	reshade::unregister_event<reshade::addon_event::destroy_device>(on_destroy_device);
	reshade::unregister_event<reshade::addon_event::destroy_command_list>(on_destroy_command_list);
	reshade::unregister_event<reshade::addon_event::destroy_command_queue>(on_destroy_command_queue);
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
	reshade::unregister_event<reshade::addon_event::execute_command_list>(on_execute_primary);
	reshade::unregister_event<reshade::addon_event::execute_secondary_command_list>(on_execute_secondary);

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
