/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "dll_log.hpp"
#include "runtime_vk.hpp"
#include "runtime_vk_objects.hpp"
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

reshade::vulkan::runtime_vk::runtime_vk(device_impl *device, command_queue_impl *graphics_queue) :
	api_object_impl(VK_NULL_HANDLE), _device_impl(device), _device(device->_orig), _queue_impl(graphics_queue), _queue((VkQueue)graphics_queue->get_native_object()), _cmd_impl(static_cast<command_list_immediate_impl *>(graphics_queue->get_immediate_command_list()))
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

	// Find a supported stencil format
	const VkFormat possible_stencil_formats[] = {
		VK_FORMAT_S8_UINT,
		VK_FORMAT_D16_UNORM_S8_UINT,
		VK_FORMAT_D24_UNORM_S8_UINT,
		VK_FORMAT_D32_SFLOAT_S8_UINT
	};

	for (const VkFormat format : possible_stencil_formats)
	{
		VkFormatProperties format_props = {};
		device->_instance_dispatch_table.GetPhysicalDeviceFormatProperties(device->_physical_device, format, &format_props);

		if ((format_props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0)
		{
			_effect_stencil_format = format;
			break;
		}
	}
}
reshade::vulkan::runtime_vk::~runtime_vk()
{
	on_reset();

#if RESHADE_GUI
	vk.DestroyPipeline(_device, _imgui.pipeline, nullptr);
	_imgui.pipeline = VK_NULL_HANDLE;
	vk.DestroyPipelineLayout(_device, _imgui.pipeline_layout, nullptr);
	_imgui.pipeline_layout = VK_NULL_HANDLE;
	vk.DestroyDescriptorSetLayout(_device, _imgui.descriptor_layout, nullptr);
	_imgui.descriptor_layout = VK_NULL_HANDLE;
	vk.DestroySampler(_device, _imgui.sampler, nullptr);
	_imgui.sampler = VK_NULL_HANDLE;
#endif

	vk.DestroyDescriptorSetLayout(_device, _effect_descriptor_layout, nullptr);
	_effect_descriptor_layout = VK_NULL_HANDLE;
}

bool reshade::vulkan::runtime_vk::on_init(VkSwapchainKHR swapchain, const VkSwapchainCreateInfoKHR &desc, HWND hwnd)
{
	_orig = swapchain;

	RECT window_rect = {};
	GetClientRect(hwnd, &window_rect);

	_width = desc.imageExtent.width;
	_height = desc.imageExtent.height;
	_window_width = window_rect.right;
	_window_height = window_rect.bottom;
	_color_bit_depth = desc.imageFormat >= VK_FORMAT_A2R10G10B10_UNORM_PACK32 && desc.imageFormat <= VK_FORMAT_A2B10G10R10_SINT_PACK32 ? 10 : 8;
	_backbuffer_format = desc.imageFormat;

	if (_queue == VK_NULL_HANDLE)
		return false;

	// Create back buffer shader image
	assert(_backbuffer_format != VK_FORMAT_UNDEFINED);
	_backbuffer_image = create_image(
		_width, _height, 1, _backbuffer_format,
		VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VMA_MEMORY_USAGE_GPU_ONLY,
		VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT, VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT);
	if (_backbuffer_image == VK_NULL_HANDLE)
		return false;
	set_debug_name_image(_backbuffer_image, "ReShade back buffer");

	_backbuffer_image_view[0] = create_image_view(_backbuffer_image, make_format_normal(_backbuffer_format), 1, VK_IMAGE_ASPECT_COLOR_BIT);
	if (_backbuffer_image_view[0] == VK_NULL_HANDLE)
		return false;
	_backbuffer_image_view[1] = create_image_view(_backbuffer_image, make_format_srgb(_backbuffer_format), 1, VK_IMAGE_ASPECT_COLOR_BIT);
	if (_backbuffer_image_view[1] == VK_NULL_HANDLE)
		return false;

	// Create effect depth-stencil resource
	assert(_effect_stencil_format != VK_FORMAT_UNDEFINED);
	_effect_stencil = create_image(
		_width, _height, 1, _effect_stencil_format,
		VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VMA_MEMORY_USAGE_GPU_ONLY);
	if (_effect_stencil == VK_NULL_HANDLE)
		return false;
	set_debug_name_image(_effect_stencil, "ReShade stencil buffer");

	_effect_stencil_view = create_image_view(_effect_stencil, _effect_stencil_format, 1, VK_IMAGE_ASPECT_STENCIL_BIT);
	if (_effect_stencil_view == VK_NULL_HANDLE)
		return false;

	// Create default render pass
	for (uint32_t k = 0; k < 2; ++k)
	{
		VkAttachmentReference attachment_refs[2] = {};
		attachment_refs[0].attachment = 0;
		attachment_refs[0].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		attachment_refs[1].attachment = 1;
		attachment_refs[1].layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		VkAttachmentDescription attachment_descs[2] = {};
		attachment_descs[0].format = k == 0 ? make_format_normal(_backbuffer_format) : make_format_srgb(_backbuffer_format);
		attachment_descs[0].samples = VK_SAMPLE_COUNT_1_BIT;
		attachment_descs[0].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
		attachment_descs[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachment_descs[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachment_descs[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachment_descs[0].initialLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		attachment_descs[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		attachment_descs[1].format = _effect_stencil_format;
		attachment_descs[1].samples = VK_SAMPLE_COUNT_1_BIT;
		attachment_descs[1].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachment_descs[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachment_descs[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
		attachment_descs[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachment_descs[1].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		attachment_descs[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		VkSubpassDependency subdep = {};
		subdep.srcSubpass = VK_SUBPASS_EXTERNAL;
		subdep.dstSubpass = 0;
		subdep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		subdep.dstStageMask = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;
		subdep.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		subdep.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

		VkSubpassDescription subpass = {};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &attachment_refs[0];
		subpass.pDepthStencilAttachment = &attachment_refs[1];

		VkRenderPassCreateInfo create_info { VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
		create_info.attachmentCount = 2;
		create_info.pAttachments = attachment_descs;
		create_info.subpassCount = 1;
		create_info.pSubpasses = &subpass;
		create_info.dependencyCount = 1;
		create_info.pDependencies = &subdep;

		if (vk.CreateRenderPass(_device, &create_info, nullptr, &_default_render_pass[k]) != VK_SUCCESS)
			return false;
	}

	// Get back buffer images
	uint32_t num_images = 0;
	if (vk.GetSwapchainImagesKHR(_device, swapchain, &num_images, nullptr) != VK_SUCCESS)
		return false;
	_swapchain_images.resize(num_images);
	if (vk.GetSwapchainImagesKHR(_device, swapchain, &num_images, _swapchain_images.data()) != VK_SUCCESS)
		return false;

	assert(desc.imageUsage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
	_render_area = desc.imageExtent;

	_swapchain_views.resize(num_images * 2);
	_swapchain_frames.resize(num_images * 2);

	for (uint32_t i = 0, k = 0; i < num_images; ++i, k += 2)
	{
		_swapchain_views[k + 1] = create_image_view(_swapchain_images[i], make_format_srgb(desc.imageFormat), 1, VK_IMAGE_ASPECT_COLOR_BIT);
		_swapchain_views[k + 0] = create_image_view(_swapchain_images[i], make_format_normal(desc.imageFormat), 1, VK_IMAGE_ASPECT_COLOR_BIT);

		const VkImageView attachment_views[2] = { _swapchain_views[k + 0], _effect_stencil_view };
		const VkImageView attachment_views_srgb[2] = { _swapchain_views[k + 1], _effect_stencil_view };

		{   VkFramebufferCreateInfo create_info { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
			create_info.renderPass = _default_render_pass[0];
			create_info.attachmentCount = 2;
			create_info.pAttachments = attachment_views;
			create_info.width = desc.imageExtent.width;
			create_info.height = desc.imageExtent.height;
			create_info.layers = 1;

			if (vk.CreateFramebuffer(_device, &create_info, nullptr, &_swapchain_frames[k + 0]) != VK_SUCCESS)
				return false;

			create_info.renderPass = _default_render_pass[1];
			create_info.pAttachments = attachment_views_srgb;

			if (vk.CreateFramebuffer(_device, &create_info, nullptr, &_swapchain_frames[k + 1]) != VK_SUCCESS)
				return false;
		}
	}

	for (uint32_t i = 0; i < NUM_QUERY_FRAMES; ++i)
	{
		VkSemaphoreCreateInfo sem_create_info { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };

		if (vk.CreateSemaphore(_device, &sem_create_info, nullptr, &_queue_sync_semaphores[i]) != VK_SUCCESS)
			return false;
	}

	// Allocate a single descriptor pool for all effects
	if (_effect_descriptor_pool == VK_NULL_HANDLE)
	{
		VkDescriptorPoolSize pool_sizes[] = {
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, MAX_EFFECT_DESCRIPTOR_SETS }, // Only need one global UBO per set
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, MAX_EFFECT_DESCRIPTOR_SETS * MAX_IMAGE_DESCRIPTOR_SETS },
			{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, MAX_EFFECT_DESCRIPTOR_SETS * MAX_IMAGE_DESCRIPTOR_SETS },
		};

		VkDescriptorPoolCreateInfo create_info { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
		// No VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT set, so that all descriptors can be reset in one go via vkResetDescriptorPool
		create_info.maxSets = MAX_EFFECT_DESCRIPTOR_SETS;
		create_info.poolSizeCount = static_cast<uint32_t>(std::size(pool_sizes));
		create_info.pPoolSizes = pool_sizes;

		if (vk.CreateDescriptorPool(_device, &create_info, nullptr, &_effect_descriptor_pool) != VK_SUCCESS)
			return false;
	}

	if (_effect_descriptor_layout == VK_NULL_HANDLE)
	{
		VkDescriptorSetLayoutBinding bindings = { 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_ALL };
		VkDescriptorSetLayoutCreateInfo create_info { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
		create_info.bindingCount = 1;
		create_info.pBindings = &bindings;

		if (vk.CreateDescriptorSetLayout(_device, &create_info, nullptr, &_effect_descriptor_layout) != VK_SUCCESS)
			return false;
	}

	// Create an empty image, which is used when no depth buffer was detected (since you cannot bind nothing to a descriptor in Vulkan)
	// Use VK_FORMAT_R16_SFLOAT format, since it is mandatory according to the spec (see https://www.khronos.org/registry/vulkan/specs/1.1/html/vkspec.html#features-required-format-support)
	_empty_depth_image = create_image(1, 1, 1, VK_FORMAT_R16_SFLOAT, VK_IMAGE_USAGE_SAMPLED_BIT, VMA_MEMORY_USAGE_GPU_ONLY);
	if (_empty_depth_image == VK_NULL_HANDLE)
		return false;
	_empty_depth_image_view = create_image_view(_empty_depth_image, VK_FORMAT_R16_SFLOAT, 1, VK_IMAGE_ASPECT_COLOR_BIT);
	if (_empty_depth_image_view == VK_NULL_HANDLE)
		return false;

	// Transition image layouts to the ones required below
	const VkCommandBuffer cmd_list = _cmd_impl->begin_commands();
	transition_layout(vk, cmd_list, _effect_stencil, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, { aspect_flags_from_format(_effect_stencil_format), 0, 1, 0, 1 });
	transition_layout(vk, cmd_list, _empty_depth_image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

#if RESHADE_GUI
	if (!init_imgui_resources())
		return false;
#endif

	return runtime::on_init(hwnd);
}
void reshade::vulkan::runtime_vk::on_reset()
{
	runtime::on_reset();

	// Make sure none of the resources below are currently in use
	wait_for_command_buffers();

	for (VkImageView view : _swapchain_views)
		vk.DestroyImageView(_device, view, nullptr);
	_swapchain_views.clear();
	for (VkFramebuffer frame : _swapchain_frames)
		vk.DestroyFramebuffer(_device, frame, nullptr);
	_swapchain_frames.clear();
	_swapchain_images.clear();

	for (VkSemaphore &semaphore : _queue_sync_semaphores)
		vk.DestroySemaphore(_device, semaphore, nullptr),
		semaphore = VK_NULL_HANDLE;

	vk.DestroyImage(_device, _backbuffer_image, nullptr);
	_backbuffer_image = VK_NULL_HANDLE;
	vk.DestroyImageView(_device, _backbuffer_image_view[0], nullptr);
	_backbuffer_image_view[0] = VK_NULL_HANDLE;
	vk.DestroyImageView(_device, _backbuffer_image_view[1], nullptr);
	_backbuffer_image_view[1] = VK_NULL_HANDLE;

	vk.DestroyRenderPass(_device, _default_render_pass[0], nullptr);
	_default_render_pass[0] = VK_NULL_HANDLE;
	vk.DestroyRenderPass(_device, _default_render_pass[1], nullptr);
	_default_render_pass[1] = VK_NULL_HANDLE;

	vk.DestroyImage(_device, _empty_depth_image, nullptr);
	_empty_depth_image = VK_NULL_HANDLE;
	vk.DestroyImageView(_device, _empty_depth_image_view, nullptr);
	_empty_depth_image_view = VK_NULL_HANDLE;

	vk.DestroyImage(_device, _effect_stencil, nullptr);
	_effect_stencil = VK_NULL_HANDLE;
	vk.DestroyImageView(_device, _effect_stencil_view, nullptr);
	_effect_stencil_view = VK_NULL_HANDLE;
	vk.DestroyDescriptorPool(_device, _effect_descriptor_pool, nullptr);
	_effect_descriptor_pool = VK_NULL_HANDLE;

#if RESHADE_GUI
	for (unsigned int i = 0; i < NUM_IMGUI_BUFFERS; ++i)
	{
		vmaDestroyBuffer(_device_impl->_alloc, _imgui.indices[i], _imgui.indices_mem[i]);
		vmaDestroyBuffer(_device_impl->_alloc, _imgui.vertices[i], _imgui.vertices_mem[i]);
		_imgui.indices[i] = VK_NULL_HANDLE;
		_imgui.vertices[i] = VK_NULL_HANDLE;
		_imgui.indices_mem[i] = VK_NULL_HANDLE;
		_imgui.vertices_mem[i] = VK_NULL_HANDLE;
		_imgui.num_indices[i] = 0;
		_imgui.num_vertices[i] = 0;
	}

	vk.DestroyDescriptorPool(_device, _imgui.descriptor_pool, nullptr);
	_imgui.descriptor_pool = VK_NULL_HANDLE;
#endif

	// Free all unmanaged device memory allocated via the 'create_image' and 'create_buffer' functions
	vmaFreeMemoryPages(_device_impl->_alloc, _allocations.size(), _allocations.data());
	_allocations.clear();
}

void reshade::vulkan::runtime_vk::on_present(VkQueue queue, uint32_t swapchain_image_index, std::vector<VkSemaphore> &wait)
{
	if (!_is_initialized)
		return;

	_swap_index = swapchain_image_index;

	update_and_render_effects();
	runtime::on_present();

#ifndef NDEBUG
	// Some operations force a wait for idle in ReShade, which invalidates the wait semaphores, so signal them again (keeps the validation layers happy)
	if (_wait_for_idle_happened)
	{
		VkSubmitInfo submit_info { VK_STRUCTURE_TYPE_SUBMIT_INFO };
		std::vector<VkPipelineStageFlags> wait_stages(wait.size(), VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
		submit_info.waitSemaphoreCount = static_cast<uint32_t>(wait.size());
		submit_info.pWaitSemaphores = wait.data();
		submit_info.pWaitDstStageMask = wait_stages.data();
		submit_info.signalSemaphoreCount = static_cast<uint32_t>(wait.size());
		submit_info.pSignalSemaphores = wait.data();
		vk.QueueSubmit(queue, 1, &submit_info, VK_NULL_HANDLE);

		_wait_for_idle_happened = false;
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

bool reshade::vulkan::runtime_vk::capture_screenshot(uint8_t *buffer) const
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
			return false;
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
		vk.DeviceWaitIdle(_device);
#ifndef NDEBUG
		_wait_for_idle_happened = true;
#endif

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

bool reshade::vulkan::runtime_vk::init_effect(size_t index)
{
	effect &effect = _effects[index];

	// Load shader modules
	struct shader_modules
	{
		bool loaded = false;
		runtime_vk *const runtime;
		std::vector<VkShaderModule> list;
		std::unordered_map<std::string, VkShaderModule> entry_points;

		shader_modules(runtime_vk *runtime, const reshadefx::module &effect_module) : runtime(runtime)
		{
			VkResult res = VK_SUCCESS;

			// There are various issues with SPIR-V modules that have multiple entry points on all major GPU vendors.
			// On AMD for instance creating a graphics pipeline just fails with a generic VK_ERROR_OUT_OF_HOST_MEMORY. On NVIDIA artifacts occur on some driver versions.
			// To work around these problems, create a separate shader module for every entry point and rewrite the SPIR-V module for each to removes all but a single entry point (and associated functions/variables).
			for (size_t i = 0; i < effect_module.entry_points.size() && res == VK_SUCCESS; ++i)
			{
				const reshadefx::entry_point &entry_point = effect_module.entry_points[i];

				uint32_t current_function = 0, current_function_offset = 0;
				std::vector<uint32_t> spirv = effect_module.spirv;
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
						if (entry_point.name != reinterpret_cast<const char *>(&spirv[inst + 3]))
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

				VkShaderModuleCreateInfo create_info { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
				create_info.codeSize = spirv.size() * sizeof(uint32_t);
				create_info.pCode = spirv.data();

				res = runtime->vk.CreateShaderModule(runtime->_device, &create_info, nullptr, &list.emplace_back());

				entry_points[entry_point.name] = list.back();
			}

			if (res != VK_SUCCESS)
			{
				LOG(ERROR) << "Failed to create shader module! Vulkan error code is " << res << '.';
				return;
			}

			loaded = true;
		}
		~shader_modules()
		{
			for (const VkShaderModule module : list)
				runtime->vk.DestroyShaderModule(runtime->_device, module, nullptr);
		}
	}
	shader_modules(this, effect.module);
	if (!shader_modules.loaded)
		return false;

	if (_effect_data.size() <= index)
		_effect_data.resize(index + 1);
	effect_data &effect_data = _effect_data[index];

	// Create query pool for time measurements
	{   VkQueryPoolCreateInfo create_info { VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO };
		create_info.queryType = VK_QUERY_TYPE_TIMESTAMP;
		create_info.queryCount = static_cast<uint32_t>(effect.module.techniques.size() * 2 * NUM_QUERY_FRAMES);

		if (vk.CreateQueryPool(_device, &create_info, nullptr, &effect_data.query_pool) != VK_SUCCESS)
			return false;
	}

	// Initialize pipeline layout
	{   std::vector<VkDescriptorSetLayoutBinding> bindings;
		bindings.reserve(effect.module.num_sampler_bindings);
		for (uint32_t i = 0; i < effect.module.num_sampler_bindings; ++i)
			bindings.push_back({ i, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_ALL });

		VkDescriptorSetLayoutCreateInfo create_info { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
		create_info.bindingCount = uint32_t(bindings.size());
		create_info.pBindings = bindings.data();

		if (vk.CreateDescriptorSetLayout(_device, &create_info, nullptr, &effect_data.sampler_layout) != VK_SUCCESS)
			return false;
	}

	if (effect.module.num_storage_bindings != 0)
	{
		std::vector<VkDescriptorSetLayoutBinding> bindings;
		bindings.reserve(effect.module.num_storage_bindings);
		for (uint32_t i = 0; i < effect.module.num_storage_bindings; ++i)
			bindings.push_back({ i, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT });

		VkDescriptorSetLayoutCreateInfo create_info { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
		create_info.bindingCount = uint32_t(bindings.size());
		create_info.pBindings = bindings.data();

		if (vk.CreateDescriptorSetLayout(_device, &create_info, nullptr, &effect_data.storage_layout) != VK_SUCCESS)
			return false;
	}

	const VkDescriptorSetLayout set_layouts[3] = { _effect_descriptor_layout, effect_data.sampler_layout, effect_data.storage_layout };

	{   VkPipelineLayoutCreateInfo create_info { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
		create_info.setLayoutCount = effect.module.num_storage_bindings == 0 ? 2 : 3; // [0] = Global UBO, [1] = Samplers, [2] = Storage Images
		create_info.pSetLayouts = set_layouts;

		if (vk.CreatePipelineLayout(_device, &create_info, nullptr, &effect_data.pipeline_layout) != VK_SUCCESS)
			return false;
	}

	// Create global uniform buffer object
	if (!effect.uniform_data_storage.empty())
	{
		effect_data.ubo = create_buffer(
			effect.uniform_data_storage.size(),
			VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU,
			0, 0, &effect_data.ubo_mem);
		if (effect_data.ubo == VK_NULL_HANDLE)
			return false;
	}

	// Initialize image and sampler bindings
	assert(effect.module.num_texture_bindings == 0); // Use combined image samplers
	std::vector<VkDescriptorImageInfo> sampler_bindings(effect.module.num_sampler_bindings);
	std::vector<VkDescriptorImageInfo> storage_bindings(effect.module.num_storage_bindings);

	for (const reshadefx::sampler_info &info : effect.module.samplers)
	{
		const texture &texture = look_up_texture_by_name(info.texture_name);

		VkDescriptorImageInfo &image_binding = sampler_bindings[info.binding];
		image_binding.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		if (texture.semantic == "COLOR")
		{
			image_binding.imageView = _backbuffer_image_view[info.srgb];
		}
		else if (!texture.semantic.empty())
		{
			// Set to a default view to avoid crash because of this not being set
			image_binding.imageView = _empty_depth_image_view;

			if (const auto it = _texture_semantic_bindings.find(texture.semantic);
				it != _texture_semantic_bindings.end())
				image_binding.imageView = it->second;

			// Keep track of the texture descriptor to simplify updating it
			effect_data.texture_semantic_to_binding.push_back({ texture.semantic, info.binding });
		}
		else
		{
			image_binding.imageView = static_cast<tex_data *>(texture.impl)->view[info.srgb];
		}

		// Unset bindings are not allowed, so fail initialization for the entire effect in that case
		if (assert(image_binding.imageView != VK_NULL_HANDLE); image_binding.imageView == VK_NULL_HANDLE)
			return false;

		VkSamplerCreateInfo create_info { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
		create_info.addressModeU = static_cast<VkSamplerAddressMode>(static_cast<uint32_t>(info.address_u) - 1);
		create_info.addressModeV = static_cast<VkSamplerAddressMode>(static_cast<uint32_t>(info.address_v) - 1);
		create_info.addressModeW = static_cast<VkSamplerAddressMode>(static_cast<uint32_t>(info.address_w) - 1);
		create_info.mipLodBias = info.lod_bias;
		create_info.anisotropyEnable = VK_FALSE;
		create_info.maxAnisotropy = 1.0f;
		create_info.compareEnable = VK_FALSE;
		create_info.compareOp = VK_COMPARE_OP_ALWAYS;
		create_info.minLod = info.min_lod;
		create_info.maxLod = info.max_lod;

		switch (info.filter)
		{
		case reshadefx::texture_filter::min_mag_mip_point:
			create_info.magFilter = VK_FILTER_NEAREST;
			create_info.minFilter = VK_FILTER_NEAREST;
			create_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
			break;
		case reshadefx::texture_filter::min_mag_point_mip_linear:
			create_info.magFilter = VK_FILTER_NEAREST;
			create_info.minFilter = VK_FILTER_NEAREST;
			create_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
			break;
		case reshadefx::texture_filter::min_point_mag_linear_mip_point:
			create_info.magFilter = VK_FILTER_LINEAR;
			create_info.minFilter = VK_FILTER_NEAREST;
			create_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
			break;
		case reshadefx::texture_filter::min_point_mag_mip_linear:
			create_info.magFilter = VK_FILTER_LINEAR;
			create_info.minFilter = VK_FILTER_NEAREST;
			create_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
			break;
		case reshadefx::texture_filter::min_linear_mag_mip_point:
			create_info.magFilter = VK_FILTER_NEAREST;
			create_info.minFilter = VK_FILTER_LINEAR;
			create_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
			break;
		case reshadefx::texture_filter::min_linear_mag_point_mip_linear:
			create_info.magFilter = VK_FILTER_NEAREST;
			create_info.minFilter = VK_FILTER_LINEAR;
			create_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
			break;
		case reshadefx::texture_filter::min_mag_linear_mip_point:
			create_info.magFilter = VK_FILTER_LINEAR;
			create_info.minFilter = VK_FILTER_LINEAR;
			create_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
			break;
		case reshadefx::texture_filter::min_mag_mip_linear:
			create_info.magFilter = VK_FILTER_LINEAR;
			create_info.minFilter = VK_FILTER_LINEAR;
			create_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
			break;
		}

		// Generate hash for sampler description
		size_t desc_hash = 2166136261;
		for (size_t i = 0; i < sizeof(create_info); ++i)
			desc_hash = (desc_hash * 16777619) ^ reinterpret_cast<const uint8_t *>(&create_info)[i];

		std::unordered_map<size_t, VkSampler>::iterator it = _effect_sampler_states.find(desc_hash);
		if (it == _effect_sampler_states.end())
		{
			VkSampler sampler = VK_NULL_HANDLE;
			if (vk.CreateSampler(_device, &create_info, nullptr, &sampler) != VK_SUCCESS)
				return false;
			it = _effect_sampler_states.emplace(desc_hash, sampler).first;
		}

		image_binding.sampler = it->second;
	}

	for (const reshadefx::storage_info &info : effect.module.storages)
	{
		const texture &texture = look_up_texture_by_name(info.texture_name);

		VkDescriptorImageInfo &image_binding = storage_bindings[info.binding];
		image_binding.imageView = static_cast<tex_data *>(texture.impl)->view[0];
		image_binding.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

		// Unset bindings are not allowed, so fail initialization for the entire effect in that case
		if (image_binding.imageView == VK_NULL_HANDLE)
			return false;
	}

	uint32_t num_passes = 0;
	for (const reshadefx::technique_info &info : effect.module.techniques)
		num_passes += static_cast<uint32_t>(info.passes.size());

	std::vector<VkDescriptorSet> sets(1 + 2 * num_passes);
	std::vector<VkWriteDescriptorSet> writes;
	writes.reserve(sets.size());

	{   VkDescriptorSetAllocateInfo alloc_info { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
		alloc_info.descriptorPool = _effect_descriptor_pool;
		alloc_info.descriptorSetCount = 1 + (effect.module.num_storage_bindings == 0 ? 1 : 2) * num_passes;

		std::vector<VkDescriptorSetLayout> alloc_set_layouts(sets.size());
		alloc_set_layouts[0] = _effect_descriptor_layout;
		for (size_t i = 0; i < num_passes; ++i)
		{
			alloc_set_layouts[1 + i] = effect_data.sampler_layout;
			alloc_set_layouts[1 + num_passes + i] = effect_data.storage_layout;
		}
		alloc_info.pSetLayouts = alloc_set_layouts.data();

		if (vk.AllocateDescriptorSets(_device, &alloc_info, sets.data()) != VK_SUCCESS)
		{
			LOG(ERROR) << "Too many effects loaded. Only " << (MAX_EFFECT_DESCRIPTOR_SETS / 2) << " effects can be active simultaneously in Vulkan.";
			return false;
		}
	}

	effect_data.ubo_set = sets[0];
	const VkDescriptorBufferInfo ubo_info = { effect_data.ubo, 0, VK_WHOLE_SIZE };
	if (effect_data.ubo != VK_NULL_HANDLE)
	{
		VkWriteDescriptorSet &write = writes.emplace_back();
		write = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
		write.dstSet = effect_data.ubo_set;
		write.dstBinding = 0;
		write.descriptorCount = 1;
		write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		write.pBufferInfo = &ubo_info;
	}

	std::vector<uint8_t> spec_data;
	std::vector<VkSpecializationMapEntry> spec_constants;
	for (const reshadefx::uniform_info &constant : effect.module.spec_constants)
	{
		const uint32_t id = static_cast<uint32_t>(spec_constants.size());
		const uint32_t offset = static_cast<uint32_t>(spec_data.size());
		spec_data.resize(offset + constant.size);
		spec_constants.push_back({ id, offset, constant.size });
		std::memcpy(spec_data.data() + offset, &constant.initializer_value.as_uint[0], constant.size);
	}

	const VkSpecializationInfo spec_info {
		static_cast<uint32_t>(spec_constants.size()), spec_constants.data(),
		spec_data.size(), spec_data.data()
	};

	uint32_t technique_index = 0;
	uint32_t total_pass_index = 0;
	for (technique &technique : _techniques)
	{
		if (technique.impl != nullptr || technique.effect_index != index)
			continue;

		auto impl = new technique_data();
		technique.impl = impl;

		// Offset index so that a query exists for each command frame and two subsequent ones are used for before/after stamps
		impl->query_base_index = technique_index++ * 2 * NUM_QUERY_FRAMES;

		impl->passes.resize(technique.passes.size());
		for (size_t pass_index = 0; pass_index < technique.passes.size(); ++pass_index, ++total_pass_index)
		{
			pass_data &pass_data = impl->passes[pass_index];
			const reshadefx::pass_info &pass_info = technique.passes[pass_index];

			if (!pass_info.cs_entry_point.empty())
			{
				impl->has_compute_passes = true;

				VkComputePipelineCreateInfo create_info { VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };
				create_info.stage = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
				create_info.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
				create_info.stage.module = shader_modules.entry_points.at(pass_info.cs_entry_point);;
				create_info.stage.pName = pass_info.cs_entry_point.c_str();
				create_info.stage.pSpecializationInfo = &spec_info;
				create_info.layout = effect_data.pipeline_layout;

				const VkResult res = vk.CreateComputePipelines(_device, VK_NULL_HANDLE, 1, &create_info, nullptr, &pass_data.pipeline);
				if (res != VK_SUCCESS)
				{
					LOG(ERROR) << "Failed to create compute pipeline for pass " << pass_index << " in technique '" << technique.name << "'! Vulkan error code is " << res << '.';
					return false;
				}
			}
			else
			{
				const auto convert_blend_op = [](reshadefx::pass_blend_op value) -> VkBlendOp {
					switch (value)
					{
					default:
					case reshadefx::pass_blend_op::add: return VK_BLEND_OP_ADD;
					case reshadefx::pass_blend_op::subtract: return VK_BLEND_OP_SUBTRACT;
					case reshadefx::pass_blend_op::rev_subtract: return VK_BLEND_OP_REVERSE_SUBTRACT;
					case reshadefx::pass_blend_op::min: return VK_BLEND_OP_MIN;
					case reshadefx::pass_blend_op::max: return VK_BLEND_OP_MAX;
					}
				};
				const auto convert_blend_func = [](reshadefx::pass_blend_func value) -> VkBlendFactor {
					switch (value)
					{
					case reshadefx::pass_blend_func::zero: return VK_BLEND_FACTOR_ZERO;
					default:
					case reshadefx::pass_blend_func::one: return VK_BLEND_FACTOR_ONE;
					case reshadefx::pass_blend_func::src_color: return VK_BLEND_FACTOR_SRC_COLOR;
					case reshadefx::pass_blend_func::src_alpha: return VK_BLEND_FACTOR_SRC_ALPHA;
					case reshadefx::pass_blend_func::inv_src_color: return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
					case reshadefx::pass_blend_func::inv_src_alpha: return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
					case reshadefx::pass_blend_func::dst_color: return VK_BLEND_FACTOR_DST_COLOR;
					case reshadefx::pass_blend_func::dst_alpha: return VK_BLEND_FACTOR_DST_ALPHA;
					case reshadefx::pass_blend_func::inv_dst_color: return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
					case reshadefx::pass_blend_func::inv_dst_alpha: return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
					}
				};
				const auto convert_stencil_op = [](reshadefx::pass_stencil_op value) -> VkStencilOp {
					switch (value)
					{
					case reshadefx::pass_stencil_op::zero: return VK_STENCIL_OP_ZERO;
					default:
					case reshadefx::pass_stencil_op::keep: return VK_STENCIL_OP_KEEP;
					case reshadefx::pass_stencil_op::invert: return VK_STENCIL_OP_INVERT;
					case reshadefx::pass_stencil_op::replace: return VK_STENCIL_OP_REPLACE;
					case reshadefx::pass_stencil_op::incr: return VK_STENCIL_OP_INCREMENT_AND_WRAP;
					case reshadefx::pass_stencil_op::incr_sat: return VK_STENCIL_OP_INCREMENT_AND_CLAMP;
					case reshadefx::pass_stencil_op::decr: return VK_STENCIL_OP_DECREMENT_AND_WRAP;
					case reshadefx::pass_stencil_op::decr_sat: return VK_STENCIL_OP_DECREMENT_AND_CLAMP;
					}
				};
				const auto convert_stencil_func = [](reshadefx::pass_stencil_func value) -> VkCompareOp {
					switch (value)
					{
					case reshadefx::pass_stencil_func::never: return VK_COMPARE_OP_NEVER;
					case reshadefx::pass_stencil_func::equal: return VK_COMPARE_OP_EQUAL;
					case reshadefx::pass_stencil_func::not_equal: return VK_COMPARE_OP_NOT_EQUAL;
					case reshadefx::pass_stencil_func::less: return VK_COMPARE_OP_LESS;
					case reshadefx::pass_stencil_func::less_equal: return VK_COMPARE_OP_LESS_OR_EQUAL;
					case reshadefx::pass_stencil_func::greater: return VK_COMPARE_OP_GREATER;
					case reshadefx::pass_stencil_func::greater_equal: return VK_COMPARE_OP_GREATER_OR_EQUAL;
					default:
					case reshadefx::pass_stencil_func::always: return VK_COMPARE_OP_ALWAYS;
					}
				};

				const VkRect2D scissor_rect = {
					{ 0, 0 },
					{ pass_info.viewport_width ? pass_info.viewport_width : _width,
					  pass_info.viewport_height ? pass_info.viewport_height : _height }
				};
				const VkViewport viewport_rect = {
					0.0f, 0.0f,
					static_cast<float>(scissor_rect.extent.width), static_cast<float>(scissor_rect.extent.height),
					0.0f, 1.0f
				};

				pass_data.begin_info.renderArea = scissor_rect;

				uint32_t num_color_attachments = 0;
				VkImageView attachment_views[9] = {};
				VkAttachmentReference attachment_refs[9] = {};
				VkAttachmentDescription attachment_descs[9] = {};
				VkPipelineColorBlendAttachmentState attachment_blends[8];
				attachment_blends[0].blendEnable = pass_info.blend_enable;
				attachment_blends[0].srcColorBlendFactor = convert_blend_func(pass_info.src_blend);
				attachment_blends[0].dstColorBlendFactor = convert_blend_func(pass_info.dest_blend);
				attachment_blends[0].colorBlendOp = convert_blend_op(pass_info.blend_op);
				attachment_blends[0].srcAlphaBlendFactor = convert_blend_func(pass_info.src_blend_alpha);
				attachment_blends[0].dstAlphaBlendFactor = convert_blend_func(pass_info.dest_blend_alpha);
				attachment_blends[0].alphaBlendOp = convert_blend_op(pass_info.blend_op_alpha);
				attachment_blends[0].colorWriteMask = pass_info.color_write_mask;

				for (uint32_t k = 0; k < 8 && !pass_info.render_target_names[k].empty(); ++k, ++num_color_attachments)
				{
					tex_data *const tex_impl = static_cast<tex_data *>(
						look_up_texture_by_name(pass_info.render_target_names[k]).impl);

					pass_data.modified_resources.push_back(tex_impl);

					attachment_views[k] = tex_impl->view[2 + pass_info.srgb_write_enable];
					attachment_blends[k] = attachment_blends[0];

					VkAttachmentReference &attachment_ref = attachment_refs[k];
					attachment_ref.attachment = k;
					attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

					VkAttachmentDescription &attachment_desc = attachment_descs[k];
					attachment_desc.format = tex_impl->formats[pass_info.srgb_write_enable];
					attachment_desc.samples = VK_SAMPLE_COUNT_1_BIT;
					attachment_desc.loadOp = pass_info.clear_render_targets ? VK_ATTACHMENT_LOAD_OP_CLEAR : pass_info.blend_enable ? VK_ATTACHMENT_LOAD_OP_LOAD : VK_ATTACHMENT_LOAD_OP_DONT_CARE;
					attachment_desc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
					attachment_desc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
					attachment_desc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
					attachment_desc.initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
					attachment_desc.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				}

				if (pass_info.clear_render_targets)
				{
					pass_data.begin_info.clearValueCount = num_color_attachments;
					pass_data.begin_info.pClearValues = pass_data.clear_values; // These are initialized to zero already
				}

				if (pass_info.render_target_names[0].empty())
				{
					num_color_attachments = 1;
					pass_data.begin_info.renderPass = _default_render_pass[pass_info.srgb_write_enable];
					pass_data.begin_info.framebuffer = VK_NULL_HANDLE; // Select the correct swap chain frame buffer during rendering
				}
				else
				{
					uint32_t num_stencil_attachments = 0;

					if (pass_info.stencil_enable && // Only need to attach stencil if stencil is actually used in this pass
						scissor_rect.extent.width == _width &&
						scissor_rect.extent.height == _height)
					{
						num_stencil_attachments = 1;
						const uint32_t stencil_idx = num_color_attachments;

						attachment_views[stencil_idx] = _effect_stencil_view;

						VkAttachmentReference &attachment_ref = attachment_refs[stencil_idx];
						attachment_ref.attachment = stencil_idx;
						attachment_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

						VkAttachmentDescription &attachment_desc = attachment_descs[stencil_idx];
						attachment_desc.format = _effect_stencil_format;
						attachment_desc.samples = VK_SAMPLE_COUNT_1_BIT;
						attachment_desc.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
						attachment_desc.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
						attachment_desc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
						attachment_desc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
						attachment_desc.initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
						attachment_desc.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
					}

					{   // Synchronize any writes to render targets in previous passes with reads from them in this pass
						VkSubpassDependency subdep = {};
						subdep.srcSubpass = VK_SUBPASS_EXTERNAL;
						subdep.dstSubpass = 0;
						subdep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
						subdep.dstStageMask = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;
						subdep.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
						subdep.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

						VkSubpassDescription subpass = {};
						subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
						subpass.colorAttachmentCount = num_color_attachments;
						subpass.pColorAttachments = attachment_refs;
						subpass.pDepthStencilAttachment = num_stencil_attachments ? &attachment_refs[num_color_attachments] : nullptr;

						VkRenderPassCreateInfo create_info { VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
						create_info.attachmentCount = num_color_attachments + num_stencil_attachments;
						create_info.pAttachments = attachment_descs;
						create_info.subpassCount = 1;
						create_info.pSubpasses = &subpass;
						create_info.dependencyCount = 1;
						create_info.pDependencies = &subdep;

						if (vk.CreateRenderPass(_device, &create_info, nullptr, &pass_data.begin_info.renderPass) != VK_SUCCESS)
							return false;
					}

					{   VkFramebufferCreateInfo create_info { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
						create_info.renderPass = pass_data.begin_info.renderPass;
						create_info.attachmentCount = num_color_attachments + num_stencil_attachments;
						create_info.pAttachments = attachment_views;
						create_info.width = scissor_rect.extent.width;
						create_info.height = scissor_rect.extent.height;
						create_info.layers = 1;

						if (vk.CreateFramebuffer(_device, &create_info, nullptr, &pass_data.begin_info.framebuffer) != VK_SUCCESS)
							return false;
					}
				}

				VkPipelineShaderStageCreateInfo stages[2];
				stages[0] = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
				stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
				stages[0].module = shader_modules.entry_points.at(pass_info.vs_entry_point);
				stages[0].pName = pass_info.vs_entry_point.c_str();
				stages[0].pSpecializationInfo = &spec_info;
				stages[1] = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
				stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
				stages[1].module = shader_modules.entry_points.at(pass_info.ps_entry_point);
				stages[1].pName = pass_info.ps_entry_point.c_str();
				stages[1].pSpecializationInfo = &spec_info;

				VkPipelineVertexInputStateCreateInfo vertex_info { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
				// No vertex attributes

				VkPipelineInputAssemblyStateCreateInfo ia_info { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
				switch (pass_info.topology)
				{
				case reshadefx::primitive_topology::point_list:
					ia_info.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
					break;
				case reshadefx::primitive_topology::line_list:
					ia_info.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
					break;
				case reshadefx::primitive_topology::line_strip:
					ia_info.topology = VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
					break;
				default:
				case reshadefx::primitive_topology::triangle_list:
					ia_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
					break;
				case reshadefx::primitive_topology::triangle_strip:
					ia_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
					break;
				}

				VkPipelineViewportStateCreateInfo viewport_info { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
				viewport_info.viewportCount = 1;
				viewport_info.pViewports = &viewport_rect;
				viewport_info.scissorCount = 1;
				viewport_info.pScissors = &scissor_rect;

				VkPipelineRasterizationStateCreateInfo raster_info { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
				raster_info.polygonMode = VK_POLYGON_MODE_FILL;
				raster_info.cullMode = VK_CULL_MODE_NONE;
				raster_info.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
				raster_info.lineWidth = 1.0f;

				VkPipelineMultisampleStateCreateInfo ms_info { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
				ms_info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

				VkPipelineColorBlendStateCreateInfo blend_info { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
				blend_info.attachmentCount = num_color_attachments;
				blend_info.pAttachments = attachment_blends;

				VkPipelineDepthStencilStateCreateInfo depth_info { VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
				depth_info.depthTestEnable = VK_FALSE;
				depth_info.depthWriteEnable = VK_FALSE;
				depth_info.depthCompareOp = VK_COMPARE_OP_ALWAYS;
				depth_info.stencilTestEnable = pass_info.stencil_enable;
				depth_info.front.failOp = convert_stencil_op(pass_info.stencil_op_fail);
				depth_info.front.passOp = convert_stencil_op(pass_info.stencil_op_pass);
				depth_info.front.depthFailOp = convert_stencil_op(pass_info.stencil_op_depth_fail);
				depth_info.front.compareOp = convert_stencil_func(pass_info.stencil_comparison_func);
				depth_info.front.compareMask = pass_info.stencil_read_mask;
				depth_info.front.writeMask = pass_info.stencil_write_mask;
				depth_info.front.reference = pass_info.stencil_reference_value;
				depth_info.back = depth_info.front;
				depth_info.minDepthBounds = 0.0f;
				depth_info.maxDepthBounds = 1.0f;

				VkGraphicsPipelineCreateInfo create_info { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
				create_info.stageCount = static_cast<uint32_t>(std::size(stages));
				create_info.pStages = stages;
				create_info.pVertexInputState = &vertex_info;
				create_info.pInputAssemblyState = &ia_info;
				create_info.pViewportState = &viewport_info;
				create_info.pRasterizationState = &raster_info;
				create_info.pMultisampleState = &ms_info;
				create_info.pDepthStencilState = &depth_info;
				create_info.pColorBlendState = &blend_info;
				create_info.layout = effect_data.pipeline_layout;
				create_info.renderPass = pass_data.begin_info.renderPass;

				const VkResult res = vk.CreateGraphicsPipelines(_device, VK_NULL_HANDLE, 1, &create_info, nullptr, &pass_data.pipeline);
				if (res != VK_SUCCESS)
				{
					LOG(ERROR) << "Failed to create graphics pipeline for pass " << pass_index << " in technique '" << technique.name << "'! Vulkan error code is " << res << '.';
					return false;
				}
			}

			pass_data.set[0] = sets[1 + total_pass_index];
			for (const reshadefx::sampler_info &info : pass_info.samplers)
			{
				VkWriteDescriptorSet &write = writes.emplace_back();
				write = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
				write.dstSet = pass_data.set[0];
				write.dstBinding = info.binding;
				write.descriptorCount = 1;
				write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
				write.pImageInfo = &sampler_bindings[info.binding];

				const texture &texture = look_up_texture_by_name(info.texture_name);
				if (texture.semantic.empty())
					continue;

				for (auto &binding : effect_data.texture_semantic_to_binding)
				{
					if (binding.semantic != texture.semantic)
						continue;

					binding.sets.push_back(pass_data.set[0]);
				}
			}
			pass_data.set[1] = sets[1 + num_passes + total_pass_index];
			for (const reshadefx::storage_info &info : pass_info.storages)
			{
				VkWriteDescriptorSet &write = writes.emplace_back();
				write = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
				write.dstSet = pass_data.set[1];
				write.dstBinding = info.binding;
				write.descriptorCount = 1;
				write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
				write.pImageInfo = &storage_bindings[info.binding];

				tex_data *const tex_impl = static_cast<tex_data *>(
					look_up_texture_by_name(info.texture_name).impl);

				pass_data.modified_resources.push_back(tex_impl);
			}
		}
	}

	vk.UpdateDescriptorSets(_device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);

	effect_data.image_bindings = std::move(sampler_bindings);

	return true;
}
void reshade::vulkan::runtime_vk::unload_effect(size_t index)
{
	// Make sure no effect resources are currently in use
	wait_for_command_buffers();

	for (technique &tech : _techniques)
	{
		if (tech.effect_index != index)
			continue;

		const auto impl = static_cast<technique_data *>(tech.impl);
		if (impl == nullptr)
			continue;

		for (pass_data &pass_data : impl->passes)
		{
			if (pass_data.begin_info.renderPass != _default_render_pass[0] &&
				pass_data.begin_info.renderPass != _default_render_pass[1])
				vk.DestroyRenderPass(_device, pass_data.begin_info.renderPass, nullptr);
			vk.DestroyFramebuffer(_device, pass_data.begin_info.framebuffer, nullptr);
			vk.DestroyPipeline(_device, pass_data.pipeline, nullptr);
		}

		delete impl;
		tech.impl = nullptr;
	}

	runtime::unload_effect(index);

	if (index < _effect_data.size())
	{
		effect_data &effect_data = _effect_data[index];

		vk.DestroyQueryPool(_device, effect_data.query_pool, nullptr);
		effect_data.query_pool = VK_NULL_HANDLE;
		vk.DestroyPipelineLayout(_device, effect_data.pipeline_layout, nullptr);
		effect_data.pipeline_layout = VK_NULL_HANDLE;
		vk.DestroyDescriptorSetLayout(_device, effect_data.sampler_layout, nullptr);
		effect_data.sampler_layout = VK_NULL_HANDLE;
		vk.DestroyDescriptorSetLayout(_device, effect_data.storage_layout, nullptr);
		effect_data.storage_layout = VK_NULL_HANDLE;
		vmaDestroyBuffer(_device_impl->_alloc, effect_data.ubo, effect_data.ubo_mem);
		effect_data.ubo = VK_NULL_HANDLE;
		effect_data.ubo_mem = VK_NULL_HANDLE;
		effect_data.texture_semantic_to_binding.clear();
		effect_data.image_bindings.clear();
	}
}
void reshade::vulkan::runtime_vk::unload_effects()
{
	// Make sure no effect resources are currently in use
	wait_for_command_buffers();

	for (technique &tech : _techniques)
	{
		const auto impl = static_cast<technique_data *>(tech.impl);
		if (impl == nullptr)
			continue;

		for (pass_data &pass_data : impl->passes)
		{
			if (pass_data.begin_info.renderPass != _default_render_pass[0] &&
				pass_data.begin_info.renderPass != _default_render_pass[1])
				vk.DestroyRenderPass(_device, pass_data.begin_info.renderPass, nullptr);
			vk.DestroyFramebuffer(_device, pass_data.begin_info.framebuffer, nullptr);
			vk.DestroyPipeline(_device, pass_data.pipeline, nullptr);
		}

		delete impl;
		tech.impl = nullptr;
	}

	runtime::unload_effects();

	if (_effect_descriptor_pool != VK_NULL_HANDLE)
	{
		vk.ResetDescriptorPool(_device, _effect_descriptor_pool, 0);
	}

	for (const effect_data &data : _effect_data)
	{
		vk.DestroyQueryPool(_device, data.query_pool, nullptr);
		vk.DestroyPipelineLayout(_device, data.pipeline_layout, nullptr);
		vk.DestroyDescriptorSetLayout(_device, data.sampler_layout, nullptr);
		vk.DestroyDescriptorSetLayout(_device, data.storage_layout, nullptr);
		vmaDestroyBuffer(_device_impl->_alloc, data.ubo, data.ubo_mem);
	}

	for (auto &[hash, sampler] : _effect_sampler_states)
	{
		vk.DestroySampler(_device, sampler, nullptr);
	}

	_effect_data.clear();
	_effect_sampler_states.clear();
}

bool reshade::vulkan::runtime_vk::init_texture(texture &texture)
{
	auto impl = new tex_data();
	texture.impl = impl;

	// Do not create resource if it is a special reference, those are set in 'render_technique' and 'update_texture_bindings'
	if (!texture.semantic.empty())
		return true;

	impl->width = texture.width;
	impl->height = texture.height;
	impl->levels = texture.levels;

	switch (texture.format)
	{
	case reshadefx::texture_format::r8:
		impl->formats[0] = VK_FORMAT_R8_UNORM;
		break;
	case reshadefx::texture_format::r16f:
		impl->formats[0] = VK_FORMAT_R16_SFLOAT;
		break;
	case reshadefx::texture_format::r32f:
		impl->formats[0] = VK_FORMAT_R32_SFLOAT;
		break;
	case reshadefx::texture_format::rg8:
		impl->formats[0] = VK_FORMAT_R8G8_UNORM;
		break;
	case reshadefx::texture_format::rg16:
		impl->formats[0] = VK_FORMAT_R16G16_UNORM;
		break;
	case reshadefx::texture_format::rg16f:
		impl->formats[0] = VK_FORMAT_R16G16_SFLOAT;
		break;
	case reshadefx::texture_format::rg32f:
		impl->formats[0] = VK_FORMAT_R32G32_SFLOAT;
		break;
	case reshadefx::texture_format::rgba8:
		impl->formats[0] = VK_FORMAT_R8G8B8A8_UNORM;
		impl->formats[1] = VK_FORMAT_R8G8B8A8_SRGB;
		break;
	case reshadefx::texture_format::rgba16:
		impl->formats[0] = VK_FORMAT_R16G16B16A16_UNORM;
		break;
	case reshadefx::texture_format::rgba16f:
		impl->formats[0] = VK_FORMAT_R16G16B16A16_SFLOAT;
		break;
	case reshadefx::texture_format::rgba32f:
		impl->formats[0] = VK_FORMAT_R32G32B32A32_SFLOAT;
		break;
	case reshadefx::texture_format::rgb10a2:
		impl->formats[0] = VK_FORMAT_A2R10G10B10_UNORM_PACK32;
		break;
	}

	// Need TRANSFER_DST for texture data upload
	VkImageUsageFlags usage_flags = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	if (texture.levels > 1) // Add required TRANSFER_SRC flag for mipmap generation
		usage_flags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	if (texture.render_target)
		usage_flags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	if (texture.storage_access)
		usage_flags |= VK_IMAGE_USAGE_STORAGE_BIT;

	VkImageCreateFlags image_flags = 0;
	// Add mutable format flag required to create a SRGB view of the image
	if (impl->formats[1] != VK_FORMAT_UNDEFINED)
		image_flags = VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;
	else
		impl->formats[1] = impl->formats[0];

	impl->image = create_image(
		texture.width, texture.height, texture.levels, impl->formats[0],
		usage_flags, VMA_MEMORY_USAGE_GPU_ONLY,
		image_flags, 0, &impl->image_mem);
	if (impl->image == VK_NULL_HANDLE)
		return false;
	set_debug_name_image(impl->image, texture.unique_name.c_str());

	// Create shader views
	impl->view[0] = create_image_view(impl->image, impl->formats[0], VK_REMAINING_MIP_LEVELS, VK_IMAGE_ASPECT_COLOR_BIT);
	impl->view[1] = impl->formats[0] != impl->formats[1] && !texture.storage_access ? // sRGB formats do not support storage usage
		create_image_view(impl->image, impl->formats[1], VK_REMAINING_MIP_LEVELS, VK_IMAGE_ASPECT_COLOR_BIT) :
		impl->view[0];

	// Create render target views (with a single level)
	if (texture.levels > 1)
	{
		impl->view[2] = create_image_view(impl->image, impl->formats[0], 1, VK_IMAGE_ASPECT_COLOR_BIT);
		impl->view[3] = impl->formats[0] != impl->formats[1] && !texture.storage_access ?
			create_image_view(impl->image, impl->formats[1], 1, VK_IMAGE_ASPECT_COLOR_BIT) :
			impl->view[2];
	}
	else
	{
		impl->view[2] = impl->view[0];
		impl->view[3] = impl->view[1];
	}

#if RESHADE_GUI
	// Only need to allocate descriptor sets for textures when push descriptors are not available
	if (_imgui.descriptor_pool != VK_NULL_HANDLE)
	{
		VkDescriptorSetAllocateInfo alloc_info { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
		alloc_info.descriptorPool = _imgui.descriptor_pool;
		alloc_info.descriptorSetCount = 1;
		assert(_imgui.descriptor_layout != VK_NULL_HANDLE);
		alloc_info.pSetLayouts = &_imgui.descriptor_layout;

		if (vk.AllocateDescriptorSets(_device, &alloc_info, &impl->descriptor_set) != VK_SUCCESS)
		{
			LOG(ERROR) << "Too many textures loaded. Only " << MAX_IMAGE_DESCRIPTOR_SETS << " textures can be active simultaneously in Vulkan.";
			return false;
		}

		VkWriteDescriptorSet write { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
		write.dstSet = impl->descriptor_set;
		write.dstBinding = 0;
		write.descriptorCount = 1;
		write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		const VkDescriptorImageInfo image_info { _imgui.sampler, impl->view[0], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
		write.pImageInfo = &image_info;

		vk.UpdateDescriptorSets(_device, 1, &write, 0, nullptr);
	}
#endif

	const VkCommandBuffer cmd_list = _cmd_impl->begin_commands();
	{
		transition_layout(vk, cmd_list, impl->image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

		// Clear texture to zero since by default its contents are undefined
		const VkClearColorValue clear_value = {};
		const VkImageSubresourceRange range = { VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS };
		vk.CmdClearColorImage(cmd_list, impl->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clear_value, 1, &range);

		// Transition to shader read image layout
		transition_layout(vk, cmd_list, impl->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

		_cmd_impl->flush_and_wait(_queue);
	}

	return true;
}
void reshade::vulkan::runtime_vk::upload_texture(const texture &texture, const uint8_t *pixels)
{
	auto impl = static_cast<tex_data *>(texture.impl);
	assert(impl != nullptr && texture.semantic.empty() && pixels != nullptr);

	// Allocate host memory for upload
	VkBuffer intermediate = VK_NULL_HANDLE;
	VmaAllocation intermediate_mem = VK_NULL_HANDLE;

	{   VkBufferCreateInfo create_info { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
		create_info.size = texture.width * texture.height * 4;
		create_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

		VmaAllocationCreateInfo alloc_info = {};
		alloc_info.usage = VMA_MEMORY_USAGE_CPU_ONLY;

		if (vmaCreateBuffer(_device_impl->_alloc, &create_info, &alloc_info, &intermediate, &intermediate_mem, nullptr) != VK_SUCCESS)
			return;
	}

	// Fill upload buffer with pixel data
	uint8_t *mapped_data = nullptr;
	if (vmaMapMemory(_device_impl->_alloc, intermediate_mem, reinterpret_cast<void **>(&mapped_data)) == VK_SUCCESS)
	{
		switch (texture.format)
		{
		case reshadefx::texture_format::r8:
			for (uint32_t y = 0; y < texture.height; ++y, mapped_data += texture.width * 1, pixels += texture.width * 4)
				for (uint32_t x = 0; x < texture.width; ++x)
					mapped_data[x] = pixels[x * 4];
			break;
		case reshadefx::texture_format::rg8:
			for (uint32_t y = 0; y < texture.height; ++y, mapped_data += texture.width * 2, pixels += texture.width * 4)
				for (uint32_t x = 0; x < texture.width; ++x)
					mapped_data[x * 2 + 0] = pixels[x * 4 + 0],
					mapped_data[x * 2 + 1] = pixels[x * 4 + 1];
			break;
		case reshadefx::texture_format::rgba8:
			for (uint32_t y = 0; y < texture.height; ++y, mapped_data += texture.width * 4, pixels += texture.width * 4)
				std::memcpy(mapped_data, pixels, texture.width * 4);
			break;
		default:
			mapped_data = nullptr;
			LOG(ERROR) << "Texture upload is not supported for format " << static_cast<unsigned int>(texture.format) << " of texture '" << texture.unique_name << "'!";
			break;
		}

		vmaUnmapMemory(_device_impl->_alloc, intermediate_mem);
	}

	if (mapped_data != nullptr)
	{
		const VkCommandBuffer cmd_list = _cmd_impl->begin_commands();

		transition_layout(vk, cmd_list, impl->image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
		{ // Copy data from upload buffer into target texture
			VkBufferImageCopy copy_region = {};
			copy_region.imageExtent = { texture.width, texture.height, 1u };
			copy_region.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };

			vk.CmdCopyBufferToImage(cmd_list, intermediate, impl->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy_region);
		}
		transition_layout(vk, cmd_list, impl->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

		generate_mipmaps(impl);

		_cmd_impl->flush_and_wait(_queue);
	}

	vmaDestroyBuffer(_device_impl->_alloc, intermediate, intermediate_mem);
}
void reshade::vulkan::runtime_vk::destroy_texture(texture &texture)
{
	if (texture.impl == nullptr)
		return;
	auto impl = static_cast<tex_data *>(texture.impl);

	// Make sure texture is not still in use before destroying it
	wait_for_command_buffers();

	vmaDestroyImage(_device_impl->_alloc, impl->image, impl->image_mem);
	if (impl->view[0] != VK_NULL_HANDLE)
		vk.DestroyImageView(_device, impl->view[0], nullptr);
	if (impl->view[1] != impl->view[0])
		vk.DestroyImageView(_device, impl->view[1], nullptr);
	if (impl->view[2] != impl->view[0])
		vk.DestroyImageView(_device, impl->view[2], nullptr);
	if (impl->view[3] != impl->view[2] && impl->view[3] != impl->view[1])
		vk.DestroyImageView(_device, impl->view[3], nullptr);
#if RESHADE_GUI
	if (impl->descriptor_set != VK_NULL_HANDLE)
		vk.FreeDescriptorSets(_device, _imgui.descriptor_pool, 1, &impl->descriptor_set);
#endif

	delete impl;
	texture.impl = nullptr;
}
void reshade::vulkan::runtime_vk::generate_mipmaps(const tex_data *impl)
{
	assert(impl != nullptr);

	if (impl->levels <= 1)
		return; // No need to generate mipmaps when texture does not have any

	int32_t width = impl->width;
	int32_t height = impl->height;
	const VkCommandBuffer cmd_list = _cmd_impl->begin_commands();

	for (uint32_t level = 1; level < impl->levels; ++level, width /= 2, height /= 2)
	{
		VkImageBlit blit;
		blit.srcOffsets[0] = { 0, 0, 0 };
		blit.srcOffsets[1] = { width, height, 1 };
		blit.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, level - 1, 0, 1 };
		blit.dstOffsets[0] = { 0, 0, 0 };
		blit.dstOffsets[1] = { width > 1 ? width / 2 : 1, height > 1 ? height / 2 : 1, 1 };
		blit.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, level, 0, 1 };

		transition_layout(vk, cmd_list, impl->image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, { VK_IMAGE_ASPECT_COLOR_BIT, level - 1, 1, 0, 1 });
		transition_layout(vk, cmd_list, impl->image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, { VK_IMAGE_ASPECT_COLOR_BIT, level, 1, 0, 1 });
		vk.CmdBlitImage(cmd_list, impl->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, impl->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);
		transition_layout(vk, cmd_list, impl->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, { VK_IMAGE_ASPECT_COLOR_BIT, level - 1, 1, 0, 1 });
		transition_layout(vk, cmd_list, impl->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, { VK_IMAGE_ASPECT_COLOR_BIT, level, 1, 0, 1 });
	}
}

void reshade::vulkan::runtime_vk::render_technique(technique &technique)
{
	const auto impl = static_cast<technique_data *>(technique.impl);
	effect_data &effect_data = _effect_data[technique.effect_index];

	// Evaluate queries from oldest frame in queue
	if (uint64_t timestamps[2];
		vk.GetQueryPoolResults(_device, effect_data.query_pool,
			impl->query_base_index + ((_framecount + 1) % NUM_QUERY_FRAMES) * 2, 2,
		sizeof(timestamps), timestamps, sizeof(uint64_t), VK_QUERY_RESULT_64_BIT) == VK_SUCCESS)
	{
		technique.average_gpu_duration.append(timestamps[1] - timestamps[0]);
	}

	const VkCommandBuffer cmd_list = _cmd_impl->begin_commands();

#ifndef NDEBUG
	const bool insert_debug_markers = vk.CmdDebugMarkerBeginEXT != nullptr && vk.CmdDebugMarkerEndEXT != nullptr;
	if (insert_debug_markers)
	{
		VkDebugMarkerMarkerInfoEXT debug_info { VK_STRUCTURE_TYPE_DEBUG_MARKER_MARKER_INFO_EXT };
		debug_info.pMarkerName = technique.name.c_str();
		debug_info.color[0] = 0.8f;
		debug_info.color[1] = 0.0f;
		debug_info.color[2] = 0.8f;
		debug_info.color[3] = 1.0f;

		vk.CmdDebugMarkerBeginEXT(cmd_list, &debug_info);
	}
#endif

	// Reset current queries and then write time stamp value
	vk.CmdResetQueryPool(cmd_list, effect_data.query_pool, impl->query_base_index + (_framecount % NUM_QUERY_FRAMES) * 2, 2);
	vk.CmdWriteTimestamp(cmd_list, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, effect_data.query_pool, impl->query_base_index + (_framecount % NUM_QUERY_FRAMES) * 2);

	RESHADE_ADDON_EVENT(reshade_before_effects, this, _cmd_impl);

	vk.CmdBindDescriptorSets(cmd_list, VK_PIPELINE_BIND_POINT_GRAPHICS, effect_data.pipeline_layout, 0, 1, &effect_data.ubo_set, 0, nullptr);
	if (impl->has_compute_passes)
		vk.CmdBindDescriptorSets(cmd_list, VK_PIPELINE_BIND_POINT_COMPUTE, effect_data.pipeline_layout, 0, 1, &effect_data.ubo_set, 0, nullptr);

	// Setup shader constants
	if (effect_data.ubo != VK_NULL_HANDLE)
		vk.CmdUpdateBuffer(cmd_list, effect_data.ubo, 0, _effects[technique.effect_index].uniform_data_storage.size(), _effects[technique.effect_index].uniform_data_storage.data());

	bool is_effect_stencil_cleared = false;
	bool needs_implicit_backbuffer_copy = true; // First pass always needs the back buffer updated

	for (size_t pass_index = 0; pass_index < technique.passes.size(); ++pass_index)
	{
		if (needs_implicit_backbuffer_copy)
		{
			// Save back buffer of previous pass
			const VkImageCopy copy_range = {
				{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 }, { 0, 0, 0 },
				{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 }, { 0, 0, 0 }, { _width, _height, 1 }
			};
			transition_layout(vk, cmd_list, _backbuffer_image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
			transition_layout(vk, cmd_list, _swapchain_images[_swap_index], VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
			vk.CmdCopyImage(cmd_list, _swapchain_images[_swap_index], VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, _backbuffer_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy_range);
			transition_layout(vk, cmd_list, _backbuffer_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
			transition_layout(vk, cmd_list, _swapchain_images[_swap_index], VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
		}

		const pass_data &pass_data = impl->passes[pass_index];
		const reshadefx::pass_info &pass_info = technique.passes[pass_index];

#ifndef NDEBUG
		if (insert_debug_markers)
		{
			const std::string pass_name = pass_info.name.empty() ? "Pass " + std::to_string(pass_index) : pass_info.name;

			VkDebugMarkerMarkerInfoEXT debug_info { VK_STRUCTURE_TYPE_DEBUG_MARKER_MARKER_INFO_EXT };
			debug_info.pMarkerName = pass_name.c_str();
			debug_info.color[0] = 0.8f;
			debug_info.color[1] = 0.8f;
			debug_info.color[2] = 0.8f;
			debug_info.color[3] = 1.0f;

			vk.CmdDebugMarkerBeginEXT(cmd_list, &debug_info);
		}
#endif

		if (!pass_info.cs_entry_point.empty())
		{
			// Compute shaders do not write to the back buffer, so no update necessary
			needs_implicit_backbuffer_copy = false;

			for (const tex_data *storage_resource : pass_data.modified_resources)
				transition_layout(vk, cmd_list, storage_resource->image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);

			vk.CmdBindPipeline(cmd_list, VK_PIPELINE_BIND_POINT_COMPUTE, pass_data.pipeline);
			vk.CmdBindDescriptorSets(cmd_list, VK_PIPELINE_BIND_POINT_COMPUTE, effect_data.pipeline_layout, 1, 2, pass_data.set, 0, nullptr);
			vk.CmdDispatch(cmd_list, pass_info.viewport_width, pass_info.viewport_height, pass_info.viewport_dispatch_z);

			for (const tex_data *storage_resource : pass_data.modified_resources)
				transition_layout(vk, cmd_list, storage_resource->image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		}
		else
		{
			if (pass_info.stencil_enable && !is_effect_stencil_cleared)
			{
				is_effect_stencil_cleared = true;

				const VkImageSubresourceRange clear_range = { VK_IMAGE_ASPECT_STENCIL_BIT, 0, 1, 0, 1 };
				const VkClearDepthStencilValue clear_value = { 1.0f, 0 };
				transition_layout(vk, cmd_list, _effect_stencil, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, { aspect_flags_from_format(_effect_stencil_format), 0, 1, 0, 1 });
				vk.CmdClearDepthStencilImage(cmd_list, _effect_stencil, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clear_value, 1, &clear_range);
				transition_layout(vk, cmd_list, _effect_stencil, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, { aspect_flags_from_format(_effect_stencil_format), 0, 1, 0, 1 });
			}

			VkRenderPassBeginInfo begin_info = pass_data.begin_info;
			if (begin_info.framebuffer == VK_NULL_HANDLE)
			{
				begin_info.framebuffer = _swapchain_frames[_swap_index * 2 + pass_info.srgb_write_enable];
				needs_implicit_backbuffer_copy = true;
			}
			else
			{
				needs_implicit_backbuffer_copy = false;
			}

			vk.CmdBeginRenderPass(cmd_list, &begin_info, VK_SUBPASS_CONTENTS_INLINE);

			vk.CmdBindPipeline(cmd_list, VK_PIPELINE_BIND_POINT_GRAPHICS, pass_data.pipeline);
			vk.CmdBindDescriptorSets(cmd_list, VK_PIPELINE_BIND_POINT_GRAPHICS, effect_data.pipeline_layout, 1, 1, pass_data.set, 0, nullptr);
			vk.CmdDraw(cmd_list, pass_info.num_vertices, 1, 0, 0);

			vk.CmdEndRenderPass(cmd_list);
		}

		// Generate mipmaps for modified resources
		for (const tex_data *texture : pass_data.modified_resources)
			generate_mipmaps(texture);

#ifndef NDEBUG
		if (insert_debug_markers)
			vk.CmdDebugMarkerEndEXT(cmd_list);
#endif
	}

	RESHADE_ADDON_EVENT(reshade_after_effects, this, _cmd_impl);

	vk.CmdWriteTimestamp(cmd_list, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, effect_data.query_pool, impl->query_base_index + (_framecount % NUM_QUERY_FRAMES) * 2 + 1);

#ifndef NDEBUG
	if (insert_debug_markers)
		vk.CmdDebugMarkerEndEXT(cmd_list);
#endif
}

void reshade::vulkan::runtime_vk::wait_for_command_buffers()
{
	// Wait for all queues to finish to ensure no command buffers are in flight after this call
	vk.DeviceWaitIdle(_device);
#ifndef NDEBUG
	_wait_for_idle_happened = true;
#endif

	// Make sure any pending work gets executed here, so it is not enqueued later in 'on_present' (at which point the referenced objects may have been destroyed by the code calling this)
	// Do this after waiting for idle, since it should run after all work by the application is done and is synchronous anyway
	_cmd_impl->flush_and_wait(_queue);
}

void reshade::vulkan::runtime_vk::set_debug_name(uint64_t object, VkDebugReportObjectTypeEXT type, const char *name) const
{
	if (vk.DebugMarkerSetObjectNameEXT != nullptr)
	{
		VkDebugMarkerObjectNameInfoEXT name_info { VK_STRUCTURE_TYPE_DEBUG_MARKER_OBJECT_NAME_INFO_EXT };
		name_info.object = object;
		name_info.objectType = type;
		name_info.pObjectName = name;

		vk.DebugMarkerSetObjectNameEXT(_device, &name_info);
	}
}

VkImage reshade::vulkan::runtime_vk::create_image(uint32_t width, uint32_t height, uint32_t levels, VkFormat format,
	VkImageUsageFlags usage, VmaMemoryUsage mem_usage,
	VkImageCreateFlags flags, VmaAllocationCreateFlags mem_flags, VmaAllocation *out_mem)
{
	VkImageCreateInfo create_info { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
	create_info.flags = flags;
	create_info.imageType = VK_IMAGE_TYPE_2D;
	create_info.format = format;
	create_info.extent = { width, height, 1u };
	create_info.mipLevels = levels;
	create_info.arrayLayers = 1;
	create_info.samples = VK_SAMPLE_COUNT_1_BIT;
	create_info.tiling = VK_IMAGE_TILING_OPTIMAL;
	create_info.usage = usage;
	create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	create_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	const VkFormat format_list[2] = { make_format_normal(format), make_format_srgb(format) };
	VkImageFormatListCreateInfoKHR format_list_info { VK_STRUCTURE_TYPE_IMAGE_FORMAT_LIST_CREATE_INFO_KHR };

	if (format_list[0] != format_list[1] && (flags & VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT) != 0)
	{
		format_list_info.viewFormatCount = 2;
		format_list_info.pViewFormats = format_list;

		create_info.pNext = &format_list_info;
	}

	VmaAllocation alloc = VK_NULL_HANDLE;
	VmaAllocationCreateInfo alloc_info = {};
	alloc_info.flags = mem_flags;
	alloc_info.usage = mem_usage;

	VkImage image = VK_NULL_HANDLE;
	const VkResult res = vmaCreateImage(_device_impl->_alloc, &create_info, &alloc_info, &image, &alloc, nullptr);
	if (res != VK_SUCCESS)
	{
		LOG(ERROR) << "Failed to create image! Vulkan error code is " << res << '.';
		LOG(DEBUG) << "> Details: Width = " << width << ", Height = " << height << ", Levels = " << levels << ", Format = " << format << ", Usage = " << std::hex << usage << std::dec;

		if (out_mem != nullptr)
			*out_mem = VK_NULL_HANDLE;
		return VK_NULL_HANDLE;
	}

	if (out_mem != nullptr)
		*out_mem = alloc;
	else
		_allocations.push_back(alloc);

	return image;
}
VkBuffer reshade::vulkan::runtime_vk::create_buffer(VkDeviceSize size,
	VkBufferUsageFlags usage, VmaMemoryUsage mem_usage,
	VkBufferCreateFlags flags, VmaAllocationCreateFlags mem_flags, VmaAllocation *out_mem)
{
	VkBufferCreateInfo create_info { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
	create_info.flags = flags;
	create_info.size = size;
	create_info.usage = usage;
	create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VmaAllocation alloc = VK_NULL_HANDLE;
	VmaAllocationCreateInfo alloc_info = {};
	alloc_info.flags = mem_flags;
	alloc_info.usage = mem_usage;

	// Make sure host visible allocations are coherent, since no explicit flushing is performed
	if (mem_usage == VMA_MEMORY_USAGE_CPU_TO_GPU)
		alloc_info.requiredFlags |= VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

	VkBuffer buffer = VK_NULL_HANDLE;
	const VkResult res = vmaCreateBuffer(_device_impl->_alloc, &create_info, &alloc_info, &buffer, &alloc, nullptr);
	if (res != VK_SUCCESS)
	{
		LOG(ERROR) << "Failed to create buffer! Vulkan error code is " << res << '.';
		LOG(DEBUG) << "> Details: Size = " << size << ", Usage = " << std::hex << usage << std::dec;

		if (out_mem != nullptr)
			*out_mem = VK_NULL_HANDLE;
		return VK_NULL_HANDLE;
	}

	if (out_mem != nullptr)
		*out_mem = alloc;
	else
		_allocations.push_back(alloc);

	return buffer;
}
VkImageView reshade::vulkan::runtime_vk::create_image_view(VkImage image, VkFormat format, uint32_t levels, VkImageAspectFlags aspect)
{
	VkImageViewCreateInfo create_info { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
	create_info.image = image;
	create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
	create_info.format = format;
	create_info.components = { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY };
	create_info.subresourceRange = { aspect, 0, levels, 0, 1 };

	VkImageView view = VK_NULL_HANDLE;
	if (vk.CreateImageView(_device, &create_info, nullptr, &view) != VK_SUCCESS)
		return VK_NULL_HANDLE;

	return view;
}

void reshade::vulkan::runtime_vk::update_texture_bindings(const char *semantic, api::resource_view_handle srv)
{
	const VkImageView view = (VkImageView)srv.handle;

	VkDescriptorImageInfo image_binding = { VK_NULL_HANDLE, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
	if (view != VK_NULL_HANDLE)
	{
		image_binding.imageView = view;

		_texture_semantic_bindings[semantic] = view;
	}
	else
	{
		image_binding.imageView = _empty_depth_image_view;

		_texture_semantic_bindings.erase(semantic);
	}

	// Make sure all previous frames have finished before freeing the image view and updating descriptors (since they may be in use otherwise)
	wait_for_command_buffers();

	// Update image bindings
	std::vector<VkWriteDescriptorSet> writes;

	for (effect_data &effect_data : _effect_data)
	{
		for (const auto &binding : effect_data.texture_semantic_to_binding)
		{
			if (binding.semantic != semantic)
				continue;

			for (VkDescriptorSet set : binding.sets)
			{
				// Set sampler handle, which should be the same for all writes
				assert(image_binding.sampler == VK_NULL_HANDLE || image_binding.sampler == effect_data.image_bindings[binding.index].sampler);
				image_binding.sampler = effect_data.image_bindings[binding.index].sampler;

				VkWriteDescriptorSet &write = writes.emplace_back();
				write = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
				write.dstSet = set;
				write.dstBinding = binding.index;
				write.descriptorCount = 1;
				write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
				write.pImageInfo = &image_binding;
			}
		}
	}

	vk.UpdateDescriptorSets(_device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
}
