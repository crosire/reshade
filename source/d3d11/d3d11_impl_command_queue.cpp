/*
 * Copyright (C) 2021 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "d3d11_impl_device.hpp"
#include "d3d11_impl_device_context.hpp"
#include "d3d11_impl_type_convert.hpp"

reshade::d3d11::device_context_impl::device_context_impl(device_impl *device, ID3D11DeviceContext *context) :
	api_object_impl(context),
	_device_impl(device)
{
	context->QueryInterface(&_annotations);
}

reshade::api::device *reshade::d3d11::device_context_impl::get_device()
{
	return _device_impl;
}

reshade::api::command_list *reshade::d3d11::device_context_impl::get_immediate_command_list()
{
	assert(_orig->GetType() == D3D11_DEVICE_CONTEXT_IMMEDIATE);

	return this;
}

void reshade::d3d11::device_context_impl::wait_idle() const
{
#if 0
	assert(_orig->GetType() == D3D11_DEVICE_CONTEXT_IMMEDIATE);

	D3D11_QUERY_DESC temp_query_desc;
	temp_query_desc.Query = D3D11_QUERY_EVENT;
	temp_query_desc.MiscFlags = 0;

	com_ptr<ID3D11Query> temp_query;
	if (SUCCEEDED(_device_impl->_orig->CreateQuery(&temp_query_desc, &temp_query)))
	{
		_orig->End(temp_query.get());
		while (_orig->GetData(temp_query.get(), nullptr, 0, 0) == S_FALSE)
			Sleep(0);
	}
#endif
}

void reshade::d3d11::device_context_impl::flush_immediate_command_list() const
{
	assert(_orig->GetType() == D3D11_DEVICE_CONTEXT_IMMEDIATE);

	_orig->Flush();
}

bool reshade::d3d11::device_context_impl::wait(api::fence fence, uint64_t value)
{
	if (fence.handle & 1)
	{
		const auto impl = reinterpret_cast<fence_impl *>(fence.handle ^ 1);
		if (value > impl->current_value)
			return false;

		while (true)
		{
			const HRESULT hr = _orig->GetData(impl->event_queries[value % std::size(impl->event_queries)].get(), nullptr, 0, 0);
			if (hr == S_OK)
				return true;
			if (hr != S_FALSE)
				break;

			Sleep(1);
		}

		return false;
	}

	if (com_ptr<ID3D11Fence> fence_object;
		SUCCEEDED(reinterpret_cast<IUnknown *>(fence.handle)->QueryInterface(&fence_object)))
	{
		if (com_ptr<ID3D11DeviceContext4> device_context4;
			SUCCEEDED(_orig->QueryInterface(&device_context4)))
		{
			return SUCCEEDED(device_context4->Wait(fence_object.get(), value));
		}

		return false;
	}

	return _device_impl->wait(fence, value, UINT64_MAX);
}
bool reshade::d3d11::device_context_impl::signal(api::fence fence, uint64_t value)
{
	if (fence.handle & 1)
	{
		const auto impl = reinterpret_cast<fence_impl *>(fence.handle ^ 1);
		if (value < impl->current_value)
			return false;
		impl->current_value = value;

		_orig->End(impl->event_queries[value % std::size(impl->event_queries)].get());
		return true;
	}

	if (com_ptr<ID3D11Fence> fence_object;
		SUCCEEDED(reinterpret_cast<IUnknown *>(fence.handle)->QueryInterface(&fence_object)))
	{
		if (com_ptr<ID3D11DeviceContext4> device_context4;
			SUCCEEDED(_orig->QueryInterface(&device_context4)))
		{
			return SUCCEEDED(device_context4->Signal(fence_object.get(), value));
		}

		return false;
	}

	return _device_impl->signal(fence, value);
}

uint64_t reshade::d3d11::device_context_impl::get_timestamp_frequency() const
{
	assert(_orig->GetType() == D3D11_DEVICE_CONTEXT_IMMEDIATE);

	D3D11_QUERY_DESC temp_query_desc;
	temp_query_desc.Query = D3D11_QUERY_TIMESTAMP_DISJOINT;
	temp_query_desc.MiscFlags = 0;

	com_ptr<ID3D11Query> temp_query;
	if (SUCCEEDED(_device_impl->_orig->CreateQuery(&temp_query_desc, &temp_query)))
	{
		_orig->Begin(temp_query.get());
		_orig->End(temp_query.get());

		D3D11_QUERY_DATA_TIMESTAMP_DISJOINT temp_query_data = {};
		while (_orig->GetData(temp_query.get(), &temp_query_data, sizeof(temp_query_data), 0) == S_FALSE)
			Sleep(1);

		return temp_query_data.Frequency;
	}

	return 0;
}
