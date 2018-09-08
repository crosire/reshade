/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "log.hpp"
#include "opengl_runtime.hpp"
#include "opengl_effect_compiler.hpp"
#include "input.hpp"
#include <imgui.h>
#include <assert.h>

namespace reshade::opengl
{
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

	opengl_runtime::opengl_runtime(HDC device) :
		runtime(get_renderer_id()), _hdc(device)
	{
		assert(device != nullptr);

		_vendor_id = 0;
		_device_id = 0;
		_input = input::register_window(WindowFromDC(_hdc));

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

	bool opengl_runtime::init_backbuffer_texture()
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
	bool opengl_runtime::init_default_depth_stencil()
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
	bool opengl_runtime::init_fx_resources()
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
	bool opengl_runtime::init_imgui_resources()
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
			"	Frag_UV = UV;\n"
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
	bool opengl_runtime::init_imgui_font_atlas()
	{
		int width, height;
		unsigned char *pixels;

		ImGui::SetCurrentContext(_imgui_context);
		ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

		GLuint font_atlas_id = 0;

		glGenTextures(1, &font_atlas_id);
		glBindTexture(GL_TEXTURE_2D, font_atlas_id);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

		opengl_tex_data obj = { };
		obj.id[0] = font_atlas_id;

		_imgui_font_atlas_texture = std::make_unique<opengl_tex_data>(obj);

		return true;
	}

	bool opengl_runtime::on_init(unsigned int width, unsigned int height)
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
			!init_fx_resources() ||
			!init_imgui_resources() ||
			!init_imgui_font_atlas())
		{
			_stateblock.apply();

			return false;
		}

		_stateblock.apply();

		return runtime::on_init();
	}
	void opengl_runtime::on_reset()
	{
		if (!is_initialized())
		{
			return;
		}

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
	void opengl_runtime::on_reset_effect()
	{
		runtime::on_reset_effect();

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
	void opengl_runtime::on_present()
	{
		if (!is_initialized())
			return;

		detect_depth_source();

		// Evaluate queries
		for (technique &technique : _techniques)
		{
			opengl_technique_data &technique_data = *technique.impl->as<opengl_technique_data>();

			if (technique.enabled && technique_data.query_in_flight)
			{
				GLuint64 elapsed_time = 0;
				glGetQueryObjectui64v(technique_data.query, GL_QUERY_RESULT, &elapsed_time);

				technique.average_gpu_duration.append(elapsed_time);
				technique_data.query_in_flight = false;
			}
		}

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

		// Force Direct3D coordinate conventions
		GLint clip_origin, clip_depthmode;

		if (gl3wProcs.gl.ClipControl != nullptr)
		{
			glGetIntegerv(GL_CLIP_ORIGIN, &clip_origin);
			glGetIntegerv(GL_CLIP_DEPTH_MODE, &clip_depthmode);
			glClipControl(GL_LOWER_LEFT, GL_ZERO_TO_ONE);
		}

		// Apply post processing
		if (is_effect_loaded())
		{
			// Setup vertex input
			glBindVertexArray(_default_vao);

			// Setup global states
			glDisable(GL_CULL_FACE);
			glDisable(GL_DEPTH_TEST);
			glFrontFace(GL_CCW);
			glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

			// Apply post processing
			on_present_effect();
		}

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

		if (gl3wProcs.gl.ClipControl != nullptr
			&& (clip_origin != GL_LOWER_LEFT || clip_depthmode != GL_ZERO_TO_ONE))
		{
			glClipControl(clip_origin, clip_depthmode);
		}
	}
	void opengl_runtime::on_draw_call(unsigned int vertices)
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
	void opengl_runtime::on_fbo_attachment(GLenum target, GLenum attachment, GLenum objecttarget, GLuint object, GLint level)
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

	void opengl_runtime::capture_frame(uint8_t *buffer) const
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
	bool opengl_runtime::load_effect(const reshadefx::spirv_module &module, std::string &errors)
	{
		return opengl_effect_compiler(this, module, errors).run();
	}
	bool opengl_runtime::update_texture(texture &texture, const uint8_t *data)
	{
		if (texture.impl_reference != texture_reference::none)
		{
			return false;
		}

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
		{
			glGenerateMipmap(GL_TEXTURE_2D);
		}

		glBindTexture(GL_TEXTURE_2D, previous);

		return true;
	}
	bool opengl_runtime::update_texture_reference(texture &texture, texture_reference id)
	{
		GLuint new_reference[2] = { };

		switch (id)
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

		texture.impl_reference = id;

		const auto texture_impl = texture.impl->as<opengl_tex_data>();

		assert(texture_impl != nullptr);

		if (texture_impl->id[0] == new_reference[0] &&
			texture_impl->id[1] == new_reference[1])
		{
			return true;
		}

		if (texture_impl->should_delete)
		{
			glDeleteTextures(2, texture_impl->id);
		}

		texture_impl->id[0] = new_reference[0];
		texture_impl->id[1] = new_reference[1];
		texture_impl->should_delete = false;

		return true;
	}

	void opengl_runtime::render_technique(const technique &technique)
	{
		opengl_technique_data &technique_data = *technique.impl->as<opengl_technique_data>();

		if (!technique_data.query_in_flight)
			glBeginQuery(GL_TIME_ELAPSED, technique_data.query);

		// Clear depth stencil
		glBindFramebuffer(GL_FRAMEBUFFER, _default_backbuffer_fbo);
		glClearBufferfi(GL_DEPTH_STENCIL, 0, 1.0f, 0);

		// Setup shader constants
		if (technique.uniform_storage_index >= 0)
		{
			glBindBufferBase(GL_UNIFORM_BUFFER, 0, _effect_ubos[technique.uniform_storage_index].first);
			glBufferSubData(GL_UNIFORM_BUFFER, 0, _effect_ubos[technique.uniform_storage_index].second, get_uniform_value_storage().data() + technique.uniform_storage_offset);
		}

		// Setup shader resources
		for (GLsizei i = 0; i < static_cast<GLsizei>(technique_data.samplers.size()); i++)
		{
			glActiveTexture(GL_TEXTURE0 + i);
			glBindTexture(GL_TEXTURE_2D, technique_data.samplers[i].texture->id[technique_data.samplers[i].is_srgb]);
			glBindSampler(i, technique_data.samplers[i].id);
		}

		for (const auto &pass_object : technique.passes)
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
	void opengl_runtime::render_imgui_draw_data(ImDrawData *draw_data)
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

	void opengl_runtime::detect_depth_source()
	{
		static int cooldown = 0, traffic = 0;

		if (cooldown-- > 0)
		{
			traffic += g_network_traffic > 0;
			return;
		}
		else
		{
			cooldown = 30;

			if (traffic > 10)
			{
				traffic = 0;
				_depth_source = 0;
				create_depth_texture(0, 0, GL_NONE);
				return;
			}
			else
			{
				traffic = 0;
			}
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
	void opengl_runtime::create_depth_texture(GLuint width, GLuint height, GLenum format)
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
		for (auto &texture : _textures)
		{
			if (texture.impl_reference == texture_reference::depth_buffer)
			{
				update_texture_reference(texture, texture_reference::depth_buffer);
			}
		}
	}
}
