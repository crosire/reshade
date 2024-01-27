/*
 * Copyright (C) 2021 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "opengl_impl_device.hpp"
#include "opengl_impl_device_context.hpp"
#include "opengl_impl_type_convert.hpp"

#define gl gl3wProcs.gl

reshade::opengl::device_context_impl::device_context_impl(device_impl *device, HGLRC hglrc) :
	api_object_impl(hglrc),
	_device_impl(device),
	_default_fbo_width(device->_default_fbo_desc.texture.width),
	_default_fbo_height(device->_default_fbo_desc.texture.height)
{
	// Create mipmap generation program used in the 'generate_mipmaps' function
	{
		static const char *const mipmap_shader =
			"#version 430\n"
			"layout(binding = 0) uniform sampler2D src;\n"
			"layout(binding = 1) uniform writeonly image2D dest;\n"
			"layout(location = 0) uniform vec3 texel;\n"
			"layout(local_size_x = 8, local_size_y = 8) in;\n"
			"void main()\n"
			"{\n"
			"	vec2 uv = texel.xy * (vec2(gl_GlobalInvocationID.xy) + vec2(0.5));\n"
			"	imageStore(dest, ivec2(gl_GlobalInvocationID.xy), textureLod(src, uv, int(texel.z)));\n"
			"}\n";

		const GLuint mipmap_cs = gl.CreateShader(GL_COMPUTE_SHADER);
		gl.ShaderSource(mipmap_cs, 1, &mipmap_shader, 0);
		gl.CompileShader(mipmap_cs);

		_mipmap_program = gl.CreateProgram();
		gl.AttachShader(_mipmap_program, mipmap_cs);
		gl.LinkProgram(_mipmap_program);
		gl.DeleteShader(mipmap_cs);

		gl.GenSamplers(1, &_mipmap_sampler);
		gl.SamplerParameteri(_mipmap_sampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
		gl.SamplerParameteri(_mipmap_sampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		gl.SamplerParameteri(_mipmap_sampler, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		gl.SamplerParameteri(_mipmap_sampler, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		gl.SamplerParameteri(_mipmap_sampler, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	}

	// Generate push constants buffer name
	gl.GenBuffers(1, &_push_constants);
}
reshade::opengl::device_context_impl::~device_context_impl()
{
	// Destroy framebuffers
	for (const auto &fbo_data : _fbo_lookup)
		gl.DeleteFramebuffers(1, &fbo_data.second);

	// Destroy push constants buffer
	gl.DeleteBuffers(1, &_push_constants);

	// Destroy mipmap generation program
	gl.DeleteProgram(_mipmap_program);
	gl.DeleteSamplers(1, &_mipmap_sampler);
}

reshade::api::device *reshade::opengl::device_context_impl::get_device()
{
	return _device_impl;
}

void reshade::opengl::device_context_impl::flush_immediate_command_list() const
{
	gl.Flush();
}

void reshade::opengl::device_context_impl::wait_idle() const
{
	gl.Finish();
}

bool reshade::opengl::device_context_impl::wait(api::fence fence, uint64_t value)
{
	if ((fence.handle >> 40) == 0xFFFFFFFF)
	{
#if 0
		const GLuint object = fence.handle & 0xFFFFFFFF;
		glSemaphoreParameterui64vEXT(object, GL_D3D12_FENCE_VALUE_EXT, &value);
		glWaitSemaphoreEXT(object, 0, nullptr, 0, nullptr, nullptr);
		return true;
#else
		return false;
#endif
	}

	const auto impl = reinterpret_cast<fence_impl *>(fence.handle);
	if (value > impl->current_value)
		return false;

	const GLsync &sync_object = impl->sync_objects[value % std::size(impl->sync_objects)];
	if (sync_object != 0)
	{
		gl.WaitSync(sync_object, 0, GL_TIMEOUT_IGNORED);
		return true;
	}
	else
	{
		return false;
	}
}
bool reshade::opengl::device_context_impl::signal(api::fence fence, uint64_t value)
{
	if ((fence.handle >> 40) == 0xFFFFFFFF)
	{
#if 0
		const GLuint object = fence.handle & 0xFFFFFFFF;
		glSemaphoreParameterui64vEXT(object, GL_D3D12_FENCE_VALUE_EXT, &value);
		glSignalSemaphoreEXT(object, 0, nullptr, 0, nullptr, nullptr);
		return true;
#else
		return false;
#endif
	}

	const auto impl = reinterpret_cast<fence_impl *>(fence.handle);
	if (value < impl->current_value)
		return false;
	impl->current_value = value;

	GLsync &sync_object = impl->sync_objects[value % std::size(impl->sync_objects)];
	gl.DeleteSync(sync_object);

	sync_object = gl.FenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
	return sync_object != 0;
}
