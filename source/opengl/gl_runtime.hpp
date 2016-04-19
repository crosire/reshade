#pragma once

#include <gl\gl3w.h>

#include "runtime.hpp"
#include "runtime_objects.hpp"
#include "gl_stateblock.hpp"

namespace reshade
{
	struct gl_texture : texture
	{
		gl_texture() : id()
		{
		}
		~gl_texture()
		{
			if (type == texture_type::image)
			{
				glDeleteTextures(2, id);
			}
		}

		GLuint id[2];
	};
	struct gl_sampler
	{
		GLuint id;
		gl_texture *texture;
		bool is_srgb;
	};
	struct gl_pass : technique::pass
	{
		gl_pass() :
			program(0),
			fbo(0), draw_textures(),
			stencil_reference(0),
			stencil_mask(0), stencil_read_mask(0),
			viewport_width(0), viewport_height(0),
			draw_buffers(), blend_eq_color(GL_NONE), blend_eq_alpha(GL_NONE), blend_src(GL_NONE), blend_dest(GL_NONE), depth_func(GL_NONE), stencil_func(GL_NONE), stencil_op_fail(GL_NONE), stencil_op_z_fail(GL_NONE), stencil_op_z_pass(GL_NONE),
			srgb(GL_FALSE), blend(GL_FALSE), depth_mask(GL_FALSE), depth_test(GL_FALSE), color_mask()
		{
		}
		~gl_pass()
		{
			glDeleteProgram(program);
			glDeleteFramebuffers(1, &fbo);
		}

		GLuint program;
		GLuint fbo, draw_textures[8];
		GLint stencil_reference;
		GLuint stencil_mask, stencil_read_mask;
		GLsizei viewport_width, viewport_height;
		GLenum draw_buffers[8], blend_eq_color, blend_eq_alpha, blend_src, blend_dest, depth_func, stencil_func, stencil_op_fail, stencil_op_z_fail, stencil_op_z_pass;
		GLboolean srgb, blend, depth_mask, depth_test, stencil_test, color_mask[4];
	};

	class gl_runtime : public runtime
	{
	public:
		gl_runtime(HDC device);

		bool on_init(unsigned int width, unsigned int height);
		void on_reset() override;
		void on_reset_effect() override;
		void on_present() override;
		void on_draw_call(unsigned int vertices) override;
		void on_apply_effect() override;
		void on_apply_effect_technique(const technique &technique) override;
		void on_fbo_attachment(GLenum target, GLenum attachment, GLenum objecttarget, GLuint object, GLint level);

		static void update_texture_datatype(gl_texture &texture, texture_type source, GLuint newtexture, GLuint newtexture_srgb);

		HDC _hdc;

		GLuint _reference_count, _current_vertex_count;
		GLuint _default_backbuffer_fbo, _default_backbuffer_rbo[2], _backbuffer_texture[2];
		GLuint _depth_source_fbo, _depth_source, _depth_texture, _blit_fbo;
		std::vector<struct gl_sampler> _effect_samplers;
		GLuint _default_vao, _effect_ubo;

	private:
		struct depth_source_info
		{
			GLint width, height, level, format;
			GLfloat drawcall_count, vertices_count;
		};

		bool init_backbuffer_texture();
		bool init_default_depth_stencil();
		bool init_fx_resources();
		bool init_imgui_resources();
		bool init_imgui_font_atlas();

		void screenshot(unsigned char *buffer) const override;
		bool update_effect(const fx::syntax_tree &ast, const std::vector<std::string> &pragmas, std::string &errors) override;
		bool update_texture(texture &texture, const unsigned char *data, size_t size) override;

		void render_draw_lists(ImDrawData *data) override;

		void detect_depth_source();
		void create_depth_texture(GLuint width, GLuint height, GLenum format);

		utils::gl_stateblock _stateblock;
		std::unordered_map<GLuint, depth_source_info> _depth_source_table;

		GLuint _imgui_shader_program = 0, _imgui_VertHandle = 0, _imgui_FragHandle = 0;
		int _imgui_attribloc_tex = 0, _imgui_attribloc_projmtx = 0;
		int _imgui_attribloc_pos = 0, _imgui_attribloc_uv = 0, _imgui_attribloc_color = 0;
		GLuint _imgui_vbo[2] = { }, _imgui_vao = 0;
	};
}