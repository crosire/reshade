/*
 * Copyright (C) 2021 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "opengl_impl_device.hpp"
#include "opengl_impl_device_context.hpp"
#include "opengl_impl_type_convert.hpp"

#define gl _device_impl->_dispatch_table

reshade::opengl::device_context_impl::device_context_impl(device_impl *device, HGLRC hglrc) :
	api_object_impl(hglrc),
	_device_impl(device),
	_default_fbo_width(device->_default_fbo_desc.texture.width),
	_default_fbo_height(device->_default_fbo_desc.texture.height)
{
#ifndef NDEBUG
	const auto debug_message_callback = [](unsigned int /* source */, unsigned int type, unsigned int /* id */, unsigned int /* severity */, int /* length */, const char *message, const void * /* userParam */) {
		if (type == GL_DEBUG_TYPE_ERROR || type == GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR)
			OutputDebugStringA(message), OutputDebugStringA("\n");
	};

	gl.Enable(GL_DEBUG_OUTPUT);
	gl.Enable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
	gl.DebugMessageCallback(debug_message_callback, nullptr);
#endif
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
		if (!gl.EXT_semaphore)
			return false;

		const GLuint object = fence.handle & 0xFFFFFFFF;

		gl.SemaphoreParameterui64vEXT(object, GL_D3D12_FENCE_VALUE_EXT, &value);
		gl.WaitSemaphoreEXT(object, 0, nullptr, 0, nullptr, nullptr);
		return true;
	}

	const auto impl = reinterpret_cast<fence_impl *>(fence.handle);
	if (value > impl->current_value)
		return false;

	const GLsync sync_object = impl->sync_objects[value % std::size(impl->sync_objects)];
	if (sync_object == 0)
		return false;

	gl.WaitSync(sync_object, 0, GL_TIMEOUT_IGNORED);
	return true;
}
bool reshade::opengl::device_context_impl::signal(api::fence fence, uint64_t value)
{
	if ((fence.handle >> 40) == 0xFFFFFFFF)
	{
		if (!gl.EXT_semaphore)
			return false;

		const GLuint object = fence.handle & 0xFFFFFFFF;

		gl.SemaphoreParameterui64vEXT(object, GL_D3D12_FENCE_VALUE_EXT, &value);
		gl.SignalSemaphoreEXT(object, 0, nullptr, 0, nullptr, nullptr);
		return true;
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
