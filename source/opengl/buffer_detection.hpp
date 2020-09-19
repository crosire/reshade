/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include "opengl.hpp"
#include <unordered_map>

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
			GLuint obj, level;
			GLuint width, height;
			GLenum target, format;
			draw_stats stats;
		};

		void reset(GLuint default_width, GLuint default_height, GLenum default_format);

		GLuint total_vertices() const { return _stats.vertices; }
		GLuint total_drawcalls() const { return _stats.drawcalls; }

		void on_draw(GLsizei vertices);
		void on_draw_vertex(GLsizei vertices) { _current_vertex_count += vertices; }

#if RESHADE_DEPTH
		void on_fbo_attachment(GLenum attachment, GLenum target, GLuint object, GLint level);
		void on_delete_fbo_attachment(GLenum target, GLuint object);

		const auto &depth_buffer_counters() const { return _depth_source_table; }

		depthstencil_info find_best_depth_texture(GLuint width, GLuint height,
			GLuint override = std::numeric_limits<GLuint>::max()) const;
#endif

	private:
		GLuint _current_vertex_count = 0; // Used to calculate vertex count inside glBegin/glEnd pairs
		draw_stats _stats;
#if RESHADE_DEPTH
		std::unordered_map<GLuint, depthstencil_info> _depth_source_table;
#endif
	};
}
