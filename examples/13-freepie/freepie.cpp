/*
 * Copyright (C) 2014 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <reshade.hpp>

template <typename T>
class shared_memory
{
public:
	shared_memory() = default;
	explicit shared_memory(LPCTSTR name)
	{
		handle = OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE, name);
		if (handle != nullptr)
			mapped = MapViewOfFile(handle, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(T));
	}
	shared_memory(const shared_memory &other) = delete;
	shared_memory(shared_memory &&other) { operator=(other); }
	~shared_memory()
	{
		if (mapped != nullptr)
			UnmapViewOfFile(mapped);
		if (handle != nullptr)
			CloseHandle(handle);
	}

	shared_memory &operator=(const shared_memory &other) = delete;
	shared_memory &operator=(shared_memory &&other)
	{
		handle = other.handle;
		mapped = other.mapped;
		other.handle = nullptr;
		other.mapped = nullptr;
		return *this;
	}

	operator T &()
	{
		return *static_cast<T *>(mapped);
	}

	operator bool() const
	{
		return handle != NULL && handle != INVALID_HANDLE_VALUE;
	}

private:
	HANDLE handle = nullptr;
	LPVOID mapped = nullptr;
};

struct freepie_io_data
{
	float yaw;
	float pitch;
	float roll;

	float x;
	float y;
	float z;
};

struct freepie_io_shared_data
{
	int32_t data_id;
	freepie_io_data data;
};

bool freepie_io_read(uint32_t index, freepie_io_data *output)
{
	const size_t FREEPIE_IO_MAX_SLOTS = 4;
	if (index >= FREEPIE_IO_MAX_SLOTS)
		return false;

	static shared_memory<freepie_io_shared_data[FREEPIE_IO_MAX_SLOTS]> memory;
	if (!memory) memory = shared_memory<freepie_io_shared_data[FREEPIE_IO_MAX_SLOTS]>(TEXT("FPGeneric"));
	if (!memory) return false;

	*output = memory[index].data;

	return true;
}

static void update_uniform_variables(reshade::api::effect_runtime *runtime, reshade::api::command_list *, reshade::api::resource_view, reshade::api::resource_view)
{
	runtime->enumerate_uniform_variables(nullptr, [](reshade::api::effect_runtime *runtime, reshade::api::effect_uniform_variable variable) {
		char source[64];
		if (runtime->get_annotation_string_from_uniform_variable(variable, "source", source) && std::strcmp(source, "freepie") == 0)
		{
			int index = 0;
			runtime->get_annotation_int_from_uniform_variable(variable, "index", &index, 1);

			if (freepie_io_data data;
				freepie_io_read(index, &data))
			{
				runtime->set_uniform_value_float(variable, &data.yaw, 3 * 2);
			}
		}
	});
}

void register_addon_freepie()
{
	reshade::register_event<reshade::addon_event::reshade_begin_effects>(update_uniform_variables);
}
void unregister_addon_freepie()
{
	reshade::unregister_event<reshade::addon_event::reshade_begin_effects>(update_uniform_variables);
}

extern "C" __declspec(dllexport) const char *NAME = "FreePIE";
extern "C" __declspec(dllexport) const char *DESCRIPTION = "Adds support for reading FreePIE input data in effects.";

BOOL APIENTRY DllMain(HMODULE hModule, DWORD fdwReason, LPVOID)
{
	switch (fdwReason)
	{
	case DLL_PROCESS_ATTACH:
		if (!reshade::register_addon(hModule))
			return FALSE;
		register_addon_freepie();
		break;
	case DLL_PROCESS_DETACH:
		unregister_addon_freepie();
		reshade::unregister_addon(hModule);
		break;
	}

	return TRUE;
}
