/*
 * Copyright (C) 2014 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause OR MIT
 */

#include "dll_log.hpp"
#include "lockfree_linear_map.hpp"
#include "vulkan_hooks.hpp"
#include "vulkan_impl_device.hpp"
#include "vulkan_impl_command_list.hpp"
#include "vulkan_impl_type_convert.hpp"
#include <algorithm>

extern lockfree_linear_map<void *, reshade::vulkan::device_impl *, 8> g_vulkan_devices;

#define GET_DISPATCH_PTR(name, object) \
	GET_DISPATCH_PTR_FROM(name, g_vulkan_devices.at(dispatch_key_from_handle(object)))
#define GET_DISPATCH_PTR_FROM(name, data) \
	assert((data) != nullptr); \
	PFN_vk##name trampoline = (data)->_dispatch_table.name; \
	assert(trampoline != nullptr)

#if RESHADE_ADDON
extern VkImageAspectFlags aspect_flags_from_format(VkFormat format);

static void invoke_begin_render_pass_event(const reshade::vulkan::device_impl *device_impl, reshade::vulkan::object_data<VK_OBJECT_TYPE_COMMAND_BUFFER> *cmd_impl, const VkRenderPassBeginInfo *begin_info)
{
	if (!reshade::has_addon_event<reshade::addon_event::begin_render_pass>())
		return;

	const auto render_pass_data = device_impl->get_private_data_for_object<VK_OBJECT_TYPE_RENDER_PASS>(cmd_impl->current_render_pass);
	const auto framebuffer_data = device_impl->get_private_data_for_object<VK_OBJECT_TYPE_FRAMEBUFFER>(cmd_impl->current_framebuffer);

	const VkSubpassDescription &subpass = render_pass_data->subpasses[cmd_impl->current_subpass];

	uint32_t num_transitions = 0;
	temp_mem<VkImageMemoryBarrier, 9> transitions(subpass.colorAttachmentCount + 1);

	temp_mem<reshade::api::render_pass_render_target_desc, 8> rts(subpass.colorAttachmentCount);
	for (uint32_t i = 0; i < subpass.colorAttachmentCount; ++i)
	{
		reshade::api::render_pass_render_target_desc &rt = rts[i];
		const uint32_t a = subpass.pColorAttachments[i].attachment;

		if (a != VK_ATTACHMENT_UNUSED)
		{
			const VkAttachmentDescription &desc = render_pass_data->attachments[a];

			rt.view = { (uint64_t)framebuffer_data->attachments[a] };
			rt.load_op = reshade::vulkan::convert_render_pass_load_op(desc.loadOp);
			rt.store_op = reshade::vulkan::convert_render_pass_store_op(desc.storeOp);
			std::memset(rt.clear_color, 0, sizeof(rt.clear_color));

			if (begin_info != nullptr)
			{
				if (desc.loadOp == VK_ATTACHMENT_LOAD_OP_CLEAR && begin_info->clearValueCount > a)
					std::copy_n(begin_info->pClearValues[a].color.float32, 4, rt.clear_color);

				if (desc.initialLayout != VK_IMAGE_LAYOUT_UNDEFINED && desc.initialLayout != VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
				{
					VkImageMemoryBarrier &transition = transitions[num_transitions++];
					transition = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
					transition.oldLayout = desc.initialLayout;
					transition.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
					transition.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
					transition.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
					transition.image = (VkImage)device_impl->get_resource_from_view(rt.view).handle;;
					transition.subresourceRange = { aspect_flags_from_format(desc.format), 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS };
				}
			}
		}
		else
		{
			rt.view = { 0 };
			rt.load_op = reshade::api::render_pass_load_op::discard;
			rt.store_op = reshade::api::render_pass_store_op::discard;
			std::memset(rt.clear_color, 0, sizeof(rt.clear_color));
		}
	}

	reshade::api::render_pass_depth_stencil_desc ds;
	if (subpass.pDepthStencilAttachment != nullptr)
	{
		const uint32_t a = subpass.pDepthStencilAttachment->attachment;

		if (a != VK_ATTACHMENT_UNUSED)
		{
			const VkAttachmentDescription &desc = render_pass_data->attachments[a];

			ds.view = { (uint64_t)framebuffer_data->attachments[a] };
			ds.depth_load_op = reshade::vulkan::convert_render_pass_load_op(desc.loadOp);
			ds.depth_store_op = reshade::vulkan::convert_render_pass_store_op(desc.storeOp);
			ds.stencil_load_op = reshade::vulkan::convert_render_pass_load_op(desc.stencilLoadOp);
			ds.stencil_store_op = reshade::vulkan::convert_render_pass_store_op(desc.stencilStoreOp);
			ds.clear_depth = 0.0f;
			ds.clear_stencil = 0;

			if (begin_info != nullptr)
			{
				if (desc.loadOp == VK_ATTACHMENT_LOAD_OP_CLEAR && begin_info->clearValueCount > a)
				{
					ds.clear_depth = begin_info->pClearValues[a].depthStencil.depth;
					ds.clear_stencil = static_cast<uint8_t>(begin_info->pClearValues[a].depthStencil.stencil);
				}

				if (desc.initialLayout != VK_IMAGE_LAYOUT_UNDEFINED && desc.initialLayout != VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
				{
					VkImageMemoryBarrier &transition = transitions[num_transitions++];
					transition = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
					transition.oldLayout = desc.initialLayout;
					transition.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
					transition.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
					transition.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
					transition.image = (VkImage)device_impl->get_resource_from_view(ds.view).handle;;
					transition.subresourceRange = { aspect_flags_from_format(desc.format), 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS };
				}
			}
		}
		else
		{
			ds.view = { 0 };
			ds.depth_load_op = reshade::api::render_pass_load_op::discard;
			ds.depth_store_op = reshade::api::render_pass_store_op::discard;
			ds.stencil_load_op = reshade::api::render_pass_load_op::discard;
			ds.stencil_store_op = reshade::api::render_pass_store_op::discard;
			ds.clear_depth = 0.0f;
			ds.clear_stencil = 0;
		}
	}

	// The 'begin_render_pass' event assumes the resource to be in 'resource_usage::render_target' or 'reshade_usage::depth_stencil' state, so need to transition here
	if (num_transitions != 0)
		device_impl->_dispatch_table.CmdPipelineBarrier(cmd_impl->_orig, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, num_transitions, transitions.p);

	reshade::invoke_addon_event<reshade::addon_event::begin_render_pass>(cmd_impl, subpass.colorAttachmentCount, rts.p, subpass.pDepthStencilAttachment != nullptr ? &ds : nullptr);

	for (uint32_t i = 0; i < num_transitions; ++i)
		std::swap(transitions[i].oldLayout, transitions[i].newLayout);

	if (num_transitions != 0)
		device_impl->_dispatch_table.CmdPipelineBarrier(cmd_impl->_orig, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, num_transitions, transitions.p);
}
static void invoke_begin_render_pass_event(reshade::vulkan::object_data<VK_OBJECT_TYPE_COMMAND_BUFFER> *cmd_impl, const VkRenderingInfo *rendering_info)
{
	assert(rendering_info != nullptr);

	if (!reshade::has_addon_event<reshade::addon_event::begin_render_pass>())
		return;

	temp_mem<reshade::api::render_pass_render_target_desc, 8> rts(rendering_info->colorAttachmentCount);
	for (uint32_t i = 0; i < rendering_info->colorAttachmentCount; ++i)
	{
		reshade::api::render_pass_render_target_desc &rt = rts[i];
		rt.view = { (uint64_t)rendering_info->pColorAttachments[i].imageView };
		rt.load_op = reshade::vulkan::convert_render_pass_load_op(rendering_info->pColorAttachments[i].loadOp);
		rt.store_op = reshade::vulkan::convert_render_pass_store_op(rendering_info->pColorAttachments[i].storeOp);
		std::copy_n(rendering_info->pColorAttachments[i].clearValue.color.float32, 4, rt.clear_color);
	}

	reshade::api::render_pass_depth_stencil_desc ds;
	if (rendering_info->pDepthAttachment != nullptr)
	{
		ds.view = { (uint64_t)rendering_info->pDepthAttachment->imageView };
		ds.depth_load_op = reshade::vulkan::convert_render_pass_load_op(rendering_info->pDepthAttachment->loadOp);
		ds.depth_store_op = reshade::vulkan::convert_render_pass_store_op(rendering_info->pDepthAttachment->storeOp);
		ds.clear_depth = rendering_info->pDepthAttachment->clearValue.depthStencil.depth;
	}
	else
	{
		ds.depth_load_op = reshade::api::render_pass_load_op::discard;
		ds.depth_store_op = reshade::api::render_pass_store_op::discard;
		ds.clear_depth = 0.0f;
	}
	if (rendering_info->pStencilAttachment != nullptr)
	{
		ds.view = { (uint64_t)rendering_info->pDepthAttachment->imageView };
		ds.stencil_load_op = reshade::vulkan::convert_render_pass_load_op(rendering_info->pStencilAttachment->loadOp);
		ds.stencil_store_op = reshade::vulkan::convert_render_pass_store_op(rendering_info->pStencilAttachment->storeOp);
		ds.clear_stencil = static_cast<uint8_t>(rendering_info->pStencilAttachment->clearValue.depthStencil.stencil);
	}
	else
	{
		ds.stencil_load_op = reshade::api::render_pass_load_op::discard;
		ds.stencil_store_op = reshade::api::render_pass_store_op::discard;
		ds.clear_stencil = 0;
	}

	reshade::invoke_addon_event<reshade::addon_event::begin_render_pass>(cmd_impl, rendering_info->colorAttachmentCount, rts.p, rendering_info->pDepthAttachment != nullptr || rendering_info->pStencilAttachment != nullptr ? &ds : nullptr);
}

static inline uint32_t calc_subresource_index(reshade::vulkan::device_impl *device, VkImage image, const VkImageSubresourceLayers &layers, uint32_t layer = 0)
{
	const uint32_t levels = device->get_private_data_for_object<VK_OBJECT_TYPE_IMAGE>(image)->create_info.mipLevels;
	return layers.mipLevel + (layers.baseArrayLayer + layer) * levels;
}
#endif

VkResult VKAPI_CALL vkBeginCommandBuffer(VkCommandBuffer commandBuffer, const VkCommandBufferBeginInfo *pBeginInfo)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(commandBuffer));

#if RESHADE_ADDON
	const auto cmd_impl = device_impl->get_private_data_for_object<VK_OBJECT_TYPE_COMMAND_BUFFER>(commandBuffer);

	// Begin does perform an implicit reset if command pool was created with 'VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT'
	reshade::invoke_addon_event<reshade::addon_event::reset_command_list>(cmd_impl);

	if (pBeginInfo->pInheritanceInfo != nullptr && (pBeginInfo->flags & VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT) != 0)
	{
		const VkCommandBufferInheritanceInfo &inheritance_info = *pBeginInfo->pInheritanceInfo;

		cmd_impl->current_subpass = inheritance_info.subpass;
		cmd_impl->current_render_pass = inheritance_info.renderPass;
		cmd_impl->current_framebuffer = inheritance_info.framebuffer;

		invoke_begin_render_pass_event(device_impl, cmd_impl, nullptr);
	}
#endif

	GET_DISPATCH_PTR_FROM(BeginCommandBuffer, device_impl);
	const VkResult result = trampoline(commandBuffer, pBeginInfo);
#if RESHADE_VERBOSE_LOG
	if (result < VK_SUCCESS)
	{
		LOG(WARN) << "vkBeginCommandBuffer" << " failed with error code " << result << '.';
	}
#endif
	return result;
}
VkResult VKAPI_CALL vkEndCommandBuffer(VkCommandBuffer commandBuffer)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(commandBuffer));

#if RESHADE_ADDON
	const auto cmd_impl = device_impl->get_private_data_for_object<VK_OBJECT_TYPE_COMMAND_BUFFER>(commandBuffer);

	if (cmd_impl->current_render_pass != VK_NULL_HANDLE)
	{
		reshade::invoke_addon_event<reshade::addon_event::end_render_pass>(cmd_impl);

		cmd_impl->current_subpass = std::numeric_limits<uint32_t>::max();
		cmd_impl->current_render_pass = VK_NULL_HANDLE;
		cmd_impl->current_framebuffer = VK_NULL_HANDLE;
	}

	reshade::invoke_addon_event<reshade::addon_event::close_command_list>(cmd_impl);
#endif

	GET_DISPATCH_PTR_FROM(EndCommandBuffer, device_impl);
	const VkResult result = trampoline(commandBuffer);
#if RESHADE_VERBOSE_LOG
	if (result < VK_SUCCESS)
	{
		LOG(WARN) << "vkEndCommandBuffer" << " failed with error code " << result << '.';
	}
#endif
	return result;
}

void VKAPI_CALL vkCmdBindPipeline(VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint, VkPipeline pipeline)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(commandBuffer));

	GET_DISPATCH_PTR_FROM(CmdBindPipeline, device_impl);
	trampoline(commandBuffer, pipelineBindPoint, pipeline);

#if RESHADE_ADDON && !RESHADE_ADDON_LITE
	if (!reshade::has_addon_event<reshade::addon_event::bind_pipeline>())
		return;

	reshade::vulkan::command_list_impl *const cmd_impl = device_impl->get_private_data_for_object<VK_OBJECT_TYPE_COMMAND_BUFFER>(commandBuffer);

	const auto pipeline_stages =
		pipelineBindPoint == VK_PIPELINE_BIND_POINT_COMPUTE ? reshade::api::pipeline_stage::all_compute :
		pipelineBindPoint == VK_PIPELINE_BIND_POINT_GRAPHICS ? reshade::api::pipeline_stage::all_graphics :
		static_cast<reshade::api::pipeline_stage>(0); // Unknown pipeline bind point

	reshade::invoke_addon_event<reshade::addon_event::bind_pipeline>(
		cmd_impl,
		pipeline_stages,
		reshade::api::pipeline { (uint64_t)pipeline });
#endif
}

void VKAPI_CALL vkCmdSetViewport(VkCommandBuffer commandBuffer, uint32_t firstViewport, uint32_t viewportCount, const VkViewport *pViewports)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(commandBuffer));

	GET_DISPATCH_PTR_FROM(CmdSetViewport, device_impl);
	trampoline(commandBuffer, firstViewport, viewportCount, pViewports);

#if RESHADE_ADDON
	if (!reshade::has_addon_event<reshade::addon_event::bind_viewports>())
		return;

	reshade::vulkan::command_list_impl *const cmd_impl = device_impl->get_private_data_for_object<VK_OBJECT_TYPE_COMMAND_BUFFER>(commandBuffer);

	temp_mem<reshade::api::viewport> viewport_data(viewportCount);
	for (uint32_t i = 0; i < viewportCount; ++i)
	{
		std::memcpy(&viewport_data[i], &pViewports[i], sizeof(VkViewport));

		// Flip viewport vertically (see 'command_list_impl::bind_viewports')
		viewport_data[i].y += viewport_data[i].height;
		viewport_data[i].height = -viewport_data[i].height;
	}

	reshade::invoke_addon_event<reshade::addon_event::bind_viewports>(
		cmd_impl, firstViewport, viewportCount, viewport_data.p);
#endif
}
void VKAPI_CALL vkCmdSetScissor(VkCommandBuffer commandBuffer, uint32_t firstScissor, uint32_t scissorCount, const VkRect2D *pScissors)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(commandBuffer));

	GET_DISPATCH_PTR_FROM(CmdSetScissor, device_impl);
	trampoline(commandBuffer, firstScissor, scissorCount, pScissors);

#if RESHADE_ADDON
	if (!reshade::has_addon_event<reshade::addon_event::bind_scissor_rects>())
		return;

	reshade::vulkan::command_list_impl *const cmd_impl = device_impl->get_private_data_for_object<VK_OBJECT_TYPE_COMMAND_BUFFER>(commandBuffer);

	temp_mem<reshade::api::rect> rect_data(scissorCount);
	for (uint32_t i = 0; i < scissorCount; ++i)
	{
		rect_data[i].left = pScissors[i].offset.x;
		rect_data[i].top = pScissors[i].offset.y;
		rect_data[i].right = pScissors[i].offset.x + static_cast<int32_t>(pScissors[i].extent.width);
		rect_data[i].bottom = pScissors[i].offset.y + static_cast<int32_t>(pScissors[i].extent.height);
	}

	reshade::invoke_addon_event<reshade::addon_event::bind_scissor_rects>(
		cmd_impl, firstScissor, scissorCount, rect_data.p);
#endif
}

void VKAPI_CALL vkCmdSetDepthBias(VkCommandBuffer commandBuffer, float depthBiasConstantFactor, float depthBiasClamp, float depthBiasSlopeFactor)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(commandBuffer));

	GET_DISPATCH_PTR_FROM(CmdSetDepthBias, device_impl);
	trampoline(commandBuffer, depthBiasConstantFactor, depthBiasClamp, depthBiasSlopeFactor);

#if RESHADE_ADDON && !RESHADE_ADDON_LITE
	if (!reshade::has_addon_event<reshade::addon_event::bind_pipeline_states>())
		return;

	reshade::vulkan::command_list_impl *const cmd_impl = device_impl->get_private_data_for_object<VK_OBJECT_TYPE_COMMAND_BUFFER>(commandBuffer);

	const reshade::api::dynamic_state states[3] = { reshade::api::dynamic_state::depth_bias, reshade::api::dynamic_state::depth_bias_clamp, reshade::api::dynamic_state::depth_bias_slope_scaled };
	const uint32_t values[3] = { *reinterpret_cast<const uint32_t *>(&depthBiasConstantFactor), *reinterpret_cast<const uint32_t *>(&depthBiasClamp), *reinterpret_cast<const uint32_t *>(&depthBiasSlopeFactor) };

	reshade::invoke_addon_event<reshade::addon_event::bind_pipeline_states>(
		cmd_impl, 3, states, values);
#endif
}
void VKAPI_CALL vkCmdSetBlendConstants(VkCommandBuffer commandBuffer, const float blendConstants[4])
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(commandBuffer));

	GET_DISPATCH_PTR_FROM(CmdSetBlendConstants, device_impl);
	trampoline(commandBuffer, blendConstants);

#if RESHADE_ADDON && !RESHADE_ADDON_LITE
	assert(blendConstants != nullptr);

	if (!reshade::has_addon_event<reshade::addon_event::bind_pipeline_states>())
		return;

	reshade::vulkan::command_list_impl *const cmd_impl = device_impl->get_private_data_for_object<VK_OBJECT_TYPE_COMMAND_BUFFER>(commandBuffer);

	const reshade::api::dynamic_state state = reshade::api::dynamic_state::blend_constant;
	const uint32_t value =
		((static_cast<uint32_t>(blendConstants[0] * 255.f) & 0xFF)      ) |
		((static_cast<uint32_t>(blendConstants[1] * 255.f) & 0xFF) <<  8) |
		((static_cast<uint32_t>(blendConstants[2] * 255.f) & 0xFF) << 16) |
		((static_cast<uint32_t>(blendConstants[3] * 255.f) & 0xFF) << 24);

	reshade::invoke_addon_event<reshade::addon_event::bind_pipeline_states>(cmd_impl, 1, &state, &value);
#endif
}
void VKAPI_CALL vkCmdSetStencilCompareMask(VkCommandBuffer commandBuffer, VkStencilFaceFlags faceMask, uint32_t compareMask)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(commandBuffer));

	GET_DISPATCH_PTR_FROM(CmdSetStencilCompareMask, device_impl);
	trampoline(commandBuffer, faceMask, compareMask);

#if RESHADE_ADDON && !RESHADE_ADDON_LITE
	if (faceMask != VK_STENCIL_FACE_FRONT_AND_BACK || !reshade::has_addon_event<reshade::addon_event::bind_pipeline_states>())
		return;

	reshade::vulkan::command_list_impl *const cmd_impl = device_impl->get_private_data_for_object<VK_OBJECT_TYPE_COMMAND_BUFFER>(commandBuffer);

	const reshade::api::dynamic_state state = reshade::api::dynamic_state::stencil_read_mask;

	reshade::invoke_addon_event<reshade::addon_event::bind_pipeline_states>(cmd_impl, 1, &state, &compareMask);
#endif
}
void VKAPI_CALL vkCmdSetStencilWriteMask(VkCommandBuffer commandBuffer, VkStencilFaceFlags faceMask, uint32_t writeMask)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(commandBuffer));

	GET_DISPATCH_PTR_FROM(CmdSetStencilWriteMask, device_impl);
	trampoline(commandBuffer, faceMask, writeMask);

#if RESHADE_ADDON && !RESHADE_ADDON_LITE
	if (faceMask != VK_STENCIL_FACE_FRONT_AND_BACK || !reshade::has_addon_event<reshade::addon_event::bind_pipeline_states>())
		return;

	reshade::vulkan::command_list_impl *const cmd_impl = device_impl->get_private_data_for_object<VK_OBJECT_TYPE_COMMAND_BUFFER>(commandBuffer);

	const reshade::api::dynamic_state state = reshade::api::dynamic_state::stencil_write_mask;

	reshade::invoke_addon_event<reshade::addon_event::bind_pipeline_states>(cmd_impl, 1, &state, &writeMask);
#endif
}
void VKAPI_CALL vkCmdSetStencilReference(VkCommandBuffer commandBuffer, VkStencilFaceFlags faceMask, uint32_t reference)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(commandBuffer));

	GET_DISPATCH_PTR_FROM(CmdSetStencilReference, device_impl);
	trampoline(commandBuffer, faceMask, reference);

#if RESHADE_ADDON && !RESHADE_ADDON_LITE
	if (faceMask != VK_STENCIL_FACE_FRONT_AND_BACK || !reshade::has_addon_event<reshade::addon_event::bind_pipeline_states>())
		return;

	reshade::vulkan::command_list_impl *const cmd_impl = device_impl->get_private_data_for_object<VK_OBJECT_TYPE_COMMAND_BUFFER>(commandBuffer);

	const reshade::api::dynamic_state state = reshade::api::dynamic_state::stencil_reference_value;

	reshade::invoke_addon_event<reshade::addon_event::bind_pipeline_states>(cmd_impl, 1, &state, &reference);
#endif
}

void VKAPI_CALL vkCmdBindDescriptorSets(VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint, VkPipelineLayout layout, uint32_t firstSet, uint32_t descriptorSetCount, const VkDescriptorSet *pDescriptorSets, uint32_t dynamicOffsetCount, const uint32_t *pDynamicOffsets)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(commandBuffer));

	GET_DISPATCH_PTR_FROM(CmdBindDescriptorSets, device_impl);
	trampoline(commandBuffer, pipelineBindPoint, layout, firstSet, descriptorSetCount, pDescriptorSets, dynamicOffsetCount, pDynamicOffsets);

#if RESHADE_ADDON && !RESHADE_ADDON_LITE
	if (!reshade::has_addon_event<reshade::addon_event::bind_descriptor_sets>())
		return;

	reshade::vulkan::command_list_impl *const cmd_impl = device_impl->get_private_data_for_object<VK_OBJECT_TYPE_COMMAND_BUFFER>(commandBuffer);

	const auto shader_stages =
		pipelineBindPoint == VK_PIPELINE_BIND_POINT_COMPUTE ? reshade::api::shader_stage::all_compute :
		pipelineBindPoint == VK_PIPELINE_BIND_POINT_GRAPHICS ? reshade::api::shader_stage::all_graphics :
		static_cast<reshade::api::shader_stage>(0); // Unknown pipeline bind point

	reshade::invoke_addon_event<reshade::addon_event::bind_descriptor_sets>(
		cmd_impl,
		shader_stages,
		reshade::api::pipeline_layout { (uint64_t)layout },
		firstSet, descriptorSetCount,
		reinterpret_cast<const reshade::api::descriptor_set *>(pDescriptorSets));
#endif
}

void VKAPI_CALL vkCmdBindIndexBuffer(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, VkIndexType indexType)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(commandBuffer));

	GET_DISPATCH_PTR_FROM(CmdBindIndexBuffer, device_impl);
	trampoline(commandBuffer, buffer, offset, indexType);

#if RESHADE_ADDON && !RESHADE_ADDON_LITE
	if (!reshade::has_addon_event<reshade::addon_event::bind_index_buffer>())
		return;

	reshade::vulkan::command_list_impl *const cmd_impl = device_impl->get_private_data_for_object<VK_OBJECT_TYPE_COMMAND_BUFFER>(commandBuffer);

	reshade::invoke_addon_event<reshade::addon_event::bind_index_buffer>(
		cmd_impl, reshade::api::resource { (uint64_t)buffer }, offset, indexType == VK_INDEX_TYPE_UINT8_EXT ? 1 : indexType == VK_INDEX_TYPE_UINT16 ? 2 : 4);
#endif
}
void VKAPI_CALL vkCmdBindVertexBuffers(VkCommandBuffer commandBuffer, uint32_t firstBinding, uint32_t bindingCount, const VkBuffer *pBuffers, const VkDeviceSize *pOffsets)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(commandBuffer));

	GET_DISPATCH_PTR_FROM(CmdBindVertexBuffers, device_impl);
	trampoline(commandBuffer, firstBinding, bindingCount, pBuffers, pOffsets);

#if RESHADE_ADDON && !RESHADE_ADDON_LITE
	if (!reshade::has_addon_event<reshade::addon_event::bind_vertex_buffers>())
		return;

	reshade::vulkan::command_list_impl *const cmd_impl = device_impl->get_private_data_for_object<VK_OBJECT_TYPE_COMMAND_BUFFER>(commandBuffer);

	static_assert(sizeof(*pBuffers) == sizeof(reshade::api::resource));

	reshade::invoke_addon_event<reshade::addon_event::bind_vertex_buffers>(
		cmd_impl, firstBinding, bindingCount, reinterpret_cast<const reshade::api::resource *>(pBuffers), pOffsets, nullptr);
#endif
}

void VKAPI_CALL vkCmdDraw(VkCommandBuffer commandBuffer, uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(commandBuffer));
#if RESHADE_ADDON
	reshade::vulkan::command_list_impl *const cmd_impl = device_impl->get_private_data_for_object<VK_OBJECT_TYPE_COMMAND_BUFFER>(commandBuffer);

	if (reshade::invoke_addon_event<reshade::addon_event::draw>(cmd_impl, vertexCount, instanceCount, firstVertex, firstInstance))
		return;
#endif

	GET_DISPATCH_PTR_FROM(CmdDraw, device_impl);
	trampoline(commandBuffer, vertexCount, instanceCount, firstVertex, firstInstance);
}
void VKAPI_CALL vkCmdDrawIndexed(VkCommandBuffer commandBuffer, uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(commandBuffer));
#if RESHADE_ADDON
	reshade::vulkan::command_list_impl *const cmd_impl = device_impl->get_private_data_for_object<VK_OBJECT_TYPE_COMMAND_BUFFER>(commandBuffer);

	if (reshade::invoke_addon_event<reshade::addon_event::draw_indexed>(cmd_impl, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance))
		return;
#endif

	GET_DISPATCH_PTR_FROM(CmdDrawIndexed, device_impl);
	trampoline(commandBuffer, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
}
void VKAPI_CALL vkCmdDrawIndirect(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, uint32_t drawCount, uint32_t stride)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(commandBuffer));
#if RESHADE_ADDON
	reshade::vulkan::command_list_impl *const cmd_impl = device_impl->get_private_data_for_object<VK_OBJECT_TYPE_COMMAND_BUFFER>(commandBuffer);

	if (reshade::invoke_addon_event<reshade::addon_event::draw_or_dispatch_indirect>(cmd_impl, reshade::api::indirect_command::draw, reshade::api::resource { (uint64_t)buffer }, offset, drawCount, stride))
		return;
#endif

	GET_DISPATCH_PTR_FROM(CmdDrawIndirect, device_impl);
	trampoline(commandBuffer, buffer, offset, drawCount, stride);
}
void VKAPI_CALL vkCmdDrawIndexedIndirect(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, uint32_t drawCount, uint32_t stride)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(commandBuffer));
#if RESHADE_ADDON
	reshade::vulkan::command_list_impl *const cmd_impl = device_impl->get_private_data_for_object<VK_OBJECT_TYPE_COMMAND_BUFFER>(commandBuffer);

	if (reshade::invoke_addon_event<reshade::addon_event::draw_or_dispatch_indirect>(cmd_impl, reshade::api::indirect_command::draw_indexed, reshade::api::resource { (uint64_t)buffer }, offset, drawCount, stride))
		return;
#endif

	GET_DISPATCH_PTR_FROM(CmdDrawIndexedIndirect, device_impl);
	trampoline(commandBuffer, buffer, offset, drawCount, stride);
}
void VKAPI_CALL vkCmdDispatch(VkCommandBuffer commandBuffer, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(commandBuffer));
#if RESHADE_ADDON
	reshade::vulkan::command_list_impl *const cmd_impl = device_impl->get_private_data_for_object<VK_OBJECT_TYPE_COMMAND_BUFFER>(commandBuffer);

	if (reshade::invoke_addon_event<reshade::addon_event::dispatch>(cmd_impl, groupCountX, groupCountY, groupCountZ))
		return;
#endif

	GET_DISPATCH_PTR_FROM(CmdDispatch, device_impl);
	trampoline(commandBuffer, groupCountX, groupCountY, groupCountZ);
}
void VKAPI_CALL vkCmdDispatchIndirect(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(commandBuffer));
#if RESHADE_ADDON
	reshade::vulkan::command_list_impl *const cmd_impl = device_impl->get_private_data_for_object<VK_OBJECT_TYPE_COMMAND_BUFFER>(commandBuffer);

	if (reshade::invoke_addon_event<reshade::addon_event::draw_or_dispatch_indirect>(cmd_impl, reshade::api::indirect_command::dispatch, reshade::api::resource { (uint64_t)buffer }, offset, 1, 0))
		return;
#endif

	GET_DISPATCH_PTR_FROM(CmdDispatchIndirect, device_impl);
	trampoline(commandBuffer, buffer, offset);
}

void VKAPI_CALL vkCmdCopyBuffer(VkCommandBuffer commandBuffer, VkBuffer srcBuffer, VkBuffer dstBuffer, uint32_t regionCount, const VkBufferCopy *pRegions)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(commandBuffer));

#if RESHADE_ADDON && !RESHADE_ADDON_LITE
	if (reshade::has_addon_event<reshade::addon_event::copy_buffer_region>())
	{
		reshade::vulkan::command_list_impl *const cmd_impl = device_impl->get_private_data_for_object<VK_OBJECT_TYPE_COMMAND_BUFFER>(commandBuffer);

		for (uint32_t i = 0; i < regionCount; ++i)
		{
			const VkBufferCopy &region = pRegions[i];

			if (reshade::invoke_addon_event<reshade::addon_event::copy_buffer_region>(
					cmd_impl,
					reshade::api::resource { (uint64_t)srcBuffer }, region.srcOffset,
					reshade::api::resource { (uint64_t)dstBuffer }, region.dstOffset, region.size))
				return; // TODO: This skips copy of all regions, rather than just the one specified to this event call
		}
	}
#endif

	GET_DISPATCH_PTR_FROM(CmdCopyBuffer, device_impl);
	trampoline(commandBuffer, srcBuffer, dstBuffer, regionCount, pRegions);
}
void VKAPI_CALL vkCmdCopyImage(VkCommandBuffer commandBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkImage dstImage, VkImageLayout dstImageLayout, uint32_t regionCount, const VkImageCopy *pRegions)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(commandBuffer));

#if RESHADE_ADDON && !RESHADE_ADDON_LITE
	if (reshade::has_addon_event<reshade::addon_event::copy_texture_region>())
	{
		reshade::vulkan::command_list_impl *const cmd_impl = device_impl->get_private_data_for_object<VK_OBJECT_TYPE_COMMAND_BUFFER>(commandBuffer);

		for (uint32_t i = 0; i < regionCount; ++i)
		{
			const VkImageCopy &region = pRegions[i];

			const reshade::api::subresource_box src_box = {
				region.srcOffset.x,
				region.srcOffset.y,
				region.srcOffset.z,
				region.srcOffset.x + static_cast<int32_t>(region.extent.width),
				region.srcOffset.y + static_cast<int32_t>(region.extent.height),
				region.srcOffset.z + static_cast<int32_t>(region.extent.depth)
			};
			const reshade::api::subresource_box dst_box = {
				region.dstOffset.x,
				region.dstOffset.y,
				region.dstOffset.z,
				region.dstOffset.x + static_cast<int32_t>(region.extent.width),
				region.dstOffset.y + static_cast<int32_t>(region.extent.height),
				region.dstOffset.z + static_cast<int32_t>(region.extent.depth)
			};

			for (uint32_t layer = 0; layer < region.srcSubresource.layerCount; ++layer)
			{
				if (reshade::invoke_addon_event<reshade::addon_event::copy_texture_region>(
						cmd_impl,
						reshade::api::resource { (uint64_t)srcImage }, calc_subresource_index(device_impl, srcImage, region.srcSubresource, layer), &src_box,
						reshade::api::resource { (uint64_t)dstImage }, calc_subresource_index(device_impl, dstImage, region.dstSubresource, layer), &dst_box,
						reshade::api::filter_mode::min_mag_mip_point))
					return; // TODO: This skips copy of all regions, rather than just the one specified to this event call
			}
		}
	}
#endif

	GET_DISPATCH_PTR_FROM(CmdCopyImage, device_impl);
	trampoline(commandBuffer, srcImage, srcImageLayout, dstImage, dstImageLayout, regionCount, pRegions);
}
void VKAPI_CALL vkCmdBlitImage(VkCommandBuffer commandBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkImage dstImage, VkImageLayout dstImageLayout, uint32_t regionCount, const VkImageBlit *pRegions, VkFilter filter)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(commandBuffer));

#if RESHADE_ADDON && !RESHADE_ADDON_LITE
	if (reshade::has_addon_event<reshade::addon_event::copy_texture_region>())
	{
		reshade::vulkan::command_list_impl *const cmd_impl = device_impl->get_private_data_for_object<VK_OBJECT_TYPE_COMMAND_BUFFER>(commandBuffer);

		for (uint32_t i = 0; i < regionCount; ++i)
		{
			const VkImageBlit &region = pRegions[i];

			for (uint32_t layer = 0; layer < region.srcSubresource.layerCount; ++layer)
			{
				static_assert(sizeof(region.srcOffsets) == sizeof(reshade::api::subresource_box) && sizeof(region.dstOffsets) == sizeof(reshade::api::subresource_box));

				if (reshade::invoke_addon_event<reshade::addon_event::copy_texture_region>(
						cmd_impl,
						reshade::api::resource { (uint64_t)srcImage }, calc_subresource_index(device_impl, srcImage, region.srcSubresource, layer), reinterpret_cast<const reshade::api::subresource_box *>(&region.srcOffsets[0].x),
						reshade::api::resource { (uint64_t)dstImage }, calc_subresource_index(device_impl, dstImage, region.dstSubresource, layer), reinterpret_cast<const reshade::api::subresource_box *>(&region.dstOffsets[0].x),
						filter == VK_FILTER_NEAREST ? reshade::api::filter_mode::min_mag_mip_point : reshade::api::filter_mode::min_mag_mip_linear))
					return; // TODO: This skips copy of all regions, rather than just the one specified to this event call
			}
		}
	}
#endif

	GET_DISPATCH_PTR_FROM(CmdBlitImage, device_impl);
	trampoline(commandBuffer, srcImage, srcImageLayout, dstImage, dstImageLayout, regionCount, pRegions, filter);
}
void VKAPI_CALL vkCmdCopyBufferToImage(VkCommandBuffer commandBuffer, VkBuffer srcBuffer, VkImage dstImage, VkImageLayout dstImageLayout, uint32_t regionCount, const VkBufferImageCopy *pRegions)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(commandBuffer));

#if RESHADE_ADDON && !RESHADE_ADDON_LITE
	if (reshade::has_addon_event<reshade::addon_event::copy_buffer_to_texture>())
	{
		reshade::vulkan::command_list_impl *const cmd_impl = device_impl->get_private_data_for_object<VK_OBJECT_TYPE_COMMAND_BUFFER>(commandBuffer);

		for (uint32_t i = 0; i < regionCount; ++i)
		{
			const VkBufferImageCopy &region = pRegions[i];

			const reshade::api::subresource_box dst_box = {
				region.imageOffset.x,
				region.imageOffset.y,
				region.imageOffset.z,
				region.imageOffset.x + static_cast<int32_t>(region.imageExtent.width),
				region.imageOffset.y + static_cast<int32_t>(region.imageExtent.height),
				region.imageOffset.z + static_cast<int32_t>(region.imageExtent.depth)
			};

			for (uint32_t layer = 0; layer < region.imageSubresource.layerCount; ++layer)
			{
				// TODO: Calculate correct buffer offset for layers following the first
				if (reshade::invoke_addon_event<reshade::addon_event::copy_buffer_to_texture>(
						cmd_impl,
						reshade::api::resource { (uint64_t)srcBuffer }, region.bufferOffset, region.bufferRowLength, region.bufferImageHeight,
						reshade::api::resource { (uint64_t)dstImage  }, calc_subresource_index(device_impl, dstImage, region.imageSubresource, layer), &dst_box))
					return; // TODO: This skips copy of all regions, rather than just the one specified to this event call
			}
		}
	}
#endif

	GET_DISPATCH_PTR_FROM(CmdCopyBufferToImage, device_impl);
	trampoline(commandBuffer, srcBuffer, dstImage, dstImageLayout, regionCount, pRegions);
}
void VKAPI_CALL vkCmdCopyImageToBuffer(VkCommandBuffer commandBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkBuffer dstBuffer, uint32_t regionCount, const VkBufferImageCopy *pRegions)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(commandBuffer));

#if RESHADE_ADDON && !RESHADE_ADDON_LITE
	if (reshade::has_addon_event<reshade::addon_event::copy_texture_to_buffer>())
	{
		reshade::vulkan::command_list_impl *const cmd_impl = device_impl->get_private_data_for_object<VK_OBJECT_TYPE_COMMAND_BUFFER>(commandBuffer);

		for (uint32_t i = 0; i < regionCount; ++i)
		{
			const VkBufferImageCopy &region = pRegions[i];

			const reshade::api::subresource_box src_box = {
				region.imageOffset.x,
				region.imageOffset.y,
				region.imageOffset.z,
				region.imageOffset.x + static_cast<int32_t>(region.imageExtent.width),
				region.imageOffset.y + static_cast<int32_t>(region.imageExtent.height),
				region.imageOffset.z + static_cast<int32_t>(region.imageExtent.depth)
			};

			for (uint32_t layer = 0; layer < region.imageSubresource.layerCount; ++layer)
			{
				// TODO: Calculate correct buffer offset for layers following the first
				if (reshade::invoke_addon_event<reshade::addon_event::copy_texture_to_buffer>(
						cmd_impl,
						reshade::api::resource { (uint64_t)srcImage  }, calc_subresource_index(device_impl, srcImage, region.imageSubresource, layer), &src_box,
						reshade::api::resource { (uint64_t)dstBuffer }, region.bufferOffset, region.bufferRowLength, region.bufferImageHeight))
					return; // TODO: This skips copy of all regions, rather than just the one specified to this event call
			}
		}
	}
#endif

	GET_DISPATCH_PTR_FROM(CmdCopyImageToBuffer, device_impl);
	trampoline(commandBuffer, srcImage, srcImageLayout, dstBuffer, regionCount, pRegions);
}

void VKAPI_CALL vkCmdClearColorImage(VkCommandBuffer commandBuffer, VkImage image, VkImageLayout imageLayout, const VkClearColorValue *pColor, uint32_t rangeCount, const VkImageSubresourceRange *pRanges)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(commandBuffer));

#if RESHADE_ADDON
	if (reshade::has_addon_event<reshade::addon_event::clear_render_target_view>())
	{
		reshade::vulkan::command_list_impl *const cmd_impl = device_impl->get_private_data_for_object<VK_OBJECT_TYPE_COMMAND_BUFFER>(commandBuffer);

		VkImageMemoryBarrier transition { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
		transition.oldLayout = imageLayout;
		transition.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		transition.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		transition.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		transition.image = image;
		transition.subresourceRange.baseMipLevel = std::numeric_limits<uint32_t>::max();
		transition.subresourceRange.baseArrayLayer = std::numeric_limits<uint32_t>::max();

		for (uint32_t i = 0; i < rangeCount; ++i)
		{
			transition.subresourceRange.aspectMask |= pRanges[i].aspectMask;
			transition.subresourceRange.baseMipLevel = std::min(transition.subresourceRange.baseMipLevel, pRanges[i].baseMipLevel);
			transition.subresourceRange.levelCount = std::max(transition.subresourceRange.levelCount, pRanges[i].levelCount);
			transition.subresourceRange.baseArrayLayer = std::min(transition.subresourceRange.baseArrayLayer, pRanges[i].baseArrayLayer);
			transition.subresourceRange.layerCount = std::max(transition.subresourceRange.layerCount, pRanges[i].layerCount);
		}

		// The 'clear_render_target_view' event assumes the resource to be in 'resource_usage::render_target' state, so need to transition here
		device_impl->_dispatch_table.CmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &transition);

		const VkImageView default_view = device_impl->get_private_data_for_object<VK_OBJECT_TYPE_IMAGE>(image)->default_view;
		assert(default_view != VK_NULL_HANDLE);

		const bool skip = reshade::invoke_addon_event<reshade::addon_event::clear_render_target_view>(cmd_impl, reshade::api::resource_view { (uint64_t)default_view }, pColor->float32, 0, nullptr);

		std::swap(transition.oldLayout, transition.newLayout);
		device_impl->_dispatch_table.CmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &transition);

		if (skip)
			return;
	}
#endif

	GET_DISPATCH_PTR_FROM(CmdClearColorImage, device_impl);
	trampoline(commandBuffer, image, imageLayout, pColor, rangeCount, pRanges);
}
void VKAPI_CALL vkCmdClearDepthStencilImage(VkCommandBuffer commandBuffer, VkImage image, VkImageLayout imageLayout, const VkClearDepthStencilValue *pDepthStencil, uint32_t rangeCount, const VkImageSubresourceRange *pRanges)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(commandBuffer));

#if RESHADE_ADDON
	if (reshade::has_addon_event<reshade::addon_event::clear_depth_stencil_view>())
	{
		reshade::vulkan::command_list_impl *const cmd_impl = device_impl->get_private_data_for_object<VK_OBJECT_TYPE_COMMAND_BUFFER>(commandBuffer);

		VkImageMemoryBarrier transition { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
		transition.oldLayout = imageLayout;
		transition.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		transition.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		transition.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		transition.image = image;
		transition.subresourceRange.baseMipLevel = std::numeric_limits<uint32_t>::max();
		transition.subresourceRange.baseArrayLayer = std::numeric_limits<uint32_t>::max();

		for (uint32_t i = 0; i < rangeCount; ++i)
		{
			transition.subresourceRange.aspectMask |= pRanges[i].aspectMask;
			transition.subresourceRange.baseMipLevel = std::min(transition.subresourceRange.baseMipLevel, pRanges[i].baseMipLevel);
			transition.subresourceRange.levelCount = std::max(transition.subresourceRange.levelCount, pRanges[i].levelCount);
			transition.subresourceRange.baseArrayLayer = std::min(transition.subresourceRange.baseArrayLayer, pRanges[i].baseArrayLayer);
			transition.subresourceRange.layerCount = std::max(transition.subresourceRange.layerCount, pRanges[i].layerCount);
		}

		// The 'clear_depth_stencil_view' event assumes the resource to be in 'resource_usage::depth_stencil' state, so need to transition here
		device_impl->_dispatch_table.CmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &transition);

		const VkImageView default_view = device_impl->get_private_data_for_object<VK_OBJECT_TYPE_IMAGE>(image)->default_view;
		assert(default_view != VK_NULL_HANDLE);

		const bool skip = reshade::invoke_addon_event<reshade::addon_event::clear_depth_stencil_view>(
			cmd_impl,
			reshade::api::resource_view { (uint64_t)default_view },
			transition.subresourceRange.aspectMask & VK_IMAGE_ASPECT_DEPTH_BIT ? &pDepthStencil->depth : nullptr,
			transition.subresourceRange.aspectMask & VK_IMAGE_ASPECT_STENCIL_BIT ? reinterpret_cast<const uint8_t *>(&pDepthStencil->stencil) : nullptr,
			0, nullptr);

		std::swap(transition.oldLayout, transition.newLayout);
		device_impl->_dispatch_table.CmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &transition);

		if (skip)
			return;
	}
#endif

	GET_DISPATCH_PTR_FROM(CmdClearDepthStencilImage, device_impl);
	trampoline(commandBuffer, image, imageLayout, pDepthStencil, rangeCount, pRanges);
}
void VKAPI_CALL vkCmdClearAttachments(VkCommandBuffer commandBuffer, uint32_t attachmentCount, const VkClearAttachment *pAttachments, uint32_t rectCount, const VkClearRect *pRects)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(commandBuffer));

	VkClearAttachment *attachments = const_cast<VkClearAttachment *>(pAttachments);
#if RESHADE_ADDON
	temp_mem<VkClearAttachment> new_attachments(attachmentCount);

	if (reshade::has_addon_event<reshade::addon_event::clear_depth_stencil_view>() ||
		reshade::has_addon_event<reshade::addon_event::clear_render_target_view>())
	{
		const auto cmd_impl = device_impl->get_private_data_for_object<VK_OBJECT_TYPE_COMMAND_BUFFER>(commandBuffer);

		const auto render_pass_data = device_impl->get_private_data_for_object<VK_OBJECT_TYPE_RENDER_PASS>(cmd_impl->current_render_pass);
		const auto framebuffer_data = device_impl->get_private_data_for_object<VK_OBJECT_TYPE_FRAMEBUFFER>(cmd_impl->current_framebuffer);

		const VkSubpassDescription &subpass = render_pass_data->subpasses[cmd_impl->current_subpass];

		temp_mem<reshade::api::rect> rect_data(rectCount);
		for (uint32_t i = 0; i < rectCount; ++i)
		{
			rect_data[i].left = pRects[i].rect.offset.x;
			rect_data[i].top = pRects[i].rect.offset.y;
			rect_data[i].right = pRects[i].rect.offset.x + static_cast<int32_t>(pRects[i].rect.extent.width);
			rect_data[i].bottom = pRects[i].rect.offset.y + static_cast<int32_t>(pRects[i].rect.extent.height);
		}

		uint32_t new_attachment_count = 0;
		for (uint32_t i = 0; i < attachmentCount; ++i)
		{
			const VkClearAttachment &clear_attachment = pAttachments[i];

			if (clear_attachment.aspectMask & VK_IMAGE_ASPECT_COLOR_BIT)
			{
				const uint32_t a = subpass.pColorAttachments[clear_attachment.colorAttachment].attachment;
				assert(a != VK_ATTACHMENT_UNUSED);

				if (reshade::invoke_addon_event<reshade::addon_event::clear_render_target_view>(
						cmd_impl,
						reshade::api::resource_view { (uint64_t)framebuffer_data->attachments[a] },
						clear_attachment.clearValue.color.float32,
						rectCount, rect_data.p))
					continue;
			}
			else if (clear_attachment.aspectMask & (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT))
			{
				assert(subpass.pDepthStencilAttachment != nullptr);
				const uint32_t a = subpass.pDepthStencilAttachment->attachment;
				assert(a != VK_ATTACHMENT_UNUSED);

				if (reshade::invoke_addon_event<reshade::addon_event::clear_depth_stencil_view>(
						cmd_impl,
						reshade::api::resource_view { (uint64_t)framebuffer_data->attachments[a] },
						clear_attachment.aspectMask & VK_IMAGE_ASPECT_DEPTH_BIT ? &clear_attachment.clearValue.depthStencil.depth : nullptr,
						clear_attachment.aspectMask & VK_IMAGE_ASPECT_STENCIL_BIT ? reinterpret_cast<const uint8_t *>(&clear_attachment.clearValue.depthStencil.stencil) : nullptr,
						rectCount, rect_data.p))
					continue;
			}

			new_attachments[new_attachment_count++] = clear_attachment;
		}

		if (new_attachment_count == 0)
			return;

		attachments = new_attachments.p;
		attachmentCount = new_attachment_count;
	}
#endif

	GET_DISPATCH_PTR_FROM(CmdClearAttachments, device_impl);
	trampoline(commandBuffer, attachmentCount, attachments, rectCount, pRects);
}

void VKAPI_CALL vkCmdResolveImage(VkCommandBuffer commandBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkImage dstImage, VkImageLayout dstImageLayout, uint32_t regionCount, const VkImageResolve *pRegions)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(commandBuffer));

#if RESHADE_ADDON && !RESHADE_ADDON_LITE
	if (reshade::has_addon_event<reshade::addon_event::resolve_texture_region>())
	{
		reshade::vulkan::command_list_impl *const cmd_impl = device_impl->get_private_data_for_object<VK_OBJECT_TYPE_COMMAND_BUFFER>(commandBuffer);

		for (uint32_t i = 0; i < regionCount; ++i)
		{
			const VkImageResolve &region = pRegions[i];

			const reshade::api::subresource_box src_box = {
				region.srcOffset.x,
				region.srcOffset.y,
				region.srcOffset.z,
				region.srcOffset.x + static_cast<int32_t>(region.extent.width),
				region.srcOffset.y + static_cast<int32_t>(region.extent.height),
				region.srcOffset.z + static_cast<int32_t>(region.extent.depth)
			};

			for (uint32_t layer = 0; layer < region.srcSubresource.layerCount; ++layer)
			{
				if (reshade::invoke_addon_event<reshade::addon_event::resolve_texture_region>(
						cmd_impl,
						reshade::api::resource { (uint64_t)srcImage }, calc_subresource_index(device_impl, srcImage, region.srcSubresource, layer), &src_box,
						reshade::api::resource { (uint64_t)dstImage }, calc_subresource_index(device_impl, dstImage, region.dstSubresource, layer), region.dstOffset.x, region.dstOffset.y, region.dstOffset.z, reshade::api::format::unknown))
					return; // TODO: This skips resolve of all regions, rather than just the one specified to this event call
			}
		}
	}
#endif

	GET_DISPATCH_PTR_FROM(CmdResolveImage, device_impl);
	trampoline(commandBuffer, srcImage, srcImageLayout, dstImage, dstImageLayout, regionCount, pRegions);
}

void VKAPI_CALL vkCmdPipelineBarrier(VkCommandBuffer commandBuffer, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, VkDependencyFlags dependencyFlags, uint32_t memoryBarrierCount, const VkMemoryBarrier *pMemoryBarriers, uint32_t bufferMemoryBarrierCount, const VkBufferMemoryBarrier *pBufferMemoryBarriers, uint32_t imageMemoryBarrierCount, const VkImageMemoryBarrier *pImageMemoryBarriers)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(commandBuffer));

	GET_DISPATCH_PTR_FROM(CmdPipelineBarrier, device_impl);
	trampoline(commandBuffer, srcStageMask, dstStageMask, dependencyFlags, memoryBarrierCount, pMemoryBarriers, bufferMemoryBarrierCount, pBufferMemoryBarriers, imageMemoryBarrierCount, pImageMemoryBarriers);

#if RESHADE_ADDON
	const uint32_t num_barriers = bufferMemoryBarrierCount + imageMemoryBarrierCount;

	if (num_barriers == 0 || !reshade::has_addon_event<reshade::addon_event::barrier>())
		return;

	reshade::vulkan::command_list_impl *const cmd_impl = device_impl->get_private_data_for_object<VK_OBJECT_TYPE_COMMAND_BUFFER>(commandBuffer);

	temp_mem<reshade::api::resource> resources(num_barriers);
	temp_mem<reshade::api::resource_usage> old_state(num_barriers), new_state(num_barriers);

	for (uint32_t i = 0; i < bufferMemoryBarrierCount; ++i)
	{
		const VkBufferMemoryBarrier &barrier = pBufferMemoryBarriers[i];

		resources[i] = { (uint64_t)barrier.buffer };
		old_state[i] = reshade::vulkan::convert_access_to_usage(barrier.srcAccessMask);
		new_state[i] = reshade::vulkan::convert_access_to_usage(barrier.dstAccessMask);
	}
	for (uint32_t i = 0, k = bufferMemoryBarrierCount; i < imageMemoryBarrierCount; ++i, ++k)
	{
		const VkImageMemoryBarrier &barrier = pImageMemoryBarriers[i];

		resources[k] = { (uint64_t)barrier.image };
		old_state[k] = reshade::vulkan::convert_image_layout_to_usage(barrier.oldLayout);
		new_state[k] = reshade::vulkan::convert_image_layout_to_usage(barrier.newLayout);
	}

	reshade::invoke_addon_event<reshade::addon_event::barrier>(cmd_impl, num_barriers, resources.p, old_state.p, new_state.p);
#endif
}

void VKAPI_CALL vkCmdBeginQuery(VkCommandBuffer commandBuffer, VkQueryPool queryPool, uint32_t query, VkQueryControlFlags flags)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(commandBuffer));

#if RESHADE_ADDON && !RESHADE_ADDON_LITE
	reshade::vulkan::command_list_impl *const cmd_impl = device_impl->get_private_data_for_object<VK_OBJECT_TYPE_COMMAND_BUFFER>(commandBuffer);
	const auto pool_data = device_impl->get_private_data_for_object<VK_OBJECT_TYPE_QUERY_POOL>(queryPool);

	if (reshade::invoke_addon_event<reshade::addon_event::begin_query>(cmd_impl, reshade::api::query_pool { (uint64_t)queryPool }, reshade::vulkan::convert_query_type(pool_data->type), query))
		return;
#endif

	GET_DISPATCH_PTR_FROM(CmdBeginQuery, device_impl);
	trampoline(commandBuffer, queryPool, query, flags);
}
void VKAPI_CALL vkCmdEndQuery(VkCommandBuffer commandBuffer, VkQueryPool queryPool, uint32_t query)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(commandBuffer));

#if RESHADE_ADDON && !RESHADE_ADDON_LITE
	reshade::vulkan::command_list_impl *const cmd_impl = device_impl->get_private_data_for_object<VK_OBJECT_TYPE_COMMAND_BUFFER>(commandBuffer);
	const auto pool_data = device_impl->get_private_data_for_object<VK_OBJECT_TYPE_QUERY_POOL>(queryPool);

	if (reshade::invoke_addon_event<reshade::addon_event::end_query>(cmd_impl, reshade::api::query_pool { (uint64_t)queryPool }, reshade::vulkan::convert_query_type(pool_data->type), query))
		return;
#endif

	GET_DISPATCH_PTR_FROM(CmdEndQuery, device_impl);
	trampoline(commandBuffer, queryPool, query);
}
void VKAPI_CALL vkCmdWriteTimestamp(VkCommandBuffer commandBuffer, VkPipelineStageFlagBits pipelineStage, VkQueryPool queryPool, uint32_t query)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(commandBuffer));

#if RESHADE_ADDON && !RESHADE_ADDON_LITE
	reshade::vulkan::command_list_impl *const cmd_impl = device_impl->get_private_data_for_object<VK_OBJECT_TYPE_COMMAND_BUFFER>(commandBuffer);
	assert(device_impl->get_private_data_for_object<VK_OBJECT_TYPE_QUERY_POOL>(queryPool)->type == VK_QUERY_TYPE_TIMESTAMP);

	if (reshade::invoke_addon_event<reshade::addon_event::end_query>(cmd_impl, reshade::api::query_pool { (uint64_t)queryPool }, reshade::api::query_type::timestamp, query))
		return;
#endif

	GET_DISPATCH_PTR_FROM(CmdWriteTimestamp, device_impl);
	trampoline(commandBuffer, pipelineStage, queryPool, query);
}

void VKAPI_CALL vkCmdCopyQueryPoolResults(VkCommandBuffer commandBuffer, VkQueryPool queryPool, uint32_t firstQuery, uint32_t queryCount, VkBuffer dstBuffer, VkDeviceSize dstOffset, VkDeviceSize stride, VkQueryResultFlags flags)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(commandBuffer));

#if RESHADE_ADDON && !RESHADE_ADDON_LITE
	reshade::vulkan::command_list_impl *const cmd_impl = device_impl->get_private_data_for_object<VK_OBJECT_TYPE_COMMAND_BUFFER>(commandBuffer);
	const auto pool_data = device_impl->get_private_data_for_object<VK_OBJECT_TYPE_QUERY_POOL>(queryPool);

	assert(stride <= std::numeric_limits<uint32_t>::max());

	if (reshade::invoke_addon_event<reshade::addon_event::copy_query_pool_results>(
			cmd_impl,
			reshade::api::query_pool { (uint64_t)queryPool },
			reshade::vulkan::convert_query_type(pool_data->type),
			firstQuery,
			queryCount,
			reshade::api::resource { (uint64_t)dstBuffer },
			dstOffset,
			static_cast<uint32_t>(stride)))
		return;
#endif

	GET_DISPATCH_PTR_FROM(CmdCopyQueryPoolResults, device_impl);
	trampoline(commandBuffer, queryPool, firstQuery, queryCount, dstBuffer, dstOffset, stride, flags);
}

void VKAPI_CALL vkCmdPushConstants(VkCommandBuffer commandBuffer, VkPipelineLayout layout, VkShaderStageFlags stageFlags, uint32_t offset, uint32_t size, const void *pValues)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(commandBuffer));

	GET_DISPATCH_PTR_FROM(CmdPushConstants, device_impl);
	trampoline(commandBuffer, layout, stageFlags, offset, size, pValues);

#if RESHADE_ADDON && !RESHADE_ADDON_LITE
	if (!reshade::has_addon_event<reshade::addon_event::push_constants>())
		return;

	reshade::vulkan::command_list_impl *const cmd_impl = device_impl->get_private_data_for_object<VK_OBJECT_TYPE_COMMAND_BUFFER>(commandBuffer);
	const auto layout_data = device_impl->get_private_data_for_object<VK_OBJECT_TYPE_PIPELINE_LAYOUT>(layout);

	reshade::invoke_addon_event<reshade::addon_event::push_constants>(
		cmd_impl,
		static_cast<reshade::api::shader_stage>(stageFlags),
		reshade::api::pipeline_layout { (uint64_t)layout },
		static_cast<uint32_t>(layout_data->set_layouts.size()),
		offset / 4,
		size / 4,
		static_cast<const uint32_t *>(pValues));
#endif
}

void VKAPI_CALL vkCmdBeginRenderPass(VkCommandBuffer commandBuffer, const VkRenderPassBeginInfo *pRenderPassBegin, VkSubpassContents contents)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(commandBuffer));

#if RESHADE_ADDON
	const auto cmd_impl = device_impl->get_private_data_for_object<VK_OBJECT_TYPE_COMMAND_BUFFER>(commandBuffer);

	assert(cmd_impl->current_render_pass == VK_NULL_HANDLE);

	cmd_impl->current_subpass = 0;
	cmd_impl->current_render_pass = pRenderPassBegin->renderPass;
	cmd_impl->current_framebuffer = pRenderPassBegin->framebuffer;

	invoke_begin_render_pass_event(device_impl, cmd_impl, pRenderPassBegin);
#endif

	GET_DISPATCH_PTR_FROM(CmdBeginRenderPass, device_impl);
	trampoline(commandBuffer, pRenderPassBegin, contents);
}
void VKAPI_CALL vkCmdNextSubpass(VkCommandBuffer commandBuffer, VkSubpassContents contents)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(commandBuffer));

#if RESHADE_ADDON
	const auto cmd_impl = device_impl->get_private_data_for_object<VK_OBJECT_TYPE_COMMAND_BUFFER>(commandBuffer);

	assert(cmd_impl->current_render_pass != VK_NULL_HANDLE);

	reshade::invoke_addon_event<reshade::addon_event::end_render_pass>(cmd_impl);

	cmd_impl->current_subpass++;

	invoke_begin_render_pass_event(device_impl, cmd_impl, nullptr);
#endif

	GET_DISPATCH_PTR_FROM(CmdNextSubpass, device_impl);
	trampoline(commandBuffer, contents);
}
void VKAPI_CALL vkCmdEndRenderPass(VkCommandBuffer commandBuffer)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(commandBuffer));

#if RESHADE_ADDON
	const auto cmd_impl = device_impl->get_private_data_for_object<VK_OBJECT_TYPE_COMMAND_BUFFER>(commandBuffer);

	assert(cmd_impl->current_render_pass != VK_NULL_HANDLE);

	reshade::invoke_addon_event<reshade::addon_event::end_render_pass>(cmd_impl);

	cmd_impl->current_subpass = std::numeric_limits<uint32_t>::max();
	cmd_impl->current_render_pass = VK_NULL_HANDLE;
	cmd_impl->current_framebuffer = VK_NULL_HANDLE;
#endif

	GET_DISPATCH_PTR_FROM(CmdEndRenderPass, device_impl);
	trampoline(commandBuffer);
}

void VKAPI_CALL vkCmdExecuteCommands(VkCommandBuffer commandBuffer, uint32_t commandBufferCount, const VkCommandBuffer *pCommandBuffers)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(commandBuffer));

#if RESHADE_ADDON
	if (reshade::has_addon_event<reshade::addon_event::execute_secondary_command_list>())
	{
		reshade::vulkan::command_list_impl *const cmd_impl = device_impl->get_private_data_for_object<VK_OBJECT_TYPE_COMMAND_BUFFER>(commandBuffer);

		for (uint32_t i = 0; i < commandBufferCount; ++i)
		{
			reshade::vulkan::command_list_impl *const secondary_cmd_impl = device_impl->get_private_data_for_object<VK_OBJECT_TYPE_COMMAND_BUFFER>(pCommandBuffers[i]);

			reshade::invoke_addon_event<reshade::addon_event::execute_secondary_command_list>(cmd_impl, secondary_cmd_impl);
		}
	}
#endif

	GET_DISPATCH_PTR_FROM(CmdExecuteCommands, device_impl);
	trampoline(commandBuffer, commandBufferCount, pCommandBuffers);
}

void VKAPI_CALL vkCmdDrawIndirectCount(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, VkBuffer countBuffer, VkDeviceSize countBufferOffset, uint32_t maxDrawCount, uint32_t stride)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(commandBuffer));
#if RESHADE_ADDON
	reshade::vulkan::command_list_impl *const cmd_impl = device_impl->get_private_data_for_object<VK_OBJECT_TYPE_COMMAND_BUFFER>(commandBuffer);

	if (reshade::invoke_addon_event<reshade::addon_event::draw_or_dispatch_indirect>(cmd_impl, reshade::api::indirect_command::draw, reshade::api::resource { (uint64_t)buffer }, offset, maxDrawCount, stride))
		return;
#endif

	GET_DISPATCH_PTR_FROM(CmdDrawIndirectCount, device_impl);
	trampoline(commandBuffer, buffer, offset, countBuffer, countBufferOffset, maxDrawCount, stride);
}
void VKAPI_CALL vkCmdDrawIndexedIndirectCount(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, VkBuffer countBuffer, VkDeviceSize countBufferOffset, uint32_t maxDrawCount, uint32_t stride)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(commandBuffer));
#if RESHADE_ADDON
	reshade::vulkan::command_list_impl *const cmd_impl = device_impl->get_private_data_for_object<VK_OBJECT_TYPE_COMMAND_BUFFER>(commandBuffer);

	if (reshade::invoke_addon_event<reshade::addon_event::draw_or_dispatch_indirect>(cmd_impl, reshade::api::indirect_command::draw_indexed, reshade::api::resource { (uint64_t)buffer }, offset, maxDrawCount, stride))
		return;
#endif

	GET_DISPATCH_PTR_FROM(CmdDrawIndexedIndirectCount, device_impl);
	trampoline(commandBuffer, buffer, offset, countBuffer, countBufferOffset, maxDrawCount, stride);
}

void VKAPI_CALL vkCmdBeginRenderPass2(VkCommandBuffer commandBuffer, const VkRenderPassBeginInfo *pRenderPassBegin, const VkSubpassBeginInfo *pSubpassBeginInfo)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(commandBuffer));

#if RESHADE_ADDON
	const auto cmd_impl = device_impl->get_private_data_for_object<VK_OBJECT_TYPE_COMMAND_BUFFER>(commandBuffer);

	assert(cmd_impl->current_render_pass == VK_NULL_HANDLE);

	cmd_impl->current_subpass = 0;
	cmd_impl->current_render_pass = pRenderPassBegin->renderPass;
	cmd_impl->current_framebuffer = pRenderPassBegin->framebuffer;

	invoke_begin_render_pass_event(device_impl, cmd_impl, pRenderPassBegin);
#endif

	GET_DISPATCH_PTR_FROM(CmdBeginRenderPass2, device_impl);
	trampoline(commandBuffer, pRenderPassBegin, pSubpassBeginInfo);
}
void VKAPI_CALL vkCmdNextSubpass2(VkCommandBuffer commandBuffer, const VkSubpassBeginInfo *pSubpassBeginInfo, const VkSubpassEndInfo *pSubpassEndInfo)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(commandBuffer));

#if RESHADE_ADDON
	const auto cmd_impl = device_impl->get_private_data_for_object<VK_OBJECT_TYPE_COMMAND_BUFFER>(commandBuffer);

	assert(cmd_impl->current_render_pass != VK_NULL_HANDLE);

	reshade::invoke_addon_event<reshade::addon_event::end_render_pass>(cmd_impl);

	cmd_impl->current_subpass++;

	invoke_begin_render_pass_event(device_impl, cmd_impl, nullptr);
#endif

	GET_DISPATCH_PTR_FROM(CmdNextSubpass2, device_impl);
	trampoline(commandBuffer, pSubpassBeginInfo, pSubpassEndInfo);
}
void VKAPI_CALL vkCmdEndRenderPass2(VkCommandBuffer commandBuffer, const VkSubpassEndInfo *pSubpassEndInfo)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(commandBuffer));

#if RESHADE_ADDON
	const auto cmd_impl = device_impl->get_private_data_for_object<VK_OBJECT_TYPE_COMMAND_BUFFER>(commandBuffer);

	assert(cmd_impl->current_render_pass != VK_NULL_HANDLE);

	reshade::invoke_addon_event<reshade::addon_event::end_render_pass>(cmd_impl);

	cmd_impl->current_subpass = std::numeric_limits<uint32_t>::max();
	cmd_impl->current_render_pass = VK_NULL_HANDLE;
	cmd_impl->current_framebuffer = VK_NULL_HANDLE;
#endif

	GET_DISPATCH_PTR_FROM(CmdEndRenderPass2, device_impl);
	trampoline(commandBuffer, pSubpassEndInfo);
}

void VKAPI_CALL vkCmdPipelineBarrier2(VkCommandBuffer commandBuffer, const VkDependencyInfo *pDependencyInfo)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(commandBuffer));

	GET_DISPATCH_PTR_FROM(CmdPipelineBarrier2, device_impl);
	trampoline(commandBuffer, pDependencyInfo);

#if RESHADE_ADDON
	assert(pDependencyInfo != nullptr);

	const uint32_t num_barriers = pDependencyInfo->bufferMemoryBarrierCount + pDependencyInfo->imageMemoryBarrierCount;

	if (num_barriers == 0 || !reshade::has_addon_event<reshade::addon_event::barrier>())
		return;

	reshade::vulkan::command_list_impl *const cmd_impl = device_impl->get_private_data_for_object<VK_OBJECT_TYPE_COMMAND_BUFFER>(commandBuffer);

	temp_mem<reshade::api::resource> resources(num_barriers);
	temp_mem<reshade::api::resource_usage> old_state(num_barriers), new_state(num_barriers);

	for (uint32_t i = 0; i < pDependencyInfo->bufferMemoryBarrierCount; ++i)
	{
		const VkBufferMemoryBarrier2 &barrier = pDependencyInfo->pBufferMemoryBarriers[i];

		resources[i] = { (uint64_t)barrier.buffer };
		old_state[i] = reshade::vulkan::convert_access_to_usage(static_cast<VkAccessFlags>(barrier.srcAccessMask & 0xFFFFFFFF));
		new_state[i] = reshade::vulkan::convert_access_to_usage(static_cast<VkAccessFlags>(barrier.dstAccessMask & 0xFFFFFFFF));
	}
	for (uint32_t i = 0, k = pDependencyInfo->bufferMemoryBarrierCount; i < pDependencyInfo->imageMemoryBarrierCount; ++i, ++k)
	{
		const VkImageMemoryBarrier2 &barrier = pDependencyInfo->pImageMemoryBarriers[i];

		resources[k] = { (uint64_t)barrier.image };
		old_state[k] = reshade::vulkan::convert_image_layout_to_usage(barrier.oldLayout);
		new_state[k] = reshade::vulkan::convert_image_layout_to_usage(barrier.newLayout);
	}

	reshade::invoke_addon_event<reshade::addon_event::barrier>(cmd_impl, num_barriers, resources.p, old_state.p, new_state.p);
#endif
}

void VKAPI_CALL vkCmdWriteTimestamp2(VkCommandBuffer commandBuffer, VkPipelineStageFlags2 stage, VkQueryPool queryPool, uint32_t query)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(commandBuffer));

#if RESHADE_ADDON && !RESHADE_ADDON_LITE
	reshade::vulkan::command_list_impl *const cmd_impl = device_impl->get_private_data_for_object<VK_OBJECT_TYPE_COMMAND_BUFFER>(commandBuffer);
	assert(device_impl->get_private_data_for_object<VK_OBJECT_TYPE_QUERY_POOL>(queryPool)->type == VK_QUERY_TYPE_TIMESTAMP);

	if (reshade::invoke_addon_event<reshade::addon_event::end_query>(cmd_impl, reshade::api::query_pool { (uint64_t)queryPool }, reshade::api::query_type::timestamp, query))
		return;
#endif

	GET_DISPATCH_PTR_FROM(CmdWriteTimestamp2, device_impl);
	trampoline(commandBuffer, stage, queryPool, query);
}

void VKAPI_CALL vkCmdCopyBuffer2(VkCommandBuffer commandBuffer, const VkCopyBufferInfo2KHR *pCopyBufferInfo)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(commandBuffer));

#if RESHADE_ADDON && !RESHADE_ADDON_LITE
	if (reshade::has_addon_event<reshade::addon_event::copy_buffer_region>())
	{
		reshade::vulkan::command_list_impl *const cmd_impl = device_impl->get_private_data_for_object<VK_OBJECT_TYPE_COMMAND_BUFFER>(commandBuffer);

		for (uint32_t i = 0; i < pCopyBufferInfo->regionCount; ++i)
		{
			const VkBufferCopy2KHR &region = pCopyBufferInfo->pRegions[i];

			if (reshade::invoke_addon_event<reshade::addon_event::copy_buffer_region>(
					cmd_impl,
					reshade::api::resource { (uint64_t)pCopyBufferInfo->srcBuffer }, region.srcOffset,
					reshade::api::resource { (uint64_t)pCopyBufferInfo->dstBuffer }, region.dstOffset, region.size))
				return; // TODO: This skips copy of all regions, rather than just the one specified to this event call
		}
	}
#endif

	GET_DISPATCH_PTR_FROM(CmdCopyBuffer2, device_impl);
	trampoline(commandBuffer, pCopyBufferInfo);
}
void VKAPI_CALL vkCmdCopyImage2(VkCommandBuffer commandBuffer, const VkCopyImageInfo2KHR *pCopyImageInfo)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(commandBuffer));

#if RESHADE_ADDON && !RESHADE_ADDON_LITE
	if (reshade::has_addon_event<reshade::addon_event::copy_texture_region>())
	{
		reshade::vulkan::command_list_impl *const cmd_impl = device_impl->get_private_data_for_object<VK_OBJECT_TYPE_COMMAND_BUFFER>(commandBuffer);

		for (uint32_t i = 0; i < pCopyImageInfo->regionCount; ++i)
		{
			const VkImageCopy2KHR &region = pCopyImageInfo->pRegions[i];

			const reshade::api::subresource_box src_box = {
				region.srcOffset.x,
				region.srcOffset.y,
				region.srcOffset.z,
				region.srcOffset.x + static_cast<int32_t>(region.extent.width),
				region.srcOffset.y + static_cast<int32_t>(region.extent.height),
				region.srcOffset.z + static_cast<int32_t>(region.extent.depth)
			};
			const reshade::api::subresource_box dst_box = {
				region.dstOffset.x,
				region.dstOffset.y,
				region.dstOffset.z,
				region.dstOffset.x + static_cast<int32_t>(region.extent.width),
				region.dstOffset.y + static_cast<int32_t>(region.extent.height),
				region.dstOffset.z + static_cast<int32_t>(region.extent.depth)
			};

			for (uint32_t layer = 0; layer < region.srcSubresource.layerCount; ++layer)
			{
				if (reshade::invoke_addon_event<reshade::addon_event::copy_texture_region>(
						cmd_impl,
						reshade::api::resource { (uint64_t)pCopyImageInfo->srcImage }, calc_subresource_index(device_impl, pCopyImageInfo->srcImage, region.srcSubresource, layer), &src_box,
						reshade::api::resource { (uint64_t)pCopyImageInfo->dstImage }, calc_subresource_index(device_impl, pCopyImageInfo->dstImage, region.dstSubresource, layer), &dst_box,
						reshade::api::filter_mode::min_mag_mip_point))
					return; // TODO: This skips copy of all regions, rather than just the one specified to this event call
			}
		}
	}
#endif

	GET_DISPATCH_PTR_FROM(CmdCopyImage2, device_impl);
	trampoline(commandBuffer, pCopyImageInfo);
}
void VKAPI_CALL vkCmdCopyBufferToImage2(VkCommandBuffer commandBuffer, const VkCopyBufferToImageInfo2KHR *pCopyBufferToImageInfo)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(commandBuffer));

#if RESHADE_ADDON && !RESHADE_ADDON_LITE
	if (reshade::has_addon_event<reshade::addon_event::copy_buffer_to_texture>())
	{
		reshade::vulkan::command_list_impl *const cmd_impl = device_impl->get_private_data_for_object<VK_OBJECT_TYPE_COMMAND_BUFFER>(commandBuffer);

		for (uint32_t i = 0; i < pCopyBufferToImageInfo->regionCount; ++i)
		{
			const VkBufferImageCopy2KHR &region = pCopyBufferToImageInfo->pRegions[i];

			const reshade::api::subresource_box dst_box = {
				region.imageOffset.x,
				region.imageOffset.y,
				region.imageOffset.z,
				region.imageOffset.x + static_cast<int32_t>(region.imageExtent.width),
				region.imageOffset.y + static_cast<int32_t>(region.imageExtent.height),
				region.imageOffset.z + static_cast<int32_t>(region.imageExtent.depth)
			};

			for (uint32_t layer = 0; layer < region.imageSubresource.layerCount; ++layer)
			{
				// TODO: Calculate correct buffer offset for layers following the first
				if (reshade::invoke_addon_event<reshade::addon_event::copy_buffer_to_texture>(
						cmd_impl,
						reshade::api::resource { (uint64_t)pCopyBufferToImageInfo->srcBuffer }, region.bufferOffset, region.bufferRowLength, region.bufferImageHeight,
						reshade::api::resource { (uint64_t)pCopyBufferToImageInfo->dstImage }, calc_subresource_index(device_impl, pCopyBufferToImageInfo->dstImage, region.imageSubresource, layer), &dst_box))
					return; // TODO: This skips copy of all regions, rather than just the one specified to this event call
			}
		}
	}
#endif

	GET_DISPATCH_PTR_FROM(CmdCopyBufferToImage2, device_impl);
	trampoline(commandBuffer, pCopyBufferToImageInfo);
}
void VKAPI_CALL vkCmdCopyImageToBuffer2(VkCommandBuffer commandBuffer, const VkCopyImageToBufferInfo2KHR *pCopyImageToBufferInfo)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(commandBuffer));

#if RESHADE_ADDON && !RESHADE_ADDON_LITE
	if (reshade::has_addon_event<reshade::addon_event::copy_texture_to_buffer>())
	{
		reshade::vulkan::command_list_impl *const cmd_impl = device_impl->get_private_data_for_object<VK_OBJECT_TYPE_COMMAND_BUFFER>(commandBuffer);

		for (uint32_t i = 0; i < pCopyImageToBufferInfo->regionCount; ++i)
		{
			const VkBufferImageCopy2KHR &region = pCopyImageToBufferInfo->pRegions[i];

			const reshade::api::subresource_box src_box = {
				region.imageOffset.x,
				region.imageOffset.y,
				region.imageOffset.z,
				region.imageOffset.x + static_cast<int32_t>(region.imageExtent.width),
				region.imageOffset.y + static_cast<int32_t>(region.imageExtent.height),
				region.imageOffset.z + static_cast<int32_t>(region.imageExtent.depth)
			};

			for (uint32_t layer = 0; layer < region.imageSubresource.layerCount; ++layer)
			{
				// TODO: Calculate correct buffer offset for layers following the first
				if (reshade::invoke_addon_event<reshade::addon_event::copy_texture_to_buffer>(
						cmd_impl,
						reshade::api::resource { (uint64_t)pCopyImageToBufferInfo->srcImage }, calc_subresource_index(device_impl, pCopyImageToBufferInfo->srcImage, region.imageSubresource, layer), &src_box,
						reshade::api::resource { (uint64_t)pCopyImageToBufferInfo->dstBuffer }, region.bufferOffset, region.bufferRowLength, region.bufferImageHeight))
					return; // TODO: This skips copy of all regions, rather than just the one specified to this event call
			}
		}
	}
#endif

	GET_DISPATCH_PTR_FROM(CmdCopyImageToBuffer2, device_impl);
	trampoline(commandBuffer, pCopyImageToBufferInfo);
}
void VKAPI_CALL vkCmdBlitImage2(VkCommandBuffer commandBuffer, const VkBlitImageInfo2KHR *pBlitImageInfo)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(commandBuffer));

#if RESHADE_ADDON && !RESHADE_ADDON_LITE
	if (reshade::has_addon_event<reshade::addon_event::copy_texture_region>())
	{
		reshade::vulkan::command_list_impl *const cmd_impl = device_impl->get_private_data_for_object<VK_OBJECT_TYPE_COMMAND_BUFFER>(commandBuffer);

		for (uint32_t i = 0; i < pBlitImageInfo->regionCount; ++i)
		{
			const VkImageBlit2KHR &region = pBlitImageInfo->pRegions[i];

			for (uint32_t layer = 0; layer < region.srcSubresource.layerCount; ++layer)
			{
				static_assert(sizeof(region.srcOffsets) == sizeof(reshade::api::subresource_box) && sizeof(region.dstOffsets) == sizeof(reshade::api::subresource_box));

				if (reshade::invoke_addon_event<reshade::addon_event::copy_texture_region>(
						cmd_impl,
						reshade::api::resource { (uint64_t)pBlitImageInfo->srcImage }, calc_subresource_index(device_impl, pBlitImageInfo->srcImage, region.srcSubresource, layer), reinterpret_cast<const reshade::api::subresource_box *>(&region.srcOffsets[0].x),
						reshade::api::resource { (uint64_t)pBlitImageInfo->dstImage }, calc_subresource_index(device_impl, pBlitImageInfo->dstImage, region.dstSubresource, layer), reinterpret_cast<const reshade::api::subresource_box *>(&region.dstOffsets[0].x),
						pBlitImageInfo->filter == VK_FILTER_NEAREST ? reshade::api::filter_mode::min_mag_mip_point : reshade::api::filter_mode::min_mag_mip_linear))
					return; // TODO: This skips copy of all regions, rather than just the one specified to this event call
			}
		}
	}
#endif

	GET_DISPATCH_PTR_FROM(CmdBlitImage2, device_impl);
	trampoline(commandBuffer, pBlitImageInfo);
}
void VKAPI_CALL vkCmdResolveImage2(VkCommandBuffer commandBuffer, const VkResolveImageInfo2KHR *pResolveImageInfo)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(commandBuffer));

#if RESHADE_ADDON && !RESHADE_ADDON_LITE
	if (reshade::has_addon_event<reshade::addon_event::resolve_texture_region>())
	{
		reshade::vulkan::command_list_impl *const cmd_impl = device_impl->get_private_data_for_object<VK_OBJECT_TYPE_COMMAND_BUFFER>(commandBuffer);

		for (uint32_t i = 0; i < pResolveImageInfo->regionCount; ++i)
		{
			const VkImageResolve2KHR &region = pResolveImageInfo->pRegions[i];

			const reshade::api::subresource_box src_box = {
				region.srcOffset.x,
				region.srcOffset.y,
				region.srcOffset.z,
				region.srcOffset.x + static_cast<int32_t>(region.extent.width),
				region.srcOffset.y + static_cast<int32_t>(region.extent.height),
				region.srcOffset.z + static_cast<int32_t>(region.extent.depth)
			};

			for (uint32_t layer = 0; layer < region.srcSubresource.layerCount; ++layer)
			{
				if (reshade::invoke_addon_event<reshade::addon_event::resolve_texture_region>(
						cmd_impl,
						reshade::api::resource { (uint64_t)pResolveImageInfo->srcImage }, calc_subresource_index(device_impl, pResolveImageInfo->srcImage, region.srcSubresource, layer), &src_box,
						reshade::api::resource { (uint64_t)pResolveImageInfo->dstImage }, calc_subresource_index(device_impl, pResolveImageInfo->dstImage, region.dstSubresource, layer), region.dstOffset.x, region.dstOffset.y, region.dstOffset.z, reshade::api::format::unknown))
					return; // TODO: This skips resolve of all regions, rather than just the one specified to this event call
			}
		}
	}
#endif

	GET_DISPATCH_PTR_FROM(CmdResolveImage2, device_impl);
	trampoline(commandBuffer, pResolveImageInfo);
}

void VKAPI_CALL vkCmdBeginRendering(VkCommandBuffer commandBuffer, const VkRenderingInfo *pRenderingInfo)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(commandBuffer));

#if RESHADE_ADDON
	const auto cmd_impl = device_impl->get_private_data_for_object<VK_OBJECT_TYPE_COMMAND_BUFFER>(commandBuffer);

	invoke_begin_render_pass_event(cmd_impl, pRenderingInfo);
#endif

	GET_DISPATCH_PTR_FROM(CmdBeginRendering, device_impl);
	trampoline(commandBuffer, pRenderingInfo);
}
void VKAPI_CALL vkCmdEndRendering(VkCommandBuffer commandBuffer)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(commandBuffer));

#if RESHADE_ADDON
	const auto cmd_impl = device_impl->get_private_data_for_object<VK_OBJECT_TYPE_COMMAND_BUFFER>(commandBuffer);

	reshade::invoke_addon_event<reshade::addon_event::end_render_pass>(cmd_impl);
#endif

	GET_DISPATCH_PTR_FROM(CmdEndRendering, device_impl);
	trampoline(commandBuffer);
}

void VKAPI_CALL vkCmdPushDescriptorSetKHR(VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint, VkPipelineLayout layout, uint32_t set, uint32_t descriptorWriteCount, const VkWriteDescriptorSet *pDescriptorWrites)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(commandBuffer));

	GET_DISPATCH_PTR_FROM(CmdPushDescriptorSetKHR, device_impl);
	trampoline(commandBuffer, pipelineBindPoint, layout, set, descriptorWriteCount, pDescriptorWrites);

#if RESHADE_ADDON && !RESHADE_ADDON_LITE
	if (!reshade::has_addon_event<reshade::addon_event::push_descriptors>())
		return;

	reshade::vulkan::command_list_impl *const cmd_impl = device_impl->get_private_data_for_object<VK_OBJECT_TYPE_COMMAND_BUFFER>(commandBuffer);

	uint32_t max_descriptors = 0;
	for (uint32_t i = 0; i < descriptorWriteCount; ++i)
		max_descriptors = std::max(max_descriptors, pDescriptorWrites[i].descriptorCount);
	temp_mem<uint64_t> descriptors(max_descriptors * 2);

	const auto shader_stages =
		pipelineBindPoint == VK_PIPELINE_BIND_POINT_COMPUTE ? reshade::api::shader_stage::all_compute :
		pipelineBindPoint == VK_PIPELINE_BIND_POINT_GRAPHICS ? reshade::api::shader_stage::all_graphics :
		static_cast<reshade::api::shader_stage>(0); // Unknown pipeline bind point

	for (uint32_t i = 0, j = 0; i < descriptorWriteCount; ++i, j = 0)
	{
		const VkWriteDescriptorSet &write = pDescriptorWrites[i];

		reshade::api::descriptor_set_update update;
		update.set = { 0 };
		update.binding = write.dstBinding;
		update.array_offset = write.dstArrayElement;
		update.count = write.descriptorCount;
		update.type = reshade::vulkan::convert_descriptor_type(write.descriptorType);
		update.descriptors = descriptors.p;

		switch (write.descriptorType)
		{
		case VK_DESCRIPTOR_TYPE_SAMPLER:
			for (uint32_t k = 0; k < write.descriptorCount; ++k, ++j)
				descriptors[j + 0] = (uint64_t)write.pImageInfo[k].sampler;
			break;
		case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
			for (uint32_t k = 0; k < write.descriptorCount; ++k, j += 2)
				descriptors[j + 0] = (uint64_t)write.pImageInfo[k].sampler,
				descriptors[j + 1] = (uint64_t)write.pImageInfo[k].imageView;
			break;
		case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
		case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
			for (uint32_t k = 0; k < write.descriptorCount; ++k, ++j)
				descriptors[j + 0] = (uint64_t)write.pImageInfo[k].imageView;
			break;
		case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
		case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
			static_assert(sizeof(reshade::api::buffer_range) == sizeof(VkDescriptorBufferInfo));
			update.descriptors = write.pBufferInfo;
			break;
		}

		reshade::invoke_addon_event<reshade::addon_event::push_descriptors>(
			cmd_impl,
			shader_stages,
			reshade::api::pipeline_layout { (uint64_t)layout },
			set,
			update);
	}
#endif
}

void VKAPI_CALL vkCmdBindTransformFeedbackBuffersEXT(VkCommandBuffer commandBuffer, uint32_t firstBinding, uint32_t bindingCount, const VkBuffer *pBuffers, const VkDeviceSize *pOffsets, const VkDeviceSize *pSizes)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(commandBuffer));

	GET_DISPATCH_PTR_FROM(CmdBindTransformFeedbackBuffersEXT, device_impl);
	trampoline(commandBuffer, firstBinding, bindingCount, pBuffers, pOffsets, pSizes);

#if RESHADE_ADDON && !RESHADE_ADDON_LITE
	if (!reshade::has_addon_event<reshade::addon_event::bind_stream_output_buffers>())
		return;

	reshade::vulkan::command_list_impl *const cmd_impl = device_impl->get_private_data_for_object<VK_OBJECT_TYPE_COMMAND_BUFFER>(commandBuffer);

	static_assert(sizeof(*pBuffers) == sizeof(reshade::api::resource));

	reshade::invoke_addon_event<reshade::addon_event::bind_stream_output_buffers>(
		cmd_impl, firstBinding, bindingCount, reinterpret_cast<const reshade::api::resource *>(pBuffers), pOffsets, pSizes);
#endif
}

void VKAPI_CALL vkCmdBeginQueryIndexedEXT(VkCommandBuffer commandBuffer, VkQueryPool queryPool, uint32_t query, VkQueryControlFlags flags, uint32_t index)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(commandBuffer));

#if RESHADE_ADDON && !RESHADE_ADDON_LITE
	reshade::vulkan::command_list_impl *const cmd_impl = device_impl->get_private_data_for_object<VK_OBJECT_TYPE_COMMAND_BUFFER>(commandBuffer);
	const auto pool_data = device_impl->get_private_data_for_object<VK_OBJECT_TYPE_QUERY_POOL>(queryPool);

	if (reshade::invoke_addon_event<reshade::addon_event::begin_query>(cmd_impl, reshade::api::query_pool{ (uint64_t)queryPool }, reshade::vulkan::convert_query_type(pool_data->type, index), query))
		return;
#endif

	GET_DISPATCH_PTR_FROM(CmdBeginQueryIndexedEXT, device_impl);
	trampoline(commandBuffer, queryPool, query, flags, index);
}
void VKAPI_CALL vkCmdEndQueryIndexedEXT(VkCommandBuffer commandBuffer, VkQueryPool queryPool, uint32_t query, uint32_t index)
{
	reshade::vulkan::device_impl *const device_impl = g_vulkan_devices.at(dispatch_key_from_handle(commandBuffer));

#if RESHADE_ADDON && !RESHADE_ADDON_LITE
	reshade::vulkan::command_list_impl *const cmd_impl = device_impl->get_private_data_for_object<VK_OBJECT_TYPE_COMMAND_BUFFER>(commandBuffer);
	const auto pool_data = device_impl->get_private_data_for_object<VK_OBJECT_TYPE_QUERY_POOL>(queryPool);

	if (reshade::invoke_addon_event<reshade::addon_event::end_query>(cmd_impl, reshade::api::query_pool{ (uint64_t)queryPool }, reshade::vulkan::convert_query_type(pool_data->type, index), query))
		return;
#endif

	GET_DISPATCH_PTR_FROM(CmdEndQueryIndexedEXT, device_impl);
	trampoline(commandBuffer, queryPool, query, index);
}
