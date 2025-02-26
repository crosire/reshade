/*
 * Copyright (C) 2022 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause OR MIT
 */

#include <imgui.h>
#include <reshade.hpp>
#include "state_tracking.hpp"

using namespace reshade::api;

static bool s_filter_width_and_height = true;

struct __declspec(uuid("7251932A-ADAF-4DFC-B5CB-9A4E8CD5D6EB")) device_data
{
	effect_runtime *main_runtime = nullptr;
	uint32_t offset_from_last_pass = 0;
	uint32_t last_render_pass_count = std::numeric_limits<uint32_t>::max();
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
	const auto data = runtime->get_device()->get_private_data<device_data>();
	if (data == nullptr)
		return;

	// Assume last created effect runtime is the main one
	data->main_runtime = runtime;
}
static void on_destroy_effect_runtime(effect_runtime *runtime)
{
	const auto data = runtime->get_device()->get_private_data<device_data>();
	if (data == nullptr)
		return;

	if (runtime == data->main_runtime)
		data->main_runtime = nullptr;
}

// Called after game has rendered a render pass, so check if it makes sense to render effects then (e.g. after main scene rendering, before UI rendering)
static void on_end_render_pass(command_list *cmd_list)
{
	auto &cmd_data = *cmd_list->get_private_data<command_list_data>();

	if (cmd_data.has_multiple_rtvs || cmd_data.current_main_rtv == 0)
		return; // Ignore when game is rendering to multiple render targets simultaneously

	device *const device = cmd_list->get_device();
	const auto data = device->get_private_data<device_data>();

	// Optionally ignore render targets that do not match the effect runtime back buffer dimensions
	if (s_filter_width_and_height)
	{
		uint32_t width, height;
		data->main_runtime->get_screenshot_width_and_height(&width, &height);

		const resource_desc render_target_desc = device->get_resource_desc(device->get_resource_from_view(cmd_data.current_main_rtv));

		if (render_target_desc.texture.width != width || render_target_desc.texture.height != height)
			return;
	}

	// Render post-processing effects when a specific render pass is found (instead of at the end of the frame)
	// This is not perfect, since there may be multiple command lists, so a global index is not conclusive and this may cause effects to be rendered multiple times ...
	if (cmd_data.current_render_pass_index++ == (data->last_render_pass_count - data->offset_from_last_pass))
	{
		const state_tracking *const current_state = cmd_list->get_private_data<state_tracking>();

		// This does not handle sRGB correctly, since it would need to pass in separate render target views created with a non-sRGB and a sRGB format variant of the target resource
		// For simplicity and demonstration purposes the same render target view (which can be either non-sRGB or sRGB, depending on what the application created it with) for both cases is passed in here, but this should be fixed in a proper implementation
		data->main_runtime->render_effects(cmd_list, cmd_data.current_main_rtv, cmd_data.current_main_rtv);

		// Re-apply state to the command list, as it may have been modified by the call to 'render_effects'
		current_state->apply(cmd_list);
	}
}

static void on_begin_render_pass(command_list *cmd_list, uint32_t count, const render_pass_render_target_desc *rts, const render_pass_depth_stencil_desc *)
{
	auto &cmd_data = *cmd_list->get_private_data<command_list_data>();
	cmd_data.has_multiple_rtvs = count > 1;
	cmd_data.current_main_rtv = (count != 0) ? rts[0].view : resource_view { 0 };
}
static void on_bind_render_targets_and_depth_stencil(command_list *cmd_list, uint32_t count, const resource_view *rtvs, resource_view)
{
	auto &cmd_data = *cmd_list->get_private_data<command_list_data>();

	const resource_view new_main_rtv = (count != 0) ? rtvs[0] : resource_view { 0 };
	if (new_main_rtv != cmd_data.current_main_rtv)
		on_end_render_pass(cmd_list);

	cmd_data.has_multiple_rtvs = count > 1;
	cmd_data.current_main_rtv = new_main_rtv;
}

static void on_execute(command_queue *queue, command_list *cmd_list)
{
	const auto &cmd_data = *cmd_list->get_private_data<command_list_data>();
	device_data *const data = queue->get_device()->get_private_data<device_data>();

	// Now that this command list is executed, add its render pass count to the total of the current frame
	data->current_render_pass_count += cmd_data.current_render_pass_index + 1;
}
static void on_reset_command_list(command_list *cmd_list)
{
	auto &cmd_data = *cmd_list->get_private_data<command_list_data>();
	cmd_data.current_render_pass_index = 0;
}

static void on_present(effect_runtime *runtime)
{
	device *const device = runtime->get_device();
	const auto data = device->get_private_data<device_data>();

	if (data == nullptr || runtime != data->main_runtime)
		return;

	// The 'reset_command_list' event is not called for immediate command lists, so call it manually here in D3D9/10/11/OpenGL to reset the current render pass index every frame
	if (device->get_api() != device_api::d3d12 && device->get_api() != device_api::vulkan)
		on_reset_command_list(runtime->get_command_queue()->get_immediate_command_list());

	// Keep track of the total render pass count of this frame
	data->last_render_pass_count = data->current_render_pass_count > 0 ? data->current_render_pass_count : std::numeric_limits<uint32_t>::max();
	data->current_render_pass_count = 0;

	if (data->offset_from_last_pass > data->last_render_pass_count)
		data->offset_from_last_pass = data->last_render_pass_count;
}

static void on_draw_settings(effect_runtime *runtime)
{
	device *const device = runtime->get_device();
	const auto data = device->get_private_data<device_data>();

	if (data == nullptr || runtime != data->main_runtime)
	{
		ImGui::TextUnformatted("This is not the main effect runtime.");
		return;
	}

	ImGui::Checkbox("Filter out render passes with different dimensions", &s_filter_width_and_height);

	ImGui::Text("Current render pass count: %u", data->last_render_pass_count);
	ImGui::Text("Offset from end of frame to render effects at: %u", data->offset_from_last_pass);

	if (data->offset_from_last_pass < data->last_render_pass_count)
	{
		ImGui::SameLine();
		if (ImGui::SmallButton("+"))
			data->offset_from_last_pass++;
	}

	if (data->offset_from_last_pass != 0)
	{
		ImGui::SameLine();
		if (ImGui::SmallButton("-"))
			data->offset_from_last_pass--;
	}
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD fdwReason, LPVOID)
{
	switch (fdwReason)
	{
	case DLL_PROCESS_ATTACH:
		if (!reshade::register_addon(hModule))
			return FALSE;

		// Need to register events for state tracking before the rest, so that the state block of a command list is up-to-date by the time any of the below callback functions are called
		state_tracking::register_events();

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
		state_tracking::unregister_events();

		reshade::unregister_addon(hModule);
		break;
	}

	return TRUE;
}
