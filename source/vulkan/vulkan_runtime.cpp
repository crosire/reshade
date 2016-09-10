#include "vulkan_runtime.hpp"
#include "log.hpp"
#include "input.hpp"

extern std::unordered_map<VkDevice, VkPhysicalDevice> g_vulkan_device_mapping;
extern std::unordered_map<VkSwapchainKHR, std::vector<VkImage>> g_vulkan_swapchain_images;

namespace reshade
{
	vulkan_runtime::vulkan_runtime(VkDevice device, VkSwapchainKHR swapchain) : runtime(0x20000), _device(device), _swapchain(swapchain)
	{
	}

	bool vulkan_runtime::on_init(const VkSwapchainCreateInfoKHR &desc, HWND hwnd)
	{
		_width = desc.imageExtent.width;
		_height = desc.imageExtent.height;
		_input = input::register_window(hwnd);

		return runtime::on_init();
	}
	void vulkan_runtime::on_reset()
	{
		if (!_is_initialized)
		{
			return;
		}

		runtime::on_reset();
	}
	void vulkan_runtime::on_reset_effect()
	{
		runtime::on_reset_effect();
	}
	void vulkan_runtime::on_present(uint32_t swapchain_image_index)
	{
		runtime::on_present_effect();
		runtime::on_present();
	}

	void vulkan_runtime::capture_frame(uint8_t *buffer) const
	{
	}
	bool vulkan_runtime::load_effect(const reshadefx::syntax_tree &ast, std::string & errors)
	{
		return false;
	}
	bool vulkan_runtime::update_texture(texture &texture, const uint8_t *data)
	{
		return false;
	}
	bool vulkan_runtime::update_texture_reference(texture &texture, texture_reference id)
	{
		return false;
	}

	void vulkan_runtime::render_technique(const technique &technique)
	{
	}
	void vulkan_runtime::render_draw_lists(ImDrawData *data)
	{
	}
}
