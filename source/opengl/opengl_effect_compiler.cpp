/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "opengl_runtime.hpp"
#include "opengl_effect_compiler.hpp"
#include <assert.h>
#include <iomanip>
#include <fstream>
#include <algorithm>

namespace reshade::opengl
{
	using namespace reshadefx;

	static GLenum literal_to_comp_func(unsigned int value)
	{
		switch (value)
		{
		default:
		case 8:
			return GL_ALWAYS;
		case 1:
			return GL_NEVER;
		case 3:
			return GL_EQUAL;
		case 6:
			return GL_NOTEQUAL;
		case 2:
			return GL_LESS;
		case 4:
			return GL_LEQUAL;
		case 5:
			return GL_GREATER;
		case 7:
			return GL_GEQUAL;
		}
	}
	static GLenum literal_to_blend_eq(unsigned int value)
	{
		switch (value)
		{
		case 1:
			return GL_FUNC_ADD;
		case 2:
			return GL_FUNC_SUBTRACT;
		case 3:
			return GL_FUNC_REVERSE_SUBTRACT;
		case 4:
			return GL_MIN;
		case 5:
			return GL_MAX;
		}

		return GL_NONE;
	}
	static GLenum literal_to_blend_func(unsigned int value)
	{
		switch (value)
		{
		case 0:
			return GL_ZERO;
		case 1:
			return GL_ONE;
		case 2:
			return GL_SRC_COLOR;
		case 3:
			return GL_SRC_ALPHA;
		case 4:
			return GL_ONE_MINUS_SRC_COLOR;
		case 5:
			return GL_ONE_MINUS_SRC_ALPHA;
		case 8:
			return GL_DST_COLOR;
		case 6:
			return GL_DST_ALPHA;
		case 9:
			return GL_ONE_MINUS_DST_COLOR;
		case 7:
			return GL_ONE_MINUS_DST_ALPHA;
		}

		return GL_NONE;
	}
	static GLenum literal_to_stencil_op(unsigned int value)
	{
		switch (value)
		{
		default:
		case 1:
			return GL_KEEP;
		case 0:
			return GL_ZERO;
		case 3:
			return GL_REPLACE;
		case 7:
			return GL_INCR_WRAP;
		case 4:
			return GL_INCR;
		case 8:
			return GL_DECR_WRAP;
		case 5:
			return GL_DECR;
		case 6:
			return GL_INVERT;
		}
	}
	static GLenum literal_to_wrap_mode(texture_address_mode value)
	{
		switch (value)
		{
		case texture_address_mode::wrap:
			return GL_REPEAT;
		case texture_address_mode::mirror:
			return GL_MIRRORED_REPEAT;
		case texture_address_mode::clamp:
			return GL_CLAMP_TO_EDGE;
		case texture_address_mode::border:
			return GL_CLAMP_TO_BORDER;
		}

		return GL_NONE;
	}
	static void literal_to_filter_mode(texture_filter value, GLenum &minfilter, GLenum &magfilter)
	{
		switch (value)
		{
		case texture_filter::min_mag_mip_point:
			minfilter = GL_NEAREST_MIPMAP_NEAREST;
			magfilter = GL_NEAREST;
			break;
		case texture_filter::min_mag_point_mip_linear:
			minfilter = GL_NEAREST_MIPMAP_LINEAR;
			magfilter = GL_NEAREST;
			break;
		case texture_filter::min_point_mag_linear_mip_point:
			minfilter = GL_NEAREST_MIPMAP_NEAREST;
			magfilter = GL_LINEAR;
			break;
		case texture_filter::min_point_mag_mip_linear:
			minfilter = GL_NEAREST_MIPMAP_LINEAR;
			magfilter = GL_LINEAR;
			break;
		case texture_filter::min_linear_mag_mip_point:
			minfilter = GL_LINEAR_MIPMAP_NEAREST;
			magfilter = GL_NEAREST;
			break;
		case texture_filter::min_linear_mag_point_mip_linear:
			minfilter = GL_LINEAR_MIPMAP_LINEAR;
			magfilter = GL_NEAREST;
			break;
		case texture_filter::min_mag_linear_mip_point:
			minfilter = GL_LINEAR_MIPMAP_NEAREST;
			magfilter = GL_LINEAR;
			break;
		case texture_filter::min_mag_mip_linear:
			minfilter = GL_LINEAR_MIPMAP_LINEAR;
			magfilter = GL_LINEAR;
			break;
		}
	}
	static void literal_to_format(texture_format value, GLenum &internalformat, GLenum &internalformatsrgb)
	{
		switch (value)
		{
		case texture_format::r8:
			internalformat = internalformatsrgb = GL_R8;
			break;
		case texture_format::r16f:
			internalformat = internalformatsrgb = GL_R16F;
			break;
		case texture_format::r32f:
			internalformat = internalformatsrgb = GL_R32F;
			break;
		case texture_format::rg8:
			internalformat = internalformatsrgb = GL_RG8;
			break;
		case texture_format::rg16:
			internalformat = internalformatsrgb = GL_RG16;
			break;
		case texture_format::rg16f:
			internalformat = internalformatsrgb = GL_RG16F;
			break;
		case texture_format::rg32f:
			internalformat = internalformatsrgb = GL_RG32F;
			break;
		case texture_format::rgba8:
			internalformat = GL_RGBA8;
			internalformatsrgb = GL_SRGB8_ALPHA8;
			break;
		case texture_format::rgba16:
			internalformat = internalformatsrgb = GL_RGBA16;
			break;
		case texture_format::rgba16f:
			internalformat = internalformatsrgb = GL_RGBA16F;
			break;
		case texture_format::rgba32f:
			internalformat = internalformatsrgb = GL_RGBA32F;
			break;
		case texture_format::dxt1:
			internalformat = 0x83F1; // GL_COMPRESSED_RGBA_S3TC_DXT1_EXT
			internalformatsrgb = 0x8C4D; // GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT
			break;
		case texture_format::dxt3:
			internalformat = 0x83F2; // GL_COMPRESSED_RGBA_S3TC_DXT3_EXT
			internalformatsrgb = 0x8C4E; // GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT
			break;
		case texture_format::dxt5:
			internalformat = 0x83F3; // GL_COMPRESSED_RGBA_S3TC_DXT5_EXT
			internalformatsrgb = 0x8C4F; // GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT
			break;
		case texture_format::latc1:
			internalformat = internalformatsrgb = 0x8C70; // GL_COMPRESSED_LUMINANCE_LATC1_EXT
			break;
		case texture_format::latc2:
			internalformat = internalformatsrgb = 0x8C72; // GL_COMPRESSED_LUMINANCE_ALPHA_LATC2_EXT
			break;
		default:
			internalformat = internalformatsrgb = GL_NONE;
			break;
		}
	}

	static inline uintptr_t align(uintptr_t address, size_t alignment)
	{
		if (address % alignment != 0)
			address += alignment - address % alignment;
		return address;
	}

	static void copy_annotations(const std::unordered_map<std::string, std::pair<type, constant>> &source, std::unordered_map<std::string, variant> &target)
	{
		for (const auto &annotation : source)
			switch (annotation.second.first.base)
			{
			case type::t_int:
				target.insert({ annotation.first, variant(annotation.second.second.as_int[0]) });
				break;
			case type::t_bool:
			case type::t_uint:
				target.insert({ annotation.first, variant(annotation.second.second.as_uint[0]) });
				break;
			case type::t_float:
				target.insert({ annotation.first, variant(annotation.second.second.as_float[0]) });
				break;
			case type::t_string:
				target.insert({ annotation.first, variant(annotation.second.second.string_data) });
				break;
			}
	}

	opengl_effect_compiler::opengl_effect_compiler(opengl_runtime *runtime, const module &module, std::string &errors) :
		_runtime(runtime),
		_success(true),
		_module(&module),
		_errors(errors)
	{
	}

	bool opengl_effect_compiler::run()
	{
		// Compile all entry points
		for (const auto &entry : _module->entry_points)
		{
			compile_entry_point(entry.first, entry.second);
		}

		// No need to setup resources if any of the shaders failed to compile
		if (!_success)
			return false;

		_uniform_storage_offset = _runtime->get_uniform_value_storage().size();

		for (const auto &texture : _module->textures)
		{
			visit_texture(texture);
		}
		for (const auto &sampler : _module->samplers)
		{
			visit_sampler(sampler);
		}
		for (const auto &uniform : _module->uniforms)
		{
			visit_uniform(uniform);
		}
		for (const auto &technique : _module->techniques)
		{
			visit_technique(technique);
		}

		if (_uniform_buffer_size != 0)
		{
			GLuint ubo = 0;
			glGenBuffers(1, &ubo);

			GLint previous = 0;
			glGetIntegerv(GL_UNIFORM_BUFFER_BINDING, &previous);

			glBindBuffer(GL_UNIFORM_BUFFER, ubo);
			glBufferData(GL_UNIFORM_BUFFER, _uniform_buffer_size, _runtime->get_uniform_value_storage().data() + _uniform_storage_offset, GL_DYNAMIC_DRAW);

			glBindBuffer(GL_UNIFORM_BUFFER, previous);

			_runtime->_effect_ubos.emplace_back(ubo, _uniform_buffer_size);
		}

		for (auto &it : vs_entry_points)
			glDeleteShader(it.second);
		vs_entry_points.clear();
		for (auto &it : fs_entry_points)
			glDeleteShader(it.second);
		fs_entry_points.clear();

		return _success;
	}

	void opengl_effect_compiler::error(const std::string &message)
	{
		_success = false;

		_errors += "error: " + message + '\n';
	}
	void opengl_effect_compiler::warning(const std::string &message)
	{
		_errors += "warning: " + message + '\n';
	}

	void opengl_effect_compiler::visit_texture(const texture_info &texture_info)
	{
		const auto existing_texture = _runtime->find_texture(texture_info.unique_name);

		if (existing_texture != nullptr)
		{
			if (texture_info.semantic.empty() && (
				existing_texture->width != texture_info.width ||
				existing_texture->height != texture_info.height ||
				existing_texture->levels != texture_info.levels ||
				existing_texture->format != static_cast<texture_format>(texture_info.format)))
				error(existing_texture->effect_filename + " already created a texture with the same name but different dimensions; textures are shared across all effects, so either rename the variable or adjust the dimensions so they match");
			return;
		}

		texture obj;
		obj.name = texture_info.unique_name;
		obj.unique_name = texture_info.unique_name;
		copy_annotations(texture_info.annotations, obj.annotations);
		obj.width = texture_info.width;
		obj.height = texture_info.height;
		obj.levels = texture_info.levels;
		obj.format = static_cast<texture_format>(texture_info.format);

		GLenum internalformat = GL_RGBA8, internalformat_srgb = GL_SRGB8_ALPHA8;
		literal_to_format(obj.format, internalformat, internalformat_srgb);

		obj.impl = std::make_unique<opengl_tex_data>();
		const auto obj_data = obj.impl->as<opengl_tex_data>();

		if (texture_info.semantic == "COLOR")
		{
			_runtime->update_texture_reference(obj, texture_reference::back_buffer);
		}
		else if (texture_info.semantic == "DEPTH")
		{
			_runtime->update_texture_reference(obj, texture_reference::depth_buffer);
		}
		else if (!texture_info.semantic.empty())
		{
			error("invalid semantic");
			return;
		}
		else
		{
			obj_data->should_delete = true;
			glGenTextures(2, obj_data->id);

			GLint previous = 0, previous_fbo = 0;
			glGetIntegerv(GL_TEXTURE_BINDING_2D, &previous);
			glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &previous_fbo);

			glBindTexture(GL_TEXTURE_2D, obj_data->id[0]);
			glTexStorage2D(GL_TEXTURE_2D, obj.levels, internalformat, obj.width, obj.height);
			glTextureView(obj_data->id[1], GL_TEXTURE_2D, obj_data->id[0], internalformat_srgb, 0, obj.levels, 0, 1);
			glBindTexture(GL_TEXTURE_2D, previous);

			// Clear texture to black
			glBindFramebuffer(GL_DRAW_FRAMEBUFFER, _runtime->_blit_fbo);
			glFramebufferTexture(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, obj_data->id[0], 0);
			glDrawBuffer(GL_COLOR_ATTACHMENT1);
			const GLuint clearColor[4] = { 0, 0, 0, 0 };
			glClearBufferuiv(GL_COLOR, 0, clearColor);
			glFramebufferTexture(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, 0, 0);
			glBindFramebuffer(GL_DRAW_FRAMEBUFFER, previous_fbo);
		}

		_runtime->add_texture(std::move(obj));
	}

	void opengl_effect_compiler::visit_sampler(const sampler_info &sampler_info)
	{
		const auto existing_texture = _runtime->find_texture(sampler_info.texture_name);

		if (!existing_texture)
			return;

		size_t hash = 2166136261;
		hash = (hash * 16777619) ^ sampler_info.address_u;
		hash = (hash * 16777619) ^ sampler_info.address_v;
		hash = (hash * 16777619) ^ sampler_info.address_w;
		hash = (hash * 16777619) ^ sampler_info.filter;
		hash = (hash * 16777619) ^ reinterpret_cast<const uint32_t &>(sampler_info.lod_bias);
		hash = (hash * 16777619) ^ reinterpret_cast<const uint32_t &>(sampler_info.min_lod);
		hash = (hash * 16777619) ^ reinterpret_cast<const uint32_t &>(sampler_info.max_lod);

		auto it = _runtime->_effect_sampler_states.find(hash);

		if (it == _runtime->_effect_sampler_states.end())
		{
			GLenum minfilter = GL_NONE, magfilter = GL_NONE;
			literal_to_filter_mode(static_cast<texture_filter>(sampler_info.filter), minfilter, magfilter);

			GLuint sampler_id = 0;
			glGenSamplers(1, &sampler_id);
			glSamplerParameteri(sampler_id, GL_TEXTURE_WRAP_S, literal_to_wrap_mode(static_cast<texture_address_mode>(sampler_info.address_u)));
			glSamplerParameteri(sampler_id, GL_TEXTURE_WRAP_T, literal_to_wrap_mode(static_cast<texture_address_mode>(sampler_info.address_v)));
			glSamplerParameteri(sampler_id, GL_TEXTURE_WRAP_R, literal_to_wrap_mode(static_cast<texture_address_mode>(sampler_info.address_w)));
			glSamplerParameteri(sampler_id, GL_TEXTURE_MAG_FILTER, magfilter);
			glSamplerParameteri(sampler_id, GL_TEXTURE_MIN_FILTER, minfilter);
			glSamplerParameterf(sampler_id, GL_TEXTURE_LOD_BIAS, sampler_info.lod_bias);
			glSamplerParameterf(sampler_id, GL_TEXTURE_MIN_LOD, sampler_info.min_lod);
			glSamplerParameterf(sampler_id, GL_TEXTURE_MAX_LOD, sampler_info.max_lod);

			it = _runtime->_effect_sampler_states.emplace(hash, sampler_id).first;
		}

		opengl_sampler_data sampler;
		sampler.id = it->second;
		sampler.texture = existing_texture->impl->as<opengl_tex_data>();
		sampler.is_srgb = sampler_info.srgb;
		sampler.has_mipmaps = existing_texture->levels > 1;

		_sampler_bindings.resize(std::max(_sampler_bindings.size(), size_t(sampler_info.binding + 1)));

		_sampler_bindings[sampler_info.binding] = std::move(sampler);
	}

	void opengl_effect_compiler::visit_uniform(const uniform_info &uniform_info)
	{
		uniform obj;
		obj.name = uniform_info.name;
		obj.rows = uniform_info.type.rows;
		obj.columns = uniform_info.type.cols;
		obj.elements = std::max(1, uniform_info.type.array_length);
		obj.storage_size = uniform_info.size;
		obj.storage_offset = _uniform_storage_offset + uniform_info.offset;
		copy_annotations(uniform_info.annotations, obj.annotations);

		switch (uniform_info.type.base)
		{
		case type::t_int:
			obj.displaytype = obj.basetype = uniform_datatype::signed_integer;
			break;
		case type::t_uint:
			obj.displaytype = obj.basetype = uniform_datatype::unsigned_integer;
			break;
		case type::t_float:
			obj.displaytype = obj.basetype = uniform_datatype::floating_point;
			break;
		}

		_uniform_buffer_size = std::max<GLintptr>(_uniform_buffer_size, obj.storage_offset + obj.storage_size);

		auto &uniform_storage = _runtime->get_uniform_value_storage();

		if (obj.storage_offset + obj.storage_size >= static_cast<ptrdiff_t>(uniform_storage.size()))
		{
			uniform_storage.resize(uniform_storage.size() + 128);
		}

		if (uniform_info.has_initializer_value)
		{
			memcpy(uniform_storage.data() + obj.storage_offset, uniform_info.initializer_value.as_float, obj.storage_size);
		}
		else
		{
			memset(uniform_storage.data() + obj.storage_offset, 0, obj.storage_size);
		}

		_runtime->add_uniform(std::move(obj));
	}

	void opengl_effect_compiler::visit_technique(const technique_info &technique_info)
	{
		technique obj;
		obj.impl = std::make_unique<opengl_technique_data>();
		obj.name = technique_info.name;
		copy_annotations(technique_info.annotations, obj.annotations);

		const auto obj_data = obj.impl->as<opengl_technique_data>();

		glGenQueries(1, &obj_data->query);

		if (_uniform_buffer_size != 0)
		{
			obj.uniform_storage_index = _runtime->_effect_ubos.size();
			obj.uniform_storage_offset = _uniform_storage_offset;
		}

		obj_data->samplers = _sampler_bindings;

		for (const auto &pass_info : technique_info.passes)
		{
			auto &pass = static_cast<opengl_pass_data &>(*obj.passes.emplace_back(std::make_unique<opengl_pass_data>()));

			pass.color_mask[0] = (pass_info.color_write_mask & (1 << 0)) != 0;
			pass.color_mask[1] = (pass_info.color_write_mask & (1 << 1)) != 0;
			pass.color_mask[2] = (pass_info.color_write_mask & (1 << 2)) != 0;
			pass.color_mask[3] = (pass_info.color_write_mask & (1 << 3)) != 0;
			pass.stencil_test = pass_info.stencil_enable;
			pass.stencil_read_mask = pass_info.stencil_read_mask;
			pass.stencil_mask = pass_info.stencil_write_mask;
			pass.stencil_func = literal_to_comp_func(pass_info.stencil_comparison_func);
			pass.stencil_op_z_pass = literal_to_stencil_op(pass_info.stencil_op_pass);
			pass.stencil_op_fail = literal_to_stencil_op(pass_info.stencil_op_fail);
			pass.stencil_op_z_fail = literal_to_stencil_op(pass_info.stencil_op_depth_fail);
			pass.blend = pass_info.blend_enable;
			pass.blend_eq_color = literal_to_blend_eq(pass_info.blend_op);
			pass.blend_eq_alpha = literal_to_blend_eq(pass_info.blend_op_alpha);
			pass.blend_src = literal_to_blend_func(pass_info.src_blend);
			pass.blend_dest = literal_to_blend_func(pass_info.dest_blend);
			pass.blend_src_alpha = literal_to_blend_func(pass_info.src_blend_alpha);
			pass.blend_dest_alpha = literal_to_blend_func(pass_info.dest_blend_alpha);
			pass.stencil_reference = pass_info.stencil_reference_value;
			pass.srgb = pass_info.srgb_write_enable;
			pass.clear_render_targets = pass_info.clear_render_targets;

			glGenFramebuffers(1, &pass.fbo);
			glBindFramebuffer(GL_FRAMEBUFFER, pass.fbo);

			bool backbuffer_fbo = true;

			for (unsigned int i = 0; i < 8; ++i)
			{
				const std::string &render_target = pass_info.render_target_names[i];

				if (render_target.empty())
					continue;

				const auto texture = _runtime->find_texture(render_target);

				if (texture == nullptr)
				{
					error("texture not found");
					return;
				}

				if (pass.viewport_width != 0 && pass.viewport_height != 0 && (texture->width != static_cast<unsigned int>(pass.viewport_width) || texture->height != static_cast<unsigned int>(pass.viewport_height)))
				{
					error("cannot use multiple render targets with different sized textures");
					return;
				}
				else
				{
					pass.viewport_width = texture->width;
					pass.viewport_height = texture->height;
				}

				backbuffer_fbo = false;

				const auto texture_data = texture->impl->as<opengl_tex_data>();

				glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, texture_data->id[pass.srgb], 0);

				pass.draw_buffers[i] = GL_COLOR_ATTACHMENT0 + i;
				pass.draw_textures[i] = texture_data->id[pass.srgb];
			}

			if (backbuffer_fbo)
			{
				glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, _runtime->_default_backbuffer_rbo[0]);

				pass.draw_buffers[0] = GL_COLOR_ATTACHMENT0;
				pass.draw_textures[0] = _runtime->_backbuffer_texture[1];

				RECT rect;
				GetClientRect(WindowFromDC(_runtime->_hdc), &rect);

				pass.viewport_width = static_cast<GLsizei>(rect.right - rect.left);
				pass.viewport_height = static_cast<GLsizei>(rect.bottom - rect.top);
			}

			glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, _runtime->_default_backbuffer_rbo[1]);

			assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);

			glBindFramebuffer(GL_FRAMEBUFFER, 0);

			pass.program = glCreateProgram();

			const GLuint vs_shader_id = vs_entry_points[pass_info.vs_entry_point];
			const GLuint fs_shader_id = fs_entry_points[pass_info.ps_entry_point];

			if (vs_shader_id != 0)
				glAttachShader(pass.program, vs_shader_id);
			if (fs_shader_id != 0)
				glAttachShader(pass.program, fs_shader_id);

			glLinkProgram(pass.program);

			if (vs_shader_id != 0)
				glDetachShader(pass.program, vs_shader_id);
			if (fs_shader_id != 0)
				glDetachShader(pass.program, fs_shader_id);

			GLint status = GL_FALSE;

			glGetProgramiv(pass.program, GL_LINK_STATUS, &status);

			if (status == GL_FALSE)
			{
				GLint logsize = 0;
				glGetProgramiv(pass.program, GL_INFO_LOG_LENGTH, &logsize);

				if (logsize != 0)
				{
					std::string log(logsize, '\0');
					glGetProgramInfoLog(pass.program, logsize, nullptr, log.data());

					_errors += log;
				}

				glDeleteProgram(pass.program);
				pass.program = 0;

				error("program linking failed");
				return;
			}
		}

		_runtime->add_technique(std::move(obj));
	}

	void opengl_effect_compiler::compile_entry_point(const std::string &entry_point, bool is_ps)
	{
		const GLuint shader_id = glCreateShader(is_ps ? GL_FRAGMENT_SHADER : GL_VERTEX_SHADER);

#if 0
		glShaderBinary(1, &shader_id, GL_SHADER_BINARY_FORMAT_SPIR_V, _module->spirv.data(), _module->spirv.size() * sizeof(uint32_t));

		glSpecializeShader(shader_id, entry_point.c_str(), 0, nullptr, nullptr);
#else
		std::string defines =
			"#version 450\n"
			"#define ENTRY_POINT_" + entry_point + " 1\n";
		if (!is_ps) // OpenGL does not allow using 'discard' in this profile
			defines += "#define discard\n";

		GLsizei lengths[] = { static_cast<GLsizei>(defines.size()), static_cast<GLsizei>(_module->hlsl.size()) };
		const GLchar *sources[] = { defines.c_str(), _module->hlsl.c_str() };
		glShaderSource(shader_id, 2, sources, lengths);

		glCompileShader(shader_id);
#endif

		GLint status = GL_FALSE;

		glGetShaderiv(shader_id, GL_COMPILE_STATUS, &status);

		if (status == GL_FALSE)
		{
			GLint logsize = 0;
			glGetShaderiv(shader_id, GL_INFO_LOG_LENGTH, &logsize);

			if (logsize != 0)
			{
				std::string log(logsize, '\0');
				glGetShaderInfoLog(shader_id, logsize, nullptr, &log.front());

				_errors += log;
			}

			error("internal shader compilation failed");
			return;
		}

		if (is_ps)
			fs_entry_points[entry_point] = shader_id;
		else
			vs_entry_points[entry_point] = shader_id;
	}
}
