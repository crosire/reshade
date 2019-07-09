/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include <vulkan/vulkan.h>

struct vk_device_table
{
	PFN_vkQueueWaitIdle vkQueueWaitIdle;
	PFN_vkDeviceWaitIdle vkDeviceWaitIdle;

	PFN_vkGetDeviceQueue vkGetDeviceQueue;
	PFN_vkGetSwapchainImagesKHR vkGetSwapchainImagesKHR;
	PFN_vkGetPhysicalDeviceMemoryProperties vkGetPhysicalDeviceMemoryProperties;

	PFN_vkQueueSubmit vkQueueSubmit;

	PFN_vkCreateFence vkCreateFence;
	PFN_vkDestroyFence vkDestroyFence;
	PFN_vkWaitForFences vkWaitForFences;
	PFN_vkResetFences vkResetFences;

	PFN_vkMapMemory vkMapMemory;
	PFN_vkFreeMemory vkFreeMemory;
	PFN_vkUnmapMemory vkUnmapMemory;
	PFN_vkAllocateMemory vkAllocateMemory;

	PFN_vkCreateImage vkCreateImage;
	PFN_vkDestroyImage vkDestroyImage;
	PFN_vkGetImageSubresourceLayout vkGetImageSubresourceLayout;
	PFN_vkGetImageMemoryRequirements vkGetImageMemoryRequirements;
	PFN_vkBindImageMemory vkBindImageMemory;

	PFN_vkCreateBuffer vkCreateBuffer;
	PFN_vkDestroyBuffer vkDestroyBuffer;
	PFN_vkGetBufferMemoryRequirements vkGetBufferMemoryRequirements;
	PFN_vkBindBufferMemory vkBindBufferMemory;
	PFN_vkBindBufferMemory2 vkBindBufferMemory2;

	PFN_vkCreateImageView vkCreateImageView;
	PFN_vkDestroyImageView vkDestroyImageView;

	PFN_vkCreateRenderPass vkCreateRenderPass;
	PFN_vkDestroyRenderPass vkDestroyRenderPass;

	PFN_vkCreateFramebuffer vkCreateFramebuffer;
	PFN_vkDestroyFramebuffer vkDestroyFramebuffer;

	PFN_vkResetCommandPool vkResetCommandPool;
	PFN_vkCreateCommandPool vkCreateCommandPool;
	PFN_vkDestroyCommandPool vkDestroyCommandPool;
	PFN_vkAllocateCommandBuffers vkAllocateCommandBuffers;
	PFN_vkEndCommandBuffer vkEndCommandBuffer;
	PFN_vkBeginCommandBuffer vkBeginCommandBuffer;

	PFN_vkResetDescriptorPool vkResetDescriptorPool;
	PFN_vkCreateDescriptorPool vkCreateDescriptorPool;
	PFN_vkDestroyDescriptorPool vkDestroyDescriptorPool;
	PFN_vkAllocateDescriptorSets vkAllocateDescriptorSets;
	PFN_vkUpdateDescriptorSets vkUpdateDescriptorSets;

	PFN_vkCreateDescriptorSetLayout vkCreateDescriptorSetLayout;
	PFN_vkDestroyDescriptorSetLayout vkDestroyDescriptorSetLayout;

	PFN_vkCreateGraphicsPipelines vkCreateGraphicsPipelines;
	PFN_vkDestroyPipeline vkDestroyPipeline;

	PFN_vkCreatePipelineLayout vkCreatePipelineLayout;
	PFN_vkDestroyPipelineLayout vkDestroyPipelineLayout;

	PFN_vkCreateSampler vkCreateSampler;
	PFN_vkDestroySampler vkDestroySampler;

	PFN_vkCreateShaderModule vkCreateShaderModule;
	PFN_vkDestroyShaderModule vkDestroyShaderModule;

	PFN_vkCmdBlitImage vkCmdBlitImage;
	PFN_vkCmdCopyImage vkCmdCopyImage;
	PFN_vkCmdCopyBufferToImage vkCmdCopyBufferToImage;
	PFN_vkCmdUpdateBuffer vkCmdUpdateBuffer;
	PFN_vkCmdBindDescriptorSets vkCmdBindDescriptorSets;
	PFN_vkCmdPushConstants vkCmdPushConstants;
	PFN_vkCmdPushDescriptorSetKHR vkCmdPushDescriptorSetKHR;
	PFN_vkCmdClearDepthStencilImage vkCmdClearDepthStencilImage;
	PFN_vkCmdEndRenderPass vkCmdEndRenderPass;
	PFN_vkCmdBeginRenderPass vkCmdBeginRenderPass;
	PFN_vkCmdBindPipeline vkCmdBindPipeline;
	PFN_vkCmdBindIndexBuffer vkCmdBindIndexBuffer;
	PFN_vkCmdBindVertexBuffers vkCmdBindVertexBuffers;
	PFN_vkCmdSetScissor vkCmdSetScissor;
	PFN_vkCmdSetViewport vkCmdSetViewport;
	PFN_vkCmdDraw vkCmdDraw;
	PFN_vkCmdDrawIndexed vkCmdDrawIndexed;
	PFN_vkCmdPipelineBarrier vkCmdPipelineBarrier;

	PFN_vkDebugMarkerSetObjectNameEXT vkDebugMarkerSetObjectNameEXT;
};
