/*
 * Copyright (C) 2021 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "reshade_api_device.hpp"

void reshade::opengl::device_impl::flush_immediate_command_list() const
{
	glFlush();
}
