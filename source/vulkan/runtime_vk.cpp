/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "dll_log.hpp"
#include "runtime_vk.hpp"
#include "runtime_objects.hpp"
#include "format_utils.hpp"

static inline void transition_layout(const VkLayerDispatchTable &vk, VkCommandBuffer cmd_list, VkImage image, VkImageLayout old_layout, VkImageLayout new_layout,
	const VkImageSubresourceRange &subresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS })
{
	const auto layout_to_access = [](VkImageLayout layout) -> VkAccessFlags {
		switch (layout)
		{
		default:
		case VK_IMAGE_LAYOUT_UNDEFINED:
			return 0; // No prending writes to flush
		case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
			return VK_ACCESS_TRANSFER_READ_BIT;
		case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
			return VK_ACCESS_TRANSFER_WRITE_BIT;
		case VK_IMAGE_LAYOUT_GENERAL:
			return VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
		case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
			return VK_ACCESS_SHADER_READ_BIT;
		case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:
			return VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
		case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
		case VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL:
		case VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL:
			return VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
		case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
			return VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		}
	};
	const auto layout_to_stage = [](VkImageLayout layout) -> VkPipelineStageFlags {
		switch (layout)
		{
		default:
		case VK_IMAGE_LAYOUT_UNDEFINED:
			return VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT; // Do not wait on any previous stage
		case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
		case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
			return VK_PIPELINE_STAGE_TRANSFER_BIT;
		case VK_IMAGE_LAYOUT_GENERAL:
			return VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
		case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
			return VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
		case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:
		case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
		case VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL:
		case VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL:
			return VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR: // Can use color attachment output here, since the semaphores wait on that stage
		case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
			return VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		}
	};

	VkImageMemoryBarrier transition { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
	transition.srcAccessMask = layout_to_access(old_layout);
	transition.dstAccessMask = layout_to_access(new_layout);
	transition.oldLayout = old_layout;
	transition.newLayout = new_layout;
	transition.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	transition.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	transition.image = image;
	transition.subresourceRange = subresource;

	vk.CmdPipelineBarrier(cmd_list, layout_to_stage(old_layout), layout_to_stage(new_layout), 0, 0, nullptr, 0, nullptr, 1, &transition);
}

#define vk _device_impl->_dispatch_table

reshade::vulkan::runtime_impl::runtime_impl(device_impl *device, command_queue_impl *graphics_queue) :
	api_object_impl(VK_NULL_HANDLE), // Swap chain object is later set in 'on_init' below
	_device_impl(device),
	_device(device->_orig),
	_queue_impl(graphics_queue),
	_queue(graphics_queue->_orig),
	_cmd_impl(static_cast<command_list_immediate_impl *>(graphics_queue->get_immediate_command_list()))
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
reshade::vulkan::runtime_impl::~runtime_impl()
{
	on_reset();
}

bool reshade::vulkan::runtime_impl::on_init(VkSwapchainKHR swapchain, const VkSwapchainCreateInfoKHR &desc, HWND hwnd)
{
	_orig = swapchain;

	_width = _window_width = desc.imageExtent.width;
	_height = _window_height = desc.imageExtent.height;
	_color_bit_depth = desc.imageFormat >= VK_FORMAT_A2R10G10B10_UNORM_PACK32 && desc.imageFormat <= VK_FORMAT_A2B10G10R10_SINT_PACK32 ? 10 : 8;
	_backbuffer_format = desc.imageFormat;

	if (hwnd != nullptr)
	{
		RECT window_rect = {};
		GetClientRect(hwnd, &window_rect);

		_window_width = window_rect.right;
		_window_height = window_rect.bottom;
	}

	if (_queue == VK_NULL_HANDLE)
		return false;

	uint32_t num_images = 0;
	if (swapchain != VK_NULL_HANDLE)
	{
		// Get back buffer images
		if (vk.GetSwapchainImagesKHR(_device, swapchain, &num_images, nullptr) != VK_SUCCESS)
			return false;
		_swapchain_images.resize(num_images);
		if (vk.GetSwapchainImagesKHR(_device, swapchain, &num_images, _swapchain_images.data()) != VK_SUCCESS)
			return false;
	}
	else
	{
		num_images = static_cast<uint32_t>(_swapchain_images.size());
		assert(num_images != 0);
	}

	assert(desc.imageUsage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);

	const api::format backbuffer_format_srgb = convert_format(make_format_srgb(_backbuffer_format));
	const api::format backbuffer_format_normal = convert_format(make_format_normal(_backbuffer_format));

	_swapchain_views.resize(num_images * 2);

	for (uint32_t i = 0, k = 0; i < num_images; ++i, k += 2)
	{
		if (!_device_impl->create_resource_view(
			reinterpret_cast<api::resource &>(_swapchain_images[i]),
			api::resource_usage::render_target,
			{ convert_format(make_format_srgb(_backbuffer_format)), 0, 1, 0, 1 },
			reinterpret_cast<api::resource_view *>(&_swapchain_views[k + 1])))
			return false;
		if (!_device_impl->create_resource_view(
			reinterpret_cast<api::resource &>(_swapchain_images[i]),
			api::resource_usage::render_target,
			{ convert_format(make_format_normal(_backbuffer_format)), 0, 1, 0, 1 },
			reinterpret_cast<api::resource_view *>(&_swapchain_views[k + 0])))
			return false;
	}

	for (uint32_t i = 0; i < NUM_QUERY_FRAMES; ++i)
	{
		VkSemaphoreCreateInfo sem_create_info { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };

		if (vk.CreateSemaphore(_device, &sem_create_info, nullptr, &_queue_sync_semaphores[i]) != VK_SUCCESS)
			return false;
	}

	return runtime::on_init(hwnd);
}
void reshade::vulkan::runtime_impl::on_reset()
{
	runtime::on_reset();

	for (VkImageView view : _swapchain_views)
		_device_impl->destroy_resource_view(reinterpret_cast<api::resource_view &>(view));
	_swapchain_views.clear();

	if (_orig == VK_NULL_HANDLE)
		for (VkImage image : _swapchain_images)
			_device_impl->destroy_resource(reinterpret_cast<api::resource &>(image));
	_swapchain_images.clear();

	for (VkSemaphore &semaphore : _queue_sync_semaphores)
		vk.DestroySemaphore(_device, semaphore, nullptr),
		semaphore = VK_NULL_HANDLE;
}

void reshade::vulkan::runtime_impl::on_present(VkQueue queue, const uint32_t swapchain_image_index, std::vector<VkSemaphore> &wait)
{
	if (!_is_initialized)
		return;

	_swap_index = swapchain_image_index;

	update_and_render_effects();
	runtime::on_present();

#ifndef NDEBUG
	// Some operations force a wait for idle in ReShade, which invalidates the wait semaphores, so signal them again (keeps the validation layers happy)
	if (_device_impl->_wait_for_idle_happened)
	{
		VkSubmitInfo submit_info { VK_STRUCTURE_TYPE_SUBMIT_INFO };
		std::vector<VkPipelineStageFlags> wait_stages(wait.size(), VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
		submit_info.waitSemaphoreCount = static_cast<uint32_t>(wait.size());
		submit_info.pWaitSemaphores = wait.data();
		submit_info.pWaitDstStageMask = wait_stages.data();
		submit_info.signalSemaphoreCount = static_cast<uint32_t>(wait.size());
		submit_info.pSignalSemaphores = wait.data();
		vk.QueueSubmit(queue, 1, &submit_info, VK_NULL_HANDLE);

		_device_impl->_wait_for_idle_happened = false;
	}
#endif

	// If the application is presenting with a different queue than rendering, synchronize these two queues first
	// This ensures that it has finished rendering before ReShade applies its own rendering
	if (queue != _queue)
	{
		// Signal a semaphore from the queue the application is presenting with
		VkSubmitInfo submit_info { VK_STRUCTURE_TYPE_SUBMIT_INFO };
		submit_info.signalSemaphoreCount = 1;
		submit_info.pSignalSemaphores = &_queue_sync_semaphores[_queue_sync_index];

		vk.QueueSubmit(queue, 1, &submit_info, VK_NULL_HANDLE);

		// Wait on that semaphore before the immediate command list flush below
		wait.push_back(submit_info.pSignalSemaphores[0]);

		_queue_sync_index = (_queue_sync_index + 1) % NUM_QUERY_FRAMES;
	}

	_cmd_impl->flush(_queue, wait);
}
bool reshade::vulkan::runtime_impl::on_layer_submit(uint32_t eye, VkImage source, const VkExtent2D &source_extent, VkFormat source_format, VkSampleCountFlags source_samples, uint32_t source_layer_index, const float bounds[4], VkImage *target_image)
{
	assert(eye < 2 && source != VK_NULL_HANDLE);

	VkOffset3D source_region_offset = { 0, 0, 0 };
	VkExtent3D source_region_extent = { source_extent.width, source_extent.height, 1 };
	if (bounds != nullptr)
	{
		source_region_offset.x = static_cast<int32_t>(source_extent.width * std::min(bounds[0], bounds[2]));
		source_region_extent.width = static_cast<uint32_t>(source_extent.width* std::max(bounds[0], bounds[2]) - source_region_offset.x);
		source_region_offset.y = static_cast<int32_t>(source_extent.height * std::min(bounds[1], bounds[3]));
		source_region_extent.height = static_cast<uint32_t>(source_extent.height * std::max(bounds[1], bounds[3]) - source_region_offset.y);
	}

	VkExtent2D target_extent = { source_region_extent.width, source_region_extent.height };
	target_extent.width *= 2;

	VkCommandBuffer cmd_list = VK_NULL_HANDLE;

	if (target_extent.width != _width || target_extent.height != _height || source_format != _backbuffer_format)
	{
		on_reset();

		VkImage image = VK_NULL_HANDLE;

		if (!_device_impl->create_resource(
			{ target_extent.width, target_extent.height, 1, 1, convert_format(source_format), 1, api::memory_heap::gpu_only, api::resource_usage::render_target | api::resource_usage::copy_source | api::resource_usage::copy_dest },
			nullptr,
			api::resource_usage::undefined,
			reinterpret_cast<api::resource *>(&image)))
		{
			LOG(ERROR) << "Failed to create region texture!";
			LOG(DEBUG) << "> Details: Width = " << target_extent.width << ", Height = " << target_extent.height << ", Format = " << source_format;
			return false;
		}

		_swapchain_images.resize(1);
		_swapchain_images[0] = image;

		VkSwapchainCreateInfoKHR desc = { VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
		desc.imageExtent = target_extent;
		desc.imageFormat = source_format;
		desc.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

		if (!on_init(VK_NULL_HANDLE, desc, nullptr))
		{
			LOG(ERROR) << "Failed to initialize Vulkan runtime environment on runtime " << this << '!';
			return false;
		}

		cmd_list = _cmd_impl->begin_commands();
		transition_layout(vk, cmd_list, _swapchain_images[0], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
	}
	else
	{
		cmd_list = _cmd_impl->begin_commands();
		transition_layout(vk, cmd_list, _swapchain_images[0], VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
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

	transition_layout(vk, cmd_list, _swapchain_images[0], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

	*target_image = _swapchain_images[0];

	return true;
}

bool reshade::vulkan::runtime_impl::capture_screenshot(uint8_t *buffer) const
{
	if (_color_bit_depth != 8 && _color_bit_depth != 10)
	{
		LOG(ERROR) << "Screenshots are not supported for back buffer format " << _backbuffer_format << '!';
		return false;
	}

	const size_t data_pitch = _width * 4;

	VkBuffer intermediate = VK_NULL_HANDLE;
	VmaAllocation intermediate_mem = VK_NULL_HANDLE;

	{   VkBufferCreateInfo create_info { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
		create_info.size = data_pitch * _height;
		create_info.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;

		VmaAllocationCreateInfo alloc_info = {};
		alloc_info.usage = VMA_MEMORY_USAGE_GPU_TO_CPU;

		if (vmaCreateBuffer(_device_impl->_alloc, &create_info, &alloc_info, &intermediate, &intermediate_mem, nullptr) != VK_SUCCESS)
		{
			LOG(ERROR) << "Failed to create system memory buffer for screenshot capture!";
			LOG(DEBUG) << "> Details: Width = " << create_info.size;
			return false;
		}
	}

	// Copy image into download buffer
	uint8_t *mapped_data = nullptr;
	{
		const VkCommandBuffer cmd_list = _cmd_impl->begin_commands();

		transition_layout(vk, cmd_list, _swapchain_images[_swap_index], VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
		{
			VkBufferImageCopy copy;
			copy.bufferOffset = 0;
			copy.bufferRowLength = _width;
			copy.bufferImageHeight = _height;
			copy.imageOffset = { 0, 0, 0 };
			copy.imageExtent = { _width, _height, 1 };
			copy.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };

			vk.CmdCopyImageToBuffer(cmd_list, _swapchain_images[_swap_index], VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, intermediate, 1, &copy);
		}
		transition_layout(vk, cmd_list, _swapchain_images[_swap_index], VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

		// Wait for any rendering by the application finish before submitting
		// It may have submitted that to a different queue, so simply wait for all to idle here
		_device_impl->wait_idle();

		// Execute and wait for completion
		_cmd_impl->flush_and_wait(_queue);

		// Copy data from intermediate image into output buffer
		if (vmaMapMemory(_device_impl->_alloc, intermediate_mem, reinterpret_cast<void **>(&mapped_data)) != VK_SUCCESS)
			mapped_data = nullptr;
	}

	if (mapped_data != nullptr)
	{
		for (uint32_t y = 0; y < _height; y++, buffer += data_pitch, mapped_data += data_pitch)
		{
			if (_color_bit_depth == 10)
			{
				for (uint32_t x = 0; x < data_pitch; x += 4)
				{
					const uint32_t rgba = *reinterpret_cast<const uint32_t *>(mapped_data + x);
					// Divide by 4 to get 10-bit range (0-1023) into 8-bit range (0-255)
					buffer[x + 0] = ( (rgba & 0x000003FF)        /  4) & 0xFF;
					buffer[x + 1] = (((rgba & 0x000FFC00) >> 10) /  4) & 0xFF;
					buffer[x + 2] = (((rgba & 0x3FF00000) >> 20) /  4) & 0xFF;
					buffer[x + 3] = (((rgba & 0xC0000000) >> 30) * 85) & 0xFF;
					if (_backbuffer_format >= VK_FORMAT_A2B10G10R10_UNORM_PACK32 &&
						_backbuffer_format <= VK_FORMAT_A2B10G10R10_SINT_PACK32)
						std::swap(buffer[x + 0], buffer[x + 2]);
				}
			}
			else
			{
				std::memcpy(buffer, mapped_data, data_pitch);

				if (_backbuffer_format >= VK_FORMAT_B8G8R8A8_UNORM &&
					_backbuffer_format <= VK_FORMAT_B8G8R8A8_SRGB)
				{
					// Format is BGRA, but output should be RGBA, so flip channels
					for (uint32_t x = 0; x < data_pitch; x += 4)
						std::swap(buffer[x + 0], buffer[x + 2]);
				}
			}
		}

		vmaUnmapMemory(_device_impl->_alloc, intermediate_mem);
	}

	vmaDestroyBuffer(_device_impl->_alloc, intermediate, intermediate_mem);

	return mapped_data != nullptr;
}

bool reshade::vulkan::runtime_impl::compile_effect(effect &effect, api::shader_stage type, const std::string &entry_point, api::shader_module &out)
{
	// There are various issues with SPIR-V modules that have multiple entry points on all major GPU vendors.
	// On AMD for instance creating a graphics pipeline just fails with a generic VK_ERROR_OUT_OF_HOST_MEMORY. On NVIDIA artifacts occur on some driver versions.
	// To work around these problems, create a separate shader module for every entry point and rewrite the SPIR-V module for each to removes all but a single entry point (and associated functions/variables).
	uint32_t current_function = 0, current_function_offset = 0;
	std::vector<uint32_t> spirv = effect.module.spirv;
	std::vector<uint32_t> functions_to_remove, variables_to_remove;

	for (uint32_t inst = 5 /* Skip SPIR-V header information */; inst < spirv.size();)
	{
		const uint32_t op = spirv[inst] & 0xFFFF;
		const uint32_t len = (spirv[inst] >> 16) & 0xFFFF;
		assert(len != 0);

		switch (op)
		{
		case 15: // OpEntryPoint
			// Look for any non-matching entry points
			if (entry_point != reinterpret_cast<const char *>(&spirv[inst + 3]))
			{
				functions_to_remove.push_back(spirv[inst + 2]);

				// Get interface variables
				for (size_t k = inst + 3 + ((strlen(reinterpret_cast<const char *>(&spirv[inst + 3])) + 4) / 4); k < inst + len; ++k)
					variables_to_remove.push_back(spirv[k]);

				// Remove this entry point from the module
				spirv.erase(spirv.begin() + inst, spirv.begin() + inst + len);
				continue;
			}
			break;
		case 16: // OpExecutionMode
			if (std::find(functions_to_remove.begin(), functions_to_remove.end(), spirv[inst + 1]) != functions_to_remove.end())
			{
				spirv.erase(spirv.begin() + inst, spirv.begin() + inst + len);
				continue;
			}
			break;
		case 59: // OpVariable
			// Remove all declarations of the interface variables for non-matching entry points
			if (std::find(variables_to_remove.begin(), variables_to_remove.end(), spirv[inst + 2]) != variables_to_remove.end())
			{
				spirv.erase(spirv.begin() + inst, spirv.begin() + inst + len);
				continue;
			}
			break;
		case 71: // OpDecorate
			// Remove all decorations targeting any of the interface variables for non-matching entry points
			if (std::find(variables_to_remove.begin(), variables_to_remove.end(), spirv[inst + 1]) != variables_to_remove.end())
			{
				spirv.erase(spirv.begin() + inst, spirv.begin() + inst + len);
				continue;
			}
			break;
		case 54: // OpFunction
			current_function = spirv[inst + 2];
			current_function_offset = inst;
			break;
		case 56: // OpFunctionEnd
			// Remove all function definitions for non-matching entry points
			if (std::find(functions_to_remove.begin(), functions_to_remove.end(), current_function) != functions_to_remove.end())
			{
				spirv.erase(spirv.begin() + current_function_offset, spirv.begin() + inst + len);
				inst = current_function_offset;
				continue;
			}
			break;
		}

		inst += len;
	}

	return _device_impl->create_shader_module(type, api::shader_format::spirv, spirv.data(), spirv.size() * sizeof(uint32_t), entry_point.c_str(), &out);
}
