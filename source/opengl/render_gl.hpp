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
	void convert_resource_desc(const api::resource_desc &desc, GLsizei *levels, GLenum *internalformat, GLsizei *width, GLsizei *height = nullptr, GLsizei *depth = nullptr);
	std::pair<api::resource_type, api::resource_desc> convert_resource_desc(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height = 1, GLsizei depth = 1);

	void convert_resource_view_desc(const api::resource_view_desc &desc, GLenum *internalformat, GLuint *minlevel, GLuint *numlevels, GLuint *minlayer, GLuint *numlayers);
	api::resource_view_desc convert_resource_view_desc(GLenum target, GLenum internalformat, GLuint minlevel, GLuint numlevels, GLuint minlayer, GLuint numlayers);

	class device_impl : public api::device, public api::command_queue, public api::command_list, api::api_data
	{
	public:
		device_impl(HDC hdc);
		~device_impl();

		bool get_data(const uint8_t guid[16], uint32_t size, void *data) override { return api_data::get_data(guid, size, data); }
		void set_data(const uint8_t guid[16], uint32_t size, const void *data) override  { api_data::set_data(guid, size, data); }

		api::render_api get_api() override { return api::render_api::opengl; }

		bool check_format_support(uint32_t format, api::resource_usage usage) override;

		bool is_resource_valid(api::resource_handle resource) override;
		bool is_resource_view_valid(api::resource_view_handle view) override;

		bool create_resource(api::resource_type type, const api::resource_desc &desc, api::resource_usage initial_state, api::resource_handle *out_resource) override;
		bool create_resource_view(api::resource_handle resource, api::resource_view_type type, const api::resource_view_desc &desc, api::resource_view_handle *out_view) override;

		void destroy_resource(api::resource_handle resource) override;
		void destroy_resource_view(api::resource_view_handle view) override;

		void get_resource_from_view(api::resource_view_handle view, api::resource_handle *out_resource) override;

		api::resource_desc get_resource_desc(api::resource_handle resource) override;

		void wait_idle() override;

		api::resource_view_handle get_depth_stencil_from_fbo(GLuint fbo);
		api::resource_view_handle get_render_target_from_fbo(GLuint fbo, GLuint drawbuffer);

		api::device *get_device() override { return this; }

		api::command_list *get_immediate_command_list() override { return this; }

		void transition_state(api::resource_handle, api::resource_usage, api::resource_usage) override { /* NOP */ }

		void clear_depth_stencil_view(api::resource_view_handle dsv, uint32_t clear_flags, float depth, uint8_t stencil) override;
		void clear_render_target_view(api::resource_view_handle rtv, const float color[4]) override;

		void copy_resource(api::resource_handle source, api::resource_handle dest) override;

		bool _compatibility_context = false;
		std::unordered_set<HDC> _hdcs;
		GLuint current_vertex_count = 0; // Used to calculate vertex count inside glBegin/glEnd pairs

	protected:
		GLuint _default_fbo_width = 0;
		GLuint _default_fbo_height = 0;
		GLenum _default_depth_format = GL_NONE;

	private:
		GLuint _copy_fbo[2] = {};
	};
}
