/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include <vulkan/vulkan.h>
#include "runtime.hpp"

namespace reshade::vulkan
{
	class runtime_vulkan : public runtime
	{
	public:
		runtime_vulkan(VkDevice device, VkPhysicalDevice physical_device);

		bool on_init(VkSwapchainKHR swapchain, const VkSwapchainCreateInfoKHR &desc, HWND hwnd);
		void on_reset();
		void on_present(uint32_t swapchain_image_index);

		void capture_screenshot(uint8_t *buffer) const override;

		bool init_texture(texture &texture);
		void upload_texture(texture &texture, const uint8_t *pixels);

		bool compile_effect(effect_data &effect);
		void unload_effects();

		void render_technique(technique &technique) override;

#if RESHADE_GUI
		void init_imgui_resources();
		void render_imgui_draw_data(ImDrawData *data) override;
#endif

		VkDevice _device;
		VkPhysicalDevice _physical_device;
		VkSwapchainKHR _swapchain;
	};
}
