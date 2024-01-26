/*
 * Copyright (C) 2021 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "d3d9_impl_device.hpp"

void reshade::d3d9::device_impl::wait_idle() const
{
	com_ptr<IDirect3DQuery9> temp_query;
	if (SUCCEEDED(_orig->CreateQuery(D3DQUERYTYPE_EVENT, &temp_query)))
	{
		temp_query->Issue(D3DISSUE_END);
		while (temp_query->GetData(nullptr, 0, D3DGETDATA_FLUSH) == S_FALSE)
			Sleep(0);
	}
}

void reshade::d3d9::device_impl::flush_immediate_command_list() const
{
	com_ptr<IDirect3DQuery9> temp_query;
	if (SUCCEEDED(_orig->CreateQuery(D3DQUERYTYPE_EVENT, &temp_query)))
	{
		temp_query->Issue(D3DISSUE_END);
		temp_query->GetData(nullptr, 0, D3DGETDATA_FLUSH);
	}
}

uint64_t reshade::d3d9::device_impl::get_timestamp_frequency() const
{
	com_ptr<IDirect3DQuery9> temp_query;
	if (SUCCEEDED(_orig->CreateQuery(D3DQUERYTYPE_TIMESTAMPFREQ, &temp_query)))
	{
		temp_query->Issue(D3DISSUE_END);

		UINT64 frequency = 0;
		while (temp_query->GetData(&frequency, sizeof(frequency), D3DGETDATA_FLUSH) == S_FALSE)
			Sleep(1);

		return frequency;
	}

	return 0;
}
