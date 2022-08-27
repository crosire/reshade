/*
 * Copyright (C) 2021 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "opengl_impl_device.hpp"

#define gl gl3wProcs.gl

void reshade::opengl::device_impl::flush_immediate_command_list() const
{
	gl.Flush();
}

void reshade::opengl::device_impl::wait_idle() const
{
	gl.Finish();
}
