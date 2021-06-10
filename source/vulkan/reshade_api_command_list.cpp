/*
 * Copyright (C) 2021 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "reshade_api_device.hpp"
#include "reshade_api_command_list.hpp"
#include "reshade_api_type_convert.hpp"
#include <algorithm>

#define vk _device_impl->_dispatch_table

extern VkImageAspectFlags aspect_flags_from_format(VkFormat format);

static inline void convert_subresource(uint32_t subresource, const VkImageCreateInfo &create_info, VkImageSubresourceLayers &result)
{
	result.aspectMask = aspect_flags_from_format(create_info.format);
	result.mipLevel = subresource % create_info.mipLevels;
	result.baseArrayLayer = subresource / create_info.mipLevels;
	result.layerCount = 1;
}

reshade::vulkan::command_list_impl::command_list_impl(device_impl *device, VkCommandBuffer cmd_list) :
	api_object_impl(cmd_list), _device_impl(device), _has_commands(cmd_list != VK_NULL_HANDLE)
{
#if RESHADE_ADDON
	if (_has_commands) // Do not call add-on event for immediate command list
		invoke_addon_event<addon_event::init_command_list>(this);
#endif
}
reshade::vulkan::command_list_impl::~command_list_impl()
{
#if RESHADE_ADDON
	if (_has_commands)
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

	std::vector<VkImageMemoryBarrier> image_barriers;
	image_barriers.reserve(count);
	std::vector<VkBufferMemoryBarrier> buffer_barriers;
	buffer_barriers.reserve(count);

	VkPipelineStageFlags src_stage_mask = 0;
	VkPipelineStageFlags dst_stage_mask = 0;

	for (uint32_t i = 0; i < count; ++i)
	{
		const resource_data data = _device_impl->lookup_resource(resources[i]);

		if (data.is_image())
		{
			VkImageMemoryBarrier &transition = image_barriers.emplace_back();
			transition = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
			transition.srcAccessMask = convert_usage_to_access(old_states[i]);
			transition.dstAccessMask = convert_usage_to_access(new_states[i]);
			transition.oldLayout = convert_usage_to_image_layout(old_states[i]);
			transition.newLayout = convert_usage_to_image_layout(new_states[i]);
			transition.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			transition.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			transition.image = data.image;
			transition.subresourceRange = { aspect_flags_from_format(data.image_create_info.format), 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS };
		}
		else
		{
			VkBufferMemoryBarrier &transition = buffer_barriers.emplace_back();
			transition = { VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER };
			transition.srcAccessMask = convert_usage_to_access(old_states[i]);
			transition.dstAccessMask = convert_usage_to_access(new_states[i]);
			transition.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			transition.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			transition.buffer = data.buffer;
			transition.offset = 0;
			transition.size = VK_WHOLE_SIZE;
		}

		src_stage_mask |= convert_usage_to_pipeline_stage(old_states[i], true, _device_impl->_enabled_features);
		dst_stage_mask |= convert_usage_to_pipeline_stage(new_states[i], false, _device_impl->_enabled_features);
	}

	assert(src_stage_mask != 0 && dst_stage_mask != 0);

	vk.CmdPipelineBarrier(_orig, src_stage_mask, dst_stage_mask, 0, 0, nullptr, static_cast<uint32_t>(buffer_barriers.size()), buffer_barriers.data(), static_cast<uint32_t>(image_barriers.size()), image_barriers.data());
}

void reshade::vulkan::command_list_impl::begin_render_pass(api::render_pass pass)
{
	_has_commands = true;

	assert(pass.handle != 0);

	const auto pass_impl = reinterpret_cast<const render_pass_impl *>(pass.handle);

	VkRenderPassBeginInfo begin_info { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
	begin_info.renderPass = pass_impl->render_pass;
	begin_info.framebuffer = pass_impl->fbo;
	begin_info.renderArea = pass_impl->render_area;

	vk.CmdBeginRenderPass(_orig, &begin_info, VK_SUBPASS_CONTENTS_INLINE);

	_current_fbo = pass_impl->fbo;
	_current_render_area = pass_impl->render_area;
}
void reshade::vulkan::command_list_impl::finish_render_pass()
{
	vk.CmdEndRenderPass(_orig);

#ifndef NDEBUG
	_current_fbo = VK_NULL_HANDLE;
#endif
}

void reshade::vulkan::command_list_impl::bind_pipeline(api::pipeline_stage type, api::pipeline pipeline)
{
	assert(type == api::pipeline_stage::all_compute || type == api::pipeline_stage::all_graphics);

	vk.CmdBindPipeline(_orig,
		type == api::pipeline_stage::all_compute ? VK_PIPELINE_BIND_POINT_COMPUTE : VK_PIPELINE_BIND_POINT_GRAPHICS,
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
			float blend_constant[4];
			blend_constant[0] = ((values[i]      ) & 0xFF) / 255.0f;
			blend_constant[1] = ((values[i] >>  4) & 0xFF) / 255.0f;
			blend_constant[2] = ((values[i] >>  8) & 0xFF) / 255.0f;
			blend_constant[3] = ((values[i] >> 12) & 0xFF) / 255.0f;

			vk.CmdSetBlendConstants(_orig, blend_constant);
			break;
		}
		case api::dynamic_state::stencil_read_mask:
			vk.CmdSetStencilCompareMask(_orig, VK_STENCIL_FACE_FRONT_AND_BACK, values[i]);
			break;
		case api::dynamic_state::stencil_write_mask:
			vk.CmdSetStencilWriteMask(_orig, VK_STENCIL_FACE_FRONT_AND_BACK, values[i]);
			break;
		case api::dynamic_state::stencil_reference_value:
			vk.CmdSetStencilReference(_orig, VK_STENCIL_FACE_FRONT_AND_BACK, values[i]);
			break;
		default:
			assert(false);
			break;
		}
	}
}
void reshade::vulkan::command_list_impl::bind_viewports(uint32_t first, uint32_t count, const float *viewports)
{
#if 0
	_device_impl->vk.CmdSetViewport(_orig, first, count, reinterpret_cast<const VkViewport *>(viewports));
#else
	assert(count <= 16);
	VkViewport new_viewports[16];
	for (uint32_t i = 0, k = 0; i < count; ++i, k += 6)
	{
		new_viewports[i].x = viewports[k + 0];
		new_viewports[i].y = viewports[k + 1];
		new_viewports[i].width = viewports[k + 2];
		new_viewports[i].height = viewports[k + 3];
		new_viewports[i].minDepth = viewports[k + 4];
		new_viewports[i].maxDepth = viewports[k + 5];

		// Flip viewport vertically
		new_viewports[i].y += new_viewports[i].height;
		new_viewports[i].height = -new_viewports[i].height;
	}

	vk.CmdSetViewport(_orig, first, count, new_viewports);
#endif
}
void reshade::vulkan::command_list_impl::bind_scissor_rects(uint32_t first, uint32_t count, const int32_t *rects)
{
	const auto rect_data = static_cast<VkRect2D *>(alloca(sizeof(VkRect2D) * count));
	for (uint32_t i = 0, k = 0; i < count; ++i, k += 4)
	{
		rect_data[i].offset.x = rects[k + 0];
		rect_data[i].offset.y = rects[k + 1];
		rect_data[i].extent.width = rects[k + 2] - rects[k + 0];
		rect_data[i].extent.height = rects[k + 3] - rects[k + 1];
	}

	vk.CmdSetScissor(_orig, first, count, rect_data);
}

void reshade::vulkan::command_list_impl::push_constants(api::shader_stage stages, api::pipeline_layout layout, uint32_t, uint32_t offset, uint32_t count, const void *values)
{
	assert(count != 0);

	vk.CmdPushConstants(_orig,
		(VkPipelineLayout)layout.handle,
		static_cast<VkShaderStageFlags>(stages),
		offset * 4, count * 4, values);
}
void reshade::vulkan::command_list_impl::push_descriptors(api::shader_stage stages, api::pipeline_layout layout, uint32_t layout_index, api::descriptor_type type, uint32_t first, uint32_t count, const void *descriptors)
{
	assert(count != 0);

	VkWriteDescriptorSet write { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
	write.dstBinding = first;
	write.descriptorCount = count;
	write.descriptorType = static_cast<VkDescriptorType>(type);

	std::vector<VkDescriptorImageInfo> image_info(count);
	std::vector<VkDescriptorBufferInfo> buffer_info(count);

	switch (type)
	{
	case api::descriptor_type::sampler:
		for (uint32_t i = 0; i < count; ++i)
		{
			const auto &descriptor = static_cast<const api::sampler *>(descriptors)[i];
			image_info[i].sampler = (VkSampler)descriptor.handle;
		}
		write.pImageInfo = image_info.data();
		break;
	case api::descriptor_type::sampler_with_resource_view:
		for (uint32_t i = 0; i < count; ++i)
		{
			const auto &descriptor = static_cast<const api::sampler_with_resource_view *>(descriptors)[i];
			image_info[i].sampler = (VkSampler)descriptor.sampler.handle;
			image_info[i].imageView = (VkImageView)descriptor.view.handle;
			image_info[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		}
		write.pImageInfo = image_info.data();
		break;
	case api::descriptor_type::shader_resource_view:
		for (uint32_t i = 0; i < count; ++i)
		{
			const auto &descriptor = static_cast<const api::resource_view *>(descriptors)[i];
			image_info[i].imageView = (VkImageView)descriptor.handle;
			image_info[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		}
		write.pImageInfo = image_info.data();
		break;
	case api::descriptor_type::unordered_access_view:
		for (uint32_t i = 0; i < count; ++i)
		{
			const auto &descriptor = static_cast<const api::resource_view *>(descriptors)[i];
			image_info[i].imageView = (VkImageView)descriptor.handle;
			image_info[i].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		}
		write.pImageInfo = image_info.data();
		break;
	case api::descriptor_type::constant_buffer:
		for (uint32_t i = 0; i < count; ++i)
		{
			const auto &descriptor = static_cast<const api::resource *>(descriptors)[i];
			buffer_info[i].buffer = (VkBuffer)descriptor.handle;
			buffer_info[i].offset = 0;
			buffer_info[i].range = VK_WHOLE_SIZE;
		}
		write.pBufferInfo = buffer_info.data();
		break;
	}

	if (vk.CmdPushDescriptorSetKHR != nullptr)
	{
		vk.CmdPushDescriptorSetKHR(_orig,
			stages == api::shader_stage::compute ? VK_PIPELINE_BIND_POINT_COMPUTE : VK_PIPELINE_BIND_POINT_GRAPHICS,
			(VkPipelineLayout)layout.handle, layout_index,
			1, &write);
	}
	else
	{
		assert(first == 0);

		VkDescriptorSetAllocateInfo alloc_info { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
		alloc_info.descriptorPool = _device_impl->_transient_descriptor_pool[_device_impl->_transient_index % 4];
		alloc_info.descriptorSetCount = 1;
		alloc_info.pSetLayouts = &_device_impl->_pipeline_layout_list[(VkPipelineLayout)layout.handle][layout_index];

		if (vk.AllocateDescriptorSets(_device_impl->_orig, &alloc_info, &write.dstSet) != VK_SUCCESS)
			return;

		vk.UpdateDescriptorSets(_device_impl->_orig, 1, &write, 0, nullptr);

		vk.CmdBindDescriptorSets(_orig,
			stages == api::shader_stage::compute ? VK_PIPELINE_BIND_POINT_COMPUTE : VK_PIPELINE_BIND_POINT_GRAPHICS,
			(VkPipelineLayout)layout.handle,
			layout_index, 1, &write.dstSet,
			0, nullptr);
	}
}
void reshade::vulkan::command_list_impl::bind_descriptor_sets(api::shader_stage stages, api::pipeline_layout layout, uint32_t first, uint32_t count, const api::descriptor_set *sets)
{
	vk.CmdBindDescriptorSets(_orig,
		stages == api::shader_stage::compute ? VK_PIPELINE_BIND_POINT_COMPUTE : VK_PIPELINE_BIND_POINT_GRAPHICS,
		(VkPipelineLayout)layout.handle,
		first, count, reinterpret_cast<const VkDescriptorSet *>(sets),
		0, nullptr);
}

void reshade::vulkan::command_list_impl::bind_index_buffer(api::resource buffer, uint64_t offset, uint32_t index_size)
{
	vk.CmdBindIndexBuffer(_orig, (VkBuffer)buffer.handle, offset, index_size == 1 ? VK_INDEX_TYPE_UINT8_EXT : index_size == 2 ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32);
}
void reshade::vulkan::command_list_impl::bind_vertex_buffers(uint32_t first, uint32_t count, const api::resource *buffers, const uint64_t *offsets, const uint32_t *)
{
	vk.CmdBindVertexBuffers(_orig, first, count, reinterpret_cast<const VkBuffer *>(buffers), offsets);
}

void reshade::vulkan::command_list_impl::draw(uint32_t vertices, uint32_t instances, uint32_t first_vertex, uint32_t first_instance)
{
	_has_commands = true;

	assert(_current_fbo != VK_NULL_HANDLE);

	vk.CmdDraw(_orig, vertices, instances, first_vertex, first_instance);
}
void reshade::vulkan::command_list_impl::draw_indexed(uint32_t indices, uint32_t instances, uint32_t first_index, int32_t vertex_offset, uint32_t first_instance)
{
	_has_commands = true;

	assert(_current_fbo != VK_NULL_HANDLE);

	vk.CmdDrawIndexed(_orig, indices, instances, first_index, vertex_offset, first_instance);
}
void reshade::vulkan::command_list_impl::dispatch(uint32_t num_groups_x, uint32_t num_groups_y, uint32_t num_groups_z)
{
	_has_commands = true;

	vk.CmdDispatch(_orig, num_groups_x, num_groups_y, num_groups_z);
}
void reshade::vulkan::command_list_impl::draw_or_dispatch_indirect(uint32_t type, api::resource buffer, uint64_t offset, uint32_t draw_count, uint32_t stride)
{
	_has_commands = true;

	switch (type)
	{
	case 1:
		vk.CmdDrawIndirect(_orig, (VkBuffer)buffer.handle, offset, draw_count, stride);
		break;
	case 2:
		vk.CmdDrawIndexedIndirect(_orig, (VkBuffer)buffer.handle, offset, draw_count, stride);
		break;
	case 3:
		for (uint32_t i = 0; i < draw_count; ++i)
			vk.CmdDispatchIndirect(_orig, (VkBuffer)buffer.handle, offset + i * stride);
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

				copy_texture_region(src, subresource, nullptr, dst, subresource, nullptr, api::filter_type::min_mag_mip_point);
			}
		}
	}
}
void reshade::vulkan::command_list_impl::copy_buffer_region(api::resource src, uint64_t src_offset, api::resource dst, uint64_t dst_offset, uint64_t size)
{
	_has_commands = true;

	if (size == std::numeric_limits<uint64_t>::max())
	{
		const resource_data src_data = _device_impl->lookup_resource(src);
		size  = src_data.buffer_create_info.size;
	}

	VkBufferCopy region;
	region.srcOffset = src_offset;
	region.dstOffset = dst_offset;
	region.size = size;

	vk.CmdCopyBuffer(_orig, (VkBuffer)src.handle, (VkBuffer)dst.handle, 1, &region);
}
void reshade::vulkan::command_list_impl::copy_buffer_to_texture(api::resource src, uint64_t src_offset, uint32_t row_length, uint32_t slice_height, api::resource dst, uint32_t dst_subresource, const int32_t dst_box[6])
{
	_has_commands = true;

	const resource_data dst_data = _device_impl->lookup_resource(dst);
	assert(dst_data.is_image());

	VkBufferImageCopy region;
	region.bufferOffset = src_offset;
	region.bufferRowLength = row_length;
	region.bufferImageHeight = slice_height;

	convert_subresource(dst_subresource, dst_data.image_create_info, region.imageSubresource);
	if (dst_box != nullptr)
	{
		std::copy_n(dst_box, 3, &region.imageOffset.x);
		region.imageExtent.width  = dst_box[3] - dst_box[0];
		region.imageExtent.height = dst_box[4] - dst_box[1];
		region.imageExtent.depth  = dst_box[5] - dst_box[2];
	}
	else
	{
		region.imageOffset = { 0, 0, 0 };
		region.imageExtent.width  = std::max(1u, dst_data.image_create_info.extent.width  >> region.imageSubresource.mipLevel);
		region.imageExtent.height = std::max(1u, dst_data.image_create_info.extent.height >> region.imageSubresource.mipLevel);
		region.imageExtent.depth  = std::max(1u, dst_data.image_create_info.extent.depth  >> region.imageSubresource.mipLevel);
	}

	vk.CmdCopyBufferToImage(_orig, (VkBuffer)src.handle, (VkImage)dst.handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
}
void reshade::vulkan::command_list_impl::copy_texture_region(api::resource src, uint32_t src_subresource, const int32_t src_box[6], api::resource dst, uint32_t dst_subresource, const int32_t dst_box[6], api::filter_type filter)
{
	_has_commands = true;

	const resource_data src_data = _device_impl->lookup_resource(src);
	const resource_data dst_data = _device_impl->lookup_resource(dst);
	assert(src_data.is_image() && dst_data.is_image());

	if ((src_box == nullptr && dst_box == nullptr) || (src_box != nullptr && dst_box != nullptr &&
		(src_box[3] - src_box[0]) == (dst_box[3] - dst_box[0]) &&
		(src_box[4] - src_box[1]) == (dst_box[4] - dst_box[1]) &&
		(src_box[5] - src_box[2]) == (dst_box[5] - dst_box[2])))
	{
		VkImageCopy region;

		convert_subresource(src_subresource, src_data.image_create_info, region.srcSubresource);
		if (src_box != nullptr)
			std::copy_n(src_box, 3, &region.srcOffset.x);
		else
			region.srcOffset = { 0, 0, 0 };

		convert_subresource(dst_subresource, dst_data.image_create_info, region.dstSubresource);
		if (dst_box != nullptr)
			std::copy_n(dst_box, 3, &region.dstOffset.x);
		else
			region.dstOffset = { 0, 0, 0 };

		if (src_box != nullptr)
		{
			region.extent.width = src_box[3] - src_box[0];
			region.extent.height = src_box[4] - src_box[1];
			region.extent.depth = src_box[5] - src_box[2];
		}
		else
		{
			region.extent.width = std::max(1u, src_data.image_create_info.extent.width >> region.srcSubresource.mipLevel);
			region.extent.height = std::max(1u, src_data.image_create_info.extent.height >> region.srcSubresource.mipLevel);
			region.extent.depth = std::max(1u, src_data.image_create_info.extent.depth >> region.srcSubresource.mipLevel);
		}

		vk.CmdCopyImage(_orig,
			(VkImage)src.handle, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			(VkImage)dst.handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1, &region);
	}
	else
	{
		VkImageBlit region;

		convert_subresource(src_subresource, src_data.image_create_info, region.srcSubresource);
		if (src_box != nullptr)
		{
			std::copy_n(src_box, 6, &region.srcOffsets[0].x);
		}
		else
		{
			region.srcOffsets[0] = { 0, 0, 0 };
			region.srcOffsets[1] = {
				static_cast<int32_t>(std::max(1u, src_data.image_create_info.extent.width >> region.srcSubresource.mipLevel)),
				static_cast<int32_t>(std::max(1u, src_data.image_create_info.extent.height >> region.srcSubresource.mipLevel)),
				static_cast<int32_t>(std::max(1u, src_data.image_create_info.extent.depth >> region.srcSubresource.mipLevel)) };
		}

		convert_subresource(dst_subresource, dst_data.image_create_info, region.dstSubresource);
		if (dst_box != nullptr)
		{
			std::copy_n(dst_box, 6, &region.dstOffsets[0].x);
		}
		else
		{
			region.dstOffsets[0] = { 0, 0, 0 };
			region.dstOffsets[1] = {
				static_cast<int32_t>(std::max(1u, dst_data.image_create_info.extent.width >> region.dstSubresource.mipLevel)),
				static_cast<int32_t>(std::max(1u, dst_data.image_create_info.extent.height >> region.dstSubresource.mipLevel)),
				static_cast<int32_t>(std::max(1u, dst_data.image_create_info.extent.depth >> region.dstSubresource.mipLevel)) };
		}

		vk.CmdBlitImage(_orig,
			(VkImage)src.handle, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			(VkImage)dst.handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1, &region,
			filter == api::filter_type::min_mag_mip_linear || filter == api::filter_type::min_mag_linear_mip_point ? VK_FILTER_LINEAR : VK_FILTER_NEAREST);
	}
}
void reshade::vulkan::command_list_impl::copy_texture_to_buffer(api::resource src, uint32_t src_subresource, const int32_t src_box[6], api::resource dst, uint64_t dst_offset, uint32_t row_length, uint32_t slice_height)
{
	_has_commands = true;

	const resource_data src_data = _device_impl->lookup_resource(src);
	assert(src_data.is_image());

	VkBufferImageCopy region;
	region.bufferOffset = dst_offset;
	region.bufferRowLength = row_length;
	region.bufferImageHeight = slice_height;

	convert_subresource(src_subresource, src_data.image_create_info, region.imageSubresource);
	if (src_box != nullptr)
	{
		std::copy_n(src_box, 3, &region.imageOffset.x);
		region.imageExtent.width  = src_box[3] - src_box[0];
		region.imageExtent.height = src_box[4] - src_box[1];
		region.imageExtent.depth  = src_box[5] - src_box[2];
	}
	else
	{
		region.imageOffset = { 0, 0, 0 };
		region.imageExtent.width  = std::max(1u, src_data.image_create_info.extent.width  >> region.imageSubresource.mipLevel);
		region.imageExtent.height = std::max(1u, src_data.image_create_info.extent.height >> region.imageSubresource.mipLevel);
		region.imageExtent.depth  = std::max(1u, src_data.image_create_info.extent.depth  >> region.imageSubresource.mipLevel);
	}

	vk.CmdCopyImageToBuffer(_orig, (VkImage)src.handle, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, (VkBuffer)dst.handle, 1, &region);
}
void reshade::vulkan::command_list_impl::resolve_texture_region(api::resource src, uint32_t src_subresource, const int32_t src_box[6], api::resource dst, uint32_t dst_subresource, const int32_t dst_offset[3], api::format)
{
	_has_commands = true;

	const resource_data src_data = _device_impl->lookup_resource(src);
	const resource_data dst_data = _device_impl->lookup_resource(dst);
	assert(src_data.is_image() && dst_data.is_image());

	VkImageResolve region;

	convert_subresource(src_subresource, src_data.image_create_info, region.srcSubresource);
	if (src_box != nullptr)
		std::copy_n(src_box, 3, &region.srcOffset.x);
	else
		region.srcOffset = { 0, 0, 0 };

	convert_subresource(dst_subresource, dst_data.image_create_info, region.dstSubresource);
	if (dst_offset != nullptr)
		std::copy_n(dst_offset, 3, &region.dstOffset.x);
	else
		region.dstOffset = { 0, 0, 0 };

	if (src_box != nullptr)
	{
		region.extent.width  = src_box[3] - src_box[0];
		region.extent.height = src_box[4] - src_box[1];
		region.extent.depth  = src_box[5] - src_box[2];
	}
	else
	{
		region.extent.width  = std::max(1u, src_data.image_create_info.extent.width  >> region.srcSubresource.mipLevel);
		region.extent.height = std::max(1u, src_data.image_create_info.extent.height >> region.srcSubresource.mipLevel);
		region.extent.depth  = std::max(1u, src_data.image_create_info.extent.depth  >> region.srcSubresource.mipLevel);
	}

	vk.CmdResolveImage(_orig,
		(VkImage)src.handle, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		(VkImage)dst.handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		1, &region);
}

void reshade::vulkan::command_list_impl::clear_attachments(api::attachment_type clear_flags, const float color[4], float depth, uint8_t stencil, uint32_t num_rects, const int32_t *rects)
{
	_has_commands = true;

	assert(_current_fbo != VK_NULL_HANDLE);

	uint32_t num_clear_attachments = 0;
	VkClearAttachment clear_attachments[9];
	const VkImageAspectFlags aspect_mask = static_cast<VkImageAspectFlags>(clear_flags);

	if (aspect_mask & (VK_IMAGE_ASPECT_COLOR_BIT))
	{
		std::unique_lock<std::mutex> lock(_device_impl->_mutex);

		const auto &framebuffer_data = _device_impl->_framebuffer_list.at(_current_fbo);

		uint32_t index = 0;
		for (VkImageAspectFlags format_flags : framebuffer_data.attachment_types)
		{
			if (format_flags != VK_IMAGE_ASPECT_COLOR_BIT)
				continue;

			clear_attachments[num_clear_attachments].aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			clear_attachments[num_clear_attachments].colorAttachment = index++;
			std::memcpy(clear_attachments[num_clear_attachments].clearValue.color.float32, color, 4 * sizeof(float));
			++num_clear_attachments;
		}
	}

	if (aspect_mask & (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT))
	{
		clear_attachments[num_clear_attachments].aspectMask = aspect_mask & (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT);
		clear_attachments[num_clear_attachments].clearValue.depthStencil.depth = depth;
		clear_attachments[num_clear_attachments].clearValue.depthStencil.stencil = stencil;
		++num_clear_attachments;
	}

	assert(num_clear_attachments != 0);

	if (num_rects == 0)
	{
		VkClearRect clear_rect;
		clear_rect.rect = _current_render_area;
		clear_rect.baseArrayLayer = 0;
		clear_rect.layerCount = 1;

		vk.CmdClearAttachments(_orig, num_clear_attachments, clear_attachments, 1, &clear_rect);
	}
	else
	{
		const auto clear_rects = static_cast<VkClearRect *>(alloca(sizeof(VkClearRect) * num_rects));

		for (uint32_t i = 0, k = 0; i < num_rects; ++i, k += 4)
		{
			clear_rects[i].rect.offset.x = rects[k + 0];
			clear_rects[i].rect.offset.y = rects[k + 1];
			clear_rects[i].rect.extent.width = rects[k + 2] - rects[k + 0];
			clear_rects[i].rect.extent.height = rects[k + 3] - rects[k + 1];
			clear_rects[i].baseArrayLayer = 0;
			clear_rects[i].layerCount = 1;
		}

		vk.CmdClearAttachments(_orig, num_clear_attachments, clear_attachments, num_rects, clear_rects);
	}
}
void reshade::vulkan::command_list_impl::clear_depth_stencil_view(api::resource_view dsv, api::attachment_type clear_flags, float depth, uint8_t stencil, uint32_t num_rects, const int32_t *)
{
	_has_commands = true;

	assert(num_rects == 0);
	const resource_view_data dsv_data = _device_impl->lookup_resource_view(dsv);
	assert(dsv_data.is_image_view()); // Has to be an image

	// Transition state to VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL (since it will be in 'resource_usage::depth_stencil_write' at this point)
	VkImageMemoryBarrier transition { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
	transition.oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	transition.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	transition.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	transition.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	transition.image = dsv_data.image_create_info.image;
	transition.subresourceRange = { static_cast<VkImageAspectFlags>(clear_flags), 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS };
	vk.CmdPipelineBarrier(_orig, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &transition);

	const VkClearDepthStencilValue clear_value = { depth, stencil };
	vk.CmdClearDepthStencilImage(_orig, dsv_data.image_create_info.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clear_value, 1, &transition.subresourceRange);

	std::swap(transition.oldLayout, transition.newLayout);
	vk.CmdPipelineBarrier(_orig, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &transition);
}
void reshade::vulkan::command_list_impl::clear_render_target_view(api::resource_view rtv, const float color[4], uint32_t num_rects, const int32_t *)
{
	_has_commands = true;

	assert(num_rects == 0);
	const resource_view_data rtv_data = _device_impl->lookup_resource_view(rtv);
	assert(rtv_data.is_image_view()); // Has to be an image

	// Transition state to VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL (since it will be in 'resource_usage::render_target' at this point)
	VkImageMemoryBarrier transition { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
	transition.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	transition.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	transition.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	transition.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	transition.image = rtv_data.image_create_info.image;
	transition.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS };
	vk.CmdPipelineBarrier(_orig, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &transition);

	VkClearColorValue clear_value;
	std::memcpy(clear_value.float32, color, 4 * sizeof(float));

	vk.CmdClearColorImage(_orig, rtv_data.image_create_info.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clear_value, 1, &transition.subresourceRange);

	std::swap(transition.oldLayout, transition.newLayout);
	vk.CmdPipelineBarrier(_orig, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &transition);
}
void reshade::vulkan::command_list_impl::clear_unordered_access_view_uint(api::resource_view uav, const uint32_t values[4], uint32_t num_rects, const int32_t *)
{
	_has_commands = true;

	assert(num_rects == 0);
	const resource_view_data uav_data = _device_impl->lookup_resource_view(uav);
	assert(uav_data.is_image_view()); // Has to be an image

	const VkImageSubresourceRange range = { VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS };

	VkClearColorValue clear_value;
	std::memcpy(clear_value.uint32, values, 4 * sizeof(uint32_t));

	vk.CmdClearColorImage(_orig, uav_data.image_create_info.image, VK_IMAGE_LAYOUT_GENERAL, &clear_value, 1, &range);
}
void reshade::vulkan::command_list_impl::clear_unordered_access_view_float(api::resource_view uav, const float values[4], uint32_t num_rects, const int32_t *)
{
	_has_commands = true;

	assert(num_rects == 0);
	const resource_view_data &uav_data = _device_impl->lookup_resource_view(uav);
	assert(uav_data.is_image_view()); // Has to be an image

	const VkImageSubresourceRange range = { VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS };

	VkClearColorValue clear_value;
	std::memcpy(clear_value.float32, values, 4 * sizeof(float));

	vk.CmdClearColorImage(_orig, uav_data.image_create_info.image, VK_IMAGE_LAYOUT_GENERAL, &clear_value, 1, &range);
}

void reshade::vulkan::command_list_impl::generate_mipmaps(api::resource_view srv)
{
	assert(srv.handle != 0);
	const resource_view_data view_data = _device_impl->lookup_resource_view(srv);
	assert(view_data.is_image_view());
	const resource_data res_data = _device_impl->lookup_resource({ (uint64_t)view_data.image_create_info.image });

	VkImageMemoryBarrier barrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
	barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
	barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
	barrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = view_data.image_create_info.image;
	barrier.subresourceRange = view_data.image_create_info.subresourceRange;
	vk.CmdPipelineBarrier(_orig, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

	int32_t width = res_data.image_create_info.extent.width >> view_data.image_create_info.subresourceRange.baseMipLevel;
	int32_t height = res_data.image_create_info.extent.height >> view_data.image_create_info.subresourceRange.baseMipLevel;

	for (uint32_t level = view_data.image_create_info.subresourceRange.baseMipLevel + 1; level < res_data.image_create_info.mipLevels; ++level, width /= 2, height /= 2)
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
		blit.dstSubresource = { barrier.subresourceRange.aspectMask, level, barrier.subresourceRange.baseArrayLayer, barrier.subresourceRange.layerCount };
		vk.CmdBlitImage(_orig, view_data.image_create_info.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, view_data.image_create_info.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);

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
	barrier.subresourceRange = view_data.image_create_info.subresourceRange;
	vk.CmdPipelineBarrier(_orig, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}

void reshade::vulkan::command_list_impl::begin_query(api::query_pool pool, api::query_type, uint32_t index)
{
	_has_commands = true;

	assert(pool.handle != 0);

	vk.CmdResetQueryPool(_orig, (VkQueryPool)pool.handle, index, 1);
	vk.CmdBeginQuery(_orig, (VkQueryPool)pool.handle, index, 0);
}
void reshade::vulkan::command_list_impl::finish_query(api::query_pool pool, api::query_type type, uint32_t index)
{
	_has_commands = true;

	assert(pool.handle != 0);

	if (type == api::query_type::timestamp)
	{
		vk.CmdResetQueryPool(_orig, (VkQueryPool)pool.handle, index, 1);
		vk.CmdWriteTimestamp(_orig, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, (VkQueryPool)pool.handle, index);
	}
	else
	{
		vk.CmdEndQuery(_orig, (VkQueryPool)pool.handle, index);
	}
}
void reshade::vulkan::command_list_impl::copy_query_pool_results(api::query_pool pool, api::query_type, uint32_t first, uint32_t count, api::resource dst, uint64_t dst_offset, uint32_t stride)
{
	_has_commands = true;

	assert(pool.handle != 0);

	vk.CmdCopyQueryPoolResults(_orig, (VkQueryPool)pool.handle, first, count, (VkBuffer)dst.handle, dst_offset, stride, VK_QUERY_RESULT_64_BIT);
}

void reshade::vulkan::command_list_impl::begin_debug_event(const char *label, const float color[4])
{
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
void reshade::vulkan::command_list_impl::finish_debug_event()
{
	if (vk.CmdEndDebugUtilsLabelEXT == nullptr)
		return;

	vk.CmdEndDebugUtilsLabelEXT(_orig);
}
void reshade::vulkan::command_list_impl::insert_debug_marker(const char *label, const float color[4])
{
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
