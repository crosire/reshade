/*
 * Copyright (C) 2014 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "vulkan_impl_device.hpp"
#include "vulkan_impl_swapchain.hpp"
#include "vulkan_impl_type_convert.hpp"

#define vk _device_impl->_dispatch_table

reshade::vulkan::swapchain_impl::swapchain_impl(device_impl *device) :
	api_object_impl(VK_NULL_HANDLE), // Swap chain object is later set in 'on_init' below
	_device_impl(device)
{
}
reshade::vulkan::swapchain_impl::~swapchain_impl()
{
	on_reset();
}

reshade::api::device *reshade::vulkan::swapchain_impl::get_device()
{
	return _device_impl;
}

reshade::api::resource reshade::vulkan::swapchain_impl::get_back_buffer(uint32_t index)
{
	return { (uint64_t)_swapchain_images[index] };
}

uint32_t reshade::vulkan::swapchain_impl::get_back_buffer_count() const
{
	return static_cast<uint32_t>(_swapchain_images.size());
}
uint32_t reshade::vulkan::swapchain_impl::get_current_back_buffer_index() const
{
	return _swap_index;
}

void reshade::vulkan::swapchain_impl::set_current_back_buffer_index(uint32_t index)
{
	_swap_index = index;
}

bool reshade::vulkan::swapchain_impl::check_color_space_support(api::color_space color_space) const
{
	uint32_t num_formats = 0;
	_device_impl->_instance_dispatch_table.GetPhysicalDeviceSurfaceFormatsKHR(_device_impl->_physical_device, _create_info.surface, &num_formats, nullptr);
	std::vector<VkSurfaceFormatKHR> formats(num_formats);
	_device_impl->_instance_dispatch_table.GetPhysicalDeviceSurfaceFormatsKHR(_device_impl->_physical_device, _create_info.surface, &num_formats, formats.data());

	for (const VkSurfaceFormatKHR &format : formats)
		if (format.format == _create_info.imageFormat && format.colorSpace == convert_color_space(color_space))
			return true;

	return false;
}

reshade::api::color_space reshade::vulkan::swapchain_impl::get_color_space() const
{
	return convert_color_space(_create_info.imageColorSpace);
}

void reshade::vulkan::swapchain_impl::on_init(VkSwapchainKHR swapchain, const VkSwapchainCreateInfoKHR &create_info, HWND hwnd)
{
	_orig = swapchain;

	// Get back buffer images
	uint32_t num_images = 0;
	if (vk.GetSwapchainImagesKHR(_device_impl->_orig, swapchain, &num_images, nullptr) != VK_SUCCESS)
		return;
	_swapchain_images.resize(num_images);
	if (vk.GetSwapchainImagesKHR(_device_impl->_orig, swapchain, &num_images, _swapchain_images.data()) != VK_SUCCESS)
		return;

	// Add swap chain images to the image list
	object_data<VK_OBJECT_TYPE_IMAGE> data;
	data.allocation = VK_NULL_HANDLE;
	data.create_info = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
	data.create_info.imageType = VK_IMAGE_TYPE_2D;
	data.create_info.format = create_info.imageFormat;
	data.create_info.extent = { create_info.imageExtent.width, create_info.imageExtent.height, 1 };
	data.create_info.mipLevels = 1;
	data.create_info.arrayLayers = create_info.imageArrayLayers;
	data.create_info.samples = VK_SAMPLE_COUNT_1_BIT;
	data.create_info.usage = create_info.imageUsage;
	data.create_info.sharingMode = create_info.imageSharingMode;
	data.create_info.initialLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	for (uint32_t i = 0; i < num_images; ++i)
		_device_impl->register_object<VK_OBJECT_TYPE_IMAGE>(_swapchain_images[i], std::move(data));

	assert(create_info.imageUsage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);

	_hwnd = hwnd;
	_create_info = create_info;
}
void reshade::vulkan::swapchain_impl::on_reset()
{
	// Remove swap chain images from the image list
	for (VkImage image : _swapchain_images)
		_device_impl->unregister_object<VK_OBJECT_TYPE_IMAGE>(image);
	_swapchain_images.clear();
}
