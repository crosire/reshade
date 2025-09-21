/*
 * Copyright (C) 2022 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "d3d10_device.hpp"
#include "d3d10_resource.hpp"
#include "com_utils.hpp"
#include "hook_manager.hpp"

void STDMETHODCALLTYPE ID3D10Resource_GetDevice(ID3D10Resource *pResource, ID3D10Device **ppDevice)
{
	reshade::hooks::call(ID3D10Resource_GetDevice, reshade::hooks::vtable_from_instance(pResource) + 3)(pResource, ppDevice);

	const auto device = *ppDevice;
	assert(device != nullptr);

	const auto device_proxy = get_private_pointer_d3dx<D3D10Device>(device);
	if (device_proxy != nullptr && device_proxy->_orig == device)
	{
		InterlockedIncrement(&device_proxy->_ref);
		*ppDevice = device_proxy;
	}
}

#if RESHADE_ADDON >= 2

#include "d3d10_impl_type_convert.hpp"
#include "addon_manager.hpp"

using reshade::d3d10::to_handle;

static void invoke_map_buffer_region_event(ID3D10Buffer *resource, D3D10_MAP map_type, void **data)
{
	com_ptr<ID3D10Device> device;
	resource->GetDevice(&device);

	const auto device_proxy = get_private_pointer_d3dx<D3D10Device>(device.get());
	if (device_proxy == nullptr)
		return;

	reshade::invoke_addon_event<reshade::addon_event::map_buffer_region>(
		device_proxy,
		to_handle(resource),
		0,
		UINT64_MAX,
		reshade::d3d10::convert_access_flags(map_type),
		data);
}
static void invoke_unmap_buffer_region_event(ID3D10Buffer *resource)
{
	com_ptr<ID3D10Device> device;
	resource->GetDevice(&device);

	const auto device_proxy = get_private_pointer_d3dx<D3D10Device>(device.get());
	if (device_proxy == nullptr)
		return;

	reshade::invoke_addon_event<reshade::addon_event::unmap_buffer_region>(device_proxy, to_handle(resource));
}
static void invoke_map_texture_region_event(ID3D10Resource *resource, UINT subresource, D3D10_MAP map_type, reshade::api::subresource_data *data)
{
	com_ptr<ID3D10Device> device;
	resource->GetDevice(&device);

	const auto device_proxy = get_private_pointer_d3dx<D3D10Device>(device.get());
	if (device_proxy == nullptr)
		return;

	reshade::invoke_addon_event<reshade::addon_event::map_texture_region>(
		device_proxy,
		to_handle(resource),
		subresource,
		nullptr,
		reshade::d3d10::convert_access_flags(map_type),
		data);
}
static void invoke_unmap_texture_region_event(ID3D10Resource *resource, UINT subresource)
{
	com_ptr<ID3D10Device> device;
	resource->GetDevice(&device);

	const auto device_proxy = get_private_pointer_d3dx<D3D10Device>(device.get());
	if (device_proxy == nullptr)
		return;

	reshade::invoke_addon_event<reshade::addon_event::unmap_texture_region>(device_proxy, to_handle(resource), subresource);
}

HRESULT STDMETHODCALLTYPE ID3D10Buffer_Map(ID3D10Buffer *pResource, D3D10_MAP MapType, UINT MapFlags, void **ppData)
{
	const HRESULT hr = reshade::hooks::call(ID3D10Buffer_Map, reshade::hooks::vtable_from_instance(pResource) + 10)(pResource, MapType, MapFlags, ppData);
	if (SUCCEEDED(hr))
	{
		assert(ppData != nullptr);

		invoke_map_buffer_region_event(pResource, MapType, ppData);
	}

	return hr;
}
HRESULT STDMETHODCALLTYPE ID3D10Buffer_Unmap(ID3D10Buffer *pResource)
{
	invoke_unmap_buffer_region_event(pResource);

	return reshade::hooks::call(ID3D10Buffer_Unmap, reshade::hooks::vtable_from_instance(pResource) + 11)(pResource);
}

HRESULT STDMETHODCALLTYPE ID3D10Texture1D_Map(ID3D10Texture1D *pResource, UINT Subresource, D3D10_MAP MapType, UINT MapFlags, void **ppData)
{
	const HRESULT hr = reshade::hooks::call(ID3D10Texture1D_Map, reshade::hooks::vtable_from_instance(pResource) + 10)(pResource, Subresource, MapType, MapFlags, ppData);
	if (SUCCEEDED(hr) && ppData != nullptr)
	{
		reshade::api::subresource_data data;
		data.data = *ppData;
		data.row_pitch = 0;
		data.slice_pitch = 0;

		invoke_map_texture_region_event(pResource, Subresource, MapType, &data);

		*ppData = data.data;
	}

	return hr;
}
HRESULT STDMETHODCALLTYPE ID3D10Texture1D_Unmap(ID3D10Texture1D *pResource, UINT Subresource)
{
	invoke_unmap_texture_region_event(pResource, Subresource);

	return reshade::hooks::call(ID3D10Texture1D_Unmap, reshade::hooks::vtable_from_instance(pResource) + 11)(pResource, Subresource);
}

HRESULT STDMETHODCALLTYPE ID3D10Texture2D_Map(ID3D10Texture2D *pResource, UINT Subresource, D3D10_MAP MapType, UINT MapFlags, D3D10_MAPPED_TEXTURE2D *pMappedTex2D)
{
	const HRESULT hr = reshade::hooks::call(ID3D10Texture2D_Map, reshade::hooks::vtable_from_instance(pResource) + 10)(pResource, Subresource, MapType, MapFlags, pMappedTex2D);
	if (SUCCEEDED(hr) && pMappedTex2D != nullptr)
	{
		reshade::api::subresource_data data;
		data.data = pMappedTex2D->pData;
		data.row_pitch = pMappedTex2D->RowPitch;
		data.slice_pitch = 0;

		invoke_map_texture_region_event(pResource, Subresource, MapType, &data);

		pMappedTex2D->pData = data.data;
		pMappedTex2D->RowPitch = data.row_pitch;
	}

	return hr;
}
HRESULT STDMETHODCALLTYPE ID3D10Texture2D_Unmap(ID3D10Texture2D *pResource, UINT Subresource)
{
	invoke_unmap_texture_region_event(pResource, Subresource);

	return reshade::hooks::call(ID3D10Texture2D_Unmap, reshade::hooks::vtable_from_instance(pResource) + 11)(pResource, Subresource);
}

HRESULT STDMETHODCALLTYPE ID3D10Texture3D_Map(ID3D10Texture3D *pResource, UINT Subresource, D3D10_MAP MapType, UINT MapFlags, D3D10_MAPPED_TEXTURE3D *pMappedTex3D)
{
	const HRESULT hr = reshade::hooks::call(ID3D10Texture3D_Map, reshade::hooks::vtable_from_instance(pResource) + 10)(pResource, Subresource, MapType, MapFlags, pMappedTex3D);
	if (SUCCEEDED(hr) && pMappedTex3D != nullptr)
	{
		invoke_map_texture_region_event(pResource, Subresource, MapType, reinterpret_cast<reshade::api::subresource_data *>(pMappedTex3D));
	}

	return hr;
}
HRESULT STDMETHODCALLTYPE ID3D10Texture3D_Unmap(ID3D10Texture3D *pResource, UINT Subresource)
{
	invoke_unmap_texture_region_event(pResource, Subresource);

	return reshade::hooks::call(ID3D10Texture3D_Unmap, reshade::hooks::vtable_from_instance(pResource) + 11)(pResource, Subresource);
}

#endif
