#include "log.hpp"
#include "gl_runtime.hpp"
#include "gl_fx_compiler.hpp"
#include "gui.hpp"
#include "input.hpp"

#include <assert.h>
#include <nanovg_gl.h>

#ifdef _DEBUG
	#define GLCHECK(call) { glGetError(); call; GLenum __e = glGetError(); if (__e != GL_NO_ERROR) { char __m[1024]; sprintf_s(__m, "OpenGL Error %x at line %d: %s", __e, __LINE__, #call); MessageBoxA(nullptr, __m, 0, MB_ICONERROR); } }
#else
	#define GLCHECK(call) call
#endif

namespace reshade
{
	namespace
	{
		unsigned int get_renderer_id()
		{
			GLint major = 0, minor = 0;
			GLCHECK(glGetIntegerv(GL_MAJOR_VERSION, &major));
			GLCHECK(glGetIntegerv(GL_MAJOR_VERSION, &minor));

			return 0x10000 | (major << 12) | (minor << 8);
		}
	}

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
	inline void flip_bc1_block(unsigned char *block)
	{
		// BC1 Block:
		//  [0-1]  color 0
		//  [2-3]  color 1
		//  [4-7]  color indices

		std::swap(block[4], block[7]);
		std::swap(block[5], block[6]);
	}
	inline void flip_bc2_block(unsigned char *block)
	{
		// BC2 Block:
		//  [0-7]  alpha indices
		//  [8-15] color block

		std::swap(block[0], block[6]);
		std::swap(block[1], block[7]);
		std::swap(block[2], block[4]);
		std::swap(block[3], block[5]);

		flip_bc1_block(block + 8);
	}
	inline void flip_bc4_block(unsigned char *block)
	{
		// BC4 Block:
		//  [0]    red 0
		//  [1]    red 1
		//  [2-7]  red indices

		const unsigned int line_0_1 = block[2] + 256 * (block[3] + 256 * block[4]);
		const unsigned int line_2_3 = block[5] + 256 * (block[6] + 256 * block[7]);
		const unsigned int line_1_0 = ((line_0_1 & 0x000FFF) << 12) | ((line_0_1 & 0xFFF000) >> 12);
		const unsigned int line_3_2 = ((line_2_3 & 0x000FFF) << 12) | ((line_2_3 & 0xFFF000) >> 12);
		block[2] = static_cast<unsigned char>((line_3_2 & 0xFF));
		block[3] = static_cast<unsigned char>((line_3_2 & 0xFF00) >> 8);
		block[4] = static_cast<unsigned char>((line_3_2 & 0xFF0000) >> 16);
		block[5] = static_cast<unsigned char>((line_1_0 & 0xFF));
		block[6] = static_cast<unsigned char>((line_1_0 & 0xFF00) >> 8);
		block[7] = static_cast<unsigned char>((line_1_0 & 0xFF0000) >> 16);
	}
	inline void flip_bc3_block(unsigned char *block)
	{
		// BC3 Block:
		//  [0-7]  alpha block
		//  [8-15] color block

		flip_bc4_block(block);
		flip_bc1_block(block + 8);
	}
	inline void flip_bc5_block(unsigned char *block)
	{
		// BC5 Block:
		//  [0-7]  red block
		//  [8-15] green block

		flip_bc4_block(block);
		flip_bc4_block(block + 8);
	}
	void flip_image_data(const texture *texture, unsigned char *data)
	{
		typedef void (*FlipBlockFunc)(unsigned char *block);

		size_t blocksize = 0;
		bool compressed = false;
		FlipBlockFunc compressedFunc = nullptr;

		switch (texture->format)
		{
			case texture::pixelformat::r8:
				blocksize = 1;
				break;
			case texture::pixelformat::rg8:
				blocksize = 2;
				break;
			case texture::pixelformat::r32f:
			case texture::pixelformat::rgba8:
				blocksize = 4;
				break;
			case texture::pixelformat::rgba16:
			case texture::pixelformat::rgba16f:
				blocksize = 8;
				break;
			case texture::pixelformat::rgba32f:
				blocksize = 16;
				break;
			case texture::pixelformat::dxt1:
				blocksize = 8;
				compressed = true;
				compressedFunc = &flip_bc1_block;
				break;
			case texture::pixelformat::dxt3:
				blocksize = 16;
				compressed = true;
				compressedFunc = &flip_bc2_block;
				break;
			case texture::pixelformat::dxt5:
				blocksize = 16;
				compressed = true;
				compressedFunc = &flip_bc3_block;
				break;
			case texture::pixelformat::latc1:
				blocksize = 8;
				compressed = true;
				compressedFunc = &flip_bc4_block;
				break;
			case texture::pixelformat::latc2:
				blocksize = 16;
				compressed = true;
				compressedFunc = &flip_bc5_block;
				break;
			default:
				return;
		}

		if (compressed)
		{
			const size_t w = (texture->width + 3) / 4;
			const size_t h = (texture->height + 3) / 4;
			const size_t stride = w * blocksize;

			for (size_t y = 0; y < h; ++y)
			{
				unsigned char *dataLine = data + stride * (h - 1 - y);

				for (size_t x = 0; x < stride; x += blocksize)
				{
					compressedFunc(dataLine + x);
				}
			}
		}
		else
		{
			const size_t w = texture->width;
			const size_t h = texture->height;
			const size_t stride = w * blocksize;
			unsigned char *templine = static_cast<unsigned char *>(::alloca(stride));

			for (size_t y = 0; 2 * y < h; ++y)
			{
				unsigned char *line1 = data + stride * y;
				unsigned char *line2 = data + stride * (h - 1 - y);

				std::memcpy(templine, line1, stride);
				std::memcpy(line1, line2, stride);
				std::memcpy(line2, templine, stride);
			}
		}
	}

	gl_runtime::gl_runtime(HDC device) : runtime(get_renderer_id()), _hdc(device), _reference_count(1), _default_backbuffer_fbo(0), _default_backbuffer_rbo(), _backbuffer_texture(), _depth_source_fbo(0), _depth_source(0), _depth_texture(0), _blit_fbo(0), _default_vao(0), _effect_ubo(0)
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

	bool gl_runtime::on_init(unsigned int width, unsigned int height)
	{
		assert(width != 0 && height != 0);

		_width = width;
		_height = height;

		// Clear errors
		GLenum status = glGetError();

		_stateblock.capture();

		#pragma region Generate backbuffer targets
		GLCHECK(glGenRenderbuffers(2, _default_backbuffer_rbo));

		GLCHECK(glBindRenderbuffer(GL_RENDERBUFFER, _default_backbuffer_rbo[0]));
		glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, width, height);
		GLCHECK(glBindRenderbuffer(GL_RENDERBUFFER, _default_backbuffer_rbo[1]));
		glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);

		status = glGetError();

		GLCHECK(glBindRenderbuffer(GL_RENDERBUFFER, 0));

		if (status != GL_NO_ERROR)
		{
			LOG(TRACE) << "Failed to create backbuffer renderbuffer with error code " << status;

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
			LOG(TRACE) << "Failed to create backbuffer framebuffer object with status code " << status;

			GLCHECK(glDeleteFramebuffers(1, &_default_backbuffer_fbo));
			GLCHECK(glDeleteRenderbuffers(2, _default_backbuffer_rbo));

			return false;
		}

		GLCHECK(glGenTextures(2, _backbuffer_texture));

		GLCHECK(glBindTexture(GL_TEXTURE_2D, _backbuffer_texture[0]));
		glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, width, height);
		glTextureView(_backbuffer_texture[1], GL_TEXTURE_2D, _backbuffer_texture[0], GL_SRGB8_ALPHA8, 0, 1, 0, 1);
		
		status = glGetError();

		GLCHECK(glBindTexture(GL_TEXTURE_2D, 0));

		if (status != GL_NO_ERROR)
		{
			LOG(TRACE) << "Failed to create backbuffer texture with error code " << status;

			GLCHECK(glDeleteTextures(2, _backbuffer_texture));
			GLCHECK(glDeleteFramebuffers(1, &_default_backbuffer_fbo));
			GLCHECK(glDeleteRenderbuffers(2, _default_backbuffer_rbo));

			return false;
		}
		#pragma endregion

		#pragma region Generate depthbuffer targets
		const depth_source_info defaultdepth = { static_cast<GLint>(width), static_cast<GLint>(height), 0, GL_DEPTH24_STENCIL8 };

		_depth_source_table[0] = defaultdepth;

		LOG(TRACE) << "Switched depth source to default depthstencil.";

		GLCHECK(glGenTextures(1, &_depth_texture));

		GLCHECK(glBindTexture(GL_TEXTURE_2D, _depth_texture));
		glTexStorage2D(GL_TEXTURE_2D, 1, defaultdepth.format, defaultdepth.width, defaultdepth.height);

		status = glGetError();

		GLCHECK(glBindTexture(GL_TEXTURE_2D, 0));

		if (status != GL_NO_ERROR)
		{
			LOG(TRACE) << "Failed to create depth texture with error code " << status;

			GLCHECK(glDeleteTextures(1, &_depth_texture));
			GLCHECK(glDeleteTextures(2, _backbuffer_texture));
			GLCHECK(glDeleteFramebuffers(1, &_default_backbuffer_fbo));
			GLCHECK(glDeleteRenderbuffers(2, _default_backbuffer_rbo));

			return false;
		}
		#pragma endregion

		GLCHECK(glGenFramebuffers(1, &_blit_fbo));

		GLCHECK(glBindFramebuffer(GL_FRAMEBUFFER, _blit_fbo));
		GLCHECK(glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, _depth_texture, 0));
		GLCHECK(glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, _backbuffer_texture[1], 0));

		status = glCheckFramebufferStatus(GL_FRAMEBUFFER);

		GLCHECK(glBindFramebuffer(GL_FRAMEBUFFER, 0));

		if (status != GL_FRAMEBUFFER_COMPLETE)
		{
			LOG(TRACE) << "Failed to create blit framebuffer object with status code " << status;

			GLCHECK(glDeleteFramebuffers(1, &_blit_fbo));
			GLCHECK(glDeleteTextures(1, &_depth_texture));
			GLCHECK(glDeleteTextures(2, _backbuffer_texture));
			GLCHECK(glDeleteFramebuffers(1, &_default_backbuffer_fbo));
			GLCHECK(glDeleteRenderbuffers(2, _default_backbuffer_rbo));

			return false;
		}

		GLCHECK(glGenVertexArrays(1, &_default_vao));

		_gui.reset(new gui(this, nvgCreateGL3(0)));

		_stateblock.apply();

		return runtime::on_init();
	}
	void gl_runtime::on_reset()
	{
		if (!_is_initialized)
		{
			return;
		}

		runtime::on_reset();

		// Destroy NanoVG
		nvgDeleteGL3(_gui->context());
		_gui.reset();

		// Destroy resources
		GLCHECK(glDeleteBuffers(1, &_effect_ubo));
		GLCHECK(glDeleteVertexArrays(1, &_default_vao));
		GLCHECK(glDeleteFramebuffers(1, &_default_backbuffer_fbo));
		GLCHECK(glDeleteFramebuffers(1, &_depth_source_fbo));
		GLCHECK(glDeleteFramebuffers(1, &_blit_fbo));
		GLCHECK(glDeleteRenderbuffers(2, _default_backbuffer_rbo));
		GLCHECK(glDeleteTextures(2, _backbuffer_texture));
		GLCHECK(glDeleteTextures(1, &_depth_texture));

		_effect_ubo = 0;
		_default_vao = 0;
		_default_backbuffer_fbo = 0;
		_depth_source_fbo = 0;
		_blit_fbo = 0;
		_default_backbuffer_rbo[0] = 0;
		_default_backbuffer_rbo[1] = 0;
		_backbuffer_texture[0] = 0;
		_backbuffer_texture[1] = 0;
		_depth_texture = 0;

		_depth_source = 0;
	}
	void gl_runtime::on_reset_effect()
	{
		runtime::on_reset_effect();

		for (auto &sampler : _effect_samplers)
		{
			GLCHECK(glDeleteSamplers(1, &sampler.id));
		}

		_effect_samplers.clear();
	}
	void gl_runtime::on_present()
	{
		if (!_is_initialized)
		{
			LOG(TRACE) << "Failed to present! Runtime is in a lost state.";
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
		on_apply_effect();

		glDisable(GL_FRAMEBUFFER_SRGB);

		// Reset render target and copy to frame buffer
		GLCHECK(glBindFramebuffer(GL_READ_FRAMEBUFFER, _default_backbuffer_fbo));
		GLCHECK(glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0));
		GLCHECK(glReadBuffer(GL_COLOR_ATTACHMENT0));
		GLCHECK(glDrawBuffer(GL_BACK));
		GLCHECK(glBlitFramebuffer(0, 0, _width, _height, 0, 0, _width, _height, GL_COLOR_BUFFER_BIT, GL_NEAREST));
		GLCHECK(glViewport(0, 0, _width, _height));

		// Apply presenting
		runtime::on_present();

		// Apply states
		_stateblock.apply();
	}
	void gl_runtime::on_draw_call(unsigned int vertices)
	{
		runtime::on_draw_call(vertices);

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
			it->second.drawcall_count = static_cast<GLfloat>(_drawcalls);
			it->second.vertices_count += vertices;
		}
	}
	void gl_runtime::on_apply_effect()
	{
		if (!_is_effect_compiled)
		{
			return;
		}

		// Setup vertex input
		GLCHECK(glBindVertexArray(_default_vao));

		// Setup shader resources
		for (GLsizei sampler = 0, samplerCount = static_cast<GLsizei>(_effect_samplers.size()); sampler < samplerCount; sampler++)
		{
			GLCHECK(glActiveTexture(GL_TEXTURE0 + sampler));
			GLCHECK(glBindTexture(GL_TEXTURE_2D, _effect_samplers[sampler].texture->id[_effect_samplers[sampler].is_srgb]));
			GLCHECK(glBindSampler(sampler, _effect_samplers[sampler].id));
		}

		// Setup shader constants
		GLCHECK(glBindBufferBase(GL_UNIFORM_BUFFER, 0, _effect_ubo));

		// Apply post processing
		runtime::on_apply_effect();

		// Reset states
		GLCHECK(glBindSampler(0, 0));
	}
	void gl_runtime::on_apply_effect_technique(const technique &technique)
	{
		runtime::on_apply_effect_technique(technique);

		// Clear depth-stencil
		GLCHECK(glBindFramebuffer(GL_FRAMEBUFFER, _default_backbuffer_fbo));
		GLCHECK(glClearBufferfi(GL_DEPTH_STENCIL, 0, 1.0f, 0));

		// Update shader constants
		if (_effect_ubo != 0)
		{
			auto &uniform_storage = get_uniform_value_storage();
			GLCHECK(glBufferSubData(GL_UNIFORM_BUFFER, 0, uniform_storage.size(), uniform_storage.data()));
		}

		for (const auto &pass_ptr : technique.passes)
		{
			const auto &pass = *static_cast<const gl_pass *>(pass_ptr.get());

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

			for (GLuint k = 0; k < 8; k++)
			{
				if (pass.draw_buffers[k] == GL_NONE)
				{
					continue;
				}

				const GLfloat color[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
				GLCHECK(glClearBufferfv(GL_COLOR, k, color));
			}

			// Draw triangle
			GLCHECK(glDrawArrays(GL_TRIANGLES, 0, 3));

			// Update shader resources
			for (GLuint id : pass.draw_textures)
			{
				for (GLsizei sampler = 0, samplerCount = static_cast<GLsizei>(_effect_samplers.size()); sampler < samplerCount; sampler++)
				{
					const gl_texture *const texture = _effect_samplers[sampler].texture;

					if (texture->levels > 1 && (texture->id[0] == id || texture->id[1] == id))
					{
						GLCHECK(glActiveTexture(GL_TEXTURE0 + sampler));
						GLCHECK(glGenerateMipmap(GL_TEXTURE_2D));
					}
				}
			}
		}
	}

	void gl_runtime::on_fbo_attachment(GLenum target, GLenum attachment, GLenum objecttarget, GLuint object, GLint level)
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
			GLCHECK(glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_WIDTH, &info.width));
			GLCHECK(glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_HEIGHT, &info.height));
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
			GLCHECK(glGetTexLevelParameteriv(objecttarget, level, GL_TEXTURE_WIDTH, &info.width));
			GLCHECK(glGetTexLevelParameteriv(objecttarget, level, GL_TEXTURE_HEIGHT, &info.height));
			GLCHECK(glGetTexLevelParameteriv(objecttarget, level, GL_TEXTURE_INTERNAL_FORMAT, &info.format));
			
			GLCHECK(glBindTexture(objecttarget, previous));
		}

		LOG(TRACE) << "Adding frame buffer " << fbo << " attachment " << object << " (Attachment Type: " << attachment << ", Object Type: " << objecttarget << ", Width: " << info.width << ", Height: " << info.height << ", Format: " << info.format << ") to list of possible depth candidates ...";

		_depth_source_table.emplace(id, info);
	}

	void gl_runtime::screenshot(unsigned char *buffer) const
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
	bool gl_runtime::update_effect(const fx::nodetree &ast, const std::vector<std::string> &/*pragmas*/, std::string &errors)
	{
		return gl_fx_compiler(this, ast, errors).run();
	}
	bool gl_runtime::update_texture(texture *texture, const unsigned char *data, size_t size)
	{
		const auto texture_impl = dynamic_cast<gl_texture *>(texture);

		assert(texture_impl != nullptr);
		assert(data != nullptr && size > 0);

		if (texture_impl->basetype != texture::datatype::image)
		{
			return false;
		}

		GLCHECK(glPixelStorei(GL_UNPACK_ROW_LENGTH, 0));
		GLCHECK(glPixelStorei(GL_UNPACK_IMAGE_HEIGHT, 0));
		GLCHECK(glPixelStorei(GL_UNPACK_SKIP_ROWS, 0));
		GLCHECK(glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0));

		GLint previous = 0;
		GLCHECK(glGetIntegerv(GL_TEXTURE_BINDING_2D, &previous));

		// Copy image data
		const std::unique_ptr<unsigned char[]> dataFlipped(new unsigned char[size]);
		std::memcpy(dataFlipped.get(), data, size);

		// Flip image data vertically
		flip_image_data(texture, dataFlipped.get());

		// Bind and update texture
		GLCHECK(glBindTexture(GL_TEXTURE_2D, texture_impl->id[0]));

		if (texture->format >= texture::pixelformat::dxt1 && texture->format <= texture::pixelformat::latc2)
		{
			GLCHECK(glCompressedTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, texture->width, texture->height, GL_UNSIGNED_BYTE, static_cast<GLsizei>(size), dataFlipped.get()));
		}
		else
		{
			GLint dataAlignment = 4;
			GLenum dataFormat = GL_RGBA, dataType = GL_UNSIGNED_BYTE;

			switch (texture->format)
			{
				case texture::pixelformat::r8:
					dataFormat = GL_RED;
					dataAlignment = 1;
					break;
				case texture::pixelformat::r16f:
					dataType = GL_UNSIGNED_SHORT;
					dataFormat = GL_RED;
					dataAlignment = 2;
					break;
				case texture::pixelformat::r32f:
					dataType = GL_FLOAT;
					dataFormat = GL_RED;
					break;
				case texture::pixelformat::rg8:
					dataFormat = GL_RG;
					dataAlignment = 2;
					break;
				case texture::pixelformat::rg16:
				case texture::pixelformat::rg16f:
					dataType = GL_UNSIGNED_SHORT;
					dataFormat = GL_RG;
					dataAlignment = 2;
					break;
				case texture::pixelformat::rg32f:
					dataType = GL_FLOAT;
					dataFormat = GL_RG;
					break;
				case texture::pixelformat::rgba16:
				case texture::pixelformat::rgba16f:
					dataType = GL_UNSIGNED_SHORT;
					dataAlignment = 2;
					break;
				case texture::pixelformat::rgba32f:
					dataType = GL_FLOAT;
					break;
			}

			GLCHECK(glPixelStorei(GL_UNPACK_ALIGNMENT, dataAlignment));
			GLCHECK(glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, texture->width, texture->height, dataFormat, dataType, dataFlipped.get()));
			GLCHECK(glPixelStorei(GL_UNPACK_ALIGNMENT, 4));
		}

		if (texture->levels > 1)
		{
			GLCHECK(glGenerateMipmap(GL_TEXTURE_2D));
		}

		GLCHECK(glBindTexture(GL_TEXTURE_2D, previous));

		return true;
	}
	void gl_runtime::update_texture_datatype(texture *texture, texture::datatype source, GLuint newtexture, GLuint newtexture_srgb)
	{
		const auto texture_impl = static_cast<gl_texture *>(texture);

		if (texture_impl->basetype == texture::datatype::image)
		{
			GLCHECK(glDeleteTextures(2, texture_impl->id));
		}

		texture_impl->basetype = source;

		if (newtexture_srgb == 0)
		{
			newtexture_srgb = newtexture;
		}

		if (texture_impl->id[0] == newtexture && texture_impl->id[1] == newtexture_srgb)
		{
			return;
		}

		texture_impl->id[0] = newtexture;
		texture_impl->id[1] = newtexture_srgb;
	}

	void gl_runtime::detect_depth_source()
	{
		static int cooldown = 0, traffic = 0;

		if (cooldown-- > 0)
		{
			traffic += s_network_traffic > 0;
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

			if (((depthstencil_info.vertices_count * (1.2f - depthstencil_info.drawcall_count / _drawcalls)) >= (best_info.vertices_count * (1.2f - best_info.drawcall_count / _drawcalls))) &&
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

					LOG(TRACE) << "Switched depth source to render buffer " << best_match << ".";

					GLCHECK(glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, best_match));
				}
				else
				{
					LOG(TRACE) << "Switched depth source to texture " << best_match << ".";

					GLCHECK(glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, best_match, best_info.level));
				}

				const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);

				if (status != GL_FRAMEBUFFER_COMPLETE)
				{
					LOG(TRACE) << "Failed to create depth source frame buffer with status code " << status << ".";

					GLCHECK(glDeleteFramebuffers(1, &_depth_source_fbo));
					_depth_source_fbo = 0;
				}
			}
			else
			{
				LOG(TRACE) << "Switched depth source to default frame buffer.";

				GLCHECK(glDeleteFramebuffers(1, &_depth_source_fbo));
				_depth_source_fbo = 0;
			}

			GLCHECK(glBindFramebuffer(GL_FRAMEBUFFER, previous_fbo));
		}
	}
	void gl_runtime::create_depth_texture(GLuint width, GLuint height, GLenum format)
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
		for (const auto &texture : _textures)
		{
			if (texture->basetype == texture::datatype::depthbuffer)
			{
				update_texture_datatype(texture.get(), texture::datatype::depthbuffer, _depth_texture, 0);
			}
		}
	}
}
