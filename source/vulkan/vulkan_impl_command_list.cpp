/*
 * Copyright (C) 2021 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "vulkan_impl_device.hpp"
#include "vulkan_impl_command_list.hpp"
#include "vulkan_impl_type_convert.hpp"
#include "dll_log.hpp"
#include <algorithm>

#define vk _device_impl->_dispatch_table

static inline void convert_subresource(uint32_t subresource, const VkImageCreateInfo &create_info, VkImageSubresourceLayers &subresource_info)
{
	subresource_info.aspectMask = reshade::vulkan::aspect_flags_from_format(create_info.format);
	subresource_info.mipLevel = subresource % create_info.mipLevels;
	subresource_info.baseArrayLayer = subresource / create_info.mipLevels;
	subresource_info.layerCount = 1;
}

reshade::vulkan::command_list_impl::command_list_impl(device_impl *device, VkCommandBuffer cmd_list) :
	api_object_impl(cmd_list),
	_device_impl(device)
{
#if RESHADE_ADDON
	if (_orig != VK_NULL_HANDLE) // Do not call add-on event for immediate command list (since it is internal and not used by the application)
		invoke_addon_event<addon_event::init_command_list>(this);
#endif
}
reshade::vulkan::command_list_impl::~command_list_impl()
{
#if RESHADE_ADDON
	if (_orig != VK_NULL_HANDLE)
		invoke_addon_event<addon_event::destroy_command_list>(this);
#endif
}

reshade::api::device *reshade::vulkan::command_list_impl::get_device()
{
	return _device_impl;
}

void reshade::vulkan::command_list_impl::barrier(uint32_t count, const api::resource *resources, const api::resource_usage *old_states, const api::resource_usage *new_states)
{
	if (count == 0)
		return;

	_has_commands = true;

	uint32_t num_mem_barriers = 0;
	temp_mem<VkMemoryBarrier> mem_barriers(count);
	uint32_t num_image_barriers = 0;
	temp_mem<VkImageMemoryBarrier> image_barriers(count);
	uint32_t num_buffer_barriers = 0;
	temp_mem<VkBufferMemoryBarrier> buffer_barriers(count);

	VkPipelineStageFlags src_stage_mask = 0;
	VkPipelineStageFlags dst_stage_mask = 0;

	for (uint32_t i = 0; i < count; ++i)
	{
		if (resources[i].handle == 0)
		{
			VkMemoryBarrier &barrier = mem_barriers[num_mem_barriers++];
			barrier = { VK_STRUCTURE_TYPE_MEMORY_BARRIER };
			barrier.srcAccessMask = convert_usage_to_access(old_states[i]);
			barrier.dstAccessMask = convert_usage_to_access(new_states[i]);

			src_stage_mask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
			dst_stage_mask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
			continue;
		}

		const auto data = _device_impl->get_private_data_for_object<VK_OBJECT_TYPE_IMAGE>((VkImage)resources[i].handle);
		if (data->create_info.sType == VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO)
		{
			VkImageMemoryBarrier &barrier = image_barriers[num_image_barriers++];
			barrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
			barrier.srcAccessMask = convert_usage_to_access(old_states[i]);
			barrier.dstAccessMask = convert_usage_to_access(new_states[i]);
			barrier.oldLayout = convert_usage_to_image_layout(old_states[i]);
			barrier.newLayout = convert_usage_to_image_layout(new_states[i]);
			barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.image = (VkImage)resources[i].handle;
			barrier.subresourceRange = { aspect_flags_from_format(data->create_info.format), 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS };
		}
		else
		{
			VkBufferMemoryBarrier &barrier = buffer_barriers[num_buffer_barriers++];
			barrier = { VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER };
			barrier.srcAccessMask = convert_usage_to_access(old_states[i]);
			barrier.dstAccessMask = convert_usage_to_access(new_states[i]);
			barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.buffer = (VkBuffer)resources[i].handle;
			barrier.offset = 0;
			barrier.size = VK_WHOLE_SIZE;
		}

		src_stage_mask |= convert_usage_to_pipeline_stage(old_states[i], true, _device_impl->_enabled_features);
		dst_stage_mask |= convert_usage_to_pipeline_stage(new_states[i], false, _device_impl->_enabled_features);
	}

	assert(src_stage_mask != 0 && dst_stage_mask != 0);

	vk.CmdPipelineBarrier(_orig, src_stage_mask, dst_stage_mask, 0, num_mem_barriers, mem_barriers.p, num_buffer_barriers, buffer_barriers.p, num_image_barriers, image_barriers.p);
}

void reshade::vulkan::command_list_impl::begin_render_pass(uint32_t count, const api::render_pass_render_target_desc *rts, const api::render_pass_depth_stencil_desc *ds)
{
	_has_commands = true;

	if (_device_impl->_dynamic_rendering_ext)
	{
		VkRenderingInfo rendering_info { VK_STRUCTURE_TYPE_RENDERING_INFO };
		rendering_info.renderArea.extent.width = std::numeric_limits<uint32_t>::max();
		rendering_info.renderArea.extent.height = std::numeric_limits<uint32_t>::max();
		rendering_info.layerCount = std::numeric_limits<uint32_t>::max();

		temp_mem<VkRenderingAttachmentInfo, 8> color_attachments(count);
		for (uint32_t i = 0; i < count; ++i)
		{
			color_attachments[i] = { VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
			color_attachments[i].imageView = (VkImageView)rts[i].view.handle;
			color_attachments[i].imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			color_attachments[i].loadOp = convert_render_pass_load_op(rts[i].load_op);
			color_attachments[i].storeOp = convert_render_pass_store_op(rts[i].store_op);
			std::copy_n(rts[i].clear_color, 4, color_attachments[i].clearValue.color.float32);

			const auto view_data = _device_impl->get_private_data_for_object<VK_OBJECT_TYPE_IMAGE_VIEW>(color_attachments[i].imageView);
			const auto image_data = _device_impl->get_private_data_for_object<VK_OBJECT_TYPE_IMAGE>(view_data->create_info.image);

			rendering_info.renderArea.extent.width = std::min(rendering_info.renderArea.extent.width, image_data->create_info.extent.width);
			rendering_info.renderArea.extent.height = std::min(rendering_info.renderArea.extent.height, image_data->create_info.extent.height);
			rendering_info.layerCount = std::min(rendering_info.layerCount, image_data->create_info.arrayLayers);
		}

		rendering_info.colorAttachmentCount = count;
		rendering_info.pColorAttachments = color_attachments.p;

		VkRenderingAttachmentInfo depth_attachment, stencil_attachment;
		if (ds != nullptr && ds->view.handle != 0)
		{
			const auto view_data = _device_impl->get_private_data_for_object<VK_OBJECT_TYPE_IMAGE_VIEW>((VkImageView)ds->view.handle);
			const auto image_data = _device_impl->get_private_data_for_object<VK_OBJECT_TYPE_IMAGE>(view_data->create_info.image);

			if (aspect_flags_from_format(image_data->create_info.format) & VK_IMAGE_ASPECT_DEPTH_BIT)
			{
				depth_attachment = { VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
				depth_attachment.imageView = (VkImageView)ds->view.handle;
				depth_attachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
				depth_attachment.loadOp = convert_render_pass_load_op(ds->depth_load_op);
				depth_attachment.storeOp = convert_render_pass_store_op(ds->depth_store_op);
				depth_attachment.clearValue.depthStencil.depth = ds->clear_depth;

				rendering_info.pDepthAttachment = &depth_attachment;
			}

			if (aspect_flags_from_format(image_data->create_info.format) & VK_IMAGE_ASPECT_STENCIL_BIT)
			{
				stencil_attachment = { VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
				stencil_attachment.imageView = (VkImageView)ds->view.handle;
				stencil_attachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
				stencil_attachment.loadOp = convert_render_pass_load_op(ds->stencil_load_op);
				stencil_attachment.storeOp = convert_render_pass_store_op(ds->stencil_store_op);
				stencil_attachment.clearValue.depthStencil.stencil = ds->clear_stencil;

				rendering_info.pStencilAttachment = &stencil_attachment;
			}

			rendering_info.renderArea.extent.width = std::min(rendering_info.renderArea.extent.width, image_data->create_info.extent.width);
			rendering_info.renderArea.extent.height = std::min(rendering_info.renderArea.extent.height, image_data->create_info.extent.height);
			rendering_info.layerCount = std::min(rendering_info.layerCount, image_data->create_info.arrayLayers);
		}

		vk.CmdBeginRendering(_orig, &rendering_info);
	}
	else
	{
		size_t hash = 0;
		for (uint32_t i = 0; i < count; ++i)
		{
			hash_combine(hash, rts[i].view.handle);
			hash_combine(hash, rts[i].load_op);
			hash_combine(hash, rts[i].store_op);
			hash_combine(hash, rts[i].clear_color[0]);
			hash_combine(hash, rts[i].clear_color[1]);
			hash_combine(hash, rts[i].clear_color[2]);
			hash_combine(hash, rts[i].clear_color[3]);
		}
		if (ds != nullptr)
		{
			hash_combine(hash, ds->view.handle);
			hash_combine(hash, ds->depth_load_op);
			hash_combine(hash, ds->depth_store_op);
			hash_combine(hash, ds->stencil_load_op);
			hash_combine(hash, ds->stencil_store_op);
			hash_combine(hash, ds->clear_depth);
			hash_combine(hash, ds->clear_stencil);
		}

		const uint32_t max_attachments = count + 1;

		VkRenderPassBeginInfo begin_info;

		std::unique_lock<std::shared_mutex> lock(_device_impl->_mutex);

		if (const auto it = _device_impl->_render_pass_lookup.find(hash);
			it != _device_impl->_render_pass_lookup.end())
		{
			begin_info = it->second;
		}
		else
		{
			lock.unlock();

			temp_mem<VkImageView, 9> attach_views(max_attachments);
			temp_mem<VkAttachmentReference, 9> attach_refs(max_attachments);
			temp_mem<VkAttachmentDescription, 9> attach_descs(max_attachments);

			// Synchronize any writes to render targets in previous passes with reads from them in this pass
			VkSubpassDependency subdep = {};
			subdep.srcSubpass = VK_SUBPASS_EXTERNAL;
			subdep.dstSubpass = 0;
			subdep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			subdep.dstStageMask = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;
			subdep.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			subdep.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

			VkSubpassDescription subpass = {};
			subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
			subpass.colorAttachmentCount = count;
			subpass.pColorAttachments = attach_refs.p;
			subpass.pDepthStencilAttachment = (ds != nullptr && ds->view.handle != 0) ? &attach_refs[count] : nullptr;

			VkRenderPassCreateInfo render_pass_create_info { VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
			render_pass_create_info.attachmentCount = subpass.colorAttachmentCount + (subpass.pDepthStencilAttachment != nullptr ? 1 : 0);
			render_pass_create_info.pAttachments = attach_descs.p;
			render_pass_create_info.subpassCount = 1;
			render_pass_create_info.pSubpasses = &subpass;
			render_pass_create_info.dependencyCount = 1;
			render_pass_create_info.pDependencies = &subdep;

			VkFramebufferCreateInfo framebuffer_create_info { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
			framebuffer_create_info.width = framebuffer_create_info.height = framebuffer_create_info.layers = std::numeric_limits<uint32_t>::max();
			framebuffer_create_info.attachmentCount = render_pass_create_info.attachmentCount;
			framebuffer_create_info.pAttachments = attach_views.p;

			for (uint32_t i = 0; i < subpass.colorAttachmentCount; ++i)
			{
				attach_views[i] = (VkImageView)rts[i].view.handle;

				const auto view_data = _device_impl->get_private_data_for_object<VK_OBJECT_TYPE_IMAGE_VIEW>(attach_views[i]);
				const auto image_data = _device_impl->get_private_data_for_object<VK_OBJECT_TYPE_IMAGE>(view_data->create_info.image);

				VkAttachmentReference &attach_ref = attach_refs[i];
				attach_ref.attachment = (rts[i].load_op == api::render_pass_load_op::discard && rts[i].store_op == api::render_pass_store_op::discard) ? VK_ATTACHMENT_UNUSED : i;
				attach_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

				VkAttachmentDescription &attach_desc = attach_descs[i];
				attach_desc.flags = 0;
				attach_desc.format = view_data->create_info.format;
				attach_desc.samples = image_data->create_info.samples;
				attach_desc.loadOp = convert_render_pass_load_op(rts[i].load_op);
				attach_desc.storeOp = convert_render_pass_store_op(rts[i].store_op);
				attach_desc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
				attach_desc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
				attach_desc.initialLayout = attach_ref.layout;
				attach_desc.finalLayout = attach_ref.layout;

				framebuffer_create_info.width  = std::min(framebuffer_create_info.width,  image_data->create_info.extent.width);
				framebuffer_create_info.height = std::min(framebuffer_create_info.height, image_data->create_info.extent.height);
				framebuffer_create_info.layers = std::min(framebuffer_create_info.layers, view_data->create_info.subresourceRange.layerCount);
			}

			if (subpass.pDepthStencilAttachment != nullptr)
			{
				attach_views[count] = (VkImageView)ds->view.handle;

				const auto view_data = _device_impl->get_private_data_for_object<VK_OBJECT_TYPE_IMAGE_VIEW>(attach_views[count]);
				const auto image_data = _device_impl->get_private_data_for_object<VK_OBJECT_TYPE_IMAGE>(view_data->create_info.image);

				VkAttachmentReference &attach_ref = attach_refs[count];
				attach_ref.attachment =
					(ds->depth_load_op == api::render_pass_load_op::discard && ds->depth_store_op == api::render_pass_store_op::discard) &&
					(ds->stencil_load_op == api::render_pass_load_op::discard && ds->stencil_store_op == api::render_pass_store_op::discard) ? VK_ATTACHMENT_UNUSED : count;
				attach_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

				VkAttachmentDescription &attach_desc = attach_descs[count];
				attach_desc.flags = 0;
				attach_desc.format = view_data->create_info.format;
				attach_desc.samples = image_data->create_info.samples;
				attach_desc.loadOp = convert_render_pass_load_op(ds->depth_load_op);
				attach_desc.storeOp = convert_render_pass_store_op(ds->depth_store_op);
				attach_desc.stencilLoadOp = convert_render_pass_load_op(ds->stencil_load_op);
				attach_desc.stencilStoreOp = convert_render_pass_store_op(ds->stencil_store_op);
				attach_desc.initialLayout = attach_ref.layout;
				attach_desc.finalLayout = attach_ref.layout;

				framebuffer_create_info.width = std::min(framebuffer_create_info.width, image_data->create_info.extent.width);
				framebuffer_create_info.height = std::min(framebuffer_create_info.height, image_data->create_info.extent.height);
				framebuffer_create_info.layers = std::min(framebuffer_create_info.layers, view_data->create_info.subresourceRange.layerCount);
			}

			begin_info = { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };

			if (vk.CreateRenderPass(_device_impl->_orig, &render_pass_create_info, nullptr, &begin_info.renderPass) != VK_SUCCESS)
				return;

			framebuffer_create_info.renderPass = begin_info.renderPass;

			if (vk.CreateFramebuffer(_device_impl->_orig, &framebuffer_create_info, nullptr, &begin_info.framebuffer) != VK_SUCCESS)
			{
				vk.DestroyRenderPass(_device_impl->_orig, begin_info.renderPass, nullptr);
				return;
			}

			begin_info.renderArea.extent.width = framebuffer_create_info.width;
			begin_info.renderArea.extent.height = framebuffer_create_info.height;

			lock.lock();

			_device_impl->_render_pass_lookup.emplace(hash, begin_info);
		}

		lock.unlock();

		temp_mem<VkClearValue, 9> clear_values(max_attachments);
		for (uint32_t i = 0; i < count; ++i)
		{
			std::copy_n(rts[i].clear_color, 4, clear_values[i].color.float32);
		}
		if (ds != nullptr)
		{
			clear_values[count].depthStencil.depth = ds->clear_depth;
			clear_values[count].depthStencil.stencil = ds->clear_stencil;
		}

		begin_info.clearValueCount = count + (ds != nullptr ? 1 : 0);
		begin_info.pClearValues = clear_values.p;

		vk.CmdBeginRenderPass(_orig, &begin_info, VK_SUBPASS_CONTENTS_INLINE);
	}
}
void reshade::vulkan::command_list_impl::end_render_pass()
{
	assert(_has_commands);

	if (_device_impl->_dynamic_rendering_ext)
	{
		vk.CmdEndRendering(_orig);
	}
	else
	{
		vk.CmdEndRenderPass(_orig);
	}
}
void reshade::vulkan::command_list_impl::bind_render_targets_and_depth_stencil(uint32_t, const api::resource_view *, api::resource_view)
{
	assert(false);
}

void reshade::vulkan::command_list_impl::bind_pipeline(api::pipeline_stage stages, api::pipeline pipeline)
{
	assert(pipeline.handle != 0);
	// Cannot bind state to individual pipeline stages
	assert(stages == api::pipeline_stage::all_compute || stages == api::pipeline_stage::all_graphics);

	vk.CmdBindPipeline(_orig,
		stages == api::pipeline_stage::all_compute ? VK_PIPELINE_BIND_POINT_COMPUTE : VK_PIPELINE_BIND_POINT_GRAPHICS,
		(VkPipeline)pipeline.handle);
}
void reshade::vulkan::command_list_impl::bind_pipeline_states(uint32_t count, const api::dynamic_state *states, const uint32_t *values)
{
	for (uint32_t i = 0; i < count; ++i)
	{
		switch (states[i])
		{
		case api::dynamic_state::blend_constant:
		{
			const float blend_constant[4] = { ((values[i]) & 0xFF) / 255.0f, ((values[i] >> 4) & 0xFF) / 255.0f, ((values[i] >> 8) & 0xFF) / 255.0f, ((values[i] >> 12) & 0xFF) / 255.0f };
			vk.CmdSetBlendConstants(_orig, blend_constant);
			continue;
		}
		case api::dynamic_state::stencil_read_mask:
			vk.CmdSetStencilCompareMask(_orig, VK_STENCIL_FACE_FRONT_AND_BACK, values[i]);
			continue;
		case api::dynamic_state::stencil_write_mask:
			vk.CmdSetStencilWriteMask(_orig, VK_STENCIL_FACE_FRONT_AND_BACK, values[i]);
			continue;
		case api::dynamic_state::stencil_reference_value:
			vk.CmdSetStencilReference(_orig, VK_STENCIL_FACE_FRONT_AND_BACK, values[i]);
			continue;
		}

		if (!_device_impl->_extended_dynamic_state_ext)
		{
			assert(false);
			continue;
		}

		switch (states[i])
		{
		case api::dynamic_state::cull_mode:
			vk.CmdSetCullMode(_orig, convert_cull_mode(static_cast<api::cull_mode>(values[i])));
			continue;
		case api::dynamic_state::front_counter_clockwise:
			vk.CmdSetFrontFace(_orig, values[i] != 0 ? VK_FRONT_FACE_COUNTER_CLOCKWISE : VK_FRONT_FACE_CLOCKWISE);
			continue;
		case api::dynamic_state::primitive_topology:
			vk.CmdSetPrimitiveTopology(_orig, convert_primitive_topology(static_cast<api::primitive_topology>(values[i])));
			continue;
		case api::dynamic_state::depth_enable:
			vk.CmdSetDepthTestEnable(_orig, values[i]);
			continue;
		case api::dynamic_state::depth_write_mask:
			vk.CmdSetDepthWriteEnable(_orig, values[i]);
			continue;
		case api::dynamic_state::depth_func:
			vk.CmdSetDepthCompareOp(_orig, convert_compare_op(static_cast<api::compare_op>(values[i])));
			continue;
		case api::dynamic_state::stencil_enable:
			vk.CmdSetStencilTestEnable(_orig, values[i]);
			continue;
		case api::dynamic_state::logic_op:
			vk.CmdSetLogicOpEXT(_orig, convert_logic_op(static_cast<api::logic_op>(values[i])));
			continue;
		case api::dynamic_state::render_target_write_mask:
			vk.CmdSetColorWriteEnableEXT(_orig, 1, &values[i]);
			continue;
		default:
			assert(false);
			continue;
		}
	}
}
void reshade::vulkan::command_list_impl::bind_viewports(uint32_t first, uint32_t count, const api::viewport *viewports)
{
	temp_mem<VkViewport> viewport_data(count);
	for (uint32_t i = 0; i < count; ++i)
	{
		std::memcpy(&viewport_data[i], &viewports[i], sizeof(VkViewport));

		// Flip viewport vertically
		viewport_data[i].y += viewport_data[i].height;
		viewport_data[i].height = -viewport_data[i].height;
	}

	vk.CmdSetViewport(_orig, first, count, viewport_data.p);
}
void reshade::vulkan::command_list_impl::bind_scissor_rects(uint32_t first, uint32_t count, const api::rect *rects)
{
	temp_mem<VkRect2D> rect_data(count);
	for (uint32_t i = 0; i < count; ++i)
	{
		rect_data[i].offset.x = rects[i].left;
		rect_data[i].offset.y = rects[i].top;
		rect_data[i].extent.width = rects[i].width();
		rect_data[i].extent.height = rects[i].height();
	}

	vk.CmdSetScissor(_orig, first, count, rect_data.p);
}

void reshade::vulkan::command_list_impl::push_constants(api::shader_stage stages, api::pipeline_layout layout, uint32_t, uint32_t first, uint32_t count, const void *values)
{
	assert(count != 0);

	vk.CmdPushConstants(_orig,
		(VkPipelineLayout)layout.handle,
		static_cast<VkShaderStageFlags>(stages),
		first * 4, count * 4, values);
}
void reshade::vulkan::command_list_impl::push_descriptors(api::shader_stage stages, api::pipeline_layout layout, uint32_t layout_param, const api::descriptor_set_update &update)
{
	if (update.count == 0)
		return;

	assert(update.set.handle == 0);

	VkWriteDescriptorSet write { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
	write.dstBinding = update.binding;
	write.dstArrayElement = update.array_offset;
	write.descriptorCount = update.count;
	write.descriptorType = convert_descriptor_type(update.type, true);

	temp_mem<VkDescriptorImageInfo> image_info(update.count);

	switch (update.type)
	{
	case api::descriptor_type::sampler:
		write.pImageInfo = image_info.p;
		for (uint32_t k = 0; k < update.count; ++k)
		{
			const auto &descriptor = static_cast<const api::sampler *>(update.descriptors)[k];
			image_info[k].sampler = (VkSampler)descriptor.handle;
		}
		break;
	case api::descriptor_type::sampler_with_resource_view:
		write.pImageInfo = image_info.p;
		for (uint32_t k = 0; k < update.count; ++k)
		{
			const auto &descriptor = static_cast<const api::sampler_with_resource_view *>(update.descriptors)[k];
			image_info[k].sampler = (VkSampler)descriptor.sampler.handle;
			image_info[k].imageView = (VkImageView)descriptor.view.handle;
			image_info[k].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		}
		break;
	case api::descriptor_type::shader_resource_view:
	case api::descriptor_type::unordered_access_view:
		if (const auto descriptors = static_cast<const api::resource_view *>(update.descriptors);
			_device_impl->get_private_data_for_object<VK_OBJECT_TYPE_IMAGE_VIEW>((VkImageView)descriptors[0].handle)->create_info.sType == VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO)
		{
			write.pImageInfo = image_info.p;
			for (uint32_t k = 0; k < update.count; ++k)
			{
				const auto &descriptor = descriptors[k];
				image_info[k].imageView = (VkImageView)descriptor.handle;
				image_info[k].imageLayout = (update.type == api::descriptor_type::unordered_access_view) ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			}
		}
		else
		{
			write.descriptorType = convert_descriptor_type(update.type, false);

			static_assert(sizeof(*descriptors) == sizeof(VkBufferView));
			write.pTexelBufferView = reinterpret_cast<const VkBufferView *>(descriptors);
		}
		break;
	case api::descriptor_type::constant_buffer:
	case api::descriptor_type::shader_storage_buffer:
		static_assert(sizeof(api::buffer_range) == sizeof(VkDescriptorBufferInfo));
		write.pBufferInfo = static_cast<const VkDescriptorBufferInfo *>(update.descriptors);
		break;
	default:
		assert(false);
		break;
	}

	if (_device_impl->_push_descriptor_ext)
	{
		vk.CmdPushDescriptorSetKHR(_orig,
			stages == api::shader_stage::compute ? VK_PIPELINE_BIND_POINT_COMPUTE : VK_PIPELINE_BIND_POINT_GRAPHICS,
			(VkPipelineLayout)layout.handle, layout_param,
			1, &write);
		return;
	}

	assert(update.binding == 0 && update.array_offset == 0);

	const VkPipelineLayout pipeline_layout = (VkPipelineLayout)layout.handle;
	const VkDescriptorSetLayout set_layout = (VkDescriptorSetLayout)_device_impl->get_private_data_for_object<VK_OBJECT_TYPE_PIPELINE_LAYOUT>(pipeline_layout)->set_layouts[layout_param];

	VkDescriptorSetAllocateInfo alloc_info { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
	alloc_info.descriptorPool = _device_impl->_transient_descriptor_pool[_device_impl->_transient_index % 4];
	alloc_info.descriptorSetCount = 1;
	alloc_info.pSetLayouts = &set_layout;

	// Access to descriptor pools must be externally synchronized, so lock for the duration of allocation from the transient descriptor pool
	if (const std::unique_lock<std::shared_mutex> lock(_device_impl->_mutex);
		vk.AllocateDescriptorSets(_device_impl->_orig, &alloc_info, &write.dstSet) != VK_SUCCESS)
	{
		LOG(ERROR) << "Failed to allocate " << update.count << " transient descriptor handle(s) of type " << static_cast<uint32_t>(update.type) << '!';
		return;
	}

	vk.UpdateDescriptorSets(_device_impl->_orig, 1, &write, 0, nullptr);

	vk.CmdBindDescriptorSets(_orig,
		stages == api::shader_stage::compute ? VK_PIPELINE_BIND_POINT_COMPUTE : VK_PIPELINE_BIND_POINT_GRAPHICS,
		(VkPipelineLayout)layout.handle,
		layout_param, 1, &write.dstSet,
		0, nullptr);
}
void reshade::vulkan::command_list_impl::bind_descriptor_sets(api::shader_stage stages, api::pipeline_layout layout, uint32_t first, uint32_t count, const api::descriptor_set *sets)
{
	if ((stages & api::shader_stage::all_compute) == api::shader_stage::all_compute)
	{
		vk.CmdBindDescriptorSets(_orig,
			VK_PIPELINE_BIND_POINT_COMPUTE,
			(VkPipelineLayout)layout.handle,
			first, count, reinterpret_cast<const VkDescriptorSet *>(sets), 0, nullptr);
	}
	if ((stages & api::shader_stage::all_graphics) == api::shader_stage::all_graphics)
	{
		vk.CmdBindDescriptorSets(_orig,
			VK_PIPELINE_BIND_POINT_GRAPHICS,
			(VkPipelineLayout)layout.handle,
			first, count, reinterpret_cast<const VkDescriptorSet *>(sets), 0, nullptr);
	}
}

void reshade::vulkan::command_list_impl::bind_index_buffer(api::resource buffer, uint64_t offset, uint32_t index_size)
{
	vk.CmdBindIndexBuffer(_orig, (VkBuffer)buffer.handle, offset, index_size == 1 ? VK_INDEX_TYPE_UINT8_EXT : index_size == 2 ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32);
}
void reshade::vulkan::command_list_impl::bind_vertex_buffers(uint32_t first, uint32_t count, const api::resource *buffers, const uint64_t *offsets, const uint32_t *)
{
	vk.CmdBindVertexBuffers(_orig, first, count, reinterpret_cast<const VkBuffer *>(buffers), offsets);
}
void reshade::vulkan::command_list_impl::bind_stream_output_buffers(uint32_t first, uint32_t count, const api::resource *buffers, const uint64_t *offsets, const uint64_t *max_sizes, const api::resource *, const uint64_t *)
{
	if (vk.CmdBindTransformFeedbackBuffersEXT != nullptr)
		vk.CmdBindTransformFeedbackBuffersEXT(_orig, first, count, reinterpret_cast<const VkBuffer *>(buffers), offsets, max_sizes);
}

void reshade::vulkan::command_list_impl::draw(uint32_t vertex_count, uint32_t instance_count, uint32_t first_vertex, uint32_t first_instance)
{
	_has_commands = true;

	vk.CmdDraw(_orig, vertex_count, instance_count, first_vertex, first_instance);
}
void reshade::vulkan::command_list_impl::draw_indexed(uint32_t index_count, uint32_t instance_count, uint32_t first_index, int32_t vertex_offset, uint32_t first_instance)
{
	_has_commands = true;

	vk.CmdDrawIndexed(_orig, index_count, instance_count, first_index, vertex_offset, first_instance);
}
void reshade::vulkan::command_list_impl::dispatch(uint32_t group_count_x, uint32_t group_count_y, uint32_t group_count_z)
{
	_has_commands = true;

	vk.CmdDispatch(_orig, group_count_x, group_count_y, group_count_z);
}
void reshade::vulkan::command_list_impl::draw_or_dispatch_indirect(api::indirect_command type, api::resource buffer, uint64_t offset, uint32_t draw_count, uint32_t stride)
{
	_has_commands = true;

	switch (type)
	{
	case api::indirect_command::draw:
		vk.CmdDrawIndirect(_orig, (VkBuffer)buffer.handle, offset, draw_count, stride);
		break;
	case api::indirect_command::draw_indexed:
		vk.CmdDrawIndexedIndirect(_orig, (VkBuffer)buffer.handle, offset, draw_count, stride);
		break;
	case api::indirect_command::dispatch:
		for (uint32_t i = 0; i < draw_count; ++i)
			vk.CmdDispatchIndirect(_orig, (VkBuffer)buffer.handle, offset + static_cast<uint64_t>(i) * stride);
		break;
	}
}

void reshade::vulkan::command_list_impl::copy_resource(api::resource src, api::resource dst)
{
	const api::resource_desc desc = _device_impl->get_resource_desc(src);

	if (desc.type == api::resource_type::buffer)
	{
		copy_buffer_region(src, 0, dst, 0, ~0llu);
	}
	else
	{
		for (uint32_t layer = 0; layer < desc.texture.depth_or_layers; ++layer)
		{
			for (uint32_t level = 0; level < desc.texture.levels; ++level)
			{
				const uint32_t subresource = level + layer * desc.texture.levels;

				copy_texture_region(src, subresource, nullptr, dst, subresource, nullptr, api::filter_mode::min_mag_mip_point);
			}
		}
	}
}
void reshade::vulkan::command_list_impl::copy_buffer_region(api::resource src, uint64_t src_offset, api::resource dst, uint64_t dst_offset, uint64_t size)
{
	_has_commands = true;

	if (UINT64_MAX == size)
		size = _device_impl->get_private_data_for_object<VK_OBJECT_TYPE_BUFFER>((VkBuffer)src.handle)->create_info.size;

	VkBufferCopy region;
	region.srcOffset = src_offset;
	region.dstOffset = dst_offset;
	region.size = size;

	vk.CmdCopyBuffer(_orig, (VkBuffer)src.handle, (VkBuffer)dst.handle, 1, &region);
}
void reshade::vulkan::command_list_impl::copy_buffer_to_texture(api::resource src, uint64_t src_offset, uint32_t row_length, uint32_t slice_height, api::resource dst, uint32_t dst_subresource, const api::subresource_box *dst_box)
{
	_has_commands = true;

	const auto dst_data = _device_impl->get_private_data_for_object<VK_OBJECT_TYPE_IMAGE>((VkImage)dst.handle);

	VkBufferImageCopy region;
	region.bufferOffset = src_offset;
	region.bufferRowLength = row_length;
	region.bufferImageHeight = slice_height;

	convert_subresource(dst_subresource, dst_data->create_info, region.imageSubresource);
	if (dst_box != nullptr)
	{
		region.imageOffset.x = dst_box->left;
		region.imageOffset.y = dst_box->top;
		region.imageOffset.z = dst_box->front;

		region.imageExtent.width  = dst_box->right - dst_box->left;
		region.imageExtent.height = dst_box->bottom - dst_box->top;
		region.imageExtent.depth  = dst_box->back - dst_box->front;

		if (dst_data->create_info.imageType != VK_IMAGE_TYPE_3D)
		{
			region.imageSubresource.layerCount = region.imageExtent.depth;
			region.imageExtent.depth = 1;
		}
	}
	else
	{
		region.imageOffset = { 0, 0, 0 };

		region.imageExtent.width  = std::max(1u, dst_data->create_info.extent.width  >> region.imageSubresource.mipLevel);
		region.imageExtent.height = std::max(1u, dst_data->create_info.extent.height >> region.imageSubresource.mipLevel);
		region.imageExtent.depth  = std::max(1u, dst_data->create_info.extent.depth  >> region.imageSubresource.mipLevel);
	}

	vk.CmdCopyBufferToImage(_orig, (VkBuffer)src.handle, (VkImage)dst.handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
}
void reshade::vulkan::command_list_impl::copy_texture_region(api::resource src, uint32_t src_subresource, const api::subresource_box *src_box, api::resource dst, uint32_t dst_subresource, const api::subresource_box *dst_box, api::filter_mode filter)
{
	_has_commands = true;

	const auto src_data = _device_impl->get_private_data_for_object<VK_OBJECT_TYPE_IMAGE>((VkImage)src.handle);
	const auto dst_data = _device_impl->get_private_data_for_object<VK_OBJECT_TYPE_IMAGE>((VkImage)dst.handle);

	if ((src_box == nullptr && dst_box == nullptr && std::memcmp(&src_data->create_info.extent, &dst_data->create_info.extent, sizeof(VkExtent3D)) == 0) ||
		(src_box != nullptr && dst_box != nullptr && src_box->width() == dst_box->width() && src_box->height() == dst_box->height() && src_box->depth() == dst_box->depth()))
	{
		VkImageCopy region;

		convert_subresource(src_subresource, src_data->create_info, region.srcSubresource);
		if (src_box != nullptr)
			std::copy_n(&src_box->left, 3, &region.srcOffset.x);
		else
			region.srcOffset = { 0, 0, 0 };

		convert_subresource(dst_subresource, dst_data->create_info, region.dstSubresource);
		if (dst_box != nullptr)
			std::copy_n(&dst_box->left, 3, &region.dstOffset.x);
		else
			region.dstOffset = { 0, 0, 0 };

		if (src_box != nullptr)
		{
			region.extent.width  = src_box->width();
			region.extent.height = src_box->height();
			region.extent.depth  = src_box->depth();

			if (src_data->create_info.imageType != VK_IMAGE_TYPE_3D)
			{
				region.srcSubresource.layerCount = region.extent.depth;
				region.dstSubresource.layerCount = region.extent.depth;
				region.extent.depth = 1;
			}
		}
		else
		{
			region.extent.width  = std::max(1u, src_data->create_info.extent.width  >> region.srcSubresource.mipLevel);
			region.extent.height = std::max(1u, src_data->create_info.extent.height >> region.srcSubresource.mipLevel);
			region.extent.depth  = std::max(1u, src_data->create_info.extent.depth  >> region.srcSubresource.mipLevel);
		}

		vk.CmdCopyImage(_orig,
			(VkImage)src.handle, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			(VkImage)dst.handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1, &region);
	}
	else
	{
		VkImageBlit region;

		convert_subresource(src_subresource, src_data->create_info, region.srcSubresource);
		if (src_box != nullptr)
		{
			std::copy_n(&src_box->left, 6, &region.srcOffsets[0].x);

			if (src_data->create_info.imageType != VK_IMAGE_TYPE_3D)
			{
				region.srcSubresource.layerCount = src_box->depth();
				region.srcOffsets[1].z = region.srcOffsets[0].z + 1;
			}
		}
		else
		{
			region.srcOffsets[0] = { 0, 0, 0 };
			region.srcOffsets[1] = {
				static_cast<int32_t>(std::max(1u, src_data->create_info.extent.width  >> region.srcSubresource.mipLevel)),
				static_cast<int32_t>(std::max(1u, src_data->create_info.extent.height >> region.srcSubresource.mipLevel)),
				static_cast<int32_t>(std::max(1u, src_data->create_info.extent.depth  >> region.srcSubresource.mipLevel)) };
		}

		convert_subresource(dst_subresource, dst_data->create_info, region.dstSubresource);
		if (dst_box != nullptr)
		{
			std::copy_n(&dst_box->left, 6, &region.dstOffsets[0].x);

			if (src_data->create_info.imageType != VK_IMAGE_TYPE_3D)
			{
				region.dstSubresource.layerCount = dst_box->depth();
				region.dstOffsets[1].z = region.dstOffsets[0].z + 1;
			}
		}
		else
		{
			region.dstOffsets[0] = { 0, 0, 0 };
			region.dstOffsets[1] = {
				static_cast<int32_t>(std::max(1u, dst_data->create_info.extent.width  >> region.dstSubresource.mipLevel)),
				static_cast<int32_t>(std::max(1u, dst_data->create_info.extent.height >> region.dstSubresource.mipLevel)),
				static_cast<int32_t>(std::max(1u, dst_data->create_info.extent.depth  >> region.dstSubresource.mipLevel)) };
		}

		vk.CmdBlitImage(_orig,
			(VkImage)src.handle, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			(VkImage)dst.handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1, &region,
			filter == api::filter_mode::min_mag_mip_linear || filter == api::filter_mode::min_mag_linear_mip_point ? VK_FILTER_LINEAR : VK_FILTER_NEAREST);
	}
}
void reshade::vulkan::command_list_impl::copy_texture_to_buffer(api::resource src, uint32_t src_subresource, const api::subresource_box *src_box, api::resource dst, uint64_t dst_offset, uint32_t row_length, uint32_t slice_height)
{
	_has_commands = true;

	const auto src_data = _device_impl->get_private_data_for_object<VK_OBJECT_TYPE_IMAGE>((VkImage)src.handle);

	VkBufferImageCopy region;
	region.bufferOffset = dst_offset;
	region.bufferRowLength = row_length;
	region.bufferImageHeight = slice_height;

	convert_subresource(src_subresource, src_data->create_info, region.imageSubresource);

	if (src_box != nullptr)
	{
		region.imageOffset.x = src_box->left;
		region.imageOffset.y = src_box->top;
		region.imageOffset.z = src_box->front;

		region.imageExtent.width  = src_box->width();
		region.imageExtent.height = src_box->height();
		region.imageExtent.depth  = src_box->depth();

		if (src_data->create_info.imageType != VK_IMAGE_TYPE_3D)
		{
			region.imageSubresource.layerCount = region.imageExtent.depth;
			region.imageExtent.depth = 1;
		}
	}
	else
	{
		region.imageOffset = { 0, 0, 0 };

		region.imageExtent.width  = std::max(1u, src_data->create_info.extent.width  >> region.imageSubresource.mipLevel);
		region.imageExtent.height = std::max(1u, src_data->create_info.extent.height >> region.imageSubresource.mipLevel);
		region.imageExtent.depth  = std::max(1u, src_data->create_info.extent.depth  >> region.imageSubresource.mipLevel);
	}

	vk.CmdCopyImageToBuffer(_orig, (VkImage)src.handle, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, (VkBuffer)dst.handle, 1, &region);
}
void reshade::vulkan::command_list_impl::resolve_texture_region(api::resource src, uint32_t src_subresource, const api::subresource_box *src_box, api::resource dst, uint32_t dst_subresource, int32_t dst_x, int32_t dst_y, int32_t dst_z, api::format)
{
	_has_commands = true;

	const auto src_data = _device_impl->get_private_data_for_object<VK_OBJECT_TYPE_IMAGE>((VkImage)src.handle);
	const auto dst_data = _device_impl->get_private_data_for_object<VK_OBJECT_TYPE_IMAGE>((VkImage)dst.handle);

	VkImageResolve region;

	convert_subresource(src_subresource, src_data->create_info, region.srcSubresource);
	convert_subresource(dst_subresource, dst_data->create_info, region.dstSubresource);

	if (src_box != nullptr)
	{
		region.srcOffset.x = src_box->left;
		region.srcOffset.y = src_box->top;
		region.srcOffset.z = src_box->front;

		region.extent.width  = src_box->width();
		region.extent.height = src_box->height();
		region.extent.depth  = src_box->depth();

		if (src_data->create_info.imageType != VK_IMAGE_TYPE_3D)
		{
			region.srcSubresource.layerCount = region.extent.depth;
			region.dstSubresource.layerCount = region.extent.depth;
			region.extent.depth = 1;
		}
	}
	else
	{
		region.srcOffset = { 0, 0, 0 };

		region.extent.width  = std::max(1u, src_data->create_info.extent.width  >> region.srcSubresource.mipLevel);
		region.extent.height = std::max(1u, src_data->create_info.extent.height >> region.srcSubresource.mipLevel);
		region.extent.depth  = std::max(1u, src_data->create_info.extent.depth  >> region.srcSubresource.mipLevel);
	}

	region.dstOffset = { dst_x, dst_y, dst_z };

	vk.CmdResolveImage(_orig,
		(VkImage)src.handle, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		(VkImage)dst.handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		1, &region);
}

void reshade::vulkan::command_list_impl::clear_depth_stencil_view(api::resource_view dsv, const float *depth, const uint8_t *stencil, uint32_t rect_count, const api::rect *)
{
	_has_commands = true;

	assert(rect_count == 0);

	const auto dsv_data = _device_impl->get_private_data_for_object<VK_OBJECT_TYPE_IMAGE_VIEW>((VkImageView)dsv.handle);

	VkImageAspectFlags clear_flags = 0;
	VkClearDepthStencilValue clear_value = {};
	if (depth != nullptr)
		clear_flags |= VK_IMAGE_ASPECT_DEPTH_BIT,
		clear_value.depth = *depth;
	if (stencil != nullptr)
		clear_flags |= VK_IMAGE_ASPECT_STENCIL_BIT,
		clear_value.stencil = *stencil;

	// Transition state to VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL (since it will be in 'resource_usage::depth_stencil_write' at this point)
	VkImageMemoryBarrier barrier { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
	barrier.oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = dsv_data->create_info.image;
	barrier.subresourceRange = dsv_data->create_info.subresourceRange;
	barrier.subresourceRange.aspectMask &= clear_flags;
	vk.CmdPipelineBarrier(_orig, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

	vk.CmdClearDepthStencilImage(_orig, dsv_data->create_info.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clear_value, 1, &barrier.subresourceRange);

	std::swap(barrier.oldLayout, barrier.newLayout);
	vk.CmdPipelineBarrier(_orig, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}
void reshade::vulkan::command_list_impl::clear_render_target_view(api::resource_view rtv, const float color[4], uint32_t rect_count, const api::rect *)
{
	_has_commands = true;

	assert(rect_count == 0);

	const auto rtv_data = _device_impl->get_private_data_for_object<VK_OBJECT_TYPE_IMAGE_VIEW>((VkImageView)rtv.handle);

	// Transition state to VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL (since it will be in 'resource_usage::render_target' at this point)
	VkImageMemoryBarrier barrier { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
	barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = rtv_data->create_info.image;
	barrier.subresourceRange = rtv_data->create_info.subresourceRange;
	vk.CmdPipelineBarrier(_orig, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

	VkClearColorValue clear_value;
	std::memcpy(clear_value.float32, color, 4 * sizeof(float));

	vk.CmdClearColorImage(_orig, rtv_data->create_info.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clear_value, 1, &barrier.subresourceRange);

	std::swap(barrier.oldLayout, barrier.newLayout);
	vk.CmdPipelineBarrier(_orig, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}
void reshade::vulkan::command_list_impl::clear_unordered_access_view_uint(api::resource_view uav, const uint32_t values[4], uint32_t rect_count, const api::rect *)
{
	_has_commands = true;

	assert(rect_count == 0);

	const auto uav_data = _device_impl->get_private_data_for_object<VK_OBJECT_TYPE_IMAGE_VIEW>((VkImageView)uav.handle);

	VkClearColorValue clear_value;
	std::memcpy(clear_value.uint32, values, 4 * sizeof(uint32_t));

	vk.CmdClearColorImage(_orig, uav_data->create_info.image, VK_IMAGE_LAYOUT_GENERAL, &clear_value, 1, &uav_data->create_info.subresourceRange);
}
void reshade::vulkan::command_list_impl::clear_unordered_access_view_float(api::resource_view uav, const float values[4], uint32_t rect_count, const api::rect *)
{
	_has_commands = true;

	assert(rect_count == 0);

	const auto uav_data = _device_impl->get_private_data_for_object<VK_OBJECT_TYPE_IMAGE_VIEW>((VkImageView)uav.handle);

	VkClearColorValue clear_value;
	std::memcpy(clear_value.float32, values, 4 * sizeof(float));

	vk.CmdClearColorImage(_orig, uav_data->create_info.image, VK_IMAGE_LAYOUT_GENERAL, &clear_value, 1, &uav_data->create_info.subresourceRange);
}

void reshade::vulkan::command_list_impl::generate_mipmaps(api::resource_view srv)
{
	const auto view_data = _device_impl->get_private_data_for_object<VK_OBJECT_TYPE_IMAGE_VIEW>((VkImageView)srv.handle);
	const auto image_data = _device_impl->get_private_data_for_object<VK_OBJECT_TYPE_IMAGE>(view_data->create_info.image);

	VkImageMemoryBarrier barrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
	barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
	barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
	barrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = view_data->create_info.image;
	barrier.subresourceRange = view_data->create_info.subresourceRange;
	vk.CmdPipelineBarrier(_orig, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

	int32_t width = image_data->create_info.extent.width >> view_data->create_info.subresourceRange.baseMipLevel;
	int32_t height = image_data->create_info.extent.height >> view_data->create_info.subresourceRange.baseMipLevel;

	for (uint32_t level = view_data->create_info.subresourceRange.baseMipLevel + 1; level < image_data->create_info.mipLevels; ++level, width /= 2, height /= 2)
	{
		barrier.subresourceRange.baseMipLevel = level;
		barrier.subresourceRange.levelCount = 1;

		barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		vk.CmdPipelineBarrier(_orig, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

		VkImageBlit blit;
		blit.srcOffsets[0] = { 0, 0, 0 };
		blit.srcOffsets[1] = { width, height, 1 };
		blit.srcSubresource = { barrier.subresourceRange.aspectMask, level - 1, barrier.subresourceRange.baseArrayLayer, barrier.subresourceRange.layerCount };
		blit.dstOffsets[0] = { 0, 0, 0 };
		blit.dstOffsets[1] = { width > 1 ? width / 2 : 1, height > 1 ? height / 2 : 1, 1 };
		blit.dstSubresource = { barrier.subresourceRange.aspectMask, level    , barrier.subresourceRange.baseArrayLayer, barrier.subresourceRange.layerCount };
		vk.CmdBlitImage(_orig, view_data->create_info.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, view_data->create_info.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);

		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		vk.CmdPipelineBarrier(_orig, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
	}

	barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
	barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	barrier.subresourceRange = view_data->create_info.subresourceRange;
	vk.CmdPipelineBarrier(_orig, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}

void reshade::vulkan::command_list_impl::begin_query(api::query_pool pool, api::query_type type, uint32_t index)
{
	_has_commands = true;

	assert(pool.handle != 0);
	assert(_device_impl->get_private_data_for_object<VK_OBJECT_TYPE_QUERY_POOL>((VkQueryPool)pool.handle)->type == convert_query_type(type));

	vk.CmdResetQueryPool(_orig, (VkQueryPool)pool.handle, index, 1);

	switch (type)
	{
	default:
		vk.CmdBeginQuery(_orig, (VkQueryPool)pool.handle, index, 0);
		break;
	case api::query_type::stream_output_statistics_0:
	case api::query_type::stream_output_statistics_1:
	case api::query_type::stream_output_statistics_2:
	case api::query_type::stream_output_statistics_3:
		if (vk.CmdBeginQueryIndexedEXT != nullptr)
			vk.CmdBeginQueryIndexedEXT(_orig, (VkQueryPool)pool.handle, index, 0, static_cast<uint32_t>(type) - static_cast<uint32_t>(api::query_type::stream_output_statistics_0));
		break;
	}
}
void reshade::vulkan::command_list_impl::end_query(api::query_pool pool, api::query_type type, uint32_t index)
{
	_has_commands = true;

	assert(pool.handle != 0);
	assert(_device_impl->get_private_data_for_object<VK_OBJECT_TYPE_QUERY_POOL>((VkQueryPool)pool.handle)->type == convert_query_type(type));

	switch (type)
	{
	default:
		vk.CmdEndQuery(_orig, (VkQueryPool)pool.handle, index);
		break;
	case api::query_type::timestamp:
		vk.CmdResetQueryPool(_orig, (VkQueryPool)pool.handle, index, 1);
		vk.CmdWriteTimestamp(_orig, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, (VkQueryPool)pool.handle, index);
		break;
	case api::query_type::stream_output_statistics_0:
	case api::query_type::stream_output_statistics_1:
	case api::query_type::stream_output_statistics_2:
	case api::query_type::stream_output_statistics_3:
		if (vk.CmdEndQueryIndexedEXT != nullptr)
			vk.CmdEndQueryIndexedEXT(_orig, (VkQueryPool)pool.handle, index, static_cast<uint32_t>(type) - static_cast<uint32_t>(api::query_type::stream_output_statistics_0));
		break;
	}
}
void reshade::vulkan::command_list_impl::copy_query_pool_results(api::query_pool pool, api::query_type type, uint32_t first, uint32_t count, api::resource dst, uint64_t dst_offset, uint32_t stride)
{
	_has_commands = true;

	assert(pool.handle != 0);
	assert(_device_impl->get_private_data_for_object<VK_OBJECT_TYPE_QUERY_POOL>((VkQueryPool)pool.handle)->type == convert_query_type(type));

	vk.CmdCopyQueryPoolResults(_orig, (VkQueryPool)pool.handle, first, count, (VkBuffer)dst.handle, dst_offset, stride, VK_QUERY_RESULT_64_BIT);
}

void reshade::vulkan::command_list_impl::begin_debug_event(const char *label, const float color[4])
{
	assert(label != nullptr);

	if (vk.CmdBeginDebugUtilsLabelEXT == nullptr)
		return;

	VkDebugUtilsLabelEXT label_info { VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT };
	label_info.pLabelName = label;

	// The optional color value is ignored if all elements are set to zero
	if (color != nullptr)
	{
		label_info.color[0] = color[0];
		label_info.color[1] = color[1];
		label_info.color[2] = color[2];
		label_info.color[3] = color[3];
	}

	vk.CmdBeginDebugUtilsLabelEXT(_orig, &label_info);
}
void reshade::vulkan::command_list_impl::end_debug_event()
{
	if (vk.CmdEndDebugUtilsLabelEXT == nullptr)
		return;

	vk.CmdEndDebugUtilsLabelEXT(_orig);
}
void reshade::vulkan::command_list_impl::insert_debug_marker(const char *label, const float color[4])
{
	assert(label != nullptr);

	if (vk.CmdInsertDebugUtilsLabelEXT == nullptr)
		return;

	VkDebugUtilsLabelEXT label_info { VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT };
	label_info.pLabelName = label;

	if (color != nullptr)
	{
		label_info.color[0] = color[0];
		label_info.color[1] = color[1];
		label_info.color[2] = color[2];
		label_info.color[3] = color[3];
	}

	vk.CmdInsertDebugUtilsLabelEXT(_orig, &label_info);
}
