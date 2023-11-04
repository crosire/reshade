/*
 * Copyright (C) 2021 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "d3d10_impl_device.hpp"
#include "d3d10_impl_type_convert.hpp"

void reshade::d3d10::device_impl::wait_idle() const
{
	D3D10_QUERY_DESC temp_query_desc;
	temp_query_desc.Query = D3D10_QUERY_EVENT;
	temp_query_desc.MiscFlags = 0;

	com_ptr<ID3D10Query> temp_query;
	if (SUCCEEDED(_orig->CreateQuery(&temp_query_desc, &temp_query)))
	{
		temp_query->End();
		while (temp_query->GetData(nullptr, 0, 0) == S_FALSE)
			Sleep(0);
	}
}

void reshade::d3d10::device_impl::flush_immediate_command_list() const
{
	_orig->Flush();
}

bool reshade::d3d10::device_impl::wait(api::fence fence, uint64_t value)
{
	if (fence.handle & 1)
	{
		wait_idle();

		return value <= reinterpret_cast<fence_impl *>(fence.handle ^ 1)->current_value;
	}

	if (com_ptr<IDXGIKeyedMutex> keyed_mutex;
		SUCCEEDED(reinterpret_cast<IUnknown *>(fence.handle)->QueryInterface(&keyed_mutex)))
	{
		return SUCCEEDED(keyed_mutex->AcquireSync(value, INFINITE));
	}

	return false;
}
bool reshade::d3d10::device_impl::signal(api::fence fence, uint64_t value)
{
	if (fence.handle & 1)
	{
		const auto impl = reinterpret_cast<fence_impl *>(fence.handle ^ 1);

		if (value >= impl->current_value)
		{
			impl->current_value = value;
			return true;
		}
		else
		{
			return false;
		}
	}

	if (com_ptr<IDXGIKeyedMutex> keyed_mutex;
		SUCCEEDED(reinterpret_cast<IUnknown *>(fence.handle)->QueryInterface(&keyed_mutex)))
	{
		return SUCCEEDED(keyed_mutex->ReleaseSync(value));
	}

	return false;
}
