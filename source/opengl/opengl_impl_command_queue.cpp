/*
 * Copyright (C) 2021 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "opengl_impl_device.hpp"

void reshade::opengl::device_impl::flush_immediate_command_list() const
{
	glFlush();
}

void reshade::opengl::device_impl::wait_idle() const
{
	glFinish();
}
