/*
 * Copyright (C) 2021 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include "addon_manager.hpp"
#pragma warning(push)
#pragma warning(disable: 4100 4127 4324 4703) // Disable a bunch of warnings thrown by VMA code
#include <vk_mem_alloc.h>
#pragma warning(pop)
#include <vk_layer_dispatch_table.h>

namespace reshade::vulkan
{
	template <VkObjectType type> struct object_data;

	class device_impl : public api::api_object_impl<VkDevice, api::device>
	{
		friend class command_list_impl;
		friend class command_queue_impl;

	public:
		device_impl(
			VkDevice device,
			VkPhysicalDevice physical_device,
			const VkLayerInstanceDispatchTable &instance_table, const VkLayerDispatchTable &device_table, const VkPhysicalDeviceFeatures &enabled_features,
			bool custom_border_color_ext,
			bool extended_dynamic_state_ext);
		~device_impl();

		api::device_api get_api() const final { return api::device_api::vulkan; }

		bool check_capability(api::device_caps capability) const final;
		bool check_format_support(api::format format, api::resource_usage usage) const final;

		bool create_sampler(const api::sampler_desc &desc, api::sampler *out_handle) final;
		void destroy_sampler(api::sampler handle) final;

		bool create_resource(const api::resource_desc &desc, const api::subresource_data *initial_data, api::resource_usage initial_state, api::resource *out_handle) final;
		void destroy_resource(api::resource handle) final;

		api::resource_desc get_resource_desc(api::resource resource) const final;
		void set_resource_name(api::resource handle, const char *name) final;

		bool create_resource_view(api::resource resource, api::resource_usage usage_type, const api::resource_view_desc &desc, api::resource_view *out_handle) final;
		void destroy_resource_view(api::resource_view handle) final;

		api::resource get_resource_from_view(api::resource_view view) const final;
		api::resource_view_desc get_resource_view_desc(api::resource_view view) const final;
		void set_resource_view_name(api::resource_view handle, const char *name) final;

		bool create_pipeline(const api::pipeline_desc &desc, uint32_t dynamic_state_count, const api::dynamic_state *dynamic_states, api::pipeline *out_handle) final;
		bool create_compute_pipeline(const api::pipeline_desc &desc, api::pipeline *out_handle);
		bool create_graphics_pipeline(const api::pipeline_desc &desc, uint32_t dynamic_state_count, const api::dynamic_state *dynamic_states, api::pipeline *out_handle);
		void destroy_pipeline(api::pipeline handle) final;

		bool create_render_pass(const api::render_pass_desc &desc, api::render_pass *out_handle) final;
		void destroy_render_pass(api::render_pass handle) final;

		bool create_framebuffer(const api::framebuffer_desc &desc, api::framebuffer *out_handle) final;
		void destroy_framebuffer(api::framebuffer handle) final;

		bool create_pipeline_layout(uint32_t param_count, const api::pipeline_layout_param *params, api::pipeline_layout *out_handle) final;
		void destroy_pipeline_layout(api::pipeline_layout handle) final;

		bool create_descriptor_set_layout(uint32_t range_count, const api::descriptor_range *ranges, bool push_descriptors, api::descriptor_set_layout *out_handle) final;
		void destroy_descriptor_set_layout(api::descriptor_set_layout handle) final;

		bool create_query_pool(api::query_type type, uint32_t size, api::query_pool *out_handle) final;
		void destroy_query_pool(api::query_pool handle) final;

		bool create_descriptor_sets(uint32_t count, const api::descriptor_set_layout *layouts, api::descriptor_set *out_sets) final;
		void destroy_descriptor_sets(uint32_t count, const api::descriptor_set *sets) final;

		bool map_buffer_region(api::resource resource, uint64_t offset, uint64_t size, api::map_access access, void **out_data) final;
		void unmap_buffer_region(api::resource resource) final;
		bool map_texture_region(api::resource resource, uint32_t subresource, const int32_t box[6], api::map_access access, api::subresource_data *out_data) final;
		void unmap_texture_region(api::resource resource, uint32_t subresource) final;

		void update_buffer_region(const void *data, api::resource resource, uint64_t offset, uint64_t size) final;
		void update_texture_region(const api::subresource_data &data, api::resource resource, uint32_t subresource, const int32_t box[6]) final;

		void update_descriptor_sets(uint32_t count, const api::descriptor_set_update *updates) final;

		bool get_query_pool_results(api::query_pool pool, uint32_t first, uint32_t count, void *results, uint32_t stride) final;

		void wait_idle() const final;

		void get_pipeline_layout_desc(api::pipeline_layout layout, uint32_t *out_count, api::pipeline_layout_param *out_params) const final;

		void get_descriptor_pool_offset(api::descriptor_set set, api::descriptor_pool *out_pool, uint32_t *out_offset) const final;

		void get_descriptor_set_layout_desc(api::descriptor_set_layout layout, uint32_t *out_count, api::descriptor_range *out_ranges) const final;

		api::resource_view get_framebuffer_attachment(api::framebuffer framebuffer, api::attachment_type type, uint32_t index) const final;

		void advance_transient_descriptor_pool();

		api::pipeline_desc convert_pipeline_desc(const VkComputePipelineCreateInfo &create_info) const;
		api::pipeline_desc convert_pipeline_desc(const VkGraphicsPipelineCreateInfo &create_info) const;

		template <VkObjectType type, typename... Args>
		void register_object(typename object_data<type>::Handle object, Args... args)
		{
			assert(object != VK_NULL_HANDLE);
			uint64_t private_data = reinterpret_cast<uint64_t>(new object_data<type>(std::forward<Args>(args)...));
			_dispatch_table.SetPrivateDataEXT(_orig, type, (uint64_t)object, _private_data_slot, private_data);
		}
		void register_object(VkObjectType type, uint64_t object, void *private_data)
		{
			_dispatch_table.SetPrivateDataEXT(_orig, type, object, _private_data_slot, reinterpret_cast<uint64_t>(private_data));
		}

		template <VkObjectType type>
		void unregister_object(typename object_data<type>::Handle object)
		{
			if (object == VK_NULL_HANDLE)
				return;

			uint64_t private_data = 0;
			_dispatch_table.GetPrivateDataEXT(_orig, type, (uint64_t)object, _private_data_slot, &private_data);
			delete reinterpret_cast<object_data<type> *>(private_data);
			_dispatch_table.SetPrivateDataEXT(_orig, type, (uint64_t)object, _private_data_slot, 0);
		}
		void unregister_object(VkObjectType type, uint64_t object)
		{
			_dispatch_table.SetPrivateDataEXT(_orig, type, object, _private_data_slot, 0);
		}

		template <VkObjectType type>
		__forceinline object_data<type> *get_user_data_for_object(typename object_data<type>::Handle object) const
		{
			assert(object != VK_NULL_HANDLE);
			uint64_t private_data = 0;
			_dispatch_table.GetPrivateDataEXT(_orig, type, (uint64_t)object, _private_data_slot, &private_data);
			assert(private_data != 0);
			return reinterpret_cast<object_data<type> *>(private_data);
		}

		const VkPhysicalDevice _physical_device;
		const VkLayerDispatchTable _dispatch_table;
		const VkLayerInstanceDispatchTable _instance_dispatch_table;

		uint32_t _graphics_queue_family_index = std::numeric_limits<uint32_t>::max();
		std::vector<command_queue_impl *> _queues;

		const bool _custom_border_color_ext = false;
		const bool _extended_dynamic_state_ext = false;
		const VkPhysicalDeviceFeatures _enabled_features = {};

#ifndef NDEBUG
		mutable bool _wait_for_idle_happened = false;
#endif

	private:
		bool create_shader_module(VkShaderStageFlagBits stage, const api::shader_desc &desc, VkPipelineShaderStageCreateInfo &stage_info, VkSpecializationInfo &spec_info, std::vector<VkSpecializationMapEntry> &spec_map);

		VmaAllocator _alloc = nullptr;
		VkDescriptorPool _descriptor_pool = VK_NULL_HANDLE;
		VkDescriptorPool _transient_descriptor_pool[4] = {};
		uint32_t _transient_index = 0;
		VkPrivateDataSlotEXT _private_data_slot = VK_NULL_HANDLE;
	};
}
