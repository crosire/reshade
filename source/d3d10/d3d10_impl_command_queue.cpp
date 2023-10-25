/*
 * Copyright (C) 2021 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "d3d10_impl_device.hpp"

void reshade::d3d10::device_impl::flush_immediate_command_list() const
{
	_orig->Flush();
}
