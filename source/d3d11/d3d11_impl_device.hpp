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

	public:
		explicit device_impl(ID3D11Device *device);
		~device_impl();

		api::device_api get_api() const final { return api::device_api::d3d11; }

		bool check_capability(api::device_caps capability) const final;
		bool check_format_support(api::format format, api::resource_usage usage) const final;

		bool create_sampler(const api::sampler_desc &desc, api::sampler *out) final;
		bool create_resource(const api::resource_desc &desc, const api::subresource_data *initial_data, api::resource_usage initial_state, api::resource *out) final;
		bool create_resource_view(api::resource resource, api::resource_usage usage_type, const api::resource_view_desc &desc, api::resource_view *out) final;

		bool create_pipeline(const api::pipeline_desc &desc, api::pipeline *out) final;
		bool create_graphics_pipeline(const api::pipeline_desc &desc, api::pipeline *out);
		bool create_input_layout(const api::pipeline_desc &desc, api::pipeline *out);
		bool create_vertex_shader(const api::pipeline_desc &desc, api::pipeline *out);
		bool create_hull_shader(const api::pipeline_desc &desc, api::pipeline *out);
		bool create_domain_shader(const api::pipeline_desc &desc, api::pipeline *out);
		bool create_geometry_shader(const api::pipeline_desc &desc, api::pipeline *out);
		bool create_pixel_shader(const api::pipeline_desc &desc, api::pipeline *out);
		bool create_compute_shader(const api::pipeline_desc &desc, api::pipeline *out);
		bool create_blend_state(const api::pipeline_desc &desc, api::pipeline *out);
		bool create_rasterizer_state(const api::pipeline_desc &desc, api::pipeline *out);
		bool create_depth_stencil_state(const api::pipeline_desc &desc, api::pipeline *out);

		bool create_pipeline_layout(const api::pipeline_layout_desc &desc, api::pipeline_layout *out) final;
		bool create_descriptor_set_layout(const api::descriptor_set_layout_desc &desc, api::descriptor_set_layout *out) final;

		bool create_query_pool(api::query_type type, uint32_t size, api::query_pool *out) final;
		bool create_render_pass(const api::render_pass_desc &desc, api::render_pass *out) final;
		bool create_framebuffer(const api::framebuffer_desc &desc, api::framebuffer *out) final;
		bool create_descriptor_sets(api::descriptor_set_layout layout, uint32_t count, api::descriptor_set *out) final;

		void destroy_sampler(api::sampler handle) final;
		void destroy_resource(api::resource handle) final;
		void destroy_resource_view(api::resource_view handle) final;

		void destroy_pipeline(api::pipeline_stage type, api::pipeline handle) final;
		void destroy_pipeline_layout(api::pipeline_layout handle) final;
		void destroy_descriptor_set_layout(api::descriptor_set_layout handle) final;

		void destroy_query_pool(api::query_pool handle) final;
		void destroy_render_pass(api::render_pass handle) final;
		void destroy_framebuffer(api::framebuffer handle) final;
		void destroy_descriptor_sets(api::descriptor_set_layout layout, uint32_t count, const api::descriptor_set *sets) final;

		bool get_attachment(api::framebuffer fbo, api::attachment_type type, uint32_t index, api::resource_view *out) const final;
		uint32_t get_attachment_count(api::framebuffer pass, api::attachment_type type) const final;

		void get_resource_from_view(api::resource_view view, api::resource *out) const final;
		api::resource_desc get_resource_desc(api::resource resource) const final;

		bool map_resource(api::resource resource, uint32_t subresource, api::map_access access, void **data, uint32_t *row_pitch, uint32_t *slice_pitch) final;
		void unmap_resource(api::resource resource, uint32_t subresource) final;

		void upload_buffer_region(const void *data, api::resource dst, uint64_t dst_offset, uint64_t size) final;
		void upload_texture_region(const api::subresource_data &data, api::resource dst, uint32_t dst_subresource, const int32_t dst_box[6]) final;

		void update_descriptor_sets(uint32_t num_writes, const api::descriptor_set_write *writes, uint32_t num_copies, const api::descriptor_set_copy *copies) final;

		bool get_query_pool_results(api::query_pool pool, uint32_t first, uint32_t count, void *results, uint32_t stride) final;

		void wait_idle() const final { /* no-op */ }

		void set_resource_name(api::resource resource, const char *name) final;

	private:
		ID3D11DeviceContext *_immediate_context_orig = nullptr;
		com_ptr<ID3D11VertexShader> _copy_vert_shader;
		com_ptr<ID3D11PixelShader>  _copy_pixel_shader;
		com_ptr<ID3D11SamplerState> _copy_sampler_state;
	};
}
