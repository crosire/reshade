/*
 * Copyright (C) 2021 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include "addon_manager.hpp"
#include "lockfree_table.hpp"

#pragma warning(push)
#pragma warning(disable: 4100 4127 4324 4703) // Disable a bunch of warnings thrown by VMA code
#include <vk_mem_alloc.h>
#pragma warning(pop)
#include <vk_layer_dispatch_table.h>

namespace reshade::vulkan
{
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

		bool map_resource(api::resource resource, uint32_t subresource, api::map_access access, api::subresource_data *out_data) final;
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

		template <typename T>
		void register_object(uint64_t object, T &&data)
		{
			static_cast<concurrent_hash_table<uint64_t, T> &>(_objects).emplace(object, std::move(data));
		}

		template <typename T>
		void unregister_object(uint64_t object)
		{
			static_cast<concurrent_hash_table<uint64_t, T> &>(_objects).erase(object);
		}

		template <typename T>
		__forceinline T get_native_object_data(uint64_t object) const
		{
			return static_cast<const concurrent_hash_table<uint64_t, T> &>(_objects).at(object);
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

		VmaAllocator _alloc = nullptr;

		VkDescriptorPool _descriptor_pool = VK_NULL_HANDLE;
		VkDescriptorPool _transient_descriptor_pool[4] = {};
		uint32_t _transient_index = 0;

		class :
			public concurrent_hash_table<uint64_t, struct resource_data>,
			public concurrent_hash_table<uint64_t, struct resource_view_data>,
			public concurrent_hash_table<uint64_t, struct render_pass_data>,
			public concurrent_hash_table<uint64_t, struct framebuffer_data>,
			public concurrent_hash_table<uint64_t, struct shader_module_data>,
			public concurrent_hash_table<uint64_t, struct pipeline_layout_data>,
			public concurrent_hash_table<uint64_t, struct descriptor_set_data>,
			public concurrent_hash_table<uint64_t, struct descriptor_set_layout_data>
		{} _objects;
	};
}
