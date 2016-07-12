#pragma once

#include <vulkan/vulkan.h>
#include "runtime.hpp"

namespace reshade
{
	struct vulkan_tex_data : base_object
	{
	};
	struct vulkan_pass_data : base_object
	{
	};

	class vulkan_runtime : public runtime
	{
	public:
		vulkan_runtime(VkDevice device, VkSwapchainKHR swapchain);

		bool on_init(const VkSwapchainCreateInfoKHR &desc, HWND hwnd);
		void on_reset();
		void on_reset_effect() override;
		void on_present(uint32_t swapchain_image_index);

		void capture_frame(uint8_t *buffer) const override;
		bool update_effect(const reshadefx::syntax_tree &ast, std::string &errors) override;
		bool update_texture(texture &texture, const uint8_t *data) override;
		bool update_texture_reference(texture &texture, texture_reference id) override;

		void render_technique(const technique &technique) override;
		void render_draw_lists(ImDrawData *data) override;

		VkDevice _device;
		VkSwapchainKHR _swapchain;
	};
}