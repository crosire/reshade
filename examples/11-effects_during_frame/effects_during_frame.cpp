/*
 * Copyright (C) 2022 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause OR MIT
 */

#include <imgui.h>
#include <reshade.hpp>

using namespace reshade::api;

struct __declspec(uuid("7251932A-ADAF-4DFC-B5CB-9A4E8CD5D6EB")) device_data
{
	effect_runtime *main_runtime = nullptr;
	uint32_t offset_from_last_pass = 0;
	uint32_t last_render_pass_count = 1;
	uint32_t current_render_pass_count = 0;
};
struct __declspec(uuid("036CD16B-E823-4D6C-A137-5C335D6FD3E6")) command_list_data
{
	bool has_multiple_rtvs = false;
	resource_view current_main_rtv = { 0 };
	uint32_t current_render_pass_index = 0;
};

static void on_init_device(device *device)
{
	device->create_private_data<device_data>();
}
static void on_destroy_device(device *device)
{
	device->destroy_private_data<device_data>();
}

static void on_init_command_list(command_list *cmd_list)
{
	cmd_list->create_private_data<command_list_data>();
}
static void on_destroy_command_list(command_list *cmd_list)
{
	cmd_list->destroy_private_data<command_list_data>();
}

static void on_init_effect_runtime(effect_runtime *runtime)
{
	auto &dev_data = runtime->get_device()->get_private_data<device_data>();
	// Assume last created effect runtime is the main one
	dev_data.main_runtime = runtime;
}
static void on_destroy_effect_runtime(effect_runtime *runtime)
{
	auto &dev_data = runtime->get_device()->get_private_data<device_data>();
	if (runtime == dev_data.main_runtime)
		dev_data.main_runtime = nullptr;
}

// Called after game has rendered a render pass, so check if it makes to render effects then (e.g. after main scene rendering, before UI rendering)
static void on_end_render_pass(command_list *cmd_list)
{
	auto &data = cmd_list->get_private_data<command_list_data>();

	if (data.has_multiple_rtvs || data.current_main_rtv == 0)
		return; // Ignore when game is rendering to multiple render targets simultaneously

	device *const device = cmd_list->get_device();
	const auto &dev_data = device->get_private_data<device_data>();

	uint32_t width, height;
	dev_data.main_runtime->get_screenshot_width_and_height(&width, &height);

	const resource_desc render_target_desc = device->get_resource_desc(device->get_resource_from_view(data.current_main_rtv));

	if (render_target_desc.texture.width != width || render_target_desc.texture.height != height)
		return; // Ignore render targets that do not match the effect runtime back buffer dimensions

	// Render post-processing effects when a specific render pass is found (instead of at the end of the frame)
	// This is not perfect, since there may be multiple command lists at this will try and render effects in every single one ...
	if (data.current_render_pass_index++ == (dev_data.last_render_pass_count - dev_data.offset_from_last_pass))
		dev_data.main_runtime->render_effects(cmd_list, data.current_main_rtv);
}

static void on_begin_render_pass(command_list *cmd_list, uint32_t count, const render_pass_render_target_desc *rts, const render_pass_depth_stencil_desc *)
{
	auto &data = cmd_list->get_private_data<command_list_data>();
	data.has_multiple_rtvs = count > 1;
	data.current_main_rtv = (count != 0) ? rts[0].view : resource_view{ 0 };
}
static void on_bind_render_targets_and_depth_stencil(command_list *cmd_list, uint32_t count, const resource_view *rtvs, resource_view)
{
	auto &data = cmd_list->get_private_data<command_list_data>();

	const resource_view new_main_rtv = (count != 0) ? rtvs[0] : resource_view { 0 };
	if (new_main_rtv != data.current_main_rtv)
		on_end_render_pass(cmd_list);

	data.has_multiple_rtvs = count > 1;
	data.current_main_rtv = new_main_rtv;
}

static void on_execute(command_queue *queue, command_list *cmd_list)
{
	auto &dev_data = queue->get_device()->get_private_data<device_data>();
	const auto &data = cmd_list->get_private_data<command_list_data>();

	// Now that this command list is executed, add its render pass count to the total of the current frame
	dev_data.current_render_pass_count += data.current_render_pass_index + 1;
}
static void on_reset_command_list(command_list *cmd_list)
{
	auto &data = cmd_list->get_private_data<command_list_data>();
	data.current_render_pass_index = 0;
}

static void on_present(effect_runtime *runtime)
{
	device *const device = runtime->get_device();
	auto &dev_data = device->get_private_data<device_data>();

	if (runtime != dev_data.main_runtime)
		return;

	command_list *const immediate_cmd_list = runtime->get_command_queue()->get_immediate_command_list();
	on_execute(runtime->get_command_queue(), immediate_cmd_list);
	on_reset_command_list(immediate_cmd_list);

	// Keep track of the total render pass count of this frame
	dev_data.last_render_pass_count = dev_data.current_render_pass_count > 0 ? dev_data.current_render_pass_count : 1;
	dev_data.current_render_pass_count = 0;

	if (dev_data.offset_from_last_pass > dev_data.last_render_pass_count)
		dev_data.offset_from_last_pass = dev_data.last_render_pass_count;
}

static void on_draw_settings(effect_runtime *runtime)
{
	device *const device = runtime->get_device();
	auto &dev_data = device->get_private_data<device_data>();

	if (runtime != dev_data.main_runtime)
	{
		ImGui::TextUnformatted("This is not the main effect runtime.");
		return;
	}

	ImGui::Text("Current render pass count: %u", dev_data.last_render_pass_count);
	ImGui::Text("Offset from end of frame to render effects at: %u", dev_data.offset_from_last_pass);

	if (dev_data.offset_from_last_pass < dev_data.last_render_pass_count)
	{
		ImGui::SameLine();
		if (ImGui::SmallButton("+"))
			dev_data.offset_from_last_pass++;
	}

	if (dev_data.offset_from_last_pass != 0)
	{
		ImGui::SameLine();
		if (ImGui::SmallButton("-"))
			dev_data.offset_from_last_pass--;
	}
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD fdwReason, LPVOID)
{
	switch (fdwReason)
	{
	case DLL_PROCESS_ATTACH:
		if (!reshade::register_addon(hModule))
			return FALSE;
		reshade::register_event<reshade::addon_event::init_device>(on_init_device);
		reshade::register_event<reshade::addon_event::destroy_device>(on_destroy_device);
		reshade::register_event<reshade::addon_event::init_command_list>(on_init_command_list);
		reshade::register_event<reshade::addon_event::destroy_command_list>(on_destroy_command_list);
		reshade::register_event<reshade::addon_event::init_effect_runtime>(on_init_effect_runtime);
		reshade::register_event<reshade::addon_event::destroy_effect_runtime>(on_destroy_effect_runtime);
		reshade::register_event<reshade::addon_event::end_render_pass>(on_end_render_pass);
		reshade::register_event<reshade::addon_event::begin_render_pass>(on_begin_render_pass);
		reshade::register_event<reshade::addon_event::bind_render_targets_and_depth_stencil>(on_bind_render_targets_and_depth_stencil);
		reshade::register_event<reshade::addon_event::execute_command_list>(on_execute);
		reshade::register_event<reshade::addon_event::reset_command_list>(on_reset_command_list);
		reshade::register_event<reshade::addon_event::reshade_present>(on_present);
		reshade::register_overlay(nullptr, on_draw_settings);
		break;
	case DLL_PROCESS_DETACH:
		reshade::unregister_addon(hModule);
		break;
	}

	return TRUE;
}
