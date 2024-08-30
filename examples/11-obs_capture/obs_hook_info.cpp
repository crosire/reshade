/*
 * Copyright (C) 2014 Hugh Bailey
 * Copyright (C) 2022 Patrick Mours
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Adapted from https://github.com/obsproject/obs-studio/blob/master/plugins/win-capture/graphics-hook/graphics-hook.c
 */

#include "obs_hook_info.hpp"
#include <chrono>
#include <Windows.h>

namespace
{
	struct thread_data
	{
		CRITICAL_SECTION mutexes[NUM_BUFFERS];
		CRITICAL_SECTION data_mutex;
		void *volatile cur_data;
		uint8_t *shmem_textures[2];
		HANDLE copy_thread;
		HANDLE copy_event;
		HANDLE stop_event;
		volatile int cur_tex;
		unsigned int pitch;
		unsigned int cy;
		volatile bool locked_textures[NUM_BUFFERS];
	} thread_data = {};

	HANDLE signal_init = NULL, signal_ready = NULL, signal_exit = NULL;
	HANDLE signal_restart = NULL, signal_stop = NULL;
	HANDLE tex_mutexes[2] = { NULL, NULL };
	HANDLE filemap_hook_info = NULL, dup_hook_mutex = NULL;

	unsigned int shmem_id_counter = 0;
	void *shmem_info = nullptr;
	HANDLE shmem_file_handle = 0;

	volatile bool active = false;

	bool create_event(HANDLE &handle, LPCWSTR name, DWORD pid)
	{
		wchar_t name_with_pid[64];
		swprintf_s(name_with_pid, L"%s%lu", name, pid);

		return (handle = CreateEventW(nullptr, FALSE, FALSE, name_with_pid)) != nullptr;
	}
	void destroy_event(HANDLE &handle)
	{
		if (handle)
		{
			CloseHandle(handle);
			handle = nullptr;
		}
	}

	bool open_mutex(HANDLE &handle, LPCWSTR name, DWORD pid)
	{
		wchar_t name_with_pid[64];
		swprintf_s(name_with_pid, L"%s%lu", name, pid);

		return (handle = OpenMutexW(SYNCHRONIZE, FALSE, name_with_pid)) != nullptr;
	}
	bool create_mutex(HANDLE &handle, LPCWSTR name, DWORD pid)
	{
		wchar_t name_with_pid[64];
		swprintf_s(name_with_pid, L"%s%lu", name, pid);

		return (handle = CreateMutexW(nullptr, FALSE, name_with_pid)) != nullptr;
	}
	void destroy_mutex(HANDLE &handle)
	{
		if (handle)
		{
			CloseHandle(handle);
			handle = nullptr;
		}
	}

	bool create_file_mapping(HANDLE &handle, LPVOID &data, DWORD size, LPCWSTR format, ...)
	{
		va_list args; va_start(args, format);
		wchar_t name[64];
		_vsnwprintf_s(name, ARRAYSIZE(name), format, args); va_end(args);

		handle = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, size, name);
		if (handle != nullptr)
		{
			data = MapViewOfFile(handle, FILE_MAP_ALL_ACCESS, 0, 0, size);
			if (data != nullptr)
				return true;

			CloseHandle(handle);
		}

		return false;
	}
	void destroy_file_mapping(HANDLE &handle, LPVOID &data)
	{
		if (data)
		{
			UnmapViewOfFile(data);
			data = nullptr;
		}

		if (handle)
		{
			CloseHandle(handle);
			handle = nullptr;
		}
	}
}

hook_info *global_hook_info = nullptr;

bool hook_init()
{
	const DWORD pid = GetCurrentProcessId();

	if (open_mutex(dup_hook_mutex, L"graphics_hook_dup_mutex", pid))
	{
		CloseHandle(dup_hook_mutex);
		dup_hook_mutex = nullptr;
		return false;
	}

	if (!create_mutex(dup_hook_mutex, L"graphics_hook_dup_mutex", pid))
		return false;

	if (!create_event(signal_init, EVENT_HOOK_INIT, pid))
		return false;
	if (!create_event(signal_exit, EVENT_HOOK_EXIT, pid))
		return false;

	if (!create_event(signal_restart, EVENT_CAPTURE_RESTART, pid))
		return false;
	if (!create_event(signal_stop, EVENT_CAPTURE_STOP, pid))
		return false;
	if (!create_event(signal_ready, EVENT_HOOK_READY, pid))
		return false;

	if (!create_mutex(tex_mutexes[0], MUTEX_TEXTURE1, pid))
		return false;
	if (!create_mutex(tex_mutexes[1], MUTEX_TEXTURE2, pid))
		return false;

	if (!create_file_mapping(filemap_hook_info, reinterpret_cast<void *&>(global_hook_info), sizeof(hook_info), SHMEM_HOOK_INFO L"%lu", pid))
		return false;

	capture_signal_restart();

	return true;
}

void hook_free()
{
	if (!dup_hook_mutex)
		return;

	destroy_file_mapping(filemap_hook_info, reinterpret_cast<void *&>(global_hook_info));

	destroy_mutex(tex_mutexes[1]);
	destroy_mutex(tex_mutexes[0]);

	destroy_event(signal_ready);
	destroy_event(signal_stop);
	destroy_event(signal_restart);
	destroy_event(signal_exit);
	destroy_event(signal_init);

	destroy_mutex(dup_hook_mutex);
}

static DWORD CALLBACK copy_thread(LPVOID)
{
	const size_t pitch = thread_data.pitch, cy = thread_data.cy;
	HANDLE events[2] = { nullptr, nullptr };
	int shmem_id = 0;

	if (DuplicateHandle(GetCurrentProcess(), thread_data.copy_event, GetCurrentProcess(), &events[0], 0, false, DUPLICATE_SAME_ACCESS) &&
		DuplicateHandle(GetCurrentProcess(), thread_data.stop_event, GetCurrentProcess(), &events[1], 0, false, DUPLICATE_SAME_ACCESS))
	{
		while (WaitForMultipleObjects(2, events, false, INFINITE) == WAIT_OBJECT_0)
		{
			EnterCriticalSection(&thread_data.data_mutex);
			const auto cur_tex = thread_data.cur_tex;
			const auto cur_data = thread_data.cur_data;
			LeaveCriticalSection(&thread_data.data_mutex);

			if (cur_tex < NUM_BUFFERS && !!cur_data) {
				EnterCriticalSection(&thread_data.mutexes[cur_tex]);

				int lock_id = -1;
				const int next_id = shmem_id == 0 ? 1 : 0;

				DWORD wait_result = wait_result = WaitForSingleObject(tex_mutexes[shmem_id], 0);
				if (wait_result == WAIT_OBJECT_0 || wait_result == WAIT_ABANDONED)
				{
					lock_id = shmem_id;
				}
				else
				{
					wait_result = WaitForSingleObject(tex_mutexes[next_id], 0);
					if (wait_result == WAIT_OBJECT_0 || wait_result == WAIT_ABANDONED)
					{
						lock_id = next_id;
					}
				}

				if (lock_id != -1)
				{
					memcpy(thread_data.shmem_textures[lock_id], cur_data, pitch * cy);

					ReleaseMutex(tex_mutexes[lock_id]);

					static_cast<shmem_data *>(shmem_info)->last_tex = lock_id;

					shmem_id = lock_id == 0 ? 1 : 0;
				}

				LeaveCriticalSection(&thread_data.mutexes[cur_tex]);
			}
		}
	}

	for (size_t i = 0; i < 2; i++)
		if (events[i])
			CloseHandle(events[i]);

	return 0;
}

void shmem_copy_data(size_t idx, void *volatile data)
{
	EnterCriticalSection(&thread_data.data_mutex);
	thread_data.cur_tex = static_cast<int>(idx);
	thread_data.cur_data = data;
	thread_data.locked_textures[idx] = true;
	LeaveCriticalSection(&thread_data.data_mutex);

	SetEvent(thread_data.copy_event);
}

bool shmem_texture_data_lock(int idx)
{
	EnterCriticalSection(&thread_data.data_mutex);
	const bool locked = thread_data.locked_textures[idx];
	LeaveCriticalSection(&thread_data.data_mutex);

	if (locked)
	{
		EnterCriticalSection(&thread_data.mutexes[idx]);
		return true;
	}

	return false;
}
void shmem_texture_data_unlock(int idx)
{
	EnterCriticalSection(&thread_data.data_mutex);
	thread_data.locked_textures[idx] = false;
	LeaveCriticalSection(&thread_data.data_mutex);

	LeaveCriticalSection(&thread_data.mutexes[idx]);
}

bool capture_init_shtex(shtex_data *&data, void *window, uint32_t cx, uint32_t cy, uint32_t format, bool flip, uintptr_t handle)
{
	const HWND root_window = GetAncestor(static_cast<HWND>(window), GA_ROOT);
	if (!create_file_mapping(shmem_file_handle, shmem_info, sizeof(shtex_data), SHMEM_TEXTURE "_%llu_%u", static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(root_window)), ++shmem_id_counter))
		return false;

	data = static_cast<shtex_data *>(shmem_info);
	data->tex_handle = static_cast<uint32_t>(handle);

	global_hook_info->hook_ver_major = HOOK_VER_MAJOR;
	global_hook_info->hook_ver_minor = HOOK_VER_MINOR;
	global_hook_info->window = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(window));
	global_hook_info->type = CAPTURE_TYPE_TEXTURE;
	global_hook_info->format = format;
	global_hook_info->flip = flip;
	global_hook_info->map_id = shmem_id_counter;
	global_hook_info->map_size = sizeof(shtex_data);
	global_hook_info->cx = cx;
	global_hook_info->cy = cy;
	global_hook_info->UNUSED_base_cx = cx;
	global_hook_info->UNUSED_base_cy = cy;

	if (!capture_signal_ready())
		return false;
	return active = true;
}

bool capture_init_shmem(shmem_data *&data, void *window, uint32_t cx, uint32_t cy, uint32_t pitch, uint32_t format, bool flip)
{
	uint32_t tex_size = cy * pitch;
	uint32_t aligned_header = (sizeof(shmem_data) + (32 - 1)) & ~(32 - 1);
	uint32_t aligned_tex = (tex_size + (32 - 1)) & ~(32 - 1);
	uint32_t total_size = aligned_header + aligned_tex * 2 + 32;

	const HWND root_window = GetAncestor(static_cast<HWND>(window), GA_ROOT);
	if (!create_file_mapping(shmem_file_handle, shmem_info, total_size, SHMEM_TEXTURE "_%llu_%u", static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(root_window)), ++shmem_id_counter))
		return false;

	data = static_cast<shmem_data *>(shmem_info);

	// To ensure fast copy rate, align texture data to 256 bit addresses
	uintptr_t align_pos = reinterpret_cast<uintptr_t>(shmem_info);
	align_pos += aligned_header;
	align_pos &= ~(32 - 1);
	align_pos -= reinterpret_cast<uintptr_t>(shmem_info);

	if (align_pos < sizeof(shmem_data))
		align_pos += 32;

	data->last_tex = -1;
	data->tex1_offset = static_cast<uint32_t>(align_pos);
	data->tex2_offset = data->tex1_offset + aligned_tex;

	global_hook_info->hook_ver_major = HOOK_VER_MAJOR;
	global_hook_info->hook_ver_minor = HOOK_VER_MINOR;
	global_hook_info->window = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(window));
	global_hook_info->type = CAPTURE_TYPE_MEMORY;
	global_hook_info->format = format;
	global_hook_info->flip = flip;
	global_hook_info->map_id = shmem_id_counter;
	global_hook_info->map_size = total_size;
	global_hook_info->pitch = pitch;
	global_hook_info->cx = cx;
	global_hook_info->cy = cy;
	global_hook_info->UNUSED_base_cx = cx;
	global_hook_info->UNUSED_base_cy = cy;

	thread_data.pitch = pitch;
	thread_data.cy = cy;
	thread_data.shmem_textures[0] = reinterpret_cast<uint8_t *>(data) + data->tex1_offset;
	thread_data.shmem_textures[1] = reinterpret_cast<uint8_t *>(data) + data->tex2_offset;

	thread_data.copy_event = CreateEvent(nullptr, false, false, nullptr);
	if (!thread_data.copy_event)
		return false;

	thread_data.stop_event = CreateEvent(nullptr, true, false, nullptr);
	if (!thread_data.stop_event)
		return false;

	for (int i = 0; i < NUM_BUFFERS; i++)
		InitializeCriticalSection(&thread_data.mutexes[i]);
	InitializeCriticalSection(&thread_data.data_mutex);

	thread_data.copy_thread = CreateThread(nullptr, 0, copy_thread, nullptr, 0, nullptr);
	if (!thread_data.copy_thread)
		return false;

	if (!capture_signal_ready())
		return false;
	return active = true;
}

void capture_free()
{
	if (thread_data.copy_thread)
	{
		SetEvent(thread_data.stop_event);
		if (WaitForSingleObject(thread_data.copy_thread, 500) != WAIT_OBJECT_0)
			TerminateThread(thread_data.copy_thread, static_cast<DWORD>(-1));
		CloseHandle(thread_data.copy_thread);
	}

	destroy_event(thread_data.stop_event);
	destroy_event(thread_data.copy_event);

	for (int i = 0; i < NUM_BUFFERS; i++)
		DeleteCriticalSection(&thread_data.mutexes[i]);
	DeleteCriticalSection(&thread_data.data_mutex);

	memset(&thread_data, 0, sizeof(thread_data));

	destroy_file_mapping(shmem_file_handle, shmem_info);

	capture_signal_restart();
	active = false;
}

bool capture_ready()
{
	if (!capture_active())
		return false;

	if (!global_hook_info->frame_interval)
		return true;

	static std::chrono::high_resolution_clock::time_point last_time;
	const auto t = std::chrono::high_resolution_clock::now();
	const auto elapsed = t - last_time;

	if (elapsed < std::chrono::nanoseconds(global_hook_info->frame_interval))
		return false;

	last_time = (elapsed > std::chrono::nanoseconds(global_hook_info->frame_interval * 2)) ? t : last_time + std::chrono::nanoseconds(global_hook_info->frame_interval);
	return true;
}

bool capture_alive()
{
	HANDLE handle;
	if (open_mutex(handle, WINDOW_HOOK_KEEPALIVE, GetCurrentProcessId()))
	{
		destroy_mutex(handle);
		return true;
	}

	return false;
}

bool capture_active()
{
	return active;
}

bool capture_stopped()
{
	return WaitForSingleObject(signal_stop, 0) == WAIT_OBJECT_0;
}

bool capture_restarted()
{
	return WaitForSingleObject(signal_restart, 0) == WAIT_OBJECT_0;
}

bool capture_signal_ready()
{
	return SetEvent(signal_ready) != FALSE;
}

bool capture_signal_restart()
{
	return SetEvent(signal_restart) != FALSE;
}
