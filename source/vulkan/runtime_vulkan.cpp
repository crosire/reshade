/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "log.hpp"
#include "runtime_vulkan.hpp"
#include "runtime_objects.hpp"
#include "resource_loading.hpp"
#include <imgui.h>

#define check_result(call) \
	if ((call) != VK_SUCCESS) \
		return

namespace reshade::vulkan
{
	struct vulkan_tex_data : base_object
	{
		~vulkan_tex_data()
		{
			runtime->_funcs.vkDestroyImage(runtime->_device, image, nullptr);
			runtime->_funcs.vkDestroyImageView(runtime->_device, view[0], nullptr);
			if (view[1] != view[0])
				runtime->_funcs.vkDestroyImageView(runtime->_device, view[1], nullptr);
			if (view[2] != view[0])
				runtime->_funcs.vkDestroyImageView(runtime->_device, view[2], nullptr);
			if (view[3] != view[2] && view[3] != view[1])
				runtime->_funcs.vkDestroyImageView(runtime->_device, view[3], nullptr);
		}

		VkFormat formats[2] = {};
		VkImage image = VK_NULL_HANDLE;
		VkImageView view[4] = {};
		VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
		runtime_vulkan *runtime = nullptr;
	};

	struct vulkan_effect_data
	{
		VkDescriptorSet set[2] = {};
		VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
		VkDescriptorSetLayout set_layout = VK_NULL_HANDLE;
		VkBuffer ubo = VK_NULL_HANDLE;
		VkDeviceMemory ubo_mem = VK_NULL_HANDLE;
		VkDeviceSize storage_size = 0;
		VkDeviceSize storage_offset = 0;
	};

	struct vulkan_technique_data : base_object
	{
	};

	struct vulkan_pass_data : base_object
	{
		VkPipeline pipeline = VK_NULL_HANDLE;
		VkClearValue clear_values[8] = {};
		VkRenderPassBeginInfo begin_info = { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
	};

	static uint32_t find_memory_type_index(const VkPhysicalDeviceMemoryProperties &props, VkMemoryPropertyFlags flags, uint32_t type_bits)
	{
		for (uint32_t i = 0; i < props.memoryTypeCount; i++)
			if ((props.memoryTypes[i].propertyFlags & flags) == flags && type_bits & (1 << i))
				return i;
		return std::numeric_limits<uint32_t>::max();
	}
}

reshade::vulkan::runtime_vulkan::runtime_vulkan(VkDevice device, VkPhysicalDevice physical_device, const vk_device_table &table) :
	_device(device), _physical_device(physical_device), _funcs(table)
{
	_renderer_id = 0x20000;

	table.vkGetPhysicalDeviceMemoryProperties(physical_device, &_memory_props);
}

VkImage reshade::vulkan::runtime_vulkan::create_image(uint32_t width, uint32_t height, uint32_t levels, VkFormat format, VkImageUsageFlags usage_flags, VkMemoryPropertyFlags mem_flags, VkImageCreateFlags flags)
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
	create_info.usage = usage_flags;
	create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	create_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	vk_handle<VK_OBJECT_TYPE_IMAGE> res(_device, _funcs);
	check_result(_funcs.vkCreateImage(_device, &create_info, nullptr, &res)) VK_NULL_HANDLE;

	if (mem_flags != 0)
	{
		VkMemoryRequirements reqs = {};
		_funcs.vkGetImageMemoryRequirements(_device, res, &reqs);

		// TODO: Implement memory allocator. Shouldn't put everything in dedicated allocations, but eh.
		VkMemoryAllocateInfo alloc_info { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
		alloc_info.allocationSize = reqs.size;
		alloc_info.memoryTypeIndex = find_memory_type_index(_memory_props, mem_flags, reqs.memoryTypeBits);
		VkMemoryDedicatedAllocateInfo dedicated_info { VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO };
		alloc_info.pNext = &dedicated_info;
		dedicated_info.image = res;

		vk_handle<VK_OBJECT_TYPE_DEVICE_MEMORY> mem(_device, _funcs);
		check_result(_funcs.vkAllocateMemory(_device, &alloc_info, nullptr, &mem)) VK_NULL_HANDLE;
		check_result(_funcs.vkBindImageMemory(_device, res, mem, 0)) VK_NULL_HANDLE;

		_allocations.push_back(mem.release());
	}

	return res.release();
}
VkBuffer reshade::vulkan::runtime_vulkan::create_buffer(VkDeviceSize size, VkBufferUsageFlags usage_flags, VkMemoryPropertyFlags mem_flags)
{
	VkBufferCreateInfo create_info { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
	create_info.size = size;
	create_info.usage = usage_flags;
	create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	vk_handle<VK_OBJECT_TYPE_BUFFER> res(_device, _funcs);
	check_result(_funcs.vkCreateBuffer(_device, &create_info, nullptr, &res)) VK_NULL_HANDLE;

	if (mem_flags != 0)
	{
		VkMemoryRequirements reqs = {};
		_funcs.vkGetBufferMemoryRequirements(_device, res, &reqs);

		// TODO: Implement memory allocator. Shouldn't put everything in dedicated allocations, but eh.
		VkMemoryAllocateInfo alloc_info { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
		alloc_info.allocationSize = reqs.size;
		alloc_info.memoryTypeIndex = find_memory_type_index(_memory_props, mem_flags, reqs.memoryTypeBits);
		VkMemoryDedicatedAllocateInfo dedicated_info { VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO };
		alloc_info.pNext = &dedicated_info;
		dedicated_info.buffer = res;

		vk_handle<VK_OBJECT_TYPE_DEVICE_MEMORY> mem(_device, _funcs);
		check_result(_funcs.vkAllocateMemory(_device, &alloc_info, nullptr, &mem)) VK_NULL_HANDLE;
		check_result(_funcs.vkBindBufferMemory(_device, res, mem, 0)) VK_NULL_HANDLE;

		_allocations.push_back(mem.release());
	}

	return res.release();
}
VkImageView reshade::vulkan::runtime_vulkan::create_image_view(VkImage image, VkFormat format, uint32_t levels)
{
	VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT;
	if (format >= VK_FORMAT_D16_UNORM_S8_UINT && format <= VK_FORMAT_D32_SFLOAT_S8_UINT)
		aspect = VK_IMAGE_ASPECT_STENCIL_BIT;

	VkImageViewCreateInfo create_info { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
	create_info.image = image;
	create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
	create_info.format = format;
	create_info.components = { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY };
	create_info.subresourceRange = { aspect, 0, levels, 0, 1 };

	vk_handle<VK_OBJECT_TYPE_IMAGE_VIEW> res(_device, _funcs);
	check_result(_funcs.vkCreateImageView(_device, &create_info, nullptr, &res)) VK_NULL_HANDLE;

	return res.release();
}

void reshade::vulkan::runtime_vulkan::set_object_name(uint64_t handle, VkDebugReportObjectTypeEXT type, const char *name) const
{
#ifdef _DEBUG
	if (_funcs.vkDebugMarkerSetObjectNameEXT == nullptr)
		return;

	VkDebugMarkerObjectNameInfoEXT name_info { VK_STRUCTURE_TYPE_DEBUG_MARKER_OBJECT_NAME_INFO_EXT };
	name_info.object = handle;
	name_info.objectType = type;
	name_info.pObjectName = name;

	_funcs.vkDebugMarkerSetObjectNameEXT(_device, &name_info);
#endif
}

bool reshade::vulkan::runtime_vulkan::on_init(VkSwapchainKHR swapchain, const VkSwapchainCreateInfoKHR &desc, HWND hwnd)
{
	// Update swapchain to the new one
	_swapchain = swapchain;

	RECT window_rect = {};
	GetClientRect(hwnd, &window_rect);

	_width = desc.imageExtent.width;
	_height = desc.imageExtent.height;
	_window_width = window_rect.right - window_rect.left;
	_window_height = window_rect.bottom - window_rect.top;
	_backbuffer_format = desc.imageFormat;
	_backbuffer_color_depth = _backbuffer_format >= VK_FORMAT_A2R10G10B10_UNORM_PACK32 && _backbuffer_format <= VK_FORMAT_A2B10G10R10_SINT_PACK32 ? 10 : 8;

	const uint32_t queue_family_index = 0; // TODO
	_funcs.vkGetDeviceQueue(_device, queue_family_index, 0, &_current_queue);

	_backbuffer_texture = create_image(
		_width, _height, 1, _backbuffer_format,
		VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	if (_backbuffer_texture == VK_NULL_HANDLE)
		return false;
	_backbuffer_texture_view = create_image_view(_backbuffer_texture, _backbuffer_format);
	if (_backbuffer_texture_view == VK_NULL_HANDLE)
		return false;

	_default_depthstencil = create_image(
		_width, _height, 1, VK_FORMAT_D24_UNORM_S8_UINT,
		VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	if (_default_depthstencil == VK_NULL_HANDLE)
		return false;
	_default_depthstencil_view = create_image_view(_default_depthstencil, VK_FORMAT_D24_UNORM_S8_UINT);
	if (_default_depthstencil_view == VK_NULL_HANDLE)
		return false;

	{   VkAttachmentReference attachment_refs[2] = {};
		attachment_refs[0].attachment = 0;
		attachment_refs[0].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		attachment_refs[1].attachment = 1;
		attachment_refs[1].layout = VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL;
		VkAttachmentDescription attachment_descs[2] = {};
		attachment_descs[0].format = _backbuffer_format;
		attachment_descs[0].samples = VK_SAMPLE_COUNT_1_BIT;
		attachment_descs[0].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
		attachment_descs[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachment_descs[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachment_descs[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachment_descs[0].initialLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		attachment_descs[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		attachment_descs[1].format = VK_FORMAT_D24_UNORM_S8_UINT;
		attachment_descs[1].samples = VK_SAMPLE_COUNT_1_BIT;
		attachment_descs[1].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachment_descs[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachment_descs[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
		attachment_descs[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachment_descs[1].initialLayout = VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL;
		attachment_descs[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL;

		VkSubpassDependency subdep = {};
		subdep.srcSubpass = VK_SUBPASS_EXTERNAL;
		subdep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		subdep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		subdep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
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

		check_result(_funcs.vkCreateRenderPass(_device, &create_info, nullptr, &_default_render_pass)) false;
	}

	uint32_t num_images = 0;
	check_result(_funcs.vkGetSwapchainImagesKHR(_device, _swapchain, &num_images, nullptr)) false;
	_swapchain_images.resize(num_images);
	check_result(_funcs.vkGetSwapchainImagesKHR(_device, _swapchain, &num_images, _swapchain_images.data())) false;

	assert(desc.imageUsage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);

	_render_area = desc.imageExtent;

	_cmd_pool.resize(num_images);
	_swapchain_views.resize(num_images);
	_swapchain_frames.resize(num_images);

	for (uint32_t i = 0; i < num_images; ++i)
	{
		_swapchain_views[i] = create_image_view(_swapchain_images[i], desc.imageFormat);

		const VkImageView attachment_views[2] = { _swapchain_views[i], _default_depthstencil_view };

		{   VkFramebufferCreateInfo create_info { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
			create_info.renderPass = _default_render_pass;
			create_info.attachmentCount = 2;
			create_info.pAttachments = attachment_views;
			create_info.width = desc.imageExtent.width;
			create_info.height = desc.imageExtent.height;
			create_info.layers = 1;

			check_result(_funcs.vkCreateFramebuffer(_device, &create_info, nullptr, &_swapchain_frames[i])) false;
		}

		{   VkCommandPoolCreateInfo create_info { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
			create_info.queueFamilyIndex = queue_family_index;

			check_result(_funcs.vkCreateCommandPool(_device, &create_info, nullptr, &_cmd_pool[i])) false;
		}
	}

	{   VkFenceCreateInfo create_info { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };

		check_result(_funcs.vkCreateFence(_device, &create_info, nullptr, &_wait_fence)) false;
	}

	{   const VkDescriptorPoolSize pool_sizes[] = {
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 }, // Only need one global UBO per set
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 128 } // Limit to 128 image bindings per set for now
		};

		VkDescriptorPoolCreateInfo create_info { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
		create_info.maxSets = 100; // Limit to 100 effects for now
		create_info.poolSizeCount = _countof(pool_sizes);
		create_info.pPoolSizes = pool_sizes;

		check_result(_funcs.vkCreateDescriptorPool(_device, &create_info, nullptr, &_effect_descriptor_pool)) false;
	}

	{   const VkDescriptorSetLayoutBinding bindings = { 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_ALL_GRAPHICS };
		VkDescriptorSetLayoutCreateInfo create_info { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
		create_info.bindingCount = 1;
		create_info.pBindings = &bindings;

		check_result(_funcs.vkCreateDescriptorSetLayout(_device, &create_info, nullptr, &_effect_ubo_layout)) false;
	}

#if RESHADE_GUI
	if (!init_imgui_resources())
		return false;
#endif

	return runtime::on_init(hwnd);
}
void reshade::vulkan::runtime_vulkan::on_reset()
{
	runtime::on_reset();

	_funcs.vkDeviceWaitIdle(_device);

	_funcs.vkDestroyDescriptorSetLayout(_device, _effect_ubo_layout, nullptr);
	_funcs.vkDestroyDescriptorPool(_device, _effect_descriptor_pool, nullptr);

	_funcs.vkDestroyImageView(_device, _backbuffer_texture_view, nullptr);
	_funcs.vkDestroyImageView(_device, _default_depthstencil_view, nullptr);
	_funcs.vkDestroyImage(_device, _backbuffer_texture, nullptr);
	_funcs.vkDestroyImage(_device, _default_depthstencil, nullptr);

	_funcs.vkDestroyRenderPass(_device, _default_render_pass, nullptr);
	_default_render_pass = VK_NULL_HANDLE;
	for (VkImageView view : _swapchain_views)
		_funcs.vkDestroyImageView(_device, view, nullptr);
	_swapchain_views.clear();
	for (VkFramebuffer frame : _swapchain_frames)
		_funcs.vkDestroyFramebuffer(_device, frame, nullptr);
	_swapchain_frames.clear();
	_swapchain_images.clear();

	_funcs.vkDestroyFence(_device, _wait_fence, nullptr);
	_wait_fence = VK_NULL_HANDLE;
	for (VkCommandPool pool : _cmd_pool)
		_funcs.vkDestroyCommandPool(_device, pool, nullptr);
	_cmd_pool.clear();

#if RESHADE_GUI
	_funcs.vkDestroyPipeline(_device, _imgui_pipeline, nullptr);
	_imgui_pipeline = VK_NULL_HANDLE;
	_funcs.vkDestroyPipelineLayout(_device, _imgui_pipeline_layout, nullptr);
	_imgui_pipeline_layout = VK_NULL_HANDLE;
	_funcs.vkDestroyDescriptorSetLayout(_device, _imgui_descriptor_set_layout, nullptr);
	_imgui_descriptor_set_layout = VK_NULL_HANDLE;

	_funcs.vkDestroySampler(_device, _imgui_font_sampler, nullptr);
	_imgui_font_sampler = VK_NULL_HANDLE;
	_funcs.vkDestroyBuffer(_device, _imgui_index_buffer, nullptr);
	_imgui_index_buffer = VK_NULL_HANDLE;
	_imgui_index_buffer_size = 0;
	_funcs.vkDestroyBuffer(_device, _imgui_vertex_buffer, nullptr);
	_imgui_vertex_buffer = VK_NULL_HANDLE;
	_imgui_vertex_buffer_size = 0;
	_funcs.vkFreeMemory(_device, _imgui_vertex_mem, nullptr);
	_imgui_vertex_mem = VK_NULL_HANDLE;
#endif

	for (VkDeviceMemory allocation : _allocations)
		_funcs.vkFreeMemory(_device, allocation, nullptr);
	_allocations.clear();
}

void reshade::vulkan::runtime_vulkan::on_present(uint32_t swapchain_image_index)
{
	_swap_index = swapchain_image_index;

	_funcs.vkQueueWaitIdle(_current_queue); // TOOD
	_funcs.vkResetCommandPool(_device, _cmd_pool[swapchain_image_index], 0);

	update_and_render_effects();
	runtime::on_present();
}

void reshade::vulkan::runtime_vulkan::capture_screenshot(uint8_t *buffer) const
{
	vk_handle<VK_OBJECT_TYPE_IMAGE> intermediate(_device, _funcs);
	vk_handle<VK_OBJECT_TYPE_DEVICE_MEMORY> intermediate_mem(_device, _funcs);

	{   VkImageCreateInfo create_info { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
		create_info.imageType = VK_IMAGE_TYPE_2D;
		create_info.format = _backbuffer_format;
		create_info.extent = { _width, _height, 1u };
		create_info.mipLevels = 1;
		create_info.arrayLayers = 1;
		create_info.samples = VK_SAMPLE_COUNT_1_BIT;
		create_info.tiling = VK_IMAGE_TILING_LINEAR;
		create_info.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		create_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

		check_result(_funcs.vkCreateImage(_device, &create_info, nullptr, &intermediate));

		VkMemoryRequirements reqs = {};
		_funcs.vkGetImageMemoryRequirements(_device, intermediate, &reqs);

		VkMemoryAllocateInfo alloc_info { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
		alloc_info.allocationSize = reqs.size;
		alloc_info.memoryTypeIndex = find_memory_type_index(_memory_props,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, reqs.memoryTypeBits);

		vk_handle<VK_OBJECT_TYPE_DEVICE_MEMORY> mem(_device, _funcs);
		check_result(_funcs.vkAllocateMemory(_device, &alloc_info, nullptr, &intermediate_mem));
		check_result(_funcs.vkBindImageMemory(_device, intermediate, intermediate_mem, 0));
	}

	const VkCommandBuffer cmd_list = create_command_list();
	if (cmd_list == VK_NULL_HANDLE)
		return;

	transition_layout(cmd_list, intermediate, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
	transition_layout(cmd_list, _swapchain_images[_swap_index], VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
	{
		VkImageBlit blit;
		blit.srcOffsets[0] = { 0, 0, 0 };
		blit.srcOffsets[1] = { int32_t(_width), int32_t(_height), 1 };
		blit.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
		blit.dstOffsets[0] = { 0, 0, 0 };
		blit.dstOffsets[1] = { int32_t(_width), int32_t(_height), 1 };
		blit.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };

		_funcs.vkCmdBlitImage(cmd_list, _swapchain_images[_swap_index], VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, intermediate, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_NEAREST);
	}
	transition_layout(cmd_list, intermediate, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);
	transition_layout(cmd_list, _swapchain_images[_swap_index], VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

	// Execute and wait for completion
	execute_command_list(cmd_list);

	// Get data layout of the intermediate image
	VkImageSubresource subresource { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0 };
	VkSubresourceLayout subresource_layout = {};
	_funcs.vkGetImageSubresourceLayout(_device, intermediate, &subresource, &subresource_layout);

	// Copy data from intermediate image into output buffer
	uint8_t *mapped_data;
	check_result(_funcs.vkMapMemory(_device, intermediate_mem, 0, VK_WHOLE_SIZE, 0, reinterpret_cast<void **>(&mapped_data)));

	const size_t data_pitch = _width * 4;
	const VkDeviceSize download_pitch = subresource_layout.rowPitch;

	for (uint32_t y = 0; y < _height; y++, buffer += data_pitch, mapped_data += download_pitch)
	{
		if (_backbuffer_format == VK_FORMAT_A2R10G10B10_UNORM_PACK32 ||
			_backbuffer_format == VK_FORMAT_A2R10G10B10_SNORM_PACK32 ||
			_backbuffer_format == VK_FORMAT_A2R10G10B10_USCALED_PACK32 ||
			_backbuffer_format == VK_FORMAT_A2R10G10B10_SSCALED_PACK32)
		{
			for (uint32_t x = 0; x < data_pitch; x += 4)
			{
				const uint32_t rgba = *reinterpret_cast<const uint32_t *>(mapped_data + x);
				// Divide by 4 to get 10-bit range (0-1023) into 8-bit range (0-255)
				buffer[x + 0] = ((rgba & 0x3FF) / 4) & 0xFF;
				buffer[x + 1] = (((rgba & 0xFFC00) >> 10) / 4) & 0xFF;
				buffer[x + 2] = (((rgba & 0x3FF00000) >> 20) / 4) & 0xFF;
				buffer[x + 3] = 0xFF;
			}
		}
		else
		{
			memcpy(buffer, mapped_data, data_pitch);

			for (uint32_t x = 0; x < data_pitch; x += 4)
				buffer[x + 3] = 0xFF; // Clear alpha channel
		}
	}

	_funcs.vkUnmapMemory(_device, intermediate_mem);
}

bool reshade::vulkan::runtime_vulkan::init_texture(texture &info)
{
	info.impl = std::make_unique<vulkan_tex_data>();
	auto impl = info.impl->as<vulkan_tex_data>();
	impl->runtime = this;

	// Do not create resource if it is a reference, it is set in 'render_technique'
	if (info.impl_reference != texture_reference::none)
		return true;

	switch (info.format)
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
	VkImageUsageFlags usage_flags = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	// Add required TRANSFER_SRC flag for mipmap generation
	if (info.levels > 1)
		usage_flags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

	VkImageCreateFlags image_flags = 0;
	// Add mutable format flag required to create a SRGB view of the image
	if (impl->formats[1] != VK_FORMAT_UNDEFINED)
		image_flags = VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;
	else
		impl->formats[1] = impl->formats[0];

	impl->image = create_image(
		info.width, info.height, info.levels, impl->formats[0],
		usage_flags,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		image_flags);
	if (impl->image == VK_NULL_HANDLE)
		return false;

	set_object_name((uint64_t)impl->image, VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT, info.unique_name.c_str());

	// Create shader views
	impl->view[0] = create_image_view(impl->image, impl->formats[0], VK_REMAINING_MIP_LEVELS);
	impl->view[1] = impl->formats[0] != impl->formats[1] ?
		create_image_view(impl->image, impl->formats[1], VK_REMAINING_MIP_LEVELS) :
		impl->view[0];

	// Create render target views (with a single level)
	if (info.levels > 1)
	{
		impl->view[2] = create_image_view(impl->image, impl->formats[0], 1);
		impl->view[3] = impl->formats[0] != impl->formats[1] ?
			create_image_view(impl->image, impl->formats[1], 1) :
			impl->view[2];
	}
	else
	{
		impl->view[2] = impl->view[0];
		impl->view[3] = impl->view[1];
	}

	return true;
}
void reshade::vulkan::runtime_vulkan::upload_texture(texture &texture, const uint8_t *pixels)
{
	assert(pixels != nullptr);
	assert(texture.impl_reference == texture_reference::none);

	vk_handle<VK_OBJECT_TYPE_BUFFER> intermediate(_device, _funcs);
	vk_handle<VK_OBJECT_TYPE_DEVICE_MEMORY> intermediate_mem(_device, _funcs);

	{   VkBufferCreateInfo create_info { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
		create_info.size = texture.width * texture.height * 4;
		create_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		check_result(_funcs.vkCreateBuffer(_device, &create_info, nullptr, &intermediate));
	}

	// Allocate host memory for upload
	{   VkMemoryRequirements reqs = {};
		_funcs.vkGetBufferMemoryRequirements(_device, intermediate, &reqs);

		VkMemoryAllocateInfo alloc_info { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
		alloc_info.allocationSize = reqs.size;
		alloc_info.memoryTypeIndex = find_memory_type_index(_memory_props, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, reqs.memoryTypeBits);

		check_result(_funcs.vkAllocateMemory(_device, &alloc_info, nullptr, &intermediate_mem));
		check_result(_funcs.vkBindBufferMemory(_device, intermediate, intermediate_mem, 0));
	}

	// Fill upload buffer with pixel data
	uint8_t *mapped_data;
	check_result(_funcs.vkMapMemory(_device, intermediate_mem, 0, VK_WHOLE_SIZE, 0, reinterpret_cast<void **>(&mapped_data)));

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
			memcpy(mapped_data, pixels, texture.width * 4);
		break;
	default:
		LOG(ERROR) << "Texture upload is not supported for format " << static_cast<unsigned int>(texture.format) << '!';
		break;
	}

	_funcs.vkUnmapMemory(_device, intermediate_mem);

	auto impl = texture.impl->as<vulkan_tex_data>();
	assert(impl != nullptr);

	const VkCommandBuffer cmd_list = create_command_list();

	transition_layout(cmd_list, impl->image, impl->layout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
	{ // Copy data from upload buffer into target texture
		VkBufferImageCopy copy_region = {};
		copy_region.imageExtent = { texture.width, texture.height, 1u };
		copy_region.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };

		_funcs.vkCmdCopyBufferToImage(cmd_list, intermediate, impl->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy_region);
	}
	transition_layout(cmd_list, impl->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, impl->layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	generate_mipmaps(cmd_list, texture);

	execute_command_list(cmd_list);
}

void reshade::vulkan::runtime_vulkan::generate_mipmaps(const VkCommandBuffer cmd_list, texture &texture)
{
	if (texture.levels <= 1)
		return; // No need to generate mipmaps when texture does not have any

	auto impl = texture.impl->as<vulkan_tex_data>();
	assert(impl != nullptr);

	int32_t width = texture.width;
	int32_t height = texture.height;

	for (uint32_t level = 1; level < texture.levels; ++level, width /= 2, height /= 2)
	{
		VkImageBlit blit;
		blit.srcOffsets[0] = { 0, 0, 0 };
		blit.srcOffsets[1] = { width, height, 1 };
		blit.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, level - 1, 0, 1 };
		blit.dstOffsets[0] = { 0, 0, 0 };
		blit.dstOffsets[1] = { width > 1 ? width / 2 : 1, height > 1 ? height / 2 : 1, 1 };
		blit.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, level, 0, 1 };

		transition_layout(cmd_list, impl->image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, { VK_IMAGE_ASPECT_COLOR_BIT, level - 1, 1, 0, 1 });
		transition_layout(cmd_list, impl->image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, { VK_IMAGE_ASPECT_COLOR_BIT, level, 1, 0, 1 });
		_funcs.vkCmdBlitImage(cmd_list, impl->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, impl->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);
		transition_layout(cmd_list, impl->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, { VK_IMAGE_ASPECT_COLOR_BIT, level - 1, 1, 0, 1 });
		transition_layout(cmd_list, impl->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, { VK_IMAGE_ASPECT_COLOR_BIT, level, 1, 0, 1 });
	}
}

VkCommandBuffer reshade::vulkan::runtime_vulkan::create_command_list(VkCommandBufferLevel level) const
{
	VkCommandBuffer cmd_list = VK_NULL_HANDLE;
	VkCommandBufferAllocateInfo alloc_info { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
	alloc_info.commandPool = _cmd_pool[_swap_index];
	alloc_info.level = level;
	alloc_info.commandBufferCount = 1;

	check_result(_funcs.vkAllocateCommandBuffers(_device, &alloc_info, &cmd_list)) VK_NULL_HANDLE;

	VkCommandBufferBeginInfo begin_info { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	check_result(_funcs.vkBeginCommandBuffer(cmd_list, &begin_info)) VK_NULL_HANDLE;

	return cmd_list;
}
void reshade::vulkan::runtime_vulkan::execute_command_list(VkCommandBuffer cmd_list) const
{
	check_result(_funcs.vkEndCommandBuffer(cmd_list));

	VkSubmitInfo submit_info { VK_STRUCTURE_TYPE_SUBMIT_INFO };
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &cmd_list;

	assert(_current_queue != VK_NULL_HANDLE);
	VkResult res = _funcs.vkQueueSubmit(_current_queue, 1, &submit_info, _wait_fence);
	assert(res == VK_SUCCESS);
	res = _funcs.vkWaitForFences(_device, 1, &_wait_fence, VK_TRUE, UINT64_MAX);
	assert(res == VK_SUCCESS);
	res = _funcs.vkResetFences(_device, 1, &_wait_fence);
	assert(res == VK_SUCCESS);
}
void reshade::vulkan::runtime_vulkan::execute_command_list_async(VkCommandBuffer cmd_list) const
{
	check_result(_funcs.vkEndCommandBuffer(cmd_list));

	VkSubmitInfo submit_info { VK_STRUCTURE_TYPE_SUBMIT_INFO };
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &cmd_list;

	assert(_current_queue != VK_NULL_HANDLE);
	VkResult res = _funcs.vkQueueSubmit(_current_queue, 1, &submit_info, VK_NULL_HANDLE);
	assert(res == VK_SUCCESS);
}

void reshade::vulkan::runtime_vulkan::transition_layout(VkCommandBuffer cmd_list, VkImage image, VkImageLayout old_layout, VkImageLayout new_layout, VkImageSubresourceRange subresource) const
{
	const auto layout_to_access = [](VkImageLayout layout) -> VkAccessFlags {
		switch (layout)
		{
		case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
			return VK_ACCESS_TRANSFER_READ_BIT;
		case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
			return VK_ACCESS_TRANSFER_WRITE_BIT;
		case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
			return VK_ACCESS_SHADER_READ_BIT;
		case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:
			return VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
		case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
		case VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL:
		case VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL:
			return VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		}
		return 0;
	};
	const auto layout_to_stage = [](VkImageLayout layout) -> VkPipelineStageFlags {
		switch (layout)
		{
		case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
		case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
			return VK_PIPELINE_STAGE_TRANSFER_BIT;
		case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
			return VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:
		case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
		case VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL:
		case VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL:
			return VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		}
		return VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
	};

	VkImageMemoryBarrier transition { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
	transition.image = image;
	transition.subresourceRange = subresource;
	transition.srcAccessMask = layout_to_access(old_layout);
	transition.dstAccessMask = layout_to_access(new_layout);
	transition.oldLayout = old_layout;
	transition.newLayout = new_layout;

	_funcs.vkCmdPipelineBarrier(cmd_list, layout_to_stage(old_layout), layout_to_stage(new_layout), 0, 0, nullptr, 0, nullptr, 1, &transition);
}

bool reshade::vulkan::runtime_vulkan::compile_effect(effect_data &effect)
{
	vk_handle<VK_OBJECT_TYPE_SHADER_MODULE> module(_device, _funcs);

	// Load shader module
	{   VkShaderModuleCreateInfo create_info { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
		create_info.codeSize = effect.module.spirv.size() * sizeof(uint32_t);
		create_info.pCode = effect.module.spirv.data();

		const VkResult res = _funcs.vkCreateShaderModule(_device, &create_info, nullptr, &module);
		if (res != VK_SUCCESS)
		{
			effect.errors += "Failed to create shader module. Vulkan error code " + std::to_string(res) + ".";
			return false;
		}
	}

	if (_effect_data.size() <= effect.index)
		_effect_data.resize(effect.index + 1);

	vulkan_effect_data &effect_data = _effect_data[effect.index];
	effect_data.storage_size = effect.storage_size;
	effect_data.storage_offset = effect.storage_offset;

	// Initialize pipeline layout
	{   std::vector<VkDescriptorSetLayoutBinding> bindings;
		bindings.reserve(effect.module.num_sampler_bindings);
		for (uint32_t i = 0; i < effect.module.num_sampler_bindings; ++i)
			bindings.push_back({ i, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_ALL_GRAPHICS });

		VkDescriptorSetLayoutCreateInfo create_info { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
		create_info.bindingCount = uint32_t(bindings.size());
		create_info.pBindings = bindings.data();

		check_result(_funcs.vkCreateDescriptorSetLayout(_device, &create_info, nullptr, &effect_data.set_layout)) false;
	}

	const VkDescriptorSetLayout set_layouts[2] = { _effect_ubo_layout, effect_data.set_layout };

	{   VkPipelineLayoutCreateInfo create_info { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
		create_info.setLayoutCount = 2; // [0] = Global UBO, [1] = Samplers
		create_info.pSetLayouts = set_layouts;

		check_result(_funcs.vkCreatePipelineLayout(_device, &create_info, nullptr, &effect_data.pipeline_layout)) false;
	}

	// Create global uniform buffer object
	if (effect.storage_size != 0)
	{
		effect_data.ubo = create_buffer(
			effect.storage_size,
			VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	}

	// Initialize image and sampler bindings
	std::vector<VkDescriptorImageInfo> image_bindings(effect.module.num_sampler_bindings);

	for (const reshadefx::sampler_info &info : effect.module.samplers)
	{
		const auto existing_texture = std::find_if(_textures.begin(), _textures.end(),
			[&texture_name = info.texture_name](const auto &item) {
			return item.unique_name == texture_name && item.impl != nullptr;
		});
		if (existing_texture == _textures.end())
			return false;

		VkDescriptorImageInfo &image_binding = image_bindings[info.binding];
		image_binding.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		switch (existing_texture->impl_reference)
		{
		case texture_reference::back_buffer:
			image_binding.imageView = _backbuffer_texture_view;
			break;
		case texture_reference::depth_buffer:
			// TODO (set to backbuffer for now to make validation layers happy)
			image_binding.imageView = _backbuffer_texture_view;
			break;
		default:
			image_binding.imageView = existing_texture->impl->as<vulkan_tex_data>()->view[info.srgb];
			break;
		}

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

		auto it = _effect_sampler_states.find(desc_hash);
		if (it == _effect_sampler_states.end())
		{
			VkSampler sampler = VK_NULL_HANDLE;
			check_result(_funcs.vkCreateSampler(_device, &create_info, nullptr, &sampler)) false;
			it = _effect_sampler_states.emplace(desc_hash, sampler).first;
		}

		image_binding.sampler = it->second;
	}

	{   VkDescriptorSetAllocateInfo alloc_info { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
		alloc_info.descriptorPool = _effect_descriptor_pool;
		alloc_info.descriptorSetCount = 2;
		alloc_info.pSetLayouts = set_layouts;

		check_result(_funcs.vkAllocateDescriptorSets(_device, &alloc_info, effect_data.set)) false;

		VkWriteDescriptorSet writes[2];
		writes[0] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
		writes[0].dstSet = effect_data.set[0];
		writes[0].dstBinding = 0;
		writes[0].descriptorCount = 1;
		writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		const VkDescriptorBufferInfo ubo_info = {
			effect_data.ubo, 0, effect.storage_size };
		writes[0].pBufferInfo = &ubo_info;

		writes[1] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
		writes[1].dstSet = effect_data.set[1];
		writes[1].dstBinding = 0;
		writes[1].descriptorCount = uint32_t(image_bindings.size());
		writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		writes[1].pImageInfo = image_bindings.data();

		_funcs.vkUpdateDescriptorSets(_device, 2, writes, 0, nullptr);
	}

	bool success = true;

	std::vector<uint8_t> spec_data;
	std::vector<VkSpecializationMapEntry> spec_map;
	for (uint32_t spec_id = 0; spec_id < uint32_t(effect.module.spec_constants.size()); ++spec_id)
	{
		const auto &constant = effect.module.spec_constants[spec_id];
		spec_map.push_back({ spec_id, constant.offset, constant.size });
		spec_data.resize(spec_data.size() + constant.size);
		memcpy(spec_data.data() + spec_data.size() - constant.size, &constant.initializer_value.as_uint[0], constant.size);
	}

	for (technique &technique : _techniques)
		if (technique.impl == nullptr && technique.effect_index == effect.index)
			success &= init_technique(technique, module,
				VkSpecializationInfo { uint32_t(spec_map.size()), spec_map.data(), spec_data.size(), spec_data.data() });

	return success;
}
void reshade::vulkan::runtime_vulkan::unload_effects()
{
	// Wait for all GPU operations to finish so resources are no longer referenced
	_funcs.vkDeviceWaitIdle(_device);

	runtime::unload_effects();

	_funcs.vkResetDescriptorPool(_device, _effect_descriptor_pool, 0);

	for (const vulkan_effect_data &data : _effect_data)
	{
		_funcs.vkDestroyPipelineLayout(_device, data.pipeline_layout, nullptr);
		_funcs.vkDestroyDescriptorSetLayout(_device, data.set_layout, nullptr);
		_funcs.vkDestroyBuffer(_device, data.ubo, nullptr);
		_funcs.vkFreeMemory(_device, data.ubo_mem, nullptr);
	}

	_effect_data.clear();
}

bool reshade::vulkan::runtime_vulkan::init_technique(technique &info, VkShaderModule module, const VkSpecializationInfo &spec_info)
{
	info.impl = std::make_unique<vulkan_technique_data>();

	for (size_t pass_index = 0; pass_index < info.passes.size(); ++pass_index)
	{
		info.passes_data.push_back(std::make_unique<vulkan_pass_data>());

		auto &pass_data = *info.passes_data.back()->as<vulkan_pass_data>();
		const auto &pass_info = info.passes[pass_index];

		const auto literal_to_comp_func = [](unsigned int value) -> VkCompareOp {
			switch (value)
			{
			default:
			case 8: return VK_COMPARE_OP_ALWAYS;
			case 1: return VK_COMPARE_OP_NEVER;
			case 3: return VK_COMPARE_OP_EQUAL;
			case 6: return VK_COMPARE_OP_NOT_EQUAL;
			case 2: return VK_COMPARE_OP_LESS;
			case 4: return VK_COMPARE_OP_LESS_OR_EQUAL;
			case 5: return VK_COMPARE_OP_GREATER;
			case 7: return VK_COMPARE_OP_GREATER_OR_EQUAL;
			}
		};
		const auto literal_to_stencil_op = [](unsigned int value) -> VkStencilOp {
			switch (value) {
			default:
			case 1: return VK_STENCIL_OP_KEEP;
			case 0: return VK_STENCIL_OP_ZERO;
			case 3: return VK_STENCIL_OP_REPLACE;
			case 4: return VK_STENCIL_OP_INCREMENT_AND_CLAMP;
			case 5: return VK_STENCIL_OP_DECREMENT_AND_CLAMP;
			case 6: return VK_STENCIL_OP_INVERT;
			case 7: return VK_STENCIL_OP_INCREMENT_AND_WRAP;
			case 8: return VK_STENCIL_OP_DECREMENT_AND_WRAP;
			}
		};
		const auto literal_to_blend_factor = [](unsigned int value) -> VkBlendFactor {
			switch (value)
			{
			default:
			case 0: return VK_BLEND_FACTOR_ZERO;
			case 1: return VK_BLEND_FACTOR_ONE;
			case 2: return VK_BLEND_FACTOR_SRC_COLOR;
			case 3: return VK_BLEND_FACTOR_SRC_ALPHA;
			case 4: return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
			case 5: return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
			case 8: return VK_BLEND_FACTOR_DST_COLOR;
			case 6: return VK_BLEND_FACTOR_DST_ALPHA;
			case 9: return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
			case 7: return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
			}
		};

		const VkRect2D scissor_rect = {
			{ 0, 0 },
			{ pass_info.viewport_width ? pass_info.viewport_width : frame_width(),
			  pass_info.viewport_height ? pass_info.viewport_height : frame_height() }
		};
		const VkViewport viewport_rect = {
			0.0f, 0.0f,
			static_cast<float>(scissor_rect.extent.width), static_cast<float>(scissor_rect.extent.height),
			0.0f, 1.0f
		};

		pass_data.begin_info.renderArea = scissor_rect;

		uint32_t num_color_attachments = 0;
		uint32_t num_depth_attachments = 0;
		VkImageView attachment_views[9] = {};
		VkAttachmentReference attachment_refs[9] = {};
		VkAttachmentDescription attachment_descs[9] = {};
		VkPipelineColorBlendAttachmentState attachment_blends[8];

		for (uint32_t attach_idx = 0; attach_idx < 8; ++attach_idx, ++num_color_attachments)
		{
			attachment_blends[attach_idx].blendEnable = pass_info.blend_enable;
			attachment_blends[attach_idx].srcColorBlendFactor = literal_to_blend_factor(pass_info.src_blend);
			attachment_blends[attach_idx].dstColorBlendFactor = literal_to_blend_factor(pass_info.dest_blend);
			attachment_blends[attach_idx].colorBlendOp = static_cast<VkBlendOp>(pass_info.blend_op - 1);
			attachment_blends[attach_idx].srcAlphaBlendFactor = literal_to_blend_factor(pass_info.src_blend_alpha);
			attachment_blends[attach_idx].dstAlphaBlendFactor = literal_to_blend_factor(pass_info.dest_blend_alpha);
			attachment_blends[attach_idx].alphaBlendOp = static_cast<VkBlendOp>(pass_info.blend_op_alpha - 1);
			attachment_blends[attach_idx].colorWriteMask = pass_info.color_write_mask;

			if (pass_info.render_target_names[attach_idx].empty())
				break; // Skip unbound render targets

			const auto render_target_texture = std::find_if(_textures.begin(), _textures.end(),
				[&render_target = pass_info.render_target_names[attach_idx]](const auto &item) {
				return item.unique_name == render_target;
			});
			if (render_target_texture == _textures.end())
				return assert(false), false;

			auto texture_impl = render_target_texture->impl->as<vulkan_tex_data>();
			assert(texture_impl != nullptr);

			attachment_views[attach_idx] = texture_impl->view[2 + pass_info.srgb_write_enable];

			VkAttachmentReference &attachment_ref = attachment_refs[attach_idx];
			attachment_ref.attachment = attach_idx;
			attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

			VkAttachmentDescription &attachment_desc = attachment_descs[attach_idx];
			attachment_desc.format = texture_impl->formats[pass_info.srgb_write_enable];
			attachment_desc.samples = VK_SAMPLE_COUNT_1_BIT;
			attachment_desc.loadOp = pass_info.clear_render_targets ? VK_ATTACHMENT_LOAD_OP_CLEAR : pass_info.blend_enable ? VK_ATTACHMENT_LOAD_OP_LOAD : VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			attachment_desc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			attachment_desc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			attachment_desc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			attachment_desc.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			attachment_desc.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		}

		if (pass_info.clear_render_targets)
		{
			pass_data.begin_info.clearValueCount = num_color_attachments;
			pass_data.begin_info.pClearValues = pass_data.clear_values; // These are initialized to zero already
		}

		if (num_color_attachments == 0)
		{
			num_color_attachments = 1;
			pass_data.begin_info.renderPass = _default_render_pass;
			pass_data.begin_info.framebuffer = VK_NULL_HANDLE; // Select the correct swapchain frame buffer during rendering
		}
		else
		{
			if (scissor_rect.extent.width == _width && scissor_rect.extent.height == _height)
			{
				num_depth_attachments = 1;
				const uint32_t attach_idx = num_color_attachments;

				attachment_views[attach_idx] = _default_depthstencil_view;

				VkAttachmentReference &attachment_ref = attachment_refs[attach_idx];
				attachment_ref.attachment = attach_idx;
				attachment_ref.layout = VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL;

				VkAttachmentDescription &attachment_desc = attachment_descs[attach_idx];
				attachment_desc.format = VK_FORMAT_D24_UNORM_S8_UINT;
				attachment_desc.samples = VK_SAMPLE_COUNT_1_BIT;
				attachment_desc.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
				attachment_desc.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
				attachment_desc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
				attachment_desc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
				attachment_desc.initialLayout = VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL;
				attachment_desc.finalLayout = VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL;
			}

			{   VkSubpassDependency subdep = {};
				subdep.srcSubpass = VK_SUBPASS_EXTERNAL;
				subdep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
				subdep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
				subdep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

				VkSubpassDescription subpass = {};
				subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
				subpass.colorAttachmentCount = num_color_attachments;
				subpass.pColorAttachments = attachment_refs;
				subpass.pDepthStencilAttachment = num_depth_attachments ? &attachment_refs[num_color_attachments] : nullptr;

				VkRenderPassCreateInfo create_info { VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
				create_info.attachmentCount = num_color_attachments + num_depth_attachments;
				create_info.pAttachments = attachment_descs;
				create_info.subpassCount = 1;
				create_info.pSubpasses = &subpass;
				create_info.dependencyCount = 1;
				create_info.pDependencies = &subdep;

				check_result(_funcs.vkCreateRenderPass(_device, &create_info, nullptr, &pass_data.begin_info.renderPass)) false;
			}

			{   VkFramebufferCreateInfo create_info { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
				create_info.renderPass = pass_data.begin_info.renderPass;
				create_info.attachmentCount = num_color_attachments + num_depth_attachments;
				create_info.pAttachments = attachment_views;
				create_info.width = scissor_rect.extent.width;
				create_info.height = scissor_rect.extent.height;
				create_info.layers = 1;

				check_result(_funcs.vkCreateFramebuffer(_device, &create_info, nullptr, &pass_data.begin_info.framebuffer)) false;
			}
		}

		VkPipelineShaderStageCreateInfo stages[2];
		stages[0] = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
		stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
		stages[0].module = module;
		stages[0].pName = pass_info.vs_entry_point.c_str();
		stages[0].pSpecializationInfo = &spec_info;
		stages[1] = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
		stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		stages[1].module = module;
		stages[1].pName = pass_info.ps_entry_point.c_str();
		stages[1].pSpecializationInfo = &spec_info;

		VkPipelineVertexInputStateCreateInfo vertex_info = { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
		// No vertex attributes

		VkPipelineInputAssemblyStateCreateInfo ia_info = { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
		ia_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

		VkPipelineViewportStateCreateInfo viewport_info = { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
		viewport_info.viewportCount = 1;
		viewport_info.pViewports = &viewport_rect;
		viewport_info.scissorCount = 1;
		viewport_info.pScissors = &scissor_rect;

		VkPipelineRasterizationStateCreateInfo raster_info = { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
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
		depth_info.front.failOp = literal_to_stencil_op(pass_info.stencil_op_fail);
		depth_info.front.passOp = literal_to_stencil_op(pass_info.stencil_op_pass);
		depth_info.front.depthFailOp = literal_to_stencil_op(pass_info.stencil_op_depth_fail);
		depth_info.front.compareOp = literal_to_comp_func(pass_info.stencil_comparison_func);
		depth_info.front.compareMask = pass_info.stencil_read_mask;
		depth_info.front.writeMask = pass_info.stencil_write_mask;
		depth_info.front.reference = pass_info.stencil_reference_value;
		depth_info.back = depth_info.front;
		depth_info.minDepthBounds = 0.0f;
		depth_info.minDepthBounds = 1.0f;

		VkGraphicsPipelineCreateInfo create_info { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
		create_info.stageCount = _countof(stages);
		create_info.pStages = stages;
		create_info.pVertexInputState = &vertex_info;
		create_info.pInputAssemblyState = &ia_info;
		create_info.pViewportState = &viewport_info;
		create_info.pRasterizationState = &raster_info;
		create_info.pMultisampleState = &ms_info;
		create_info.pDepthStencilState = &depth_info;
		create_info.pColorBlendState = &blend_info;
		create_info.layout = _effect_data[info.effect_index].pipeline_layout;
		create_info.renderPass = pass_data.begin_info.renderPass;

		check_result(_funcs.vkCreateGraphicsPipelines(_device, VK_NULL_HANDLE, 1, &create_info, nullptr, &pass_data.pipeline)) false;
	}

	return true;
}

void reshade::vulkan::runtime_vulkan::render_technique(technique &technique)
{
	vulkan_effect_data &effect_data = _effect_data[technique.effect_index];

	const VkCommandBuffer cmd_list = create_command_list();
	if (cmd_list == VK_NULL_HANDLE)
		return;

	_funcs.vkCmdBindDescriptorSets(cmd_list, VK_PIPELINE_BIND_POINT_GRAPHICS, effect_data.pipeline_layout, 0, 2, effect_data.set, 0, nullptr);

	// Setup shader constants
	if (effect_data.storage_size != 0)
		_funcs.vkCmdUpdateBuffer(cmd_list, effect_data.ubo, 0, effect_data.storage_size, _uniform_data_storage.data() + effect_data.storage_offset);

	// Clear default depth stencil
	const VkImageSubresourceRange clear_range = { VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT, 0, 1, 0, 1 };
	const VkClearDepthStencilValue clear_value = { 1.0f, 0 };
	transition_layout(cmd_list, _default_depthstencil, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, { VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT, 0, 1, 0, 1 });
	_funcs.vkCmdClearDepthStencilImage(cmd_list, _default_depthstencil, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clear_value, 1, &clear_range);
	transition_layout(cmd_list, _default_depthstencil, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL, { VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT, 0, 1, 0, 1 });

	for (size_t i = 0; i < technique.passes.size(); ++i)
	{
		const auto &pass_info = technique.passes[i];
		const auto &pass_data = *technique.passes_data[i]->as<vulkan_pass_data>();

		// Transition render targets
		for (unsigned int k = 0; k < 8 && !pass_info.render_target_names[k].empty(); ++k)
		{
			const auto texture_impl = std::find_if(_textures.begin(), _textures.end(),
				[&render_target = pass_info.render_target_names[k]](const auto &item) {
				return item.unique_name == render_target;
			})->impl->as<vulkan_tex_data>();

			if (texture_impl->layout != VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
				transition_layout(cmd_list, texture_impl->image, texture_impl->layout, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
			texture_impl->layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		}

		// Save back buffer of previous pass
		const VkImageCopy copy_range = {
			{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 }, { 0, 0, 0 },
			{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 }, { 0, 0, 0 }, { _width, _height, 1 }
		};
		transition_layout(cmd_list, _backbuffer_texture, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
		transition_layout(cmd_list, _swapchain_images[_swap_index], VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
		_funcs.vkCmdCopyImage(cmd_list, _swapchain_images[_swap_index], VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, _backbuffer_texture, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy_range);
		transition_layout(cmd_list, _backbuffer_texture, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		transition_layout(cmd_list, _swapchain_images[_swap_index], VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

		VkRenderPassBeginInfo begin_info = pass_data.begin_info;
		if (begin_info.framebuffer == VK_NULL_HANDLE)
			begin_info.framebuffer = _swapchain_frames[_swap_index];
		_funcs.vkCmdBeginRenderPass(cmd_list, &begin_info, VK_SUBPASS_CONTENTS_INLINE);

		// Setup states
		_funcs.vkCmdBindPipeline(cmd_list, VK_PIPELINE_BIND_POINT_GRAPHICS, pass_data.pipeline);

		// Draw triangle
		_funcs.vkCmdDraw(cmd_list, 3, 1, 0, 0);
		_vertices += 3; _drawcalls += 1;

		_funcs.vkCmdEndRenderPass(cmd_list);

		// Generate mipmaps
		for (unsigned int k = 0; k < 8 && !pass_info.render_target_names[k].empty(); ++k)
		{
			const auto render_target_texture = std::find_if(_textures.begin(), _textures.end(),
				[&render_target = pass_info.render_target_names[k]](const auto &item) {
				return item.unique_name == render_target;
			});

			generate_mipmaps(cmd_list, *render_target_texture);
		}
	}

	execute_command_list_async(cmd_list);
}

#if RESHADE_GUI
bool reshade::vulkan::runtime_vulkan::init_imgui_resources()
{
	vk_handle<VK_OBJECT_TYPE_SHADER_MODULE> vs_module(_device, _funcs);
	vk_handle<VK_OBJECT_TYPE_SHADER_MODULE> fs_module(_device, _funcs);

	VkPipelineShaderStageCreateInfo stages[2];
	stages[0] = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
	stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	stages[0].pName = "main";
	stages[1] = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
	stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	stages[1].pName = "main";

	{   VkShaderModuleCreateInfo create_info { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };

		const resources::data_resource vs = resources::load_data_resource(IDR_IMGUI_VS_SPIRV);
		create_info.codeSize = vs.data_size;
		create_info.pCode = static_cast<const uint32_t *>(vs.data);
		check_result(_funcs.vkCreateShaderModule(_device, &create_info, nullptr, &vs_module)) false;
		stages[0].module = vs_module;

		const resources::data_resource ps = resources::load_data_resource(IDR_IMGUI_PS_SPIRV);
		create_info.codeSize = ps.data_size;
		create_info.pCode = static_cast<const uint32_t *>(ps.data);
		check_result(_funcs.vkCreateShaderModule(_device, &create_info, nullptr, &fs_module)) false;
		stages[1].module = fs_module;
	}

	{   VkSamplerCreateInfo create_info { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
		create_info.magFilter = VK_FILTER_LINEAR;
		create_info.minFilter = VK_FILTER_LINEAR;
		create_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		create_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		create_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		create_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		create_info.minLod = -1000;
		create_info.maxLod = +1000;

		check_result(_funcs.vkCreateSampler(_device, &create_info, nullptr, &_imgui_font_sampler)) false;
	}

	{   VkDescriptorSetLayoutBinding bindings[1] = {};
		bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		bindings[0].descriptorCount = 1;
		bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		bindings[0].pImmutableSamplers = &_imgui_font_sampler;

		VkDescriptorSetLayoutCreateInfo create_info { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
		create_info.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR;
		create_info.bindingCount = _countof(bindings);
		create_info.pBindings = bindings;

		check_result(_funcs.vkCreateDescriptorSetLayout(_device, &create_info, nullptr, &_imgui_descriptor_set_layout)) false;
	}

	{   const VkPushConstantRange push_constants[] = {
			{ VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(float) * 4 }
		};
		const VkDescriptorSetLayout descriptor_layouts[] = {
			_imgui_descriptor_set_layout
		};

		VkPipelineLayoutCreateInfo create_info { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
		create_info.setLayoutCount = _countof(descriptor_layouts);
		create_info.pSetLayouts = descriptor_layouts;
		create_info.pushConstantRangeCount = _countof(push_constants);
		create_info.pPushConstantRanges = push_constants;

		check_result(_funcs.vkCreatePipelineLayout(_device, &create_info, nullptr, &_imgui_pipeline_layout)) false;
	}

	VkVertexInputBindingDescription binding_desc[1] = {};
	binding_desc[0].binding = 0;
	binding_desc[0].stride = sizeof(ImDrawVert);
	binding_desc[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	VkVertexInputAttributeDescription attribute_desc[3] = {};
	attribute_desc[0].location = 0;
	attribute_desc[0].binding = binding_desc[0].binding;
	attribute_desc[0].format = VK_FORMAT_R32G32_SFLOAT;
	attribute_desc[0].offset = offsetof(ImDrawVert, pos);
	attribute_desc[1].location = 1;
	attribute_desc[1].binding = binding_desc[0].binding;
	attribute_desc[1].format = VK_FORMAT_R32G32_SFLOAT;
	attribute_desc[1].offset = offsetof(ImDrawVert, uv );
	attribute_desc[2].location = 2;
	attribute_desc[2].binding = binding_desc[0].binding;
	attribute_desc[2].format = VK_FORMAT_R8G8B8A8_UNORM;
	attribute_desc[2].offset = offsetof(ImDrawVert, col);

	VkPipelineVertexInputStateCreateInfo vertex_info = { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
	vertex_info.vertexBindingDescriptionCount = _countof(binding_desc);
	vertex_info.pVertexBindingDescriptions = binding_desc;
	vertex_info.vertexAttributeDescriptionCount = _countof(attribute_desc);
	vertex_info.pVertexAttributeDescriptions = attribute_desc;

	VkPipelineInputAssemblyStateCreateInfo ia_info = { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
	ia_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

	VkPipelineViewportStateCreateInfo viewport_info = { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
	viewport_info.viewportCount = 1;
	viewport_info.scissorCount = 1;

	VkPipelineRasterizationStateCreateInfo raster_info = { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
	raster_info.polygonMode = VK_POLYGON_MODE_FILL;
	raster_info.cullMode = VK_CULL_MODE_NONE;
	raster_info.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	raster_info.lineWidth = 1.0f;

	VkPipelineMultisampleStateCreateInfo ms_info { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
	ms_info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	VkPipelineColorBlendAttachmentState color_attachment = {};
	color_attachment.blendEnable = VK_TRUE;
	color_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	color_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	color_attachment.colorBlendOp = VK_BLEND_OP_ADD;
	color_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	color_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	color_attachment.alphaBlendOp = VK_BLEND_OP_ADD;
	color_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

	VkPipelineColorBlendStateCreateInfo blend_info { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
	blend_info.attachmentCount = 1;
	blend_info.pAttachments = &color_attachment;

	VkPipelineDepthStencilStateCreateInfo depth_info { VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
	depth_info.depthTestEnable = VK_FALSE;

	VkDynamicState dynamic_states[2] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
	VkPipelineDynamicStateCreateInfo dynamic_state { VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
	dynamic_state.dynamicStateCount = _countof(dynamic_states);
	dynamic_state.pDynamicStates = dynamic_states;

	VkGraphicsPipelineCreateInfo create_info { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
	create_info.stageCount = _countof(stages);
	create_info.pStages = stages;
	create_info.pVertexInputState = &vertex_info;
	create_info.pInputAssemblyState = &ia_info;
	create_info.pViewportState = &viewport_info;
	create_info.pRasterizationState = &raster_info;
	create_info.pMultisampleState = &ms_info;
	create_info.pDepthStencilState = &depth_info;
	create_info.pColorBlendState = &blend_info;
	create_info.pDynamicState = &dynamic_state;
	create_info.layout = _imgui_pipeline_layout;
	create_info.renderPass = _default_render_pass;

	check_result(_funcs.vkCreateGraphicsPipelines(_device, VK_NULL_HANDLE, 1, &create_info, nullptr, &_imgui_pipeline)) false;

	return true;
}

void reshade::vulkan::runtime_vulkan::render_imgui_draw_data(ImDrawData *draw_data)
{
	bool resize_mem = false;

	// Create and grow vertex/index buffers if needed
	if (_imgui_index_buffer_size < uint32_t(draw_data->TotalIdxCount))
	{
		const uint32_t new_size = draw_data->TotalIdxCount + 10000;
		_imgui_index_buffer = create_buffer(
			new_size * sizeof(ImDrawIdx),
			VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
			0);
		if (_imgui_index_buffer == VK_NULL_HANDLE)
			return;

		resize_mem = true;
		_imgui_index_buffer_size = new_size;
	}
	if (_imgui_vertex_buffer_size < uint32_t(draw_data->TotalVtxCount))
	{
		const uint32_t new_size = draw_data->TotalVtxCount + 5000;
		_imgui_vertex_buffer = create_buffer(
			new_size * sizeof(ImDrawVert),
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
			0);
		if (_imgui_vertex_buffer == VK_NULL_HANDLE)
			return;

		resize_mem = true;
		_imgui_vertex_buffer_size = new_size;
	}

	if (resize_mem)
	{
		VkMemoryRequirements index_reqs = {};
		_funcs.vkGetBufferMemoryRequirements(_device, _imgui_index_buffer, &index_reqs);
		VkMemoryRequirements vertex_reqs = {};
		_funcs.vkGetBufferMemoryRequirements(_device, _imgui_vertex_buffer, &vertex_reqs);

		VkMemoryAllocateInfo alloc_info { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
		alloc_info.allocationSize = index_reqs.size + vertex_reqs.size;
		alloc_info.memoryTypeIndex = find_memory_type_index(_memory_props, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, index_reqs.memoryTypeBits & vertex_reqs.memoryTypeBits);

		check_result(_funcs.vkAllocateMemory(_device, &alloc_info, nullptr, &_imgui_vertex_mem));

		VkBindBufferMemoryInfo bind_infos[2];
		bind_infos[0] = { VK_STRUCTURE_TYPE_BIND_BUFFER_MEMORY_INFO };
		bind_infos[0].buffer = _imgui_index_buffer;
		bind_infos[0].memory = _imgui_vertex_mem;
		bind_infos[0].memoryOffset = 0;
		bind_infos[1] = { VK_STRUCTURE_TYPE_BIND_BUFFER_MEMORY_INFO };
		bind_infos[1].buffer = _imgui_vertex_buffer;
		bind_infos[1].memory = _imgui_vertex_mem;
		bind_infos[1].memoryOffset = _imgui_vertex_mem_offset = index_reqs.size;

		check_result(_funcs.vkBindBufferMemory2(_device, _countof(bind_infos), bind_infos));
	}

	ImDrawIdx *idx_dst; ImDrawVert *vtx_dst;
	check_result(_funcs.vkMapMemory(_device, _imgui_vertex_mem, 0, VK_WHOLE_SIZE, 0, reinterpret_cast<void **>(&idx_dst)));
	vtx_dst = reinterpret_cast<ImDrawVert *>(reinterpret_cast<uint8_t *>(idx_dst) + _imgui_vertex_mem_offset);

	for (int n = 0; n < draw_data->CmdListsCount; n++)
	{
		const ImDrawList *draw_list = draw_data->CmdLists[n];
		memcpy(idx_dst, draw_list->IdxBuffer.Data, draw_list->IdxBuffer.Size * sizeof(ImDrawIdx));
		memcpy(vtx_dst, draw_list->VtxBuffer.Data, draw_list->VtxBuffer.Size * sizeof(ImDrawVert));
		idx_dst += draw_list->IdxBuffer.Size;
		vtx_dst += draw_list->VtxBuffer.Size;
	}

	_funcs.vkUnmapMemory(_device, _imgui_vertex_mem);

	const VkCommandBuffer cmd_list = create_command_list();

	{   VkRenderPassBeginInfo begin_info { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
		begin_info.renderPass = _default_render_pass;
		begin_info.framebuffer = _swapchain_frames[_swap_index];
		begin_info.renderArea.extent = _render_area;
		_funcs.vkCmdBeginRenderPass(cmd_list, &begin_info, VK_SUBPASS_CONTENTS_INLINE);
	}

	// Setup orthographic projection matrix
	const float scale[2] = {
		2.0f / draw_data->DisplaySize.x,
		2.0f / draw_data->DisplaySize.y
	};
	const float translate[2] = {
		-1.0f - draw_data->DisplayPos.x * scale[0],
		-1.0f - draw_data->DisplayPos.y * scale[1]
	};
	_funcs.vkCmdPushConstants(cmd_list, _imgui_pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, sizeof(float) * 0, sizeof(float) * 2, scale);
	_funcs.vkCmdPushConstants(cmd_list, _imgui_pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, sizeof(float) * 2, sizeof(float) * 2, translate);

	// Setup render state and render draw lists
	_funcs.vkCmdBindIndexBuffer(cmd_list, _imgui_index_buffer, 0, sizeof(ImDrawIdx) == 2 ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32);
	const VkDeviceSize offset = 0;
	_funcs.vkCmdBindVertexBuffers(cmd_list, 0, 1, &_imgui_vertex_buffer, &offset);
	const VkViewport viewport = { 0.0f, 0.0f, draw_data->DisplaySize.x, draw_data->DisplaySize.y, 0.0f, 1.0f };
	_funcs.vkCmdSetViewport(cmd_list, 0, 1, &viewport);
	_funcs.vkCmdBindPipeline(cmd_list, VK_PIPELINE_BIND_POINT_GRAPHICS, _imgui_pipeline);

	uint32_t vtx_offset = 0, idx_offset = 0;
	for (int n = 0; n < draw_data->CmdListsCount; ++n)
	{
		const ImDrawList *const draw_list = draw_data->CmdLists[n];

		for (const ImDrawCmd &cmd : draw_list->CmdBuffer)
		{
			assert(cmd.TextureId != 0);
			assert(cmd.UserCallback == nullptr);

			const VkRect2D scissor_rect = {
				// Offset
				{ static_cast<int32_t>(cmd.ClipRect.x - draw_data->DisplayPos.x),
				  static_cast<int32_t>(cmd.ClipRect.y - draw_data->DisplayPos.y) },
				// Extent
				{ static_cast<uint32_t>(cmd.ClipRect.z - cmd.ClipRect.x),
				  static_cast<uint32_t>(cmd.ClipRect.w - cmd.ClipRect.y) }
			};
			_funcs.vkCmdSetScissor(cmd_list, 0, 1, &scissor_rect);

			auto tex_data = static_cast<const vulkan_tex_data *>(cmd.TextureId);
			// TODO: Transition resource state of the user texture?
			assert(tex_data->layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

			VkWriteDescriptorSet write { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
			write.dstBinding = 0;
			write.descriptorCount = 1;
			write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			const VkDescriptorImageInfo image_info { _imgui_font_sampler, tex_data->view[0], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
			write.pImageInfo = &image_info;
			_funcs.vkCmdPushDescriptorSetKHR(cmd_list, VK_PIPELINE_BIND_POINT_GRAPHICS, _imgui_pipeline_layout, 0, 1, &write);

			_funcs.vkCmdDrawIndexed(cmd_list, cmd.ElemCount, 1, idx_offset, vtx_offset, 0);

			idx_offset += cmd.ElemCount;
		}

		vtx_offset += draw_list->VtxBuffer.Size;
	}

	_funcs.vkCmdEndRenderPass(cmd_list);

	execute_command_list_async(cmd_list);
}
#endif
