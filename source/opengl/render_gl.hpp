/*
 * Copyright (C) 2021 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include "opengl.hpp"
#include "addon_manager.hpp"
#include <unordered_set>

namespace reshade::opengl
{
	inline api::resource_handle make_resource_handle(GLenum target, GLuint object)
	{
		if (object == GL_NONE)
			return { 0 };
		return { (static_cast<uint64_t>(target) << 40) | object };
	}
	inline api::resource_view_handle make_resource_view_handle(GLenum target_or_attachment, GLuint object)
	{
		if (object == GL_NONE)
			return { 0 };
		return { (static_cast<uint64_t>(target_or_attachment) << 40) | object };
	}

	class device_impl : public api::api_object_impl<HGLRC, api::device, api::command_queue, api::command_list>
	{
	public:
		device_impl(HDC hdc, HGLRC hglrc);
		~device_impl();

		api::render_api get_api() const final { return api::render_api::opengl; }

		bool check_format_support(uint32_t format, api::resource_usage usage) const final;

		bool check_resource_handle_valid(api::resource_handle resource) const final;
		bool check_resource_view_handle_valid(api::resource_view_handle view) const final;

		bool create_resource(const api::resource_desc &desc, const api::mapped_subresource *initial_data, api::resource_usage initial_state, api::resource_handle *out_resource) final;
		bool create_resource_view(api::resource_handle resource, api::resource_usage usage_type, const api::resource_view_desc &desc, api::resource_view_handle *out_view) final;

		void destroy_resource(api::resource_handle resource) final;
		void destroy_resource_view(api::resource_view_handle view) final;

		void get_resource_from_view(api::resource_view_handle view, api::resource_handle *out_resource) const final;

		api::resource_desc get_resource_desc(api::resource_handle resource) const final;

		void wait_idle() const final;

		api::resource_view_handle get_depth_stencil_from_fbo(GLuint fbo) const;
		api::resource_view_handle get_render_target_from_fbo(GLuint fbo, GLuint drawbuffer) const;

		api::device *get_device() override { return this; }

		api::command_list *get_immediate_command_list() final { return this; }

		void flush_immediate_command_list() const final;

		void copy_resource(api::resource_handle source, api::resource_handle destination) final;

		void transition_state(api::resource_handle, api::resource_usage, api::resource_usage) final { /* no-op */ }

		void clear_depth_stencil_view(api::resource_view_handle dsv, uint32_t clear_flags, float depth, uint8_t stencil) final;
		void clear_render_target_view(api::resource_view_handle rtv, const float color[4]) final;

	public:
		bool _compatibility_context = false;
		std::unordered_set<HDC> _hdcs;
		GLuint _current_vertex_count = 0; // Used to calculate vertex count inside glBegin/glEnd pairs

	private:
		GLuint _copy_fbo[2] = {};

	protected:
		// Cached context information for quick access
		GLuint _default_fbo_width = 0;
		GLuint _default_fbo_height = 0;
		GLenum _default_color_format = GL_NONE;
		GLenum _default_depth_format = GL_NONE;
	};
}
