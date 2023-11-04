/*
 * Copyright (C) 2021 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "d3d11_impl_device.hpp"
#include "d3d11_impl_device_context.hpp"

reshade::d3d11::device_context_impl::device_context_impl(device_impl *device, ID3D11DeviceContext *context) :
	api_object_impl(context), _device_impl(device)
{
	context->QueryInterface(&_annotations);

#if RESHADE_ADDON
	invoke_addon_event<addon_event::init_command_list>(this);
	if (_orig->GetType() == D3D11_DEVICE_CONTEXT_IMMEDIATE)
		invoke_addon_event<addon_event::init_command_queue>(this);
#endif
}
reshade::d3d11::device_context_impl::~device_context_impl()
{
#if RESHADE_ADDON
	if (_orig->GetType() == D3D11_DEVICE_CONTEXT_IMMEDIATE)
		invoke_addon_event<addon_event::destroy_command_queue>(this);
	invoke_addon_event<addon_event::destroy_command_list>(this);
#endif
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

void reshade::d3d11::device_context_impl::flush_immediate_command_list() const
{
	assert(_orig->GetType() == D3D11_DEVICE_CONTEXT_IMMEDIATE);

	_orig->Flush();
}

bool reshade::d3d11::device_context_impl::wait(api::fence handle, uint64_t value)
{
	if (com_ptr<ID3D11Fence> fence;
		SUCCEEDED(reinterpret_cast<IUnknown *>(handle.handle)->QueryInterface(&fence)))
	{
		if (com_ptr<ID3D11DeviceContext4> device_context4;
			SUCCEEDED(_orig->QueryInterface(&device_context4)))
			return SUCCEEDED(device_context4->Wait(fence.get(), value));
		else
			return false;
	}

	if (com_ptr<IDXGIKeyedMutex> keyed_mutex;
		SUCCEEDED(reinterpret_cast<IUnknown *>(handle.handle)->QueryInterface(&keyed_mutex)))
	{
		return SUCCEEDED(keyed_mutex->AcquireSync(value, INFINITE));
	}

	return false;
}
bool reshade::d3d11::device_context_impl::signal(api::fence handle, uint64_t value)
{
	if (com_ptr<ID3D11Fence> fence;
		SUCCEEDED(reinterpret_cast<IUnknown *>(handle.handle)->QueryInterface(&fence)))
	{
		if (com_ptr<ID3D11DeviceContext4> device_context4;
			SUCCEEDED(_orig->QueryInterface(&device_context4)))
			return SUCCEEDED(device_context4->Signal(fence.get(), value));
		else
			return false;
	}

	if (com_ptr<IDXGIKeyedMutex> keyed_mutex;
		SUCCEEDED(reinterpret_cast<IUnknown *>(handle.handle)->QueryInterface(&keyed_mutex)))
	{
		return SUCCEEDED(keyed_mutex->ReleaseSync(value));
	}

	return false;
}
