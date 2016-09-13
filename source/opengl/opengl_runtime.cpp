#include "log.hpp"
#include "opengl_runtime.hpp"
#include "opengl_effect_compiler.hpp"
#include "input.hpp"
#include <imgui.h>
#include <assert.h>

#ifdef _DEBUG
	#define GLCHECK(call) { glGetError(); call; GLenum __e = glGetError(); if (__e != GL_NO_ERROR) { char __m[1024]; sprintf_s(__m, "OpenGL Error %x at line %d: %s", __e, __LINE__, #call); MessageBoxA(nullptr, __m, 0, MB_ICONERROR); } }
#else
	#define GLCHECK(call) call
#endif

namespace reshade::opengl
{
	GLenum target_to_binding(GLenum target)
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
	unsigned int get_renderer_id()
	{
		GLint major = 0, minor = 0;
		GLCHECK(glGetIntegerv(GL_MAJOR_VERSION, &major));
		GLCHECK(glGetIntegerv(GL_MAJOR_VERSION, &minor));

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
		if (GetModuleHandleA("nvd3d9wrap.dll") == nullptr && GetModuleHandleA("nvd3d9wrapx.dll") == nullptr)
		{
			DISPLAY_DEVICEA dd;
			dd.cb = sizeof(DISPLAY_DEVICEA);

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
		GLCHECK(glGenRenderbuffers(2, _default_backbuffer_rbo));

		GLCHECK(glBindRenderbuffer(GL_RENDERBUFFER, _default_backbuffer_rbo[0]));
		glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, _width, _height);
		GLCHECK(glBindRenderbuffer(GL_RENDERBUFFER, _default_backbuffer_rbo[1]));
		glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, _width, _height);

		GLenum status = glGetError();

		GLCHECK(glBindRenderbuffer(GL_RENDERBUFFER, 0));

		if (status != GL_NO_ERROR)
		{
			LOG(ERROR) << "Failed to create backbuffer renderbuffer with error code " << status;

			GLCHECK(glDeleteRenderbuffers(2, _default_backbuffer_rbo));

			return false;
		}

		GLCHECK(glGenFramebuffers(1, &_default_backbuffer_fbo));

		GLCHECK(glBindFramebuffer(GL_FRAMEBUFFER, _default_backbuffer_fbo));
		GLCHECK(glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, _default_backbuffer_rbo[0]));
		GLCHECK(glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, _default_backbuffer_rbo[1]));

		status = glCheckFramebufferStatus(GL_FRAMEBUFFER);

		GLCHECK(glBindFramebuffer(GL_FRAMEBUFFER, 0));

		if (status != GL_FRAMEBUFFER_COMPLETE)
		{
			LOG(ERROR) << "Failed to create backbuffer framebuffer object with status code " << status;

			GLCHECK(glDeleteFramebuffers(1, &_default_backbuffer_fbo));
			GLCHECK(glDeleteRenderbuffers(2, _default_backbuffer_rbo));

			return false;
		}

		GLCHECK(glGenTextures(2, _backbuffer_texture));

		GLCHECK(glBindTexture(GL_TEXTURE_2D, _backbuffer_texture[0]));
		glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, _width, _height);
		glTextureView(_backbuffer_texture[1], GL_TEXTURE_2D, _backbuffer_texture[0], GL_SRGB8_ALPHA8, 0, 1, 0, 1);

		status = glGetError();

		GLCHECK(glBindTexture(GL_TEXTURE_2D, 0));

		if (status != GL_NO_ERROR)
		{
			LOG(ERROR) << "Failed to create backbuffer texture with error code " << status;

			GLCHECK(glDeleteTextures(2, _backbuffer_texture));
			GLCHECK(glDeleteFramebuffers(1, &_default_backbuffer_fbo));
			GLCHECK(glDeleteRenderbuffers(2, _default_backbuffer_rbo));

			return false;
		}

		return true;
	}
	bool opengl_runtime::init_default_depth_stencil()
	{
		const depth_source_info defaultdepth = {
			static_cast<GLint>(_width),
			static_cast<GLint>(_height),
			0,
			GL_DEPTH24_STENCIL8
		};

		_depth_source_table[0] = defaultdepth;

		GLCHECK(glGenTextures(1, &_depth_texture));

		GLCHECK(glBindTexture(GL_TEXTURE_2D, _depth_texture));
		glTexStorage2D(GL_TEXTURE_2D, 1, defaultdepth.format, defaultdepth.width, defaultdepth.height);

		GLenum status = glGetError();

		GLCHECK(glBindTexture(GL_TEXTURE_2D, 0));

		if (status != GL_NO_ERROR)
		{
			LOG(ERROR) << "Failed to create depth texture with error code " << status;

			GLCHECK(glDeleteTextures(1, &_depth_texture));
			GLCHECK(glDeleteTextures(2, _backbuffer_texture));
			GLCHECK(glDeleteFramebuffers(1, &_default_backbuffer_fbo));
			GLCHECK(glDeleteRenderbuffers(2, _default_backbuffer_rbo));

			return false;
		}

		return true;
	}
	bool opengl_runtime::init_fx_resources()
	{
		GLCHECK(glGenFramebuffers(1, &_blit_fbo));

		GLCHECK(glBindFramebuffer(GL_FRAMEBUFFER, _blit_fbo));
		GLCHECK(glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, _depth_texture, 0));
		GLCHECK(glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, _backbuffer_texture[1], 0));

		GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);

		GLCHECK(glBindFramebuffer(GL_FRAMEBUFFER, 0));

		if (status != GL_FRAMEBUFFER_COMPLETE)
		{
			LOG(ERROR) << "Failed to create blit framebuffer object with status code " << status;

			GLCHECK(glDeleteFramebuffers(1, &_blit_fbo));
			GLCHECK(glDeleteTextures(1, &_depth_texture));
			GLCHECK(glDeleteTextures(2, _backbuffer_texture));
			GLCHECK(glDeleteFramebuffers(1, &_default_backbuffer_fbo));
			GLCHECK(glDeleteRenderbuffers(2, _default_backbuffer_rbo));

			return false;
		}

		GLCHECK(glGenVertexArrays(1, &_default_vao));

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
		if (!_is_initialized)
		{
			return;
		}

		runtime::on_reset();

		// Destroy resources
		GLCHECK(glDeleteVertexArrays(1, &_default_vao));
		GLCHECK(glDeleteFramebuffers(1, &_default_backbuffer_fbo));
		GLCHECK(glDeleteFramebuffers(1, &_depth_source_fbo));
		GLCHECK(glDeleteFramebuffers(1, &_blit_fbo));
		GLCHECK(glDeleteRenderbuffers(2, _default_backbuffer_rbo));
		GLCHECK(glDeleteTextures(2, _backbuffer_texture));
		GLCHECK(glDeleteTextures(1, &_depth_texture));
		GLCHECK(glDeleteVertexArrays(1, &_imgui_vao));
		GLCHECK(glDeleteBuffers(2, _imgui_vbo));
		GLCHECK(glDeleteProgram(_imgui_shader_program));

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

		for (auto &sampler : _effect_samplers)
		{
			GLCHECK(glDeleteSamplers(1, &sampler.id));
		}

		_effect_samplers.clear();

		for (auto &uniform_buffer : _effect_ubos)
		{
			GLCHECK(glDeleteBuffers(1, &uniform_buffer.first));
		}

		_effect_ubos.clear();
	}
	void opengl_runtime::on_present()
	{
		if (!_is_initialized)
		{
			LOG(ERROR) << "Failed to present! Runtime is in a lost state.";
			return;
		}
		else if (_drawcalls == 0)
		{
			return;
		}

		detect_depth_source();

		// Capture states
		_stateblock.capture();

		// Copy frame buffer
		GLCHECK(glBindFramebuffer(GL_READ_FRAMEBUFFER, 0));
		GLCHECK(glBindFramebuffer(GL_DRAW_FRAMEBUFFER, _default_backbuffer_fbo));
		GLCHECK(glReadBuffer(GL_BACK));
		GLCHECK(glDrawBuffer(GL_COLOR_ATTACHMENT0));
		GLCHECK(glBlitFramebuffer(0, 0, _width, _height, 0, 0, _width, _height, GL_COLOR_BUFFER_BIT, GL_NEAREST));

		// Copy depth buffer
		GLCHECK(glBindFramebuffer(GL_READ_FRAMEBUFFER, _depth_source_fbo));
		GLCHECK(glBindFramebuffer(GL_DRAW_FRAMEBUFFER, _blit_fbo));
		GLCHECK(glBlitFramebuffer(0, 0, _width, _height, 0, 0, _width, _height, GL_DEPTH_BUFFER_BIT, GL_NEAREST));

		// Apply post processing
		if (!_techniques.empty())
		{
			// Setup vertex input
			GLCHECK(glBindVertexArray(_default_vao));

			// Setup shader resources
			for (GLsizei sampler = 0, samplerCount = static_cast<GLsizei>(_effect_samplers.size()); sampler < samplerCount; sampler++)
			{
				GLCHECK(glActiveTexture(GL_TEXTURE0 + sampler));
				GLCHECK(glBindTexture(GL_TEXTURE_2D, _effect_samplers[sampler].texture->id[_effect_samplers[sampler].is_srgb]));
				GLCHECK(glBindSampler(sampler, _effect_samplers[sampler].id));
			}

			// Apply post processing
			runtime::on_present_effect();

			// Reset states
			GLCHECK(glBindSampler(0, 0));
		}

		glDisable(GL_FRAMEBUFFER_SRGB);

		// Reset render target and copy to frame buffer
		GLCHECK(glBindFramebuffer(GL_READ_FRAMEBUFFER, _default_backbuffer_fbo));
		GLCHECK(glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0));
		GLCHECK(glReadBuffer(GL_COLOR_ATTACHMENT0));
		GLCHECK(glDrawBuffer(GL_BACK));
		GLCHECK(glBlitFramebuffer(0, 0, _width, _height, 0, 0, _width, _height, GL_COLOR_BUFFER_BIT, GL_NEAREST));

		// Apply presenting
		runtime::on_present();

		// Apply states
		_stateblock.apply();
	}
	void opengl_runtime::on_draw_call(unsigned int vertices)
	{
		_vertices += vertices;
		_drawcalls += 1;

		GLint fbo = 0, object = 0, objecttarget = GL_NONE;
		GLCHECK(glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &fbo));

		if (fbo != 0)
		{
			GLCHECK(glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &objecttarget));

			if (objecttarget == GL_NONE)
			{
				return;
			}
			else
			{
				GLCHECK(glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &object));
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
		GLCHECK(glGetIntegerv(target_to_binding(target), &fbo));

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
			GLCHECK(glGetIntegerv(GL_RENDERBUFFER_BINDING, &previous));

			// Get depth-stencil parameters from render buffer
			GLCHECK(glBindRenderbuffer(GL_RENDERBUFFER, object));
			GLCHECK(glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_WIDTH, reinterpret_cast<int *>(&info.width)));
			GLCHECK(glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_HEIGHT, reinterpret_cast<int *>(&info.height)));
			GLCHECK(glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_INTERNAL_FORMAT, &info.format));

			GLCHECK(glBindRenderbuffer(GL_RENDERBUFFER, previous));
		}
		else
		{
			if (objecttarget == GL_TEXTURE)
			{
				objecttarget = GL_TEXTURE_2D;
			}

			GLint previous = 0;
			GLCHECK(glGetIntegerv(target_to_binding(objecttarget), &previous));

			// Get depth-stencil parameters from texture
			GLCHECK(glBindTexture(objecttarget, object));
			info.level = level;
			GLCHECK(glGetTexLevelParameteriv(objecttarget, level, GL_TEXTURE_WIDTH, reinterpret_cast<int *>(&info.width)));
			GLCHECK(glGetTexLevelParameteriv(objecttarget, level, GL_TEXTURE_HEIGHT, reinterpret_cast<int *>(&info.height)));
			GLCHECK(glGetTexLevelParameteriv(objecttarget, level, GL_TEXTURE_INTERNAL_FORMAT, &info.format));
			
			GLCHECK(glBindTexture(objecttarget, previous));
		}

		_depth_source_table.emplace(id, info);
	}

	void opengl_runtime::capture_frame(uint8_t *buffer) const
	{
		GLCHECK(glReadBuffer(GL_BACK));
		GLCHECK(glReadPixels(0, 0, static_cast<GLsizei>(_width), static_cast<GLsizei>(_height), GL_RGBA, GL_UNSIGNED_BYTE, buffer));

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
	bool opengl_runtime::load_effect(const reshadefx::syntax_tree &ast, std::string &errors)
	{
		return opengl_effect_compiler(this, ast, errors).run();
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

		GLCHECK(glPixelStorei(GL_UNPACK_ROW_LENGTH, 0));
		GLCHECK(glPixelStorei(GL_UNPACK_IMAGE_HEIGHT, 0));
		GLCHECK(glPixelStorei(GL_UNPACK_SKIP_ROWS, 0));
		GLCHECK(glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0));
		GLCHECK(glPixelStorei(GL_UNPACK_ALIGNMENT, 4));

		GLint previous = 0;
		GLCHECK(glGetIntegerv(GL_TEXTURE_BINDING_2D, &previous));

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
		GLCHECK(glBindTexture(GL_TEXTURE_2D, texture_impl->id[0]));
		GLCHECK(glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, texture.width, texture.height, GL_RGBA, GL_UNSIGNED_BYTE, data_flipped.data()));

		if (texture.levels > 1)
		{
			GLCHECK(glGenerateMipmap(GL_TEXTURE_2D));
		}

		GLCHECK(glBindTexture(GL_TEXTURE_2D, previous));

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
			GLCHECK(glDeleteTextures(2, texture_impl->id));
		}

		texture_impl->id[0] = new_reference[0];
		texture_impl->id[1] = new_reference[1];
		texture_impl->should_delete = false;

		return true;
	}

	void opengl_runtime::render_technique(const technique &technique)
	{
		// Clear depth-stencil
		GLCHECK(glBindFramebuffer(GL_FRAMEBUFFER, _default_backbuffer_fbo));
		GLCHECK(glClearBufferfi(GL_DEPTH_STENCIL, 0, 1.0f, 0));

		// Setup shader constants
		if (technique.uniform_storage_index >= 0)
		{
			GLCHECK(glBindBufferBase(GL_UNIFORM_BUFFER, 0, _effect_ubos[technique.uniform_storage_index].first));
			GLCHECK(glBufferSubData(GL_UNIFORM_BUFFER, 0, _effect_ubos[technique.uniform_storage_index].second, get_uniform_value_storage().data() + technique.uniform_storage_offset));
		}

		for (const auto &pass_ptr : technique.passes)
		{
			const auto &pass = *static_cast<const opengl_pass_data *>(pass_ptr.get());

			// Setup states
			GLCHECK(glUseProgram(pass.program));
			GLCHECK(pass.srgb ? glEnable(GL_FRAMEBUFFER_SRGB) : glDisable(GL_FRAMEBUFFER_SRGB));
			GLCHECK(glDisable(GL_SCISSOR_TEST));
			GLCHECK(glFrontFace(GL_CCW));
			GLCHECK(glPolygonMode(GL_FRONT_AND_BACK, GL_FILL));
			GLCHECK(glDisable(GL_CULL_FACE));
			GLCHECK(glColorMask(pass.color_mask[0], pass.color_mask[1], pass.color_mask[2], pass.color_mask[3]));
			GLCHECK(pass.blend ? glEnable(GL_BLEND) : glDisable(GL_BLEND));
			GLCHECK(glBlendFunc(pass.blend_src, pass.blend_dest));
			GLCHECK(glBlendEquationSeparate(pass.blend_eq_color, pass.blend_eq_alpha));
			GLCHECK(pass.depth_test ? glEnable(GL_DEPTH_TEST) : glDisable(GL_DEPTH_TEST));
			GLCHECK(glDepthMask(pass.depth_mask));
			GLCHECK(glDepthFunc(pass.depth_func));
			GLCHECK(pass.stencil_test ? glEnable(GL_STENCIL_TEST) : glDisable(GL_STENCIL_TEST));
			GLCHECK(glStencilFunc(pass.stencil_func, pass.stencil_reference, pass.stencil_read_mask));
			GLCHECK(glStencilOp(pass.stencil_op_fail, pass.stencil_op_z_fail, pass.stencil_op_z_pass));
			GLCHECK(glStencilMask(pass.stencil_mask));

			// Save frame buffer of previous pass
			GLCHECK(glBindFramebuffer(GL_READ_FRAMEBUFFER, _default_backbuffer_fbo));
			GLCHECK(glBindFramebuffer(GL_DRAW_FRAMEBUFFER, _blit_fbo));
			GLCHECK(glReadBuffer(GL_COLOR_ATTACHMENT0));
			GLCHECK(glDrawBuffer(GL_COLOR_ATTACHMENT0));
			GLCHECK(glBlitFramebuffer(0, 0, _width, _height, 0, 0, _width, _height, GL_COLOR_BUFFER_BIT, GL_NEAREST));

			// Setup render targets
			GLCHECK(glBindFramebuffer(GL_FRAMEBUFFER, pass.fbo));
			GLCHECK(glDrawBuffers(8, pass.draw_buffers));
			GLCHECK(glViewport(0, 0, pass.viewport_width, pass.viewport_height));

			if (pass.clear_render_targets)
			{
				for (GLuint k = 0; k < 8; k++)
				{
					if (pass.draw_buffers[k] == GL_NONE)
					{
						continue;
					}

					const GLfloat color[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
					GLCHECK(glClearBufferfv(GL_COLOR, k, color));
				}
			}

			// Draw triangle
			GLCHECK(glDrawArrays(GL_TRIANGLES, 0, 3));

			_vertices += 3;
			_drawcalls += 1;

			// Update shader resources
			for (GLuint id : pass.draw_textures)
			{
				for (GLsizei sampler = 0, samplerCount = static_cast<GLsizei>(_effect_samplers.size()); sampler < samplerCount; sampler++)
				{
					const auto texture = _effect_samplers[sampler].texture;

					if (_effect_samplers[sampler].has_mipmaps && (texture->id[0] == id || texture->id[1] == id))
					{
						GLCHECK(glActiveTexture(GL_TEXTURE0 + sampler));
						GLCHECK(glGenerateMipmap(GL_TEXTURE_2D));
					}
				}
			}
		}
	}
	void opengl_runtime::render_draw_lists(ImDrawData *draw_data)
	{
		// Setup render state: alpha-blending enabled, no face culling, no depth testing, scissor enabled
		glEnable(GL_BLEND);
		glBlendEquation(GL_FUNC_ADD);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glDisable(GL_CULL_FACE);
		glDisable(GL_DEPTH_TEST);
		glEnable(GL_SCISSOR_TEST);
		glDisable(GL_FRAMEBUFFER_SRGB);
		glActiveTexture(GL_TEXTURE0);
		glUseProgram(_imgui_shader_program);
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
			GLCHECK(glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previous_fbo));

			GLCHECK(glBindFramebuffer(GL_FRAMEBUFFER, _blit_fbo));
			GLCHECK(glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, _depth_texture, 0));

			assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);

			_depth_source = best_match;

			if (best_match != 0)
			{
				if (_depth_source_fbo == 0)
				{
					GLCHECK(glGenFramebuffers(1, &_depth_source_fbo));
				}

				GLCHECK(glBindFramebuffer(GL_FRAMEBUFFER, _depth_source_fbo));

				if ((best_match & 0x80000000) != 0)
				{
					best_match ^= 0x80000000;

					GLCHECK(glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, best_match));
				}
				else
				{
					GLCHECK(glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, best_match, best_info.level));
				}

				const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);

				if (status != GL_FRAMEBUFFER_COMPLETE)
				{
					LOG(ERROR) << "Failed to create depth source frame buffer with status code " << status << ".";

					GLCHECK(glDeleteFramebuffers(1, &_depth_source_fbo));
					_depth_source_fbo = 0;
				}
			}
			else
			{
				GLCHECK(glDeleteFramebuffers(1, &_depth_source_fbo));
				_depth_source_fbo = 0;
			}

			GLCHECK(glBindFramebuffer(GL_FRAMEBUFFER, previous_fbo));
		}
	}
	void opengl_runtime::create_depth_texture(GLuint width, GLuint height, GLenum format)
	{
		GLCHECK(glDeleteTextures(1, &_depth_texture));

		if (format != GL_NONE)
		{
			GLint previousTex = 0;
			GLCHECK(glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTex));

			// Clear errors
			GLenum status = glGetError();

			GLCHECK(glGenTextures(1, &_depth_texture));

			if (format == GL_DEPTH_STENCIL)
			{
				format = GL_UNSIGNED_INT_24_8;
			}

			GLCHECK(glBindTexture(GL_TEXTURE_2D, _depth_texture));
			glTexStorage2D(GL_TEXTURE_2D, 1, format, width, height);

			status = glGetError();

			if (status != GL_NO_ERROR)
			{
				LOG(ERROR) << "Failed to create depth texture for format " << format << " with error code " << status;

				GLCHECK(glDeleteTextures(1, &_depth_texture));

				_depth_texture = 0;
			}

			GLCHECK(glBindTexture(GL_TEXTURE_2D, previousTex));
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
