/*
 * Copyright (C) 2021 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include <imgui.h>
#include <reshade.hpp>
#include "descriptor_set_tracking.hpp"
#include <mutex>
#include <algorithm>
#include <unordered_set>
#include <malloc.h>

imgui_function_table g_imgui_function_table;

using namespace reshade::api;

struct set_data
{
	uint32_t offset;
	uint32_t range;
	descriptor_set_layout layout;
};

struct tex_data
{
	resource_desc desc;
	resource_view last_view;
	uint64_t last_used;
};

struct tex_hash
{
	inline size_t operator()(resource_view value) const
	{
		return static_cast<size_t>(value.handle);
	}
};

struct cmd_data
{
	static constexpr uint8_t GUID[16] = { 0xf3, 0x26, 0xa1, 0xeb, 0x50, 0x62, 0x45, 0x3e, 0x98, 0x52, 0xa7, 0x87, 0x59, 0x4a, 0x97, 0x7a };

	std::unordered_set<resource_view, tex_hash> current_texture_list;
};

struct device_data
{
	static constexpr uint8_t GUID[16] = { 0x0c, 0xe5, 0x1b, 0x56, 0xa9, 0x73, 0x41, 0x04, 0xbc, 0xca, 0x94, 0x56, 0x86, 0xf5, 0x01, 0x70 };

	resource green_texture = {};
	resource_view green_texture_srv = {};
	resource_view replaced_texture_srv = {};
	std::unordered_set<resource_view, tex_hash> current_texture_list;
	std::map<resource, tex_data> total_texture_list;
	std::vector<resource_view> destroyed_views;
	uint64_t frame_index = 0;

	bool filter = false;
	float scale = 1.0f;
};

static std::mutex s_mutex;
static bool s_is_in_reshade_runtime = false;

static void on_init_device(device *device)
{
	auto &data = device->create_user_data<device_data>(device_data::GUID);

	constexpr uint32_t GREEN = 0xff00ff00;

	subresource_data initial_data;
	initial_data.data = const_cast<uint32_t *>(&GREEN);
	initial_data.row_pitch = sizeof(GREEN);
	initial_data.slice_pitch = sizeof(GREEN);

	if (!device->create_resource(resource_desc(1, 1, 1, 1, format::r8g8b8a8_unorm, 1, memory_heap::gpu_only, resource_usage::shader_resource), &initial_data, resource_usage::shader_resource, &data.green_texture))
	{
		reshade::log_message(1, "Failed to create green texture!");
	}
	if (!device->create_resource_view(data.green_texture, resource_usage::shader_resource, resource_view_desc(format::r8g8b8a8_unorm), &data.green_texture_srv))
	{
		reshade::log_message(1, "Failed to create green texture view!");
	}
}
static void on_destroy_device(device *device)
{
	auto &data = device->get_user_data<device_data>(device_data::GUID);

	device->destroy_resource(data.green_texture);
	device->destroy_resource_view(data.green_texture_srv);

	device->destroy_user_data<device_data>(device_data::GUID);
}
static void on_init_cmd_list(command_list *cmd_list)
{
	cmd_list->create_user_data<cmd_data>(cmd_data::GUID);
}
static void on_destroy_cmd_list(command_list *cmd_list)
{
	cmd_list->destroy_user_data<cmd_data>(cmd_data::GUID);
}

static void on_init_texture(device *device, const resource_desc &desc, const subresource_data *, resource_usage, resource res)
{
	if (desc.type != resource_type::texture_2d)
		return;
	if (device->get_api() != device_api::opengl && (desc.usage & (resource_usage::shader_resource | resource_usage::depth_stencil | resource_usage::render_target)) != resource_usage::shader_resource)
		return;

	auto &data = device->get_user_data<device_data>(device_data::GUID);

	std::lock_guard<std::mutex> lock(s_mutex);

	data.total_texture_list.emplace(res, tex_data { desc });
}
static void on_destroy_texture(device *device, resource res)
{
	auto &data = device->get_user_data<device_data>(device_data::GUID);

	// In some cases the 'destroy_device' event may be called before all resources have been destroyed
	if (&data == nullptr)
		return;

	std::lock_guard<std::mutex> lock(s_mutex);

	if (const auto it = data.total_texture_list.find(res);
		it != data.total_texture_list.end())
	{
		if (s_is_in_reshade_runtime)
			/*Sleep(5)*/;
		else
			device->wait_idle(); // In case a command list is still in flight using one of those

		data.total_texture_list.erase(it);
	}
}
static void on_destroy_texture_view(device *device, resource_view view)
{
	auto &data = device->get_user_data<device_data>(device_data::GUID);

	// In some cases the 'destroy_device' event may be called before all resource views have been destroyed
	if (&data == nullptr)
		return;

	std::lock_guard<std::mutex> lock(s_mutex);

	data.destroyed_views.push_back(view);

	for (auto &tex : data.total_texture_list)
	{
		if (tex.second.last_view == view)
		{
			tex.second.last_view.handle = 0;
		}
	}
}

static void on_push_descriptors(command_list *cmd_list, shader_stage stages, pipeline_layout layout, uint32_t param_index, const descriptor_set_update &update)
{
	s_is_in_reshade_runtime = false;

	if ((stages & shader_stage::pixel) != shader_stage::pixel || (update.type != descriptor_type::shader_resource_view && update.type != descriptor_type::sampler_with_resource_view))
		return;

	device *const device = cmd_list->get_device();
	auto &data = device->get_user_data<device_data>(device_data::GUID);
	auto &cmd_data = cmd_list->get_user_data<::cmd_data>(::cmd_data::GUID);

	for (uint32_t i = 0; i < update.count; ++i)
	{
		if (update.type == descriptor_type::shader_resource_view)
		{
			resource_view descriptor = static_cast<const resource_view *>(update.descriptors)[i];
			if (descriptor.handle == 0)
				continue;

			cmd_data.current_texture_list.emplace(descriptor);

			if (data.replaced_texture_srv == descriptor)
			{
				descriptor = data.green_texture_srv;

				descriptor_set_update new_update = update;
				new_update.binding += i;
				new_update.array_offset = 0;
				new_update.count = 1;
				new_update.descriptors = &descriptor;

				cmd_list->push_descriptors(stages, layout, param_index, new_update);
			}
		}
		else
		{
			sampler_with_resource_view descriptor = static_cast<const sampler_with_resource_view *>(update.descriptors)[i];
			if (descriptor.view.handle == 0)
				continue;

			cmd_data.current_texture_list.emplace(descriptor.view);

			if (data.replaced_texture_srv == descriptor.view)
			{
				descriptor.view = data.green_texture_srv;

				descriptor_set_update new_update = update;
				new_update.binding += i;
				new_update.array_offset = 0;
				new_update.count = 1;
				new_update.descriptors = &descriptor;

				cmd_list->push_descriptors(stages, layout, param_index, new_update);
			}
		}
	}
}
static void on_bind_descriptor_sets(command_list *cmd_list, shader_stage stages, pipeline_layout layout, uint32_t first, uint32_t count, const descriptor_set *sets)
{
	s_is_in_reshade_runtime = false;

	if ((stages & shader_stage::pixel) != shader_stage::pixel)
		return;

	device *const device = cmd_list->get_device();
	auto &cmd_data = cmd_list->get_user_data<::cmd_data>(::cmd_data::GUID);
	auto &descriptor_data = device->get_user_data<descriptor_set_tracking>(descriptor_set_tracking::GUID);
	assert((&descriptor_data) != nullptr);

	uint32_t param_count = 0;
	device->get_pipeline_layout_desc(layout, &param_count, nullptr);
	const auto params = static_cast<pipeline_layout_param *>(_malloca(param_count * sizeof(pipeline_layout_param)));
	device->get_pipeline_layout_desc(layout, &param_count, params);
	assert(first + count <= param_count);

	for (uint32_t i = 0; i < count; ++i)
	{
		assert(params[first + i].type == pipeline_layout_param_type::descriptor_set);
	
		uint32_t range_count = 0;
		device->get_descriptor_set_layout_desc(params[first + i].descriptor_layout, &range_count, nullptr);
		std::vector<descriptor_range> ranges(range_count);
		device->get_descriptor_set_layout_desc(params[first + i].descriptor_layout, &range_count, ranges.data());

		uint32_t base_offset = 0;
		descriptor_pool pool = { 0 };
		device->get_descriptor_pool_offset(sets[i], &pool, &base_offset);

		for (uint32_t k = 0; k < range_count; ++k)
		{
			const descriptor_range &range = ranges[k];
	
			if ((range.visibility & shader_stage::pixel) != shader_stage::pixel || (range.type != descriptor_type::shader_resource_view && range.type != descriptor_type::sampler_with_resource_view))
				continue;

			for (uint32_t j = 0; j < std::min(10u, range.count); ++j)
			{
				resource_view descriptor = descriptor_data.lookup_descriptor(pool, base_offset + range.offset + j);
				if (descriptor.handle == 0)
					continue;

				cmd_data.current_texture_list.emplace(descriptor);
			}
		}
	}

	_freea(params);
}

static void on_execute(command_queue *, command_list *cmd_list)
{
	device *const device = cmd_list->get_device();
	auto &data = device->get_user_data<device_data>(device_data::GUID);
	auto &cmd_data = cmd_list->get_user_data<::cmd_data>(::cmd_data::GUID);

	data.current_texture_list.insert(cmd_data.current_texture_list.begin(), cmd_data.current_texture_list.end());
	cmd_data.current_texture_list.clear();
}

static void on_present(command_queue *queue, swapchain *runtime)
{
	device *const device = runtime->get_device();

	if (device->get_api() != device_api::d3d12 && device->get_api() != device_api::vulkan)
		on_execute(queue, queue->get_immediate_command_list());

	auto &data = device->get_user_data<device_data>(device_data::GUID);

	data.frame_index++;

	std::lock_guard<std::mutex> lock(s_mutex);

	for (auto srv : data.current_texture_list)
	{
		if (std::find(data.destroyed_views.begin(), data.destroyed_views.end(), srv) != data.destroyed_views.end())
			continue;

		resource res = device->get_resource_from_view(srv);

		if (const auto it = data.total_texture_list.find(res);
			it != data.total_texture_list.end())
		{
			tex_data &tex_data = it->second;
			tex_data.last_view = srv;
			tex_data.last_used = data.frame_index;
		}
	}

	data.current_texture_list.clear();
	data.destroyed_views.clear();

	s_is_in_reshade_runtime = true;
}

// See implementation in 'texturemod_dump.cpp'
extern bool dump_texture(command_queue *queue, resource tex, const resource_desc &desc);

static void draw_overlay(effect_runtime *runtime, void *)
{
	assert(s_is_in_reshade_runtime);

	device *const device = runtime->get_device();
	auto &data = device->get_user_data<device_data>(device_data::GUID);

	ImGui::Checkbox("Show only used this frame", &data.filter);

	const bool save_all_textures = ImGui::Button("Save All", ImVec2(ImGui::GetWindowContentRegionWidth(), 0));

	ImGui::TextUnformatted("You can hover over a texture below with the mouse cursor to replace it with green.");
	ImGui::TextUnformatted("Clicking one will save it as an image to disk.");

	ImGui::PushItemWidth(ImGui::GetWindowContentRegionWidth());
	ImGui::SliderFloat("##scale", &data.scale, 0.01f, 2.0f, "%.3f", ImGuiSliderFlags_NoInput);
	ImGui::PopItemWidth();

	const auto total_width = ImGui::GetWindowContentRegionWidth();
	const auto num_columns = static_cast<unsigned int>(std::ceilf(total_width / (50.0f * data.scale * 13)));
	const auto single_image_max_size = (total_width / num_columns) - 5.0f;

	data.replaced_texture_srv = { 0 };

	std::lock_guard<std::mutex> lock(s_mutex);

	std::vector<std::pair<resource, tex_data>> filtered_texture_list;
	for (const auto &[tex, tex_data] : data.total_texture_list)
	{
		if (tex_data.last_view.handle == 0)
			continue;
		if (data.filter && tex_data.last_used != data.frame_index)
			continue;

		if (save_all_textures)
			dump_texture(runtime->get_command_queue(), tex, tex_data.desc);

		filtered_texture_list.emplace_back(tex, tex_data);
	}

	int texture_index = 0;

	for (size_t i = 0; i < filtered_texture_list.size(); ++i)
	{
		const tex_data &tex_data = filtered_texture_list[i].second;

		ImGui::PushID(texture_index);
		ImGui::BeginGroup();

		ImGui::Text("%ux%u", tex_data.desc.texture.width, tex_data.desc.texture.height);

		const float aspect_ratio = static_cast<float>(tex_data.desc.texture.width) / static_cast<float>(tex_data.desc.texture.height);

		const ImVec2 pos = ImGui::GetCursorScreenPos();
		const ImVec2 size = aspect_ratio > 1 ? ImVec2(single_image_max_size, single_image_max_size / aspect_ratio) : ImVec2(single_image_max_size * aspect_ratio, single_image_max_size);

		ImGui::Image(tex_data.last_view.handle, size, ImVec2(0, 0), ImVec2(1, 1), ImGui::IsMouseHoveringRect(pos, ImVec2(pos.x + size.x, pos.y + size.y)) ? ImColor(0.0f, 1.0f, 0.0f) : ImColor(1.0f, 1.0f, 1.0f));

		if (ImGui::IsItemHovered())
			data.replaced_texture_srv = tex_data.last_view;

		if (ImGui::IsItemClicked())
			dump_texture(runtime->get_command_queue(), filtered_texture_list[i].first, tex_data.desc);

		if (aspect_ratio < 1)
		{
			ImGui::SameLine(0.0f, 0.0f);
			ImGui::Dummy(ImVec2(single_image_max_size * (1 - aspect_ratio), single_image_max_size));
		}

		ImGui::EndGroup();
		ImGui::PopID();

		if ((texture_index++ % num_columns) != (num_columns - 1))
			ImGui::SameLine(0.0f, 5.0f);
		else
			ImGui::Spacing();
	}

	if ((texture_index % num_columns) != 0)
		ImGui::NewLine(); // Reset ImGui::SameLine() so the following starts on a new line
}

extern void register_addon_descriptor_set_tracking(); // See implementation in 'descriptor_set_tracking.cpp'
extern void unregister_addon_descriptor_set_tracking();

void register_addon_texmod_overlay()
{
	register_addon_descriptor_set_tracking();

	reshade::register_overlay("TexMod", draw_overlay);

	reshade::register_event<reshade::addon_event::init_device>(on_init_device);
	reshade::register_event<reshade::addon_event::destroy_device>(on_destroy_device);
	reshade::register_event<reshade::addon_event::init_command_list>(on_init_cmd_list);
	reshade::register_event<reshade::addon_event::destroy_command_list>(on_destroy_cmd_list);

	reshade::register_event<reshade::addon_event::init_resource>(on_init_texture);
	reshade::register_event<reshade::addon_event::destroy_resource>(on_destroy_texture);
	reshade::register_event<reshade::addon_event::destroy_resource_view>(on_destroy_texture_view);

	reshade::register_event<reshade::addon_event::push_descriptors>(on_push_descriptors);
	reshade::register_event<reshade::addon_event::bind_descriptor_sets>(on_bind_descriptor_sets);

	reshade::register_event<reshade::addon_event::execute_command_list>(on_execute);
	reshade::register_event<reshade::addon_event::present>(on_present);
}
void unregister_addon_texmod_overlay()
{
	unregister_addon_descriptor_set_tracking();

	reshade::unregister_overlay("TexMod", draw_overlay);

	reshade::unregister_event<reshade::addon_event::init_device>(on_init_device);
	reshade::unregister_event<reshade::addon_event::destroy_device>(on_destroy_device);
	reshade::unregister_event<reshade::addon_event::init_command_list>(on_init_cmd_list);
	reshade::unregister_event<reshade::addon_event::destroy_command_list>(on_destroy_cmd_list);

	reshade::unregister_event<reshade::addon_event::init_resource>(on_init_texture);
	reshade::unregister_event<reshade::addon_event::destroy_resource>(on_destroy_texture);
	reshade::unregister_event<reshade::addon_event::destroy_resource_view>(on_destroy_texture_view);

	reshade::unregister_event<reshade::addon_event::push_descriptors>(on_push_descriptors);
	reshade::unregister_event<reshade::addon_event::bind_descriptor_sets>(on_bind_descriptor_sets);

	reshade::unregister_event<reshade::addon_event::execute_command_list>(on_execute);
	reshade::unregister_event<reshade::addon_event::present>(on_present);
}
