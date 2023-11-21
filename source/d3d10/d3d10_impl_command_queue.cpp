/*
 * Copyright (C) 2021 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "d3d10_impl_device.hpp"

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

uint64_t reshade::d3d10::device_impl::get_timestamp_frequency() const
{
	D3D10_QUERY_DESC temp_query_desc;
	temp_query_desc.Query = D3D10_QUERY_TIMESTAMP_DISJOINT;
	temp_query_desc.MiscFlags = 0;

	com_ptr<ID3D10Query> temp_query;
	if (SUCCEEDED(_orig->CreateQuery(&temp_query_desc, &temp_query)))
	{
		temp_query->Begin();
		temp_query->End();

		wait_idle();

		D3D10_QUERY_DATA_TIMESTAMP_DISJOINT temp_query_data;
		if (temp_query->GetData(&temp_query_data, sizeof(temp_query_data), 0) == S_OK)
			return temp_query_data.Frequency;
	}

	return 0;
}
