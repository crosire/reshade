/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "runtime_objects.hpp"

namespace reshade::opengl
{
	struct tex_data
	{
		GLuint id[2] = {};
		GLuint levels = 0;
		GLenum internal_format = GL_NONE;
	};

	struct sampler_data
	{
		GLuint id;
		tex_data *texture;
		bool is_srgb_format;
	};

	struct pass_data
	{
		GLuint fbo = 0;
		GLuint program = 0;
		GLenum blend_eq_color = GL_NONE;
		GLenum blend_eq_alpha = GL_NONE;
		GLenum blend_src = GL_NONE;
		GLenum blend_src_alpha = GL_NONE;
		GLenum blend_dest = GL_NONE;
		GLenum blend_dest_alpha = GL_NONE;
		GLenum stencil_func = GL_NONE;
		GLenum stencil_op_fail = GL_NONE;
		GLenum stencil_op_z_fail = GL_NONE;
		GLenum stencil_op_z_pass = GL_NONE;
		GLenum draw_targets[8] = {};
		std::vector<tex_data *> storages;
		std::vector<sampler_data> samplers;
	};

	struct technique_data
	{
		GLuint query = 0;
		bool query_in_flight = false;
		std::vector<pass_data> passes;
	};
}
