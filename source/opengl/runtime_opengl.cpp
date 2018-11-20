/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "log.hpp"
#include "input.hpp"
#include "runtime_opengl.hpp"
#include <imgui.h>
#include <assert.h>

namespace reshade::opengl
{
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

	static GLenum target_to_binding(GLenum target)
	{
		switch (target)
		{
			case GL_FRAMEBUFFER:
				return GL_FRAMEBUFFER_BINDING;
			case GL_READ_FRAMEBUFFER:
				return GL_READ_FRAMEBUFFER_BINDING;
			case GL_DRAW_FRAMEBUFFER:
				return GL_DRAW_FRAMEBUFFER_BINDING;
			case GL_RENDERBUFFER:
				return GL_RENDERBUFFER_BINDING;
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
			default:
				return GL_NONE;
		}
	}
	static unsigned int get_renderer_id()
	{
		GLint major = 0, minor = 0;
		glGetIntegerv(GL_MAJOR_VERSION, &major);
		glGetIntegerv(GL_MAJOR_VERSION, &minor);

		return 0x10000 | (major << 12) | (minor << 8);
	}

	runtime_opengl::runtime_opengl(HDC device) :
		runtime(get_renderer_id()), _hdc(device)
	{
		assert(device != nullptr);

		_vendor_id = 0;
		_device_id = 0;

		// Get vendor and device information on NVIDIA Optimus devices
		if (GetModuleHandleW(L"nvd3d9wrap.dll") == nullptr &&
			GetModuleHandleW(L"nvd3d9wrapx.dll") == nullptr)
		{
			DISPLAY_DEVICEA dd = { sizeof(dd) };

			for (DWORD i = 0; EnumDisplayDevicesA(nullptr, i, &dd, 0) != FALSE; ++i)
			{
				if ((dd.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE) != 0)
				{
					const std::string id = dd.DeviceID;

					if (id.length() > 20)
					{
						_vendor_id = std::stoi(id.substr(8, 4), nullptr, 16);
						_device_id = std::stoi(id.substr(17, 4), nullptr, 16);
					}
					break;
				}
			}
		}

		// Get vendor and device information on general devices
		if (_vendor_id == 0)
		{
			const std::string name = reinterpret_cast<const char *>(glGetString(GL_VENDOR));

			if (name.find("NVIDIA") != std::string::npos)
			{
				_vendor_id = 0x10DE;
			}
			else if (name.find("AMD") != std::string::npos || name.find("ATI") != std::string::npos)
			{
				_vendor_id = 0x1002;
			}
			else if (name.find("Intel") != std::string::npos)
			{
				_vendor_id = 0x8086;
			}
		}
	}

	bool runtime_opengl::init_backbuffer_texture()
	{
		glGenRenderbuffers(2, _default_backbuffer_rbo);

		glBindRenderbuffer(GL_RENDERBUFFER, _default_backbuffer_rbo[0]);
		glRenderbufferStorage(GL_RENDERBUFFER, GL_SRGB8_ALPHA8, _width, _height);
		glBindRenderbuffer(GL_RENDERBUFFER, _default_backbuffer_rbo[1]);
		glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, _width, _height);

		GLenum status = glGetError();

		glBindRenderbuffer(GL_RENDERBUFFER, 0);

		if (status != GL_NO_ERROR)
		{
			LOG(ERROR) << "Failed to create back buffer RBO with error code " << status;

			glDeleteRenderbuffers(2, _default_backbuffer_rbo);

			return false;
		}

		glGenFramebuffers(1, &_default_backbuffer_fbo);

		glBindFramebuffer(GL_FRAMEBUFFER, _default_backbuffer_fbo);
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, _default_backbuffer_rbo[0]);
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, _default_backbuffer_rbo[1]);

		status = glCheckFramebufferStatus(GL_FRAMEBUFFER);

		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		if (status != GL_FRAMEBUFFER_COMPLETE)
		{
			LOG(ERROR) << "Failed to create back buffer FBO with status code " << status;

			glDeleteFramebuffers(1, &_default_backbuffer_fbo);
			glDeleteRenderbuffers(2, _default_backbuffer_rbo);

			return false;
		}

		glGenTextures(2, _backbuffer_texture);

		glBindTexture(GL_TEXTURE_2D, _backbuffer_texture[0]);
		glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, _width, _height);
		glTextureView(_backbuffer_texture[1], GL_TEXTURE_2D, _backbuffer_texture[0], GL_SRGB8_ALPHA8, 0, 1, 0, 1);

		status = glGetError();

		glBindTexture(GL_TEXTURE_2D, 0);

		if (status != GL_NO_ERROR)
		{
			LOG(ERROR) << "Failed to create back buffer texture with error code " << status;

			glDeleteTextures(2, _backbuffer_texture);
			glDeleteFramebuffers(1, &_default_backbuffer_fbo);
			glDeleteRenderbuffers(2, _default_backbuffer_rbo);

			return false;
		}

		return true;
	}
	bool runtime_opengl::init_default_depth_stencil()
	{
		const depth_source_info defaultdepth = {
			_width,
			_height,
			0,
			GL_DEPTH24_STENCIL8
		};

		_depth_source_table[0] = defaultdepth;

		glGenTextures(1, &_depth_texture);

		glBindTexture(GL_TEXTURE_2D, _depth_texture);
		glTexStorage2D(GL_TEXTURE_2D, 1, defaultdepth.format, defaultdepth.width, defaultdepth.height);

		GLenum status = glGetError();

		glBindTexture(GL_TEXTURE_2D, 0);

		if (status != GL_NO_ERROR)
		{
			LOG(ERROR) << "Failed to create depth texture with error code " << status;

			glDeleteTextures(1, &_depth_texture);
			glDeleteTextures(2, _backbuffer_texture);
			glDeleteFramebuffers(1, &_default_backbuffer_fbo);
			glDeleteRenderbuffers(2, _default_backbuffer_rbo);

			return false;
		}

		return true;
	}
	bool runtime_opengl::init_fx_resources()
	{
		glGenFramebuffers(1, &_blit_fbo);

		glBindFramebuffer(GL_FRAMEBUFFER, _blit_fbo);
		glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, _depth_texture, 0);
		glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, _backbuffer_texture[1], 0);

		GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);

		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		if (status != GL_FRAMEBUFFER_COMPLETE)
		{
			LOG(ERROR) << "Failed to create blit FBO with status code " << status;

			glDeleteFramebuffers(1, &_blit_fbo);
			glDeleteTextures(1, &_depth_texture);
			glDeleteTextures(2, _backbuffer_texture);
			glDeleteFramebuffers(1, &_default_backbuffer_fbo);
			glDeleteRenderbuffers(2, _default_backbuffer_rbo);

			return false;
		}

		glGenVertexArrays(1, &_default_vao);

		return true;
	}

	bool runtime_opengl::on_init(unsigned int width, unsigned int height)
	{
		_width = width;
		_height = height;

		_stateblock.capture();

		// Clear errors
		glGetError();

		// Clear pixel storage modes to defaults
		glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
		glPixelStorei(GL_UNPACK_SWAP_BYTES, GL_FALSE);
		glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
		glPixelStorei(GL_UNPACK_IMAGE_HEIGHT, 0);
		glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
		glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);

		if (!init_backbuffer_texture() ||
			!init_default_depth_stencil() ||
			!init_fx_resources()
#if RESHADE_GUI
			|| !init_imgui_resources()
#endif
			)
		{
			_stateblock.apply();

			return false;
		}

		_stateblock.apply();

		return runtime::on_init(WindowFromDC(_hdc));
	}
	void runtime_opengl::on_reset()
	{
		runtime::on_reset();

		// Destroy resources
		glDeleteVertexArrays(1, &_default_vao);
		glDeleteFramebuffers(1, &_default_backbuffer_fbo);
		glDeleteFramebuffers(1, &_depth_source_fbo);
		glDeleteFramebuffers(1, &_blit_fbo);
		glDeleteRenderbuffers(2, _default_backbuffer_rbo);
		glDeleteTextures(2, _backbuffer_texture);
		glDeleteTextures(1, &_depth_texture);
		glDeleteVertexArrays(1, &_imgui_vao);
		glDeleteBuffers(2, _imgui_vbo);
		glDeleteProgram(_imgui_shader_program);

		_default_vao = 0;
		_default_backbuffer_fbo = 0;
		_depth_source_fbo = 0;
		_blit_fbo = 0;
		_default_backbuffer_rbo[0] = 0;
		_default_backbuffer_rbo[1] = 0;
		_backbuffer_texture[0] = 0;
		_backbuffer_texture[1] = 0;
		_depth_texture = 0;
		_imgui_shader_program = 0;
		_imgui_vao = _imgui_vbo[0] = _imgui_vbo[1] = 0;

		_depth_source = 0;
	}
	void runtime_opengl::on_present()
	{
		if (!_is_initialized)
			return;

		detect_depth_source();

		// Capture states
		_stateblock.capture();

		// Copy frame buffer
		glDisable(GL_SCISSOR_TEST);
		glDisable(GL_FRAMEBUFFER_SRGB);
		glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, _default_backbuffer_fbo);
		glReadBuffer(GL_BACK);
		glDrawBuffer(GL_COLOR_ATTACHMENT0);
		glBlitFramebuffer(0, 0, _width, _height, 0, 0, _width, _height, GL_COLOR_BUFFER_BIT, GL_NEAREST);

		// Copy depth buffer
		glBindFramebuffer(GL_READ_FRAMEBUFFER, _depth_source_fbo);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, _blit_fbo);
		glBlitFramebuffer(0, 0, _width, _height, 0, 0, _width, _height, GL_DEPTH_BUFFER_BIT, GL_NEAREST);

		// Set up clip space to something consistent
		if (gl3wProcs.gl.ClipControl != nullptr)
			glClipControl(GL_LOWER_LEFT, GL_ZERO_TO_ONE);

		// Setup vertex input
		glBindVertexArray(_default_vao);

		// Setup global states
		glDisable(GL_CULL_FACE);
		glDisable(GL_DEPTH_TEST);
		glFrontFace(GL_CCW);
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

		// Apply post processing
		update_and_render_effects();

		// Reset render target and copy to frame buffer
		glDisable(GL_SCISSOR_TEST);
		glDisable(GL_FRAMEBUFFER_SRGB);
		glBindFramebuffer(GL_READ_FRAMEBUFFER, _default_backbuffer_fbo);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
		glReadBuffer(GL_COLOR_ATTACHMENT0);
		glDrawBuffer(GL_BACK);
		glBlitFramebuffer(0, 0, _width, _height, 0, 0, _width, _height, GL_COLOR_BUFFER_BIT, GL_NEAREST);

		// Apply presenting
		runtime::on_present();

		// Apply states
		_stateblock.apply();
	}
	void runtime_opengl::on_draw_call(unsigned int vertices)
	{
		_vertices += vertices;
		_drawcalls += 1;

		GLint fbo = 0, object = 0, objecttarget = GL_NONE;
		glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &fbo);

		if (fbo != 0)
		{
			glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &objecttarget);

			if (objecttarget == GL_NONE)
			{
				return;
			}
			else
			{
				glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &object);
			}
		}

		const auto it = _depth_source_table.find(object | (objecttarget == GL_RENDERBUFFER ? 0x80000000 : 0));

		if (it != _depth_source_table.end())
		{
			it->second.drawcall_count = _drawcalls;
			it->second.vertices_count += vertices;
		}
	}
	void runtime_opengl::on_fbo_attachment(GLenum target, GLenum attachment, GLenum objecttarget, GLuint object, GLint level)
	{
		if (object == 0 || (attachment != GL_DEPTH_ATTACHMENT && attachment != GL_DEPTH_STENCIL_ATTACHMENT))
		{
			return;
		}

		// Get current frame buffer
		GLint fbo = 0;
		glGetIntegerv(target_to_binding(target), &fbo);

		assert(fbo != 0);

		if (static_cast<GLuint>(fbo) == _default_backbuffer_fbo || static_cast<GLuint>(fbo) == _depth_source_fbo || static_cast<GLuint>(fbo) == _blit_fbo)
		{
			return;
		}

		const GLuint id = object | (objecttarget == GL_RENDERBUFFER ? 0x80000000 : 0);
		
		if (_depth_source_table.find(id) != _depth_source_table.end())
		{
			return;
		}

		depth_source_info info = { 0, 0, 0, GL_NONE };

		if (objecttarget == GL_RENDERBUFFER)
		{
			GLint previous = 0;
			glGetIntegerv(GL_RENDERBUFFER_BINDING, &previous);

			// Get depth stencil parameters from render buffer
			glBindRenderbuffer(GL_RENDERBUFFER, object);
			glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_WIDTH, reinterpret_cast<int *>(&info.width));
			glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_HEIGHT, reinterpret_cast<int *>(&info.height));
			glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_INTERNAL_FORMAT, &info.format);

			glBindRenderbuffer(GL_RENDERBUFFER, previous);
		}
		else
		{
			if (objecttarget == GL_TEXTURE)
			{
				objecttarget = GL_TEXTURE_2D;
			}

			GLint previous = 0;
			glGetIntegerv(target_to_binding(objecttarget), &previous);

			// Get depth stencil parameters from texture
			glBindTexture(objecttarget, object);
			info.level = level;
			glGetTexLevelParameteriv(objecttarget, level, GL_TEXTURE_WIDTH, reinterpret_cast<int *>(&info.width));
			glGetTexLevelParameteriv(objecttarget, level, GL_TEXTURE_HEIGHT, reinterpret_cast<int *>(&info.height));
			glGetTexLevelParameteriv(objecttarget, level, GL_TEXTURE_INTERNAL_FORMAT, &info.format);
			
			glBindTexture(objecttarget, previous);
		}

		_depth_source_table.emplace(id, info);
	}

	void runtime_opengl::capture_frame(uint8_t *buffer) const
	{
		glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
		glReadBuffer(GL_BACK);

		glReadPixels(0, 0, static_cast<GLsizei>(_width), static_cast<GLsizei>(_height), GL_RGBA, GL_UNSIGNED_BYTE, buffer);

		// Flip image
		const unsigned int pitch = _width * 4;

		for (unsigned int y = 0; y * 2 < _height; ++y)
		{
			const unsigned int i1 = y * pitch;
			const unsigned int i2 = (_height - 1 - y) * pitch;

			for (unsigned int x = 0; x < pitch; x += 4)
			{
				buffer[i1 + x + 3] = 0xFF;
				buffer[i2 + x + 3] = 0xFF;

				std::swap(buffer[i1 + x + 0], buffer[i2 + x + 0]);
				std::swap(buffer[i1 + x + 1], buffer[i2 + x + 1]);
				std::swap(buffer[i1 + x + 2], buffer[i2 + x + 2]);
			}
		}
	}
	void runtime_opengl::update_texture(texture &texture, const uint8_t *data)
	{
		assert(texture.impl_reference == texture_reference::none);

		const auto texture_impl = texture.impl->as<opengl_tex_data>();

		assert(data != nullptr);
		assert(texture_impl != nullptr);

		glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
		glPixelStorei(GL_UNPACK_IMAGE_HEIGHT, 0);
		glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
		glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
		glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

		GLint previous = 0;
		glGetIntegerv(GL_TEXTURE_BINDING_2D, &previous);

		// Flip image data vertically
		unsigned int stride = texture.width * 4;
		std::vector<uint8_t> data_flipped(data, data + stride * texture.height);
		const auto temp = static_cast<uint8_t *>(alloca(stride));

		for (unsigned int y = 0; 2 * y < texture.height; y++)
		{
			const auto line1 = data_flipped.data() + stride * (y);
			const auto line2 = data_flipped.data() + stride * (texture.height - 1 - y);

			std::memcpy(temp, line1, stride);
			std::memcpy(line1, line2, stride);
			std::memcpy(line2, temp, stride);
		}

		// Bind and update texture
		glBindTexture(GL_TEXTURE_2D, texture_impl->id[0]);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, texture.width, texture.height, GL_RGBA, GL_UNSIGNED_BYTE, data_flipped.data());

		if (texture.levels > 1)
			glGenerateMipmap(GL_TEXTURE_2D);

		glBindTexture(GL_TEXTURE_2D, previous);
	}
	bool runtime_opengl::update_texture_reference(texture &texture)
	{
		GLuint new_reference[2] = {};

		switch (texture.impl_reference)
		{
		case texture_reference::back_buffer:
			new_reference[0] = _backbuffer_texture[0];
			new_reference[1] = _backbuffer_texture[1];
			break;
		case texture_reference::depth_buffer:
			new_reference[0] = _depth_texture;
			new_reference[1] = _depth_texture;
			break;
		default:
			return false;
		}

		const auto texture_impl = texture.impl->as<opengl_tex_data>();

		assert(texture_impl != nullptr);

		if (texture_impl->id[0] == new_reference[0] &&
			texture_impl->id[1] == new_reference[1])
			return true;

		if (texture_impl->should_delete)
			glDeleteTextures(2, texture_impl->id);

		texture_impl->id[0] = new_reference[0];
		texture_impl->id[1] = new_reference[1];
		texture_impl->should_delete = false;

		return true;
	}

	bool runtime_opengl::compile_effect(effect_data &effect)
	{
		// Add specialization constant defines to source code
#if 0
		std::vector<GLuint> spec_constants;
		std::vector<GLuint> spec_constant_values;
		for (const auto &constant : module.spec_constants)
		{
			spec_constants.push_back(constant.offset);
			spec_constant_values.push_back(constant.initializer_value.as_uint[0]);
		}
#else
		std::string spec_constants;
		for (const auto &constant : effect.module.spec_constants)
		{
			spec_constants += "#define SPEC_CONSTANT_" + constant.name + ' ';

			switch (constant.type.base)
			{
			case reshadefx::type::t_int:
				spec_constants += std::to_string(constant.initializer_value.as_int[0]);
				break;
			case reshadefx::type::t_bool:
			case reshadefx::type::t_uint:
				spec_constants += std::to_string(constant.initializer_value.as_uint[0]);
				break;
			case reshadefx::type::t_float:
				spec_constants += std::to_string(constant.initializer_value.as_float[0]);
				break;
			}

			spec_constants += '\n';
		}
#endif

		std::unordered_map<std::string, GLuint> entry_points;

		// Compile all entry points
		for (const auto &entry_point : effect.module.entry_points)
		{
			GLuint shader_id = glCreateShader(entry_point.second ? GL_FRAGMENT_SHADER : GL_VERTEX_SHADER);

#if 0
			glShaderBinary(1, &shader_id, GL_SHADER_BINARY_FORMAT_SPIR_V, module.spirv.data(), module.spirv.size() * sizeof(uint32_t));

			glSpecializeShader(shader_id, entry_point.first.c_str(), GLuint(spec_constants.size()), spec_constants.data(), spec_constant_values.data());
#else
			std::string defines =
				"#version 450\n"
				"#define ENTRY_POINT_" + entry_point.first + " 1\n";
			if (!entry_point.second) // OpenGL does not allow using 'discard' in the vertex shader profile
				defines += "#define discard\n"
					"#define dFdx(x) x\n" // 'dFdx' and 'dFdx' too are only available in fragment shaders
					"#define dFdy(y) y\n";
			defines += spec_constants;

			GLsizei lengths[] = { static_cast<GLsizei>(defines.size()), static_cast<GLsizei>(effect.module.hlsl.size()) };
			const GLchar *sources[] = { defines.c_str(), effect.module.hlsl.c_str() };
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

					effect.errors += log;
				}
				else
				{
					effect.errors += "error: internal shader compilation failed\n";
				}

				for (auto &it : entry_points)
					glDeleteShader(it.second);

				// No need to setup resources if any of the shaders failed to compile
				return false;
			}

			entry_points[entry_point.first] = shader_id;
		}

		if (effect.storage_size != 0)
		{
			GLuint ubo = 0;
			glGenBuffers(1, &ubo);

			GLint previous = 0;
			glGetIntegerv(GL_UNIFORM_BUFFER_BINDING, &previous);

			glBindBuffer(GL_UNIFORM_BUFFER, ubo);
			glBufferData(GL_UNIFORM_BUFFER, effect.storage_size, _uniform_data_storage.data() + effect.storage_offset, GL_DYNAMIC_DRAW);

			glBindBuffer(GL_UNIFORM_BUFFER, previous);

			_effect_ubos.emplace_back(ubo, effect.storage_size);
		}

		bool success = true;

		opengl_technique_data technique_init;
		technique_init.uniform_storage_index = _effect_ubos.size() - 1;
		technique_init.uniform_storage_offset = effect.storage_offset;

		for (const reshadefx::sampler_info &info : effect.module.samplers)
			success &= add_sampler(info, technique_init);

		for (technique &technique : _techniques)
			if (technique.impl == nullptr && technique.effect_index == effect.index)
				success &= init_technique(technique, technique_init, entry_points, effect.errors);

		for (auto &it : entry_points)
			glDeleteShader(it.second);

		return success;
	}
	void runtime_opengl::unload_effects()
	{
		runtime::unload_effects();

		for (const auto &it : _effect_sampler_states)
		{
			glDeleteSamplers(1, &it.second);
		}

		_effect_sampler_states.clear();

		for (auto &uniform_buffer : _effect_ubos)
		{
			glDeleteBuffers(1, &uniform_buffer.first);
		}

		_effect_ubos.clear();
	}

	bool runtime_opengl::add_sampler(const reshadefx::sampler_info &info, opengl_technique_data &technique_init)
	{
		const auto existing_texture = std::find_if(_textures.begin(), _textures.end(),
			[&texture_name = info.texture_name](const auto &item) {
			return item.unique_name == texture_name;
		});

		if (existing_texture == _textures.end())
			return false;

		size_t hash = 2166136261;
		hash = (hash * 16777619) ^ static_cast<uint32_t>(info.address_u);
		hash = (hash * 16777619) ^ static_cast<uint32_t>(info.address_v);
		hash = (hash * 16777619) ^ static_cast<uint32_t>(info.address_w);
		hash = (hash * 16777619) ^ static_cast<uint32_t>(info.filter);
		hash = (hash * 16777619) ^ reinterpret_cast<const uint32_t &>(info.lod_bias);
		hash = (hash * 16777619) ^ reinterpret_cast<const uint32_t &>(info.min_lod);
		hash = (hash * 16777619) ^ reinterpret_cast<const uint32_t &>(info.max_lod);

		auto it = _effect_sampler_states.find(hash);

		if (it == _effect_sampler_states.end())
		{
			GLenum minfilter = GL_NONE, magfilter = GL_NONE;

			switch (info.filter)
			{
			case reshadefx::texture_filter::min_mag_mip_point:
				minfilter = GL_NEAREST_MIPMAP_NEAREST;
				magfilter = GL_NEAREST;
				break;
			case reshadefx::texture_filter::min_mag_point_mip_linear:
				minfilter = GL_NEAREST_MIPMAP_LINEAR;
				magfilter = GL_NEAREST;
				break;
			case reshadefx::texture_filter::min_point_mag_linear_mip_point:
				minfilter = GL_NEAREST_MIPMAP_NEAREST;
				magfilter = GL_LINEAR;
				break;
			case reshadefx::texture_filter::min_point_mag_mip_linear:
				minfilter = GL_NEAREST_MIPMAP_LINEAR;
				magfilter = GL_LINEAR;
				break;
			case reshadefx::texture_filter::min_linear_mag_mip_point:
				minfilter = GL_LINEAR_MIPMAP_NEAREST;
				magfilter = GL_NEAREST;
				break;
			case reshadefx::texture_filter::min_linear_mag_point_mip_linear:
				minfilter = GL_LINEAR_MIPMAP_LINEAR;
				magfilter = GL_NEAREST;
				break;
			case reshadefx::texture_filter::min_mag_linear_mip_point:
				minfilter = GL_LINEAR_MIPMAP_NEAREST;
				magfilter = GL_LINEAR;
				break;
			case reshadefx::texture_filter::min_mag_mip_linear:
				minfilter = GL_LINEAR_MIPMAP_LINEAR;
				magfilter = GL_LINEAR;
				break;
			}

			const auto convert_address_mode = [](reshadefx::texture_address_mode value) {
				switch (value)
				{
				case reshadefx::texture_address_mode::wrap:
					return GL_REPEAT;
				case reshadefx::texture_address_mode::mirror:
					return GL_MIRRORED_REPEAT;
				case reshadefx::texture_address_mode::clamp:
					return GL_CLAMP_TO_EDGE;
				case reshadefx::texture_address_mode::border:
					return GL_CLAMP_TO_BORDER;
				default:
					return GL_NONE;
				}
			};

			GLuint sampler_id = 0;
			glGenSamplers(1, &sampler_id);
			glSamplerParameteri(sampler_id, GL_TEXTURE_WRAP_S, convert_address_mode(info.address_u));
			glSamplerParameteri(sampler_id, GL_TEXTURE_WRAP_T, convert_address_mode(info.address_v));
			glSamplerParameteri(sampler_id, GL_TEXTURE_WRAP_R, convert_address_mode(info.address_w));
			glSamplerParameteri(sampler_id, GL_TEXTURE_MAG_FILTER, magfilter);
			glSamplerParameteri(sampler_id, GL_TEXTURE_MIN_FILTER, minfilter);
			glSamplerParameterf(sampler_id, GL_TEXTURE_LOD_BIAS, info.lod_bias);
			glSamplerParameterf(sampler_id, GL_TEXTURE_MIN_LOD, info.min_lod);
			glSamplerParameterf(sampler_id, GL_TEXTURE_MAX_LOD, info.max_lod);

			it = _effect_sampler_states.emplace(hash, sampler_id).first;
		}

		opengl_sampler_data sampler;
		sampler.id = it->second;
		sampler.texture = existing_texture->impl->as<opengl_tex_data>();
		sampler.is_srgb = info.srgb;
		sampler.has_mipmaps = existing_texture->levels > 1;

		technique_init.samplers.resize(std::max(technique_init.samplers.size(), size_t(info.binding + 1)));

		technique_init.samplers[info.binding] = std::move(sampler);

		return true;
	}
	bool runtime_opengl::init_texture(texture &texture)
	{
		texture.impl = std::make_unique<opengl_tex_data>();

		const auto texture_data = texture.impl->as<opengl_tex_data>();

		if (texture.impl_reference != texture_reference::none)
			return update_texture_reference(texture);

		GLenum internalformat = GL_RGBA8, internalformat_srgb = GL_SRGB8_ALPHA8;

		switch (texture.format)
		{
		case reshadefx::texture_format::r8:
			internalformat = internalformat_srgb = GL_R8;
			break;
		case reshadefx::texture_format::r16f:
			internalformat = internalformat_srgb = GL_R16F;
			break;
		case reshadefx::texture_format::r32f:
			internalformat = internalformat_srgb = GL_R32F;
			break;
		case reshadefx::texture_format::rg8:
			internalformat = internalformat_srgb = GL_RG8;
			break;
		case reshadefx::texture_format::rg16:
			internalformat = internalformat_srgb = GL_RG16;
			break;
		case reshadefx::texture_format::rg16f:
			internalformat = internalformat_srgb = GL_RG16F;
			break;
		case reshadefx::texture_format::rg32f:
			internalformat = internalformat_srgb = GL_RG32F;
			break;
		case reshadefx::texture_format::rgba8:
			internalformat = GL_RGBA8;
			internalformat_srgb = GL_SRGB8_ALPHA8;
			break;
		case reshadefx::texture_format::rgba16:
			internalformat = internalformat_srgb = GL_RGBA16;
			break;
		case reshadefx::texture_format::rgba16f:
			internalformat = internalformat_srgb = GL_RGBA16F;
			break;
		case reshadefx::texture_format::rgba32f:
			internalformat = internalformat_srgb = GL_RGBA32F;
			break;
		case reshadefx::texture_format::dxt1:
			internalformat = GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
			internalformat_srgb = 0x8C4D; // GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT
			break;
		case reshadefx::texture_format::dxt3:
			internalformat = GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
			internalformat_srgb = 0x8C4E; // GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT
			break;
		case reshadefx::texture_format::dxt5:
			internalformat = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
			internalformat_srgb = 0x8C4F; // GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT
			break;
		case reshadefx::texture_format::latc1:
			internalformat = internalformat_srgb = 0x8C70; // GL_COMPRESSED_LUMINANCE_LATC1_EXT
			break;
		case reshadefx::texture_format::latc2:
			internalformat = internalformat_srgb = 0x8C72; // GL_COMPRESSED_LUMINANCE_ALPHA_LATC2_EXT
			break;
		default:
			internalformat = internalformat_srgb = GL_NONE;
			break;
		}

		texture_data->should_delete = true;
		glGenTextures(2, texture_data->id);

		GLint previous = 0, previous_fbo = 0;
		glGetIntegerv(GL_TEXTURE_BINDING_2D, &previous);
		glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &previous_fbo);

		glBindTexture(GL_TEXTURE_2D, texture_data->id[0]);
		glTexStorage2D(GL_TEXTURE_2D, texture.levels, internalformat, texture.width, texture.height);
		glTextureView(texture_data->id[1], GL_TEXTURE_2D, texture_data->id[0], internalformat_srgb, 0, texture.levels, 0, 1);
		glBindTexture(GL_TEXTURE_2D, previous);

		// Clear texture to black
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, _blit_fbo);
		glFramebufferTexture(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, texture_data->id[0], 0);
		glDrawBuffer(GL_COLOR_ATTACHMENT1);
		const GLuint clear_color[4] = { 0, 0, 0, 0 };
		glClearBufferuiv(GL_COLOR, 0, clear_color);
		glFramebufferTexture(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, 0, 0);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, previous_fbo);

		return true;
	}
	bool runtime_opengl::init_technique(technique &technique, const opengl_technique_data &impl_init, const std::unordered_map<std::string, GLuint> &entry_points, std::string &errors)
	{
		technique.impl = std::make_unique<opengl_technique_data>(impl_init);

		const auto technique_data = technique.impl->as<opengl_technique_data>();

		glGenQueries(1, &technique_data->query);

		for (size_t pass_index = 0; pass_index < technique.passes.size(); ++pass_index)
		{
			technique.passes_data.push_back(std::make_unique<opengl_pass_data>());

			auto &pass = *technique.passes_data.back()->as<opengl_pass_data>();
			const auto &pass_info = technique.passes[pass_index];

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
			pass.viewport_width = pass_info.viewport_width;
			pass.viewport_height = pass_info.viewport_height;

			glGenFramebuffers(1, &pass.fbo);
			glBindFramebuffer(GL_FRAMEBUFFER, pass.fbo);

			bool backbuffer_fbo = true;

			for (unsigned int k = 0; k < 8; ++k)
			{
				if (pass_info.render_target_names[k].empty())
					continue; // Skip unbound render targets

				const auto render_target_texture = std::find_if(_textures.begin(), _textures.end(),
					[&render_target = pass_info.render_target_names[k]](const auto &item) {
					return item.unique_name == render_target;
				});

				if (render_target_texture == _textures.end())
					return assert(false), false;

				backbuffer_fbo = false;

				const auto texture_impl = render_target_texture->impl->as<opengl_tex_data>();

				assert(texture_impl != nullptr);

				glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + k, texture_impl->id[pass.srgb], 0);

				pass.draw_buffers[k] = GL_COLOR_ATTACHMENT0 + k;
				pass.draw_textures[k] = texture_impl->id[pass.srgb];
			}

			if (backbuffer_fbo)
			{
				glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, _default_backbuffer_rbo[0]);

				pass.draw_buffers[0] = GL_COLOR_ATTACHMENT0;
				pass.draw_textures[0] = _backbuffer_texture[1];

				RECT rect;
				GetClientRect(WindowFromDC(_hdc), &rect);

				pass.viewport_width = static_cast<GLsizei>(rect.right - rect.left);
				pass.viewport_height = static_cast<GLsizei>(rect.bottom - rect.top);
			}

			assert(pass.viewport_width != 0);
			assert(pass.viewport_height != 0);

			glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, _default_backbuffer_rbo[1]);

			assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);

			glBindFramebuffer(GL_FRAMEBUFFER, 0);

			pass.program = glCreateProgram();

			const GLuint vs_shader_id = entry_points.at(pass_info.vs_entry_point);
			const GLuint fs_shader_id = entry_points.at(pass_info.ps_entry_point);

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

					errors += log;
				}

				glDeleteProgram(pass.program);
				pass.program = 0;

				LOG(ERROR) << "Failed to link program for pass " << pass_index << " in technique '" << technique.name << "'.";
				return false;
			}
		}

		return true;
	}

	void runtime_opengl::render_technique(const technique &technique)
	{
		opengl_technique_data &technique_data = *technique.impl->as<opengl_technique_data>();

		// Evaluate queries
		if (technique_data.query_in_flight)
		{
			GLuint64 elapsed_time = 0;
			glGetQueryObjectui64v(technique_data.query, GL_QUERY_RESULT, &elapsed_time);

			const_cast<struct technique &>(technique).average_gpu_duration.append(elapsed_time);
			technique_data.query_in_flight = false;
		}

		if (!technique_data.query_in_flight)
			glBeginQuery(GL_TIME_ELAPSED, technique_data.query);

		// Clear depth stencil
		glBindFramebuffer(GL_FRAMEBUFFER, _default_backbuffer_fbo);
		glClearBufferfi(GL_DEPTH_STENCIL, 0, 1.0f, 0);

		// Setup shader constants
		if (technique_data.uniform_storage_index >= 0)
		{
			glBindBufferBase(GL_UNIFORM_BUFFER, 0, _effect_ubos[technique_data.uniform_storage_index].first);
			glBufferSubData(GL_UNIFORM_BUFFER, 0, _effect_ubos[technique_data.uniform_storage_index].second, _uniform_data_storage.data() + technique_data.uniform_storage_offset);
		}

		// Setup shader resources
		for (GLsizei i = 0; i < static_cast<GLsizei>(technique_data.samplers.size()); i++)
		{
			glActiveTexture(GL_TEXTURE0 + i);
			glBindTexture(GL_TEXTURE_2D, technique_data.samplers[i].texture->id[technique_data.samplers[i].is_srgb]);
			glBindSampler(i, technique_data.samplers[i].id);
		}

		for (const auto &pass_object : technique.passes_data)
		{
			const opengl_pass_data &pass = *pass_object->as<opengl_pass_data>();

			// Save frame buffer of previous pass
			glDisable(GL_FRAMEBUFFER_SRGB);
			glBindFramebuffer(GL_READ_FRAMEBUFFER, _default_backbuffer_fbo);
			glBindFramebuffer(GL_DRAW_FRAMEBUFFER, _blit_fbo);
			glReadBuffer(GL_COLOR_ATTACHMENT0);
			glDrawBuffer(GL_COLOR_ATTACHMENT0);
			glBlitFramebuffer(0, 0, _width, _height, 0, 0, _width, _height, GL_COLOR_BUFFER_BIT, GL_NEAREST);

			// Setup states
			glUseProgram(pass.program);
			glColorMask(pass.color_mask[0], pass.color_mask[1], pass.color_mask[2], pass.color_mask[3]);
			glBlendFuncSeparate(pass.blend_src, pass.blend_dest, pass.blend_src_alpha, pass.blend_dest_alpha);
			glBlendEquationSeparate(pass.blend_eq_color, pass.blend_eq_alpha);
			glStencilFunc(pass.stencil_func, pass.stencil_reference, pass.stencil_read_mask);
			glStencilOp(pass.stencil_op_fail, pass.stencil_op_z_fail, pass.stencil_op_z_pass);
			glStencilMask(pass.stencil_mask);

			if (pass.srgb)
			{
				glEnable(GL_FRAMEBUFFER_SRGB);
			}
			else
			{
				glDisable(GL_FRAMEBUFFER_SRGB);
			}

			if (pass.blend)
			{
				glEnable(GL_BLEND);
			}
			else
			{
				glDisable(GL_BLEND);
			}

			if (pass.stencil_test)
			{
				glEnable(GL_STENCIL_TEST);
			}
			else
			{
				glDisable(GL_STENCIL_TEST);
			}

			// Setup render targets
			glBindFramebuffer(GL_FRAMEBUFFER, pass.fbo);
			glDrawBuffers(8, pass.draw_buffers);
			glViewport(0, 0, pass.viewport_width, pass.viewport_height);

			if (pass.clear_render_targets)
			{
				for (GLuint k = 0; k < 8; k++)
				{
					if (pass.draw_buffers[k] == GL_NONE)
					{
						continue;
					}

					const GLfloat color[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
					glClearBufferfv(GL_COLOR, k, color);
				}
			}

			// Draw triangle
			glDrawArrays(GL_TRIANGLES, 0, 3);

			_vertices += 3;
			_drawcalls += 1;

			// Update shader resources
			for (GLuint texture_id : pass.draw_textures)
			{
				for (GLsizei i = 0; i < static_cast<GLsizei>(technique_data.samplers.size()); i++)
				{
					const auto texture = technique_data.samplers[i].texture;

					if (technique_data.samplers[i].has_mipmaps && (texture->id[0] == texture_id || texture->id[1] == texture_id))
					{
						glActiveTexture(GL_TEXTURE0 + i);
						glGenerateMipmap(GL_TEXTURE_2D);
					}
				}
			}
		}

		if (!technique_data.query_in_flight)
			glEndQuery(GL_TIME_ELAPSED);
		technique_data.query_in_flight = true;
	}

#if RESHADE_GUI
	bool runtime_opengl::init_imgui_resources()
	{
		const GLchar *vertex_shader[] = {
			"#version 330\n"
			"uniform mat4 ProjMtx;\n"
			"in vec2 Position, UV;\n"
			"in vec4 Color;\n"
			"out vec2 Frag_UV;\n"
			"out vec4 Frag_Color;\n"
			"void main()\n"
			"{\n"
			"	Frag_UV = UV * vec2(1.0, -1.0) + vec2(0.0, 1.0);\n" // Texture coordinates were flipped in 'update_texture'
			"	Frag_Color = Color;\n"
			"	gl_Position = ProjMtx * vec4(Position.xy, 0, 1);\n"
			"}\n"
		};
		const GLchar *fragment_shader[] = {
			"#version 330\n"
			"uniform sampler2D Texture;\n"
			"in vec2 Frag_UV;\n"
			"in vec4 Frag_Color;\n"
			"out vec4 Out_Color;\n"
			"void main()\n"
			"{\n"
			"	Out_Color = Frag_Color * texture(Texture, Frag_UV.st);\n"
			"}\n"
		};

		_imgui_shader_program = glCreateProgram();
		const auto imgui_vs = glCreateShader(GL_VERTEX_SHADER);
		const auto imgui_fs = glCreateShader(GL_FRAGMENT_SHADER);
		glShaderSource(imgui_vs, 1, vertex_shader, 0);
		glShaderSource(imgui_fs, 1, fragment_shader, 0);
		glCompileShader(imgui_vs);
		glCompileShader(imgui_fs);
		glAttachShader(_imgui_shader_program, imgui_vs);
		glAttachShader(_imgui_shader_program, imgui_fs);
		glLinkProgram(_imgui_shader_program);
		glDeleteShader(imgui_vs);
		glDeleteShader(imgui_fs);

		_imgui_attribloc_tex = glGetUniformLocation(_imgui_shader_program, "Texture");
		_imgui_attribloc_projmtx = glGetUniformLocation(_imgui_shader_program, "ProjMtx");
		_imgui_attribloc_pos = glGetAttribLocation(_imgui_shader_program, "Position");
		_imgui_attribloc_uv = glGetAttribLocation(_imgui_shader_program, "UV");
		_imgui_attribloc_color = glGetAttribLocation(_imgui_shader_program, "Color");

		glGenBuffers(2, _imgui_vbo);

		glGenVertexArrays(1, &_imgui_vao);
		glBindVertexArray(_imgui_vao);
		glBindBuffer(GL_ARRAY_BUFFER, _imgui_vbo[0]);
		glEnableVertexAttribArray(_imgui_attribloc_pos);
		glEnableVertexAttribArray(_imgui_attribloc_uv);
		glEnableVertexAttribArray(_imgui_attribloc_color);
		glVertexAttribPointer(_imgui_attribloc_pos, 2, GL_FLOAT, GL_FALSE, sizeof(ImDrawVert), reinterpret_cast<GLvoid *>(offsetof(ImDrawVert, pos)));
		glVertexAttribPointer(_imgui_attribloc_uv, 2, GL_FLOAT, GL_FALSE, sizeof(ImDrawVert), reinterpret_cast<GLvoid *>(offsetof(ImDrawVert, uv)));
		glVertexAttribPointer(_imgui_attribloc_color, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(ImDrawVert), reinterpret_cast<GLvoid *>(offsetof(ImDrawVert, col)));

		return true;
	}

	void runtime_opengl::render_imgui_draw_data(ImDrawData *draw_data)
	{
		glEnable(GL_BLEND);
		glBlendEquation(GL_FUNC_ADD);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glEnable(GL_SCISSOR_TEST);
		glDisable(GL_STENCIL_TEST);
		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		glActiveTexture(GL_TEXTURE0);
		glUseProgram(_imgui_shader_program);
		glBindSampler(0, 0);
		glBindVertexArray(_imgui_vao);
		glViewport(0, 0, _width, _height);

		const float ortho_projection[16] = {
			2.0f / _width, 0.0f, 0.0f, 0.0f,
			0.0f, -2.0f / _height, 0.0f, 0.0f,
			0.0f, 0.0f, -1.0f, 0.0f,
			-1.0f, 1.0f, 0.0f, 1.0f
		};

		glUniform1i(_imgui_attribloc_tex, 0);
		glUniformMatrix4fv(_imgui_attribloc_projmtx, 1, GL_FALSE, ortho_projection);

		// Render command lists
		for (int n = 0; n < draw_data->CmdListsCount; n++)
		{
			const ImDrawIdx *idx_buffer_offset = 0;
			ImDrawList *const cmd_list = draw_data->CmdLists[n];

			glBindBuffer(GL_ARRAY_BUFFER, _imgui_vbo[0]);
			glBufferData(GL_ARRAY_BUFFER, cmd_list->VtxBuffer.size() * sizeof(ImDrawVert), &cmd_list->VtxBuffer.front(), GL_STREAM_DRAW);

			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _imgui_vbo[1]);
			glBufferData(GL_ELEMENT_ARRAY_BUFFER, cmd_list->IdxBuffer.size() * sizeof(ImDrawIdx), &cmd_list->IdxBuffer.front(), GL_STREAM_DRAW);

			for (ImDrawCmd *cmd = cmd_list->CmdBuffer.begin(); cmd != cmd_list->CmdBuffer.end(); idx_buffer_offset += cmd->ElemCount, cmd++)
			{
				glScissor(
					static_cast<GLint>(cmd->ClipRect.x),
					static_cast<GLint>(_height - cmd->ClipRect.w),
					static_cast<GLint>(cmd->ClipRect.z - cmd->ClipRect.x),
					static_cast<GLint>(cmd->ClipRect.w - cmd->ClipRect.y));
				glBindTexture(GL_TEXTURE_2D, static_cast<const opengl_tex_data *>(cmd->TextureId)->id[0]);

				glDrawElements(GL_TRIANGLES, cmd->ElemCount, sizeof(ImDrawIdx) == 2 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT, idx_buffer_offset);
			}
		}
	}
#endif

	void runtime_opengl::detect_depth_source()
	{
		if (_framecount % 30)
			return;

		if (_has_high_network_activity)
		{
			_depth_source = 0;
			create_depth_texture(0, 0, GL_NONE);
			return;
		}

		GLuint best_match = 0;
		depth_source_info best_info = { 0, 0, 0, GL_NONE };

		for (auto it = _depth_source_table.begin(); it != _depth_source_table.end(); ++it)
		{
			const auto depthstencil = it->first;
			auto &depthstencil_info = it->second;

			if (depthstencil_info.drawcall_count == 0)
			{
				continue;
			}

			if (((depthstencil_info.vertices_count * (1.2f - float(depthstencil_info.drawcall_count) / _drawcalls)) >= (best_info.vertices_count * (1.2f - float(best_info.drawcall_count) / _drawcalls))) &&
				((depthstencil_info.width > _width * 0.95 && depthstencil_info.width < _width * 1.05) && (depthstencil_info.height > _height * 0.95 && depthstencil_info.height < _height * 1.05)))
			{
				best_match = depthstencil;
				best_info = depthstencil_info;
			}

			depthstencil_info.drawcall_count = depthstencil_info.vertices_count = 0;
		}

		if (best_match == 0)
		{
			best_info = _depth_source_table.at(0);
		}

		if (_depth_source != best_match || _depth_texture == 0)
		{
			const auto &previous_info = _depth_source_table.at(_depth_source);

			if ((best_info.width != previous_info.width || best_info.height != previous_info.height || best_info.format != previous_info.format) || _depth_texture == 0)
			{
				// Resize depth texture
				create_depth_texture(best_info.width, best_info.height, best_info.format);
			}

			GLint previous_fbo = 0;
			glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previous_fbo);

			glBindFramebuffer(GL_FRAMEBUFFER, _blit_fbo);
			glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, _depth_texture, 0);

			assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);

			_depth_source = best_match;

			if (best_match != 0)
			{
				if (_depth_source_fbo == 0)
				{
					glGenFramebuffers(1, &_depth_source_fbo);
				}

				glBindFramebuffer(GL_FRAMEBUFFER, _depth_source_fbo);

				if ((best_match & 0x80000000) != 0)
				{
					best_match ^= 0x80000000;

					glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, best_match);
				}
				else
				{
					glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, best_match, best_info.level);
				}

				const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);

				if (status != GL_FRAMEBUFFER_COMPLETE)
				{
					LOG(ERROR) << "Failed to create depth source frame buffer with status code " << status << ".";

					glDeleteFramebuffers(1, &_depth_source_fbo);
					_depth_source_fbo = 0;
				}
			}
			else
			{
				glDeleteFramebuffers(1, &_depth_source_fbo);
				_depth_source_fbo = 0;
			}

			glBindFramebuffer(GL_FRAMEBUFFER, previous_fbo);
		}
	}

	void runtime_opengl::create_depth_texture(GLuint width, GLuint height, GLenum format)
	{
		glDeleteTextures(1, &_depth_texture);

		if (format != GL_NONE)
		{
			GLint previousTex = 0;
			glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTex);

			// Clear errors
			glGetError();

			glGenTextures(1, &_depth_texture);

			if (format == GL_DEPTH_STENCIL)
			{
				format = GL_UNSIGNED_INT_24_8;
			}

			glBindTexture(GL_TEXTURE_2D, _depth_texture);
			glTexStorage2D(GL_TEXTURE_2D, 1, format, width, height);

			GLenum status = glGetError();

			if (status != GL_NO_ERROR)
			{
				LOG(ERROR) << "Failed to create depth texture for format " << format << " with error code " << status;

				glDeleteTextures(1, &_depth_texture);

				_depth_texture = 0;
			}

			glBindTexture(GL_TEXTURE_2D, previousTex);
		}
		else
		{
			_depth_texture = 0;
		}

		// Update effect textures
		for (auto &tex : _textures)
			if (tex.impl != nullptr && tex.impl_reference == texture_reference::depth_buffer)
				update_texture_reference(tex);
	}
}
