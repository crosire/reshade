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
			GLint _vao;
			GLint _vbo;
			GLint _ubo;
			GLint _program;
			GLint _textures2d[8], _samplers[8];
			GLint _active_texture;
			GLint _viewport[4];
			GLint _scissor_test;
			GLint _blend;
			GLint _blend_src, _blend_dest;
			GLint _blend_eq_color, _blend_eq_alpha;
			GLint _depth_test;
			GLboolean _depth_mask;
			GLint _depth_func;
			GLint _stencil_test;
			GLint _stencil_ref;
			GLint _stencil_func;
			GLint _stencil_op_fail, _stencil_op_zfail, _stencil_op_zpass;
			GLint _stencil_read_mask, _stencil_mask;
			GLint _polygon_mode, _frontface;
			GLint _cullface, _cullface_mode;
			GLint _fbo;
			GLint _srgb;
			GLboolean _color_mask[4];
			GLenum _drawbuffers[8];
		};
	}
}
