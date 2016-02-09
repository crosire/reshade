#pragma once

#include <gl\gl3w.h>

namespace reshade
{
	namespace utils
	{
		class gl_stateblock
		{
		public:
			gl_stateblock();

			void capture();
			void apply() const;

		private:
			GLuint _vao;
			GLuint _vbo;
			GLuint _ubo;
			GLuint _program;
			GLuint _textures2d[8], _samplers[8];
			GLenum _active_texture;
			GLint _viewport[4];
			GLboolean _scissor_test;
			GLboolean _blend;
			GLenum _blend_src, _blend_dest;
			GLenum _blend_eq_color, _blend_eq_alpha;
			GLboolean _depth_test;
			GLboolean _depth_mask;
			GLenum _depth_func;
			GLboolean _stencil_test;
			GLint _stencil_ref;
			GLenum _stencil_func;
			GLenum _stencil_op_fail, _stencil_op_zfail, _stencil_op_zpass;
			GLuint _stencil_read_mask, _stencil_mask;
			GLenum _polygon_mode, _frontface;
			GLenum _cullface, _cullface_mode;
			GLuint _fbo;
			GLboolean _srgb;
			GLboolean _color_mask[4];
			GLenum _drawbuffers[8];
		};
	}
}
