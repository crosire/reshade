/*
 * Copyright (C) 2021 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#if defined(RESHADE_API_LIBRARY_EXPORT)

#include "reshade.hpp"
#include "addon_manager.hpp"
#include "runtime.hpp"
#include "dll_log.hpp"
#include "ini_file.hpp"
#include <cstring> // std::strlen

void ReShadeLogMessage([[maybe_unused]] HMODULE module, int level, const char *message)
{
	std::string prefix;
#if RESHADE_ADDON
	if (module != nullptr)
	{
		reshade::addon_info *const info = reshade::find_addon(module);
		if (info != nullptr)
			prefix = "[" + info->name + "] ";
	}
#endif

	reshade::log::message(static_cast<reshade::log::level>(level), "%.*s%s", static_cast<int>(prefix.size()), prefix.c_str(), message);
}

void ReShadeGetBasePath(char *path, size_t *size)
{
	if (size == nullptr)
		return;

	const std::string path_string = g_reshade_base_path.u8string();

	if (path == nullptr)
	{
		*size = path_string.size() + 1;
	}
	else if (*size != 0)
	{
		*size = path_string.copy(path, *size - 1);
		path[*size] = '\0';
	}
}

bool ReShadeGetConfigValue(HMODULE, reshade::api::effect_runtime *runtime, const char *section, const char *key, char *value, size_t *size)
{
	ini_file &config = (runtime != nullptr) ? ini_file::load_cache(static_cast<reshade::runtime *>(runtime)->get_config_path()) : reshade::global_config();

	const std::string section_string = section != nullptr ? section : std::string();
	const std::string key_string = key != nullptr ? key : std::string();

	std::vector<std::string> elements;
	config.get(section_string, key_string, elements);

	if (size != nullptr || value != nullptr)
	{
		std::string value_string;
		for (const std::string &element : elements)
		{
			value_string += element;
			value_string += '\0';
		}

		if (size != nullptr)
		{
			if (elements.empty())
			{
				*size = 0;
			}
			else if (value == nullptr)
			{
				*size = value_string.size() + 1;
			}
			else if (*size != 0)
			{
				*size = value_string.copy(value, *size - 1);
				value[*size] = '\0';
			}
		}
	}

	return !elements.empty();
}

void ReShadeSetConfigValue(HMODULE module, reshade::api::effect_runtime *runtime, const char *section, const char *key, const char *value)
{
	return ReShadeSetConfigArray(module, runtime, section, key, value, value != nullptr ? std::strlen(value) : 0);
}
void ReShadeSetConfigArray(HMODULE, reshade::api::effect_runtime *runtime, const char *section, const char *key, const char *value, size_t size)
{
	ini_file &config = (runtime != nullptr) ? ini_file::load_cache(static_cast<reshade::runtime *>(runtime)->get_config_path()) : reshade::global_config();

	const std::string section_string = section != nullptr ? section : std::string();
	const std::string key_string = key != nullptr ? key : std::string();

	if (size == 0)
	{
		config.remove_key(section_string, key_string);
		return;
	}

	std::vector<std::string> elements;
	for (size_t i = 0, k = 0; i < size; i++)
	{
		if (k >= elements.size())
			elements.resize(k + 1);

		if (value[i] == '\0')
		{
			k++;
			continue;
		}

		elements[k] += value[i];
	}

	config.set(section_string, key_string, elements);
}

#include "d3d9/d3d9_impl_device.hpp"
#include "d3d9/d3d9_impl_swapchain.hpp"
#include "d3d10/d3d10_impl_device.hpp"
#include "d3d10/d3d10_impl_swapchain.hpp"
#include "d3d11/d3d11_impl_device.hpp"
#include "d3d11/d3d11_impl_device_context.hpp"
#include "d3d11/d3d11_impl_swapchain.hpp"
#include "d3d12/d3d12_impl_device.hpp"
#include "d3d12/d3d12_impl_command_queue.hpp"
#include "d3d12/d3d12_impl_swapchain.hpp"
#include "opengl/opengl_impl_device.hpp"
#include "opengl/opengl_impl_device_context.hpp"
#include "opengl/opengl_impl_swapchain.hpp"
#include "vulkan/vulkan_impl_device.hpp"
#include "vulkan/vulkan_impl_command_queue.hpp"
#include "vulkan/vulkan_impl_swapchain.hpp"

bool ReShadeCreateEffectRuntime(reshade::api::device_api api, void *opaque_device, void *opaque_command_queue, void *opaque_swapchain, const char *config_path, reshade::api::effect_runtime **out_runtime)
{
	if (out_runtime == nullptr)
		return false;
	*out_runtime = nullptr;

	if (opaque_swapchain == nullptr || config_path == nullptr)
		return false;

	reshade::api::swapchain *swapchain_impl = nullptr;
	reshade::api::command_queue *graphics_queue_impl = nullptr;

	switch (api)
	{
	case reshade::api::device_api::d3d9:
	{
		com_ptr<IDirect3DDevice9> device;
		com_ptr<IDirect3DSwapChain9> swapchain;
		if (FAILED(static_cast<IUnknown *>(opaque_swapchain)->QueryInterface(&swapchain)))
			return false;
		if (FAILED(swapchain->GetDevice(&device)) || device.get() != opaque_device)
			return false;

		const auto device_impl = new reshade::d3d9::device_impl(device.get());
		swapchain_impl = new reshade::d3d9::swapchain_impl(device_impl, swapchain.get());
		graphics_queue_impl = device_impl;
		break;
	}
	case reshade::api::device_api::d3d10:
	{
		com_ptr<ID3D10Device1> device;
		com_ptr<IDXGISwapChain> swapchain;
		if (FAILED(static_cast<IUnknown *>(opaque_swapchain)->QueryInterface(&swapchain)))
			return false;
		if (FAILED(swapchain->GetDevice(IID_PPV_ARGS(&device))) || device.get() != opaque_device)
			return false;

		const auto device_impl = new reshade::d3d10::device_impl(device.get());
		swapchain_impl = new reshade::d3d10::swapchain_impl(device_impl, swapchain.get());
		graphics_queue_impl = device_impl;
		break;
	}
	case reshade::api::device_api::d3d11:
	{
		com_ptr<ID3D11Device> device;
		com_ptr<IDXGISwapChain> swapchain;
		if (FAILED(static_cast<IUnknown *>(opaque_swapchain)->QueryInterface(&swapchain)))
			return false;
		if (FAILED(swapchain->GetDevice(IID_PPV_ARGS(&device))) || device.get() != opaque_device)
			return false;

		com_ptr<ID3D11DeviceContext> device_context;
		if (opaque_command_queue != nullptr)
		{
			if (FAILED(static_cast<IUnknown *>(opaque_command_queue)->QueryInterface(&device_context)))
				return false;
		}
		else
		{
			device->GetImmediateContext(&device_context);
		}

		const auto device_impl = new reshade::d3d11::device_impl(device.get());
		swapchain_impl = new reshade::d3d11::swapchain_impl(device_impl, swapchain.get());
		graphics_queue_impl = new reshade::d3d11::device_context_impl(device_impl, device_context.get());
		break;
	}
	case reshade::api::device_api::d3d12:
	{
		com_ptr<ID3D12Device> device;
		com_ptr<IDXGISwapChain3> swapchain;
		if (FAILED(static_cast<IUnknown *>(opaque_swapchain)->QueryInterface(&swapchain)))
			return false;
		if (FAILED(swapchain->GetDevice(IID_PPV_ARGS(&device))) || device.get() != opaque_device)
			return false;

		com_ptr<ID3D12CommandQueue> command_queue;
		if (opaque_command_queue == nullptr ||
			FAILED(static_cast<IUnknown *>(opaque_command_queue)->QueryInterface(IID_PPV_ARGS(&command_queue))))
			return false;

		const auto device_impl = new reshade::d3d12::device_impl(device.get());
		swapchain_impl = new reshade::d3d12::swapchain_impl(device_impl, swapchain.get());
		graphics_queue_impl = new reshade::d3d12::command_queue_impl(device_impl, command_queue.get());
		break;
	}
	case reshade::api::device_api::opengl:
	{
		const HDC hdc = static_cast<HDC>(opaque_swapchain);
		if (hdc == nullptr || WindowFromDC(hdc) == nullptr)
			return false;

		gl3wInit();

		const auto device_impl = new reshade::opengl::device_impl(hdc, static_cast<HGLRC>(opaque_device));
		swapchain_impl = new reshade::opengl::swapchain_impl(device_impl, hdc);
		graphics_queue_impl = new reshade::opengl::device_context_impl(device_impl, static_cast<HGLRC>(opaque_device));
		break;
	}
	default:
		return false;
	}

	const auto runtime = new reshade::runtime(swapchain_impl, graphics_queue_impl, std::filesystem::u8path(config_path), false);
	if (!runtime->on_init())
	{
		ReShadeDestroyEffectRuntime(runtime);
		return false;
	}

	*out_runtime = runtime;
	return true;
}

void ReShadeDestroyEffectRuntime(reshade::api::effect_runtime *runtime)
{
	if (runtime == nullptr)
		return;

	reshade::api::device *const device = runtime->get_device();
	reshade::api::swapchain *const swapchain = static_cast<reshade::runtime *>(runtime)->get_swapchain();
	reshade::api::command_queue *const graphics_queue = runtime->get_command_queue();

	static_cast<reshade::runtime *>(runtime)->on_reset();

	delete static_cast<reshade::runtime *>(runtime);

	switch (device->get_api())
	{
	case reshade::api::device_api::d3d9:
		delete static_cast<reshade::d3d9::swapchain_impl *>(swapchain);
		delete static_cast<reshade::d3d9::device_impl *>(device);
		break;
	case reshade::api::device_api::d3d10:
		delete static_cast<reshade::d3d10::swapchain_impl *>(swapchain);
		delete static_cast<reshade::d3d10::device_impl *>(device);
		break;
	case reshade::api::device_api::d3d11:
		delete static_cast<reshade::d3d11::swapchain_impl *>(swapchain);
		delete static_cast<reshade::d3d11::device_context_impl *>(graphics_queue);
		delete static_cast<reshade::d3d11::device_impl *>(device);
		break;
	case reshade::api::device_api::d3d12:
		delete static_cast<reshade::d3d12::swapchain_impl *>(swapchain);
		delete static_cast<reshade::d3d12::command_queue_impl *>(graphics_queue);
		delete static_cast<reshade::d3d12::device_impl *>(device);
		break;
	case reshade::api::device_api::opengl:
		delete static_cast<reshade::opengl::swapchain_impl *>(swapchain);
		delete static_cast<reshade::opengl::device_context_impl *>(graphics_queue);
		delete static_cast<reshade::opengl::device_impl *>(device);
		break;
	case reshade::api::device_api::vulkan:
		delete static_cast<reshade::vulkan::swapchain_impl *>(swapchain);
		delete static_cast<reshade::vulkan::command_queue_impl *>(graphics_queue);
		delete static_cast<reshade::vulkan::device_impl *>(device);
		break;
	}
}

void ReShadeUpdateAndPresentEffectRuntime(reshade::api::effect_runtime *runtime)
{
	if (runtime == nullptr)
		return;

	reshade::api::command_queue *const present_queue = runtime->get_command_queue();

	static_cast<reshade::runtime *>(runtime)->on_present(present_queue);

	present_queue->flush_immediate_command_list();
}

#if RESHADE_GUI

#include "imgui_function_table_19180.hpp"
#include "imgui_function_table_19040.hpp"
#include "imgui_function_table_19000.hpp"
#include "imgui_function_table_18971.hpp"
#include "imgui_function_table_18600.hpp"

extern "C" __declspec(dllexport) const void *ReShadeGetImGuiFunctionTable(uint32_t version)
{
	if (version == 19180)
		return &g_imgui_function_table_19180;
	if (version == 19040)
		return &g_imgui_function_table_19040;
	if (version >= 19000 && version < 19040)
		return &g_imgui_function_table_19000;
	if (version == 18971)
		return &g_imgui_function_table_18971;
	if (version == 18600)
		return &g_imgui_function_table_18600;

	reshade::log::message(reshade::log::level::error, "Failed to retrieve ImGui function table, because the requested ImGui version (%u) is not supported.", version);
	return nullptr;
}

#endif

#endif
