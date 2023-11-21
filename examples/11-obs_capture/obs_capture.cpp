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
			shtex_data *shtex_info;
			reshade::api::resource texture;
			HANDLE handle;
		} shtex;
		/* shared memory */
		struct
		{
			reshade::api::resource copy_surfaces[NUM_BUFFERS];
			bool texture_ready[NUM_BUFFERS];
			bool texture_mapped[NUM_BUFFERS];
			uint32_t pitch;
			shmem_data *shmem_info;
			int cur_tex;
			int copy_wait;
		} shmem;
	};
} data;

static bool capture_impl_init(reshade::api::device *device, reshade::api::resource back_buffer, void *window)
{
	const reshade::api::resource_desc desc = device->get_resource_desc(back_buffer);
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
				&data.shtex.texture,
				&data.shtex.handle))
			return false;

		if (!capture_init_shtex(data.shtex.shtex_info, window, data.cx, data.cy, static_cast<uint32_t>(data.format), false, (uintptr_t)data.shtex.handle))
			return false;
	}
	else
	{
		data.using_shtex = false;

		for (int i = 0; i < NUM_BUFFERS; i++)
		{
			if (!device->create_resource(
					reshade::api::resource_desc(data.cx, data.cy, 1, 1, data.format, 1, reshade::api::memory_heap::gpu_to_cpu, reshade::api::resource_usage::copy_dest),
					nullptr,
					reshade::api::resource_usage::cpu_access,
					&data.shmem.copy_surfaces[i]))
				return false;
		}

		reshade::api::subresource_data mapped;
		if (device->map_texture_region(data.shmem.copy_surfaces[0], 0, nullptr, reshade::api::map_access::read_only, &mapped))
		{
			data.shmem.pitch = mapped.row_pitch;
			device->unmap_texture_region(data.shmem.copy_surfaces[0], 0);
		}

		if (!capture_init_shmem(data.shmem.shmem_info, window, data.cx, data.cy, data.shmem.pitch, static_cast<uint32_t>(data.format), false))
			return false;
	}

	return true;
}
static void capture_impl_free(reshade::api::device *device)
{
	capture_free();

	if (data.using_shtex)
	{
		device->destroy_resource(data.shtex.texture);
	}
	else
	{
		for (int i = 0; i < NUM_BUFFERS; i++)
		{
			if (data.shmem.copy_surfaces[i] == 0)
				continue;

			if (data.shmem.texture_mapped[i])
				device->unmap_texture_region(data.shmem.copy_surfaces[i], 0);

			device->destroy_resource(data.shmem.copy_surfaces[i]);
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

		cmd_list->resolve_texture_region(back_buffer, 0, nullptr, data.shtex.texture, 0, 0, 0, 0, data.format);

		cmd_list->barrier(back_buffer, reshade::api::resource_usage::resolve_source, reshade::api::resource_usage::present);
	}
	else
	{
		cmd_list->barrier(back_buffer, reshade::api::resource_usage::present, reshade::api::resource_usage::copy_source);

		cmd_list->copy_resource(back_buffer, data.shtex.texture);

		cmd_list->barrier(back_buffer, reshade::api::resource_usage::copy_source, reshade::api::resource_usage::present);
	}
}
static void capture_impl_shmem(reshade::api::command_queue *queue, reshade::api::resource back_buffer)
{
	reshade::api::device *device = queue->get_device();
	reshade::api::command_list *cmd_list = queue->get_immediate_command_list();

	int next_tex = (data.shmem.cur_tex + 1) % NUM_BUFFERS;

	if (data.shmem.texture_ready[next_tex])
	{
		data.shmem.texture_ready[next_tex] = false;

		reshade::api::subresource_data mapped;
		if (device->map_texture_region(data.shmem.copy_surfaces[next_tex], 0, nullptr, reshade::api::map_access::read_only, &mapped))
		{
			data.shmem.texture_mapped[next_tex] = true;
			shmem_copy_data(next_tex, mapped.data);
		}
	}

	if (data.shmem.copy_wait < NUM_BUFFERS - 1)
	{
		data.shmem.copy_wait++;
	}
	else
	{
		if (shmem_texture_data_lock(data.shmem.cur_tex))
		{
			device->unmap_texture_region(data.shmem.copy_surfaces[data.shmem.cur_tex], 0);
			data.shmem.texture_mapped[data.shmem.cur_tex] = false;
			shmem_texture_data_unlock(data.shmem.cur_tex);
		}

		if (data.multisampled)
		{
			cmd_list->barrier(back_buffer, reshade::api::resource_usage::present, reshade::api::resource_usage::resolve_source);
			cmd_list->barrier(data.shmem.copy_surfaces[data.shmem.cur_tex], reshade::api::resource_usage::cpu_access, reshade::api::resource_usage::resolve_dest);

			cmd_list->resolve_texture_region(back_buffer, 0, nullptr, data.shmem.copy_surfaces[data.shmem.cur_tex], 0, 0, 0, 0, static_cast<reshade::api::format>(data.format));

			cmd_list->barrier(data.shmem.copy_surfaces[data.shmem.cur_tex], reshade::api::resource_usage::resolve_dest, reshade::api::resource_usage::cpu_access);
			cmd_list->barrier(back_buffer, reshade::api::resource_usage::resolve_source, reshade::api::resource_usage::present);
		}
		else
		{
			cmd_list->barrier(back_buffer, reshade::api::resource_usage::present, reshade::api::resource_usage::copy_source);
			cmd_list->barrier(data.shmem.copy_surfaces[data.shmem.cur_tex], reshade::api::resource_usage::cpu_access, reshade::api::resource_usage::copy_dest);

			cmd_list->copy_resource(back_buffer, data.shmem.copy_surfaces[data.shmem.cur_tex]);

			cmd_list->barrier(data.shmem.copy_surfaces[data.shmem.cur_tex], reshade::api::resource_usage::copy_dest, reshade::api::resource_usage::cpu_access);
			cmd_list->barrier(back_buffer, reshade::api::resource_usage::copy_source, reshade::api::resource_usage::present);
		}

		data.shmem.texture_ready[data.shmem.cur_tex] = true;
	}

	data.shmem.cur_tex = next_tex;
}

static void on_present(reshade::api::effect_runtime *runtime)
{
	if (global_hook_info == nullptr)
		return;

	const reshade::api::resource back_buffer = runtime->get_current_back_buffer();

	if (capture_should_stop())
		capture_impl_free(runtime->get_device());

	if (capture_should_init())
		capture_impl_init(runtime->get_device(), back_buffer, runtime->get_hwnd());

	if (capture_ready())
	{
		if (data.using_shtex)
			capture_impl_shtex(runtime->get_command_queue(), back_buffer);
		else
			capture_impl_shmem(runtime->get_command_queue(), back_buffer);
	}
}

static void on_destroy(reshade::api::effect_runtime *runtime)
{
	if (global_hook_info == nullptr)
		return;

	capture_impl_free(runtime->get_device());
}

extern "C" __declspec(dllexport) const char *NAME = "OBS Capture";
extern "C" __declspec(dllexport) const char *DESCRIPTION = "An OBS capture driver which overrides the one OBS ships with to be able to give more control over where in the frame to send images to OBS.";

extern "C" __declspec(dllexport) bool AddonInit(HMODULE addon_module, HMODULE reshade_module)
{
	if (!reshade::register_addon(addon_module, reshade_module))
		return false;

	if (!hook_init())
	{
		reshade::unregister_addon(addon_module, reshade_module);
		return false;
	}

	// Change this event to e.g. 'reshade_begin_effects' to send images to OBS before ReShade effects are applied, or 'reshade_render_technique' to send after a specific technique.
	reshade::register_event<reshade::addon_event::reshade_present>(on_present);
	reshade::register_event<reshade::addon_event::destroy_effect_runtime>(on_destroy);

	return true;
}
extern "C" __declspec(dllexport) void AddonUninit(HMODULE addon_module, HMODULE reshade_module)
{
	hook_free();

	reshade::unregister_addon(addon_module, reshade_module);
}
