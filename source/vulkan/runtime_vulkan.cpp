/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "log.hpp"
#include "runtime_vulkan.hpp"
#include "runtime_objects.hpp"

namespace reshade::vulkan
{
}

reshade::vulkan::runtime_vulkan::runtime_vulkan(VkDevice device, VkSwapchainKHR swapchain) :
	_device(device), _swapchain(swapchain)
{
	_renderer_id = 0x20000;
}

bool reshade::vulkan::runtime_vulkan::on_init(const VkSwapchainCreateInfoKHR &desc, HWND hwnd)
{
	_width = desc.imageExtent.width;
	_height = desc.imageExtent.height;

	return runtime::on_init(hwnd);
}
void reshade::vulkan::runtime_vulkan::on_reset()
{
	runtime::on_reset();
}

void reshade::vulkan::runtime_vulkan::on_present(uint32_t swapchain_image_index)
{
	update_and_render_effects();
	runtime::on_present();
}

void reshade::vulkan::runtime_vulkan::capture_screenshot(uint8_t *buffer) const
{
}

bool reshade::vulkan::runtime_vulkan::init_texture(texture &texture)
{
	return false;
}
void reshade::vulkan::runtime_vulkan::upload_texture(texture &texture, const uint8_t *pixels)
{

}

bool reshade::vulkan::runtime_vulkan::compile_effect(effect_data &effect)
{
	return false;
}
void reshade::vulkan::runtime_vulkan::unload_effects()
{
	runtime::unload_effects();
}

void reshade::vulkan::runtime_vulkan::render_technique(technique &technique)
{
}

#if RESHADE_GUI
void reshade::vulkan::runtime_vulkan::init_imgui_resources()
{

}

void reshade::vulkan::runtime_vulkan::render_imgui_draw_data(ImDrawData *data)
{
}
#endif
