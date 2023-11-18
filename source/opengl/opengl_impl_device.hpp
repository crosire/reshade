/*
 * Copyright (C) 2021 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <GL/gl3w.h>
#include "reshade_api_object_impl.hpp"
#include <unordered_map>

namespace reshade::opengl
{
	class device_impl : public api::api_object_impl<HGLRC, api::device>
	{
		friend class render_context_impl;

	public:
		device_impl(HDC initial_hdc, HGLRC shared_hglrc, bool compatibility_context = false);
		~device_impl();

		api::device_api get_api() const final { return api::device_api::opengl; }

		bool get_compatibility_context() const { return _compatibility_context; }

		api::device_properties get_properties() const;

		bool check_capability(api::device_caps capability) const final;
		bool check_format_support(api::format format, api::resource_usage usage) const final;

		bool create_sampler(const api::sampler_desc &desc, api::sampler *out_handle) final;
		void destroy_sampler(api::sampler handle) final;

		bool create_resource(const api::resource_desc &desc, const api::subresource_data *initial_data, api::resource_usage initial_state, api::resource *out_handle, HANDLE *shared_handle = nullptr) final;
		void destroy_resource(api::resource handle) final;

		api::resource_desc get_resource_desc(api::resource resource) const final;

		bool create_resource_view(api::resource resource, api::resource_usage usage_type, const api::resource_view_desc &desc, api::resource_view *out_handle) final;
		void destroy_resource_view(api::resource_view handle) override;

		api::format get_resource_format(GLenum target, GLenum object) const;

		api::resource get_resource_from_view(api::resource_view view) const final;
		api::resource_view_desc get_resource_view_desc(api::resource_view view) const final;

		bool map_buffer_region(api::resource resource, uint64_t offset, uint64_t size, api::map_access access, void **out_data) final;
		void unmap_buffer_region(api::resource resource) final;
		bool map_texture_region(api::resource resource, uint32_t subresource, const api::subresource_box *box, api::map_access access, api::subresource_data *out_data) final;
		void unmap_texture_region(api::resource resource, uint32_t subresource) final;

		void update_buffer_region(const void *data, api::resource resource, uint64_t offset, uint64_t size) final;
		void update_texture_region(const api::subresource_data &data, api::resource resource, uint32_t subresource, const api::subresource_box *box) final;

		bool create_pipeline(api::pipeline_layout layout, uint32_t subobjecte_count, const api::pipeline_subobject *subobjects, api::pipeline *out_handle) final;
		void destroy_pipeline(api::pipeline handle) final;

		bool create_pipeline_layout(uint32_t param_count, const api::pipeline_layout_param *params, api::pipeline_layout *out_handle) final;
		void destroy_pipeline_layout(api::pipeline_layout handle) final;

		bool allocate_descriptor_tables(uint32_t count, api::pipeline_layout layout, uint32_t layout_param, api::descriptor_table *out_tables) final;
		void free_descriptor_tables(uint32_t count, const api::descriptor_table *tables) final;

		void get_descriptor_heap_offset(api::descriptor_table table, uint32_t binding, uint32_t array_offset, api::descriptor_heap *out_heap, uint32_t *out_offset) const final;

		void copy_descriptor_tables(uint32_t count, const api::descriptor_table_copy *copies) final;
		void update_descriptor_tables(uint32_t count, const api::descriptor_table_update *updates) final;

		bool create_query_heap(api::query_type type, uint32_t size, api::query_heap *out_handle) final;
		void destroy_query_heap(api::query_heap handle) final;

		bool get_query_heap_results(api::query_heap heap, uint32_t first, uint32_t count, void *results, uint32_t stride) final;

		void set_resource_name(api::resource handle, const char *name) final;
		void set_resource_view_name(api::resource_view handle, const char *name) final;

		bool create_fence(uint64_t initial_value, api::fence_flags flags, api::fence *out_handle, HANDLE *shared_handle = nullptr) final;
		void destroy_fence(api::fence handle) final;

		uint64_t get_completed_fence_value(api::fence fence) const final;

		bool wait(api::fence fence, uint64_t value, uint64_t timeout) final;
		bool signal(api::fence fence, uint64_t value) final;

	protected:
		// Cached context information for quick access
		int  _pixel_format;
		api::format _default_depth_format;
		api::resource_desc _default_fbo_desc = {};
		bool _supports_dsa; // Direct State Access (core since OpenGL 4.5)
		bool _compatibility_context;

	private:
		std::vector<GLuint> _reserved_buffer_names;
		std::vector<GLuint> _reserved_texture_names;

		struct map_info
		{
			api::subresource_data data;
			api::subresource_box box;
			api::map_access access;
		};

		std::unordered_map<size_t, map_info> _map_lookup;
	};
}
