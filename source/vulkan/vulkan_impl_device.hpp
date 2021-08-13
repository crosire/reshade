/*
 * Copyright (C) 2021 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include "addon_manager.hpp"
#include <mutex>
#include <unordered_map>

#pragma warning(push)
#pragma warning(disable: 4100 4127 4324 4703) // Disable a bunch of warnings thrown by VMA code
#include <vk_mem_alloc.h>
#pragma warning(pop)
#include <vk_layer_dispatch_table.h>

namespace reshade::vulkan
{
	struct resource_data
	{
		bool is_image() const { return type != VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO; }

		union
		{
			VkImage image;
			VkBuffer buffer;
		};
		union
		{
			VkStructureType type;
			VkImageCreateInfo image_create_info;
			VkBufferCreateInfo buffer_create_info;
		};

		VmaAllocation allocation;
	};

	struct resource_view_data
	{
		bool is_image_view() const { return type != VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO; }

		union
		{
			VkImageView image_view;
			VkBufferView buffer_view;
		};
		union
		{
			VkStructureType type;
			VkImageViewCreateInfo image_create_info;
			VkBufferViewCreateInfo buffer_create_info;
		};
	};

	struct render_pass_data
	{
		struct attachment
		{
			VkImageLayout initial_layout;
			VkImageAspectFlags clear_flags;
			VkImageAspectFlags format_flags;
		};

		std::vector<attachment> attachments;
	};

	struct framebuffer_data
	{
		VkExtent2D area;
		std::vector<VkImageView> attachments;
		std::vector<VkImageAspectFlags> attachment_types;
	};

	struct shader_module_data
	{
		std::vector<uint32_t> spirv;
	};

	struct pipeline_layout_data
	{
		std::vector<api::pipeline_layout_param> desc;
	};

	struct descriptor_set_layout_data
	{
		void calc_binding_from_offset(uint32_t offset, uint32_t &last_binding, uint32_t &array_offset) const
		{
			last_binding = 0;
			array_offset = 0;

			for (const auto [binding_offset, binding] : binding_to_offset)
			{
				if (offset < binding_offset || offset > binding_offset + array_offset)
					continue;

				last_binding = binding;
				array_offset = offset - binding_offset;
			}
		}
		uint32_t calc_offset_from_binding(uint32_t binding, uint32_t array_offset) const
		{
			return binding_to_offset.at(binding) + array_offset;
		}

		std::vector<api::descriptor_range> desc;
		std::unordered_map<uint32_t, uint32_t> binding_to_offset;
		bool push_descriptors;
	};

	class device_impl : public api::api_object_impl<VkDevice, api::device>
	{
		friend class command_list_impl;
		friend class command_queue_impl;

	public:
		device_impl(VkDevice device, VkPhysicalDevice physical_device, const VkLayerInstanceDispatchTable &instance_table, const VkLayerDispatchTable &device_table, const VkPhysicalDeviceFeatures &enabled_features);
		~device_impl();

		api::device_api get_api() const final { return api::device_api::vulkan; }

		bool check_capability(api::device_caps capability) const final;
		bool check_format_support(api::format format, api::resource_usage usage) const final;

		bool create_sampler(const api::sampler_desc &desc, api::sampler *out) final;
		void destroy_sampler(api::sampler handle) final;

		bool create_resource(const api::resource_desc &desc, const api::subresource_data *initial_data, api::resource_usage initial_state, api::resource *out) final;
		void destroy_resource(api::resource handle) final;

		bool create_resource_view(api::resource resource, api::resource_usage usage_type, const api::resource_view_desc &desc, api::resource_view *out) final;
		void destroy_resource_view(api::resource_view handle) final;

		bool create_pipeline(const api::pipeline_desc &desc, api::pipeline *out) final;
		bool create_compute_pipeline(const api::pipeline_desc &desc, api::pipeline *out);
		bool create_graphics_pipeline(const api::pipeline_desc &desc, api::pipeline *out);
		void destroy_pipeline(api::pipeline_stage type, api::pipeline handle) final;

		bool create_pipeline_layout(uint32_t count, const api::pipeline_layout_param *params, api::pipeline_layout *out) final;
		void destroy_pipeline_layout(api::pipeline_layout handle) final;

		bool create_descriptor_set_layout(uint32_t count, const api::descriptor_range *bindings, bool push_descriptors, api::descriptor_set_layout *out) final;
		void destroy_descriptor_set_layout(api::descriptor_set_layout handle) final;

		bool create_query_pool(api::query_type type, uint32_t size, api::query_pool *out) final;
		void destroy_query_pool(api::query_pool handle) final;

		bool create_render_pass(const api::render_pass_desc &desc, api::render_pass *out) final;
		void destroy_render_pass(api::render_pass handle) final;

		bool create_framebuffer(const api::framebuffer_desc &desc, api::framebuffer *out) final;
		void destroy_framebuffer(api::framebuffer handle) final;

		bool map_resource(api::resource resource, uint32_t subresource, api::map_access access, void **data, uint32_t *row_pitch, uint32_t *slice_pitch) final;
		void unmap_resource(api::resource resource, uint32_t subresource) final;

		void upload_buffer_region(const void *data, api::resource dst, uint64_t dst_offset, uint64_t size) final;
		void upload_texture_region(const api::subresource_data &data, api::resource dst, uint32_t dst_subresource, const int32_t dst_box[6]) final;

		bool get_query_pool_results(api::query_pool pool, uint32_t first, uint32_t count, void *results, uint32_t stride) final;

		bool allocate_descriptor_sets(uint32_t count, const api::descriptor_set_layout *layouts, api::descriptor_set *out) final;
		void free_descriptor_sets(uint32_t count, const api::descriptor_set_layout *layouts, const api::descriptor_set *sets) final;
		void update_descriptor_sets(uint32_t count, const api::write_descriptor_set *updates) final;

		void wait_idle() const final;

		void set_resource_name(api::resource resource, const char *name) final;

		void get_pipeline_layout_desc(api::pipeline_layout layout, uint32_t *count, api::pipeline_layout_param *params) const final;
		void get_descriptor_set_layout_desc(api::descriptor_set_layout layout, uint32_t *count, api::descriptor_range *bindings) const final;

		api::resource_desc get_resource_desc(api::resource resource) const final;
		void get_resource_from_view(api::resource_view view, api::resource *out) const final;
		bool get_framebuffer_attachment(api::framebuffer framebuffer, api::attachment_type type, uint32_t index, api::resource_view *out) const final;

		void advance_transient_descriptor_pool();

#if RESHADE_ADDON
		uint32_t get_subresource_index(VkImage image, const VkImageSubresourceLayers &layers, uint32_t layer = 0) const
		{
			return layers.mipLevel + (layers.baseArrayLayer + layer) * lookup_resource({ (uint64_t)image }).image_create_info.mipLevels;
		}

		api::resource_view get_default_view(VkImage image)
		{
			VkImageViewCreateInfo create_info{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
			create_info.image = image;
			register_image_view((VkImageView)image, create_info); // Register fake image view for this image
			return { (uint64_t)image };
		}
#endif
		resource_data lookup_resource(api::resource resource) const
		{
			assert(resource.handle != 0);
			const std::lock_guard<std::mutex> lock(_mutex);
			return _resources.at(resource.handle);
		}
		resource_view_data lookup_resource_view(api::resource_view view) const
		{
			assert(view.handle != 0);
			const std::lock_guard<std::mutex> lock(_mutex);
			return _views.at(view.handle);
		}
		render_pass_data lookup_render_pass(VkRenderPass pass) const
		{
			const std::lock_guard<std::mutex> lock(_mutex);
			return _render_pass_list.at(pass);
		}
		framebuffer_data lookup_framebuffer(VkFramebuffer fbo) const
		{
			const std::lock_guard<std::mutex> lock(_mutex);
			return _framebuffer_list.at(fbo);
		}
		pipeline_layout_data lookup_pipeline_layout(VkPipelineLayout layout) const
		{
			const std::lock_guard<std::mutex> lock(_mutex);
			return _pipeline_layout_list.at(layout);
		}
		descriptor_set_layout_data lookup_descriptor_set(VkDescriptorSet set) const
		{
			const std::lock_guard<std::mutex> lock(_mutex);
			return _descriptor_set_layout_list.at((VkDescriptorSetLayout)set);
		}
		descriptor_set_layout_data lookup_descriptor_set_layout(VkDescriptorSetLayout layout) const
		{
			const std::lock_guard<std::mutex> lock(_mutex);
			return _descriptor_set_layout_list.at(layout);
		}

		void register_image(VkImage image, const VkImageCreateInfo &create_info, VmaAllocation allocation = VK_NULL_HANDLE)
		{
			resource_data data;
			data.image = image;
			data.image_create_info = create_info;
			data.allocation = allocation;

			const std::lock_guard<std::mutex> lock(_mutex);
			_resources.emplace((uint64_t)image, std::move(data));
		}
		void register_image_view(VkImageView image_view, const VkImageViewCreateInfo &create_info)
		{
			resource_view_data data;
			data.image_view = image_view;
			data.image_create_info = create_info;

			const std::lock_guard<std::mutex> lock(_mutex);
			_views.emplace((uint64_t)image_view, std::move(data));
		}
		void register_buffer(VkBuffer buffer, const VkBufferCreateInfo &create_info, VmaAllocation allocation = VK_NULL_HANDLE)
		{
			resource_data data;
			data.buffer = buffer;
			data.buffer_create_info = create_info;
			data.allocation = allocation;

			const std::lock_guard<std::mutex> lock(_mutex);
			_resources.emplace((uint64_t)buffer, std::move(data));
		}
		void register_buffer_view(VkBufferView buffer_view, const VkBufferViewCreateInfo &create_info)
		{
			resource_view_data data;
			data.buffer_view = buffer_view;
			data.buffer_create_info = create_info;

			const std::lock_guard<std::mutex> lock(_mutex);
			_views.emplace((uint64_t)buffer_view, std::move(data));
		}
		void register_render_pass(VkRenderPass pass, render_pass_data &&data)
		{
			const std::lock_guard<std::mutex> lock(_mutex);
			_render_pass_list.emplace(pass, std::move(data));
		}
		void register_framebuffer(VkFramebuffer fbo, framebuffer_data &&data)
		{
			const std::lock_guard<std::mutex> lock(_mutex);
			_framebuffer_list.emplace(fbo, std::move(data));
		}
		void register_shader_module(VkShaderModule module, shader_module_data &&data)
		{
			const std::lock_guard<std::mutex> lock(_mutex);
			_shader_module_list.emplace(module, std::move(data));
		}
		void register_pipeline_layout(VkPipelineLayout layout, pipeline_layout_data &&data)
		{
			const std::lock_guard<std::mutex> lock(_mutex);
			_pipeline_layout_list.emplace(layout, std::move(data));
		}
		void register_descriptor_set(VkDescriptorSet set, VkDescriptorSetLayout layout)
		{
			const std::lock_guard<std::mutex> lock(_mutex);
			_descriptor_set_layout_list.emplace((VkDescriptorSetLayout)set, _descriptor_set_layout_list.at(layout));
		}
		void register_descriptor_set_layout(VkDescriptorSetLayout layout, descriptor_set_layout_data &&data)
		{
			const std::lock_guard<std::mutex> lock(_mutex);
			_descriptor_set_layout_list.emplace(layout, std::move(data));
		}

		void unregister_image(VkImage image)
		{
			const std::lock_guard<std::mutex> lock(_mutex);
			_resources.erase((uint64_t)image);
		}
		void unregister_image_view(VkImageView image_view)
		{
			const std::lock_guard<std::mutex> lock(_mutex);
			_views.erase((uint64_t)image_view);
		}
		void unregister_buffer(VkBuffer buffer)
		{
			const std::lock_guard<std::mutex> lock(_mutex);
			_resources.erase((uint64_t)buffer);
		}
		void unregister_buffer_view(VkBufferView buffer_view)
		{
			const std::lock_guard<std::mutex> lock(_mutex);
			_views.erase((uint64_t)buffer_view);
		}
		void unregister_render_pass(VkRenderPass pass)
		{
			const std::lock_guard<std::mutex> lock(_mutex);
			_render_pass_list.erase(pass);
		}
		void unregister_framebuffer(VkFramebuffer fbo)
		{
			const std::lock_guard<std::mutex> lock(_mutex);
			_framebuffer_list.erase(fbo);
		}
		void unregister_shader_module(VkShaderModule module)
		{
			const std::lock_guard<std::mutex> lock(_mutex);
			_shader_module_list.erase(module);
		}
		void unregister_pipeline_layout(VkPipelineLayout layout)
		{
			const std::lock_guard<std::mutex> lock(_mutex);
			_pipeline_layout_list.erase(layout);
		}
		void unregister_descriptor_set(VkDescriptorSet set)
		{
			const std::lock_guard<std::mutex> lock(_mutex);
			_descriptor_set_layout_list.erase((VkDescriptorSetLayout)set);
		}
		void unregister_descriptor_set_layout(VkDescriptorSetLayout layout)
		{
			const std::lock_guard<std::mutex> lock(_mutex);
			_descriptor_set_layout_list.erase(layout);
		}

		api::pipeline_desc convert_pipeline_desc(const VkComputePipelineCreateInfo &create_info) const;
		api::pipeline_desc convert_pipeline_desc(const VkGraphicsPipelineCreateInfo &create_info) const;

		const VkPhysicalDevice _physical_device;
		const VkLayerDispatchTable _dispatch_table;
		const VkLayerInstanceDispatchTable _instance_dispatch_table;

		uint32_t _graphics_queue_family_index = std::numeric_limits<uint32_t>::max();
		std::vector<command_queue_impl *> _queues;
		VkPhysicalDeviceFeatures _enabled_features = {};

#ifndef NDEBUG
		mutable bool _wait_for_idle_happened = false;
#endif

	private:
		bool create_shader_module(VkShaderStageFlagBits stage, const api::shader_desc &desc, VkPipelineShaderStageCreateInfo &stage_info, VkSpecializationInfo &spec_info, std::vector<VkSpecializationMapEntry> &spec_map);

		mutable std::mutex _mutex;

		VmaAllocator _alloc = nullptr;
		std::unordered_map<uint64_t, resource_data> _resources;
		std::unordered_map<uint64_t, resource_view_data> _views;

		std::unordered_map<VkRenderPass, render_pass_data> _render_pass_list;
		std::unordered_map<VkFramebuffer, framebuffer_data> _framebuffer_list;
		std::unordered_map<VkShaderModule, shader_module_data> _shader_module_list;
		std::unordered_map<VkPipelineLayout, pipeline_layout_data> _pipeline_layout_list;
		std::unordered_map<VkDescriptorSetLayout, descriptor_set_layout_data> _descriptor_set_layout_list;

		VkDescriptorPool _descriptor_pool = VK_NULL_HANDLE;
		VkDescriptorPool _transient_descriptor_pool[4] = {};
		uint32_t _transient_index = 0;
	};
}
