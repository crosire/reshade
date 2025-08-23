/*
 * Copyright (C) 2023 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "state_block.hpp"
#include "d3d9/d3d9_impl_state_block.hpp"
#include "d3d10/d3d10_impl_state_block.hpp"
#include "d3d11/d3d11_impl_state_block.hpp"
#include "opengl/opengl_impl_device.hpp"
#include "opengl/opengl_impl_state_block.hpp"

void reshade::api::create_state_block(api::device *device, state_block *out_state_block)
{
	switch (device->get_api())
	{
	case api::device_api::d3d9:
		*out_state_block = { reinterpret_cast<uintptr_t>(new d3d9::state_block(reinterpret_cast<IDirect3DDevice9 *>(device->get_native()))) };
		break;
	case api::device_api::d3d10:
		*out_state_block = { reinterpret_cast<uintptr_t>(new d3d10::state_block(reinterpret_cast<ID3D10Device *>(device->get_native()))) };
		break;
	case api::device_api::d3d11:
		*out_state_block = { reinterpret_cast<uintptr_t>(new d3d11::state_block(reinterpret_cast<ID3D11Device *>(device->get_native()))) };
		break;
	case api::device_api::opengl:
		*out_state_block = { reinterpret_cast<uintptr_t>(new opengl::state_block(static_cast<opengl::device_impl *>(device))) };
		break;
	default:
		*out_state_block = { 0 };
	}
}
void reshade::api::destroy_state_block(api::device *device, state_block state_block)
{
	switch (device->get_api())
	{
	case api::device_api::d3d9:
		delete reinterpret_cast<d3d9::state_block *>(state_block.handle);
		break;
	case api::device_api::d3d10:
		delete reinterpret_cast<d3d10::state_block *>(state_block.handle);
		break;
	case api::device_api::d3d11:
		delete reinterpret_cast<d3d11::state_block *>(state_block.handle);
		break;
	case api::device_api::opengl:
		delete reinterpret_cast<opengl::state_block *>(state_block.handle);
		break;
	}
}

void reshade::api::apply_state(api::command_list *cmd_list, state_block state_block)
{
	api::device *const device = cmd_list->get_device();

	switch (device->get_api())
	{
	case api::device_api::d3d9:
		reinterpret_cast<d3d9::state_block *>(state_block.handle)->apply_and_release();
		break;
	case api::device_api::d3d10:
		reinterpret_cast<d3d10::state_block *>(state_block.handle)->apply_and_release();
		break;
	case api::device_api::d3d11:
		reinterpret_cast<d3d11::state_block *>(state_block.handle)->apply_and_release();
		break;
	case api::device_api::opengl:
		reinterpret_cast<opengl::state_block *>(state_block.handle)->apply();
		break;
	}
}
void reshade::api::capture_state(api::command_list *cmd_list, state_block state_block)
{
	api::device *const device = cmd_list->get_device();

	switch (device->get_api())
	{
	case api::device_api::d3d9:
		reinterpret_cast<d3d9::state_block *>(state_block.handle)->capture();
		break;
	case api::device_api::d3d10:
		reinterpret_cast<d3d10::state_block *>(state_block.handle)->capture();
		break;
	case api::device_api::d3d11:
		reinterpret_cast<d3d11::state_block *>(state_block.handle)->capture(reinterpret_cast<ID3D11DeviceContext *>(cmd_list->get_native()));
		break;
	case api::device_api::opengl:
		reinterpret_cast<opengl::state_block *>(state_block.handle)->capture();
		break;
	}
}
