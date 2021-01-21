/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "state_tracking.hpp"
#include <cmath>
#include <cassert>

void reshade::opengl::state_tracking::reset(GLuint default_width, GLuint default_height, GLenum default_format)
{
	// Reset statistics for next frame
	_stats = { 0, 0 };

#if RESHADE_DEPTH
	_best_copy_stats = { 0, 0 };
	_depth_source_table.clear();

	// Initialize information for the default depth buffer
	_depth_source_table[0] = { 0, default_width, default_height, 0, 0, GL_FRAMEBUFFER_DEFAULT, default_format };
#else
	UNREFERENCED_PARAMETER(default_width);
	UNREFERENCED_PARAMETER(default_height);
	UNREFERENCED_PARAMETER(default_format);
#endif
}
void reshade::opengl::state_tracking::release()
{
#if RESHADE_DEPTH
	glDeleteTextures(1, &_clear_texture);
	_clear_texture = 0;
	glDeleteFramebuffers(1, &_copy_fbo);
	_copy_fbo = 0;
#endif
}

#if RESHADE_DEPTH
static bool current_depth_source(GLint &object, GLint &target)
{
	target = GL_FRAMEBUFFER_DEFAULT;
	glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &object);

	// Zero is valid too, in which case the default depth buffer is referenced, instead of a FBO
	if (object != 0)
	{
		glGetFramebufferAttachmentParameteriv(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &target);
		if (target == GL_NONE)
			return false; // FBO does not have a depth attachment
		glGetFramebufferAttachmentParameteriv(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &object);
	}

	return true;
}
#endif

void reshade::opengl::state_tracking::on_draw(GLsizei vertices)
{
	vertices += _current_vertex_count;
	_current_vertex_count = 0;

	_stats.vertices += vertices;
	_stats.drawcalls += 1;

#if RESHADE_DEPTH
	if (GLint object = 0, target;
		current_depth_source(object, target))
	{
		auto &counters = _depth_source_table[object | (target == GL_RENDERBUFFER ? 0x80000000 : 0)];
		counters.total_stats.vertices += vertices;
		counters.total_stats.drawcalls += 1;
		counters.current_stats.vertices += vertices;
		counters.current_stats.drawcalls += 1;
	}
#endif
}

#if RESHADE_DEPTH
static void get_rbo_param(GLuint id, GLenum param, GLuint &value)
{
	if (gl3wProcs.gl.GetNamedRenderbufferParameteriv != nullptr)
	{
		glGetNamedRenderbufferParameteriv(id, param, reinterpret_cast<GLint *>(&value));
	}
	else
	{
		GLint prev_binding = 0;
		glGetIntegerv(GL_RENDERBUFFER_BINDING, &prev_binding);
		glBindRenderbuffer(GL_RENDERBUFFER, id);
		glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_WIDTH, reinterpret_cast<GLint *>(&value));
		glBindRenderbuffer(GL_RENDERBUFFER, prev_binding);
	}
}
static void get_tex_param(GLuint id, GLenum param, GLuint &value)
{
	if (gl3wProcs.gl.GetTextureParameteriv != nullptr)
	{
		glGetTextureParameteriv(id, param, reinterpret_cast<GLint *>(&value));
	}
	else
	{
		GLint prev_binding = 0;
		glGetIntegerv(GL_TEXTURE_BINDING_2D, &prev_binding);
		glBindTexture(GL_TEXTURE_2D, id);
		glGetTexParameteriv(GL_TEXTURE_2D, param, reinterpret_cast<GLint *>(&value));
		glBindTexture(GL_TEXTURE_2D, prev_binding);
	}
}
static void get_tex_level_param(GLuint id, GLuint level, GLenum param, GLuint &value)
{
	if (gl3wProcs.gl.GetTextureLevelParameteriv != nullptr)
	{
		glGetTextureLevelParameteriv(id, level, param, reinterpret_cast<GLint *>(&value));
	}
	else
	{
		GLint prev_binding = 0;
		glGetIntegerv(GL_TEXTURE_BINDING_2D, &prev_binding);
		glBindTexture(GL_TEXTURE_2D, id);
		glGetTexLevelParameteriv(GL_TEXTURE_2D, level, param, reinterpret_cast<GLint *>(&value));
		glBindTexture(GL_TEXTURE_2D, prev_binding);
	}
}

void reshade::opengl::state_tracking::on_bind_draw_fbo()
{
	GLint object = 0, target;
	if (!current_depth_source(object, target))
		return;

	depthstencil_info &info = _depth_source_table[object | (target == GL_RENDERBUFFER ? 0x80000000 : 0)];
	info.obj = object;
	info.target = target;

	switch (target)
	{
	case GL_TEXTURE:
		glGetFramebufferAttachmentParameteriv(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL, reinterpret_cast<GLint *>(&info.level));
		glGetFramebufferAttachmentParameteriv(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL, reinterpret_cast<GLint *>(&info.layer));
		get_tex_param(object, GL_TEXTURE_TARGET, info.target);
		get_tex_level_param(object, info.level, GL_TEXTURE_WIDTH, info.width);
		get_tex_level_param(object, info.level, GL_TEXTURE_HEIGHT, info.height);
		get_tex_level_param(object, info.level, GL_TEXTURE_INTERNAL_FORMAT, info.format);
		break;
	case GL_RENDERBUFFER:
		get_rbo_param(object, GL_RENDERBUFFER_WIDTH, info.width);
		get_rbo_param(object, GL_RENDERBUFFER_HEIGHT, info.height);
		get_rbo_param(object, GL_RENDERBUFFER_INTERNAL_FORMAT, info.format);
		break;
	case GL_FRAMEBUFFER_DEFAULT:
		break;
	}
}
void reshade::opengl::state_tracking::on_clear_attachments(GLbitfield mask)
{
	if ((mask & GL_DEPTH_BUFFER_BIT) == 0)
		return;

	GLint object = 0, target;
	if (!current_depth_source(object, target))
		return;

	const GLuint id = object | (target == GL_RENDERBUFFER ? 0x80000000 : 0);
	if (id != depthstencil_clear_index.first)
		return;

	auto &counters = _depth_source_table[id];

	// Ignore clears when there was no meaningful workload
	if (counters.current_stats.drawcalls == 0)
		return;

	counters.clears.push_back(counters.current_stats);

	// Make a backup copy of the depth texture before it is cleared
	if (depthstencil_clear_index.second == 0 ?
		// If clear index override is set to zero, always copy any suitable buffers
		counters.current_stats.vertices > _best_copy_stats.vertices :
		counters.clears.size() == depthstencil_clear_index.second)
	{
		_best_copy_stats = counters.current_stats;

		GLint read_fbo = 0;
		GLint draw_fbo = 0;
		glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &read_fbo);
		glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &draw_fbo);

		if (_copy_fbo == 0)
		{
			glGenFramebuffers(1, &_copy_fbo);
		}
		if (_clear_texture == 0)
		{
			glGenTextures(1, &_clear_texture);
			glBindTexture(GL_TEXTURE_2D, _clear_texture);
			glTexStorage2D(GL_TEXTURE_2D, 1, counters.format, counters.width, counters.height);

			glBindFramebuffer(GL_FRAMEBUFFER, _copy_fbo);
			glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, _clear_texture, 0);
			assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
		}

		glBindFramebuffer(GL_READ_FRAMEBUFFER, draw_fbo);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, _copy_fbo);
		glBlitFramebuffer(0, 0, counters.width, counters.height, 0, 0, counters.width, counters.height, GL_DEPTH_BUFFER_BIT, GL_NEAREST);

		glBindFramebuffer(GL_READ_FRAMEBUFFER, read_fbo);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, draw_fbo);
	}

	// Reset draw call stats for clears
	counters.current_stats = { 0, 0 };
}

reshade::opengl::state_tracking::depthstencil_info reshade::opengl::state_tracking::find_best_depth_texture(GLuint width, GLuint height, GLuint override)
{
	depthstencil_info best_snapshot = _depth_source_table.at(0); // Always fall back to default depth buffer if no better match is found

	if (override != std::numeric_limits<GLuint>::max())
	{
		const auto source_it = _depth_source_table.find(override);
		if (source_it != _depth_source_table.end())
			return source_it->second;
		else
			return best_snapshot;
	}

	for (const auto &[image, snapshot] : _depth_source_table)
	{
		if (snapshot.total_stats.drawcalls == 0)
			continue; // Skip unused

		if (use_aspect_ratio_heuristics)
		{
			assert(width != 0 && height != 0);
			const float w = static_cast<float>(width);
			const float w_ratio = w / snapshot.width;
			const float h = static_cast<float>(height);
			const float h_ratio = h / snapshot.height;
			const float aspect_ratio = (w / h) - (static_cast<float>(snapshot.width) / snapshot.height);

			if (std::fabs(aspect_ratio) > 0.1f || w_ratio > 1.85f || h_ratio > 1.85f || w_ratio < 0.5f || h_ratio < 0.5f)
				continue; // Not a good fit
		}

		const auto curr_weight = snapshot.total_stats.vertices * (1.2f - static_cast<float>(snapshot.total_stats.drawcalls) / _stats.drawcalls);
		const auto best_weight = best_snapshot.total_stats.vertices * (1.2f - static_cast<float>(best_snapshot.total_stats.drawcalls) / _stats.vertices);
		if (curr_weight >= best_weight)
		{
			best_snapshot = snapshot;
		}
	}


	const GLuint id = best_snapshot.obj | (best_snapshot.target == GL_RENDERBUFFER ? 0x80000000 : 0);
	if (depthstencil_clear_index.first != id)
		release();
	depthstencil_clear_index.first = id;

	if (preserve_depth_buffers && _clear_texture != 0)
	{
		best_snapshot.obj = _clear_texture;
		best_snapshot.level = 0;
		best_snapshot.target = GL_TEXTURE_2D;
	}

	return best_snapshot;
}
#endif
