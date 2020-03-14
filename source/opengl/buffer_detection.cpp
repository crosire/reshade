/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "buffer_detection.hpp"
#include <cmath>
#include <cassert>

void reshade::opengl::buffer_detection::reset(GLuint default_width, GLuint default_height, GLenum default_format)
{
	// Reset statistics for next frame
	_stats.vertices = 0;
	_stats.drawcalls = 0;

#if RESHADE_DEPTH
	// Initialize information for the default depth buffer
	_depth_source_table[0] = { 0, 0, default_width, default_height, 0, default_format };

	// Do not clear depth source table, since FBO attachments are usually only created during startup
	for (auto &source : _depth_source_table)
		// Instead only reset the draw call statistics
		source.second.stats = { 0, 0 };
#else
	UNREFERENCED_PARAMETER(default_width);
	UNREFERENCED_PARAMETER(default_height);
	UNREFERENCED_PARAMETER(default_format);
#endif
}

void reshade::opengl::buffer_detection::on_draw(GLsizei vertices)
{
	vertices += _current_vertex_count;
	_current_vertex_count = 0;

	_stats.vertices += vertices;
	_stats.drawcalls += 1;

#if RESHADE_DEPTH
	GLint object = 0;
	GLint target = GL_NONE;
	glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &object);
	if (object != 0) { // Zero is valid too, in which case the default depth buffer is referenced, instead of a FBO
		glGetFramebufferAttachmentParameteriv(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &target);
		if (target == GL_NONE)
			return; // FBO does not have a depth attachment
		glGetFramebufferAttachmentParameteriv(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &object);
	}

	if (const auto it = _depth_source_table.find(object | (target == GL_RENDERBUFFER ? 0x80000000 : 0));
		it != _depth_source_table.end())
	{
		it->second.stats.vertices += vertices;
		it->second.stats.drawcalls += 1;
	}
#endif
}

#if RESHADE_DEPTH
void reshade::opengl::buffer_detection::on_fbo_attachment(GLenum attachment, GLenum target, GLuint object, GLint level)
{
	if (object == 0 || (attachment != GL_DEPTH_ATTACHMENT && attachment != GL_DEPTH_STENCIL_ATTACHMENT))
		return;

	const GLuint id = object | (target == GL_RENDERBUFFER ? 0x80000000 : 0);
	if (_depth_source_table.find(id) != _depth_source_table.end())
		return;

	depthstencil_info info = { object, static_cast<GLuint>(level), 0, 0, target, GL_NONE };

	if (target == GL_RENDERBUFFER)
	{
		GLint previous_rbo = 0;
		glGetIntegerv(GL_RENDERBUFFER_BINDING, &previous_rbo);

		// Get depth-stencil parameters from RBO
		glBindRenderbuffer(GL_RENDERBUFFER, object);

		if (GLenum err = glGetError(); err == GL_NO_ERROR)
		{
			glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_WIDTH, reinterpret_cast<GLint *>(&info.width));
			glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_HEIGHT, reinterpret_cast<GLint *>(&info.height));
			glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_INTERNAL_FORMAT, reinterpret_cast<GLint *>(&info.format));
		}
		else
		{
			// Something went wrong during binding, cannot get valid information from this RBO
			return;
		}

		glBindRenderbuffer(GL_RENDERBUFFER, previous_rbo);
	}
	else
	{
		const auto target_to_binding = [](GLenum target) -> GLenum {
			switch (target)
			{
			default:
			case GL_TEXTURE_2D:
				return GL_TEXTURE_BINDING_2D;
			case GL_TEXTURE_2D_ARRAY:
				return GL_TEXTURE_BINDING_2D_ARRAY;
			case GL_TEXTURE_2D_MULTISAMPLE:
				return GL_TEXTURE_BINDING_2D_MULTISAMPLE;
			case GL_TEXTURE_2D_MULTISAMPLE_ARRAY:
				return GL_TEXTURE_BINDING_2D_MULTISAMPLE_ARRAY;
			case GL_TEXTURE_3D:
				return GL_TEXTURE_BINDING_3D;
			case GL_TEXTURE_CUBE_MAP:
			case GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
			case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
			case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
			case GL_TEXTURE_CUBE_MAP_POSITIVE_X:
			case GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
			case GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
				return GL_TEXTURE_BINDING_CUBE_MAP;
			case GL_TEXTURE_CUBE_MAP_ARRAY:
				return GL_TEXTURE_BINDING_CUBE_MAP_ARRAY;
			}
		};

		const GLenum binding = target_to_binding(target);
		if (binding == GL_TEXTURE_BINDING_3D ||
			binding == GL_TEXTURE_BINDING_CUBE_MAP ||
			binding == GL_TEXTURE_BINDING_CUBE_MAP_ARRAY)
			return; // Ignore attachments that are not two-dimensional

		GLint previous_tex = 0;
		glGetIntegerv(binding, &previous_tex);

		// Get depth-stencil parameters from texture
		glBindTexture(target, object);

		if (GLenum err = glGetError(); err == GL_NO_ERROR)
		{
			glGetTexLevelParameteriv(target, level, GL_TEXTURE_WIDTH, reinterpret_cast<GLint *>(&info.width));
			glGetTexLevelParameteriv(target, level, GL_TEXTURE_HEIGHT, reinterpret_cast<GLint *>(&info.height));
			glGetTexLevelParameteriv(target, level, GL_TEXTURE_INTERNAL_FORMAT, reinterpret_cast<GLint *>(&info.format));
		}
		else
		{
			// Something went wrong during binding, cannot get valid information from this texture
			return;
		}

		glBindTexture(target, previous_tex);
	}

	_depth_source_table.emplace(id, info);
}
void reshade::opengl::buffer_detection::on_delete_fbo_attachment(GLenum target, GLuint object)
{
	if (object == 0)
		return;

	const GLuint id = object | (target == GL_RENDERBUFFER ? 0x80000000 : 0);
	_depth_source_table.erase(id);
}

reshade::opengl::buffer_detection::depthstencil_info reshade::opengl::buffer_detection::find_best_depth_texture(GLuint width, GLuint height, GLuint override) const
{
	GLuint best_match = 0;
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
		if (snapshot.stats.drawcalls == 0 || snapshot.stats.vertices == 0)
			continue; // Skip unused

		if (width != 0 && height != 0)
		{
			const float w = static_cast<float>(width);
			const float w_ratio = w / snapshot.width;
			const float h = static_cast<float>(height);
			const float h_ratio = h / snapshot.height;
			const float aspect_ratio = (w / h) - (static_cast<float>(snapshot.width) / snapshot.height);

			if (std::fabs(aspect_ratio) > 0.1f || w_ratio > 1.85f || h_ratio > 1.85f || w_ratio < 0.5f || h_ratio < 0.5f)
				continue; // Not a good fit
		}

		const auto curr_weight = snapshot.stats.vertices * (1.2f - static_cast<float>(snapshot.stats.drawcalls) / _stats.drawcalls);
		const auto best_weight = best_snapshot.stats.vertices * (1.2f - static_cast<float>(best_snapshot.stats.drawcalls) / _stats.vertices);
		if (curr_weight >= best_weight)
		{
			best_match = image;
			best_snapshot = snapshot;
		}
	}

	return best_snapshot;
}
#endif
