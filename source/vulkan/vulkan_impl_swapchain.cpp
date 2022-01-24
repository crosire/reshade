/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "dll_log.hpp"
#include "vulkan_impl_device.hpp"
#include "vulkan_impl_command_queue.hpp"
#include "vulkan_impl_swapchain.hpp"
#include "vulkan_impl_type_convert.hpp"

#define vk static_cast<device_impl *>(_device)->_dispatch_table

reshade::vulkan::swapchain_impl::swapchain_impl(device_impl *device, command_queue_impl *graphics_queue) :
	api_object_impl(VK_NULL_HANDLE, device, graphics_queue) // Swap chain object is later set in 'on_init' below
{
	VkPhysicalDeviceProperties device_props = {};
	device->_instance_dispatch_table.GetPhysicalDeviceProperties(device->_physical_device, &device_props);

	_renderer_id = 0x20000 |
		VK_VERSION_MAJOR(device_props.apiVersion) << 12 |
		VK_VERSION_MINOR(device_props.apiVersion) <<  8;

	_vendor_id = device_props.vendorID;
	_device_id = device_props.deviceID;

	// NVIDIA has a custom driver version scheme, so extract the proper minor version from it
	const uint32_t driver_minor_version = _vendor_id == 0x10DE ?
		(device_props.driverVersion >> 14) & 0xFF : VK_VERSION_MINOR(device_props.driverVersion);
	LOG(INFO) << "Running on " << device_props.deviceName << " Driver " << VK_VERSION_MAJOR(device_props.driverVersion) << '.' << driver_minor_version;
}
reshade::vulkan::swapchain_impl::~swapchain_impl()
{
	on_reset();
}

reshade::api::resource reshade::vulkan::swapchain_impl::get_back_buffer(uint32_t index)
{
	return { (uint64_t)_swapchain_images[index] };
}
reshade::api::resource reshade::vulkan::swapchain_impl::get_back_buffer_resolved(uint32_t index)
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

bool reshade::vulkan::swapchain_impl::on_init(VkSwapchainKHR swapchain, const VkSwapchainCreateInfoKHR &desc, HWND hwnd)
{
	_orig = swapchain;

	uint32_t num_images = 0;
	if (swapchain != VK_NULL_HANDLE)
	{
		// Get back buffer images
		if (vk.GetSwapchainImagesKHR(static_cast<device_impl *>(_device)->_orig, swapchain, &num_images, nullptr) != VK_SUCCESS)
			return false;
		_swapchain_images.resize(num_images);
		if (vk.GetSwapchainImagesKHR(static_cast<device_impl *>(_device)->_orig, swapchain, &num_images, _swapchain_images.data()) != VK_SUCCESS)
			return false;

		// Add swap chain images to the image list
		object_data<VK_OBJECT_TYPE_IMAGE> data;
		data.allocation = VK_NULL_HANDLE;
		data.create_info = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
		data.create_info.imageType = VK_IMAGE_TYPE_2D;
		data.create_info.format = desc.imageFormat;
		data.create_info.extent = { desc.imageExtent.width, desc.imageExtent.height, 1 };
		data.create_info.mipLevels = 1;
		data.create_info.arrayLayers = desc.imageArrayLayers;
		data.create_info.samples = VK_SAMPLE_COUNT_1_BIT;
		data.create_info.usage = desc.imageUsage;
		data.create_info.sharingMode = desc.imageSharingMode;
		data.create_info.initialLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

		for (uint32_t i = 0; i < num_images; ++i)
			static_cast<device_impl *>(_device)->register_object<VK_OBJECT_TYPE_IMAGE>(_swapchain_images[i], std::move(data));
	}
	else
	{
		num_images = static_cast<uint32_t>(_swapchain_images.size());
		assert(num_images != 0);
	}

	assert(desc.imageUsage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);

#if RESHADE_ADDON
	invoke_addon_event<addon_event::init_swapchain>(this);
#endif

	_width = desc.imageExtent.width;
	_height = desc.imageExtent.height;
	_back_buffer_format = convert_format(desc.imageFormat);

	for (uint32_t i = 0; i < NUM_SYNC_SEMAPHORES; ++i)
	{
		VkSemaphoreCreateInfo sem_create_info { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };

		if (vk.CreateSemaphore(static_cast<device_impl *>(_device)->_orig, &sem_create_info, nullptr, &_queue_sync_semaphores[i]) != VK_SUCCESS)
		{
			LOG(ERROR) << "Failed to create queue synchronization semaphore!";
			return false;
		}
	}

	return runtime::on_init(hwnd);
}
void reshade::vulkan::swapchain_impl::on_reset()
{
	if (_swapchain_images.empty())
		return;

	runtime::on_reset();

#if RESHADE_ADDON
	invoke_addon_event<addon_event::destroy_swapchain>(this);
#endif

	if (_orig == VK_NULL_HANDLE)
		for (VkImage image : _swapchain_images)
			static_cast<device_impl *>(_device)->destroy_resource({ (uint64_t)image });
	else
		// Remove swap chain images from the image list
		for (VkImage image : _swapchain_images)
			static_cast<device_impl *>(_device)->unregister_object<VK_OBJECT_TYPE_IMAGE>(image);
	_swapchain_images.clear();

	for (VkSemaphore &semaphore : _queue_sync_semaphores)
		vk.DestroySemaphore(static_cast<device_impl *>(_device)->_orig, semaphore, nullptr),
		semaphore = VK_NULL_HANDLE;
}

void reshade::vulkan::swapchain_impl::on_present(VkQueue queue, const uint32_t swapchain_image_index, std::vector<VkSemaphore> &wait)
{
	_swap_index = swapchain_image_index;

	if (!is_initialized())
		return;

	runtime::on_present();

#if RESHADE_ADDON
	invoke_addon_event<addon_event::reshade_present>(this);
#endif

#ifndef NDEBUG
	// Some operations force a wait for idle in ReShade, which invalidates the wait semaphores, so signal them again (keeps the validation layers happy)
	if (static_cast<device_impl *>(_device)->_wait_for_idle_happened)
	{
		VkSubmitInfo submit_info { VK_STRUCTURE_TYPE_SUBMIT_INFO };
		std::vector<VkPipelineStageFlags> wait_stages(wait.size(), VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
		submit_info.waitSemaphoreCount = static_cast<uint32_t>(wait.size());
		submit_info.pWaitSemaphores = wait.data();
		submit_info.pWaitDstStageMask = wait_stages.data();
		submit_info.signalSemaphoreCount = static_cast<uint32_t>(wait.size());
		submit_info.pSignalSemaphores = wait.data();
		vk.QueueSubmit(queue, 1, &submit_info, VK_NULL_HANDLE);

		static_cast<device_impl *>(_device)->_wait_for_idle_happened = false;
	}
#endif

	// If the application is presenting with a different queue than rendering, synchronize these two queues first
	// This ensures that it has finished rendering before ReShade applies its own rendering
	if (queue != static_cast<command_queue_impl *>(_graphics_queue)->_orig)
	{
		// Signal a semaphore from the queue the application is presenting with
		VkSubmitInfo submit_info { VK_STRUCTURE_TYPE_SUBMIT_INFO };
		submit_info.signalSemaphoreCount = 1;
		submit_info.pSignalSemaphores = &_queue_sync_semaphores[_queue_sync_index];

		vk.QueueSubmit(queue, 1, &submit_info, VK_NULL_HANDLE);

		// Wait on that semaphore before the immediate command list flush below
		wait.push_back(submit_info.pSignalSemaphores[0]);

		_queue_sync_index = (_queue_sync_index + 1) % NUM_SYNC_SEMAPHORES;

		static_cast<command_queue_impl *>(_graphics_queue)->flush_immediate_command_list(wait);
	}
}
