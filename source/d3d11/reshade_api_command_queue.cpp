/*
 * Copyright (C) 2021 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "reshade_api_device.hpp"
#include "reshade_api_device_context.hpp"
#include "reshade_api_type_convert.hpp"

reshade::d3d11::device_context_impl::device_context_impl(device_impl *device, ID3D11DeviceContext *context) :
	api_object_impl(context), _device_impl(device)
{
	context->QueryInterface(&_annotations);

	_current_fbo = new framebuffer_impl();

#if RESHADE_ADDON
	if (_orig->GetType() != D3D11_DEVICE_CONTEXT_IMMEDIATE)
		invoke_addon_event<addon_event::init_command_list>(this);
	else
		invoke_addon_event<addon_event::init_command_queue>(this);
#endif
}
reshade::d3d11::device_context_impl::~device_context_impl()
{
#if RESHADE_ADDON
	if (_orig->GetType() != D3D11_DEVICE_CONTEXT_IMMEDIATE)
		invoke_addon_event<addon_event::destroy_command_list>(this);
	else
		invoke_addon_event<addon_event::destroy_command_queue>(this);
#endif

	delete _current_fbo;
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
