/*
 * Copyright (C) 2014 Hugh Bailey
 * Copyright (C) 2022 Patrick Mours
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Adapted from https://github.com/obsproject/obs-studio/blob/master/plugins/win-capture/graphics-hook/d3d11-capture.cpp
 */

#include <reshade.hpp>
#include "obs_hook_info.hpp"

struct capture_data
{
	uint32_t cx;
	uint32_t cy;
	reshade::api::format format;
	bool using_shtex;
	bool multisampled;

	union
	{
		/* shared texture */
		struct
		{
			struct shtex_data *shtex_info;
			reshade::api::resource texture;
			HANDLE handle;
		};
		/* shared memory */
		struct
		{
			reshade::api::resource copy_surfaces[NUM_BUFFERS];
			bool texture_ready[NUM_BUFFERS];
			bool texture_mapped[NUM_BUFFERS];
			uint32_t pitch;
			struct shmem_data *shmem_info;
			int cur_tex;
			int copy_wait;
		};
	};
} data;

static bool capture_impl_init(reshade::api::swapchain *swapchain)
{
	reshade::api::device *const device = swapchain->get_device();

	const reshade::api::resource_desc desc = device->get_resource_desc(swapchain->get_current_back_buffer());
	data.format = reshade::api::format_to_default_typed(desc.texture.format, 0);
	data.multisampled = desc.texture.samples > 1;
	data.cx = desc.texture.width;
	data.cy = desc.texture.height;

	if (device->get_api() != reshade::api::device_api::opengl && !global_hook_info->force_shmem)
	{
		data.using_shtex = true;

		if (!device->create_resource(
				reshade::api::resource_desc(data.cx, data.cy, 1, 1, data.format, 1, reshade::api::memory_heap::gpu_only, reshade::api::resource_usage::shader_resource | reshade::api::resource_usage::copy_dest, reshade::api::resource_flags::shared),
				nullptr,
				reshade::api::resource_usage::copy_dest,
				&data.texture,
				&data.handle))
			return false;

		if (!capture_init_shtex(&data.shtex_info, swapchain->get_hwnd(), data.cx, data.cy, static_cast<uint32_t>(data.format), false, (uintptr_t)data.handle))
			return false;
	}
	else
	{
		data.using_shtex = false;

		for (size_t i = 0; i < NUM_BUFFERS; i++)
		{
			if (!device->create_resource(
					reshade::api::resource_desc(data.cx, data.cy, 1, 1, data.format, 1, reshade::api::memory_heap::gpu_to_cpu, reshade::api::resource_usage::copy_dest),
					nullptr,
					reshade::api::resource_usage::cpu_access,
					&data.copy_surfaces[i]))
				return false;
		}

		reshade::api::subresource_data mapped;
		if (device->map_texture_region(data.copy_surfaces[0], 0, nullptr, reshade::api::map_access::read_only, &mapped))
		{
			data.pitch = mapped.row_pitch;
			device->unmap_texture_region(data.copy_surfaces[0], 0);
		}

		if (!capture_init_shmem(&data.shmem_info, swapchain->get_hwnd(), data.cx, data.cy, data.pitch, static_cast<uint32_t>(data.format), false))
			return false;
	}

	return true;
}
static void capture_impl_free(reshade::api::swapchain *swapchain)
{
	reshade::api::device *const device = swapchain->get_device();

	capture_free();

	if (data.using_shtex)
	{
		device->destroy_resource(data.texture);
	}
	else
	{
		for (size_t i = 0; i < NUM_BUFFERS; i++)
		{
			if (data.copy_surfaces[i] == 0)
				continue;

			if (data.texture_mapped[i])
				device->unmap_texture_region(data.copy_surfaces[i], 0);

			device->destroy_resource(data.copy_surfaces[i]);
		}
	}

	memset(&data, 0, sizeof(data));
}

static void capture_impl_shtex(reshade::api::command_queue *queue, reshade::api::resource back_buffer)
{
	reshade::api::command_list *cmd_list = queue->get_immediate_command_list();

	if (data.multisampled)
	{
		cmd_list->barrier(back_buffer, reshade::api::resource_usage::present, reshade::api::resource_usage::resolve_source);

		cmd_list->resolve_texture_region(back_buffer, 0, nullptr, data.texture, 0, 0, 0, 0, data.format);

		cmd_list->barrier(back_buffer, reshade::api::resource_usage::resolve_source, reshade::api::resource_usage::present);
	}
	else
	{
		cmd_list->barrier(back_buffer, reshade::api::resource_usage::present, reshade::api::resource_usage::copy_source);

		cmd_list->copy_resource(back_buffer, data.texture);

		cmd_list->barrier(back_buffer, reshade::api::resource_usage::copy_source, reshade::api::resource_usage::present);
	}
}
static void capture_impl_shmem(reshade::api::command_queue *queue, reshade::api::resource back_buffer)
{
	reshade::api::device *device = queue->get_device();
	reshade::api::command_list *cmd_list = queue->get_immediate_command_list();

	int next_tex = (data.cur_tex + 1) % NUM_BUFFERS;

	if (data.texture_ready[next_tex])
	{
		data.texture_ready[next_tex] = false;

		reshade::api::subresource_data mapped;
		if (device->map_texture_region(data.copy_surfaces[next_tex], 0, nullptr, reshade::api::map_access::read_only, &mapped))
		{
			data.texture_mapped[next_tex] = true;
			shmem_copy_data(next_tex, mapped.data);
		}
	}

	if (data.copy_wait < NUM_BUFFERS - 1)
	{
		data.copy_wait++;
	}
	else
	{
		if (shmem_texture_data_lock(data.cur_tex))
		{
			device->unmap_texture_region(data.copy_surfaces[data.cur_tex], 0);
			data.texture_mapped[data.cur_tex] = false;
			shmem_texture_data_unlock(data.cur_tex);
		}

		if (data.multisampled)
		{
			cmd_list->barrier(back_buffer, reshade::api::resource_usage::present, reshade::api::resource_usage::resolve_source);
			cmd_list->barrier(data.copy_surfaces[data.cur_tex], reshade::api::resource_usage::cpu_access, reshade::api::resource_usage::resolve_dest);

			cmd_list->resolve_texture_region(back_buffer, 0, nullptr, data.copy_surfaces[data.cur_tex], 0, 0, 0, 0, static_cast<reshade::api::format>(data.format));

			cmd_list->barrier(data.copy_surfaces[data.cur_tex], reshade::api::resource_usage::resolve_dest, reshade::api::resource_usage::cpu_access);
			cmd_list->barrier(back_buffer, reshade::api::resource_usage::resolve_source, reshade::api::resource_usage::present);
		}
		else
		{
			cmd_list->barrier(back_buffer, reshade::api::resource_usage::present, reshade::api::resource_usage::copy_source);
			cmd_list->barrier(data.copy_surfaces[data.cur_tex], reshade::api::resource_usage::cpu_access, reshade::api::resource_usage::copy_dest);

			cmd_list->copy_resource(back_buffer, data.copy_surfaces[data.cur_tex]);

			cmd_list->barrier(data.copy_surfaces[data.cur_tex], reshade::api::resource_usage::copy_dest, reshade::api::resource_usage::cpu_access);
			cmd_list->barrier(back_buffer, reshade::api::resource_usage::copy_source, reshade::api::resource_usage::present);
		}

		data.texture_ready[data.cur_tex] = true;
	}

	data.cur_tex = next_tex;
}

static void on_present(reshade::api::effect_runtime *swapchain)
{
	if (global_hook_info == nullptr)
		return;

	if (capture_should_stop())
		capture_impl_free(swapchain);

	if (capture_should_init())
		capture_impl_init(swapchain);

	if (capture_ready())
	{
		reshade::api::resource back_buffer = swapchain->get_current_back_buffer();

		if (data.using_shtex)
			capture_impl_shtex(swapchain->get_command_queue(), back_buffer);
		else
			capture_impl_shmem(swapchain->get_command_queue(), back_buffer);
	}
}

static void on_destroy_swapchain(reshade::api::swapchain *swapchain)
{
	if (global_hook_info == nullptr)
		return;

	capture_impl_free(swapchain);
}

extern "C" __declspec(dllexport) const char *NAME = "OBS Capture";
extern "C" __declspec(dllexport) const char *DESCRIPTION = "Simple OBS capture driver which replaces the one OBS ships with to give more control over where in the frame to send images to OBS.";

BOOL APIENTRY DllMain(HMODULE hModule, DWORD fdwReason, LPVOID)
{
	switch (fdwReason)
	{
	case DLL_PROCESS_ATTACH:
		if (!hook_init())
			return FALSE;
		if (!reshade::register_addon(hModule))
			return FALSE;
		reshade::register_event<reshade::addon_event::reshade_present>(on_present);
		reshade::register_event<reshade::addon_event::destroy_swapchain>(on_destroy_swapchain);
		break;
	case DLL_PROCESS_DETACH:
		reshade::unregister_addon(hModule);
		hook_free();
		break;
	}

	return TRUE;
}
