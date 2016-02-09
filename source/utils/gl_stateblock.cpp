#include "gl_stateblock.hpp"

namespace reshade
{
	namespace utils
	{
		gl_stateblock::gl_stateblock()
		{
			ZeroMemory(this, sizeof(*this));
		}

		void gl_stateblock::capture()
		{
			glGetIntegerv(GL_VERTEX_ARRAY_BINDING, reinterpret_cast<GLint *>(&_vao));
			glGetIntegerv(GL_ARRAY_BUFFER_BINDING, reinterpret_cast<GLint *>(&_vbo));
			glGetIntegerv(GL_UNIFORM_BUFFER_BINDING, reinterpret_cast<GLint *>(&_ubo));
			glGetIntegerv(GL_CURRENT_PROGRAM, reinterpret_cast<GLint *>(&_program));

			for (GLuint i = 0; i < ARRAYSIZE(_textures2d); i++)
			{
				glActiveTexture(GL_TEXTURE0 + i);
				glGetIntegerv(GL_TEXTURE_BINDING_2D, reinterpret_cast<GLint *>(&_textures2d[i]));
				glGetIntegerv(GL_SAMPLER_BINDING, reinterpret_cast<GLint *>(&_samplers[i]));
			}

			glGetIntegerv(GL_ACTIVE_TEXTURE, reinterpret_cast<GLint *>(&_active_texture));
			glGetIntegerv(GL_VIEWPORT, _viewport);
			_scissor_test = glIsEnabled(GL_SCISSOR_TEST);
			_blend = glIsEnabled(GL_BLEND);
			glGetIntegerv(GL_BLEND_SRC, reinterpret_cast<GLint *>(&_blend_src));
			glGetIntegerv(GL_BLEND_DST, reinterpret_cast<GLint *>(&_blend_dest));
			glGetIntegerv(GL_BLEND_EQUATION_RGB, reinterpret_cast<GLint *>(&_blend_eq_color));
			glGetIntegerv(GL_BLEND_EQUATION_ALPHA, reinterpret_cast<GLint *>(&_blend_eq_alpha));
			_depth_test = glIsEnabled(GL_DEPTH_TEST);
			glGetBooleanv(GL_DEPTH_WRITEMASK, &_depth_mask);
			glGetIntegerv(GL_DEPTH_FUNC, reinterpret_cast<GLint *>(&_depth_func));
			_stencil_test = glIsEnabled(GL_STENCIL_TEST);
			glGetIntegerv(GL_STENCIL_REF, &_stencil_ref);
			glGetIntegerv(GL_STENCIL_FUNC, reinterpret_cast<GLint *>(&_stencil_func));
			glGetIntegerv(GL_STENCIL_FAIL, reinterpret_cast<GLint *>(&_stencil_op_fail));
			glGetIntegerv(GL_STENCIL_PASS_DEPTH_FAIL, reinterpret_cast<GLint *>(&_stencil_op_zfail));
			glGetIntegerv(GL_STENCIL_PASS_DEPTH_PASS, reinterpret_cast<GLint *>(&_stencil_op_zpass));
			glGetIntegerv(GL_STENCIL_VALUE_MASK, reinterpret_cast<GLint *>(&_stencil_read_mask));
			glGetIntegerv(GL_STENCIL_WRITEMASK, reinterpret_cast<GLint *>(&_stencil_mask));
			glGetIntegerv(GL_POLYGON_MODE, reinterpret_cast<GLint *>(&_polygon_mode));
			glGetIntegerv(GL_FRONT_FACE, reinterpret_cast<GLint *>(&_frontface));
			_cullface = glIsEnabled(GL_CULL_FACE);
			glGetIntegerv(GL_CULL_FACE_MODE, reinterpret_cast<GLint *>(&_cullface_mode));
			glGetIntegerv(GL_FRAMEBUFFER_BINDING, reinterpret_cast<GLint *>(&_fbo));
			_srgb = glIsEnabled(GL_FRAMEBUFFER_SRGB);
			glGetBooleanv(GL_COLOR_WRITEMASK, _color_mask);

			for (GLuint i = 0; i < ARRAYSIZE(_drawbuffers); i++)
			{
				glGetIntegerv(GL_DRAW_BUFFER0 + i, reinterpret_cast<GLint *>(&_drawbuffers[i]));
			}
		}
		void gl_stateblock::apply() const
		{
			glBindVertexArray(glIsVertexArray(_vao) ? _vao : 0);
			glBindBuffer(GL_ARRAY_BUFFER, glIsBuffer(_vbo) ? _vbo : 0);
			glBindBuffer(GL_UNIFORM_BUFFER, glIsBuffer(_ubo) ? _ubo : 0);
			glUseProgram(glIsProgram(_program) ? _program : 0);

			for (GLuint i = 0; i < ARRAYSIZE(_textures2d); i++)
			{
				glActiveTexture(GL_TEXTURE0 + i);
				glBindTexture(GL_TEXTURE_2D, glIsTexture(_textures2d[i]) ? _textures2d[i] : 0);
				glBindSampler(i, glIsSampler(_samplers[i]) ? _samplers[i] : 0);
			}

			glActiveTexture(_active_texture);
			glViewport(_viewport[0], _viewport[1], _viewport[2], _viewport[3]);
			_scissor_test ? glEnable(GL_SCISSOR_TEST) : glDisable(GL_SCISSOR_TEST);
			_blend ? glEnable(GL_BLEND) : glDisable(GL_BLEND);
			glBlendFunc(_blend_src, _blend_dest);
			glBlendEquationSeparate(_blend_eq_color, _blend_eq_alpha);
			_depth_test ? glEnable(GL_DEPTH_TEST) : glDisable(GL_DEPTH_TEST);
			glDepthMask(_depth_mask);
			glDepthFunc(_depth_func);
			_stencil_test ? glEnable(GL_STENCIL_TEST) : glDisable(GL_STENCIL_TEST);
			glStencilFunc(_stencil_func, _stencil_ref, _stencil_read_mask);
			glStencilOp(_stencil_op_fail, _stencil_op_zfail, _stencil_op_zpass);
			glStencilMask(_stencil_mask);
			glPolygonMode(GL_FRONT_AND_BACK, _polygon_mode);
			glFrontFace(_frontface);
			_cullface ? glEnable(GL_CULL_FACE) : glDisable(GL_CULL_FACE);
			glCullFace(_cullface_mode);
			glBindFramebuffer(GL_FRAMEBUFFER, glIsFramebuffer(_fbo) ? _fbo : 0);
			_srgb ? glEnable(GL_FRAMEBUFFER_SRGB) : glDisable(GL_FRAMEBUFFER_SRGB);
			glColorMask(_color_mask[0], _color_mask[1], _color_mask[2], _color_mask[3]);

			if (_drawbuffers[1] == GL_NONE &&
				_drawbuffers[2] == GL_NONE &&
				_drawbuffers[3] == GL_NONE &&
				_drawbuffers[4] == GL_NONE &&
				_drawbuffers[5] == GL_NONE &&
				_drawbuffers[6] == GL_NONE &&
				_drawbuffers[7] == GL_NONE)
			{
				glDrawBuffer(_drawbuffers[0]);
			}
			else
			{
				glDrawBuffers(8, _drawbuffers);
			}
		}
	}
}
