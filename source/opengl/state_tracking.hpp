/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include "opengl.hpp"
#include <unordered_map>

namespace reshade::opengl
{
	class state_tracking
	{
	public:
		struct draw_stats
		{
			GLuint vertices = 0;
			GLuint drawcalls = 0;
		};
		struct depthstencil_info
		{
			GLuint obj;
			GLuint width, height, level, layer;
			GLenum target, format;
			draw_stats total_stats;
			draw_stats current_stats;
			std::vector<draw_stats> clears;
		};

		void reset(GLuint default_width, GLuint default_height, GLenum default_format);
		void release();

		GLuint total_vertices() const { return _stats.vertices; }
		GLuint total_drawcalls() const { return _stats.drawcalls; }

		void on_draw(GLsizei vertices);
		void on_draw_vertex(GLsizei vertices) { _current_vertex_count += vertices; }
#if RESHADE_DEPTH
		void on_bind_draw_fbo();
		void on_clear_attachments(GLbitfield mask);

		// Detection Settings
		bool preserve_depth_buffers = false;
		bool use_aspect_ratio_heuristics = true;
		std::pair<GLuint, GLuint> depthstencil_clear_index = { 0, 0 };

		const auto &depth_buffer_counters() const { return _depth_source_table; }

		depthstencil_info find_best_depth_texture(GLuint width, GLuint height,
			GLuint override = std::numeric_limits<GLuint>::max());
#endif

	private:
		GLuint _current_vertex_count = 0; // Used to calculate vertex count inside glBegin/glEnd pairs
		draw_stats _stats;
#if RESHADE_DEPTH
		draw_stats _best_copy_stats;
		std::unordered_map<GLuint, depthstencil_info> _depth_source_table;
		GLuint _copy_fbo = 0;
		GLuint _clear_texture = 0;
#endif
	};
}
