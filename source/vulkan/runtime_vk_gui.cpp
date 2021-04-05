/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#if RESHADE_GUI

#include "dll_resources.hpp"
#include "runtime_vk.hpp"
#include "runtime_vk_objects.hpp"
#include <imgui.h>

#define vk _device_impl->_dispatch_table

bool reshade::vulkan::runtime_impl::init_imgui_resources()
{
	if (_imgui.sampler == VK_NULL_HANDLE)
	{
		VkSamplerCreateInfo create_info { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
		create_info.magFilter = VK_FILTER_LINEAR;
		create_info.minFilter = VK_FILTER_LINEAR;
		create_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		create_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		create_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		create_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		create_info.minLod = -1000;
		create_info.maxLod = +1000;

		if (vk.CreateSampler(_device, &create_info, nullptr, &_imgui.sampler) != VK_SUCCESS)
			return false;
	}

	// Only need to allocate descriptors when push descriptors are not supported
	if (vk.CmdPushDescriptorSetKHR == nullptr && _imgui.descriptor_pool == VK_NULL_HANDLE)
	{
		VkDescriptorPoolSize pool_sizes[] = {
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, MAX_IMAGE_DESCRIPTOR_SETS } // Single image per set
		};

		VkDescriptorPoolCreateInfo create_info { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
		create_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
		create_info.maxSets = MAX_IMAGE_DESCRIPTOR_SETS;
		create_info.poolSizeCount = static_cast<uint32_t>(std::size(pool_sizes));
		create_info.pPoolSizes = pool_sizes;

		if (vk.CreateDescriptorPool(_device, &create_info, nullptr, &_imgui.descriptor_pool) != VK_SUCCESS)
			return false;
	}

	if (_imgui.descriptor_layout == VK_NULL_HANDLE)
	{
		VkDescriptorSetLayoutBinding bindings[1] = {};
		bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		bindings[0].descriptorCount = 1;
		bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		bindings[0].pImmutableSamplers = &_imgui.sampler;

		VkDescriptorSetLayoutCreateInfo create_info { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
		if (vk.CmdPushDescriptorSetKHR != nullptr)
			create_info.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR;
		create_info.bindingCount = static_cast<uint32_t>(std::size(bindings));
		create_info.pBindings = bindings;

		if (vk.CreateDescriptorSetLayout(_device, &create_info, nullptr, &_imgui.descriptor_layout) != VK_SUCCESS)
			return false;
	}

	if (_imgui.pipeline_layout == VK_NULL_HANDLE)
	{
		const VkPushConstantRange push_constants[] = {
			{ VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(float) * 4 }
		};
		const VkDescriptorSetLayout descriptor_layouts[] = {
			_imgui.descriptor_layout
		};

		VkPipelineLayoutCreateInfo create_info { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
		create_info.setLayoutCount = static_cast<uint32_t>(std::size(descriptor_layouts));
		create_info.pSetLayouts = descriptor_layouts;
		create_info.pushConstantRangeCount = static_cast<uint32_t>(std::size(push_constants));
		create_info.pPushConstantRanges = push_constants;

		if (vk.CreatePipelineLayout(_device, &create_info, nullptr, &_imgui.pipeline_layout) != VK_SUCCESS)
			return false;
	}

	if (_imgui.pipeline != VK_NULL_HANDLE)
		return true;

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
		if (vk.CreateShaderModule(_device, &create_info, nullptr, &stages[0].module) != VK_SUCCESS)
		{
			return false;
		}

		const resources::data_resource ps = resources::load_data_resource(IDR_IMGUI_PS_SPIRV);
		create_info.codeSize = ps.data_size;
		create_info.pCode = static_cast<const uint32_t *>(ps.data);
		if (vk.CreateShaderModule(_device, &create_info, nullptr, &stages[1].module) != VK_SUCCESS)
		{
			vk.DestroyShaderModule(_device, stages[0].module, nullptr);
			return false;
		}
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

	VkPipelineVertexInputStateCreateInfo vertex_info { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
	vertex_info.vertexBindingDescriptionCount = static_cast<uint32_t>(std::size(binding_desc));
	vertex_info.pVertexBindingDescriptions = binding_desc;
	vertex_info.vertexAttributeDescriptionCount = static_cast<uint32_t>(std::size(attribute_desc));
	vertex_info.pVertexAttributeDescriptions = attribute_desc;

	VkPipelineInputAssemblyStateCreateInfo ia_info { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
	ia_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

	VkPipelineViewportStateCreateInfo viewport_info { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
	viewport_info.viewportCount = 1;
	viewport_info.scissorCount = 1;

	VkPipelineRasterizationStateCreateInfo raster_info { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
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

	VkDynamicState dynamic_states[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
	VkPipelineDynamicStateCreateInfo dynamic_state { VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
	dynamic_state.dynamicStateCount = static_cast<uint32_t>(std::size(dynamic_states));
	dynamic_state.pDynamicStates = dynamic_states;

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
	create_info.pDynamicState = &dynamic_state;
	create_info.layout = _imgui.pipeline_layout;
	create_info.renderPass = _default_render_pass[0];

	const VkResult res = vk.CreateGraphicsPipelines(_device, VK_NULL_HANDLE, 1, &create_info, nullptr, &_imgui.pipeline);

	vk.DestroyShaderModule(_device, stages[0].module, nullptr);
	vk.DestroyShaderModule(_device, stages[1].module, nullptr);

	return res == VK_SUCCESS;
}

void reshade::vulkan::runtime_impl::render_imgui_draw_data(ImDrawData *draw_data)
{
	// Need to multi-buffer vertex data so not to modify data below when the previous frame is still in flight
	const unsigned int buffer_index = _framecount % NUM_IMGUI_BUFFERS;

	// Create and grow vertex/index buffers if needed
	if (_imgui.num_indices[buffer_index] < draw_data->TotalIdxCount)
	{
		wait_for_command_buffers(); // Be safe and ensure nothing still uses this buffer

		_device_impl->destroy_resource(reinterpret_cast<api::resource_handle &>(_imgui.indices[buffer_index]));

		const int new_size = draw_data->TotalIdxCount + 10000;
		if (!_device_impl->create_resource(
			{ new_size * sizeof(ImDrawIdx), api::resource_usage::index_buffer, api::memory_usage::cpu_to_gpu },
			api::resource_usage::undefined, reinterpret_cast<api::resource_handle *>(&_imgui.indices[buffer_index])))
			return;
		set_debug_name_buffer(_imgui.indices[buffer_index], "ImGui index buffer");
		_imgui.num_indices[buffer_index] = new_size;
	}
	if (_imgui.num_vertices[buffer_index] < draw_data->TotalVtxCount)
	{
		wait_for_command_buffers();

		_device_impl->destroy_resource(reinterpret_cast<api::resource_handle &>(_imgui.vertices[buffer_index]));

		const int new_size = draw_data->TotalVtxCount + 5000;
		if (!_device_impl->create_resource(
			{ new_size * sizeof(ImDrawVert), api::resource_usage::vertex_buffer, api::memory_usage::cpu_to_gpu },
			api::resource_usage::undefined, reinterpret_cast<api::resource_handle *>(&_imgui.vertices[buffer_index])))
			return;
		set_debug_name_buffer(_imgui.vertices[buffer_index], "ImGui vertex buffer");
		_imgui.num_vertices[buffer_index] = new_size;
	}

	const VmaAllocation indices_mem = _device_impl->_resources.at((uint64_t)_imgui.indices[buffer_index]).allocation;
	const VmaAllocation vertices_mem = _device_impl->_resources.at((uint64_t)_imgui.vertices[buffer_index]).allocation;

	if (ImDrawIdx *idx_dst;
		vmaMapMemory(_device_impl->_alloc, indices_mem, reinterpret_cast<void **>(&idx_dst)) == VK_SUCCESS)
	{
		for (int n = 0; n < draw_data->CmdListsCount; ++n)
		{
			const ImDrawList *const draw_list = draw_data->CmdLists[n];
			std::memcpy(idx_dst, draw_list->IdxBuffer.Data, draw_list->IdxBuffer.Size * sizeof(ImDrawIdx));
			idx_dst += draw_list->IdxBuffer.Size;
		}

		vmaUnmapMemory(_device_impl->_alloc, indices_mem);
	}
	if (ImDrawVert *vtx_dst;
		vmaMapMemory(_device_impl->_alloc, vertices_mem, reinterpret_cast<void **>(&vtx_dst)) == VK_SUCCESS)
	{
		for (int n = 0; n < draw_data->CmdListsCount; ++n)
		{
			const ImDrawList *const draw_list = draw_data->CmdLists[n];
			std::memcpy(vtx_dst, draw_list->VtxBuffer.Data, draw_list->VtxBuffer.Size * sizeof(ImDrawVert));
			vtx_dst += draw_list->VtxBuffer.Size;
		}

		vmaUnmapMemory(_device_impl->_alloc, vertices_mem);
	}

	const VkCommandBuffer cmd_list = _cmd_impl->begin_commands();

	{   VkRenderPassBeginInfo begin_info { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
		begin_info.renderPass = _default_render_pass[0];
		begin_info.framebuffer = _swapchain_frames[_swap_index * 2];
		begin_info.renderArea.extent = _render_area;
		vk.CmdBeginRenderPass(cmd_list, &begin_info, VK_SUBPASS_CONTENTS_INLINE);
	}

	// Setup render state
	vk.CmdBindPipeline(cmd_list, VK_PIPELINE_BIND_POINT_GRAPHICS, _imgui.pipeline);

	// Setup orthographic projection matrix
	const float scale[2] = {
		2.0f / draw_data->DisplaySize.x,
		2.0f / draw_data->DisplaySize.y
	};
	const float translate[2] = {
		-1.0f - draw_data->DisplayPos.x * scale[0],
		-1.0f - draw_data->DisplayPos.y * scale[1]
	};
	vk.CmdPushConstants(cmd_list, _imgui.pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, sizeof(float) * 0, sizeof(float) * 2, scale);
	vk.CmdPushConstants(cmd_list, _imgui.pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, sizeof(float) * 2, sizeof(float) * 2, translate);

	VkDeviceSize vertex_offset = 0;
	vk.CmdBindIndexBuffer(cmd_list, _imgui.indices[buffer_index], 0, sizeof(ImDrawIdx) == 2 ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32);
	vk.CmdBindVertexBuffers(cmd_list, 0, 1, &_imgui.vertices[buffer_index], &vertex_offset);

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

			const auto tex_impl = static_cast<const tex_data *>(cmd.TextureId);

			// Use push descriptor extension when available
			if (vk.CmdPushDescriptorSetKHR != nullptr)
			{
				VkWriteDescriptorSet write { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
				write.dstBinding = 0;
				write.descriptorCount = 1;
				write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
				const VkDescriptorImageInfo image_info { _imgui.sampler, tex_impl->view[0], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
				write.pImageInfo = &image_info;
				vk.CmdPushDescriptorSetKHR(cmd_list, VK_PIPELINE_BIND_POINT_GRAPHICS, _imgui.pipeline_layout, 0, 1, &write);
			}
			else
			{
				assert(tex_impl->descriptor_set != VK_NULL_HANDLE);
				vk.CmdBindDescriptorSets(cmd_list, VK_PIPELINE_BIND_POINT_GRAPHICS, _imgui.pipeline_layout, 0, 1, &tex_impl->descriptor_set, 0, nullptr);
			}

			vk.CmdDrawIndexed(cmd_list, cmd.ElemCount, 1, cmd.IdxOffset + idx_offset, cmd.VtxOffset + vtx_offset, 0);

		}

		idx_offset += draw_list->IdxBuffer.Size;
		vtx_offset += draw_list->VtxBuffer.Size;
	}

	vk.CmdEndRenderPass(cmd_list);
}

#endif
