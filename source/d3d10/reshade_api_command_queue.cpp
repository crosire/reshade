/*
 * Copyright (C) 2021 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "reshade_api_device.hpp"

void reshade::d3d10::device_impl::flush_immediate_command_list() const
{
	_orig->Flush();
}
