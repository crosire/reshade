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
	_backbuffer_format = convert_format(desc.imageFormat);

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

bool reshade::vulkan::swapchain_impl::on_vr_submit(uint32_t eye, VkImage source, const VkExtent2D &source_extent, VkFormat source_format, VkSampleCountFlags source_samples, uint32_t source_layer_index, const float bounds[4], VkImage *target_image)
{
	assert(eye < 2 && source != VK_NULL_HANDLE);

	VkOffset3D source_region_offset = { 0, 0, 0 };
	VkExtent3D source_region_extent = { source_extent.width, source_extent.height, 1 };
	if (bounds != nullptr)
	{
		source_region_offset.x = static_cast<int32_t>(std::floor(source_extent.width * std::min(bounds[0], bounds[2])));
		source_region_offset.y = static_cast<int32_t>(std::floor(source_extent.height * std::min(bounds[1], bounds[3])));
		source_region_extent.width = static_cast<uint32_t>(std::ceil(source_extent.width* std::max(bounds[0], bounds[2]) - source_region_offset.x));
		source_region_extent.height = static_cast<uint32_t>(std::ceil(source_extent.height * std::max(bounds[1], bounds[3]) - source_region_offset.y));
	}

	VkExtent2D target_extent = { source_region_extent.width, source_region_extent.height };
	target_extent.width *= 2;

	VkCommandBuffer cmd_list = VK_NULL_HANDLE;

	// Due to rounding errors with the bounds we have to use a tolerance of 1 pixel per eye (2 pixels in total)
	const int32_t width_difference = std::abs(static_cast<int32_t>(target_extent.width) - static_cast<int32_t>(_width));

	if (width_difference > 2 || target_extent.height != _height || convert_format(source_format) != _backbuffer_format)
	{
		on_reset();

		_is_vr = true;

		api::resource image = {};
		if (!static_cast<device_impl *>(_device)->create_resource(
			api::resource_desc(target_extent.width, target_extent.height, 1, 1, convert_format(source_format), 1, api::memory_heap::gpu_only, api::resource_usage::render_target | api::resource_usage::copy_source | api::resource_usage::copy_dest),
			nullptr, api::resource_usage::undefined, &image))
		{
			LOG(ERROR) << "Failed to create region texture!";
			LOG(DEBUG) << "> Details: Width = " << target_extent.width << ", Height = " << target_extent.height << ", Format = " << source_format;
			return false;
		}

		_swapchain_images.resize(1);
		_swapchain_images[0] = (VkImage)image.handle;

		VkSwapchainCreateInfoKHR desc = { VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
		desc.imageExtent = target_extent;
		desc.imageFormat = source_format;
		desc.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

		if (!on_init(VK_NULL_HANDLE, desc, nullptr))
			return false;

		cmd_list = static_cast<command_list_immediate_impl *>(static_cast<command_queue_impl *>(_graphics_queue)->get_immediate_command_list())->begin_commands();

		VkImageMemoryBarrier transition { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
		transition.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		transition.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		transition.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		transition.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		transition.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		transition.image = _swapchain_images[0];
		transition.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS };

		vk.CmdPipelineBarrier(cmd_list, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &transition);
	}
	else
	{
		cmd_list = static_cast<command_list_immediate_impl *>(static_cast<command_queue_impl *>(_graphics_queue)->get_immediate_command_list())->begin_commands();

		VkImageMemoryBarrier transition { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
		transition.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		transition.oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		transition.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		transition.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		transition.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		transition.image = _swapchain_images[0];
		transition.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS };

		vk.CmdPipelineBarrier(cmd_list, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &transition);
	}

	// Copy region of the source texture
	if (source_samples == VK_SAMPLE_COUNT_1_BIT)
	{
		const VkImageCopy copy_region = {
			{ VK_IMAGE_ASPECT_COLOR_BIT, 0, source_layer_index, 1 }, source_region_offset,
			{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 }, { static_cast<int32_t>(eye * source_region_extent.width), 0, 0 }, source_region_extent
		};
		vk.CmdCopyImage(cmd_list, source, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, _swapchain_images[0], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy_region);
	}
	else
	{
		const VkImageResolve resolve_region = {
			{ VK_IMAGE_ASPECT_COLOR_BIT, 0, source_layer_index, 1 }, source_region_offset,
			{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 }, { static_cast<int32_t>(eye * source_region_extent.width), 0, 0 }, source_region_extent
		};
		vk.CmdResolveImage(cmd_list, source, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, _swapchain_images[0], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &resolve_region);
	}

	{
		VkImageMemoryBarrier transition { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
		transition.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		transition.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		transition.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		transition.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		transition.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		transition.image = _swapchain_images[0];
		transition.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS };

		vk.CmdPipelineBarrier(cmd_list, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &transition);
	}

	*target_image = _swapchain_images[0];

	return true;
}
