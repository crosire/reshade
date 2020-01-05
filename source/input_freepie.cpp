/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "input_freepie.hpp"
#include <Windows.h>

template <typename T>
class shared_memory
{
public:
	shared_memory() {}
	shared_memory(LPCTSTR name)
	{
		handle = OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE, name);
		if (handle != NULL)
			mapped = MapViewOfFile(handle, FILE_MAP_ALL_ACCESS, 0, 0, 50);
	}
	shared_memory(const shared_memory &other) = delete;
	shared_memory(shared_memory &&other) { operator=(other); }
	~shared_memory()
	{
		if (mapped != NULL)
			UnmapViewOfFile(mapped);
		if (handle != NULL)
			CloseHandle(handle);
	}

	shared_memory &operator=(const shared_memory &other) = delete;
	shared_memory &operator=(shared_memory &&other)
	{
		handle = other.handle;
		mapped = other.mapped;
		other.handle = NULL;
		other.mapped = NULL;
		return *this;
	}

	operator T &()
	{
		return *(T*)mapped;
	}

	operator bool() const
	{
		return handle != NULL && handle != INVALID_HANDLE_VALUE;
	}

private:
	HANDLE handle = NULL;
	LPVOID mapped = NULL;
};

const size_t FREEPIE_IO_MAX_SLOTS = 4;

bool freepie_io_6dof_read(uint32_t index, freepie_io_6dof_data *output)
{
	if (index >= FREEPIE_IO_MAX_SLOTS)
		return false;

	static shared_memory<freepie_io_shared_data[FREEPIE_IO_MAX_SLOTS]> memory;
	if (!memory) memory = shared_memory<freepie_io_shared_data[FREEPIE_IO_MAX_SLOTS]>(TEXT("FPGeneric"));
	if (!memory) return false;

	*output = memory[index].data;

	return true;
}
