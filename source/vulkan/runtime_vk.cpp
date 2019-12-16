/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "dll_log.hpp"
#include "dll_resources.hpp"
#include "runtime_vk.hpp"
#include "runtime_config.hpp"
#include "runtime_objects.hpp"
#include "format_utils.hpp"
#include <imgui.h>
#include <imgui_internal.h>

#define check_result(call) \
	if ((call) != VK_SUCCESS) \
		return

namespace reshade::vulkan
{
	const uint32_t MAX_EFFECT_DESCRIPTOR_SETS = 100;

	struct vulkan_tex_data : base_object
	{
		~vulkan_tex_data()
		{
			runtime->vk.DestroyImage(runtime->_device, image, nullptr);
			runtime->vk.DestroyImageView(runtime->_device, view[0], nullptr);
			if (view[1] != view[0])
				runtime->vk.DestroyImageView(runtime->_device, view[1], nullptr);
			if (view[2] != view[0])
				runtime->vk.DestroyImageView(runtime->_device, view[2], nullptr);
			if (view[3] != view[2] && view[3] != view[1])
				runtime->vk.DestroyImageView(runtime->_device, view[3], nullptr);
		}

		VkImage image = VK_NULL_HANDLE;
		VkImageView view[4] = {};
		VkFormat formats[2] = {};
		runtime_vk *runtime = nullptr;
	};

	struct vulkan_technique_data : base_object
	{
		uint32_t query_index = 0;
	};

	struct vulkan_pass_data : base_object
	{
		~vulkan_pass_data()
		{
			if (begin_info.renderPass != runtime->_default_render_pass[0] &&
				begin_info.renderPass != runtime->_default_render_pass[1])
				runtime->vk.DestroyRenderPass(runtime->_device, begin_info.renderPass, nullptr);
			if (begin_info.framebuffer != VK_NULL_HANDLE)
				runtime->vk.DestroyFramebuffer(runtime->_device, begin_info.framebuffer, nullptr);
			runtime->vk.DestroyPipeline(runtime->_device, pipeline, nullptr);
		}

		VkPipeline pipeline = VK_NULL_HANDLE;
		VkClearValue clear_values[8] = {};
		VkRenderPassBeginInfo begin_info = { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
		runtime_vk *runtime = nullptr;
	};

	struct vulkan_effect_data
	{
		VkQueryPool query_pool = VK_NULL_HANDLE;
		VkDescriptorSet set[2] = {};
		VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
		VkDescriptorSetLayout set_layout = VK_NULL_HANDLE;
		VkBuffer ubo = VK_NULL_HANDLE;
		VkDeviceMemory ubo_mem = VK_NULL_HANDLE;
		reshadefx::module module;
		std::vector<VkDescriptorImageInfo> image_bindings;
		uint32_t depth_image_binding = std::numeric_limits<uint32_t>::max();
	};

	uint32_t find_memory_type_index(const VkPhysicalDeviceMemoryProperties &props, VkMemoryPropertyFlags flags, uint32_t type_bits)
	{
		for (uint32_t i = 0; i < props.memoryTypeCount; i++)
			if ((props.memoryTypes[i].propertyFlags & flags) == flags && type_bits & (1 << i))
				return i;
		return std::numeric_limits<uint32_t>::max();
	}

	void transition_layout(const VkLayerDispatchTable &vk, VkCommandBuffer cmd_list, VkImage image, VkImageLayout old_layout, VkImageLayout new_layout,
		VkImageSubresourceRange subresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS })
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
			case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
			case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
				return VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
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
			case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
				return VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			}
			return VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
		};

		VkImageMemoryBarrier transition{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
		transition.image = image;
		transition.subresourceRange = subresource;
		transition.srcAccessMask = layout_to_access(old_layout);
		transition.dstAccessMask = layout_to_access(new_layout);
		transition.oldLayout = old_layout;
		transition.newLayout = new_layout;

		vk.CmdPipelineBarrier(cmd_list, layout_to_stage(old_layout), layout_to_stage(new_layout), 0, 0, nullptr, 0, nullptr, 1, &transition);
	}
}

reshade::vulkan::runtime_vk::runtime_vk(VkDevice device, VkPhysicalDevice physical_device, const VkLayerInstanceDispatchTable &instance_table, const VkLayerDispatchTable &device_table) :
	_device(device), _physical_device(physical_device), vk(device_table)
{
	_renderer_id = 0x20000;

	instance_table.GetPhysicalDeviceMemoryProperties(physical_device, &_memory_props);

	uint32_t num_queue_families = 0;
	instance_table.GetPhysicalDeviceQueueFamilyProperties(_physical_device, &num_queue_families, nullptr);
	std::vector<VkQueueFamilyProperties> queue_families(num_queue_families);
	instance_table.GetPhysicalDeviceQueueFamilyProperties(_physical_device, &num_queue_families, queue_families.data());

	// Find a queue with graphics support
	for (uint32_t i = 0; i < num_queue_families; ++i)
	{
		if (queue_families[i].queueCount > 0 && (queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT))
		{
			_queue_family_index = i;
			break;
		}
	}

	// TODO: Need to ensure the device was actually created with support for this queue
	vk.GetDeviceQueue(device, _queue_family_index, 0, &_main_queue);

#if RESHADE_GUI && RESHADE_VULKAN_CAPTURE_DEPTH_BUFFERS
	subscribe_to_ui("Vulkan", [this]() { draw_depth_debug_menu(); });
#endif
#if RESHADE_VULKAN_CAPTURE_DEPTH_BUFFERS
	subscribe_to_load_config([this](const ini_file &config) {
		config.get("VULKAN_BUFFER_DETECTION", "UseAspectRatioHeuristics", _use_aspect_ratio_heuristics);
	});
	subscribe_to_save_config([this](ini_file &config) {
		config.set("VULKAN_BUFFER_DETECTION", "UseAspectRatioHeuristics", _use_aspect_ratio_heuristics);
	});
#endif
}

bool reshade::vulkan::runtime_vk::on_init(VkSwapchainKHR swapchain, const VkSwapchainCreateInfoKHR &desc, HWND hwnd)
{
	// Update swapchain to the new one
	_swapchain = swapchain;

	RECT window_rect = {};
	GetClientRect(hwnd, &window_rect);

	_width = desc.imageExtent.width;
	_height = desc.imageExtent.height;
	_window_width = window_rect.right - window_rect.left;
	_window_height = window_rect.bottom - window_rect.top;
	_color_bit_depth = desc.imageFormat >= VK_FORMAT_A2R10G10B10_UNORM_PACK32 && desc.imageFormat <= VK_FORMAT_A2B10G10R10_SINT_PACK32 ? 10 : 8;
	_backbuffer_format = desc.imageFormat;

	// Create back buffer shader image
	_backbuffer_image = create_image(
		_width, _height, 1, _backbuffer_format,
		VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT);
	if (_backbuffer_image == VK_NULL_HANDLE)
		return false;
	_backbuffer_image_view[0] = create_image_view(_backbuffer_image, make_format_normal(_backbuffer_format), 1, VK_IMAGE_ASPECT_COLOR_BIT);
	if (_backbuffer_image_view[0] == VK_NULL_HANDLE)
		return false;
	_backbuffer_image_view[1] = create_image_view(_backbuffer_image, make_format_srgb(_backbuffer_format), 1, VK_IMAGE_ASPECT_COLOR_BIT);
	if (_backbuffer_image_view[1] == VK_NULL_HANDLE)
		return false;

	// Create effect depth stencil resource
	_effect_depthstencil = create_image(
		_width, _height, 1, VK_FORMAT_D24_UNORM_S8_UINT,
		VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	if (_effect_depthstencil == VK_NULL_HANDLE)
		return false;
	_effect_depthstencil_view = create_image_view(_effect_depthstencil, VK_FORMAT_D24_UNORM_S8_UINT, 1, VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT);
	if (_effect_depthstencil_view == VK_NULL_HANDLE)
		return false;

	// Create default render pass
	for (uint32_t k = 0; k < 2; ++k)
	{
		VkAttachmentReference attachment_refs[2] = {};
		attachment_refs[0].attachment = 0;
		attachment_refs[0].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		attachment_refs[1].attachment = 1;
		attachment_refs[1].layout = VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL;
		VkAttachmentDescription attachment_descs[2] = {};
		attachment_descs[0].format = k == 0 ? make_format_normal(_backbuffer_format) : make_format_srgb(_backbuffer_format);
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
		attachment_descs[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE; // VK_ATTACHMENT_LOAD_OP_LOAD;
		attachment_descs[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachment_descs[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; // VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL;
		attachment_descs[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL;

		VkSubpassDependency subdep = {};
		subdep.srcSubpass = VK_SUBPASS_EXTERNAL;
		subdep.dstSubpass = 0;
		subdep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		subdep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		subdep.srcAccessMask = 0;
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

		check_result(vk.CreateRenderPass(_device, &create_info, nullptr, &_default_render_pass[k])) false;
	}

	// Get back buffer images
	uint32_t num_images = 0;
	check_result(vk.GetSwapchainImagesKHR(_device, _swapchain, &num_images, nullptr)) false;
	_swapchain_images.resize(num_images);
	check_result(vk.GetSwapchainImagesKHR(_device, _swapchain, &num_images, _swapchain_images.data())) false;

	assert(desc.imageUsage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
	_render_area = desc.imageExtent;

	_swapchain_views.resize(num_images * 2);
	_swapchain_frames.resize(num_images * 2);

	for (uint32_t i = 0, k = 0; i < num_images; ++i, k += 2)
	{
		// TODO: This is technically illegal, since swapchain images are not created with VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT, but it seems to work for now
		_swapchain_views[k + 0] = create_image_view(_swapchain_images[i], make_format_normal(desc.imageFormat), 1, VK_IMAGE_ASPECT_COLOR_BIT);
		_swapchain_views[k + 1] = create_image_view(_swapchain_images[i], make_format_srgb(desc.imageFormat), 1, VK_IMAGE_ASPECT_COLOR_BIT);

		const VkImageView attachment_views[2] = { _swapchain_views[k + 0], _effect_depthstencil_view };
		const VkImageView attachment_views_srgb[2] = { _swapchain_views[k + 1], _effect_depthstencil_view };

		{   VkFramebufferCreateInfo create_info { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
			create_info.renderPass = _default_render_pass[0];
			create_info.attachmentCount = 2;
			create_info.pAttachments = attachment_views;
			create_info.width = desc.imageExtent.width;
			create_info.height = desc.imageExtent.height;
			create_info.layers = 1;

			check_result(vk.CreateFramebuffer(_device, &create_info, nullptr, &_swapchain_frames[k + 0])) false;

			create_info.renderPass = _default_render_pass[1];
			create_info.pAttachments = attachment_views_srgb;

			check_result(vk.CreateFramebuffer(_device, &create_info, nullptr, &_swapchain_frames[k + 1])) false;
		}
	}

	// Reset index since a few commands are recorded during initialization below
	_cmd_index = 0;
	VkCommandBuffer cmd_buffers[NUM_COMMAND_FRAMES];

	{   VkCommandPoolCreateInfo create_info { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
		create_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		create_info.queueFamilyIndex = _queue_family_index;

		check_result(vk.CreateCommandPool(_device, &create_info, nullptr, &_cmd_pool)) false;
	}

	{   VkCommandBufferAllocateInfo alloc_info { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
		alloc_info.commandPool = _cmd_pool;
		alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		alloc_info.commandBufferCount = NUM_COMMAND_FRAMES;

		check_result(vk.AllocateCommandBuffers(_device, &alloc_info, cmd_buffers)) VK_NULL_HANDLE;
	}

	for (uint32_t i = 0; i < NUM_COMMAND_FRAMES; ++i)
	{
		_cmd_buffers[i].first = cmd_buffers[i];
		_cmd_buffers[i].second = false; // Command buffers are in initial state

#ifdef _DEBUG
		// The validation layers expect the loader to have set the dispatch pointer, but this does not happen when calling down the chain, so fix it here
		*reinterpret_cast<void **>(cmd_buffers[i]) = *reinterpret_cast<void **>(_device);
#endif

		VkFenceCreateInfo create_info { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
		create_info.flags = VK_FENCE_CREATE_SIGNALED_BIT; // Create signaled so first status check in 'on_present' succeeds

		check_result(vk.CreateFence(_device, &create_info, nullptr, &_cmd_fences[i])) false;
	}

	// Allocate a single descriptor pool for all effects
	{   const VkDescriptorPoolSize pool_sizes[] = {
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 }, // Only need one global UBO per set
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 128 } // TODO: Limit to 128 image bindings per set for now
		};

		VkDescriptorPoolCreateInfo create_info { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
		// No VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT set, so that all descriptors can be reset in one go via vkResetDescriptorPool
		create_info.maxSets = MAX_EFFECT_DESCRIPTOR_SETS; // TODO: Limit to 100 effects for now
		create_info.poolSizeCount = _countof(pool_sizes);
		create_info.pPoolSizes = pool_sizes;

		check_result(vk.CreateDescriptorPool(_device, &create_info, nullptr, &_effect_descriptor_pool)) false;
	}

	{   const VkDescriptorSetLayoutBinding bindings = { 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_ALL_GRAPHICS };
		VkDescriptorSetLayoutCreateInfo create_info { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
		create_info.bindingCount = 1;
		create_info.pBindings = &bindings;

		check_result(vk.CreateDescriptorSetLayout(_device, &create_info, nullptr, &_effect_descriptor_layout)) false;
	}

	// Create an empty image, which is used when no depth buffer was detected (since you cannot bind nothing to a descriptor in Vulkan)
	_empty_depth_image = create_image(1, 1, 1, VK_FORMAT_R16_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	if (_empty_depth_image == VK_NULL_HANDLE)
		return false;
	_empty_depth_image_view = create_image_view(_empty_depth_image, VK_FORMAT_R16_UNORM, 1, VK_IMAGE_ASPECT_COLOR_BIT);
	if (_empty_depth_image_view == VK_NULL_HANDLE)
		return false;
	if (begin_command_buffer())
	{
		transition_layout(vk, _cmd_buffers[_cmd_index].first, _empty_depth_image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		execute_command_buffer();
	}

#if RESHADE_GUI
	if (!init_imgui_resources())
		return false;
#endif

	return runtime::on_init(hwnd);
}
void reshade::vulkan::runtime_vk::on_reset()
{
	runtime::on_reset();

	wait_for_command_buffers(); // Make sure none of the resources below are currently in use

	for (VkImageView view : _swapchain_views)
		vk.DestroyImageView(_device, view, nullptr);
	_swapchain_views.clear();
	for (VkFramebuffer frame : _swapchain_frames)
		vk.DestroyFramebuffer(_device, frame, nullptr);
	_swapchain_frames.clear();
	_swapchain_images.clear();

	for (VkFence &fence : _cmd_fences)
		vk.DestroyFence(_device, fence, nullptr),
		fence = VK_NULL_HANDLE;

	VkCommandBuffer cmd_buffers[NUM_COMMAND_FRAMES] = {};
	for (uint32_t i = 0; i < NUM_COMMAND_FRAMES; ++i)
		std::swap(cmd_buffers[i], _cmd_buffers[i].first);
	vk.FreeCommandBuffers(_device, _cmd_pool, NUM_COMMAND_FRAMES, cmd_buffers);
	vk.DestroyCommandPool(_device, _cmd_pool, nullptr);
	_cmd_pool = VK_NULL_HANDLE;

	_backbuffer_image_view[1] = VK_NULL_HANDLE;
	vk.DestroyImage(_device, _backbuffer_image, nullptr);
	vk.DestroyImageView(_device, _backbuffer_image_view[0], nullptr);
	_backbuffer_image_view[0] = VK_NULL_HANDLE;
	vk.DestroyImageView(_device, _backbuffer_image_view[1], nullptr);
	_backbuffer_image = VK_NULL_HANDLE;
	vk.DestroyImageView(_device, _depth_image_view, nullptr);
	_depth_image_view = VK_NULL_HANDLE;

	vk.DestroyRenderPass(_device, _default_render_pass[0], nullptr);
	_default_render_pass[0] = VK_NULL_HANDLE;
	vk.DestroyRenderPass(_device, _default_render_pass[1], nullptr);
	_default_render_pass[1] = VK_NULL_HANDLE;

	vk.DestroyImage(_device, _empty_depth_image, nullptr);
	_empty_depth_image = VK_NULL_HANDLE;
	vk.DestroyImageView(_device, _empty_depth_image_view, nullptr);
	_empty_depth_image_view = VK_NULL_HANDLE;

	vk.DestroyImage(_device, _effect_depthstencil, nullptr);
	_effect_depthstencil = VK_NULL_HANDLE;
	vk.DestroyImageView(_device, _effect_depthstencil_view, nullptr);
	_effect_depthstencil_view = VK_NULL_HANDLE;
	vk.DestroyDescriptorPool(_device, _effect_descriptor_pool, nullptr);
	_effect_descriptor_pool = VK_NULL_HANDLE;
	vk.DestroyDescriptorSetLayout(_device, _effect_descriptor_layout, nullptr);
	_effect_descriptor_layout = VK_NULL_HANDLE;

#if RESHADE_GUI
	vk.FreeMemory(_device, _imgui_index_mem, nullptr);
	_imgui_index_mem = VK_NULL_HANDLE;
	vk.DestroyBuffer(_device, _imgui_index_buffer, nullptr);
	_imgui_index_buffer = VK_NULL_HANDLE;
	_imgui_index_buffer_size = 0;
	vk.FreeMemory(_device, _imgui_vertex_mem, nullptr);
	_imgui_vertex_mem = VK_NULL_HANDLE;
	vk.DestroyBuffer(_device, _imgui_vertex_buffer, nullptr);
	_imgui_vertex_buffer = VK_NULL_HANDLE;
	_imgui_vertex_buffer_size = 0;

	vk.DestroyPipeline(_device, _imgui_pipeline, nullptr);
	_imgui_pipeline = VK_NULL_HANDLE;
	vk.DestroyPipelineLayout(_device, _imgui_pipeline_layout, nullptr);
	_imgui_pipeline_layout = VK_NULL_HANDLE;
	vk.DestroyDescriptorSetLayout(_device, _imgui_descriptor_set_layout, nullptr);
	_imgui_descriptor_set_layout = VK_NULL_HANDLE;
	vk.DestroySampler(_device, _imgui_font_sampler, nullptr);
	_imgui_font_sampler = VK_NULL_HANDLE;
#endif

#if RESHADE_VULKAN_CAPTURE_DEPTH_BUFFERS
	_has_depth_texture = false;
	_depth_image_override = VK_NULL_HANDLE;
#endif

	// Free all device memory allocated via the 'create_image' and 'create_buffer' functions
	for (VkDeviceMemory allocation : _allocations)
		vk.FreeMemory(_device, allocation, nullptr);
	_allocations.clear();
}

void reshade::vulkan::runtime_vk::on_present(VkQueue queue, uint32_t swapchain_image_index, buffer_detection_context &tracker)
{
	if (!_is_initialized)
		return;

	_vertices = tracker.total_vertices();
	_drawcalls = tracker.total_drawcalls();

	_cmd_index = _framecount % NUM_COMMAND_FRAMES;
	_swap_index = swapchain_image_index;
	const VkFence fence = _cmd_fences[_cmd_index];

	// Make sure all buffers from the command pool used this frame have finished before resetting it
	if (vk.GetFenceStatus(_device, fence) != VK_SUCCESS)
	{
		LOG(WARN) << "Command pool still has buffers in flight. Skipping frame ...";
		return;
	}

#if RESHADE_VULKAN_CAPTURE_DEPTH_BUFFERS
	_current_tracker = &tracker;
	const auto best_snapshot = tracker.find_best_depth_texture(_use_aspect_ratio_heuristics ? _width : 0, _height, _depth_image_override);
	update_depthstencil_image(_has_high_network_activity ? VK_NULL_HANDLE : best_snapshot.image, best_snapshot.image_layout, best_snapshot.image_info.format);
#endif

	update_and_render_effects();

	runtime::on_present();

	// Submit all asynchronous commands in one batch to the current queue
	if (auto &cmd_info = _cmd_buffers[_cmd_index];
		cmd_info.second)
	{
		// Only reset fence before an actual submit which can signal it again
		vk.ResetFences(_device, 1, &fence);

		check_result(vk.EndCommandBuffer(cmd_info.first));

		VkSubmitInfo submit_info { VK_STRUCTURE_TYPE_SUBMIT_INFO };
		submit_info.commandBufferCount = 1;
		submit_info.pCommandBuffers = &cmd_info.first;

		vk.QueueSubmit(queue, 1, &submit_info, fence);

		cmd_info.second = false; // Command buffer is now in invalid state and ready for a reset
	}

	// Signal that all fences are currently enqueued, so can be waited upon (see 'wait_for_finish')
	_cmd_index = std::numeric_limits<uint32_t>::max();
}

bool reshade::vulkan::runtime_vk::capture_screenshot(uint8_t *buffer) const
{
	const size_t data_pitch = _width * 4;

	vk_handle<VK_OBJECT_TYPE_BUFFER> intermediate(_device, vk);
	vk_handle<VK_OBJECT_TYPE_DEVICE_MEMORY> intermediate_mem(_device, vk);

	{   VkBufferCreateInfo create_info { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
		create_info.size = _width * _height * 4;
		create_info.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		check_result(vk.CreateBuffer(_device, &create_info, nullptr, &intermediate)) false;

		VkMemoryRequirements reqs = {};
		vk.GetBufferMemoryRequirements(_device, intermediate, &reqs);

		VkMemoryAllocateInfo alloc_info { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
		alloc_info.allocationSize = reqs.size;
		alloc_info.memoryTypeIndex = find_memory_type_index(_memory_props,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, reqs.memoryTypeBits);

		if (alloc_info.memoryTypeIndex == std::numeric_limits<uint32_t>::max())
			return false;

		vk_handle<VK_OBJECT_TYPE_DEVICE_MEMORY> mem(_device, vk);
		check_result(vk.AllocateMemory(_device, &alloc_info, nullptr, &intermediate_mem)) false;
		check_result(vk.BindBufferMemory(_device, intermediate, intermediate_mem, 0)) false;
	}

	if (!begin_command_buffer())
		return false;
	const VkCommandBuffer cmd_list = _cmd_buffers[_cmd_index].first;

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

	// Execute and wait for completion
	execute_command_buffer();

	// Copy data from intermediate image into output buffer
	uint8_t *mapped_data;
	check_result(vk.MapMemory(_device, intermediate_mem, 0, VK_WHOLE_SIZE, 0, reinterpret_cast<void **>(&mapped_data))) false;

	for (uint32_t y = 0; y < _height; y++, buffer += data_pitch, mapped_data += data_pitch)
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
			std::memcpy(buffer, mapped_data, data_pitch);

			for (uint32_t x = 0; x < data_pitch; x += 4)
			{
				buffer[x + 3] = 0xFF; // Clear alpha channel
				if (_backbuffer_format >= VK_FORMAT_B8G8R8A8_UNORM && _backbuffer_format <= VK_FORMAT_B8G8R8A8_SRGB)
					std::swap(buffer[x + 0], buffer[x + 2]); // Format is BGRA, but output should be RGBA, so flip channels
			}
		}
	}

	vk.UnmapMemory(_device, intermediate_mem);

	return true;
}

bool reshade::vulkan::runtime_vk::init_effect(size_t index)
{
	effect &effect = _effects[index];

	vk_handle<VK_OBJECT_TYPE_SHADER_MODULE> module(_device, vk);

	// Load shader module
	{   VkShaderModuleCreateInfo create_info { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
		create_info.codeSize = effect.module.spirv.size() * sizeof(uint32_t);
		create_info.pCode = effect.module.spirv.data();

		const VkResult res = vk.CreateShaderModule(_device, &create_info, nullptr, &module);
		if (res != VK_SUCCESS)
		{
			effect.errors += "Failed to create shader module. Vulkan error code " + std::to_string(res) + ".";
			return false;
		}
	}

	if (_effect_data.size() <= index)
		_effect_data.resize(index + 1);

	vulkan_effect_data &effect_data = _effect_data[index];
	effect_data.module = effect.module;

	// Create query pool for time measurements
	{   VkQueryPoolCreateInfo create_info { VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO };
		create_info.queryType = VK_QUERY_TYPE_TIMESTAMP;
		create_info.queryCount = static_cast<uint32_t>(effect.module.techniques.size() * 2 * NUM_COMMAND_FRAMES);

		check_result(vk.CreateQueryPool(_device, &create_info, nullptr, &effect_data.query_pool)) false;
	}

	// Initialize pipeline layout
	{   std::vector<VkDescriptorSetLayoutBinding> bindings;
		bindings.reserve(effect.module.num_sampler_bindings);
		for (uint32_t i = 0; i < effect.module.num_sampler_bindings; ++i)
			bindings.push_back({ i, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_ALL_GRAPHICS });

		VkDescriptorSetLayoutCreateInfo create_info { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
		create_info.bindingCount = uint32_t(bindings.size());
		create_info.pBindings = bindings.data();

		check_result(vk.CreateDescriptorSetLayout(_device, &create_info, nullptr, &effect_data.set_layout)) false;
	}

	const VkDescriptorSetLayout set_layouts[2] = { _effect_descriptor_layout, effect_data.set_layout };

	{   VkPipelineLayoutCreateInfo create_info { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
		create_info.setLayoutCount = 2; // [0] = Global UBO, [1] = Samplers
		create_info.pSetLayouts = set_layouts;

		check_result(vk.CreatePipelineLayout(_device, &create_info, nullptr, &effect_data.pipeline_layout)) false;
	}

	// Create global uniform buffer object
	if (!effect.uniform_data_storage.empty())
	{
		effect_data.ubo = create_buffer(
			effect.uniform_data_storage.size(),
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
			image_binding.imageView = _backbuffer_image_view[info.srgb];
			break;
		case texture_reference::depth_buffer:
			if (_depth_image_view != VK_NULL_HANDLE)
				image_binding.imageView = _depth_image_view;
			else // Set to a default view to avoid crash because of this being null
				image_binding.imageView = _empty_depth_image_view;
			// Keep track of the depth buffer texture descriptor to simplify updating it
			effect_data.depth_image_binding = info.binding;
			break;
		default:
			image_binding.imageView = existing_texture->impl->as<vulkan_tex_data>()->view[info.srgb];
			break;
		}

		assert(image_binding.imageView != VK_NULL_HANDLE);

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
			check_result(vk.CreateSampler(_device, &create_info, nullptr, &sampler)) false;
			it = _effect_sampler_states.emplace(desc_hash, sampler).first;
		}

		image_binding.sampler = it->second;
	}

	effect_data.image_bindings = image_bindings;

	{   VkDescriptorSetAllocateInfo alloc_info { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
		alloc_info.descriptorPool = _effect_descriptor_pool;
		alloc_info.descriptorSetCount = 2;
		alloc_info.pSetLayouts = set_layouts;

		if (vk.AllocateDescriptorSets(_device, &alloc_info, effect_data.set) != VK_SUCCESS)
		{
			LOG(ERROR) << "Too many effects loaded. Only " << (MAX_EFFECT_DESCRIPTOR_SETS / 2) << " effects can be active simultaneously in Vulkan.";
			return false;
		}

		uint32_t num_writes = 0;
		VkWriteDescriptorSet writes[2];
		const VkDescriptorBufferInfo ubo_info = { effect_data.ubo, 0, VK_WHOLE_SIZE };

		if (effect_data.ubo != VK_NULL_HANDLE)
		{
			writes[num_writes] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
			writes[num_writes].dstSet = effect_data.set[0];
			writes[num_writes].dstBinding = 0;
			writes[num_writes].descriptorCount = 1;
			writes[num_writes].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			writes[num_writes].pBufferInfo = &ubo_info;
			++num_writes;
		}

		if (!image_bindings.empty())
		{
			writes[num_writes] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
			writes[num_writes].dstSet = effect_data.set[1];
			writes[num_writes].dstBinding = 0;
			writes[num_writes].descriptorCount = uint32_t(image_bindings.size());
			writes[num_writes].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			writes[num_writes].pImageInfo = image_bindings.data();
			++num_writes;
		}

		vk.UpdateDescriptorSets(_device, num_writes, writes, 0, nullptr);
	}

	bool success = true;

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

	uint32_t technique_index = 0;
	for (technique &technique : _techniques)
	{
		if (technique.impl != nullptr || technique.effect_index != index)
			continue;

		technique.impl = std::make_unique<vulkan_technique_data>();
		auto &technique_data = *technique.impl->as<vulkan_technique_data>();
		// Offset index so that a query exists for each command frame and two subsequent ones are used for before/after stamps
		technique_data.query_index = technique_index++ * 2 * NUM_COMMAND_FRAMES;

		for (size_t pass_index = 0; pass_index < technique.passes.size(); ++pass_index)
		{
			technique.passes_data.push_back(std::make_unique<vulkan_pass_data>());
			auto &pass_data = *technique.passes_data.back()->as<vulkan_pass_data>();
			const auto &pass_info = technique.passes[pass_index];

			pass_data.runtime = this;

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
				// TODO: Why does the viewport have to be upside down for things to render right?
				0.0f, static_cast<float>(scissor_rect.extent.height),
				static_cast<float>(scissor_rect.extent.width), -static_cast<float>(scissor_rect.extent.height),
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
				attachment_desc.initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				attachment_desc.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			}

			if (pass_info.clear_render_targets)
			{
				pass_data.begin_info.clearValueCount = num_color_attachments;
				pass_data.begin_info.pClearValues = pass_data.clear_values; // These are initialized to zero already
			}

			if (num_color_attachments == 0)
			{
				num_color_attachments = 1;
				pass_data.begin_info.renderPass = _default_render_pass[pass_info.srgb_write_enable];
				pass_data.begin_info.framebuffer = VK_NULL_HANDLE; // Select the correct swapchain frame buffer during rendering
			}
			else
			{
				if (scissor_rect.extent.width == _width && scissor_rect.extent.height == _height)
				{
					num_depth_attachments = 1;
					const uint32_t attach_idx = num_color_attachments;

					attachment_views[attach_idx] = _effect_depthstencil_view;

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

					VkRenderPassCreateInfo create_info{ VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
					create_info.attachmentCount = num_color_attachments + num_depth_attachments;
					create_info.pAttachments = attachment_descs;
					create_info.subpassCount = 1;
					create_info.pSubpasses = &subpass;
					create_info.dependencyCount = 1;
					create_info.pDependencies = &subdep;

					check_result(vk.CreateRenderPass(_device, &create_info, nullptr, &pass_data.begin_info.renderPass)) false;
				}

				{   VkFramebufferCreateInfo create_info{ VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
					create_info.renderPass = pass_data.begin_info.renderPass;
					create_info.attachmentCount = num_color_attachments + num_depth_attachments;
					create_info.pAttachments = attachment_views;
					create_info.width = scissor_rect.extent.width;
					create_info.height = scissor_rect.extent.height;
					create_info.layers = 1;

					check_result(vk.CreateFramebuffer(_device, &create_info, nullptr, &pass_data.begin_info.framebuffer)) false;
				}
			}

			VkSpecializationInfo spec_info { uint32_t(spec_constants.size()), spec_constants.data(), spec_data.size(), spec_data.data() };

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

			VkPipelineMultisampleStateCreateInfo ms_info{ VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
			ms_info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

			VkPipelineColorBlendStateCreateInfo blend_info{ VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
			blend_info.attachmentCount = num_color_attachments;
			blend_info.pAttachments = attachment_blends;

			VkPipelineDepthStencilStateCreateInfo depth_info{ VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
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

			VkGraphicsPipelineCreateInfo create_info{ VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
			create_info.stageCount = _countof(stages);
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

			check_result(vk.CreateGraphicsPipelines(_device, VK_NULL_HANDLE, 1, &create_info, nullptr, &pass_data.pipeline)) false;
		}
	}

	return success;
}
void reshade::vulkan::runtime_vk::unload_effect(size_t index)
{
	wait_for_command_buffers(); // Make sure no effect resources are currently in use

	runtime::unload_effect(index);

	if (index < _effect_data.size())
	{
		vulkan_effect_data &effect_data = _effect_data[index];

		vk.DestroyQueryPool(_device, effect_data.query_pool, nullptr);
		effect_data.query_pool = VK_NULL_HANDLE;
		vk.DestroyPipelineLayout(_device, effect_data.pipeline_layout, nullptr);
		effect_data.pipeline_layout = VK_NULL_HANDLE;
		vk.DestroyDescriptorSetLayout(_device, effect_data.set_layout, nullptr);
		effect_data.set_layout = VK_NULL_HANDLE;
		vk.DestroyBuffer(_device, effect_data.ubo, nullptr);
		effect_data.ubo = VK_NULL_HANDLE;
		vk.FreeMemory(_device, effect_data.ubo_mem, nullptr);
		effect_data.ubo_mem = VK_NULL_HANDLE;
	}
}
void reshade::vulkan::runtime_vk::unload_effects()
{
	wait_for_command_buffers(); // Make sure no effect resources are currently in use

	runtime::unload_effects();

	if (_effect_descriptor_pool != VK_NULL_HANDLE)
	{
		vk.ResetDescriptorPool(_device, _effect_descriptor_pool, 0);
	}

	for (const vulkan_effect_data &data : _effect_data)
	{
		vk.DestroyQueryPool(_device, data.query_pool, nullptr);
		vk.DestroyPipelineLayout(_device, data.pipeline_layout, nullptr);
		vk.DestroyDescriptorSetLayout(_device, data.set_layout, nullptr);
		vk.DestroyBuffer(_device, data.ubo, nullptr);
		vk.FreeMemory(_device, data.ubo_mem, nullptr);
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
	texture.impl = std::make_unique<vulkan_tex_data>();
	auto impl = texture.impl->as<vulkan_tex_data>();
	impl->runtime = this;

	// Do not create resource if it is a reference, it is set in 'render_technique'
	if (texture.impl_reference != texture_reference::none)
		return true;

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
	VkImageUsageFlags usage_flags = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	// Add required TRANSFER_SRC flag for mipmap generation
	if (texture.levels > 1)
		usage_flags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

	VkImageCreateFlags image_flags = 0;
	// Add mutable format flag required to create a SRGB view of the image
	if (impl->formats[1] != VK_FORMAT_UNDEFINED)
		image_flags = VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;
	else
		impl->formats[1] = impl->formats[0];

	impl->image = create_image(
		texture.width, texture.height, texture.levels, impl->formats[0],
		usage_flags,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		image_flags);
	if (impl->image == VK_NULL_HANDLE)
		return false;

#ifdef _DEBUG
	if (vk.DebugMarkerSetObjectNameEXT != nullptr)
	{
		VkDebugMarkerObjectNameInfoEXT name_info{ VK_STRUCTURE_TYPE_DEBUG_MARKER_OBJECT_NAME_INFO_EXT };
		name_info.object = (uint64_t)impl->image;
		name_info.objectType = VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT;
		name_info.pObjectName = texture.unique_name.c_str();

		vk.DebugMarkerSetObjectNameEXT(_device, &name_info);
	}
#endif

	// Create shader views
	impl->view[0] = create_image_view(impl->image, impl->formats[0], VK_REMAINING_MIP_LEVELS, VK_IMAGE_ASPECT_COLOR_BIT);
	impl->view[1] = impl->formats[0] != impl->formats[1] ?
		create_image_view(impl->image, impl->formats[1], VK_REMAINING_MIP_LEVELS, VK_IMAGE_ASPECT_COLOR_BIT) :
		impl->view[0];

	// Create render target views (with a single level)
	if (texture.levels > 1)
	{
		impl->view[2] = create_image_view(impl->image, impl->formats[0], 1, VK_IMAGE_ASPECT_COLOR_BIT);
		impl->view[3] = impl->formats[0] != impl->formats[1] ?
			create_image_view(impl->image, impl->formats[1], 1, VK_IMAGE_ASPECT_COLOR_BIT) :
			impl->view[2];
	}
	else
	{
		impl->view[2] = impl->view[0];
		impl->view[3] = impl->view[1];
	}

	// Transition to shader read image layout
	if (begin_command_buffer())
	{
		transition_layout(vk, _cmd_buffers[_cmd_index].first, impl->image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		execute_command_buffer();
	}

	return true;
}
void reshade::vulkan::runtime_vk::upload_texture(texture &texture, const uint8_t *pixels)
{
	auto impl = texture.impl->as<vulkan_tex_data>();
	assert(impl != nullptr && pixels != nullptr && texture.impl_reference == texture_reference::none);

	// Allocate host memory for upload
	vk_handle<VK_OBJECT_TYPE_BUFFER> intermediate(_device, vk,
		create_buffer(texture.width * texture.height * 4, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT));
	if (intermediate == VK_NULL_HANDLE)
		return;
	vk_handle<VK_OBJECT_TYPE_DEVICE_MEMORY> intermediate_mem(_device, vk, _allocations.back());
	_allocations.pop_back(); // Take ownership of the allocation

	// Fill upload buffer with pixel data
	uint8_t *mapped_data;
	check_result(vk.MapMemory(_device, intermediate_mem, 0, VK_WHOLE_SIZE, 0, reinterpret_cast<void **>(&mapped_data)));

	bool unsupported_format = false;
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
		unsupported_format = true;
		LOG(ERROR) << "Texture upload is not supported for format " << static_cast<unsigned int>(texture.format) << '!';
		break;
	}

	vk.UnmapMemory(_device, intermediate_mem);

	if (unsupported_format || !begin_command_buffer())
		return;
	const VkCommandBuffer cmd_list = _cmd_buffers[_cmd_index].first;

	transition_layout(vk, cmd_list, impl->image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
	{ // Copy data from upload buffer into target texture
		VkBufferImageCopy copy_region = {};
		copy_region.imageExtent = { texture.width, texture.height, 1u };
		copy_region.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };

		vk.CmdCopyBufferToImage(cmd_list, intermediate, impl->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy_region);
	}
	transition_layout(vk, cmd_list, impl->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	generate_mipmaps(texture);

	execute_command_buffer();
}
void reshade::vulkan::runtime_vk::generate_mipmaps(texture &texture)
{
	if (texture.levels <= 1)
		return; // No need to generate mipmaps when texture does not have any

	auto impl = texture.impl->as<vulkan_tex_data>();
	assert(impl != nullptr);

	int32_t width = texture.width;
	int32_t height = texture.height;
	const VkCommandBuffer cmd_list = _cmd_buffers[_cmd_index].first;

	for (uint32_t level = 1; level < texture.levels; ++level, width /= 2, height /= 2)
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
	vulkan_effect_data &effect_data = _effect_data[technique.effect_index];
	vulkan_technique_data &technique_data = *technique.impl->as<vulkan_technique_data>();

	// Evaluate queries from oldest frame in queue
	if (uint64_t timestamps[2];
		vk.GetQueryPoolResults(_device, effect_data.query_pool,
			technique_data.query_index + ((_cmd_index + 1) % NUM_COMMAND_FRAMES) * 2, 2,
		sizeof(timestamps), timestamps, sizeof(uint64_t), VK_QUERY_RESULT_64_BIT) == VK_SUCCESS)
	{
		technique.average_gpu_duration.append(timestamps[1] - timestamps[0]);
	}

	if (!begin_command_buffer())
		return;
	const VkCommandBuffer cmd_list = _cmd_buffers[_cmd_index].first;

	// Reset current queries and then write time stamp value
	vk.CmdResetQueryPool(cmd_list, effect_data.query_pool, technique_data.query_index + _cmd_index * 2, 2);
	vk.CmdWriteTimestamp(cmd_list, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, effect_data.query_pool, technique_data.query_index + _cmd_index * 2);

	vk.CmdBindDescriptorSets(cmd_list, VK_PIPELINE_BIND_POINT_GRAPHICS, effect_data.pipeline_layout, 0, 2, effect_data.set, 0, nullptr);

	// Setup shader constants
	if (effect_data.ubo != VK_NULL_HANDLE)
		vk.CmdUpdateBuffer(cmd_list, effect_data.ubo, 0, _effects[technique.effect_index].uniform_data_storage.size(), _effects[technique.effect_index].uniform_data_storage.data());

	// Clear default depth stencil
	const VkImageSubresourceRange clear_range = { VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT, 0, 1, 0, 1 };
	const VkClearDepthStencilValue clear_value = { 1.0f, 0 };
	transition_layout(vk, cmd_list, _effect_depthstencil, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, { VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT, 0, 1, 0, 1 });
	vk.CmdClearDepthStencilImage(cmd_list, _effect_depthstencil, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clear_value, 1, &clear_range);
	transition_layout(vk, cmd_list, _effect_depthstencil, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL, { VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT, 0, 1, 0, 1 });

	if (_depth_image != VK_NULL_HANDLE)
	{
		// Transition layout of depth stencil image
		transition_layout(vk, cmd_list, _depth_image, _depth_image_layout, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, { _depth_image_aspect, 0, 1, 0, 1 });
	}

	for (size_t i = 0; i < technique.passes.size(); ++i)
	{
		const auto &pass_info = technique.passes[i];
		const auto &pass_data = *technique.passes_data[i]->as<vulkan_pass_data>();

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

		VkRenderPassBeginInfo begin_info = pass_data.begin_info;
		if (begin_info.framebuffer == VK_NULL_HANDLE)
			begin_info.framebuffer = _swapchain_frames[_swap_index * 2 + pass_info.srgb_write_enable];
		vk.CmdBeginRenderPass(cmd_list, &begin_info, VK_SUBPASS_CONTENTS_INLINE);

		// Setup states
		vk.CmdBindPipeline(cmd_list, VK_PIPELINE_BIND_POINT_GRAPHICS, pass_data.pipeline);

		// Draw triangle
		vk.CmdDraw(cmd_list, pass_info.num_vertices, 1, 0, 0);

		_vertices += pass_info.num_vertices;
		_drawcalls += 1;

		vk.CmdEndRenderPass(cmd_list);

		// Generate mipmaps
		for (unsigned int k = 0; k < 8 && !pass_info.render_target_names[k].empty(); ++k)
		{
			const auto render_target_texture = std::find_if(_textures.begin(), _textures.end(),
				[&render_target = pass_info.render_target_names[k]](const auto &item) {
				return item.unique_name == render_target;
			});

			generate_mipmaps(*render_target_texture);
		}
	}

	if (_depth_image != VK_NULL_HANDLE)
	{
		assert(_depth_image_layout != VK_IMAGE_LAYOUT_UNDEFINED);

		// Reset image layout of depth stencil image
		transition_layout(vk, cmd_list, _depth_image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, _depth_image_layout, { _depth_image_aspect, 0, 1, 0, 1 });
	}

	vk.CmdWriteTimestamp(cmd_list, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, effect_data.query_pool, technique_data.query_index + _cmd_index * 2 + 1);
}

bool reshade::vulkan::runtime_vk::begin_command_buffer() const
{
	assert(_cmd_index < NUM_COMMAND_FRAMES);

	// Check if this command buffer is in initial state or ready for a reset
	if (auto &cmd_info = _cmd_buffers[_cmd_index];
		!cmd_info.second)
	{
		VkCommandBufferBeginInfo begin_info{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
		begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

		check_result(vk.BeginCommandBuffer(cmd_info.first, &begin_info)) false;

		cmd_info.second = true; // Command buffer is now in recording state
	}

	return true;
}
void reshade::vulkan::runtime_vk::execute_command_buffer() const
{
	auto &cmd_info = _cmd_buffers[_cmd_index];

	assert(cmd_info.second); // Can only execute command buffers that are recording

	check_result(vk.EndCommandBuffer(cmd_info.first));

	VkSubmitInfo submit_info { VK_STRUCTURE_TYPE_SUBMIT_INFO };
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &cmd_info.first;

	const VkFence fence = _cmd_fences[_cmd_index];

	// Wait for fence to become available, then submit using it
	VkResult res = vk.WaitForFences(_device, 1, &fence, VK_TRUE, UINT64_MAX);
	if (res == VK_SUCCESS)
	{
		res = vk.ResetFences(_device, 1, &fence);
		assert(res == VK_SUCCESS);

		// Can use the main queue here, since it is immediately synchronized with the host again anyway
		res = vk.QueueSubmit(_main_queue, 1, &submit_info, fence);
		assert(res == VK_SUCCESS);
		res = vk.WaitForFences(_device, 1, &fence, VK_TRUE, UINT64_MAX);
		assert(res == VK_SUCCESS);
	}

	cmd_info.second = false; // Command buffer is now in invalid state and ready for a reset
}
void reshade::vulkan::runtime_vk::wait_for_command_buffers()
{
	if (_cmd_index < NUM_COMMAND_FRAMES &&
		_cmd_buffers[_cmd_index].second)
		// Make sure any pending work gets executed here, so it is not enqueued later in 'on_present' (at which point the referenced objects may have been destroyed by the code calling this)
		execute_command_buffer();

#if 1
	vk.QueueWaitIdle(_main_queue);
#else
	std::vector<VkFence> pending_fences = _cmd_fences;
	if (_cmd_index < NUM_COMMAND_FRAMES)
	{
		// The current fence cannot be signaled at this point, since it is enqueued later in 'on_present', so remove it from the list of fences to wait on
		pending_fences.erase(pending_fences.begin() + _cmd_index);
	}

	// Wait on all remaining fences (with a timeout of 1 second to be safe)
	vk.WaitForFences(_device, uint32_t(pending_fences.size()), pending_fences.data(), VK_TRUE, 1'000'000'000);
#endif
}

VkImage reshade::vulkan::runtime_vk::create_image(uint32_t width, uint32_t height, uint32_t levels, VkFormat format, VkImageUsageFlags usage_flags, VkMemoryPropertyFlags mem_flags, VkImageCreateFlags flags)
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

	vk_handle<VK_OBJECT_TYPE_IMAGE> res(_device, vk);
	check_result(vk.CreateImage(_device, &create_info, nullptr, &res)) VK_NULL_HANDLE;

	if (mem_flags != 0)
	{
		VkMemoryRequirements reqs = {};
		vk.GetImageMemoryRequirements(_device, res, &reqs);

		// TODO: Implement memory allocator. Shouldn't put everything in dedicated allocations, but eh.
		VkMemoryAllocateInfo alloc_info { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
		alloc_info.allocationSize = reqs.size;
		alloc_info.memoryTypeIndex = find_memory_type_index(_memory_props, mem_flags, reqs.memoryTypeBits);

		if (alloc_info.memoryTypeIndex == std::numeric_limits<uint32_t>::max())
			return VK_NULL_HANDLE;

		VkMemoryDedicatedAllocateInfo dedicated_info { VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO };
		alloc_info.pNext = &dedicated_info;
		dedicated_info.image = res;

		vk_handle<VK_OBJECT_TYPE_DEVICE_MEMORY> mem(_device, vk);
		check_result(vk.AllocateMemory(_device, &alloc_info, nullptr, &mem)) VK_NULL_HANDLE;
		check_result(vk.BindImageMemory(_device, res, mem, 0)) VK_NULL_HANDLE;

		_allocations.push_back(mem.release());
	}

	return res.release();
}
VkImageView reshade::vulkan::runtime_vk::create_image_view(VkImage image, VkFormat format, uint32_t levels, VkImageAspectFlags aspect)
{
	VkImageViewCreateInfo create_info { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
	create_info.image = image;
	create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
	create_info.format = format;
	create_info.components = { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY };
	create_info.subresourceRange = { aspect, 0, levels, 0, 1 };

	vk_handle<VK_OBJECT_TYPE_IMAGE_VIEW> res(_device, vk);
	check_result(vk.CreateImageView(_device, &create_info, nullptr, &res)) VK_NULL_HANDLE;

	return res.release();
}
VkBuffer reshade::vulkan::runtime_vk::create_buffer(VkDeviceSize size, VkBufferUsageFlags usage_flags, VkMemoryPropertyFlags mem_flags)
{
	VkBufferCreateInfo create_info { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
	create_info.size = size;
	create_info.usage = usage_flags;
	create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	vk_handle<VK_OBJECT_TYPE_BUFFER> res(_device, vk);
	check_result(vk.CreateBuffer(_device, &create_info, nullptr, &res)) VK_NULL_HANDLE;

	if (mem_flags != 0)
	{
		VkMemoryRequirements reqs = {};
		vk.GetBufferMemoryRequirements(_device, res, &reqs);

		// TODO: Implement memory allocator. Shouldn't put everything in dedicated allocations, but eh.
		VkMemoryAllocateInfo alloc_info { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
		alloc_info.allocationSize = reqs.size;
		alloc_info.memoryTypeIndex = find_memory_type_index(_memory_props, mem_flags, reqs.memoryTypeBits);

		if (alloc_info.memoryTypeIndex == std::numeric_limits<uint32_t>::max())
			return false;

		VkMemoryDedicatedAllocateInfo dedicated_info { VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO };
		alloc_info.pNext = &dedicated_info;
		dedicated_info.buffer = res;

		vk_handle<VK_OBJECT_TYPE_DEVICE_MEMORY> mem(_device, vk);
		check_result(vk.AllocateMemory(_device, &alloc_info, nullptr, &mem)) VK_NULL_HANDLE;
		check_result(vk.BindBufferMemory(_device, res, mem, 0)) VK_NULL_HANDLE;

		_allocations.push_back(mem.release());
	}

	return res.release();
}
VkBufferView reshade::vulkan::runtime_vk::create_buffer_view(VkBuffer buffer, VkFormat format)
{
	VkBufferViewCreateInfo create_info { VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO };
	create_info.buffer = buffer;
	create_info.format = format;
	create_info.offset = 0;
	create_info.range = VK_WHOLE_SIZE;

	vk_handle<VK_OBJECT_TYPE_BUFFER_VIEW> res(_device, vk);
	check_result(vk.CreateBufferView(_device, &create_info, nullptr, &res)) VK_NULL_HANDLE;

	return res.release();
}

#if RESHADE_GUI
bool reshade::vulkan::runtime_vk::init_imgui_resources()
{
	vk_handle<VK_OBJECT_TYPE_SHADER_MODULE> vs_module(_device, vk);
	vk_handle<VK_OBJECT_TYPE_SHADER_MODULE> fs_module(_device, vk);

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
		check_result(vk.CreateShaderModule(_device, &create_info, nullptr, &vs_module)) false;
		stages[0].module = vs_module;

		const resources::data_resource ps = resources::load_data_resource(IDR_IMGUI_PS_SPIRV);
		create_info.codeSize = ps.data_size;
		create_info.pCode = static_cast<const uint32_t *>(ps.data);
		check_result(vk.CreateShaderModule(_device, &create_info, nullptr, &fs_module)) false;
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

		check_result(vk.CreateSampler(_device, &create_info, nullptr, &_imgui_font_sampler)) false;
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

		check_result(vk.CreateDescriptorSetLayout(_device, &create_info, nullptr, &_imgui_descriptor_set_layout)) false;
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

		check_result(vk.CreatePipelineLayout(_device, &create_info, nullptr, &_imgui_pipeline_layout)) false;
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
	attribute_desc[1].offset = offsetof(ImDrawVert, uv);
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
	create_info.renderPass = _default_render_pass[0];

	check_result(vk.CreateGraphicsPipelines(_device, VK_NULL_HANDLE, 1, &create_info, nullptr, &_imgui_pipeline)) false;

	return true;
}

void reshade::vulkan::runtime_vk::render_imgui_draw_data(ImDrawData *draw_data)
{
	// Need to multi-buffer vertex data so not to modify data below when the previous frame is still in flight
	const unsigned int buffer_index = _framecount % NUM_IMGUI_BUFFERS;

	// Attempt to allocate memory if it failed previously
	bool resize_mem = _imgui_index_buffer == VK_NULL_HANDLE || _imgui_vertex_buffer == VK_NULL_HANDLE;

	// Create and grow vertex/index buffers if needed
	if (_imgui_index_buffer_size < draw_data->TotalIdxCount * sizeof(ImDrawIdx))
	{
		resize_mem = true;
		_imgui_index_buffer_size = (draw_data->TotalIdxCount + 10000) * sizeof(ImDrawIdx);
	}
	if (_imgui_vertex_buffer_size < draw_data->TotalVtxCount * sizeof(ImDrawVert))
	{
		resize_mem = true;
		_imgui_vertex_buffer_size = (draw_data->TotalVtxCount + 5000) * sizeof(ImDrawVert);
	}

	if (resize_mem)
	{
		wait_for_command_buffers(); // Make sure memory is not currently in use before freeing it

		vk.FreeMemory(_device, _imgui_index_mem, nullptr);
		_imgui_index_mem = VK_NULL_HANDLE;
		vk.DestroyBuffer(_device, _imgui_index_buffer, nullptr);
		_imgui_index_buffer = create_buffer(_imgui_index_buffer_size * NUM_IMGUI_BUFFERS, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
		if (_imgui_index_buffer == VK_NULL_HANDLE)
			return;
		_imgui_index_mem = _allocations.back();
		_allocations.pop_back();

		vk.FreeMemory(_device, _imgui_vertex_mem, nullptr);
		_imgui_vertex_mem = VK_NULL_HANDLE;
		vk.DestroyBuffer(_device, _imgui_vertex_buffer, nullptr);
		_imgui_vertex_buffer = create_buffer(_imgui_vertex_buffer_size * NUM_IMGUI_BUFFERS, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
		if (_imgui_vertex_buffer == VK_NULL_HANDLE)
			return;
		_imgui_vertex_mem = _allocations.back();
		_allocations.pop_back();
	}

	// Map only the memory portion associated with the current frame
	ImDrawIdx *idx_dst; ImDrawVert *vtx_dst;
	// TODO: Index buffer is never unmapped in case of failure when mapping vertex buffer
	check_result(vk.MapMemory(_device, _imgui_index_mem, _imgui_index_buffer_size * buffer_index, _imgui_index_buffer_size, 0, reinterpret_cast<void **>(&idx_dst)));
	check_result(vk.MapMemory(_device, _imgui_vertex_mem, _imgui_vertex_buffer_size * buffer_index, _imgui_vertex_buffer_size, 0, reinterpret_cast<void **>(&vtx_dst)));

	for (int n = 0; n < draw_data->CmdListsCount; n++)
	{
		const ImDrawList *draw_list = draw_data->CmdLists[n];
		std::memcpy(idx_dst, draw_list->IdxBuffer.Data, draw_list->IdxBuffer.Size * sizeof(ImDrawIdx));
		std::memcpy(vtx_dst, draw_list->VtxBuffer.Data, draw_list->VtxBuffer.Size * sizeof(ImDrawVert));
		idx_dst += draw_list->IdxBuffer.Size;
		vtx_dst += draw_list->VtxBuffer.Size;
	}

	vk.UnmapMemory(_device, _imgui_index_mem);
	vk.UnmapMemory(_device, _imgui_vertex_mem);

	if (!begin_command_buffer())
		return;
	const VkCommandBuffer cmd_list = _cmd_buffers[_cmd_index].first;

	{   VkRenderPassBeginInfo begin_info { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
		begin_info.renderPass = _default_render_pass[0];
		begin_info.framebuffer = _swapchain_frames[_swap_index * 2];
		begin_info.renderArea.extent = _render_area;
		vk.CmdBeginRenderPass(cmd_list, &begin_info, VK_SUBPASS_CONTENTS_INLINE);
	}

	// Setup render state
	vk.CmdBindPipeline(cmd_list, VK_PIPELINE_BIND_POINT_GRAPHICS, _imgui_pipeline);

	// Setup orthographic projection matrix
	const float scale[2] = {
		2.0f / draw_data->DisplaySize.x,
		2.0f / draw_data->DisplaySize.y
	};
	const float translate[2] = {
		-1.0f - draw_data->DisplayPos.x * scale[0],
		-1.0f - draw_data->DisplayPos.y * scale[1]
	};
	vk.CmdPushConstants(cmd_list, _imgui_pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, sizeof(float) * 0, sizeof(float) * 2, scale);
	vk.CmdPushConstants(cmd_list, _imgui_pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, sizeof(float) * 2, sizeof(float) * 2, translate);

	const VkDeviceSize index_offset = buffer_index * _imgui_index_buffer_size;
	const VkDeviceSize vertex_offset = buffer_index * _imgui_vertex_buffer_size;
	vk.CmdBindIndexBuffer(cmd_list, _imgui_index_buffer, index_offset, sizeof(ImDrawIdx) == 2 ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32);
	vk.CmdBindVertexBuffers(cmd_list, 0, 1, &_imgui_vertex_buffer, &vertex_offset);

	// Set pipeline before binding viewport, since the pipelines has to enable dynamic viewport first
	const VkViewport viewport = { 0.0f, 0.0f, draw_data->DisplaySize.x, draw_data->DisplaySize.y, 0.0f, 1.0f };
	vk.CmdSetViewport(cmd_list, 0, 1, &viewport);

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
			vk.CmdSetScissor(cmd_list, 0, 1, &scissor_rect);

			auto tex_data = static_cast<const vulkan_tex_data *>(cmd.TextureId);

			VkWriteDescriptorSet write { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
			write.dstBinding = 0;
			write.descriptorCount = 1;
			write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			const VkDescriptorImageInfo image_info { _imgui_font_sampler, tex_data->view[0], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
			write.pImageInfo = &image_info;
			vk.CmdPushDescriptorSetKHR(cmd_list, VK_PIPELINE_BIND_POINT_GRAPHICS, _imgui_pipeline_layout, 0, 1, &write);

			vk.CmdDrawIndexed(cmd_list, cmd.ElemCount, 1, cmd.IdxOffset + idx_offset, cmd.VtxOffset + vtx_offset, 0);

		}

		idx_offset += draw_list->IdxBuffer.Size;
		vtx_offset += draw_list->VtxBuffer.Size;
	}

	vk.CmdEndRenderPass(cmd_list);
}
#endif

#if RESHADE_VULKAN_CAPTURE_DEPTH_BUFFERS
void reshade::vulkan::runtime_vk::draw_depth_debug_menu()
{
	if (ImGui::CollapsingHeader("Depth Buffers", ImGuiTreeNodeFlags_DefaultOpen))
	{
		if (ImGui::Checkbox("Use aspect ratio heuristics", &_use_aspect_ratio_heuristics))
			runtime::save_config();

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		for (const auto &[depth_image, snapshot] : _current_tracker->depth_buffer_counters())
		{
			char label[512] = "";
			sprintf_s(label, "%s0x%016llx", (depth_image == _depth_image ? "> " : "  "), (uint64_t)depth_image);

			const bool msaa = snapshot.image_info.samples != VK_SAMPLE_COUNT_1_BIT;
			if (msaa) // Disable widget for MSAA textures
			{
				ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
				ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
			}

			if (bool value = (_depth_image_override == depth_image);
				ImGui::Checkbox(label, &value))
				_depth_image_override = value ? depth_image : VK_NULL_HANDLE;

			ImGui::SameLine();
			ImGui::Text("| %4ux%-4u | %5u draw calls ==> %8u vertices |%s",
				snapshot.image_info.extent.width, snapshot.image_info.extent.height, snapshot.stats.drawcalls, snapshot.stats.vertices, (msaa ? " MSAA" : ""));

			if (msaa)
			{
				ImGui::PopStyleColor();
				ImGui::PopItemFlag();
			}
		}

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();
	}
}

void reshade::vulkan::runtime_vk::update_depthstencil_image(VkImage image, VkImageLayout layout, VkFormat image_format)
{
	if (image == _depth_image)
		return;

	assert(layout != VK_IMAGE_LAYOUT_UNDEFINED || image == VK_NULL_HANDLE);

	_depth_image = image;
	_depth_image_layout = layout;
	_depth_image_aspect = aspect_flags_from_format(image_format);

	// Make sure all previous frames have finished before freeing the image view and updating descriptors (since they may be in use otherwise)
	wait_for_command_buffers();

	vk.DestroyImageView(_device, _depth_image_view, nullptr);
	_depth_image_view = VK_NULL_HANDLE;

	VkDescriptorImageInfo image_binding = { VK_NULL_HANDLE, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };

	if (image != VK_NULL_HANDLE)
	{
		_depth_image_view = create_image_view(image, image_format, 1, VK_IMAGE_ASPECT_DEPTH_BIT);
		image_binding.imageView = _depth_image_view;

		for (auto &tex : _textures)
		{
			if (tex.impl != nullptr && tex.impl_reference == texture_reference::depth_buffer)
			{
				tex.width = frame_width();
				tex.height = frame_height();
			}
		}

		_has_depth_texture = true;
	}
	else
	{
		image_binding.imageView = _empty_depth_image_view;

		_has_depth_texture = false;
	}

	// Update image bindings
	std::vector<VkWriteDescriptorSet> writes;

	for (vulkan_effect_data &effect_data : _effect_data)
	{
		if (effect_data.set[1] == VK_NULL_HANDLE)
			continue; // Skip effects without image bindings
		if (effect_data.depth_image_binding == std::numeric_limits<uint32_t>::max())
			continue; // Skip effects that do not have a depth buffer binding

		// Set sampler handle, which should be the same for all writes
		assert(image_binding.sampler == VK_NULL_HANDLE || image_binding.sampler == effect_data.image_bindings[effect_data.depth_image_binding].sampler);
		image_binding.sampler = effect_data.image_bindings[effect_data.depth_image_binding].sampler;

		VkWriteDescriptorSet write{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
		write.dstSet = effect_data.set[1];
		write.dstBinding = effect_data.depth_image_binding;
		write.descriptorCount = 1;
		write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		write.pImageInfo = &image_binding;

		writes.push_back(std::move(write));
	}

	vk.UpdateDescriptorSets(_device, uint32_t(writes.size()), writes.data(), 0, nullptr);
}
#endif
