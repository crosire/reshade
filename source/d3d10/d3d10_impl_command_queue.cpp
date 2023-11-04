/*
 * Copyright (C) 2021 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "d3d10_impl_device.hpp"

void reshade::d3d10::device_impl::flush_immediate_command_list() const
{
	_orig->Flush();
}

bool reshade::d3d10::device_impl::wait(api::fence handle, uint64_t value)
{
	if (com_ptr<IDXGIKeyedMutex> keyed_mutex;
		SUCCEEDED(reinterpret_cast<IUnknown *>(handle.handle)->QueryInterface(&keyed_mutex)))
	{
		return SUCCEEDED(keyed_mutex->AcquireSync(value, INFINITE));
	}

	return false;
}
bool reshade::d3d10::device_impl::signal(api::fence handle, uint64_t value)
{
	if (com_ptr<IDXGIKeyedMutex> keyed_mutex;
		SUCCEEDED(reinterpret_cast<IUnknown *>(handle.handle)->QueryInterface(&keyed_mutex)))
	{
		return SUCCEEDED(keyed_mutex->ReleaseSync(value));
	}

	return false;
}
