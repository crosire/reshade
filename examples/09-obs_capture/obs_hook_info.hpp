/*
 * Adapted from https://github.com/obsproject/obs-studio/blob/master/plugins/win-capture/graphics-hook/graphics-hook.h
 *
 * Copyright (C) 2014 Hugh Bailey
 * Copyright (C) 2022 Patrick Mours
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <cstdint>

#define HOOK_VER_MAJOR 1
#define HOOK_VER_MINOR 7
#define HOOK_VER_PATCH 1

#define NUM_BUFFERS 3

#define EVENT_CAPTURE_RESTART L"CaptureHook_Restart"
#define EVENT_CAPTURE_STOP L"CaptureHook_Stop"

#define EVENT_HOOK_READY L"CaptureHook_HookReady"
#define EVENT_HOOK_EXIT L"CaptureHook_Exit"

#define EVENT_HOOK_INIT L"CaptureHook_Initialize"

#define WINDOW_HOOK_KEEPALIVE L"CaptureHook_KeepAlive"

#define MUTEX_TEXTURE1 L"CaptureHook_TextureMutex1"
#define MUTEX_TEXTURE2 L"CaptureHook_TextureMutex2"

#define SHMEM_HOOK_INFO L"CaptureHook_HookInfo"
#define SHMEM_TEXTURE L"CaptureHook_Texture"

#pragma pack(push, 8)

enum capture_type
{
	CAPTURE_TYPE_MEMORY,
	CAPTURE_TYPE_TEXTURE,
};

struct hook_info
{
	/* hook version */
	uint32_t hook_ver_major;
	uint32_t hook_ver_minor;

	/* capture info */
	capture_type type;
	uint32_t window;
	uint32_t format;
	uint32_t cx;
	uint32_t cy;
	uint32_t UNUSED_base_cx;
	uint32_t UNUSED_base_cy;
	uint32_t pitch;
	uint32_t map_id;
	uint32_t map_size;
	bool flip;

	/* additional options */
	uint64_t frame_interval;
	bool UNUSED_use_scale;
	bool force_shmem;
	bool capture_overlay;
	bool allow_srgb_alias;

	uint8_t reserved[574];
};
static_assert(sizeof(hook_info) == 648);

struct shmem_data
{
	volatile int last_tex;
	uint32_t tex1_offset;
	uint32_t tex2_offset;
};

struct shtex_data
{
	uint32_t tex_handle;
};

#pragma pack(pop)

extern hook_info *global_hook_info;

extern void shmem_copy_data(size_t idx, void *volatile data);
extern bool shmem_texture_data_lock(int idx);
extern void shmem_texture_data_unlock(int idx);

bool hook_init();
void hook_free();

bool capture_init_shtex(shtex_data **data, void *window, uint32_t cx, uint32_t cy, uint32_t format, bool flip, uintptr_t handle);
bool capture_init_shmem(shmem_data **data, void *window, uint32_t cx, uint32_t cy, uint32_t pitch, uint32_t format, bool flip);
void capture_free();

bool capture_ready();
bool capture_alive();
bool capture_active();
bool capture_stopped();
bool capture_restarted();

bool capture_signal_ready();
bool capture_signal_restart();

inline bool capture_should_stop()
{
	return capture_active() && capture_stopped() && !capture_alive();
}
inline bool capture_should_init()
{
	return !capture_active() && capture_restarted() && capture_alive();
}
