#pragma once

#include <gl/gl3w.h>

#include "runtime.hpp"
#include "runtime_objects.hpp"
#include "opengl_stateblock.hpp"

namespace reshade
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
	struct opengl_pass_data : base_object
	{
		opengl_pass_data() :
			program(0),
			fbo(0), draw_textures(),
			stencil_reference(0),
			stencil_mask(0), stencil_read_mask(0),
			viewport_width(0), viewport_height(0),
			draw_buffers(), blend_eq_color(GL_NONE), blend_eq_alpha(GL_NONE), blend_src(GL_NONE), blend_dest(GL_NONE), depth_func(GL_NONE), stencil_func(GL_NONE), stencil_op_fail(GL_NONE), stencil_op_z_fail(GL_NONE), stencil_op_z_pass(GL_NONE),
			srgb(GL_FALSE), blend(GL_FALSE), depth_mask(GL_FALSE), depth_test(GL_FALSE), color_mask()
		{
		}
		~opengl_pass_data()
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
		bool clear_render_targets;
	};
	struct opengl_sampler
	{
		GLuint id;
		opengl_tex_data *texture;
		bool is_srgb;
		bool has_mipmaps;
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
		bool update_effect(const reshadefx::syntax_tree &ast, std::string &errors) override;
		bool update_texture(texture &texture, const uint8_t *data) override;
		bool update_texture_reference(texture &texture, unsigned short id) override;

		void render_technique(const technique &technique) override;
		void render_draw_lists(ImDrawData *data) override;

		HDC _hdc;

		GLuint _reference_count = 1, _current_vertex_count = 0;
		GLuint _default_backbuffer_fbo = 0, _default_backbuffer_rbo[2] = { }, _backbuffer_texture[2] = { };
		GLuint _depth_source_fbo = 0, _depth_source = 0, _depth_texture = 0, _blit_fbo = 0;
		std::vector<struct opengl_sampler> _effect_samplers;
		GLuint _default_vao = 0;
		std::vector<std::pair<GLuint, GLsizei>> _effect_ubos;

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

		void detect_depth_source();
		void create_depth_texture(GLuint width, GLuint height, GLenum format);

		utils::opengl_stateblock _stateblock;
		std::unordered_map<GLuint, depth_source_info> _depth_source_table;

		GLuint _imgui_shader_program = 0, _imgui_VertHandle = 0, _imgui_FragHandle = 0;
		int _imgui_attribloc_tex = 0, _imgui_attribloc_projmtx = 0;
		int _imgui_attribloc_pos = 0, _imgui_attribloc_uv = 0, _imgui_attribloc_color = 0;
		GLuint _imgui_vbo[2] = { }, _imgui_vao = 0;
	};
}