/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include <map>
#include "opengl.hpp"

#define RESHADE_OPENGL_CAPTURE_DEPTH_BUFFERS 1

namespace reshade::opengl
{
	class buffer_detection
	{
	public:
		struct draw_stats
		{
			GLuint vertices = 0;
			GLuint drawcalls = 0;
		};
		struct depthstencil_info
		{
			GLuint handle;
			GLint width;
			GLint height;
			GLint level;
			GLint format;
			draw_stats stats;
		};

		void reset(GLuint default_width, GLuint default_height, GLenum default_format);

		GLuint total_vertices() const { return _stats.vertices; }
		GLuint total_drawcalls() const { return _stats.drawcalls; }

		void on_draw(GLsizei vertices);
		void on_draw_vertex(GLsizei vertices) { _current_vertex_count += vertices; }

		void on_fbo_attachment(GLenum attachment, GLenum target, GLuint object, GLint level);

#if RESHADE_OPENGL_CAPTURE_DEPTH_BUFFERS
		const auto &depth_buffer_counters() const { return _depth_source_table; }

		depthstencil_info find_best_depth_texture(GLuint width, GLuint height,
			GLuint override = std::numeric_limits<GLuint>::max());
#endif

	private:
		GLuint _current_vertex_count = 0; // Used to calculate vertex count inside glBegin/glEnd pairs
		draw_stats _stats;
#if RESHADE_OPENGL_CAPTURE_DEPTH_BUFFERS
		std::map<GLuint, depthstencil_info> _depth_source_table;
#endif
	};
}
