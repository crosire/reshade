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
}
reshade::opengl::device_context_impl::~device_context_impl()
{
	// Destroy framebuffers
	for (const auto &fbo_data : _fbo_lookup)
		gl.DeleteFramebuffers(1, &fbo_data.second);

	// Destroy vertex array objects
	for (const auto &vao_data : _vao_lookup)
		gl.DeleteVertexArrays(1, &vao_data.second);

	// Destroy push constants buffers
	gl.DeleteBuffers(static_cast<GLsizei>(_push_constants.size()), _push_constants.data());
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
