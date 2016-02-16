#pragma once

#include "runtime.hpp"
#include "utils\gl_stateblock.hpp"

#include <algorithm>
#include <gl\gl3w.h>

namespace reshade
{
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
		void on_apply_effect_technique(const technique *technique) override;
		void on_fbo_attachment(GLenum target, GLenum attachment, GLenum objecttarget, GLuint object, GLint level);

		static void update_texture_datatype(texture *texture, texture::datatype source, GLuint newtexture, GLuint newtexture_srgb);

		HDC _hdc;

		GLuint _reference_count, _current_vertex_count;
		GLuint _default_backbuffer_fbo, _default_backbuffer_rbo[2], _backbuffer_texture[2];
		GLuint _depth_source_fbo, _depth_source, _depth_texture, _blit_fbo;
		std::vector<struct gl_sampler> _effect_samplers;
		GLuint _default_vao, _default_vbo, _effect_ubo;

	private:
		struct depth_source_info
		{
			GLint width, height, level, format;
			GLfloat drawcall_count, vertices_count;
		};

		void screenshot(unsigned char *buffer) const override;
		bool update_effect(const fx::nodetree &ast, const std::vector<std::string> &pragmas, std::string &errors) override;
		bool update_texture(texture *texture, const unsigned char *data, size_t size) override;

		void detect_depth_source();
		void create_depth_texture(GLuint width, GLuint height, GLenum format);

		utils::gl_stateblock _stateblock;
		std::unordered_map<GLuint, depth_source_info> _depth_source_table;
	};
}