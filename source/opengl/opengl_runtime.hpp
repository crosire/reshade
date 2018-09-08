/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include "runtime.hpp"
#include "opengl_stateblock.hpp"

namespace reshade::opengl
{
	struct opengl_tex_data : base_object
	{
		~opengl_tex_data()
		{
			if (should_delete)
			{
				glDeleteTextures(2, id);
			}
		}

		bool should_delete = false;
		GLuint id[2] = { };
	};

	struct opengl_sampler_data
	{
		GLuint id;
		opengl_tex_data *texture;
		bool is_srgb;
		bool has_mipmaps;
	};

	struct opengl_pass_data : base_object
	{
		~opengl_pass_data()
		{
			if (program)
				glDeleteProgram(program);
			glDeleteFramebuffers(1, &fbo);
		}

		GLuint program = 0;
		GLuint fbo = 0, draw_textures[8] = { };
		GLint stencil_reference = 0;
		GLuint stencil_mask = 0, stencil_read_mask = 0;
		GLsizei viewport_width = 0, viewport_height = 0;
		GLenum draw_buffers[8] = { };
		GLenum stencil_func = GL_NONE, stencil_op_fail = GL_NONE, stencil_op_z_fail = GL_NONE, stencil_op_z_pass = GL_NONE;
		GLenum blend_eq_color = GL_NONE, blend_eq_alpha = GL_NONE, blend_src = GL_NONE, blend_dest = GL_NONE, blend_src_alpha = GL_NONE, blend_dest_alpha = GL_NONE;
		GLboolean color_mask[4] = { };
		bool srgb = false, blend = false, stencil_test = false, clear_render_targets = true;
	};

	struct opengl_technique_data : base_object
	{
		~opengl_technique_data()
		{
			glDeleteQueries(1, &query);
		}

		GLuint query = 0;
		bool query_in_flight = false;
		std::vector<opengl_sampler_data> samplers;
	};

	class opengl_runtime : public runtime
	{
	public:
		opengl_runtime(HDC device);

		bool on_init(unsigned int width, unsigned int height);
		void on_reset();
		void on_reset_effect() override;
		void on_present();
		void on_draw_call(unsigned int vertices);
		void on_fbo_attachment(GLenum target, GLenum attachment, GLenum objecttarget, GLuint object, GLint level);

		void capture_frame(uint8_t *buffer) const override;
		bool load_effect(const reshadefx::spirv_module &module, std::string &errors) override;
		bool update_texture(texture &texture, const uint8_t *data) override;
		bool update_texture_reference(texture &texture, texture_reference id);

		void render_technique(const technique &technique) override;
		void render_imgui_draw_data(ImDrawData *data) override;

		HDC _hdc;

		GLuint _reference_count = 1, _current_vertex_count = 0;
		GLuint _default_backbuffer_fbo = 0, _default_backbuffer_rbo[2] = { }, _backbuffer_texture[2] = { };
		GLuint _depth_source_fbo = 0, _depth_source = 0, _depth_texture = 0, _blit_fbo = 0;
		GLuint _default_vao = 0;
		std::vector<std::pair<GLuint, GLsizeiptr>> _effect_ubos;
		std::unordered_map<size_t, GLuint> _effect_sampler_states;

	private:
		struct depth_source_info
		{
			unsigned int width, height;
			GLint level, format;
			unsigned int drawcall_count, vertices_count;
		};

		bool init_backbuffer_texture();
		bool init_default_depth_stencil();
		bool init_fx_resources();
		bool init_imgui_resources();
		bool init_imgui_font_atlas();

		void detect_depth_source();
		void create_depth_texture(GLuint width, GLuint height, GLenum format);

		opengl_stateblock _stateblock;
		std::unordered_map<GLuint, depth_source_info> _depth_source_table;

		GLuint _imgui_shader_program = 0, _imgui_VertHandle = 0, _imgui_FragHandle = 0;
		int _imgui_attribloc_tex = 0, _imgui_attribloc_projmtx = 0;
		int _imgui_attribloc_pos = 0, _imgui_attribloc_uv = 0, _imgui_attribloc_color = 0;
		GLuint _imgui_vbo[2] = { }, _imgui_vao = 0;
	};
}