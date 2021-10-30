/*
 * Copyright (C) 2021 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include <d3d11_4.h>
#include "com_ptr.hpp"
#include "addon_manager.hpp"

namespace reshade::d3d11
{
	class device_impl : public api::api_object_impl<ID3D11Device *, api::device>
	{
		friend class swapchain_impl;
		friend class device_context_impl;

	public:
		explicit device_impl(ID3D11Device *device);
		~device_impl();

		api::device_api get_api() const final { return api::device_api::d3d11; }

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
		bool create_graphics_pipeline(const api::pipeline_desc &desc, api::pipeline *out_handle);
		bool create_input_layout(const api::pipeline_desc &desc, api::pipeline *out_handle);
		bool create_vertex_shader(const api::pipeline_desc &desc, api::pipeline *out_handle);
		bool create_hull_shader(const api::pipeline_desc &desc, api::pipeline *out_handle);
		bool create_domain_shader(const api::pipeline_desc &desc, api::pipeline *out_handle);
		bool create_geometry_shader(const api::pipeline_desc &desc, api::pipeline *out_handle);
		bool create_pixel_shader(const api::pipeline_desc &desc, api::pipeline *out_handle);
		bool create_compute_shader(const api::pipeline_desc &desc, api::pipeline *out_handle);
		bool create_rasterizer_state(const api::pipeline_desc &desc, api::pipeline *out_handle);
		bool create_blend_state(const api::pipeline_desc &desc, api::pipeline *out_handle);
		bool create_depth_stencil_state(const api::pipeline_desc &desc, api::pipeline *out_handle);
		void destroy_pipeline(api::pipeline handle) final;

		bool create_render_pass(const api::render_pass_desc &desc, api::render_pass *out_handle) final;
		void destroy_render_pass(api::render_pass handle) final;

		bool create_framebuffer(const api::framebuffer_desc &desc, api::framebuffer *out_handle) final;
		void destroy_framebuffer(api::framebuffer handle) final;

		api::resource_view get_framebuffer_attachment(api::framebuffer framebuffer, api::attachment_type type, uint32_t index) const final;

		bool create_pipeline_layout(uint32_t param_count, const api::pipeline_layout_param *params, api::pipeline_layout *out_handle) final;
		void destroy_pipeline_layout(api::pipeline_layout handle) final;

		void get_pipeline_layout_desc(api::pipeline_layout layout, uint32_t *out_count, api::pipeline_layout_param *out_params) const final;

		bool create_descriptor_set_layout(uint32_t range_count, const api::descriptor_range *ranges, bool push_descriptors, api::descriptor_set_layout *out_handle) final;
		void destroy_descriptor_set_layout(api::descriptor_set_layout handle) final;

		void get_descriptor_set_layout_desc(api::descriptor_set_layout layout, uint32_t *out_count, api::descriptor_range *out_ranges) const final;

		bool create_query_pool(api::query_type type, uint32_t size, api::query_pool *out) final;
		void destroy_query_pool(api::query_pool handle) final;

		bool create_descriptor_sets(uint32_t count, const api::descriptor_set_layout *layouts, api::descriptor_set *out_sets) final;
		void destroy_descriptor_sets(uint32_t count, const api::descriptor_set *sets) final;

		void get_descriptor_pool_offset(api::descriptor_set set, api::descriptor_pool *out_pool, uint32_t *out_offset) const final;

		bool map_buffer_region(api::resource resource, uint64_t offset, uint64_t size, api::map_access access, void **out_data) final;
		void unmap_buffer_region(api::resource resource) final;
		bool map_texture_region(api::resource resource, uint32_t subresource, const int32_t box[6], api::map_access access, api::subresource_data *out_data) final;
		void unmap_texture_region(api::resource resource, uint32_t subresource) final;

		void update_buffer_region(const void *data, api::resource resource, uint64_t offset, uint64_t size) final;
		void update_texture_region(const api::subresource_data &data, api::resource resource, uint32_t subresource, const int32_t box[6]) final;

		void update_descriptor_sets(uint32_t count, const api::descriptor_set_update *updates) final;

		bool get_query_pool_results(api::query_pool pool, uint32_t first, uint32_t count, void *results, uint32_t stride) final;

		void wait_idle() const final { /* no-op */ }

	private:
		com_ptr<ID3D11VertexShader> _copy_vert_shader;
		com_ptr<ID3D11PixelShader>  _copy_pixel_shader;
		com_ptr<ID3D11SamplerState> _copy_sampler_state;
	};
}
